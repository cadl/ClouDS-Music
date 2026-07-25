#include "dsp_firmware_help.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(dsp_firmware_prompt_needed(
        false, NM3DS_NDSP_COMPONENT_NOT_FOUND));
    assert(!dsp_firmware_prompt_needed(
        true, NM3DS_NDSP_COMPONENT_NOT_FOUND));
    assert(!dsp_firmware_prompt_needed(false, 0));
    assert(!dsp_firmware_prompt_needed(false, UINT32_C(0xD8A0A046)));

    puts("dsp firmware help tests: ok");
    return 0;
}
