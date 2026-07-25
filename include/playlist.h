#pragma once

#include "model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PLAYLIST_COMPACT_RECORDS 16U
#define PLAYLIST_COMPACT_IDLE_MS 1000U
#define PLAYLIST_COMPACT_BYTES (64U * 1024U)

typedef struct {
    char snapshot_path[320];
    char journal_path[320];
    size_t journal_records;
    size_t journal_bytes;
    uint64_t last_mutation_ms;
    int64_t persisted_selected_song_id;
    PlayMode persisted_play_mode;
    bool ready;
} PlaylistStore;

int playlist_load(AppState *app, const char *path,
                  char *error, size_t error_size);
int playlist_save(const AppState *app, const char *path,
                  char *error, size_t error_size);

int playlist_store_load(PlaylistStore *store, AppState *app,
                        const char *snapshot_path, const char *journal_path,
                        uint64_t now_ms, char *error, size_t error_size);
int playlist_store_add(PlaylistStore *store, AppState *app,
                       const Song *song, uint64_t now_ms,
                       char *error, size_t error_size);
int playlist_store_add_batch(PlaylistStore *store, AppState *app,
                             const Song *songs, size_t song_count,
                             uint64_t now_ms, size_t *added,
                             size_t *existing, bool *full,
                             char *error, size_t error_size);
bool playlist_store_add_would_evict(const AppState *app, const Song *song);
int playlist_store_remove(PlaylistStore *store, AppState *app, int index,
                          uint64_t now_ms, char *error, size_t error_size);
int playlist_store_move(PlaylistStore *store, AppState *app, int index,
                        int delta, uint64_t now_ms,
                        char *error, size_t error_size);
int playlist_store_clear(PlaylistStore *store, AppState *app,
                         uint64_t now_ms, char *error, size_t error_size);
int playlist_store_set_cover_url(PlaylistStore *store, AppState *app,
                                 int64_t song_id, const char *pic_url,
                                 uint64_t now_ms,
                                 char *error, size_t error_size);
int playlist_store_set_fee(PlaylistStore *store, AppState *app,
                           int64_t song_id, uint8_t fee, uint64_t now_ms,
                           char *error, size_t error_size);
int playlist_store_set_play_mode(PlaylistStore *store, AppState *app,
                                 PlayMode play_mode, uint64_t now_ms,
                                 char *error, size_t error_size);
int playlist_store_save_state(PlaylistStore *store, const AppState *app,
                              uint64_t now_ms,
                              char *error, size_t error_size);
bool playlist_store_should_compact(const PlaylistStore *store,
                                   uint64_t now_ms);
int playlist_store_compact(PlaylistStore *store, const AppState *app,
                           char *error, size_t error_size);

bool playlist_set_cover_url(AppState *app, int64_t song_id,
                            const char *pic_url);
int playlist_remove(AppState *app, int index);
int playlist_move(AppState *app, int index, int delta);
void playlist_clear(AppState *app);
