#include "song_index.h"

#include "i18n.h"
#include "song_text.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SONG_INDEX_VERSION 1U
#define SONG_INDEX_HEADER_SIZE 32U
#define SONG_INDEX_RECORD_SIZE 649U

static const uint8_t song_index_magic[8] = {
    'C', 'D', 'S', 'S', 'O', 'N', 'G', '1'
};

struct SongIndexWriter {
    FILE *file;
    char path[320];
    char part_path[328];
    char backup_path[328];
    int64_t source_id;
    size_t song_count;
    bool committed;
};

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

static int write_header(FILE *file, int64_t source_id, uint32_t count) {
    uint8_t header[SONG_INDEX_HEADER_SIZE] = {0};
    memcpy(header, song_index_magic, sizeof(song_index_magic));
    put_u32_le(header + 8, SONG_INDEX_VERSION);
    put_u32_le(header + 12, SONG_INDEX_HEADER_SIZE);
    put_u64_le(header + 16, (uint64_t)source_id);
    put_u32_le(header + 24, count);
    put_u32_le(header + 28, SONG_INDEX_RECORD_SIZE);
    return fwrite(header, 1, sizeof(header), file) == sizeof(header) ? 0 : -1;
}

static void encode_song(uint8_t output[SONG_INDEX_RECORD_SIZE],
                        const Song *song) {
    memset(output, 0, SONG_INDEX_RECORD_SIZE);
    put_u64_le(output, (uint64_t)song->id);
    snprintf((char *)output + 8, sizeof(song->title), "%s", song->title);
    snprintf((char *)output + 136, sizeof(song->artist), "%s", song->artist);
    snprintf((char *)output + 232, sizeof(song->album), "%s", song->album);
    snprintf((char *)output + 328, sizeof(song->pic_url), "%s", song->pic_url);
    output[648] = song->fee;
}

static int decode_song(const uint8_t input[SONG_INDEX_RECORD_SIZE],
                       Song *song) {
    memset(song, 0, sizeof(*song));
    song->id = (int64_t)get_u64_le(input);
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

SongIndexWriter *song_index_writer_create(
    const char *path, int64_t source_id,
    char *error, size_t error_size) {
    if (!path || !path[0] || source_id <= 0) {
        set_error(error, error_size, "缓存路径无效");
        return NULL;
    }
    SongIndexWriter *writer = (SongIndexWriter *)calloc(1, sizeof(*writer));
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
    writer->source_id = source_id;
    if (access(writer->path, F_OK) != 0 &&
        access(writer->backup_path, F_OK) == 0)
        (void)rename(writer->backup_path, writer->path);
    else if (access(writer->path, F_OK) == 0)
        (void)remove(writer->backup_path);
    (void)remove(writer->part_path);
    writer->file = fopen(writer->part_path, "wb");
    if (!writer->file || write_header(writer->file, source_id, 0) != 0) {
        if (writer->file) fclose(writer->file);
        (void)remove(writer->part_path);
        free(writer);
        set_error(error, error_size, "无法创建缓存文件");
        return NULL;
    }
    return writer;
}

int song_index_writer_append(SongIndexWriter *writer, const Song *song,
                             char *error, size_t error_size) {
    if (!writer || !writer->file || writer->committed || !song ||
        song->id <= 0 || !song_fee_valid(song->fee)) {
        set_error(error, error_size, "专辑歌曲列表不可用");
        return -1;
    }
    if (writer->song_count >= SONG_INDEX_MAX_RECORDS) {
        set_error(error, error_size, "专辑歌曲数量过多");
        return -1;
    }
    uint8_t encoded[SONG_INDEX_RECORD_SIZE];
    encode_song(encoded, song);
    if (fwrite(encoded, 1, sizeof(encoded), writer->file) != sizeof(encoded)) {
        set_error(error, error_size, "无法写入媒体缓存");
        return -1;
    }
    writer->song_count++;
    return 0;
}

int song_index_writer_commit(SongIndexWriter *writer, size_t *song_count,
                             char *error, size_t error_size) {
    if (!writer || !writer->file || writer->committed ||
        writer->song_count == 0 || writer->song_count > UINT32_MAX) {
        set_error(error, error_size, "专辑歌曲列表不可用");
        return -1;
    }
    bool success = fseek(writer->file, 0, SEEK_SET) == 0 &&
                   write_header(writer->file, writer->source_id,
                                (uint32_t)writer->song_count) == 0 &&
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
    if (song_count) *song_count = writer->song_count;
    return 0;
}

void song_index_writer_destroy(SongIndexWriter *writer) {
    if (!writer) return;
    if (writer->file) fclose(writer->file);
    if (!writer->committed) (void)remove(writer->part_path);
    free(writer);
}

int song_index_read_page(
    const char *path, int64_t source_id, size_t offset,
    Song *songs, size_t capacity, size_t *count, bool *has_more,
    size_t *total_count, char *error, size_t error_size) {
    if (!path || !path[0] || source_id <= 0 || !songs || capacity == 0 ||
        !count || !has_more) {
        set_error(error, error_size, "缓存路径无效");
        return -1;
    }
    *count = 0;
    *has_more = false;
    if (total_count) *total_count = 0;
    FILE *file = fopen(path, "rb");
    if (!file) return 1;
    uint8_t header[SONG_INDEX_HEADER_SIZE];
    bool valid = fread(header, 1, sizeof(header), file) == sizeof(header) &&
        memcmp(header, song_index_magic, sizeof(song_index_magic)) == 0 &&
        get_u32_le(header + 8) == SONG_INDEX_VERSION &&
        get_u32_le(header + 12) == SONG_INDEX_HEADER_SIZE &&
        get_u64_le(header + 16) == (uint64_t)source_id &&
        get_u32_le(header + 28) == SONG_INDEX_RECORD_SIZE;
    uint32_t total = valid ? get_u32_le(header + 24) : 0;
    if (valid && (total == 0 || total > SONG_INDEX_MAX_RECORDS)) valid = false;
    if (valid && fseek(file, 0, SEEK_END) == 0) {
        long size = ftell(file);
        uint64_t expected = SONG_INDEX_HEADER_SIZE +
                            (uint64_t)total * SONG_INDEX_RECORD_SIZE;
        if (size < 0 || (uint64_t)size != expected) valid = false;
    } else valid = false;
    if (!valid) {
        fclose(file);
        set_error(error, error_size, "专辑歌曲列表不可用");
        return -1;
    }
    if (total_count) *total_count = total;
    if (offset < total) {
        size_t available = (size_t)total - offset;
        size_t wanted = available < capacity ? available : capacity;
        uint64_t position = SONG_INDEX_HEADER_SIZE +
                            (uint64_t)offset * SONG_INDEX_RECORD_SIZE;
        if (position > LONG_MAX || fseek(file, (long)position, SEEK_SET) != 0) {
            fclose(file);
            set_error(error, error_size, "专辑歌曲列表不可用");
            return -1;
        }
        for (size_t i = 0; i < wanted; i++) {
            uint8_t encoded[SONG_INDEX_RECORD_SIZE];
            if (fread(encoded, 1, sizeof(encoded), file) != sizeof(encoded) ||
                decode_song(encoded, &songs[*count]) != 0) {
                fclose(file);
                *count = 0;
                set_error(error, error_size, "专辑歌曲列表不可用");
                return -1;
            }
            (*count)++;
        }
        *has_more = offset + *count < total;
    }
    if (fclose(file) != 0) {
        *count = 0;
        set_error(error, error_size, "专辑歌曲列表不可用");
        return -1;
    }
    return 0;
}
