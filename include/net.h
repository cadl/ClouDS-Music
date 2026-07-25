#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NET_DOWNLOAD_MIB (1024ULL * 1024ULL)
#define NET_DOWNLOAD_COVER_MAX_BYTES (2ULL * NET_DOWNLOAD_MIB)
#define NET_DOWNLOAD_STORAGE_RESERVE_BYTES (64ULL * NET_DOWNLOAD_MIB)

typedef enum {
    NET_ERROR_NONE = 0,
    NET_ERROR_CANCELLED,
    NET_ERROR_TRANSPORT,
    NET_ERROR_TLS_VERIFY,
    NET_ERROR_AUTH,
    NET_ERROR_HTTP,
    NET_ERROR_OTHER
} NetErrorKind;

static inline bool net_error_is_transport(NetErrorKind failure) {
    return failure == NET_ERROR_TRANSPORT ||
           failure == NET_ERROR_TLS_VERIFY;
}

typedef void (*NetProgress)(uint64_t received, uint64_t total, void *userdata);
typedef void (*NetPublish)(uint64_t published, uint64_t total, void *userdata);
typedef int (*NetCancel)(void *userdata);
typedef int (*NetResponseWrite)(const uint8_t *data, size_t size,
                                void *userdata);
typedef int (*NetResponseReset)(void *userdata);

int net_init(char *error, size_t error_size);
void net_exit(void);
int net_wifi_status(bool *connected);
int net_probe_https_controlled(const char *url,
                               NetCancel cancel, void *cancel_userdata,
                               NetErrorKind *failure,
                               char *error, size_t error_size);

int net_post_form(const char *url, const char *body,
                  const char *cookie, uint8_t **response,
                  size_t *response_size, char *error, size_t error_size);
int net_post_form_controlled(const char *url, const char *body,
                             const char *cookie, uint8_t **response,
                             size_t *response_size,
                             char *response_cookie, size_t cookie_size,
                             NetCancel cancel, void *cancel_userdata,
                             char *error, size_t error_size);
int net_post_form_controlled_ex(
    const char *url, const char *body, const char *cookie,
    uint8_t **response, size_t *response_size,
    char *response_cookie, size_t cookie_size,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size);
int net_post_form_referer_controlled(
    const char *url, const char *body, const char *cookie,
    const char *referer, uint8_t **response, size_t *response_size,
    NetCancel cancel, void *cancel_userdata,
    char *error, size_t error_size);
int net_post_form_referer_controlled_ex(
    const char *url, const char *body, const char *cookie,
    const char *referer, uint8_t **response, size_t *response_size,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size);
int net_post_form_stream_controlled_ex(
    const char *url, const char *body, const char *cookie,
    uint64_t max_response_bytes,
    NetResponseWrite write, NetResponseReset reset, void *stream_userdata,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size);

int net_get(const char *url, uint8_t **response, size_t *response_size,
            char *error, size_t error_size);
int net_get_controlled(const char *url, uint8_t **response,
                       size_t *response_size,
                       NetCancel cancel, void *cancel_userdata,
                       char *error, size_t error_size);
int net_get_controlled_ex(const char *url, uint8_t **response,
                          size_t *response_size,
                          NetCancel cancel, void *cancel_userdata,
                          NetErrorKind *failure,
                          char *error, size_t error_size);

int net_download_file(const char *url, const char *path, uint64_t max_bytes,
                      NetProgress progress, void *userdata,
                      char *error, size_t error_size);
int net_download_file_controlled(const char *url, const char *path,
                                 uint64_t max_bytes,
                                 NetProgress progress, void *userdata,
                                 NetCancel cancel, void *cancel_userdata,
                                 char *error, size_t error_size);
int net_download_file_controlled_ex(
    const char *url, const char *path, uint64_t max_bytes,
    NetProgress progress, void *userdata,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size);
int net_download_file_part_controlled(
    const char *url, const char *path, uint64_t max_bytes,
    NetProgress progress, void *progress_userdata,
    NetPublish publish, void *publish_userdata,
    NetCancel cancel, void *cancel_userdata,
    char *error, size_t error_size);
int net_download_file_part_controlled_ex(
    const char *url, const char *path, uint64_t max_bytes,
    NetProgress progress, void *progress_userdata,
    NetPublish publish, void *publish_userdata,
    NetCancel cancel, void *cancel_userdata, NetErrorKind *failure,
    char *error, size_t error_size);
