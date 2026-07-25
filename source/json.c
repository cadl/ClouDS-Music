#include "json.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int token_new(JsonDoc *doc, int capacity, JsonType type,
                     int start, int parent) {
    if (doc->count >= capacity) return -1;
    int index = doc->count++;
    JsonToken *token = &doc->tokens[index];
    token->type = type;
    token->start = start;
    token->end = -1;
    token->size = 0;
    token->parent = parent;
    if (parent >= 0) doc->tokens[parent].size++;
    return index;
}

static int parse_string(JsonDoc *doc, int capacity, int *position,
                        int parent) {
    const char *text = doc->text;
    int start = *position + 1;
    for (int pos = start; text[pos]; pos++) {
        unsigned char c = (unsigned char)text[pos];
        if (c == '"') {
            int index = token_new(doc, capacity, JSON_STRING, start, parent);
            if (index < 0) return -1;
            doc->tokens[index].end = pos;
            *position = pos;
            return index;
        }
        if (c < 0x20) return -2;
        if (c != '\\') continue;
        c = (unsigned char)text[++pos];
        if (!c) return -2;
        if (c == '"' || c == '/' || c == '\\' || c == 'b' || c == 'f' ||
            c == 'n' || c == 'r' || c == 't') continue;
        if (c != 'u') return -2;
        for (int i = 0; i < 4; i++) {
            c = (unsigned char)text[++pos];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F'))) return -2;
        }
    }
    return -2;
}

static int parse_primitive(JsonDoc *doc, int capacity, int *position,
                           int parent) {
    const char *text = doc->text;
    int start = *position;
    int pos = start;
    while (text[pos] && text[pos] != ' ' && text[pos] != '\t' &&
           text[pos] != '\r' && text[pos] != '\n' && text[pos] != ',' &&
           text[pos] != ']' && text[pos] != '}') {
        unsigned char c = (unsigned char)text[pos];
        if (c < 0x20 || c == ':' || c == '[' || c == '{' || c == '"')
            return -2;
        pos++;
    }
    if (pos == start) return -2;
    int index = token_new(doc, capacity, JSON_PRIMITIVE, start, parent);
    if (index < 0) return -1;
    doc->tokens[index].end = pos;
    *position = pos - 1;
    return index;
}

int json_parse(JsonDoc *doc, const char *text, JsonToken *tokens, int capacity) {
    if (!doc || !text || !tokens || capacity <= 0) return -2;
    doc->text = text;
    doc->tokens = tokens;
    doc->count = 0;
    int parent = -1;

    for (int pos = 0; text[pos]; pos++) {
        char c = text[pos];
        if (c == '{' || c == '[') {
            JsonType type = c == '{' ? JSON_OBJECT : JSON_ARRAY;
            int index = token_new(doc, capacity, type, pos, parent);
            if (index < 0) return -1;
            parent = index;
        } else if (c == '}' || c == ']') {
            JsonType expected = c == '}' ? JSON_OBJECT : JSON_ARRAY;
            if (parent < 0 || doc->tokens[parent].type != expected) return -2;
            doc->tokens[parent].end = pos + 1;
            parent = doc->tokens[parent].parent;
        } else if (c == '"') {
            int result = parse_string(doc, capacity, &pos, parent);
            if (result < 0) return result;
        } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
                   c == ':' || c == ',') {
            continue;
        } else {
            int result = parse_primitive(doc, capacity, &pos, parent);
            if (result < 0) return result;
        }
    }
    if (parent >= 0 || doc->count == 0) return -2;
    return doc->count;
}

static char *find_key_array(char *text, const char *key) {
    size_t key_length = strlen(key);
    for (char *cursor = text; *cursor; cursor++) {
        if (*cursor != '"') continue;
        char *start = cursor + 1;
        char *end = start;
        bool escaped = false;
        while (*end) {
            if (escaped) escaped = false;
            else if (*end == '\\') escaped = true;
            else if (*end == '"') break;
            end++;
        }
        if (!*end) return NULL;
        bool matches = !memchr(start, '\\', (size_t)(end - start)) &&
                       (size_t)(end - start) == key_length &&
                       memcmp(start, key, key_length) == 0;
        cursor = end;
        if (!matches) continue;
        char *value = end + 1;
        while (*value == ' ' || *value == '\t' ||
               *value == '\r' || *value == '\n') value++;
        if (*value != ':') continue;
        value++;
        while (*value == ' ' || *value == '\t' ||
               *value == '\r' || *value == '\n') value++;
        if (*value == '[') return value + 1;
    }
    return NULL;
}

static int next_array_object(char **cursor, char **object, size_t *length) {
    char *current = *cursor;
    while (*current == ' ' || *current == '\t' || *current == '\r' ||
           *current == '\n' || *current == ',') current++;
    if (*current == ']') {
        *cursor = current + 1;
        return 0;
    }
    if (*current != '{') return JSON_VISIT_INVALID;

    char *start = current;
    int object_depth = 0;
    int array_depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (; *current; current++) {
        char c = *current;
        if (in_string) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') in_string = false;
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '{') {
            object_depth++;
        } else if (c == '}') {
            if (--object_depth < 0) return JSON_VISIT_INVALID;
            if (object_depth == 0) {
                if (array_depth != 0) return JSON_VISIT_INVALID;
                *object = start;
                *length = (size_t)(current - start + 1);
                *cursor = current + 1;
                return 1;
            }
        } else if (c == '[') {
            array_depth++;
        } else if (c == ']') {
            if (--array_depth < 0) return JSON_VISIT_INVALID;
        }
    }
    return JSON_VISIT_INVALID;
}

int json_visit_array_objects(char *text, const char *key,
                             JsonToken *tokens, int capacity,
                             JsonObjectVisitor visitor, void *userdata) {
    if (!text || !key || !key[0] || !tokens || capacity <= 0 || !visitor)
        return JSON_VISIT_INVALID;
    char *cursor = find_key_array(text, key);
    if (!cursor) return JSON_VISIT_NOT_FOUND;

    int visited = 0;
    for (;;) {
        char *object = NULL;
        size_t length = 0;
        int next = next_array_object(&cursor, &object, &length);
        if (next <= 0) return next == 0 ? visited : next;
        char saved = object[length];
        object[length] = '\0';
        JsonDoc doc;
        int parsed = json_parse(&doc, object, tokens, capacity);
        if (parsed < 0) {
            object[length] = saved;
            return parsed == -1 ? JSON_VISIT_TOKENS_EXHAUSTED :
                                  JSON_VISIT_INVALID;
        }
        int visit = visitor(&doc, userdata);
        object[length] = saved;
        visited++;
        if (visit > 0) return visited;
        if (visit < 0) return JSON_VISIT_CALLBACK_FAILED;
    }
}

static int raw_equals(const JsonDoc *doc, int token, const char *value) {
    if (!doc || token < 0 || token >= doc->count || !value) return 0;
    const JsonToken *t = &doc->tokens[token];
    size_t len = (size_t)(t->end - t->start);
    return strlen(value) == len &&
           memcmp(doc->text + t->start, value, len) == 0;
}

int json_obj_get(const JsonDoc *doc, int object, const char *key) {
    if (!doc || object < 0 || object >= doc->count ||
        doc->tokens[object].type != JSON_OBJECT) return -1;
    int pending_key = -1;
    for (int i = object + 1; i < doc->count; i++) {
        const JsonToken *token = &doc->tokens[i];
        if (token->start >= doc->tokens[object].end) break;
        if (token->parent != object) continue;
        if (pending_key < 0) {
            if (token->type != JSON_STRING) return -1;
            pending_key = i;
        } else {
            if (raw_equals(doc, pending_key, key)) return i;
            pending_key = -1;
        }
    }
    return -1;
}

int json_arr_get(const JsonDoc *doc, int array, int index) {
    if (!doc || array < 0 || array >= doc->count || index < 0 ||
        doc->tokens[array].type != JSON_ARRAY) return -1;
    int found = 0;
    for (int i = array + 1; i < doc->count; i++) {
        const JsonToken *token = &doc->tokens[i];
        if (token->start >= doc->tokens[array].end) break;
        if (token->parent == array) {
            if (found == index) return i;
            found++;
        }
    }
    return -1;
}

int json_arr_size(const JsonDoc *doc, int array) {
    if (!doc || array < 0 || array >= doc->count ||
        doc->tokens[array].type != JSON_ARRAY) return -1;
    int count = 0;
    for (int i = array + 1; i < doc->count; i++) {
        if (doc->tokens[i].start >= doc->tokens[array].end) break;
        if (doc->tokens[i].parent == array) count++;
    }
    return count;
}

static int hex4(const char *text, uint32_t *value) {
    uint32_t result = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char c = (unsigned char)text[i];
        uint32_t digit;
        if (c >= '0' && c <= '9') digit = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') digit = (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = (uint32_t)(c - 'A' + 10);
        else return -1;
        result = (result << 4) | digit;
    }
    *value = result;
    return 0;
}

static int append_utf8(char *out, size_t out_size, size_t *used, uint32_t cp) {
    unsigned char bytes[4];
    size_t count;
    if (cp <= 0x7f) {
        bytes[0] = (unsigned char)cp; count = 1;
    } else if (cp <= 0x7ff) {
        bytes[0] = (unsigned char)(0xc0 | (cp >> 6));
        bytes[1] = (unsigned char)(0x80 | (cp & 0x3f)); count = 2;
    } else if (cp <= 0xffff) {
        bytes[0] = (unsigned char)(0xe0 | (cp >> 12));
        bytes[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | (cp & 0x3f)); count = 3;
    } else if (cp <= 0x10ffff) {
        bytes[0] = (unsigned char)(0xf0 | (cp >> 18));
        bytes[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3f));
        bytes[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3f));
        bytes[3] = (unsigned char)(0x80 | (cp & 0x3f)); count = 4;
    } else return -1;
    if (*used + count + 1 > out_size) return -1;
    memcpy(out + *used, bytes, count);
    *used += count;
    return 0;
}

int json_string(const JsonDoc *doc, int token, char *out, size_t out_size) {
    if (!doc || !out || out_size == 0 || token < 0 || token >= doc->count ||
        doc->tokens[token].type != JSON_STRING) return -1;
    const JsonToken *t = &doc->tokens[token];
    size_t used = 0;
    for (int i = t->start; i < t->end; i++) {
        unsigned char c = (unsigned char)doc->text[i];
        if (c != '\\') {
            if (used + 2 > out_size) return -1;
            out[used++] = (char)c;
            continue;
        }
        if (++i >= t->end) return -1;
        c = (unsigned char)doc->text[i];
        if (c == '"' || c == '\\' || c == '/') {
            if (used + 2 > out_size) return -1;
            out[used++] = (char)c;
        } else if (c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't') {
            static const char escaped[] = "\b\f\n\r\t";
            static const char names[] = "bfnrt";
            const char *found = strchr(names, (int)c);
            if (!found || used + 2 > out_size) return -1;
            out[used++] = escaped[found - names];
        } else if (c == 'u') {
            if (i + 4 >= t->end) return -1;
            uint32_t cp;
            if (hex4(doc->text + i + 1, &cp) != 0) return -1;
            i += 4;
            if (cp >= 0xd800 && cp <= 0xdbff && i + 6 < t->end &&
                doc->text[i + 1] == '\\' && doc->text[i + 2] == 'u') {
                uint32_t low;
                if (hex4(doc->text + i + 3, &low) == 0 &&
                    low >= 0xdc00 && low <= 0xdfff) {
                    cp = 0x10000 + ((cp - 0xd800) << 10) + (low - 0xdc00);
                    i += 6;
                }
            }
            if (cp >= 0xd800 && cp <= 0xdfff) return -1;
            if (append_utf8(out, out_size, &used, cp) != 0) return -1;
        } else return -1;
    }
    out[used] = '\0';
    return (int)used;
}

int json_i64(const JsonDoc *doc, int token, int64_t *out) {
    if (!doc || !out || token < 0 || token >= doc->count ||
        doc->tokens[token].type != JSON_PRIMITIVE) return -1;
    const JsonToken *t = &doc->tokens[token];
    size_t len = (size_t)(t->end - t->start);
    if (len == 0 || len >= 48) return -1;
    char buffer[48];
    memcpy(buffer, doc->text + t->start, len);
    buffer[len] = '\0';
    errno = 0;
    char *end = NULL;
    long long value = strtoll(buffer, &end, 10);
    if (errno != 0 || !end || *end != '\0') return -1;
    *out = (int64_t)value;
    return 0;
}

int json_is_null(const JsonDoc *doc, int token) {
    return doc && token >= 0 && token < doc->count &&
           doc->tokens[token].type == JSON_PRIMITIVE &&
           raw_equals(doc, token, "null");
}
