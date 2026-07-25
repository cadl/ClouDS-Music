#include "logo.h"

#include "gpu_texture.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define BRAND_LOGO_SIZE 48U
#define BRAND_LOGO_TEXTURE_SIZE 64U

/* Generated from icon-v4.png (SHA-256:
 * 93178f5e60c9a7154003320d705cb1b2d1e153d0f7465944cdfcd76e4f169769).
 * Each pair is a run length followed by an index into the palette. */
static const uint8_t BRAND_LOGO_PALETTE[][4] = {
    {54, 179, 253, 255},
    {252, 212, 90, 255},
    {253, 240, 221, 255},
    {178, 169, 207, 255},
    {201, 174, 248, 255},
    {23, 22, 80, 255},
    {105, 102, 214, 255},
    {65, 62, 150, 255},
    {58, 56, 142, 255},
    {41, 115, 184, 255},
    {89, 190, 246, 255},
    {26, 40, 105, 255},
    {94, 93, 200, 255},
    {71, 68, 157, 255},
    {9, 17, 72, 255},
    {49, 152, 220, 255},
    {253, 225, 176, 255},
    {240, 214, 204, 255},
    {211, 29, 30, 255},
    {250, 87, 83, 255},
    {249, 201, 180, 255},
    {251, 190, 179, 255},
};

static const uint8_t BRAND_LOGO_RLE[] = {
    232, 0, 2, 1, 13, 0, 3, 2, 28, 0, 1, 1, 1, 0, 1, 1, 1, 3, 1, 0,
    1, 1, 10, 0, 5, 2, 28, 0, 4, 1, 10, 0, 2, 2, 1, 4, 6, 2, 23, 0,
    8, 1, 7, 0, 6, 4, 1, 2, 4, 4, 24, 0, 4, 1, 9, 0, 11, 4, 24, 0,
    4, 1, 25, 0, 9, 5, 9, 0, 1, 1, 1, 0, 1, 1, 1, 3, 1, 0, 1, 1,
    21, 0, 3, 5, 9, 6, 2, 5, 9, 0, 2, 1, 21, 0, 2, 5, 3, 6, 9, 7,
    2, 6, 2, 5, 29, 0, 1, 5, 2, 6, 3, 7, 9, 5, 2, 7, 2, 6, 1, 5,
    27, 0, 1, 5, 1, 6, 2, 7, 1, 8, 2, 5, 9, 0, 2, 5, 2, 7, 1, 6,
    1, 5, 1, 9, 16, 0, 1, 10, 3, 2, 4, 0, 1, 11, 1, 12, 1, 13, 1, 7,
    2, 5, 3, 0, 4, 11, 6, 0, 1, 5, 2, 7, 1, 12, 1, 6, 1, 11, 4, 0,
    2, 2, 17, 0, 1, 5, 1, 6, 2, 7, 2, 5, 3, 0, 4, 5, 6, 0, 1, 5,
    2, 7, 2, 6, 1, 5, 3, 0, 2, 4, 3, 2, 15, 0, 1, 5, 1, 6, 1, 7,
    1, 5, 3, 0, 1, 14, 1, 5, 4, 2, 1, 5, 1, 11, 5, 0, 1, 5, 1, 7,
    2, 6, 1, 5, 22, 0, 1, 5, 1, 6, 1, 7, 1, 5, 3, 0, 1, 15, 8, 2,
    1, 5, 5, 0, 1, 5, 1, 7, 1, 13, 1, 6, 1, 5, 10, 0, 11, 4, 1, 5,
    1, 7, 1, 5, 3, 4, 1, 5, 1, 14, 9, 2, 1, 5, 5, 4, 2, 5, 1, 13,
    1, 5, 20, 4, 1, 5, 1, 7, 1, 5, 3, 4, 1, 5, 11, 2, 1, 16, 1, 5,
    5, 4, 2, 5, 1, 13, 1, 5, 19, 4, 1, 5, 1, 7, 1, 5, 3, 4, 1, 5,
    11, 2, 1, 16, 3, 5, 3, 4, 2, 5, 1, 7, 1, 5, 17, 4, 1, 3, 1, 5,
    1, 11, 5, 5, 1, 17, 4, 2, 2, 18, 9, 2, 1, 5, 1, 4, 4, 5, 1, 6,
    1, 5, 16, 4, 1, 3, 1, 5, 1, 6, 1, 5, 1, 7, 1, 13, 1, 5, 1, 14,
    1, 16, 4, 2, 2, 18, 1, 19, 8, 2, 1, 12, 2, 5, 2, 13, 1, 5, 1, 6,
    1, 5, 15, 4, 1, 5, 1, 11, 2, 6, 1, 5, 2, 13, 1, 5, 1, 16, 5, 2,
    1, 18, 1, 19, 1, 18, 2, 17, 7, 2, 2, 5, 2, 13, 1, 5, 2, 6, 1, 5,
    13, 4, 1, 5, 1, 7, 3, 6, 1, 5, 2, 13, 1, 5, 6, 2, 1, 18, 2, 19,
    2, 18, 7, 2, 1, 16, 1, 5, 2, 13, 1, 5, 3, 6, 1, 5, 12, 4, 1, 5,
    1, 7, 3, 6, 1, 5, 1, 13, 1, 8, 1, 5, 6, 2, 1, 18, 4, 19, 1, 18,
    6, 2, 1, 16, 1, 5, 1, 8, 1, 13, 1, 5, 3, 6, 1, 5, 12, 4, 1, 5,
    1, 7, 3, 6, 1, 5, 1, 13, 1, 8, 1, 5, 6, 2, 1, 18, 5, 19, 1, 18,
    1, 19, 4, 2, 1, 16, 1, 5, 1, 8, 1, 13, 1, 5, 3, 6, 1, 5, 12, 4,
    1, 5, 1, 7, 3, 6, 1, 5, 1, 13, 1, 8, 1, 5, 6, 2, 1, 18, 6, 19,
    1, 18, 4, 2, 1, 16, 1, 5, 2, 8, 1, 5, 3, 6, 1, 5, 12, 4, 1, 5,
    1, 13, 3, 6, 1, 5, 2, 8, 1, 5, 1, 16, 5, 2, 1, 18, 4, 19, 2, 18,
    6, 2, 1, 16, 1, 5, 1, 8, 1, 5, 3, 6, 1, 5, 12, 4, 1, 5, 1, 7,
    3, 6, 1, 5, 2, 8, 1, 5, 1, 16, 5, 2, 1, 18, 2, 19, 2, 18, 10, 2,
    1, 11, 1, 5, 3, 6, 1, 5, 13, 4, 2, 5, 2, 7, 1, 5, 1, 8, 1, 5,
    1, 16, 6, 2, 1, 18, 1, 19, 1, 18, 12, 2, 1, 20, 1, 5, 2, 7, 1, 5,
    16, 4, 1, 7, 1, 8, 2, 5, 2, 16, 6, 2, 2, 18, 13, 2, 1, 20, 1, 5,
    1, 7, 1, 8, 17, 4, 4, 5, 3, 16, 20, 2, 1, 20, 3, 5, 18, 4, 1, 5,
    1, 17, 5, 16, 17, 2, 3, 16, 1, 17, 1, 5, 19, 4, 1, 5, 2, 17, 5, 16,
    3, 2, 2, 16, 10, 2, 2, 16, 1, 20, 2, 17, 1, 5, 20, 4, 1, 5, 2, 17,
    10, 16, 4, 2, 6, 16, 2, 17, 1, 20, 1, 5, 22, 4, 1, 5, 2, 17, 4, 16,
    3, 17, 6, 16, 3, 17, 1, 20, 1, 16, 2, 17, 1, 2, 1, 5, 11, 4, 2, 21,
    1, 20, 9, 21, 1, 20, 1, 5, 2, 17, 4, 20, 2, 5, 1, 17, 4, 16, 1, 20,
    1, 17, 2, 5, 3, 17, 1, 5, 1, 14, 4, 21, 1, 20, 11, 21, 1, 20, 8, 21,
    1, 20, 1, 5, 1, 17, 2, 20, 1, 17, 1, 5, 2, 20, 1, 5, 1, 20, 4, 17,
    1, 5, 2, 20, 3, 5, 1, 21, 1, 20, 25, 21, 1, 20, 1, 21, 4, 5, 2, 21,
    1, 20, 1, 21, 5, 5, 6, 20, 5, 21, 1, 20, 32, 21, 1, 20, 1, 21, 1, 20,
    3, 21, 2, 20, 31, 21, 1, 20, 50, 21, 1, 20, 12, 21, 2, 20, 45, 21, 1, 20,
    1, 21, 1, 20, 34, 21, 1, 20, 98, 21, 2, 20, 24, 21,
};

static unsigned int morton8(unsigned int x, unsigned int y) {
    return (x & 1U) | ((y & 1U) << 1U) |
           ((x & 2U) << 1U) | ((y & 2U) << 2U) |
           ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

static size_t tiled_offset(unsigned int x, unsigned int y) {
    size_t tile = (size_t)(y >> 3U) *
                  (BRAND_LOGO_TEXTURE_SIZE >> 3U) + (x >> 3U);
    return tile * 64U + morton8(x & 7U, y & 7U);
}

bool brand_logo_init(BrandLogo *logo) {
    if (!logo) return false;
    memset(logo, 0, sizeof(*logo));

    const size_t texture_pixels =
        (size_t)BRAND_LOGO_TEXTURE_SIZE * BRAND_LOGO_TEXTURE_SIZE;
    u32 *pixels = (u32 *)calloc(texture_pixels, sizeof(*pixels));
    if (!pixels) return false;

    size_t source_pixel = 0;
    for (size_t i = 0; i + 1U < sizeof(BRAND_LOGO_RLE); i += 2U) {
        unsigned int count = BRAND_LOGO_RLE[i];
        unsigned int palette_index = BRAND_LOGO_RLE[i + 1U];
        if (palette_index >= sizeof(BRAND_LOGO_PALETTE) /
                             sizeof(BRAND_LOGO_PALETTE[0]) ||
            source_pixel + count > BRAND_LOGO_SIZE * BRAND_LOGO_SIZE) {
            free(pixels);
            return false;
        }
        const uint8_t *color = BRAND_LOGO_PALETTE[palette_index];
        for (unsigned int run = 0; run < count; run++, source_pixel++) {
            unsigned int x = (unsigned int)(source_pixel % BRAND_LOGO_SIZE);
            unsigned int y = (unsigned int)(source_pixel / BRAND_LOGO_SIZE);
            pixels[tiled_offset(x, y)] =
                gpu_texture_rgba8(color[0], color[1], color[2], color[3]);
        }
    }
    if (source_pixel != BRAND_LOGO_SIZE * BRAND_LOGO_SIZE ||
        !C3D_TexInit(&logo->texture, BRAND_LOGO_TEXTURE_SIZE,
                     BRAND_LOGO_TEXTURE_SIZE, GPU_RGBA8)) {
        free(pixels);
        return false;
    }

    C3D_TexUpload(&logo->texture, pixels);
    free(pixels);
    C3D_TexSetFilter(&logo->texture, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&logo->texture, GPU_CLAMP_TO_BORDER,
                   GPU_CLAMP_TO_BORDER);

    logo->subtexture.width = BRAND_LOGO_SIZE;
    logo->subtexture.height = BRAND_LOGO_SIZE;
    logo->subtexture.left = 0.0f;
    logo->subtexture.top = 1.0f;
    logo->subtexture.right =
        (float)BRAND_LOGO_SIZE / BRAND_LOGO_TEXTURE_SIZE;
    logo->subtexture.bottom =
        1.0f - (float)BRAND_LOGO_SIZE / BRAND_LOGO_TEXTURE_SIZE;
    logo->ready = true;
    return true;
}

void brand_logo_clear(BrandLogo *logo) {
    if (!logo) return;
    if (logo->ready) C3D_TexDelete(&logo->texture);
    memset(logo, 0, sizeof(*logo));
}

bool brand_logo_draw(BrandLogo *logo,
                     float x, float y, float z, float size) {
    if (!logo || !logo->ready || size <= 0.0f) return false;
    C2D_Image image = {
        &logo->texture,
        &logo->subtexture
    };
    float scale = size / BRAND_LOGO_SIZE;
    C2D_DrawImageAt(image, x, y, z, NULL, scale, scale);
    return true;
}
