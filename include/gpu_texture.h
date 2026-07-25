#pragma once

#include <stdint.h>

/* PICA200 GPU_RGBA8 texels are stored as A, B, G, R bytes.  On the 3DS's
 * little-endian CPU that corresponds to the numeric value 0xRRGGBBAA.
 * C2D_Color32 uses a different layout intended for Citro2D draw colors. */
static inline uint32_t gpu_texture_rgba8(uint8_t red, uint8_t green,
                                         uint8_t blue, uint8_t alpha) {
    return ((uint32_t)red << 24U) |
           ((uint32_t)green << 16U) |
           ((uint32_t)blue << 8U) |
           (uint32_t)alpha;
}
