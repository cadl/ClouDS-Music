#include "playback_navigation.h"

#include <stdint.h>

bool playback_selection_stays_on_page(const AppState *app) {
    if (!app) return false;
    if (now_playing_view_has_detail(app->now_playing_view)) return true;
    if (app->tab != TAB_DISCOVER) return false;
    if (app->discover_section == DISCOVER_RECOMMENDATIONS) return true;
    if (app->discover_section == DISCOVER_SEARCH) return true;
    if (app->discover_section == DISCOVER_CLOUD) return true;
    return app->discover_section == DISCOVER_LIBRARY &&
           app->library_view == LIBRARY_TRACKS;
}

bool playback_back_should_preserve_extras(const AppState *app) {
    if (!app) return false;
    return app->tab != TAB_NOW_PLAYING ||
           now_playing_view_has_detail(app->now_playing_view);
}

bool playback_album_page_target(size_t current_offset, bool has_more,
                                int direction, size_t *target_offset,
                                int *target_selected) {
    if (!target_offset || !target_selected || direction == 0) return false;
    if (direction > 0) {
        if (!has_more || current_offset > SIZE_MAX - NM3DS_ALBUM_PAGE)
            return false;
        *target_offset = current_offset + NM3DS_ALBUM_PAGE;
        *target_selected = 0;
        return true;
    }
    if (current_offset == 0) return false;
    *target_offset = current_offset >= NM3DS_ALBUM_PAGE ?
                     current_offset - NM3DS_ALBUM_PAGE : 0;
    *target_selected = -1;
    return true;
}
