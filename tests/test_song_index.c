#include "song_index.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static Song make_song(int64_t id, const char *title, uint8_t fee) {
    Song song;
    memset(&song, 0, sizeof(song));
    song.id = id;
    snprintf(song.title, sizeof(song.title), "%s", title);
    snprintf(song.artist, sizeof(song.artist), "Artist %lld", (long long)id);
    snprintf(song.album, sizeof(song.album), "Album");
    snprintf(song.pic_url, sizeof(song.pic_url), "https://img/%lld",
             (long long)id);
    song.fee = fee;
    return song;
}

int main(void) {
    const char *path = "/tmp/nm3ds-song-index-test.bin";
    char part[128];
    char backup[128];
    snprintf(part, sizeof(part), "%s.part", path);
    snprintf(backup, sizeof(backup), "%s.bak", path);
    remove(path);
    remove(part);
    remove(backup);

    char error[192] = {0};
    SongIndexWriter *writer = song_index_writer_create(
        path, 1234, error, sizeof(error));
    assert(writer != NULL);
    Song first = make_song(11, "First", SONG_FEE_FREE);
    Song second = make_song(22, "第二首", SONG_FEE_VIP);
    Song third = make_song(33, "Third", SONG_FEE_UNKNOWN);
    assert(song_index_writer_append(writer, &first,
                                    error, sizeof(error)) == 0);
    assert(song_index_writer_append(writer, &second,
                                    error, sizeof(error)) == 0);
    assert(song_index_writer_append(writer, &third,
                                    error, sizeof(error)) == 0);
    size_t total = 0;
    assert(song_index_writer_commit(writer, &total,
                                    error, sizeof(error)) == 0);
    assert(total == 3);
    song_index_writer_destroy(writer);
    assert(access(part, F_OK) != 0);

    Song page[2];
    size_t count = 0;
    bool has_more = false;
    assert(song_index_read_page(path, 1234, 0, page, 2, &count,
                                &has_more, &total,
                                error, sizeof(error)) == 0);
    assert(count == 2 && has_more && total == 3);
    assert(page[0].id == 11 && strcmp(page[0].title, "First") == 0);
    assert(page[1].id == 22 && strcmp(page[1].title, "第二首") == 0);
    assert(page[1].fee == SONG_FEE_VIP);
    assert(song_index_read_page(path, 1234, 2, page, 2, &count,
                                &has_more, &total,
                                error, sizeof(error)) == 0);
    assert(count == 1 && !has_more && page[0].id == 33);
    assert(song_index_read_page(path, 9999, 0, page, 2, &count,
                                &has_more, &total,
                                error, sizeof(error)) < 0);

    writer = song_index_writer_create(path, 5678, error, sizeof(error));
    assert(writer != NULL);
    Song replacement = make_song(44, "Replacement", SONG_FEE_ALBUM);
    assert(song_index_writer_append(writer, &replacement,
                                    error, sizeof(error)) == 0);
    assert(song_index_writer_commit(writer, &total,
                                    error, sizeof(error)) == 0);
    song_index_writer_destroy(writer);
    assert(song_index_read_page(path, 5678, 0, page, 2, &count,
                                &has_more, &total,
                                error, sizeof(error)) == 0);
    assert(count == 1 && page[0].id == 44 && !has_more);

    writer = song_index_writer_create(path, 6789, error, sizeof(error));
    assert(writer != NULL);
    Song abandoned = make_song(55, "Abandoned", SONG_FEE_FREE);
    assert(song_index_writer_append(writer, &abandoned,
                                    error, sizeof(error)) == 0);
    song_index_writer_destroy(writer);
    assert(access(part, F_OK) != 0);
    assert(song_index_read_page(path, 5678, 0, page, 2, &count,
                                &has_more, &total,
                                error, sizeof(error)) == 0);
    assert(count == 1 && page[0].id == 44);

    FILE *corrupt = fopen(path, "ab");
    assert(corrupt != NULL);
    assert(fputc('x', corrupt) == 'x');
    assert(fclose(corrupt) == 0);
    assert(song_index_read_page(path, 5678, 0, page, 2, &count,
                                &has_more, &total,
                                error, sizeof(error)) < 0);

    remove(path);
    remove(part);
    remove(backup);
    puts("song index tests: ok");
    return 0;
}
