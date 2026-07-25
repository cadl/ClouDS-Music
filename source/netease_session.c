#include "netease.h"

#include <stdio.h>
#include <string.h>

_Static_assert(NETEASE_COOKIE_CAPACITY >=
               NETEASE_MUSIC_U_CAPACITY + 512U,
               "NetEase cookie buffer must include device metadata");

static int rebuild_cookie(NeteaseClient *client) {
    if (!client) return -1;
    int length = snprintf(client->cookie, sizeof(client->cookie),
        "osver=16.2; deviceId=%s; os=iPhone%%20OS; appver=9.0.90; "
        "versioncode=140; channel=distribution; _ntes_nuid=%s%s%s",
        client->device_id, client->device_id,
        client->music_u[0] ? "; MUSIC_U=" : "",
        client->music_u);
    if (length < 0 || (size_t)length >= sizeof(client->cookie)) {
        client->cookie[0] = '\0';
        return -1;
    }
    return 0;
}

int netease_set_music_u(NeteaseClient *client, const char *cookie) {
    if (!client || !cookie) return -1;
    if (!cookie[0]) {
        client->music_u[0] = '\0';
        client->nickname[0] = '\0';
        client->user_id = 0;
        return rebuild_cookie(client);
    }
    const char *value = strstr(cookie, "MUSIC_U=");
    value = value ? value + strlen("MUSIC_U=") : cookie;
    while (*value == ' ' || *value == '\t') value++;
    size_t length = 0;
    while (value[length] && value[length] != ';' && value[length] != '\r' &&
           value[length] != '\n' && value[length] != ' ') length++;
    if (length == 0 || length >= sizeof(client->music_u)) return -1;
    memcpy(client->music_u, value, length);
    client->music_u[length] = '\0';
    return rebuild_cookie(client);
}

bool netease_logged_in(const NeteaseClient *client) {
    return client && client->music_u[0];
}
