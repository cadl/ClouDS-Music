#include "lyric_animation.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void assert_near(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.0001f);
}

int main(void) {
    LyricAnimation animation;
    lyric_animation_clear(&animation);
    lyric_animation_update(&animation, 42, 12, 0, 0);

    LyricAnimationFrame frame = lyric_animation_frame(&animation, 0);
    assert(frame.ready);
    assert_near(frame.focus, 0.0f);
    assert_near(frame.scroll, 0.0f);

    lyric_animation_update(&animation, 42, 12, 1, 10);
    frame = lyric_animation_frame(&animation, 60);
    float interrupted_focus = frame.focus;
    assert(interrupted_focus > 0.0f && interrupted_focus < 1.0f);

    /* A second short line must continue from the visible in-between state,
     * rather than snapping back to the previous discrete line. */
    lyric_animation_update(&animation, 42, 12, 2, 60);
    frame = lyric_animation_frame(&animation, 60);
    assert_near(frame.focus, interrupted_focus);
    frame = lyric_animation_frame(&animation, 110);
    assert(frame.focus > interrupted_focus && frame.focus < 2.0f);

    frame = lyric_animation_frame(&animation, 280);
    assert_near(frame.focus, 2.0f);
    lyric_animation_finish(&animation, &frame);
    assert(!animation.transitioning);

    lyric_animation_update(&animation, 42, 12, 7, 290);
    frame = lyric_animation_frame(&animation, 290);
    assert_near(frame.focus, 7.0f);
    assert_near(frame.scroll, 4.0f);

    lyric_animation_update(&animation, 99, 3, 1, 300);
    frame = lyric_animation_frame(&animation, 300);
    assert_near(frame.focus, 1.0f);
    assert_near(frame.scroll, 0.0f);

    assert_near(lyric_animation_eye_shift(3, 3.0f), 2.40f);
    assert_near(lyric_animation_eye_shift(2, 3.0f), 1.45f);
    assert_near(lyric_animation_eye_shift(4, 3.0f), 1.45f);
    assert_near(lyric_animation_eye_shift(1, 3.0f), 0.90f);
    assert_near(lyric_animation_eye_shift(5, 3.0f), 0.90f);
    assert_near(lyric_animation_eye_shift(0, 3.0f), 0.45f);
    assert_near(lyric_animation_eye_shift(6, 3.0f), 0.45f);

    assert_near(lyric_animation_depth_emphasis(3, 3.0f), 1.00f);
    assert_near(lyric_animation_depth_emphasis(2, 3.0f), 0.24f);
    assert_near(lyric_animation_depth_emphasis(4, 3.0f), 0.24f);
    assert_near(lyric_animation_depth_emphasis(1, 3.0f), 0.08f);
    assert_near(lyric_animation_depth_emphasis(5, 3.0f), 0.08f);
    assert_near(lyric_animation_depth_emphasis(0, 3.0f), 0.00f);
    assert_near(lyric_animation_depth_emphasis(6, 3.0f), 0.00f);

    float left_mid = lyric_animation_eye_shift(3, 3.5f);
    float right_mid = lyric_animation_eye_shift(4, 3.5f);
    assert_near(left_mid, right_mid);
    assert_near(left_mid, 1.925f);
    assert_near(lyric_animation_depth_emphasis(3, 3.5f), 0.62f);
    assert_near(lyric_animation_depth_emphasis(4, 3.5f), 0.62f);

    assert_near(lyric_animation_immersive_eye_shift(3, 3.0f), 3.05f);
    assert_near(lyric_animation_immersive_eye_shift(2, 3.0f), 1.95f);
    assert_near(lyric_animation_immersive_eye_shift(4, 3.0f), 1.95f);
    assert_near(lyric_animation_immersive_eye_shift(1, 3.0f), 1.05f);
    assert_near(lyric_animation_immersive_eye_shift(0, 3.0f), 0.55f);
    assert_near(lyric_animation_immersive_eye_shift(3, 3.5f), 2.50f);
    assert_near(lyric_animation_immersive_eye_shift(4, 3.5f), 2.50f);
    assert_near(lyric_animation_immersive_depth_shift(-1.0f), 0.20f);
    assert_near(lyric_animation_immersive_depth_shift(0.0f), 0.20f);
    assert_near(lyric_animation_immersive_depth_shift(0.5f), 1.775f);
    assert_near(lyric_animation_immersive_depth_shift(0.9f), 3.035f);
    assert_near(lyric_animation_immersive_depth_shift(2.0f), 3.35f);
    assert_near(lyric_animation_pixel_snap(12.49f), 12.0f);
    assert_near(lyric_animation_pixel_snap(12.50f), 13.0f);
    assert_near(lyric_animation_pixel_snap(-1.50f), -2.0f);

    /* Long active lines pause briefly at both ends and traverse the exact
     * overflow during the time available before the next lyric. */
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 2250, 1000, 6000), 0.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 3500, 1000, 6000), 46.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 4750, 1000, 6000), 92.0f);
    /* A two-second line gets the 800 ms minimum ending hold. */
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 1500, 1000, 3000), 0.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 1850, 1000, 3000), 46.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 2200, 1000, 3000), 92.0f);
    /* An extremely short line caps that hold at 40%. */
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 1600, 1000, 2000), 92.0f);
    assert_near(lyric_animation_horizontal_offset(
                    180.0f, 208.0f, 4000, 1000, 6000), 0.0f);
    assert_near(lyric_animation_horizontal_offset(
                    300.0f, 208.0f, 4000, 2000, 2000), 0.0f);

    puts("lyric animation tests: ok");
    return 0;
}
