#include "json.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int expected_id;
    int count;
    int limit;
} VisitState;

static int visit_large_song(const JsonDoc *doc, void *userdata) {
    VisitState *state = (VisitState *)userdata;
    int64_t id = 0;
    assert(json_i64(doc, json_obj_get(doc, 0, "id"), &id) == 0);
    assert(id == state->expected_id++);
    state->count++;
    return state->count == state->limit ? 1 : 0;
}

int main(void) {
    const char *text =
        "{\"code\":200,\"result\":{\"songs\":["
        "{\"id\":347230,\"name\":\"海阔天空\","
        "\"artists\":[{\"name\":\"Beyond\"}]},"
        "{\"id\":2,\"name\":\"A\\u0026B \\uD83C\\uDFB5\"}]}}";
    JsonToken tokens[64];
    JsonDoc doc;
    assert(json_parse(&doc, text, tokens, 64) > 0);
    int result = json_obj_get(&doc, 0, "result");
    int songs = json_obj_get(&doc, result, "songs");
    assert(json_arr_size(&doc, songs) == 2);

    int song = json_arr_get(&doc, songs, 0);
    int64_t id = 0;
    assert(json_i64(&doc, json_obj_get(&doc, song, "id"), &id) == 0);
    assert(id == 347230);
    char value[64];
    assert(json_string(&doc, json_obj_get(&doc, song, "name"),
                       value, sizeof(value)) > 0);
    assert(strcmp(value, "海阔天空") == 0);

    song = json_arr_get(&doc, songs, 1);
    assert(json_string(&doc, json_obj_get(&doc, song, "name"),
                       value, sizeof(value)) > 0);
    assert(strcmp(value, "A&B 🎵") == 0);

    const char *null_text = "{\"url\":null,\"items\":[]}";
    assert(json_parse(&doc, null_text, tokens, 64) > 0);
    assert(json_is_null(&doc, json_obj_get(&doc, 0, "url")));
    assert(json_arr_size(&doc, json_obj_get(&doc, 0, "items")) == 0);

    size_t capacity = 128U * 1024U;
    char *large = (char *)malloc(capacity);
    assert(large != NULL);
    size_t used = (size_t)snprintf(large, capacity,
                                   "{\"data\":{\"dailySongs\":[");
    for (int item = 0; item < 180; item++) {
        int wrote = snprintf(large + used, capacity - used,
                             "%s{\"id\":%d,\"padding\":[",
                             item ? "," : "", item);
        assert(wrote > 0 && (size_t)wrote < capacity - used);
        used += (size_t)wrote;
        for (int value_index = 0; value_index < 50; value_index++) {
            wrote = snprintf(large + used, capacity - used,
                             "%s%d", value_index ? "," : "", value_index);
            assert(wrote > 0 && (size_t)wrote < capacity - used);
            used += (size_t)wrote;
        }
        wrote = snprintf(large + used, capacity - used, "]}");
        assert(wrote > 0 && (size_t)wrote < capacity - used);
        used += (size_t)wrote;
    }
    int wrote = snprintf(large + used, capacity - used, "]}}");
    assert(wrote > 0 && (size_t)wrote < capacity - used);

    JsonToken *whole_tokens = (JsonToken *)calloc(8192, sizeof(JsonToken));
    assert(whole_tokens != NULL);
    assert(json_parse(&doc, large, whole_tokens, 8192) == -1);
    free(whole_tokens);

    JsonToken item_tokens[128];
    VisitState state = {0, 0, 18};
    assert(json_visit_array_objects(
        large, "dailySongs", item_tokens, 128,
        visit_large_song, &state) == 18);
    assert(state.count == 18);
    assert(json_visit_array_objects(
        large, "recommend", item_tokens, 128,
        visit_large_song, &state) == JSON_VISIT_NOT_FOUND);
    VisitState exhausted_state = {0, 0, 1};
    assert(json_visit_array_objects(
        large, "dailySongs", item_tokens, 4,
        visit_large_song, &exhausted_state) ==
        JSON_VISIT_TOKENS_EXHAUSTED);
    assert(strstr(large, "},{\"id\":1") != NULL);
    free(large);

    capacity = 128U * 1024U;
    char *playlists = (char *)malloc(capacity);
    assert(playlists != NULL);
    used = (size_t)snprintf(playlists, capacity, "{\"playlist\":[");
    for (int item = 0; item < 9; item++) {
        wrote = snprintf(
            playlists + used, capacity - used,
            "%s{\"id\":%d,\"creator\":{\"userId\":42},"
            "\"trackCount\":100,\"name\":\"Playlist %d\",\"padding\":[",
            item ? "," : "", item, item);
        assert(wrote > 0 && (size_t)wrote < capacity - used);
        used += (size_t)wrote;
        for (int value_index = 0; value_index < 1000; value_index++) {
            wrote = snprintf(playlists + used, capacity - used,
                             "%s%d", value_index ? "," : "", value_index);
            assert(wrote > 0 && (size_t)wrote < capacity - used);
            used += (size_t)wrote;
        }
        wrote = snprintf(playlists + used, capacity - used, "]}");
        assert(wrote > 0 && (size_t)wrote < capacity - used);
        used += (size_t)wrote;
    }
    wrote = snprintf(playlists + used, capacity - used, "]}");
    assert(wrote > 0 && (size_t)wrote < capacity - used);
    whole_tokens = (JsonToken *)calloc(8192, sizeof(JsonToken));
    assert(whole_tokens != NULL);
    assert(json_parse(&doc, playlists, whole_tokens, 8192) == -1);
    free(whole_tokens);
    JsonToken *playlist_item_tokens =
        (JsonToken *)calloc(2048, sizeof(JsonToken));
    assert(playlist_item_tokens != NULL);
    VisitState playlist_state = {0, 0, 9};
    assert(json_visit_array_objects(
        playlists, "playlist", playlist_item_tokens, 2048,
        visit_large_song, &playlist_state) == 9);
    assert(playlist_state.count == 9);
    assert(strstr(playlists, "},{\"id\":1") != NULL);
    free(playlist_item_tokens);
    free(playlists);

    char escaped_object[] =
        "{\"dailySongs\":[{\"id\":7,\"text\":\"} ] \\\" still text\"}]}";
    VisitState escaped_state = {7, 0, 1};
    assert(json_visit_array_objects(
        escaped_object, "dailySongs", item_tokens, 128,
        visit_large_song, &escaped_state) == 1);
    assert(escaped_state.count == 1);

    puts("json tests: ok");
    return 0;
}
