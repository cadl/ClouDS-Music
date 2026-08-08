#include "auth.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char magic[4];
    uint32_t version;
    char device_id[NETEASE_DEVICE_ID_CAPACITY];
    char music_u[512];
} LegacyAuthFile;

typedef struct {
    char magic[4];
    uint32_t version;
} AuthHeader;

static void make_token(char *output, size_t length) {
    static const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
    for (size_t i = 0; i < length; i++)
        output[i] = alphabet[i % (sizeof(alphabet) - 1)];
    output[length] = '\0';
}

int main(void) {
    const char *path = "/tmp/nm3ds-auth-test.bin";
    remove(path);

    NeteaseClient client;
    memset(&client, 0, sizeof(client));
    memset(client.device_id, 'd', NETEASE_DEVICE_ID_CAPACITY - 1);
    client.device_id[NETEASE_DEVICE_ID_CAPACITY - 1] = '\0';

    char token[707];
    make_token(token, sizeof(token) - 1);
    char response_cookie[
        sizeof(token) + sizeof("__csrf=test; MUSIC_U=; NMTID=test") - 1];
    snprintf(response_cookie, sizeof(response_cookie),
             "__csrf=test; MUSIC_U=%s; NMTID=test", token);
    assert(netease_set_music_u(&client, response_cookie) == 0);
    assert(strcmp(client.music_u, token) == 0);
    assert(strstr(client.cookie, token) != NULL);
    assert(netease_logged_in(&client));
    client.user_id = 1234567;
    snprintf(client.nickname, sizeof(client.nickname), "Cloud owner");

    char error[192];
    assert(auth_save(&client, path, error, sizeof(error)) == 0);
    FILE *saved = fopen(path, "rb");
    assert(saved != NULL);
    AuthHeader header;
    assert(fread(&header, 1, sizeof(header), saved) == sizeof(header));
    assert(fclose(saved) == 0);
    assert(memcmp(header.magic, "AUTH", 4) == 0);
    assert(header.version == 3);

    NeteaseClient loaded;
    memset(&loaded, 0, sizeof(loaded));
    assert(auth_load(&loaded, path, error, sizeof(error)) == 0);
    assert(strcmp(loaded.device_id, client.device_id) == 0);
    assert(strcmp(loaded.music_u, token) == 0);
    assert(strstr(loaded.cookie, token) != NULL);
    assert(loaded.user_id == client.user_id);
    assert(strcmp(loaded.nickname, client.nickname) == 0);

    char oversized[NETEASE_MUSIC_U_CAPACITY + 1];
    make_token(oversized, NETEASE_MUSIC_U_CAPACITY);
    assert(netease_set_music_u(&loaded, oversized) == -1);
    assert(strcmp(loaded.music_u, token) == 0);

    LegacyAuthFile legacy;
    memset(&legacy, 0, sizeof(legacy));
    memcpy(legacy.magic, "AUTH", 4);
    legacy.version = 1;
    snprintf(legacy.device_id, sizeof(legacy.device_id), "legacy-device");
    make_token(legacy.music_u, 400);
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(&legacy, 1, sizeof(legacy), file) == sizeof(legacy));
    assert(fclose(file) == 0);

    memset(&loaded, 0, sizeof(loaded));
    assert(auth_load(&loaded, path, error, sizeof(error)) == 0);
    assert(strcmp(loaded.device_id, legacy.device_id) == 0);
    assert(strcmp(loaded.music_u, legacy.music_u) == 0);

    assert(auth_should_clear_after_validation(
               NETEASE_FAILURE_AUTH_INVALID));
    assert(!auth_should_clear_after_validation(
               NETEASE_FAILURE_TRANSPORT));
    assert(!auth_should_clear_after_validation(
               NETEASE_FAILURE_TLS_VERIFY));
    assert(!auth_should_clear_after_validation(
               NETEASE_FAILURE_OTHER));
    assert(netease_failure_is_transport(NETEASE_FAILURE_TRANSPORT));
    assert(netease_failure_is_transport(NETEASE_FAILURE_TLS_VERIFY));
    assert(!netease_failure_is_transport(NETEASE_FAILURE_AUTH_INVALID));

    auth_clear(&loaded, path);
    assert(!netease_logged_in(&loaded));
    assert(access(path, F_OK) != 0);
    puts("auth tests: ok");
    return 0;
}
