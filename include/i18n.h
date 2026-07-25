#pragma once

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>

typedef enum {
    APP_LANGUAGE_CHINESE = 0,
    APP_LANGUAGE_ENGLISH,
    APP_LANGUAGE_COUNT
} AppLanguage;

bool i18n_language_valid(int language);
void i18n_set_language(AppLanguage language);
AppLanguage i18n_get_language(void);
const char *i18n_text(const char *chinese);
int i18n_vsnprintf(char *output, size_t output_size,
                   const char *format, va_list args);
int i18n_snprintf(char *output, size_t output_size,
                  const char *format, ...);
