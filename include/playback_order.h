#pragma once

#include <stddef.h>
#include <stdint.h>

#include "model.h"

int playback_next_index(size_t queue_count, int current_index,
                        PlayMode play_mode, uint64_t entropy);
int playback_next_available_index(size_t queue_count, int current_index,
                                  PlayMode play_mode, uint64_t entropy,
                                  const bool *available);
