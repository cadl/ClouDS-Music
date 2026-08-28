#pragma once

#include "model.h"
#include "net.h"

#include <stdbool.h>
#include <stddef.h>

#define NETEASE_DEVICE_ID_CAPACITY 65U
#define NETEASE_MUSIC_U_CAPACITY 2048U
#define NETEASE_COOKIE_CAPACITY 2560U

typedef enum {
    NETEASE_FAILURE_NONE = 0,
    NETEASE_FAILURE_CANCELLED,
    NETEASE_FAILURE_TRANSPORT,
    NETEASE_FAILURE_TLS_VERIFY,
    NETEASE_FAILURE_AUTH_INVALID,
    NETEASE_FAILURE_OTHER
} NeteaseFailure;

static inline bool netease_failure_is_transport(NeteaseFailure failure) {
    return failure == NETEASE_FAILURE_TRANSPORT ||
           failure == NETEASE_FAILURE_TLS_VERIFY;
}

typedef struct {
    char device_id[NETEASE_DEVICE_ID_CAPACITY];
    char cookie[NETEASE_COOKIE_CAPACITY];
    char music_u[NETEASE_MUSIC_U_CAPACITY];
    char nickname[96];
    int64_t user_id;
    NetCancel cancel;
    void *cancel_userdata;
    NeteaseFailure last_failure;
} NeteaseClient;

typedef struct {
    bool is_trial;
    char format[NM3DS_CLOUD_FORMAT_CAPACITY];
} NeteasePlaybackInfo;

void netease_init(NeteaseClient *client);
void netease_set_cancel(NeteaseClient *client, NetCancel cancel,
                        void *userdata);
void netease_reset_failure(NeteaseClient *client);
NeteaseFailure netease_last_failure(const NeteaseClient *client);
int netease_set_music_u(NeteaseClient *client, const char *cookie);
bool netease_logged_in(const NeteaseClient *client);
int netease_search(NeteaseClient *client, const char *query,
                   size_t offset, Song *songs, size_t capacity,
                   size_t *count, bool *has_more,
                   char *error, size_t error_size);
int netease_song_detail(NeteaseClient *client, int64_t song_id,
                        Song *song, char *error, size_t error_size);
int netease_song_album_detail(NeteaseClient *client, int64_t song_id,
                              Song *song, int64_t *album_id,
                              char *error, size_t error_size);
int netease_song_artists(NeteaseClient *client, int64_t song_id,
                         NeteaseArtist *artists, size_t capacity,
                         size_t *count, char *error, size_t error_size);
int netease_discover(NeteaseClient *client, size_t offset,
                     Song *songs, size_t capacity, size_t *count,
                     bool *has_more, size_t *total_count,
                     bool *total_known,
                     char *error, size_t error_size);
int netease_recommend(NeteaseClient *client, size_t offset,
                      Song *songs, size_t capacity, size_t *count,
                      bool *has_more, size_t *total_count,
                      bool *total_known,
                      char *error, size_t error_size);
int netease_user_playlists(NeteaseClient *client, size_t offset,
                           NeteasePlaylist *playlists, size_t capacity,
                           size_t *count, bool *has_more,
                           char *error, size_t error_size);
int netease_user_cloud(NeteaseClient *client, size_t offset,
                       NeteaseCloudTrack *tracks, size_t capacity,
                       size_t *count, bool *has_more,
                       char *error, size_t error_size);
int netease_playlist_tracks(NeteaseClient *client, int64_t playlist_id,
                            size_t offset, bool refresh_index,
                            Song *songs, size_t capacity,
                            size_t *count, bool *has_more,
                            size_t *total_count,
                            char *error, size_t error_size);
int netease_album_tracks(NeteaseClient *client, int64_t album_id,
                         size_t offset, bool refresh_index,
                         Song *songs, size_t capacity,
                         size_t *count, bool *has_more,
                         size_t *total_count,
                         char *error, size_t error_size);
int netease_artist_albums(NeteaseClient *client, int64_t artist_id,
                          size_t offset, NeteaseAlbum *albums,
                          size_t capacity, size_t *count, bool *has_more,
                          char *error, size_t error_size);
int netease_artist_songs(NeteaseClient *client, int64_t artist_id,
                         size_t offset, Song *songs, size_t capacity,
                         size_t *count, bool *has_more,
                         char *error, size_t error_size);
int netease_lyrics(NeteaseClient *client, int64_t song_id,
                   LyricLine *lines, size_t capacity,
                   size_t *count, char *error, size_t error_size);
int netease_song_url(NeteaseClient *client, int64_t song_id,
                     char *url, size_t url_size,
                     NeteasePlaybackInfo *playback,
                     char *error, size_t error_size);
int netease_login_qr_key(NeteaseClient *client, char *key, size_t key_size,
                         char *error, size_t error_size);
int netease_login_qr_check(NeteaseClient *client, const char *key,
                           int *status_code, char *message,
                           size_t message_size,
                           char *error, size_t error_size);
int netease_account(NeteaseClient *client,
                    char *error, size_t error_size);
