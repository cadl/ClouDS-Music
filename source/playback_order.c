#include "playback_order.h"

int playback_next_index(size_t queue_count, int current_index,
                        PlayMode play_mode, uint64_t entropy) {
    if (queue_count == 0) return -1;

    bool has_current = current_index >= 0 &&
                       (size_t)current_index < queue_count;
    if (play_mode != PLAY_MODE_SHUFFLE)
        return has_current ?
               (current_index + 1) % (int)queue_count : 0;

    if (!has_current)
        return (int)(entropy % queue_count);
    if (queue_count == 1) return current_index;

    int next = (int)(entropy % (queue_count - 1));
    if (next >= current_index) next++;
    return next;
}

int playback_next_available_index(size_t queue_count, int current_index,
                                  PlayMode play_mode, uint64_t entropy,
                                  const bool *available) {
    if (!available || queue_count == 0) return -1;
    size_t available_count = 0;
    for (size_t i = 0; i < queue_count; i++)
        if (available[i]) available_count++;
    if (available_count == 0) return -1;

    bool has_current = current_index >= 0 &&
                       (size_t)current_index < queue_count;
    bool current_available = has_current &&
                             available[current_index];
    if (play_mode != PLAY_MODE_SHUFFLE) {
        int start = has_current ? current_index : -1;
        for (size_t offset = 1; offset <= queue_count; offset++) {
            int candidate = (start + (int)offset) % (int)queue_count;
            if (available[candidate]) return candidate;
        }
        return -1;
    }

    size_t choices = available_count - (current_available ? 1U : 0U);
    if (choices == 0) return current_index;
    size_t wanted = (size_t)(entropy % choices);
    for (size_t i = 0; i < queue_count; i++) {
        if (!available[i] || (current_available && (int)i == current_index))
            continue;
        if (wanted == 0) return (int)i;
        wanted--;
    }
    return -1;
}
