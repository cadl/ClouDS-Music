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

    assert(media_stream_start_target(0, 0) == 256U * 1024U);
    assert(media_stream_start_target(20U * 1024U * 1024U, 0) ==
           1024U * 1024U);
    /* An embedded FLAC cover is not audio and must not start playback. */
    assert(media_stream_start_target(30U * 1024U * 1024U, 3U * 1024U * 1024U) ==
           4U * 1024U * 1024U);
    assert(media_stream_start_target(0, 500000U) == 500000U + 256U * 1024U);
    assert(media_stream_start_target(600000U, 500000U) == 600000U);
    assert(media_stream_start_target(UINT64_MAX, UINT64_MAX - 1U) ==
           UINT64_MAX);

    /* CD FLAC: four seconds of audio. */
    assert(media_rebuffer_bytes(110000U, 4, 64U * 1024U) == 440000U);
    /* Hi-res FLAC is capped at the largest start prebuffer. */
    assert(media_rebuffer_bytes(350000U, 4, 64U * 1024U) == 1024U * 1024U);
    assert(media_rebuffer_bytes(UINT64_MAX, 4, 64U * 1024U) == 1024U * 1024U);
    assert(media_rebuffer_bytes(1000U, 4, 81008U) == 81008U);
    assert(media_rebuffer_bytes(0, 0, 64U * 1024U) == 64U * 1024U);

    /* A window load in flight always blocks decoding. */
    assert(!media_stream_can_resume(true, true, false, UINT64_MAX, 0));
    /* Waiting for the window ends with the load, whatever the lead. */
    assert(media_stream_can_resume(false, false, false, 0, 1024U * 1024U));
    /* A starved stream waits for the full lead unless the download ended. */
    assert(!media_stream_can_resume(false, false, true, 1000U, 440000U));
    assert(media_stream_can_resume(false, false, true, 440000U, 440000U));
    assert(media_stream_can_resume(false, true, true, 0, 440000U));

    uint64_t whole_song = 4000000U;
    uint64_t start_target = media_prebuffer_target(whole_song);
    assert(media_download_percent(start_target, whole_song) == 15U);
    assert(media_download_percent(start_target / 2U, start_target) == 50U);
    assert(media_download_percent(start_target, start_target) == 100U);
    puts("media policy tests passed");
    return 0;
}
