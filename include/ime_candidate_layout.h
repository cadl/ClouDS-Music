#pragma once

#include "ime_pinyin.h"

typedef struct {
    int item_count;
    int page_count;
    int page_starts[IME_MAX_CANDIDATES + 1];
    float item_widths[IME_MAX_CANDIDATES];
} ImeCandidateLayout;

void ime_candidate_layout_build(ImeCandidateLayout *layout,
                                const float *text_widths, int item_count,
                                float available_width, float minimum_width,
                                float horizontal_padding, float gap);
int ime_candidate_layout_page_start(const ImeCandidateLayout *layout,
                                    int page);
int ime_candidate_layout_page_end(const ImeCandidateLayout *layout,
                                  int page);
