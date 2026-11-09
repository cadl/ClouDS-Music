#include "player.h"

#include "audio_format.h"
#include "flac_decoder.h"
#include "i18n.h"
#include "media_policy.h"

#include <3ds.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_STDIO
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "minimp3_ex.h"
#pragma GCC diagnostic pop

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAYER_BUFFERS 6
#define PLAYER_FRAMES_PER_BUFFER 4096
#define PLAYER_REBUFFER_BYTES (64U * 1024U)
/* FLAC needs several times the bytes of standard MP3 per second, so leaving
 * an underrun waits for this much audio rather than a fixed byte count. */
#define PLAYER_FLAC_REBUFFER_SECONDS 4U
/* Decimated FLAC is decoded through this small staging buffer. */
#define PLAYER_FLAC_CHUNK_FRAMES 1024U

typedef enum {
    PLAYER_DECODER_NONE = 0,
    PLAYER_DECODER_MP3,
    PLAYER_DECODER_FLAC
} PlayerDecoderKind;

typedef struct {
    ProgressiveFile *source;
    uint64_t position;
} ProgressiveCursor;

typedef struct {
    FILE *file;
    PlayerCancelFn cancelled;
    void *cancel_userdata;
} PrepareFile;

typedef struct {
    FILE *file;
    bool failed;
} ProbeFile;

/* Byte source behind dr_flac.  The decoder reads a virtual stream made of the
 * synthesized STREAMINFO header followed by the file's FLAC frames. */
typedef struct {
    AudioProbe probe;
    ProgressiveFile *progressive;
    FILE *file;
    uint64_t file_position;
    /* Real file offset the decoder may not read past: the end of a complete
     * file, or the end of the resident prefetch window while streaming. */
    uint64_t limit;
    uint64_t position;
    PlayerCancelFn cancelled;
    void *cancel_userdata;
} FlacSource;

typedef struct {
    drflac *decoder;
    FlacSource *source;
    int16_t *scratch;
    unsigned int factor;
    /* Compressed bytes that must be resident before each buffer decode. */
    uint64_t read_bound;
    uint64_t rebuffer_bytes;
    bool seekable;
} FlacStream;

typedef struct {
    mp3dec_ex_t decoder;
    mp3dec_io_t io;
    ProgressiveCursor *cursor;
    bool decoder_open;
    int channels;
    int sample_rate;
    uint64_t total_frames;
} PreparedStream;

struct PreparedAudio {
    PlayerDecoderKind kind;
    mp3dec_ex_t decoder;
    mp3dec_io_t io;
    FILE *file;
    bool decoder_open;
    FlacStream flac;
    int channels;
    int sample_rate;
    uint64_t total_frames;
    bool seek_ready;
};

struct Player {
    PlayerDecoderKind kind;
    mp3dec_ex_t decoder;
    mp3dec_io_t io;
    FILE *file;
    ProgressiveCursor *stream_cursor;
    FlacStream flac;
    bool decoder_open;
    bool active;
    bool paused;
    bool buffering;
    bool stream_waiting;
    /* The wait is a network underrun rather than a prefetch window load. */
    bool stream_starved;
    bool streaming;
    bool indexing;
    bool eof;
    bool finished_latch;
    bool ndsp_ready;
    Result ndsp_result;
    int channels;
    int sample_rate;
    int16_t *pcm;
    ndspWaveBuf waves[PLAYER_BUFFERS];
    bool queued[PLAYER_BUFFERS];
    uint64_t played_frames;
    uint64_t total_frames;
    float volume;
};

static size_t file_read(void *buffer, size_t size, void *userdata) {
    return fread(buffer, 1, size, (FILE *)userdata);
}

static int file_seek(uint64_t position, void *userdata) {
    if (position > (uint64_t)LONG_MAX) return -1;
    return fseek((FILE *)userdata, (long)position, SEEK_SET);
}

static size_t prepare_file_read(void *buffer, size_t size, void *userdata) {
    PrepareFile *source = (PrepareFile *)userdata;
    if (!source || !source->file ||
        (source->cancelled && source->cancelled(source->cancel_userdata)))
        return 0;
    return fread(buffer, 1, size, source->file);
}

static int prepare_file_seek(uint64_t position, void *userdata) {
    PrepareFile *source = (PrepareFile *)userdata;
    if (!source || !source->file || position > (uint64_t)LONG_MAX ||
        (source->cancelled && source->cancelled(source->cancel_userdata)))
        return -1;
    return fseek(source->file, (long)position, SEEK_SET);
}

static size_t progressive_read(void *buffer, size_t size, void *userdata) {
    ProgressiveCursor *cursor = (ProgressiveCursor *)userdata;
    if (!cursor || !cursor->source) return 0;
    size_t read = progressive_file_read_at(cursor->source, cursor->position,
                                           buffer, size);
    cursor->position += read;
    return read;
}

static int progressive_seek(uint64_t position, void *userdata) {
    ProgressiveCursor *cursor = (ProgressiveCursor *)userdata;
    if (!cursor || !cursor->source) return -1;
    ProgressiveSnapshot snapshot;
    progressive_file_snapshot(cursor->source, &snapshot);
    if (position > snapshot.published) return -1;
    cursor->position = position;
    return 0;
}

static void progressive_cursor_destroy(ProgressiveCursor *cursor) {
    if (!cursor) return;
    progressive_file_release(cursor->source);
    free(cursor);
}

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

static bool probe_file_read(void *userdata, uint64_t position,
                            void *buffer, size_t size) {
    ProbeFile *source = (ProbeFile *)userdata;
    if (position > (uint64_t)LONG_MAX ||
        fseek(source->file, (long)position, SEEK_SET) != 0 ||
        fread(buffer, 1, size, source->file) != size) {
        source->failed = true;
        return false;
    }
    return true;
}

static uint64_t flac_source_real_position(const FlacSource *source) {
    uint64_t real = 0;
    (void)audio_flac_virtual_header(&source->probe, source->position,
                                    NULL, 0, &real);
    return real;
}

static size_t flac_source_read(void *userdata, void *buffer, size_t size) {
    FlacSource *source = (FlacSource *)userdata;
    if (source->cancelled && source->cancelled(source->cancel_userdata))
        return 0;
    uint8_t *output = (uint8_t *)buffer;
    uint64_t real = 0;
    size_t copied = audio_flac_virtual_header(&source->probe,
                                              source->position,
                                              output, size, &real);
    source->position += copied;
    if (copied == size || real >= source->limit) return copied;
    size_t wanted = size - copied;
    if (wanted > source->limit - real)
        wanted = (size_t)(source->limit - real);
    size_t read = 0;
    if (source->file) {
        if (source->file_position != real) {
            if (real > (uint64_t)LONG_MAX ||
                fseek(source->file, (long)real, SEEK_SET) != 0) {
                source->file_position = UINT64_MAX;
                return copied;
            }
            source->file_position = real;
        }
        read = fread(output + copied, 1, wanted, source->file);
        source->file_position = read == wanted ?
                                source->file_position + read : UINT64_MAX;
    } else if (source->progressive) {
        /* The streaming gate made [real, limit) resident beforehand, so this
         * never waits for I/O. */
        read = progressive_file_read_at(source->progressive, real,
                                        output + copied, wanted);
    }
    source->position += read;
    return copied + read;
}

static drflac_bool32 flac_source_seek(void *userdata, int offset,
                                      drflac_seek_origin origin) {
    FlacSource *source = (FlacSource *)userdata;
    if (offset < 0) return DRFLAC_FALSE;
    uint64_t target;
    if (origin == DRFLAC_SEEK_SET) target = (uint64_t)offset;
    else if (origin == DRFLAC_SEEK_CUR)
        target = source->position + (uint64_t)offset;
    else return DRFLAC_FALSE;
    if (target > audio_flac_virtual_size(&source->probe, source->limit))
        return DRFLAC_FALSE;
    source->position = target;
    return DRFLAC_TRUE;
}

static drflac_bool32 flac_source_tell(void *userdata, drflac_int64 *cursor) {
    *cursor = (drflac_int64)((FlacSource *)userdata)->position;
    return DRFLAC_TRUE;
}

static void flac_source_destroy(FlacSource *source) {
    if (!source) return;
    if (source->file) fclose(source->file);
    progressive_file_release(source->progressive);
    free(source);
}

static void flac_stream_close(FlacStream *stream) {
    if (!stream) return;
    if (stream->decoder) drflac_close(stream->decoder);
    flac_source_destroy(stream->source);
    free(stream->scratch);
    memset(stream, 0, sizeof(*stream));
}

/* Takes ownership of `source`, which is released on failure. */
static int flac_stream_open(FlacStream *stream, FlacSource *source,
                            uint64_t audio_bytes, int *channels,
                            int *sample_rate, uint64_t *total_frames,
                            char *error, size_t error_size) {
    memset(stream, 0, sizeof(*stream));
    stream->source = source;
    /* Reject from STREAMINFO before dr_flac sizes its sample buffers. */
    const AudioFlacInfo *info = &source->probe.flac;
    unsigned int factor = audio_decimation_factor(info->sample_rate);
    if ((info->channels != 1 && info->channels != 2) || factor == 0) {
        set_error(error, error_size, "不支持的 FLAC 格式（%d 声道，%d Hz）",
                  (int)info->channels, (int)info->sample_rate);
        flac_stream_close(stream);
        return -1;
    }
    stream->decoder = drflac_open(flac_source_read, flac_source_seek,
                                  flac_source_tell, source, NULL);
    if (!stream->decoder) {
        flac_stream_close(stream);
        set_error(error, error_size, "FLAC 打开失败");
        return -1;
    }
    /* Seeks use CRC-checked binary search.  If that fails on a damaged file,
     * report the failure instead of decoding from the first frame on the
     * UI thread. */
    stream->decoder->_noBruteForceSeek = DRFLAC_TRUE;
    if (factor > 1) {
        stream->scratch = (int16_t *)malloc(
            (size_t)PLAYER_FLAC_CHUNK_FRAMES * info->channels *
            sizeof(int16_t));
        if (!stream->scratch) {
            flac_stream_close(stream);
            set_error(error, error_size, "内存不足，无法准备音频");
            return -1;
        }
    }
    stream->factor = factor;
    stream->read_bound = audio_flac_read_bound(
        info, (uint64_t)PLAYER_FRAMES_PER_BUFFER * factor,
        DR_FLAC_BUFFER_SIZE);
    uint64_t minimum = stream->read_bound > PLAYER_REBUFFER_BYTES ?
                       stream->read_bound : PLAYER_REBUFFER_BYTES;
    stream->rebuffer_bytes = media_rebuffer_bytes(
        audio_flac_bytes_per_second(info, audio_bytes),
        PLAYER_FLAC_REBUFFER_SECONDS, minimum);
    stream->seekable = info->total_frames > 0;
    *channels = (int)info->channels;
    *sample_rate = (int)(info->sample_rate / factor);
    *total_frames = info->total_frames / factor;
    return 0;
}

static size_t flac_stream_read(FlacStream *stream, int16_t *output,
                               size_t frames, int channels) {
    if (stream->factor <= 1)
        return (size_t)drflac_read_pcm_frames_s16(stream->decoder, frames,
                                                  output);
    size_t produced = 0;
    while (produced < frames) {
        size_t wanted = (frames - produced) * stream->factor;
        if (wanted > PLAYER_FLAC_CHUNK_FRAMES)
            wanted = PLAYER_FLAC_CHUNK_FRAMES;
        size_t decoded = (size_t)drflac_read_pcm_frames_s16(
            stream->decoder, wanted, stream->scratch);
        produced += audio_decimate_s16(
            stream->scratch, decoded, (unsigned int)channels,
            stream->factor, output + produced * (size_t)channels);
        if (decoded < wanted) break;
    }
    return produced;
}

static int flac_stream_seek(FlacStream *stream, uint64_t frame) {
    if (!stream->decoder || !stream->seekable) return -1;
    uint64_t source_frame = frame > UINT64_MAX / stream->factor ?
                            UINT64_MAX : frame * stream->factor;
    return drflac_seek_to_pcm_frame(stream->decoder, source_frame) ? 0 : -1;
}

static bool player_has_decoder(const Player *player) {
    return player->kind == PLAYER_DECODER_FLAC ? player->flac.decoder != NULL :
                                                 player->decoder_open;
}

/* Returns the progressive file still feeding the decoder, if any, and the
 * file offset of its next read. */
static ProgressiveFile *stream_source(const Player *player,
                                      uint64_t *position) {
    if (player->kind == PLAYER_DECODER_FLAC) {
        FlacSource *source = player->flac.source;
        if (!source || !source->progressive) return NULL;
        *position = flac_source_real_position(source);
        return source->progressive;
    }
    if (!player->stream_cursor) return NULL;
    *position = player->stream_cursor->position;
    return player->stream_cursor->source;
}

static void player_close_decoder(Player *player) {
    if (player->decoder_open) {
        mp3dec_ex_close(&player->decoder);
        player->decoder_open = false;
    }
    if (player->file) {
        fclose(player->file);
        player->file = NULL;
    }
    progressive_cursor_destroy(player->stream_cursor);
    player->stream_cursor = NULL;
    flac_stream_close(&player->flac);
    player->kind = PLAYER_DECODER_NONE;
}

static int player_decoder_seek(Player *player, uint64_t frame) {
    if (player->kind == PLAYER_DECODER_FLAC)
        return flac_stream_seek(&player->flac, frame);
    uint64_t sample = frame * (uint64_t)player->channels;
    return mp3dec_ex_seek(&player->decoder, sample) != 0 ? -1 : 0;
}

/* Makes every byte the next FLAC buffer decode may touch resident in the
 * prefetch window.  Returns false, with stream_waiting set, when decoding
 * has to wait for the network or for a window load. */
static bool flac_stream_ready(Player *player) {
    FlacSource *source = player->flac.source;
    if (!source || !source->progressive) return true;
    ProgressiveSnapshot snapshot;
    progressive_file_snapshot(source->progressive, &snapshot);
    uint64_t real = flac_source_real_position(source);
    uint64_t available = snapshot.published > real ?
                         snapshot.published - real : 0;
    uint64_t needed = player->flac.read_bound;
    if (available < needed) {
        if (!snapshot.complete && !snapshot.failed) {
            player->stream_waiting = true;
            player->stream_starved = true;
            return false;
        }
        needed = available;
    }
    size_t resident = needed ?
        progressive_file_window(source->progressive, real, (size_t)needed) : 0;
    if (resident < needed) {
        /* The bytes are on the SD card; only the window has to move. */
        player->stream_waiting = true;
        player->stream_starved = false;
        return false;
    }
    source->limit = real + resident;
    return true;
}

void player_prepared_destroy(PreparedAudio *prepared) {
    if (!prepared) return;
    if (prepared->decoder_open)
        mp3dec_ex_close(&prepared->decoder);
    if (prepared->file) fclose(prepared->file);
    flac_stream_close(&prepared->flac);
    free(prepared);
}

static int prepare_flac(PreparedAudio *prepared, const AudioProbe *probe,
                        uint64_t file_size, PreparedAudio **output,
                        PlayerCancelFn cancelled, void *cancel_userdata,
                        char *error, size_t error_size) {
    FlacSource *source = (FlacSource *)calloc(1, sizeof(*source));
    if (!source) {
        player_prepared_destroy(prepared);
        set_error(error, error_size, "内存不足，无法准备音频");
        return -1;
    }
    source->probe = *probe;
    source->file = prepared->file;
    source->file_position = UINT64_MAX;
    source->limit = file_size;
    source->cancelled = cancelled;
    source->cancel_userdata = cancel_userdata;
    prepared->file = NULL;
    prepared->kind = PLAYER_DECODER_FLAC;
    if (flac_stream_open(&prepared->flac, source,
                         file_size - probe->audio_offset,
                         &prepared->channels, &prepared->sample_rate,
                         &prepared->total_frames,
                         error, error_size) != 0) {
        player_prepared_destroy(prepared);
        return -1;
    }
    if (cancelled && cancelled(cancel_userdata)) {
        set_error(error, error_size, "FLAC 准备已取消");
        player_prepared_destroy(prepared);
        return -1;
    }
    source->cancelled = NULL;
    source->cancel_userdata = NULL;
    /* FLAC frames carry their own positions; no seek index is needed. */
    prepared->seek_ready = true;
    *output = prepared;
    return 0;
}

static int prepare_audio(const char *path, PreparedAudio **output,
                         bool build_seek_index,
                         PlayerCancelFn cancelled, void *cancel_userdata,
                         char *error, size_t error_size) {
    if (!path || !output) {
        set_error(error, error_size, "音频文件无效");
        return -1;
    }
    *output = NULL;
    PreparedAudio *prepared = (PreparedAudio *)calloc(1, sizeof(*prepared));
    if (!prepared) {
        set_error(error, error_size, "内存不足，无法准备音频");
        return -1;
    }
    prepared->file = fopen(path, "rb");
    if (!prepared->file) {
        player_prepared_destroy(prepared);
        set_error(error, error_size, "无法打开缓存的音频");
        return -1;
    }
    long file_end = -1;
    if (fseek(prepared->file, 0, SEEK_END) == 0)
        file_end = ftell(prepared->file);
    if (file_end > 0) {
        ProbeFile probe_source = {prepared->file, false};
        AudioProbe probe;
        uint64_t needed = 0;
        AudioProbeStatus status = audio_probe(
            probe_file_read, &probe_source, (uint64_t)file_end,
            &probe, &needed);
        if (status == AUDIO_PROBE_DONE && probe.format == AUDIO_FORMAT_FLAC)
            return prepare_flac(prepared, &probe, (uint64_t)file_end, output,
                                cancelled, cancel_userdata,
                                error, error_size);
        /* A complete file cut inside its FLAC header is still not MP3. */
        if (!probe_source.failed &&
            (status == AUDIO_PROBE_INVALID ||
             (status == AUDIO_PROBE_NEED_MORE &&
              probe.format == AUDIO_FORMAT_FLAC))) {
            player_prepared_destroy(prepared);
            set_error(error, error_size, "FLAC 文件头无效");
            return -1;
        }
    }
    if (fseek(prepared->file, 0, SEEK_SET) != 0) {
        player_prepared_destroy(prepared);
        set_error(error, error_size, "无法打开缓存的音频");
        return -1;
    }
    prepared->kind = PLAYER_DECODER_MP3;
    PrepareFile source = {prepared->file, cancelled, cancel_userdata};
    prepared->io.read = cancelled ? prepare_file_read : file_read;
    prepared->io.read_data = cancelled ? (void *)&source : prepared->file;
    prepared->io.seek = cancelled ? prepare_file_seek : file_seek;
    prepared->io.seek_data = cancelled ? (void *)&source : prepared->file;
    int flags = MP3D_SEEK_TO_SAMPLE;
    if (!build_seek_index) flags |= MP3D_DO_NOT_SCAN;
    int result = mp3dec_ex_open_cb(&prepared->decoder, &prepared->io, flags);
    if (result != 0) {
        mp3dec_ex_close(&prepared->decoder);
        /* With cancellation enabled, io.read_data points to the stack-local
         * PrepareFile wrapper rather than directly to the FILE. */
        fclose(prepared->file);
        prepared->file = NULL;
        free(prepared);
        set_error(error, error_size, "MP3 打开失败：%d", result);
        return -1;
    }
    prepared->decoder_open = true;
    prepared->channels = prepared->decoder.info.channels;
    prepared->sample_rate = prepared->decoder.info.hz;
    if ((prepared->channels != 1 && prepared->channels != 2) ||
        prepared->sample_rate <= 0) {
        set_error(error, error_size, "不支持的 MP3 格式（%d 声道，%d Hz）",
                  prepared->channels, prepared->sample_rate);
        player_prepared_destroy(prepared);
        return -1;
    }
    if (build_seek_index) {
        /* Xing/VBR headers let minimp3 determine the duration without scanning
         * the file.  A non-zero seek forces the complete index to be built on
         * this worker thread instead of the UI/audio thread. */
        uint64_t probe_sample = (uint64_t)prepared->channels;
        if (mp3dec_ex_seek(&prepared->decoder, probe_sample) != 0 ||
            mp3dec_ex_seek(&prepared->decoder, 0) != 0) {
            set_error(error, error_size, "MP3 跳转索引准备失败");
            player_prepared_destroy(prepared);
            return -1;
        }
    }
    if (cancelled && cancelled(cancel_userdata)) {
        set_error(error, error_size, "MP3 准备已取消");
        player_prepared_destroy(prepared);
        return -1;
    }
    prepared->io.read = file_read;
    prepared->io.read_data = prepared->file;
    prepared->io.seek = file_seek;
    prepared->io.seek_data = prepared->file;
    prepared->decoder.io = &prepared->io;
    prepared->total_frames = prepared->decoder.samples /
                             (uint64_t)prepared->channels;
    prepared->seek_ready = build_seek_index;
    *output = prepared;
    return 0;
}

int player_prepare_audio(const char *path, PreparedAudio **output,
                         char *error, size_t error_size) {
    return prepare_audio(path, output, true, NULL, NULL,
                         error, error_size);
}

int player_prepare_audio_controlled(const char *path,
                                    PreparedAudio **output,
                                    PlayerCancelFn cancelled,
                                    void *cancel_userdata,
                                    char *error, size_t error_size) {
    return prepare_audio(path, output, true, cancelled, cancel_userdata,
                         error, error_size);
}

int player_prepare_audio_fast(const char *path, PreparedAudio **output,
                              char *error, size_t error_size) {
    return prepare_audio(path, output, false, NULL, NULL,
                         error, error_size);
}

Player *player_create(char *error, size_t error_size) {
    Player *player = (Player *)calloc(1, sizeof(Player));
    if (!player) {
        set_error(error, error_size, "内存不足");
        return NULL;
    }
    Result result = ndspInit();
    if (R_FAILED(result)) {
        player->ndsp_result = result;
        set_error(error, error_size,
                  "NDSP 不可用（%08lX），请先提取 DSP 固件",
                  (unsigned long)result);
        return player;
    }
    player->ndsp_ready = true;
    player->volume = 1.0f;
    size_t samples = (size_t)PLAYER_BUFFERS * PLAYER_FRAMES_PER_BUFFER * 2;
    player->pcm = (int16_t *)linearAlloc(samples * sizeof(int16_t));
    if (!player->pcm) {
        ndspExit();
        free(player);
        set_error(error, error_size, "无法分配音频缓冲区");
        return NULL;
    }
    memset(player->pcm, 0, samples * sizeof(int16_t));
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspSetMasterVol(player->volume);
    return player;
}

static int fill_buffer(Player *player, int index) {
    if (player->eof) return 0;
    if (player->kind == PLAYER_DECODER_FLAC && !flac_stream_ready(player))
        return 0;
    int16_t *output = player->pcm +
        (size_t)index * PLAYER_FRAMES_PER_BUFFER * 2;
    size_t frames;
    if (player->kind == PLAYER_DECODER_FLAC) {
        frames = flac_stream_read(&player->flac, output,
                                  PLAYER_FRAMES_PER_BUFFER, player->channels);
    } else {
        size_t wanted = (size_t)PLAYER_FRAMES_PER_BUFFER * player->channels;
        frames = mp3dec_ex_read(&player->decoder, output, wanted) /
                 (size_t)player->channels;
    }
    if (frames == 0) {
        uint64_t position = 0;
        ProgressiveFile *source = stream_source(player, &position);
        if (source) {
            ProgressiveSnapshot snapshot;
            progressive_file_snapshot(source, &snapshot);
            if (snapshot.io_pending ||
                (!snapshot.complete && !snapshot.failed)) {
                player->stream_waiting = true;
                player->stream_starved = true;
                return 0;
            }
        }
        player->eof = true;
        return 0;
    }
    player->stream_waiting = false;
    size_t samples = frames * (size_t)player->channels;
    ndspWaveBuf *wave = &player->waves[index];
    memset(wave, 0, sizeof(*wave));
    wave->data_pcm16 = output;
    wave->nsamples = (u32)frames;
    wave->looping = false;
    DSP_FlushDataCache(output, samples * sizeof(int16_t));
    ndspChnWaveBufAdd(0, wave);
    player->queued[index] = true;
    return 1;
}

void player_stop(Player *player) {
    if (!player) return;
    if (player->ndsp_ready) {
        ndspChnSetPaused(0, false);
        ndspChnWaveBufClear(0);
    }
    player_close_decoder(player);
    memset(player->waves, 0, sizeof(player->waves));
    memset(player->queued, 0, sizeof(player->queued));
    player->active = false;
    player->paused = false;
    player->buffering = false;
    player->stream_waiting = false;
    player->stream_starved = false;
    player->streaming = false;
    player->indexing = false;
    player->eof = false;
    player->finished_latch = false;
    player->played_frames = 0;
    player->total_frames = 0;
    player->channels = 0;
    player->sample_rate = 0;
}

static void configure_channel(Player *player) {
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, (float)player->sample_rate);
    ndspChnSetFormat(0, player->channels == 2 ?
                     NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {1.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ndspChnSetMix(0, mix);
}

static int start_player(Player *player, double seconds, bool paused,
                        char *error, size_t error_size) {
    configure_channel(player);
    if (seconds < 0.0) seconds = 0.0;
    double duration = player_duration(player);
    if (duration > 0.0 && seconds > duration) seconds = duration;
    if (seconds > 0.0) {
        uint64_t frame = (uint64_t)(seconds * player->sample_rate);
        if (player_decoder_seek(player, frame) != 0) {
            set_error(error, error_size,
                      player->kind == PLAYER_DECODER_FLAC ?
                      "切换音频源时 FLAC 跳转失败" :
                      "切换音频源时 MP3 跳转失败");
            player_stop(player);
            return -1;
        }
        player->played_frames = frame;
    }
    player->active = true;
    player->paused = paused;
    for (int i = 0; i < PLAYER_BUFFERS; i++) (void)fill_buffer(player, i);
    bool any = false;
    for (int i = 0; i < PLAYER_BUFFERS; i++) any = any || player->queued[i];
    if (!any) {
        if (player->stream_waiting) {
            player->buffering = true;
            ndspChnSetPaused(0, true);
            return 0;
        }
        set_error(error, error_size,
                  player->kind == PLAYER_DECODER_FLAC ?
                  "FLAC 中没有可解码的音频" : "MP3 中没有可解码的音频");
        player_stop(player);
        return -1;
    }
    ndspChnSetPaused(0, paused);
    return 0;
}

/* Moves the decoder owned by `prepared` into `player`, which must already
 * have released its own decoder, and frees the empty wrapper. */
static void install_prepared(Player *player, PreparedAudio *prepared) {
    player->kind = prepared->kind;
    if (prepared->kind == PLAYER_DECODER_FLAC) {
        player->flac = prepared->flac;
        memset(&prepared->flac, 0, sizeof(prepared->flac));
    } else {
        player->decoder = prepared->decoder;
        player->io = prepared->io;
        player->file = prepared->file;
        player->io.read_data = player->file;
        player->io.seek_data = player->file;
        player->decoder.io = &player->io;
        player->decoder_open = true;
        prepared->decoder_open = false;
        prepared->file = NULL;
    }
    player->channels = prepared->channels;
    player->sample_rate = prepared->sample_rate;
    player->total_frames = prepared->total_frames;
    free(prepared);
}

static int adopt_prepared(Player *player, PreparedAudio *prepared,
                          double seconds, bool paused,
                          char *error, size_t error_size) {
    if (!player || !prepared) {
        player_prepared_destroy(prepared);
        set_error(error, error_size, "准备好的音频无效");
        return -1;
    }
    if (!player->ndsp_ready) {
        player_prepared_destroy(prepared);
        set_error(error, error_size,
                  "NDSP 不可用（%08lX），音频已缓存但无法播放",
                  (unsigned long)player->ndsp_result);
        return -1;
    }
    player_stop(player);
    player->indexing = !prepared->seek_ready;
    install_prepared(player, prepared);
    return start_player(player, seconds, paused, error, error_size);
}

int player_open_prepared(Player *player, PreparedAudio *prepared,
                         char *error, size_t error_size) {
    return adopt_prepared(player, prepared, 0.0, false,
                          error, error_size);
}

int player_replace_prepared(Player *player, PreparedAudio *prepared,
                            double seconds, char *error, size_t error_size) {
    if (player && prepared && player->active &&
        (player->streaming || player->indexing)) {
        if (!player->ndsp_ready) {
            player_prepared_destroy(prepared);
            set_error(error, error_size,
                      "NDSP 不可用（%08lX），音频已缓存但无法播放",
                      (unsigned long)player->ndsp_result);
            return -1;
        }
        if (prepared->kind != player->kind ||
            prepared->channels != player->channels ||
            prepared->sample_rate != player->sample_rate) {
            player_prepared_destroy(prepared);
            set_error(error, error_size,
                      "切换音频源时音频格式发生变化");
            return -1;
        }
        if (prepared->kind == PLAYER_DECODER_FLAC) {
            FlacSource *live = player->flac.source;
            FlacSource *complete = prepared->flac.source;
            if (!live || !live->progressive || !complete || !complete->file ||
                live->probe.audio_offset != complete->probe.audio_offset ||
                memcmp(live->probe.flac_header, complete->probe.flac_header,
                       AUDIO_FLAC_HEADER_SIZE) != 0) {
                player_prepared_destroy(prepared);
                set_error(error, error_size,
                          "切换音频源时音频格式发生变化");
                return -1;
            }
            /* The committed file has the same virtual stream layout as the
             * growing one, so the live decoder simply continues from the
             * complete file: no seek on this thread and no audible gap. */
            live->file = complete->file;
            live->file_position = UINT64_MAX;
            live->limit = complete->limit;
            complete->file = NULL;
            progressive_file_release(live->progressive);
            live->progressive = NULL;
            player_prepared_destroy(prepared);
            player->stream_waiting = false;
            player->stream_starved = false;
            player->streaming = false;
            player->indexing = false;
            player->buffering = false;
            ndspChnSetPaused(0, player->paused);
            return 0;
        }

        /* The decoder is ahead of the audible position by every PCM buffer
         * already queued in NDSP.  Seek the complete decoder to the end of
         * that queue, then swap only decoder ownership.  Keeping those wave
         * buffers queued avoids the audible gap caused by clearing and
         * rebuilding the channel when a complete seek index takes over. */
        uint64_t resume_frame = player->played_frames;
        for (int i = 0; i < PLAYER_BUFFERS; i++) {
            if (!player->queued[i]) continue;
            uint64_t frames = player->waves[i].nsamples;
            resume_frame = UINT64_MAX - resume_frame < frames ?
                           UINT64_MAX : resume_frame + frames;
        }
        if (prepared->total_frames > 0 &&
            resume_frame > prepared->total_frames)
            resume_frame = prepared->total_frames;
        uint64_t resume_sample =
            resume_frame > UINT64_MAX / (uint64_t)prepared->channels ?
            UINT64_MAX : resume_frame * (uint64_t)prepared->channels;
        if (mp3dec_ex_seek(&prepared->decoder, resume_sample) != 0) {
            player_prepared_destroy(prepared);
            set_error(error, error_size, "切换音频源时 MP3 跳转失败");
            return -1;
        }

        player_close_decoder(player);
        install_prepared(player, prepared);
        player->stream_waiting = false;
        player->stream_starved = false;
        player->streaming = false;
        player->indexing = false;
        player->buffering = false;
        player->eof = player->total_frames > 0 &&
                      resume_frame >= player->total_frames;
        ndspChnSetPaused(0, player->paused);
        return 0;
    }
    bool paused = player_is_paused(player);
    return adopt_prepared(player, prepared, seconds, paused,
                          error, error_size);
}

static int open_flac_stream(Player *player, ProgressiveFile *source,
                            const AudioProbe *probe,
                            char *error, size_t error_size) {
    FlacSource *flac_source = (FlacSource *)calloc(1, sizeof(*flac_source));
    if (!flac_source) {
        set_error(error, error_size, "内存不足，无法打开媒体流");
        return -1;
    }
    progressive_file_retain(source);
    flac_source->progressive = source;
    flac_source->probe = *probe;
    /* Only the synthesized header is readable until the gate admits frames. */
    flac_source->limit = probe->audio_offset;
    ProgressiveSnapshot snapshot;
    progressive_file_snapshot(source, &snapshot);
    uint64_t audio_bytes = snapshot.total > probe->audio_offset ?
                           snapshot.total - probe->audio_offset : 0;
    FlacStream stream;
    int channels = 0;
    int sample_rate = 0;
    uint64_t total_frames = 0;
    if (flac_stream_open(&stream, flac_source, audio_bytes, &channels,
                         &sample_rate, &total_frames,
                         error, error_size) != 0)
        return -1;
    if (stream.read_bound > PROGRESSIVE_WINDOW_SIZE) {
        /* Blocks this large cannot be decoded from one prefetch window. */
        flac_stream_close(&stream);
        set_error(error, error_size, "FLAC 需完整下载后播放");
        return PLAYER_STREAM_DEFERRED;
    }

    player_stop(player);
    player->kind = PLAYER_DECODER_FLAC;
    player->flac = stream;
    player->streaming = true;
    player->channels = channels;
    player->sample_rate = sample_rate;
    player->total_frames = total_frames;
    return start_player(player, 0.0, false, error, error_size);
}

int player_open_stream(Player *player, ProgressiveFile *source,
                       char *error, size_t error_size) {
    if (!player || !source) {
        set_error(error, error_size, "渐进式音频流无效");
        return -1;
    }
    if (!player->ndsp_ready) {
        set_error(error, error_size,
                  "NDSP 不可用（%08lX），无法播放音频",
                  (unsigned long)player->ndsp_result);
        return -1;
    }
    AudioProbe probe;
    ProgressiveProbeState probe_state = progressive_file_probe(source, &probe);
    if (probe_state == PROGRESSIVE_PROBE_INVALID) {
        set_error(error, error_size, "FLAC 文件头无效");
        return -1;
    }
    if (probe_state == PROGRESSIVE_PROBE_READY &&
        probe.format == AUDIO_FORMAT_FLAC)
        return open_flac_stream(player, source, &probe, error, error_size);

    PreparedStream *prepared = (PreparedStream *)calloc(1, sizeof(*prepared));
    ProgressiveCursor *cursor = (ProgressiveCursor *)calloc(1, sizeof(*cursor));
    if (!prepared || !cursor) {
        free(prepared);
        free(cursor);
        set_error(error, error_size, "内存不足，无法打开媒体流");
        return -1;
    }
    progressive_file_retain(source);
    cursor->source = source;
    prepared->cursor = cursor;
    prepared->io.read = progressive_read;
    prepared->io.read_data = cursor;
    prepared->io.seek = progressive_seek;
    prepared->io.seek_data = cursor;
    int result = mp3dec_ex_open_cb(&prepared->decoder, &prepared->io,
                                   MP3D_SEEK_TO_SAMPLE | MP3D_DO_NOT_SCAN);
    if (result != 0) {
        mp3dec_ex_close(&prepared->decoder);
        progressive_cursor_destroy(cursor);
        free(prepared);
        set_error(error, error_size, "流式 MP3 打开失败：%d", result);
        return -1;
    }
    prepared->decoder_open = true;
    prepared->channels = prepared->decoder.info.channels;
    prepared->sample_rate = prepared->decoder.info.hz;
    if ((prepared->channels != 1 && prepared->channels != 2) ||
        prepared->sample_rate <= 0) {
        mp3dec_ex_close(&prepared->decoder);
        progressive_cursor_destroy(cursor);
        free(prepared);
        set_error(error, error_size, "不支持的流式 MP3 格式");
        return -1;
    }
    prepared->total_frames = prepared->decoder.samples /
                             (uint64_t)prepared->channels;

    player_stop(player);
    player->kind = PLAYER_DECODER_MP3;
    player->decoder = prepared->decoder;
    player->io = prepared->io;
    player->stream_cursor = cursor;
    player->io.read_data = cursor;
    player->io.seek_data = cursor;
    player->decoder.io = &player->io;
    player->decoder_open = true;
    player->streaming = true;
    player->channels = prepared->channels;
    player->sample_rate = prepared->sample_rate;
    player->total_frames = prepared->total_frames;
    prepared->decoder_open = false;
    prepared->cursor = NULL;
    free(prepared);
    return start_player(player, 0.0, false, error, error_size);
}

int player_open(Player *player, const char *path,
                char *error, size_t error_size) {
    PreparedAudio *prepared = NULL;
    if (player_prepare_audio(path, &prepared, error, error_size) != 0)
        return -1;
    return player_open_prepared(player, prepared, error, error_size);
}

void player_update(Player *player) {
    if (!player || !player->active || player->paused) return;
    for (int i = 0; i < PLAYER_BUFFERS; i++) {
        if (player->queued[i] &&
            player->waves[i].status == NDSP_WBUF_DONE) {
            player->played_frames += player->waves[i].nsamples;
            player->queued[i] = false;
        }
    }
    uint64_t position = 0;
    ProgressiveFile *source = player->stream_waiting ?
                              stream_source(player, &position) : NULL;
    if (source) {
        ProgressiveSnapshot snapshot;
        progressive_file_snapshot(source, &snapshot);
        uint64_t available = snapshot.published > position ?
                             snapshot.published - position : 0;
        uint64_t threshold = player->kind == PLAYER_DECODER_FLAC ?
                             player->flac.rebuffer_bytes :
                             PLAYER_REBUFFER_BYTES;
        if (media_stream_can_resume(snapshot.io_pending,
                                    snapshot.complete || snapshot.failed,
                                    player->stream_starved,
                                    available, threshold))
            player->stream_waiting = false;
    }
    if (!player->stream_waiting && !player->eof) {
        for (int i = 0; i < PLAYER_BUFFERS; i++)
            if (!player->queued[i]) (void)fill_buffer(player, i);
    }
    bool any = false;
    for (int i = 0; i < PLAYER_BUFFERS; i++) any = any || player->queued[i];
    if (any) {
        if (player->buffering) {
            player->buffering = false;
            ndspChnSetPaused(0, false);
        }
        return;
    }
    if (player->stream_waiting) {
        player->buffering = true;
        ndspChnSetPaused(0, true);
        return;
    }
    if (!any && player->eof) {
        ndspChnWaveBufClear(0);
        player_close_decoder(player);
        player->streaming = false;
        player->buffering = false;
        player->active = false;
        player->finished_latch = true;
    }
}

void player_toggle_pause(Player *player) {
    if (!player || !player->active) return;
    player_set_paused(player, !player->paused);
}

void player_set_paused(Player *player, bool paused) {
    if (!player || !player->active) return;
    player->paused = paused;
    ndspChnSetPaused(0, paused || player->buffering);
}

int player_seek(Player *player, double seconds,
                char *error, size_t error_size) {
    if (!player || !player->active || !player_has_decoder(player) ||
        player->sample_rate <= 0 || player->channels <= 0) {
        set_error(error, error_size, "没有可跳转的播放中歌曲");
        return -1;
    }
    if (player->streaming) {
        set_error(error, error_size,
                  "下载完成后才能跳转");
        return -1;
    }
    if (player->kind == PLAYER_DECODER_FLAC && !player->flac.seekable) {
        set_error(error, error_size, "FLAC 时长未知，无法跳转");
        return -1;
    }
    if (seconds < 0.0) seconds = 0.0;
    double duration = player_duration(player);
    if (duration > 0.0 && seconds > duration) seconds = duration;
    uint64_t frame = (uint64_t)(seconds * player->sample_rate);
    bool paused = player->paused;
    ndspChnSetPaused(0, true);
    ndspChnWaveBufClear(0);
    memset(player->waves, 0, sizeof(player->waves));
    memset(player->queued, 0, sizeof(player->queued));
    if (player_decoder_seek(player, frame) != 0) {
        ndspChnSetPaused(0, paused);
        set_error(error, error_size,
                  player->kind == PLAYER_DECODER_FLAC ?
                  "FLAC 跳转失败" : "MP3 跳转失败");
        return -1;
    }
    player->played_frames = frame;
    player->eof = false;
    player->finished_latch = false;
    for (int i = 0; i < PLAYER_BUFFERS; i++) (void)fill_buffer(player, i);
    bool any = false;
    for (int i = 0; i < PLAYER_BUFFERS; i++) any = any || player->queued[i];
    if (!any) {
        player->eof = true;
        set_error(error, error_size, "跳转位置已到歌曲末尾");
        ndspChnSetPaused(0, paused);
        return -1;
    }
    player->paused = paused;
    ndspChnSetPaused(0, paused);
    return 0;
}

void player_set_volume(Player *player, float volume) {
    if (!player) return;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    player->volume = volume;
    if (player->ndsp_ready) ndspSetMasterVol(volume);
}

float player_volume(const Player *player) {
    return player ? player->volume : 0.0f;
}

bool player_is_active(const Player *player) {
    return player && player->active;
}

bool player_is_available(const Player *player) {
    return player && player->ndsp_ready;
}

uint32_t player_ndsp_result(const Player *player) {
    return player ? (uint32_t)player->ndsp_result : 0;
}

bool player_is_paused(const Player *player) {
    return player && player->active && player->paused;
}

bool player_is_buffering(const Player *player) {
    return player && player->active && player->buffering;
}

bool player_is_streaming(const Player *player) {
    return player && player->active && player->streaming;
}

bool player_is_indexing(const Player *player) {
    return player && player->active && player->indexing;
}

bool player_can_seek(const Player *player) {
    return player && player->active &&
           !player->streaming && !player->indexing;
}

bool player_finished(Player *player) {
    if (!player || !player->finished_latch) return false;
    player->finished_latch = false;
    return true;
}

double player_position(const Player *player) {
    if (!player || player->sample_rate <= 0) return 0.0;
    uint64_t frames = player->played_frames;
    bool live_queue = false;
    u32 current = 0U;
    if (player->ndsp_ready && player->active && !player->buffering) {
        /* Snapshot the DSP sequence around the queue state. A retry closes
         * the normal boundary race where the current buffer changes while
         * this function is accounting completed buffers. */
        for (int attempt = 0; attempt < 2; attempt++) {
            u16 sequence_before = ndspChnGetWaveBufSeq(0);
            frames = player->played_frames;
            live_queue = false;
            for (int i = 0; i < PLAYER_BUFFERS; i++) {
                if (!player->queued[i]) continue;
                if (player->waves[i].status == NDSP_WBUF_DONE) {
                    /* NDSP may finish a wave after player_update() but before
                     * drawing. Count it now so samplePos resetting to zero
                     * cannot make the displayed time move backward. */
                    uint64_t completed = player->waves[i].nsamples;
                    frames = UINT64_MAX - frames < completed ?
                             UINT64_MAX : frames + completed;
                } else live_queue = true;
            }
            current = live_queue ? ndspChnGetSamplePos(0) : 0U;
            if (sequence_before == ndspChnGetWaveBufSeq(0)) break;
        }
    }
    if (current <= PLAYER_FRAMES_PER_BUFFER) {
        uint64_t sample = current;
        frames = UINT64_MAX - frames < sample ?
                 UINT64_MAX : frames + sample;
    }
    double value = (double)frames / player->sample_rate;
    double duration = player_duration(player);
    return duration > 0.0 && value > duration ? duration : value;
}

double player_duration(const Player *player) {
    if (!player || player->sample_rate <= 0) return 0.0;
    return (double)player->total_frames / player->sample_rate;
}

void player_destroy(Player *player) {
    if (!player) return;
    player_stop(player);
    if (player->pcm) linearFree(player->pcm);
    if (player->ndsp_ready) ndspExit();
    free(player);
}
