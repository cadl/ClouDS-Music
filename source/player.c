#include "player.h"

#include "i18n.h"

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
    mp3dec_ex_t decoder;
    mp3dec_io_t io;
    ProgressiveCursor *cursor;
    bool decoder_open;
    int channels;
    int sample_rate;
    uint64_t total_frames;
} PreparedStream;

struct PreparedAudio {
    mp3dec_ex_t decoder;
    mp3dec_io_t io;
    FILE *file;
    bool decoder_open;
    int channels;
    int sample_rate;
    uint64_t total_frames;
    bool seek_ready;
};

struct Player {
    mp3dec_ex_t decoder;
    mp3dec_io_t io;
    FILE *file;
    ProgressiveCursor *stream_cursor;
    bool decoder_open;
    bool active;
    bool paused;
    bool buffering;
    bool stream_waiting;
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

void player_prepared_destroy(PreparedAudio *prepared) {
    if (!prepared) return;
    if (prepared->decoder_open)
        mp3dec_ex_close(&prepared->decoder);
    if (prepared->file) fclose(prepared->file);
    free(prepared);
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
        set_error(error, error_size, "内存不足，无法准备 MP3");
        return -1;
    }
    prepared->file = fopen(path, "rb");
    if (!prepared->file) {
        player_prepared_destroy(prepared);
        set_error(error, error_size, "无法打开缓存的 MP3");
        return -1;
    }
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
    int16_t *output = player->pcm +
        (size_t)index * PLAYER_FRAMES_PER_BUFFER * 2;
    size_t wanted = (size_t)PLAYER_FRAMES_PER_BUFFER * player->channels;
    size_t samples = mp3dec_ex_read(&player->decoder, output, wanted);
    if (samples == 0) {
        if (player->streaming && player->stream_cursor) {
            ProgressiveSnapshot snapshot;
            progressive_file_snapshot(player->stream_cursor->source,
                                       &snapshot);
            if (snapshot.io_pending ||
                (!snapshot.complete && !snapshot.failed)) {
                player->stream_waiting = true;
                return 0;
            }
        }
        player->eof = true;
        return 0;
    }
    player->stream_waiting = false;
    size_t frames = samples / (size_t)player->channels;
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
    memset(player->waves, 0, sizeof(player->waves));
    memset(player->queued, 0, sizeof(player->queued));
    player->active = false;
    player->paused = false;
    player->buffering = false;
    player->stream_waiting = false;
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
        uint64_t sample = frame * (uint64_t)player->channels;
        if (mp3dec_ex_seek(&player->decoder, sample) != 0) {
            set_error(error, error_size, "切换音频源时 MP3 跳转失败");
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
        set_error(error, error_size, "MP3 中没有可解码的音频");
        player_stop(player);
        return -1;
    }
    ndspChnSetPaused(0, paused);
    return 0;
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
    player->decoder = prepared->decoder;
    player->io = prepared->io;
    player->file = prepared->file;
    player->io.read_data = player->file;
    player->io.seek_data = player->file;
    player->decoder.io = &player->io;
    player->decoder_open = true;
    player->channels = prepared->channels;
    player->sample_rate = prepared->sample_rate;
    player->total_frames = prepared->total_frames;
    player->indexing = !prepared->seek_ready;
    prepared->decoder_open = false;
    prepared->file = NULL;
    free(prepared);
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
        if (prepared->channels != player->channels ||
            prepared->sample_rate != player->sample_rate) {
            player_prepared_destroy(prepared);
            set_error(error, error_size,
                      "切换音频源时 MP3 格式发生变化");
            return -1;
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

        if (player->decoder_open) mp3dec_ex_close(&player->decoder);
        if (player->file) fclose(player->file);
        progressive_cursor_destroy(player->stream_cursor);
        player->stream_cursor = NULL;
        player->decoder = prepared->decoder;
        player->io = prepared->io;
        player->file = prepared->file;
        player->io.read_data = player->file;
        player->io.seek_data = player->file;
        player->decoder.io = &player->io;
        player->decoder_open = true;
        player->channels = prepared->channels;
        player->sample_rate = prepared->sample_rate;
        player->total_frames = prepared->total_frames;
        player->stream_waiting = false;
        player->streaming = false;
        player->indexing = false;
        player->buffering = false;
        player->eof = player->total_frames > 0 &&
                      resume_frame >= player->total_frames;
        prepared->decoder_open = false;
        prepared->file = NULL;
        free(prepared);
        ndspChnSetPaused(0, player->paused);
        return 0;
    }
    bool paused = player_is_paused(player);
    return adopt_prepared(player, prepared, seconds, paused,
                          error, error_size);
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
    if (player->stream_waiting && player->stream_cursor) {
        ProgressiveSnapshot snapshot;
        progressive_file_snapshot(player->stream_cursor->source, &snapshot);
        uint64_t available = snapshot.published > player->stream_cursor->position ?
                             snapshot.published - player->stream_cursor->position : 0;
        if (!snapshot.io_pending &&
            (snapshot.complete || snapshot.failed ||
             available >= PLAYER_REBUFFER_BYTES))
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
    if (!player || !player->active || !player->decoder_open ||
        player->sample_rate <= 0 || player->channels <= 0) {
        set_error(error, error_size, "没有可跳转的播放中歌曲");
        return -1;
    }
    if (player->streaming) {
        set_error(error, error_size,
                  "下载完成后才能跳转");
        return -1;
    }
    if (seconds < 0.0) seconds = 0.0;
    double duration = player_duration(player);
    if (duration > 0.0 && seconds > duration) seconds = duration;
    uint64_t frame = (uint64_t)(seconds * player->sample_rate);
    uint64_t sample = frame * (uint64_t)player->channels;
    bool paused = player->paused;
    ndspChnSetPaused(0, true);
    ndspChnWaveBufClear(0);
    memset(player->waves, 0, sizeof(player->waves));
    memset(player->queued, 0, sizeof(player->queued));
    if (mp3dec_ex_seek(&player->decoder, sample) != 0) {
        ndspChnSetPaused(0, paused);
        set_error(error, error_size, "MP3 跳转失败");
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
