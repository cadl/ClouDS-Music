#include "diagnostic_text.h"

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

static bool starts_with_ci(const char *text, const char *prefix) {
    if (!text || !prefix) return false;
    while (*prefix) {
        if (!*text ||
            tolower((unsigned char)*text) !=
                tolower((unsigned char)*prefix))
            return false;
        text++;
        prefix++;
    }
    return true;
}

static bool contains_sensitive_marker(const char *text) {
    static const char *const markers[] = {
        "MUSIC_U", "codekey", "cookie", "authorization", "csrf_token",
        "auth_key", "token=", "signature="
    };
    while (text && *text) {
        if (starts_with_ci(text, "https://") ||
            starts_with_ci(text, "http://")) {
            while (*text && !isspace((unsigned char)*text)) text++;
            continue;
        }
        for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++)
            if (starts_with_ci(text, markers[i])) return true;
        text++;
    }
    return false;
}

static bool append_byte(char *output, size_t output_size,
                        size_t *used, char value) {
    if (*used + 1 >= output_size) return false;
    output[(*used)++] = value;
    return true;
}

static bool append_text(char *output, size_t output_size,
                        size_t *used, const char *text) {
    while (*text)
        if (!append_byte(output, output_size, used, *text++)) return false;
    return true;
}

size_t diagnostic_sanitize_detail(char *output, size_t output_size,
                                  const char *input) {
    if (!output || output_size == 0) return 0;
    output[0] = '\0';
    if (!input) input = "";
    if (contains_sensitive_marker(input)) {
        static const char redacted[] = "[redacted sensitive detail]";
        size_t length = sizeof(redacted) - 1;
        if (length >= output_size) length = output_size - 1;
        memcpy(output, redacted, length);
        output[length] = '\0';
        return length;
    }

    size_t used = 0;
    const unsigned char *cursor = (const unsigned char *)input;
    while (*cursor) {
        if (starts_with_ci((const char *)cursor, "https://") ||
            starts_with_ci((const char *)cursor, "http://")) {
            if (!append_text(output, output_size, &used, "[url]")) break;
            while (*cursor && !isspace(*cursor)) cursor++;
            continue;
        }
        if (*cursor == '"' || *cursor == '\\') {
            if (!append_byte(output, output_size, &used, '\\') ||
                !append_byte(output, output_size, &used, (char)*cursor))
                break;
        } else if (*cursor < 0x20 || *cursor == 0x7f) {
            if (!append_byte(output, output_size, &used, ' ')) break;
        } else if (!append_byte(output, output_size, &used, (char)*cursor)) {
            break;
        }
        cursor++;
    }
    output[used] = '\0';
    return used;
}
