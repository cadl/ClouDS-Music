#pragma once

#include "model.h"

#include <stdbool.h>

bool playback_selection_stays_on_page(const AppState *app);
bool playback_back_should_preserve_extras(const AppState *app);
bool playback_page_target(size_t current_offset, size_t page_size,
                          bool has_more, int direction,
                          size_t *target_offset, int *target_selected);
