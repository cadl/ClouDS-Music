#pragma once

#include "net.h"
#include "player.h"
#include "progressive.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int64_t song_id;
    char url[1536];
    char path[256];
    uint64_t cache_limit;
    bool prepare_cached;
    bool cache_changed;
    bool audio_is_trial;
} MediaJob;

typedef struct {
    bool busy;
    int64_t song_id;
    uint64_t received;
    uint64_t total;
    uint64_t published;
    bool prefetch_ready;
    bool prepare_cached;
    bool audio_is_trial;
    char status[192];
} MediaSnapshot;

typedef struct {
    bool success;
    bool cancelled;
    NetErrorKind failure;
    char error[192];
    int64_t song_id;
    bool prepare_cached;
    bool audio_is_trial;
    PreparedAudio *prepared_audio;
    bool cache_stats_valid;
    bool cache_over_limit;
    bool cache_evicted;
    uint64_t cache_bytes;
    size_t cache_audio_files;
    size_t cache_cover_files;
    size_t cache_lyric_files;
} MediaResult;

typedef struct MediaWorker MediaWorker;

MediaWorker *media_worker_create(char *error, size_t error_size);
void media_worker_destroy(MediaWorker *worker);
bool media_worker_submit(MediaWorker *worker, const MediaJob *job);
void media_worker_cancel(MediaWorker *worker);
void media_worker_snapshot(MediaWorker *worker, MediaSnapshot *snapshot);
ProgressiveFile *media_worker_stream(MediaWorker *worker, int64_t song_id);
bool media_worker_take_result(MediaWorker *worker, MediaResult *result);
