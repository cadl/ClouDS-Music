#include "netease.h"

#include "cloud_data.h"
#include "eapi.h"
#include "i18n.h"
#include "json.h"
#include "net.h"
#include "playlist_index.h"
#include "song_index.h"
#include "song_text.h"
#include "storage_paths.h"
#include "unicode_text.h"
#include "weapi.h"

#include <3ds.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define EAPI_BASE "https://interface.music.163.com/eapi/"
#define WEAPI_BASE "https://music.163.com/weapi/"
#define WEAPI_REFERER "https://music.163.com/"
#define JSON_TOKENS 8192
#define RECOMMEND_SONG_TOKENS 1024
#define ALBUM_SONG_TOKENS 1024
#define ARTIST_ITEM_TOKENS 1024
#define USER_PLAYLIST_TOKENS 2048
#define DISCOVER_URL "https://music.163.com/api/personalized/newsong"
#define EAPI_HEADER_CAPACITY (NETEASE_MUSIC_U_CAPACITY + 512U)
#define EAPI_PAYLOAD_CAPACITY 4096U
#define PLAYLIST_DETAIL_MAX_WIRE_BYTES (16U * 1024U * 1024U)
#define PLAYLIST_DETAIL_MAX_JSON_BYTES (16U * 1024U * 1024U)

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static NeteaseFailure net_failure(NetErrorKind failure) {
    if (failure == NET_ERROR_CANCELLED) return NETEASE_FAILURE_CANCELLED;
    if (failure == NET_ERROR_TLS_VERIFY)
        return NETEASE_FAILURE_TLS_VERIFY;
    if (failure == NET_ERROR_TRANSPORT) return NETEASE_FAILURE_TRANSPORT;
    if (failure == NET_ERROR_AUTH) return NETEASE_FAILURE_AUTH_INVALID;
    return failure == NET_ERROR_NONE ? NETEASE_FAILURE_NONE :
                                       NETEASE_FAILURE_OTHER;
}

static int json_escape(const char *input, char *output, size_t output_size) {
    size_t used = 0;
    for (const unsigned char *p = (const unsigned char *)input; *p; p++) {
        const char *escape = NULL;
        if (*p == '"') escape = "\\\"";
        else if (*p == '\\') escape = "\\\\";
        else if (*p == '\n') escape = "\\n";
        else if (*p == '\r') escape = "\\r";
        else if (*p == '\t') escape = "\\t";
        if (escape) {
            size_t len = strlen(escape);
            if (used + len + 1 > output_size) return -1;
            memcpy(output + used, escape, len);
            used += len;
        } else if (*p < 0x20) {
            if (used + 7 > output_size) return -1;
            snprintf(output + used, 7, "\\u%04x", *p);
            used += 6;
        } else {
            if (used + 2 > output_size) return -1;
            output[used++] = (char)*p;
        }
    }
    output[used] = '\0';
    return 0;
}

static int append_header(const NeteaseClient *client, char *output, size_t size) {
    unsigned long long milliseconds = (unsigned long long)osGetTime();
    int used = snprintf(output, size,
        "\"e_r\":true,\"header\":{"
        "\"osver\":\"16.2\",\"deviceId\":\"%s\","
        "\"os\":\"iPhone OS\",\"appver\":\"9.0.90\","
        "\"versioncode\":\"140\",\"mobilename\":\"Nintendo 3DS\","
        "\"buildver\":\"%lu\",\"resolution\":\"400x240\","
        "\"channel\":\"distribution\",\"requestId\":\"%llu_%04llu\"",
        client->device_id, (unsigned long)time(NULL), milliseconds,
        milliseconds % 10000ULL);
    if (used < 0 || (size_t)used >= size) return -1;
    int tail = client->music_u[0] ?
        snprintf(output + used, size - (size_t)used,
                 ",\"MUSIC_U\":\"%s\"}", client->music_u) :
        snprintf(output + used, size - (size_t)used, "}");
    return tail < 0 || (size_t)tail >= size - (size_t)used ? -1 : used + tail;
}

static int api_request_ex(NeteaseClient *client, const char *api_path,
                          const char *route, const char *payload,
                          char **response_json,
                          char *response_cookie, size_t cookie_size,
                          char *error, size_t error_size) {
    char *form = NULL;
    if (eapi_build_form(api_path, payload, &form, error, error_size) != 0)
        return -1;
    char endpoint[256];
    snprintf(endpoint, sizeof(endpoint), "%s%s", EAPI_BASE, route);
    uint8_t *body = NULL;
    size_t body_size = 0;
    NetErrorKind failure = NET_ERROR_NONE;
    int result = net_post_form_controlled_ex(
        endpoint, form, client->cookie, &body, &body_size,
        response_cookie, cookie_size, client->cancel, client->cancel_userdata,
        &failure,
        error, error_size);
    free(form);
    if (result != 0) {
        client->last_failure = net_failure(failure);
        return -1;
    }
    result = eapi_decode_response(body, body_size, response_json,
                                  error, error_size);
    if (result != 0 || *response_json != (char *)body) free(body);
    return result;
}

static int api_request(NeteaseClient *client, const char *api_path,
                       const char *route, const char *payload,
                       char **response_json, char *error, size_t error_size) {
    return api_request_ex(client, api_path, route, payload, response_json,
                          NULL, 0, error, error_size);
}

typedef struct {
    PlaylistTrackIndexWriter *writer;
    EapiStreamDecoder *decoder;
    char *error;
    size_t error_size;
} PlaylistIndexStream;

static int playlist_index_decoded_write(const uint8_t *data, size_t size,
                                        void *userdata) {
    PlaylistIndexStream *stream = (PlaylistIndexStream *)userdata;
    return playlist_track_index_writer_write(
        stream->writer, data, size, stream->error, stream->error_size);
}

static int playlist_index_wire_write(const uint8_t *data, size_t size,
                                     void *userdata) {
    PlaylistIndexStream *stream = (PlaylistIndexStream *)userdata;
    return eapi_stream_decoder_feed(
        stream->decoder, data, size, stream->error, stream->error_size);
}

static int playlist_index_stream_reset(void *userdata) {
    PlaylistIndexStream *stream = (PlaylistIndexStream *)userdata;
    if (!stream || !stream->writer || !stream->decoder) return -1;
    if (stream->error && stream->error_size) stream->error[0] = '\0';
    if (playlist_track_index_writer_reset(
            stream->writer, stream->error, stream->error_size) != 0)
        return -1;
    return eapi_stream_decoder_reset(
        stream->decoder, stream->error, stream->error_size);
}

static int api_request_playlist_index(
    NeteaseClient *client, int64_t playlist_id, const char *payload,
    size_t *track_count, char *error, size_t error_size) {
    char *form = NULL;
    if (eapi_build_form("/api/v6/playlist/detail", payload,
                        &form, error, error_size) != 0)
        return -1;
    PlaylistTrackIndexWriter *writer = playlist_track_index_writer_create(
        PLAYLIST_TRACK_INDEX_PATH, playlist_id, error, error_size);
    if (!writer) {
        free(form);
        return -1;
    }
    PlaylistIndexStream stream = {
        .writer = writer,
        .error = error,
        .error_size = error_size,
    };
    stream.decoder = eapi_stream_decoder_create(
        PLAYLIST_DETAIL_MAX_JSON_BYTES, playlist_index_decoded_write, &stream,
        error, error_size);
    if (!stream.decoder) {
        playlist_track_index_writer_destroy(writer);
        free(form);
        return -1;
    }
    NetErrorKind failure = NET_ERROR_NONE;
    int result = net_post_form_stream_controlled_ex(
        EAPI_BASE "v6/playlist/detail", form, client->cookie,
        PLAYLIST_DETAIL_MAX_WIRE_BYTES,
        playlist_index_wire_write, playlist_index_stream_reset, &stream,
        client->cancel, client->cancel_userdata, &failure,
        error, error_size);
    free(form);
    if (result == 0 && client->cancel &&
        client->cancel(client->cancel_userdata)) {
        failure = NET_ERROR_CANCELLED;
        set_error(error, error_size, "请求已取消");
        result = -1;
    }
    if (result == 0)
        result = eapi_stream_decoder_finish(
            stream.decoder, error, error_size);
    if (result == 0 && client->cancel &&
        client->cancel(client->cancel_userdata)) {
        failure = NET_ERROR_CANCELLED;
        set_error(error, error_size, "请求已取消");
        result = -1;
    }
    if (result == 0)
        result = playlist_track_index_writer_commit(
            writer, track_count, error, error_size);
    eapi_stream_decoder_destroy(stream.decoder);
    playlist_track_index_writer_destroy(writer);
    if (result != 0)
        client->last_failure = failure == NET_ERROR_NONE ?
                               NETEASE_FAILURE_OTHER : net_failure(failure);
    return result;
}

static int weapi_secret(unsigned char secret[WEAPI_SECRET_LENGTH],
                        char *error, size_t error_size) {
    static const unsigned char alphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    Result result = psInit();
    if (R_FAILED(result)) {
        set_error(error, error_size, "安全随机数初始化失败：%08lX",
                  (unsigned long)result);
        return -1;
    }
    size_t used = 0;
    unsigned char random[32];
    while (used < WEAPI_SECRET_LENGTH) {
        result = PS_GenerateRandomBytes(random, sizeof(random));
        if (R_FAILED(result)) break;
        for (size_t i = 0; i < sizeof(random) && used < WEAPI_SECRET_LENGTH;
             i++) {
            if (random[i] < 248U)
                secret[used++] = alphabet[random[i] % 62U];
        }
    }
    memset(random, 0, sizeof(random));
    psExit();
    if (R_FAILED(result)) {
        memset(secret, 0, WEAPI_SECRET_LENGTH);
        set_error(error, error_size, "安全随机数生成失败：%08lX",
                  (unsigned long)result);
        return -1;
    }
    return 0;
}

static int weapi_request(NeteaseClient *client, const char *route,
                         const char *payload, char **response_json,
                         char *error, size_t error_size) {
    unsigned char secret[WEAPI_SECRET_LENGTH];
    if (weapi_secret(secret, error, error_size) != 0) return -1;
    char *form = NULL;
    int result = weapi_build_form(payload, secret, &form, error, error_size);
    memset(secret, 0, sizeof(secret));
    if (result != 0) return -1;
    char endpoint[256];
    int length = snprintf(endpoint, sizeof(endpoint), "%s%s",
                          WEAPI_BASE, route);
    if (length < 0 || (size_t)length >= sizeof(endpoint)) {
        free(form);
        set_error(error, error_size, "WEAPI 端点地址过长");
        return -1;
    }
    uint8_t *body = NULL;
    size_t body_size = 0;
    NetErrorKind failure = NET_ERROR_NONE;
    result = net_post_form_referer_controlled_ex(
        endpoint, form, client->cookie, WEAPI_REFERER,
        &body, &body_size, client->cancel, client->cancel_userdata,
        &failure,
        error, error_size);
    free(form);
    if (result != 0) {
        client->last_failure = net_failure(failure);
        return -1;
    }
    size_t first = 0;
    while (first < body_size &&
           (body[first] == ' ' || body[first] == '\r' ||
            body[first] == '\n' || body[first] == '\t')) first++;
    if (first == body_size || (body[first] != '{' && body[first] != '[')) {
        free(body);
        set_error(error, error_size, "WEAPI 返回了非 JSON 数据");
        return -1;
    }
    if (first) {
        body_size -= first;
        memmove(body, body + first, body_size);
        body[body_size] = '\0';
    }
    *response_json = (char *)body;
    return 0;
}

void netease_init(NeteaseClient *client) {
    if (!client) return;
    memset(client, 0, sizeof(*client));
    unsigned long long now = (unsigned long long)osGetTime();
    unsigned long long tick = (unsigned long long)svcGetSystemTick();
    snprintf(client->device_id, sizeof(client->device_id),
             "%016llx%016llx", now, tick);
    (void)netease_set_music_u(client, "");
}

void netease_set_cancel(NeteaseClient *client, NetCancel cancel,
                        void *userdata) {
    if (!client) return;
    client->cancel = cancel;
    client->cancel_userdata = userdata;
}

void netease_reset_failure(NeteaseClient *client) {
    if (client) client->last_failure = NETEASE_FAILURE_NONE;
}

NeteaseFailure netease_last_failure(const NeteaseClient *client) {
    return client ? client->last_failure : NETEASE_FAILURE_OTHER;
}

static int parse_doc(const char *json, JsonDoc *doc, JsonToken **storage,
                     char *error, size_t error_size) {
    *storage = (JsonToken *)calloc(JSON_TOKENS, sizeof(JsonToken));
    if (!*storage) {
        set_error(error, error_size, "内存不足");
        return -1;
    }
    int count = json_parse(doc, json, *storage, JSON_TOKENS);
    if (count < 0) {
        free(*storage);
        *storage = NULL;
        set_error(error, error_size, count == -1 ?
                  "JSON 响应过大" : "JSON 响应无效");
        return -1;
    }
    return 0;
}

static void response_error(const JsonDoc *doc, char *error, size_t error_size) {
    int message = json_obj_get(doc, 0, "message");
    if (message >= 0 && json_string(doc, message, error, error_size) >= 0)
        return;
    int64_t code = 0;
    if (json_i64(doc, json_obj_get(doc, 0, "code"), &code) == 0)
        set_error(error, error_size, "网易云 API 错误 %lld", (long long)code);
    else set_error(error, error_size, "网易云响应异常");
}

static void read_string(const JsonDoc *doc, int object, const char *key,
                        char *output, size_t output_size, const char *fallback) {
    int token = json_obj_get(doc, object, key);
    if (token < 0 || json_string(doc, token, output, output_size) < 0)
        snprintf(output, output_size, "%s", i18n_text(fallback));
}

static bool json_token_equals(const JsonDoc *doc, int token,
                              const char *value) {
    if (!doc || !value || token < 0 || token >= doc->count) return false;
    const JsonToken *item = &doc->tokens[token];
    size_t length = (size_t)(item->end - item->start);
    return strlen(value) == length &&
           memcmp(doc->text + item->start, value, length) == 0;
}

static bool valid_song_fee(int64_t fee) {
    return fee == SONG_FEE_FREE || fee == SONG_FEE_VIP ||
           fee == SONG_FEE_ALBUM || fee == SONG_FEE_LOW_QUALITY_FREE;
}

static uint8_t read_song_fee(const JsonDoc *doc, int item, int object) {
    int token = json_obj_get(doc, object, "fee");
    if (token < 0 && item != object) token = json_obj_get(doc, item, "fee");
    if (token < 0) {
        int privilege = json_obj_get(doc, object, "privilege");
        if (privilege < 0 && item != object)
            privilege = json_obj_get(doc, item, "privilege");
        token = privilege >= 0 ? json_obj_get(doc, privilege, "fee") : -1;
    }
    int64_t fee = SONG_FEE_UNKNOWN;
    return token >= 0 && json_i64(doc, token, &fee) == 0 &&
           valid_song_fee(fee) ? (uint8_t)fee : SONG_FEE_UNKNOWN;
}

static void read_artists(const JsonDoc *doc, int song,
                         char *output, size_t output_size) {
    int artists = json_obj_get(doc, song, "artists");
    if (artists < 0) artists = json_obj_get(doc, song, "ar");
    output[0] = '\0';
    if (artists < 0 || doc->tokens[artists].type != JSON_ARRAY) {
        snprintf(output, output_size, "%s", i18n_text("未知歌手"));
        return;
    }
    int limit = json_arr_size(doc, artists);
    if (limit > 2) limit = 2;
    for (int i = 0; i < limit; i++) {
        int artist = json_arr_get(doc, artists, i);
        char name[64];
        read_string(doc, artist, "name", name, sizeof(name), "");
        if (!name[0]) continue;
        size_t used = strlen(output);
        snprintf(output + used, output_size - used, "%s%s",
                 used ? " / " : "", name);
    }
    if (!output[0])
        snprintf(output, output_size, "%s", i18n_text("未知歌手"));
}

static void read_artist_refs(const JsonDoc *doc, int song,
                             NeteaseArtist *output, size_t capacity,
                             size_t *count) {
    if (count) *count = 0;
    if (!doc || song < 0 || !output || capacity == 0 || !count) return;
    int artists = json_obj_get(doc, song, "artists");
    if (artists < 0) artists = json_obj_get(doc, song, "ar");
    if (artists < 0 || doc->tokens[artists].type != JSON_ARRAY) return;
    int available = json_arr_size(doc, artists);
    for (int i = 0; i < available && *count < capacity; i++) {
        int item = json_arr_get(doc, artists, i);
        NeteaseArtist artist;
        memset(&artist, 0, sizeof(artist));
        if (json_i64(doc, json_obj_get(doc, item, "id"), &artist.id) != 0 ||
            artist.id <= 0)
            continue;
        bool duplicate = false;
        for (size_t existing = 0; existing < *count; existing++)
            if (output[existing].id == artist.id) duplicate = true;
        if (duplicate) continue;
        read_string(doc, item, "name", artist.name, sizeof(artist.name),
                    "未知歌手");
        (void)utf8_compose_hangul_nfc(artist.name);
        output[(*count)++] = artist;
    }
}

static int read_song_with_album_id(const JsonDoc *doc, int item, Song *song,
                                   int64_t *album_id) {
    if (!doc || item < 0 || !song) return -1;
    int wrapped = json_obj_get(doc, item, "song");
    int object = wrapped >= 0 ? wrapped : item;
    memset(song, 0, sizeof(*song));
    if (json_i64(doc, json_obj_get(doc, object, "id"), &song->id) != 0)
        return -1;
    read_string(doc, object, "name", song->title, sizeof(song->title),
                "未知歌曲");
    read_artists(doc, object, song->artist, sizeof(song->artist));
    int album = json_obj_get(doc, object, "album");
    if (album < 0) album = json_obj_get(doc, object, "al");
    if (album_id) {
        *album_id = 0;
        (void)json_i64(doc, json_obj_get(doc, album, "id"), album_id);
        if (*album_id < 0) *album_id = 0;
    }
    read_string(doc, album, "name", song->album, sizeof(song->album), "");
    read_string(doc, item, "picUrl", song->pic_url,
                sizeof(song->pic_url), "");
    if (!song->pic_url[0])
        read_string(doc, album, "picUrl", song->pic_url,
                    sizeof(song->pic_url), "");
    song->fee = read_song_fee(doc, item, object);
    song_text_compose_hangul_nfc(song);
    return 0;
}

static int read_song(const JsonDoc *doc, int item, Song *song) {
    return read_song_with_album_id(doc, item, song, NULL);
}

typedef struct {
    Song *songs;
    size_t capacity;
    size_t offset;
    size_t visited;
    size_t count;
    size_t total_count;
    bool has_more;
    bool scan_to_end;
} SongArrayReader;

static int read_song_object(const JsonDoc *doc, void *userdata) {
    SongArrayReader *reader = (SongArrayReader *)userdata;
    size_t index = reader->visited++;
    bool in_page = index >= reader->offset &&
                   index - reader->offset < reader->capacity;
    if (index >= reader->offset && !in_page)
        reader->has_more = true;
    if (in_page && read_song(doc, 0, &reader->songs[reader->count]) == 0) {
        reader->total_count++;
        reader->count++;
    } else if (!in_page) {
        int wrapped = json_obj_get(doc, 0, "song");
        int object = wrapped >= 0 ? wrapped : 0;
        int64_t id = 0;
        if (json_i64(doc, json_obj_get(doc, object, "id"), &id) == 0)
            reader->total_count++;
    }
    if (reader->has_more && !reader->scan_to_end) return 1;
    return 0;
}

static int read_recommendation_array(char *json, size_t offset,
                                     Song *songs, size_t capacity,
                                     size_t *count, bool *has_more,
                                     size_t *total_count,
                                     char *error, size_t error_size) {
    JsonToken *tokens = (JsonToken *)calloc(
        RECOMMEND_SONG_TOKENS, sizeof(JsonToken));
    if (!tokens) {
        set_error(error, error_size, "内存不足");
        return -1;
    }
    SongArrayReader reader = {
        .songs = songs,
        .capacity = capacity,
        .offset = offset,
        .scan_to_end = true,
    };
    int result = json_visit_array_objects(
        json, "dailySongs", tokens, RECOMMEND_SONG_TOKENS,
        read_song_object, &reader);
    if (result == JSON_VISIT_NOT_FOUND)
        result = json_visit_array_objects(
            json, "recommend", tokens, RECOMMEND_SONG_TOKENS,
            read_song_object, &reader);
    free(tokens);
    *count = reader.count;
    *has_more = reader.has_more;
    *total_count = reader.total_count;
    if (result == JSON_VISIT_TOKENS_EXHAUSTED) {
        set_error(error, error_size, "推荐项目的 JSON 过大");
        return -1;
    }
    if (result == JSON_VISIT_NOT_FOUND) {
        JsonToken error_tokens[64];
        JsonDoc error_doc;
        if (json_parse(&error_doc, json, error_tokens, 64) > 0)
            response_error(&error_doc, error, error_size);
        else
            set_error(error, error_size,
                      "每日推荐列表不可用");
        return -1;
    }
    if (result < 0) {
        set_error(error, error_size, "每日推荐的 JSON 无效");
        return -1;
    }
    if (*count == 0) {
        set_error(error, error_size, "没有可用的每日推荐");
        return -1;
    }
    return 0;
}

static int request_song_details(NeteaseClient *client, const int64_t *ids,
                                size_t id_count, Song *songs,
                                int64_t *album_ids,
                                NeteaseArtist *artists,
                                size_t artist_capacity,
                                size_t *artist_count,
                                size_t capacity, size_t *count,
                                char *error, size_t error_size) {
    if (!client || !ids || id_count == 0 ||
        id_count > NM3DS_LIBRARY_PAGE || !songs || capacity == 0 || !count) {
        set_error(error, error_size, "歌曲详情请求无效");
        return -1;
    }
    *count = 0;
    if (artist_count) *artist_count = 0;
    char escaped_ids[640];
    char plain_ids[256];
    size_t escaped_used = 0;
    size_t plain_used = 0;
    escaped_ids[escaped_used++] = '[';
    plain_ids[plain_used++] = '[';
    for (size_t i = 0; i < id_count; i++) {
        if (ids[i] <= 0) {
            set_error(error, error_size, "歌曲 ID 无效");
            return -1;
        }
        int wrote = snprintf(escaped_ids + escaped_used,
                             sizeof(escaped_ids) - escaped_used,
                             "%s{\\\"id\\\":%lld}", i ? "," : "",
                             (long long)ids[i]);
        if (wrote < 0 || (size_t)wrote >= sizeof(escaped_ids) - escaped_used)
            goto request_too_large;
        escaped_used += (size_t)wrote;
        wrote = snprintf(plain_ids + plain_used,
                         sizeof(plain_ids) - plain_used,
                         "%s%lld", i ? "," : "", (long long)ids[i]);
        if (wrote < 0 || (size_t)wrote >= sizeof(plain_ids) - plain_used)
            goto request_too_large;
        plain_used += (size_t)wrote;
    }
    if (escaped_used + 2 > sizeof(escaped_ids) ||
        plain_used + 2 > sizeof(plain_ids)) goto request_too_large;
    escaped_ids[escaped_used++] = ']';
    escaped_ids[escaped_used] = '\0';
    plain_ids[plain_used++] = ']';
    plain_ids[plain_used] = '\0';

    char header[EAPI_HEADER_CAPACITY];
    if (append_header(client, header, sizeof(header)) < 0) {
        set_error(error, error_size, "无法构建请求头");
        return -1;
    }
    char payload[EAPI_PAYLOAD_CAPACITY];
    int length = snprintf(payload, sizeof(payload),
        "{\"c\":\"%s\",\"ids\":\"%s\",%s}",
        escaped_ids, plain_ids, header);
    if (length < 0 || (size_t)length >= sizeof(payload))
        goto request_too_large;

    char *json = NULL;
    if (api_request(client, "/api/v3/song/detail", "v3/song/detail",
                    payload, &json, error, error_size) != 0) return -1;
    JsonDoc doc;
    JsonToken *tokens = NULL;
    if (parse_doc(json, &doc, &tokens, error, error_size) != 0) {
        free(json);
        return -1;
    }
    int array = json_obj_get(&doc, 0, "songs");
    if (array < 0 || doc.tokens[array].type != JSON_ARRAY) {
        response_error(&doc, error, error_size);
        free(tokens);
        free(json);
        return -1;
    }
    int available = json_arr_size(&doc, array);
    if ((size_t)available > capacity) available = (int)capacity;
    for (int i = 0; i < available; i++) {
        int item = json_arr_get(&doc, array, i);
        int64_t album_id = 0;
        if (read_song_with_album_id(&doc, item, &songs[*count],
                                    &album_id) == 0) {
            if (album_ids) album_ids[*count] = album_id;
            if (artists && artist_count && *count == 0) {
                int wrapped = json_obj_get(&doc, item, "song");
                read_artist_refs(&doc, wrapped >= 0 ? wrapped : item,
                                 artists, artist_capacity, artist_count);
            }
            (*count)++;
        }
    }
    free(tokens);
    free(json);
    if (*count == 0) {
        set_error(error, error_size, "歌曲详情不可用");
        return -1;
    }
    return 0;

request_too_large:
    set_error(error, error_size, "歌曲详情请求过大");
    return -1;
}

int netease_song_detail(NeteaseClient *client, int64_t song_id,
                        Song *song, char *error, size_t error_size) {
    if (!song || song_id <= 0) {
        set_error(error, error_size, "歌曲详情请求无效");
        return -1;
    }
    size_t count = 0;
    if (request_song_details(client, &song_id, 1, song, NULL,
                             NULL, 0, NULL, 1, &count,
                             error, error_size) != 0)
        return -1;
    if (count != 1 || song->id != song_id) {
        memset(song, 0, sizeof(*song));
        set_error(error, error_size, "歌曲详情响应异常");
        return -1;
    }
    return 0;
}

int netease_song_album_detail(NeteaseClient *client, int64_t song_id,
                              Song *song, int64_t *album_id,
                              char *error, size_t error_size) {
    if (!song || !album_id || song_id <= 0) {
        set_error(error, error_size, "歌曲详情请求无效");
        return -1;
    }
    *album_id = 0;
    size_t count = 0;
    if (request_song_details(client, &song_id, 1, song, album_id,
                             NULL, 0, NULL, 1, &count,
                             error, error_size) != 0)
        return -1;
    if (count != 1 || song->id != song_id || *album_id <= 0) {
        memset(song, 0, sizeof(*song));
        *album_id = 0;
        set_error(error, error_size, "歌曲的专辑信息不可用");
        return -1;
    }
    return 0;
}

int netease_song_artists(NeteaseClient *client, int64_t song_id,
                         NeteaseArtist *artists, size_t capacity,
                         size_t *count, char *error, size_t error_size) {
    if (!client || song_id <= 0 || !artists || capacity == 0 || !count) {
        set_error(error, error_size, "歌曲艺人请求无效");
        return -1;
    }
    *count = 0;
    Song song;
    size_t song_count = 0;
    if (capacity > NM3DS_SONG_ARTISTS_MAX)
        capacity = NM3DS_SONG_ARTISTS_MAX;
    if (request_song_details(client, &song_id, 1, &song, NULL,
                             artists, capacity, count, 1, &song_count,
                             error, error_size) != 0)
        return -1;
    if (song_count != 1 || song.id != song_id || *count == 0) {
        memset(artists, 0, capacity * sizeof(artists[0]));
        *count = 0;
        set_error(error, error_size, "歌曲的艺人信息不可用");
        return -1;
    }
    return 0;
}

int netease_search(NeteaseClient *client, const char *query,
                   size_t offset, Song *songs, size_t capacity,
                   size_t *count, bool *has_more,
                   char *error, size_t error_size) {
    if (!client || !query || !songs || !count || capacity == 0) {
        set_error(error, error_size, "搜索请求无效");
        return -1;
    }
    *count = 0;
    if (has_more) *has_more = false;
    char escaped[384];
    if (json_escape(query, escaped, sizeof(escaped)) != 0) {
        set_error(error, error_size, "搜索文本过长");
        return -1;
    }
    char header[EAPI_HEADER_CAPACITY];
    if (append_header(client, header, sizeof(header)) < 0) {
        set_error(error, error_size, "无法构建请求头");
        return -1;
    }
    char payload[EAPI_PAYLOAD_CAPACITY];
    int length = snprintf(payload, sizeof(payload),
        "{\"s\":\"%s\",\"type\":1,\"offset\":%u,"
        "\"total\":\"true\",\"limit\":%u,%s}",
        escaped, (unsigned int)offset, (unsigned int)capacity, header);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        set_error(error, error_size, "搜索请求过大");
        return -1;
    }
    char *json = NULL;
    if (api_request(client, "/api/search/get", "search/get", payload,
                    &json, error, error_size) != 0) return -1;

    JsonDoc doc;
    JsonToken *tokens = NULL;
    if (parse_doc(json, &doc, &tokens, error, error_size) != 0) {
        free(json);
        return -1;
    }
    int result = json_obj_get(&doc, 0, "result");
    int array = result >= 0 ? json_obj_get(&doc, result, "songs") : -1;
    if (array < 0 || doc.tokens[array].type != JSON_ARRAY) {
        response_error(&doc, error, error_size);
        free(tokens);
        free(json);
        return -1;
    }
    int available = json_arr_size(&doc, array);
    if ((size_t)available > capacity) available = (int)capacity;
    for (int i = 0; i < available; i++) {
        int item = json_arr_get(&doc, array, i);
        Song *song = &songs[*count];
        if (read_song(&doc, item, song) != 0) continue;
        (*count)++;
    }
    int64_t total = 0;
    int total_token = result >= 0 ? json_obj_get(&doc, result, "songCount") : -1;
    if (has_more) {
        if (total_token >= 0 && json_i64(&doc, total_token, &total) == 0)
            *has_more = offset + *count < (size_t)total;
        else *has_more = *count == capacity;
    }
    free(tokens);
    free(json);
    if (*count == 0) {
        set_error(error, error_size, "没有找到歌曲");
        return -1;
    }
    return 0;
}

int netease_discover(NeteaseClient *client, size_t offset,
                     Song *songs, size_t capacity, size_t *count,
                     bool *has_more, size_t *total_count,
                     bool *total_known,
                     char *error, size_t error_size) {
    if (!songs || capacity == 0 || capacity == SIZE_MAX ||
        !count || !has_more || !total_count || !total_known ||
        offset > SIZE_MAX - capacity - 1U) {
        set_error(error, error_size, "推荐请求无效");
        return -1;
    }
    *count = 0;
    *has_more = false;
    *total_count = 0;
    *total_known = false;
    /* The public endpoint supports limit but not offset. Request a prefix
       through one item beyond this page, then discard earlier objects while
       streaming so only the current page stays in Song storage. */
    char url[128];
    size_t limit = offset + capacity + 1U;
    int length = snprintf(url, sizeof(url), "%s?limit=%zu",
                          DISCOVER_URL, limit);
    if (length < 0 || (size_t)length >= sizeof(url)) {
        set_error(error, error_size, "推荐请求过大");
        return -1;
    }
    uint8_t *body = NULL;
    size_t body_size = 0;
    NetErrorKind failure = NET_ERROR_NONE;
    if (net_get_controlled_ex(url, &body, &body_size,
                              client ? client->cancel : NULL,
                              client ? client->cancel_userdata : NULL,
                              &failure, error, error_size) != 0) {
        if (client) client->last_failure = net_failure(failure);
        return -1;
    }
    JsonToken *tokens = (JsonToken *)calloc(
        RECOMMEND_SONG_TOKENS, sizeof(JsonToken));
    if (!tokens) {
        free(body);
        set_error(error, error_size, "内存不足");
        return -1;
    }
    SongArrayReader reader = {
        .songs = songs,
        .capacity = capacity,
        .offset = offset,
    };
    int result = json_visit_array_objects(
        (char *)body, "result", tokens, RECOMMEND_SONG_TOKENS,
        read_song_object, &reader);
    free(tokens);
    *count = reader.count;
    *has_more = reader.has_more;
    *total_count = reader.total_count;
    *total_known = !reader.has_more;
    if (result == JSON_VISIT_NOT_FOUND) {
        JsonToken error_tokens[64];
        JsonDoc error_doc;
        if (json_parse(&error_doc, (const char *)body,
                       error_tokens, 64) > 0)
            response_error(&error_doc, error, error_size);
        else set_error(error, error_size, "公开推荐列表不可用");
        free(body);
        return -1;
    }
    free(body);
    if (result == JSON_VISIT_TOKENS_EXHAUSTED) {
        set_error(error, error_size, "公开推荐项目的 JSON 过大");
        return -1;
    }
    if (result < 0) {
        set_error(error, error_size, "公开推荐的 JSON 无效");
        return -1;
    }
    if (*count == 0) {
        set_error(error, error_size,
                  offset ? "没有更多公开推荐" : "没有可用的公开推荐");
        return -1;
    }
    return 0;
}

int netease_recommend(NeteaseClient *client, size_t offset,
                      Song *songs, size_t capacity, size_t *count,
                      bool *has_more, size_t *total_count,
                      bool *total_known,
                      char *error, size_t error_size) {
    if (!client || !netease_logged_in(client) || !songs || capacity == 0 ||
        !count || !has_more || !total_count || !total_known) {
        set_error(error, error_size, "登录后才能获取每日推荐");
        return -1;
    }
    *count = 0;
    *has_more = false;
    *total_count = 0;
    *total_known = false;
    static const char payload[] =
        "{\"afresh\":false,\"csrf_token\":\"\"}";
    char *json = NULL;
    if (weapi_request(client, "v3/discovery/recommend/songs", payload,
                      &json, error, error_size) != 0) return -1;
    int result = read_recommendation_array(
        json, offset, songs, capacity, count, has_more, total_count,
        error, error_size);
    free(json);
    if (result == 0) *total_known = true;
    return result;
}

typedef struct {
    NeteaseClient *client;
    NeteasePlaylist *playlists;
    size_t capacity;
    size_t visited;
    size_t count;
    bool has_more;
} UserPlaylistReader;

static int read_user_playlist_object(const JsonDoc *doc, void *userdata) {
    UserPlaylistReader *reader = (UserPlaylistReader *)userdata;
    size_t index = reader->visited++;
    if (index >= reader->capacity) {
        reader->has_more = true;
        return 1;
    }
    NeteasePlaylist playlist;
    memset(&playlist, 0, sizeof(playlist));
    if (json_i64(doc, json_obj_get(doc, 0, "id"), &playlist.id) != 0)
        return 0;
    int creator = json_obj_get(doc, 0, "creator");
    (void)json_i64(doc, json_obj_get(doc, creator, "userId"),
                   &playlist.creator_id);
    playlist.owned = playlist.creator_id == reader->client->user_id;
    int64_t track_count = 0;
    if (json_i64(doc, json_obj_get(doc, 0, "trackCount"),
                 &track_count) == 0 && track_count > 0)
        playlist.track_count = track_count > UINT32_MAX ? UINT32_MAX :
                               (uint32_t)track_count;
    read_string(doc, 0, "name", playlist.name,
                sizeof(playlist.name), "未命名歌单");
    reader->playlists[reader->count++] = playlist;
    return 0;
}

int netease_user_playlists(NeteaseClient *client, size_t offset,
                           NeteasePlaylist *playlists, size_t capacity,
                           size_t *count, bool *has_more,
                           char *error, size_t error_size) {
    if (!client || !netease_logged_in(client) || client->user_id <= 0 ||
        !playlists || capacity == 0 || capacity == SIZE_MAX ||
        !count || !has_more) {
        set_error(error, error_size, "登录后才能查看歌单");
        return -1;
    }
    *count = 0;
    *has_more = false;
    char header[EAPI_HEADER_CAPACITY];
    if (append_header(client, header, sizeof(header)) < 0) {
        set_error(error, error_size, "无法构建请求头");
        return -1;
    }
    char payload[EAPI_PAYLOAD_CAPACITY];
    size_t request_limit = capacity + 1U;
    int length = snprintf(payload, sizeof(payload),
        "{\"uid\":\"%lld\",\"offset\":%zu,\"limit\":%zu,"
        "\"includeVideo\":false,%s}",
        (long long)client->user_id, offset,
        request_limit, header);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        set_error(error, error_size, "歌单请求过大");
        return -1;
    }
    char *json = NULL;
    if (api_request(client, "/api/user/playlist", "user/playlist", payload,
                    &json, error, error_size) != 0) return -1;
    JsonToken *tokens = (JsonToken *)calloc(
        USER_PLAYLIST_TOKENS, sizeof(JsonToken));
    if (!tokens) {
        free(json);
        set_error(error, error_size, "内存不足");
        return -1;
    }
    UserPlaylistReader reader = {
        .client = client,
        .playlists = playlists,
        .capacity = capacity,
    };
    int result = json_visit_array_objects(
        json, "playlist", tokens, USER_PLAYLIST_TOKENS,
        read_user_playlist_object, &reader);
    free(tokens);
    *count = reader.count;
    *has_more = reader.has_more;
    if (result == JSON_VISIT_TOKENS_EXHAUSTED) {
        free(json);
        set_error(error, error_size, "用户歌单项目的 JSON 过大");
        return -1;
    }
    if (result == JSON_VISIT_NOT_FOUND) {
        JsonToken error_tokens[64];
        JsonDoc error_doc;
        if (json_parse(&error_doc, json, error_tokens, 64) > 0)
            response_error(&error_doc, error, error_size);
        else set_error(error, error_size, "用户歌单列表不可用");
        free(json);
        return -1;
    }
    if (result < 0) {
        free(json);
        set_error(error, error_size, "用户歌单的 JSON 无效");
        return -1;
    }
    free(json);
    if (*count == 0) {
        set_error(error, error_size, "没有可用的用户歌单");
        return -1;
    }
    return 0;
}

int netease_user_cloud(NeteaseClient *client, size_t offset,
                       NeteaseCloudTrack *tracks, size_t capacity,
                       size_t *count, bool *has_more,
                       char *error, size_t error_size) {
    if (!client || !netease_logged_in(client) || client->user_id <= 0 ||
        !tracks || capacity == 0 || capacity == SIZE_MAX ||
        !count || !has_more) {
        set_error(error, error_size, "登录后才能查看音乐云盘");
        return -1;
    }
    char payload[192];
    int length = snprintf(payload, sizeof(payload),
        "{\"limit\":%zu,\"offset\":%zu,\"csrf_token\":\"\"}",
        capacity + 1U, offset);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        set_error(error, error_size, "云盘请求过大");
        return -1;
    }
    char *json = NULL;
    if (weapi_request(client, "v1/cloud/get", payload,
                      &json, error, error_size) != 0)
        return -1;
    int result = cloud_parse_response(
        json, client->user_id, tracks, capacity,
        count, has_more, error, error_size);
    free(json);
    return result;
}

int netease_playlist_tracks(NeteaseClient *client, int64_t playlist_id,
                            size_t offset, bool refresh_index,
                            Song *songs, size_t capacity,
                            size_t *count, bool *has_more,
                            size_t *total_count,
                            char *error, size_t error_size) {
    if (!client || !netease_logged_in(client) || playlist_id <= 0 ||
        !songs || capacity == 0 || !count || !has_more || !total_count) {
        set_error(error, error_size, "登录后才能查看歌单歌曲");
        return -1;
    }
    *count = 0;
    *has_more = false;
    *total_count = 0;
    char header[EAPI_HEADER_CAPACITY];
    if (append_header(client, header, sizeof(header)) < 0) {
        set_error(error, error_size, "无法构建请求头");
        return -1;
    }
    char payload[EAPI_PAYLOAD_CAPACITY];
    int length = snprintf(payload, sizeof(payload),
        "{\"id\":\"%lld\",\"n\":0,\"s\":8,%s}",
        (long long)playlist_id, header);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        set_error(error, error_size, "歌单详情请求过大");
        return -1;
    }
    int64_t ids[NM3DS_LIBRARY_PAGE];
    size_t id_count = 0;
    if (capacity > NM3DS_LIBRARY_PAGE) capacity = NM3DS_LIBRARY_PAGE;
    int cached = refresh_index ? 1 : playlist_track_index_read_page(
        PLAYLIST_TRACK_INDEX_PATH, playlist_id, offset,
        ids, capacity, &id_count, has_more, total_count,
        error, error_size);
    if (cached != 0) {
        if (api_request_playlist_index(client, playlist_id, payload,
                                       total_count,
                                       error, error_size) != 0)
            return -1;
        if (playlist_track_index_read_page(
                PLAYLIST_TRACK_INDEX_PATH, playlist_id, offset,
                ids, capacity, &id_count, has_more, total_count,
                error, error_size) != 0)
            return -1;
    }
    if (id_count == 0) {
        set_error(error, error_size, "歌单本页没有歌曲");
        return -1;
    }

    return request_song_details(client, ids, id_count, songs, NULL,
                                NULL, 0, NULL, capacity,
                                count, error, error_size);
}

typedef struct {
    NeteaseClient *client;
    SongIndexWriter *writer;
    size_t song_count;
    char *error;
    size_t error_size;
} AlbumSongIndexContext;

static int write_album_song_object(const JsonDoc *doc, void *userdata) {
    AlbumSongIndexContext *context = (AlbumSongIndexContext *)userdata;
    if (context->client->cancel &&
        context->client->cancel(context->client->cancel_userdata)) {
        set_error(context->error, context->error_size, "请求已取消");
        return -1;
    }
    Song song;
    if (read_song(doc, 0, &song) != 0) {
        set_error(context->error, context->error_size,
                  "专辑歌曲列表不可用");
        return -1;
    }
    if (song_index_writer_append(context->writer, &song,
                                 context->error,
                                 context->error_size) != 0)
        return -1;
    context->song_count++;
    return 0;
}

static int refresh_album_song_index(NeteaseClient *client, int64_t album_id,
                                    size_t *track_count,
                                    char *error, size_t error_size) {
    char route[64];
    int route_length = snprintf(route, sizeof(route), "v1/album/%lld",
                                (long long)album_id);
    if (route_length < 0 || (size_t)route_length >= sizeof(route)) {
        set_error(error, error_size, "专辑请求过大");
        return -1;
    }
    char *json = NULL;
    if (weapi_request(client, route, "{}", &json,
                      error, error_size) != 0)
        return -1;

    SongIndexWriter *writer = song_index_writer_create(
        ALBUM_TRACK_INDEX_PATH, album_id, error, error_size);
    if (!writer) {
        free(json);
        return -1;
    }
    JsonToken *tokens = (JsonToken *)calloc(
        ALBUM_SONG_TOKENS, sizeof(JsonToken));
    if (!tokens) {
        song_index_writer_destroy(writer);
        free(json);
        set_error(error, error_size, "内存不足");
        return -1;
    }
    AlbumSongIndexContext context = {
        .client = client,
        .writer = writer,
        .error = error,
        .error_size = error_size,
    };
    int visited = json_visit_array_objects(
        json, "songs", tokens, ALBUM_SONG_TOKENS,
        write_album_song_object, &context);
    int result = 0;
    if (visited == JSON_VISIT_TOKENS_EXHAUSTED) {
        set_error(error, error_size, "专辑歌曲项目的 JSON 过大");
        result = -1;
    } else if (visited == JSON_VISIT_NOT_FOUND) {
        JsonToken error_tokens[64];
        JsonDoc error_doc;
        if (json_parse(&error_doc, json, error_tokens, 64) > 0)
            response_error(&error_doc, error, error_size);
        else set_error(error, error_size, "专辑歌曲列表不可用");
        result = -1;
    } else if (visited == JSON_VISIT_CALLBACK_FAILED) {
        if (!error || error_size == 0 || !error[0])
            set_error(error, error_size, "专辑歌曲列表不可用");
        result = -1;
    } else if (visited < 0) {
        set_error(error, error_size, "专辑歌曲列表的 JSON 无效");
        result = -1;
    } else if (context.song_count == 0) {
        set_error(error, error_size, "专辑歌曲列表不可用");
        result = -1;
    } else if (client->cancel &&
               client->cancel(client->cancel_userdata)) {
        set_error(error, error_size, "请求已取消");
        result = -1;
    } else {
        result = song_index_writer_commit(
            writer, track_count, error, error_size);
    }
    free(tokens);
    song_index_writer_destroy(writer);
    free(json);
    return result;
}

int netease_album_tracks(NeteaseClient *client, int64_t album_id,
                         size_t offset, bool refresh_index,
                         Song *songs, size_t capacity,
                         size_t *count, bool *has_more,
                         size_t *total_count,
                         char *error, size_t error_size) {
    if (!client || album_id <= 0 || !songs || capacity == 0 || !count ||
        !has_more || !total_count) {
        set_error(error, error_size, "专辑歌曲请求无效");
        return -1;
    }
    *count = 0;
    *has_more = false;
    *total_count = 0;
    if (capacity > NM3DS_ALBUM_PAGE) capacity = NM3DS_ALBUM_PAGE;
    int cached = refresh_index ? 1 : song_index_read_page(
        ALBUM_TRACK_INDEX_PATH, album_id, offset,
        songs, capacity, count, has_more, total_count,
        error, error_size);
    if (cached != 0) {
        if (!refresh_index) {
            set_error(error, error_size,
                      "专辑歌曲缓存不可用，请重新打开专辑");
            return -1;
        }
        if (refresh_album_song_index(client, album_id, total_count,
                                     error, error_size) != 0)
            return -1;
        if (song_index_read_page(
                ALBUM_TRACK_INDEX_PATH, album_id, offset,
                songs, capacity, count, has_more, total_count,
                error, error_size) != 0)
            return -1;
    }
    if (*count == 0) {
        set_error(error, error_size, "专辑本页没有歌曲");
        return -1;
    }
    return 0;
}

typedef struct {
    NeteaseAlbum *albums;
    size_t capacity;
    size_t visited;
    size_t count;
    bool has_more;
} ArtistAlbumReader;

static int read_artist_album_object(const JsonDoc *doc, void *userdata) {
    ArtistAlbumReader *reader = (ArtistAlbumReader *)userdata;
    if (reader->visited++ >= reader->capacity) {
        reader->has_more = true;
        return 1;
    }
    NeteaseAlbum album;
    memset(&album, 0, sizeof(album));
    if (json_i64(doc, json_obj_get(doc, 0, "id"), &album.id) != 0 ||
        album.id <= 0)
        return 0;
    int64_t track_count = 0;
    int count_token = json_obj_get(doc, 0, "size");
    if (count_token < 0) count_token = json_obj_get(doc, 0, "trackCount");
    if (json_i64(doc, count_token, &track_count) == 0 && track_count > 0)
        album.track_count = track_count > UINT32_MAX ? UINT32_MAX :
                                                       (uint32_t)track_count;
    read_string(doc, 0, "name", album.name, sizeof(album.name),
                "未命名专辑");
    (void)utf8_compose_hangul_nfc(album.name);
    reader->albums[reader->count++] = album;
    return 0;
}

static int artist_list_error(char *json, int visit_result,
                             const char *missing, const char *invalid,
                             const char *too_large,
                             char *error, size_t error_size) {
    if (visit_result == JSON_VISIT_TOKENS_EXHAUSTED) {
        set_error(error, error_size, "%s", too_large);
        return -1;
    }
    if (visit_result == JSON_VISIT_NOT_FOUND) {
        JsonToken error_tokens[64];
        JsonDoc error_doc;
        if (json_parse(&error_doc, json, error_tokens, 64) > 0)
            response_error(&error_doc, error, error_size);
        else set_error(error, error_size, "%s", missing);
        return -1;
    }
    if (visit_result < 0) {
        set_error(error, error_size, "%s", invalid);
        return -1;
    }
    return 0;
}

int netease_artist_albums(NeteaseClient *client, int64_t artist_id,
                          size_t offset, NeteaseAlbum *albums,
                          size_t capacity, size_t *count, bool *has_more,
                          char *error, size_t error_size) {
    if (!client || artist_id <= 0 || !albums || capacity == 0 ||
        capacity > NM3DS_ARTIST_ALBUM_PAGE || capacity == SIZE_MAX ||
        !count || !has_more) {
        set_error(error, error_size, "艺人专辑请求无效");
        return -1;
    }
    *count = 0;
    *has_more = false;
    char route[96];
    int route_length = snprintf(route, sizeof(route), "artist/albums/%lld",
                                (long long)artist_id);
    if (route_length < 0 || (size_t)route_length >= sizeof(route)) {
        set_error(error, error_size, "艺人专辑请求过大");
        return -1;
    }
    char payload[192];
    int payload_length = snprintf(
        payload, sizeof(payload),
        "{\"limit\":%zu,\"offset\":%zu,\"total\":true,"
        "\"csrf_token\":\"\"}", capacity + 1U, offset);
    if (payload_length < 0 ||
        (size_t)payload_length >= sizeof(payload)) {
        set_error(error, error_size, "艺人专辑请求过大");
        return -1;
    }
    char *json = NULL;
    if (weapi_request(client, route, payload, &json,
                      error, error_size) != 0)
        return -1;
    JsonToken *tokens = (JsonToken *)calloc(
        ARTIST_ITEM_TOKENS, sizeof(JsonToken));
    if (!tokens) {
        free(json);
        set_error(error, error_size, "内存不足");
        return -1;
    }
    ArtistAlbumReader reader = {
        .albums = albums,
        .capacity = capacity,
    };
    int result = json_visit_array_objects(
        json, "hotAlbums", tokens, ARTIST_ITEM_TOKENS,
        read_artist_album_object, &reader);
    free(tokens);
    *count = reader.count;
    *has_more = reader.has_more;
    if (artist_list_error(json, result,
                          "艺人专辑列表不可用",
                          "艺人专辑列表的 JSON 无效",
                          "艺人专辑项目的 JSON 过大",
                          error, error_size) != 0) {
        free(json);
        return -1;
    }
    free(json);
    if (*count == 0) {
        set_error(error, error_size,
                  offset ? "艺人本页没有更多专辑" :
                           "这个艺人没有可显示的专辑");
        return -1;
    }
    return 0;
}

typedef struct {
    Song *songs;
    size_t capacity;
    size_t visited;
    size_t count;
    bool has_more;
} ArtistSongReader;

static int read_artist_song_object(const JsonDoc *doc, void *userdata) {
    ArtistSongReader *reader = (ArtistSongReader *)userdata;
    if (reader->visited++ >= reader->capacity) {
        reader->has_more = true;
        return 1;
    }
    Song song;
    if (read_song(doc, 0, &song) == 0)
        reader->songs[reader->count++] = song;
    return 0;
}

int netease_artist_songs(NeteaseClient *client, int64_t artist_id,
                         size_t offset, Song *songs, size_t capacity,
                         size_t *count, bool *has_more,
                         char *error, size_t error_size) {
    if (!client || artist_id <= 0 || !songs || capacity == 0 ||
        capacity > NM3DS_ARTIST_SONG_PAGE || capacity == SIZE_MAX ||
        !count || !has_more) {
        set_error(error, error_size, "艺人歌曲请求无效");
        return -1;
    }
    *count = 0;
    *has_more = false;
    char header[EAPI_HEADER_CAPACITY];
    if (append_header(client, header, sizeof(header)) < 0) {
        set_error(error, error_size, "无法构建请求头");
        return -1;
    }
    char payload[EAPI_PAYLOAD_CAPACITY];
    int length = snprintf(
        payload, sizeof(payload),
        "{\"id\":\"%lld\",\"private_cloud\":\"true\","
        "\"work_type\":1,\"order\":\"hot\",\"offset\":%zu,"
        "\"limit\":%zu,%s}",
        (long long)artist_id, offset, capacity + 1U, header);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        set_error(error, error_size, "艺人歌曲请求过大");
        return -1;
    }
    char *json = NULL;
    if (api_request(client, "/api/v1/artist/songs", "v1/artist/songs",
                    payload, &json, error, error_size) != 0)
        return -1;
    JsonToken *tokens = (JsonToken *)calloc(
        ARTIST_ITEM_TOKENS, sizeof(JsonToken));
    if (!tokens) {
        free(json);
        set_error(error, error_size, "内存不足");
        return -1;
    }
    ArtistSongReader reader = {
        .songs = songs,
        .capacity = capacity,
    };
    int result = json_visit_array_objects(
        json, "songs", tokens, ARTIST_ITEM_TOKENS,
        read_artist_song_object, &reader);
    free(tokens);
    *count = reader.count;
    *has_more = reader.has_more;
    if (artist_list_error(json, result,
                          "艺人歌曲列表不可用",
                          "艺人歌曲列表的 JSON 无效",
                          "艺人歌曲项目的 JSON 过大",
                          error, error_size) != 0) {
        free(json);
        return -1;
    }
    free(json);
    if (*count == 0) {
        set_error(error, error_size,
                  offset ? "艺人本页没有更多歌曲" :
                           "这个艺人没有可显示的歌曲");
        return -1;
    }
    return 0;
}

int netease_login_qr_key(NeteaseClient *client, char *key, size_t key_size,
                         char *error, size_t error_size) {
    if (!client || !key || key_size == 0) {
        set_error(error, error_size, "扫码登录请求无效");
        return -1;
    }
    key[0] = '\0';
    char header[EAPI_HEADER_CAPACITY];
    if (append_header(client, header, sizeof(header)) < 0) {
        set_error(error, error_size, "无法构建请求头");
        return -1;
    }
    char payload[EAPI_PAYLOAD_CAPACITY];
    int length = snprintf(payload, sizeof(payload), "{\"type\":3,%s}", header);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        set_error(error, error_size, "扫码登录请求过大");
        return -1;
    }
    char *json = NULL;
    if (api_request(client, "/api/login/qrcode/unikey",
                    "login/qrcode/unikey", payload,
                    &json, error, error_size) != 0) return -1;
    JsonDoc doc;
    JsonToken *tokens = NULL;
    if (parse_doc(json, &doc, &tokens, error, error_size) != 0) {
        free(json);
        return -1;
    }
    int token = json_obj_get(&doc, 0, "unikey");
    if (token < 0) {
        int data = json_obj_get(&doc, 0, "data");
        token = data >= 0 ? json_obj_get(&doc, data, "unikey") : -1;
    }
    int result = token >= 0 ? json_string(&doc, token, key, key_size) : -1;
    if (result < 0 || !key[0])
        response_error(&doc, error, error_size);
    free(tokens);
    free(json);
    return result < 0 || !key[0] ? -1 : 0;
}

int netease_login_qr_check(NeteaseClient *client, const char *key,
                           int *status_code, char *message,
                           size_t message_size,
                           char *error, size_t error_size) {
    if (!client || !key || !key[0] || !status_code) {
        set_error(error, error_size, "扫码登录状态请求无效");
        return -1;
    }
    *status_code = 0;
    if (message && message_size) message[0] = '\0';
    char escaped[160];
    if (json_escape(key, escaped, sizeof(escaped)) != 0) {
        set_error(error, error_size, "扫码登录密钥过长");
        return -1;
    }
    char header[EAPI_HEADER_CAPACITY];
    if (append_header(client, header, sizeof(header)) < 0) {
        set_error(error, error_size, "无法构建请求头");
        return -1;
    }
    char payload[EAPI_PAYLOAD_CAPACITY];
    int length = snprintf(payload, sizeof(payload),
                          "{\"type\":3,\"key\":\"%s\",%s}",
                          escaped, header);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        set_error(error, error_size, "扫码登录状态请求过大");
        return -1;
    }
    char response_cookie[sizeof(client->music_u) + 16];
    char *json = NULL;
    if (api_request_ex(client, "/api/login/qrcode/client/login",
                       "login/qrcode/client/login", payload, &json,
                       response_cookie, sizeof(response_cookie),
                       error, error_size) != 0) return -1;
    JsonDoc doc;
    JsonToken *tokens = NULL;
    if (parse_doc(json, &doc, &tokens, error, error_size) != 0) {
        free(json);
        return -1;
    }
    int64_t code = 0;
    if (json_i64(&doc, json_obj_get(&doc, 0, "code"), &code) != 0) {
        response_error(&doc, error, error_size);
        free(tokens);
        free(json);
        return -1;
    }
    *status_code = (int)code;
    if (message && message_size) {
        int token = json_obj_get(&doc, 0, "message");
        if (token >= 0) (void)json_string(&doc, token, message, message_size);
    }
    if (*status_code == 803) {
        int cookie_token = json_obj_get(&doc, 0, "cookie");
        char body_cookie[NETEASE_COOKIE_CAPACITY] = {0};
        if (cookie_token >= 0)
            (void)json_string(&doc, cookie_token, body_cookie,
                              sizeof(body_cookie));
        const char *cookie = response_cookie[0] ? response_cookie : body_cookie;
        if (netease_set_music_u(client, cookie) != 0) {
            set_error(error, error_size,
                      "登录 Cookie 缺失或过大");
            free(tokens);
            free(json);
            return -1;
        }
    }
    free(tokens);
    free(json);
    return 0;
}

int netease_account(NeteaseClient *client,
                    char *error, size_t error_size) {
    if (!client || !netease_logged_in(client)) {
        set_error(error, error_size, "尚未登录");
        return -1;
    }
    char header[EAPI_HEADER_CAPACITY];
    if (append_header(client, header, sizeof(header)) < 0) {
        set_error(error, error_size, "无法构建请求头");
        return -1;
    }
    char payload[EAPI_PAYLOAD_CAPACITY];
    int length = snprintf(payload, sizeof(payload), "{%s}", header);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        set_error(error, error_size, "账户请求过大");
        return -1;
    }
    char *json = NULL;
    if (api_request(client, "/api/nuser/account/get", "nuser/account/get",
                    payload, &json, error, error_size) != 0) return -1;
    JsonDoc doc;
    JsonToken *tokens = NULL;
    if (parse_doc(json, &doc, &tokens, error, error_size) != 0) {
        free(json);
        return -1;
    }
    int account = json_obj_get(&doc, 0, "account");
    int profile = json_obj_get(&doc, 0, "profile");
    int64_t user_id = 0;
    int id_token = account >= 0 ? json_obj_get(&doc, account, "id") : -1;
    int name_token = profile >= 0 ? json_obj_get(&doc, profile, "nickname") : -1;
    if (json_i64(&doc, id_token, &user_id) != 0 || name_token < 0 ||
        json_string(&doc, name_token, client->nickname,
                    sizeof(client->nickname)) < 0) {
        int64_t code = 0;
        bool explicit_invalid =
            (json_i64(&doc, json_obj_get(&doc, 0, "code"), &code) == 0 &&
             (code == 301 || code == 302)) ||
            (account >= 0 && profile >= 0 &&
             json_is_null(&doc, account) && json_is_null(&doc, profile));
        if (explicit_invalid)
            client->last_failure = NETEASE_FAILURE_AUTH_INVALID;
        response_error(&doc, error, error_size);
        free(tokens);
        free(json);
        return -1;
    }
    client->user_id = user_id;
    free(tokens);
    free(json);
    return 0;
}

static int parse_lrc_time(const char *tag, uint32_t *time_ms) {
    if (!tag || !time_ms || !isdigit((unsigned char)tag[0])) return -1;
    char *end = NULL;
    long minutes = strtol(tag, &end, 10);
    if (!end || *end != ':' || minutes < 0) return -1;
    char *seconds_end = NULL;
    double seconds = strtod(end + 1, &seconds_end);
    if (!seconds_end || *seconds_end != ']' || seconds < 0.0 || seconds >= 60.0)
        return -1;
    double total = ((double)minutes * 60.0 + seconds) * 1000.0;
    if (total < 0.0 || total > 4294967295.0) return -1;
    *time_ms = (uint32_t)(total + 0.5);
    return 0;
}

static size_t parse_lrc(char *lrc, LyricLine *lines, size_t capacity) {
    size_t count = 0;
    char *save = NULL;
    for (char *line = strtok_r(lrc, "\r\n", &save);
         line && count < capacity;
         line = strtok_r(NULL, "\r\n", &save)) {
        if (line[0] != '[') continue;
        char *close = strchr(line, ']');
        if (!close || !close[1]) continue;
        uint32_t time_ms;
        if (parse_lrc_time(line + 1, &time_ms) != 0) continue;
        const char *text = close + 1;
        while (*text == ' ' || *text == '\t') text++;
        if (!*text) continue;
        (void)utf8_compose_hangul_nfc((char *)text);
        lines[count].time_ms = time_ms;
        (void)utf8_copy_truncated(lines[count].text,
                                  sizeof(lines[count].text), text);
        count++;
    }
    return count;
}

int netease_lyrics(NeteaseClient *client, int64_t song_id,
                   LyricLine *lines, size_t capacity,
                   size_t *count, char *error, size_t error_size) {
    if (song_id <= 0 || !lines || capacity == 0 || !count) {
        set_error(error, error_size, "歌词请求无效");
        return -1;
    }
    *count = 0;
    char url[256];
    snprintf(url, sizeof(url),
             "https://music.163.com/api/song/lyric?id=%lld&lv=-1&kv=-1&tv=-1",
             (long long)song_id);
    uint8_t *body = NULL;
    size_t body_size = 0;
    NetErrorKind failure = NET_ERROR_NONE;
    if (net_get_controlled_ex(url, &body, &body_size,
                              client ? client->cancel : NULL,
                              client ? client->cancel_userdata : NULL,
                              &failure, error, error_size) != 0) {
        if (client) client->last_failure = net_failure(failure);
        return -1;
    }
    JsonDoc doc;
    JsonToken *tokens = NULL;
    if (parse_doc((const char *)body, &doc, &tokens, error, error_size) != 0) {
        free(body);
        return -1;
    }
    int lrc_object = json_obj_get(&doc, 0, "lrc");
    int lyric_token = lrc_object >= 0 ? json_obj_get(&doc, lrc_object, "lyric") : -1;
    char *lrc = (char *)malloc(body_size + 1);
    if (!lrc || lyric_token < 0 ||
        json_string(&doc, lyric_token, lrc, body_size + 1) < 0) {
        free(lrc);
        free(tokens);
        free(body);
        set_error(error, error_size, "歌词不可用");
        return -1;
    }
    *count = parse_lrc(lrc, lines, capacity);
    free(lrc);
    free(tokens);
    free(body);
    if (*count == 0) {
        set_error(error, error_size, "歌词不可用");
        return -1;
    }
    return 0;
}

int netease_song_url(NeteaseClient *client, int64_t song_id,
                     char *url, size_t url_size,
                     NeteasePlaybackInfo *playback,
                     char *error, size_t error_size) {
    if (!client || song_id <= 0 || !url || url_size == 0) {
        set_error(error, error_size, "歌曲无效");
        return -1;
    }
    url[0] = '\0';
    if (playback) memset(playback, 0, sizeof(*playback));
    char header[EAPI_HEADER_CAPACITY];
    if (append_header(client, header, sizeof(header)) < 0) {
        set_error(error, error_size, "无法构建请求头");
        return -1;
    }
    char payload[EAPI_PAYLOAD_CAPACITY];
    int length = snprintf(payload, sizeof(payload),
        "{\"ids\":\"[%lld]\",\"level\":\"standard\","
        "\"encodeType\":\"mp3\",%s}", (long long)song_id, header);
    if (length < 0 || (size_t)length >= sizeof(payload)) {
        set_error(error, error_size, "歌曲请求过大");
        return -1;
    }
    char *json = NULL;
    if (api_request(client, "/api/song/enhance/player/url/v1",
                    "song/enhance/player/url/v1", payload,
                    &json, error, error_size) != 0) return -1;
    JsonDoc doc;
    JsonToken *tokens = NULL;
    if (parse_doc(json, &doc, &tokens, error, error_size) != 0) {
        free(json);
        return -1;
    }
    int data = json_obj_get(&doc, 0, "data");
    int item = data >= 0 ? json_arr_get(&doc, data, 0) : -1;
    int url_token = item >= 0 ? json_obj_get(&doc, item, "url") : -1;
    if (url_token < 0 || json_is_null(&doc, url_token) ||
        json_string(&doc, url_token, url, url_size) < 0 || !url[0]) {
        set_error(error, error_size,
                  "歌曲不可用（地区、VIP 或版权限制）");
        free(tokens);
        free(json);
        return -1;
    }
    NeteasePlaybackInfo parsed_playback;
    memset(&parsed_playback, 0, sizeof(parsed_playback));
    int trial = json_obj_get(&doc, item, "freeTrialInfo");
    parsed_playback.is_trial =
        trial >= 0 && !json_is_null(&doc, trial) &&
        !json_token_equals(&doc, trial, "null");
    int format = json_obj_get(&doc, item, "type");
    if (format < 0) format = json_obj_get(&doc, item, "encodeType");
    if (format >= 0 && !json_is_null(&doc, format))
        (void)json_string(&doc, format, parsed_playback.format,
                          sizeof(parsed_playback.format));
    for (char *cursor = parsed_playback.format; *cursor; cursor++)
        *cursor = (char)tolower((unsigned char)*cursor);
    if (parsed_playback.format[0] &&
        strcmp(parsed_playback.format, "mp3") != 0 &&
        strcmp(parsed_playback.format, "mpeg") != 0) {
        set_error(error, error_size,
                  "服务端返回 %s，本机仅支持 MP3",
                  parsed_playback.format);
        url[0] = '\0';
        free(tokens);
        free(json);
        return -1;
    }
    if (playback) *playback = parsed_playback;
    free(tokens);
    free(json);
    return 0;
}
