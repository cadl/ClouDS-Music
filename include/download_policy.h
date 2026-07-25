#pragma once

#include <stdint.h>

typedef enum {
    DOWNLOAD_LIMIT_OK = 0,
    DOWNLOAD_LIMIT_INVALID,
    DOWNLOAD_LIMIT_SIZE,
    DOWNLOAD_LIMIT_SPACE
} DownloadLimitResult;

uint64_t download_space_budget(uint64_t free_bytes, uint64_t reserve_bytes);
DownloadLimitResult download_limit_check(uint64_t received,
                                         uint64_t incoming,
                                         uint64_t maximum_bytes,
                                         uint64_t space_budget);
