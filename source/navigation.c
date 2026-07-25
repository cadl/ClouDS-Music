#include "navigation.h"

int navigation_grid_move(int selected, size_t count, int columns, int rows,
                         int dx, int dy) {
    if (count == 0 || columns <= 0 || rows <= 0) return 0;
    if (selected < 0 || (size_t)selected >= count) selected = 0;
    int page_size = columns * rows;
    int page = selected / page_size;
    int slot = selected % page_size;
    int row = slot / columns;
    int col = slot % columns;
    int next_page = page;
    int next_row = row + dy;
    int next_col = col + dx;

    if (next_col < 0 || next_col >= columns) return selected;
    if (next_row < 0) {
        if (page == 0) return selected;
        next_page--;
        next_row = rows - 1;
    } else if (next_row >= rows) {
        if ((size_t)((page + 1) * page_size) >= count) return selected;
        next_page++;
        next_row = 0;
    }

    int next = next_page * page_size + next_row * columns + next_col;
    while ((size_t)next >= count && next_col > 0) {
        next_col--;
        next--;
    }
    return next >= 0 && (size_t)next < count ? next : selected;
}

int navigation_list_move_selectable(int selected, size_t count, int delta,
                                    const bool *selectable) {
    if (count == 0 || !selectable || delta == 0) return -1;
    int direction = delta < 0 ? -1 : 1;
    int next = selected >= 0 && (size_t)selected < count ? selected :
               (direction > 0 ? -1 : 0);
    for (size_t checked = 0; checked < count; checked++) {
        next += direction;
        if (next < 0) next = (int)count - 1;
        if ((size_t)next >= count) next = 0;
        if (selectable[next]) return next;
    }
    return -1;
}

int navigation_list_page_move(int selected, size_t count, int page_size,
                              int direction, const bool *selectable) {
    if (count == 0) return -1;
    direction = direction < 0 ? -1 : direction > 0 ? 1 : 0;
    if (direction == 0 || page_size <= 0)
        return selected >= 0 && (size_t)selected < count &&
               (!selectable || selectable[selected]) ? selected : -1;
    if (selected < 0 || (size_t)selected >= count) {
        int index = direction > 0 ? 0 : (int)count - 1;
        while (index >= 0 && (size_t)index < count) {
            if (!selectable || selectable[index]) return index;
            index += direction;
        }
        return -1;
    }

    int last_index = (int)count - 1;
    int target;
    if (direction > 0) {
        int remaining = last_index - selected;
        target = page_size >= remaining ? last_index : selected + page_size;
    } else {
        target = page_size >= selected ? 0 : selected - page_size;
    }

    /* Prefer a selectable row on the requested page or farther in the same
     * direction.  Only fall back toward the old row at the list boundary. */
    for (int index = target;
         index >= 0 && (size_t)index < count;
         index += direction) {
        if (!selectable || selectable[index]) return index;
    }

    for (int index = target - direction;
         direction > 0 ? index > selected : index < selected;
         index -= direction) {
        if (!selectable || selectable[index]) return index;
    }
    return !selectable || selectable[selected] ? selected : -1;
}
