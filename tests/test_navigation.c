#include "navigation.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(navigation_grid_move(0, 12, 2, 3, 0, 1) == 2);
    assert(navigation_grid_move(1, 12, 2, 3, 0, 1) == 3);
    assert(navigation_grid_move(0, 12, 2, 3, 1, 0) == 1);
    assert(navigation_grid_move(1, 12, 2, 3, 1, 0) == 1);
    assert(navigation_grid_move(1, 12, 2, 3, -1, 0) == 0);
    assert(navigation_grid_move(4, 12, 2, 3, 0, 1) == 6);
    assert(navigation_grid_move(6, 12, 2, 3, 0, -1) == 4);
    assert(navigation_grid_move(11, 12, 2, 3, 0, 1) == 11);

    assert(navigation_grid_move(5, 7, 2, 3, 0, 1) == 6);
    assert(navigation_grid_move(6, 7, 2, 3, 1, 0) == 6);
    assert(navigation_grid_move(0, 0, 2, 3, 0, 1) == 0);

    assert(navigation_grid_move(0, 4, 2, 2, 1, 0) == 1);
    assert(navigation_grid_move(1, 4, 2, 2, 0, 1) == 3);
    assert(navigation_grid_move(3, 4, 2, 2, -1, 0) == 2);
    assert(navigation_grid_move(2, 4, 2, 2, 0, -1) == 0);

    assert(navigation_grid_move(0, 2, 2, 1, 1, 0) == 1);
    assert(navigation_grid_move(1, 2, 2, 1, -1, 0) == 0);
    assert(navigation_grid_move(0, 2, 2, 1, -1, 0) == 0);
    assert(navigation_grid_move(1, 2, 2, 1, 1, 0) == 1);

    const bool selectable[] = {false, true, false, true};
    assert(navigation_list_move_selectable(1, 4, 1, selectable) == 3);
    assert(navigation_list_move_selectable(3, 4, 1, selectable) == 1);
    assert(navigation_list_move_selectable(3, 4, -1, selectable) == 1);
    assert(navigation_list_move_selectable(-1, 4, 1, selectable) == 1);
    const bool none[] = {false, false};
    assert(navigation_list_move_selectable(0, 2, 1, none) == -1);

    assert(navigation_list_page_move(0, 10, 4, 1, NULL) == 4);
    assert(navigation_list_page_move(4, 10, 4, 1, NULL) == 8);
    assert(navigation_list_page_move(8, 10, 4, 1, NULL) == 9);
    assert(navigation_list_page_move(9, 10, 4, 1, NULL) == 9);
    assert(navigation_list_page_move(9, 10, 4, -1, NULL) == 5);
    assert(navigation_list_page_move(1, 10, 4, -1, NULL) == 0);
    assert(navigation_list_page_move(0, 10, 4, -1, NULL) == 0);

    const bool sparse[] = {
        true, false, false, true, false, false, true, false, false, false
    };
    assert(navigation_list_page_move(0, 10, 4, 1, sparse) == 6);
    assert(navigation_list_page_move(3, 10, 4, 1, sparse) == 6);
    assert(navigation_list_page_move(6, 10, 4, -1, sparse) == 0);
    assert(navigation_list_page_move(6, 10, 4, 1, sparse) == 6);
    assert(navigation_list_page_move(-1, 10, 4, 1, sparse) == 0);
    assert(navigation_list_page_move(-1, 10, 4, -1, sparse) == 6);
    assert(navigation_list_page_move(0, 2, 4, 1, none) == -1);
    assert(navigation_list_page_move(0, 0, 4, 1, NULL) == -1);
    const bool unavailable_edges[] = {false, true, false};
    assert(navigation_list_page_move(0, 3, 4, -1,
                                     unavailable_edges) == -1);
    assert(navigation_list_page_move(2, 3, 4, 1,
                                     unavailable_edges) == -1);

    puts("navigation tests: ok");
    return 0;
}
