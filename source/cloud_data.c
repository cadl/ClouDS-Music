#include "cloud_data.h"

#include "i18n.h"
#include "json.h"
#include "song_text.h"
#include "unicode_text.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLOUD_ITEM_TOKENS 2048

typedef struct {
    int64_t owner_user_id;
    NeteaseCloudTrack *tracks;
    size_t capacity;
    size_t visited;
    size_t count;
    bool has_more;
} CloudReader;

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static void read_string(const JsonDoc *doc, int object, const char *key,
                        char *output, size_t output_size) {
    int token = json_obj_get(doc, object, key);
    if (token < 0 || json_string(doc, token, output, output_size) < 0)
        output[0] = '\0';
}

void cloud_format_from_filename(const char *filename,
                                char output[NM3DS_CLOUD_FORMAT_CAPACITY]) {
    if (!output) return;
    output[0] = '\0';
    if (!filename || !filename[0]) return;
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    const char *dot = strrchr(base, '.');
    if (!dot || !dot[1]) return;
    size_t used = 0;
    for (const unsigned char *cursor = (const unsigned char *)dot + 1;
         *cursor && used + 1 < NM3DS_CLOUD_FORMAT_CAPACITY; cursor++) {
        if (!isalnum(*cursor)) {
            output[0] = '\0';
            return;
        }
        output[used++] = (char)tolower(*cursor);
    }
    output[used] = '\0';
}

static void filename_title(const char *filename, char *output,
                           size_t output_size) {
    output[0] = '\0';
    if (!filename || !filename[0]) return;
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    (void)utf8_copy_truncated(output, output_size, base);
    char *dot = strrchr(output, '.');
    if (dot && dot != output) *dot = '\0';
}

static void read_artists(const JsonDoc *doc, int song,
                         char *output, size_t output_size) {
    int artists = json_obj_get(doc, song, "artists");
    if (artists < 0) artists = json_obj_get(doc, song, "ar");
    output[0] = '\0';
    if (artists < 0 || doc->tokens[artists].type != JSON_ARRAY) return;
    int limit = json_arr_size(doc, artists);
    if (limit > 2) limit = 2;
    for (int index = 0; index < limit; index++) {
        int artist = json_arr_get(doc, artists, index);
        char name[64];
        read_string(doc, artist, "name", name, sizeof(name));
        if (!name[0]) continue;
        size_t used = strlen(output);
        if (used + 1 >= output_size) break;
        snprintf(output + used, output_size - used, "%s%s",
                 used ? " / " : "", name);
    }
}

static uint8_t read_fee(const JsonDoc *doc, int song) {
    int64_t fee = SONG_FEE_UNKNOWN;
    int token = json_obj_get(doc, song, "fee");
    if (token < 0) {
        int privilege = json_obj_get(doc, song, "privilege");
        token = privilege >= 0 ? json_obj_get(doc, privilege, "fee") : -1;
    }
    if (token < 0 || json_i64(doc, token, &fee) != 0) return SONG_FEE_UNKNOWN;
    return fee == SONG_FEE_FREE || fee == SONG_FEE_VIP ||
           fee == SONG_FEE_ALBUM || fee == SONG_FEE_LOW_QUALITY_FREE ?
           (uint8_t)fee : SONG_FEE_UNKNOWN;
}

static int read_simple_song(const JsonDoc *doc, int song, Song *output) {
    memset(output, 0, sizeof(*output));
    if (song < 0 || doc->tokens[song].type != JSON_OBJECT ||
        json_i64(doc, json_obj_get(doc, song, "id"), &output->id) != 0 ||
        output->id <= 0)
        return -1;
    read_string(doc, song, "name", output->title, sizeof(output->title));
    read_artists(doc, song, output->artist, sizeof(output->artist));
    int album = json_obj_get(doc, song, "album");
    if (album < 0) album = json_obj_get(doc, song, "al");
    read_string(doc, album, "name", output->album, sizeof(output->album));
    read_string(doc, album, "picUrl", output->pic_url,
                sizeof(output->pic_url));
    output->fee = read_fee(doc, song);
    return 0;
}

static void fill_cloud_fallback(const JsonDoc *doc, Song *song,
                                const char *filename) {
    if (song->id <= 0)
        (void)json_i64(doc, json_obj_get(doc, 0, "songId"), &song->id);
    if (!song->title[0]) {
        read_string(doc, 0, "songName", song->title, sizeof(song->title));
        if (!song->title[0])
            filename_title(filename, song->title, sizeof(song->title));
    }
    if (!song->artist[0])
        read_string(doc, 0, "artist", song->artist, sizeof(song->artist));
    if (!song->album[0])
        read_string(doc, 0, "album", song->album, sizeof(song->album));
    if (!song->pic_url[0]) {
        read_string(doc, 0, "cover", song->pic_url, sizeof(song->pic_url));
        if (!song->pic_url[0])
            read_string(doc, 0, "picUrl", song->pic_url,
                        sizeof(song->pic_url));
    }
    if (!song->title[0])
        snprintf(song->title, sizeof(song->title), "%s",
                 i18n_text("未知歌曲"));
    if (!song->artist[0])
        snprintf(song->artist, sizeof(song->artist), "%s",
                 i18n_text("未知歌手"));
}

static int read_cloud_object(const JsonDoc *doc, void *userdata) {
    CloudReader *reader = (CloudReader *)userdata;
    /* Offsets are counted in API rows, not successfully parsed songs.  The
       extra requested row only proves that another page exists; consuming it
       here would duplicate it when the next request advances by capacity. */
    if (reader->visited++ >= reader->capacity) {
        reader->has_more = true;
        return 1;
    }
    NeteaseCloudTrack track;
    memset(&track, 0, sizeof(track));
    char filename[320];
    read_string(doc, 0, "fileName", filename, sizeof(filename));
    int simple = json_obj_get(doc, 0, "simpleSong");
    (void)read_simple_song(doc, simple, &track.song);
    fill_cloud_fallback(doc, &track.song, filename);
    if (track.song.id <= 0) return 0;
    track.song.cloud_owner_user_id = reader->owner_user_id;
    cloud_format_from_filename(filename, track.format);
    int64_t value = 0;
    if (json_i64(doc, json_obj_get(doc, 0, "fileSize"), &value) == 0 &&
        value > 0)
        track.file_size = (uint64_t)value;
    value = 0;
    if (json_i64(doc, json_obj_get(doc, 0, "bitrate"), &value) == 0 &&
        value > 0)
        track.bitrate = value > UINT32_MAX ? UINT32_MAX : (uint32_t)value;
    song_text_compose_hangul_nfc(&track.song);
    reader->tracks[reader->count++] = track;
    return 0;
}

int cloud_parse_response(char *json, int64_t owner_user_id,
                         NeteaseCloudTrack *tracks, size_t capacity,
                         size_t *count, bool *has_more,
                         char *error, size_t error_size) {
    if (!json || owner_user_id <= 0 || !tracks || capacity == 0 ||
        !count || !has_more) {
        set_error(error, error_size, "云盘请求无效");
        return -1;
    }
    *count = 0;
    *has_more = false;
    JsonToken *tokens = (JsonToken *)calloc(CLOUD_ITEM_TOKENS,
                                             sizeof(*tokens));
    if (!tokens) {
        set_error(error, error_size, "内存不足");
        return -1;
    }
    CloudReader reader = {
        .owner_user_id = owner_user_id,
        .tracks = tracks,
        .capacity = capacity,
    };
    int result = json_visit_array_objects(
        json, "data", tokens, CLOUD_ITEM_TOKENS,
        read_cloud_object, &reader);
    free(tokens);
    *count = reader.count;
    *has_more = reader.has_more;
    if (result == JSON_VISIT_TOKENS_EXHAUSTED) {
        set_error(error, error_size, "云盘歌曲项目的 JSON 过大");
        return -1;
    }
    if (result == JSON_VISIT_NOT_FOUND) {
        set_error(error, error_size, "云盘列表不可用");
        return -1;
    }
    if (result < 0) {
        set_error(error, error_size, "云盘列表的 JSON 无效");
        return -1;
    }
    return 0;
}
