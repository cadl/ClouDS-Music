#include "control_hint_layout.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void assert_close(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.01f);
}

int main(void) {
    UiControlHintPlan plan;
    int compact[6] = {50, 70, 83, 60, 82, 55};
    assert(ui_control_hint_plan(compact, 6, &plan));
    assert(plan.count == 6);
    assert(plan.rows == 3);
    assert(plan.show_title);
    assert(!plan.fixed_grid_fallback);
    for (size_t i = 0; i < 6; i++) {
        assert(plan.placements[i].cell == i);
        assert(plan.placements[i].span == 1);
    }

    int one_wide[5] = {60, 70, 130, 55, 65};
    assert(ui_control_hint_plan(one_wide, 5, &plan));
    assert(plan.rows == 3);
    assert(plan.show_title);
    assert(plan.placements[2].cell == 2);
    assert(plan.placements[2].span == 2);
    assert(plan.placements[3].cell == 4);

    int seven_hints[7] = {50, 70, 60, 55, 65, 83, 58};
    assert(ui_control_hint_plan(seven_hints, 7, &plan));
    assert(plan.count == 7);
    assert(plan.rows == 4);
    assert(!plan.show_title);
    assert(!plan.fixed_grid_fallback);
    for (size_t i = 0; i < 7; i++) {
        assert(plan.placements[i].cell == i);
        assert(plan.placements[i].span == 1);
    }

    int four_rows[6] = {60, 130, 55, 65, 70, 60};
    assert(ui_control_hint_plan(four_rows, 6, &plan));
    assert(plan.rows == 4);
    assert(!plan.show_title);
    assert(!plan.fixed_grid_fallback);
    assert(plan.placements[1].span == 2);

    int fallback[6] = {130, 130, 130, 55, 55, 55};
    assert(ui_control_hint_plan(fallback, 6, &plan));
    assert(plan.rows == 3);
    assert(plan.show_title);
    assert(plan.fixed_grid_fallback);
    for (size_t i = 0; i < 6; i++) {
        assert(plan.placements[i].cell == i);
        assert(plan.placements[i].span == 1);
    }

    assert(!ui_control_hint_plan(compact, UI_CONTROL_HINT_MAX + 1, &plan));
    assert(ui_control_marquee_cycle_ms(14.0f) == 3500);
    assert_close(ui_control_marquee_offset(1000, 14.0f), 0.0f);
    assert_close(ui_control_marquee_offset(2000, 14.0f), 7.0f);
    assert_close(ui_control_marquee_offset(2500, 14.0f), 14.0f);
    assert_close(ui_control_marquee_offset(4000, 14.0f), 14.0f);

    puts("control hint layout tests: ok");
    return 0;
}
