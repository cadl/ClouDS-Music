#include "power_policy.h"

bool playback_should_prevent_sleep(bool player_active, bool player_paused,
                                   bool playback_pending,
                                   bool headset_connected) {
    return headset_connected &&
           (playback_pending || (player_active && !player_paused));
}

LidSkipAction lid_skip_action(bool lid_lr_skip_enabled, bool shell_closed,
                              bool headset_connected, bool l_down,
                              bool r_down, bool l_held, bool r_held) {
    if (!lid_lr_skip_enabled || !shell_closed || !headset_connected ||
        l_down == r_down || (l_held && r_held))
        return LID_SKIP_NONE;
    return l_down ? LID_SKIP_PREVIOUS : LID_SKIP_NEXT;
}

bool ui_render_due(unsigned int update_frame) {
    return (update_frame & 1U) == 0U;
}
