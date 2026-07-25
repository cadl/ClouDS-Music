#pragma once

#include <stddef.h>
#include <stdint.h>

#define COVER_ART_SIZE 128U
#define COVER_ART_PIXELS (COVER_ART_SIZE * COVER_ART_SIZE)

typedef enum {
    COVER_IMAGE_UNKNOWN = 0,
    COVER_IMAGE_JPEG,
    COVER_IMAGE_PNG
} CoverImageFormat;

CoverImageFormat cover_image_format(const uint8_t *header, size_t size);
int cover_decode_image(const char *path, uint32_t *tiled,
                       size_t pixel_count, char *error, size_t error_size);
