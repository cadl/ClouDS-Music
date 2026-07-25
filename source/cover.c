#include "cover.h"

#include "i18n.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

void cover_init(CoverArt *cover) {
    if (cover) memset(cover, 0, sizeof(*cover));
}

void cover_clear(CoverArt *cover) {
    if (!cover) return;
    if (cover->ready) C3D_TexDelete(&cover->texture);
    memset(cover, 0, sizeof(*cover));
}

bool cover_matches(const CoverArt *cover, int64_t song_id) {
    return cover && cover->ready && song_id > 0 && cover->song_id == song_id;
}

C2D_Image cover_image(CoverArt *cover) {
    C2D_Image image = {0};
    if (cover && cover->ready) {
        image.tex = &cover->texture;
        image.subtex = &cover->subtexture;
    }
    return image;
}

int cover_upload_rgba(CoverArt *cover, const uint32_t *tiled,
                      size_t pixel_count, int64_t song_id,
                      char *error, size_t error_size) {
    if (!cover || !tiled || pixel_count < COVER_ART_PIXELS || song_id <= 0) {
        set_error(error, error_size, "解码后的封面无效");
        return -1;
    }
    cover_clear(cover);
    if (!C3D_TexInit(&cover->texture, COVER_ART_SIZE,
                     COVER_ART_SIZE, GPU_RGBA8)) {
        set_error(error, error_size, "无法分配封面纹理");
        return -1;
    }
    C3D_TexUpload(&cover->texture, tiled);
    C3D_TexSetFilter(&cover->texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&cover->texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    cover->subtexture.width = COVER_ART_SIZE;
    cover->subtexture.height = COVER_ART_SIZE;
    cover->subtexture.left = 0.0f;
    cover->subtexture.top = 1.0f;
    cover->subtexture.right = 1.0f;
    cover->subtexture.bottom = 0.0f;
    cover->song_id = song_id;
    cover->ready = true;
    return 0;
}

int cover_load_image(CoverArt *cover, const char *path, int64_t song_id,
                     char *error, size_t error_size) {
    uint32_t *tiled = (uint32_t *)malloc(
        COVER_ART_PIXELS * sizeof(*tiled));
    if (!tiled) {
        set_error(error, error_size, "内存不足，无法解码封面");
        return -1;
    }
    int result = cover_decode_image(path, tiled, COVER_ART_PIXELS,
                                    error, error_size);
    if (result == 0)
        result = cover_upload_rgba(cover, tiled, COVER_ART_PIXELS,
                                   song_id, error, error_size);
    free(tiled);
    return result;
}
