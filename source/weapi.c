#include "weapi.h"

#include "i18n.h"

#include <mbedtls/aes.h>
#include <mbedtls/base64.h>
#include <mbedtls/bignum.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEAPI_BLOCK_SIZE 16U
#define WEAPI_RSA_BYTES 128U

static const unsigned char WEAPI_IV[] =
    "0102030405060708";
static const unsigned char WEAPI_PRESET_KEY[] =
    "0CoJUm6Qyw8W8jud";
static const char WEAPI_BASE62[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
static const char WEAPI_RSA_EXPONENT[] = "010001";
static const char WEAPI_RSA_MODULUS[] =
    "e0b509f6259df8642dbc35662901477df22677ec152b5ff68ace615bb7b725152b"
    "3ab17a876aea8a5aa76d2e417629ec4ee341f56135fccf695280104e0312ecbda9"
    "2557c93870114af6c9d05c4f7f0c3685b7a46bee255932575cce10b424d813cfe4"
    "875d3e82047b97ddef52741d546b8e289dc6935b3ece0462db0a22b8e7";

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static bool valid_secret(
    const unsigned char secret[WEAPI_SECRET_LENGTH]) {
    if (!secret) return false;
    for (size_t i = 0; i < WEAPI_SECRET_LENGTH; i++) {
        if (secret[i] == '\0' || !strchr(WEAPI_BASE62, secret[i]))
            return false;
    }
    return true;
}

static int aes_cbc_base64(const unsigned char *input, size_t input_size,
                          const unsigned char key[WEAPI_BLOCK_SIZE],
                          char **output, char *error, size_t error_size) {
    if (!input || !key || !output || input_size > SIZE_MAX - WEAPI_BLOCK_SIZE) {
        set_error(error, error_size, "WEAPI AES 输入无效");
        return -1;
    }
    *output = NULL;
    size_t padding = WEAPI_BLOCK_SIZE - input_size % WEAPI_BLOCK_SIZE;
    size_t padded_size = input_size + padding;
    unsigned char *plain = (unsigned char *)malloc(padded_size);
    unsigned char *encrypted = (unsigned char *)malloc(padded_size);
    if (!plain || !encrypted) {
        free(plain);
        free(encrypted);
        set_error(error, error_size, "内存不足");
        return -1;
    }
    memcpy(plain, input, input_size);
    memset(plain + input_size, (int)padding, padding);

    unsigned char iv[WEAPI_BLOCK_SIZE];
    memcpy(iv, WEAPI_IV, sizeof(iv));
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int result = mbedtls_aes_setkey_enc(&aes, key, 128);
    if (result == 0)
        result = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT,
                                       padded_size, iv, plain, encrypted);
    mbedtls_aes_free(&aes);
    free(plain);
    if (result != 0) {
        free(encrypted);
        set_error(error, error_size, "WEAPI AES 失败：%d", result);
        return -1;
    }

    if (padded_size > (SIZE_MAX - 2U) / 4U * 3U) {
        free(encrypted);
        set_error(error, error_size, "WEAPI 请求过大");
        return -1;
    }
    size_t encoded_capacity = 4U * ((padded_size + 2U) / 3U);
    char *encoded = (char *)malloc(encoded_capacity + 1U);
    if (!encoded) {
        free(encrypted);
        set_error(error, error_size, "内存不足");
        return -1;
    }
    size_t encoded_size = 0;
    result = mbedtls_base64_encode(
        (unsigned char *)encoded, encoded_capacity + 1U, &encoded_size,
        encrypted, padded_size);
    free(encrypted);
    if (result != 0) {
        free(encoded);
        set_error(error, error_size, "WEAPI Base64 失败：%d", result);
        return -1;
    }
    encoded[encoded_size] = '\0';
    *output = encoded;
    return 0;
}

static int rsa_secret_hex(
    const unsigned char secret[WEAPI_SECRET_LENGTH],
    char output[WEAPI_RSA_BYTES * 2U + 1U],
    char *error, size_t error_size) {
    unsigned char reversed[WEAPI_SECRET_LENGTH];
    for (size_t i = 0; i < WEAPI_SECRET_LENGTH; i++)
        reversed[i] = secret[WEAPI_SECRET_LENGTH - 1U - i];

    mbedtls_mpi input;
    mbedtls_mpi exponent;
    mbedtls_mpi modulus;
    mbedtls_mpi encrypted;
    mbedtls_mpi_init(&input);
    mbedtls_mpi_init(&exponent);
    mbedtls_mpi_init(&modulus);
    mbedtls_mpi_init(&encrypted);
    int result = mbedtls_mpi_read_binary(&input, reversed, sizeof(reversed));
    if (result == 0)
        result = mbedtls_mpi_read_string(&exponent, 16, WEAPI_RSA_EXPONENT);
    if (result == 0)
        result = mbedtls_mpi_read_string(&modulus, 16, WEAPI_RSA_MODULUS);
    if (result == 0)
        result = mbedtls_mpi_exp_mod(&encrypted, &input, &exponent,
                                     &modulus, NULL);
    unsigned char bytes[WEAPI_RSA_BYTES];
    if (result == 0)
        result = mbedtls_mpi_write_binary(&encrypted, bytes, sizeof(bytes));
    mbedtls_mpi_free(&encrypted);
    mbedtls_mpi_free(&modulus);
    mbedtls_mpi_free(&exponent);
    mbedtls_mpi_free(&input);
    if (result != 0) {
        set_error(error, error_size, "WEAPI RSA 失败：%d", result);
        return -1;
    }

    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(bytes); i++) {
        output[i * 2U] = hex[bytes[i] >> 4];
        output[i * 2U + 1U] = hex[bytes[i] & 0x0f];
    }
    output[sizeof(bytes) * 2U] = '\0';
    return 0;
}

static bool form_safe(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
           c == '~';
}

static size_t form_encoded_size(const char *value) {
    size_t size = 0;
    for (const unsigned char *p = (const unsigned char *)value; *p; p++)
        size += form_safe(*p) ? 1U : 3U;
    return size;
}

static char *form_encode(char *output, const char *value) {
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (form_safe(*p)) {
            *output++ = (char)*p;
        } else {
            *output++ = '%';
            *output++ = hex[*p >> 4];
            *output++ = hex[*p & 0x0f];
        }
    }
    return output;
}

int weapi_build_form(const char *json,
                     const unsigned char secret[WEAPI_SECRET_LENGTH],
                     char **form, char *error, size_t error_size) {
    if (!form) {
        set_error(error, error_size, "WEAPI 请求无效");
        return -1;
    }
    *form = NULL;
    if (!json || !valid_secret(secret)) {
        set_error(error, error_size, "WEAPI 请求无效");
        return -1;
    }
    char *first = NULL;
    char *params = NULL;
    if (aes_cbc_base64((const unsigned char *)json, strlen(json),
                       WEAPI_PRESET_KEY, &first, error, error_size) != 0)
        return -1;
    if (aes_cbc_base64((const unsigned char *)first, strlen(first),
                       secret, &params, error, error_size) != 0) {
        free(first);
        return -1;
    }
    free(first);

    char enc_sec_key[WEAPI_RSA_BYTES * 2U + 1U];
    if (rsa_secret_hex(secret, enc_sec_key, error, error_size) != 0) {
        free(params);
        return -1;
    }
    static const char prefix[] = "params=";
    static const char middle[] = "&encSecKey=";
    size_t params_size = form_encoded_size(params);
    if (params_size > SIZE_MAX - sizeof(prefix) - sizeof(middle) -
                          strlen(enc_sec_key)) {
        free(params);
        set_error(error, error_size, "WEAPI 请求过大");
        return -1;
    }
    size_t form_size = sizeof(prefix) - 1U + params_size +
                       sizeof(middle) - 1U + strlen(enc_sec_key);
    char *encoded = (char *)malloc(form_size + 1U);
    if (!encoded) {
        free(params);
        set_error(error, error_size, "内存不足");
        return -1;
    }
    char *cursor = encoded;
    memcpy(cursor, prefix, sizeof(prefix) - 1U);
    cursor += sizeof(prefix) - 1U;
    cursor = form_encode(cursor, params);
    free(params);
    memcpy(cursor, middle, sizeof(middle) - 1U);
    cursor += sizeof(middle) - 1U;
    memcpy(cursor, enc_sec_key, strlen(enc_sec_key));
    cursor += strlen(enc_sec_key);
    *cursor = '\0';
    *form = encoded;
    return 0;
}
