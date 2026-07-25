#include "download_policy.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    assert(download_space_budget(100, 20) == 80);
    assert(download_space_budget(20, 20) == 0);
    assert(download_space_budget(19, 20) == 0);

    assert(download_limit_check(40, 20, 100, 80) == DOWNLOAD_LIMIT_OK);
    assert(download_limit_check(80, 20, 100, 100) == DOWNLOAD_LIMIT_OK);
    assert(download_limit_check(80, 21, 100, 200) == DOWNLOAD_LIMIT_SIZE);
    assert(download_limit_check(70, 11, 100, 80) == DOWNLOAD_LIMIT_SPACE);
    assert(download_limit_check(0, 1, 0, 100) == DOWNLOAD_LIMIT_INVALID);
    assert(download_limit_check(UINT64_MAX - 4, 5, UINT64_MAX, UINT64_MAX) ==
           DOWNLOAD_LIMIT_SIZE);

    puts("download policy tests passed");
    return 0;
}
