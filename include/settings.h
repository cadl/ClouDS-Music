#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "i18n.h"

typedef struct {
    uint64_t cache_limit;
    AppLanguage language;
    bool debug_logging;
} AppSettings;

void settings_defaults(AppSettings *settings);
int settings_load(const char *path, AppSettings *settings,
                  char *error, size_t error_size);
int settings_save(const char *path, const AppSettings *settings,
                  char *error, size_t error_size);
