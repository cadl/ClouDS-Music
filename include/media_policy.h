#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MEDIA_PREBUFFER_PERCENT 15U
#define MEDIA_PREBUFFER_MIN_BYTES (256U * 1024U)
#define MEDIA_PREBUFFER_MAX_BYTES (1024U * 1024U)

uint64_t media_prebuffer_target(uint64_t total_bytes);
/* Bytes that must be published before progressive playback starts: any
 * container metadata before `audio_offset` plus the audio prebuffer. */
uint64_t media_stream_start_target(uint64_t total_bytes,
                                   uint64_t audio_offset);
/* Lead required before leaving a network underrun: `seconds` of audio,
 * capped at the largest start prebuffer and never below `minimum`. */
uint64_t media_rebuffer_bytes(uint64_t bytes_per_second, unsigned int seconds,
                              uint64_t minimum);
/* Whether a waiting stream may decode again.  Waiting for the prefetch
 * window to load ends with the load; only a starved stream needs a lead. */
bool media_stream_can_resume(bool io_pending, bool finished, bool starved,
                             uint64_t available, uint64_t threshold);
unsigned int media_download_percent(uint64_t received_bytes,
                                    uint64_t total_bytes);
