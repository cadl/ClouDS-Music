#pragma once

#include <stdint.h>

#define MEDIA_PREBUFFER_PERCENT 15U
#define MEDIA_PREBUFFER_MIN_BYTES (256U * 1024U)
#define MEDIA_PREBUFFER_MAX_BYTES (1024U * 1024U)

uint64_t media_prebuffer_target(uint64_t total_bytes);
unsigned int media_download_percent(uint64_t received_bytes,
                                    uint64_t total_bytes);
