#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IMMERSIVE_FONT_MAGIC "IMBF"
#define IMMERSIVE_FONT_VERSION 1U
#define IMMERSIVE_FONT_MAX_GLYPH_WIDTH 24U
#define IMMERSIVE_FONT_MAX_GLYPH_HEIGHT 32U
#define IMMERSIVE_FONT_MAX_BITMAP_BYTES 96U
#define IMMERSIVE_FONT_HEADER_BYTES 16U

typedef struct {
    const uint8_t *bytes;
    size_t size;
    uint32_t glyph_count;
    uint16_t glyph_width;
    uint16_t glyph_height;
    uint16_t bitmap_bytes;
    size_t entry_bytes;
} ImmersiveFontData;

typedef struct {
    uint32_t codepoint;
    uint8_t advance;
    const uint8_t *bitmap;
} ImmersiveFontGlyph;

bool immersive_font_data_init(ImmersiveFontData *font,
                              const uint8_t *bytes, size_t size);
bool immersive_font_data_lookup(const ImmersiveFontData *font,
                                uint32_t codepoint,
                                ImmersiveFontGlyph *glyph);
bool immersive_font_glyph_pixel(const ImmersiveFontData *font,
                                const ImmersiveFontGlyph *glyph,
                                unsigned int x, unsigned int y);
