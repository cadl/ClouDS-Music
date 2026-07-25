#include "download_policy.h"

#include <stdint.h>

uint64_t download_space_budget(uint64_t free_bytes, uint64_t reserve_bytes) {
    return free_bytes > reserve_bytes ? free_bytes - reserve_bytes : 0;
}

DownloadLimitResult download_limit_check(uint64_t received,
                                         uint64_t incoming,
                                         uint64_t maximum_bytes,
                                         uint64_t space_budget) {
    if (maximum_bytes == 0) return DOWNLOAD_LIMIT_INVALID;
    if (incoming > UINT64_MAX - received) return DOWNLOAD_LIMIT_SIZE;
    uint64_t total = received + incoming;
    if (total > maximum_bytes) return DOWNLOAD_LIMIT_SIZE;
    if (total > space_budget) return DOWNLOAD_LIMIT_SPACE;
    return DOWNLOAD_LIMIT_OK;
}
