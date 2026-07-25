#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool unicode_codepoint_is_hangul(uint32_t codepoint);
bool utf8_contains_hangul(const char *text);
size_t utf8_compose_hangul_nfc(char *text);
size_t utf8_copy_truncated(char *output, size_t output_size,
                           const char *input);
