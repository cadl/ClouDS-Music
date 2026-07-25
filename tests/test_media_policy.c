#include "media_policy.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    assert(MEDIA_PREBUFFER_PERCENT == 15U);
    assert(media_prebuffer_target(0) == 256U * 1024U);
    assert(media_prebuffer_target(128U * 1024U) == 128U * 1024U);
    assert(media_prebuffer_target(512U * 1024U) == 256U * 1024U);
    assert(media_prebuffer_target(2U * 1024U * 1024U) ==
           2ULL * 1024U * 1024U * 15U / 100U);
    assert(media_prebuffer_target(4U * 1024U * 1024U) ==
           4ULL * 1024U * 1024U * 15U / 100U);
    assert(media_prebuffer_target(7U * 1024U * 1024U) == 1024U * 1024U);
    assert(media_prebuffer_target(20U * 1024U * 1024U) == 1024U * 1024U);
    assert(media_prebuffer_target(UINT64_MAX) == 1024U * 1024U);
    assert(media_download_percent(0, 0) == 0U);
    assert(media_download_percent(0, 256U * 1024U) == 0U);
    assert(media_download_percent(128U * 1024U, 256U * 1024U) == 50U);
    assert(media_download_percent(255U * 1024U, 256U * 1024U) == 99U);
    assert(media_download_percent(256U * 1024U, 256U * 1024U) == 100U);
    assert(media_download_percent(512U * 1024U, 256U * 1024U) == 100U);

    uint64_t whole_song = 4000000U;
    uint64_t start_target = media_prebuffer_target(whole_song);
    assert(media_download_percent(start_target, whole_song) == 15U);
    assert(media_download_percent(start_target / 2U, start_target) == 50U);
    assert(media_download_percent(start_target, start_target) == 100U);
    puts("media policy tests passed");
    return 0;
}
