#include "ime_candidate_layout.h"

#include <string.h>

void ime_candidate_layout_build(ImeCandidateLayout *layout,
                                const float *text_widths, int item_count,
                                float available_width, float minimum_width,
                                float horizontal_padding, float gap) {
    if (!layout) return;
    memset(layout, 0, sizeof(*layout));
    if (!text_widths || item_count <= 0 || available_width <= 0.0f) return;
    if (item_count > IME_MAX_CANDIDATES)
        item_count = IME_MAX_CANDIDATES;
    if (minimum_width < 0.0f) minimum_width = 0.0f;
    if (horizontal_padding < 0.0f) horizontal_padding = 0.0f;
    if (gap < 0.0f) gap = 0.0f;

    layout->item_count = item_count;
    for (int i = 0; i < item_count; i++) {
        float width = text_widths[i] + horizontal_padding;
        if (width < minimum_width) width = minimum_width;
        if (width > available_width) width = available_width;
        layout->item_widths[i] = width;
    }

    int item = 0;
    while (item < item_count) {
        int page = layout->page_count++;
        int first = item;
        float used = 0.0f;
        layout->page_starts[page] = first;
        while (item < item_count) {
            float extra = layout->item_widths[item];
            if (item > first) extra += gap;
            if (item > first && used + extra > available_width) break;
            used += extra;
            item++;
        }
    }
    layout->page_starts[layout->page_count] = item_count;
}

int ime_candidate_layout_page_start(const ImeCandidateLayout *layout,
                                    int page) {
    if (!layout || page < 0 || page >= layout->page_count) return 0;
    return layout->page_starts[page];
}

int ime_candidate_layout_page_end(const ImeCandidateLayout *layout,
                                  int page) {
    if (!layout || page < 0 || page >= layout->page_count) return 0;
    return layout->page_starts[page + 1];
}
