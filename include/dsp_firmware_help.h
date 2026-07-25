#pragma once

#include <stdbool.h>
#include <stdint.h>

/* libctru returns this value when ndspInit cannot obtain the DSP component
 * from either /3ds/dspfirm.cdc or the Homebrew Launcher hb:ndsp handle. */
#define NM3DS_NDSP_COMPONENT_NOT_FOUND UINT32_C(0xD880A7FA)

static inline bool dsp_firmware_prompt_needed(bool player_available,
                                              uint32_t ndsp_result) {
    return !player_available &&
           ndsp_result == NM3DS_NDSP_COMPONENT_NOT_FOUND;
}
