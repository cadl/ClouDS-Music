#pragma once

#include "search_page.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "i18n.h"

#define NM3DS_MAX_RESULTS 12
#define NM3DS_RECOMMEND_RESULTS 18
#define NM3DS_LIBRARY_PAGE 8
#define NM3DS_LIBRARY_BATCH_PAGE NM3DS_LIBRARY_PAGE
#define NM3DS_CLOUD_PAGE 8
#define NM3DS_CLOUD_FORMAT_CAPACITY 12
#define NM3DS_ALBUM_PAGE 8
#define NM3DS_ALBUM_VISIBLE_ROWS 7
#define NM3DS_ARTIST_PAGE 8
#define NM3DS_ARTIST_VISIBLE_ROWS 7
#define NM3DS_SONG_ARTISTS_MAX 8
#define NM3DS_MAX_QUEUE 1000
#define NM3DS_PREFETCH_SCAN_MAX 16
#define NM3DS_MAX_LYRICS 96

static inline size_t queue_cache_scan_index_for(size_t count, size_t start,
                                                size_t ordinal) {
    if (count == 0 || start >= count || ordinal >= count) return count;
    size_t tail = count - start;
    return ordinal < tail ? start + ordinal : ordinal - tail;
}

static inline size_t queue_cache_scan_unknown_index_for(
    size_t count, size_t start, size_t next_ordinal, const bool *known,
    size_t *ordinal_out) {
    if (!known) return count;
    for (size_t ordinal = next_ordinal; ordinal < count; ordinal++) {
        size_t index = queue_cache_scan_index_for(count, start, ordinal);
        if (index < count && !known[index]) {
            if (ordinal_out) *ordinal_out = ordinal;
            return index;
        }
    }
    if (ordinal_out) *ordinal_out = count;
    return count;
}

typedef enum {
    SONG_FEE_FREE = 0,
    SONG_FEE_VIP = 1,
    SONG_FEE_ALBUM = 4,
    SONG_FEE_LOW_QUALITY_FREE = 8,
    SONG_FEE_UNKNOWN = 255
} SongFee;

typedef struct {
    int64_t id;
    char title[128];
    char artist[96];
    char album[96];
    char pic_url[320];
    uint8_t fee;
    /* Zero for catalog songs. Cloud songs are bound to the account that
       listed them so a cached private upload cannot be reused after an
       account switch. */
    int64_t cloud_owner_user_id;
} Song;

static inline bool song_is_vip(const Song *song) {
    return song && song->fee == SONG_FEE_VIP;
}

static inline bool song_offline_full_allowed(const Song *song,
                                             bool logged_in) {
    return song &&
           song->cloud_owner_user_id == 0 &&
           (logged_in || song->fee == SONG_FEE_FREE ||
            song->fee == SONG_FEE_LOW_QUALITY_FREE);
}

static inline bool song_cloud_access_allowed(const Song *song,
                                             bool logged_in,
                                             int64_t user_id) {
    return song && (song->cloud_owner_user_id == 0 ||
                    (logged_in && user_id > 0 &&
                     song->cloud_owner_user_id == user_id));
}

static inline bool song_offline_full_allowed_for_user(
    const Song *song, bool logged_in, int64_t user_id) {
    if (!song) return false;
    if (song->cloud_owner_user_id > 0)
        return song_cloud_access_allowed(song, logged_in, user_id);
    return song_offline_full_allowed(song, logged_in);
}

typedef struct {
    Song song;
    uint64_t file_size;
    uint32_t bitrate;
    char format[NM3DS_CLOUD_FORMAT_CAPACITY];
} NeteaseCloudTrack;

typedef struct {
    uint32_t time_ms;
    char text[160];
} LyricLine;

typedef enum {
    IMMERSIVE_LYRIC_STYLE_WHEEL = 0,
    IMMERSIVE_LYRIC_STYLE_FLIP,
    IMMERSIVE_LYRIC_STYLE_FADE,
    IMMERSIVE_LYRIC_STYLE_CRAWL,
    IMMERSIVE_LYRIC_STYLE_COUNT
} ImmersiveLyricStyle;

typedef struct {
    int64_t id;
    int64_t creator_id;
    uint32_t track_count;
    bool owned;
    char name[128];
} NeteasePlaylist;

typedef struct {
    int64_t id;
    char name[96];
} NeteaseArtist;

typedef struct {
    int64_t id;
    uint32_t track_count;
    char name[128];
} NeteaseAlbum;

typedef enum {
    TAB_NOW_PLAYING = 0,
    TAB_DISCOVER,
    TAB_SETTINGS,
    TAB_COUNT
} AppTab;

typedef enum {
    DISCOVER_HOME = 0,
    DISCOVER_RECOMMENDATION_SOURCES,
    DISCOVER_RECOMMENDATIONS,
    DISCOVER_LIBRARY,
    DISCOVER_CLOUD,
    DISCOVER_SEARCH
} DiscoverSection;

typedef enum {
    RECOMMEND_SOURCE_PUBLIC = 0,
    RECOMMEND_SOURCE_DAILY,
    RECOMMEND_SOURCE_COUNT
} RecommendationSource;

typedef enum {
    DISCOVER_ITEM_RECOMMENDATIONS = 0,
    DISCOVER_ITEM_LIBRARY,
    DISCOVER_ITEM_CLOUD,
    DISCOVER_ITEM_SEARCH,
    DISCOVER_ITEM_ACCOUNT,
    DISCOVER_ITEM_COUNT
} DiscoverHomeItem;

typedef enum {
    LIBRARY_PLAYLISTS = 0,
    LIBRARY_TRACKS
} LibraryView;

typedef enum {
    BULK_ENQUEUE_NONE = 0,
    BULK_ENQUEUE_LIBRARY,
    BULK_ENQUEUE_RECOMMENDATIONS,
    BULK_ENQUEUE_ALBUM,
    BULK_ENQUEUE_ARTIST_SONGS
} BulkEnqueueKind;

typedef enum {
    APP_FOCUS_CONTENT = 0,
    APP_FOCUS_PLAYLIST
} AppFocus;

typedef enum {
    NOW_PLAYING_DEFAULT = 0,
    NOW_PLAYING_ALBUM,
    NOW_PLAYING_ARTIST_PICKER,
    NOW_PLAYING_ARTIST_ALBUMS,
    NOW_PLAYING_ARTIST_SONGS
} NowPlayingView;

static inline bool now_playing_view_is_artist(NowPlayingView view) {
    return view == NOW_PLAYING_ARTIST_PICKER ||
           view == NOW_PLAYING_ARTIST_ALBUMS ||
           view == NOW_PLAYING_ARTIST_SONGS;
}

static inline bool now_playing_view_has_detail(NowPlayingView view) {
    return view != NOW_PLAYING_DEFAULT;
}

typedef enum {
    LOGIN_CONTINUATION_NONE = 0,
    LOGIN_CONTINUATION_LIBRARY,
    LOGIN_CONTINUATION_CLOUD,
    LOGIN_CONTINUATION_DAILY_RECOMMENDATION
} LoginContinuation;

typedef enum {
    APP_IDLE = 0,
    APP_SEARCHING,
    APP_LOADING_DISCOVER,
    APP_LOADING_LIBRARY,
    APP_LOADING_LIBRARY_TRACKS,
    APP_LOADING_CLOUD,
    APP_LOADING_ALBUM,
    APP_LOADING_ARTIST,
    APP_BULK_ENQUEUE,
    APP_LOADING_EXTRAS,
    APP_RESOLVING,
    APP_DOWNLOADING,
    APP_BUFFERING,
    APP_MANAGING_CACHE,
    APP_PLAYING,
    APP_PAUSED,
    APP_ERROR
} AppMode;

typedef enum {
    SETTINGS_LANGUAGE = 0,
    SETTINGS_CACHE_LIMIT,
    SETTINGS_DEBUG_LOGGING,
    SETTINGS_CACHE_CLEAR,
    SETTINGS_CONTACT,
    SETTINGS_REPOSITORY,
    SETTINGS_USAGE_NOTICE,
    SETTINGS_VERSION,
    SETTINGS_ITEM_COUNT
} SettingsItem;

static inline bool settings_item_is_interactive(int item) {
    return item >= SETTINGS_LANGUAGE && item <= SETTINGS_CACHE_CLEAR;
}

static inline bool settings_item_is_adjustable(int item) {
    return item == SETTINGS_LANGUAGE || item == SETTINGS_CACHE_LIMIT ||
           item == SETTINGS_DEBUG_LOGGING;
}

typedef enum {
    PLAY_MODE_SEQUENCE = 0,
    PLAY_MODE_REPEAT_ONE,
    PLAY_MODE_SHUFFLE,
    PLAY_MODE_COUNT
} PlayMode;

typedef struct {
    AppTab tab;
    AppFocus focus;

    Song discover[NM3DS_RECOMMEND_RESULTS];
    size_t discover_count;
    int discover_selected;
    size_t discover_offset;
    bool discover_has_more;
    size_t discover_total_count;
    bool discover_total_known;
    RecommendationSource discover_source;
    int discover_source_selected;
    size_t discover_saved_offsets[RECOMMEND_SOURCE_COUNT];
    int discover_saved_selections[RECOMMEND_SOURCE_COUNT];
    DiscoverSection discover_section;
    int discover_home_selected;

    LibraryView library_view;
    NeteasePlaylist library_playlists[NM3DS_LIBRARY_PAGE];
    size_t library_playlist_count;
    int library_playlist_selected;
    size_t library_playlist_offset;
    bool library_playlist_has_more;
    int64_t library_open_id;
    char library_open_name[128];
    uint32_t library_open_track_count;
    Song library_tracks[NM3DS_LIBRARY_PAGE];
    size_t library_track_count;
    int library_track_selected;
    size_t library_track_offset;
    bool library_track_has_more;

    NeteaseCloudTrack cloud_tracks[NM3DS_CLOUD_PAGE];
    size_t cloud_track_count;
    int cloud_track_selected;
    size_t cloud_track_offset;
    bool cloud_track_has_more;
    BulkEnqueueKind bulk_enqueue_kind;
    RecommendationSource bulk_enqueue_recommendation_source;
    bool bulk_enqueue_confirm;
    bool bulk_enqueue_active;
    size_t bulk_enqueue_page;
    size_t bulk_enqueue_processed;
    size_t bulk_enqueue_added;
    size_t bulk_enqueue_existing;

    Song search[NM3DS_MAX_RESULTS];
    size_t search_count;
    int search_selected;
    SearchPageState search_page;
    bool search_has_more;

    NowPlayingView now_playing_view;
    NowPlayingView album_return_view;
    int64_t album_id;
    int64_t album_source_song_id;
    char album_name[96];
    Song album_tracks[NM3DS_ALBUM_PAGE];
    size_t album_track_count;
    int album_track_selected;
    int album_track_pending_selected;
    size_t album_track_offset;
    size_t album_track_total;
    bool album_track_has_more;

    int64_t artist_source_song_id;
    NeteaseArtist artist_choices[NM3DS_SONG_ARTISTS_MAX];
    size_t artist_choice_count;
    int artist_choice_selected;
    int64_t artist_id;
    char artist_name[96];
    NeteaseAlbum artist_albums[NM3DS_ARTIST_PAGE];
    size_t artist_album_count;
    int artist_album_selected;
    int artist_album_pending_selected;
    size_t artist_album_offset;
    bool artist_album_has_more;
    Song artist_songs[NM3DS_ARTIST_PAGE];
    size_t artist_song_count;
    int artist_song_selected;
    int artist_song_pending_selected;
    size_t artist_song_offset;
    bool artist_song_has_more;

    Song queue[NM3DS_MAX_QUEUE];
    bool queue_offline_playable[NM3DS_MAX_QUEUE];
    bool queue_cache_known[NM3DS_MAX_QUEUE];
    size_t queue_cache_scan_start;
    size_t queue_cache_scan_next;
    uint32_t queue_cache_scan_generation;
    bool queue_cache_scan_pending;
    bool queue_cache_scan_in_flight;
    size_t queue_count;
    int queue_selected;
    int current_queue;
    int pending_queue;
    int64_t extras_song_id;
    int64_t audio_cached_song_id;
    int64_t extras_cached_song_id;
    int64_t prefetch_anchor_song_id;
    int64_t prefetch_active_song_id;
    size_t prefetch_checked_count;
    bool prefetch_done;
    bool queue_replace_confirm;
    bool queue_replace_stay_on_page;
    Song queue_replace_song;
    bool current_audio_is_trial;
    PlayMode play_mode;
    float volume;
    bool seek_dragging;
    float seek_ratio;
    uint64_t logout_confirm_until;
    uint64_t exit_confirm_until;
    uint64_t cache_limit_confirm_until;
    uint64_t clear_cache_confirm_until;
    uint64_t cache_limit;
    uint64_t cache_bytes;
    size_t cache_audio_files;
    size_t cache_cover_files;
    size_t cache_lyric_files;
    int cache_limit_selected;
    int cache_limit_confirm_choice;
    int settings_selected;
    AppLanguage language;
    bool debug_logging;
    bool dsp_firmware_prompt_open;
    uint32_t dsp_firmware_result;

    bool account_open;
    LoginContinuation login_continuation;
    bool logged_in;
    bool wifi_connected;
    bool network_online;
    bool network_certificate_error;
    bool network_certificate_prompt_open;
    bool network_certificate_prompt_shown;
    bool network_probe_failure_logged;
    bool battery_available;
    bool battery_charging;
    uint8_t battery_level;
    bool login_qr_ready;
    int login_code;
    char login_qr_key[128];
    char nickname[96];
    int64_t user_id;
    uint64_t login_next_poll_ms;

    LyricLine lyrics[NM3DS_MAX_LYRICS];
    size_t lyric_count;
    int64_t lyric_song_id;
    bool immersive_lyrics;
    ImmersiveLyricStyle immersive_lyric_style;
    uint64_t immersive_controls_since_ms;

    char query[96];
    char status[192];
    AppMode mode;
    uint64_t downloaded;
    uint64_t download_total;
    int64_t media_progress_song_id;
    uint64_t media_loaded_bytes;
    uint64_t media_total_bytes;
    uint64_t media_start_target_bytes;
} AppState;
