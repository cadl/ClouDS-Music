#pragma once

#include "model.h"

#include <stddef.h>

int lyric_cache_load(const char *path, LyricLine *lines, size_t capacity,
                     size_t *count, char *error, size_t error_size);
int lyric_cache_save(const char *path, const LyricLine *lines, size_t count,
                     char *error, size_t error_size);
