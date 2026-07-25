#include "playlist_index.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void write_chunks(PlaylistTrackIndexWriter *writer,
                         const char *json, size_t chunk) {
    char error[192] = {0};
    size_t length = strlen(json);
    for (size_t offset = 0; offset < length; offset += chunk) {
        size_t size = length - offset;
        if (size > chunk) size = chunk;
        assert(playlist_track_index_writer_write(
                   writer, (const uint8_t *)json + offset, size,
                   error, sizeof(error)) == 0);
    }
}

int main(void) {
    const char *path = "/tmp/nm3ds-playlist-tracks-test.bin";
    char part[128];
    char backup[128];
    snprintf(part, sizeof(part), "%s.part", path);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    remove(path);
    remove(part);
    remove(backup);

    char error[192] = {0};
    PlaylistTrackIndexWriter *writer = playlist_track_index_writer_create(
        path, 1234, error, sizeof(error));
    assert(writer != NULL);
    const char *first =
        "{\"note\":\"trackIds is not a key: [{\\\"id\\\":99}]\","
        "\"playlist\":{\"trackIds\":["
        "{\"id\":11,\"label\":\"a } ] { string\"},"
        "{\"nested\":{\"id\":999},\"id\":22},"
        "{\"id\":33,\"metadata\":[{\"id\":888}]}]}}";
    write_chunks(writer, first, 1);
    size_t total = 0;
    assert(playlist_track_index_writer_commit(
               writer, &total, error, sizeof(error)) == 0);
    assert(total == 3);
    playlist_track_index_writer_destroy(writer);
    assert(access(part, F_OK) != 0);

    int64_t ids[2] = {0};
    size_t count = 0;
    bool has_more = false;
    total = 0;
    assert(playlist_track_index_read_page(
               path, 1234, 0, ids, 2, &count, &has_more, &total,
               error, sizeof(error)) == 0);
    assert(count == 2 && ids[0] == 11 && ids[1] == 22);
    assert(has_more && total == 3);
    assert(playlist_track_index_read_page(
               path, 1234, 2, ids, 2, &count, &has_more, &total,
               error, sizeof(error)) == 0);
    assert(count == 1 && ids[0] == 33 && !has_more && total == 3);
    assert(playlist_track_index_read_page(
               path, 9999, 0, ids, 2, &count, &has_more, &total,
               error, sizeof(error)) < 0);

    writer = playlist_track_index_writer_create(
        path, 5678, error, sizeof(error));
    assert(writer != NULL);
    write_chunks(writer, "{\"playlist\":{\"trackIds\":[{\"id\":44}]", 7);
    assert(playlist_track_index_writer_reset(
               writer, error, sizeof(error)) == 0);
    write_chunks(writer,
                 "{\"playlist\":{\"trackIds\":[{\"id\":55},{\"id\":66}]}}",
                 5);
    assert(playlist_track_index_writer_commit(
               writer, &total, error, sizeof(error)) == 0);
    assert(total == 2);
    playlist_track_index_writer_destroy(writer);
    assert(playlist_track_index_read_page(
               path, 5678, 0, ids, 2, &count, &has_more, &total,
               error, sizeof(error)) == 0);
    assert(count == 2 && ids[0] == 55 && ids[1] == 66 && !has_more);

    writer = playlist_track_index_writer_create(
        path, 7777, error, sizeof(error));
    assert(writer != NULL);
    write_chunks(writer, "{\"playlist\":{\"trackIds\":[{\"id\":77}", 3);
    assert(playlist_track_index_writer_commit(
               writer, &total, error, sizeof(error)) < 0);
    playlist_track_index_writer_destroy(writer);
    assert(access(part, F_OK) != 0);
    assert(playlist_track_index_read_page(
               path, 5678, 0, ids, 2, &count, &has_more, &total,
               error, sizeof(error)) == 0);

    writer = playlist_track_index_writer_create(
        path, 6789, error, sizeof(error));
    assert(writer != NULL);
    write_chunks(writer, "{\"padding\":\"", 4);
    uint8_t padding[32768];
    memset(padding, 'a', sizeof(padding));
    for (int i = 0; i < 65; i++)
        assert(playlist_track_index_writer_write(
                   writer, padding, sizeof(padding),
                   error, sizeof(error)) == 0);
    write_chunks(writer,
                 "\",\"playlist\":{\"trackIds\":[{\"id\":77},{\"id\":88}]}}",
                 9);
    assert(playlist_track_index_writer_commit(
               writer, &total, error, sizeof(error)) == 0);
    assert(total == 2);
    playlist_track_index_writer_destroy(writer);
    assert(playlist_track_index_read_page(
               path, 6789, 0, ids, 2, &count, &has_more, &total,
               error, sizeof(error)) == 0);
    assert(count == 2 && ids[0] == 77 && ids[1] == 88 && !has_more);

    writer = playlist_track_index_writer_create(
        path, 9999, error, sizeof(error));
    assert(writer != NULL);
    write_chunks(writer, "{\"playlist\":{\"trackIds\":[", 8);
    char item[48];
    for (uint32_t i = 1; i <= PLAYLIST_TRACK_INDEX_MAX_TRACKS; i++) {
        int written = snprintf(item, sizeof(item),
                               "{\"id\":%u},", (unsigned int)i);
        assert(written > 0 && (size_t)written < sizeof(item));
        assert(playlist_track_index_writer_write(
                   writer, (const uint8_t *)item, (size_t)written,
                   error, sizeof(error)) == 0);
    }
    int written = snprintf(
        item, sizeof(item), "{\"id\":%u}]}}",
        (unsigned int)(PLAYLIST_TRACK_INDEX_MAX_TRACKS + 1U));
    assert(written > 0 && (size_t)written < sizeof(item));
    assert(playlist_track_index_writer_write(
               writer, (const uint8_t *)item, (size_t)written,
               error, sizeof(error)) < 0);
    playlist_track_index_writer_destroy(writer);
    assert(access(part, F_OK) != 0);
    assert(playlist_track_index_read_page(
               path, 6789, 0, ids, 2, &count, &has_more, &total,
               error, sizeof(error)) == 0);

    FILE *corrupt = fopen(path, "ab");
    assert(corrupt != NULL);
    assert(fputc('x', corrupt) == 'x');
    assert(fclose(corrupt) == 0);
    assert(playlist_track_index_read_page(
               path, 6789, 0, ids, 2, &count, &has_more, &total,
               error, sizeof(error)) < 0);

    writer = playlist_track_index_writer_create_for_array(
        path, 6000, "songs", error, sizeof(error));
    assert(writer != NULL);
    write_chunks(writer,
                 "{\"album\":{\"id\":6000},\"songs\":["
                 "{\"id\":101,\"ar\":[{\"id\":1}]},"
                 "{\"name\":\"trackIds\",\"id\":102}]}",
                 3);
    assert(playlist_track_index_writer_commit(
               writer, &total, error, sizeof(error)) == 0);
    assert(total == 2);
    playlist_track_index_writer_destroy(writer);
    assert(playlist_track_index_read_page(
               path, 6000, 0, ids, 2, &count, &has_more, &total,
               error, sizeof(error)) == 0);
    assert(count == 2 && ids[0] == 101 && ids[1] == 102 && !has_more);

    remove(path);
    remove(part);
    remove(backup);
    puts("playlist index tests: ok");
    return 0;
}
