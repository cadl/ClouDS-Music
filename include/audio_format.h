#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* "fLaC", one STREAMINFO block header and its 34-byte body. */
#define AUDIO_FLAC_STREAMINFO_SIZE 34U
#define AUDIO_FLAC_HEADER_SIZE (4U + 4U + AUDIO_FLAC_STREAMINFO_SIZE)
/* NDSP output is 32728 Hz; feeding it more than 48 kHz only adds DSP and
 * queue pressure, so higher rates are decimated by a power of two. */
#define AUDIO_OUTPUT_RATE_MAX 48000U
#define AUDIO_DECIMATION_MAX 8U

typedef enum {
    AUDIO_FORMAT_UNKNOWN = 0,
    /* Anything that is not FLAC is handed to minimp3, which validates it. */
    AUDIO_FORMAT_MP3,
    AUDIO_FORMAT_FLAC
} AudioFormat;

typedef enum {
    AUDIO_PROBE_DONE = 0,
    AUDIO_PROBE_NEED_MORE,
    AUDIO_PROBE_INVALID
} AudioProbeStatus;

typedef struct {
    uint32_t min_block;
    uint32_t max_block;
    uint32_t max_frame_bytes;
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
    uint64_t total_frames;
} AudioFlacInfo;

typedef struct {
    AudioFormat format;
    /* Byte offset of the first FLAC frame; zero for MP3. */
    uint64_t audio_offset;
    AudioFlacInfo flac;
    /* A minimal FLAC stream header containing only STREAMINFO.  Presenting
     * it in front of the frames keeps ID3 tags, embedded pictures and other
     * untrusted metadata away from the decoder. */
    uint8_t flac_header[AUDIO_FLAC_HEADER_SIZE];
} AudioProbe;

/* Reads exactly `size` bytes at `position`; returns false on I/O failure. */
typedef bool (*AudioProbeRead)(void *userdata, uint64_t position,
                               void *buffer, size_t size);

/* Identifies the container using only bytes below `available`.  When more
 * data is required, returns AUDIO_PROBE_NEED_MORE and sets `needed` to the
 * byte count that must be available before trying again.  `format` is FLAC
 * as soon as the stream marker is seen, even when the header is incomplete
 * or invalid, so a truncated complete file is never mistaken for MP3. */
AudioProbeStatus audio_probe(AudioProbeRead read, void *userdata,
                             uint64_t available, AudioProbe *probe,
                             uint64_t *needed);

/* Length of the virtual FLAC stream presented to the decoder. */
uint64_t audio_flac_virtual_size(const AudioProbe *probe, uint64_t file_size);

/* Serves virtual stream bytes that come from the synthesized header.  Returns
 * how many bytes were copied; `real_position` receives the file offset of
 * the first byte that must be read from the file itself. */
size_t audio_flac_virtual_header(const AudioProbe *probe, uint64_t position,
                                 void *buffer, size_t size,
                                 uint64_t *real_position);

/* Worst-case compressed bytes the decoder may request while producing
 * `frames` PCM frames, including its internal read-ahead.  Encoders fall back
 * to verbatim subframes; for fixed-blocksize streams the STREAMINFO maximum
 * frame size also bounds unusual encodings that exceed verbatim. */
uint64_t audio_flac_read_bound(const AudioFlacInfo *info, uint64_t frames,
                               size_t reader_buffer);

/* Estimated FLAC bitrate in bytes per second of audio. */
uint64_t audio_flac_bytes_per_second(const AudioFlacInfo *info,
                                     uint64_t audio_bytes);

/* Power-of-two factor that brings `sample_rate` to AUDIO_OUTPUT_RATE_MAX or
 * below, or zero when the rate is unsupported. */
unsigned int audio_decimation_factor(uint32_t sample_rate);

/* Averages each group of `factor` input frames into one output frame.
 * Returns the number of output frames; a trailing partial group is dropped. */
size_t audio_decimate_s16(const int16_t *input, size_t input_frames,
                          unsigned int channels, unsigned int factor,
                          int16_t *output);
