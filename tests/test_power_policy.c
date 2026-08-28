#include "power_policy.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(!playback_should_prevent_sleep(false, false, false, false));
    assert(!playback_should_prevent_sleep(true, false, false, false));
    assert(playback_should_prevent_sleep(true, false, false, true));
    assert(!playback_should_prevent_sleep(true, true, false, true));
    assert(playback_should_prevent_sleep(false, false, true, true));
    assert(!playback_should_prevent_sleep(false, false, true, false));
    assert(lid_skip_action(false, true, true, true, false, true, false) ==
           LID_SKIP_NONE);
    assert(lid_skip_action(true, false, true, true, false, true, false) ==
           LID_SKIP_NONE);
    assert(lid_skip_action(true, true, false, true, false, true, false) ==
           LID_SKIP_NONE);
    assert(lid_skip_action(true, true, true, true, false, true, false) ==
           LID_SKIP_PREVIOUS);
    assert(lid_skip_action(true, true, true, false, true, false, true) ==
           LID_SKIP_NEXT);
    assert(lid_skip_action(true, true, true, true, true, true, true) ==
           LID_SKIP_NONE);
    assert(lid_skip_action(true, true, true, false, true, true, true) ==
           LID_SKIP_NONE);
    assert(lid_skip_action(true, true, true, false, false, false, false) ==
           LID_SKIP_NONE);
    assert(ui_render_due(0));
    assert(!ui_render_due(1));
    assert(ui_render_due(2));
    assert(!ui_render_due(3));
    puts("power policy tests passed");
    return 0;
}
