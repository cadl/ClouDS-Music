#pragma once

#include <stdbool.h>

bool playback_should_prevent_sleep(bool player_active, bool player_paused,
                                   bool playback_pending,
                                   bool headset_connected);
/* Returns true on alternate 60 Hz updates for a 30 FPS presentation rate. */
bool ui_render_due(unsigned int update_frame);
