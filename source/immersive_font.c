#include "immersive_font.h"

#include "gpu_texture.h"

#include <3ds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMMERSIVE_FONT_MAX_FILE_BYTES (2U * 1024U * 1024U)

static unsigned int morton8(unsigned int x, unsigned int y) {
    return (x & 1U) | ((y & 1U) << 1U) |
           ((x & 2U) << 1U) | ((y & 2U) << 2U) |
           ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

static size_t tiled_offset(unsigned int x, unsigned int y) {
    size_t tile = (size_t)(y >> 3U) *
                  (IMMERSIVE_FONT_TEXTURE_WIDTH >> 3U) + (x >> 3U);
    return tile * 64U + morton8(x & 7U, y & 7U);
}

static void clear_texture(ImmersiveFont *font) {
    if (!font || !font->texture.data) return;
    memset(font->texture.data, 0, font->texture.size);
}

static uint32_t opaque_color(uint32_t color) {
    return color | 0xFF000000U;
}

static uint32_t texture_color(uint32_t color) {
    color = opaque_color(color);
    return gpu_texture_rgba8(
        (uint8_t)color, (uint8_t)(color >> 8U),
        (uint8_t)(color >> 16U), 0xFFU);
}

static int cached_slot(const ImmersiveFont *font, uint32_t codepoint,
                       uint32_t color) {
    color = opaque_color(color);
    for (size_t i = 0; font && i < font->cached_count; i++)
        if (font->cached_codepoints[i] == codepoint &&
            font->cached_colors[i] == color)
            return (int)i;
    return -1;
}

static bool cache_glyph(ImmersiveFont *font, uint32_t codepoint,
                        uint32_t color) {
    if (!font || !font->ready ||
        font->cached_count >= font->cache_capacity)
        return false;
    color = opaque_color(color);
    if (cached_slot(font, codepoint, color) >= 0) return true;

    ImmersiveFontGlyph glyph;
    if (!immersive_font_data_lookup(&font->data, codepoint, &glyph))
        return false;
    size_t slot = font->cached_count;
    unsigned int left = (unsigned int)(slot % font->cache_columns) *
                        font->data.glyph_width;
    unsigned int top = (unsigned int)(slot / font->cache_columns) *
                       font->data.glyph_height;
    uint32_t *pixels = (uint32_t *)font->texture.data;
    uint32_t packed_color = texture_color(color);
    for (unsigned int y = 0; y < font->data.glyph_height; y++) {
        for (unsigned int x = 0; x < font->data.glyph_width; x++) {
            if (immersive_font_glyph_pixel(&font->data, &glyph, x, y))
                pixels[tiled_offset(left + x, top + y)] = packed_color;
        }
    }
    font->cached_codepoints[slot] = codepoint;
    font->cached_colors[slot] = color;
    font->cached_count++;
    return true;
}

void immersive_font_init(ImmersiveFont *font) {
    if (font) memset(font, 0, sizeof(*font));
}

void immersive_font_clear(ImmersiveFont *font) {
    if (!font) return;
    if (font->ready) C3D_TexDelete(&font->texture);
    free(font->file_bytes);
    immersive_font_init(font);
}

int immersive_font_load(ImmersiveFont *font, const char *path) {
    if (!font || !path) return -1;
    if (font->ready) return 0;

    FILE *stream = fopen(path, "rb");
    if (!stream) return -1;
    if (fseek(stream, 0, SEEK_END) != 0) {
        fclose(stream);
        return -1;
    }
    long encoded_size = ftell(stream);
    if (encoded_size <= 0 ||
        (unsigned long)encoded_size > IMMERSIVE_FONT_MAX_FILE_BYTES ||
        fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return -1;
    }

    size_t size = (size_t)encoded_size;
    uint8_t *bytes = (uint8_t *)malloc(size);
    if (!bytes) {
        fclose(stream);
        return -1;
    }
    bool read_ok = fread(bytes, 1, size, stream) == size && !ferror(stream);
    fclose(stream);
    ImmersiveFontData data;
    if (!read_ok || !immersive_font_data_init(&data, bytes, size) ||
        !C3D_TexInit(&font->texture, IMMERSIVE_FONT_TEXTURE_WIDTH,
                     IMMERSIVE_FONT_TEXTURE_HEIGHT, GPU_RGBA8)) {
        free(bytes);
        memset(&font->texture, 0, sizeof(font->texture));
        return -1;
    }

    font->file_bytes = bytes;
    font->data = data;
    font->cached_count = 0;
    font->cache_columns = IMMERSIVE_FONT_TEXTURE_WIDTH / data.glyph_width;
    size_t cache_rows = IMMERSIVE_FONT_TEXTURE_HEIGHT / data.glyph_height;
    font->cache_capacity = font->cache_columns * cache_rows;
    if (font->cache_capacity > IMMERSIVE_FONT_CACHE_CAPACITY)
        font->cache_capacity = IMMERSIVE_FONT_CACHE_CAPACITY;
    font->ready = true;
    clear_texture(font);
    C3D_TexFlush(&font->texture);
    C3D_TexSetFilter(&font->texture, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&font->texture, GPU_CLAMP_TO_BORDER,
                   GPU_CLAMP_TO_BORDER);
    return 0;
}

bool immersive_font_ready(const ImmersiveFont *font) {
    return font && font->ready;
}

float immersive_font_glyph_height(const ImmersiveFont *font) {
    return font && font->ready ? font->data.glyph_height : 0.0f;
}

void immersive_font_begin_frame(ImmersiveFont *font) {
    if (!font || !font->ready || !font->reset_pending) return;
    font->cached_count = 0;
    font->reset_pending = false;
    memset(font->cached_codepoints, 0, sizeof(font->cached_codepoints));
    memset(font->cached_colors, 0, sizeof(font->cached_colors));
    clear_texture(font);
    C3D_TexFlush(&font->texture);
}

bool immersive_font_glyph_advance(const ImmersiveFont *font,
                                  uint32_t codepoint, float *advance) {
    ImmersiveFontGlyph glyph;
    if (!font || !font->ready ||
        !immersive_font_data_lookup(&font->data, codepoint, &glyph))
        return false;
    if (advance) *advance = glyph.advance;
    return true;
}

void immersive_font_prepare_texts(ImmersiveFont *font,
                                  const char *const *texts,
                                  const uint32_t *colors, size_t count) {
    if (!font || !font->ready ||
        ((!texts || !colors) && count > 0))
        return;
    font->cached_count = 0;
    font->reset_pending = false;
    memset(font->cached_codepoints, 0, sizeof(font->cached_codepoints));
    memset(font->cached_colors, 0, sizeof(font->cached_colors));
    clear_texture(font);

    for (size_t i = 0; i < count; i++) {
        const uint8_t *cursor = (const uint8_t *)texts[i];
        while (cursor && *cursor) {
            u32 codepoint = 0xFFFDU;
            ssize_t decoded = decode_utf8(&codepoint, cursor);
            cursor += decoded > 0 ? (size_t)decoded : 1U;
            ImmersiveFontGlyph glyph;
            if (!immersive_font_data_lookup(&font->data,
                                            codepoint, &glyph))
                codepoint = 0x25A1U;
            (void)cache_glyph(font, codepoint, colors[i]);
        }
    }
    C3D_TexFlush(&font->texture);
}

void immersive_font_cache_text(ImmersiveFont *font, const char *text,
                               uint32_t color) {
    if (!font || !font->ready || !text || !text[0]) return;
    u32 missing[256];
    size_t missing_count = 0;
    const uint8_t *cursor = (const uint8_t *)text;
    while (*cursor) {
        u32 codepoint = 0xFFFDU;
        ssize_t decoded = decode_utf8(&codepoint, cursor);
        cursor += decoded > 0 ? (size_t)decoded : 1U;
        ImmersiveFontGlyph glyph;
        if (!immersive_font_data_lookup(&font->data, codepoint, &glyph))
            codepoint = 0x25A1U;
        if (cached_slot(font, codepoint, color) >= 0)
            continue;
        bool duplicate = false;
        for (size_t i = 0; i < missing_count; i++)
            duplicate |= missing[i] == codepoint;
        if (!duplicate && missing_count < sizeof(missing) / sizeof(missing[0]))
            missing[missing_count++] = codepoint;
    }
    if (missing_count == 0) return;

    size_t available = font->cache_capacity - font->cached_count;
    size_t add_count = missing_count < available ? missing_count : available;
    if (add_count == 0) {
        font->reset_pending = true;
        return;
    }
    C2D_Flush();
    for (size_t i = 0; i < add_count; i++)
        (void)cache_glyph(font, missing[i], color);
    C3D_TexFlush(&font->texture);
    if (add_count < missing_count) font->reset_pending = true;
}

bool immersive_font_draw_glyph(ImmersiveFont *font, uint32_t codepoint,
                               float x, float y, float z, uint32_t color) {
    return immersive_font_draw_glyph_scaled(
        font, codepoint, x, y, z, 1.0f, 1.0f, color);
}

bool immersive_font_draw_glyph_scaled(
    ImmersiveFont *font, uint32_t codepoint,
    float x, float y, float z, float scale_x, float scale_y,
    uint32_t color) {
    int slot = cached_slot(font, codepoint, color);
    if (!font || !font->ready || slot < 0 ||
        scale_x <= 0.0f || scale_y <= 0.0f) return false;
    unsigned int left = (unsigned int)((size_t)slot % font->cache_columns) *
                        font->data.glyph_width;
    unsigned int top = (unsigned int)((size_t)slot / font->cache_columns) *
                       font->data.glyph_height;
    Tex3DS_SubTexture subtexture = {
        .width = font->data.glyph_width,
        .height = font->data.glyph_height,
        .left = (float)left / IMMERSIVE_FONT_TEXTURE_WIDTH,
        .top = 1.0f - (float)top / IMMERSIVE_FONT_TEXTURE_HEIGHT,
        .right = (float)(left + font->data.glyph_width) /
                 IMMERSIVE_FONT_TEXTURE_WIDTH,
        .bottom = 1.0f -
                  (float)(top + font->data.glyph_height) /
                  IMMERSIVE_FONT_TEXTURE_HEIGHT,
    };
    C2D_Image image = {&font->texture, &subtexture};
    C2D_ImageTint tint;
    float alpha = (float)(color >> 24U) / 255.0f;
    (void)C2D_SetTintMode(C2D_TintSolid);
    C2D_AlphaImageTint(&tint, alpha);
    return C2D_DrawImageAt(
        image, x, y, z, &tint, scale_x, scale_y);
}
