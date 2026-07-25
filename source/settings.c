#include "settings.h"

#include "cache.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
} SettingsFileV1;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t reserved;
} SettingsFileV2;

typedef struct {
    char magic[4];
    uint32_t version;
    uint64_t cache_limit;
    uint32_t language;
    uint32_t debug_logging;
} SettingsFileV3;

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

void settings_defaults(AppSettings *settings) {
    if (!settings) return;
    settings->cache_limit = NM3DS_CACHE_LIMIT_DEFAULT;
    settings->language = APP_LANGUAGE_CHINESE;
    settings->debug_logging = false;
}

int settings_load(const char *path, AppSettings *settings,
                  char *error, size_t error_size) {
    if (!path || !settings) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return 1;

    SettingsFileV3 saved;
    memset(&saved, 0, sizeof(saved));
    size_t bytes = fread(&saved, 1, sizeof(saved), file);
    bool eof = fgetc(file) == EOF && !ferror(file);
    fclose(file);

    bool common_valid = bytes >= sizeof(SettingsFileV1) && eof &&
                        memcmp(saved.magic, "SETT", 4) == 0 &&
                        cache_limit_option_index(saved.cache_limit) >= 0;
    bool v1_valid = common_valid && saved.version == 1 &&
                    bytes == sizeof(SettingsFileV1);
    bool v2_valid = common_valid && saved.version == 2 &&
                    bytes == sizeof(SettingsFileV2) &&
                    i18n_language_valid((int)saved.language);
    bool v3_valid = common_valid && saved.version == 3 &&
                    bytes == sizeof(SettingsFileV3) &&
                    i18n_language_valid((int)saved.language) &&
                    saved.debug_logging <= 1U;
    bool valid = v1_valid || v2_valid || v3_valid;
    if (!valid) {
        set_error(error, error_size, "保存的设置无效");
        return -1;
    }
    settings->cache_limit = saved.cache_limit;
    settings->language = v2_valid || v3_valid ?
                         (AppLanguage)saved.language : APP_LANGUAGE_CHINESE;
    settings->debug_logging = v3_valid && saved.debug_logging != 0;
    return 0;
}

int settings_save(const char *path, const AppSettings *settings,
                  char *error, size_t error_size) {
    if (!path || !settings ||
        cache_limit_option_index(settings->cache_limit) < 0 ||
        !i18n_language_valid(settings->language))
        return -1;
    char temporary[320];
    int written = snprintf(temporary, sizeof(temporary), "%s.part", path);
    if (written < 0 || (size_t)written >= sizeof(temporary)) {
        set_error(error, error_size, "设置文件路径过长");
        return -1;
    }
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        set_error(error, error_size, "无法保存设置");
        return -1;
    }
    SettingsFileV3 saved;
    memset(&saved, 0, sizeof(saved));
    memcpy(saved.magic, "SETT", 4);
    saved.version = 3;
    saved.cache_limit = settings->cache_limit;
    saved.language = (uint32_t)settings->language;
    saved.debug_logging = settings->debug_logging ? 1U : 0U;
    bool wrote = fwrite(&saved, 1, sizeof(saved), file) == sizeof(saved);
    int close_result = fclose(file);
    if (!wrote || close_result != 0) {
        remove(temporary);
        set_error(error, error_size, "无法写入设置");
        return -1;
    }
    remove(path);
    if (rename(temporary, path) != 0) {
        remove(temporary);
        set_error(error, error_size, "无法提交设置");
        return -1;
    }
    return 0;
}
