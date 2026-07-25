#pragma once

#include "ui_layout.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    UI_CONTROL_HINT_MAX = 8,
    UI_CONTROL_HINT_COLUMNS = 2,
    UI_CONTROL_HINT_STANDARD_ROWS = 3,
    UI_CONTROL_HINT_COMPACT_ROWS = 4,
    UI_CONTROL_HINT_HALF_WIDTH =
        UI_CONTROL_LEFT_CELL_RIGHT - UI_CONTROL_LEFT_CELL_X,
    UI_CONTROL_MARQUEE_START_PAUSE_MS = 1500,
    UI_CONTROL_MARQUEE_END_PAUSE_MS = 1000,
};

#define UI_CONTROL_MARQUEE_PIXELS_PER_SECOND 14.0f

typedef struct {
    uint8_t cell;
    uint8_t span;
} UiControlHintPlacement;

typedef struct {
    UiControlHintPlacement placements[UI_CONTROL_HINT_MAX];
    size_t count;
    uint8_t rows;
    bool show_title;
    bool fixed_grid_fallback;
} UiControlHintPlan;

bool ui_control_hint_plan(const int *required_widths, size_t count,
                          UiControlHintPlan *plan);
uint64_t ui_control_marquee_cycle_ms(float overflow_width);
float ui_control_marquee_offset(uint64_t elapsed_ms,
                                float overflow_width);
