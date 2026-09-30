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

uint64_t media_stream_start_target(uint64_t total_bytes,
                                   uint64_t audio_offset) {
    uint64_t audio_bytes = total_bytes > audio_offset ?
                           total_bytes - audio_offset : 0;
    uint64_t target = media_prebuffer_target(audio_bytes);
    return audio_offset > UINT64_MAX - target ? UINT64_MAX :
                                                audio_offset + target;
}

uint64_t media_rebuffer_bytes(uint64_t bytes_per_second, unsigned int seconds,
                              uint64_t minimum) {
    uint64_t target = seconds && bytes_per_second > UINT64_MAX / seconds ?
                      UINT64_MAX : bytes_per_second * seconds;
    if (target > MEDIA_PREBUFFER_MAX_BYTES) target = MEDIA_PREBUFFER_MAX_BYTES;
    return target < minimum ? minimum : target;
}

bool media_stream_can_resume(bool io_pending, bool finished, bool starved,
                             uint64_t available, uint64_t threshold) {
    if (io_pending) return false;
    return finished || !starved || available >= threshold;
}

unsigned int media_download_percent(uint64_t received_bytes,
                                    uint64_t total_bytes) {
    if (total_bytes == 0) return 0;
    if (received_bytes >= total_bytes) return 100;
    return (unsigned int)((double)received_bytes * 100.0 /
                          (double)total_bytes);
}
