#include "cache.h"
#include "ui_layout.h"
#include "model.h"

#include <assert.h>
#include <stdio.h>

static int last_row_bottom(int first_y, int step, int visible_rows) {
    return first_y + (visible_rows - 1) * step +
           UI_CONTENT_TEXT_MIN_HEIGHT;
}

int main(void) {
    assert(UI_TOP_PAGE_FOOTER_X >= 0);
    assert(UI_TOP_PAGE_FOOTER_X + UI_TOP_PAGE_FOOTER_WIDTH <=
           UI_TOP_SCREEN_WIDTH);
    assert(UI_TOP_PAGE_FOOTER_Y + UI_TOP_PAGE_FOOTER_HEIGHT <=
           UI_TOP_SCREEN_HEIGHT);
    assert(UI_TOP_PAGE_FOOTER_HEIGHT >= UI_CONTENT_TEXT_MIN_HEIGHT);

    assert(ui_stereo_eye_shift(1.0f, 0.0f,
                               UI_NOW_TITLE_STEREO_DISPARITY) == 0);
    assert(ui_stereo_eye_shift(0.0f, 1.0f,
                               UI_NOW_TITLE_STEREO_DISPARITY) == 0);
    assert(ui_stereo_eye_shift(1.0f, 1.0f,
                               UI_NOW_COVER_STEREO_DISPARITY) == 1);
    assert(ui_stereo_eye_shift(-1.0f, 1.0f,
                               UI_NOW_COVER_STEREO_DISPARITY) == 0);
    assert(ui_stereo_eye_shift(1.0f, 1.0f,
                               UI_NOW_ARTIST_STEREO_DISPARITY) == 1);
    assert(ui_stereo_eye_shift(-1.0f, 1.0f,
                               UI_NOW_ARTIST_STEREO_DISPARITY) == -1);
    assert(ui_stereo_eye_shift(1.0f, 1.0f,
                               UI_NOW_TITLE_STEREO_DISPARITY) == 2);
    assert(ui_stereo_eye_shift(-1.0f, 1.0f,
                               UI_NOW_TITLE_STEREO_DISPARITY) == -1);
    assert(ui_stereo_eye_shift(1.0f, 2.0f,
                               UI_NOW_TITLE_STEREO_DISPARITY) == 2);
    assert(ui_stereo_eye_shift(-1.0f, 0.5f,
                               UI_NOW_TITLE_STEREO_DISPARITY) == -1);

    assert(UI_CONTROL_LEFT_CELL_RIGHT + UI_CONTROL_ACTION_GAP <=
           UI_CONTROL_RIGHT_CELL_X);
    assert(UI_CONTROL_RIGHT_CELL_RIGHT < 188);
    assert(ui_control_action_x(UI_CONTROL_LEFT_CELL_X, 6) == 58);
    assert(ui_control_action_width(UI_CONTROL_LEFT_CELL_X,
                                   UI_CONTROL_LEFT_CELL_RIGHT, 6) == 38);
    assert(ui_control_action_width(UI_CONTROL_RIGHT_CELL_X,
                                   UI_CONTROL_RIGHT_CELL_RIGHT, 6) == 38);
    assert(ui_control_action_width(UI_CONTROL_LEFT_CELL_X,
                                   UI_CONTROL_LEFT_CELL_RIGHT, 1) == 68);
    assert(ui_control_action_width(UI_CONTROL_LEFT_CELL_X,
                                   UI_CONTROL_LEFT_CELL_RIGHT,
                                   SIZE_MAX) == 0);

    assert(UI_BOTTOM_FOOTER_Y + UI_BOTTOM_FOOTER_HEIGHT <=
           UI_BOTTOM_SCREEN_HEIGHT);
    assert(UI_BOTTOM_STATUS_X + UI_BOTTOM_STATUS_WIDTH <=
           UI_BOTTOM_BATTERY_REGION_X);
    assert(UI_BOTTOM_BATTERY_X >= UI_BOTTOM_BATTERY_REGION_X);
    assert(UI_BOTTOM_BATTERY_X + UI_BOTTOM_BATTERY_WIDTH <=
           UI_BOTTOM_SCREEN_WIDTH);
    assert(UI_BOTTOM_BATTERY_Y >= UI_BOTTOM_FOOTER_Y);
    assert(UI_BOTTOM_BATTERY_Y + UI_BOTTOM_BATTERY_HEIGHT <=
           UI_BOTTOM_SCREEN_HEIGHT);

    assert(last_row_bottom(UI_RECOMMEND_FIRST_ROW_Y,
                           UI_RECOMMEND_ROW_STEP,
                           UI_RECOMMEND_VISIBLE_ROWS) <=
           UI_TOP_PAGE_FOOTER_Y);
    assert(last_row_bottom(UI_LIBRARY_PLAYLIST_FIRST_ROW_Y,
                           UI_LIBRARY_PLAYLIST_ROW_STEP,
                           UI_LIBRARY_PLAYLIST_VISIBLE_ROWS) <=
           UI_TOP_PAGE_FOOTER_Y);
    assert(last_row_bottom(UI_LIBRARY_TRACK_FIRST_ROW_Y,
                           UI_LIBRARY_TRACK_ROW_STEP,
                           UI_LIBRARY_TRACK_VISIBLE_ROWS) <=
           UI_TOP_PAGE_FOOTER_Y);
    assert(last_row_bottom(UI_SEARCH_FIRST_ROW_Y,
                           UI_SEARCH_ROW_STEP,
                           UI_SEARCH_VISIBLE_ROWS) <=
           UI_TOP_PAGE_FOOTER_Y);
    assert(last_row_bottom(UI_ALBUM_FIRST_ROW_Y,
                           UI_ALBUM_ROW_STEP,
                           NM3DS_ALBUM_VISIBLE_ROWS) <=
           UI_ALBUM_SCROLLBAR_Y + UI_ALBUM_SCROLLBAR_HEIGHT);
    assert(UI_ALBUM_SCROLLBAR_X + UI_ALBUM_SCROLLBAR_WIDTH <=
           UI_TOP_SCREEN_WIDTH);
    int album_thumb = ui_scrollbar_thumb_height(
        UI_ALBUM_SCROLLBAR_HEIGHT, NM3DS_ALBUM_VISIBLE_ROWS,
        NM3DS_ALBUM_PAGE,
        UI_ALBUM_SCROLLBAR_MIN_THUMB_HEIGHT);
    assert(album_thumb >= UI_ALBUM_SCROLLBAR_MIN_THUMB_HEIGHT);
    assert(album_thumb < UI_ALBUM_SCROLLBAR_HEIGHT);
    assert(ui_scrollbar_thumb_y(
               UI_ALBUM_SCROLLBAR_Y, UI_ALBUM_SCROLLBAR_HEIGHT,
               album_thumb, 0, NM3DS_ALBUM_VISIBLE_ROWS,
               NM3DS_ALBUM_PAGE) ==
           UI_ALBUM_SCROLLBAR_Y);
    assert(ui_scrollbar_thumb_y(
               UI_ALBUM_SCROLLBAR_Y, UI_ALBUM_SCROLLBAR_HEIGHT,
               album_thumb, 1, NM3DS_ALBUM_VISIBLE_ROWS,
               NM3DS_ALBUM_PAGE) +
           album_thumb ==
           UI_ALBUM_SCROLLBAR_Y + UI_ALBUM_SCROLLBAR_HEIGHT);
    assert(UI_QUEUE_VISIBLE_ROWS == 4);

    assert(UI_CONTENT_LIST_SCROLLBAR_X >= 0);
    assert(UI_CONTENT_LIST_SCROLLBAR_X +
           UI_CONTENT_LIST_SCROLLBAR_WIDTH <= UI_TOP_SCREEN_WIDTH);
    assert(UI_RECOMMEND_SCROLLBAR_Y <= UI_RECOMMEND_FIRST_ROW_Y);
    assert(UI_RECOMMEND_SCROLLBAR_Y + UI_RECOMMEND_SCROLLBAR_HEIGHT <=
           UI_TOP_PAGE_FOOTER_Y);
    assert(UI_LIBRARY_PLAYLIST_SCROLLBAR_Y <=
           UI_LIBRARY_PLAYLIST_FIRST_ROW_Y);
    assert(UI_LIBRARY_PLAYLIST_SCROLLBAR_Y +
           UI_LIBRARY_PLAYLIST_SCROLLBAR_HEIGHT <= UI_TOP_PAGE_FOOTER_Y);
    assert(UI_LIBRARY_TRACK_SCROLLBAR_Y <= UI_LIBRARY_TRACK_FIRST_ROW_Y);
    assert(UI_LIBRARY_TRACK_SCROLLBAR_Y +
           UI_LIBRARY_TRACK_SCROLLBAR_HEIGHT <= UI_TOP_PAGE_FOOTER_Y);
    assert(UI_SEARCH_SCROLLBAR_Y <= UI_SEARCH_FIRST_ROW_Y);
    assert(UI_SEARCH_SCROLLBAR_Y + UI_SEARCH_SCROLLBAR_HEIGHT <=
           UI_TOP_PAGE_FOOTER_Y);

    assert(ui_scrollbar_thumb_height(100, 8, 8, 16) == 100);
    assert(ui_scrollbar_thumb_height(100, 8, 16, 16) == 50);
    assert(ui_scrollbar_thumb_height(100, 1, 100, 16) == 16);
    int list_thumb_height =
        ui_scrollbar_thumb_height(100, 6, NM3DS_MAX_RESULTS, 16);
    assert(ui_scrollbar_thumb_y(10, 100, list_thumb_height,
                                0, 6, NM3DS_MAX_RESULTS) == 10);
    assert(ui_scrollbar_thumb_y(10, 100, list_thumb_height,
                                6, 6, NM3DS_MAX_RESULTS) +
           list_thumb_height == 110);

    assert(UI_SETTINGS_CACHE_Y + UI_SETTINGS_CACHE_HEIGHT <=
           UI_SETTINGS_LANGUAGE_Y);
    assert(UI_SETTINGS_LANGUAGE_Y + UI_SETTINGS_LANGUAGE_HEIGHT <=
           UI_SETTINGS_LIMIT_Y);
    assert(UI_SETTINGS_LIMIT_Y + UI_SETTINGS_LIMIT_HEIGHT <=
           UI_SETTINGS_DEBUG_Y);
    assert(UI_SETTINGS_LIMIT_OPTION_STEP >=
           UI_SETTINGS_LIMIT_OPTION_WIDTH);
    assert(UI_SETTINGS_LIMIT_OPTION_X >= 10);
    assert(UI_SETTINGS_LIMIT_OPTION_X +
           (NM3DS_CACHE_LIMIT_OPTION_COUNT - 1) *
               UI_SETTINGS_LIMIT_OPTION_STEP +
           UI_SETTINGS_LIMIT_OPTION_WIDTH <= 390);
    assert(UI_SETTINGS_DEBUG_Y + UI_SETTINGS_DEBUG_HEIGHT <=
           UI_SETTINGS_CLEAR_Y);
    assert(UI_SETTINGS_CLEAR_Y + UI_SETTINGS_CLEAR_HEIGHT <=
           UI_SETTINGS_CONTACT_Y);
    assert(UI_SETTINGS_CONTACT_Y + UI_SETTINGS_CONTACT_HEIGHT <=
           UI_SETTINGS_REPOSITORY_Y);
    assert(UI_SETTINGS_REPOSITORY_Y + UI_SETTINGS_REPOSITORY_HEIGHT <=
           UI_SETTINGS_USAGE_NOTICE_Y);
    assert(UI_SETTINGS_USAGE_NOTICE_Y + UI_SETTINGS_USAGE_NOTICE_HEIGHT <=
           UI_SETTINGS_VERSION_Y);
    assert(UI_SETTINGS_VERSION_Y + UI_SETTINGS_VERSION_HEIGHT ==
           UI_SETTINGS_CONTENT_BOTTOM);
    assert(UI_SETTINGS_CONTENT_BOTTOM >
           UI_TOP_SCREEN_HEIGHT);

    assert(SETTINGS_ITEM_COUNT == 8);
    assert(SETTINGS_CONTACT == SETTINGS_CACHE_CLEAR + 1);
    assert(SETTINGS_REPOSITORY == SETTINGS_CONTACT + 1);
    assert(SETTINGS_USAGE_NOTICE == SETTINGS_REPOSITORY + 1);
    assert(SETTINGS_VERSION == SETTINGS_USAGE_NOTICE + 1);
    assert(settings_item_is_interactive(SETTINGS_LANGUAGE));
    assert(settings_item_is_interactive(SETTINGS_CACHE_CLEAR));
    assert(!settings_item_is_interactive(SETTINGS_VERSION));
    assert(!settings_item_is_interactive(SETTINGS_REPOSITORY));
    assert(!settings_item_is_interactive(SETTINGS_CONTACT));
    assert(!settings_item_is_interactive(SETTINGS_USAGE_NOTICE));
    assert(settings_item_is_adjustable(SETTINGS_CACHE_LIMIT));
    assert(!settings_item_is_adjustable(SETTINGS_CACHE_CLEAR));
    assert(!settings_item_is_adjustable(SETTINGS_VERSION));

    int contact_scroll = ui_settings_scroll_offset_for_row(
        UI_SETTINGS_CONTACT_Y, UI_SETTINGS_CONTACT_HEIGHT);
    assert(contact_scroll > 0);
    assert(UI_SETTINGS_CONTACT_Y < UI_SETTINGS_VIEW_BOTTOM);
    assert(UI_SETTINGS_CONTACT_Y + UI_SETTINGS_CONTACT_HEIGHT >
           UI_SETTINGS_VIEW_BOTTOM);
    assert(ui_settings_row_is_visible(UI_SETTINGS_CONTACT_Y,
                                      UI_SETTINGS_CONTACT_HEIGHT, 0));
    assert(ui_settings_row_is_visible(UI_SETTINGS_CONTACT_Y,
                                      UI_SETTINGS_CONTACT_HEIGHT,
                                      contact_scroll));
    assert(!ui_settings_row_is_visible(UI_SETTINGS_CACHE_Y,
                                       UI_SETTINGS_CACHE_HEIGHT,
                                       contact_scroll));
    int notice_scroll = ui_settings_scroll_offset_for_row(
        UI_SETTINGS_USAGE_NOTICE_Y, UI_SETTINGS_USAGE_NOTICE_HEIGHT);
    assert(notice_scroll > contact_scroll);
    assert(ui_settings_row_is_visible(UI_SETTINGS_USAGE_NOTICE_Y,
                                      UI_SETTINGS_USAGE_NOTICE_HEIGHT,
                                      notice_scroll));
    int version_scroll = ui_settings_scroll_offset_for_row(
        UI_SETTINGS_VERSION_Y, UI_SETTINGS_VERSION_HEIGHT);
    assert(version_scroll > notice_scroll);
    assert(ui_settings_row_is_visible(UI_SETTINGS_VERSION_Y,
                                      UI_SETTINGS_VERSION_HEIGHT,
                                      version_scroll));

    assert(UI_SETTINGS_SCROLLBAR_X + UI_SETTINGS_SCROLLBAR_WIDTH <=
           UI_TOP_SCREEN_WIDTH);
    assert(UI_SETTINGS_SCROLLBAR_Y >= UI_SETTINGS_VIEW_TOP);
    assert(UI_SETTINGS_SCROLLBAR_Y + UI_SETTINGS_SCROLLBAR_HEIGHT <=
           UI_SETTINGS_VIEW_BOTTOM);
    int thumb_height = ui_settings_scrollbar_thumb_height();
    assert(thumb_height >= UI_SETTINGS_SCROLLBAR_MIN_THUMB_HEIGHT);
    assert(thumb_height < UI_SETTINGS_SCROLLBAR_HEIGHT);
    assert(ui_settings_scrollbar_thumb_y(0) == UI_SETTINGS_SCROLLBAR_Y);
    assert(ui_settings_scrollbar_thumb_y(version_scroll) + thumb_height ==
           UI_SETTINGS_SCROLLBAR_Y + UI_SETTINGS_SCROLLBAR_HEIGHT);
    assert(ui_settings_can_scroll_down(0));
    assert(ui_settings_can_scroll_down(notice_scroll));
    assert(!ui_settings_can_scroll_down(version_scroll));

    assert(UI_SETTINGS_DOWN_HINT_X >= 0);
    assert(UI_SETTINGS_DOWN_HINT_X + UI_SETTINGS_DOWN_HINT_WIDTH <=
           UI_TOP_SCREEN_WIDTH);
    assert(UI_SETTINGS_DOWN_HINT_X + UI_SETTINGS_DOWN_HINT_WIDTH / 2 ==
           UI_TOP_SCREEN_WIDTH / 2);
    assert(UI_SETTINGS_DOWN_HINT_Y >= UI_SETTINGS_VIEW_TOP);
    assert(UI_SETTINGS_DOWN_HINT_Y + UI_SETTINGS_DOWN_HINT_HEIGHT <=
           UI_SETTINGS_VIEW_BOTTOM);

    puts("ui layout tests: ok");
    return 0;
}
