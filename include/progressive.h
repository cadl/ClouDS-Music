#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "audio_format.h"

/* Size of the single prefetch window readers may consume without I/O. */
#define PROGRESSIVE_WINDOW_SIZE (128U * 1024U)

typedef struct ProgressiveFile ProgressiveFile;

typedef enum {
    PROGRESSIVE_PROBE_PENDING = 0,
    PROGRESSIVE_PROBE_READY,
    PROGRESSIVE_PROBE_INVALID
} ProgressiveProbeState;

typedef struct {
    uint64_t published;
    uint64_t total;
    bool complete;
    bool failed;
    bool cancelled;
    bool prefetch_ready;
    bool io_pending;
    ProgressiveProbeState probe_state;
    /* First audio byte; the initial prefetch window starts here. */
    uint64_t audio_offset;
    char error[192];
} ProgressiveSnapshot;

ProgressiveFile *progressive_file_create(const char *final_path,
                                         char *error, size_t error_size);
void progressive_file_retain(ProgressiveFile *file);
void progressive_file_release(ProgressiveFile *file);
void progressive_file_publish(ProgressiveFile *file,
                              uint64_t published, uint64_t total);
int progressive_file_commit(ProgressiveFile *file,
                            char *error, size_t error_size);
void progressive_file_fail(ProgressiveFile *file, bool cancelled,
                           const char *error);
size_t progressive_file_read_at(ProgressiveFile *file, uint64_t position,
                                void *buffer, size_t size);
void progressive_file_snapshot(ProgressiveFile *file,
                               ProgressiveSnapshot *snapshot);
/* Records the container detected from the published prefix.  NULL marks the
 * header invalid.  FLAC moves the initial prefetch to the first frame. */
void progressive_file_set_probe(ProgressiveFile *file,
                                const AudioProbe *probe);
ProgressiveProbeState progressive_file_probe(ProgressiveFile *file,
                                             AudioProbe *probe);
/* Returns how many contiguous bytes at `position` are resident in the
 * prefetch window without copying them.  When fewer than `wanted` are held
 * and more are published, schedules a full window load at `position`. */
size_t progressive_file_window(ProgressiveFile *file, uint64_t position,
                               size_t wanted);
