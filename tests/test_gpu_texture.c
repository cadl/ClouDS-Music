#include "gpu_texture.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    assert(gpu_texture_rgba8(255, 255, 0, 255) == UINT32_C(0xFFFF00FF));
    assert(gpu_texture_rgba8(255, 0, 0, 255) == UINT32_C(0xFF0000FF));
    assert(gpu_texture_rgba8(0, 0, 255, 255) == UINT32_C(0x0000FFFF));
    assert(gpu_texture_rgba8(0x12, 0x34, 0x56, 0x78) ==
           UINT32_C(0x12345678));

    puts("GPU texture packing tests: ok");
    return 0;
}
