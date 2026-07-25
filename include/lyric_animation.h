#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LYRIC_ANIMATION_VISIBLE_ROWS 7
#define LYRIC_ANIMATION_DURATION_MS 220U
#define LYRIC_HORIZONTAL_FALLBACK_DURATION_MS 4000U

typedef struct {
    bool ready;
    bool transitioning;
    int64_t song_id;
    size_t line_count;
    int active_index;
    float scroll_from;
    float scroll_target;
    float focus_from;
    float focus_target;
    uint64_t transition_started_ms;
} LyricAnimation;

typedef struct {
    bool ready;
    float transition;
    float scroll;
    float focus;
    int active_index;
    uint32_t playback_ms;
    uint32_t active_line_start_ms;
    uint32_t active_line_end_ms;
} LyricAnimationFrame;

void lyric_animation_clear(LyricAnimation *animation);
void lyric_animation_update(LyricAnimation *animation, int64_t song_id,
                            size_t line_count, int active_index,
                            uint64_t now_ms);
LyricAnimationFrame lyric_animation_frame(const LyricAnimation *animation,
                                           uint64_t now_ms);
void lyric_animation_finish(LyricAnimation *animation,
                            const LyricAnimationFrame *frame);
float lyric_animation_line_focus(int line_index, float focus_index);
float lyric_animation_eye_shift(int line_index, float focus_index);
float lyric_animation_depth_emphasis(int line_index, float focus_index);
float lyric_animation_immersive_eye_shift(int line_index, float focus_index);
float lyric_animation_immersive_depth_shift(float depth);
float lyric_animation_pixel_snap(float value);
float lyric_animation_horizontal_offset(float text_width, float viewport_width,
                                        uint32_t playback_ms,
                                        uint32_t line_start_ms,
                                        uint32_t line_end_ms);
