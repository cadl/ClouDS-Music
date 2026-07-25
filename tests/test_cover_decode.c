#include "cover_decode.h"
#include "gpu_texture.h"

#include <assert.h>
#include <stdio.h>

#include <jpeglib.h>
#include <png.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static void write_test_jpeg(const char *path) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    struct jpeg_compress_struct info;
    struct jpeg_error_mgr error;
    memset(&info, 0, sizeof(info));
    info.err = jpeg_std_error(&error);
    jpeg_create_compress(&info);
    jpeg_stdio_dest(&info, file);
    info.image_width = 8;
    info.image_height = 8;
    info.input_components = 3;
    info.in_color_space = JCS_RGB;
    jpeg_set_defaults(&info);
    jpeg_set_quality(&info, 100, TRUE);
    jpeg_start_compress(&info, TRUE);
    uint8_t row[8 * 3];
    for (size_t i = 0; i < sizeof(row); i += 3) {
        row[i] = 220;
        row[i + 1] = 40;
        row[i + 2] = 20;
    }
    while (info.next_scanline < info.image_height) {
        JSAMPROW scanline = row;
        assert(jpeg_write_scanlines(&info, &scanline, 1) == 1);
    }
    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
    assert(fclose(file) == 0);
}

static void write_invalid_image(const char *path) {
    static const uint8_t data[8] = {'n', 'o', 't', '-', 'i', 'm', 'g', '\n'};
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(data, 1, sizeof(data), file) == sizeof(data));
    assert(fclose(file) == 0);
}

static void write_test_png(const char *path, png_uint_32 width,
                           png_uint_32 height, bool interlaced) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    png_structp png = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    assert(png != NULL);
    png_infop info = png_create_info_struct(png);
    assert(info != NULL);
    assert(setjmp(png_jmpbuf(png)) == 0);
    png_init_io(png, file);
    png_set_IHDR(png, info, width, height, 8,
                 PNG_COLOR_TYPE_RGBA,
                 interlaced ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    int passes = png_set_interlace_handling(png);
    size_t row_size = (size_t)width * 4U;
    uint8_t *row = (uint8_t *)malloc(row_size);
    assert(row != NULL);
    for (png_uint_32 x = 0; x < width; x++) {
        row[(size_t)x * 4U] = 10;
        row[(size_t)x * 4U + 1U] = 20;
        row[(size_t)x * 4U + 2U] = 30;
        row[(size_t)x * 4U + 3U] = 64;
    }
    for (int pass = 0; pass < passes; pass++)
        for (png_uint_32 y = 0; y < height; y++)
            png_write_row(png, row);
    png_write_end(png, NULL);
    free(row);
    png_destroy_write_struct(&png, &info);
    assert(fclose(file) == 0);
}

int main(int argc, char **argv) {
    static const uint8_t png_signature[8] = {
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a
    };
    static const uint8_t jpeg_signature[3] = {0xff, 0xd8, 0xff};
    static const uint8_t unknown_signature[3] = {'G', 'I', 'F'};
    assert(cover_image_format(png_signature, sizeof(png_signature)) ==
           COVER_IMAGE_PNG);
    assert(cover_image_format(jpeg_signature, sizeof(jpeg_signature)) ==
           COVER_IMAGE_JPEG);
    assert(cover_image_format(unknown_signature,
                              sizeof(unknown_signature)) ==
           COVER_IMAGE_UNKNOWN);

    uint32_t *tiled = (uint32_t *)malloc(
        COVER_ART_PIXELS * sizeof(*tiled));
    assert(tiled != NULL);
    char error[192] = {0};
    assert(cover_decode_image("icon-v4.png", tiled, COVER_ART_PIXELS,
                              error, sizeof(error)) == 0);
    assert(tiled[tiled_offset(0, 0)] ==
           gpu_texture_rgba8(54, 179, 253, 255));
    assert(tiled[tiled_offset(64, 64)] ==
           gpu_texture_rgba8(240, 214, 204, 255));
    assert(tiled[tiled_offset(127, 127)] ==
           gpu_texture_rgba8(251, 190, 179, 255));
    assert(cover_decode_image("banner-v2.png", tiled, COVER_ART_PIXELS,
                              error, sizeof(error)) == 0);

    const char *interlaced_path =
        "/tmp/nm3ds-cover-decode-interlaced.png";
    write_test_png(interlaced_path, 16, 16, true);
    assert(cover_decode_image(interlaced_path, tiled, COVER_ART_PIXELS,
                              error, sizeof(error)) == 0);
    assert(tiled[tiled_offset(0, 0)] ==
           gpu_texture_rgba8(10, 20, 30, 64));
    assert(tiled[tiled_offset(127, 127)] ==
           gpu_texture_rgba8(10, 20, 30, 64));
    assert(remove(interlaced_path) == 0);

    const char *oversized_path =
        "/tmp/nm3ds-cover-decode-oversized.png";
    write_test_png(oversized_path, 257, 1, false);
    error[0] = '\0';
    assert(cover_decode_image(oversized_path, tiled, COVER_ART_PIXELS,
                              error, sizeof(error)) != 0);
    assert(error[0] != '\0');
    assert(remove(oversized_path) == 0);

    const char *jpeg_path = "/tmp/nm3ds-cover-decode.jpg";
    write_test_jpeg(jpeg_path);
    assert(cover_decode_image(jpeg_path, tiled, COVER_ART_PIXELS,
                              error, sizeof(error)) == 0);
    uint32_t red = tiled[tiled_offset(0, 0)];
    assert((red >> 24U) > 200U);
    assert(((red >> 16U) & 0xffU) < 80U);
    assert(((red >> 8U) & 0xffU) < 80U);
    assert((red & 0xffU) == 255U);
    assert(remove(jpeg_path) == 0);

    const char *invalid_path = "/tmp/nm3ds-cover-decode.invalid";
    write_invalid_image(invalid_path);
    error[0] = '\0';
    assert(cover_decode_image(invalid_path, tiled, COVER_ART_PIXELS,
                              error, sizeof(error)) != 0);
    assert(error[0] != '\0');
    assert(remove(invalid_path) == 0);

    if (argc > 1) {
        error[0] = '\0';
        assert(cover_decode_image(argv[1], tiled, COVER_ART_PIXELS,
                                  error, sizeof(error)) == 0);
    }
    free(tiled);
    puts("cover decoder tests: ok");
    return 0;
}
