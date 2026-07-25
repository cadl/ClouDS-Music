#include "prefetch_policy.h"

int prefetch_queue_index(size_t queue_count, int current_index,
                         size_t checked_count) {
    size_t scan_count = queue_count > 1 ? queue_count - 1 : 0;
    if (scan_count > NM3DS_PREFETCH_SCAN_MAX)
        scan_count = NM3DS_PREFETCH_SCAN_MAX;
    if (queue_count < 2 || current_index < 0 ||
        (size_t)current_index >= queue_count ||
        checked_count >= scan_count)
        return -1;
    return (int)(((size_t)current_index + checked_count + 1) % queue_count);
}
