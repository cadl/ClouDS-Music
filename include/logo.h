#pragma once

#include <citro2d.h>
#include <stdbool.h>

typedef struct {
    C3D_Tex texture;
    Tex3DS_SubTexture subtexture;
    bool ready;
} BrandLogo;

bool brand_logo_init(BrandLogo *logo);
void brand_logo_clear(BrandLogo *logo);
bool brand_logo_draw(BrandLogo *logo,
                     float x, float y, float z, float size);
