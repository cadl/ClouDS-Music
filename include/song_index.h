#pragma once

#include "model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SONG_INDEX_MAX_RECORDS 5000U

typedef struct SongIndexWriter SongIndexWriter;

SongIndexWriter *song_index_writer_create(
    const char *path, int64_t source_id,
    char *error, size_t error_size);
int song_index_writer_append(SongIndexWriter *writer, const Song *song,
                             char *error, size_t error_size);
int song_index_writer_commit(SongIndexWriter *writer, size_t *song_count,
                             char *error, size_t error_size);
void song_index_writer_destroy(SongIndexWriter *writer);

int song_index_read_page(
    const char *path, int64_t source_id, size_t offset,
    Song *songs, size_t capacity, size_t *count, bool *has_more,
    size_t *total_count, char *error, size_t error_size);
