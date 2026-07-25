#include "search_page.h"

void search_page_reset(SearchPageState *state) {
    if (!state) return;
    state->committed_offset = 0;
    state->pending_offset = 0;
    state->loading = false;
}

void search_page_begin(SearchPageState *state, size_t offset) {
    if (!state) return;
    state->pending_offset = offset;
    state->loading = true;
}

bool search_page_commit(SearchPageState *state, size_t offset) {
    if (!state || !state->loading || state->pending_offset != offset)
        return false;
    state->committed_offset = offset;
    state->pending_offset = offset;
    state->loading = false;
    return true;
}

void search_page_cancel(SearchPageState *state) {
    if (!state) return;
    state->pending_offset = state->committed_offset;
    state->loading = false;
}
