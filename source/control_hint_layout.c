#include "control_hint_layout.h"

#include <string.h>

static bool try_flow_plan(const int *required_widths, size_t count,
                          uint8_t rows, UiControlHintPlan *plan) {
    size_t cell = 0;
    size_t capacity = (size_t)rows * UI_CONTROL_HINT_COLUMNS;
    for (size_t i = 0; i < count; i++) {
        uint8_t span = required_widths[i] <= UI_CONTROL_HINT_HALF_WIDTH ?
                       1U : 2U;
        if (span == 2U && cell % UI_CONTROL_HINT_COLUMNS != 0)
            cell++;
        if (cell + span > capacity) return false;
        plan->placements[i].cell = (uint8_t)cell;
        plan->placements[i].span = span;
        cell += span;
    }
    plan->rows = rows;
    return true;
}

bool ui_control_hint_plan(const int *required_widths, size_t count,
                          UiControlHintPlan *plan) {
    if (plan) memset(plan, 0, sizeof(*plan));
    if (!plan || (!required_widths && count > 0) ||
        count > UI_CONTROL_HINT_MAX)
        return false;
    plan->count = count;
    if (try_flow_plan(required_widths, count,
                      UI_CONTROL_HINT_STANDARD_ROWS, plan)) {
        plan->show_title = true;
        return true;
    }
    if (try_flow_plan(required_widths, count,
                      UI_CONTROL_HINT_COMPACT_ROWS, plan)) {
        plan->show_title = false;
        return true;
    }

    /* Preserve every hint when flowing would consume too many rows.  Long
     * actions stay in their half-cell and are exposed by the marquee. */
    plan->fixed_grid_fallback = true;
    plan->show_title = count <= (size_t)UI_CONTROL_HINT_STANDARD_ROWS *
                                UI_CONTROL_HINT_COLUMNS;
    plan->rows = plan->show_title ? UI_CONTROL_HINT_STANDARD_ROWS :
                                   UI_CONTROL_HINT_COMPACT_ROWS;
    for (size_t i = 0; i < count; i++) {
        plan->placements[i].cell = (uint8_t)i;
        plan->placements[i].span = 1U;
    }
    return true;
}

uint64_t ui_control_marquee_cycle_ms(float overflow_width) {
    if (overflow_width < 0.0f) overflow_width = 0.0f;
    float scroll_ms = overflow_width * 1000.0f /
                      UI_CONTROL_MARQUEE_PIXELS_PER_SECOND;
    uint64_t rounded_scroll_ms = (uint64_t)(scroll_ms + 0.999f);
    return UI_CONTROL_MARQUEE_START_PAUSE_MS + rounded_scroll_ms +
           UI_CONTROL_MARQUEE_END_PAUSE_MS;
}

float ui_control_marquee_offset(uint64_t elapsed_ms,
                                float overflow_width) {
    if (overflow_width <= 0.0f ||
        elapsed_ms <= UI_CONTROL_MARQUEE_START_PAUSE_MS)
        return 0.0f;
    uint64_t moving_ms = elapsed_ms - UI_CONTROL_MARQUEE_START_PAUSE_MS;
    float offset = (float)moving_ms *
                   UI_CONTROL_MARQUEE_PIXELS_PER_SECOND / 1000.0f;
    return offset < overflow_width ? offset : overflow_width;
}
