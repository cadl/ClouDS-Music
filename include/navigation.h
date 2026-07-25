#pragma once

#include <stdbool.h>
#include <stddef.h>

int navigation_grid_move(int selected, size_t count, int columns, int rows,
                         int dx, int dy);
int navigation_list_move_selectable(int selected, size_t count, int delta,
                                    const bool *selectable);
int navigation_list_page_move(int selected, size_t count, int page_size,
                              int direction, const bool *selectable);
