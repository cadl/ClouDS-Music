#include "cache.h"

#include "i18n.h"

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define CACHE_GROUP_INITIAL_CAPACITY 32U

typedef enum {
    CACHE_FILE_AUDIO = 0,
    CACHE_FILE_COVER,
    CACHE_FILE_LYRIC
} CacheFileKind;

typedef struct {
    uint64_t asset_sizes[CACHE_ASSET_LYRIC + 1];
    uint64_t size;
    time_t modified;
    int64_t song_id;
    bool protected_group;
} CacheGroup;

static const uint64_t cache_limits[NM3DS_CACHE_LIMIT_OPTION_COUNT] = {
    64ULL * NM3DS_CACHE_MIB,
    128ULL * NM3DS_CACHE_MIB,
    256ULL * NM3DS_CACHE_MIB,
    512ULL * NM3DS_CACHE_MIB,
    NM3DS_CACHE_LIMIT_UNLIMITED
};

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

uint64_t cache_limit_option(size_t index) {
    return index < NM3DS_CACHE_LIMIT_OPTION_COUNT ? cache_limits[index] : 0;
}

int cache_limit_option_index(uint64_t limit) {
    for (size_t i = 0; i < NM3DS_CACHE_LIMIT_OPTION_COUNT; i++)
        if (cache_limits[i] == limit) return (int)i;
    return -1;
}

bool cache_limit_is_unlimited(uint64_t limit) {
    return limit == NM3DS_CACHE_LIMIT_UNLIMITED;
}

static int64_t song_id_from_name(const char *name, const char *suffix) {
    if (!name || !suffix) return -1;
    char *end = NULL;
    long long value = strtoll(name, &end, 10);
    if (value <= 0 || !end || strcmp(end, suffix) != 0) return -1;
    return (int64_t)value;
}

static const char *asset_filename(CacheAssetKind asset) {
    switch (asset) {
        case CACHE_ASSET_AUDIO: return NM3DS_CACHE_AUDIO_FILENAME;
        case CACHE_ASSET_AUDIO_TRIAL: return NM3DS_CACHE_AUDIO_TRIAL_FILENAME;
        case CACHE_ASSET_COVER: return NM3DS_CACHE_COVER_FILENAME;
        case CACHE_ASSET_LYRIC: return NM3DS_CACHE_LYRIC_FILENAME;
    }
    return NULL;
}

static int ensure_directory(const char *path, char *error,
                            size_t error_size) {
    if (mkdir(path, 0777) == 0) return 0;
    if (errno == EEXIST) {
        struct stat info;
        if (stat(path, &info) == 0 && S_ISDIR(info.st_mode)) return 0;
    }
    set_error(error, error_size, "无法创建缓存目录");
    return -1;
}

int cache_ensure_song_directory(const char *root, int64_t song_id,
                                char *error, size_t error_size) {
    if (!root || !root[0] || song_id <= 0) {
        set_error(error, error_size, "歌曲缓存目录无效");
        return -1;
    }
    if (ensure_directory(root, error, error_size) != 0) return -1;
    char data[320];
    int written = snprintf(data, sizeof(data), "%s/%s", root,
                           NM3DS_CACHE_DATA_DIRECTORY);
    if (written < 0 || (size_t)written >= sizeof(data)) {
        set_error(error, error_size, "缓存目录路径过长");
        return -1;
    }
    if (ensure_directory(data, error, error_size) != 0) return -1;
    char song[320];
    written = snprintf(song, sizeof(song), "%s/%lld", data,
                       (long long)song_id);
    if (written < 0 || (size_t)written >= sizeof(song)) {
        set_error(error, error_size, "歌曲缓存路径过长");
        return -1;
    }
    return ensure_directory(song, error, error_size);
}

int cache_song_path(const char *root, int64_t song_id, CacheAssetKind asset,
                    char *path, size_t path_size) {
    const char *filename = asset_filename(asset);
    if (!root || !root[0] || song_id <= 0 || !filename || !path ||
        path_size == 0)
        return -1;
    int written = snprintf(path, path_size, "%s/%s/%lld/%s", root,
                           NM3DS_CACHE_DATA_DIRECTORY, (long long)song_id,
                           filename);
    return written >= 0 && (size_t)written < path_size ? 0 : -1;
}

int cache_song_audio_path(const char *root, int64_t song_id,
                          CacheAudioType type, char *path, size_t path_size) {
    CacheAssetKind asset;
    if (type == CACHE_AUDIO_TYPE_FULL)
        asset = CACHE_ASSET_AUDIO;
    else if (type == CACHE_AUDIO_TYPE_TRIAL)
        asset = CACHE_ASSET_AUDIO_TRIAL;
    else
        return -1;
    return cache_song_path(root, song_id, asset, path, path_size);
}

bool cache_song_has_asset(const char *root, int64_t song_id,
                          CacheAssetKind asset) {
    char path[320];
    struct stat info;
    return cache_song_path(root, song_id, asset, path, sizeof(path)) == 0 &&
           stat(path, &info) == 0 && S_ISREG(info.st_mode) &&
           info.st_size > 0;
}

bool cache_song_has_audio_type(const char *root, int64_t song_id,
                               CacheAudioType type) {
    char path[320];
    struct stat info;
    return cache_song_audio_path(root, song_id, type,
                                 path, sizeof(path)) == 0 &&
           stat(path, &info) == 0 && S_ISREG(info.st_mode) &&
           info.st_size > 0;
}

CacheAudioType cache_song_audio_type(const char *root, int64_t song_id) {
    bool full = cache_song_has_audio_type(root, song_id,
                                          CACHE_AUDIO_TYPE_FULL);
    bool trial = cache_song_has_audio_type(root, song_id,
                                           CACHE_AUDIO_TYPE_TRIAL);
    if (full == trial) return CACHE_AUDIO_TYPE_UNKNOWN;
    return full ? CACHE_AUDIO_TYPE_FULL : CACHE_AUDIO_TYPE_TRIAL;
}

CacheAudioType cache_song_online_audio_type(const char *root, int64_t song_id,
                                            bool allow_full) {
    CacheAudioType wanted = allow_full ? CACHE_AUDIO_TYPE_FULL :
                                         CACHE_AUDIO_TYPE_TRIAL;
    return cache_song_has_audio_type(root, song_id, wanted) ? wanted :
           CACHE_AUDIO_TYPE_UNKNOWN;
}

CacheAudioType cache_song_offline_audio_type(const char *root, int64_t song_id,
                                             bool allow_full) {
    if (allow_full &&
        cache_song_has_audio_type(root, song_id, CACHE_AUDIO_TYPE_FULL))
        return CACHE_AUDIO_TYPE_FULL;
    if (cache_song_has_audio_type(root, song_id, CACHE_AUDIO_TYPE_TRIAL))
        return CACHE_AUDIO_TYPE_TRIAL;
    return CACHE_AUDIO_TYPE_UNKNOWN;
}

bool cache_song_remove_other_audio_types(const char *root, int64_t song_id,
                                         CacheAudioType keep_type) {
    if (!root || song_id <= 0 ||
        (keep_type != CACHE_AUDIO_TYPE_FULL &&
         keep_type != CACHE_AUDIO_TYPE_TRIAL))
        return false;
    CacheAssetKind remove_asset = keep_type == CACHE_AUDIO_TYPE_FULL ?
                                  CACHE_ASSET_AUDIO_TRIAL :
                                  CACHE_ASSET_AUDIO;
    char path[320];
    if (cache_song_path(root, song_id, remove_asset,
                        path, sizeof(path)) == 0)
        return remove(path) == 0;
    return false;
}

bool cache_song_is_complete(const char *root, int64_t song_id) {
    return cache_song_audio_type(root, song_id) != CACHE_AUDIO_TYPE_UNKNOWN &&
           cache_song_has_asset(root, song_id, CACHE_ASSET_COVER) &&
           cache_song_has_asset(root, song_id, CACHE_ASSET_LYRIC);
}

static void gather_song_asset(const char *root, int64_t song_id,
                              CacheAssetKind asset, CacheFileKind kind,
                              CacheGroup *group, CacheStats *stats) {
    char path[320];
    if (cache_song_path(root, song_id, asset, path, sizeof(path)) != 0)
        return;
    struct stat info;
    if (stat(path, &info) != 0 || info.st_size <= 0 ||
        !S_ISREG(info.st_mode))
        return;
    stats->bytes += (uint64_t)info.st_size;
    if (kind == CACHE_FILE_AUDIO) stats->audio_files++;
    else if (kind == CACHE_FILE_COVER) stats->cover_files++;
    else stats->lyric_files++;
    if (!group) return;
    if (group->size == 0 || info.st_mtime < group->modified)
        group->modified = info.st_mtime;
    group->asset_sizes[asset] = (uint64_t)info.st_size;
    group->size += (uint64_t)info.st_size;
}

static bool append_cache_group(CacheGroup **groups, size_t *count,
                               size_t *capacity,
                               const CacheGroup *group) {
    if (!groups || !count || !capacity || !group) return false;
    if (*count >= *capacity) {
        size_t next = *capacity ? *capacity * 2U :
                                  CACHE_GROUP_INITIAL_CAPACITY;
        if (next < *capacity || next > SIZE_MAX / sizeof(**groups))
            return false;
        CacheGroup *resized = (CacheGroup *)realloc(
            *groups, next * sizeof(**groups));
        if (!resized) return false;
        *groups = resized;
        *capacity = next;
    }
    (*groups)[(*count)++] = *group;
    return true;
}

static int gather_song_directories(const char *root,
                                   int64_t protected_song,
                                   CacheGroup **groups, size_t *count,
                                   CacheStats *stats,
                                   CacheCancelFn cancelled,
                                   void *userdata) {
    char data[320];
    int written = snprintf(data, sizeof(data), "%s/%s", root,
                           NM3DS_CACHE_DATA_DIRECTORY);
    if (written < 0 || (size_t)written >= sizeof(data)) return 0;
    DIR *directory = opendir(data);
    if (!directory) return 0;
    size_t capacity = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (cancelled && cancelled(userdata)) {
            closedir(directory);
            return -2;
        }
        if (entry->d_name[0] == '.') continue;
        int64_t song_id = song_id_from_name(entry->d_name, "");
        if (song_id <= 0) continue;
        char song_directory[320];
        written = snprintf(song_directory, sizeof(song_directory), "%s/%s",
                           data, entry->d_name);
        struct stat info;
        if (written < 0 || (size_t)written >= sizeof(song_directory) ||
            stat(song_directory, &info) != 0 || !S_ISDIR(info.st_mode))
            continue;
        CacheGroup group;
        memset(&group, 0, sizeof(group));
        group.song_id = song_id;
        group.protected_group = protected_song > 0 &&
                                song_id == protected_song;
        gather_song_asset(root, song_id, CACHE_ASSET_AUDIO,
                          CACHE_FILE_AUDIO, &group, stats);
        gather_song_asset(root, song_id, CACHE_ASSET_AUDIO_TRIAL,
                          CACHE_FILE_AUDIO, &group, stats);
        gather_song_asset(root, song_id, CACHE_ASSET_COVER,
                          CACHE_FILE_COVER, &group, stats);
        gather_song_asset(root, song_id, CACHE_ASSET_LYRIC,
                          CACHE_FILE_LYRIC, &group, stats);
        if (groups && group.size > 0 &&
            !append_cache_group(groups, count, &capacity, &group)) {
            closedir(directory);
            return -3;
        }
    }
    closedir(directory);
    return 0;
}

static int gather(const char *root, int64_t protected_song,
                  CacheGroup **groups, size_t *count, CacheStats *stats,
                  CacheCancelFn cancelled, void *userdata) {
    memset(stats, 0, sizeof(*stats));
    if (groups) *groups = NULL;
    if (count) *count = 0;
    int result = gather_song_directories(
        root, protected_song, groups, count, stats,
        cancelled, userdata);
    if (result != 0 && groups) {
        free(*groups);
        *groups = NULL;
        if (count) *count = 0;
    }
    return result;
}

static int compare_oldest(const void *left, const void *right) {
    const CacheGroup *a = (const CacheGroup *)left;
    const CacheGroup *b = (const CacheGroup *)right;
    if (a->modified < b->modified) return -1;
    if (a->modified > b->modified) return 1;
    if (a->song_id < b->song_id) return -1;
    if (a->song_id > b->song_id) return 1;
    return 0;
}

static int remove_group(const char *root, CacheGroup *group,
                        CacheStats *stats, CacheCancelFn cancelled,
                        void *userdata) {
    for (int asset = CACHE_ASSET_AUDIO;
         asset <= CACHE_ASSET_LYRIC; asset++) {
        uint64_t size = group->asset_sizes[asset];
        if (!size) continue;
        if (cancelled && cancelled(userdata)) return -2;
        char path[320];
        if (cache_song_path(root, group->song_id, (CacheAssetKind)asset,
                            path, sizeof(path)) != 0 ||
            remove(path) != 0)
            continue;
        stats->bytes -= size;
        if (asset == CACHE_ASSET_AUDIO ||
            asset == CACHE_ASSET_AUDIO_TRIAL)
            stats->audio_files--;
        else if (asset == CACHE_ASSET_COVER) stats->cover_files--;
        else stats->lyric_files--;
        group->asset_sizes[asset] = 0;
    }
    if (group->song_id > 0) {
        char directory[320];
        int written = snprintf(directory, sizeof(directory), "%s/%s/%lld",
                               root, NM3DS_CACHE_DATA_DIRECTORY,
                               (long long)group->song_id);
        if (written >= 0 && (size_t)written < sizeof(directory))
            (void)rmdir(directory);
    }
    return 0;
}

int cache_scan_controlled(const char *root, CacheStats *stats,
                          CacheCancelFn cancelled, void *userdata,
                          char *error, size_t error_size) {
    if (!root || !stats) {
        set_error(error, error_size, "缓存路径无效");
        return -1;
    }
    int result = gather(root, -1, NULL, NULL, stats,
                        cancelled, userdata);
    if (result == -2)
        set_error(error, error_size, "缓存扫描已取消");
    return result;
}

int cache_scan(const char *root, CacheStats *stats,
               char *error, size_t error_size) {
    return cache_scan_controlled(root, stats, NULL, NULL,
                                 error, error_size);
}

int cache_prune_controlled(const char *root, uint64_t limit,
                           int64_t protected_song, CacheStats *stats,
                           bool *removed_any,
                           CacheCancelFn cancelled, void *userdata,
                           char *error, size_t error_size) {
    if (removed_any) *removed_any = false;
    if (!root || !stats) {
        set_error(error, error_size, "缓存路径无效");
        return -1;
    }
    if (cache_limit_is_unlimited(limit))
        return cache_scan_controlled(root, stats, cancelled, userdata,
                                     error, error_size);
    CacheGroup *groups = NULL;
    size_t count = 0;
    int result = gather(root, protected_song, &groups, &count, stats,
                        cancelled, userdata);
    if (result == -2) {
        set_error(error, error_size, "缓存清理已取消");
        return result;
    }
    if (result == -3) {
        set_error(error, error_size, "内存不足，无法清理缓存");
        return -1;
    }
    if (count > 1) qsort(groups, count, sizeof(*groups), compare_oldest);
    for (size_t i = 0; i < count && stats->bytes > limit; i++) {
        if (!groups[i].size || groups[i].protected_group) continue;
        uint64_t bytes_before = stats->bytes;
        if (remove_group(root, &groups[i], stats,
                         cancelled, userdata) == -2) {
            if (removed_any && stats->bytes < bytes_before)
                *removed_any = true;
            free(groups);
            set_error(error, error_size, "缓存清理已取消");
            return -2;
        }
        if (removed_any && stats->bytes < bytes_before)
            *removed_any = true;
    }
    free(groups);
    if (stats->bytes > limit) {
        set_error(error, error_size, "缓存仍超出上限，已保留受保护文件");
        return 1;
    }
    return 0;
}

int cache_prune(const char *root, uint64_t limit, int64_t protected_song,
                CacheStats *stats, char *error, size_t error_size) {
    return cache_prune_controlled(root, limit, protected_song, stats,
                                  NULL, NULL, NULL, error, error_size);
}

int cache_clear_controlled(const char *root, int64_t protected_song,
                           CacheStats *stats, bool *removed_any,
                           CacheCancelFn cancelled,
                           void *userdata, char *error, size_t error_size) {
    if (removed_any) *removed_any = false;
    if (!root || !stats) {
        set_error(error, error_size, "缓存路径无效");
        return -1;
    }
    CacheGroup *groups = NULL;
    size_t count = 0;
    int result = gather(root, protected_song, &groups, &count, stats,
                        cancelled, userdata);
    if (result == -2) {
        set_error(error, error_size, "缓存清理已取消");
        return result;
    }
    if (result == -3) {
        set_error(error, error_size, "内存不足，无法清空缓存");
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        if (!groups[i].size || groups[i].protected_group) continue;
        uint64_t bytes_before = stats->bytes;
        if (remove_group(root, &groups[i], stats,
                         cancelled, userdata) == -2) {
            if (removed_any && stats->bytes < bytes_before)
                *removed_any = true;
            free(groups);
            set_error(error, error_size, "缓存清理已取消");
            return -2;
        }
        if (removed_any && stats->bytes < bytes_before)
            *removed_any = true;
    }
    free(groups);
    return 0;
}

int cache_clear(const char *root, int64_t protected_song,
                CacheStats *stats, char *error, size_t error_size) {
    return cache_clear_controlled(root, protected_song, stats,
                                  NULL, NULL, NULL, error, error_size);
}
