#pragma once

#include "model.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void cloud_format_from_filename(const char *filename,
                                char output[NM3DS_CLOUD_FORMAT_CAPACITY]);

int cloud_parse_response(char *json, int64_t owner_user_id,
                         NeteaseCloudTrack *tracks, size_t capacity,
                         size_t *count, bool *has_more,
                         char *error, size_t error_size);
