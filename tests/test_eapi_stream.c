#include "eapi.h"

#include <assert.h>
#include <mbedtls/aes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static const uint8_t eapi_key[] = "e82ckenh8dichen8";

typedef struct {
    uint8_t data[4096];
    size_t size;
} Collector;

static int collect(const uint8_t *data, size_t size, void *userdata) {
    Collector *collector = (Collector *)userdata;
    if (size > sizeof(collector->data) - collector->size) return -1;
    memcpy(collector->data + collector->size, data, size);
    collector->size += size;
    return 0;
}

static uint8_t *encrypt_response(const uint8_t *plain, size_t plain_size,
                                 size_t *encrypted_size) {
    size_t padding = 16U - plain_size % 16U;
    *encrypted_size = plain_size + padding;
    uint8_t *padded = (uint8_t *)malloc(*encrypted_size);
    uint8_t *encrypted = (uint8_t *)malloc(*encrypted_size);
    assert(padded && encrypted);
    memcpy(padded, plain, plain_size);
    memset(padded + plain_size, (int)padding, padding);
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    assert(mbedtls_aes_setkey_enc(&aes, eapi_key, 128) == 0);
    for (size_t offset = 0; offset < *encrypted_size; offset += 16)
        assert(mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT,
                                     padded + offset,
                                     encrypted + offset) == 0);
    mbedtls_aes_free(&aes);
    free(padded);
    return encrypted;
}

static uint8_t *gzip_text(const char *text, size_t *compressed_size) {
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    assert(deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED,
                        16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    size_t input_size = strlen(text);
    size_t capacity = compressBound(input_size) + 32U;
    uint8_t *compressed = (uint8_t *)malloc(capacity);
    assert(compressed != NULL);
    stream.next_in = (Bytef *)text;
    stream.avail_in = (uInt)input_size;
    stream.next_out = compressed;
    stream.avail_out = (uInt)capacity;
    assert(deflate(&stream, Z_FINISH) == Z_STREAM_END);
    *compressed_size = stream.total_out;
    assert(deflateEnd(&stream) == Z_OK);
    return compressed;
}

static void feed_chunks(EapiStreamDecoder *decoder,
                        const uint8_t *data, size_t size, size_t chunk,
                        char *error, size_t error_size) {
    for (size_t offset = 0; offset < size; offset += chunk) {
        size_t amount = size - offset;
        if (amount > chunk) amount = chunk;
        assert(eapi_stream_decoder_feed(
                   decoder, data + offset, amount,
                   error, error_size) == 0);
    }
}

int main(void) {
    static const char json[] =
        "{\"playlist\":{\"trackIds\":[{\"id\":11},{\"id\":22}]}}";
    char error[192] = {0};
    Collector collector = {0};
    EapiStreamDecoder *decoder = eapi_stream_decoder_create(
        sizeof(collector.data), collect, &collector,
        error, sizeof(error));
    assert(decoder != NULL);

    feed_chunks(decoder, (const uint8_t *)json, strlen(json), 1,
                error, sizeof(error));
    assert(eapi_stream_decoder_finish(decoder, error, sizeof(error)) == 0);
    assert(collector.size == strlen(json));
    assert(memcmp(collector.data, json, collector.size) == 0);

    size_t encrypted_size = 0;
    uint8_t *encrypted = encrypt_response(
        (const uint8_t *)json, strlen(json), &encrypted_size);
    collector.size = 0;
    assert(eapi_stream_decoder_reset(decoder, error, sizeof(error)) == 0);
    feed_chunks(decoder, encrypted, encrypted_size, 7,
                error, sizeof(error));
    assert(eapi_stream_decoder_finish(decoder, error, sizeof(error)) == 0);
    assert(collector.size == strlen(json));
    assert(memcmp(collector.data, json, collector.size) == 0);
    free(encrypted);

    char collision_json[256];
    encrypted = NULL;
    for (unsigned int nonce = 0; nonce < 100000U; nonce++) {
        int written = snprintf(
            collision_json, sizeof(collision_json),
            "{\"nonce\":%u,\"playlist\":{\"trackIds\":[{\"id\":11}]}}",
            nonce);
        assert(written > 0 && (size_t)written < sizeof(collision_json));
        encrypted = encrypt_response(
            (const uint8_t *)collision_json, (size_t)written,
            &encrypted_size);
        if (encrypted[0] == '{' || encrypted[0] == '[') break;
        free(encrypted);
        encrypted = NULL;
    }
    assert(encrypted != NULL);
    collector.size = 0;
    assert(eapi_stream_decoder_reset(decoder, error, sizeof(error)) == 0);
    feed_chunks(decoder, encrypted, encrypted_size, 3,
                error, sizeof(error));
    assert(eapi_stream_decoder_finish(decoder, error, sizeof(error)) == 0);
    assert(collector.size == strlen(collision_json));
    assert(memcmp(collector.data, collision_json, collector.size) == 0);
    free(encrypted);

    size_t gzip_size = 0;
    uint8_t *gzip = gzip_text(json, &gzip_size);
    encrypted = encrypt_response(gzip, gzip_size, &encrypted_size);
    free(gzip);
    collector.size = 0;
    assert(eapi_stream_decoder_reset(decoder, error, sizeof(error)) == 0);
    feed_chunks(decoder, encrypted, encrypted_size, 13,
                error, sizeof(error));
    assert(eapi_stream_decoder_finish(decoder, error, sizeof(error)) == 0);
    assert(collector.size == strlen(json));
    assert(memcmp(collector.data, json, collector.size) == 0);

    encrypted[encrypted_size - 1] ^= 1U;
    collector.size = 0;
    error[0] = '\0';
    assert(eapi_stream_decoder_reset(decoder, error, sizeof(error)) == 0);
    feed_chunks(decoder, encrypted, encrypted_size, 16,
                error, sizeof(error));
    assert(eapi_stream_decoder_finish(decoder, error, sizeof(error)) < 0);
    free(encrypted);
    eapi_stream_decoder_destroy(decoder);

    collector.size = 0;
    error[0] = '\0';
    decoder = eapi_stream_decoder_create(
        8, collect, &collector, error, sizeof(error));
    assert(decoder != NULL);
    assert(eapi_stream_decoder_feed(
               decoder, (const uint8_t *)json, strlen(json),
               error, sizeof(error)) < 0);
    eapi_stream_decoder_destroy(decoder);

    puts("eapi stream tests: ok");
    return 0;
}
