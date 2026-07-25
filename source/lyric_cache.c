#include "lyric_cache.h"

#include "i18n.h"
#include "unicode_text.h"

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LYRIC_CACHE_HEADER "# ClouDS-Music lyric cache v1\n"
#define LYRIC_CACHE_LINE_CAPACITY 192U

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static size_t bounded_length(const char *text, size_t capacity) {
    size_t length = 0;
    if (!text) return capacity;
    while (length < capacity && text[length]) length++;
    return length;
}

static int parse_cached_line(char *line, LyricLine *lyric) {
    if (!line || !lyric || line[0] != '[') return -1;
    char *cursor = line + 1;
    char *end = NULL;
    errno = 0;
    unsigned long minutes = strtoul(cursor, &end, 10);
    if (errno || end == cursor || *end != ':') return -1;
    cursor = end + 1;
    errno = 0;
    unsigned long seconds = strtoul(cursor, &end, 10);
    if (errno || end == cursor || *end != '.' || seconds >= 60) return -1;
    cursor = end + 1;
    if (cursor[0] < '0' || cursor[0] > '9' ||
        cursor[1] < '0' || cursor[1] > '9' ||
        cursor[2] < '0' || cursor[2] > '9' || cursor[3] != ']')
        return -1;
    unsigned long millis = (unsigned long)(cursor[0] - '0') * 100UL +
                           (unsigned long)(cursor[1] - '0') * 10UL +
                           (unsigned long)(cursor[2] - '0');
    char *text = cursor + 4;
    (void)utf8_compose_hangul_nfc(text);
    size_t text_length = strlen(text);
    if (text_length == 0 || text_length >= sizeof(lyric->text)) return -1;
    uint64_t total = ((uint64_t)minutes * 60ULL + seconds) * 1000ULL + millis;
    if (total > UINT32_MAX) return -1;
    lyric->time_ms = (uint32_t)total;
    memcpy(lyric->text, text, text_length + 1);
    return 0;
}

int lyric_cache_load(const char *path, LyricLine *lines, size_t capacity,
                     size_t *count, char *error, size_t error_size) {
    if (!path || !lines || capacity == 0 || !count) {
        set_error(error, error_size, "无效的歌词缓存请求");
        return -1;
    }
    *count = 0;
    errno = 0;
    FILE *file = fopen(path, "rb");
    if (!file) {
        if (errno == ENOENT) return 1;
        set_error(error, error_size, "无法打开歌词缓存");
        return -1;
    }

    char line[LYRIC_CACHE_LINE_CAPACITY];
    if (!fgets(line, sizeof(line), file) ||
        strcmp(line, LYRIC_CACHE_HEADER) != 0) {
        fclose(file);
        set_error(error, error_size, "歌词缓存头无效");
        return -1;
    }
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        bool complete = length > 0 && line[length - 1] == '\n';
        if (complete) line[--length] = '\0';
        if (length > 0 && line[length - 1] == '\r') line[--length] = '\0';
        if (!complete && !feof(file)) {
            fclose(file);
            set_error(error, error_size, "歌词缓存行过长");
            *count = 0;
            return -1;
        }
        if (*count >= capacity ||
            parse_cached_line(line, &lines[*count]) != 0) {
            fclose(file);
            set_error(error, error_size, "歌词缓存项目无效");
            *count = 0;
            return -1;
        }
        (*count)++;
    }
    bool read_failed = ferror(file) != 0;
    int close_result = fclose(file);
    if (read_failed || close_result != 0 || *count == 0) {
        set_error(error, error_size, "无法读取歌词缓存");
        *count = 0;
        return -1;
    }
    return 0;
}

int lyric_cache_save(const char *path, const LyricLine *lines, size_t count,
                     char *error, size_t error_size) {
    if (!path || !lines || count == 0 || count > NM3DS_MAX_LYRICS) {
        set_error(error, error_size, "待缓存的歌词无效");
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        size_t length = bounded_length(lines[i].text, sizeof(lines[i].text));
        if (length == 0 || length >= sizeof(lines[i].text) ||
            strchr(lines[i].text, '\n') || strchr(lines[i].text, '\r')) {
            set_error(error, error_size, "歌词缓存项目无效");
            return -1;
        }
    }

    char temporary[320];
    int written = snprintf(temporary, sizeof(temporary), "%s.part", path);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        set_error(error, error_size, "歌词缓存路径过长");
        return -1;
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        set_error(error, error_size, "无法创建歌词缓存");
        return -1;
    }
    bool success = fputs(LYRIC_CACHE_HEADER, file) >= 0;
    for (size_t i = 0; success && i < count; i++) {
        uint32_t total_seconds = lines[i].time_ms / 1000U;
        unsigned long minutes = (unsigned long)(total_seconds / 60U);
        unsigned int seconds = total_seconds % 60U;
        unsigned int millis = lines[i].time_ms % 1000U;
        success = fprintf(file, "[%lu:%02u.%03u]%s\n", minutes, seconds,
                          millis, lines[i].text) >= 0;
    }
    if (success) success = fflush(file) == 0;
    if (fclose(file) != 0) success = false;
    if (!success) {
        remove(temporary);
        set_error(error, error_size, "无法写入歌词缓存");
        return -1;
    }
    remove(path);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        set_error(error, error_size, "无法提交歌词缓存");
        return -1;
    }
    return 0;
}
