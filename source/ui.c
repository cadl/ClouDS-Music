#include "ui.h"

#include "cache.h"
#include "control_hint_layout.h"
#include "cover.h"
#include "i18n.h"
#include "ime_candidate_layout.h"
#include "ime_pinyin.h"
#include "immersive_font.h"
#include "immersive_lyrics.h"
#include "logo.h"
#include "lyric_animation.h"
#include "now_playing_policy.h"
#include "qrcodegen.h"
#include "ui_layout.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COL_BG       C2D_Color32(9, 11, 17, 255)
#define COL_PANEL    C2D_Color32(18, 22, 31, 255)
#define COL_PANEL_2  C2D_Color32(29, 34, 45, 255)
#define COL_GRID     C2D_Color32(47, 54, 69, 255)
#define COL_TEXT     C2D_Color32(244, 241, 231, 255)
#define COL_MUTED    C2D_Color32(145, 154, 169, 255)
#define COL_DIM      C2D_Color32(83, 91, 108, 255)
#define COL_RED      C2D_Color32(235, 55, 82, 255)
#define COL_ORANGE   C2D_Color32(255, 157, 60, 255)
#define COL_CYAN     C2D_Color32(73, 222, 207, 255)
#define COL_BLACK    C2D_Color32(4, 5, 8, 255)
#define COL_SCREEN_OFF C2D_Color32(0, 0, 0, 255)

#define BOTTOM_PLAYER_WIDTH 192
#define PLAYER_HELP_Y 8
#define PLAYER_HELP_H 91
#define PLAYER_PANEL_Y 104
#define PLAYER_PANEL_H 111
#define PROGRESS_X 12
#define PROGRESS_Y 115
#define PROGRESS_W 170
#define PROGRESS_TOUCH_Y 108
#define PROGRESS_TOUCH_H 30
#define QUEUE_LIST_Y 35
#define QUEUE_ROW_HEIGHT 38
#define QUEUE_TITLE_X 214.0f
#define QUEUE_TITLE_WIDTH 96.0f
#define CONTROL_LEFT_X ((float)UI_CONTROL_LEFT_CELL_X)
#define CONTROL_RIGHT_X ((float)UI_CONTROL_RIGHT_CELL_X)
#define CONTROL_ROW_1_Y 31.0f
#define CONTROL_ROW_2_Y 52.0f
#define CONTROL_ROW_3_Y 73.0f
#define CONTROL_COMPACT_ROW_1_Y 11.0f
#define CONTROL_ROW_STEP 21.0f

#define TRANSPORT_CENTER_Y 164
#define SECONDARY_CONTROL_Y 190
#define SECONDARY_CONTROL_H 23
#define PREVIOUS_X 10
#define PREVIOUS_W 45
#define PREVIOUS_H 38
#define PREVIOUS_Y (TRANSPORT_CENTER_Y - PREVIOUS_H / 2)
#define PLAY_X 61
#define PLAY_W 70
#define PLAY_H 50
#define PLAY_Y (TRANSPORT_CENTER_Y - PLAY_H / 2)
#define NEXT_X 137
#define NEXT_W 45
#define NEXT_H 38
#define NEXT_Y (TRANSPORT_CENTER_Y - NEXT_H / 2)
#define MODE_X 137
#define MODE_Y SECONDARY_CONTROL_Y
#define MODE_W 45
#define MODE_H SECONDARY_CONTROL_H
#define ALBUM_X 10
#define ALBUM_Y SECONDARY_CONTROL_Y
#define ALBUM_W 121
#define ALBUM_H SECONDARY_CONTROL_H
#define IME_CANDIDATE_X 70.0f
#define IME_CANDIDATE_RIGHT 316.0f
#define IME_CANDIDATE_Y 3.0f
#define IME_CANDIDATE_H 26.0f
#define IME_CANDIDATE_MIN_W 24.0f
#define IME_CANDIDATE_PADDING 6.0f
#define IME_CANDIDATE_GAP 2.0f
#define IME_LANGUAGE_X 5.0f
#define IME_LANGUAGE_W 47.0f
#define IME_SYMBOLS_X 56.0f
#define IME_SYMBOLS_W 47.0f
#define IME_SPACE_X 107.0f
#define IME_SPACE_W 69.0f
#define IME_CANCEL_X 180.0f
#define IME_CANCEL_W 56.0f
#define IME_SEARCH_X 240.0f
#define IME_SEARCH_W 75.0f
#define IME_ACTION_Y 172.0f
#define IME_ACTION_H 35.0f
#define LYRIC_VISIBLE_ROWS LYRIC_ANIMATION_VISIBLE_ROWS
#define LYRIC_ROW_HEIGHT 22.0f
#define LYRIC_TOP_Y 69.0f
#define LYRIC_FADE_TOP_Y 60.0f
#define LYRIC_BOTTOM_Y \
    (LYRIC_TOP_Y + (LYRIC_VISIBLE_ROWS - 1) * LYRIC_ROW_HEIGHT)
#define LYRIC_FADE_BOTTOM_Y 219.0f
#define LYRIC_TEXT_X 170.0f
#define LYRIC_TEXT_WIDTH 208.0f
#define LYRIC_TEXT_CLIP_HEIGHT 20.0f
#define IMMERSIVE_LYRIC_TEXT_WIDTH 360.0f
#define IMMERSIVE_LYRIC_CENTER_Y 116.0f
#define IMMERSIVE_WHEEL_LINE_RADIUS 3
#define IMMERSIVE_WHEEL_VISIBLE_DISTANCE 2.60f
#define IMMERSIVE_WHEEL_ANGLE_STEP 0.60f
#define IMMERSIVE_WHEEL_Y_RADIUS 86.0f
#define IMMERSIVE_WHEEL_TEXT_LIMIT 24U
#define IMMERSIVE_CRAWL_HORIZON_Y 38.0f
#define IMMERSIVE_CRAWL_DEPTH_SPAN 202.0f
#define IMMERSIVE_CRAWL_NEAR_DEPTH 0.90f
#define IMMERSIVE_CRAWL_ROW_DEPTH 0.10f
#define IMMERSIVE_CRAWL_LINE_COUNT 7
#define IMMERSIVE_CRAWL_HISTORY_LINES (IMMERSIVE_CRAWL_LINE_COUNT - 1)
/* Seven 24-codepoint rows exactly fit the current 168-slot 24x32 atlas in
 * the worst case, so keep the row limit paired with the line count. */
#define IMMERSIVE_CRAWL_TEXT_LIMIT 24U
#define IMMERSIVE_FLIP_TEXT_LIMIT 30U
#define UI_MENU_TEXT_GLYPHS 4096U
#define UI_MENU_FONT_PATH "romfs:/ui-menu-font.bcfnt"
#define IMMERSIVE_FONT_PATH "romfs:/immersive-font.bin"
#define CONTENT_POINT_FONT_PATH "romfs:/content-point-font.bin"
#define CONTENT_LARGE_POINT_FONT_PATH "romfs:/content-large-point-font.bin"
#define CONTENT_POINT_FONT_PIXELS 18.0f
#define CONTENT_POINT_FONT_Y_OFFSET -2.0f
#define TOP_SCREEN_WIDTH 400.0f
#define TOP_SCREEN_HEIGHT 240.0f
#define STEREO_SLIDER_THRESHOLD 0.01f

struct Ui {
    C3D_RenderTarget *top_left;
    C3D_RenderTarget *top_right;
    C3D_RenderTarget *bottom;
    C2D_TextBuf menu_text_buffer;
    C2D_Font menu_font;
    CoverArt cover;
    BrandLogo brand_logo;
    PinyinIme *ime;
    bool ime_attempted;
    bool ime_open;
    bool ime_chinese;
    bool ime_symbols;
    bool ime_candidate_layout_dirty;
    int ime_candidate_page;
    int ime_candidate_selected;
    float ime_candidate_text_widths[IME_MAX_CANDIDATES];
    ImeCandidateLayout ime_candidate_layout;
    char ime_text[96];
    uint8_t qr_temp[qrcodegen_BUFFER_LEN_MAX];
    uint8_t qr_code[qrcodegen_BUFFER_LEN_MAX];
    bool qr_ready;
    LyricAnimation lyric_animation;
    ImmersiveFont content_point_font;
    ImmersiveFont content_large_point_font;
    ImmersiveFont immersive_font;
    int64_t immersive_font_song_id;
    int immersive_font_active_index;
    ImmersiveLyricStyle immersive_font_style;
    uint64_t control_marquee_signature;
    uint64_t control_marquee_started_ms;
    size_t control_marquee_active;
    uint64_t queue_marquee_signature;
    uint64_t queue_marquee_started_ms;
};

typedef enum {
    UI_TEXT_TINY,
    UI_TEXT_LABEL,
    UI_TEXT_CAPTION,
    UI_TEXT_SMALL,
    UI_TEXT_BODY,
    UI_TEXT_LARGE,
    UI_TEXT_TITLE,
    UI_TEXT_DISPLAY,
    UI_TEXT_STYLE_COUNT
} UiTextStyle;

typedef struct {
    float preferred_px;
    float min_px;
} UiTextMetrics;

/* These are logical pixels at the 3DS screen's native resolution.  Keep all
 * call sites on semantic styles so a readability adjustment stays global. */
static const UiTextMetrics UI_TEXT_METRICS[UI_TEXT_STYLE_COUNT] = {
    [UI_TEXT_TINY] = {UI_CONTENT_TEXT_MIN_HEIGHT,
                      UI_CONTENT_TEXT_MIN_HEIGHT},
    [UI_TEXT_LABEL] = {UI_CONTENT_TEXT_MIN_HEIGHT,
                       UI_CONTENT_TEXT_MIN_HEIGHT},
    [UI_TEXT_CAPTION] = {UI_CONTENT_TEXT_MIN_HEIGHT,
                         UI_CONTENT_TEXT_MIN_HEIGHT},
    [UI_TEXT_SMALL] = {UI_CONTENT_TEXT_MIN_HEIGHT,
                       UI_CONTENT_TEXT_MIN_HEIGHT},
    [UI_TEXT_BODY] = {UI_CONTENT_TEXT_MIN_HEIGHT,
                      UI_CONTENT_TEXT_MIN_HEIGHT},
    [UI_TEXT_LARGE] = {UI_CONTENT_TEXT_MIN_HEIGHT,
                       UI_CONTENT_TEXT_MIN_HEIGHT},
    [UI_TEXT_TITLE] = {21.0f, UI_CONTENT_TEXT_MIN_HEIGHT},
    [UI_TEXT_DISPLAY] = {24.0f, UI_CONTENT_TEXT_MIN_HEIGHT},
};

static const UiTextMetrics *text_metrics(UiTextStyle style) {
    return &UI_TEXT_METRICS[style < UI_TEXT_STYLE_COUNT ?
                            style : UI_TEXT_BODY];
}

/* Compact 5x7 bitmap alphabet. Each row uses its low five bits. */
static const uint8_t PIXEL_GLYPHS[36][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
};

static uint8_t pixel_row(char c, int row) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return PIXEL_GLYPHS[c - 'A'][row];
    if (c >= '0' && c <= '9') return PIXEL_GLYPHS[26 + c - '0'][row];
    switch (c) {
        case '-': return row == 3 ? 14 : 0;
        case '_': return row == 6 ? 31 : 0;
        case '=': return (row == 2 || row == 4) ? 31 : 0;
        case '/': return row == 0 ? 1 : row == 1 ? 2 : row == 2 ? 2 :
                         row == 3 ? 4 : row == 4 ? 8 : row == 5 ? 8 : 16;
        case '>': return row == 1 ? 16 : row == 2 ? 8 : row == 3 ? 4 :
                         row == 4 ? 8 : row == 5 ? 16 : 0;
        case '<': return row == 1 ? 1 : row == 2 ? 2 : row == 3 ? 4 :
                         row == 4 ? 2 : row == 5 ? 1 : 0;
        case ':': return (row == 2 || row == 5) ? 4 : 0;
        case '.': return row == 6 ? 4 : 0;
        case '+': return row == 3 ? 14 : (row == 2 || row == 4) ? 4 : 0;
        case '?': return row == 0 ? 14 : row == 1 ? 17 : row == 2 ? 2 :
                         row == 3 ? 4 : row == 5 ? 4 : 0;
        default: return 0;
    }
}

static uint8_t brand_pixel_row(char c, int row) {
    static const uint8_t lower_c[7] = {0, 0, 14, 16, 16, 17, 14};
    static const uint8_t lower_i[7] = {4, 0, 12, 4, 4, 4, 14};
    static const uint8_t lower_l[7] = {12, 4, 4, 4, 4, 4, 14};
    static const uint8_t lower_o[7] = {0, 0, 14, 17, 17, 17, 14};
    static const uint8_t lower_s[7] = {0, 0, 15, 16, 14, 1, 30};
    static const uint8_t lower_u[7] = {0, 0, 17, 17, 17, 19, 13};
    switch (c) {
        case 'c': return lower_c[row];
        case 'i': return lower_i[row];
        case 'l': return lower_l[row];
        case 'o': return lower_o[row];
        case 's': return lower_s[row];
        case 'u': return lower_u[row];
        default: return pixel_row(c, row);
    }
}

static void draw_pixel_text(const char *text, float x, float y, float z,
                            int scale, u32 color, bool brand_case) {
    if (!text || scale <= 0) return;
    float cursor = x;
    for (const char *p = text; *p; p++, cursor += 6 * scale) {
        for (int row = 0; row < 7; row++) {
            uint8_t bits = brand_case ?
                           brand_pixel_row(*p, row) : pixel_row(*p, row);
            int col = 0;
            while (col < 5) {
                if (bits & (1U << (4 - col))) {
                    int start = col;
                    while (col < 5 && (bits & (1U << (4 - col)))) col++;
                    C2D_DrawRectSolid(cursor + start * scale,
                        y + row * scale, z, (col - start) * scale,
                        scale, color);
                } else col++;
            }
        }
    }
}

static void pixel_text(const char *text, float x, float y, float z,
                       int scale, u32 color) {
    draw_pixel_text(text, x, y, z, scale, color, false);
}

static void draw_cached_audio_icon(float x, float y, u32 color) {
    /* Five-pixel download arrow entering a small tray. */
    C2D_DrawRectSolid(x + 2, y, 0.6f, 1, 3, color);
    C2D_DrawRectSolid(x + 1, y + 2, 0.6f, 3, 1, color);
    C2D_DrawRectSolid(x + 2, y + 3, 0.6f, 1, 1, color);
    C2D_DrawRectSolid(x, y + 4, 0.6f, 1, 2, color);
    C2D_DrawRectSolid(x + 4, y + 4, 0.6f, 1, 2, color);
    C2D_DrawRectSolid(x, y + 6, 0.6f, 5, 1, color);
}

static void brand_pixel_text(const char *text, float x, float y, float z,
                             int scale, u32 color) {
    draw_pixel_text(text, x, y, z, scale, color, true);
}

static void panel(float x, float y, float w, float h, u32 fill, u32 border) {
    C2D_DrawRectSolid(x, y, 0.1f, w, h, border);
    C2D_DrawRectSolid(x + 2, y + 2, 0.2f, w - 4, h - 4, fill);
}

static void draw_startup_progress_bar(float x, float y, float width,
                                      float height, float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    C2D_DrawRectSolid(x, y, 0.2f, width, height, COL_GRID);
    C2D_DrawRectSolid(x + 2, y + 2, 0.3f,
                      width - 4, height - 4, COL_PANEL_2);
    float fill = (width - 4) * progress;
    if (fill > 0.0f)
        C2D_DrawRectSolid(x + 2, y + 2, 0.4f,
                          fill, height - 4, COL_CYAN);
}

static size_t utf8_prefix(const char *input, char *output, size_t output_size,
                          size_t codepoints) {
    size_t in = 0, out = 0, count = 0;
    while (input && input[in] && count < codepoints) {
        unsigned char c = (unsigned char)input[in];
        size_t bytes = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        if (out + bytes + 1 >= output_size) break;
        memcpy(output + out, input + in, bytes);
        out += bytes;
        in += bytes;
        count++;
    }
    if (input && input[in] && out + 4 < output_size) {
        memcpy(output + out, "...", 3);
        out += 3;
    }
    output[out] = '\0';
    return out;
}

static size_t utf8_codepoints(const char *value) {
    size_t count = 0;
    for (size_t i = 0; value && value[i]; count++) {
        unsigned char c = (unsigned char)value[i];
        i += c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
    }
    return count;
}

static size_t content_point_utf8_unit(const uint8_t *cursor,
                                      u32 *codepoint) {
    if (!cursor || !*cursor || !codepoint) return 0;
    *codepoint = 0xFFFDU;
    ssize_t decoded = decode_utf8(codepoint, cursor);
    return decoded > 0 ? (size_t)decoded : 1U;
}

static u32 point_glyph_or_replacement(const ImmersiveFont *font,
                                      u32 codepoint, float *advance) {
    if (immersive_font_glyph_advance(font, codepoint, advance))
        return codepoint;
    if (immersive_font_glyph_advance(font, 0x25A1U, advance))
        return 0x25A1U;
    if (advance) *advance = CONTENT_POINT_FONT_PIXELS;
    return 0;
}

static ImmersiveFont *content_point_font(Ui *ui, float pixels) {
    if (!ui) return NULL;
    if (pixels > CONTENT_POINT_FONT_PIXELS + 0.01f &&
        immersive_font_ready(&ui->content_large_point_font))
        return &ui->content_large_point_font;
    return &ui->content_point_font;
}

static void content_point_text_dimensions(ImmersiveFont *font,
                                          const char *value,
                                          float line_height,
                                          float *width, float *height) {
    float total_width = 0.0f;
    const uint8_t *cursor = (const uint8_t *)value;
    while (font && cursor && *cursor) {
        u32 codepoint;
        size_t bytes = content_point_utf8_unit(cursor, &codepoint);
        float advance = 0.0f;
        (void)point_glyph_or_replacement(font, codepoint, &advance);
        total_width += advance;
        cursor += bytes;
    }
    if (width) *width = total_width;
    if (height) *height = line_height;
}

static void content_point_text_draw(ImmersiveFont *font, const char *value,
                                    float x, float y, u32 color) {
    if (!font || !value || !value[0]) return;
    x = roundf(x);
    y = roundf(y);
    immersive_font_cache_text(font, "\xE2\x96\xA1", color);
    immersive_font_cache_text(font, value, color);
    const uint8_t *cursor = (const uint8_t *)value;
    float draw_x = x;
    while (*cursor) {
        u32 codepoint;
        size_t bytes = content_point_utf8_unit(cursor, &codepoint);
        float advance = 0.0f;
        u32 glyph = point_glyph_or_replacement(font, codepoint, &advance);
        if (glyph && immersive_font_draw_glyph(
                font, glyph,
                draw_x, y + CONTENT_POINT_FONT_Y_OFFSET,
                0.7f, color)) {}
        draw_x += advance;
        cursor += bytes;
    }
}

static void content_text_dimensions(Ui *ui, const char *value, float pixels,
                                    float *width, float *height) {
    content_point_text_dimensions(
        content_point_font(ui, pixels), value, pixels, width, height);
}

static void content_text_draw(Ui *ui, const char *value, float x, float y,
                              float pixels, u32 color) {
    content_point_text_draw(
        content_point_font(ui, pixels), value, x, y, color);
}

static float menu_text_scale(float pixels) {
    /* Citro2D normalizes BCFNT cells to a 30px logical height. */
    return pixels / 30.0f;
}

static bool menu_text_parse(Ui *ui, const char *value, C2D_Text *text) {
    if (!ui || !ui->menu_font || !ui->menu_text_buffer ||
        !value || !value[0] || !text)
        return false;
    const char *end = C2D_TextFontParse(
        text, ui->menu_font, ui->menu_text_buffer, value);
    if (!end || *end) return false;
    C2D_TextOptimize(text);
    return true;
}

static void menu_text_dimensions(Ui *ui, const char *value, float pixels,
                                 float *width, float *height) {
    C2D_Text text;
    if (!menu_text_parse(ui, value, &text)) {
        content_text_dimensions(ui, value, pixels, width, height);
        return;
    }
    float scale = menu_text_scale(pixels);
    C2D_TextGetDimensions(&text, scale, scale, width, height);
}

static void menu_text_draw(Ui *ui, const char *value, float x, float y,
                           float pixels, u32 color) {
    C2D_Text text;
    if (!menu_text_parse(ui, value, &text)) {
        content_text_draw(ui, value, x, y, pixels, color);
        return;
    }
    float scale = menu_text_scale(pixels);
    C2D_DrawText(&text, C2D_WithColor,
                 roundf(x), roundf(y), 0.7f, scale, scale, color);
}

static float content_prefix_fit(Ui *ui, const char *value,
                                char *output, size_t output_size,
                                size_t limit, float pixels,
                                float max_width) {
    size_t source_chars = utf8_codepoints(value);
    size_t kept = source_chars < limit ? source_chars : limit;
    utf8_prefix(value, output, output_size, kept);

    float width = 0.0f;
    content_text_dimensions(ui, output, pixels, &width, NULL);
    for (int pass = 0; pass < 8 && max_width > 0.0f &&
         width > max_width && kept > 0; pass++) {
        size_t fitted = width > 0.0f ?
            (size_t)((float)kept * max_width / width) : 0;
        if (fitted >= kept) fitted = kept - 1;
        kept = fitted;
        utf8_prefix(value, output, output_size, kept);
        content_text_dimensions(ui, output, pixels, &width, NULL);
    }
    return width;
}

static void smooth_text_fit(Ui *ui, const char *value, float x, float y,
                            UiTextStyle style,
                            float max_width, u32 color, size_t limit) {
    if (!ui || !value || !value[0]) return;
    const UiTextMetrics *metrics = text_metrics(style);
    char shortened[256];
    utf8_prefix(value, shortened, sizeof(shortened), limit);
    float width = 0.0f;
    float pixels = metrics->preferred_px;
    content_text_dimensions(ui, shortened, pixels, &width, NULL);
    if (max_width > 0.0f && width > max_width &&
        metrics->min_px < pixels) {
        pixels = metrics->min_px;
        content_text_dimensions(ui, shortened, pixels, &width, NULL);
    }
    if (max_width > 0.0f && width > max_width)
        content_prefix_fit(ui, value, shortened, sizeof(shortened), limit,
                           pixels, max_width);
    content_text_draw(ui, shortened, floorf(x + 0.5f),
                      floorf(y + 0.5f), pixels, color);
}

static void smooth_text_centered(Ui *ui, const char *value,
                                 float x, float y, float w, float h,
                                 UiTextStyle style,
                                 u32 color, size_t limit) {
    if (!ui || !value || !value[0] || w <= 0.0f || h <= 0.0f) return;
    const UiTextMetrics *metrics = text_metrics(style);
    char shortened[128];
    utf8_prefix(value, shortened, sizeof(shortened), limit);
    float width = 0.0f, height = 0.0f;
    float pixels = metrics->preferred_px;
    float inner_width = w > 6.0f ? w - 6.0f : w;
    float inner_height = h > 4.0f ? h - 4.0f : h;
    content_text_dimensions(ui, shortened, pixels, &width, &height);
    if ((width > inner_width || height > inner_height) &&
        metrics->min_px < pixels) {
        pixels = metrics->min_px;
        content_text_dimensions(ui, shortened, pixels, &width, &height);
    }
    if (width > inner_width)
        width = content_prefix_fit(ui, value, shortened, sizeof(shortened),
                                   limit, pixels, inner_width);
    content_text_dimensions(ui, shortened, pixels, &width, &height);
    float draw_x = floorf(x + (w - width) / 2.0f + 0.5f);
    float draw_y = floorf(y + (h - height) / 2.0f + 0.5f);
    content_text_draw(ui, shortened, draw_x, draw_y, pixels, color);
}

static float menu_prefix_fit(Ui *ui, const char *value,
                             char *output, size_t output_size,
                             size_t limit, float pixels,
                             float max_width) {
    size_t source_chars = utf8_codepoints(value);
    size_t kept = source_chars < limit ? source_chars : limit;
    utf8_prefix(value, output, output_size, kept);

    float width = 0.0f;
    menu_text_dimensions(ui, output, pixels, &width, NULL);
    for (int pass = 0; pass < 8 && max_width > 0.0f &&
         width > max_width && kept > 0; pass++) {
        size_t fitted = width > 0.0f ?
            (size_t)((float)kept * max_width / width) : 0;
        if (fitted >= kept) fitted = kept - 1;
        kept = fitted;
        utf8_prefix(value, output, output_size, kept);
        menu_text_dimensions(ui, output, pixels, &width, NULL);
    }
    return width;
}

static void menu_text_fit(Ui *ui, const char *value, float x, float y,
                          UiTextStyle style,
                          float max_width, u32 color, size_t limit) {
    if (!ui || !value || !value[0]) return;
    const UiTextMetrics *metrics = text_metrics(style);
    char shortened[256];
    utf8_prefix(value, shortened, sizeof(shortened), limit);
    float width = 0.0f;
    float pixels = metrics->preferred_px;
    menu_text_dimensions(ui, shortened, pixels, &width, NULL);
    if (max_width > 0.0f && width > max_width &&
        metrics->min_px < pixels) {
        pixels = metrics->min_px;
        menu_text_dimensions(ui, shortened, pixels, &width, NULL);
    }
    if (max_width > 0.0f && width > max_width)
        menu_prefix_fit(ui, value, shortened, sizeof(shortened), limit,
                        pixels, max_width);
    menu_text_draw(ui, shortened, floorf(x + 0.5f),
                   floorf(y + 0.5f), pixels, color);
}

static void menu_text_centered(Ui *ui, const char *value,
                               float x, float y, float w, float h,
                               UiTextStyle style,
                               u32 color, size_t limit) {
    if (!ui || !value || !value[0] || w <= 0.0f || h <= 0.0f) return;
    const UiTextMetrics *metrics = text_metrics(style);
    char shortened[128];
    utf8_prefix(value, shortened, sizeof(shortened), limit);
    float width = 0.0f, height = 0.0f;
    float pixels = metrics->preferred_px;
    float inner_width = w > 6.0f ? w - 6.0f : w;
    float inner_height = h > 4.0f ? h - 4.0f : h;
    menu_text_dimensions(ui, shortened, pixels, &width, &height);
    if ((width > inner_width || height > inner_height) &&
        metrics->min_px < pixels) {
        pixels = metrics->min_px;
        menu_text_dimensions(ui, shortened, pixels, &width, &height);
    }
    if (width > inner_width)
        width = menu_prefix_fit(ui, value, shortened, sizeof(shortened),
                                limit, pixels, inner_width);
    menu_text_dimensions(ui, shortened, pixels, &width, &height);
    float draw_x = floorf(x + (w - width) / 2.0f + 0.5f);
    float draw_y = floorf(y + (h - height) / 2.0f + 0.5f);
    menu_text_draw(ui, shortened, draw_x, draw_y, pixels, color);
}

static void draw_page_indicator(Ui *ui, const char *text) {
    menu_text_centered(ui, text,
                       UI_TOP_PAGE_FOOTER_X, UI_TOP_PAGE_FOOTER_Y,
                       UI_TOP_PAGE_FOOTER_WIDTH,
                       UI_TOP_PAGE_FOOTER_HEIGHT,
                       UI_TEXT_SMALL, COL_MUTED, 16);
}

static void draw_scrollbar_geometry(int x, int y, int width, int height,
                                    int thumb_y, int thumb_height,
                                    bool focused) {
    if (width <= 0 || height <= 0 || thumb_height <= 0) return;
    C2D_DrawRectSolid(x, y, 0.2f, width, height, COL_GRID);
    C2D_DrawRectSolid(x - 1, thumb_y, 0.3f, width + 2, thumb_height,
                      focused ? COL_ORANGE : COL_MUTED);
}

static void draw_content_list_scrollbar(const AppState *app,
                                        int y, int height,
                                        size_t page_first_visible,
                                        size_t visible_rows,
                                        size_t page_items) {
    int thumb_height = ui_scrollbar_thumb_height(
        height, visible_rows, page_items,
        UI_CONTENT_LIST_SCROLLBAR_MIN_THUMB_HEIGHT);
    int thumb_y = ui_scrollbar_thumb_y(
        y, height, thumb_height, page_first_visible, visible_rows,
        page_items);
    draw_scrollbar_geometry(
        UI_CONTENT_LIST_SCROLLBAR_X, y,
        UI_CONTENT_LIST_SCROLLBAR_WIDTH, height,
        thumb_y, thumb_height,
        app && app->focus == APP_FOCUS_CONTENT);
}

static void label_text(Ui *ui, const char *text, float x, float y,
                       UiTextStyle style, u32 color) {
    if (!ui || !text || !text[0]) return;
    text = i18n_text(text);
    menu_text_draw(
        ui, text, x, y, text_metrics(style)->preferred_px, color);
}

static float label_width(Ui *ui, const char *text, UiTextStyle style) {
    float width = 0.0f;
    if (!ui || !text) return 0.0f;
    text = i18n_text(text);
    menu_text_dimensions(
        ui, text, text_metrics(style)->preferred_px, &width, NULL);
    return width;
}

static void label_centered(Ui *ui, const char *text,
                           float x, float y, float w, float h,
                           UiTextStyle style, u32 color) {
    float width = label_width(ui, text, style);
    float height = text_metrics(style)->preferred_px;
    label_text(ui, text, floorf(x + (w - width) / 2.0f + 0.5f),
               floorf(y + (h - height) / 2.0f + 0.5f), style, color);
}

static bool waiting_for_playback(const AppState *app) {
    return app && app->pending_queue >= 0 &&
           (size_t)app->pending_queue < app->queue_count;
}

static const Song *current_song(const AppState *app) {
    return app && app->current_queue >= 0 &&
        (size_t)app->current_queue < app->queue_count ?
        &app->queue[app->current_queue] : NULL;
}

static const Song *display_song(const AppState *app) {
    if (!app) return NULL;
    int index = now_playing_display_index(
        app->queue_count, app->current_queue, app->pending_queue);
    return index >= 0 ? &app->queue[index] : NULL;
}

static void draw_shoulder_key(float x, const char *label) {
    const float width = 32.0f;
    C2D_DrawRectSolid(x + 4, 4, 0.2f, width - 8, 2, COL_GRID);
    C2D_DrawRectSolid(x + 2, 6, 0.2f, width - 4, 2, COL_GRID);
    C2D_DrawRectSolid(x, 8, 0.2f, width, 13, COL_GRID);
    C2D_DrawRectSolid(x + 4, 6, 0.3f, width - 8, 2, COL_PANEL_2);
    C2D_DrawRectSolid(x + 2, 8, 0.3f, width - 4, 11, COL_PANEL_2);
    C2D_DrawRectSolid(x + 5, 7, 0.4f, width - 10, 1, COL_DIM);
    C2D_DrawRectSolid(x + 4, 21, 0.1f, width - 8, 2, COL_BLACK);
    pixel_text(label, x + 13, 10, 0.5f, 1, COL_MUTED);
}

static void draw_offline_wifi_icon(float x, float y, float scale) {
    u32 signal = COL_DIM;
    C2D_DrawRectSolid(x + 2 * scale, y + 1 * scale, 0.6f,
                      10 * scale, 2 * scale, signal);
    C2D_DrawRectSolid(x, y + 3 * scale, 0.6f,
                      2 * scale, 4 * scale, signal);
    C2D_DrawRectSolid(x + 12 * scale, y + 3 * scale, 0.6f,
                      2 * scale, 4 * scale, signal);
    C2D_DrawRectSolid(x + 4 * scale, y + 6 * scale, 0.6f,
                      6 * scale, 2 * scale, signal);
    C2D_DrawRectSolid(x + 2 * scale, y + 8 * scale, 0.6f,
                      2 * scale, 3 * scale, signal);
    C2D_DrawRectSolid(x + 10 * scale, y + 8 * scale, 0.6f,
                      2 * scale, 3 * scale, signal);
    C2D_DrawRectSolid(x + 6 * scale, y + 11 * scale, 0.6f,
                      3 * scale, 3 * scale, signal);
    for (int i = 0; i < 7; i++)
        C2D_DrawRectSolid(x + (float)(1 + i * 2) * scale,
                          y + (float)(i * 2) * scale, 0.8f,
                          2 * scale, 2 * scale, COL_RED);
}

static void draw_header(const AppState *app) {
    static const char *labels[TAB_COUNT] = {
        "NOW PLAYING", "DISCOVER", "SETTINGS"
    };
    /* Keep the three labels evenly spaced between the shoulder-key hints. */
    static const float starts[TAB_COUNT] = {140, 226, 293};
    C2D_DrawRectSolid(0, 0, 0.0f, 400, 30, COL_PANEL);
    brand_pixel_text("ClouDS Music", 10, 11, 0.5f, 1, COL_TEXT);
    draw_shoulder_key(91, "L");
    draw_shoulder_key(362, "R");
    for (int i = 0; i < TAB_COUNT; i++) {
        bool active = app->tab == (AppTab)i;
        pixel_text(labels[i], starts[i], 11, 0.5f, 1,
                   active ? COL_TEXT : COL_DIM);
        if (active)
            C2D_DrawRectSolid(starts[i], 27, 0.4f,
                              strlen(labels[i]) * 6.0f,
                              3, COL_RED);
    }
    if (!app->network_online) draw_offline_wifi_icon(345, 7, 1.0f);
}

static int active_lyric(const AppState *app, const Player *player) {
    if (!app || app->lyric_count == 0) return -1;
    if (!current_song(app)) return 0;
    uint32_t now = (uint32_t)(player_position(player) * 1000.0);
    int active = -1;
    for (size_t i = 0; i < app->lyric_count; i++) {
        if (app->lyrics[i].time_ms > now) break;
        active = (int)i;
    }
    return active < 0 ? 0 : active;
}

static LyricAnimationFrame prepare_lyric_frame(Ui *ui, const AppState *app,
                                               const Player *player) {
    LyricAnimationFrame frame = {0};
    const Song *song = display_song(app);
    if (!ui || !app || !song || app->lyric_count == 0 ||
        app->lyric_song_id != song->id) {
        if (ui) lyric_animation_clear(&ui->lyric_animation);
        return frame;
    }

    int active = active_lyric(app, player);
    uint64_t now_ms = osGetTime();
    lyric_animation_update(&ui->lyric_animation, song ? song->id : 0,
                           app->lyric_count, active, now_ms);
    frame = lyric_animation_frame(&ui->lyric_animation, now_ms);
    if (!frame.ready) return frame;

    double playback_seconds = player_position(player);
    double duration_seconds = player_duration(player);
    uint64_t playback_ms = playback_seconds > 0.0 ?
                           (uint64_t)(playback_seconds * 1000.0) : 0U;
    uint64_t duration_ms = duration_seconds > 0.0 ?
                           (uint64_t)(duration_seconds * 1000.0) : 0U;
    if (playback_ms > UINT32_MAX) playback_ms = UINT32_MAX;
    if (duration_ms > UINT32_MAX) duration_ms = UINT32_MAX;

    uint32_t line_start = app->lyrics[active].time_ms;
    uint32_t line_end = 0U;
    for (size_t i = (size_t)active + 1U; i < app->lyric_count; i++) {
        if (app->lyrics[i].time_ms > line_start) {
            line_end = app->lyrics[i].time_ms;
            break;
        }
    }
    if (line_end == 0U && duration_ms > line_start)
        line_end = (uint32_t)duration_ms;
    if (line_end == 0U) {
        uint64_t fallback = (uint64_t)line_start +
            LYRIC_HORIZONTAL_FALLBACK_DURATION_MS;
        line_end = fallback > UINT32_MAX ? UINT32_MAX : (uint32_t)fallback;
    }

    frame.active_index = active;
    frame.playback_ms = (uint32_t)playback_ms;
    frame.active_line_start_ms = line_start;
    frame.active_line_end_ms = line_end;
    return frame;
}

static void finish_lyric_frame(Ui *ui, const LyricAnimationFrame *frame) {
    if (!ui) return;
    lyric_animation_finish(&ui->lyric_animation, frame);
}

static u32 color_with_alpha(u32 color, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    u32 original = color >> 24;
    u32 scaled = (u32)((float)original * alpha + 0.5f);
    return (color & 0x00FFFFFFU) | (scaled << 24);
}

static float lyric_visibility(float y) {
    if (y < LYRIC_TOP_Y)
        return (y - LYRIC_FADE_TOP_Y) /
               (LYRIC_TOP_Y - LYRIC_FADE_TOP_Y);
    if (y > LYRIC_BOTTOM_Y)
        return (LYRIC_FADE_BOTTOM_Y - y) /
               (LYRIC_FADE_BOTTOM_Y - LYRIC_BOTTOM_Y);
    return 1.0f;
}

static void set_top_screen_clip(float x, float y, float width, float height) {
    /* Top-screen render targets are stored rotated. Convert the logical
     * 400x240 Citro2D rectangle to the physical 240x400 scissor rectangle. */
    float logical_left = fmaxf(0.0f, x);
    float logical_top = fmaxf(0.0f, y);
    float logical_right = fminf(TOP_SCREEN_WIDTH, x + width);
    float logical_bottom = fminf(TOP_SCREEN_HEIGHT, y + height);
    u32 left = (u32)floorf(TOP_SCREEN_HEIGHT - logical_bottom);
    u32 top = (u32)floorf(TOP_SCREEN_WIDTH - logical_right);
    u32 right = (u32)ceilf(TOP_SCREEN_HEIGHT - logical_top);
    u32 bottom = (u32)ceilf(TOP_SCREEN_WIDTH - logical_left);
    C3D_SetScissor(GPU_SCISSOR_NORMAL, left, top, right, bottom);
}

static void draw_active_lyric(Ui *ui, const char *text,
                              float x, float y, float width,
                              const LyricAnimationFrame *frame, u32 color) {
    float pixels = text_metrics(UI_TEXT_LARGE)->preferred_px;
    float text_width = 0.0f;
    content_text_dimensions(ui, text, pixels, &text_width, NULL);
    if (text_width <= width) {
        content_text_draw(ui, text, floorf(x + 0.5f), y, pixels, color);
        return;
    }

    float offset = lyric_animation_horizontal_offset(
        text_width, width, frame->playback_ms,
        frame->active_line_start_ms, frame->active_line_end_ms);
    /* Citro3D render state is consumed when Citro2D flushes its vertex batch,
     * so isolate the clipped line between two explicit flushes. */
    C2D_Flush();
    set_top_screen_clip(x, y - 2.0f, width, LYRIC_TEXT_CLIP_HEIGHT);
    /* Native point glyphs stay crisp by moving only on whole pixels. */
    content_text_draw(ui, text,
                      lyric_animation_pixel_snap(x - offset),
                      y, pixels, color);
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

static size_t immersive_utf8_unit(const uint8_t *cursor, u32 *codepoint) {
    if (!cursor || !*cursor || !codepoint) return 0;
    *codepoint = 0xFFFDU;
    ssize_t decoded = decode_utf8(codepoint, cursor);
    return decoded > 0 ? (size_t)decoded : 1U;
}

static u32 immersive_glyph_or_replacement(Ui *ui, u32 codepoint,
                                          float *advance) {
    if (!ui) return 0;
    return point_glyph_or_replacement(
        &ui->immersive_font, codepoint, advance);
}

static float immersive_text_width(Ui *ui, const char *text) {
    float width = 0.0f;
    const uint8_t *cursor = (const uint8_t *)text;
    while (ui && cursor && *cursor) {
        u32 codepoint;
        size_t bytes = immersive_utf8_unit(cursor, &codepoint);
        float advance = 0.0f;
        (void)immersive_glyph_or_replacement(ui, codepoint, &advance);
        width += advance;
        cursor += bytes;
    }
    return width;
}

static void immersive_text_draw_scaled(
    Ui *ui, const char *text, float x, float y,
    float scale_x, float scale_y, u32 color) {
    const uint8_t *cursor = (const uint8_t *)text;
    float draw_x = x;
    while (ui && cursor && *cursor) {
        u32 codepoint;
        size_t bytes = immersive_utf8_unit(cursor, &codepoint);
        float advance = 0.0f;
        u32 glyph = immersive_glyph_or_replacement(
            ui, codepoint, &advance);
        if (glyph)
            (void)immersive_font_draw_glyph_scaled(
                &ui->immersive_font, glyph, draw_x, y, 0.7f,
                scale_x, scale_y, color);
        draw_x += advance * scale_x;
        cursor += bytes;
    }
}

static float immersive_text_prefix_fit(Ui *ui, const char *text,
                                       char *output, size_t output_size,
                                       size_t limit, float max_width) {
    size_t source_chars = utf8_codepoints(text);
    size_t kept = source_chars < limit ? source_chars : limit;
    for (;;) {
        utf8_prefix(text, output, output_size, kept);
        float width = immersive_text_width(ui, output);
        if (width <= max_width || kept == 0) return width;
        kept--;
    }
}

static void draw_immersive_centered_scaled_text(
    Ui *ui, const char *text, float center_x, float center_y,
    float parallax, float scale_x, float scale_y,
    float max_width, size_t text_limit, u32 color) {
    if (!ui || !text || !text[0] ||
        scale_x <= 0.0f || scale_y <= 0.0f) return;
    char shortened[256];
    utf8_prefix(text, shortened, sizeof(shortened), text_limit);
    const char *display = shortened;
    float width = immersive_text_width(ui, display);
    float unscaled_limit = max_width / scale_x;
    if (width > unscaled_limit) {
        width = immersive_text_prefix_fit(
            ui, text, shortened, sizeof(shortened),
            text_limit, unscaled_limit);
    }
    float height = immersive_font_glyph_height(&ui->immersive_font);
    float x = center_x - width * scale_x * 0.5f + parallax;
    float y = center_y - height * scale_y * 0.5f;
    immersive_text_draw_scaled(
        ui, display, lyric_animation_pixel_snap(x),
        lyric_animation_pixel_snap(y), scale_x, scale_y, color);
}

static void prepare_immersive_style_cache(
    Ui *ui, const AppState *app, const LyricAnimationFrame *frame) {
    if (!ui || !app || !frame) return;
    if (ui->immersive_font_song_id == app->lyric_song_id &&
        ui->immersive_font_active_index == frame->active_index &&
        ui->immersive_font_style == app->immersive_lyric_style) return;

    const char *texts[IMMERSIVE_CRAWL_LINE_COUNT];
    char shortened[IMMERSIVE_CRAWL_LINE_COUNT][160];
    u32 colors[IMMERSIVE_CRAWL_LINE_COUNT];
    size_t count = 0;
    int first = frame->active_index;
    int last = frame->active_index;
    u32 color = COL_TEXT;
    if (app->immersive_lyric_style == IMMERSIVE_LYRIC_STYLE_WHEEL) {
        first -= IMMERSIVE_WHEEL_LINE_RADIUS;
        last += IMMERSIVE_WHEEL_LINE_RADIUS;
    } else if (app->immersive_lyric_style == IMMERSIVE_LYRIC_STYLE_FLIP)
        first--;
    else if (app->immersive_lyric_style == IMMERSIVE_LYRIC_STYLE_CRAWL) {
        /* Never reveal future lyrics: the active line owns the nearest,
         * bottom row and older lines recede toward the vanishing point. */
        first -= IMMERSIVE_CRAWL_HISTORY_LINES;
        color = COL_ORANGE;
    }
    for (int index = first;
         index <= last && count < IMMERSIVE_CRAWL_LINE_COUNT;
         index++) {
        if (index < 0 || index >= (int)app->lyric_count) continue;
        if (app->immersive_lyric_style == IMMERSIVE_LYRIC_STYLE_CRAWL ||
            app->immersive_lyric_style == IMMERSIVE_LYRIC_STYLE_WHEEL) {
            size_t limit = app->immersive_lyric_style ==
                               IMMERSIVE_LYRIC_STYLE_WHEEL ?
                           IMMERSIVE_WHEEL_TEXT_LIMIT :
                           IMMERSIVE_CRAWL_TEXT_LIMIT;
            utf8_prefix(
                app->lyrics[index].text, shortened[count],
                sizeof(shortened[count]), limit);
            texts[count] = shortened[count];
        } else {
            texts[count] = app->lyrics[index].text;
        }
        u32 glyph_color = color;
        if (index != frame->active_index) {
            if (app->immersive_lyric_style ==
                    IMMERSIVE_LYRIC_STYLE_WHEEL)
                glyph_color = COL_MUTED;
            else if (app->immersive_lyric_style ==
                     IMMERSIVE_LYRIC_STYLE_FLIP)
                glyph_color = COL_DIM;
        }
        colors[count++] = glyph_color;
    }
    if (count > 0) {
        C2D_Flush();
        immersive_font_prepare_texts(
            &ui->immersive_font, texts, colors, count);
    }
    ui->immersive_font_song_id = app->lyric_song_id;
    ui->immersive_font_active_index = frame->active_index;
    ui->immersive_font_style = app->immersive_lyric_style;
}

static void draw_immersive_controls(Ui *ui, const AppState *app) {
    float alpha = immersive_lyrics_controls_alpha(
        osGetTime(), app->immersive_controls_since_ms);
    if (alpha <= 0.0f) return;
    u32 hint_color = color_with_alpha(COL_MUTED, alpha);
    label_text(ui, "B 返回", 10.0f, 7.0f,
               UI_TEXT_LABEL, hint_color);
    float switch_width = label_width(ui, "Y 切换", UI_TEXT_LABEL);
    label_text(ui, "Y 切换", TOP_SCREEN_WIDTH - switch_width - 10.0f,
               7.0f, UI_TEXT_LABEL, hint_color);
}

static void draw_immersive_wheel_row(
    Ui *ui, const AppState *app, const LyricAnimationFrame *frame,
    int index, float eye_sign, float stereo_slider) {
    if (index < 0 || index >= (int)app->lyric_count) return;
    float distance = (float)index - frame->focus;
    if (fabsf(distance) > IMMERSIVE_WHEEL_VISIBLE_DISTANCE) return;
    float angle = distance * IMMERSIVE_WHEEL_ANGLE_STEP;
    float front = cosf(angle);
    if (front <= 0.0f) return;
    float prominence = front * front;
    float center_y = IMMERSIVE_LYRIC_CENTER_Y +
                     sinf(angle) * IMMERSIVE_WHEEL_Y_RADIUS;
    float scale_x = 0.38f + prominence * 0.82f;
    float scale_y = scale_x * (0.72f + front * 0.28f);
    float parallax = lyric_animation_pixel_snap(
        eye_sign * stereo_slider *
        lyric_animation_immersive_eye_shift(index, frame->focus));
    float alpha = 0.48f + prominence * 0.52f;
    u32 base_color = index == frame->active_index ? COL_TEXT : COL_MUTED;
    draw_immersive_centered_scaled_text(
        ui, app->lyrics[index].text,
        TOP_SCREEN_WIDTH * 0.5f, center_y,
        parallax, scale_x, scale_y,
        IMMERSIVE_LYRIC_TEXT_WIDTH + 10.0f,
        IMMERSIVE_WHEEL_TEXT_LIMIT,
        color_with_alpha(base_color, alpha));
}

static void draw_immersive_wheel(
    Ui *ui, const AppState *app, const LyricAnimationFrame *frame,
    float eye_sign, float stereo_slider) {
    int active = frame->active_index;
    if (active < 0 || active >= (int)app->lyric_count) return;
    for (int index = active - IMMERSIVE_WHEEL_LINE_RADIUS;
         index <= active + IMMERSIVE_WHEEL_LINE_RADIUS;
         index++) {
        if (index == active) continue;
        draw_immersive_wheel_row(
            ui, app, frame, index, eye_sign, stereo_slider);
    }
    /* Draw the audible line last so it stays crisp where wheel rows overlap. */
    draw_immersive_wheel_row(
        ui, app, frame, active, eye_sign, stereo_slider);
}

static void draw_immersive_flip(
    Ui *ui, const AppState *app, const LyricAnimationFrame *frame,
    float eye_sign, float stereo_slider) {
    int active = frame->active_index;
    if (active < 0 || active >= (int)app->lyric_count) return;
    float progress = immersive_lyrics_flip_progress(
        frame->playback_ms, frame->active_line_start_ms);

    if (active > 0 && progress < 1.0f) {
        float previous_alpha = 1.0f - progress;
        float previous_scale_x = 1.0f - progress * 0.10f;
        float previous_scale_y = 1.0f - progress * 0.86f;
        float previous_center = IMMERSIVE_LYRIC_CENTER_Y - progress * 44.0f;
        float previous_depth = 0.11f + previous_alpha * 0.45f;
        float previous_parallax = lyric_animation_pixel_snap(
            eye_sign * stereo_slider *
            lyric_animation_immersive_depth_shift(previous_depth));
        draw_immersive_centered_scaled_text(
            ui, app->lyrics[active - 1].text,
            TOP_SCREEN_WIDTH * 0.5f, previous_center,
            previous_parallax, previous_scale_x, previous_scale_y,
            IMMERSIVE_LYRIC_TEXT_WIDTH, IMMERSIVE_FLIP_TEXT_LIMIT,
            color_with_alpha(COL_DIM, previous_alpha));
    }

    float active_scale_x = 0.90f + progress * 0.10f;
    float active_scale_y = 0.12f + progress * 0.88f;
    float active_center = IMMERSIVE_LYRIC_CENTER_Y +
                          (1.0f - progress) * 42.0f;
    float active_depth = 0.43f + progress * 0.47f;
    float active_parallax = lyric_animation_pixel_snap(
        eye_sign * stereo_slider *
        lyric_animation_immersive_depth_shift(active_depth));
    draw_immersive_centered_scaled_text(
        ui, app->lyrics[active].text,
        TOP_SCREEN_WIDTH * 0.5f, active_center,
        active_parallax, active_scale_x, active_scale_y,
        IMMERSIVE_LYRIC_TEXT_WIDTH, IMMERSIVE_FLIP_TEXT_LIMIT,
        color_with_alpha(COL_TEXT, 0.20f + progress * 0.80f));
}

static void draw_immersive_fade(
    Ui *ui, const AppState *app, const LyricAnimationFrame *frame,
    float eye_sign, float stereo_slider) {
    int active = frame->active_index;
    if (active < 0 || active >= (int)app->lyric_count) return;
    float alpha = immersive_lyrics_fade_alpha(
        frame->playback_ms, frame->active_line_start_ms,
        frame->active_line_end_ms);
    if (alpha <= 0.0f) return;

    /* Keep position and scale fixed: the timestamp itself supplies the sudden
     * entrance, while opacity is the only motion during the line. */
    float parallax = lyric_animation_pixel_snap(
        eye_sign * stereo_slider *
        lyric_animation_immersive_depth_shift(0.90f));
    draw_immersive_centered_scaled_text(
        ui, app->lyrics[active].text,
        TOP_SCREEN_WIDTH * 0.5f, IMMERSIVE_LYRIC_CENTER_Y,
        parallax, 1.0f, 1.0f,
        IMMERSIVE_LYRIC_TEXT_WIDTH, IMMERSIVE_FLIP_TEXT_LIMIT,
        color_with_alpha(COL_TEXT, alpha));
}

static float immersive_crawl_visibility(float depth) {
    if (depth <= 0.20f || depth >= 1.03f) return 0.0f;
    float alpha = 1.0f;
    if (depth < 0.34f) alpha = (depth - 0.20f) / 0.14f;
    if (depth > 0.94f) {
        float bottom = (1.05f - depth) / 0.11f;
        if (bottom < alpha) alpha = bottom;
    }
    return alpha;
}

static void draw_immersive_crawl(
    Ui *ui, const AppState *app, const LyricAnimationFrame *frame,
    float eye_sign, float stereo_slider) {
    int active = frame->active_index;
    if (active < 0 || active >= (int)app->lyric_count) return;
    float crawl = immersive_lyrics_crawl_progress(
        frame->playback_ms, frame->active_line_start_ms,
        frame->active_line_end_ms);
    float entry = immersive_lyrics_flip_progress(
        frame->playback_ms, frame->active_line_start_ms);
    for (int index = active - IMMERSIVE_CRAWL_HISTORY_LINES;
         index <= active;
         index++) {
        if (index < 0 || index >= (int)app->lyric_count) continue;
        int age = active - index;
        float depth = IMMERSIVE_CRAWL_NEAR_DEPTH -
            ((float)age + crawl) * IMMERSIVE_CRAWL_ROW_DEPTH;
        float visibility = immersive_crawl_visibility(depth);
        if (visibility <= 0.0f) continue;
        float center_y = IMMERSIVE_CRAWL_HORIZON_Y +
            IMMERSIVE_CRAWL_DEPTH_SPAN * depth * depth;
        float scale_x = 0.16f + depth * 0.94f;
        float scale_y = scale_x * (0.66f + depth * 0.18f);
        float parallax = lyric_animation_pixel_snap(
            eye_sign * stereo_slider *
            lyric_animation_immersive_depth_shift(depth));
        float emphasis = index == active ? entry : 0.58f;
        draw_immersive_centered_scaled_text(
            ui, app->lyrics[index].text,
            TOP_SCREEN_WIDTH * 0.5f, center_y,
            parallax, scale_x, scale_y,
            IMMERSIVE_LYRIC_TEXT_WIDTH + 10.0f,
            IMMERSIVE_CRAWL_TEXT_LIMIT,
            color_with_alpha(COL_ORANGE, visibility * emphasis));
    }
}

static void draw_immersive_lyrics(
    Ui *ui, const AppState *app, const LyricAnimationFrame *frame,
    float eye_sign, float stereo_slider) {
    if (!ui || !app || !frame || !frame->ready) return;
    prepare_immersive_style_cache(ui, app, frame);
    switch (app->immersive_lyric_style) {
        case IMMERSIVE_LYRIC_STYLE_FLIP:
            draw_immersive_flip(
                ui, app, frame, eye_sign, stereo_slider);
            break;
        case IMMERSIVE_LYRIC_STYLE_FADE:
            draw_immersive_fade(
                ui, app, frame, eye_sign, stereo_slider);
            break;
        case IMMERSIVE_LYRIC_STYLE_CRAWL:
            draw_immersive_crawl(
                ui, app, frame, eye_sign, stereo_slider);
            break;
        case IMMERSIVE_LYRIC_STYLE_WHEEL:
        default:
            draw_immersive_wheel(
                ui, app, frame, eye_sign, stereo_slider);
            break;
    }
    draw_immersive_controls(ui, app);
}

static void draw_song_title(Ui *ui, const Song *song, float x, float y,
                            UiTextStyle style,
                            float width, u32 color, size_t max_chars) {
    if (!song) return;
    if (song_is_vip(song)) {
        pixel_text("VIP", x, y + 5, 0.5f, 1, COL_ORANGE);
        x += 22.0f;
        width = width > 22.0f ? width - 22.0f : 1.0f;
    }
    smooth_text_fit(ui, song->title, x, y, style,
                    width, color, max_chars);
}

static void draw_now(Ui *ui, const AppState *app,
                     const LyricAnimationFrame *lyric_frame, float eye_sign,
                     float stereo_slider) {
    const Song *song = display_song(app);
    bool preparing = now_playing_display_is_pending(
        app->queue_count, app->current_queue, app->pending_queue);
    float cover_parallax = (float)ui_stereo_eye_shift(
        eye_sign, stereo_slider, UI_NOW_COVER_STEREO_DISPARITY);
    float artist_parallax = (float)ui_stereo_eye_shift(
        eye_sign, stereo_slider, UI_NOW_ARTIST_STEREO_DISPARITY);
    float title_parallax = (float)ui_stereo_eye_shift(
        eye_sign, stereo_slider, UI_NOW_TITLE_STEREO_DISPARITY);
    panel(10, 40, 138, 138, COL_PANEL, COL_GRID);
    if (song && cover_matches(&ui->cover, song->id)) {
        C2D_Image image = cover_image(&ui->cover);
        C2D_DrawImageAt(image, 15 + cover_parallax, 45,
                        0.5f, NULL, 1.0f, 1.0f);
    } else {
        (void)brand_logo_draw(
            &ui->brand_logo, 15 + cover_parallax, 45, 0.5f, 128);
    }
    label_text(ui, preparing ? "准备播放" : "正在播放", 12, 181,
               UI_TEXT_LABEL, preparing ? COL_ORANGE : COL_RED);
    if (song) {
        draw_song_title(ui, song, 12 + title_parallax, 199,
                        UI_TEXT_LARGE, 134, COL_TEXT, 24);
        smooth_text_fit(ui, song->artist, 12 + artist_parallax, 220,
                        UI_TEXT_BODY, 134, COL_MUTED, 28);
    } else {
        menu_text_fit(ui, i18n_text("还没有正在播放的歌曲"), 12, 200,
                      UI_TEXT_LARGE, 134, COL_MUTED, 16);
        label_text(ui, "打开“发现”", 12, 220,
                   UI_TEXT_LABEL, COL_MUTED);
    }

    panel(157, 40, 233, 190, COL_PANEL, COL_GRID);
    label_text(ui, "歌词", 168, 43, UI_TEXT_LABEL, COL_CYAN);
    C2D_DrawRectSolid(168, 64, 0.3f, 210, 1, COL_GRID);
    if (!lyric_frame || !lyric_frame->ready) {
        const char *empty = "选择歌曲后显示同步歌词";
        if (song) {
            if (app->lyric_song_id == song->id)
                empty = "暂无歌词";
            else if (waiting_for_playback(app) ||
                     app->extras_song_id == song->id)
                empty = "正在加载同步歌词";
            else
                empty = "同步歌词未加载";
        }
        menu_text_centered(ui, i18n_text(empty), 168, 107, 210, 44,
                           UI_TEXT_LARGE, COL_MUTED, 18);
        return;
    }
    float scroll = lyric_frame->scroll;
    int first = (int)floorf(scroll) - 1;
    int last = (int)floorf(scroll) + LYRIC_VISIBLE_ROWS + 1;
    for (int index = first; index <= last; index++) {
        if (index < 0 || index >= (int)app->lyric_count) continue;
        float y = LYRIC_TOP_Y + ((float)index - scroll) * LYRIC_ROW_HEIGHT;
        float visibility = lyric_visibility(y);
        if (visibility <= 0.0f) continue;
        if (visibility > 1.0f) visibility = 1.0f;
        y = floorf(y + 0.5f);

        float focus = lyric_animation_line_focus(index,
                                                 lyric_frame->focus);
        /* Stable rows land on distinct depth tiers. Interpolation around the
         * moving focus keeps those tiers from popping during a lyric change. */
        float parallax = eye_sign * stereo_slider *
                         lyric_animation_eye_shift(index,
                                                   lyric_frame->focus);
        if (focus > 0.0f) {
            float alpha = focus * visibility;
            C2D_DrawRectSolid(163 + parallax, y - 2, 0.3f, 221, 20,
                              color_with_alpha(COL_PANEL_2, alpha));
            C2D_DrawRectSolid(163 + parallax, y - 2, 0.4f, 3, 20,
                              color_with_alpha(COL_RED, alpha));
        }
        u32 text_color = index == lyric_frame->active_index ?
            COL_TEXT : COL_DIM;
        u32 visible_text_color = color_with_alpha(text_color, visibility);
        if (index == lyric_frame->active_index) {
            draw_active_lyric(ui, app->lyrics[index].text,
                              LYRIC_TEXT_X + parallax, y, LYRIC_TEXT_WIDTH,
                              lyric_frame, visible_text_color);
        } else {
            smooth_text_fit(ui, app->lyrics[index].text,
                            LYRIC_TEXT_X + parallax, y,
                            UI_TEXT_LARGE, LYRIC_TEXT_WIDTH,
                            visible_text_color, 36);
        }
    }
}

static int album_window_start(const AppState *app) {
    if (!app || app->album_track_count <= NM3DS_ALBUM_VISIBLE_ROWS) return 0;
    int first = app->album_track_selected -
                NM3DS_ALBUM_VISIBLE_ROWS / 2;
    if (first < 0) first = 0;
    if (first + NM3DS_ALBUM_VISIBLE_ROWS > (int)app->album_track_count)
        first = (int)app->album_track_count - NM3DS_ALBUM_VISIBLE_ROWS;
    return first < 0 ? 0 : first;
}

static void draw_album_song_row(Ui *ui, const AppState *app, int index,
                                int row) {
    const Song *song = &app->album_tracks[index];
    float y = (float)(UI_ALBUM_FIRST_ROW_Y + row * UI_ALBUM_ROW_STEP);
    bool selected = app->focus == APP_FOCUS_CONTENT &&
                    index == app->album_track_selected;
    if (selected) {
        C2D_DrawRectSolid(161, y - 2, 0.2f, 216,
                          UI_ALBUM_ROW_STEP - 1, COL_PANEL_2);
        C2D_DrawRectSolid(161, y - 2, 0.3f, 3,
                          UI_ALBUM_ROW_STEP - 1, COL_RED);
    }
    char number[5];
    size_t absolute = app->album_track_offset + (size_t)index + 1U;
    snprintf(number, sizeof(number), "%03u",
             (unsigned int)(absolute > 999U ? absolute % 1000U : absolute));
    pixel_text(number, 168, y + 4, 0.5f, 1,
               selected ? COL_ORANGE : COL_DIM);
    draw_song_title(ui, song, 192, y,
                    UI_TEXT_LARGE, 124,
                    selected ? COL_TEXT : COL_MUTED, 22);
    smooth_text_fit(ui, song->artist, 322, y,
                    UI_TEXT_SMALL, 52,
                    selected ? COL_CYAN : COL_DIM, 12);
}

static void draw_album(Ui *ui, const AppState *app) {
    panel(10, 40, 138, 138, COL_PANEL, COL_GRID);
    const Song *song = display_song(app);
    if (song && cover_matches(&ui->cover, song->id)) {
        C2D_Image image = cover_image(&ui->cover);
        C2D_DrawImageAt(image, 15, 45, 0.5f, NULL, 1.0f, 1.0f);
    } else {
        (void)brand_logo_draw(&ui->brand_logo, 15, 45, 0.5f, 128);
    }
    label_text(ui, "专辑", 12, 181, UI_TEXT_LABEL, COL_ORANGE);
    smooth_text_fit(ui,
                    app->album_name[0] ? app->album_name :
                                         i18n_text("未知专辑"),
                    12, 199, UI_TEXT_LARGE, 134, COL_TEXT, 22);
    char total[32];
    i18n_snprintf(total, sizeof(total), "%u 首歌曲",
                  (unsigned int)app->album_track_total);
    menu_text_fit(ui, total, 12, 220,
                  UI_TEXT_BODY, 134, COL_MUTED, 16);

    panel(157, 40, 233, 190, COL_PANEL,
          app->focus == APP_FOCUS_CONTENT ? COL_CYAN : COL_GRID);
    label_text(ui, "专辑歌曲", 168, 43, UI_TEXT_LABEL, COL_CYAN);
    label_text(ui, "B 返回",
               378.0f - label_width(ui, "B 返回", UI_TEXT_LABEL), 43,
               UI_TEXT_LABEL, COL_MUTED);
    C2D_DrawRectSolid(168, 64, 0.3f, 210, 1, COL_GRID);
    if (app->album_track_count == 0) {
        const char *message = app->mode == APP_LOADING_ALBUM ?
            "正在加载专辑歌曲" : "这个专辑没有可显示的歌曲";
        menu_text_centered(ui, i18n_text(message),
                           166, 112, 212, 44,
                           UI_TEXT_LARGE, COL_MUTED, 20);
    } else {
        int first = album_window_start(app);
        for (int row = 0;
             row < NM3DS_ALBUM_VISIBLE_ROWS &&
             first + row < (int)app->album_track_count; row++)
            draw_album_song_row(ui, app, first + row, row);
    }

    C2D_DrawRectSolid(UI_ALBUM_SCROLLBAR_X, UI_ALBUM_SCROLLBAR_Y, 0.4f,
                      UI_ALBUM_SCROLLBAR_WIDTH, UI_ALBUM_SCROLLBAR_HEIGHT,
                      COL_GRID);
    size_t visible = app->album_track_count < NM3DS_ALBUM_VISIBLE_ROWS ?
                     app->album_track_count : NM3DS_ALBUM_VISIBLE_ROWS;
    int first = album_window_start(app);
    if (app->album_track_count > 0) {
        int thumb_height = ui_scrollbar_thumb_height(
            UI_ALBUM_SCROLLBAR_HEIGHT, visible, app->album_track_count,
            UI_ALBUM_SCROLLBAR_MIN_THUMB_HEIGHT);
        int thumb_y = ui_scrollbar_thumb_y(
            UI_ALBUM_SCROLLBAR_Y, UI_ALBUM_SCROLLBAR_HEIGHT, thumb_height,
            (size_t)first, visible, app->album_track_count);
        C2D_DrawRectSolid(UI_ALBUM_SCROLLBAR_X, thumb_y, 0.5f,
                          UI_ALBUM_SCROLLBAR_WIDTH, thumb_height,
                          COL_CYAN);
    }
}

static void draw_compact_song_row(Ui *ui, const Song *song,
                                  unsigned int display_index,
                                  float y, bool selected) {
    if (selected) {
        C2D_DrawRectSolid(12, y - 2, 0.2f, 376, 20, COL_PANEL_2);
        C2D_DrawRectSolid(12, y - 2, 0.3f, 3, 20, COL_RED);
    }
    unsigned int display = display_index > 999U ?
                           display_index % 1000U : display_index;
    char number[4];
    snprintf(number, sizeof(number), "%02u", display);
    pixel_text(number, 20, y + 3, 0.5f, 1,
               selected ? COL_ORANGE : COL_DIM);
    draw_song_title(ui, song, 45, y,
                    UI_TEXT_LARGE, 211,
                    selected ? COL_TEXT : COL_MUTED, 32);
    smooth_text_fit(ui, song->artist, 270, y,
                    UI_TEXT_SMALL, 116,
                    selected ? COL_CYAN : COL_MUTED, 20);
}

static void draw_discover_loading_panel(Ui *ui, const char *message) {
    panel(20, 76, 360, 120, COL_PANEL, COL_GRID);
    menu_text_centered(ui, i18n_text(message),
                       20, 108, 360, 34,
                       UI_TEXT_BODY, COL_TEXT, 22);
    menu_text_centered(ui, i18n_text("B 取消"),
                       20, 140, 360, 30,
                       UI_TEXT_SMALL, COL_MUTED, 16);
}

static void draw_discover_recommendations(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 推荐", 10, 34, UI_TEXT_LABEL, COL_ORANGE);
    label_text(ui, "B 返回",
               388.0f - label_width(ui, "B 返回", UI_TEXT_LABEL), 34,
               UI_TEXT_LABEL, COL_MUTED);
    menu_text_fit(ui, i18n_text(
                  app->discover_source == RECOMMEND_SOURCE_DAILY ?
                      "每日个性化推荐" :
                      "公开新歌 · 无需登录"),
                  145, 35, UI_TEXT_LARGE, 170,
                  COL_MUTED, 28);
    if (app->mode == APP_LOADING_DISCOVER) {
        draw_discover_loading_panel(ui, "加载中");
        return;
    }
    if (app->discover_count == 0) {
        panel(20, 76, 360, 120, COL_PANEL, COL_GRID);
        const char *message =
            app->discover_source == RECOMMEND_SOURCE_DAILY ?
                "每日推荐尚未加载" : "公开新歌尚未加载";
        menu_text_fit(ui, i18n_text(message), 104, 119,
                      UI_TEXT_TITLE, 192, COL_TEXT, 20);
        label_text(ui, "A 重试", 168, 147, UI_TEXT_LABEL, COL_MUTED);
        return;
    }
    const int visible = UI_RECOMMEND_VISIBLE_ROWS;
    int first = app->discover_selected - 3;
    if (first < 0) first = 0;
    if (first + visible > (int)app->discover_count)
        first = (int)app->discover_count - visible;
    if (first < 0) first = 0;
    for (int row = 0;
         row < visible && first + row < (int)app->discover_count; row++) {
        int index = first + row;
        draw_compact_song_row(
            ui, &app->discover[index],
            (unsigned int)(app->discover_offset + (size_t)index + 1U),
            (float)(UI_RECOMMEND_FIRST_ROW_Y +
                    row * UI_RECOMMEND_ROW_STEP),
            app->focus == APP_FOCUS_CONTENT &&
            index == app->discover_selected);
    }
    draw_content_list_scrollbar(
        app, UI_RECOMMEND_SCROLLBAR_Y, UI_RECOMMEND_SCROLLBAR_HEIGHT,
        (size_t)first, UI_RECOMMEND_VISIBLE_ROWS, app->discover_count);
    char page[32];
    i18n_snprintf(page, sizeof(page), "%s 第 %u 页 %s",
             app->discover_offset ? "<" : "-",
             (unsigned int)(app->discover_offset /
                            NM3DS_RECOMMEND_RESULTS + 1),
             app->discover_has_more ? ">" : "-");
    draw_page_indicator(ui, page);
}

static void draw_library_loading(Ui *ui, const char *message) {
    draw_discover_loading_panel(ui, message);
}

static void draw_library(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 我的歌单", 10, 34, UI_TEXT_LABEL, COL_CYAN);
    label_text(ui, "B 返回",
               388.0f - label_width(ui, "B 返回", UI_TEXT_LABEL), 34,
               UI_TEXT_LABEL, COL_MUTED);
    if (!app->logged_in) {
        panel(20, 72, 360, 130, COL_PANEL, COL_GRID);
        menu_text_fit(ui, i18n_text("登录后查看我创建和收藏的歌单"),
                      84, 104,
                      UI_TEXT_TITLE, 286, COL_TEXT, 24);
        menu_text_fit(ui, i18n_text("使用网易云音乐手机端扫码"),
                      98, 133,
                      UI_TEXT_LARGE, 268, COL_MUTED, 22);
        label_text(ui, "A 登录", 168, 165, UI_TEXT_LABEL, COL_CYAN);
        return;
    }
    if (app->mode == APP_LOADING_LIBRARY) {
        draw_library_loading(ui, "加载中");
        return;
    }
    if (app->mode == APP_LOADING_LIBRARY_TRACKS) {
        draw_library_loading(ui, "加载中");
        return;
    }

    if (app->library_view == LIBRARY_PLAYLISTS) {
        menu_text_fit(ui, i18n_text("我创建的歌单 + 我收藏的歌单"),
                      150, 35,
                      UI_TEXT_BODY, 164, COL_MUTED, 24);
        if (app->library_playlist_count == 0) {
            const char *message = app->mode == APP_ERROR ?
                                  app->status :
                                  i18n_text("还没有可显示的歌单");
            if (app->mode == APP_ERROR)
                smooth_text_fit(ui, message, 54, 130,
                                UI_TEXT_BODY, 292, COL_RED, 42);
            else
                menu_text_fit(ui, message, 129, 130,
                              UI_TEXT_TITLE, 150, COL_MUTED, 18);
            label_text(ui, "A 重试", 170, 154,
                       UI_TEXT_LABEL, COL_MUTED);
            return;
        }
        for (int row = 0; row < (int)app->library_playlist_count; row++) {
            float y = (float)(UI_LIBRARY_PLAYLIST_FIRST_ROW_Y +
                              row * UI_LIBRARY_PLAYLIST_ROW_STEP);
            const NeteasePlaylist *playlist = &app->library_playlists[row];
            bool selected = app->focus == APP_FOCUS_CONTENT &&
                            row == app->library_playlist_selected;
            if (selected) {
                C2D_DrawRectSolid(12, y - 2, 0.2f, 376, 20, COL_PANEL_2);
                C2D_DrawRectSolid(12, y - 2, 0.3f, 3, 20, COL_ORANGE);
            }
            unsigned int display = (unsigned int)(
                app->library_playlist_offset + (size_t)row + 1U) % 100U;
            char number[3] = {(char)('0' + display / 10U),
                              (char)('0' + display % 10U), '\0'};
            pixel_text(number, 20, y + 3, 0.5f, 1,
                       selected ? COL_ORANGE : COL_DIM);
            smooth_text_fit(ui, playlist->name, 45, y,
                            UI_TEXT_LARGE, 238,
                            selected ? COL_TEXT : COL_MUTED, 34);
            const char *kind = i18n_text(
                playlist->owned ? "创建" : "收藏");
            char metadata[40];
            i18n_snprintf(metadata, sizeof(metadata), "%u 首 · %s",
                     (unsigned int)playlist->track_count, kind);
            menu_text_fit(ui, metadata, 292, y,
                          UI_TEXT_CAPTION, 94,
                          selected ?
                              (playlist->owned ? COL_ORANGE : COL_CYAN) :
                              COL_MUTED,
                          20);
        }
        draw_content_list_scrollbar(
            app, UI_LIBRARY_PLAYLIST_SCROLLBAR_Y,
            UI_LIBRARY_PLAYLIST_SCROLLBAR_HEIGHT,
            0, UI_LIBRARY_PLAYLIST_VISIBLE_ROWS,
            app->library_playlist_count);
        char page[32];
        i18n_snprintf(page, sizeof(page), "%s 第 %u 页 %s",
                 app->library_playlist_offset ? "<" : "-",
                 (unsigned int)(app->library_playlist_offset /
                                NM3DS_LIBRARY_PAGE + 1),
                 app->library_playlist_has_more ? ">" : "-");
        draw_page_indicator(ui, page);
        return;
    }

    label_text(ui, "歌单 /", 10, 47, UI_TEXT_LABEL, COL_ORANGE);
    smooth_text_fit(ui, app->library_open_name, 79, 46,
                    UI_TEXT_LARGE, 307, COL_TEXT, 40);
    if (app->library_track_count == 0) {
        menu_text_fit(ui, i18n_text("这个歌单没有可播放的歌曲"),
                      112, 130,
                      UI_TEXT_TITLE, 190, COL_MUTED, 20);
        label_text(ui, "B 返回", 176, 154, UI_TEXT_LABEL, COL_MUTED);
        return;
    }
    for (int row = 0; row < (int)app->library_track_count; row++) {
        int index = row;
        float y = (float)(UI_LIBRARY_TRACK_FIRST_ROW_Y +
                          row * UI_LIBRARY_TRACK_ROW_STEP);
        bool selected = app->focus == APP_FOCUS_CONTENT &&
                        index == app->library_track_selected;
        draw_compact_song_row(
            ui, &app->library_tracks[index],
            (unsigned int)(app->library_track_offset +
                           (size_t)index + 1U),
            y, selected);
    }
    draw_content_list_scrollbar(
        app, UI_LIBRARY_TRACK_SCROLLBAR_Y,
        UI_LIBRARY_TRACK_SCROLLBAR_HEIGHT,
        0, UI_LIBRARY_TRACK_VISIBLE_ROWS, app->library_track_count);
    char page[32];
    i18n_snprintf(page, sizeof(page), "%s 第 %u 页 %s",
             app->library_track_offset ? "<" : "-",
             (unsigned int)(app->library_track_offset /
                            NM3DS_LIBRARY_PAGE + 1),
             app->library_track_has_more ? ">" : "-");
    draw_page_indicator(ui, page);
}

static void draw_cloud(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 音乐云盘", 10, 34, UI_TEXT_LABEL, COL_CYAN);
    label_text(ui, "B 返回",
               388.0f - label_width(ui, "B 返回", UI_TEXT_LABEL), 34,
               UI_TEXT_LABEL, COL_MUTED);
    if (!app->logged_in) {
        panel(20, 72, 360, 130, COL_PANEL, COL_GRID);
        menu_text_fit(ui, i18n_text("登录后查看音乐云盘"),
                      104, 105, UI_TEXT_TITLE, 220, COL_TEXT, 24);
        menu_text_fit(ui, i18n_text("云盘文件将请求为 MP3 播放"),
                      88, 134, UI_TEXT_LARGE, 244, COL_MUTED, 22);
        label_text(ui, "A 登录", 168, 165, UI_TEXT_LABEL, COL_CYAN);
        return;
    }
    if (app->mode == APP_LOADING_CLOUD) {
        draw_discover_loading_panel(ui, "加载中");
        return;
    }
    if (app->cloud_track_count == 0) {
        panel(20, 76, 360, 120, COL_PANEL, COL_GRID);
        if (app->mode == APP_ERROR)
            smooth_text_fit(ui, app->status, 54, 119,
                            UI_TEXT_BODY, 292, COL_RED, 42);
        else
            menu_text_fit(ui, i18n_text("音乐云盘中没有歌曲"),
                          112, 119, UI_TEXT_TITLE, 190,
                          COL_MUTED, 20);
        label_text(ui, "A 重试", 168, 150, UI_TEXT_LABEL, COL_MUTED);
        return;
    }

    const NeteaseCloudTrack *selected =
        &app->cloud_tracks[app->cloud_track_selected];
    char selected_format[NM3DS_CLOUD_FORMAT_CAPACITY];
    snprintf(selected_format, sizeof(selected_format), "%s",
             selected->format[0] ? selected->format : "?");
    for (char *cursor = selected_format; *cursor; cursor++)
        if (*cursor >= 'a' && *cursor <= 'z') *cursor -= 'a' - 'A';
    char metadata[64];
    if (selected->file_size)
        i18n_snprintf(metadata, sizeof(metadata), "文件格式：%s · %.1f MB",
                      selected_format,
                      (double)selected->file_size /
                          (double)NM3DS_CACHE_MIB);
    else
        i18n_snprintf(metadata, sizeof(metadata), "文件格式：%s",
                      selected_format);
    menu_text_fit(ui, metadata, 190, 35, UI_TEXT_BODY, 124,
                  COL_MUTED, 20);

    for (int row = 0; row < (int)app->cloud_track_count; row++) {
        float y = (float)(UI_LIBRARY_TRACK_FIRST_ROW_Y +
                          row * UI_LIBRARY_TRACK_ROW_STEP);
        const NeteaseCloudTrack *track = &app->cloud_tracks[row];
        bool row_selected = app->focus == APP_FOCUS_CONTENT &&
                            row == app->cloud_track_selected;
        if (row_selected) {
            C2D_DrawRectSolid(12, y - 2, 0.2f, 376, 20, COL_PANEL_2);
            C2D_DrawRectSolid(12, y - 2, 0.3f, 3, 20, COL_ORANGE);
        }
        unsigned int display = (unsigned int)(
            app->cloud_track_offset + (size_t)row + 1U) % 1000U;
        char number[4];
        snprintf(number, sizeof(number), "%02u", display);
        pixel_text(number, 20, y + 3, 0.5f, 1,
                   row_selected ? COL_ORANGE : COL_DIM);
        draw_song_title(ui, &track->song, 45, y,
                        UI_TEXT_LARGE, 195,
                        row_selected ? COL_TEXT : COL_MUTED, 30);
        smooth_text_fit(ui, track->song.artist, 248, y,
                        UI_TEXT_SMALL, 94,
                        row_selected ? COL_CYAN : COL_MUTED, 18);
        char format[8];
        snprintf(format, sizeof(format), "%.7s",
                 track->format[0] ? track->format : "?");
        for (char *cursor = format; *cursor; cursor++)
            if (*cursor >= 'a' && *cursor <= 'z') *cursor -= 'a' - 'A';
        pixel_text(format, 390.0f - (float)strlen(format) * 6.0f,
                   y + 5, 0.5f, 1,
                   row_selected ?
                       (strcmp(format, "MP3") == 0 ? COL_CYAN : COL_ORANGE) :
                       COL_MUTED);
    }
    draw_content_list_scrollbar(
        app, UI_LIBRARY_TRACK_SCROLLBAR_Y,
        UI_LIBRARY_TRACK_SCROLLBAR_HEIGHT,
        0, UI_LIBRARY_TRACK_VISIBLE_ROWS, app->cloud_track_count);
    char page[32];
    i18n_snprintf(page, sizeof(page), "%s 第 %u 页 %s",
                  app->cloud_track_offset ? "<" : "-",
                  (unsigned int)(app->cloud_track_offset /
                                 NM3DS_CLOUD_PAGE + 1),
                  app->cloud_track_has_more ? ">" : "-");
    draw_page_indicator(ui, page);
}

static void draw_discover_home_card(Ui *ui, const AppState *app,
                                    int index, float x, float y,
                                    const char *title, const char *subtitle,
                                    u32 accent) {
    bool selected = app->focus == APP_FOCUS_CONTENT &&
                    app->discover_home_selected == index;
    panel(x, y, 185, 49, selected ? COL_PANEL_2 : COL_PANEL,
          selected ? accent : COL_GRID);
    C2D_DrawRectSolid(x + 7, y + 7, 0.4f, 4, 35,
                      selected ? accent : COL_DIM);
    label_text(ui, title, x + 20, y + 4, UI_TEXT_LABEL,
               selected ? COL_TEXT : accent);
    if (index == DISCOVER_ITEM_ACCOUNT && app->logged_in &&
        app->nickname[0])
        smooth_text_fit(ui, subtitle, x + 20, y + 25,
                        UI_TEXT_SMALL, 150,
                        selected ? COL_TEXT : COL_MUTED, 16);
    else
        menu_text_fit(ui, i18n_text(subtitle), x + 20, y + 25,
                      UI_TEXT_SMALL, 150,
                      selected ? COL_TEXT : COL_MUTED, 16);
}

static void draw_recommendation_source_card(
    Ui *ui, const AppState *app, RecommendationSource source,
    float x, const char *title, const char *subtitle, u32 accent) {
    bool selected = app->focus == APP_FOCUS_CONTENT &&
                    app->discover_source_selected == (int)source;
    bool needs_login = source == RECOMMEND_SOURCE_DAILY && !app->logged_in;
    panel(x, 72, 185, 112, selected ? COL_PANEL_2 : COL_PANEL,
          selected ? accent : COL_GRID);
    C2D_DrawRectSolid(x + 7, 80, 0.4f, 4, 96,
                      selected ? accent : COL_DIM);
    label_text(ui, title, x + 20, 84, UI_TEXT_LABEL,
               selected ? COL_TEXT : accent);
    menu_text_fit(ui, i18n_text(subtitle), x + 20, 113,
                  UI_TEXT_LARGE, 150,
                  selected ? COL_TEXT : COL_MUTED, 20);
    label_text(ui, needs_login ? "A 扫码登录" : "A 打开",
               x + 20, 153, UI_TEXT_LABEL,
               needs_login ? COL_ORANGE :
               (selected ? accent : COL_MUTED));
}

static void draw_recommendation_sources(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 推荐", 10, 34, UI_TEXT_LABEL, COL_ORANGE);
    label_text(ui, "B 返回",
               388.0f - label_width(ui, "B 返回", UI_TEXT_LABEL), 34,
               UI_TEXT_LABEL, COL_MUTED);
    menu_text_fit(ui, i18n_text("选择推荐来源"), 272, 35,
                  UI_TEXT_BODY, 116, COL_MUTED, 12);
    draw_recommendation_source_card(
        ui, app, RECOMMEND_SOURCE_PUBLIC, 10,
        "公开新歌", "无需登录 · 新歌", COL_ORANGE);
    draw_recommendation_source_card(
        ui, app, RECOMMEND_SOURCE_DAILY, 205,
        "每日推荐",
        app->logged_in ? "按偏好每日更新" : "登录后个性推荐",
        COL_CYAN);
}

static void draw_discover_home(Ui *ui, const AppState *app) {
    label_text(ui, "发现", 10, 34, UI_TEXT_LABEL, COL_ORANGE);
    menu_text_fit(ui, i18n_text("选择一个入口"), 302, 35,
                  UI_TEXT_BODY, 86, COL_MUTED, 8);
    draw_discover_home_card(ui, app, DISCOVER_ITEM_RECOMMENDATIONS,
                            10, 55, "推荐", "公开与每日推荐",
                            COL_ORANGE);
    draw_discover_home_card(ui, app, DISCOVER_ITEM_LIBRARY,
                            205, 55, "我的歌单", "创建和收藏的歌单",
                            COL_CYAN);
    draw_discover_home_card(ui, app, DISCOVER_ITEM_CLOUD,
                            10, 110, "音乐云盘", "我的云盘歌曲",
                            COL_ORANGE);
    draw_discover_home_card(ui, app, DISCOVER_ITEM_SEARCH,
                            205, 110, "搜索", "歌曲、歌手或专辑",
                            COL_CYAN);
    if (app->logged_in) {
        draw_discover_home_card(ui, app, DISCOVER_ITEM_ACCOUNT,
                                10, 165, "账户",
                                app->nickname[0] ? app->nickname :
                                                   "正在验证登录…",
                                COL_ORANGE);
    } else {
        draw_discover_home_card(ui, app, DISCOVER_ITEM_ACCOUNT,
                                10, 165, "账户", "未登录 · A 扫码登录",
                                COL_ORANGE);
    }
}

static void draw_offline_discover(Ui *ui, const AppState *app) {
    bool certificate_error = app->wifi_connected &&
                             app->network_certificate_error;
    label_text(ui, "发现 / 离线", 10, 34, UI_TEXT_LABEL, COL_RED);
    panel(36, 62, 328, 148, COL_PANEL, COL_GRID);
    draw_offline_wifi_icon(179, 76, 3.0f);
    menu_text_centered(ui, i18n_text(certificate_error ?
                           "证书校验失败" : "当前处于离线模式"),
                       64, 124, 272, 28,
                       UI_TEXT_TITLE,
                       certificate_error ? COL_ORANGE : COL_TEXT, 18);
    menu_text_centered(ui, i18n_text(certificate_error ?
                           "检查 3DS 系统日期与时间" :
                           "发现、搜索和我的歌单暂时不可用"),
                       54, 151, 292, 24,
                       UI_TEXT_LARGE, COL_MUTED, 28);
    label_centered(ui,
                   certificate_error ? "检查后按 A 重试" :
                   app->wifi_connected ? "A 重试网络" : "请连接 Wi-Fi",
                   104, 179, 192, 22, UI_TEXT_LABEL,
                   app->wifi_connected ? COL_CYAN : COL_DIM);
}

static void draw_search(Ui *ui, const AppState *app);

static void draw_discover(Ui *ui, const AppState *app) {
    if (!app->network_online) draw_offline_discover(ui, app);
    else if (app->discover_section == DISCOVER_HOME) draw_discover_home(ui, app);
    else if (app->discover_section == DISCOVER_RECOMMENDATION_SOURCES)
        draw_recommendation_sources(ui, app);
    else if (app->discover_section == DISCOVER_LIBRARY) draw_library(ui, app);
    else if (app->discover_section == DISCOVER_CLOUD) draw_cloud(ui, app);
    else if (app->discover_section == DISCOVER_SEARCH) draw_search(ui, app);
    else draw_discover_recommendations(ui, app);
}

static void draw_search(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 搜索", 12, 35, UI_TEXT_LABEL, COL_CYAN);
    panel(12, 52, 376, 38, COL_PANEL, COL_GRID);
    C2D_DrawCircleSolid(31, 69, 0.5f, 8, COL_RED);
    C2D_DrawCircleSolid(31, 69, 0.6f, 5, COL_PANEL);
    C2D_DrawRectSolid(36, 75, 0.6f, 9, 3, COL_RED);
    if (app->query[0])
        smooth_text_fit(ui, app->query, 51, 57,
                        UI_TEXT_TITLE, 276, COL_TEXT, 40);
    else menu_text_fit(ui, i18n_text("按 A 或 X 打开拼音输入法"),
                       51, 61, UI_TEXT_LARGE, 276,
                       COL_MUTED, 28);
    label_text(ui, "X 编辑", 337, 64, UI_TEXT_LABEL, COL_ORANGE);
    if (app->search_page.loading) {
        char loading[64];
        i18n_snprintf(loading, sizeof(loading),
                      "搜索中… 第 %u 页",
                      (unsigned int)(app->search_page.pending_offset /
                                     NM3DS_MAX_RESULTS + 1));
        menu_text_fit(ui, loading,
                      145, 147, UI_TEXT_TITLE, 235,
                      COL_MUTED, 24);
        return;
    }
    if (app->search_count == 0) {
        menu_text_fit(ui, i18n_text(
                      app->query[0] ? "没有搜索结果" :
                                      "歌曲、歌手或专辑"),
                      145, 147, UI_TEXT_TITLE, 235,
                      COL_MUTED, 18);
        return;
    }
    int first = app->search_selected - 2;
    if (first < 0) first = 0;
    if (first + UI_SEARCH_VISIBLE_ROWS > (int)app->search_count)
        first = (int)app->search_count - UI_SEARCH_VISIBLE_ROWS;
    if (first < 0) first = 0;
    for (int row = 0;
         row < UI_SEARCH_VISIBLE_ROWS &&
         first + row < (int)app->search_count; row++) {
        int index = first + row;
        float y = (float)(UI_SEARCH_FIRST_ROW_Y +
                          row * UI_SEARCH_ROW_STEP);
        bool selected = app->focus == APP_FOCUS_CONTENT &&
                        index == app->search_selected;
        draw_compact_song_row(
            ui, &app->search[index], (unsigned int)index + 1U,
            y, selected);
    }
    draw_content_list_scrollbar(
        app, UI_SEARCH_SCROLLBAR_Y, UI_SEARCH_SCROLLBAR_HEIGHT,
        (size_t)first, UI_SEARCH_VISIBLE_ROWS, app->search_count);
    char page[32];
    i18n_snprintf(page, sizeof(page), "%s 第 %u 页 %s",
             app->search_page.committed_offset ? "<" : "-",
             (unsigned int)(app->search_page.committed_offset /
                            NM3DS_MAX_RESULTS + 1),
             app->search_has_more ? ">" : "-");
    draw_page_indicator(ui, page);
}

static int settings_item_y(int item) {
    switch (item) {
        case SETTINGS_LANGUAGE: return UI_SETTINGS_LANGUAGE_Y;
        case SETTINGS_CACHE_LIMIT: return UI_SETTINGS_LIMIT_Y;
        case SETTINGS_DEBUG_LOGGING: return UI_SETTINGS_DEBUG_Y;
        case SETTINGS_CACHE_CLEAR: return UI_SETTINGS_CLEAR_Y;
        case SETTINGS_CONTACT: return UI_SETTINGS_CONTACT_Y;
        case SETTINGS_REPOSITORY: return UI_SETTINGS_REPOSITORY_Y;
        case SETTINGS_USAGE_NOTICE: return UI_SETTINGS_USAGE_NOTICE_Y;
        case SETTINGS_VERSION: return UI_SETTINGS_VERSION_Y;
        default: return UI_SETTINGS_LANGUAGE_Y;
    }
}

static int settings_item_height(int item) {
    switch (item) {
        case SETTINGS_LANGUAGE: return UI_SETTINGS_LANGUAGE_HEIGHT;
        case SETTINGS_CACHE_LIMIT: return UI_SETTINGS_LIMIT_HEIGHT;
        case SETTINGS_DEBUG_LOGGING: return UI_SETTINGS_DEBUG_HEIGHT;
        case SETTINGS_CACHE_CLEAR: return UI_SETTINGS_CLEAR_HEIGHT;
        case SETTINGS_CONTACT: return UI_SETTINGS_CONTACT_HEIGHT;
        case SETTINGS_REPOSITORY: return UI_SETTINGS_REPOSITORY_HEIGHT;
        case SETTINGS_USAGE_NOTICE: return UI_SETTINGS_USAGE_NOTICE_HEIGHT;
        case SETTINGS_VERSION: return UI_SETTINGS_VERSION_HEIGHT;
        default: return UI_SETTINGS_LANGUAGE_HEIGHT;
    }
}

static bool settings_item_focused(const AppState *app, int item) {
    return app->focus == APP_FOCUS_CONTENT &&
           app->settings_selected == item;
}

static void draw_settings_down_hint(void) {
    float center_x = UI_SETTINGS_DOWN_HINT_X +
                     UI_SETTINGS_DOWN_HINT_WIDTH / 2.0f;
    float y = UI_SETTINGS_DOWN_HINT_Y;
    C2D_DrawRectSolid(UI_SETTINGS_DOWN_HINT_X,
                      UI_SETTINGS_DOWN_HINT_Y, 0.6f,
                      UI_SETTINGS_DOWN_HINT_WIDTH,
                      UI_SETTINGS_DOWN_HINT_HEIGHT, COL_BG);
    C2D_DrawRectSolid(center_x - 2, y + 3, 0.7f,
                      4, 6, COL_ORANGE);
    C2D_DrawTriangle(center_x - 7, y + 7, COL_ORANGE,
                     center_x + 7, y + 7, COL_ORANGE,
                     center_x, y + 15, COL_ORANGE, 0.7f);
}

static void draw_settings(Ui *ui, const AppState *app) {
    int selected_y = settings_item_y(app->settings_selected);
    int selected_height = settings_item_height(app->settings_selected);
    int scroll_offset = ui_settings_scroll_offset_for_row(
        selected_y, selected_height);

    label_text(ui, "设置", 10, 34, UI_TEXT_LABEL, COL_ORANGE);
    char position[24];
    snprintf(position, sizeof(position), "UD %d/%d",
             app->settings_selected + 1, SETTINGS_ITEM_COUNT);
    pixel_text(position, 346, 39, 0.5f, 1, COL_MUTED);

    /* Rows outside the viewport are skipped instead of drawing under the
     * fixed title. The selected row determines the smallest downward scroll
     * needed to keep the whole row visible. */
    int cache_y = UI_SETTINGS_CACHE_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_CACHE_Y,
                                   UI_SETTINGS_CACHE_HEIGHT,
                                   scroll_offset)) {
        panel(10, cache_y, 380, UI_SETTINGS_CACHE_HEIGHT,
              COL_PANEL, COL_GRID);
        label_text(ui, "缓存空间", 22, cache_y + 1,
                   UI_TEXT_LABEL, COL_CYAN);
        char usage[64];
        bool unlimited = cache_limit_is_unlimited(app->cache_limit);
        if (unlimited)
            i18n_snprintf(usage, sizeof(usage), "%.1f MB",
                     (double)app->cache_bytes / (double)NM3DS_CACHE_MIB);
        else
            i18n_snprintf(usage, sizeof(usage), "%.1f / %llu MB",
                     (double)app->cache_bytes / (double)NM3DS_CACHE_MIB,
                     (unsigned long long)(app->cache_limit /
                                          NM3DS_CACHE_MIB));
        float usage_x = unlimited ?
                        304.0f - strlen(usage) * 6.0f : 260.0f;
        pixel_text(usage, usage_x, cache_y + 9, 0.5f, 1,
                   !unlimited && app->cache_bytes > app->cache_limit ?
                       COL_ORANGE : COL_TEXT);
        if (unlimited)
            menu_text_centered(ui, i18n_text("无上限"),
                               312, cache_y + 1, 66, 18,
                               UI_TEXT_TINY, COL_CYAN, 12);
        float ratio = !unlimited && app->cache_limit ?
                      (float)((double)app->cache_bytes /
                              (double)app->cache_limit) : 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        C2D_DrawRectSolid(22, cache_y + 17, 0.2f, 356, 4, COL_GRID);
        C2D_DrawRectSolid(24, cache_y + 18, 0.3f, 352 * ratio, 2,
                          !unlimited && app->cache_bytes > app->cache_limit ?
                              COL_ORANGE : COL_RED);
        char count[16];
        label_text(ui, "音频", 22, cache_y + 20,
                   UI_TEXT_TINY, COL_MUTED);
        i18n_snprintf(count, sizeof(count), "%u",
                 (unsigned int)app->cache_audio_files);
        pixel_text(count, 75, cache_y + 29, 0.5f, 1, COL_MUTED);
        label_text(ui, "封面", 130, cache_y + 20,
                   UI_TEXT_TINY, COL_MUTED);
        i18n_snprintf(count, sizeof(count), "%u",
                 (unsigned int)app->cache_cover_files);
        pixel_text(count, 190, cache_y + 29, 0.5f, 1, COL_MUTED);
        label_text(ui, "歌词", 238, cache_y + 20,
                   UI_TEXT_TINY, COL_MUTED);
        i18n_snprintf(count, sizeof(count), "%u",
                 (unsigned int)app->cache_lyric_files);
        pixel_text(count, 300, cache_y + 29, 0.5f, 1, COL_MUTED);
    }

    int language_y = UI_SETTINGS_LANGUAGE_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_LANGUAGE_Y,
                                   UI_SETTINGS_LANGUAGE_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_LANGUAGE);
        panel(10, language_y, 380, UI_SETTINGS_LANGUAGE_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "语言", 22, language_y + 3, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_ORANGE);
        const char *language_labels[APP_LANGUAGE_COUNT] = {
            "中文", "English"
        };
        for (int i = 0; i < APP_LANGUAGE_COUNT; i++) {
            float x = 216.0f + i * 82.0f;
            bool active = app->language == (AppLanguage)i;
            panel(x, language_y + 1, 76, 22,
                  active ? COL_BG : COL_PANEL_2,
                  active ? COL_ORANGE : COL_GRID);
            label_centered(ui, language_labels[i], x,
                           language_y + 1, 76, 22, UI_TEXT_LABEL,
                           active ? COL_TEXT : COL_MUTED);
        }
    }

    int limit_y = UI_SETTINGS_LIMIT_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_LIMIT_Y,
                                   UI_SETTINGS_LIMIT_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_CACHE_LIMIT);
        panel(10, limit_y, 380, UI_SETTINGS_LIMIT_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "缓存上限", 22, limit_y + 5, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_ORANGE);
        pixel_text("MB", 96, limit_y + 22, 0.5f, 1, COL_DIM);
        for (int i = 0; i < NM3DS_CACHE_LIMIT_OPTION_COUNT; i++) {
            float x = UI_SETTINGS_LIMIT_OPTION_X +
                      i * UI_SETTINGS_LIMIT_OPTION_STEP;
            uint64_t option = cache_limit_option((size_t)i);
            bool staged = i == app->cache_limit_selected;
            bool applied = option == app->cache_limit;
            panel(x, limit_y + 2, UI_SETTINGS_LIMIT_OPTION_WIDTH, 22,
                  staged ? COL_BG : COL_PANEL_2,
                  staged ? COL_ORANGE : COL_GRID);
            if (cache_limit_is_unlimited(option)) {
                menu_text_centered(ui, i18n_text("不限"),
                                   x, limit_y + 2,
                                   UI_SETTINGS_LIMIT_OPTION_WIDTH, 22,
                                   UI_TEXT_TINY,
                                   staged ? COL_TEXT : COL_MUTED, 8);
            } else {
                char label[16];
                i18n_snprintf(label, sizeof(label), "%llu",
                         (unsigned long long)(option / NM3DS_CACHE_MIB));
                pixel_text(label,
                           x + (UI_SETTINGS_LIMIT_OPTION_WIDTH -
                                strlen(label) * 6.0f) / 2.0f,
                           limit_y + 10, 0.5f, 1,
                           staged ? COL_TEXT : COL_MUTED);
            }
            if (applied)
                C2D_DrawRectSolid(x + 4, limit_y + 26,
                                  0.4f,
                                  UI_SETTINGS_LIMIT_OPTION_WIDTH - 8,
                                  2, COL_CYAN);
        }
    }

    int debug_y = UI_SETTINGS_DEBUG_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_DEBUG_Y,
                                   UI_SETTINGS_DEBUG_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_DEBUG_LOGGING);
        panel(10, debug_y, 380, UI_SETTINGS_DEBUG_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "调试日志", 22, debug_y + 3, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_ORANGE);
        const char *debug_labels[2] = {"关闭日志", "开启日志"};
        for (int i = 0; i < 2; i++) {
            float x = 216.0f + i * 82.0f;
            bool active = app->debug_logging == (i != 0);
            panel(x, debug_y + 1, 76, 22,
                  active ? COL_BG : COL_PANEL_2,
                  active ? COL_ORANGE : COL_GRID);
            label_centered(ui, i18n_text(debug_labels[i]), x,
                           debug_y + 1, 76, 22, UI_TEXT_LABEL,
                           active ? COL_TEXT : COL_MUTED);
        }
    }

    int clear_y = UI_SETTINGS_CLEAR_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_CLEAR_Y,
                                   UI_SETTINGS_CLEAR_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_CACHE_CLEAR);
        panel(10, clear_y, 380, UI_SETTINGS_CLEAR_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_RED : COL_GRID);
        label_text(ui, "清理缓存", 22, clear_y + 5, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_RED);
        menu_text_fit(ui, i18n_text("保留当前播放歌曲"), 225,
                      clear_y + 5, UI_TEXT_BODY, 153, COL_MUTED, 20);
    }

    int contact_y = UI_SETTINGS_CONTACT_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_CONTACT_Y,
                                   UI_SETTINGS_CONTACT_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_CONTACT);
        panel(10, contact_y, 380, UI_SETTINGS_CONTACT_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "联系作者反馈", 22, contact_y + 3,
                   UI_TEXT_LABEL, focused ? COL_TEXT : COL_ORANGE);
        menu_text_fit(ui, i18n_text("小红书号：cadl11"),
                      32, contact_y + 22, UI_TEXT_BODY,
                      346, COL_MUTED, 32);
        menu_text_fit(ui, i18n_text("邮箱：cadl@duck.com"),
                      32, contact_y + 41, UI_TEXT_BODY,
                      346, COL_MUTED, 32);
    }

    int repository_y = UI_SETTINGS_REPOSITORY_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_REPOSITORY_Y,
                                   UI_SETTINGS_REPOSITORY_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_REPOSITORY);
        panel(10, repository_y, 380, UI_SETTINGS_REPOSITORY_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "GitHub 仓库", 22, repository_y + 3,
                   UI_TEXT_LABEL, focused ? COL_TEXT : COL_ORANGE);
        menu_text_fit(ui, "https://github.com/cadl/",
                      32, repository_y + 22, UI_TEXT_BODY,
                      346, COL_MUTED, 32);
        menu_text_fit(ui, "ClouDS-Music",
                      32, repository_y + 41, UI_TEXT_BODY,
                      346, COL_MUTED, 32);
    }

    int notice_y = UI_SETTINGS_USAGE_NOTICE_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_USAGE_NOTICE_Y,
                                   UI_SETTINGS_USAGE_NOTICE_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_USAGE_NOTICE);
        panel(10, notice_y, 380, UI_SETTINGS_USAGE_NOTICE_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        menu_text_centered(
            ui, i18n_text("开源软件，免费发布"),
            14, notice_y + 2, 372, UI_SETTINGS_USAGE_NOTICE_HEIGHT - 4,
            UI_TEXT_BODY, focused ? COL_TEXT : COL_MUTED, 40);
    }

    int version_y = UI_SETTINGS_VERSION_Y - scroll_offset;
    if (ui_settings_row_is_visible(UI_SETTINGS_VERSION_Y,
                                   UI_SETTINGS_VERSION_HEIGHT,
                                   scroll_offset)) {
        bool focused = settings_item_focused(app, SETTINGS_VERSION);
        panel(10, version_y, 380, UI_SETTINGS_VERSION_HEIGHT,
              focused ? COL_PANEL_2 : COL_PANEL,
              focused ? COL_ORANGE : COL_GRID);
        label_text(ui, "版本", 22, version_y + 5, UI_TEXT_LABEL,
                   focused ? COL_TEXT : COL_ORANGE);
        menu_text_fit(ui,
                      NM3DS_APP_VERSION "(" NM3DS_APP_RELEASE_DATE ")",
                      208, version_y + 5, UI_TEXT_BODY,
                      170, COL_MUTED, 32);
    }

    int thumb_height = ui_settings_scrollbar_thumb_height();
    int thumb_y = ui_settings_scrollbar_thumb_y(scroll_offset);
    draw_scrollbar_geometry(
        UI_SETTINGS_SCROLLBAR_X, UI_SETTINGS_SCROLLBAR_Y,
        UI_SETTINGS_SCROLLBAR_WIDTH, UI_SETTINGS_SCROLLBAR_HEIGHT,
        thumb_y, thumb_height, app->focus == APP_FOCUS_CONTENT);
    if (ui_settings_can_scroll_down(scroll_offset))
        draw_settings_down_hint();
}

static void draw_login_qr(Ui *ui, float x, float y) {
    if (!ui->qr_ready) return;
    int size = qrcodegen_getSize(ui->qr_code);
    if (size <= 0) return;
    const int quiet = 4;
    int scale = 176 / (size + quiet * 2);
    if (scale < 2) scale = 2;
    float pixels = (float)(size + quiet * 2) * scale;
    C2D_DrawRectSolid(x, y, 0.3f, pixels, pixels, COL_TEXT);
    for (int row = 0; row < size; row++) {
        for (int col = 0; col < size; col++) {
            if (qrcodegen_getModule(ui->qr_code, col, row))
                C2D_DrawRectSolid(x + (col + quiet) * scale,
                                  y + (row + quiet) * scale,
                                  0.4f, scale, scale, COL_BLACK);
        }
    }
}

static void draw_account(Ui *ui, const AppState *app) {
    label_text(ui, "发现 / 账户", 12, 35, UI_TEXT_LABEL, COL_ORANGE);
    panel(10, 52, 380, 178, COL_PANEL, COL_GRID);
    if (app->logged_in) {
        label_text(ui, "已登录", 28, 68, UI_TEXT_LABEL, COL_CYAN);
        smooth_text_fit(ui, app->nickname, 28, 91,
                        UI_TEXT_DISPLAY, 340, COL_TEXT, 32);
        char uid[48];
        i18n_snprintf(uid, sizeof(uid), "UID %lld", (long long)app->user_id);
        pixel_text(uid, 28, 121, 0.5f, 1, COL_DIM);
        menu_text_fit(ui, i18n_text("推荐页将优先显示账户每日推荐"),
                      28, 148,
                      UI_TEXT_LARGE, 340, COL_MUTED, 24);
        label_text(ui, "B 关闭", 28, 192, UI_TEXT_LABEL, COL_MUTED);
        label_text(ui, "X 退出登录", 306, 192, UI_TEXT_TINY, COL_RED);
        return;
    }
    if (app->login_qr_ready && ui->qr_ready) {
        draw_login_qr(ui, 22, 56);
        menu_text_fit(ui, i18n_text("打开网易云音乐 APP"), 220, 72,
                      UI_TEXT_TITLE, 164, COL_TEXT, 16);
        menu_text_fit(ui, i18n_text("扫码登录"), 220, 99,
                      UI_TEXT_BODY, 164, COL_TEXT, 8);
        menu_text_fit(ui, i18n_text("扫码后请在手机确认登录"), 220, 125,
                      UI_TEXT_BODY, 164, COL_MUTED, 18);
        const char *state = app->login_code == 802 ? "请在手机确认" :
                            app->login_code == 800 ? "二维码已过期" :
                            "等待扫码";
        label_text(ui, state, 220, 150, UI_TEXT_LABEL,
                   app->login_code == 802 ? COL_ORANGE :
                   app->login_code == 800 ? COL_RED : COL_CYAN);
        label_text(ui, "A 检查", 220, 177, UI_TEXT_LABEL, COL_MUTED);
        label_text(ui, "B 关闭", 220, 195, UI_TEXT_LABEL, COL_MUTED);
    } else {
        menu_text_fit(ui, i18n_text("使用网易云音乐扫码登录"),
                      86, 103,
                      UI_TEXT_TITLE, 228, COL_TEXT, 20);
        menu_text_fit(ui, i18n_text("不会在 3DS 上保存密码"),
                      112, 132,
                      UI_TEXT_LARGE, 190, COL_MUTED, 18);
        label_text(ui, "A 创建二维码", 143, 170,
                   UI_TEXT_LABEL, COL_CYAN);
        label_text(ui, "B 关闭", 164, 196, UI_TEXT_LABEL, COL_MUTED);
    }
}

static void draw_top(Ui *ui, C3D_RenderTarget *target,
                     const AppState *app,
                     const LyricAnimationFrame *lyric_frame,
                     float eye_sign,
                     float stereo_slider) {
    C2D_TargetClear(target, app->immersive_lyrics ?
                            COL_SCREEN_OFF : COL_BG);
    C2D_SceneBegin(target);
    if (app->immersive_lyrics) {
        draw_immersive_lyrics(ui, app, lyric_frame,
                              eye_sign, stereo_slider);
        return;
    }
    draw_header(app);
    if (app->account_open && !app->logged_in && !app->network_online)
        draw_offline_discover(ui, app);
    else if (app->account_open) draw_account(ui, app);
    else if (app->tab == TAB_NOW_PLAYING) {
        if (app->album_open) draw_album(ui, app);
        else draw_now(ui, app, lyric_frame, eye_sign, stereo_slider);
    }
    else if (app->tab == TAB_DISCOVER) draw_discover(ui, app);
    else draw_settings(ui, app);
}

static void time_label(char *out, size_t size, double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    unsigned int total = (unsigned int)seconds;
    i18n_snprintf(out, size, "%u:%02u", total / 60, total % 60);
}

static void draw_transport_icon(float center, float y, bool next) {
    u32 color = COL_TEXT;
    float direction = next ? 1.0f : -1.0f;
    C2D_DrawTriangle(center - 7 * direction, y, color,
                     center - 7 * direction, y + 20, color,
                     center + 7 * direction, y + 10, color, 0.7f);
    C2D_DrawRectSolid(next ? center + 9 : center - 12, y,
                      0.7f, 3, 20, color);
}

static void draw_play_icon(float x, float y, float w, float h,
                           const AppState *app, const Player *player) {
    bool paused = player_is_paused(player);
    bool active = player_is_active(player);
    bool buffering = (!active && waiting_for_playback(app)) ||
                     (!paused && player_is_buffering(player));
    bool playing = active && !paused;
    panel(x, y, w, h, buffering ? COL_PANEL_2 :
          playing ? COL_RED : COL_PANEL_2,
          buffering ? COL_ORANGE : playing ? COL_ORANGE : COL_GRID);
    float center_x = x + w / 2.0f;
    float center_y = y + h / 2.0f;
    if (buffering) {
        static const int8_t offsets[8][2] = {
            {0, -12}, {8, -8}, {12, 0}, {8, 8},
            {0, 12}, {-8, 8}, {-12, 0}, {-8, -8}
        };
        unsigned int phase = (unsigned int)((osGetTime() / 100) % 8);
        for (unsigned int i = 0; i < 8; i++)
            C2D_DrawRectSolid(center_x + offsets[i][0] - 2,
                              center_y + offsets[i][1] - 2,
                              0.7f, 4, 4,
                              i == phase ? COL_ORANGE : COL_GRID);
    } else if (playing) {
        C2D_DrawRectSolid(center_x - 8, center_y - 12, 0.7f,
                          5, 24, COL_TEXT);
        C2D_DrawRectSolid(center_x + 3, center_y - 12, 0.7f,
                          5, 24, COL_TEXT);
    } else {
        C2D_DrawTriangle(center_x - 8, center_y - 14, COL_TEXT,
                         center_x - 8, center_y + 14, COL_TEXT,
                         center_x + 12, center_y, COL_TEXT, 0.7f);
    }
}

static int queue_window_start(const AppState *app) {
    int first = app->queue_selected - UI_QUEUE_VISIBLE_ROWS / 2;
    if (first < 0) first = 0;
    if (first + UI_QUEUE_VISIBLE_ROWS > (int)app->queue_count)
        first = (int)app->queue_count - UI_QUEUE_VISIBLE_ROWS;
    return first < 0 ? 0 : first;
}

static bool page_task_is_busy(const AppState *app) {
    return app && (app->search_page.loading ||
                   app->mode == APP_SEARCHING ||
                   app->mode == APP_LOADING_DISCOVER ||
                   app->mode == APP_LOADING_LIBRARY ||
                   app->mode == APP_LOADING_LIBRARY_TRACKS ||
                   app->mode == APP_LOADING_ALBUM ||
                   app->mode == APP_BULK_ENQUEUE ||
                   app->mode == APP_LOADING_EXTRAS ||
                   app->mode == APP_RESOLVING ||
                   app->mode == APP_DOWNLOADING ||
                   app->mode == APP_BUFFERING ||
                   app->mode == APP_MANAGING_CACHE);
}

static bool queue_item_selectable(const AppState *app, int index) {
    return app && index >= 0 && (size_t)index < app->queue_count &&
           (app->network_online || app->queue_offline_playable[index]);
}

static bool queue_has_selectable_item(const AppState *app) {
    if (!app) return false;
    for (size_t i = 0; i < app->queue_count; i++)
        if (queue_item_selectable(app, (int)i)) return true;
    return false;
}

typedef struct {
    const char *key;
    const char *action;
    u32 accent;
    bool enabled;
} ControlHint;

typedef struct {
    ControlHint values[UI_CONTROL_HINT_MAX];
    size_t count;
} ControlHintList;

static void add_control_hint(ControlHintList *list, const char *key,
                             const char *action, u32 accent, bool enabled) {
    if (!list || !key || !action || list->count >= UI_CONTROL_HINT_MAX)
        return;
    list->values[list->count++] = (ControlHint){
        .key = key,
        .action = action,
        .accent = accent,
        .enabled = enabled,
    };
}

static int control_hint_required_width(Ui *ui, const ControlHint *hint) {
    float action_width = label_width(ui, hint->action, UI_TEXT_LABEL);
    float key_width = strlen(hint->key) * UI_CONTROL_KEY_CHAR_WIDTH +
                      UI_CONTROL_KEY_PADDING;
    return (int)ceilf(key_width + UI_CONTROL_ACTION_GAP + action_width);
}

static void control_hint_geometry(const UiControlHintPlan *plan,
                                  size_t index, float *x, float *y,
                                  int *cell_right) {
    UiControlHintPlacement placement = plan->placements[index];
    int row = placement.cell / UI_CONTROL_HINT_COLUMNS;
    int column = placement.cell % UI_CONTROL_HINT_COLUMNS;
    if (x) *x = column == 0 ? CONTROL_LEFT_X : CONTROL_RIGHT_X;
    if (y) {
        float first_y = plan->show_title ? CONTROL_ROW_1_Y :
                                          CONTROL_COMPACT_ROW_1_Y;
        *y = first_y + row * CONTROL_ROW_STEP;
    }
    if (cell_right) {
        *cell_right = placement.span == 2 || column != 0 ?
                      UI_CONTROL_RIGHT_CELL_RIGHT :
                      UI_CONTROL_LEFT_CELL_RIGHT;
    }
}

static uint64_t control_hint_hash_bytes(uint64_t hash,
                                        const void *bytes, size_t size) {
    const uint8_t *cursor = (const uint8_t *)bytes;
    for (size_t i = 0; i < size; i++) {
        hash ^= cursor[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static uint64_t control_hint_hash_text(uint64_t hash, const char *value) {
    return control_hint_hash_bytes(
        hash, value ? value : "", value ? strlen(value) : 0);
}

/* Citro2D uses a tilted projection for screen targets.  A logical horizontal
 * interval therefore maps to the raw framebuffer's vertical scissor axis. */
static void begin_bottom_horizontal_clip(int left, int right) {
    if (left < 0) left = 0;
    if (right > UI_BOTTOM_SCREEN_WIDTH) right = UI_BOTTOM_SCREEN_WIDTH;
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_NORMAL,
                   0, UI_BOTTOM_SCREEN_WIDTH - right,
                   UI_BOTTOM_SCREEN_HEIGHT,
                   UI_BOTTOM_SCREEN_WIDTH - left);
}

static void end_bottom_clip(void) {
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
}

static uint64_t queue_marquee_signature(const AppState *app) {
    if (!app || app->focus != APP_FOCUS_PLAYLIST ||
        app->queue_selected < 0 ||
        (size_t)app->queue_selected >= app->queue_count)
        return 0;

    const Song *song = &app->queue[app->queue_selected];
    uint64_t signature = 1469598103934665603ULL;
    signature = control_hint_hash_bytes(
        signature, &app->queue_selected, sizeof(app->queue_selected));
    signature = control_hint_hash_bytes(
        signature, &song->id, sizeof(song->id));
    signature = control_hint_hash_bytes(
        signature, &song->fee, sizeof(song->fee));
    signature = control_hint_hash_text(signature, song->title);
    return signature == 0 ? 1 : signature;
}

static void prepare_queue_marquee(Ui *ui, const AppState *app,
                                   uint64_t now_ms) {
    uint64_t signature = queue_marquee_signature(app);
    if (ui->queue_marquee_signature == signature) return;
    ui->queue_marquee_signature = signature;
    ui->queue_marquee_started_ms = now_ms;
}

static float queue_marquee_offset(Ui *ui, uint64_t now_ms,
                                   float overflow_width) {
    if (!ui || ui->queue_marquee_signature == 0 || overflow_width <= 0.0f)
        return 0.0f;
    uint64_t elapsed = now_ms >= ui->queue_marquee_started_ms ?
                       now_ms - ui->queue_marquee_started_ms : 0;
    if (elapsed >= ui_control_marquee_cycle_ms(overflow_width)) {
        ui->queue_marquee_started_ms = now_ms;
        elapsed = 0;
    }
    return ui_control_marquee_offset(elapsed, overflow_width);
}

static void draw_queue_song_title(Ui *ui, const Song *song,
                                  float x, float y, float width,
                                  u32 color, bool selected,
                                  uint64_t now_ms) {
    if (!selected) {
        draw_song_title(ui, song, x, y, UI_TEXT_SMALL,
                        width, color, 22);
        return;
    }
    if (!ui || !song) return;
    if (song_is_vip(song)) {
        pixel_text("VIP", x, y + 5, 0.5f, 1, COL_ORANGE);
        x += 22.0f;
        width = width > 22.0f ? width - 22.0f : 1.0f;
    }

    float pixels = text_metrics(UI_TEXT_SMALL)->preferred_px;
    float text_width = 0.0f;
    content_text_dimensions(ui, song->title, pixels, &text_width, NULL);
    if (text_width <= width) {
        content_text_draw(ui, song->title, x, y, pixels, color);
        return;
    }

    float offset = queue_marquee_offset(
        ui, now_ms, text_width - width);
    begin_bottom_horizontal_clip((int)floorf(x),
                                 (int)ceilf(x + width));
    content_text_draw(ui, song->title, x - offset, y, pixels, color);
    end_bottom_clip();
}

static void draw_control_hint_at(Ui *ui, const ControlHint *hint,
                                 float x, float y, int cell_right,
                                 bool marquee_active,
                                 float scroll_offset) {
    size_t key_chars = strlen(hint->key);
    float key_label_width = key_chars * UI_CONTROL_KEY_CHAR_WIDTH;
    float key_width = key_label_width + UI_CONTROL_KEY_PADDING;
    u32 border = hint->enabled ? hint->accent : COL_GRID;
    panel(x, y, key_width, 20, COL_PANEL_2, border);
    float key_x = x + (key_width - key_label_width) / 2.0f;
    pixel_text(hint->key, key_x, y + 7, 0.6f, 1,
               hint->enabled ? COL_TEXT : COL_DIM);

    int cell_x = (int)x;
    int action_x = ui_control_action_x(cell_x, key_chars);
    int action_width = ui_control_action_width(
        cell_x, cell_right, key_chars);
    if (action_width <= 0) return;
    const char *action = i18n_text(hint->action);
    float text_width = 0.0f;
    menu_text_dimensions(
        ui, action, text_metrics(UI_TEXT_LABEL)->preferred_px,
        &text_width, NULL);
    u32 color = hint->enabled ? COL_TEXT : COL_DIM;
    if (text_width > action_width && marquee_active) {
        begin_bottom_horizontal_clip(action_x, cell_right);
        menu_text_draw(
            ui, action, action_x - scroll_offset, y + 1,
            text_metrics(UI_TEXT_LABEL)->preferred_px, color);
        end_bottom_clip();
    } else if (text_width > action_width) {
        menu_text_fit(ui, action, action_x, y + 1,
                      UI_TEXT_LABEL, action_width, color, 16);
    } else {
        menu_text_draw(ui, action, action_x, y + 1,
                       text_metrics(UI_TEXT_LABEL)->preferred_px, color);
    }
}

static void draw_control_hints(Ui *ui, const char *title, u32 accent,
                               const ControlHintList *list) {
    if (!ui || !list) return;
    int required_widths[UI_CONTROL_HINT_MAX] = {0};
    for (size_t i = 0; i < list->count; i++)
        required_widths[i] = control_hint_required_width(
            ui, &list->values[i]);
    UiControlHintPlan plan;
    if (!ui_control_hint_plan(required_widths, list->count, &plan))
        return;

    size_t overflow_indices[UI_CONTROL_HINT_MAX] = {0};
    float overflow_widths[UI_CONTROL_HINT_MAX] = {0.0f};
    size_t overflow_count = 0;
    uint64_t signature = 1469598103934665603ULL;
    signature = control_hint_hash_text(signature, i18n_text(title));
    signature = control_hint_hash_bytes(signature, &plan.rows,
                                        sizeof(plan.rows));
    signature = control_hint_hash_bytes(signature, &plan.show_title,
                                        sizeof(plan.show_title));
    for (size_t i = 0; i < list->count; i++) {
        float x, y;
        int cell_right;
        control_hint_geometry(&plan, i, &x, &y, &cell_right);
        (void)y;
        int action_width = ui_control_action_width(
            (int)x, cell_right, strlen(list->values[i].key));
        float text_width = label_width(
            ui, list->values[i].action, UI_TEXT_LABEL);
        if (action_width > 0 && text_width > action_width) {
            overflow_indices[overflow_count] = i;
            overflow_widths[overflow_count] = text_width - action_width;
            overflow_count++;
        }
        signature = control_hint_hash_text(signature, list->values[i].key);
        signature = control_hint_hash_text(
            signature, i18n_text(list->values[i].action));
        signature = control_hint_hash_bytes(
            signature, &plan.placements[i], sizeof(plan.placements[i]));
        signature = control_hint_hash_bytes(
            signature, &list->values[i].enabled,
            sizeof(list->values[i].enabled));
    }
    if (signature == 0) signature = 1;

    uint64_t now_ms = osGetTime();
    if (ui->control_marquee_signature != signature) {
        ui->control_marquee_signature = signature;
        ui->control_marquee_started_ms = now_ms;
        ui->control_marquee_active = 0;
    }
    if (overflow_count > 0) {
        if (ui->control_marquee_active >= overflow_count)
            ui->control_marquee_active = 0;
        size_t active = ui->control_marquee_active;
        uint64_t elapsed = now_ms >= ui->control_marquee_started_ms ?
                           now_ms - ui->control_marquee_started_ms : 0;
        if (elapsed >= ui_control_marquee_cycle_ms(
                overflow_widths[active])) {
            ui->control_marquee_active = (active + 1) % overflow_count;
            ui->control_marquee_started_ms = now_ms;
        }
    }

    if (plan.show_title)
        label_text(ui, title, 12, PLAYER_HELP_Y + 3,
                   UI_TEXT_LABEL, accent);
    for (size_t i = 0; i < list->count; i++) {
        float x, y;
        int cell_right;
        control_hint_geometry(&plan, i, &x, &y, &cell_right);
        float offset = 0.0f;
        bool marquee_active = false;
        if (overflow_count > 0) {
            size_t active = ui->control_marquee_active;
            if (overflow_indices[active] == i) {
                marquee_active = true;
                uint64_t elapsed = now_ms >= ui->control_marquee_started_ms ?
                                   now_ms - ui->control_marquee_started_ms : 0;
                offset = ui_control_marquee_offset(
                    elapsed, overflow_widths[active]);
            }
        }
        draw_control_hint_at(
            ui, &list->values[i], x, y, cell_right,
            marquee_active, offset);
    }
}

static void draw_page_controls(Ui *ui, const AppState *app,
                               const Player *player) {
    const char *title = "播放控制";
    u32 accent = COL_RED;
    bool busy = page_task_is_busy(app);
    bool player_active = player_is_active(player);
    bool player_paused = player_active && player_is_paused(player);
    const char *pause_action = player_paused ? "继续" : "暂停";
    ControlHintList hints = {0};
    panel(5, PLAYER_HELP_Y, 183, PLAYER_HELP_H, COL_PANEL, COL_GRID);

    if (!app->network_online && app->tab == TAB_DISCOVER &&
        (!app->account_open || !app->logged_in)) {
        title = "离线模式";
        accent = COL_RED;
        add_control_hint(&hints, "A", "重试", accent, app->wifi_connected);
        add_control_hint(&hints, "B", "返回", accent, true);
        add_control_hint(&hints, "SELECT", "列表", COL_CYAN,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->account_open) {
        title = "账户控制";
        accent = COL_ORANGE;
        if (app->logged_in) {
            add_control_hint(&hints, "A", "关闭", accent, true);
            add_control_hint(&hints, "B", "返回", accent, true);
            add_control_hint(&hints, "X", "退出登录", COL_RED, true);
        } else if (app->login_qr_ready) {
            add_control_hint(&hints, "A", "检查", accent, true);
            add_control_hint(&hints, "B", "返回", accent, true);
        } else {
            add_control_hint(&hints, "A", "扫码", accent, true);
            add_control_hint(&hints, "B", "返回", accent, true);
        }
        add_control_hint(&hints, "START", "退出", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->album_open && app->focus == APP_FOCUS_CONTENT) {
        title = "专辑控制";
        accent = COL_CYAN;
        bool has_items = app->album_track_count > 0;
        add_control_hint(&hints, "UD", "滚动", accent, has_items);
        add_control_hint(&hints, "A", "播放", accent,
                          has_items && app->mode != APP_LOADING_ALBUM);
        add_control_hint(&hints, "X", "全加", accent,
                          has_items && !busy &&
                          app->queue_count < NM3DS_MAX_QUEUE);
        add_control_hint(&hints, "B", busy ? "取消" : "返回", accent, true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "<>", "翻页", accent,
                          has_items && !busy);
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->focus == APP_FOCUS_PLAYLIST) {
        title = "播放列表控制";
        accent = COL_CYAN;
        bool has_items = queue_has_selectable_item(app);
        bool current = has_items && app->queue_selected == app->current_queue;
        bool selected_pending = has_items &&
                                app->queue_selected == app->pending_queue;
        const char *primary = selected_pending ? "加载中" :
                              current && player_active ? pause_action : "播放";
        add_control_hint(&hints, "UD", "选择", accent, has_items);
        add_control_hint(&hints, "A", primary, accent,
                          has_items && !selected_pending);
        add_control_hint(&hints, "X", "删除", COL_RED, has_items);
        if (app->album_open)
            add_control_hint(&hints, "B", busy ? "取消" : "返回", accent, true);
        else if (app->tab == TAB_NOW_PLAYING && busy)
            add_control_hint(&hints, "B", "取消", accent, true);
        else if (app->tab == TAB_NOW_PLAYING)
            add_control_hint(&hints, "Y", "沉浸", COL_RED,
                              immersive_lyrics_available(app));
        else
            add_control_hint(&hints, "B", busy ? "取消" : "返回", accent, true);
        add_control_hint(&hints, "SELECT",
                          app->album_open ? "专辑" :
                          app->tab == TAB_NOW_PLAYING ? "模式" : "上屏",
                          accent, true);
        add_control_hint(&hints, "<>", "翻页", accent, has_items);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->tab == TAB_DISCOVER &&
        app->discover_section == DISCOVER_LIBRARY) {
        title = "我的歌单";
        accent = COL_CYAN;
        bool tracks = app->library_view == LIBRARY_TRACKS;
        bool has_items = app->logged_in &&
            (tracks ? app->library_track_count > 0 :
                      app->library_playlist_count > 0);
        add_control_hint(&hints, "A",
                          app->logged_in ? (tracks ? "播放" : "打开") :
                          "登录", accent, app->logged_in ? has_items : true);
        add_control_hint(&hints, tracks ? "X" : "UD",
                          tracks ? "全加" : "选择", accent,
                          tracks ? has_items && !busy &&
                                   app->queue_count < NM3DS_MAX_QUEUE :
                                   has_items);
        add_control_hint(&hints, "<>", "翻页", accent,
                          app->logged_in && !busy);
        add_control_hint(&hints, "B", busy ? "取消" : "返回", accent,
                          true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->tab == TAB_DISCOVER &&
        app->discover_section == DISCOVER_CLOUD) {
        title = "音乐云盘控制";
        accent = COL_CYAN;
        bool has_items = app->logged_in && app->cloud_track_count > 0;
        add_control_hint(&hints, "A",
                         app->logged_in ?
                             (has_items ? "播放" : "重试") : "登录",
                         accent, true);
        add_control_hint(&hints, "UD", "选择", accent, has_items);
        add_control_hint(&hints, "Y", "刷新", accent,
                         app->logged_in && !busy);
        add_control_hint(&hints, "<>", "翻页", accent,
                         app->logged_in && !busy);
        add_control_hint(&hints, "B", busy ? "取消" : "返回",
                         accent, true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                         queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    }

    if (app->tab == TAB_DISCOVER &&
        app->discover_section == DISCOVER_HOME) {
        title = "发现页控制";
        accent = COL_ORANGE;
        add_control_hint(&hints, "A", "打开", accent, true);
        add_control_hint(&hints, "DPAD", "选择", accent, true);
    } else if (app->tab == TAB_DISCOVER &&
               app->discover_section ==
                   DISCOVER_RECOMMENDATION_SOURCES) {
        title = "推荐来源";
        accent = COL_ORANGE;
        bool daily = app->discover_source_selected ==
                     RECOMMEND_SOURCE_DAILY;
        add_control_hint(&hints, "A", daily && !app->logged_in ? "登录" : "打开",
                          accent, true);
        add_control_hint(&hints, "DPAD", "选择", accent, true);
        add_control_hint(&hints, "B", "返回", accent, true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    } else if (app->tab == TAB_DISCOVER &&
               app->discover_section == DISCOVER_RECOMMENDATIONS) {
        title = "推荐页控制";
        accent = COL_ORANGE;
        add_control_hint(&hints, "A",
                          app->discover_count ? "播放" : "重试",
                          accent, true);
        add_control_hint(&hints, "X", "全加", accent,
                          app->discover_count > 0 && !busy &&
                          app->queue_count < NM3DS_MAX_QUEUE);
        add_control_hint(&hints, "Y", "刷新", accent, !busy);
        add_control_hint(&hints, "B", busy ? "取消" : "返回",
                          accent, true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    } else if (app->tab == TAB_DISCOVER &&
               app->discover_section == DISCOVER_SEARCH) {
        title = "搜索控制";
        accent = COL_CYAN;
        add_control_hint(&hints, "X", "输入", accent, true);
        add_control_hint(&hints, "A",
                          app->search_count ? "播放" : "输入",
                          accent, !app->search_page.loading);
        add_control_hint(&hints, "UD", "选择", accent,
                          !app->search_page.loading);
        add_control_hint(&hints, "<>", "翻页", accent,
                          !app->search_page.loading);
        add_control_hint(&hints, "B", busy ? "取消" : "返回",
                          accent, true);
        add_control_hint(&hints, "SELECT", "列表", accent,
                          queue_has_selectable_item(app));
        add_control_hint(&hints, "L/R", "切页", accent, true);
        draw_control_hints(ui, title, accent, &hints);
        return;
    } else if (app->tab == TAB_SETTINGS) {
        title = "设置控制";
        accent = COL_ORANGE;
        bool interactive = settings_item_is_interactive(
            app->settings_selected);
        add_control_hint(&hints, "UD", interactive ? "选择" : "浏览",
                          accent, true);
        add_control_hint(&hints, "A",
                          app->settings_selected == SETTINGS_LANGUAGE ?
                              "切换" :
                          app->settings_selected == SETTINGS_CACHE_LIMIT ?
                              "应用" :
                          app->settings_selected == SETTINGS_DEBUG_LOGGING ?
                              "切换" :
                          app->settings_selected == SETTINGS_CACHE_CLEAR ?
                              "清理" : "无操作",
                          app->settings_selected == SETTINGS_CACHE_CLEAR ?
                              COL_RED : accent, interactive);
        add_control_hint(&hints, "<>", "调整", accent,
                          settings_item_is_adjustable(
                              app->settings_selected));
        add_control_hint(&hints, "B", busy ? "取消" : "重置",
                          accent, true);
    }
    add_control_hint(&hints, "SELECT", "列表", accent,
                          app->tab != TAB_NOW_PLAYING &&
                          queue_has_selectable_item(app));
    add_control_hint(&hints, "L/R", "切页", accent, true);
    draw_control_hints(ui, title, accent, &hints);
}

static void draw_bottom_player(Ui *ui, const AppState *app, const Player *player) {
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_RED);
    uint64_t queue_now_ms = osGetTime();
    prepare_queue_marquee(ui, app, queue_now_ms);

    /* Left 3/5: page help over transport at roughly 1:3. Right 2/5: queue. */
    draw_page_controls(ui, app, player);
    panel(5, PLAYER_PANEL_Y, 183, PLAYER_PANEL_H, COL_PANEL, COL_GRID);
    panel(195, 8, 120, 207, COL_PANEL,
          app->focus == APP_FOCUS_PLAYLIST ? COL_CYAN : COL_GRID);

    double position = player_position(player);
    double duration = player_duration(player);
    bool active_current = player_is_active(player) && current_song(app);
    bool switching = active_current && waiting_for_playback(app) &&
                     app->pending_queue != app->current_queue;
    bool initial_prebuffer = waiting_for_playback(app) && !active_current;
    float playback_ratio = duration > 0.0 ?
                           (float)(position / duration) : 0.0f;
    if (app->seek_dragging) playback_ratio = app->seek_ratio;
    if (initial_prebuffer) playback_ratio = 0.0f;
    if (playback_ratio < 0.0f) playback_ratio = 0.0f;
    if (playback_ratio > 1.0f) playback_ratio = 1.0f;
    float loaded_ratio = 0.0f;
    if (switching) {
        /* The bar continues to describe the audible song.  Pending download
         * progress is shown in the status line instead of mixing two songs
         * into one timeline. */
        loaded_ratio = 1.0f;
    } else if (app->media_total_bytes > 0) {
        loaded_ratio = (float)((double)app->media_loaded_bytes /
                               (double)app->media_total_bytes);
    } else if (initial_prebuffer && app->media_start_target_bytes > 0) {
        loaded_ratio = (float)((double)app->media_loaded_bytes /
                               (double)app->media_start_target_bytes);
    }
    if (loaded_ratio < 0.0f) loaded_ratio = 0.0f;
    if (loaded_ratio > 1.0f) loaded_ratio = 1.0f;
    C2D_DrawRectSolid(PROGRESS_X, PROGRESS_Y, 0.2f,
                      PROGRESS_W, 7, COL_GRID);
    C2D_DrawRectSolid(PROGRESS_X + 2, PROGRESS_Y + 2, 0.3f,
                      (PROGRESS_W - 4) * loaded_ratio, 3, COL_DIM);
    C2D_DrawRectSolid(PROGRESS_X + 2, PROGRESS_Y + 2, 0.4f,
                      (PROGRESS_W - 4) * playback_ratio, 3, COL_RED);
    char left[16], right[16];
    time_label(left, sizeof(left), position);
    time_label(right, sizeof(right), duration);
    pixel_text(left, 12, 126, 0.5f, 1, COL_MUTED);
    pixel_text(right, 153, 126, 0.5f, 1, COL_MUTED);

    panel(PREVIOUS_X, PREVIOUS_Y, PREVIOUS_W, PREVIOUS_H,
          COL_PANEL_2, COL_GRID);
    draw_transport_icon(PREVIOUS_X + PREVIOUS_W / 2.0f,
                        PREVIOUS_Y + 9, false);
    draw_play_icon(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, app, player);
    panel(NEXT_X, NEXT_Y, NEXT_W, NEXT_H, COL_PANEL_2, COL_GRID);
    draw_transport_icon(NEXT_X + NEXT_W / 2.0f, NEXT_Y + 9, true);
    static const char *mode_labels[PLAY_MODE_COUNT] = {
        "顺序", "单曲", "随机"
    };
    const char *mode_label = mode_labels[app->play_mode];
    panel(MODE_X, MODE_Y, MODE_W, MODE_H, COL_PANEL_2, COL_GRID);
    label_centered(ui, mode_label, MODE_X, MODE_Y, MODE_W, MODE_H,
                   UI_TEXT_TINY, COL_MUTED);
    bool album_enabled = app->album_open ||
        (current_song(app) && app->network_online);
    panel(ALBUM_X, ALBUM_Y, ALBUM_W, ALBUM_H, COL_PANEL_2,
          COL_GRID);
    label_centered(ui, app->album_open ? "返回" : "查看专辑",
                   ALBUM_X, ALBUM_Y, ALBUM_W, ALBUM_H,
                   UI_TEXT_TINY, album_enabled ? COL_TEXT : COL_DIM);

    /* Use the full header width so the longest position (500/500) still
     * fits beside the four-glyph Chinese title. */
    label_text(ui, "播放列表", 198, 10, UI_TEXT_LABEL,
               app->focus == APP_FOCUS_PLAYLIST ? COL_TEXT : COL_CYAN);
    char count[16];
    unsigned int queue_position = app->queue_selected >= 0 &&
                                  (size_t)app->queue_selected < app->queue_count ?
                                  (unsigned int)app->queue_selected + 1U : 0U;
    i18n_snprintf(count, sizeof(count), "%u/%u", queue_position,
                  (unsigned int)app->queue_count);
    pixel_text(count, 312 - (float)strlen(count) * 6.0f,
               17, 0.5f, 1, COL_DIM);
    C2D_DrawRectSolid(200, 31, 0.3f, 110, 1,
                      app->focus == APP_FOCUS_PLAYLIST ? COL_CYAN : COL_GRID);
    if (app->queue_count == 0) {
        menu_text_fit(ui, i18n_text("播放列表为空"), 211, 80,
                      UI_TEXT_LARGE, 96, COL_MUTED, 9);
        menu_text_fit(ui, i18n_text("请到发现页"), 213, 104,
                      UI_TEXT_BODY, 96, COL_MUTED, 9);
        menu_text_fit(ui, i18n_text("选择歌曲"), 226, 120,
                      UI_TEXT_BODY, 90, COL_MUTED, 7);
    } else {
        int first = queue_window_start(app);
        for (int row = 0;
             row < UI_QUEUE_VISIBLE_ROWS &&
             first + row < (int)app->queue_count;
             row++) {
            int index = first + row;
            float y = QUEUE_LIST_Y + row * QUEUE_ROW_HEIGHT;
            bool selected = app->focus == APP_FOCUS_PLAYLIST &&
                            index == app->queue_selected;
            bool current = index == app->current_queue;
            bool pending = index == app->pending_queue && !current;
            bool selectable = queue_item_selectable(app, index);
            if (selected)
                C2D_DrawRectSolid(199, y, 0.2f, 112,
                                  QUEUE_ROW_HEIGHT - 2, COL_PANEL_2);
            if (current)
                C2D_DrawRectSolid(199, y, 0.4f, 3,
                                  QUEUE_ROW_HEIGHT - 2, COL_RED);
            /* Storage workers populate this flag; never touch the SD card
             * from the per-frame render path. */
            if (app->queue_offline_playable[index])
                draw_cached_audio_icon(204, y + 26, COL_CYAN);
            pixel_text(current ? ">" : pending ? "~" :
                       selectable ? "-" : "x",
                       204, y + 15, 0.5f, 1,
                       current ? COL_RED : pending ? COL_ORANGE : COL_DIM);
            draw_queue_song_title(ui, &app->queue[index],
                                  QUEUE_TITLE_X, y + 1,
                                  QUEUE_TITLE_WIDTH,
                                  !selectable ? COL_DIM :
                                  selected ? COL_TEXT : COL_MUTED,
                                  selected, queue_now_ms);
            smooth_text_fit(ui, app->queue[index].artist, 214, y + 19,
                            UI_TEXT_CAPTION, 96,
                            !selectable ? COL_DIM :
                            current ? COL_CYAN :
                            pending ? COL_ORANGE : COL_MUTED, 24);
        }
    }
    C2D_DrawRectSolid(0, UI_BOTTOM_FOOTER_Y, 0.2f,
                      UI_BOTTOM_SCREEN_WIDTH, UI_BOTTOM_FOOTER_HEIGHT,
                      COL_PANEL);
    C2D_DrawRectSolid(7, 225, 0.4f, 3, 9,
        app->mode == APP_ERROR ? COL_RED :
        (waiting_for_playback(app) || app->mode == APP_BUFFERING) ?
        COL_ORANGE : COL_CYAN);
    u32 status_color = app->mode == APP_ERROR ? COL_RED : COL_MUTED;
    smooth_text_fit(ui, app->status, UI_BOTTOM_STATUS_X, 221,
                    UI_TEXT_LARGE, UI_BOTTOM_STATUS_WIDTH,
                    status_color, 48);
}

static void draw_battery_status(const AppState *app) {
    if (!app || !app->battery_available) return;

    const float x = UI_BOTTOM_BATTERY_X;
    const float y = UI_BOTTOM_BATTERY_Y;
    const float body_width = UI_BOTTOM_BATTERY_WIDTH - 3.0f;
    u32 fill = app->battery_charging ? COL_ORANGE :
               app->battery_level <= 1U ? COL_RED : COL_CYAN;

    C2D_DrawRectSolid(x, y, 0.8f, body_width,
                      UI_BOTTOM_BATTERY_HEIGHT, COL_MUTED);
    C2D_DrawRectSolid(x + 2.0f, y + 2.0f, 0.9f,
                      body_width - 4.0f,
                      UI_BOTTOM_BATTERY_HEIGHT - 4.0f, COL_PANEL);
    C2D_DrawRectSolid(x + body_width, y + 3.0f, 0.8f, 3.0f,
                      UI_BOTTOM_BATTERY_HEIGHT - 6.0f, COL_MUTED);
    for (u8 segment = 0; segment < app->battery_level; segment++)
        C2D_DrawRectSolid(x + 2.0f + segment * 4.0f, y + 2.0f,
                          1.0f, 3.0f,
                          UI_BOTTOM_BATTERY_HEIGHT - 4.0f, fill);

    if (app->battery_charging) {
        C2D_DrawRectSolid(UI_BOTTOM_BATTERY_REGION_X + 4.0f, y, 1.0f,
                          4.0f, 4.0f, COL_ORANGE);
        C2D_DrawRectSolid(UI_BOTTOM_BATTERY_REGION_X + 2.0f, y + 3.0f,
                          1.0f, 4.0f, 4.0f, COL_ORANGE);
        C2D_DrawRectSolid(UI_BOTTOM_BATTERY_REGION_X, y + 6.0f, 1.0f,
                          4.0f, 4.0f, COL_ORANGE);
    }
}

static void draw_dsp_firmware_step(Ui *ui, int number,
                                   const char *text, float y,
                                   bool accent) {
    char label[2] = {(char)('0' + number), '\0'};
    panel(18, y, 20, 20, accent ? COL_PANEL_2 : COL_PANEL,
          accent ? COL_ORANGE : COL_CYAN);
    pixel_text(label, 25, y + 7, 0.6f, 1,
               accent ? COL_ORANGE : COL_CYAN);
    menu_text_fit(ui, i18n_text(text), 46, y + 1,
                  UI_TEXT_BODY, 250,
                  accent ? COL_CYAN : COL_TEXT, 48);
}

static void draw_dsp_firmware_dialog(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->dsp_firmware_prompt_open) return;
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_RED);
    panel(8, 8, 304, 224, COL_PANEL_2, COL_RED);

    menu_text_fit(ui, i18n_text("需要 DSP 固件"),
                  18, 16, UI_TEXT_TITLE, 215, COL_ORANGE, 24);
    char result[16];
    snprintf(result, sizeof(result), "%08lX",
             (unsigned long)app->dsp_firmware_result);
    panel(242, 16, 62, 20, COL_PANEL, COL_RED);
    pixel_text(result, 249, 23, 0.6f, 1, COL_RED);

    menu_text_fit(ui,
                  i18n_text("未找到 DSP 固件，请打开 Rosalina"),
                  18, 45, UI_TEXT_BODY, 284, COL_MUTED, 40);
    C2D_DrawRectSolid(18, 68, 0.4f, 284, 1, COL_GRID);

    draw_dsp_firmware_step(ui, 1, "按 HOME 返回主菜单", 75, false);
    draw_dsp_firmware_step(ui, 2,
                           "默认组合键：L + ↓ + SELECT", 98, true);
    draw_dsp_firmware_step(ui, 3,
                           "进入 Miscellaneous options...", 121, false);
    draw_dsp_firmware_step(ui, 4,
                           "选择 Dump DSP firmware，按 A", 144, false);
    draw_dsp_firmware_step(ui, 5, "完全退出并重启应用", 167, false);

    menu_text_fit(ui, i18n_text("需要 Luma3DS v10.3 或更高版本"),
                  18, 191, UI_TEXT_LABEL, 284, COL_MUTED, 40);
    panel(104, 209, 112, 20, COL_PANEL, COL_CYAN);
    menu_text_centered(ui, i18n_text("A / B 关闭"),
                       106, 210, 108, 17, UI_TEXT_LABEL,
                       COL_TEXT, 16);
}

static void draw_network_certificate_dialog(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->network_certificate_prompt_open) return;
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_ORANGE);
    panel(8, 8, 304, 224, COL_PANEL_2, COL_ORANGE);

    menu_text_fit(ui, i18n_text("证书校验失败"),
                  18, 18, UI_TEXT_TITLE, 284, COL_ORANGE, 24);
    menu_text_fit(ui, i18n_text("无法建立安全连接"),
                  18, 55, UI_TEXT_BODY, 284, COL_TEXT, 32);
    menu_text_fit(ui, i18n_text("检查 3DS 系统日期与时间"),
                  18, 81, UI_TEXT_BODY, 284, COL_TEXT, 32);
    C2D_DrawRectSolid(18, 111, 0.4f, 284, 1, COL_GRID);
    menu_text_fit(ui, i18n_text("日期或年份错误会导致离线"),
                  18, 127, UI_TEXT_BODY, 284, COL_MUTED, 32);
    menu_text_fit(ui, i18n_text("时间正确仍失败时，请更新应用"),
                  18, 159, UI_TEXT_BODY, 284, COL_MUTED, 32);
    panel(104, 202, 112, 22, COL_PANEL, COL_CYAN);
    menu_text_centered(ui, i18n_text("A / B 关闭"),
                       106, 204, 108, 18, UI_TEXT_LABEL,
                       COL_TEXT, 16);
}

static void draw_queue_replace_dialog(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->queue_replace_confirm) return;
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_RED);
    panel(12, 34, 296, 172, COL_PANEL_2, COL_ORANGE);
    menu_text_centered(ui, i18n_text("播放列表已满"),
                       20, 45, 280, 27, UI_TEXT_TITLE,
                       COL_ORANGE, 16);
    label_text(ui, "即将加入", 22, 78, UI_TEXT_LABEL, COL_CYAN);
    smooth_text_fit(ui, app->queue_replace_song.title,
                    22, 96, UI_TEXT_BODY, 276, COL_TEXT, 48);
    menu_text_fit(ui, i18n_text("将移除最早加入的歌曲"),
                  22, 124, UI_TEXT_LABEL, 276, COL_RED, 48);
    if (app->queue_count > 0)
        smooth_text_fit(ui, app->queue[0].title,
                        22, 142, UI_TEXT_BODY, 276, COL_MUTED, 48);

    panel(48, 174, 92, 23, COL_PANEL, COL_CYAN);
    panel(180, 174, 92, 23, COL_PANEL, COL_GRID);
    menu_text_centered(ui, i18n_text("A 确认"),
                       50, 176, 88, 19, UI_TEXT_LABEL,
                       COL_TEXT, 8);
    menu_text_centered(ui, i18n_text("B 取消"),
                       182, 176, 88, 19, UI_TEXT_LABEL,
                       COL_MUTED, 8);
}

static void draw_bulk_enqueue_confirm(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->bulk_enqueue_confirm) return;
    bool recommendations =
        app->bulk_enqueue_kind == BULK_ENQUEUE_RECOMMENDATIONS;
    bool album = app->bulk_enqueue_kind == BULK_ENQUEUE_ALBUM;
    const char *name = recommendations ?
        i18n_text(app->bulk_enqueue_recommendation_source ==
                  RECOMMEND_SOURCE_DAILY ? "每日推荐" : "公开新歌") :
        album ? app->album_name : app->library_open_name;
    const char *description = recommendations ?
        "将按页添加当前来源的全部推荐歌曲" :
        album ? "将按页添加专辑中的全部歌曲" :
                "将按页添加歌单中的全部歌曲";
    char count[48] = {0};
    if (recommendations) {
        i18n_snprintf(count, sizeof(count),
                      app->discover_total_known ?
                          "歌曲数：%u 首" : "歌曲数：至少 %u 首",
                      (unsigned int)app->discover_total_count);
    } else if (album)
        i18n_snprintf(count, sizeof(count), "歌曲数：%u 首",
                      (unsigned int)app->album_track_total);
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_RED);
    panel(12, 34, 296, 172, COL_PANEL_2, COL_CYAN);
    menu_text_centered(ui, i18n_text("全部加入播放列表"),
                       20, 45, 280, 27, UI_TEXT_TITLE,
                       COL_CYAN, 16);
    smooth_text_fit(ui, name,
                    22, 79, UI_TEXT_BODY, 276, COL_TEXT, 48);
    if (recommendations || album)
        menu_text_fit(ui, count,
                      22, 105, UI_TEXT_LABEL, 276, COL_CYAN, 24);
    menu_text_fit(ui, i18n_text(description),
                  22, recommendations || album ? 127 : 108, UI_TEXT_LABEL,
                  276, COL_MUTED, 32);
    menu_text_fit(ui, i18n_text("播放列表空间不足时将停止添加"),
                  22, recommendations || album ? 149 : 132,
                  UI_TEXT_LABEL,
                  276, COL_ORANGE, 32);
    panel(48, 174, 92, 23, COL_PANEL, COL_CYAN);
    panel(180, 174, 92, 23, COL_PANEL, COL_GRID);
    menu_text_centered(ui, i18n_text("A 确认"),
                       50, 176, 88, 19, UI_TEXT_LABEL,
                       COL_TEXT, 8);
    menu_text_centered(ui, i18n_text("B 取消"),
                       182, 176, 88, 19, UI_TEXT_LABEL,
                       COL_MUTED, 8);
}

static void draw_bulk_enqueue_progress(Ui *ui, const AppState *app) {
    if (!ui || !app || !app->bulk_enqueue_active) return;
    bool recommendations =
        app->bulk_enqueue_kind == BULK_ENQUEUE_RECOMMENDATIONS;
    bool album = app->bulk_enqueue_kind == BULK_ENQUEUE_ALBUM;
    const char *name = recommendations ?
        i18n_text(app->bulk_enqueue_recommendation_source ==
                  RECOMMEND_SOURCE_DAILY ? "每日推荐" : "公开新歌") :
        album ? app->album_name : app->library_open_name;
    size_t total_count = recommendations ? 0 :
        album ? app->album_track_total : app->library_open_track_count;
    size_t page_size = recommendations ? NM3DS_RECOMMEND_RESULTS :
        album ? NM3DS_ALBUM_PAGE : NM3DS_LIBRARY_BATCH_PAGE;
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_RED);
    panel(12, 25, 296, 190, COL_PANEL_2, COL_CYAN);
    menu_text_centered(ui, i18n_text("正在全部加入播放列表"),
                       20, 36, 280, 27, UI_TEXT_TITLE,
                       COL_CYAN, 16);
    smooth_text_fit(ui, name,
                    25, 70, UI_TEXT_BODY, 270, COL_TEXT, 48);

    size_t total_pages = total_count ?
        (total_count + page_size - 1U) / page_size :
        app->bulk_enqueue_page;
    if (total_pages < app->bulk_enqueue_page)
        total_pages = app->bulk_enqueue_page;
    char page[48];
    if (total_count)
        i18n_snprintf(page, sizeof(page), "第 %u / %u 页",
                      (unsigned int)app->bulk_enqueue_page,
                      (unsigned int)total_pages);
    else
        i18n_snprintf(page, sizeof(page), "第 %u 页",
                      (unsigned int)app->bulk_enqueue_page);
    menu_text_centered(ui, page, 25, 96, 270, 22,
                       UI_TEXT_LABEL, COL_MUTED, 24);

    float ratio = total_count ?
        (float)app->bulk_enqueue_processed /
            (float)total_count : 0.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    C2D_DrawRectSolid(34, 123, 0.4f, 252, 10, COL_GRID);
    C2D_DrawRectSolid(36, 125, 0.5f, 248 * ratio, 6, COL_CYAN);

    char progress[80];
    if (total_count)
        i18n_snprintf(progress, sizeof(progress), "已处理 %u / %u 首",
                      (unsigned int)app->bulk_enqueue_processed,
                      (unsigned int)total_count);
    else
        i18n_snprintf(progress, sizeof(progress), "已处理 %u 首",
                      (unsigned int)app->bulk_enqueue_processed);
    menu_text_centered(ui, progress, 25, 139, 270, 22,
                       UI_TEXT_LABEL, COL_MUTED, 28);
    i18n_snprintf(progress, sizeof(progress), "新增 %u 首 · 已有 %u 首",
                  (unsigned int)app->bulk_enqueue_added,
                  (unsigned int)app->bulk_enqueue_existing);
    menu_text_centered(ui, progress, 25, 161, 270, 22,
                       UI_TEXT_LABEL, COL_TEXT, 28);
    panel(110, 187, 100, 20, COL_PANEL, COL_GRID);
    menu_text_centered(ui, i18n_text("B 取消"),
                       112, 188, 96, 18, UI_TEXT_LABEL,
                       COL_MUTED, 8);
}

static void draw_key(Ui *ui, float x, float y, float w, float h,
                     const char *label, bool accent) {
    label = i18n_text(label);
    panel(x, y, w, h, accent ? COL_RED : COL_PANEL_2,
          accent ? COL_ORANGE : COL_GRID);
    bool pixel_ascii = label[0] && !label[1] &&
        ((label[0] >= 'a' && label[0] <= 'z') ||
         (label[0] >= 'A' && label[0] <= 'Z') ||
         (label[0] >= '0' && label[0] <= '9'));
    if (pixel_ascii) {
        pixel_text(label, floorf(x + (w - 10.0f) / 2.0f + 0.5f),
                   floorf(y + (h - 14.0f) / 2.0f + 0.5f),
                   0.7f, 2, accent ? COL_TEXT : COL_MUTED);
    } else if ((unsigned char)label[0] >= 0x80U) {
        label_centered(ui, label, x, y, w, h, UI_TEXT_LABEL,
                       accent ? COL_TEXT : COL_MUTED);
    } else {
        menu_text_centered(ui, label, x, y, w, h,
                           UI_TEXT_SMALL,
                           accent ? COL_TEXT : COL_MUTED, 8);
    }
}

typedef struct {
    const char *keys;
    float x;
    float y;
    float width;
    float step;
} ImeKeyboardRow;

static const ImeKeyboardRow IME_LETTER_ROWS[] = {
    {"qwertyuiop", 4, 68, 29, 31},
    {"asdfghjkl", 19, 102, 29, 31},
    {"zxcvbnm", 50, 136, 29, 31},
};

static const ImeKeyboardRow IME_SYMBOL_ROWS[] = {
    {"1234567890", 4, 68, 29, 31},
    {"-/:;()$&@\"", 4, 102, 29, 31},
    {".,?!'#+_%", 4, 136, 27, 29},
};

static const ImeKeyboardRow *ime_keyboard_rows(const Ui *ui) {
    return ui && ui->ime_symbols ? IME_SYMBOL_ROWS : IME_LETTER_ROWS;
}

static void reset_ime_candidate_layout(Ui *ui) {
    if (!ui) return;
    ui->ime_candidate_page = 0;
    ui->ime_candidate_selected = 0;
    ui->ime_candidate_layout_dirty = true;
}

static void rebuild_ime_candidate_layout(Ui *ui) {
    if (!ui || !ui->ime_candidate_layout_dirty) return;
    int count = ime_candidate_count(ui->ime);
    float pixels = text_metrics(UI_TEXT_LARGE)->preferred_px;
    for (int i = 0; i < count; i++) {
        float width = 0.0f;
        content_text_dimensions(ui, ime_candidate(ui->ime, i), pixels,
                                &width, NULL);
        ui->ime_candidate_text_widths[i] = ceilf(width);
    }
    ime_candidate_layout_build(
        &ui->ime_candidate_layout, ui->ime_candidate_text_widths, count,
        IME_CANDIDATE_RIGHT - IME_CANDIDATE_X, IME_CANDIDATE_MIN_W,
        IME_CANDIDATE_PADDING, IME_CANDIDATE_GAP);
    if (ui->ime_candidate_page >= ui->ime_candidate_layout.page_count)
        ui->ime_candidate_page = 0;
    if (ui->ime_candidate_layout.page_count > 0) {
        int start = ime_candidate_layout_page_start(
            &ui->ime_candidate_layout, ui->ime_candidate_page);
        int end = ime_candidate_layout_page_end(
            &ui->ime_candidate_layout, ui->ime_candidate_page);
        if (ui->ime_candidate_selected < start ||
            ui->ime_candidate_selected >= end)
            ui->ime_candidate_selected = start;
    } else ui->ime_candidate_selected = 0;
    ui->ime_candidate_layout_dirty = false;
}

static int ime_visible_candidate_start(const Ui *ui) {
    return ui ? ime_candidate_layout_page_start(
                    &ui->ime_candidate_layout, ui->ime_candidate_page) : 0;
}

static int ime_visible_candidate_end(const Ui *ui) {
    return ui ? ime_candidate_layout_page_end(
                    &ui->ime_candidate_layout, ui->ime_candidate_page) : 0;
}

static void draw_ime(Ui *ui) {
    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 32, COL_PANEL);
    if (ui->ime_chinese && ui->ime && ime_active(ui->ime)) {
        rebuild_ime_candidate_layout(ui);
        const char *buffer = ime_buffer(ui->ime);
        int matched = ime_matched_length(ui->ime);
        char head[IME_BUFFER_MAX + 1];
        i18n_snprintf(head, sizeof(head), "%.*s", matched, buffer);
        pixel_text(head, 6, 12, 0.5f, 1, COL_CYAN);
        if ((int)strlen(buffer) > matched)
            pixel_text(buffer + matched, 6 + matched * 6, 12, 0.5f, 1, COL_RED);
        int page_count = ui->ime_candidate_layout.page_count;
        if (page_count > 0) {
            char page_label[32];
            i18n_snprintf(page_label, sizeof(page_label), "%d/%d",
                          ui->ime_candidate_page + 1, page_count);
            pixel_text(page_label, 6, 22, 0.5f, 1, COL_DIM);
        }
        float x = IME_CANDIDATE_X;
        int start = ime_visible_candidate_start(ui);
        int end = ime_visible_candidate_end(ui);
        for (int i = start; i < end; i++) {
            float width = ui->ime_candidate_layout.item_widths[i];
            bool selected = i == ui->ime_candidate_selected;
            panel(x, IME_CANDIDATE_Y, width,
                  IME_CANDIDATE_H,
                  selected ? COL_PANEL_2 : COL_PANEL,
                  selected ? COL_ORANGE : COL_GRID);
            smooth_text_centered(ui, ime_candidate(ui->ime, i),
                                 x, IME_CANDIDATE_Y,
                                 width, IME_CANDIDATE_H,
                                 UI_TEXT_LARGE,
                                 selected ? COL_TEXT : COL_MUTED,
                                 64);
            x += width + IME_CANDIDATE_GAP;
        }
    } else {
        menu_text_fit(ui, i18n_text(
                      ui->ime_chinese ? "拼音输入" : "英文输入"),
                      8, 5, UI_TEXT_TITLE, 170,
                      COL_CYAN, 8);
        label_text(ui, ui->ime_chinese ? "输入拼音" : "输入英文",
                   241, 7, UI_TEXT_LABEL, COL_MUTED);
    }
    panel(4, 35, 312, 28, COL_BG, COL_GRID);
    if (ui->ime_text[0])
        smooth_text_fit(ui, ui->ime_text,
                        10, 38, UI_TEXT_TITLE, 300, COL_TEXT, 40);
    else
        menu_text_fit(ui, i18n_text("歌曲、歌手或专辑"),
                      10, 38, UI_TEXT_TITLE, 300, COL_MUTED, 40);

    const ImeKeyboardRow *rows = ime_keyboard_rows(ui);
    for (int row = 0; row < 3; row++) {
        size_t length = strlen(rows[row].keys);
        for (size_t col = 0; col < length; col++) {
            char label[2] = {rows[row].keys[col], '\0'};
            draw_key(ui, rows[row].x + col * rows[row].step, rows[row].y,
                     rows[row].width, 30, label, false);
        }
    }
    draw_key(ui, 270, 136, 46, 30, "删除", false);
    draw_key(ui, IME_LANGUAGE_X, IME_ACTION_Y, IME_LANGUAGE_W, IME_ACTION_H,
             ui->ime_chinese ? "中文" : "英文", false);
    draw_key(ui, IME_SYMBOLS_X, IME_ACTION_Y, IME_SYMBOLS_W, IME_ACTION_H,
             ui->ime_symbols ? "ABC" : "123", false);
    draw_key(ui, IME_SPACE_X, IME_ACTION_Y, IME_SPACE_W, IME_ACTION_H,
             "空格", false);
    draw_key(ui, IME_CANCEL_X, IME_ACTION_Y, IME_CANCEL_W, IME_ACTION_H,
             "取消", false);
    draw_key(ui, IME_SEARCH_X, IME_ACTION_Y, IME_SEARCH_W, IME_ACTION_H,
             "搜索", true);
    if (ui->ime_chinese && ui->ime && ime_active(ui->ime) &&
        ui->ime_candidate_layout.page_count > 0) {
        menu_text_fit(ui, i18n_text("←→选字  ↑↓翻页  A确认  Y键盘"),
                      8, 217, UI_TEXT_SMALL,
                      UI_BOTTOM_STATUS_WIDTH + 4,
                      COL_MUTED, 64);
    } else {
        menu_text_fit(ui, i18n_text("X中英  Y键盘  B删除  START搜索"),
                      8, 217, UI_TEXT_CAPTION,
                      UI_BOTTOM_STATUS_WIDTH + 4,
                      COL_MUTED, 64);
    }
}

Ui *ui_create(C3D_RenderTarget *top_left, C3D_RenderTarget *top_right,
              C3D_RenderTarget *bottom) {
    if (!top_left || !bottom) return NULL;
    Ui *ui = (Ui *)calloc(1, sizeof(Ui));
    if (!ui) return NULL;
    ui->top_left = top_left;
    ui->top_right = top_right;
    ui->bottom = bottom;
    cover_init(&ui->cover);
    (void)brand_logo_init(&ui->brand_logo);
    immersive_font_init(&ui->content_point_font);
    if (immersive_font_load(
            &ui->content_point_font, CONTENT_POINT_FONT_PATH) != 0) {
        brand_logo_clear(&ui->brand_logo);
        free(ui);
        return NULL;
    }
    immersive_font_init(&ui->content_large_point_font);
    if (immersive_font_load(
            &ui->content_large_point_font,
            CONTENT_LARGE_POINT_FONT_PATH) != 0) {
        immersive_font_clear(&ui->content_point_font);
        brand_logo_clear(&ui->brand_logo);
        free(ui);
        return NULL;
    }
    ui->menu_font = C2D_FontLoad(UI_MENU_FONT_PATH);
    if (ui->menu_font) {
        ui->menu_text_buffer = C2D_TextBufNew(UI_MENU_TEXT_GLYPHS);
        if (ui->menu_text_buffer)
            C2D_FontSetFilter(ui->menu_font, GPU_NEAREST, GPU_NEAREST);
        else {
            C2D_FontFree(ui->menu_font);
            ui->menu_font = NULL;
        }
    }
    immersive_font_init(&ui->immersive_font);
    ui->immersive_font_song_id = -1;
    ui->immersive_font_active_index = -1;
    ui->immersive_font_style = IMMERSIVE_LYRIC_STYLE_COUNT;
    return ui;
}

bool ui_menu_font_ready(const Ui *ui) {
    return ui && ui->menu_font && ui->menu_text_buffer;
}

void ui_draw_startup(Ui *ui, unsigned int step, unsigned int total,
                     const char *status) {
    if (!ui) return;
    if (total == 0) total = 1;
    if (step == 0) step = 1;
    if (step > total) step = total;
    float progress = total <= 1 ? 1.0f :
        (float)(step - 1) / (float)(total - 1);
    char count[24];
    i18n_snprintf(count, sizeof(count), "%u/%u", step, total);

    gfxSet3D(false);
    immersive_font_begin_frame(&ui->content_point_font);
    immersive_font_begin_frame(&ui->content_large_point_font);
    if (ui->menu_text_buffer) C2D_TextBufClear(ui->menu_text_buffer);
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

    C2D_TargetClear(ui->top_left, COL_BG);
    C2D_SceneBegin(ui->top_left);
    brand_pixel_text("ClouDS Music", 128, 58, 0.5f, 2, COL_TEXT);
    menu_text_centered(ui, i18n_text("正在启动"),
                       40, 106, 320, 28,
                       UI_TEXT_TITLE, COL_TEXT, 16);
    draw_startup_progress_bar(60, 151, 280, 14, progress);
    menu_text_centered(ui, status ? status : i18n_text("加载中"),
                       40, 179, 320, 28,
                       UI_TEXT_BODY, COL_MUTED, 32);
    pixel_text(count, 188, 216, 0.5f, 1, COL_DIM);

    C2D_TargetClear(ui->bottom, COL_BG);
    C2D_SceneBegin(ui->bottom);
    C2D_DrawRectSolid(0, 0, 0.1f, 320, 3, COL_RED);
    brand_pixel_text("ClouDS Music", 124, 61, 0.5f, 1, COL_TEXT);
    draw_startup_progress_bar(34, 105, 252, 12, progress);
    menu_text_centered(ui, status ? status : i18n_text("加载中"),
                       24, 132, 272, 30,
                       UI_TEXT_BODY, COL_MUTED, 32);
    pixel_text(count, 148, 184, 0.5f, 1, COL_DIM);

    C3D_FrameEnd(0);
}

int ui_prepare_immersive_font(Ui *ui, char *error, size_t error_size) {
    if (!ui) return -1;
    if (immersive_font_ready(&ui->immersive_font)) return 0;
    if (immersive_font_load(&ui->immersive_font, IMMERSIVE_FONT_PATH) == 0) {
        ui->immersive_font_song_id = -1;
        ui->immersive_font_active_index = -1;
        ui->immersive_font_style = IMMERSIVE_LYRIC_STYLE_COUNT;
        return 0;
    }
    if (error && error_size)
        i18n_snprintf(error, error_size, "沉浸点阵字体不可用");
    return -1;
}

void ui_destroy(Ui *ui) {
    if (!ui) return;
    gfxSet3D(false);
    ime_destroy(ui->ime);
    immersive_font_clear(&ui->content_point_font);
    immersive_font_clear(&ui->content_large_point_font);
    immersive_font_clear(&ui->immersive_font);
    if (ui->menu_text_buffer) C2D_TextBufDelete(ui->menu_text_buffer);
    if (ui->menu_font) C2D_FontFree(ui->menu_font);
    cover_clear(&ui->cover);
    brand_logo_clear(&ui->brand_logo);
    free(ui);
}

void ui_draw(Ui *ui, const AppState *app, const Player *player) {
    if (!ui || !app) return;
    bool now_playing_visible = !app->account_open &&
                               app->tab == TAB_NOW_PLAYING &&
                               !app->album_open;
    LyricAnimationFrame lyric_frame = {0};
    if (now_playing_visible)
        lyric_frame = prepare_lyric_frame(ui, app, player);

    float stereo_slider = 0.0f;
    bool stereo_content_ready = lyric_frame.ready ||
        (now_playing_visible && display_song(app));
    if (ui->top_right && stereo_content_ready)
        stereo_slider = osGet3DSliderState();
    if (stereo_slider < 0.0f) stereo_slider = 0.0f;
    if (stereo_slider > 1.0f) stereo_slider = 1.0f;
    bool stereo_active = stereo_slider > STEREO_SLIDER_THRESHOLD;
    gfxSet3D(stereo_active);

    immersive_font_begin_frame(&ui->content_point_font);
    immersive_font_begin_frame(&ui->content_large_point_font);
    immersive_font_begin_frame(&ui->immersive_font);
    if (ui->menu_text_buffer) C2D_TextBufClear(ui->menu_text_buffer);
    draw_top(ui, ui->top_left, app, &lyric_frame,
             stereo_active ? 1.0f : 0.0f, stereo_slider);
    if (stereo_active) {
        draw_top(ui, ui->top_right, app, &lyric_frame,
                 -1.0f, stereo_slider);
    }
    if (app->immersive_lyrics) {
        C2D_TargetClear(ui->bottom, COL_SCREEN_OFF);
        C2D_SceneBegin(ui->bottom);
    } else if (app->dsp_firmware_prompt_open)
        draw_dsp_firmware_dialog(ui, app);
    else if (app->network_certificate_prompt_open)
        draw_network_certificate_dialog(ui, app);
    else if (ui->ime_open) draw_ime(ui);
    else if (app->queue_replace_confirm) draw_queue_replace_dialog(ui, app);
    else if (app->bulk_enqueue_confirm)
        draw_bulk_enqueue_confirm(ui, app);
    else if (app->bulk_enqueue_active)
        draw_bulk_enqueue_progress(ui, app);
    else draw_bottom_player(ui, app, player);
    if (!app->immersive_lyrics) draw_battery_status(app);
    if (now_playing_visible) finish_lyric_frame(ui, &lyric_frame);
}

void ui_draw_once(Ui *ui, const AppState *app, const Player *player) {
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    ui_draw(ui, app, player);
    C3D_FrameEnd(0);
}

static bool append_text(char *target, size_t size, const char *value) {
    if (!target || !value) return false;
    size_t used = strlen(target);
    size_t add = strlen(value);
    if (used + add + 1 > size) return false;
    memcpy(target + used, value, add + 1);
    return true;
}

static void utf8_backspace(char *value) {
    size_t length = strlen(value);
    if (length == 0) return;
    size_t index = length - 1;
    while (index > 0 && ((unsigned char)value[index] & 0xc0U) == 0x80U)
        index--;
    value[index] = '\0';
}

static void commit_candidate(Ui *ui, int index) {
    if (!ui || !ui->ime) return;
    const char *value = ime_candidate(ui->ime, index);
    if (value && append_text(ui->ime_text, sizeof(ui->ime_text), value)) {
        (void)ime_commit(ui->ime, index);
        reset_ime_candidate_layout(ui);
    }
}

static bool commit_ime_composition(Ui *ui) {
    if (!ui || !ui->ime || !ime_active(ui->ime)) return true;
    if (ime_candidate_count(ui->ime) > 0) {
        int selected = ui->ime_candidate_selected;
        if (selected < 0 || selected >= ime_candidate_count(ui->ime))
            selected = 0;
        commit_candidate(ui, selected);
        return !ime_active(ui->ime);
    }
    if (!append_text(ui->ime_text, sizeof(ui->ime_text),
                     ime_buffer(ui->ime))) return false;
    ime_clear(ui->ime);
    reset_ime_candidate_layout(ui);
    return true;
}

static void input_ime_key(Ui *ui, char key) {
    if (!ui) return;
    if (key >= 'a' && key <= 'z' && !ui->ime_symbols) {
        if (ui->ime_chinese && ui->ime) {
            ime_input(ui->ime, key);
            reset_ime_candidate_layout(ui);
        } else {
            char value[2] = {key, '\0'};
            (void)append_text(ui->ime_text, sizeof(ui->ime_text), value);
        }
        return;
    }
    if (!commit_ime_composition(ui)) return;
    char value[2] = {key, '\0'};
    (void)append_text(ui->ime_text, sizeof(ui->ime_text), value);
}

static void select_ime_candidate(Ui *ui, int direction) {
    if (!ui || ui->ime_candidate_layout_dirty) return;
    int start = ime_visible_candidate_start(ui);
    int end = ime_visible_candidate_end(ui);
    if (end <= start) return;
    if (ui->ime_candidate_selected < start ||
        ui->ime_candidate_selected >= end) {
        ui->ime_candidate_selected = start;
        return;
    }
    if (direction < 0)
        ui->ime_candidate_selected =
            ui->ime_candidate_selected > start ?
            ui->ime_candidate_selected - 1 : end - 1;
    else
        ui->ime_candidate_selected =
            ui->ime_candidate_selected + 1 < end ?
            ui->ime_candidate_selected + 1 : start;
}

static void change_ime_candidate_page(Ui *ui, int direction) {
    if (!ui || ui->ime_candidate_layout_dirty ||
        ui->ime_candidate_layout.page_count <= 0) return;
    int pages = ui->ime_candidate_layout.page_count;
    if (direction < 0)
        ui->ime_candidate_page = ui->ime_candidate_page > 0 ?
            ui->ime_candidate_page - 1 : pages - 1;
    else
        ui->ime_candidate_page = (ui->ime_candidate_page + 1) % pages;
    ui->ime_candidate_selected = ime_visible_candidate_start(ui);
}

bool ui_ime_begin(Ui *ui, const char *initial_text) {
    if (!ui) return false;
    if (!ui->ime_attempted) {
        ui->ime = ime_create("romfs:/pinyin_dict.bin");
        ui->ime_attempted = true;
    }
    if (ui->ime) ime_clear(ui->ime);
    i18n_snprintf(ui->ime_text, sizeof(ui->ime_text), "%s",
             initial_text ? initial_text : "");
    ui->ime_chinese = ui->ime != NULL;
    ui->ime_symbols = false;
    reset_ime_candidate_layout(ui);
    ui->ime_open = true;
    return ui->ime != NULL;
}

bool ui_ime_active(const Ui *ui) { return ui && ui->ime_open; }
const char *ui_ime_text(const Ui *ui) { return ui ? ui->ime_text : ""; }

static int hit(float x, float y, float w, float h, int px, int py) {
    return px >= (int)x && px < (int)(x + w) &&
           py >= (int)y && py < (int)(y + h);
}

UiPlayerTouchAction ui_player_touch(const AppState *app,
                                    const touchPosition *touch,
                                    int *queue_index, float *seek_ratio) {
    if (queue_index) *queue_index = -1;
    if (seek_ratio) *seek_ratio = 0.0f;
    if (!app || !touch) return UI_PLAYER_TOUCH_NONE;
    int x = touch->px;
    int y = touch->py;
    if (y >= UI_BOTTOM_FOOTER_Y) return UI_PLAYER_TOUCH_NONE;
    if (hit(PREVIOUS_X, PREVIOUS_Y, PREVIOUS_W, PREVIOUS_H, x, y))
        return UI_PLAYER_TOUCH_PREVIOUS;
    if (hit(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, x, y))
        return UI_PLAYER_TOUCH_PLAY_PAUSE;
    if (hit(NEXT_X, NEXT_Y, NEXT_W, NEXT_H, x, y))
        return UI_PLAYER_TOUCH_NEXT;
    if (hit(MODE_X, MODE_Y, MODE_W, MODE_H, x, y))
        return UI_PLAYER_TOUCH_PLAY_MODE;
    if (hit(ALBUM_X, ALBUM_Y, ALBUM_W, ALBUM_H, x, y) &&
        (app->album_open || (current_song(app) && app->network_online)))
        return UI_PLAYER_TOUCH_ALBUM;
    if (ui_player_seek_ratio(touch, seek_ratio))
        return UI_PLAYER_TOUCH_SEEK;
    if (x >= 195 && y >= 8 && y < QUEUE_LIST_Y)
        return queue_has_selectable_item(app) ?
                                  UI_PLAYER_TOUCH_PLAYLIST_FOCUS :
                                  UI_PLAYER_TOUCH_NONE;
    if (x < BOTTOM_PLAYER_WIDTH || y < QUEUE_LIST_Y ||
        y >= QUEUE_LIST_Y + UI_QUEUE_VISIBLE_ROWS * QUEUE_ROW_HEIGHT ||
        app->queue_count == 0)
        return UI_PLAYER_TOUCH_NONE;
    int row = (y - QUEUE_LIST_Y) / QUEUE_ROW_HEIGHT;
    int index = queue_window_start(app) + row;
    if (index < 0 || index >= (int)app->queue_count)
        return UI_PLAYER_TOUCH_NONE;
    if (!queue_item_selectable(app, index)) return UI_PLAYER_TOUCH_NONE;
    if (queue_index) *queue_index = index;
    return UI_PLAYER_TOUCH_QUEUE_ITEM;
}

bool ui_player_seek_ratio(const touchPosition *touch, float *seek_ratio) {
    if (!touch || !seek_ratio || touch->py < PROGRESS_TOUCH_Y ||
        touch->py >= PROGRESS_TOUCH_Y + PROGRESS_TOUCH_H ||
        touch->px < 8 || touch->px >= BOTTOM_PLAYER_WIDTH)
        return false;
    float ratio = ((float)touch->px - PROGRESS_X) / PROGRESS_W;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    *seek_ratio = ratio;
    return true;
}

static UiImeAction ime_touch(Ui *ui, int x, int y) {
    if (ui->ime_chinese && ui->ime && ime_active(ui->ime) && y < 32) {
        float candidate_x = IME_CANDIDATE_X;
        int start = ime_visible_candidate_start(ui);
        int end = ime_visible_candidate_end(ui);
        for (int i = start; i < end; i++) {
            float width = ui->ime_candidate_layout.item_widths[i];
            if (hit(candidate_x,
                    IME_CANDIDATE_Y, width,
                    IME_CANDIDATE_H, x, y)) {
                commit_candidate(ui, i);
                return UI_IME_NONE;
            }
            candidate_x += width + IME_CANDIDATE_GAP;
        }
    }
    const ImeKeyboardRow *rows = ime_keyboard_rows(ui);
    for (int row = 0; row < 3; row++) {
        for (size_t col = 0; col < strlen(rows[row].keys); col++) {
            if (!hit(rows[row].x + col * rows[row].step, rows[row].y,
                     rows[row].width, 30, x, y)) continue;
            input_ime_key(ui, rows[row].keys[col]);
            return UI_IME_NONE;
        }
    }
    if (hit(270, 136, 46, 30, x, y)) {
        if (ui->ime && ime_active(ui->ime)) {
            ime_backspace(ui->ime);
            reset_ime_candidate_layout(ui);
        }
        else utf8_backspace(ui->ime_text);
    } else if (hit(IME_LANGUAGE_X, IME_ACTION_Y, IME_LANGUAGE_W,
                   IME_ACTION_H, x, y)) {
        if (ui->ime) {
            ime_clear(ui->ime);
            ui->ime_chinese = !ui->ime_chinese;
            reset_ime_candidate_layout(ui);
        }
    } else if (hit(IME_SYMBOLS_X, IME_ACTION_Y, IME_SYMBOLS_W,
                   IME_ACTION_H, x, y)) {
        ui->ime_symbols = !ui->ime_symbols;
    } else if (hit(IME_SPACE_X, IME_ACTION_Y, IME_SPACE_W,
                   IME_ACTION_H, x, y)) {
        if (ui->ime_chinese && ui->ime && ime_active(ui->ime))
            (void)commit_ime_composition(ui);
        else (void)append_text(ui->ime_text, sizeof(ui->ime_text), " ");
    } else if (hit(IME_CANCEL_X, IME_ACTION_Y, IME_CANCEL_W,
                   IME_ACTION_H, x, y)) {
        ui->ime_open = false;
        return UI_IME_CANCEL;
    } else if (hit(IME_SEARCH_X, IME_ACTION_Y, IME_SEARCH_W,
                   IME_ACTION_H, x, y) && ui->ime_text[0]) {
        ui->ime_open = false;
        return UI_IME_SUBMIT;
    }
    return UI_IME_NONE;
}

UiImeAction ui_ime_handle(Ui *ui, u32 down, u32 repeat,
                          const touchPosition *touch) {
    if (!ui || !ui->ime_open) return UI_IME_NONE;
    if (down & KEY_SELECT) {
        ui->ime_open = false;
        return UI_IME_CANCEL;
    }
    if ((down & KEY_START) && ui->ime_text[0]) {
        ui->ime_open = false;
        return UI_IME_SUBMIT;
    }
    if (down & KEY_X) {
        if (ui->ime) {
            ime_clear(ui->ime);
            ui->ime_chinese = !ui->ime_chinese;
            reset_ime_candidate_layout(ui);
        }
    }
    if (down & KEY_Y) ui->ime_symbols = !ui->ime_symbols;
    if ((repeat & KEY_B) != 0) {
        if (ui->ime && ime_active(ui->ime)) {
            ime_backspace(ui->ime);
            reset_ime_candidate_layout(ui);
        }
        else if (ui->ime_text[0]) utf8_backspace(ui->ime_text);
        else {
            ui->ime_open = false;
            return UI_IME_CANCEL;
        }
    }
    if (ui->ime && ime_active(ui->ime)) {
        if (repeat & KEY_LEFT) select_ime_candidate(ui, -1);
        if (repeat & KEY_RIGHT) select_ime_candidate(ui, 1);
        if (down & KEY_UP) change_ime_candidate_page(ui, -1);
        if (down & KEY_DOWN) change_ime_candidate_page(ui, 1);
        if (down & KEY_A)
            commit_candidate(ui, ui->ime_candidate_selected);
    } else if ((down & KEY_A) && ui->ime_text[0]) {
        ui->ime_open = false;
        return UI_IME_SUBMIT;
    }
    if ((down & KEY_TOUCH) && touch)
        return ime_touch(ui, touch->px, touch->py);
    return UI_IME_NONE;
}

int ui_load_cover(Ui *ui, const char *path, int64_t song_id,
                  char *error, size_t error_size) {
    return ui ? cover_load_image(&ui->cover, path, song_id,
                                 error, error_size) : -1;
}

int ui_upload_cover(Ui *ui, const uint32_t *pixels, size_t pixel_count,
                    int64_t song_id, char *error, size_t error_size) {
    return ui ? cover_upload_rgba(&ui->cover, pixels, pixel_count,
                                  song_id, error, error_size) : -1;
}

void ui_clear_cover(Ui *ui) {
    if (ui) cover_clear(&ui->cover);
}

bool ui_set_login_qr(Ui *ui, const char *key) {
    if (!ui || !key || !key[0]) return false;
    char url[256];
    int written = i18n_snprintf(url, sizeof(url),
                           "https://music.163.com/login?codekey=%s", key);
    if (written < 0 || (size_t)written >= sizeof(url)) return false;
    ui->qr_ready = qrcodegen_encodeText(
        url, ui->qr_temp, ui->qr_code, qrcodegen_Ecc_MEDIUM,
        qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO, true);
    return ui->qr_ready;
}

void ui_clear_login_qr(Ui *ui) {
    if (!ui) return;
    memset(ui->qr_temp, 0, sizeof(ui->qr_temp));
    memset(ui->qr_code, 0, sizeof(ui->qr_code));
    ui->qr_ready = false;
}
