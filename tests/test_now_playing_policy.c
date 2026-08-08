#include "now_playing_policy.h"
#include "immersive_lyrics.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(now_playing_display_index(2, 0, 1) == 0);
    assert(!now_playing_display_is_pending(2, 0, 1));

    assert(now_playing_display_index(2, -1, 1) == 1);
    assert(now_playing_display_is_pending(2, -1, 1));
    assert(now_playing_extras_should_apply(2, -1, 1, 1));

    assert(now_playing_display_index(2, 3, 1) == 1);
    assert(now_playing_display_is_pending(2, 3, 1));
    assert(now_playing_extras_should_apply(2, 3, 1, 1));

    assert(now_playing_display_index(2, -1, 3) == -1);
    assert(!now_playing_display_is_pending(2, -1, 3));
    assert(!now_playing_extras_should_apply(2, -1, 3, 1));

    assert(now_playing_extras_should_apply(2, 0, 1, 0));
    assert(!now_playing_extras_should_apply(2, 0, 1, 1));
    assert(!now_playing_extras_should_apply(2, 0, 1, -1));
    assert(!now_playing_extras_should_apply(2, 0, 1, 2));

    AppState app = {0};
    app.queue_count = 2;
    app.queue[0].id = 10;
    app.queue[1].id = 20;
    app.current_queue = 0;
    app.pending_queue = 1;
    app.lyric_count = 3;
    app.lyric_song_id = 10;
    assert(immersive_lyrics_available(&app));

    app.lyric_song_id = 20;
    assert(!immersive_lyrics_available(&app));
    app.current_queue = -1;
    assert(immersive_lyrics_available(&app));

    app.lyric_count = 0;
    assert(!immersive_lyrics_available(&app));
    app.lyric_count = 3;
    app.pending_queue = 3;
    assert(!immersive_lyrics_available(&app));

    assert(immersive_lyrics_style_valid(IMMERSIVE_LYRIC_STYLE_WHEEL));
    assert(!immersive_lyrics_style_valid((ImmersiveLyricStyle)-1));
    assert(!immersive_lyrics_style_valid(IMMERSIVE_LYRIC_STYLE_COUNT));
    assert(immersive_lyrics_next_style(IMMERSIVE_LYRIC_STYLE_WHEEL) ==
           IMMERSIVE_LYRIC_STYLE_FLIP);
    assert(immersive_lyrics_next_style(IMMERSIVE_LYRIC_STYLE_FLIP) ==
           IMMERSIVE_LYRIC_STYLE_FADE);
    assert(immersive_lyrics_next_style(IMMERSIVE_LYRIC_STYLE_FADE) ==
           IMMERSIVE_LYRIC_STYLE_CRAWL);
    assert(immersive_lyrics_next_style(IMMERSIVE_LYRIC_STYLE_CRAWL) ==
           IMMERSIVE_LYRIC_STYLE_WHEEL);
    assert(immersive_lyrics_next_style(IMMERSIVE_LYRIC_STYLE_COUNT) ==
           IMMERSIVE_LYRIC_STYLE_WHEEL);
    assert(immersive_lyrics_style_name(IMMERSIVE_LYRIC_STYLE_FLIP)[0]);

    assert(immersive_lyrics_controls_alpha(1000, 1000) == 1.0f);
    assert(immersive_lyrics_controls_alpha(4000, 1000) == 1.0f);
    assert(immersive_lyrics_controls_alpha(4500, 1000) == 0.5f);
    assert(immersive_lyrics_controls_alpha(5000, 1000) == 0.0f);
    assert(immersive_lyrics_flip_progress(1000, 1000) == 0.0f);
    assert(immersive_lyrics_flip_progress(1300, 1000) == 0.5f);
    assert(immersive_lyrics_flip_progress(1600, 1000) == 1.0f);
    assert(immersive_lyrics_fade_alpha(999, 1000, 2000) == 0.0f);
    assert(immersive_lyrics_fade_alpha(1000, 1000, 2000) == 1.0f);
    assert(immersive_lyrics_fade_alpha(1500, 1000, 2000) == 1.0f);
    assert(immersive_lyrics_fade_alpha(1750, 1000, 2000) == 1.0f);
    assert(immersive_lyrics_fade_alpha(1875, 1000, 2000) == 0.5f);
    assert(immersive_lyrics_fade_alpha(2000, 1000, 2000) == 0.0f);
    assert(immersive_lyrics_fade_alpha(1000, 1000, 1000) == 1.0f);
    /* Long lines cap the fade at 400 ms; very short lines still remain fully
     * visible for their first three quarters. */
    assert(immersive_lyrics_fade_alpha(4600, 1000, 5000) == 1.0f);
    assert(immersive_lyrics_fade_alpha(4800, 1000, 5000) == 0.5f);
    assert(immersive_lyrics_fade_alpha(1150, 1000, 1200) == 1.0f);
    assert(immersive_lyrics_fade_alpha(1175, 1000, 1200) == 0.5f);
    assert(immersive_lyrics_crawl_progress(1000, 1000, 2000) == 0.0f);
    assert(immersive_lyrics_crawl_progress(1250, 1000, 2000) == 0.25f);
    assert(immersive_lyrics_crawl_progress(1500, 1000, 2000) == 0.5f);
    assert(immersive_lyrics_crawl_progress(1750, 1000, 2000) == 0.75f);
    assert(immersive_lyrics_crawl_progress(2000, 1000, 2000) == 1.0f);
    puts("now playing policy tests passed");
    return 0;
}
