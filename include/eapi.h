#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct EapiStreamDecoder EapiStreamDecoder;
typedef int (*EapiStreamWrite)(const uint8_t *data, size_t size,
                               void *userdata);

int eapi_build_form(const char *api_path, const char *json,
                    char **form, char *error, size_t error_size);
int eapi_decode_response(uint8_t *body, size_t body_size,
                         char **json, char *error, size_t error_size);

EapiStreamDecoder *eapi_stream_decoder_create(
    size_t max_decoded_bytes, EapiStreamWrite write, void *userdata,
    char *error, size_t error_size);
int eapi_stream_decoder_reset(EapiStreamDecoder *decoder,
                              char *error, size_t error_size);
int eapi_stream_decoder_feed(EapiStreamDecoder *decoder,
                             const uint8_t *data, size_t size,
                             char *error, size_t error_size);
int eapi_stream_decoder_finish(EapiStreamDecoder *decoder,
                               char *error, size_t error_size);
void eapi_stream_decoder_destroy(EapiStreamDecoder *decoder);
