#include "media_policy.h"

uint64_t media_prebuffer_target(uint64_t total_bytes) {
    if (total_bytes == 0) return MEDIA_PREBUFFER_MIN_BYTES;
    uint64_t target = total_bytes / 100U * MEDIA_PREBUFFER_PERCENT;
    target += total_bytes % 100U * MEDIA_PREBUFFER_PERCENT / 100U;
    if (target < MEDIA_PREBUFFER_MIN_BYTES)
        target = MEDIA_PREBUFFER_MIN_BYTES;
    if (target > MEDIA_PREBUFFER_MAX_BYTES)
        target = MEDIA_PREBUFFER_MAX_BYTES;
    if (target > total_bytes) target = total_bytes;
    return target;
}

unsigned int media_download_percent(uint64_t received_bytes,
                                    uint64_t total_bytes) {
    if (total_bytes == 0) return 0;
    if (received_bytes >= total_bytes) return 100;
    return (unsigned int)((double)received_bytes * 100.0 /
                          (double)total_bytes);
}
