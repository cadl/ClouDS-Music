#include "search_page.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    SearchPageState state;
    search_page_reset(&state);
    assert(state.committed_offset == 0);
    assert(state.pending_offset == 0);
    assert(!state.loading);

    search_page_begin(&state, 18);
    assert(state.committed_offset == 0);
    assert(state.pending_offset == 18);
    assert(state.loading);

    assert(!search_page_commit(&state, 36));
    assert(state.committed_offset == 0);
    assert(state.pending_offset == 18);
    assert(state.loading);

    assert(search_page_commit(&state, 18));
    assert(state.committed_offset == 18);
    assert(state.pending_offset == 18);
    assert(!state.loading);

    search_page_begin(&state, 36);
    search_page_cancel(&state);
    assert(state.committed_offset == 18);
    assert(state.pending_offset == 18);
    assert(!state.loading);

    puts("search page tests: ok");
    return 0;
}
