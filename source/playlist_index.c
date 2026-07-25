#include "playlist_index.h"

#include "i18n.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PLAYLIST_INDEX_VERSION 1U
#define PLAYLIST_INDEX_HEADER_SIZE 32U

static const uint8_t playlist_index_magic[8] = {
    'C', 'D', 'S', 'T', 'R', 'K', '1', '\0'
};

typedef enum {
    PLAYLIST_SCAN_SEARCH = 0,
    PLAYLIST_SCAN_SEARCH_STRING,
    PLAYLIST_SCAN_AFTER_KEY,
    PLAYLIST_SCAN_AFTER_COLON,
    PLAYLIST_SCAN_TRACK_IDS,
    PLAYLIST_SCAN_DONE
} PlaylistScanState;

struct PlaylistTrackIndexWriter {
    FILE *file;
    char path[320];
    char part_path[328];
    char backup_path[328];
    int64_t playlist_id;
    char array_key[16];
    size_t array_key_length;
    size_t track_count;
    PlaylistScanState state;
    size_t match;
    bool candidate;
    bool escaped;
    bool in_track_string;
    bool expect_id_colon;
    bool expect_id_value;
    bool parsing_id;
    bool id_negative;
    bool id_digits;
    bool id_overflow;
    uint64_t id_value;
    int array_depth;
    int object_depth;
    int64_t object_id;
    bool object_has_id;
    bool committed;
};

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static bool whitespace(uint8_t c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void put_u32_le(uint8_t output[4], uint32_t value) {
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8);
    output[2] = (uint8_t)(value >> 16);
    output[3] = (uint8_t)(value >> 24);
}

static void put_u64_le(uint8_t output[8], uint64_t value) {
    for (unsigned int i = 0; i < 8; i++)
        output[i] = (uint8_t)(value >> (i * 8U));
}

static uint32_t get_u32_le(const uint8_t input[4]) {
    return (uint32_t)input[0] |
           (uint32_t)input[1] << 8 |
           (uint32_t)input[2] << 16 |
           (uint32_t)input[3] << 24;
}

static uint64_t get_u64_le(const uint8_t input[8]) {
    uint64_t value = 0;
    for (unsigned int i = 0; i < 8; i++)
        value |= (uint64_t)input[i] << (i * 8U);
    return value;
}

static int write_header(FILE *file, int64_t playlist_id, uint32_t count) {
    uint8_t header[PLAYLIST_INDEX_HEADER_SIZE] = {0};
    memcpy(header, playlist_index_magic, sizeof(playlist_index_magic));
    put_u32_le(header + 8, PLAYLIST_INDEX_VERSION);
    put_u32_le(header + 12, PLAYLIST_INDEX_HEADER_SIZE);
    put_u64_le(header + 16, (uint64_t)playlist_id);
    put_u32_le(header + 24, count);
    return fwrite(header, 1, sizeof(header), file) == sizeof(header) ? 0 : -1;
}

static int writer_open_part(PlaylistTrackIndexWriter *writer,
                            char *error, size_t error_size) {
    writer->file = fopen(writer->part_path, "wb");
    if (!writer->file || write_header(writer->file, writer->playlist_id, 0) != 0) {
        if (writer->file) fclose(writer->file);
        writer->file = NULL;
        (void)remove(writer->part_path);
        set_error(error, error_size, "无法创建缓存文件");
        return -1;
    }
    writer->track_count = 0;
    writer->state = PLAYLIST_SCAN_SEARCH;
    writer->match = 0;
    writer->candidate = false;
    writer->escaped = false;
    writer->in_track_string = false;
    writer->expect_id_colon = false;
    writer->expect_id_value = false;
    writer->parsing_id = false;
    writer->id_negative = false;
    writer->id_digits = false;
    writer->id_overflow = false;
    writer->id_value = 0;
    writer->array_depth = 0;
    writer->object_depth = 0;
    writer->object_id = 0;
    writer->object_has_id = false;
    writer->committed = false;
    return 0;
}

PlaylistTrackIndexWriter *playlist_track_index_writer_create(
    const char *path, int64_t playlist_id,
    char *error, size_t error_size) {
    return playlist_track_index_writer_create_for_array(
        path, playlist_id, "trackIds", error, error_size);
}

PlaylistTrackIndexWriter *playlist_track_index_writer_create_for_array(
    const char *path, int64_t source_id, const char *array_key,
    char *error, size_t error_size) {
    if (!path || !path[0] || source_id <= 0 || !array_key ||
        !array_key[0] || strlen(array_key) >=
            sizeof(((PlaylistTrackIndexWriter *)0)->array_key)) {
        set_error(error, error_size, "缓存路径无效");
        return NULL;
    }
    PlaylistTrackIndexWriter *writer =
        (PlaylistTrackIndexWriter *)calloc(1, sizeof(*writer));
    if (!writer) {
        set_error(error, error_size, "内存不足");
        return NULL;
    }
    int path_written = snprintf(writer->path, sizeof(writer->path), "%s", path);
    int part_written = snprintf(writer->part_path, sizeof(writer->part_path),
                                "%s.part", path);
    int backup_written = snprintf(writer->backup_path,
                                  sizeof(writer->backup_path), "%s.bak", path);
    if (path_written < 0 || (size_t)path_written >= sizeof(writer->path) ||
        part_written < 0 || (size_t)part_written >= sizeof(writer->part_path) ||
        backup_written < 0 ||
        (size_t)backup_written >= sizeof(writer->backup_path)) {
        free(writer);
        set_error(error, error_size, "缓存路径无效");
        return NULL;
    }
    writer->playlist_id = source_id;
    snprintf(writer->array_key, sizeof(writer->array_key), "%s", array_key);
    writer->array_key_length = strlen(writer->array_key);
    if (access(writer->path, F_OK) != 0 &&
        access(writer->backup_path, F_OK) == 0)
        (void)rename(writer->backup_path, writer->path);
    else if (access(writer->path, F_OK) == 0)
        (void)remove(writer->backup_path);
    (void)remove(writer->part_path);
    if (writer_open_part(writer, error, error_size) != 0) {
        free(writer);
        return NULL;
    }
    return writer;
}

int playlist_track_index_writer_reset(PlaylistTrackIndexWriter *writer,
                                      char *error, size_t error_size) {
    if (!writer) {
        set_error(error, error_size, "缓存路径无效");
        return -1;
    }
    if (writer->file) (void)fclose(writer->file);
    writer->file = NULL;
    (void)remove(writer->part_path);
    return writer_open_part(writer, error, error_size);
}

static int writer_write_id(PlaylistTrackIndexWriter *writer, int64_t id,
                           char *error, size_t error_size) {
    if (id <= 0 || writer->track_count >= PLAYLIST_TRACK_INDEX_MAX_TRACKS) {
        set_error(error, error_size, writer->track_count >=
                  PLAYLIST_TRACK_INDEX_MAX_TRACKS ?
                  "JSON 响应过大" : "歌单歌曲 ID 不可用");
        return -1;
    }
    uint8_t encoded[8];
    put_u64_le(encoded, (uint64_t)id);
    if (fwrite(encoded, 1, sizeof(encoded), writer->file) != sizeof(encoded)) {
        set_error(error, error_size, "无法写入媒体缓存");
        return -1;
    }
    writer->track_count++;
    return 0;
}

static void begin_search_string(PlaylistTrackIndexWriter *writer) {
    writer->state = PLAYLIST_SCAN_SEARCH_STRING;
    writer->match = 0;
    writer->candidate = true;
    writer->escaped = false;
}

static int scan_search(PlaylistTrackIndexWriter *writer, uint8_t c) {
    if (writer->state == PLAYLIST_SCAN_SEARCH) {
        if (c == '"') begin_search_string(writer);
        return 0;
    }
    if (writer->state == PLAYLIST_SCAN_SEARCH_STRING) {
        if (writer->escaped) {
            writer->escaped = false;
            writer->candidate = false;
            return 0;
        }
        if (c == '\\') {
            writer->escaped = true;
            writer->candidate = false;
            return 0;
        }
        if (c == '"') {
            bool matched = writer->candidate &&
                           writer->match == writer->array_key_length;
            writer->state = matched ? PLAYLIST_SCAN_AFTER_KEY :
                                      PLAYLIST_SCAN_SEARCH;
            return 0;
        }
        if (!writer->candidate ||
            writer->match >= writer->array_key_length ||
            c != (uint8_t)writer->array_key[writer->match])
            writer->candidate = false;
        else writer->match++;
        return 0;
    }
    if (writer->state == PLAYLIST_SCAN_AFTER_KEY) {
        if (whitespace(c)) return 0;
        if (c == ':') {
            writer->state = PLAYLIST_SCAN_AFTER_COLON;
            return 0;
        }
        writer->state = PLAYLIST_SCAN_SEARCH;
        if (c == '"') begin_search_string(writer);
        return 0;
    }
    if (writer->state == PLAYLIST_SCAN_AFTER_COLON) {
        if (whitespace(c)) return 0;
        if (c == '[') {
            writer->state = PLAYLIST_SCAN_TRACK_IDS;
            writer->array_depth = 1;
            return 0;
        }
        writer->state = PLAYLIST_SCAN_SEARCH;
        if (c == '"') begin_search_string(writer);
    }
    return 0;
}

static void begin_track_string(PlaylistTrackIndexWriter *writer) {
    writer->state = PLAYLIST_SCAN_TRACK_IDS;
    writer->escaped = false;
    writer->in_track_string = true;
    writer->candidate =
        writer->array_depth == 1 && writer->object_depth == 1;
    writer->match = 0;
}

static int finish_id_number(PlaylistTrackIndexWriter *writer) {
    if (!writer->id_digits || writer->id_negative || writer->id_overflow ||
        writer->id_value == 0 || writer->id_value > INT64_MAX)
        return -1;
    writer->object_id = (int64_t)writer->id_value;
    writer->object_has_id = true;
    writer->parsing_id = false;
    writer->expect_id_value = false;
    return 0;
}

static int scan_track_ids(PlaylistTrackIndexWriter *writer, uint8_t c,
                          char *error, size_t error_size) {
    static const char id_key[] = "id";
    if (writer->in_track_string) {
        if (writer->escaped) {
            writer->escaped = false;
            writer->candidate = false;
            return 0;
        }
        if (c == '\\') {
            writer->escaped = true;
            writer->candidate = false;
            return 0;
        }
        if (c == '"') {
            writer->expect_id_colon = writer->candidate &&
                writer->match == sizeof(id_key) - 1;
            writer->in_track_string = false;
            return 0;
        }
        if (!writer->candidate || writer->match >= sizeof(id_key) - 1 ||
            c != (uint8_t)id_key[writer->match])
            writer->candidate = false;
        else writer->match++;
        return 0;
    }
    if (writer->parsing_id) {
        if (c >= '0' && c <= '9') {
            unsigned int digit = (unsigned int)(c - '0');
            writer->id_digits = true;
            if (writer->id_value > (UINT64_MAX - digit) / 10U)
                writer->id_overflow = true;
            else writer->id_value = writer->id_value * 10U + digit;
            return 0;
        }
        if (finish_id_number(writer) != 0) {
            set_error(error, error_size, "歌单歌曲 ID 不可用");
            return -1;
        }
    }
    if (writer->expect_id_colon) {
        if (whitespace(c)) return 0;
        writer->expect_id_colon = false;
        if (c == ':') {
            writer->expect_id_value = true;
            return 0;
        }
    }
    if (writer->expect_id_value) {
        if (whitespace(c)) return 0;
        writer->id_negative = c == '-';
        writer->id_digits = false;
        writer->id_overflow = false;
        writer->id_value = 0;
        writer->parsing_id = true;
        if (c == '-') return 0;
        if (c >= '0' && c <= '9') {
            writer->id_digits = true;
            writer->id_value = (uint64_t)(c - '0');
            return 0;
        }
        writer->parsing_id = false;
        writer->expect_id_value = false;
    }
    if (c == '"') {
        begin_track_string(writer);
        return 0;
    }
    if (c == '[') {
        writer->array_depth++;
        return 0;
    }
    if (c == ']') {
        if (writer->array_depth <= 0) goto invalid;
        if (writer->array_depth == 1) {
            if (writer->object_depth != 0) goto invalid;
            writer->array_depth = 0;
            writer->state = PLAYLIST_SCAN_DONE;
        } else writer->array_depth--;
        return 0;
    }
    if (c == '{') {
        if (writer->array_depth == 1 && writer->object_depth == 0) {
            writer->object_id = 0;
            writer->object_has_id = false;
        }
        writer->object_depth++;
        return 0;
    }
    if (c == '}') {
        if (writer->object_depth <= 0) goto invalid;
        writer->object_depth--;
        if (writer->array_depth == 1 && writer->object_depth == 0) {
            if (!writer->object_has_id ||
                writer_write_id(writer, writer->object_id,
                                error, error_size) != 0)
                return -1;
        }
    }
    return 0;

invalid:
    set_error(error, error_size, "歌单歌曲 ID 不可用");
    return -1;
}

int playlist_track_index_writer_write(PlaylistTrackIndexWriter *writer,
                                      const uint8_t *data, size_t size,
                                      char *error, size_t error_size) {
    if (!writer || !writer->file || (!data && size != 0) || writer->committed) {
        set_error(error, error_size, "歌单歌曲 ID 不可用");
        return -1;
    }
    for (size_t i = 0; i < size; i++) {
        if (writer->state < PLAYLIST_SCAN_TRACK_IDS) {
            if (scan_search(writer, data[i]) != 0) return -1;
        } else if (writer->state == PLAYLIST_SCAN_TRACK_IDS) {
            if (scan_track_ids(writer, data[i], error, error_size) != 0)
                return -1;
        }
    }
    return 0;
}

int playlist_track_index_writer_commit(PlaylistTrackIndexWriter *writer,
                                       size_t *track_count,
                                       char *error, size_t error_size) {
    if (!writer || !writer->file || writer->committed ||
        writer->state != PLAYLIST_SCAN_DONE || writer->parsing_id ||
        writer->object_depth != 0 || writer->array_depth != 0) {
        set_error(error, error_size, "歌单歌曲 ID 不可用");
        return -1;
    }
    bool success = writer->track_count <= UINT32_MAX &&
                   fseek(writer->file, 0, SEEK_SET) == 0 &&
                   write_header(writer->file, writer->playlist_id,
                                (uint32_t)writer->track_count) == 0 &&
                   fflush(writer->file) == 0;
    if (fclose(writer->file) != 0) success = false;
    writer->file = NULL;
    if (!success) {
        (void)remove(writer->part_path);
        set_error(error, error_size, "无法写入媒体缓存");
        return -1;
    }
    bool had_original = access(writer->path, F_OK) == 0;
    (void)remove(writer->backup_path);
    if (had_original && rename(writer->path, writer->backup_path) != 0) {
        (void)remove(writer->part_path);
        set_error(error, error_size, "无法提交缓存文件");
        return -1;
    }
    if (rename(writer->part_path, writer->path) != 0) {
        if (had_original) (void)rename(writer->backup_path, writer->path);
        (void)remove(writer->part_path);
        set_error(error, error_size, "无法提交缓存文件");
        return -1;
    }
    if (had_original) (void)remove(writer->backup_path);
    writer->committed = true;
    if (track_count) *track_count = writer->track_count;
    return 0;
}

void playlist_track_index_writer_destroy(PlaylistTrackIndexWriter *writer) {
    if (!writer) return;
    if (writer->file) fclose(writer->file);
    if (!writer->committed) (void)remove(writer->part_path);
    free(writer);
}

int playlist_track_index_read_page(
    const char *path, int64_t playlist_id, size_t offset,
    int64_t *ids, size_t capacity, size_t *count, bool *has_more,
    size_t *total_count, char *error, size_t error_size) {
    if (!path || !path[0] || playlist_id <= 0 || !ids || capacity == 0 ||
        !count || !has_more) {
        set_error(error, error_size, "缓存路径无效");
        return -1;
    }
    *count = 0;
    *has_more = false;
    if (total_count) *total_count = 0;
    FILE *file = fopen(path, "rb");
    if (!file) return 1;
    uint8_t header[PLAYLIST_INDEX_HEADER_SIZE];
    bool valid = fread(header, 1, sizeof(header), file) == sizeof(header) &&
        memcmp(header, playlist_index_magic, sizeof(playlist_index_magic)) == 0 &&
        get_u32_le(header + 8) == PLAYLIST_INDEX_VERSION &&
        get_u32_le(header + 12) == PLAYLIST_INDEX_HEADER_SIZE &&
        get_u64_le(header + 16) == (uint64_t)playlist_id;
    uint32_t total = valid ? get_u32_le(header + 24) : 0;
    if (valid && total > PLAYLIST_TRACK_INDEX_MAX_TRACKS) valid = false;
    if (valid && fseek(file, 0, SEEK_END) == 0) {
        long size = ftell(file);
        uint64_t expected = PLAYLIST_INDEX_HEADER_SIZE + (uint64_t)total * 8U;
        if (size < 0 || (uint64_t)size != expected) valid = false;
    } else valid = false;
    if (!valid) {
        fclose(file);
        set_error(error, error_size, "歌单歌曲 ID 不可用");
        return -1;
    }
    if (total_count) *total_count = total;
    if (offset < total) {
        size_t available = (size_t)total - offset;
        size_t wanted = available < capacity ? available : capacity;
        uint64_t position = PLAYLIST_INDEX_HEADER_SIZE + (uint64_t)offset * 8U;
        if (position > LONG_MAX || fseek(file, (long)position, SEEK_SET) != 0) {
            fclose(file);
            set_error(error, error_size, "歌单歌曲 ID 不可用");
            return -1;
        }
        for (size_t i = 0; i < wanted; i++) {
            uint8_t encoded[8];
            uint64_t value = 0;
            if (fread(encoded, 1, sizeof(encoded), file) != sizeof(encoded) ||
                (value = get_u64_le(encoded)) == 0 || value > INT64_MAX) {
                fclose(file);
                *count = 0;
                set_error(error, error_size, "歌单歌曲 ID 不可用");
                return -1;
            }
            ids[(*count)++] = (int64_t)value;
        }
        *has_more = offset + *count < total;
    }
    if (fclose(file) != 0) {
        *count = 0;
        set_error(error, error_size, "歌单歌曲 ID 不可用");
        return -1;
    }
    return 0;
}
