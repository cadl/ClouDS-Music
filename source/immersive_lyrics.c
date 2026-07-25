#include "immersive_lyrics.h"

#include "now_playing_policy.h"

bool immersive_lyrics_available(const AppState *app) {
    if (!app || app->lyric_count == 0) return false;
    int index = now_playing_display_index(
        app->queue_count, app->current_queue, app->pending_queue);
    return index >= 0 && app->lyric_song_id == app->queue[index].id;
}

bool immersive_lyrics_style_valid(ImmersiveLyricStyle style) {
    return (unsigned int)style <
           (unsigned int)IMMERSIVE_LYRIC_STYLE_COUNT;
}

ImmersiveLyricStyle immersive_lyrics_next_style(ImmersiveLyricStyle style) {
    if (!immersive_lyrics_style_valid(style))
        return IMMERSIVE_LYRIC_STYLE_WHEEL;
    return (ImmersiveLyricStyle)(
        ((int)style + 1) % (int)IMMERSIVE_LYRIC_STYLE_COUNT);
}

const char *immersive_lyrics_style_name(ImmersiveLyricStyle style) {
    switch (style) {
        case IMMERSIVE_LYRIC_STYLE_WHEEL: return "歌词滚轮";
        case IMMERSIVE_LYRIC_STYLE_FLIP: return "中心翻转";
        case IMMERSIVE_LYRIC_STYLE_FADE: return "骤现渐隐";
        case IMMERSIVE_LYRIC_STYLE_CRAWL: return "星际字幕";
        default: return "歌词滚轮";
    }
}

float immersive_lyrics_controls_alpha(uint64_t now_ms,
                                      uint64_t shown_since_ms) {
    uint64_t elapsed = now_ms > shown_since_ms ?
                       now_ms - shown_since_ms : 0U;
    if (elapsed <= IMMERSIVE_CONTROLS_HOLD_MS) return 1.0f;
    elapsed -= IMMERSIVE_CONTROLS_HOLD_MS;
    if (elapsed >= IMMERSIVE_CONTROLS_FADE_MS) return 0.0f;
    return 1.0f - (float)elapsed / (float)IMMERSIVE_CONTROLS_FADE_MS;
}

static float smooth_progress(float progress) {
    if (progress <= 0.0f) return 0.0f;
    if (progress >= 1.0f) return 1.0f;
    return progress * progress * (3.0f - 2.0f * progress);
}

float immersive_lyrics_flip_progress(uint32_t playback_ms,
                                     uint32_t line_start_ms) {
    if (playback_ms <= line_start_ms) return 0.0f;
    uint32_t elapsed = playback_ms - line_start_ms;
    if (elapsed >= IMMERSIVE_FLIP_DURATION_MS) return 1.0f;
    return smooth_progress(
        (float)elapsed / (float)IMMERSIVE_FLIP_DURATION_MS);
}

float immersive_lyrics_fade_alpha(uint32_t playback_ms,
                                  uint32_t line_start_ms,
                                  uint32_t line_end_ms) {
    /* Keep the line fully visible until shortly before the next timestamp,
     * then fade it quickly. Limit the fade to the final quarter of short lines
     * so they never start disappearing as soon as they appear. Deriving this
     * only from playback time keeps pause and seek deterministic. */
    if (playback_ms < line_start_ms) return 0.0f;
    if (line_end_ms <= line_start_ms) return 1.0f;
    if (playback_ms >= line_end_ms) return 0.0f;

    uint32_t duration = line_end_ms - line_start_ms;
    uint32_t fade_duration = duration / 4U;
    if (fade_duration > IMMERSIVE_LYRIC_FADE_DURATION_MS)
        fade_duration = IMMERSIVE_LYRIC_FADE_DURATION_MS;
    if (fade_duration == 0U) return 1.0f;

    uint32_t fade_start_ms = line_end_ms - fade_duration;
    if (playback_ms <= fade_start_ms) return 1.0f;
    float progress = (float)(playback_ms - fade_start_ms) /
                     (float)fade_duration;
    return 1.0f - smooth_progress(progress);
}

float immersive_lyrics_crawl_progress(uint32_t playback_ms,
                                      uint32_t line_start_ms,
                                      uint32_t line_end_ms) {
    if (line_end_ms <= line_start_ms || playback_ms <= line_start_ms)
        return 0.0f;
    if (playback_ms >= line_end_ms) return 1.0f;
    /* Each timestamp advances the crawl by exactly one row. Keep that motion
     * linear so the group does not ease to a stop and accelerate again at
     * every lyric boundary. The row handoff remains position-continuous. */
    return (float)(playback_ms - line_start_ms) /
           (float)(line_end_ms - line_start_ms);
}
