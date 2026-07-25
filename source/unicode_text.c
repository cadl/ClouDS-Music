#include "unicode_text.h"

#include <string.h>

#define HANGUL_SYLLABLE_BASE 0xAC00U
#define HANGUL_LEADING_BASE 0x1100U
#define HANGUL_VOWEL_BASE 0x1161U
#define HANGUL_TRAILING_BASE 0x11A7U
#define HANGUL_LEADING_COUNT 19U
#define HANGUL_VOWEL_COUNT 21U
#define HANGUL_TRAILING_COUNT 28U
#define HANGUL_PER_LEADING \
    (HANGUL_VOWEL_COUNT * HANGUL_TRAILING_COUNT)
#define HANGUL_SYLLABLE_COUNT \
    (HANGUL_LEADING_COUNT * HANGUL_PER_LEADING)

static size_t utf8_decode(const unsigned char *input, uint32_t *codepoint) {
    if (!input || !input[0] || !codepoint) return 0;
    uint32_t value;
    size_t length;
    if (input[0] < 0x80U) {
        *codepoint = input[0];
        return 1;
    } else if (input[0] >= 0xC2U && input[0] <= 0xDFU) {
        value = input[0] & 0x1FU;
        length = 2;
    } else if (input[0] >= 0xE0U && input[0] <= 0xEFU) {
        value = input[0] & 0x0FU;
        length = 3;
    } else if (input[0] >= 0xF0U && input[0] <= 0xF4U) {
        value = input[0] & 0x07U;
        length = 4;
    } else return 0;

    for (size_t i = 1; i < length; i++) {
        if (!input[i] || (input[i] & 0xC0U) != 0x80U) return 0;
        value = (value << 6) | (input[i] & 0x3FU);
    }
    if ((length == 3 && value < 0x800U) ||
        (length == 4 && value < 0x10000U) ||
        (value >= 0xD800U && value <= 0xDFFFU) || value > 0x10FFFFU)
        return 0;
    *codepoint = value;
    return length;
}

static size_t utf8_encode(char *output, uint32_t codepoint) {
    if (codepoint <= 0x7FU) {
        output[0] = (char)codepoint;
        return 1;
    }
    if (codepoint <= 0x7FFU) {
        output[0] = (char)(0xC0U | (codepoint >> 6));
        output[1] = (char)(0x80U | (codepoint & 0x3FU));
        return 2;
    }
    if (codepoint <= 0xFFFFU) {
        output[0] = (char)(0xE0U | (codepoint >> 12));
        output[1] = (char)(0x80U | ((codepoint >> 6) & 0x3FU));
        output[2] = (char)(0x80U | (codepoint & 0x3FU));
        return 3;
    }
    output[0] = (char)(0xF0U | (codepoint >> 18));
    output[1] = (char)(0x80U | ((codepoint >> 12) & 0x3FU));
    output[2] = (char)(0x80U | ((codepoint >> 6) & 0x3FU));
    output[3] = (char)(0x80U | (codepoint & 0x3FU));
    return 4;
}

bool unicode_codepoint_is_hangul(uint32_t codepoint) {
    return (codepoint >= 0x1100U && codepoint <= 0x11FFU) ||
           (codepoint >= 0x3130U && codepoint <= 0x318FU) ||
           (codepoint >= 0xA960U && codepoint <= 0xA97FU) ||
           (codepoint >= 0xAC00U && codepoint <= 0xD7A3U) ||
           (codepoint >= 0xD7B0U && codepoint <= 0xD7FFU) ||
           (codepoint >= 0xFFA0U && codepoint <= 0xFFDCU);
}

bool utf8_contains_hangul(const char *text) {
    const unsigned char *cursor = (const unsigned char *)text;
    while (cursor && *cursor) {
        uint32_t codepoint;
        size_t length = utf8_decode(cursor, &codepoint);
        if (!length) {
            cursor++;
            continue;
        }
        if (unicode_codepoint_is_hangul(codepoint)) return true;
        cursor += length;
    }
    return false;
}

size_t utf8_compose_hangul_nfc(char *text) {
    if (!text) return 0;
    const unsigned char *read = (const unsigned char *)text;
    char *write = text;
    while (*read) {
        uint32_t codepoint;
        size_t length = utf8_decode(read, &codepoint);
        if (!length) {
            *write++ = '?';
            read++;
            continue;
        }
        read += length;

        if (codepoint >= HANGUL_LEADING_BASE &&
            codepoint < HANGUL_LEADING_BASE + HANGUL_LEADING_COUNT) {
            uint32_t vowel;
            size_t vowel_length = utf8_decode(read, &vowel);
            if (vowel_length && vowel >= HANGUL_VOWEL_BASE &&
                vowel < HANGUL_VOWEL_BASE + HANGUL_VOWEL_COUNT) {
                codepoint = HANGUL_SYLLABLE_BASE +
                    (codepoint - HANGUL_LEADING_BASE) * HANGUL_PER_LEADING +
                    (vowel - HANGUL_VOWEL_BASE) * HANGUL_TRAILING_COUNT;
                read += vowel_length;
            }
        }

        uint32_t syllable_offset = codepoint - HANGUL_SYLLABLE_BASE;
        if (syllable_offset < HANGUL_SYLLABLE_COUNT &&
            syllable_offset % HANGUL_TRAILING_COUNT == 0) {
            uint32_t trailing;
            size_t trailing_length = utf8_decode(read, &trailing);
            if (trailing_length && trailing > HANGUL_TRAILING_BASE &&
                trailing < HANGUL_TRAILING_BASE + HANGUL_TRAILING_COUNT) {
                codepoint += trailing - HANGUL_TRAILING_BASE;
                read += trailing_length;
            }
        }
        write += utf8_encode(write, codepoint);
    }
    *write = '\0';
    return (size_t)(write - text);
}

size_t utf8_copy_truncated(char *output, size_t output_size,
                           const char *input) {
    if (!output || output_size == 0) return 0;
    output[0] = '\0';
    if (!input) return 0;

    size_t used = 0;
    const unsigned char *cursor = (const unsigned char *)input;
    while (*cursor) {
        uint32_t codepoint;
        size_t input_length = utf8_decode(cursor, &codepoint);
        if (!input_length) {
            if (used + 2 > output_size) break;
            output[used++] = '?';
            cursor++;
            continue;
        }
        char encoded[4];
        size_t output_length = utf8_encode(encoded, codepoint);
        if (used + output_length + 1 > output_size) break;
        memcpy(output + used, encoded, output_length);
        used += output_length;
        cursor += input_length;
    }
    output[used] = '\0';
    return used;
}
