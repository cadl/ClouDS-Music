#include "now_playing_policy.h"

static bool valid_index(size_t queue_count, int index) {
    return index >= 0 && (size_t)index < queue_count;
}

int now_playing_display_index(size_t queue_count, int current_index,
                              int pending_index) {
    if (valid_index(queue_count, current_index)) return current_index;
    if (valid_index(queue_count, pending_index)) return pending_index;
    return -1;
}

bool now_playing_display_is_pending(size_t queue_count, int current_index,
                                    int pending_index) {
    return !valid_index(queue_count, current_index) &&
           valid_index(queue_count, pending_index);
}
