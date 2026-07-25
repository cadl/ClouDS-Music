#include "immersive_font_data.h"

#include <string.h>

static uint16_t read_u16(const uint8_t *value) {
    return (uint16_t)value[0] | ((uint16_t)value[1] << 8U);
}

static uint32_t read_u32(const uint8_t *value) {
    return (uint32_t)value[0] | ((uint32_t)value[1] << 8U) |
           ((uint32_t)value[2] << 16U) | ((uint32_t)value[3] << 24U);
}

static const uint8_t *entry_at(const ImmersiveFontData *font,
                               uint32_t index) {
    return font->bytes + IMMERSIVE_FONT_HEADER_BYTES +
           (size_t)index * font->entry_bytes;
}

bool immersive_font_data_init(ImmersiveFontData *font,
                              const uint8_t *bytes, size_t size) {
    if (font) memset(font, 0, sizeof(*font));
    if (!font || !bytes || size < IMMERSIVE_FONT_HEADER_BYTES ||
        memcmp(bytes, IMMERSIVE_FONT_MAGIC, 4) != 0 ||
        read_u16(bytes + 4) != IMMERSIVE_FONT_VERSION)
        return false;

    uint16_t glyph_width = read_u16(bytes + 6);
    uint16_t glyph_height = read_u16(bytes + 8);
    uint16_t bitmap_bytes = read_u16(bytes + 10);
    size_t row_bytes = ((size_t)glyph_width + 7U) / 8U;
    if (glyph_width == 0 || glyph_height == 0 ||
        glyph_width > IMMERSIVE_FONT_MAX_GLYPH_WIDTH ||
        glyph_height > IMMERSIVE_FONT_MAX_GLYPH_HEIGHT ||
        bitmap_bytes == 0 || bitmap_bytes > IMMERSIVE_FONT_MAX_BITMAP_BYTES ||
        bitmap_bytes != row_bytes * glyph_height)
        return false;
    size_t entry_bytes = 8U + bitmap_bytes;
    uint32_t count = read_u32(bytes + 12);
    size_t entries_size = size - IMMERSIVE_FONT_HEADER_BYTES;
    if (count == 0 || entries_size % entry_bytes != 0 ||
        (size_t)count != entries_size / entry_bytes)
        return false;

    uint32_t previous = 0;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *entry = bytes + IMMERSIVE_FONT_HEADER_BYTES +
            (size_t)i * entry_bytes;
        uint32_t codepoint = read_u32(entry);
        uint8_t advance = entry[4];
        if ((i > 0 && codepoint <= previous) || advance == 0 ||
            advance > glyph_width)
            return false;
        previous = codepoint;
    }

    font->bytes = bytes;
    font->size = size;
    font->glyph_count = count;
    font->glyph_width = glyph_width;
    font->glyph_height = glyph_height;
    font->bitmap_bytes = bitmap_bytes;
    font->entry_bytes = entry_bytes;
    return true;
}

bool immersive_font_data_lookup(const ImmersiveFontData *font,
                                uint32_t codepoint,
                                ImmersiveFontGlyph *glyph) {
    if (!font || !font->bytes || !glyph) return false;
    uint32_t low = 0;
    uint32_t high = font->glyph_count;
    while (low < high) {
        uint32_t middle = low + (high - low) / 2U;
        const uint8_t *entry = entry_at(font, middle);
        uint32_t candidate = read_u32(entry);
        if (candidate < codepoint) low = middle + 1U;
        else high = middle;
    }
    if (low >= font->glyph_count) return false;
    const uint8_t *entry = entry_at(font, low);
    if (read_u32(entry) != codepoint) return false;
    glyph->codepoint = codepoint;
    glyph->advance = entry[4];
    glyph->bitmap = entry + 8;
    return true;
}

bool immersive_font_glyph_pixel(const ImmersiveFontData *font,
                                const ImmersiveFontGlyph *glyph,
                                unsigned int x, unsigned int y) {
    if (!font || !glyph || !glyph->bitmap || x >= font->glyph_width ||
        y >= font->glyph_height)
        return false;
    size_t row_bytes = ((size_t)font->glyph_width + 7U) / 8U;
    size_t offset = (size_t)y * row_bytes +
                    x / 8U;
    return (glyph->bitmap[offset] & (0x80U >> (x & 7U))) != 0;
}
