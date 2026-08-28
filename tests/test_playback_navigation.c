#include "playback_navigation.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AppState app = {0};

    app.tab = TAB_DISCOVER;
    app.discover_section = DISCOVER_RECOMMENDATIONS;
    app.discover_source = RECOMMEND_SOURCE_PUBLIC;
    assert(playback_selection_stays_on_page(&app));
    app.discover_source = RECOMMEND_SOURCE_DAILY;
    assert(playback_selection_stays_on_page(&app));

    app.discover_section = DISCOVER_LIBRARY;
    app.library_view = LIBRARY_TRACKS;
    assert(playback_selection_stays_on_page(&app));

    app.tab = TAB_NOW_PLAYING;
    app.now_playing_view = NOW_PLAYING_ALBUM;
    assert(playback_selection_stays_on_page(&app));
    assert(playback_back_should_preserve_extras(&app));

    app.now_playing_view = NOW_PLAYING_DEFAULT;
    assert(!playback_selection_stays_on_page(&app));
    assert(!playback_back_should_preserve_extras(&app));

    app.now_playing_view = NOW_PLAYING_ARTIST_PICKER;
    assert(playback_selection_stays_on_page(&app));
    assert(playback_back_should_preserve_extras(&app));
    app.now_playing_view = NOW_PLAYING_ARTIST_ALBUMS;
    assert(playback_selection_stays_on_page(&app));
    app.now_playing_view = NOW_PLAYING_ARTIST_SONGS;
    assert(playback_selection_stays_on_page(&app));

    app.now_playing_view = NOW_PLAYING_DEFAULT;
    app.tab = TAB_DISCOVER;
    app.discover_section = DISCOVER_SEARCH;
    assert(playback_selection_stays_on_page(&app));
    assert(playback_back_should_preserve_extras(&app));

    app.discover_section = DISCOVER_CLOUD;
    assert(playback_selection_stays_on_page(&app));
    assert(playback_back_should_preserve_extras(&app));

    app.discover_section = DISCOVER_HOME;
    assert(!playback_selection_stays_on_page(&app));
    app.tab = TAB_SETTINGS;
    assert(!playback_selection_stays_on_page(&app));
    assert(!playback_selection_stays_on_page(NULL));
    assert(!playback_back_should_preserve_extras(NULL));

    size_t target_offset = 99;
    int target_selected = 99;
    assert(playback_album_page_target(
        0, true, 1, &target_offset, &target_selected));
    assert(target_offset == NM3DS_ALBUM_PAGE);
    assert(target_selected == 0);
    assert(playback_album_page_target(
        NM3DS_ALBUM_PAGE, false, -1,
        &target_offset, &target_selected));
    assert(target_offset == 0);
    assert(target_selected == -1);
    assert(!playback_album_page_target(
        0, false, 1, &target_offset, &target_selected));
    assert(!playback_album_page_target(
        0, true, -1, &target_offset, &target_selected));
    assert(!playback_album_page_target(
        0, true, 0, &target_offset, &target_selected));
    assert(!playback_album_page_target(0, true, 1, NULL, &target_selected));
    assert(!playback_album_page_target(0, true, 1, &target_offset, NULL));

    puts("playback navigation tests: ok");
    return 0;
}
