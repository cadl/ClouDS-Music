#include "worker.h"

#include "i18n.h"
#include "cache.h"
#include "lyric_cache.h"
#include "storage_paths.h"

#include "net.h"

#include <3ds.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define WORKER_STACK_SIZE (96U * 1024U)
#define WORKER_PRIORITY 0x31
#define CACHE_ROOT STORAGE_ROOT
#define NETWORK_PROBE_URL "https://music.163.com/"

struct NetworkWorker {
    NeteaseClient *client;
    Thread thread;
    LightLock lock;
    LightEvent wake;
    bool stop;
    bool cancel;
    bool busy;
    bool has_job;
    bool result_ready;
    WorkerJob job;
    WorkerJobKind active_kind;
    bool active_background;
    WorkerResult result;
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

static void worker_status(NetworkWorker *worker, const char *format, ...) {
    char status[192];
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(status, sizeof(status), format, args);
    va_end(args);
    LightLock_Lock(&worker->lock);
    i18n_snprintf(worker->status, sizeof(worker->status), "%s", status);
    LightLock_Unlock(&worker->lock);
}

static int worker_cancelled(void *userdata) {
    NetworkWorker *worker = (NetworkWorker *)userdata;
    LightLock_Lock(&worker->lock);
    bool cancelled = worker->cancel || worker->stop;
    LightLock_Unlock(&worker->lock);
    return cancelled ? 1 : 0;
}

static int prepare_song_cache_directory(int64_t song_id,
                                        char *error, size_t error_size) {
    (void)mkdir("sdmc:/3ds", 0777);
    return cache_ensure_song_directory(CACHE_ROOT, song_id,
                                       error, error_size);
}

static bool existing_file(const char *path) {
    struct stat info;
    return path && stat(path, &info) == 0 && info.st_size > 0;
}

static void worker_download_progress(uint64_t received, uint64_t total,
                                     void *userdata) {
    NetworkWorker *worker = (NetworkWorker *)userdata;
    LightLock_Lock(&worker->lock);
    worker->received = received;
    worker->total = total;
    if (total) {
        i18n_snprintf(worker->status, sizeof(worker->status),
                 "后台缓存下一首 %.1f / %.1f MB",
                 (double)received / (1024.0 * 1024.0),
                 (double)total / (1024.0 * 1024.0));
    } else {
        i18n_snprintf(worker->status, sizeof(worker->status),
                 "后台缓存下一首 %.1f MB",
                 (double)received / (1024.0 * 1024.0));
    }
    LightLock_Unlock(&worker->lock);
}

static void finish_failure(NetworkWorker *worker, WorkerResult *result,
                           const char *error) {
    result->cancelled = worker_cancelled(worker) != 0;
    result->failure = result->cancelled ? NETEASE_FAILURE_CANCELLED :
                      netease_last_failure(worker->client);
    if (result->cancelled) {
        player_prepared_destroy(result->prepared_audio);
        result->prepared_audio = NULL;
    }
    if (!result->cancelled)
        i18n_snprintf(result->error, sizeof(result->error), "%s",
                 error && error[0] ? error : i18n_text("网络任务失败"));
}

static NeteaseFailure diagnostic_net_failure(NetErrorKind failure) {
    if (failure == NET_ERROR_CANCELLED) return NETEASE_FAILURE_CANCELLED;
    if (failure == NET_ERROR_TLS_VERIFY)
        return NETEASE_FAILURE_TLS_VERIFY;
    if (failure == NET_ERROR_TRANSPORT) return NETEASE_FAILURE_TRANSPORT;
    if (failure == NET_ERROR_AUTH) return NETEASE_FAILURE_AUTH_INVALID;
    return failure == NET_ERROR_NONE ? NETEASE_FAILURE_NONE :
                                       NETEASE_FAILURE_OTHER;
}

static void record_diagnostic(WorkerResult *result,
                              WorkerDiagnosticKind kind,
                              NeteaseFailure failure,
                              const char *error) {
    if (!result || result->diagnostic_kind != WORKER_DIAGNOSTIC_NONE) return;
    result->diagnostic_kind = kind;
    result->diagnostic_failure = failure;
    i18n_snprintf(result->diagnostic_error,
                  sizeof(result->diagnostic_error), "%s",
                  error && error[0] ? error : i18n_text("网络任务失败"));
}

static void worker_result_release(WorkerResult *result) {
    if (!result) return;
    player_prepared_destroy(result->prepared_audio);
    result->prepared_audio = NULL;
}

static void run_discover(NetworkWorker *worker, const WorkerJob *job,
                         WorkerResult *result) {
    char error[192] = {0};
    bool enqueue = job->kind == WORKER_JOB_RECOMMENDATION_ENQUEUE;
    const char *source = i18n_text(
        job->recommendation_source == RECOMMEND_SOURCE_DAILY ?
            "每日推荐" : "公开新歌");
    if (enqueue)
        worker_status(worker, "正在全部加入 · 第 %u 页",
                      (unsigned int)(job->offset /
                                     NM3DS_RECOMMEND_RESULTS + 1));
    else
        worker_status(worker, "正在加载%s · 第 %u 页", source,
                      (unsigned int)(job->offset /
                                     NM3DS_RECOMMEND_RESULTS + 1));
    result->offset = job->offset;
    result->recommendation_source = job->recommendation_source;
    int request = job->recommendation_source == RECOMMEND_SOURCE_DAILY ?
        netease_recommend(worker->client, job->offset, result->songs,
                          NM3DS_RECOMMEND_RESULTS, &result->song_count,
                          &result->has_more,
                          &result->recommendation_total_count,
                          &result->recommendation_total_known,
                          error, sizeof(error)) :
        netease_discover(worker->client, job->offset, result->songs,
                         NM3DS_RECOMMEND_RESULTS, &result->song_count,
                         &result->has_more,
                         &result->recommendation_total_count,
                         &result->recommendation_total_known,
                         error, sizeof(error));
    if (request != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static void run_search(NetworkWorker *worker, const WorkerJob *job,
                       WorkerResult *result) {
    char error[192] = {0};
    worker_status(worker, "搜索中 · 第 %u 页",
                  (unsigned int)(job->offset / NM3DS_MAX_RESULTS + 1));
    result->offset = job->offset;
    if (netease_search(worker->client, job->query, job->offset,
                       result->songs, NM3DS_MAX_RESULTS,
                       &result->song_count, &result->has_more,
                       error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static void run_user_playlists(NetworkWorker *worker, const WorkerJob *job,
                               WorkerResult *result) {
    char error[192] = {0};
    worker_status(worker, "歌单加载中 · 第 %u 页",
                  (unsigned int)(job->offset / NM3DS_LIBRARY_PAGE + 1));
    result->offset = job->offset;
    if (netease_user_playlists(worker->client, job->offset,
                               result->playlists, NM3DS_LIBRARY_PAGE,
                               &result->playlist_count, &result->has_more,
                               error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static void run_user_cloud(NetworkWorker *worker, const WorkerJob *job,
                           WorkerResult *result) {
    char error[192] = {0};
    worker_status(worker, "云盘加载中 · 第 %u 页",
                  (unsigned int)(job->offset / NM3DS_CLOUD_PAGE + 1));
    result->offset = job->offset;
    if (netease_user_cloud(worker->client, job->offset,
                           result->cloud_tracks, NM3DS_CLOUD_PAGE,
                           &result->cloud_track_count, &result->has_more,
                           error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static void run_playlist_tracks(NetworkWorker *worker, const WorkerJob *job,
                                WorkerResult *result) {
    char error[192] = {0};
    bool enqueue = job->kind == WORKER_JOB_PLAYLIST_ENQUEUE;
    size_t page_size = enqueue ? NM3DS_LIBRARY_BATCH_PAGE :
                                 NM3DS_LIBRARY_PAGE;
    worker_status(worker, enqueue ?
                  "正在全部加入 · 第 %u 页" :
                  "歌曲加载中 · 第 %u 页",
                  (unsigned int)(job->offset / page_size + 1));
    result->playlist_id = job->playlist_id;
    result->offset = job->offset;
    bool refresh_index = !enqueue && job->offset == 0;
    if (netease_playlist_tracks(worker->client, job->playlist_id, job->offset,
                                refresh_index, result->songs, page_size,
                                &result->song_count, &result->has_more,
                                &result->playlist_track_total,
                                error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static void run_album_tracks(NetworkWorker *worker, const WorkerJob *job,
                             WorkerResult *result) {
    char error[192] = {0};
    bool enqueue = job->kind == WORKER_JOB_ALBUM_ENQUEUE;
    int64_t album_id = job->album_id;
    Song source_song = job->song;
    result->song_id = job->song.id;
    if (album_id <= 0) {
        if (source_song.id <= 0) {
            finish_failure(worker, result, "没有可查看的当前歌曲");
            return;
        }
        worker_status(worker, "正在查找歌曲专辑");
        if (netease_song_album_detail(worker->client, source_song.id,
                                      &source_song, &album_id,
                                      error, sizeof(error)) != 0) {
            finish_failure(worker, result, error);
            return;
        }
    }
    result->album_id = album_id;
    i18n_snprintf(result->album_name, sizeof(result->album_name), "%s",
                  source_song.album);
    bool refresh_index = !enqueue && job->offset == 0 &&
                         (job->album_id <= 0 || job->refresh_index);
    if (refresh_index)
        worker_status(worker, "正在查询完整专辑歌曲列表");
    else worker_status(worker, enqueue ?
                       "正在全部加入专辑 · 第 %u 页" :
                       "正在读取专辑歌曲 · 第 %u 页",
                       (unsigned int)(job->offset / NM3DS_ALBUM_PAGE + 1));
    result->offset = job->offset;
    if (netease_album_tracks(worker->client, album_id, job->offset,
                             refresh_index,
                             result->songs, NM3DS_ALBUM_PAGE,
                             &result->song_count, &result->has_more,
                             &result->album_track_total,
                             error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static void run_song_artists(NetworkWorker *worker, const WorkerJob *job,
                             WorkerResult *result) {
    char error[192] = {0};
    result->song_id = job->song.id;
    if (job->song.id <= 0) {
        finish_failure(worker, result, "没有可查看的当前歌曲");
        return;
    }
    worker_status(worker, "正在查找歌曲艺人");
    if (netease_song_artists(worker->client, job->song.id,
                             result->artists, NM3DS_SONG_ARTISTS_MAX,
                             &result->artist_count,
                             error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static void run_artist_albums(NetworkWorker *worker, const WorkerJob *job,
                              WorkerResult *result) {
    char error[192] = {0};
    result->artist_id = job->artist_id;
    result->offset = job->offset;
    worker_status(worker, "正在加载艺人专辑 · 第 %u 页",
                  (unsigned int)(job->offset / NM3DS_ARTIST_PAGE + 1));
    if (netease_artist_albums(worker->client, job->artist_id, job->offset,
                              result->albums, NM3DS_ARTIST_PAGE,
                              &result->album_count, &result->has_more,
                              error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static void run_artist_songs(NetworkWorker *worker, const WorkerJob *job,
                             WorkerResult *result) {
    char error[192] = {0};
    bool enqueue = job->kind == WORKER_JOB_ARTIST_SONG_ENQUEUE;
    result->artist_id = job->artist_id;
    result->offset = job->offset;
    worker_status(worker, enqueue ?
                  "正在全部加入艺人歌曲 · 第 %u 页" :
                  "正在加载艺人歌曲 · 第 %u 页",
                  (unsigned int)(job->offset / NM3DS_ARTIST_PAGE + 1));
    if (netease_artist_songs(worker->client, job->artist_id, job->offset,
                             result->songs, NM3DS_ARTIST_PAGE,
                             &result->song_count, &result->has_more,
                             error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static int load_song_extras(NetworkWorker *worker, const WorkerJob *job,
                            WorkerResult *result,
                            char *error, size_t error_size) {
    Song song = job->song;
    if (!job->offline_playback && !worker_cancelled(worker)) {
        Song detailed;
        char detail_error[192] = {0};
        worker_status(worker, "正在刷新歌曲信息");
        if (netease_song_detail(worker->client, song.id, &detailed,
                                detail_error, sizeof(detail_error)) == 0) {
            result->catalog_fee = detailed.fee;
            if (!song.pic_url[0] && detailed.pic_url[0]) {
                i18n_snprintf(song.pic_url, sizeof(song.pic_url), "%s",
                              detailed.pic_url);
                i18n_snprintf(result->song_pic_url,
                              sizeof(result->song_pic_url), "%s",
                              detailed.pic_url);
            }
        } else record_diagnostic(
            result, WORKER_DIAGNOSTIC_SONG_DETAIL,
            netease_last_failure(worker->client), detail_error);
    }
    if (cache_song_path(CACHE_ROOT, song.id, CACHE_ASSET_COVER,
                        result->cover_path,
                        sizeof(result->cover_path)) != 0) {
        set_error(error, error_size, "封面缓存路径过长");
        return -1;
    }
    bool cover_cached = existing_file(result->cover_path);
    if (worker_cancelled(worker)) return 0;
    if (!cover_cached) {
        if (job->offline_playback) result->cover_path[0] = '\0';
        else if (song.pic_url[0]) {
            char cover_url[512];
            i18n_snprintf(cover_url, sizeof(cover_url), "%s%sparam=128y128",
                     song.pic_url,
                     strchr(song.pic_url, '?') ? "&" : "?");
            worker_status(worker, "正在加载专辑封面");
            NetErrorKind cover_failure = NET_ERROR_NONE;
            if (net_download_file_controlled_ex(
                    cover_url, result->cover_path,
                    NET_DOWNLOAD_COVER_MAX_BYTES, NULL, NULL,
                    worker_cancelled, worker, &cover_failure,
                    error, error_size) != 0) {
                record_diagnostic(
                    result, WORKER_DIAGNOSTIC_COVER_DOWNLOAD,
                    diagnostic_net_failure(cover_failure), error);
                result->cover_path[0] = '\0';
            }
        } else result->cover_path[0] = '\0';
    }
    if (result->cover_path[0] && !worker_cancelled(worker)) {
        worker_status(worker, "正在解码专辑封面");
        if (cover_decode_image(result->cover_path,
                               result->cover_pixels,
                               COVER_ART_PIXELS,
                               error, error_size) == 0) {
            result->cover_ready = true;
        } else {
            record_diagnostic(
                result, WORKER_DIAGNOSTIC_COVER_DECODE,
                NETEASE_FAILURE_NONE, error);
            (void)remove(result->cover_path);
            result->cover_path[0] = '\0';
        }
    }
    if (worker_cancelled(worker)) return 0;
    char lyric_path[320];
    if (cache_song_path(CACHE_ROOT, job->song.id, CACHE_ASSET_LYRIC,
                        lyric_path, sizeof(lyric_path)) != 0) {
        set_error(error, error_size, "歌词缓存路径过长");
        return -1;
    }
    worker_status(worker, "正在加载缓存歌词");
    int cached = lyric_cache_load(lyric_path, result->lyrics,
                                  NM3DS_MAX_LYRICS, &result->lyric_count,
                                  error, error_size);
    if (cached == 0) return 0;
    if (cached < 0) (void)remove(lyric_path);
    if (job->offline_playback) {
        result->lyric_count = 0;
        return 0;
    }
    if (worker_cancelled(worker)) return 0;
    worker_status(worker, "正在加载同步歌词");
    if (netease_lyrics(worker->client, job->song.id, result->lyrics,
                       NM3DS_MAX_LYRICS, &result->lyric_count,
                       error, error_size) == 0) {
        char cache_error[192];
        (void)lyric_cache_save(lyric_path, result->lyrics,
                               result->lyric_count,
                               cache_error, sizeof(cache_error));
    } else record_diagnostic(
        result, WORKER_DIAGNOSTIC_LYRICS_DOWNLOAD,
        netease_last_failure(worker->client), error);
    return 0;
}

static int prepare_cached_audio(NetworkWorker *worker, const WorkerJob *job,
                                WorkerResult *result,
                                CacheAudioType audio_type,
                                char *error, size_t error_size) {
    if (cache_song_audio_path(CACHE_ROOT, job->song.id, audio_type,
                              result->audio_path,
                              sizeof(result->audio_path)) != 0) {
        set_error(error, error_size, "音频缓存路径过长");
        return -1;
    }
    result->audio_was_cached = !job->force_download &&
                               existing_file(result->audio_path);
    if (!result->audio_was_cached) {
        set_error(error, error_size, "歌曲没有可用的离线音频缓存");
        return -1;
    }

    worker_status(worker, job->offline_playback ?
                  "正在打开离线歌曲" : "正在打开缓存歌曲");
    if (player_prepare_audio_fast(result->audio_path,
                                  &result->prepared_audio,
                                  error, error_size) != 0) {
        if (job->offline_playback) (void)remove(result->audio_path);
        return -1;
    }
    result->audio_seek_pending = true;
    if (!job->offline_playback)
        result->cache_changed = cache_song_remove_other_audio_types(
            CACHE_ROOT, job->song.id, audio_type);
    if (worker_cancelled(worker)) {
        worker_result_release(result);
        set_error(error, error_size, "请求已取消");
        return -1;
    }

    result->success = true;
    return 0;
}

static void run_prepare_song(NetworkWorker *worker, const WorkerJob *job,
                             WorkerResult *result) {
    char error[192] = {0};
    result->song_id = job->song.id;
    result->offline_playback = job->offline_playback;
    if (!song_cloud_access_allowed(
            &job->song, netease_logged_in(worker->client),
            worker->client->user_id)) {
        finish_failure(worker, result, "请登录当前音乐云盘账户");
        return;
    }
    if (!job->force_download) {
        CacheAudioType audio_type = job->offline_playback ?
            cache_song_offline_audio_type(
                CACHE_ROOT, job->song.id, job->allow_full_cache) :
            cache_song_online_audio_type(
                CACHE_ROOT, job->song.id, job->allow_full_cache);
        if (audio_type != CACHE_AUDIO_TYPE_UNKNOWN) {
            result->audio_is_trial = audio_type == CACHE_AUDIO_TYPE_TRIAL;
            if (prepare_cached_audio(worker, job, result, audio_type,
                                     error, sizeof(error)) != 0)
                finish_failure(worker, result, error);
            return;
        }
        if (job->offline_playback) {
            finish_failure(worker, result,
                           "歌曲没有可用的离线音频缓存");
            return;
        }
    } else if (job->offline_playback) {
        finish_failure(worker, result, "离线时无法重新下载歌曲");
        return;
    }

    NeteasePlaybackInfo playback;
    worker_status(worker, "正在解析音频流");
    if (netease_song_url(worker->client, job->song.id,
                         result->audio_url, sizeof(result->audio_url),
                         &playback,
                         error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->playback_resolved = true;
    result->audio_is_trial = playback.is_trial;
    CacheAudioType audio_type = playback.is_trial ? CACHE_AUDIO_TYPE_TRIAL :
                                                    CACHE_AUDIO_TYPE_FULL;
    if (prepare_song_cache_directory(job->song.id,
                                     error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    if (!job->force_download &&
        cache_song_has_audio_type(CACHE_ROOT, job->song.id, audio_type)) {
        if (prepare_cached_audio(worker, job, result, audio_type,
                                 error, sizeof(error)) != 0)
            finish_failure(worker, result, error);
        return;
    }
    if (cache_song_audio_path(CACHE_ROOT, job->song.id, audio_type,
                              result->audio_path,
                              sizeof(result->audio_path)) != 0) {
        finish_failure(worker, result, "音频缓存路径过长");
        return;
    }
    result->audio_needs_download = true;
    result->success = true;
}

static void run_song_extras(NetworkWorker *worker, const WorkerJob *job,
                            WorkerResult *result) {
    result->song_id = job->song.id;
    char error[192] = {0};
    if (prepare_song_cache_directory(job->song.id,
                                     error, sizeof(error)) != 0 ||
        load_song_extras(worker, job, result,
                         error, sizeof(error)) != 0) {
        finish_failure(worker, result,
                       error[0] ? error : "无法访问歌曲缓存");
        return;
    }
    if (worker_cancelled(worker)) {
        finish_failure(worker, result, "请求已取消");
        return;
    }
    result->song_extras_cached =
        cache_song_has_asset(CACHE_ROOT, job->song.id, CACHE_ASSET_COVER) &&
        cache_song_has_asset(CACHE_ROOT, job->song.id, CACHE_ASSET_LYRIC);
    result->success = true;
}

static int prefetch_song_extras(NetworkWorker *worker, const WorkerJob *job,
                                WorkerResult *result,
                                char *error, size_t error_size) {
    Song song = job->song;
    if (!cache_song_has_asset(CACHE_ROOT, song.id, CACHE_ASSET_COVER)) {
        if (!song.pic_url[0]) {
            Song detailed;
            worker_status(worker, "后台查找下一首的专辑封面");
            if (netease_song_detail(worker->client, song.id, &detailed,
                                    error, error_size) != 0 ||
                !detailed.pic_url[0]) {
                if (!error[0])
                    set_error(error, error_size, "下一首没有可缓存的封面");
                return -1;
            }
            i18n_snprintf(song.pic_url, sizeof(song.pic_url), "%s",
                     detailed.pic_url);
            i18n_snprintf(result->song_pic_url, sizeof(result->song_pic_url),
                     "%s", detailed.pic_url);
        }
        char cover_path[320];
        if (cache_song_path(CACHE_ROOT, song.id, CACHE_ASSET_COVER,
                            cover_path, sizeof(cover_path)) != 0) {
            set_error(error, error_size, "封面缓存路径过长");
            return -1;
        }
        char cover_url[512];
        int written = i18n_snprintf(cover_url, sizeof(cover_url),
                               "%s%sparam=128y128", song.pic_url,
                               strchr(song.pic_url, '?') ? "&" : "?");
        if (written < 0 || (size_t)written >= sizeof(cover_url)) {
            set_error(error, error_size, "封面地址过长");
            return -1;
        }
        worker_status(worker, "后台缓存下一首的专辑封面");
        if (net_download_file_controlled(
                cover_url, cover_path, NET_DOWNLOAD_COVER_MAX_BYTES,
                NULL, NULL,
                worker_cancelled, worker, error, error_size) != 0)
            return -1;
    }
    if (worker_cancelled(worker)) return -1;

    char lyric_path[320];
    if (cache_song_path(CACHE_ROOT, song.id, CACHE_ASSET_LYRIC,
                        lyric_path, sizeof(lyric_path)) != 0) {
        set_error(error, error_size, "歌词缓存路径过长");
        return -1;
    }
    int cached = lyric_cache_load(lyric_path, result->lyrics,
                                  NM3DS_MAX_LYRICS, &result->lyric_count,
                                  error, error_size);
    if (cached < 0) (void)remove(lyric_path);
    if (cached != 0) {
        if (worker_cancelled(worker)) return -1;
        worker_status(worker, "后台缓存下一首的同步歌词");
        if (netease_lyrics(worker->client, song.id, result->lyrics,
                           NM3DS_MAX_LYRICS, &result->lyric_count,
                           error, error_size) != 0)
            return -1;
        if (lyric_cache_save(lyric_path, result->lyrics,
                             result->lyric_count,
                             error, error_size) != 0)
            return -1;
    }
    result->song_extras_cached =
        cache_song_has_asset(CACHE_ROOT, song.id, CACHE_ASSET_COVER) &&
        cache_song_has_asset(CACHE_ROOT, song.id, CACHE_ASSET_LYRIC);
    if (!result->song_extras_cached) {
        set_error(error, error_size, "下一首的封面或歌词未能写入缓存");
        return -1;
    }
    return 0;
}

static void run_prefetch_song(NetworkWorker *worker, const WorkerJob *job,
                              WorkerResult *result) {
    char error[192] = {0};
    result->song_id = job->song.id;
    result->prefetch_anchor_song_id = job->protected_song;
    if (!song_cloud_access_allowed(
            &job->song, netease_logged_in(worker->client),
            worker->client->user_id)) {
        finish_failure(worker, result, "请登录当前音乐云盘账户");
        return;
    }
    CacheAudioType cached_audio = cache_song_online_audio_type(
        CACHE_ROOT, job->song.id, job->allow_full_cache);
    result->prefetch_was_cached =
        cached_audio != CACHE_AUDIO_TYPE_UNKNOWN &&
        cache_song_has_asset(CACHE_ROOT, job->song.id,
                             CACHE_ASSET_COVER) &&
        cache_song_has_asset(CACHE_ROOT, job->song.id,
                             CACHE_ASSET_LYRIC);
    if (result->prefetch_was_cached) {
        result->audio_is_trial = cached_audio == CACHE_AUDIO_TYPE_TRIAL;
        result->prefetch_complete = true;
        result->success = true;
        return;
    }
    if (prepare_song_cache_directory(job->song.id,
                                     error, sizeof(error)) != 0 ||
        prefetch_song_extras(worker, job, result,
                             error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    if (worker_cancelled(worker)) {
        finish_failure(worker, result, "请求已取消");
        return;
    }

    NeteasePlaybackInfo playback;
    char audio_url[1536];
    worker_status(worker, "后台解析下一首的音频流");
    if (netease_song_url(worker->client, job->song.id,
                         audio_url, sizeof(audio_url), &playback,
                         error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->audio_is_trial = playback.is_trial;
    CacheAudioType audio_type = playback.is_trial ? CACHE_AUDIO_TYPE_TRIAL :
                                                    CACHE_AUDIO_TYPE_FULL;
    char audio_path[320];
    if (cache_song_audio_path(CACHE_ROOT, job->song.id, audio_type,
                              audio_path, sizeof(audio_path)) != 0) {
        finish_failure(worker, result, "音频缓存路径过长");
        return;
    }
    if (!existing_file(audio_path)) {
        worker_status(worker, "后台缓存播放列表中的下一首");
        uint64_t audio_limit = job->cache_limit ? job->cache_limit :
                               NM3DS_CACHE_LIMIT_DEFAULT;
        if (net_download_file_controlled(
                audio_url, audio_path, audio_limit,
                worker_download_progress, worker,
                worker_cancelled, worker, error, sizeof(error)) != 0) {
            finish_failure(worker, result, error);
            return;
        }
    }
    cache_song_remove_other_audio_types(CACHE_ROOT, job->song.id, audio_type);
    if (worker_cancelled(worker)) {
        finish_failure(worker, result, "请求已取消");
        return;
    }

    worker_status(worker, "后台整理媒体缓存");
    CacheStats stats;
    char cache_error[192];
    uint64_t limit = job->cache_limit ? job->cache_limit :
                     NM3DS_CACHE_LIMIT_DEFAULT;
    int cache_result = cache_prune_controlled(
        CACHE_ROOT, limit, job->protected_song, &stats,
        &result->cache_evicted,
        worker_cancelled, worker, cache_error, sizeof(cache_error));
    if (cache_result >= 0) {
        result->cache_stats_valid = true;
        result->cache_over_limit = cache_result > 0;
        result->cache_bytes = stats.bytes;
        result->cache_audio_files = stats.audio_files;
        result->cache_cover_files = stats.cover_files;
        result->cache_lyric_files = stats.lyric_files;
    }
    if (worker_cancelled(worker)) {
        finish_failure(worker, result, "请求已取消");
        return;
    }
    result->prefetch_complete =
        cache_song_is_complete(CACHE_ROOT, job->song.id);
    result->success = true;
}

static void run_cache_job(NetworkWorker *worker, const WorkerJob *job,
                          WorkerResult *result) {
    CacheStats stats;
    char error[192] = {0};
    int cache_result = -1;
    bool removed_any = false;
    if (job->kind == WORKER_JOB_CACHE_SCAN) {
        worker_status(worker, "正在扫描媒体缓存");
        cache_result = cache_scan_controlled(
            CACHE_ROOT, &stats, worker_cancelled, worker,
            error, sizeof(error));
    } else if (job->kind == WORKER_JOB_CACHE_PRUNE) {
        worker_status(worker, "正在应用缓存上限");
        cache_result = cache_prune_controlled(
            CACHE_ROOT,
            job->cache_limit ? job->cache_limit : NM3DS_CACHE_LIMIT_DEFAULT,
            job->protected_song, &stats, &removed_any,
            worker_cancelled, worker,
            error, sizeof(error));
    } else {
        worker_status(worker, "正在清理媒体缓存");
        cache_result = cache_clear_controlled(
            CACHE_ROOT, job->protected_song, &stats, &removed_any,
            worker_cancelled, worker, error, sizeof(error));
    }
    result->cache_evicted = removed_any;
    if (cache_result < 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->cache_stats_valid = true;
    result->cache_over_limit = cache_result > 0;
    result->cache_bytes = stats.bytes;
    result->cache_audio_files = stats.audio_files;
    result->cache_cover_files = stats.cover_files;
    result->cache_lyric_files = stats.lyric_files;
    result->success = true;
}

static void run_login_start(NetworkWorker *worker, WorkerResult *result) {
    char error[192] = {0};
    worker_status(worker, "正在生成登录二维码");
    if (netease_login_qr_key(worker->client, result->qr_key,
                             sizeof(result->qr_key),
                             error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    result->success = true;
}

static void run_login_check(NetworkWorker *worker, const WorkerJob *job,
                            WorkerResult *result) {
    char error[192] = {0};
    worker_status(worker, "正在检查扫码登录状态");
    if (netease_login_qr_check(worker->client, job->qr_key,
                               &result->login_code,
                               result->login_message,
                               sizeof(result->login_message),
                               error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    if (result->login_code == 803) {
        worker_status(worker, "正在加载账户资料");
        if (netease_account(worker->client, error, sizeof(error)) != 0) {
            finish_failure(worker, result, error);
            return;
        }
        i18n_snprintf(result->nickname, sizeof(result->nickname), "%s",
                 worker->client->nickname);
        result->user_id = worker->client->user_id;
    }
    result->success = true;
}

static void run_account(NetworkWorker *worker, WorkerResult *result) {
    char error[192] = {0};
    worker_status(worker, "正在验证登录");
    if (netease_account(worker->client, error, sizeof(error)) != 0) {
        finish_failure(worker, result, error);
        return;
    }
    i18n_snprintf(result->nickname, sizeof(result->nickname), "%s",
             worker->client->nickname);
    result->user_id = worker->client->user_id;
    result->success = true;
}

static void run_network_probe(NetworkWorker *worker, WorkerResult *result) {
    char error[192] = {0};
    NetErrorKind failure = NET_ERROR_NONE;
    if (net_probe_https_controlled(
            NETWORK_PROBE_URL, worker_cancelled, worker, &failure,
            error, sizeof(error)) != 0) {
        result->cancelled = failure == NET_ERROR_CANCELLED ||
                            worker_cancelled(worker) != 0;
        result->failure = result->cancelled ? NETEASE_FAILURE_CANCELLED :
                                             diagnostic_net_failure(failure);
        if (!result->cancelled)
            i18n_snprintf(result->error, sizeof(result->error), "%s", error);
        return;
    }
    result->success = true;
}

static void run_queue_cache_check(NetworkWorker *worker,
                                  const WorkerJob *job,
                                  WorkerResult *result) {
    result->song_id = job->song.id;
    result->offset = job->offset;
    result->queue_cache_scan_generation =
        job->queue_cache_scan_generation;
    if (worker_cancelled(worker)) {
        finish_failure(worker, result, "请求已取消");
        return;
    }
    result->queue_cache_playable =
        cache_song_offline_audio_type(
            CACHE_ROOT, job->song.id, job->allow_full_cache) !=
        CACHE_AUDIO_TYPE_UNKNOWN;
    if (worker_cancelled(worker)) {
        finish_failure(worker, result, "请求已取消");
        return;
    }
    result->success = true;
}

static void run_job(NetworkWorker *worker, const WorkerJob *job,
                    WorkerResult *result) {
    worker_result_release(result);
    memset(result, 0, sizeof(*result));
    result->catalog_fee = SONG_FEE_UNKNOWN;
    netease_reset_failure(worker->client);
    result->kind = job->kind;
    result->background = job->background;
    switch (job->kind) {
        case WORKER_JOB_DISCOVER:
        case WORKER_JOB_RECOMMENDATION_ENQUEUE:
            run_discover(worker, job, result);
            break;
        case WORKER_JOB_USER_PLAYLISTS:
            run_user_playlists(worker, job, result);
            break;
        case WORKER_JOB_USER_CLOUD:
            run_user_cloud(worker, job, result);
            break;
        case WORKER_JOB_PLAYLIST_TRACKS:
            run_playlist_tracks(worker, job, result);
            break;
        case WORKER_JOB_PLAYLIST_ENQUEUE:
            run_playlist_tracks(worker, job, result);
            break;
        case WORKER_JOB_ALBUM_TRACKS:
        case WORKER_JOB_ALBUM_ENQUEUE:
            run_album_tracks(worker, job, result);
            break;
        case WORKER_JOB_SONG_ARTISTS:
            run_song_artists(worker, job, result);
            break;
        case WORKER_JOB_ARTIST_ALBUMS:
            run_artist_albums(worker, job, result);
            break;
        case WORKER_JOB_ARTIST_SONGS:
        case WORKER_JOB_ARTIST_SONG_ENQUEUE:
            run_artist_songs(worker, job, result);
            break;
        case WORKER_JOB_SEARCH: run_search(worker, job, result); break;
        case WORKER_JOB_PREPARE_SONG:
            run_prepare_song(worker, job, result);
            break;
        case WORKER_JOB_SONG_EXTRAS:
            run_song_extras(worker, job, result);
            break;
        case WORKER_JOB_PREFETCH_SONG:
            run_prefetch_song(worker, job, result);
            break;
        case WORKER_JOB_LOGIN_QR_START: run_login_start(worker, result); break;
        case WORKER_JOB_LOGIN_QR_CHECK:
            run_login_check(worker, job, result);
            break;
        case WORKER_JOB_ACCOUNT: run_account(worker, result); break;
        case WORKER_JOB_NETWORK_PROBE:
            run_network_probe(worker, result);
            break;
        case WORKER_JOB_QUEUE_CACHE_CHECK:
            run_queue_cache_check(worker, job, result);
            break;
        case WORKER_JOB_CACHE_SCAN:
        case WORKER_JOB_CACHE_PRUNE:
        case WORKER_JOB_CACHE_CLEAR:
            run_cache_job(worker, job, result);
            break;
        case WORKER_JOB_NONE:
        default:
            set_error(result->error, sizeof(result->error),
                      "无效的网络任务");
            break;
    }
}

static void worker_main(void *userdata) {
    NetworkWorker *worker = (NetworkWorker *)userdata;
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
        WorkerJob job = worker->job;
        worker->has_job = false;
        worker->cancel = false;
        worker->busy = true;
        worker->active_kind = job.kind;
        worker->active_background = job.background;
        worker->received = 0;
        worker->total = 0;
        worker->status[0] = '\0';
        LightLock_Unlock(&worker->lock);

        /* Store large job payloads directly in the worker-owned result
         * buffer.  Song-extra results include a decoded 128x128 cover and
         * must not live on the 96 KiB worker thread stack. */
        WorkerResult *result = &worker->result;
        run_job(worker, &job, result);

        LightLock_Lock(&worker->lock);
        worker->busy = false;
        worker->active_kind = WORKER_JOB_NONE;
        worker->active_background = false;
        worker->received = 0;
        worker->total = 0;
        if (!worker->has_job) {
            worker->result_ready = true;
        } else {
            worker_result_release(result);
            worker->result_ready = false;
        }
        bool another = worker->has_job;
        LightLock_Unlock(&worker->lock);
        if (another) LightEvent_Signal(&worker->wake);
    }
    threadExit(0);
}

NetworkWorker *network_worker_create(NeteaseClient *client,
                                     char *error, size_t error_size) {
    if (!client) {
        set_error(error, error_size, "无效的网易云客户端");
        return NULL;
    }
    NetworkWorker *worker = (NetworkWorker *)calloc(1, sizeof(*worker));
    if (!worker) {
        set_error(error, error_size, "内存不足，无法创建网络 Worker");
        return NULL;
    }
    worker->client = client;
    LightLock_Init(&worker->lock);
    LightEvent_Init(&worker->wake, RESET_ONESHOT);
    netease_set_cancel(client, worker_cancelled, worker);
    worker->thread = threadCreate(worker_main, worker, WORKER_STACK_SIZE,
                                  WORKER_PRIORITY, -2, false);
    if (!worker->thread) {
        netease_set_cancel(client, NULL, NULL);
        free(worker);
        set_error(error, error_size, "无法创建网络 Worker 线程");
        return NULL;
    }
    return worker;
}

void network_worker_destroy(NetworkWorker *worker) {
    if (!worker) return;
    LightLock_Lock(&worker->lock);
    worker->stop = true;
    worker->cancel = true;
    LightLock_Unlock(&worker->lock);
    LightEvent_Signal(&worker->wake);
    threadJoin(worker->thread, U64_MAX);
    threadFree(worker->thread);
    worker_result_release(&worker->result);
    netease_set_cancel(worker->client, NULL, NULL);
    free(worker);
}

bool network_worker_submit(NetworkWorker *worker, const WorkerJob *job) {
    if (!worker || !job || job->kind == WORKER_JOB_NONE) return false;
    LightLock_Lock(&worker->lock);
    if (worker->stop) {
        LightLock_Unlock(&worker->lock);
        return false;
    }
    if (worker->result_ready) worker_result_release(&worker->result);
    if (worker->busy) worker->cancel = true;
    worker->job = *job;
    worker->has_job = true;
    worker->result_ready = false;
    LightLock_Unlock(&worker->lock);
    LightEvent_Signal(&worker->wake);
    return true;
}

void network_worker_cancel(NetworkWorker *worker) {
    if (!worker) return;
    LightLock_Lock(&worker->lock);
    worker->cancel = true;
    worker->has_job = false;
    LightLock_Unlock(&worker->lock);
}

void network_worker_snapshot(NetworkWorker *worker, WorkerSnapshot *snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (!worker) return;
    LightLock_Lock(&worker->lock);
    snapshot->busy = worker->busy || worker->has_job;
    snapshot->background = worker->busy ? worker->active_background :
                           worker->has_job ? worker->job.background :
                           worker->result_ready ? worker->result.background :
                                                  false;
    snapshot->kind = worker->busy ? worker->active_kind :
                     worker->has_job ? worker->job.kind :
                     worker->result_ready ? worker->result.kind :
                                            WORKER_JOB_NONE;
    snapshot->queued_kind = worker->has_job ? worker->job.kind :
                                             WORKER_JOB_NONE;
    snapshot->received = worker->received;
    snapshot->total = worker->total;
    i18n_snprintf(snapshot->status, sizeof(snapshot->status), "%s", worker->status);
    LightLock_Unlock(&worker->lock);
}

bool network_worker_take_result(NetworkWorker *worker, WorkerResult *result) {
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
