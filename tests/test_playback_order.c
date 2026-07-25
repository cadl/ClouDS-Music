#include "playback_order.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(playback_next_index(0, -1, PLAY_MODE_SEQUENCE, 0) == -1);

    assert(playback_next_index(4, 1, PLAY_MODE_SEQUENCE, 0) == 2);
    assert(playback_next_index(4, 3, PLAY_MODE_SEQUENCE, 0) == 0);
    assert(playback_next_index(4, 1, PLAY_MODE_REPEAT_ONE, 0) == 2);
    assert(playback_next_index(4, -1, PLAY_MODE_SEQUENCE, 0) == 0);

    assert(playback_next_index(1, 0, PLAY_MODE_SHUFFLE, 7) == 0);
    assert(playback_next_index(4, -1, PLAY_MODE_SHUFFLE, 2) == 2);
    assert(playback_next_index(4, 2, PLAY_MODE_SHUFFLE, 0) == 0);
    assert(playback_next_index(4, 2, PLAY_MODE_SHUFFLE, 1) == 1);
    assert(playback_next_index(4, 2, PLAY_MODE_SHUFFLE, 2) == 3);
    assert(playback_next_index(4, 2, PLAY_MODE_SHUFFLE, 5) == 3);

    for (uint64_t entropy = 0; entropy < 128; entropy++)
        assert(playback_next_index(20, 7, PLAY_MODE_SHUFFLE,
                                   entropy) != 7);

    const bool available[] = {true, false, true, false};
    assert(playback_next_available_index(4, 0, PLAY_MODE_SEQUENCE, 0,
                                         available) == 2);
    assert(playback_next_available_index(4, 2, PLAY_MODE_SEQUENCE, 0,
                                         available) == 0);
    assert(playback_next_available_index(4, 1, PLAY_MODE_SEQUENCE, 0,
                                         available) == 2);
    assert(playback_next_available_index(4, 0, PLAY_MODE_SHUFFLE, 0,
                                         available) == 2);
    assert(playback_next_available_index(4, 2, PLAY_MODE_SHUFFLE, 0,
                                         available) == 0);
    const bool only_one[] = {false, true, false};
    assert(playback_next_available_index(3, 1, PLAY_MODE_SHUFFLE, 5,
                                         only_one) == 1);
    const bool unavailable[] = {false, false};
    assert(playback_next_available_index(2, 0, PLAY_MODE_SEQUENCE, 0,
                                         unavailable) == -1);

    puts("playback order tests passed");
    return 0;
}
