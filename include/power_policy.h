#pragma once

#include <stdbool.h>

bool playback_should_prevent_sleep(bool player_active, bool player_paused,
                                   bool playback_pending,
                                   bool headset_connected);

typedef enum {
    LID_SKIP_NONE = 0,
    LID_SKIP_PREVIOUS,
    LID_SKIP_NEXT,
} LidSkipAction;

LidSkipAction lid_skip_action(bool lid_lr_skip_enabled, bool shell_closed,
                              bool headset_connected, bool l_down,
                              bool r_down, bool l_held, bool r_held);
/* Returns true on alternate 60 Hz updates for a 30 FPS presentation rate. */
bool ui_render_due(unsigned int update_frame);
