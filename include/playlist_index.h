#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PLAYLIST_TRACK_INDEX_MAX_TRACKS 50000U

typedef struct PlaylistTrackIndexWriter PlaylistTrackIndexWriter;

PlaylistTrackIndexWriter *playlist_track_index_writer_create(
    const char *path, int64_t playlist_id,
    char *error, size_t error_size);
PlaylistTrackIndexWriter *playlist_track_index_writer_create_for_array(
    const char *path, int64_t source_id, const char *array_key,
    char *error, size_t error_size);
int playlist_track_index_writer_reset(PlaylistTrackIndexWriter *writer,
                                      char *error, size_t error_size);
int playlist_track_index_writer_write(PlaylistTrackIndexWriter *writer,
                                      const uint8_t *data, size_t size,
                                      char *error, size_t error_size);
int playlist_track_index_writer_commit(PlaylistTrackIndexWriter *writer,
                                       size_t *track_count,
                                       char *error, size_t error_size);
void playlist_track_index_writer_destroy(PlaylistTrackIndexWriter *writer);

int playlist_track_index_read_page(
    const char *path, int64_t playlist_id, size_t offset,
    int64_t *ids, size_t capacity, size_t *count, bool *has_more,
    size_t *total_count, char *error, size_t error_size);
