#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t committed_offset;
    size_t pending_offset;
    bool loading;
} SearchPageState;

void search_page_reset(SearchPageState *state);
void search_page_begin(SearchPageState *state, size_t offset);
bool search_page_commit(SearchPageState *state, size_t offset);
void search_page_cancel(SearchPageState *state);
