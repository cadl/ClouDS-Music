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
    assert(ui_render_due(0));
    assert(!ui_render_due(1));
    assert(ui_render_due(2));
    assert(!ui_render_due(3));
    puts("power policy tests passed");
    return 0;
}
