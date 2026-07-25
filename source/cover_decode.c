#include "cover_decode.h"

#include "gpu_texture.h"
#include "i18n.h"

#include <stdio.h>

#include <jpeglib.h>
#include <png.h>

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define COVER_SOURCE_MAX_DIMENSION 256U
#define COVER_PNG_CHUNK_LIMIT (256U * 1024U)
#define COVER_PNG_CHUNK_COUNT_LIMIT 32U

typedef struct {
    struct jpeg_decompress_struct info;
    struct jpeg_error_mgr base_error;
    jmp_buf jump;
    char message[JMSG_LENGTH_MAX];
    uint8_t *pixels;
    bool created;
} JpegDecodeState;

typedef struct {
    png_structp png;
    png_infop info;
    uint8_t *pixels;
    char message[128];
} PngDecodeState;

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static unsigned int morton8(unsigned int x, unsigned int y) {
    return (x & 1U) | ((y & 1U) << 1U) |
           ((x & 2U) << 1U) | ((y & 2U) << 2U) |
           ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

static size_t tiled_offset(unsigned int x, unsigned int y) {
    size_t tile = (size_t)(y >> 3U) * (COVER_ART_SIZE >> 3U) +
                  (x >> 3U);
    return tile * 64U + morton8(x & 7U, y & 7U);
}

static void scale_to_tiled(const uint8_t *pixels,
                           unsigned int width, unsigned int height,
                           unsigned int components, uint32_t *tiled) {
    for (unsigned int y = 0; y < COVER_ART_SIZE; y++) {
        unsigned int source_y = y * height / COVER_ART_SIZE;
        for (unsigned int x = 0; x < COVER_ART_SIZE; x++) {
            unsigned int source_x = x * width / COVER_ART_SIZE;
            const uint8_t *pixel = pixels +
                ((size_t)source_y * width + source_x) * components;
            uint8_t alpha = components == 4U ? pixel[3] : 255U;
            tiled[tiled_offset(x, y)] =
                gpu_texture_rgba8(pixel[0], pixel[1], pixel[2], alpha);
        }
    }
}

static void scale_row_to_tiled(const uint8_t *row, unsigned int width,
                               unsigned int components,
                               unsigned int target_y, uint32_t *tiled) {
    for (unsigned int x = 0; x < COVER_ART_SIZE; x++) {
        unsigned int source_x = x * width / COVER_ART_SIZE;
        const uint8_t *pixel = row + (size_t)source_x * components;
        uint8_t alpha = components == 4U ? pixel[3] : 255U;
        tiled[tiled_offset(x, target_y)] =
            gpu_texture_rgba8(pixel[0], pixel[1], pixel[2], alpha);
    }
}

CoverImageFormat cover_image_format(const uint8_t *header, size_t size) {
    if (!header) return COVER_IMAGE_UNKNOWN;
    if (size >= 8U && png_sig_cmp(header, 0, 8) == 0)
        return COVER_IMAGE_PNG;
    if (size >= 3U && header[0] == 0xffU && header[1] == 0xd8U &&
        header[2] == 0xffU)
        return COVER_IMAGE_JPEG;
    return COVER_IMAGE_UNKNOWN;
}

static void jpeg_fail(j_common_ptr info) {
    JpegDecodeState *state = (JpegDecodeState *)info->client_data;
    (*info->err->format_message)(info, state->message);
    longjmp(state->jump, 1);
}

static int decode_jpeg(FILE *file, uint32_t *tiled,
                       char *error, size_t error_size) {
    JpegDecodeState *state =
        (JpegDecodeState *)calloc(1, sizeof(*state));
    if (!state) {
        set_error(error, error_size, "内存不足，无法解码封面");
        return -1;
    }
    state->info.err = jpeg_std_error(&state->base_error);
    state->base_error.error_exit = jpeg_fail;
    state->info.client_data = state;
    if (setjmp(state->jump)) {
        if (state->created) jpeg_destroy_decompress(&state->info);
        free(state->pixels);
        set_error(error, error_size, "JPEG 解码失败：%.120s",
                  state->message);
        free(state);
        return -1;
    }

    jpeg_create_decompress(&state->info);
    state->created = true;
    jpeg_stdio_src(&state->info, file);
    (void)jpeg_read_header(&state->info, TRUE);
    state->info.out_color_space = JCS_RGB;
    unsigned int denominator = 1;
    while (denominator < 8U &&
           (state->info.image_width / denominator >
                COVER_SOURCE_MAX_DIMENSION ||
            state->info.image_height / denominator >
                COVER_SOURCE_MAX_DIMENSION))
        denominator *= 2U;
    state->info.scale_num = 1;
    state->info.scale_denom = denominator;
    (void)jpeg_start_decompress(&state->info);
    if (state->info.output_width == 0 || state->info.output_height == 0 ||
        state->info.output_width > COVER_SOURCE_MAX_DIMENSION ||
        state->info.output_height > COVER_SOURCE_MAX_DIMENSION ||
        state->info.output_components != 3) {
        jpeg_destroy_decompress(&state->info);
        state->created = false;
        set_error(error, error_size, "不支持的 JPEG 尺寸");
        free(state);
        return -1;
    }
    size_t row_bytes = (size_t)state->info.output_width * 3U;
    size_t image_bytes = row_bytes * state->info.output_height;
    state->pixels = (uint8_t *)malloc(image_bytes);
    if (!state->pixels) {
        jpeg_destroy_decompress(&state->info);
        state->created = false;
        set_error(error, error_size, "内存不足，无法解码封面");
        free(state);
        return -1;
    }
    while (state->info.output_scanline < state->info.output_height) {
        JSAMPROW row = state->pixels +
            (size_t)state->info.output_scanline * row_bytes;
        (void)jpeg_read_scanlines(&state->info, &row, 1);
    }
    unsigned int width = state->info.output_width;
    unsigned int height = state->info.output_height;
    (void)jpeg_finish_decompress(&state->info);
    jpeg_destroy_decompress(&state->info);
    state->created = false;
    scale_to_tiled(state->pixels, width, height, 3U, tiled);
    free(state->pixels);
    free(state);
    return 0;
}

static void png_fail(png_structp png, png_const_charp message) {
    PngDecodeState *state = (PngDecodeState *)png_get_error_ptr(png);
    if (state && message)
        snprintf(state->message, sizeof(state->message), "%s", message);
    png_longjmp(png, 1);
}

static void png_warn(png_structp png, png_const_charp message) {
    (void)png;
    (void)message;
}

static int decode_png(FILE *file, uint32_t *tiled,
                      char *error, size_t error_size) {
    PngDecodeState *state =
        (PngDecodeState *)calloc(1, sizeof(*state));
    if (!state) {
        set_error(error, error_size, "内存不足，无法解码封面");
        return -1;
    }
    state->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, state,
                                        png_fail, png_warn);
    if (!state->png) {
        set_error(error, error_size, "内存不足，无法解码封面");
        free(state);
        return -1;
    }
    state->info = png_create_info_struct(state->png);
    if (!state->info) {
        png_destroy_read_struct(&state->png, NULL, NULL);
        set_error(error, error_size, "内存不足，无法解码封面");
        free(state);
        return -1;
    }
    if (setjmp(png_jmpbuf(state->png))) {
        png_destroy_read_struct(&state->png, &state->info, NULL);
        free(state->pixels);
        set_error(error, error_size, "PNG 解码失败：%.120s",
                  state->message[0] ? state->message : "invalid PNG");
        free(state);
        return -1;
    }

    png_set_user_limits(state->png, COVER_SOURCE_MAX_DIMENSION,
                        COVER_SOURCE_MAX_DIMENSION);
    png_set_chunk_malloc_max(state->png, COVER_PNG_CHUNK_LIMIT);
    png_set_chunk_cache_max(state->png, COVER_PNG_CHUNK_COUNT_LIMIT);
    png_init_io(state->png, file);
    png_set_sig_bytes(state->png, 8);
    png_read_info(state->png, state->info);

    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = 0;
    int interlace = 0;
    if (!png_get_IHDR(state->png, state->info, &width, &height,
                      &bit_depth, &color_type, &interlace, NULL, NULL) ||
        width == 0 || height == 0 ||
        width > COVER_SOURCE_MAX_DIMENSION ||
        height > COVER_SOURCE_MAX_DIMENSION) {
        png_error(state->png, "unsupported PNG dimensions");
    }
    if (bit_depth == 16) png_set_strip_16(state->png);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(state->png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(state->png);
    bool has_transparency =
        png_get_valid(state->png, state->info, PNG_INFO_tRNS) != 0;
    if (has_transparency) png_set_tRNS_to_alpha(state->png);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(state->png);
    if (!(color_type & PNG_COLOR_MASK_ALPHA) && !has_transparency)
        png_set_add_alpha(state->png, 0xffU, PNG_FILLER_AFTER);

    int passes = png_set_interlace_handling(state->png);
    png_read_update_info(state->png, state->info);
    png_size_t row_bytes = png_get_rowbytes(state->png, state->info);
    if (png_get_bit_depth(state->png, state->info) != 8 ||
        png_get_channels(state->png, state->info) != 4 ||
        row_bytes != (png_size_t)width * 4U ||
        height > SIZE_MAX / row_bytes) {
        png_error(state->png, "unsupported PNG pixel layout");
    }
    if (passes == 1) {
        state->pixels = (uint8_t *)malloc(row_bytes);
        if (!state->pixels) png_error(state->png, "out of memory");
        unsigned int target_y = 0;
        for (png_uint_32 source_y = 0; source_y < height; source_y++) {
            png_read_row(state->png, state->pixels, NULL);
            while (target_y < COVER_ART_SIZE &&
                   target_y * height / COVER_ART_SIZE == source_y) {
                scale_row_to_tiled(state->pixels, (unsigned int)width,
                                   4U, target_y, tiled);
                target_y++;
            }
        }
        if (target_y != COVER_ART_SIZE)
            png_error(state->png, "incomplete PNG image");
    } else {
        size_t image_bytes = row_bytes * (size_t)height;
        state->pixels = (uint8_t *)calloc(1, image_bytes);
        if (!state->pixels) png_error(state->png, "out of memory");
        for (int pass = 0; pass < passes; pass++) {
            for (png_uint_32 y = 0; y < height; y++)
                png_read_row(state->png,
                             state->pixels + (size_t)y * row_bytes, NULL);
        }
        scale_to_tiled(state->pixels, (unsigned int)width,
                       (unsigned int)height, 4U, tiled);
    }
    png_read_end(state->png, NULL);
    png_destroy_read_struct(&state->png, &state->info, NULL);
    free(state->pixels);
    free(state);
    return 0;
}

int cover_decode_image(const char *path, uint32_t *tiled,
                       size_t pixel_count, char *error, size_t error_size) {
    if (!path || !tiled || pixel_count < COVER_ART_PIXELS) {
        set_error(error, error_size, "封面图像无效");
        return -1;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error(error, error_size, "无法打开缓存封面");
        return -1;
    }
    uint8_t header[8];
    size_t header_size = fread(header, 1, sizeof(header), file);
    CoverImageFormat format = cover_image_format(header, header_size);
    int result = -1;
    if (format == COVER_IMAGE_PNG) {
        result = decode_png(file, tiled, error, error_size);
    } else if (format == COVER_IMAGE_JPEG) {
        rewind(file);
        result = decode_jpeg(file, tiled, error, error_size);
    } else {
        set_error(error, error_size, "封面格式不受支持");
    }
    fclose(file);
    return result;
}
