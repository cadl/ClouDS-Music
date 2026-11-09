#pragma once

/* Every translation unit that uses dr_flac must see the same configuration:
 * DR_FLAC_BUFFER_SIZE changes the layout of the decoder object. */
#define DR_FLAC_NO_STDIO
#define DR_FLAC_NO_WCHAR
#define DR_FLAC_NO_OGG
#define DR_FLAC_NO_SIMD
#define DR_FLAC_BUFFER_SIZE 4096

#include "dr_flac.h"
