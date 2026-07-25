#pragma once

#include "model.h"

#include <stddef.h>

int prefetch_queue_index(size_t queue_count, int current_index,
                         size_t checked_count);
