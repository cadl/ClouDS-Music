#pragma once

#include "cover.h"
#include "model.h"
#include "netease.h"
#include "player.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

_Static_assert(NM3DS_ARTIST_SONG_PAGE <= NM3DS_RECOMMEND_RESULTS,
               "artist song pages must fit WorkerResult.songs");

typedef enum {
    WORKER_JOB_NONE = 0,
    WORKER_JOB_DISCOVER,
    WORKER_JOB_USER_PLAYLISTS,
    WORKER_JOB_USER_CLOUD,
    WORKER_JOB_PLAYLIST_TRACKS,
    WORKER_JOB_PLAYLIST_ENQUEUE,
    WORKER_JOB_ALBUM_TRACKS,
    WORKER_JOB_ALBUM_ENQUEUE,
    WORKER_JOB_SONG_ARTISTS,
    WORKER_JOB_ARTIST_ALBUMS,
    WORKER_JOB_ARTIST_SONGS,
    WORKER_JOB_ARTIST_SONG_ENQUEUE,
    WORKER_JOB_RECOMMENDATION_ENQUEUE,
    WORKER_JOB_SEARCH,
    WORKER_JOB_PREPARE_SONG,
    WORKER_JOB_SONG_EXTRAS,
    WORKER_JOB_PREFETCH_SONG,
    WORKER_JOB_LOGIN_QR_START,
    WORKER_JOB_LOGIN_QR_CHECK,
    WORKER_JOB_ACCOUNT,
    WORKER_JOB_NETWORK_PROBE,
    WORKER_JOB_QUEUE_CACHE_CHECK,
    WORKER_JOB_CACHE_SCAN,
    WORKER_JOB_CACHE_PRUNE,
    WORKER_JOB_CACHE_CLEAR
} WorkerJobKind;

typedef enum {
    WORKER_DIAGNOSTIC_NONE = 0,
    WORKER_DIAGNOSTIC_SONG_DETAIL,
    WORKER_DIAGNOSTIC_COVER_DOWNLOAD,
    WORKER_DIAGNOSTIC_COVER_DECODE,
    WORKER_DIAGNOSTIC_LYRICS_DOWNLOAD
} WorkerDiagnosticKind;

typedef struct {
    WorkerJobKind kind;
    Song song;
    char query[96];
    size_t offset;
    uint32_t queue_cache_scan_generation;
    RecommendationSource recommendation_source;
    int64_t playlist_id;
    int64_t album_id;
    int64_t artist_id;
    int64_t protected_song;
    uint64_t cache_limit;
    bool force_download;
    bool offline_playback;
    bool allow_full_cache;
    bool background;
    bool refresh_index;
    char qr_key[128];
} WorkerJob;

typedef struct {
    WorkerJobKind kind;
    bool success;
    bool cancelled;
    bool background;
    bool queue_cache_playable;
    uint32_t queue_cache_scan_generation;
    bool offline_playback;
    NeteaseFailure failure;
    RecommendationSource recommendation_source;
    char error[192];
    WorkerDiagnosticKind diagnostic_kind;
    NeteaseFailure diagnostic_failure;
    char diagnostic_error[192];

    Song songs[NM3DS_RECOMMEND_RESULTS];
    size_t song_count;
    NeteasePlaylist playlists[NM3DS_LIBRARY_PAGE];
    size_t playlist_count;
    NeteaseCloudTrack cloud_tracks[NM3DS_CLOUD_PAGE];
    size_t cloud_track_count;
    int64_t playlist_id;
    size_t playlist_track_total;
    int64_t album_id;
    char album_name[96];
    size_t album_track_total;
    NeteaseArtist artists[NM3DS_SONG_ARTISTS_MAX];
    size_t artist_count;
    int64_t artist_id;
    NeteaseAlbum albums[NM3DS_ARTIST_ALBUM_PAGE];
    size_t album_count;
    size_t offset;
    bool has_more;
    size_t recommendation_total_count;
    bool recommendation_total_known;

    int64_t song_id;
    int64_t prefetch_anchor_song_id;
    bool prefetch_was_cached;
    bool prefetch_complete;
    bool audio_was_cached;
    bool audio_needs_download;
    bool audio_seek_pending;
    bool cache_changed;
    bool playback_resolved;
    bool audio_is_trial;
    uint8_t catalog_fee;
    char audio_url[1536];
    char audio_path[256];
    PreparedAudio *prepared_audio;
    char cover_path[256];
    char song_pic_url[320];
    bool cover_ready;
    uint32_t cover_pixels[COVER_ART_PIXELS];
    bool cache_stats_valid;
    bool cache_over_limit;
    bool cache_evicted;
    uint64_t cache_bytes;
    size_t cache_audio_files;
    size_t cache_cover_files;
    size_t cache_lyric_files;
    LyricLine lyrics[NM3DS_MAX_LYRICS];
    size_t lyric_count;
    bool song_extras_cached;

    char qr_key[128];
    int login_code;
    char login_message[96];
    char nickname[96];
    int64_t user_id;
} WorkerResult;

typedef struct {
    bool busy;
    bool background;
    WorkerJobKind kind;
    WorkerJobKind queued_kind;
    uint64_t received;
    uint64_t total;
    char status[192];
} WorkerSnapshot;

typedef struct NetworkWorker NetworkWorker;

NetworkWorker *network_worker_create(NeteaseClient *client,
                                     char *error, size_t error_size);
void network_worker_destroy(NetworkWorker *worker);
bool network_worker_submit(NetworkWorker *worker, const WorkerJob *job);
void network_worker_cancel(NetworkWorker *worker);
void network_worker_snapshot(NetworkWorker *worker, WorkerSnapshot *snapshot);
bool network_worker_take_result(NetworkWorker *worker, WorkerResult *result);
