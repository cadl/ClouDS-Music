#define _POSIX_C_SOURCE 200809L

#include "cache.h"
#include "lyric_cache.h"
#include "playlist.h"
#include "settings.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    char magic[4];
    uint32_t version;
    uint32_t count;
    int32_t selected;
    uint32_t play_mode;
    float volume;
} LegacyPlaylistHeader;

typedef struct {
    int64_t id;
    char title[128];
    char artist[96];
    char album[96];
    char pic_url[320];
} LegacySong;

typedef struct {
    int64_t id;
    char title[128];
    char artist[96];
    char album[96];
    char pic_url[320];
    uint8_t fee;
} PlaylistSongV2Fixture;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
} LegacySettingsFile;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t reserved;
} SettingsFileV2Fixture;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
} SettingsFileV3Fixture;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
    uint32_t lid_lr_skip;
} SettingsFileV4Fixture;

static void write_bytes(const char *path, size_t count) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    for (size_t i = 0; i < count; i++) fputc((int)i, file);
    assert(fclose(file) == 0);
}

static int cancel_immediately(void *userdata) {
    (void)userdata;
    return 1;
}

static Song test_song(int64_t id) {
    Song song;
    memset(&song, 0, sizeof(song));
    song.id = id;
    snprintf(song.title, sizeof(song.title), "Song %lld", (long long)id);
    snprintf(song.artist, sizeof(song.artist), "Artist %lld", (long long)id);
    return song;
}

static void remove_playlist_store_files(const char *snapshot,
                                        const char *journal) {
    char path[512];
    remove(snapshot);
    remove(journal);
    snprintf(path, sizeof(path), "%s.part", snapshot);
    remove(path);
    snprintf(path, sizeof(path), "%s.bak", snapshot);
    remove(path);
}

int main(void) {
    char error[192];
    assert(queue_cache_scan_index_for(8, 3, 0) == 3);
    assert(queue_cache_scan_index_for(8, 3, 4) == 7);
    assert(queue_cache_scan_index_for(8, 3, 5) == 0);
    assert(queue_cache_scan_index_for(8, 3, 7) == 2);
    assert(queue_cache_scan_index_for(8, 8, 0) == 8);
    assert(queue_cache_scan_index_for(0, 0, 0) == 0);
    bool cache_known[] = {true, false, true, true, false};
    size_t unknown_ordinal = 0;
    assert(queue_cache_scan_unknown_index_for(
               5, 3, 0, cache_known, &unknown_ordinal) == 4);
    assert(unknown_ordinal == 1);
    assert(queue_cache_scan_unknown_index_for(
               5, 3, unknown_ordinal + 1, cache_known,
               &unknown_ordinal) == 1);
    assert(unknown_ordinal == 3);
    assert(queue_cache_scan_unknown_index_for(
               5, 3, unknown_ordinal + 1, cache_known,
               &unknown_ordinal) == 5);
    assert(unknown_ordinal == 5);
    const char *playlist_path = "/tmp/nm3ds-playlist-test.bin";
    remove(playlist_path);

    const char *settings_path = "/tmp/nm3ds-settings-test.bin";
    remove(settings_path);
    AppSettings settings;
    settings_defaults(&settings);
    assert(settings.cache_limit == NM3DS_CACHE_LIMIT_DEFAULT);
    assert(settings.language == APP_LANGUAGE_CHINESE);
    assert(!settings.debug_logging);
    assert(!settings.lid_lr_skip);
    assert(settings_load(settings_path, &settings,
                         error, sizeof(error)) == 1);
    settings.cache_limit = 128 * NM3DS_CACHE_MIB;
    settings.language = APP_LANGUAGE_ENGLISH;
    settings.debug_logging = true;
    settings.lid_lr_skip = true;
    assert(settings_save(settings_path, &settings,
                         error, sizeof(error)) == 0);
    settings_defaults(&settings);
    assert(settings_load(settings_path, &settings,
                         error, sizeof(error)) == 0);
    assert(settings.cache_limit == 128 * NM3DS_CACHE_MIB);
    assert(settings.language == APP_LANGUAGE_ENGLISH);
    assert(settings.debug_logging);
    assert(settings.lid_lr_skip);
    assert(NM3DS_CACHE_LIMIT_OPTION_COUNT == 5);
    assert(cache_limit_option(NM3DS_CACHE_LIMIT_OPTION_COUNT - 1U) ==
           NM3DS_CACHE_LIMIT_UNLIMITED);
    assert(cache_limit_option_index(NM3DS_CACHE_LIMIT_UNLIMITED) ==
           NM3DS_CACHE_LIMIT_OPTION_COUNT - 1);
    assert(cache_limit_is_unlimited(NM3DS_CACHE_LIMIT_UNLIMITED));
    settings.cache_limit = NM3DS_CACHE_LIMIT_UNLIMITED;
    assert(settings_save(settings_path, &settings,
                         error, sizeof(error)) == 0);
    settings_defaults(&settings);
    assert(settings_load(settings_path, &settings,
                         error, sizeof(error)) == 0);
    assert(cache_limit_is_unlimited(settings.cache_limit));
    settings.cache_limit = 100 * NM3DS_CACHE_MIB;
    assert(settings_save(settings_path, &settings,
                         error, sizeof(error)) == -1);

    LegacySettingsFile legacy_settings = {
        {'S', 'E', 'T', 'T'}, 1, 512 * NM3DS_CACHE_MIB
    };
    FILE *legacy_settings_file = fopen(settings_path, "wb");
    assert(legacy_settings_file != NULL);
    assert(fwrite(&legacy_settings, 1, sizeof(legacy_settings),
                  legacy_settings_file) == sizeof(legacy_settings));
    assert(fclose(legacy_settings_file) == 0);
    settings_defaults(&settings);
    settings.language = APP_LANGUAGE_ENGLISH;
    assert(settings_load(settings_path, &settings,
                         error, sizeof(error)) == 0);
    assert(settings.cache_limit == 512 * NM3DS_CACHE_MIB);
    assert(settings.language == APP_LANGUAGE_CHINESE);
    assert(!settings.debug_logging);
    assert(!settings.lid_lr_skip);

    SettingsFileV2Fixture legacy_v2 = {
        {'S', 'E', 'T', 'T'}, 2, NM3DS_CACHE_LIMIT_DEFAULT,
        APP_LANGUAGE_ENGLISH, 1
    };
    legacy_settings_file = fopen(settings_path, "wb");
    assert(legacy_settings_file != NULL);
    assert(fwrite(&legacy_v2, 1, sizeof(legacy_v2), legacy_settings_file) ==
           sizeof(legacy_v2));
    assert(fclose(legacy_settings_file) == 0);
    settings_defaults(&settings);
    assert(settings_load(settings_path, &settings,
                         error, sizeof(error)) == 0);
    assert(settings.language == APP_LANGUAGE_ENGLISH);
    assert(!settings.debug_logging);
    assert(!settings.lid_lr_skip);

    SettingsFileV3Fixture legacy_v3 = {
        {'S', 'E', 'T', 'T'}, 3, NM3DS_CACHE_LIMIT_DEFAULT,
        APP_LANGUAGE_ENGLISH, 1
    };
    legacy_settings_file = fopen(settings_path, "wb");
    assert(legacy_settings_file != NULL);
    assert(fwrite(&legacy_v3, 1, sizeof(legacy_v3), legacy_settings_file) ==
           sizeof(legacy_v3));
    assert(fclose(legacy_settings_file) == 0);
    settings_defaults(&settings);
    assert(settings_load(settings_path, &settings,
                         error, sizeof(error)) == 0);
    assert(settings.language == APP_LANGUAGE_ENGLISH);
    assert(settings.debug_logging);
    assert(!settings.lid_lr_skip);

    SettingsFileV2Fixture invalid_settings = {
        {'S', 'E', 'T', 'T'}, 2, NM3DS_CACHE_LIMIT_DEFAULT, 99, 0
    };
    legacy_settings_file = fopen(settings_path, "wb");
    assert(legacy_settings_file != NULL);
    assert(fwrite(&invalid_settings, 1, sizeof(invalid_settings),
                  legacy_settings_file) == sizeof(invalid_settings));
    assert(fclose(legacy_settings_file) == 0);
    settings_defaults(&settings);
    assert(settings_load(settings_path, &settings,
                         error, sizeof(error)) == -1);
    assert(settings.cache_limit == NM3DS_CACHE_LIMIT_DEFAULT);
    assert(settings.language == APP_LANGUAGE_CHINESE);
    assert(!settings.debug_logging);
    assert(!settings.lid_lr_skip);

    SettingsFileV3Fixture invalid_logging = {
        {'S', 'E', 'T', 'T'}, 3, NM3DS_CACHE_LIMIT_DEFAULT,
        APP_LANGUAGE_CHINESE, 2
    };
    legacy_settings_file = fopen(settings_path, "wb");
    assert(legacy_settings_file != NULL);
    assert(fwrite(&invalid_logging, 1, sizeof(invalid_logging),
                  legacy_settings_file) == sizeof(invalid_logging));
    assert(fclose(legacy_settings_file) == 0);
    settings_defaults(&settings);
    assert(settings_load(settings_path, &settings,
                         error, sizeof(error)) == -1);
    assert(!settings.debug_logging);
    assert(!settings.lid_lr_skip);

    SettingsFileV4Fixture invalid_lid_skip = {
        {'S', 'E', 'T', 'T'}, 4, NM3DS_CACHE_LIMIT_DEFAULT,
        APP_LANGUAGE_CHINESE, 0, 2
    };
    legacy_settings_file = fopen(settings_path, "wb");
    assert(legacy_settings_file != NULL);
    assert(fwrite(&invalid_lid_skip, 1, sizeof(invalid_lid_skip),
                  legacy_settings_file) == sizeof(invalid_lid_skip));
    assert(fclose(legacy_settings_file) == 0);
    settings_defaults(&settings);
    assert(settings_load(settings_path, &settings,
                         error, sizeof(error)) == -1);
    assert(!settings.lid_lr_skip);
    remove(settings_path);
    AppState app;
    memset(&app, 0, sizeof(app));
    app.queue_count = 3;
    app.queue_selected = 1;
    app.current_queue = 1;
    app.pending_queue = 2;
    app.play_mode = PLAY_MODE_SHUFFLE;
    app.volume = 0.65f;
    for (int i = 0; i < 3; i++) {
        app.queue[i].id = 100 + i;
        snprintf(app.queue[i].title, sizeof(app.queue[i].title), "Song %d", i);
    }
    snprintf(app.queue[2].title, sizeof(app.queue[2].title),
             "\xE1\x84\x92\xE1\x85\xA1\xE1\x86\xAB글");
    snprintf(app.queue[2].artist, sizeof(app.queue[2].artist),
             "\xE1\x84\x80\xE1\x85\xA1수");
    snprintf(app.queue[2].album, sizeof(app.queue[2].album),
             "\xE1\x84\x8B\xE1\x85\xA2\xE1\x86\xAF범");
    app.queue[1].fee = SONG_FEE_VIP;
    app.queue[2].cloud_owner_user_id = 7654321;
    app.queue_offline_playable[1] = true;
    app.queue_cache_known[1] = true;
    snprintf(app.queue[1].pic_url, sizeof(app.queue[1].pic_url),
             "https://example.com/101.jpg");
    assert(playlist_save(&app, playlist_path, error, sizeof(error)) == 0);
    AppState loaded;
    memset(&loaded, 0, sizeof(loaded));
    assert(playlist_load(&loaded, playlist_path, error, sizeof(error)) == 0);
    assert(loaded.queue_count == 3);
    assert(loaded.queue[1].id == 101);
    assert(!loaded.queue_offline_playable[0]);
    assert(!loaded.queue_offline_playable[1]);
    assert(!loaded.queue_offline_playable[2]);
    assert(!loaded.queue_cache_known[0]);
    assert(!loaded.queue_cache_known[1]);
    assert(!loaded.queue_cache_known[2]);
    assert(song_is_vip(&loaded.queue[1]));
    assert(!song_offline_full_allowed(&loaded.queue[1], false));
    assert(song_offline_full_allowed(&loaded.queue[1], true));
    assert(song_offline_full_allowed(&loaded.queue[0], false));
    assert(strcmp(loaded.queue[1].pic_url,
                  "https://example.com/101.jpg") == 0);
    assert(strcmp(loaded.queue[2].title, "한글") == 0);
    assert(strcmp(loaded.queue[2].artist, "가수") == 0);
    assert(strcmp(loaded.queue[2].album, "앨범") == 0);
    assert(loaded.queue[2].cloud_owner_user_id == 7654321);
    assert(!song_cloud_access_allowed(&loaded.queue[2], false, 0));
    assert(!song_cloud_access_allowed(&loaded.queue[2], true, 1));
    assert(song_cloud_access_allowed(&loaded.queue[2], true, 7654321));
    assert(song_offline_full_allowed_for_user(
        &loaded.queue[2], true, 7654321));
    assert(playlist_set_cover_url(&loaded, 100,
                                  "https://example.com/100.jpg"));
    assert(strcmp(loaded.queue[0].pic_url,
                  "https://example.com/100.jpg") == 0);
    assert(!playlist_set_cover_url(&loaded, 100,
                                   "https://example.com/replacement.jpg"));
    assert(!playlist_set_cover_url(&loaded, 999,
                                   "https://example.com/999.jpg"));
    assert(playlist_save(&loaded, playlist_path,
                         error, sizeof(error)) == 0);
    AppState enriched;
    memset(&enriched, 0, sizeof(enriched));
    assert(playlist_load(&enriched, playlist_path,
                         error, sizeof(error)) == 0);
    assert(strcmp(enriched.queue[0].pic_url,
                  "https://example.com/100.jpg") == 0);
    assert(loaded.play_mode == PLAY_MODE_SHUFFLE);
    assert(loaded.volume > 0.64f && loaded.volume < 0.66f);
    assert(playlist_move(&app, 1, -1) == 0);
    assert(app.queue[0].id == 101 && app.current_queue == 0);
    assert(app.queue_offline_playable[0]);
    assert(!app.queue_offline_playable[1]);
    assert(app.queue_cache_known[0]);
    assert(!app.queue_cache_known[1]);
    assert(playlist_remove(&app, 0) == 0);
    assert(app.queue_count == 2 && app.current_queue == -1);
    assert(!app.queue_offline_playable[0]);
    assert(!app.queue_offline_playable[1]);
    assert(!app.queue_cache_known[0]);
    assert(!app.queue_cache_known[1]);
    playlist_clear(&app);
    assert(app.queue_count == 0 && app.queue_selected == -1);
    assert(!app.queue_offline_playable[0]);
    assert(!app.queue_cache_known[0]);

    FILE *legacy = fopen(playlist_path, "wb");
    assert(legacy != NULL);
    LegacyPlaylistHeader legacy_header = {
        {'P', 'L', 'S', 'T'}, 1, 1, 0, PLAY_MODE_SEQUENCE, 1.0f
    };
    LegacySong legacy_song;
    memset(&legacy_song, 0, sizeof(legacy_song));
    legacy_song.id = 163;
    snprintf(legacy_song.title, sizeof(legacy_song.title), "Legacy song");
    assert(fwrite(&legacy_header, 1, sizeof(legacy_header), legacy) ==
           sizeof(legacy_header));
    assert(fwrite(&legacy_song, 1, sizeof(legacy_song), legacy) ==
           sizeof(legacy_song));
    assert(fclose(legacy) == 0);
    AppState legacy_loaded;
    memset(&legacy_loaded, 0, sizeof(legacy_loaded));
    assert(playlist_load(&legacy_loaded, playlist_path,
                         error, sizeof(error)) == 0);
    assert(legacy_loaded.queue_count == 1);
    assert(legacy_loaded.queue[0].id == 163);
    assert(strcmp(legacy_loaded.queue[0].title, "Legacy song") == 0);
    assert(legacy_loaded.queue[0].fee == SONG_FEE_UNKNOWN);
    assert(!song_offline_full_allowed(&legacy_loaded.queue[0], false));

    legacy = fopen(playlist_path, "wb");
    assert(legacy != NULL);
    LegacyPlaylistHeader playlist_v2_header = {
        {'P', 'L', 'S', 'T'}, 2, 1, 0, PLAY_MODE_REPEAT_ONE, 0.5f
    };
    PlaylistSongV2Fixture playlist_v2_song;
    memset(&playlist_v2_song, 0, sizeof(playlist_v2_song));
    playlist_v2_song.id = 264;
    playlist_v2_song.fee = SONG_FEE_VIP;
    snprintf(playlist_v2_song.title, sizeof(playlist_v2_song.title),
             "Version 2 song");
    assert(fwrite(&playlist_v2_header, 1, sizeof(playlist_v2_header),
                  legacy) == sizeof(playlist_v2_header));
    assert(fwrite(&playlist_v2_song, 1, sizeof(playlist_v2_song), legacy) ==
           sizeof(playlist_v2_song));
    assert(fclose(legacy) == 0);
    AppState playlist_v2_loaded;
    memset(&playlist_v2_loaded, 0, sizeof(playlist_v2_loaded));
    assert(playlist_load(&playlist_v2_loaded, playlist_path,
                         error, sizeof(error)) == 0);
    assert(playlist_v2_loaded.queue_count == 1);
    assert(playlist_v2_loaded.queue[0].id == 264);
    assert(strcmp(playlist_v2_loaded.queue[0].title, "Version 2 song") == 0);
    assert(song_is_vip(&playlist_v2_loaded.queue[0]));
    assert(playlist_v2_loaded.queue[0].cloud_owner_user_id == 0);
    assert(playlist_v2_loaded.play_mode == PLAY_MODE_REPEAT_ONE);
    remove(playlist_path);

    const char *journal_path = "/tmp/nm3ds-playlist-test.log";
    remove_playlist_store_files(playlist_path, journal_path);
    AppState *journal_app = calloc(1, sizeof(*journal_app));
    AppState *journal_loaded = calloc(1, sizeof(*journal_loaded));
    assert(journal_app != NULL && journal_loaded != NULL);
    journal_app->queue_selected = -1;
    journal_app->current_queue = -1;
    journal_app->pending_queue = -1;
    journal_loaded->queue_selected = -1;
    journal_loaded->current_queue = -1;
    journal_loaded->pending_queue = -1;
    PlaylistStore journal_store;
    assert(playlist_store_load(&journal_store, journal_app,
                               playlist_path, journal_path, 1000,
                               error, sizeof(error)) == 1);
    Song first = test_song(1000);
    assert(playlist_store_add(&journal_store, journal_app, &first, 1100,
                              error, sizeof(error)) == 0);
    assert(journal_app->queue_count == 1);
    assert(journal_store.journal_records == 1);

    PlaylistStore loaded_store;
    assert(playlist_store_load(&loaded_store, journal_loaded,
                               playlist_path, journal_path, 1200,
                               error, sizeof(error)) == 0);
    assert(journal_loaded->queue_count == 1);
    assert(journal_loaded->queue[0].id == first.id);
    size_t records_before_empty_cover = loaded_store.journal_records;
    snprintf(error, sizeof(error), "unchanged");
    assert(playlist_store_set_cover_url(
        &loaded_store, journal_loaded, first.id, "", 1250,
        error, sizeof(error)) == 0);
    assert(loaded_store.journal_records == records_before_empty_cover);
    assert(journal_loaded->queue[0].pic_url[0] == '\0');
    assert(strcmp(error, "unchanged") == 0);
    assert(playlist_store_set_cover_url(
        &loaded_store, journal_loaded, first.id,
        "https://example.com/journal.jpg", 1300,
        error, sizeof(error)) == 1);
    journal_loaded->queue_cache_known[0] = true;
    journal_loaded->queue_offline_playable[0] = true;
    Song refreshed = first;
    refreshed.fee = SONG_FEE_VIP;
    assert(playlist_store_add(&loaded_store, journal_loaded, &refreshed,
                              1400, error, sizeof(error)) == 0);
    assert(song_is_vip(&journal_loaded->queue[0]));
    assert(!journal_loaded->queue_cache_known[0]);
    assert(!journal_loaded->queue_offline_playable[0]);
    assert(playlist_store_set_play_mode(&loaded_store, journal_loaded,
                                        PLAY_MODE_SHUFFLE, 1500,
                                        error, sizeof(error)) == 0);

    Song second = test_song(1001);
    assert(playlist_store_add(&loaded_store, journal_loaded, &second, 1600,
                              error, sizeof(error)) == 1);
    assert(playlist_store_remove(&loaded_store, journal_loaded, 0, 1700,
                                 error, sizeof(error)) == 0);
    assert(journal_loaded->queue_count == 1);
    assert(journal_loaded->queue[0].id == second.id);

    /* A crash after publishing the compacted snapshot but before clearing the
       idempotent journal must not duplicate or resurrect queue entries. */
    assert(playlist_save(journal_loaded, playlist_path,
                         error, sizeof(error)) == 0);
    memset(journal_app, 0, sizeof(*journal_app));
    journal_app->queue_selected = -1;
    PlaylistStore duplicate_store;
    assert(playlist_store_load(&duplicate_store, journal_app,
                               playlist_path, journal_path, 1800,
                               error, sizeof(error)) == 0);
    assert(journal_app->queue_count == 1);
    assert(journal_app->queue[0].id == second.id);
    assert(journal_app->play_mode == PLAY_MODE_SHUFFLE);

    Song batch[] = {second, test_song(1002), test_song(1003)};
    size_t batch_added = 0;
    size_t batch_existing = 0;
    bool batch_full = false;
    assert(playlist_store_add_batch(
        &duplicate_store, journal_app, batch,
        sizeof(batch) / sizeof(batch[0]), 1900,
        &batch_added, &batch_existing, &batch_full,
        error, sizeof(error)) == 0);
    assert(batch_added == 2);
    assert(batch_existing == 1);
    assert(!batch_full);
    assert(journal_app->queue_count == 3);

    /* A torn final record is ignored, then the valid prefix is compacted so
       future appends cannot become hidden behind the damaged tail. */
    remove_playlist_store_files(playlist_path, journal_path);
    memset(journal_app, 0, sizeof(*journal_app));
    journal_app->queue_selected = -1;
    PlaylistStore torn_store;
    assert(playlist_store_load(&torn_store, journal_app,
                               playlist_path, journal_path, 2000,
                               error, sizeof(error)) == 1);
    assert(playlist_store_add(&torn_store, journal_app, &first, 2100,
                              error, sizeof(error)) == 0);
    struct stat journal_stat;
    assert(stat(journal_path, &journal_stat) == 0);
    off_t first_record_size = journal_stat.st_size;
    assert(playlist_store_add(&torn_store, journal_app, &second, 2200,
                              error, sizeof(error)) == 1);
    assert(stat(journal_path, &journal_stat) == 0);
    assert(journal_stat.st_size > first_record_size + 4);
    assert(truncate(journal_path, journal_stat.st_size - 4) == 0);
    memset(journal_loaded, 0, sizeof(*journal_loaded));
    journal_loaded->queue_selected = -1;
    PlaylistStore repaired_store;
    assert(playlist_store_load(&repaired_store, journal_loaded,
                               playlist_path, journal_path, 2300,
                               error, sizeof(error)) == 0);
    assert(journal_loaded->queue_count == 1);
    assert(journal_loaded->queue[0].id == first.id);
    assert(repaired_store.journal_records == 0);
    assert(stat(journal_path, &journal_stat) == 0 && journal_stat.st_size == 0);

    Song corrupted = test_song(1002);
    assert(playlist_store_add(&repaired_store, journal_loaded, &corrupted,
                              2400, error, sizeof(error)) == 1);
    FILE *corrupt_file = fopen(journal_path, "rb+");
    assert(corrupt_file != NULL);
    assert(fseek(corrupt_file, -1, SEEK_END) == 0);
    int last_byte = fgetc(corrupt_file);
    assert(last_byte != EOF);
    assert(fseek(corrupt_file, -1, SEEK_END) == 0);
    assert(fputc(last_byte ^ 0x5a, corrupt_file) != EOF);
    assert(fclose(corrupt_file) == 0);
    memset(journal_app, 0, sizeof(*journal_app));
    journal_app->queue_selected = -1;
    PlaylistStore crc_store;
    assert(playlist_store_load(&crc_store, journal_app,
                               playlist_path, journal_path, 2500,
                               error, sizeof(error)) == 0);
    assert(journal_app->queue_count == 1);
    assert(journal_app->queue[0].id == first.id);
    assert(crc_store.journal_records == 0);

    /* The configured idle policy compacts after 16 records, while a full
       1000-song queue remains bounded and evicts the oldest entry on add. */
    remove_playlist_store_files(playlist_path, journal_path);
    memset(journal_app, 0, sizeof(*journal_app));
    journal_app->queue_selected = -1;
    PlaylistStore large_store;
    assert(playlist_store_load(&large_store, journal_app,
                               playlist_path, journal_path, 3000,
                               error, sizeof(error)) == 1);
    for (int i = 0; i < NM3DS_MAX_QUEUE; i++) {
        Song song = test_song(10000 + i);
        assert(playlist_store_add(&large_store, journal_app, &song,
                                  3100 + (uint64_t)i,
                                  error, sizeof(error)) == i);
    }
    assert(journal_app->queue_count == NM3DS_MAX_QUEUE);
    assert(!playlist_store_should_compact(
        &large_store, large_store.last_mutation_ms +
                      PLAYLIST_COMPACT_IDLE_MS - 1));
    assert(playlist_store_should_compact(
        &large_store, large_store.last_mutation_ms +
                      PLAYLIST_COMPACT_IDLE_MS));
    Song overflow = test_song(20000);
    assert(!playlist_store_add_would_evict(journal_app,
                                           &journal_app->queue[10]));
    assert(playlist_store_add_would_evict(journal_app, &overflow));
    journal_app->queue_cache_known[1] = true;
    journal_app->queue_offline_playable[1] = true;
    assert(playlist_store_add(&large_store, journal_app, &overflow, 5000,
                              error, sizeof(error)) ==
           NM3DS_MAX_QUEUE - 1);
    assert(journal_app->queue_count == NM3DS_MAX_QUEUE);
    assert(journal_app->queue[0].id == 10001);
    assert(journal_app->queue_cache_known[0]);
    assert(journal_app->queue_offline_playable[0]);
    assert(journal_app->queue[NM3DS_MAX_QUEUE - 1].id == overflow.id);
    assert(!journal_app->queue_cache_known[NM3DS_MAX_QUEUE - 1]);
    assert(!journal_app->queue_offline_playable[NM3DS_MAX_QUEUE - 1]);

    Song full_batch[] = {journal_app->queue[10], test_song(20001)};
    int64_t oldest_before_batch = journal_app->queue[0].id;
    batch_added = batch_existing = 0;
    batch_full = false;
    assert(playlist_store_add_batch(
        &large_store, journal_app, full_batch,
        sizeof(full_batch) / sizeof(full_batch[0]), 5050,
        &batch_added, &batch_existing, &batch_full,
        error, sizeof(error)) == 0);
    assert(batch_added == 0);
    assert(batch_existing == 1);
    assert(batch_full);
    assert(journal_app->queue_count == NM3DS_MAX_QUEUE);
    assert(journal_app->queue[0].id == oldest_before_batch);

    assert(playlist_save(journal_app, playlist_path,
                         error, sizeof(error)) == 0);
    memset(journal_loaded, 0, sizeof(*journal_loaded));
    journal_loaded->queue_selected = -1;
    PlaylistStore full_duplicate_store;
    assert(playlist_store_load(&full_duplicate_store, journal_loaded,
                               playlist_path, journal_path, 5100,
                               error, sizeof(error)) == 0);
    assert(journal_loaded->queue_count == NM3DS_MAX_QUEUE);
    assert(journal_loaded->queue[0].id == 10001);
    assert(journal_loaded->queue[NM3DS_MAX_QUEUE - 1].id == overflow.id);

    assert(playlist_store_compact(&large_store, journal_app,
                                  error, sizeof(error)) == 0);
    assert(large_store.journal_records == 0);
    assert(stat(playlist_path, &journal_stat) == 0);
    assert((size_t)journal_stat.st_size ==
           sizeof(LegacyPlaylistHeader) +
           NM3DS_MAX_QUEUE * 657U);

    memset(journal_loaded, 0, sizeof(*journal_loaded));
    journal_loaded->queue_selected = -1;
    PlaylistStore compacted_store;
    assert(playlist_store_load(&compacted_store, journal_loaded,
                               playlist_path, journal_path, 6000,
                               error, sizeof(error)) == 0);
    assert(journal_loaded->queue_count == NM3DS_MAX_QUEUE);
    assert(journal_loaded->queue[0].id == 10001);
    assert(journal_loaded->queue[NM3DS_MAX_QUEUE - 1].id == overflow.id);

    char backup_path[512];
    snprintf(backup_path, sizeof(backup_path), "%s.bak", playlist_path);
    assert(rename(playlist_path, backup_path) == 0);
    memset(journal_app, 0, sizeof(*journal_app));
    journal_app->queue_selected = -1;
    PlaylistStore backup_store;
    assert(playlist_store_load(&backup_store, journal_app,
                               playlist_path, journal_path, 6100,
                               error, sizeof(error)) == 0);
    assert(journal_app->queue_count == NM3DS_MAX_QUEUE);
    assert(access(playlist_path, F_OK) == 0);
    assert(access(backup_path, F_OK) != 0);
    free(journal_loaded);
    free(journal_app);
    remove_playlist_store_files(playlist_path, journal_path);

    const char *lyric_cache_path = "/tmp/nm3ds-lyric-cache-test.lrc";
    remove(lyric_cache_path);
    LyricLine saved_lyrics[3] = {
        {1234, "第一行歌词"},
        {65432, "Second line"},
        {70000, "A\xE1\x84\x92\xE1\x85\xA1\xE1\x86\xAB글"}
    };
    LyricLine loaded_lyrics[NM3DS_MAX_LYRICS];
    size_t loaded_lyric_count = 0;
    assert(lyric_cache_load(lyric_cache_path, loaded_lyrics,
                            NM3DS_MAX_LYRICS, &loaded_lyric_count,
                            error, sizeof(error)) == 1);
    assert(lyric_cache_save(lyric_cache_path, saved_lyrics, 3,
                            error, sizeof(error)) == 0);
    assert(access("/tmp/nm3ds-lyric-cache-test.lrc.part", F_OK) != 0);
    assert(lyric_cache_load(lyric_cache_path, loaded_lyrics,
                            NM3DS_MAX_LYRICS, &loaded_lyric_count,
                            error, sizeof(error)) == 0);
    assert(loaded_lyric_count == 3);
    assert(loaded_lyrics[0].time_ms == 1234);
    assert(strcmp(loaded_lyrics[0].text, "第一行歌词") == 0);
    assert(loaded_lyrics[1].time_ms == 65432);
    assert(strcmp(loaded_lyrics[1].text, "Second line") == 0);
    assert(loaded_lyrics[2].time_ms == 70000);
    assert(strcmp(loaded_lyrics[2].text, "A한글") == 0);
    write_bytes(lyric_cache_path, 5);
    assert(lyric_cache_load(lyric_cache_path, loaded_lyrics,
                            NM3DS_MAX_LYRICS, &loaded_lyric_count,
                            error, sizeof(error)) == -1);
    assert(loaded_lyric_count == 0);
    remove(lyric_cache_path);

    const char *root = "/tmp/nm3ds-cache-layout-v3-test";
    char data[128];
    char one_directory[sizeof(data) + 2];
    char two_directory[sizeof(data) + 2];
    snprintf(data, sizeof(data), "%s/%s", root,
             NM3DS_CACHE_DATA_DIRECTORY);
    snprintf(one_directory, sizeof(one_directory), "%s/1", data);
    snprintf(two_directory, sizeof(two_directory), "%s/2", data);
    char one[128], two[128], trial[128], typed_path[128];
    char cover[128], lyric[128];
    assert(cache_song_path(root, 1, CACHE_ASSET_AUDIO,
                           one, sizeof(one)) == 0);
    assert(cache_song_path(root, 2, CACHE_ASSET_AUDIO,
                           two, sizeof(two)) == 0);
    assert(cache_song_audio_path(root, 1, CACHE_AUDIO_TYPE_FULL,
                                 typed_path, sizeof(typed_path)) == 0);
    assert(strcmp(one, typed_path) == 0);
    assert(cache_song_audio_path(root, 1, CACHE_AUDIO_TYPE_TRIAL,
                                 trial, sizeof(trial)) == 0);
    assert(cache_song_audio_path(root, 1, CACHE_AUDIO_TYPE_UNKNOWN,
                                 typed_path, sizeof(typed_path)) == -1);
    assert(cache_song_path(root, 1, CACHE_ASSET_COVER,
                           cover, sizeof(cover)) == 0);
    assert(cache_song_path(root, 1, CACHE_ASSET_LYRIC,
                           lyric, sizeof(lyric)) == 0);
    remove(two);
    remove(one);
    remove(trial);
    remove(cover);
    remove(lyric);
    rmdir(one_directory);
    rmdir(two_directory);
    rmdir(data);
    rmdir(root);

    assert(cache_ensure_song_directory(root, 1,
                                       error, sizeof(error)) == 0);
    assert(cache_ensure_song_directory(root, 2,
                                       error, sizeof(error)) == 0);
    write_bytes(one, 10);
    write_bytes(two, 20);
    write_bytes(cover, 5);
    write_bytes(lyric, 7);
    assert(cache_song_has_asset(root, 1, CACHE_ASSET_AUDIO));
    assert(cache_song_audio_type(root, 1) == CACHE_AUDIO_TYPE_FULL);
    assert(cache_song_has_audio_type(root, 1, CACHE_AUDIO_TYPE_FULL));
    assert(cache_song_has_asset(root, 1, CACHE_ASSET_COVER));
    assert(cache_song_has_asset(root, 1, CACHE_ASSET_LYRIC));
    assert(cache_song_is_complete(root, 1));
    assert(!cache_song_is_complete(root, 2));
    write_bytes(trial, 8);
    assert(cache_song_audio_type(root, 1) == CACHE_AUDIO_TYPE_UNKNOWN);
    assert(cache_song_offline_audio_type(root, 1, true) ==
           CACHE_AUDIO_TYPE_FULL);
    assert(cache_song_offline_audio_type(root, 1, false) ==
           CACHE_AUDIO_TYPE_TRIAL);
    assert(cache_song_online_audio_type(root, 1, true) ==
           CACHE_AUDIO_TYPE_FULL);
    assert(cache_song_online_audio_type(root, 1, false) ==
           CACHE_AUDIO_TYPE_TRIAL);
    assert(cache_song_remove_other_audio_types(
        root, 1, CACHE_AUDIO_TYPE_TRIAL));
    assert(!cache_song_remove_other_audio_types(
        root, 1, CACHE_AUDIO_TYPE_TRIAL));
    assert(access(one, F_OK) != 0 && access(trial, F_OK) == 0);
    assert(!cache_song_has_audio_type(root, 1, CACHE_AUDIO_TYPE_FULL));
    assert(cache_song_audio_type(root, 1) == CACHE_AUDIO_TYPE_TRIAL);
    assert(cache_song_offline_audio_type(root, 1, true) ==
           CACHE_AUDIO_TYPE_TRIAL);
    assert(cache_song_online_audio_type(root, 1, true) ==
           CACHE_AUDIO_TYPE_UNKNOWN);
    assert(cache_song_online_audio_type(root, 1, false) ==
           CACHE_AUDIO_TYPE_TRIAL);
    write_bytes(one, 10);
    assert(cache_song_audio_type(root, 1) == CACHE_AUDIO_TYPE_UNKNOWN);
    assert(cache_song_remove_other_audio_types(
        root, 1, CACHE_AUDIO_TYPE_FULL));
    assert(!cache_song_remove_other_audio_types(
        root, 1, CACHE_AUDIO_TYPE_FULL));
    assert(access(trial, F_OK) != 0);
    assert(cache_song_audio_type(root, 1) == CACHE_AUDIO_TYPE_FULL);
    CacheStats stats;
    assert(cache_scan(root, &stats, error, sizeof(error)) == 0);
    assert(stats.bytes == 42 && stats.audio_files == 2 &&
           stats.cover_files == 1 && stats.lyric_files == 1);
    assert(cache_prune(root, NM3DS_CACHE_LIMIT_UNLIMITED, 2,
                       &stats, error, sizeof(error)) == 0);
    assert(stats.bytes == 42 && access(one, F_OK) == 0 &&
           access(two, F_OK) == 0 && access(cover, F_OK) == 0 &&
           access(lyric, F_OK) == 0);
    bool removed_any = false;
    assert(cache_prune_controlled(
               root, 20, 2, &stats, &removed_any, NULL, NULL,
               error, sizeof(error)) == 0);
    assert(removed_any);
    assert(stats.bytes == 20 && access(two, F_OK) == 0);
    assert(access(one, F_OK) != 0 && access(cover, F_OK) != 0 &&
           access(lyric, F_OK) != 0 && access(one_directory, F_OK) != 0);
    assert(cache_ensure_song_directory(root, 1,
                                       error, sizeof(error)) == 0);
    write_bytes(one, 10);
    write_bytes(cover, 5);
    write_bytes(lyric, 7);
    removed_any = false;
    assert(cache_clear_controlled(
               root, 2, &stats, &removed_any, NULL, NULL,
               error, sizeof(error)) == 0);
    assert(removed_any);
    assert(stats.bytes == 20 && stats.audio_files == 1 &&
           stats.cover_files == 0 && stats.lyric_files == 0 &&
           access(two, F_OK) == 0);
    assert(cache_scan_controlled(root, &stats, cancel_immediately, NULL,
                                 error, sizeof(error)) == -2);
    assert(cache_clear(root, -1, &stats, error, sizeof(error)) == 0);
    assert(stats.bytes == 0 && access(two_directory, F_OK) != 0);

    /* Unlimited caching can exceed the former fixed 1024-file index.  A
       later finite limit must still be able to account for and prune every
       song group without retaining one full path per asset. */
    for (int i = 0; i < 1050; i++) {
        int64_t song_id = 10000 + i;
        char path[320];
        assert(cache_ensure_song_directory(root, song_id,
                                           error, sizeof(error)) == 0);
        assert(cache_song_path(root, song_id, CACHE_ASSET_AUDIO,
                               path, sizeof(path)) == 0);
        write_bytes(path, 1);
    }
    assert(cache_scan(root, &stats, error, sizeof(error)) == 0);
    assert(stats.bytes == 1050 && stats.audio_files == 1050);
    assert(cache_prune(root, 100, -1, &stats,
                       error, sizeof(error)) == 0);
    assert(stats.bytes == 100 && stats.audio_files == 100);
    assert(cache_clear(root, -1, &stats, error, sizeof(error)) == 0);
    assert(stats.bytes == 0 && stats.audio_files == 0);
    assert(rmdir(data) == 0);
    assert(rmdir(root) == 0);

    puts("storage tests: ok");
    return 0;
}
