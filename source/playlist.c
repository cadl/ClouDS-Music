#include "playlist.h"

#include "i18n.h"
#include "song_text.h"

#include <zlib.h>

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define JOURNAL_HEADER_SIZE 16U
#define JOURNAL_MAX_PAYLOAD 704U
#define STORED_SONG_SIZE 649U

#define PATCH_COVER_URL (1U << 0)
#define PATCH_FEE (1U << 1)

typedef enum {
    JOURNAL_ADD = 1,
    JOURNAL_REMOVE,
    JOURNAL_PATCH,
    JOURNAL_MOVE,
    JOURNAL_CLEAR,
    JOURNAL_STATE
} JournalRecordType;

typedef struct {
    char magic[4];
    uint32_t version;
    uint32_t count;
    int32_t selected;
    uint32_t play_mode;
    float volume;
} PlaylistHeader;

typedef struct {
    int64_t id;
    char title[128];
    char artist[96];
    char album[96];
    char pic_url[320];
} StoredSongV1;

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static bool song_fee_valid(uint8_t fee) {
    return fee == SONG_FEE_FREE || fee == SONG_FEE_VIP ||
           fee == SONG_FEE_ALBUM || fee == SONG_FEE_LOW_QUALITY_FREE ||
           fee == SONG_FEE_UNKNOWN;
}

static void put_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *output, uint32_t value) {
    for (unsigned int i = 0; i < 4; i++)
        output[i] = (uint8_t)(value >> (i * 8));
}

static void put_u64(uint8_t *output, uint64_t value) {
    for (unsigned int i = 0; i < 8; i++)
        output[i] = (uint8_t)(value >> (i * 8));
}

static uint16_t get_u16(const uint8_t *input) {
    return (uint16_t)input[0] | (uint16_t)input[1] << 8;
}

static uint32_t get_u32(const uint8_t *input) {
    uint32_t value = 0;
    for (unsigned int i = 0; i < 4; i++) value |= (uint32_t)input[i] << (i * 8);
    return value;
}

static uint64_t get_u64(const uint8_t *input) {
    uint64_t value = 0;
    for (unsigned int i = 0; i < 8; i++) value |= (uint64_t)input[i] << (i * 8);
    return value;
}

static void encode_song(uint8_t output[STORED_SONG_SIZE], const Song *song) {
    memset(output, 0, STORED_SONG_SIZE);
    put_u64(output, (uint64_t)song->id);
    snprintf((char *)output + 8, sizeof(song->title), "%s", song->title);
    snprintf((char *)output + 136, sizeof(song->artist), "%s", song->artist);
    snprintf((char *)output + 232, sizeof(song->album), "%s", song->album);
    snprintf((char *)output + 328, sizeof(song->pic_url), "%s", song->pic_url);
    output[648] = song->fee;
}

static int decode_song(const uint8_t input[STORED_SONG_SIZE], Song *song) {
    memset(song, 0, sizeof(*song));
    song->id = (int64_t)get_u64(input);
    memcpy(song->title, input + 8, sizeof(song->title));
    memcpy(song->artist, input + 136, sizeof(song->artist));
    memcpy(song->album, input + 232, sizeof(song->album));
    memcpy(song->pic_url, input + 328, sizeof(song->pic_url));
    song->title[sizeof(song->title) - 1] = '\0';
    song->artist[sizeof(song->artist) - 1] = '\0';
    song->album[sizeof(song->album) - 1] = '\0';
    song->pic_url[sizeof(song->pic_url) - 1] = '\0';
    song->fee = input[648];
    if (song->id <= 0 || !song_fee_valid(song->fee)) return -1;
    song_text_compose_hangul_nfc(song);
    return 0;
}

static int song_index(const AppState *app, int64_t song_id) {
    if (!app || song_id <= 0) return -1;
    for (size_t i = 0; i < app->queue_count; i++)
        if (app->queue[i].id == song_id) return (int)i;
    return -1;
}

static int64_t selected_song_id(const AppState *app) {
    return app && app->queue_selected >= 0 &&
           app->queue_selected < (int)app->queue_count ?
           app->queue[app->queue_selected].id : 0;
}

static void restore_selected_song(AppState *app, int64_t song_id) {
    if (!app) return;
    int index = song_index(app, song_id);
    if (index >= 0) app->queue_selected = index;
    else if (app->queue_count == 0) app->queue_selected = -1;
    else if (app->queue_selected < 0 ||
            app->queue_selected >= (int)app->queue_count)
        app->queue_selected = 0;
}

static int insert_song(AppState *app, size_t position, const Song *song) {
    if (!app || !song || app->queue_count >= NM3DS_MAX_QUEUE) return -1;
    if (position > app->queue_count) position = app->queue_count;
    if (position < app->queue_count) {
        memmove(&app->queue[position + 1], &app->queue[position],
                (app->queue_count - position) * sizeof(app->queue[0]));
        memmove(&app->queue_offline_playable[position + 1],
                &app->queue_offline_playable[position],
                (app->queue_count - position) *
                    sizeof(app->queue_offline_playable[0]));
        memmove(&app->queue_cache_known[position + 1],
                &app->queue_cache_known[position],
                (app->queue_count - position) *
                    sizeof(app->queue_cache_known[0]));
    }
    app->queue[position] = *song;
    app->queue_offline_playable[position] = false;
    app->queue_cache_known[position] = false;
    song_text_compose_hangul_nfc(&app->queue[position]);
    app->queue_count++;
    return (int)position;
}

int playlist_load(AppState *app, const char *path,
                  char *error, size_t error_size) {
    if (!app || !path) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return 1;
    PlaylistHeader header;
    if (fread(&header, 1, sizeof(header), file) != sizeof(header) ||
        memcmp(header.magic, "PLST", 4) != 0 ||
        (header.version != 1 && header.version != 2) ||
        header.count > NM3DS_MAX_QUEUE) {
        fclose(file);
        set_error(error, error_size, "保存的播放列表无效");
        return -1;
    }
    if (header.version == 1) {
        for (uint32_t i = 0; i < header.count; i++) {
            StoredSongV1 stored;
            if (fread(&stored, 1, sizeof(stored), file) != sizeof(stored)) {
                fclose(file);
                set_error(error, error_size, "保存的播放列表无效");
                return -1;
            }
            Song *song = &app->queue[i];
            memset(song, 0, sizeof(*song));
            song->id = stored.id;
            memcpy(song->title, stored.title, sizeof(song->title));
            memcpy(song->artist, stored.artist, sizeof(song->artist));
            memcpy(song->album, stored.album, sizeof(song->album));
            memcpy(song->pic_url, stored.pic_url, sizeof(song->pic_url));
            song->fee = SONG_FEE_UNKNOWN;
        }
    } else if (fread(app->queue, sizeof(app->queue[0]), header.count, file) !=
               header.count) {
        fclose(file);
        set_error(error, error_size, "保存的播放列表无效");
        return -1;
    }
    bool eof = fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (!eof) {
        set_error(error, error_size, "保存的播放列表无效");
        return -1;
    }
    for (uint32_t i = 0; i < header.count; i++) {
        uint8_t fee = app->queue[i].fee;
        if (!song_fee_valid(fee)) app->queue[i].fee = SONG_FEE_UNKNOWN;
        app->queue[i].title[sizeof(app->queue[i].title) - 1] = '\0';
        app->queue[i].artist[sizeof(app->queue[i].artist) - 1] = '\0';
        app->queue[i].album[sizeof(app->queue[i].album) - 1] = '\0';
        app->queue[i].pic_url[sizeof(app->queue[i].pic_url) - 1] = '\0';
        song_text_compose_hangul_nfc(&app->queue[i]);
    }
    app->queue_count = header.count;
    memset(app->queue_offline_playable, 0,
           sizeof(app->queue_offline_playable));
    memset(app->queue_cache_known, 0, sizeof(app->queue_cache_known));
    app->queue_selected = header.selected;
    if (app->queue_selected < 0 ||
        app->queue_selected >= (int)app->queue_count)
        app->queue_selected = app->queue_count ? 0 : -1;
    app->current_queue = -1;
    app->pending_queue = -1;
    app->play_mode = header.play_mode < PLAY_MODE_COUNT ?
                     (PlayMode)header.play_mode : PLAY_MODE_SEQUENCE;
    app->volume = header.volume;
    if (app->volume < 0.0f || app->volume > 1.0f) app->volume = 1.0f;
    return 0;
}

int playlist_save(const AppState *app, const char *path,
                  char *error, size_t error_size) {
    if (!app || !path || app->queue_count > NM3DS_MAX_QUEUE) return -1;
    char temporary[320];
    char backup[320];
    int written = snprintf(temporary, sizeof(temporary), "%s.part", path);
    int backup_written = snprintf(backup, sizeof(backup), "%s.bak", path);
    if (written < 0 || (size_t)written >= sizeof(temporary) ||
        backup_written < 0 || (size_t)backup_written >= sizeof(backup)) {
        set_error(error, error_size, "播放列表路径过长");
        return -1;
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        set_error(error, error_size, "无法保存播放列表");
        return -1;
    }
    PlaylistHeader header = {
        {'P', 'L', 'S', 'T'}, 2, (uint32_t)app->queue_count,
        app->queue_selected, (uint32_t)app->play_mode, app->volume
    };
    bool wrote = fwrite(&header, 1, sizeof(header), file) == sizeof(header) &&
                 fwrite(app->queue, sizeof(app->queue[0]), app->queue_count,
                        file) == app->queue_count &&
                 fflush(file) == 0;
    int close_result = fclose(file);
    if (!wrote || close_result != 0) {
        remove(temporary);
        set_error(error, error_size, "无法写入播放列表");
        return -1;
    }

    struct stat info;
    bool had_original = stat(path, &info) == 0;
    (void)remove(backup);
    if (had_original && rename(path, backup) != 0) {
        remove(temporary);
        set_error(error, error_size, "无法备份播放列表");
        return -1;
    }
    if (rename(temporary, path) != 0) {
        if (had_original) (void)rename(backup, path);
        remove(temporary);
        set_error(error, error_size, "无法提交播放列表");
        return -1;
    }
    if (had_original) (void)remove(backup);
    return 0;
}

static uLong journal_crc(const uint8_t header[JOURNAL_HEADER_SIZE],
                         const uint8_t *payload, size_t payload_size) {
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, header, 12);
    if (payload_size) crc = crc32(crc, payload, (uInt)payload_size);
    return crc;
}

static int append_record(PlaylistStore *store, JournalRecordType type,
                         const uint8_t *payload, size_t payload_size,
                         uint64_t now_ms, char *error, size_t error_size) {
    if (!store || !store->ready || payload_size > JOURNAL_MAX_PAYLOAD ||
        (payload_size && !payload)) {
        set_error(error, error_size, "播放列表日志不可用");
        return -1;
    }
    uint8_t header[JOURNAL_HEADER_SIZE] = {0};
    memcpy(header, "PLJ1", 4);
    put_u16(header + 4, (uint16_t)type);
    put_u32(header + 8, (uint32_t)payload_size);
    put_u32(header + 12, (uint32_t)journal_crc(header, payload, payload_size));

    FILE *file = fopen(store->journal_path, "ab");
    if (!file) {
        set_error(error, error_size, "无法打开播放列表日志");
        return -1;
    }
    bool wrote = fwrite(header, 1, sizeof(header), file) == sizeof(header) &&
                 (!payload_size ||
                  fwrite(payload, 1, payload_size, file) == payload_size) &&
                 fflush(file) == 0;
    int close_result = fclose(file);
    if (!wrote || close_result != 0) {
        store->ready = false;
        set_error(error, error_size, "无法写入播放列表日志");
        return -1;
    }
    store->journal_records++;
    store->journal_bytes += sizeof(header) + payload_size;
    store->last_mutation_ms = now_ms;
    return 0;
}

static int apply_add_record(AppState *app, const uint8_t *payload,
                            size_t payload_size) {
    if (payload_size != 12U + STORED_SONG_SIZE) return -1;
    size_t position = get_u32(payload);
    int64_t evicted_id = (int64_t)get_u64(payload + 4);
    Song song;
    if (decode_song(payload + 12, &song) != 0) return -1;
    int64_t selected_id = selected_song_id(app);

    int existing = song_index(app, song.id);
    if (existing >= 0) (void)playlist_remove(app, existing);
    int evicted = evicted_id > 0 ? song_index(app, evicted_id) : -1;
    if (evicted >= 0) (void)playlist_remove(app, evicted);
    if (app->queue_count >= NM3DS_MAX_QUEUE) {
        /* This occurs only when a journal already represented by the snapshot
         * survived compaction.  Later idempotent records restore final state. */
        restore_selected_song(app, selected_id);
        return 0;
    }
    if (insert_song(app, position, &song) < 0) return -1;
    restore_selected_song(app, selected_id);
    return 0;
}

static int apply_remove_record(AppState *app, const uint8_t *payload,
                               size_t payload_size) {
    if (payload_size != 8) return -1;
    int64_t song_id = (int64_t)get_u64(payload);
    if (song_id <= 0) return -1;
    int index = song_index(app, song_id);
    if (index >= 0) (void)playlist_remove(app, index);
    return 0;
}

static int apply_patch_record(AppState *app, const uint8_t *payload,
                              size_t payload_size) {
    if (payload_size != 333) return -1;
    int64_t song_id = (int64_t)get_u64(payload);
    uint32_t mask = get_u32(payload + 8);
    if (song_id <= 0 || mask == 0 ||
        (mask & ~(PATCH_COVER_URL | PATCH_FEE)) != 0)
        return -1;
    int index = song_index(app, song_id);
    if (index < 0) return 0;
    if (mask & PATCH_FEE) {
        if (!song_fee_valid(payload[12])) return -1;
        app->queue[index].fee = payload[12];
    }
    if (mask & PATCH_COVER_URL) {
        memcpy(app->queue[index].pic_url, payload + 13,
               sizeof(app->queue[index].pic_url));
        app->queue[index].pic_url[
            sizeof(app->queue[index].pic_url) - 1] = '\0';
    }
    return 0;
}

static int apply_move_record(AppState *app, const uint8_t *payload,
                             size_t payload_size) {
    if (payload_size != 12) return -1;
    int64_t song_id = (int64_t)get_u64(payload);
    size_t position = get_u32(payload + 8);
    if (song_id <= 0) return -1;
    int index = song_index(app, song_id);
    if (index < 0) return 0;
    int64_t selected_id = selected_song_id(app);
    Song song = app->queue[index];
    (void)playlist_remove(app, index);
    if (insert_song(app, position, &song) < 0) return -1;
    restore_selected_song(app, selected_id);
    return 0;
}

static int apply_state_record(AppState *app, const uint8_t *payload,
                              size_t payload_size) {
    if (payload_size != 12) return -1;
    int64_t selected_id = (int64_t)get_u64(payload);
    uint32_t mode = get_u32(payload + 8);
    if (selected_id < 0 || mode >= PLAY_MODE_COUNT) return -1;
    app->play_mode = (PlayMode)mode;
    restore_selected_song(app, selected_id);
    return 0;
}

static int apply_record(AppState *app, JournalRecordType type,
                        const uint8_t *payload, size_t payload_size) {
    switch (type) {
        case JOURNAL_ADD:
            return apply_add_record(app, payload, payload_size);
        case JOURNAL_REMOVE:
            return apply_remove_record(app, payload, payload_size);
        case JOURNAL_PATCH:
            return apply_patch_record(app, payload, payload_size);
        case JOURNAL_MOVE:
            return apply_move_record(app, payload, payload_size);
        case JOURNAL_CLEAR:
            if (payload_size != 0) return -1;
            playlist_clear(app);
            return 0;
        case JOURNAL_STATE:
            return apply_state_record(app, payload, payload_size);
    }
    return -1;
}

static int replay_journal(PlaylistStore *store, AppState *app,
                          bool *tail_invalid,
                          char *error, size_t error_size) {
    *tail_invalid = false;
    FILE *file = fopen(store->journal_path, "rb");
    if (!file) {
        if (errno == ENOENT) return 0;
        set_error(error, error_size, "无法读取播放列表日志");
        return -1;
    }
    uint8_t header[JOURNAL_HEADER_SIZE];
    uint8_t payload[JOURNAL_MAX_PAYLOAD];
    for (;;) {
        size_t header_size = fread(header, 1, sizeof(header), file);
        if (header_size == 0 && feof(file)) break;
        if (header_size != sizeof(header) || memcmp(header, "PLJ1", 4) != 0 ||
            get_u16(header + 6) != 0) {
            *tail_invalid = true;
            break;
        }
        uint16_t type = get_u16(header + 4);
        size_t payload_size = get_u32(header + 8);
        if (type < JOURNAL_ADD || type > JOURNAL_STATE ||
            payload_size > sizeof(payload) ||
            (payload_size &&
             fread(payload, 1, payload_size, file) != payload_size) ||
            (uint32_t)journal_crc(header, payload, payload_size) !=
                get_u32(header + 12) ||
            apply_record(app, (JournalRecordType)type,
                         payload, payload_size) != 0) {
            *tail_invalid = true;
            break;
        }
        store->journal_records++;
        store->journal_bytes += sizeof(header) + payload_size;
    }
    bool read_failed = ferror(file);
    fclose(file);
    if (read_failed) {
        set_error(error, error_size, "无法读取播放列表日志");
        return -1;
    }
    return 0;
}

int playlist_store_load(PlaylistStore *store, AppState *app,
                        const char *snapshot_path, const char *journal_path,
                        uint64_t now_ms, char *error, size_t error_size) {
    if (!store || !app || !snapshot_path || !journal_path) return -1;
    memset(store, 0, sizeof(*store));
    int snapshot_length = snprintf(store->snapshot_path,
                                   sizeof(store->snapshot_path), "%s",
                                   snapshot_path);
    int journal_length = snprintf(store->journal_path,
                                  sizeof(store->journal_path), "%s",
                                  journal_path);
    if (snapshot_length < 0 ||
        (size_t)snapshot_length >= sizeof(store->snapshot_path) ||
        journal_length < 0 ||
        (size_t)journal_length >= sizeof(store->journal_path)) {
        set_error(error, error_size, "播放列表路径过长");
        return -1;
    }

    int snapshot_result = playlist_load(app, snapshot_path,
                                        error, error_size);
    if (snapshot_result != 0) {
        char backup[320];
        int length = snprintf(backup, sizeof(backup), "%s.bak", snapshot_path);
        int backup_result = length < 0 || (size_t)length >= sizeof(backup) ?
                            1 : playlist_load(app, backup,
                                              error, error_size);
        if (backup_result == 0) {
            (void)remove(snapshot_path);
            (void)rename(backup, snapshot_path);
            snapshot_result = 0;
        } else if (backup_result < 0 || snapshot_result < 0) {
            return -1;
        }
    }

    store->ready = true;
    store->last_mutation_ms = now_ms;
    bool tail_invalid = false;
    if (replay_journal(store, app, &tail_invalid,
                       error, error_size) != 0) {
        store->ready = false;
        return -1;
    }
    store->persisted_selected_song_id = selected_song_id(app);
    store->persisted_play_mode = app->play_mode;
    bool journal_had_records = store->journal_records > 0;
    if (tail_invalid) {
        if (playlist_store_compact(store, app, error, error_size) != 0) {
            store->ready = false;
            return -1;
        }
    }
    return snapshot_result == 1 && !journal_had_records ? 1 : 0;
}

int playlist_store_add(PlaylistStore *store, AppState *app,
                       const Song *song, uint64_t now_ms,
                       char *error, size_t error_size) {
    if (!store || !app || !song || song->id <= 0 ||
        !song_fee_valid(song->fee))
        return -1;
    int existing = song_index(app, song->id);
    if (existing >= 0) {
        app->queue_selected = existing;
        if (song->fee != SONG_FEE_UNKNOWN &&
            app->queue[existing].fee != song->fee) {
            int patched = playlist_store_set_fee(
                store, app, song->id, song->fee, now_ms,
                error, error_size);
            if (patched < 0) return -1;
        }
        if (!app->queue[existing].pic_url[0] && song->pic_url[0]) {
            int patched = playlist_store_set_cover_url(
                store, app, song->id, song->pic_url, now_ms,
                error, error_size);
            if (patched < 0) return -1;
        }
        return existing;
    }

    int64_t evicted_id = 0;
    size_t position = app->queue_count;
    if (app->queue_count >= NM3DS_MAX_QUEUE) {
        evicted_id = app->queue[0].id;
        position--;
    }
    uint8_t payload[12U + STORED_SONG_SIZE];
    put_u32(payload, (uint32_t)position);
    put_u64(payload + 4, (uint64_t)evicted_id);
    encode_song(payload + 12, song);
    if (append_record(store, JOURNAL_ADD, payload, sizeof(payload),
                      now_ms, error, error_size) != 0)
        return -1;

    if (app->queue_count >= NM3DS_MAX_QUEUE) {
        memmove(&app->queue[0], &app->queue[1],
                (NM3DS_MAX_QUEUE - 1U) * sizeof(app->queue[0]));
        memmove(&app->queue_offline_playable[0],
                &app->queue_offline_playable[1],
                (NM3DS_MAX_QUEUE - 1U) *
                    sizeof(app->queue_offline_playable[0]));
        memmove(&app->queue_cache_known[0],
                &app->queue_cache_known[1],
                (NM3DS_MAX_QUEUE - 1U) *
                    sizeof(app->queue_cache_known[0]));
        app->queue_count--;
        if (app->current_queue > 0) app->current_queue--;
        else app->current_queue = -1;
        if (app->pending_queue > 0) app->pending_queue--;
        else app->pending_queue = -1;
        if (app->queue_selected > 0) app->queue_selected--;
        else app->queue_selected = -1;
    }
    int index = insert_song(app, app->queue_count, song);
    if (index >= 0) app->queue_selected = index;
    return index;
}

int playlist_store_add_batch(PlaylistStore *store, AppState *app,
                             const Song *songs, size_t song_count,
                             uint64_t now_ms, size_t *added,
                             size_t *existing, bool *full,
                             char *error, size_t error_size) {
    if (added) *added = 0;
    if (existing) *existing = 0;
    if (full) *full = false;
    if (!store || !app || (!songs && song_count > 0)) return -1;

    for (size_t i = 0; i < song_count; i++) {
        if (playlist_store_add_would_evict(app, &songs[i])) {
            if (full) *full = true;
            continue;
        }
        size_t previous_count = app->queue_count;
        if (playlist_store_add(store, app, &songs[i], now_ms,
                               error, error_size) < 0)
            return -1;
        if (app->queue_count > previous_count) {
            if (added) (*added)++;
        } else if (existing) {
            (*existing)++;
        }
    }
    return 0;
}

bool playlist_store_add_would_evict(const AppState *app, const Song *song) {
    return app && song && app->queue_count >= NM3DS_MAX_QUEUE &&
           song_index(app, song->id) < 0;
}

int playlist_store_remove(PlaylistStore *store, AppState *app, int index,
                          uint64_t now_ms, char *error, size_t error_size) {
    if (!store || !app || index < 0 || index >= (int)app->queue_count)
        return -1;
    uint8_t payload[8];
    put_u64(payload, (uint64_t)app->queue[index].id);
    if (append_record(store, JOURNAL_REMOVE, payload, sizeof(payload),
                      now_ms, error, error_size) != 0)
        return -1;
    return playlist_remove(app, index);
}

int playlist_store_move(PlaylistStore *store, AppState *app, int index,
                        int delta, uint64_t now_ms,
                        char *error, size_t error_size) {
    if (!store || !app || index < 0 || index >= (int)app->queue_count ||
        (delta != -1 && delta != 1))
        return -1;
    int target = index + delta;
    if (target < 0 || target >= (int)app->queue_count) return 1;
    uint8_t payload[12];
    put_u64(payload, (uint64_t)app->queue[index].id);
    put_u32(payload + 8, (uint32_t)target);
    if (append_record(store, JOURNAL_MOVE, payload, sizeof(payload),
                      now_ms, error, error_size) != 0)
        return -1;
    return playlist_move(app, index, delta);
}

int playlist_store_clear(PlaylistStore *store, AppState *app,
                         uint64_t now_ms, char *error, size_t error_size) {
    if (!store || !app) return -1;
    if (app->queue_count == 0) return 0;
    if (append_record(store, JOURNAL_CLEAR, NULL, 0,
                      now_ms, error, error_size) != 0)
        return -1;
    playlist_clear(app);
    return 0;
}

static int append_patch(PlaylistStore *store, AppState *app, int index,
                        uint32_t mask, const Song *replacement,
                        uint64_t now_ms, char *error, size_t error_size) {
    uint8_t payload[333] = {0};
    put_u64(payload, (uint64_t)app->queue[index].id);
    put_u32(payload + 8, mask);
    payload[12] = replacement->fee;
    memcpy(payload + 13, replacement->pic_url,
           sizeof(replacement->pic_url));
    if (append_record(store, JOURNAL_PATCH, payload, sizeof(payload),
                      now_ms, error, error_size) != 0)
        return -1;
    app->queue[index] = *replacement;
    return 1;
}

int playlist_store_set_cover_url(PlaylistStore *store, AppState *app,
                                 int64_t song_id, const char *pic_url,
                                 uint64_t now_ms,
                                 char *error, size_t error_size) {
    if (!store || !app || song_id <= 0 || !pic_url) return -1;
    if (!pic_url[0]) return 0;
    int index = song_index(app, song_id);
    if (index < 0 || app->queue[index].pic_url[0]) return 0;
    Song replacement = app->queue[index];
    snprintf(replacement.pic_url, sizeof(replacement.pic_url), "%s", pic_url);
    return append_patch(store, app, index, PATCH_COVER_URL, &replacement,
                        now_ms, error, error_size);
}

int playlist_store_set_fee(PlaylistStore *store, AppState *app,
                           int64_t song_id, uint8_t fee, uint64_t now_ms,
                           char *error, size_t error_size) {
    if (!store || !app || song_id <= 0 || !song_fee_valid(fee)) return -1;
    int index = song_index(app, song_id);
    if (index < 0 || app->queue[index].fee == fee) return 0;
    Song replacement = app->queue[index];
    replacement.fee = fee;
    int result = append_patch(store, app, index, PATCH_FEE, &replacement,
                              now_ms, error, error_size);
    if (result > 0) {
        app->queue_cache_known[index] = false;
        app->queue_offline_playable[index] = false;
    }
    return result;
}

int playlist_store_save_state(PlaylistStore *store, const AppState *app,
                              uint64_t now_ms,
                              char *error, size_t error_size) {
    if (!store || !app || app->play_mode >= PLAY_MODE_COUNT) return -1;
    int64_t selected_id = selected_song_id(app);
    if (selected_id == store->persisted_selected_song_id &&
        app->play_mode == store->persisted_play_mode)
        return 0;
    uint8_t payload[12];
    put_u64(payload, (uint64_t)selected_id);
    put_u32(payload + 8, (uint32_t)app->play_mode);
    if (append_record(store, JOURNAL_STATE, payload, sizeof(payload),
                      now_ms, error, error_size) != 0)
        return -1;
    store->persisted_selected_song_id = selected_id;
    store->persisted_play_mode = app->play_mode;
    return 0;
}

int playlist_store_set_play_mode(PlaylistStore *store, AppState *app,
                                 PlayMode play_mode, uint64_t now_ms,
                                 char *error, size_t error_size) {
    if (!store || !app || play_mode >= PLAY_MODE_COUNT) return -1;
    if (app->play_mode == play_mode) return 0;
    PlayMode previous = app->play_mode;
    app->play_mode = play_mode;
    if (playlist_store_save_state(store, app, now_ms,
                                  error, error_size) != 0) {
        app->play_mode = previous;
        return -1;
    }
    return 0;
}

bool playlist_store_should_compact(const PlaylistStore *store,
                                   uint64_t now_ms) {
    if (!store || !store->ready ||
        (store->journal_records < PLAYLIST_COMPACT_RECORDS &&
         store->journal_bytes < PLAYLIST_COMPACT_BYTES))
        return false;
    return now_ms >= store->last_mutation_ms &&
           now_ms - store->last_mutation_ms >= PLAYLIST_COMPACT_IDLE_MS;
}

int playlist_store_compact(PlaylistStore *store, const AppState *app,
                           char *error, size_t error_size) {
    if (!store || !store->ready || !app) return -1;
    if (playlist_save(app, store->snapshot_path,
                      error, error_size) != 0)
        return -1;
    FILE *journal = fopen(store->journal_path, "wb");
    if (!journal) {
        set_error(error, error_size, "无法清理播放列表日志");
        return -1;
    }
    int close_result = fclose(journal);
    if (close_result != 0) {
        set_error(error, error_size, "无法清理播放列表日志");
        return -1;
    }
    store->journal_records = 0;
    store->journal_bytes = 0;
    store->persisted_selected_song_id = selected_song_id(app);
    store->persisted_play_mode = app->play_mode;
    return 0;
}

bool playlist_set_cover_url(AppState *app, int64_t song_id,
                            const char *pic_url) {
    if (!app || song_id <= 0 || !pic_url || !pic_url[0]) return false;
    for (size_t i = 0; i < app->queue_count; i++) {
        Song *song = &app->queue[i];
        if (song->id != song_id || song->pic_url[0]) continue;
        snprintf(song->pic_url, sizeof(song->pic_url), "%s", pic_url);
        return true;
    }
    return false;
}

int playlist_remove(AppState *app, int index) {
    if (!app || index < 0 || index >= (int)app->queue_count) return -1;
    if (index + 1 < (int)app->queue_count) {
        memmove(&app->queue[index], &app->queue[index + 1],
                (app->queue_count - (size_t)index - 1) * sizeof(app->queue[0]));
        memmove(&app->queue_offline_playable[index],
                &app->queue_offline_playable[index + 1],
                (app->queue_count - (size_t)index - 1) *
                    sizeof(app->queue_offline_playable[0]));
        memmove(&app->queue_cache_known[index],
                &app->queue_cache_known[index + 1],
                (app->queue_count - (size_t)index - 1) *
                    sizeof(app->queue_cache_known[0]));
    }
    app->queue_count--;
    app->queue_offline_playable[app->queue_count] = false;
    app->queue_cache_known[app->queue_count] = false;
    if (app->current_queue == index) app->current_queue = -1;
    else if (app->current_queue > index) app->current_queue--;
    if (app->pending_queue == index) app->pending_queue = -1;
    else if (app->pending_queue > index) app->pending_queue--;
    if (app->queue_count == 0) app->queue_selected = -1;
    else if (app->queue_selected > index) app->queue_selected--;
    else if (app->queue_selected >= (int)app->queue_count)
        app->queue_selected = (int)app->queue_count - 1;
    return 0;
}

int playlist_move(AppState *app, int index, int delta) {
    if (!app || index < 0 || index >= (int)app->queue_count ||
        (delta != -1 && delta != 1)) return -1;
    int target = index + delta;
    if (target < 0 || target >= (int)app->queue_count) return 1;
    Song swap = app->queue[index];
    app->queue[index] = app->queue[target];
    app->queue[target] = swap;
    bool cached = app->queue_offline_playable[index];
    app->queue_offline_playable[index] =
        app->queue_offline_playable[target];
    app->queue_offline_playable[target] = cached;
    bool cache_known = app->queue_cache_known[index];
    app->queue_cache_known[index] = app->queue_cache_known[target];
    app->queue_cache_known[target] = cache_known;
    if (app->current_queue == index) app->current_queue = target;
    else if (app->current_queue == target) app->current_queue = index;
    if (app->pending_queue == index) app->pending_queue = target;
    else if (app->pending_queue == target) app->pending_queue = index;
    app->queue_selected = target;
    return 0;
}

void playlist_clear(AppState *app) {
    if (!app) return;
    memset(app->queue_offline_playable, 0,
           sizeof(app->queue_offline_playable));
    memset(app->queue_cache_known, 0, sizeof(app->queue_cache_known));
    app->queue_count = 0;
    app->queue_selected = -1;
    app->current_queue = -1;
    app->pending_queue = -1;
    app->lyric_count = 0;
    app->lyric_song_id = 0;
}
