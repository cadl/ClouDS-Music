#include "auth.h"

#include "i18n.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AUTH_VERSION_LEGACY 1U
#define AUTH_VERSION_WITH_LARGE_TOKEN 2U
#define AUTH_VERSION_CURRENT 3U
#define LEGACY_MUSIC_U_CAPACITY 512U
#define AUTH_V3_USER_ID_OFFSET 8U
#define AUTH_V3_DEVICE_ID_OFFSET 16U
#define AUTH_V3_MUSIC_U_OFFSET \
    (AUTH_V3_DEVICE_ID_OFFSET + NETEASE_DEVICE_ID_CAPACITY)
#define AUTH_V3_NICKNAME_OFFSET \
    (AUTH_V3_MUSIC_U_OFFSET + NETEASE_MUSIC_U_CAPACITY)
#define AUTH_V3_SIZE (AUTH_V3_NICKNAME_OFFSET + 96U)

typedef struct {
    char magic[4];
    uint32_t version;
} AuthFileHeader;

typedef struct {
    char magic[4];
    uint32_t version;
    char device_id[NETEASE_DEVICE_ID_CAPACITY];
    char music_u[LEGACY_MUSIC_U_CAPACITY];
} AuthFileV1;

typedef struct {
    char magic[4];
    uint32_t version;
    char device_id[NETEASE_DEVICE_ID_CAPACITY];
    char music_u[NETEASE_MUSIC_U_CAPACITY];
} AuthFileV2;

static void put_u64_le(uint8_t output[8], uint64_t value) {
    for (unsigned int i = 0; i < 8; i++)
        output[i] = (uint8_t)(value >> (i * 8U));
}

static uint64_t get_u64_le(const uint8_t input[8]) {
    uint64_t value = 0;
    for (unsigned int i = 0; i < 8; i++)
        value |= (uint64_t)input[i] << (i * 8U);
    return value;
}

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static int apply_auth(NeteaseClient *client,
                      const char *device_id, size_t device_capacity,
                      const char *music_u, size_t music_u_capacity) {
    if (!device_id[0] || !music_u[0] ||
        !memchr(device_id, '\0', device_capacity) ||
        !memchr(music_u, '\0', music_u_capacity)) return -1;
    snprintf(client->device_id, sizeof(client->device_id), "%s", device_id);
    return netease_set_music_u(client, music_u) == 0 ? 0 : -2;
}

int auth_load(NeteaseClient *client, const char *path,
              char *error, size_t error_size) {
    if (!client || !path) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return 1;

    AuthFileHeader header;
    int result = -1;
    bool header_ok = fread(&header, 1, sizeof(header), file) == sizeof(header) &&
                     memcmp(header.magic, "AUTH", 4) == 0 &&
                     fseek(file, 0, SEEK_SET) == 0;
    if (header_ok && header.version == AUTH_VERSION_LEGACY) {
        AuthFileV1 auth;
        if (fread(&auth, 1, sizeof(auth), file) == sizeof(auth) &&
            memcmp(auth.magic, "AUTH", 4) == 0 &&
            auth.version == AUTH_VERSION_LEGACY)
            result = apply_auth(client, auth.device_id, sizeof(auth.device_id),
                                auth.music_u, sizeof(auth.music_u));
    } else if (header_ok &&
               header.version == AUTH_VERSION_WITH_LARGE_TOKEN) {
        AuthFileV2 auth;
        if (fread(&auth, 1, sizeof(auth), file) == sizeof(auth) &&
            memcmp(auth.magic, "AUTH", 4) == 0 &&
            auth.version == AUTH_VERSION_WITH_LARGE_TOKEN)
            result = apply_auth(client, auth.device_id, sizeof(auth.device_id),
                                auth.music_u, sizeof(auth.music_u));
    } else if (header_ok && header.version == AUTH_VERSION_CURRENT) {
        uint8_t auth[AUTH_V3_SIZE];
        if (fread(auth, 1, sizeof(auth), file) == sizeof(auth) &&
            memcmp(auth, "AUTH", 4) == 0 &&
            memchr(auth + AUTH_V3_NICKNAME_OFFSET, '\0', 96U)) {
            result = apply_auth(
                client,
                (const char *)auth + AUTH_V3_DEVICE_ID_OFFSET,
                NETEASE_DEVICE_ID_CAPACITY,
                (const char *)auth + AUTH_V3_MUSIC_U_OFFSET,
                NETEASE_MUSIC_U_CAPACITY);
            int64_t user_id = (int64_t)get_u64_le(
                auth + AUTH_V3_USER_ID_OFFSET);
            if (result == 0 && user_id >= 0) {
                client->user_id = user_id;
                snprintf(client->nickname, sizeof(client->nickname), "%s",
                         (const char *)auth + AUTH_V3_NICKNAME_OFFSET);
            } else if (result == 0) {
                result = -1;
            }
        }
    }
    if (fclose(file) != 0) result = -1;
    if (result != 0)
        set_error(error, error_size, result == -2 ?
                  "保存的 MUSIC_U 无效" : "保存的登录信息无效");
    return result == 0 ? 0 : -1;
}

int auth_save(const NeteaseClient *client, const char *path,
              char *error, size_t error_size) {
    if (!client || !path || !netease_logged_in(client)) return -1;
    char temporary[320];
    int written = snprintf(temporary, sizeof(temporary), "%s.part", path);
    if (written < 0 || (size_t)written >= sizeof(temporary)) return -1;
    uint8_t auth[AUTH_V3_SIZE];
    memset(auth, 0, sizeof(auth));
    memcpy(auth, "AUTH", 4);
    uint32_t version = AUTH_VERSION_CURRENT;
    memcpy(auth + 4, &version, sizeof(version));
    put_u64_le(auth + AUTH_V3_USER_ID_OFFSET, (uint64_t)client->user_id);
    snprintf((char *)auth + AUTH_V3_DEVICE_ID_OFFSET,
             NETEASE_DEVICE_ID_CAPACITY, "%s", client->device_id);
    snprintf((char *)auth + AUTH_V3_MUSIC_U_OFFSET,
             NETEASE_MUSIC_U_CAPACITY, "%s", client->music_u);
    snprintf((char *)auth + AUTH_V3_NICKNAME_OFFSET, 96U, "%s",
             client->nickname);
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        set_error(error, error_size, "无法保存登录信息");
        return -1;
    }
    bool wrote = fwrite(auth, 1, sizeof(auth), file) == sizeof(auth);
    int close_result = fclose(file);
    if (!wrote || close_result != 0) {
        remove(temporary);
        set_error(error, error_size, "无法写入登录信息");
        return -1;
    }
    remove(path);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        set_error(error, error_size, "无法提交登录信息");
        return -1;
    }
    return 0;
}

bool auth_should_clear_after_validation(NeteaseFailure failure) {
    return failure == NETEASE_FAILURE_AUTH_INVALID;
}

void auth_clear(NeteaseClient *client, const char *path) {
    if (path) remove(path);
    if (client) (void)netease_set_music_u(client, "");
}
