#include "cloud_data.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char format[NM3DS_CLOUD_FORMAT_CAPACITY];
    cloud_format_from_filename("Folder/Track.FLAC", format);
    assert(strcmp(format, "flac") == 0);
    cloud_format_from_filename("Track.Mp3", format);
    assert(strcmp(format, "mp3") == 0);
    cloud_format_from_filename("no-extension", format);
    assert(format[0] == '\0');
    cloud_format_from_filename("bad.m4a?token", format);
    assert(format[0] == '\0');

    char json[] =
        "{\"data\":["
        "{\"simpleSong\":{\"id\":101,\"name\":\"Cloud FLAC\","
        "\"ar\":[{\"name\":\"Artist A\"}],"
        "\"al\":{\"name\":\"Album A\","
        "\"picUrl\":\"https://example.com/a.jpg\"},\"fee\":8},"
        "\"fileName\":\"Cloud.FLAC\",\"fileSize\":12582912,"
        "\"bitrate\":900000},"
        "{\"songId\":102,\"songName\":\"Fallback\","
        "\"artist\":\"Artist B\",\"album\":\"Album B\","
        "\"cover\":\"https://example.com/b.jpg\","
        "\"fileName\":\"Fallback.MP3\",\"fileSize\":3145728},"
        "{\"simpleSong\":{\"id\":103,\"name\":\"More\","
        "\"ar\":[],\"al\":{}},\"fileName\":\"More.mp3\"}]}";
    NeteaseCloudTrack tracks[2];
    size_t count = 0;
    bool has_more = false;
    char error[192];
    assert(cloud_parse_response(json, 9001, tracks, 2,
                                &count, &has_more,
                                error, sizeof(error)) == 0);
    assert(count == 2);
    assert(has_more);
    assert(tracks[0].song.id == 101);
    assert(tracks[0].song.cloud_owner_user_id == 9001);
    assert(strcmp(tracks[0].song.title, "Cloud FLAC") == 0);
    assert(strcmp(tracks[0].song.artist, "Artist A") == 0);
    assert(strcmp(tracks[0].song.album, "Album A") == 0);
    assert(strcmp(tracks[0].format, "flac") == 0);
    assert(tracks[0].file_size == 12582912);
    assert(tracks[0].bitrate == 900000);
    assert(tracks[1].song.id == 102);
    assert(strcmp(tracks[1].song.title, "Fallback") == 0);
    assert(strcmp(tracks[1].song.artist, "Artist B") == 0);
    assert(strcmp(tracks[1].format, "mp3") == 0);

    char skipped[] =
        "{\"data\":["
        "{\"songId\":201,\"songName\":\"First\","
        "\"fileName\":\"First.mp3\"},"
        "{\"songId\":0,\"fileName\":\"Invalid.flac\"},"
        "{\"songId\":203,\"songName\":\"Next page\","
        "\"fileName\":\"Next.flac\"}]}";
    count = 0;
    has_more = false;
    assert(cloud_parse_response(skipped, 9001, tracks, 2,
                                &count, &has_more,
                                error, sizeof(error)) == 0);
    assert(count == 1);
    assert(has_more);
    assert(tracks[0].song.id == 201);

    char empty[] = "{\"data\":[],\"hasMore\":false}";
    count = 99;
    has_more = true;
    assert(cloud_parse_response(empty, 9001, tracks, 2,
                                &count, &has_more,
                                error, sizeof(error)) == 0);
    assert(count == 0 && !has_more);

    char invalid[] = "{\"code\":301}";
    assert(cloud_parse_response(invalid, 9001, tracks, 2,
                                &count, &has_more,
                                error, sizeof(error)) == -1);
    assert(strstr(error, "云盘") != NULL);
    puts("cloud data tests: ok");
    return 0;
}
