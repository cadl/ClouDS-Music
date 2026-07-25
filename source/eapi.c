#include "eapi.h"

#include "i18n.h"

#include <mbedtls/aes.h>
#include <mbedtls/md5.h>
#include <mbedtls/version.h>
#include <zlib.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EAPI_SEPARATOR "-36cd479b6b5-"
#define EAPI_MAX_JSON (2U * 1024U * 1024U)
#define EAPI_STREAM_PROBE_SIZE 64U
#define EAPI_STREAM_OUTPUT_SIZE (16U * 1024U)

static const unsigned char EAPI_KEY[] = "e82ckenh8dichen8";

typedef enum {
    EAPI_STREAM_UNKNOWN = 0,
    EAPI_STREAM_PLAIN,
    EAPI_STREAM_ENCRYPTED
} EapiStreamMode;

typedef enum {
    EAPI_PAYLOAD_UNKNOWN = 0,
    EAPI_PAYLOAD_PLAIN,
    EAPI_PAYLOAD_GZIP
} EapiPayloadMode;

struct EapiStreamDecoder {
    EapiStreamWrite write;
    void *userdata;
    size_t max_decoded_bytes;
    size_t decoded_bytes;
    EapiStreamMode mode;
    EapiPayloadMode payload_mode;
    uint8_t probe[EAPI_STREAM_PROBE_SIZE];
    size_t probe_size;
    uint8_t cipher_block[16];
    size_t cipher_size;
    uint8_t pending_plain[16];
    bool has_pending_plain;
    uint8_t payload_probe[2];
    size_t payload_probe_size;
    mbedtls_aes_context aes;
    bool aes_ready;
    z_stream inflate;
    bool inflate_ready;
    bool inflate_finished;
    uint8_t output[EAPI_STREAM_OUTPUT_SIZE];
};

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static void set_error_if_empty(char *error, size_t size,
                               const char *format, ...) {
    if (!error || size == 0 || error[0]) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static int md5_digest(const unsigned char *data, size_t size,
                      unsigned char output[16]) {
#if MBEDTLS_VERSION_MAJOR >= 3
    return mbedtls_md5(data, size, output);
#else
    return mbedtls_md5_ret(data, size, output);
#endif
}

int eapi_build_form(const char *api_path, const char *json,
                    char **form, char *error, size_t error_size) {
    if (!api_path || !json || !form) {
        set_error(error, error_size, "EAPI 请求无效");
        return -1;
    }
    *form = NULL;
    size_t path_len = strlen(api_path);
    size_t json_len = strlen(json);
    const char *prefix = "nobody";
    const char *middle = "use";
    const char *suffix = "md5forencrypt";
    size_t message_len = strlen(prefix) + path_len + strlen(middle) +
                         json_len + strlen(suffix);
    char *message = (char *)malloc(message_len + 1);
    if (!message) goto no_memory;
    snprintf(message, message_len + 1, "%s%s%s%s%s",
             prefix, api_path, middle, json, suffix);

    unsigned char digest[16];
    if (md5_digest((const unsigned char *)message, message_len, digest) != 0) {
        free(message);
        set_error(error, error_size, "MD5 计算失败");
        return -1;
    }
    free(message);
    char digest_hex[33];
    for (int i = 0; i < 16; i++)
        snprintf(digest_hex + i * 2, 3, "%02x", digest[i]);

    size_t plain_len = path_len + strlen(EAPI_SEPARATOR) + json_len +
                       strlen(EAPI_SEPARATOR) + sizeof(digest_hex) - 1;
    char *plain = (char *)malloc(plain_len + 17);
    if (!plain) goto no_memory;
    snprintf(plain, plain_len + 1, "%s%s%s%s%s", api_path, EAPI_SEPARATOR,
             json, EAPI_SEPARATOR, digest_hex);
    size_t padding = 16 - (plain_len % 16);
    memset(plain + plain_len, (int)padding, padding);
    size_t encrypted_len = plain_len + padding;

    unsigned char *encrypted = (unsigned char *)malloc(encrypted_len);
    if (!encrypted) {
        free(plain);
        goto no_memory;
    }
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int result = mbedtls_aes_setkey_enc(&aes, EAPI_KEY, 128);
    for (size_t offset = 0; result == 0 && offset < encrypted_len; offset += 16)
        result = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT,
                                       (unsigned char *)plain + offset,
                                       encrypted + offset);
    mbedtls_aes_free(&aes);
    free(plain);
    if (result != 0) {
        free(encrypted);
        set_error(error, error_size, "AES 加密失败：%d", result);
        return -1;
    }

    size_t form_len = 7 + encrypted_len * 2;
    char *encoded = (char *)malloc(form_len + 1);
    if (!encoded) {
        free(encrypted);
        goto no_memory;
    }
    memcpy(encoded, "params=", 7);
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < encrypted_len; i++) {
        encoded[7 + i * 2] = hex[encrypted[i] >> 4];
        encoded[8 + i * 2] = hex[encrypted[i] & 0x0f];
    }
    encoded[form_len] = '\0';
    free(encrypted);
    *form = encoded;
    return 0;

no_memory:
    set_error(error, error_size, "内存不足");
    return -1;
}

static int inflate_gzip(const unsigned char *input, size_t input_size,
                        char **output, char *error, size_t error_size) {
    size_t capacity = 64 * 1024;
    unsigned char *buffer = (unsigned char *)malloc(capacity + 1);
    if (!buffer) {
        set_error(error, error_size, "内存不足");
        return -1;
    }
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef *)input;
    stream.avail_in = (uInt)input_size;
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        free(buffer);
        set_error(error, error_size, "gzip 初始化失败");
        return -1;
    }
    int zresult = Z_OK;
    while (zresult == Z_OK) {
        if ((size_t)stream.total_out == capacity) {
            if (capacity >= EAPI_MAX_JSON) {
                zresult = Z_MEM_ERROR;
                break;
            }
            capacity *= 2;
            unsigned char *grown = (unsigned char *)realloc(buffer, capacity + 1);
            if (!grown) {
                zresult = Z_MEM_ERROR;
                break;
            }
            buffer = grown;
        }
        stream.next_out = buffer + stream.total_out;
        stream.avail_out = (uInt)(capacity - (size_t)stream.total_out);
        zresult = inflate(&stream, Z_NO_FLUSH);
    }
    size_t written = (size_t)stream.total_out;
    inflateEnd(&stream);
    if (zresult != Z_STREAM_END) {
        free(buffer);
        set_error(error, error_size, "gzip 解码失败：%d", zresult);
        return -1;
    }
    buffer[written] = '\0';
    *output = (char *)buffer;
    return 0;
}

int eapi_decode_response(uint8_t *body, size_t body_size,
                         char **json, char *error, size_t error_size) {
    if (!body || body_size == 0 || !json) {
        set_error(error, error_size, "EAPI 响应为空");
        return -1;
    }
    *json = NULL;
    size_t first = 0;
    while (first < body_size && (body[first] == ' ' || body[first] == '\r' ||
           body[first] == '\n' || body[first] == '\t')) first++;
    if (first < body_size && (body[first] == '{' || body[first] == '[')) {
        size_t json_size = body_size - first;
        if (first) memmove(body, body + first, json_size);
        body[json_size] = '\0';
        *json = (char *)body;
        return 0;
    }
    if (body_size % 16 != 0) {
        set_error(error, error_size, "EAPI 响应异常（%lu 字节）",
                  (unsigned long)body_size);
        return -1;
    }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int result = mbedtls_aes_setkey_dec(&aes, EAPI_KEY, 128);
    unsigned char block[16];
    for (size_t offset = 0; result == 0 && offset < body_size; offset += 16) {
        result = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT,
                                       body + offset, block);
        if (result == 0) memcpy(body + offset, block, sizeof(block));
    }
    mbedtls_aes_free(&aes);
    if (result != 0) {
        set_error(error, error_size, "AES 解密失败：%d", result);
        return -1;
    }
    unsigned int padding = body[body_size - 1];
    if (padding == 0 || padding > 16 || padding > body_size) {
        set_error(error, error_size, "EAPI 填充无效");
        return -1;
    }
    for (unsigned int i = 0; i < padding; i++) {
        if (body[body_size - 1 - i] != padding) {
            set_error(error, error_size, "EAPI 填充无效");
            return -1;
        }
    }
    size_t plain_size = body_size - padding;
    if (plain_size >= 2 && body[0] == 0x1f && body[1] == 0x8b) {
        return inflate_gzip(body, plain_size, json, error, error_size);
    }
    body[plain_size] = '\0';
    if (plain_size == 0 || (body[0] != '{' && body[0] != '[')) {
        set_error(error, error_size, "EAPI 返回了非 JSON 数据");
        return -1;
    }
    *json = (char *)body;
    return 0;
}

static int stream_write_decoded(EapiStreamDecoder *decoder,
                                const uint8_t *data, size_t size,
                                char *error, size_t error_size) {
    if (size > decoder->max_decoded_bytes - decoder->decoded_bytes) {
        set_error(error, error_size, "JSON 响应过大");
        return -1;
    }
    decoder->decoded_bytes += size;
    if (size && decoder->write(data, size, decoder->userdata) != 0) {
        set_error_if_empty(error, error_size, "JSON 响应无效");
        return -1;
    }
    return 0;
}

static int stream_inflate(EapiStreamDecoder *decoder,
                          const uint8_t *data, size_t size,
                          char *error, size_t error_size) {
    if (decoder->inflate_finished && size != 0) {
        set_error(error, error_size, "JSON 响应无效");
        return -1;
    }
    decoder->inflate.next_in = (Bytef *)data;
    decoder->inflate.avail_in = (uInt)size;
    while (decoder->inflate.avail_in > 0) {
        uInt before = decoder->inflate.avail_in;
        decoder->inflate.next_out = decoder->output;
        decoder->inflate.avail_out = sizeof(decoder->output);
        int result = inflate(&decoder->inflate, Z_NO_FLUSH);
        size_t produced = sizeof(decoder->output) - decoder->inflate.avail_out;
        if (produced && stream_write_decoded(
                decoder, decoder->output, produced,
                error, error_size) != 0)
            return -1;
        if (result == Z_STREAM_END) {
            decoder->inflate_finished = true;
            if (decoder->inflate.avail_in != 0) {
                set_error(error, error_size, "JSON 响应无效");
                return -1;
            }
            return 0;
        }
        if (result != Z_OK ||
            (decoder->inflate.avail_in == before && produced == 0)) {
            set_error(error, error_size, "gzip 解码失败：%d", result);
            return -1;
        }
    }
    return 0;
}

static int stream_payload(EapiStreamDecoder *decoder,
                          const uint8_t *data, size_t size,
                          char *error, size_t error_size) {
    size_t offset = 0;
    if (decoder->payload_mode == EAPI_PAYLOAD_UNKNOWN) {
        while (offset < size && decoder->payload_probe_size < 2)
            decoder->payload_probe[decoder->payload_probe_size++] =
                data[offset++];
        if (decoder->payload_probe_size < 2) return 0;
        if (decoder->payload_probe[0] == 0x1f &&
            decoder->payload_probe[1] == 0x8b) {
            memset(&decoder->inflate, 0, sizeof(decoder->inflate));
            int result = inflateInit2(&decoder->inflate, 16 + MAX_WBITS);
            if (result != Z_OK) {
                set_error(error, error_size, "gzip 初始化失败");
                return -1;
            }
            decoder->inflate_ready = true;
            decoder->payload_mode = EAPI_PAYLOAD_GZIP;
            if (stream_inflate(decoder, decoder->payload_probe, 2,
                               error, error_size) != 0)
                return -1;
        } else {
            decoder->payload_mode = EAPI_PAYLOAD_PLAIN;
            if (stream_write_decoded(decoder, decoder->payload_probe, 2,
                                     error, error_size) != 0)
                return -1;
        }
    }
    if (offset == size) return 0;
    if (decoder->payload_mode == EAPI_PAYLOAD_GZIP)
        return stream_inflate(decoder, data + offset, size - offset,
                              error, error_size);
    return stream_write_decoded(decoder, data + offset, size - offset,
                                error, error_size);
}

static int stream_cipher(EapiStreamDecoder *decoder,
                         const uint8_t *data, size_t size,
                         char *error, size_t error_size) {
    size_t offset = 0;
    while (offset < size) {
        size_t copy = sizeof(decoder->cipher_block) - decoder->cipher_size;
        if (copy > size - offset) copy = size - offset;
        memcpy(decoder->cipher_block + decoder->cipher_size,
               data + offset, copy);
        decoder->cipher_size += copy;
        offset += copy;
        if (decoder->cipher_size != sizeof(decoder->cipher_block)) continue;
        uint8_t plain[16];
        int result = mbedtls_aes_crypt_ecb(
            &decoder->aes, MBEDTLS_AES_DECRYPT,
            decoder->cipher_block, plain);
        if (result != 0) {
            set_error(error, error_size, "AES 解密失败：%d", result);
            return -1;
        }
        decoder->cipher_size = 0;
        if (decoder->has_pending_plain && stream_payload(
                decoder, decoder->pending_plain,
                sizeof(decoder->pending_plain), error, error_size) != 0)
            return -1;
        memcpy(decoder->pending_plain, plain, sizeof(plain));
        decoder->has_pending_plain = true;
    }
    return 0;
}

EapiStreamDecoder *eapi_stream_decoder_create(
    size_t max_decoded_bytes, EapiStreamWrite write, void *userdata,
    char *error, size_t error_size) {
    if (max_decoded_bytes == 0 || !write) {
        set_error(error, error_size, "EAPI 响应为空");
        return NULL;
    }
    EapiStreamDecoder *decoder =
        (EapiStreamDecoder *)calloc(1, sizeof(*decoder));
    if (!decoder) {
        set_error(error, error_size, "内存不足");
        return NULL;
    }
    decoder->max_decoded_bytes = max_decoded_bytes;
    decoder->write = write;
    decoder->userdata = userdata;
    if (eapi_stream_decoder_reset(decoder, error, error_size) != 0) {
        if (decoder->inflate_ready) inflateEnd(&decoder->inflate);
        if (decoder->aes_ready) mbedtls_aes_free(&decoder->aes);
        free(decoder);
        return NULL;
    }
    return decoder;
}

int eapi_stream_decoder_reset(EapiStreamDecoder *decoder,
                              char *error, size_t error_size) {
    if (!decoder) {
        set_error(error, error_size, "EAPI 响应为空");
        return -1;
    }
    if (decoder->inflate_ready) inflateEnd(&decoder->inflate);
    if (decoder->aes_ready) mbedtls_aes_free(&decoder->aes);
    decoder->decoded_bytes = 0;
    decoder->mode = EAPI_STREAM_UNKNOWN;
    decoder->payload_mode = EAPI_PAYLOAD_UNKNOWN;
    decoder->probe_size = 0;
    decoder->cipher_size = 0;
    decoder->has_pending_plain = false;
    decoder->payload_probe_size = 0;
    decoder->inflate_ready = false;
    decoder->inflate_finished = false;
    memset(&decoder->inflate, 0, sizeof(decoder->inflate));
    mbedtls_aes_init(&decoder->aes);
    decoder->aes_ready = true;
    int result = mbedtls_aes_setkey_dec(&decoder->aes, EAPI_KEY, 128);
    if (result != 0) {
        set_error(error, error_size, "AES 解密失败：%d", result);
        return -1;
    }
    return 0;
}

static bool json_prefix(const uint8_t *data, size_t size) {
    size_t first = 0;
    while (first < size && (data[first] == ' ' || data[first] == '\r' ||
           data[first] == '\n' || data[first] == '\t'))
        first++;
    return first < size && (data[first] == '{' || data[first] == '[');
}

static bool whitespace_prefix(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (data[i] != ' ' && data[i] != '\r' &&
            data[i] != '\n' && data[i] != '\t')
            return false;
    }
    return true;
}

static int stream_choose_mode(EapiStreamDecoder *decoder, bool final,
                              char *error, size_t error_size) {
    if (decoder->mode != EAPI_STREAM_UNKNOWN) return 1;
    if (decoder->probe_size >= 16) {
        uint8_t first_plain[16];
        int result = mbedtls_aes_crypt_ecb(
            &decoder->aes, MBEDTLS_AES_DECRYPT,
            decoder->probe, first_plain);
        if (result != 0) {
            set_error(error, error_size, "AES 解密失败：%d", result);
            return -1;
        }
        if ((first_plain[0] == 0x1f && first_plain[1] == 0x8b) ||
            json_prefix(first_plain, sizeof(first_plain)))
            decoder->mode = EAPI_STREAM_ENCRYPTED;
    }
    if (decoder->mode == EAPI_STREAM_UNKNOWN &&
        !whitespace_prefix(decoder->probe, decoder->probe_size))
        decoder->mode = json_prefix(decoder->probe, decoder->probe_size) ?
                        EAPI_STREAM_PLAIN : EAPI_STREAM_ENCRYPTED;
    if (decoder->mode == EAPI_STREAM_UNKNOWN && !final &&
        decoder->probe_size < sizeof(decoder->probe))
        return 0;
    if (decoder->mode == EAPI_STREAM_UNKNOWN) {
        set_error(error, error_size, final ?
                  "EAPI 响应为空" : "EAPI 返回了非 JSON 数据");
        return -1;
    }
    int result = decoder->mode == EAPI_STREAM_PLAIN ?
        stream_write_decoded(decoder, decoder->probe,
                             decoder->probe_size, error, error_size) :
        stream_cipher(decoder, decoder->probe,
                      decoder->probe_size, error, error_size);
    decoder->probe_size = 0;
    return result == 0 ? 1 : -1;
}

int eapi_stream_decoder_feed(EapiStreamDecoder *decoder,
                             const uint8_t *data, size_t size,
                             char *error, size_t error_size) {
    if (!decoder || (!data && size != 0)) {
        set_error(error, error_size, "EAPI 响应为空");
        return -1;
    }
    size_t offset = 0;
    while (decoder->mode == EAPI_STREAM_UNKNOWN && offset < size) {
        if (decoder->probe_size == sizeof(decoder->probe)) {
            set_error(error, error_size, "EAPI 返回了非 JSON 数据");
            return -1;
        }
        decoder->probe[decoder->probe_size++] = data[offset++];
        if (decoder->probe_size < 16) continue;
        int chosen = stream_choose_mode(decoder, false, error, error_size);
        if (chosen < 0) return -1;
    }
    if (offset == size) return 0;
    if (decoder->mode == EAPI_STREAM_PLAIN)
        return stream_write_decoded(decoder, data + offset, size - offset,
                                    error, error_size);
    return stream_cipher(decoder, data + offset, size - offset,
                         error, error_size);
}

int eapi_stream_decoder_finish(EapiStreamDecoder *decoder,
                               char *error, size_t error_size) {
    if (!decoder) {
        set_error(error, error_size, "EAPI 响应为空");
        return -1;
    }
    if (decoder->mode == EAPI_STREAM_UNKNOWN &&
        stream_choose_mode(decoder, true, error, error_size) < 0)
        return -1;
    if (decoder->mode == EAPI_STREAM_ENCRYPTED) {
        if (decoder->cipher_size != 0 || !decoder->has_pending_plain) {
            set_error(error, error_size, "EAPI 响应异常");
            return -1;
        }
        unsigned int padding = decoder->pending_plain[15];
        if (padding == 0 || padding > 16) {
            set_error(error, error_size, "EAPI 填充无效");
            return -1;
        }
        for (unsigned int i = 0; i < padding; i++) {
            if (decoder->pending_plain[15U - i] != padding) {
                set_error(error, error_size, "EAPI 填充无效");
                return -1;
            }
        }
        size_t final_size = 16U - padding;
        if (final_size && stream_payload(
                decoder, decoder->pending_plain, final_size,
                error, error_size) != 0)
            return -1;
    }
    if (decoder->payload_mode == EAPI_PAYLOAD_GZIP &&
        !decoder->inflate_finished) {
        set_error(error, error_size, "gzip 解码失败：%d", Z_DATA_ERROR);
        return -1;
    }
    if (decoder->mode == EAPI_STREAM_ENCRYPTED &&
        decoder->payload_mode == EAPI_PAYLOAD_UNKNOWN) {
        set_error(error, error_size, "EAPI 返回了非 JSON 数据");
        return -1;
    }
    return 0;
}

void eapi_stream_decoder_destroy(EapiStreamDecoder *decoder) {
    if (!decoder) return;
    if (decoder->inflate_ready) inflateEnd(&decoder->inflate);
    if (decoder->aes_ready) mbedtls_aes_free(&decoder->aes);
    free(decoder);
}
