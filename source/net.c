#include "net.h"

#include "download_policy.h"
#include "i18n.h"
#include "network_retry.h"

#include <3ds.h>
#include <curl/curl.h>

#include <malloc.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>
#include <strings.h>

#define SOC_ALIGN 0x1000U
#define SOC_BUFFER_SIZE (1024U * 1024U)
#define HTTP_MAX_RESPONSE (2U * 1024U * 1024U)
#define HTTP_REDIRECTS 5L
#define CA_BUNDLE_PATH "romfs:/cacert.pem"
#define FILE_PUBLISH_BYTES (64U * 1024U)
#define REQUEST_RETRY_DELAY_NS (250LL * 1000LL * 1000LL)
#define REQUEST_RETRY_POLL_NS (25LL * 1000LL * 1000LL)

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
    bool overflow;
} MemorySink;

typedef struct {
    FILE *file;
    NetProgress progress;
    void *userdata;
    NetPublish publish;
    void *publish_userdata;
    NetCancel cancel;
    void *cancel_userdata;
    uint64_t received;
    uint64_t total;
    uint64_t published;
    uint64_t max_bytes;
    uint64_t space_budget;
    bool write_failed;
    bool size_exceeded;
    bool space_exceeded;
} FileSink;

typedef struct {
    NetCancel cancel;
    void *userdata;
} CancelSink;

typedef struct {
    char *cookie;
    size_t size;
} HeaderSink;

typedef struct {
    NetResponseWrite write;
    NetResponseReset reset;
    void *userdata;
    uint64_t received;
    uint64_t max_bytes;
    bool write_failed;
    bool reset_failed;
    bool size_exceeded;
} ResponseStreamSink;

static u32 *soc_buffer;
static bool ac_ready;
static bool soc_ready;
static bool curl_ready;
static bool net_ready;

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static void cleanup_services(void) {
    if (curl_ready) {
        curl_global_cleanup();
        curl_ready = false;
    }
    if (soc_ready) {
        socExit();
        soc_ready = false;
    }
    if (ac_ready) {
        acExit();
        ac_ready = false;
    }
    free(soc_buffer);
    soc_buffer = NULL;
    net_ready = false;
}

int net_init(char *error, size_t error_size) {
    if (net_ready) return 0;

    FILE *ca_file = fopen(CA_BUNDLE_PATH, "rb");
    if (!ca_file) {
        set_error(error, error_size, "RomFS 中缺少 CA 证书包");
        cleanup_services();
        return -1;
    }
    fclose(ca_file);

    Result ac_result = acInit();
    if (R_SUCCEEDED(ac_result)) ac_ready = true;

    soc_buffer = (u32 *)memalign(SOC_ALIGN, SOC_BUFFER_SIZE);
    if (!soc_buffer) {
        set_error(error, error_size, "无法分配网络套接字缓冲区");
        cleanup_services();
        return -1;
    }
    Result result = socInit(soc_buffer, SOC_BUFFER_SIZE);
    if (R_FAILED(result)) {
        set_error(error, error_size, "网络服务初始化失败：%08lX",
                  (unsigned long)result);
        cleanup_services();
        return -1;
    }
    soc_ready = true;

    CURLcode code = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (code != CURLE_OK) {
        set_error(error, error_size, "curl 初始化失败：%s",
                  curl_easy_strerror(code));
        cleanup_services();
        return -1;
    }
    curl_ready = true;
    net_ready = true;
    return 0;
}

void net_exit(void) {
    cleanup_services();
}

int net_wifi_status(bool *connected) {
    if (!connected || !ac_ready) return -1;
    u32 status = 0;
    Result result = ACU_GetWifiStatus(&status);
    if (R_FAILED(result)) return -1;
    *connected = status != 0;
    return 0;
}

static CURLcode configure_https(CURL *curl, const char *url,
                                long connect_timeout_seconds,
                                long timeout_seconds, char *curl_error) {
    CURLcode code;
#define SETOPT(option, value)                         \
    do {                                              \
        code = curl_easy_setopt(curl, option, value); \
        if (code != CURLE_OK) return code;            \
    } while (0)

    if (!url || strncmp(url, "https://", 8) != 0) return CURLE_UNSUPPORTED_PROTOCOL;
    SETOPT(CURLOPT_ERRORBUFFER, curl_error);
    SETOPT(CURLOPT_URL, url);
    SETOPT(CURLOPT_USERAGENT,
           "NeteaseMusic/9.0.90 (Nintendo 3DS; ClouDS-Music/0.2)");
    SETOPT(CURLOPT_CAINFO, CA_BUNDLE_PATH);
    SETOPT(CURLOPT_SSL_VERIFYPEER, 1L);
    SETOPT(CURLOPT_SSL_VERIFYHOST, 2L);
    SETOPT(CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    SETOPT(CURLOPT_PROTOCOLS_STR, "https");
    SETOPT(CURLOPT_REDIR_PROTOCOLS_STR, "https");
    SETOPT(CURLOPT_FOLLOWLOCATION, 1L);
    SETOPT(CURLOPT_MAXREDIRS, HTTP_REDIRECTS);
    SETOPT(CURLOPT_CONNECTTIMEOUT, connect_timeout_seconds);
    SETOPT(CURLOPT_TIMEOUT, timeout_seconds);
    SETOPT(CURLOPT_NOSIGNAL, 1L);
    SETOPT(CURLOPT_TCP_KEEPALIVE, 1L);
    SETOPT(CURLOPT_ACCEPT_ENCODING, "identity");

#undef SETOPT
    return CURLE_OK;
}

static size_t memory_write(char *data, size_t size, size_t count, void *userdata) {
    MemorySink *sink = (MemorySink *)userdata;
    if (size != 0 && count > SIZE_MAX / size) return 0;
    size_t bytes = size * count;
    if (bytes > HTTP_MAX_RESPONSE - sink->size) {
        sink->overflow = true;
        return 0;
    }
    size_t required = sink->size + bytes + 1;
    if (required > sink->capacity) {
        size_t capacity = sink->capacity ? sink->capacity : 16384;
        const size_t maximum = HTTP_MAX_RESPONSE + 1U;
        while (capacity < required) {
            if (capacity > maximum / 2U) {
                capacity = maximum;
                break;
            }
            capacity *= 2U;
        }
        uint8_t *grown = (uint8_t *)realloc(sink->data, capacity);
        if (!grown) return 0;
        sink->data = grown;
        sink->capacity = capacity;
    }
    memcpy(sink->data + sink->size, data, bytes);
    sink->size += bytes;
    sink->data[sink->size] = 0;
    return bytes;
}

static size_t response_stream_write(char *data, size_t size, size_t count,
                                    void *userdata) {
    ResponseStreamSink *sink = (ResponseStreamSink *)userdata;
    if (!sink || (size != 0 && count > SIZE_MAX / size)) return 0;
    if (sink->write_failed || sink->reset_failed) return 0;
    size_t bytes = size * count;
    if ((uint64_t)bytes > sink->max_bytes - sink->received) {
        sink->size_exceeded = true;
        return 0;
    }
    if (bytes && sink->write((const uint8_t *)data, bytes,
                             sink->userdata) != 0) {
        sink->write_failed = true;
        return 0;
    }
    sink->received += bytes;
    return bytes;
}

static bool curl_certificate_failure(CURLcode code) {
    return code == CURLE_PEER_FAILED_VERIFICATION ||
           code == CURLE_SSL_CACERT_BADFILE ||
           code == CURLE_SSL_ISSUER_ERROR;
}

static void curl_failure(CURLcode code, const char *details, bool retried,
                         char *error, size_t error_size) {
    if (code == CURLE_ABORTED_BY_CALLBACK)
        set_error(error, error_size, "请求已取消");
    else if (curl_certificate_failure(code) && details && details[0])
        set_error(error, error_size, retried ?
                  "证书校验失败；请校准日期/时间或更新应用（已重试 1 次）：%.96s" :
                  "证书校验失败；请校准日期/时间或更新应用：%.112s",
                  details);
    else if (curl_certificate_failure(code))
        set_error(error, error_size, retried ?
                  "证书校验失败；请校准日期/时间或更新应用（已重试 1 次）" :
                  "证书校验失败；请校准日期/时间或更新应用");
    else if (retried && details && details[0])
        set_error(error, error_size,
                  "HTTPS 请求失败（已重试 1 次）：%.120s", details);
    else if (retried)
        set_error(error, error_size,
                  "HTTPS 请求失败（已重试 1 次）：%s",
                  curl_easy_strerror(code));
    else if (details && details[0])
        set_error(error, error_size, "HTTPS 请求失败：%.140s", details);
    else
        set_error(error, error_size, "HTTPS 请求失败：%s",
                  curl_easy_strerror(code));
}

static NetErrorKind curl_failure_kind(CURLcode code) {
    if (curl_certificate_failure(code)) return NET_ERROR_TLS_VERIFY;
    switch (code) {
        case CURLE_OK: return NET_ERROR_NONE;
        case CURLE_ABORTED_BY_CALLBACK: return NET_ERROR_CANCELLED;
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_PARTIAL_FILE:
        case CURLE_OPERATION_TIMEDOUT:
        case CURLE_GOT_NOTHING:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
        case CURLE_SSL_CONNECT_ERROR:
            return NET_ERROR_TRANSPORT;
        default:
            return NET_ERROR_OTHER;
    }
}

static bool retryable_http_status(long status) {
    return status == 408 || status == 429 || status == 500 ||
           status == 502 || status == 503 || status == 504;
}

static NetworkRequestFailure request_failure(CURLcode code, long status,
                                             bool retry_http_status) {
    if (retry_http_status && retryable_http_status(status))
        return NETWORK_REQUEST_FAILURE_TRANSIENT;
    if (code == CURLE_OK) return NETWORK_REQUEST_FAILURE_NONE;
    if (code == CURLE_ABORTED_BY_CALLBACK)
        return NETWORK_REQUEST_FAILURE_CANCELLED;
    if (code == CURLE_PEER_FAILED_VERIFICATION)
        return NETWORK_REQUEST_FAILURE_TLS_VERIFY;
    switch (code) {
        case CURLE_COULDNT_RESOLVE_PROXY:
        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_PARTIAL_FILE:
        case CURLE_OPERATION_TIMEDOUT:
        case CURLE_GOT_NOTHING:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
        case CURLE_SSL_CONNECT_ERROR:
            return NETWORK_REQUEST_FAILURE_TRANSIENT;
        default:
            return NETWORK_REQUEST_FAILURE_PERMANENT;
    }
}

static bool wait_before_retry(NetCancel cancel, void *cancel_userdata) {
    int64_t remaining = REQUEST_RETRY_DELAY_NS;
    while (remaining > 0) {
        if (cancel && cancel(cancel_userdata)) return false;
        int64_t interval = remaining < REQUEST_RETRY_POLL_NS ?
                           remaining : REQUEST_RETRY_POLL_NS;
        svcSleepThread(interval);
        remaining -= interval;
    }
    return !cancel || !cancel(cancel_userdata);
}

typedef void (*RetryReset)(void *userdata);

static CURLcode perform_with_single_retry(
    CURL *curl, char curl_error[CURL_ERROR_SIZE], bool retry_http_status,
    NetCancel cancel, void *cancel_userdata,
    RetryReset reset, void *reset_userdata,
    long *response_status, bool *retried) {
    if (response_status) *response_status = 0;
    if (retried) *retried = false;
    curl_error[0] = '\0';
    CURLcode code = curl_easy_perform(curl);
    long status = 0;
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    NetworkRequestFailure failure = request_failure(
        code, status, retry_http_status);
    if (network_request_retry_allowed(failure, 1U, false)) {
        if (!wait_before_retry(cancel, cancel_userdata)) {
            code = CURLE_ABORTED_BY_CALLBACK;
            status = 0;
        } else {
            if (reset) reset(reset_userdata);
            curl_error[0] = '\0';
            if (retried) *retried = true;
            code = curl_easy_perform(curl);
            status = 0;
            (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        }
    }
    if (response_status) *response_status = status;
    return code;
}

static void set_failure(NetErrorKind *failure, NetErrorKind value) {
    if (failure) *failure = value;
}

static int cancel_progress(void *userdata, curl_off_t download_total,
                           curl_off_t downloaded, curl_off_t upload_total,
                           curl_off_t uploaded) {
    (void)download_total;
    (void)downloaded;
    (void)upload_total;
    (void)uploaded;
    CancelSink *sink = (CancelSink *)userdata;
    return sink && sink->cancel ? sink->cancel(sink->userdata) : 0;
}

static CURLcode configure_cancel(CURL *curl, CancelSink *sink) {
    if (!sink || !sink->cancel) return CURLE_OK;
    CURLcode code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
                                cancel_progress);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_XFERINFODATA, sink);
    return code;
}

int net_probe_https_controlled(const char *url,
                               NetCancel cancel, void *cancel_userdata,
                               NetErrorKind *failure,
                               char *error, size_t error_size) {
    set_failure(failure, NET_ERROR_NONE);
    if (!net_ready || !url) {
        set_failure(failure, !net_ready ? NET_ERROR_TRANSPORT : NET_ERROR_OTHER);
        set_error(error, error_size, "网络尚未就绪");
        return -1;
    }
    CURL *curl = curl_easy_init();
    if (!curl) {
        set_failure(failure, NET_ERROR_OTHER);
        set_error(error, error_size, "无法创建 HTTPS 探测任务");
        return -1;
    }
    char curl_error[CURL_ERROR_SIZE] = {0};
    CURLcode code = configure_https(curl, url, 5L, 5L, curl_error);
    CancelSink cancel_sink = {cancel, cancel_userdata};
    /* A HEAD response of any HTTP status proves DNS, TCP and TLS reachability
     * without downloading a page body or depending on an application API. */
    if (code == CURLE_OK) code = curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    if (code == CURLE_OK) code = configure_cancel(curl, &cancel_sink);
    long status = 0;
    bool retried = false;
    if (code == CURLE_OK)
        code = perform_with_single_retry(
            curl, curl_error, false, cancel, cancel_userdata,
            NULL, NULL, &status, &retried);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) {
        set_failure(failure, curl_failure_kind(code));
        curl_failure(code, curl_error, retried, error, error_size);
        return -1;
    }
    if (status <= 0) {
        set_failure(failure, NET_ERROR_TRANSPORT);
        set_error(error, error_size, "网络探测未收到 HTTP 响应");
        return -1;
    }
    return 0;
}

static size_t response_header(char *data, size_t size, size_t count,
                              void *userdata) {
    HeaderSink *sink = (HeaderSink *)userdata;
    if (size != 0 && count > SIZE_MAX / size) return 0;
    size_t bytes = size * count;
    static const char prefix[] = "Set-Cookie:";
    static const char name[] = "MUSIC_U=";
    if (!sink || !sink->cookie || sink->size == 0 ||
        bytes <= sizeof(prefix) - 1 ||
        strncasecmp(data, prefix, sizeof(prefix) - 1) != 0)
        return bytes;
    const char *start = data + sizeof(prefix) - 1;
    const char *end = data + bytes;
    while (start < end && (*start == ' ' || *start == '\t')) start++;
    if ((size_t)(end - start) <= sizeof(name) - 1 ||
        strncasecmp(start, name, sizeof(name) - 1) != 0)
        return bytes;
    const char *value_end = start;
    while (value_end < end && *value_end != ';' && *value_end != '\r' &&
           *value_end != '\n') value_end++;
    size_t length = (size_t)(value_end - start);
    if (length >= sink->size) length = sink->size - 1;
    memcpy(sink->cookie, start, length);
    sink->cookie[length] = '\0';
    return bytes;
}

static CURLcode append_header(struct curl_slist **headers, const char *value) {
    struct curl_slist *appended = curl_slist_append(*headers, value);
    if (!appended) return CURLE_OUT_OF_MEMORY;
    *headers = appended;
    return CURLE_OK;
}

typedef struct {
    MemorySink *sink;
    char *response_cookie;
    size_t cookie_size;
} MemoryRetryReset;

static void reset_memory_retry(void *userdata) {
    MemoryRetryReset *reset = (MemoryRetryReset *)userdata;
    if (!reset || !reset->sink) return;
    free(reset->sink->data);
    memset(reset->sink, 0, sizeof(*reset->sink));
    if (reset->response_cookie && reset->cookie_size)
        reset->response_cookie[0] = '\0';
}

static void reset_response_stream_retry(void *userdata) {
    ResponseStreamSink *sink = (ResponseStreamSink *)userdata;
    if (!sink) return;
    sink->received = 0;
    sink->write_failed = false;
    sink->size_exceeded = false;
    if (sink->reset && sink->reset(sink->userdata) != 0)
        sink->reset_failed = true;
}

int net_post_form(const char *url, const char *body,
                  const char *cookie, uint8_t **response,
                  size_t *response_size, char *error, size_t error_size) {
    return net_post_form_controlled(url, body, cookie, response, response_size,
                                    NULL, 0, NULL, NULL,
                                    error, error_size);
}

static int net_post_form_internal(
    const char *url, const char *body, const char *cookie,
    const char *referer, uint8_t **response, size_t *response_size,
    char *response_cookie, size_t cookie_size,
    NetCancel cancel, void *cancel_userdata,
    NetErrorKind *failure,
    char *error, size_t error_size) {
    set_failure(failure, NET_ERROR_NONE);
    if (!net_ready || !url || !body || !response || !response_size) {
        set_failure(failure, !net_ready ? NET_ERROR_TRANSPORT : NET_ERROR_OTHER);
        set_error(error, error_size, "网络尚未就绪");
        return -1;
    }
    *response = NULL;
    *response_size = 0;

    CURL *curl = curl_easy_init();
    if (!curl) {
        set_failure(failure, NET_ERROR_OTHER);
        set_error(error, error_size, "无法创建 HTTPS 请求");
        return -1;
    }
    char curl_error[CURL_ERROR_SIZE] = {0};
    CURLcode code = configure_https(curl, url, 15L, 30L, curl_error);
    MemorySink sink = {0};
    CancelSink cancel_sink = {cancel, cancel_userdata};
    HeaderSink header_sink = {response_cookie, cookie_size};
    if (response_cookie && cookie_size) response_cookie[0] = '\0';
    struct curl_slist *headers = NULL;
    if (code == CURLE_OK && referer)
        code = curl_easy_setopt(curl, CURLOPT_REFERER, referer);
    if (code == CURLE_OK)
        code = append_header(&headers,
            "Content-Type: application/x-www-form-urlencoded");
    if (code == CURLE_OK) code = append_header(&headers, "Accept: */*");
    if (code == CURLE_OK) code = append_header(&headers, "Connection: close");

    if (code == CURLE_OK) code = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if (code == CURLE_OK) code = curl_easy_setopt(curl, CURLOPT_POST, 1L);
    if (code == CURLE_OK) code = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                                (curl_off_t)strlen(body));
    if (code == CURLE_OK && cookie && cookie[0])
        code = curl_easy_setopt(curl, CURLOPT_COOKIE, cookie);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    if (code == CURLE_OK && response_cookie && cookie_size)
        code = curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, response_header);
    if (code == CURLE_OK && response_cookie && cookie_size)
        code = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_sink);
    if (code == CURLE_OK) code = configure_cancel(curl, &cancel_sink);
    long status = 0;
    bool retried = false;
    MemoryRetryReset retry_reset = {&sink, response_cookie, cookie_size};
    if (code == CURLE_OK)
        code = perform_with_single_retry(
            curl, curl_error, true, cancel, cancel_userdata,
            reset_memory_retry, &retry_reset, &status, &retried);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK) {
        set_failure(failure, curl_failure_kind(code));
        if (sink.overflow)
            set_error(error, error_size, "HTTPS 响应超过 2 MiB");
        else curl_failure(code, curl_error, retried, error, error_size);
        free(sink.data);
        return -1;
    }
    if (status != 200) {
        set_failure(failure, status == 401 || status == 403 ?
                             NET_ERROR_AUTH : NET_ERROR_HTTP);
        if (retried && retryable_http_status(status))
            set_error(error, error_size,
                      "服务器暂时不可用（HTTPS %ld，已重试 1 次）；请稍后重试",
                      status);
        else if (retried)
            set_error(error, error_size,
                      "HTTPS 状态码 %ld（已重试 1 次）", status);
        else set_error(error, error_size, "HTTPS 状态码 %ld", status);
        free(sink.data);
        return -1;
    }
    if (!sink.data) {
        sink.data = (uint8_t *)calloc(1, 1);
        if (!sink.data) {
            set_error(error, error_size, "内存不足");
            return -1;
        }
    }
    *response = sink.data;
    *response_size = sink.size;
    return 0;
}

int net_post_form_controlled(const char *url, const char *body,
                             const char *cookie, uint8_t **response,
                             size_t *response_size,
                             char *response_cookie, size_t cookie_size,
                             NetCancel cancel, void *cancel_userdata,
                             char *error, size_t error_size) {
    return net_post_form_controlled_ex(
        url, body, cookie, response, response_size,
        response_cookie, cookie_size, cancel, cancel_userdata, NULL,
        error, error_size);
}

int net_post_form_controlled_ex(
    const char *url, const char *body, const char *cookie,
    uint8_t **response, size_t *response_size,
    char *response_cookie, size_t cookie_size,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size) {
    return net_post_form_internal(
        url, body, cookie, NULL, response, response_size,
        response_cookie, cookie_size, cancel, cancel_userdata, failure,
        error, error_size);
}

int net_post_form_referer_controlled(
    const char *url, const char *body, const char *cookie,
    const char *referer, uint8_t **response, size_t *response_size,
    NetCancel cancel, void *cancel_userdata,
    char *error, size_t error_size) {
    return net_post_form_referer_controlled_ex(
        url, body, cookie, referer, response, response_size,
        cancel, cancel_userdata, NULL, error, error_size);
}

int net_post_form_referer_controlled_ex(
    const char *url, const char *body, const char *cookie,
    const char *referer, uint8_t **response, size_t *response_size,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size) {
    return net_post_form_internal(
        url, body, cookie, referer, response, response_size,
        NULL, 0, cancel, cancel_userdata, failure, error, error_size);
}

int net_post_form_stream_controlled_ex(
    const char *url, const char *body, const char *cookie,
    uint64_t max_response_bytes,
    NetResponseWrite write, NetResponseReset reset, void *stream_userdata,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size) {
    set_failure(failure, NET_ERROR_NONE);
    if (!net_ready || !url || !body || !write || max_response_bytes == 0 ||
        max_response_bytes > (uint64_t)INT64_MAX) {
        set_failure(failure, !net_ready ? NET_ERROR_TRANSPORT : NET_ERROR_OTHER);
        set_error(error, error_size, !net_ready ?
                  "网络尚未就绪" : "下载大小限制无效");
        return -1;
    }
    CURL *curl = curl_easy_init();
    if (!curl) {
        set_failure(failure, NET_ERROR_OTHER);
        set_error(error, error_size, "无法创建 HTTPS 请求");
        return -1;
    }
    char curl_error[CURL_ERROR_SIZE] = {0};
    CURLcode code = configure_https(curl, url, 15L, 60L, curl_error);
    CancelSink cancel_sink = {cancel, cancel_userdata};
    ResponseStreamSink sink = {
        .write = write,
        .reset = reset,
        .userdata = stream_userdata,
        .max_bytes = max_response_bytes,
    };
    struct curl_slist *headers = NULL;
    if (code == CURLE_OK)
        code = append_header(&headers,
            "Content-Type: application/x-www-form-urlencoded");
    if (code == CURLE_OK) code = append_header(&headers, "Accept: */*");
    if (code == CURLE_OK) code = append_header(&headers, "Connection: close");
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if (code == CURLE_OK) code = curl_easy_setopt(curl, CURLOPT_POST, 1L);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                                (curl_off_t)strlen(body));
    if (code == CURLE_OK && cookie && cookie[0])
        code = curl_easy_setopt(curl, CURLOPT_COOKIE, cookie);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE,
                                (curl_off_t)max_response_bytes);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                response_stream_write);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    if (code == CURLE_OK) code = configure_cancel(curl, &cancel_sink);
    long status = 0;
    bool retried = false;
    if (code == CURLE_OK)
        code = perform_with_single_retry(
            curl, curl_error, true, cancel, cancel_userdata,
            reset_response_stream_retry, &sink, &status, &retried);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK || sink.write_failed || sink.reset_failed) {
        if (code == CURLE_ABORTED_BY_CALLBACK)
            set_failure(failure, NET_ERROR_CANCELLED);
        else set_failure(failure, curl_failure_kind(code));
        if (sink.size_exceeded || code == CURLE_FILESIZE_EXCEEDED)
            set_error(error, error_size, "HTTPS 响应超过大小限制");
        else if ((sink.write_failed || sink.reset_failed) &&
                 (!error || error_size == 0 || !error[0]))
            set_error(error, error_size, "JSON 响应无效");
        else if (!sink.write_failed && !sink.reset_failed)
            curl_failure(code, curl_error, retried, error, error_size);
        return -1;
    }
    if (status != 200) {
        set_failure(failure, status == 401 || status == 403 ?
                             NET_ERROR_AUTH : NET_ERROR_HTTP);
        if (retried && retryable_http_status(status))
            set_error(error, error_size,
                      "服务器暂时不可用（HTTPS %ld，已重试 1 次）；请稍后重试",
                      status);
        else if (retried)
            set_error(error, error_size,
                      "HTTPS 状态码 %ld（已重试 1 次）", status);
        else set_error(error, error_size, "HTTPS 状态码 %ld", status);
        return -1;
    }
    return 0;
}

int net_get(const char *url, uint8_t **response, size_t *response_size,
            char *error, size_t error_size) {
    return net_get_controlled(url, response, response_size, NULL, NULL,
                              error, error_size);
}

int net_get_controlled(const char *url, uint8_t **response,
                       size_t *response_size,
                       NetCancel cancel, void *cancel_userdata,
                       char *error, size_t error_size) {
    return net_get_controlled_ex(url, response, response_size,
                                 cancel, cancel_userdata, NULL,
                                 error, error_size);
}

int net_get_controlled_ex(const char *url, uint8_t **response,
                          size_t *response_size,
                          NetCancel cancel, void *cancel_userdata,
                          NetErrorKind *failure,
                          char *error, size_t error_size) {
    set_failure(failure, NET_ERROR_NONE);
    if (!net_ready || !url || !response || !response_size) {
        set_failure(failure, !net_ready ? NET_ERROR_TRANSPORT : NET_ERROR_OTHER);
        set_error(error, error_size, "网络尚未就绪");
        return -1;
    }
    *response = NULL;
    *response_size = 0;
    CURL *curl = curl_easy_init();
    if (!curl) {
        set_failure(failure, NET_ERROR_OTHER);
        set_error(error, error_size, "无法创建 HTTPS 请求");
        return -1;
    }
    char curl_error[CURL_ERROR_SIZE] = {0};
    CURLcode code = configure_https(curl, url, 15L, 30L, curl_error);
    MemorySink sink = {0};
    CancelSink cancel_sink = {cancel, cancel_userdata};
    if (code == CURLE_OK) code = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, memory_write);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    if (code == CURLE_OK) code = configure_cancel(curl, &cancel_sink);
    long status = 0;
    bool retried = false;
    MemoryRetryReset retry_reset = {&sink, NULL, 0};
    if (code == CURLE_OK)
        code = perform_with_single_retry(
            curl, curl_error, true, cancel, cancel_userdata,
            reset_memory_retry, &retry_reset, &status, &retried);
    curl_easy_cleanup(curl);
    if (code != CURLE_OK) {
        set_failure(failure, curl_failure_kind(code));
        if (sink.overflow)
            set_error(error, error_size, "HTTPS 响应超过 2 MiB");
        else curl_failure(code, curl_error, retried, error, error_size);
        free(sink.data);
        return -1;
    }
    if (status != 200) {
        set_failure(failure, status == 401 || status == 403 ?
                             NET_ERROR_AUTH : NET_ERROR_HTTP);
        if (retried && retryable_http_status(status))
            set_error(error, error_size,
                      "服务器暂时不可用（HTTPS %ld，已重试 1 次）；请稍后重试",
                      status);
        else if (retried)
            set_error(error, error_size,
                      "HTTPS 状态码 %ld（已重试 1 次）", status);
        else set_error(error, error_size, "HTTPS 状态码 %ld", status);
        free(sink.data);
        return -1;
    }
    if (!sink.data) {
        sink.data = (uint8_t *)calloc(1, 1);
        if (!sink.data) {
            set_error(error, error_size, "内存不足");
            return -1;
        }
    }
    *response = sink.data;
    *response_size = sink.size;
    return 0;
}

static int secure_audio_url(const char *input, char *output, size_t output_size) {
    if (!input || !output || output_size == 0) return -1;
    int written;
    if (strncmp(input, "https://", 8) == 0)
        written = snprintf(output, output_size, "%s", input);
    else if (strncmp(input, "http://", 7) == 0)
        written = snprintf(output, output_size, "https://%s", input + 7);
    else return -1;
    return written >= 0 && (size_t)written < output_size ? 0 : -1;
}

static int sdmc_free_bytes(uint64_t *free_bytes) {
    if (!free_bytes) return -1;
    struct statvfs info;
    if (statvfs("sdmc:/", &info) != 0) return -1;
    uint64_t block_size = info.f_frsize ? (uint64_t)info.f_frsize :
                                          (uint64_t)info.f_bsize;
    uint64_t available = (uint64_t)info.f_bavail;
    if (block_size == 0 || available > UINT64_MAX / block_size) return -1;
    *free_bytes = available * block_size;
    return 0;
}

static bool file_limit_rejected(FileSink *sink, uint64_t received,
                                uint64_t incoming) {
    DownloadLimitResult result = download_limit_check(
        received, incoming, sink->max_bytes, sink->space_budget);
    if (result == DOWNLOAD_LIMIT_SIZE || result == DOWNLOAD_LIMIT_INVALID)
        sink->size_exceeded = true;
    else if (result == DOWNLOAD_LIMIT_SPACE)
        sink->space_exceeded = true;
    return result != DOWNLOAD_LIMIT_OK;
}

static size_t file_write(char *data, size_t size, size_t count, void *userdata) {
    FileSink *sink = (FileSink *)userdata;
    if (size != 0 && count > SIZE_MAX / size) return 0;
    size_t bytes = size * count;
    if (file_limit_rejected(sink, sink->received, bytes)) return 0;
    if (bytes && fwrite(data, 1, bytes, sink->file) != bytes) {
        sink->write_failed = true;
        return 0;
    }
    sink->received += bytes;
    if (sink->publish &&
        sink->received - sink->published >= FILE_PUBLISH_BYTES) {
        if (fflush(sink->file) != 0) {
            sink->write_failed = true;
            return 0;
        }
        sink->published = sink->received;
        sink->publish(sink->published, sink->total, sink->publish_userdata);
    }
    return bytes;
}

static int transfer_progress(void *userdata, curl_off_t download_total,
                             curl_off_t downloaded, curl_off_t upload_total,
                             curl_off_t uploaded) {
    (void)upload_total;
    (void)uploaded;
    FileSink *sink = (FileSink *)userdata;
    if (sink->cancel && sink->cancel(sink->cancel_userdata)) return 1;
    sink->total = download_total > 0 ? (uint64_t)download_total : 0;
    uint64_t current = downloaded > 0 ? (uint64_t)downloaded : sink->received;
    if ((sink->total && file_limit_rejected(sink, 0, sink->total)) ||
        file_limit_rejected(sink, 0, current))
        return 1;
    if (sink->progress) sink->progress(current, sink->total, sink->userdata);
    return 0;
}

int net_download_file(const char *url, const char *path, uint64_t max_bytes,
                      NetProgress progress, void *userdata,
                      char *error, size_t error_size) {
    return net_download_file_controlled(url, path, max_bytes, progress, userdata,
                                        NULL, NULL, error, error_size);
}

static int download_file_controlled(
    const char *url, const char *path, uint64_t max_bytes, bool finalize,
    NetProgress progress, void *progress_userdata,
    NetPublish publish, void *publish_userdata,
    NetCancel cancel, void *cancel_userdata,
    NetErrorKind *failure,
    char *error, size_t error_size);

int net_download_file_controlled(const char *url, const char *path,
                                 uint64_t max_bytes,
                                 NetProgress progress, void *userdata,
                                 NetCancel cancel, void *cancel_userdata,
                                 char *error, size_t error_size) {
    return net_download_file_controlled_ex(
        url, path, max_bytes, progress, userdata,
        cancel, cancel_userdata, NULL,
        error, error_size);
}

int net_download_file_controlled_ex(
    const char *url, const char *path, uint64_t max_bytes,
    NetProgress progress, void *userdata,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size) {
    return download_file_controlled(url, path, max_bytes, true,
                                    progress, userdata,
                                    NULL, NULL, cancel, cancel_userdata, failure,
                                    error, error_size);
}

static int download_file_controlled(
    const char *url, const char *path, uint64_t max_bytes, bool finalize,
    NetProgress progress, void *progress_userdata,
    NetPublish publish, void *publish_userdata,
    NetCancel cancel, void *cancel_userdata,
    NetErrorKind *failure,
    char *error, size_t error_size) {
    set_failure(failure, NET_ERROR_NONE);
    if (!net_ready || !url || !path || max_bytes == 0 ||
        max_bytes > (uint64_t)INT64_MAX) {
        set_failure(failure, !net_ready ? NET_ERROR_TRANSPORT : NET_ERROR_OTHER);
        set_error(error, error_size, !net_ready ?
                  "网络尚未就绪" : "下载大小限制无效");
        return -1;
    }
    char secure_url[2048];
    if (secure_audio_url(url, secure_url, sizeof(secure_url)) != 0) {
        set_failure(failure, NET_ERROR_OTHER);
        set_error(error, error_size, "媒体地址不是 HTTP(S) URL");
        return -1;
    }
    char temporary[512];
    int written = snprintf(temporary, sizeof(temporary), "%s.part", path);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        set_error(error, error_size, "缓存路径过长");
        return -1;
    }
    (void)remove(temporary);
    uint64_t free_bytes = 0;
    if (sdmc_free_bytes(&free_bytes) != 0) {
        set_failure(failure, NET_ERROR_OTHER);
        set_error(error, error_size, "无法检查 SD 卡剩余空间");
        return -1;
    }
    uint64_t space_budget = download_space_budget(
        free_bytes, NET_DOWNLOAD_STORAGE_RESERVE_BYTES);
    if (space_budget == 0) {
        set_failure(failure, NET_ERROR_OTHER);
        set_error(error, error_size, "SD 卡空间不足（需保留 %llu MiB）",
                  (unsigned long long)(NET_DOWNLOAD_STORAGE_RESERVE_BYTES /
                                      NET_DOWNLOAD_MIB));
        return -1;
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        set_error(error, error_size, "无法创建缓存文件");
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        set_failure(failure, NET_ERROR_OTHER);
        fclose(file);
        remove(temporary);
        set_error(error, error_size, "无法创建 HTTPS 下载任务");
        return -1;
    }
    char curl_error[CURL_ERROR_SIZE] = {0};
    CURLcode code = configure_https(curl, secure_url, 15L, 600L, curl_error);
    FileSink sink = {
        .file = file,
        .progress = progress,
        .userdata = progress_userdata,
        .publish = publish,
        .publish_userdata = publish_userdata,
        .cancel = cancel,
        .cancel_userdata = cancel_userdata,
        .max_bytes = max_bytes,
        .space_budget = space_budget,
    };
    if (code == CURLE_OK) code = curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    if (code == CURLE_OK) code = curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE,
                                (curl_off_t)max_bytes);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_write);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, transfer_progress);
    if (code == CURLE_OK)
        code = curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &sink);
    long status = 0;
    bool retried = false;
    if (code == CURLE_OK) {
        curl_error[0] = '\0';
        code = curl_easy_perform(curl);
        (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        NetworkRequestFailure request = request_failure(code, status, true);
        bool published = sink.publish && sink.published > 0;
        bool local_failure = sink.write_failed || sink.size_exceeded ||
                             sink.space_exceeded;
        if (!local_failure && network_request_retry_allowed(
                request, 1U, published)) {
            if (!wait_before_retry(cancel, cancel_userdata)) {
                code = CURLE_ABORTED_BY_CALLBACK;
                status = 0;
            } else {
                int retry_close = fclose(file);
                file = NULL;
                if (retry_close == 0) file = fopen(temporary, "wb");
                if (!file) {
                    sink.write_failed = true;
                    code = CURLE_WRITE_ERROR;
                    status = 0;
                } else {
                    sink.file = file;
                    sink.received = 0;
                    sink.total = 0;
                    sink.published = 0;
                    curl_error[0] = '\0';
                    retried = true;
                    code = curl_easy_perform(curl);
                    status = 0;
                    (void)curl_easy_getinfo(
                        curl, CURLINFO_RESPONSE_CODE, &status);
                }
            }
        }
    }
    if (code == CURLE_FILESIZE_EXCEEDED) sink.size_exceeded = true;
    curl_easy_cleanup(curl);
    if (code == CURLE_OK && !sink.write_failed &&
        sink.received > sink.published) {
        if (fflush(file) != 0) sink.write_failed = true;
        else {
            sink.published = sink.received;
            if (sink.publish)
                sink.publish(sink.published, sink.total,
                             sink.publish_userdata);
        }
    }
    int close_result = file ? fclose(file) : -1;
    if (code != CURLE_OK || close_result != 0 || sink.write_failed) {
        remove(temporary);
        if (sink.size_exceeded) {
            set_failure(failure, NET_ERROR_OTHER);
            set_error(error, error_size,
                      "下载文件超过大小限制（最大 %llu MiB）",
                      (unsigned long long)(max_bytes / NET_DOWNLOAD_MIB));
        } else if (sink.space_exceeded) {
            set_failure(failure, NET_ERROR_OTHER);
            set_error(error, error_size,
                      "SD 卡空间不足（需保留 %llu MiB）",
                      (unsigned long long)(NET_DOWNLOAD_STORAGE_RESERVE_BYTES /
                                          NET_DOWNLOAD_MIB));
        } else if (sink.write_failed || close_result != 0)
            set_error(error, error_size, "无法写入媒体缓存");
        else if (status && status != 200) {
            set_failure(failure, NET_ERROR_HTTP);
            if (retried && retryable_http_status(status))
                set_error(error, error_size,
                          "媒体服务器暂时不可用（HTTPS %ld，已重试 1 次）；请稍后重试",
                          status);
            else if (retried)
                set_error(error, error_size,
                          "媒体请求的 HTTPS 状态码为 %ld（已重试 1 次）",
                          status);
            else set_error(error, error_size,
                           "媒体请求的 HTTPS 状态码为 %ld", status);
        } else {
            set_failure(failure, curl_failure_kind(code));
            curl_failure(code, curl_error, retried, error, error_size);
        }
        return -1;
    }
    if (status != 200) {
        set_failure(failure, NET_ERROR_HTTP);
        remove(temporary);
        if (retried && retryable_http_status(status))
            set_error(error, error_size,
                      "媒体服务器暂时不可用（HTTPS %ld，已重试 1 次）；请稍后重试",
                      status);
        else if (retried)
            set_error(error, error_size,
                      "媒体请求的 HTTPS 状态码为 %ld（已重试 1 次）",
                      status);
        else set_error(error, error_size,
                       "媒体请求的 HTTPS 状态码为 %ld", status);
        return -1;
    }
    if (finalize) {
        remove(path);
        if (rename(temporary, path) != 0) {
            remove(temporary);
            set_error(error, error_size, "无法提交缓存文件");
            return -1;
        }
    }
    if (progress) progress(sink.received, sink.total, progress_userdata);
    return 0;
}

int net_download_file_part_controlled(
    const char *url, const char *path, uint64_t max_bytes,
    NetProgress progress, void *progress_userdata,
    NetPublish publish, void *publish_userdata,
    NetCancel cancel, void *cancel_userdata,
    char *error, size_t error_size) {
    return net_download_file_part_controlled_ex(
        url, path, max_bytes, progress, progress_userdata,
        publish, publish_userdata, cancel, cancel_userdata, NULL,
        error, error_size);
}

int net_download_file_part_controlled_ex(
    const char *url, const char *path, uint64_t max_bytes,
    NetProgress progress, void *progress_userdata,
    NetPublish publish, void *publish_userdata,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size) {
    return download_file_controlled(url, path, max_bytes, false,
                                    progress, progress_userdata,
                                    publish, publish_userdata,
                                    cancel, cancel_userdata, failure,
                                    error, error_size);
}
