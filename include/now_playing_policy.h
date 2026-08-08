#pragma once

#include <stdbool.h>
#include <stddef.h>

int now_playing_display_index(size_t queue_count, int current_index,
                              int pending_index);
bool now_playing_display_is_pending(size_t queue_count, int current_index,
                                    int pending_index);
bool now_playing_extras_should_apply(size_t queue_count, int current_index,
                                     int pending_index, int result_index);
