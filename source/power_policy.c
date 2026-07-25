#include "power_policy.h"

bool playback_should_prevent_sleep(bool player_active, bool player_paused,
                                   bool playback_pending,
                                   bool headset_connected) {
    return headset_connected &&
           (playback_pending || (player_active && !player_paused));
}

bool ui_render_due(unsigned int update_frame) {
    return (update_frame & 1U) == 0U;
}
