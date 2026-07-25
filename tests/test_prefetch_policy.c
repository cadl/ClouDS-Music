#include "prefetch_policy.h"

#include <assert.h>

int main(void) {
    assert(prefetch_queue_index(0, -1, 0) == -1);
    assert(prefetch_queue_index(1, 0, 0) == -1);
    assert(prefetch_queue_index(4, 1, 0) == 2);
    assert(prefetch_queue_index(4, 1, 1) == 3);
    assert(prefetch_queue_index(4, 1, 2) == 0);
    assert(prefetch_queue_index(4, 1, 3) == -1);
    assert(prefetch_queue_index(4, 4, 0) == -1);
    assert(prefetch_queue_index(500, 0, 15) == 16);
    assert(prefetch_queue_index(500, 0, 16) == -1);
    return 0;
}
