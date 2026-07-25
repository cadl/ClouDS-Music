#pragma once

#include <stddef.h>
#include <stdint.h>

/* Native top-screen bounds and the shared pagination footer. */
enum {
    UI_TOP_SCREEN_WIDTH = 400,
    UI_TOP_SCREEN_HEIGHT = 240,
    UI_BOTTOM_SCREEN_WIDTH = 320,
    UI_BOTTOM_SCREEN_HEIGHT = 240,
    UI_BOTTOM_FOOTER_Y = 219,
    UI_BOTTOM_FOOTER_HEIGHT = 21,
    UI_BOTTOM_STATUS_X = 16,
    UI_BOTTOM_STATUS_WIDTH = 260,
    UI_BOTTOM_BATTERY_REGION_X = 280,
    UI_BOTTOM_BATTERY_X = 290,
    UI_BOTTOM_BATTERY_Y = 225,
    UI_BOTTOM_BATTERY_WIDTH = 26,
    UI_BOTTOM_BATTERY_HEIGHT = 10,
    UI_TOP_PAGE_FOOTER_X = 244,
    UI_TOP_PAGE_FOOTER_Y = 219,
    UI_TOP_PAGE_FOOTER_WIDTH = 144,
    UI_TOP_PAGE_FOOTER_HEIGHT = 21,
    UI_CONTENT_TEXT_MIN_HEIGHT = 18,

    /* Total left-to-right disparity at the maximum 3D slider position. Keep
     * these integer tiers behind the roughly 4-5px active-lyric disparity so
     * native point glyphs remain crisp while the playback card gains depth. */
    UI_NOW_COVER_STEREO_DISPARITY = 1,
    UI_NOW_ARTIST_STEREO_DISPARITY = 2,
    UI_NOW_TITLE_STEREO_DISPARITY = 3,

    UI_CONTROL_LEFT_CELL_X = 12,
    UI_CONTROL_LEFT_CELL_RIGHT = 96,
    UI_CONTROL_RIGHT_CELL_X = 100,
    UI_CONTROL_RIGHT_CELL_RIGHT = 184,
    UI_CONTROL_KEY_CHAR_WIDTH = 6,
    UI_CONTROL_KEY_PADDING = 6,
    UI_CONTROL_ACTION_GAP = 4,

    UI_RECOMMEND_FIRST_ROW_Y = 55,
    UI_RECOMMEND_ROW_STEP = 20,
    UI_RECOMMEND_VISIBLE_ROWS = 8,

    UI_LIBRARY_PLAYLIST_FIRST_ROW_Y = 55,
    UI_LIBRARY_PLAYLIST_ROW_STEP = 20,
    UI_LIBRARY_PLAYLIST_VISIBLE_ROWS = 8,

    UI_LIBRARY_TRACK_FIRST_ROW_Y = 67,
    UI_LIBRARY_TRACK_ROW_STEP = 19,
    UI_LIBRARY_TRACK_VISIBLE_ROWS = 8,

    UI_SEARCH_FIRST_ROW_Y = 96,
    UI_SEARCH_ROW_STEP = 21,
    UI_SEARCH_VISIBLE_ROWS = 6,

    UI_CONTENT_LIST_SCROLLBAR_X = 394,
    UI_CONTENT_LIST_SCROLLBAR_WIDTH = 3,
    UI_CONTENT_LIST_SCROLLBAR_MIN_THUMB_HEIGHT = 16,
    UI_RECOMMEND_SCROLLBAR_Y = 53,
    UI_RECOMMEND_SCROLLBAR_HEIGHT = 160,
    UI_LIBRARY_PLAYLIST_SCROLLBAR_Y = 53,
    UI_LIBRARY_PLAYLIST_SCROLLBAR_HEIGHT = 160,
    UI_LIBRARY_TRACK_SCROLLBAR_Y = 65,
    UI_LIBRARY_TRACK_SCROLLBAR_HEIGHT = 153,
    UI_SEARCH_SCROLLBAR_Y = 94,
    UI_SEARCH_SCROLLBAR_HEIGHT = 125,

    UI_ALBUM_FIRST_ROW_Y = 70,
    UI_ALBUM_ROW_STEP = 22,
    UI_ALBUM_SCROLLBAR_X = 383,
    UI_ALBUM_SCROLLBAR_Y = 68,
    UI_ALBUM_SCROLLBAR_WIDTH = 3,
    UI_ALBUM_SCROLLBAR_HEIGHT = 154,
    UI_ALBUM_SCROLLBAR_MIN_THUMB_HEIGHT = 20,

    UI_QUEUE_VISIBLE_ROWS = 4,

    UI_SETTINGS_VIEW_TOP = 47,
    UI_SETTINGS_VIEW_BOTTOM = UI_TOP_SCREEN_HEIGHT,
    UI_SETTINGS_CACHE_Y = 47,
    UI_SETTINGS_CACHE_HEIGHT = 39,
    UI_SETTINGS_LANGUAGE_Y = 88,
    UI_SETTINGS_LANGUAGE_HEIGHT = 24,
    UI_SETTINGS_LIMIT_Y = 114,
    UI_SETTINGS_LIMIT_HEIGHT = 30,
    UI_SETTINGS_LIMIT_OPTION_X = 122,
    UI_SETTINGS_LIMIT_OPTION_WIDTH = 48,
    UI_SETTINGS_LIMIT_OPTION_STEP = 52,
    UI_SETTINGS_DEBUG_Y = 146,
    UI_SETTINGS_DEBUG_HEIGHT = 24,
    UI_SETTINGS_CLEAR_Y = 172,
    UI_SETTINGS_CLEAR_HEIGHT = 28,
    UI_SETTINGS_CONTACT_Y = 202,
    UI_SETTINGS_CONTACT_HEIGHT = 62,
    UI_SETTINGS_REPOSITORY_Y = 266,
    UI_SETTINGS_REPOSITORY_HEIGHT = 62,
    UI_SETTINGS_USAGE_NOTICE_Y = 330,
    UI_SETTINGS_USAGE_NOTICE_HEIGHT = 28,
    UI_SETTINGS_VERSION_Y = 360,
    UI_SETTINGS_VERSION_HEIGHT = 28,
    UI_SETTINGS_CONTENT_BOTTOM =
        UI_SETTINGS_VERSION_Y + UI_SETTINGS_VERSION_HEIGHT,
    UI_SETTINGS_SCROLLBAR_X = UI_CONTENT_LIST_SCROLLBAR_X,
    UI_SETTINGS_SCROLLBAR_Y = 49,
    UI_SETTINGS_SCROLLBAR_WIDTH = 3,
    UI_SETTINGS_SCROLLBAR_HEIGHT = 188,
    UI_SETTINGS_SCROLLBAR_MIN_THUMB_HEIGHT = 24,
    UI_SETTINGS_DOWN_HINT_X = 187,
    UI_SETTINGS_DOWN_HINT_Y = 220,
    UI_SETTINGS_DOWN_HINT_WIDTH = 26,
    UI_SETTINGS_DOWN_HINT_HEIGHT = 18,
};

static inline int ui_stereo_eye_shift(float eye_sign, float slider,
                                      unsigned int max_total_disparity) {
    if (eye_sign == 0.0f || slider <= 0.0f || max_total_disparity == 0U)
        return 0;
    if (slider > 1.0f) slider = 1.0f;
    unsigned int total =
        (unsigned int)(slider * (float)max_total_disparity + 0.5f);
    if (total > max_total_disparity) total = max_total_disparity;

    /* Odd disparities cannot be centered on the integer pixel grid. Put the
     * extra pixel in the left-eye image; the cyclopean position moves by at
     * most half a pixel while every glyph and cover edge stays pixel-aligned. */
    if (eye_sign > 0.0f) return (int)((total + 1U) / 2U);
    return -(int)(total / 2U);
}

static inline int ui_control_action_x(int cell_x, size_t key_chars) {
    if (cell_x < 0 ||
        key_chars > (size_t)(INT32_MAX - UI_CONTROL_KEY_PADDING -
                             UI_CONTROL_ACTION_GAP) /
                        UI_CONTROL_KEY_CHAR_WIDTH)
        return INT32_MAX;
    int offset = (int)key_chars * UI_CONTROL_KEY_CHAR_WIDTH +
                 UI_CONTROL_KEY_PADDING + UI_CONTROL_ACTION_GAP;
    return cell_x <= INT32_MAX - offset ? cell_x + offset : INT32_MAX;
}

static inline int ui_control_action_width(int cell_x, int cell_right,
                                          size_t key_chars) {
    int action_x = ui_control_action_x(cell_x, key_chars);
    return action_x < cell_right ? cell_right - action_x : 0;
}

static inline int ui_scrollbar_thumb_height(int track_height,
                                            size_t visible_units,
                                            size_t total_units,
                                            int min_thumb_height) {
    if (track_height <= 0 || visible_units == 0 || total_units == 0)
        return 0;
    if (visible_units >= total_units) return track_height;
    int height = (int)((size_t)track_height * visible_units / total_units);
    if (height < min_thumb_height) height = min_thumb_height;
    if (height > track_height) height = track_height;
    return height;
}

static inline int ui_scrollbar_thumb_y(int track_y, int track_height,
                                       int thumb_height,
                                       size_t first_visible,
                                       size_t visible_units,
                                       size_t total_units) {
    if (track_height <= 0 || thumb_height <= 0 ||
        visible_units == 0 || total_units <= visible_units)
        return track_y;
    size_t max_first = total_units - visible_units;
    if (first_visible > max_first) first_visible = max_first;
    int travel = track_height - thumb_height;
    return track_y + (int)((size_t)travel * first_visible / max_first);
}

static inline int ui_settings_scroll_offset_for_row(int y, int height) {
    int overflow = y + height - UI_SETTINGS_VIEW_BOTTOM;
    return overflow > 0 ? overflow : 0;
}

static inline int ui_settings_row_is_visible(int y, int height,
                                             int scroll_offset) {
    int screen_y = y - scroll_offset;
    int screen_bottom = screen_y + height;
    /* Let the next row peek out below the viewport as a continuation cue,
     * but never draw a row that starts above the fixed page title. */
    return screen_y >= UI_SETTINGS_VIEW_TOP &&
           screen_y < UI_SETTINGS_VIEW_BOTTOM &&
           screen_bottom > UI_SETTINGS_VIEW_TOP;
}

static inline int ui_settings_scrollbar_thumb_height(void) {
    int viewport_height = UI_SETTINGS_VIEW_BOTTOM - UI_SETTINGS_VIEW_TOP;
    int content_height = UI_SETTINGS_CONTENT_BOTTOM - UI_SETTINGS_VIEW_TOP;
    return ui_scrollbar_thumb_height(
        UI_SETTINGS_SCROLLBAR_HEIGHT, (size_t)viewport_height,
        (size_t)content_height, UI_SETTINGS_SCROLLBAR_MIN_THUMB_HEIGHT);
}

static inline int ui_settings_max_scroll_offset(void) {
    int max_scroll = UI_SETTINGS_CONTENT_BOTTOM - UI_SETTINGS_VIEW_BOTTOM;
    return max_scroll > 0 ? max_scroll : 0;
}

static inline int ui_settings_can_scroll_down(int scroll_offset) {
    return scroll_offset < ui_settings_max_scroll_offset();
}

static inline int ui_settings_scrollbar_thumb_y(int scroll_offset) {
    int max_scroll = ui_settings_max_scroll_offset();
    if (max_scroll <= 0) return UI_SETTINGS_SCROLLBAR_Y;
    if (scroll_offset < 0) scroll_offset = 0;
    if (scroll_offset > max_scroll) scroll_offset = max_scroll;
    int viewport_height = UI_SETTINGS_VIEW_BOTTOM - UI_SETTINGS_VIEW_TOP;
    int content_height = UI_SETTINGS_CONTENT_BOTTOM - UI_SETTINGS_VIEW_TOP;
    return ui_scrollbar_thumb_y(
        UI_SETTINGS_SCROLLBAR_Y, UI_SETTINGS_SCROLLBAR_HEIGHT,
        ui_settings_scrollbar_thumb_height(), (size_t)scroll_offset,
        (size_t)viewport_height, (size_t)content_height);
}
