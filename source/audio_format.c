#include "audio_format.h"

#include <string.h>

#define AUDIO_PROBE_MAX_ID3_TAGS 8U
#define AUDIO_PROBE_MAX_FLAC_BLOCKS 1024U
#define FLAC_BLOCK_STREAMINFO 0U
#define FLAC_BLOCK_INVALID 127U
#define FLAC_MIN_BLOCK 16U
/* Frame header (at most 16 bytes) plus CRC-16. */
#define FLAC_FRAME_OVERHEAD 18U
/* Subframe header and a worst-case wasted-bits prefix. */
#define FLAC_SUBFRAME_OVERHEAD 5U

typedef enum {
    FETCH_OK = 0,
    FETCH_MORE,
    FETCH_ERROR
} FetchResult;

static FetchResult fetch(AudioProbeRead read, void *userdata,
                         uint64_t available, uint64_t position,
                         void *buffer, size_t size, uint64_t *needed) {
    if (position > available || size > available - position) {
        if (needed)
            *needed = position > UINT64_MAX - size ? UINT64_MAX :
                      position + size;
        return FETCH_MORE;
    }
    return read(userdata, position, buffer, size) ? FETCH_OK : FETCH_ERROR;
}

static AudioProbeStatus fetch_status(FetchResult result) {
    return result == FETCH_MORE ? AUDIO_PROBE_NEED_MORE :
                                  AUDIO_PROBE_INVALID;
}

static uint32_t read_be16(const uint8_t *bytes) {
    return (uint32_t)bytes[0] << 8 | bytes[1];
}

static uint32_t read_be24(const uint8_t *bytes) {
    return (uint32_t)bytes[0] << 16 | (uint32_t)bytes[1] << 8 | bytes[2];
}

static uint64_t read_be64(const uint8_t *bytes) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; i++) value = value << 8 | bytes[i];
    return value;
}

/* Returns the full ID3v2 tag length, or zero when `header` is not one. */
static uint64_t id3v2_length(const uint8_t header[10]) {
    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3' ||
        header[3] == 0xFF || header[4] == 0xFF)
        return 0;
    uint64_t size = 0;
    for (size_t i = 6; i < 10; i++) {
        if (header[i] & 0x80U) return 0;
        size = size << 7 | header[i];
    }
    return 10U + size + ((header[5] & 0x10U) ? 10U : 0U);
}

static bool parse_streaminfo(const uint8_t body[AUDIO_FLAC_STREAMINFO_SIZE],
                             AudioFlacInfo *info) {
    memset(info, 0, sizeof(*info));
    info->min_block = read_be16(body);
    info->max_block = read_be16(body + 2);
    info->max_frame_bytes = read_be24(body + 7);
    uint64_t packed = read_be64(body + 10);
    info->sample_rate = (uint32_t)(packed >> 44);
    info->channels = (uint32_t)((packed >> 41) & 0x7U) + 1U;
    info->bits_per_sample = (uint32_t)((packed >> 36) & 0x1FU) + 1U;
    info->total_frames = packed & UINT64_C(0xFFFFFFFFF);
    return info->sample_rate > 0 && info->bits_per_sample >= 4U &&
           info->max_block >= FLAC_MIN_BLOCK &&
           info->min_block <= info->max_block;
}

AudioProbeStatus audio_probe(AudioProbeRead read, void *userdata,
                             uint64_t available, AudioProbe *probe,
                             uint64_t *needed) {
    if (!read || !probe) return AUDIO_PROBE_INVALID;
    memset(probe, 0, sizeof(*probe));
    if (needed) *needed = 0;

    uint64_t position = 0;
    uint8_t header[10];
    for (unsigned int tags = 0;; tags++) {
        FetchResult result = fetch(read, userdata, available, position,
                                   header, 4, needed);
        if (result != FETCH_OK) return fetch_status(result);
        if (header[0] != 'I' || header[1] != 'D' || header[2] != '3' ||
            tags >= AUDIO_PROBE_MAX_ID3_TAGS)
            break;
        result = fetch(read, userdata, available, position,
                       header, sizeof(header), needed);
        if (result != FETCH_OK) return fetch_status(result);
        uint64_t length = id3v2_length(header);
        if (length == 0) break;
        position += length;
    }

    if (memcmp(header, "fLaC", 4) != 0) {
        probe->format = AUDIO_FORMAT_MP3;
        return AUDIO_PROBE_DONE;
    }

    probe->format = AUDIO_FORMAT_FLAC;
    position += 4;
    uint8_t block[4];
    uint8_t streaminfo[AUDIO_FLAC_STREAMINFO_SIZE];
    FetchResult result = fetch(read, userdata, available, position,
                               block, sizeof(block), needed);
    if (result != FETCH_OK) return fetch_status(result);
    if ((block[0] & 0x7FU) != FLAC_BLOCK_STREAMINFO ||
        read_be24(block + 1) != AUDIO_FLAC_STREAMINFO_SIZE)
        return AUDIO_PROBE_INVALID;
    result = fetch(read, userdata, available, position + sizeof(block),
                   streaminfo, sizeof(streaminfo), needed);
    if (result != FETCH_OK) return fetch_status(result);
    if (!parse_streaminfo(streaminfo, &probe->flac))
        return AUDIO_PROBE_INVALID;
    bool last = (block[0] & 0x80U) != 0;
    position += sizeof(block) + sizeof(streaminfo);

    for (unsigned int blocks = 1; !last; blocks++) {
        if (blocks >= AUDIO_PROBE_MAX_FLAC_BLOCKS) return AUDIO_PROBE_INVALID;
        result = fetch(read, userdata, available, position,
                       block, sizeof(block), needed);
        if (result != FETCH_OK) return fetch_status(result);
        if ((block[0] & 0x7FU) == FLAC_BLOCK_INVALID)
            return AUDIO_PROBE_INVALID;
        last = (block[0] & 0x80U) != 0;
        position += sizeof(block) + read_be24(block + 1);
    }

    /* A frame sync code must follow the last metadata block; anything else
     * means a block length is wrong and every later offset is untrustworthy. */
    uint8_t sync[2];
    result = fetch(read, userdata, available, position,
                   sync, sizeof(sync), needed);
    if (result != FETCH_OK) return fetch_status(result);
    if (sync[0] != 0xFFU || (sync[1] & 0xFEU) != 0xF8U)
        return AUDIO_PROBE_INVALID;

    probe->audio_offset = position;
    memcpy(probe->flac_header, "fLaC", 4);
    probe->flac_header[4] = 0x80U | FLAC_BLOCK_STREAMINFO;
    probe->flac_header[5] = 0;
    probe->flac_header[6] = 0;
    probe->flac_header[7] = AUDIO_FLAC_STREAMINFO_SIZE;
    memcpy(probe->flac_header + 8, streaminfo, sizeof(streaminfo));
    return AUDIO_PROBE_DONE;
}

uint64_t audio_flac_virtual_size(const AudioProbe *probe, uint64_t file_size) {
    if (!probe || file_size <= probe->audio_offset)
        return AUDIO_FLAC_HEADER_SIZE;
    return AUDIO_FLAC_HEADER_SIZE + (file_size - probe->audio_offset);
}

size_t audio_flac_virtual_header(const AudioProbe *probe, uint64_t position,
                                 void *buffer, size_t size,
                                 uint64_t *real_position) {
    size_t copied = 0;
    if (probe && buffer && position < AUDIO_FLAC_HEADER_SIZE) {
        copied = AUDIO_FLAC_HEADER_SIZE - (size_t)position;
        if (copied > size) copied = size;
        memcpy(buffer, probe->flac_header + position, copied);
    }
    if (real_position) {
        uint64_t next = position + copied;
        uint64_t offset = probe ? probe->audio_offset : 0;
        *real_position = next > AUDIO_FLAC_HEADER_SIZE ?
                         offset + (next - AUDIO_FLAC_HEADER_SIZE) : offset;
    }
    return copied;
}

uint64_t audio_flac_read_bound(const AudioFlacInfo *info, uint64_t frames,
                               size_t reader_buffer) {
    if (!info || info->max_block == 0 || info->channels == 0 ||
        info->bits_per_sample == 0)
        return UINT64_MAX;
    /* The decoder finishes whole FLAC frames, so the last one decoded can
     * overshoot the request by up to one maximum-size block. */
    uint64_t covered = frames + info->max_block;
    uint64_t min_block = info->min_block >= FLAC_MIN_BLOCK ?
                         info->min_block : FLAC_MIN_BLOCK;
    if (min_block > info->max_block) min_block = info->max_block;
    uint64_t frame_count = covered / min_block + 1U;
    /* Encoders fall back to verbatim subframes, the largest legal encoding;
     * one extra bit per sample covers a side channel. */
    uint64_t sample_bits = (uint64_t)info->channels *
                           (info->bits_per_sample + 1U);
    uint64_t payload = (covered * sample_bits + 7U) / 8U;
    uint64_t overhead = frame_count *
        (FLAC_FRAME_OVERHEAD + info->channels * FLAC_SUBFRAME_OVERHEAD);
    uint64_t bound = payload + overhead;
    /* With variable block sizes the frame count above is too pessimistic to
     * combine with the maximum frame size. */
    if (info->min_block == info->max_block && info->max_frame_bytes > 0 &&
        frame_count * info->max_frame_bytes > bound)
        bound = frame_count * info->max_frame_bytes;
    return bound + reader_buffer;
}

uint64_t audio_flac_bytes_per_second(const AudioFlacInfo *info,
                                     uint64_t audio_bytes) {
    if (!info || info->sample_rate == 0) return 0;
    if (info->total_frames > 0 && audio_bytes > 0) {
        if (audio_bytes <= UINT64_MAX / info->sample_rate)
            return audio_bytes * info->sample_rate / info->total_frames + 1U;
        return audio_bytes / info->total_frames * info->sample_rate + 1U;
    }
    /* Unknown length: assume the typical 60% FLAC compression ratio. */
    uint64_t raw = (uint64_t)info->sample_rate * info->channels *
                   info->bits_per_sample / 8U;
    return raw * 3U / 5U + 1U;
}

unsigned int audio_decimation_factor(uint32_t sample_rate) {
    if (sample_rate == 0) return 0;
    unsigned int factor = 1;
    while (sample_rate / factor > AUDIO_OUTPUT_RATE_MAX) {
        factor *= 2U;
        if (factor > AUDIO_DECIMATION_MAX) return 0;
    }
    return factor;
}

size_t audio_decimate_s16(const int16_t *input, size_t input_frames,
                          unsigned int channels, unsigned int factor,
                          int16_t *output) {
    if (!input || !output || channels == 0 || factor == 0) return 0;
    size_t frames = input_frames / factor;
    /* Output never overtakes input, so decimating in place is safe. */
    for (size_t frame = 0; frame < frames; frame++) {
        const int16_t *group = input + frame * factor * channels;
        for (unsigned int channel = 0; channel < channels; channel++) {
            int32_t sum = 0;
            for (unsigned int i = 0; i < factor; i++)
                sum += group[i * channels + channel];
            output[frame * channels + channel] =
                (int16_t)(sum / (int32_t)factor);
        }
    }
    return frames;
}
