#include "ime_candidate_layout.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    ImeCandidateLayout layout;
    float mixed[] = {18, 36, 18, 54, 18, 36};
    ime_candidate_layout_build(&layout, mixed, 6, 100, 24, 6, 2);
    assert(layout.item_count == 6);
    assert(layout.page_count == 3);
    assert(ime_candidate_layout_page_start(&layout, 0) == 0);
    assert(ime_candidate_layout_page_end(&layout, 0) == 3);
    assert(ime_candidate_layout_page_start(&layout, 1) == 3);
    assert(ime_candidate_layout_page_end(&layout, 1) == 5);
    assert(ime_candidate_layout_page_start(&layout, 2) == 5);
    assert(ime_candidate_layout_page_end(&layout, 2) == 6);

    float single_char[10] = {18, 18, 18, 18, 18, 18, 18, 18, 18, 18};
    ime_candidate_layout_build(&layout, single_char, 10, 246, 24, 6, 2);
    assert(ime_candidate_layout_page_end(&layout, 0) == 9);
    assert(ime_candidate_layout_page_start(&layout, 1) == 9);

    float oversized[] = {400, 18};
    ime_candidate_layout_build(&layout, oversized, 2, 100, 24, 6, 2);
    assert(layout.item_widths[0] == 100);
    assert(layout.page_count == 2);
    assert(ime_candidate_layout_page_end(&layout, 0) == 1);

    ime_candidate_layout_build(&layout, NULL, 0, 100, 24, 6, 2);
    assert(layout.item_count == 0);
    assert(layout.page_count == 0);
    puts("ime candidate layout tests: ok");
    return 0;
}
