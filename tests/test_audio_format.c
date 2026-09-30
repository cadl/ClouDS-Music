#include "audio_format.h"
#include "flac_decoder.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_BUFFER_SIZE DR_FLAC_BUFFER_SIZE

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} Bytes;

typedef struct {
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits;
    uint32_t block;
    uint64_t frames;
    size_t id3_bytes;
    size_t picture_bytes;
    bool unknown_total;
} FlacSpec;

typedef struct {
    const Bytes *file;
    const AudioProbe *probe;
    uint64_t position;
    bool gated;
    uint64_t window_start;
    uint64_t window_end;
    bool violation;
} VirtualReader;

static void bytes_reserve(Bytes *bytes, size_t extra) {
    if (bytes->size + extra <= bytes->capacity) return;
    size_t capacity = bytes->capacity ? bytes->capacity : 1024;
    while (capacity < bytes->size + extra) capacity *= 2;
    bytes->data = (uint8_t *)realloc(bytes->data, capacity);
    assert(bytes->data);
    bytes->capacity = capacity;
}

static void put(Bytes *bytes, const void *data, size_t size) {
    bytes_reserve(bytes, size);
    memcpy(bytes->data + bytes->size, data, size);
    bytes->size += size;
}

static void put_u8(Bytes *bytes, uint8_t value) { put(bytes, &value, 1); }

static void put_be(Bytes *bytes, uint64_t value, unsigned int count) {
    for (unsigned int i = count; i > 0; i--)
        put_u8(bytes, (uint8_t)(value >> ((i - 1) * 8)));
}

static uint8_t crc8(const uint8_t *data, size_t size) {
    uint8_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (uint8_t)(crc & 0x80 ? (crc << 1) ^ 0x07 : crc << 1);
    }
    return crc;
}

static uint16_t crc16(const uint8_t *data, size_t size) {
    uint16_t crc = 0;
    for (size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++)
            crc = (uint16_t)(crc & 0x8000 ? (crc << 1) ^ 0x8005 : crc << 1);
    }
    return crc;
}

static int32_t source_sample(uint64_t frame, uint32_t channel,
                             uint32_t bits) {
    /* Deterministic, full-scale-ish content that differs per channel. */
    uint32_t x = (uint32_t)(frame * 2654435761U) ^ (channel * 0x9E3779B9U);
    x ^= x >> 13;
    x *= 0x5bd1e995U;
    x ^= x >> 15;
    int32_t range = 1 << (bits - 1);
    return (int32_t)(x % (uint32_t)(2 * range - 1)) - (range - 1);
}

static int16_t expected_s16(uint64_t frame, uint32_t channel, uint32_t bits) {
    int32_t sample = source_sample(frame, channel, bits);
    return (int16_t)(bits > 16 ? sample >> (bits - 16) :
                                 sample * (1 << (16 - bits)));
}

static void put_utf8_number(Bytes *bytes, uint64_t value) {
    if (value < 0x80) {
        put_u8(bytes, (uint8_t)value);
        return;
    }
    unsigned int count = value < 0x800 ? 2 : value < 0x10000 ? 3 : 4;
    assert(value < 0x200000);
    put_u8(bytes, (uint8_t)((0xFF00U >> count) |
                            (value >> (6 * (count - 1)))));
    for (unsigned int i = count - 1; i > 0; i--)
        put_u8(bytes, (uint8_t)(0x80 | ((value >> (6 * (i - 1))) & 0x3F)));
}

static Bytes encode_flac(const FlacSpec *spec) {
    Bytes out = {0};
    if (spec->id3_bytes) {
        uint32_t body = (uint32_t)spec->id3_bytes;
        uint8_t header[10] = {'I', 'D', '3', 4, 0, 0,
                              (uint8_t)((body >> 21) & 0x7F),
                              (uint8_t)((body >> 14) & 0x7F),
                              (uint8_t)((body >> 7) & 0x7F),
                              (uint8_t)(body & 0x7F)};
        put(&out, header, sizeof(header));
        for (size_t i = 0; i < spec->id3_bytes; i++) put_u8(&out, 0x55);
    }
    put(&out, "fLaC", 4);
    put_u8(&out, spec->picture_bytes ? 0x00 : 0x80);
    put_be(&out, AUDIO_FLAC_STREAMINFO_SIZE, 3);
    put_be(&out, spec->block, 2);
    put_be(&out, spec->block, 2);
    put_be(&out, 0, 3);
    put_be(&out, 0, 3);
    uint64_t total = spec->unknown_total ? 0 : spec->frames;
    uint64_t packed = (uint64_t)spec->sample_rate << 44 |
                      (uint64_t)(spec->channels - 1) << 41 |
                      (uint64_t)(spec->bits - 1) << 36 | total;
    put_be(&out, packed, 8);
    for (int i = 0; i < 16; i++) put_u8(&out, 0);
    if (spec->picture_bytes) {
        put_u8(&out, 0x80 | 6);
        put_be(&out, spec->picture_bytes, 3);
        /* Fake sync codes inside metadata must not confuse the probe. */
        for (size_t i = 0; i < spec->picture_bytes; i++)
            put_u8(&out, i % 2 ? 0xF8 : 0xFF);
    }

    uint8_t size_code = spec->bits == 16 ? 4 : spec->bits == 24 ? 6 : 1;
    for (uint64_t start = 0, index = 0; start < spec->frames;
         start += spec->block, index++) {
        uint64_t block = spec->frames - start < spec->block ?
                         spec->frames - start : spec->block;
        size_t frame_start = out.size;
        put_u8(&out, 0xFF);
        put_u8(&out, 0xF8);
        put_u8(&out, 0x70);  /* 16-bit block size follows, rate from STREAMINFO */
        put_u8(&out, (uint8_t)((spec->channels - 1) << 4 | size_code << 1));
        put_utf8_number(&out, index);
        put_be(&out, block - 1, 2);
        put_u8(&out, crc8(out.data + frame_start, out.size - frame_start));
        for (uint32_t channel = 0; channel < spec->channels; channel++) {
            put_u8(&out, 0x02);  /* verbatim, no wasted bits */
            for (uint64_t i = 0; i < block; i++) {
                int32_t sample = source_sample(start + i, channel, spec->bits);
                put_be(&out, (uint32_t)sample, spec->bits / 8);
            }
        }
        put_be(&out, crc16(out.data + frame_start, out.size - frame_start), 2);
    }
    return out;
}

static bool memory_read(void *userdata, uint64_t position,
                        void *buffer, size_t size) {
    const Bytes *bytes = (const Bytes *)userdata;
    if (position > bytes->size || size > bytes->size - position) return false;
    memcpy(buffer, bytes->data + position, size);
    return true;
}

static size_t reader_read(void *userdata, void *buffer, size_t size) {
    VirtualReader *reader = (VirtualReader *)userdata;
    uint8_t *output = (uint8_t *)buffer;
    uint64_t real = 0;
    size_t copied = audio_flac_virtual_header(reader->probe, reader->position,
                                              output, size, &real);
    reader->position += copied;
    if (copied == size || real >= reader->file->size) return copied;
    size_t wanted = size - copied;
    if (wanted > reader->file->size - real)
        wanted = (size_t)(reader->file->size - real);
    if (reader->gated &&
        (real < reader->window_start || real + wanted > reader->window_end))
        reader->violation = true;
    memcpy(output + copied, reader->file->data + real, wanted);
    reader->position += wanted;
    return copied + wanted;
}

static drflac_bool32 reader_seek(void *userdata, int offset,
                                 drflac_seek_origin origin) {
    VirtualReader *reader = (VirtualReader *)userdata;
    uint64_t size = audio_flac_virtual_size(reader->probe, reader->file->size);
    uint64_t target;
    if (origin == DRFLAC_SEEK_SET) target = (uint64_t)offset;
    else if (origin == DRFLAC_SEEK_CUR) target = reader->position + offset;
    else return DRFLAC_FALSE;
    if (target > size) return DRFLAC_FALSE;
    reader->position = target;
    return DRFLAC_TRUE;
}

static drflac_bool32 reader_tell(void *userdata, drflac_int64 *cursor) {
    *cursor = (drflac_int64)((VirtualReader *)userdata)->position;
    return DRFLAC_TRUE;
}

static uint64_t reader_real_position(const VirtualReader *reader) {
    uint64_t real = 0;
    (void)audio_flac_virtual_header(reader->probe, reader->position,
                                    NULL, 0, &real);
    return real;
}

static void probe_file(const Bytes *file, AudioProbe *probe) {
    uint64_t needed = 0;
    assert(audio_probe(memory_read, (void *)file, file->size, probe,
                       &needed) == AUDIO_PROBE_DONE);
    assert(probe->format == AUDIO_FORMAT_FLAC);
}

/* Decodes the whole stream, allowing each call to touch only the bytes the
 * player's streaming gate guarantees to be resident. */
static void check_gated_decode(const Bytes *file, const FlacSpec *spec,
                               uint64_t frames_per_call) {
    AudioProbe probe;
    probe_file(file, &probe);
    VirtualReader reader = {file, &probe, 0, false, 0, 0, false};
    drflac *flac = drflac_open(reader_read, reader_seek, reader_tell,
                               &reader, NULL);
    assert(flac);
    assert(reader.position == AUDIO_FLAC_HEADER_SIZE);
    assert(flac->channels == spec->channels);
    assert(flac->sampleRate == spec->sample_rate);
    uint64_t bound = audio_flac_read_bound(&probe.flac, frames_per_call,
                                           TEST_BUFFER_SIZE);
    int16_t *pcm = (int16_t *)malloc((size_t)frames_per_call *
                                     spec->channels * sizeof(int16_t));
    assert(pcm);
    uint64_t decoded = 0;
    reader.gated = true;
    for (;;) {
        reader.window_start = reader_real_position(&reader);
        reader.window_end = reader.window_start + bound;
        uint64_t count = drflac_read_pcm_frames_s16(flac, frames_per_call, pcm);
        assert(!reader.violation);
        if (count == 0) break;
        for (uint64_t i = 0; i < count; i++)
            for (uint32_t c = 0; c < spec->channels; c++)
                assert(pcm[i * spec->channels + c] ==
                       expected_s16(decoded + i, c, spec->bits));
        decoded += count;
    }
    assert(decoded == spec->frames);
    free(pcm);
    drflac_close(flac);
}

static void check_seek(const Bytes *file, const FlacSpec *spec) {
    AudioProbe probe;
    probe_file(file, &probe);
    VirtualReader reader = {file, &probe, 0, false, 0, 0, false};
    drflac *flac = drflac_open(reader_read, reader_seek, reader_tell,
                               &reader, NULL);
    assert(flac);
    assert(flac->totalPCMFrameCount == spec->frames);
    const uint64_t targets[] = {spec->frames / 2, 1, spec->frames - 10,
                                spec->block * 3 + 7, spec->frames / 3, 0};
    int16_t pcm[64 * 2];
    for (size_t t = 0; t < sizeof(targets) / sizeof(targets[0]); t++) {
        assert(drflac_seek_to_pcm_frame(flac, targets[t]));
        uint64_t count = drflac_read_pcm_frames_s16(flac, 8, pcm);
        assert(count == 8);
        for (uint64_t i = 0; i < count; i++)
            for (uint32_t c = 0; c < spec->channels; c++)
                assert(pcm[i * spec->channels + c] ==
                       expected_s16(targets[t] + i, c, spec->bits));
    }
    drflac_close(flac);
}

static void test_probe(void) {
    FlacSpec spec = {44100, 2, 16, 4096, 44100, 1234, 300000, false};
    Bytes file = encode_flac(&spec);
    AudioProbe probe;
    probe_file(&file, &probe);
    assert(probe.audio_offset == 10 + 1234 + 4 + 4 + 34 + 4 + 300000);
    assert(probe.flac.sample_rate == 44100);
    assert(probe.flac.channels == 2);
    assert(probe.flac.bits_per_sample == 16);
    assert(probe.flac.min_block == 4096 && probe.flac.max_block == 4096);
    assert(probe.flac.total_frames == 44100);
    assert(memcmp(probe.flac_header, "fLaC\x80\x00\x00\x22", 8) == 0);
    assert(memcmp(probe.flac_header + 8, file.data + 10 + 1234 + 8,
                  AUDIO_FLAC_STREAMINFO_SIZE) == 0);

    /* Incremental probing asks for exactly the next piece it needs. */
    uint64_t available = 0;
    unsigned int rounds = 0;
    for (;;) {
        uint64_t needed = 0;
        AudioProbeStatus status = audio_probe(memory_read, &file, available,
                                              &probe, &needed);
        if (status == AUDIO_PROBE_DONE) break;
        assert(status == AUDIO_PROBE_NEED_MORE);
        assert(needed > available && needed <= file.size);
        available = needed;
        assert(++rounds < 16);
    }
    assert(probe.format == AUDIO_FORMAT_FLAC);
    assert(available == probe.audio_offset + 2);

    /* A wrong metadata length is caught by the frame sync check. */
    file.data[10 + 1234 + 4 + 4 + 34 + 3] ^= 0x01;
    uint64_t needed = 0;
    assert(audio_probe(memory_read, &file, file.size, &probe, &needed) ==
           AUDIO_PROBE_INVALID);
    file.data[10 + 1234 + 4 + 4 + 34 + 3] ^= 0x01;

    /* STREAMINFO must come first. */
    file.data[10 + 1234 + 4] = 0x81;
    assert(audio_probe(memory_read, &file, file.size, &probe, &needed) ==
           AUDIO_PROBE_INVALID);
    assert(probe.format == AUDIO_FORMAT_FLAC);
    file.data[10 + 1234 + 4] = 0x00;

    /* A complete file cut inside its metadata is still reported as FLAC. */
    assert(audio_probe(memory_read, &file, 10 + 1234 + 4 + 20, &probe,
                       &needed) == AUDIO_PROBE_NEED_MORE);
    assert(probe.format == AUDIO_FORMAT_FLAC);
    free(file.data);

    Bytes mp3 = {0};
    const uint8_t id3[10] = {'I', 'D', '3', 3, 0, 0, 0, 0, 0, 4};
    put(&mp3, id3, sizeof(id3));
    put(&mp3, "\0\0\0\0\xFF\xFB\x90\x00", 8);
    assert(audio_probe(memory_read, &mp3, mp3.size, &probe, &needed) ==
           AUDIO_PROBE_DONE);
    assert(probe.format == AUDIO_FORMAT_MP3 && probe.audio_offset == 0);
    assert(audio_probe(memory_read, &mp3, 12, &probe, &needed) ==
           AUDIO_PROBE_NEED_MORE);
    assert(needed == 18);
    free(mp3.data);

    Bytes plain = {0};
    put(&plain, "\xFF\xFB\x90\x00", 4);
    assert(audio_probe(memory_read, &plain, plain.size, &probe, &needed) ==
           AUDIO_PROBE_DONE && probe.format == AUDIO_FORMAT_MP3);
    assert(audio_probe(memory_read, &plain, 3, &probe, &needed) ==
           AUDIO_PROBE_NEED_MORE && needed == 4);
    free(plain.data);
}

static void test_policy(void) {
    AudioFlacInfo cd = {4096, 4096, 0, 44100, 2, 16, 44100 * 180};
    uint64_t bound = audio_flac_read_bound(&cd, 4096, 4096);
    assert(bound == (4096U + 4096U) * 2U * 17U / 8U +
                    3U * (18U + 2U * 5U) + 4096U);
    /* Every supported stream must fit the 128 KiB progressive window. */
    assert(bound < 128U * 1024U);
    AudioFlacInfo hires = {4096, 4096, 0, 96000, 2, 24, 96000 * 180};
    assert(audio_flac_read_bound(&hires, 8192, 4096) < 128U * 1024U);
    AudioFlacInfo huge = {16384, 16384, 0, 192000, 2, 24, 0};
    assert(audio_flac_read_bound(&huge, 16384, 4096) > 128U * 1024U);
    AudioFlacInfo tiny_blocks = {0, 4608, 0, 44100, 2, 16, 0};
    assert(audio_flac_read_bound(&tiny_blocks, 4096, 0) >
           audio_flac_read_bound(&cd, 4096, 0));
    /* A reported maximum frame larger than verbatim raises fixed-block
     * bounds, but never variable-block ones. */
    AudioFlacInfo escaped = cd;
    escaped.max_frame_bytes = 20000;
    assert(audio_flac_read_bound(&escaped, 4096, 4096) ==
           3U * 20000U + 4096U);
    escaped.max_frame_bytes = 1000;
    assert(audio_flac_read_bound(&escaped, 4096, 4096) == bound);
    AudioFlacInfo variable = tiny_blocks;
    variable.max_frame_bytes = 20000;
    assert(audio_flac_read_bound(&variable, 4096, 0) ==
           audio_flac_read_bound(&tiny_blocks, 4096, 0));
    AudioFlacInfo empty = {0};
    assert(audio_flac_read_bound(&empty, 4096, 0) == UINT64_MAX);

    assert(audio_flac_bytes_per_second(&cd, 30U * 1000U * 1000U) ==
           30ULL * 1000U * 1000U * 44100U / (44100U * 180U) + 1U);
    AudioFlacInfo unknown = cd;
    unknown.total_frames = 0;
    assert(audio_flac_bytes_per_second(&unknown, 1000) ==
           44100U * 2U * 2U * 3U / 5U + 1U);

    assert(audio_decimation_factor(0) == 0);
    assert(audio_decimation_factor(8000) == 1);
    assert(audio_decimation_factor(44100) == 1);
    assert(audio_decimation_factor(48000) == 1);
    assert(audio_decimation_factor(88200) == 2);
    assert(audio_decimation_factor(96000) == 2);
    assert(audio_decimation_factor(176400) == 4);
    assert(audio_decimation_factor(192000) == 4);
    assert(audio_decimation_factor(384000) == 8);
    assert(audio_decimation_factor(400000) == 0);

    int16_t samples[] = {100, -100, 300, -300, 5, 7, 7, 9, 1, 1};
    assert(audio_decimate_s16(samples, 5, 2, 2, samples) == 2);
    assert(samples[0] == 200 && samples[1] == -200);
    assert(samples[2] == 6 && samples[3] == 8);
    int16_t extreme[] = {32767, 32767, -32768, -32768};
    int16_t reduced[1];
    assert(audio_decimate_s16(extreme, 4, 1, 4, reduced) == 1);
    assert(reduced[0] == 0);
    assert(audio_decimate_s16(extreme, 1, 1, 2, reduced) == 0);

    AudioProbe probe = {0};
    probe.audio_offset = 1000;
    memset(probe.flac_header, 0xAB, sizeof(probe.flac_header));
    uint8_t header[64];
    uint64_t real = 0;
    assert(audio_flac_virtual_header(&probe, 40, header, 64, &real) == 2);
    assert(header[0] == 0xAB && real == 1000);
    assert(audio_flac_virtual_header(&probe, 10, header, 4, &real) == 4);
    assert(audio_flac_virtual_header(&probe, 50, header, 4, &real) == 0);
    assert(real == 1008);
    assert(audio_flac_virtual_size(&probe, 5000) ==
           AUDIO_FLAC_HEADER_SIZE + 4000);
    assert(audio_flac_virtual_size(&probe, 10) == AUDIO_FLAC_HEADER_SIZE);
}

static void test_decode(void) {
    const FlacSpec specs[] = {
        {44100, 2, 16, 4096, 44100 * 3 + 123, 0, 0, false},
        {44100, 1, 16, 4608, 30000, 700, 200000, false},
        {96000, 2, 24, 4096, 96000 * 2 + 5, 0, 5000, false},
        {48000, 2, 16, 1152, 48000 * 2, 64, 0, false},
    };
    for (size_t i = 0; i < sizeof(specs) / sizeof(specs[0]); i++) {
        Bytes file = encode_flac(&specs[i]);
        unsigned int factor = audio_decimation_factor(specs[i].sample_rate);
        check_gated_decode(&file, &specs[i], 4096U * factor);
        check_seek(&file, &specs[i]);
        free(file.data);
    }

    /* Without a total length the stream still decodes from the start. */
    FlacSpec unknown = {44100, 2, 16, 4096, 20000, 0, 0, true};
    Bytes file = encode_flac(&unknown);
    AudioProbe probe;
    probe_file(&file, &probe);
    assert(probe.flac.total_frames == 0);
    check_gated_decode(&file, &unknown, 4096);
    free(file.data);
}

/* Optional real-encoder coverage: tests/test_audio_format file.flac ... */
static void check_external_file(const char *path) {
    FILE *input = fopen(path, "rb");
    assert(input);
    Bytes file = {0};
    uint8_t chunk[65536];
    size_t read;
    while ((read = fread(chunk, 1, sizeof(chunk), input)) > 0)
        put(&file, chunk, read);
    fclose(input);

    AudioProbe probe;
    probe_file(&file, &probe);
    unsigned int factor = audio_decimation_factor(probe.flac.sample_rate);
    assert(factor > 0);
    uint64_t frames_per_call = 4096U * factor;
    uint64_t bound = audio_flac_read_bound(&probe.flac, frames_per_call,
                                           TEST_BUFFER_SIZE);

    /* dr_flac mislocates metadata behind an ID3 prefix, so the reference
     * decoder starts at the native stream marker. */
    size_t marker = 0;
    while (marker + 4 <= file.size && memcmp(file.data + marker, "fLaC", 4))
        marker++;
    assert(marker + 4 <= file.size);
    drflac *direct = drflac_open_memory(file.data + marker,
                                        file.size - marker, NULL);
    VirtualReader reader = {&file, &probe, 0, true, 0, 0, false};
    drflac *flac = drflac_open(reader_read, reader_seek, reader_tell,
                               &reader, NULL);
    assert(direct && flac);
    size_t samples = (size_t)frames_per_call * probe.flac.channels;
    int16_t *expected = (int16_t *)malloc(samples * sizeof(int16_t));
    int16_t *actual = (int16_t *)malloc(samples * sizeof(int16_t));
    assert(expected && actual);
    uint64_t total = 0;
    for (;;) {
        reader.window_start = reader_real_position(&reader);
        reader.window_end = reader.window_start + bound;
        uint64_t want = drflac_read_pcm_frames_s16(direct, frames_per_call,
                                                   expected);
        uint64_t got = drflac_read_pcm_frames_s16(flac, frames_per_call,
                                                  actual);
        assert(!reader.violation);
        assert(want == got);
        assert(memcmp(expected, actual,
                      (size_t)got * probe.flac.channels * 2) == 0);
        if (got == 0) break;
        total += got;
    }
    assert(probe.flac.total_frames == 0 || total == probe.flac.total_frames);
    reader.gated = false;
    if (probe.flac.total_frames > 0) {
        uint64_t target = probe.flac.total_frames * 3 / 5;
        assert(drflac_seek_to_pcm_frame(direct, target));
        assert(drflac_seek_to_pcm_frame(flac, target));
        assert(drflac_read_pcm_frames_s16(direct, 256, expected) ==
               drflac_read_pcm_frames_s16(flac, 256, actual));
        assert(memcmp(expected, actual, 256U * probe.flac.channels * 2) == 0);
    }
    printf("  %s: %u Hz, %u ch, %u bit, offset %llu, bound %llu, ok\n",
           path, (unsigned int)probe.flac.sample_rate,
           (unsigned int)probe.flac.channels,
           (unsigned int)probe.flac.bits_per_sample,
           (unsigned long long)probe.audio_offset,
           (unsigned long long)bound);
    free(expected);
    free(actual);
    drflac_close(direct);
    drflac_close(flac);
    free(file.data);
}

int main(int argc, char **argv) {
    test_probe();
    test_policy();
    test_decode();
    for (int i = 1; i < argc; i++) check_external_file(argv[i]);
    puts("audio format tests passed");
    return 0;
}
