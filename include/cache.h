#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NM3DS_CACHE_MIB (1024ULL * 1024ULL)
#define NM3DS_CACHE_LIMIT_DEFAULT (256ULL * NM3DS_CACHE_MIB)
/* This is both effectively unbounded on SDMC and representable by curl_off_t. */
#define NM3DS_CACHE_LIMIT_UNLIMITED UINT64_C(0x7fffffffffffffff)
#define NM3DS_CACHE_LIMIT_OPTION_COUNT 5
#define NM3DS_CACHE_DATA_DIRECTORY "data"
#define NM3DS_CACHE_AUDIO_FILENAME "audio.mp3"
#define NM3DS_CACHE_AUDIO_TRIAL_FILENAME "audio.trial.mp3"
#define NM3DS_CACHE_COVER_FILENAME "cover.jpg"
#define NM3DS_CACHE_LYRIC_FILENAME "lyrics.lrc"

typedef enum {
    CACHE_ASSET_AUDIO = 0,
    CACHE_ASSET_AUDIO_TRIAL,
    CACHE_ASSET_COVER,
    CACHE_ASSET_LYRIC
} CacheAssetKind;

typedef enum {
    CACHE_AUDIO_TYPE_UNKNOWN = 0,
    CACHE_AUDIO_TYPE_FULL,
    CACHE_AUDIO_TYPE_TRIAL
} CacheAudioType;

typedef struct {
    uint64_t bytes;
    size_t audio_files;
    size_t cover_files;
    size_t lyric_files;
} CacheStats;

typedef int (*CacheCancelFn)(void *userdata);

uint64_t cache_limit_option(size_t index);
int cache_limit_option_index(uint64_t limit);
bool cache_limit_is_unlimited(uint64_t limit);

int cache_ensure_song_directory(const char *root, int64_t song_id,
                                char *error, size_t error_size);
int cache_song_path(const char *root, int64_t song_id, CacheAssetKind asset,
                    char *path, size_t path_size);
int cache_song_audio_path(const char *root, int64_t song_id,
                          CacheAudioType type, char *path, size_t path_size);
bool cache_song_has_asset(const char *root, int64_t song_id,
                          CacheAssetKind asset);
bool cache_song_has_audio_type(const char *root, int64_t song_id,
                               CacheAudioType type);
CacheAudioType cache_song_audio_type(const char *root, int64_t song_id);
CacheAudioType cache_song_online_audio_type(const char *root, int64_t song_id,
                                            bool allow_full);
CacheAudioType cache_song_offline_audio_type(const char *root, int64_t song_id,
                                             bool allow_full);
/* Returns true only when the unwanted cached file was actually removed. */
bool cache_song_remove_other_audio_types(const char *root, int64_t song_id,
                                         CacheAudioType keep_type);
bool cache_song_is_complete(const char *root, int64_t song_id);

int cache_scan(const char *root, CacheStats *stats,
               char *error, size_t error_size);
int cache_prune(const char *root, uint64_t limit, int64_t protected_song,
                CacheStats *stats, char *error, size_t error_size);
int cache_clear(const char *root, int64_t protected_song,
                CacheStats *stats, char *error, size_t error_size);
int cache_scan_controlled(const char *root, CacheStats *stats,
                          CacheCancelFn cancelled, void *userdata,
                          char *error, size_t error_size);
int cache_prune_controlled(const char *root, uint64_t limit,
                           int64_t protected_song, CacheStats *stats,
                           bool *removed_any,
                           CacheCancelFn cancelled, void *userdata,
                           char *error, size_t error_size);
int cache_clear_controlled(const char *root, int64_t protected_song,
                           CacheStats *stats, bool *removed_any,
                           CacheCancelFn cancelled,
                           void *userdata, char *error, size_t error_size);
