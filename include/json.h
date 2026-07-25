#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    JSON_UNDEFINED = 0,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_STRING,
    JSON_PRIMITIVE
} JsonType;

typedef struct {
    JsonType type;
    int start;
    int end;
    int size;
    int parent;
} JsonToken;

typedef struct {
    const char *text;
    JsonToken *tokens;
    int count;
} JsonDoc;

typedef int (*JsonObjectVisitor)(const JsonDoc *doc, void *userdata);

enum {
    JSON_VISIT_TOKENS_EXHAUSTED = -1,
    JSON_VISIT_INVALID = -2,
    JSON_VISIT_NOT_FOUND = -3,
    JSON_VISIT_CALLBACK_FAILED = -4
};

int json_parse(JsonDoc *doc, const char *text, JsonToken *tokens, int capacity);
int json_obj_get(const JsonDoc *doc, int object, const char *key);
int json_arr_get(const JsonDoc *doc, int array, int index);
int json_arr_size(const JsonDoc *doc, int array);
int json_string(const JsonDoc *doc, int token, char *out, size_t out_size);
int json_i64(const JsonDoc *doc, int token, int64_t *out);
int json_is_null(const JsonDoc *doc, int token);
/* Finds an object array by key, parses one object at a time with the caller's
 * reusable token buffer, and stops when the visitor returns a positive value.
 * The input is restored before this function returns. */
int json_visit_array_objects(char *text, const char *key,
                             JsonToken *tokens, int capacity,
                             JsonObjectVisitor visitor, void *userdata);
