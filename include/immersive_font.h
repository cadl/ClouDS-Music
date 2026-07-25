#pragma once

#include "immersive_font_data.h"

#include <citro2d.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define IMMERSIVE_FONT_TEXTURE_WIDTH 512U
#define IMMERSIVE_FONT_TEXTURE_HEIGHT 256U
#define IMMERSIVE_FONT_CACHE_CAPACITY 280U

typedef struct {
    uint8_t *file_bytes;
    ImmersiveFontData data;
    C3D_Tex texture;
    uint32_t cached_codepoints[IMMERSIVE_FONT_CACHE_CAPACITY];
    uint32_t cached_colors[IMMERSIVE_FONT_CACHE_CAPACITY];
    size_t cached_count;
    size_t cache_columns;
    size_t cache_capacity;
    bool reset_pending;
    bool ready;
} ImmersiveFont;

void immersive_font_init(ImmersiveFont *font);
void immersive_font_clear(ImmersiveFont *font);
int immersive_font_load(ImmersiveFont *font, const char *path);
bool immersive_font_ready(const ImmersiveFont *font);
float immersive_font_glyph_height(const ImmersiveFont *font);
void immersive_font_begin_frame(ImmersiveFont *font);
bool immersive_font_glyph_advance(const ImmersiveFont *font,
                                  uint32_t codepoint, float *advance);
void immersive_font_prepare_texts(ImmersiveFont *font,
                                  const char *const *texts,
                                  const uint32_t *colors, size_t count);
void immersive_font_cache_text(ImmersiveFont *font, const char *text,
                               uint32_t color);
bool immersive_font_draw_glyph(ImmersiveFont *font, uint32_t codepoint,
                               float x, float y, float z, uint32_t color);
bool immersive_font_draw_glyph_scaled(
    ImmersiveFont *font, uint32_t codepoint,
    float x, float y, float z, float scale_x, float scale_y,
    uint32_t color);
