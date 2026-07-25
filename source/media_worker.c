#include "media_worker.h"

#include "i18n.h"

#include "cache.h"
#include "net.h"
#include "storage_paths.h"

#include <3ds.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEDIA_WORKER_STACK_SIZE (96U * 1024U)
#define MEDIA_WORKER_PRIORITY 0x31
#define CACHE_ROOT STORAGE_ROOT

struct MediaWorker {
    Thread thread;
    LightLock lock;
    LightEvent wake;
    bool stop;
    bool cancel;
    bool busy;
    bool has_job;
    bool result_ready;
    MediaJob job;
    int64_t active_song_id;
    bool active_prepare_cached;
    bool active_audio_is_trial;
    MediaResult result;
    ProgressiveFile *stream;
    int64_t stream_song_id;
    uint64_t received;
    uint64_t total;
    char status[192];
};

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static void media_result_release(MediaResult *result) {
    if (!result) return;
    player_prepared_destroy(result->prepared_audio);
    result->prepared_audio = NULL;
}

static int media_cancelled(void *userdata) {
    MediaWorker *worker = (MediaWorker *)userdata;
    LightLock_Lock(&worker->lock);
    bool cancelled = worker->cancel || worker->stop;
    LightLock_Unlock(&worker->lock);
    return cancelled ? 1 : 0;
}

static void media_status(MediaWorker *worker, const char *format, ...) {
    char status[192];
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(status, sizeof(status), format, args);
    va_end(args);
    LightLock_Lock(&worker->lock);
    i18n_snprintf(worker->status, sizeof(worker->status), "%s", status);
    LightLock_Unlock(&worker->lock);
}

static void media_progress(uint64_t received, uint64_t total, void *userdata) {
    MediaWorker *worker = (MediaWorker *)userdata;
    LightLock_Lock(&worker->lock);
    worker->received = received;
    worker->total = total;
    if (total) {
        i18n_snprintf(worker->status, sizeof(worker->status),
                 "正在下载 %.1f / %.1f MB",
                 (double)received / (1024.0 * 1024.0),
                 (double)total / (1024.0 * 1024.0));
    } else {
        i18n_snprintf(worker->status, sizeof(worker->status),
                 "正在下载 %.1f MB",
                 (double)received / (1024.0 * 1024.0));
    }
    LightLock_Unlock(&worker->lock);
}

static void media_publish(uint64_t published, uint64_t total, void *userdata) {
    MediaWorker *worker = (MediaWorker *)userdata;
    ProgressiveFile *stream = NULL;
    LightLock_Lock(&worker->lock);
    stream = worker->stream;
    if (stream) progressive_file_retain(stream);
    LightLock_Unlock(&worker->lock);
    if (stream) {
        progressive_file_publish(stream, published, total);
        progressive_file_release(stream);
    }
}

static void replace_stream(MediaWorker *worker, ProgressiveFile *stream,
                           int64_t song_id) {
    ProgressiveFile *previous = NULL;
    if (stream) progressive_file_retain(stream);
    LightLock_Lock(&worker->lock);
    previous = worker->stream;
    worker->stream = stream;
    worker->stream_song_id = song_id;
    worker->active_song_id = song_id;
    LightLock_Unlock(&worker->lock);
    progressive_file_release(previous);
}

static void run_media_job(MediaWorker *worker, const MediaJob *job,
                          MediaResult *result) {
    media_result_release(result);
    memset(result, 0, sizeof(*result));
    result->song_id = job->song_id;
    result->prepare_cached = job->prepare_cached;
    result->audio_is_trial = job->audio_is_trial;
    char error[192] = {0};
    if (job->prepare_cached) {
        replace_stream(worker, NULL, job->song_id);
        media_status(worker, "正在后台准备跳转索引");
        if (player_prepare_audio_controlled(
                job->path, &result->prepared_audio,
                media_cancelled, worker, error, sizeof(error)) != 0) {
            result->cancelled = media_cancelled(worker) != 0;
            if (!result->cancelled)
                i18n_snprintf(result->error, sizeof(result->error),
                              "%s", error);
            return;
        }
        if (media_cancelled(worker)) {
            result->cancelled = true;
            media_result_release(result);
            return;
        }

        if (job->cache_changed) {
            media_status(worker, "正在后台整理媒体缓存");
            CacheStats stats;
            char cache_error[192];
            uint64_t limit = job->cache_limit ? job->cache_limit :
                             NM3DS_CACHE_LIMIT_DEFAULT;
            int cache_result = cache_prune_controlled(
                CACHE_ROOT, limit, job->song_id, &stats,
                &result->cache_evicted,
                media_cancelled, worker, cache_error, sizeof(cache_error));
            if (cache_result >= 0) {
                result->cache_stats_valid = true;
                result->cache_over_limit = cache_result > 0;
                result->cache_bytes = stats.bytes;
                result->cache_audio_files = stats.audio_files;
                result->cache_cover_files = stats.cover_files;
                result->cache_lyric_files = stats.lyric_files;
            }
            if (media_cancelled(worker)) {
                result->cancelled = true;
                media_result_release(result);
                return;
            }
        }
        result->success = true;
        return;
    }

    ProgressiveFile *stream = progressive_file_create(
        job->path, error, sizeof(error));
    if (!stream) {
        i18n_snprintf(result->error, sizeof(result->error), "%s", error);
        return;
    }
    replace_stream(worker, stream, job->song_id);

    media_status(worker, "正在预缓冲音频");
    uint64_t audio_limit = job->cache_limit ? job->cache_limit :
                           NM3DS_CACHE_LIMIT_DEFAULT;
    int download = net_download_file_part_controlled_ex(
        job->url, job->path, audio_limit, media_progress, worker,
        media_publish, worker, media_cancelled, worker,
        &result->failure,
        error, sizeof(error));
    if (download != 0 || media_cancelled(worker)) {
        result->cancelled = media_cancelled(worker) != 0;
        progressive_file_fail(stream, result->cancelled, error);
        if (!result->cancelled)
            i18n_snprintf(result->error, sizeof(result->error), "%s",
                     error[0] ? error : i18n_text("媒体下载失败"));
        progressive_file_release(stream);
        return;
    }
    if (progressive_file_commit(stream, error, sizeof(error)) != 0) {
        i18n_snprintf(result->error, sizeof(result->error), "%s", error);
        progressive_file_release(stream);
        return;
    }

    media_status(worker, "正在准备完整 MP3 索引");
    if (player_prepare_audio_controlled(
            job->path, &result->prepared_audio,
            media_cancelled, worker, error, sizeof(error)) != 0) {
        result->cancelled = media_cancelled(worker) != 0;
        if (!result->cancelled)
            i18n_snprintf(result->error, sizeof(result->error), "%s", error);
        progressive_file_release(stream);
        return;
    }
    cache_song_remove_other_audio_types(
        CACHE_ROOT, job->song_id,
        job->audio_is_trial ? CACHE_AUDIO_TYPE_TRIAL : CACHE_AUDIO_TYPE_FULL);
    if (media_cancelled(worker)) {
        result->cancelled = true;
        media_result_release(result);
        progressive_file_release(stream);
        return;
    }

    media_status(worker, "正在整理媒体缓存");
    CacheStats stats;
    char cache_error[192];
    uint64_t limit = job->cache_limit ? job->cache_limit :
                     NM3DS_CACHE_LIMIT_DEFAULT;
    int cache_result = cache_prune_controlled(
        CACHE_ROOT, limit, job->song_id, &stats,
        &result->cache_evicted,
        media_cancelled, worker, cache_error, sizeof(cache_error));
    if (cache_result >= 0) {
        result->cache_stats_valid = true;
        result->cache_over_limit = cache_result > 0;
        result->cache_bytes = stats.bytes;
        result->cache_audio_files = stats.audio_files;
        result->cache_cover_files = stats.cover_files;
        result->cache_lyric_files = stats.lyric_files;
    }
    if (media_cancelled(worker)) {
        result->cancelled = true;
        media_result_release(result);
        progressive_file_release(stream);
        return;
    }
    result->success = true;
    progressive_file_release(stream);
}

static void media_thread_main(void *userdata) {
    MediaWorker *worker = (MediaWorker *)userdata;
    for (;;) {
        LightEvent_Wait(&worker->wake);
        LightLock_Lock(&worker->lock);
        if (worker->stop) {
            LightLock_Unlock(&worker->lock);
            break;
        }
        if (!worker->has_job) {
            LightLock_Unlock(&worker->lock);
            continue;
        }
        MediaJob job = worker->job;
        worker->has_job = false;
        worker->cancel = false;
        worker->busy = true;
        worker->active_song_id = job.song_id;
        worker->active_prepare_cached = job.prepare_cached;
        worker->active_audio_is_trial = job.audio_is_trial;
        worker->received = 0;
        worker->total = 0;
        worker->status[0] = '\0';
        LightLock_Unlock(&worker->lock);

        run_media_job(worker, &job, &worker->result);

        LightLock_Lock(&worker->lock);
        worker->busy = false;
        worker->received = 0;
        worker->total = 0;
        if (!worker->has_job) {
            worker->result_ready = true;
        } else {
            media_result_release(&worker->result);
            worker->result_ready = false;
        }
        bool another = worker->has_job;
        LightLock_Unlock(&worker->lock);
        if (another) LightEvent_Signal(&worker->wake);
    }
    threadExit(0);
}

MediaWorker *media_worker_create(char *error, size_t error_size) {
    MediaWorker *worker = (MediaWorker *)calloc(1, sizeof(*worker));
    if (!worker) {
        set_error(error, error_size, "内存不足，无法创建媒体 Worker");
        return NULL;
    }
    LightLock_Init(&worker->lock);
    LightEvent_Init(&worker->wake, RESET_ONESHOT);
    worker->thread = threadCreate(media_thread_main, worker,
                                  MEDIA_WORKER_STACK_SIZE,
                                  MEDIA_WORKER_PRIORITY, -2, false);
    if (!worker->thread) {
        free(worker);
        set_error(error, error_size, "无法创建媒体 Worker 线程");
        return NULL;
    }
    return worker;
}

void media_worker_destroy(MediaWorker *worker) {
    if (!worker) return;
    LightLock_Lock(&worker->lock);
    worker->stop = true;
    worker->cancel = true;
    LightLock_Unlock(&worker->lock);
    LightEvent_Signal(&worker->wake);
    threadJoin(worker->thread, U64_MAX);
    threadFree(worker->thread);
    media_result_release(&worker->result);
    progressive_file_release(worker->stream);
    free(worker);
}

bool media_worker_submit(MediaWorker *worker, const MediaJob *job) {
    if (!worker || !job || job->song_id <= 0 || !job->path[0] ||
        (!job->prepare_cached && !job->url[0]))
        return false;
    LightLock_Lock(&worker->lock);
    if (worker->stop) {
        LightLock_Unlock(&worker->lock);
        return false;
    }
    if (worker->result_ready) media_result_release(&worker->result);
    if (worker->busy) worker->cancel = true;
    worker->job = *job;
    worker->has_job = true;
    worker->result_ready = false;
    LightLock_Unlock(&worker->lock);
    LightEvent_Signal(&worker->wake);
    return true;
}

void media_worker_cancel(MediaWorker *worker) {
    if (!worker) return;
    LightLock_Lock(&worker->lock);
    worker->cancel = true;
    worker->has_job = false;
    LightLock_Unlock(&worker->lock);
}

void media_worker_snapshot(MediaWorker *worker, MediaSnapshot *snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (!worker) return;
    ProgressiveFile *stream = NULL;
    LightLock_Lock(&worker->lock);
    snapshot->busy = worker->busy || worker->has_job;
    snapshot->song_id = worker->busy ? worker->active_song_id :
                        worker->job.song_id;
    snapshot->prepare_cached = worker->busy ?
        worker->active_prepare_cached : worker->job.prepare_cached;
    snapshot->received = worker->received;
    snapshot->total = worker->total;
    snapshot->audio_is_trial = worker->busy ? worker->active_audio_is_trial :
                               worker->job.audio_is_trial;
    i18n_snprintf(snapshot->status, sizeof(snapshot->status), "%s", worker->status);
    if (worker->stream && worker->stream_song_id == snapshot->song_id) {
        stream = worker->stream;
        progressive_file_retain(stream);
    }
    LightLock_Unlock(&worker->lock);
    if (stream) {
        ProgressiveSnapshot progress;
        progressive_file_snapshot(stream, &progress);
        snapshot->published = progress.published;
        snapshot->prefetch_ready = progress.prefetch_ready;
        progressive_file_release(stream);
    }
}

ProgressiveFile *media_worker_stream(MediaWorker *worker, int64_t song_id) {
    if (!worker || song_id <= 0) return NULL;
    ProgressiveFile *stream = NULL;
    LightLock_Lock(&worker->lock);
    if (worker->stream && worker->stream_song_id == song_id) {
        stream = worker->stream;
        progressive_file_retain(stream);
    }
    LightLock_Unlock(&worker->lock);
    return stream;
}

bool media_worker_take_result(MediaWorker *worker, MediaResult *result) {
    if (!worker || !result) return false;
    LightLock_Lock(&worker->lock);
    if (!worker->result_ready) {
        LightLock_Unlock(&worker->lock);
        return false;
    }
    *result = worker->result;
    worker->result.prepared_audio = NULL;
    worker->result_ready = false;
    LightLock_Unlock(&worker->lock);
    return true;
}
