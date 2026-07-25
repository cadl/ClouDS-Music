#include "lyric_animation.h"

#include <math.h>
#include <string.h>

#define LYRIC_HORIZONTAL_MIN_TAIL_HOLD_MS 800U

/* Per-eye offsets at the maximum slider position. Keep the total near-to-far
 * disparity range close to the original 3.5 px comfort budget: lyric changes
 * combine this horizontal vergence motion with vertical scrolling. */
static const float LYRIC_3D_TIER_EYE_SHIFTS[] = {
    2.40f, 1.45f, 0.90f, 0.45f
};

static const float LYRIC_3D_TIER_EMPHASIS[] = {
    1.00f, 0.24f, 0.08f, 0.00f
};

/* Immersive lyrics use native bitmap glyphs and therefore snap each eye to a
 * whole pixel. These values deliberately land the nearest, middle, and far
 * rows on distinct 3/2/1-pixel tiers at the maximum slider position. The
 * nearest row is six pixels apart across both eyes, while the visible
 * near-to-far range stays close to four pixels. */
static const float LYRIC_IMMERSIVE_3D_TIER_EYE_SHIFTS[] = {
    3.05f, 1.95f, 1.05f, 0.55f
};

static int first_visible_row(int active_index, size_t line_count) {
    int first = active_index - LYRIC_ANIMATION_VISIBLE_ROWS / 2;
    int maximum = line_count > LYRIC_ANIMATION_VISIBLE_ROWS ?
                  (int)line_count - LYRIC_ANIMATION_VISIBLE_ROWS : 0;
    if (first < 0) first = 0;
    if (first > maximum) first = maximum;
    return first;
}

static float ease(float progress) {
    if (progress <= 0.0f) return 0.0f;
    if (progress >= 1.0f) return 1.0f;
    return progress * progress * (3.0f - 2.0f * progress);
}

float lyric_animation_horizontal_offset(float text_width, float viewport_width,
                                        uint32_t playback_ms,
                                        uint32_t line_start_ms,
                                        uint32_t line_end_ms) {
    float overflow = text_width - viewport_width;
    if (overflow <= 0.0f || line_end_ms <= line_start_ms ||
        playback_ms <= line_start_ms) return 0.0f;

    uint32_t duration = line_end_ms - line_start_ms;
    /* Keep the beginning visible for one quarter. Give the ending at least
     * 800 ms when possible, but never consume more than 40% of a short line. */
    uint32_t lead_in = duration / 4U;
    uint32_t tail_hold = duration / 4U;
    if (tail_hold < LYRIC_HORIZONTAL_MIN_TAIL_HOLD_MS)
        tail_hold = LYRIC_HORIZONTAL_MIN_TAIL_HOLD_MS;
    uint32_t maximum_tail = (uint32_t)(((uint64_t)duration * 2U) / 5U);
    if (tail_hold > maximum_tail) tail_hold = maximum_tail;

    uint32_t movement_start = line_start_ms + lead_in;
    uint32_t movement_end = line_end_ms - tail_hold;
    if (playback_ms <= movement_start) return 0.0f;
    if (playback_ms >= movement_end || movement_end <= movement_start)
        return overflow;

    float progress = (float)(playback_ms - movement_start) /
                     (float)(movement_end - movement_start);
    return overflow * progress;
}

float lyric_animation_line_focus(int line_index, float focus_index) {
    float amount = 1.0f - fabsf((float)line_index - focus_index);
    if (amount < 0.0f) return 0.0f;
    if (amount > 1.0f) return 1.0f;
    return amount;
}

static float tier_value(const float *values, size_t count, float distance) {
    size_t last_tier = count - 1U;
    float value = values[last_tier];
    if (distance < (float)last_tier) {
        size_t tier = (size_t)distance;
        float fraction = distance - (float)tier;
        value = values[tier] +
            (values[tier + 1U] - values[tier]) * fraction;
    }
    return value;
}

float lyric_animation_eye_shift(int line_index, float focus_index) {
    float distance = fabsf((float)line_index - focus_index);
    return tier_value(LYRIC_3D_TIER_EYE_SHIFTS,
                      sizeof(LYRIC_3D_TIER_EYE_SHIFTS) /
                      sizeof(LYRIC_3D_TIER_EYE_SHIFTS[0]),
                      distance);
}

float lyric_animation_depth_emphasis(int line_index, float focus_index) {
    float distance = fabsf((float)line_index - focus_index);
    return tier_value(LYRIC_3D_TIER_EMPHASIS,
                      sizeof(LYRIC_3D_TIER_EMPHASIS) /
                      sizeof(LYRIC_3D_TIER_EMPHASIS[0]),
                      distance);
}

float lyric_animation_immersive_eye_shift(int line_index, float focus_index) {
    float distance = fabsf((float)line_index - focus_index);
    return tier_value(
        LYRIC_IMMERSIVE_3D_TIER_EYE_SHIFTS,
        sizeof(LYRIC_IMMERSIVE_3D_TIER_EYE_SHIFTS) /
            sizeof(LYRIC_IMMERSIVE_3D_TIER_EYE_SHIFTS[0]),
        distance);
}

float lyric_animation_immersive_depth_shift(float depth) {
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    return 0.20f + depth * 3.15f;
}

float lyric_animation_pixel_snap(float value) {
    return roundf(value);
}

static void reset(LyricAnimation *animation, int64_t song_id,
                  size_t line_count, int active_index, uint64_t now_ms) {
    float first = (float)first_visible_row(active_index, line_count);
    animation->ready = true;
    animation->transitioning = false;
    animation->song_id = song_id;
    animation->line_count = line_count;
    animation->active_index = active_index;
    animation->scroll_from = first;
    animation->scroll_target = first;
    animation->focus_from = (float)active_index;
    animation->focus_target = (float)active_index;
    animation->transition_started_ms = now_ms;
}

void lyric_animation_clear(LyricAnimation *animation) {
    if (!animation) return;
    memset(animation, 0, sizeof(*animation));
    animation->active_index = -1;
}

LyricAnimationFrame lyric_animation_frame(const LyricAnimation *animation,
                                           uint64_t now_ms) {
    LyricAnimationFrame frame = {0};
    if (!animation || !animation->ready) return frame;

    frame.ready = true;
    frame.transition = 1.0f;
    if (animation->transitioning) {
        uint64_t elapsed = now_ms > animation->transition_started_ms ?
                           now_ms - animation->transition_started_ms : 0;
        frame.transition = elapsed >= LYRIC_ANIMATION_DURATION_MS ? 1.0f :
            (float)elapsed / (float)LYRIC_ANIMATION_DURATION_MS;
    }
    float eased = ease(frame.transition);
    frame.scroll = animation->scroll_from +
        (animation->scroll_target - animation->scroll_from) * eased;
    frame.focus = animation->focus_from +
        (animation->focus_target - animation->focus_from) * eased;
    return frame;
}

void lyric_animation_update(LyricAnimation *animation, int64_t song_id,
                            size_t line_count, int active_index,
                            uint64_t now_ms) {
    if (!animation) return;
    if (line_count == 0) {
        lyric_animation_clear(animation);
        return;
    }
    if (active_index < 0) active_index = 0;
    if ((size_t)active_index >= line_count)
        active_index = (int)line_count - 1;
    if (!animation->ready || animation->song_id != song_id ||
        animation->line_count != line_count) {
        reset(animation, song_id, line_count, active_index, now_ms);
        return;
    }
    if (active_index == animation->active_index) return;

    LyricAnimationFrame current = lyric_animation_frame(animation, now_ms);
    float target_scroll =
        (float)first_visible_row(active_index, line_count);
    int active_delta = active_index - animation->active_index;
    if (active_delta < 0) active_delta = -active_delta;
    if (active_delta > 1 ||
        fabsf(target_scroll - current.scroll) > 1.5f) {
        reset(animation, song_id, line_count, active_index, now_ms);
        return;
    }

    animation->active_index = active_index;
    animation->scroll_from = current.scroll;
    animation->scroll_target = target_scroll;
    animation->focus_from = current.focus;
    animation->focus_target = (float)active_index;
    animation->transition_started_ms = now_ms;
    animation->transitioning = true;
}

void lyric_animation_finish(LyricAnimation *animation,
                            const LyricAnimationFrame *frame) {
    if (!animation || !frame || !frame->ready ||
        frame->transition < 1.0f) return;
    animation->transitioning = false;
    animation->scroll_from = animation->scroll_target;
    animation->focus_from = animation->focus_target;
}
