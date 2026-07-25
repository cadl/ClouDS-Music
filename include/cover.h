#pragma once

#include "cover_decode.h"

#include <citro2d.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    C3D_Tex texture;
    Tex3DS_SubTexture subtexture;
    int64_t song_id;
    bool ready;
} CoverArt;

void cover_init(CoverArt *cover);
void cover_clear(CoverArt *cover);
int cover_upload_rgba(CoverArt *cover, const uint32_t *tiled,
                      size_t pixel_count, int64_t song_id,
                      char *error, size_t error_size);
int cover_load_image(CoverArt *cover, const char *path, int64_t song_id,
                     char *error, size_t error_size);
bool cover_matches(const CoverArt *cover, int64_t song_id);
C2D_Image cover_image(CoverArt *cover);
