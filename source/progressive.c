#include "progressive.h"

#include "i18n.h"

#include <3ds.h>

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGRESSIVE_CACHE_SIZE (128U * 1024U)
#define PROGRESSIVE_IO_STACK_SIZE (32U * 1024U)
#define PROGRESSIVE_IO_PRIORITY 0x32

struct ProgressiveFile {
    LightLock lock;
    LightLock disk_lock;
    LightEvent io_wake;
    Thread io_thread;
    unsigned int references;
    char final_path[256];
    char part_path[272];
    uint64_t published;
    uint64_t total;
    bool complete;
    bool failed;
    bool cancelled;
    bool io_stop;
    bool io_loading;
    bool io_pending;
    uint64_t io_position;
    size_t io_size;
    uint8_t *cache;
    uint64_t cache_position;
    size_t cache_size;
    char error[192];
};

static void progressive_io_main(void *userdata) {
    ProgressiveFile *file = (ProgressiveFile *)userdata;
    for (;;) {
        LightEvent_Wait(&file->io_wake);
        LightLock_Lock(&file->lock);
        if (file->io_stop) {
            LightLock_Unlock(&file->lock);
            break;
        }
        if (!file->io_pending || file->io_loading ||
            file->io_position >= file->published) {
            LightLock_Unlock(&file->lock);
            continue;
        }
        uint64_t position = file->io_position;
        size_t size = file->io_size;
        uint64_t available = file->published - position;
        if (size > PROGRESSIVE_CACHE_SIZE) size = PROGRESSIVE_CACHE_SIZE;
        if (available < size) size = (size_t)available;
        bool complete = file->complete;
        file->io_loading = true;
        LightLock_Unlock(&file->lock);

        size_t read = 0;
        if (position <= (uint64_t)LONG_MAX) {
            LightLock_Lock(&file->disk_lock);
            const char *primary = complete ? file->final_path : file->part_path;
            const char *fallback = complete ? file->part_path : file->final_path;
            FILE *input = fopen(primary, "rb");
            if (!input) input = fopen(fallback, "rb");
            if (input) {
                if (fseek(input, (long)position, SEEK_SET) == 0)
                    read = fread(file->cache, 1, size, input);
                fclose(input);
            }
            LightLock_Unlock(&file->disk_lock);
        }

        LightLock_Lock(&file->lock);
        if (file->io_position == position) {
            file->cache_position = position;
            file->cache_size = read;
            file->io_pending = false;
        }
        file->io_loading = false;
        bool retry = file->io_pending && file->io_position < file->published;
        LightLock_Unlock(&file->lock);
        if (retry) LightEvent_Signal(&file->io_wake);
    }
    threadExit(0);
}

static void set_error(char *error, size_t size, const char *format, ...) {
    if (!error || size == 0) return;
    va_list args;
    va_start(args, format);
    i18n_vsnprintf(error, size, format, args);
    va_end(args);
}

ProgressiveFile *progressive_file_create(const char *final_path,
                                         char *error, size_t error_size) {
    if (!final_path || !final_path[0]) {
        set_error(error, error_size, "渐进式文件路径无效");
        return NULL;
    }
    ProgressiveFile *file = (ProgressiveFile *)calloc(1, sizeof(*file));
    if (!file) {
        set_error(error, error_size, "内存不足，无法创建媒体流");
        return NULL;
    }
    int final_written = snprintf(file->final_path, sizeof(file->final_path),
                                 "%s", final_path);
    int part_written = snprintf(file->part_path, sizeof(file->part_path),
                                "%s.part", final_path);
    if (final_written < 0 || (size_t)final_written >= sizeof(file->final_path) ||
        part_written < 0 || (size_t)part_written >= sizeof(file->part_path)) {
        free(file);
        set_error(error, error_size, "渐进式文件路径过长");
        return NULL;
    }
    LightLock_Init(&file->lock);
    LightLock_Init(&file->disk_lock);
    LightEvent_Init(&file->io_wake, RESET_ONESHOT);
    file->references = 1;
    file->cache = (uint8_t *)malloc(PROGRESSIVE_CACHE_SIZE);
    if (!file->cache) {
        free(file);
        set_error(error, error_size, "内存不足，无法创建媒体预取");
        return NULL;
    }
    file->io_thread = threadCreate(progressive_io_main, file,
                                   PROGRESSIVE_IO_STACK_SIZE,
                                   PROGRESSIVE_IO_PRIORITY, -2, false);
    if (!file->io_thread) {
        free(file->cache);
        free(file);
        set_error(error, error_size, "无法创建媒体 I/O 线程");
        return NULL;
    }
    return file;
}

void progressive_file_retain(ProgressiveFile *file) {
    if (!file) return;
    LightLock_Lock(&file->lock);
    file->references++;
    LightLock_Unlock(&file->lock);
}

void progressive_file_release(ProgressiveFile *file) {
    if (!file) return;
    bool destroy = false;
    LightLock_Lock(&file->lock);
    if (file->references > 0) file->references--;
    destroy = file->references == 0;
    if (destroy) file->io_stop = true;
    LightLock_Unlock(&file->lock);
    if (destroy) {
        LightEvent_Signal(&file->io_wake);
        threadJoin(file->io_thread, U64_MAX);
        threadFree(file->io_thread);
        free(file->cache);
        free(file);
    }
}

void progressive_file_publish(ProgressiveFile *file,
                              uint64_t published, uint64_t total) {
    if (!file) return;
    LightLock_Lock(&file->lock);
    if (published > file->published) file->published = published;
    if (total) file->total = total;
    bool signal = file->io_pending && file->io_position < file->published;
    if (!file->io_pending && !file->io_loading &&
        file->published >= PROGRESSIVE_CACHE_SIZE &&
        (file->cache_position != 0 ||
         file->cache_size < PROGRESSIVE_CACHE_SIZE)) {
        file->io_position = 0;
        file->io_size = PROGRESSIVE_CACHE_SIZE;
        file->io_pending = true;
        signal = true;
    }
    LightLock_Unlock(&file->lock);
    if (signal) LightEvent_Signal(&file->io_wake);
}

int progressive_file_commit(ProgressiveFile *file,
                            char *error, size_t error_size) {
    if (!file) {
        set_error(error, error_size, "渐进式文件无效");
        return -1;
    }
    LightLock_Lock(&file->disk_lock);
    (void)remove(file->final_path);
    int result = rename(file->part_path, file->final_path);
    if (result != 0) (void)remove(file->part_path);
    LightLock_Unlock(&file->disk_lock);
    LightLock_Lock(&file->lock);
    bool signal = false;
    if (result == 0) {
        file->complete = true;
        file->failed = false;
        file->cancelled = false;
        file->error[0] = '\0';
        if (file->io_pending && file->io_position >= file->published)
            file->io_pending = false;
        signal = file->io_pending && file->io_position < file->published;
    } else {
        file->failed = true;
        snprintf(file->error, sizeof(file->error), "%s",
                 i18n_text("无法提交缓存文件"));
    }
    LightLock_Unlock(&file->lock);
    if (signal) LightEvent_Signal(&file->io_wake);
    if (result != 0) {
        set_error(error, error_size, "无法提交缓存文件");
        return -1;
    }
    return 0;
}

void progressive_file_fail(ProgressiveFile *file, bool cancelled,
                           const char *error) {
    if (!file) return;
    LightLock_Lock(&file->disk_lock);
    (void)remove(file->part_path);
    LightLock_Unlock(&file->disk_lock);
    LightLock_Lock(&file->lock);
    file->failed = true;
    file->cancelled = cancelled;
    if (!file->io_loading) file->io_pending = false;
    snprintf(file->error, sizeof(file->error), "%s",
             error && error[0] ? error :
             i18n_text(cancelled ?
                 "媒体下载已取消" : "媒体下载失败"));
    LightLock_Unlock(&file->lock);
}

size_t progressive_file_read_at(ProgressiveFile *file, uint64_t position,
                                void *buffer, size_t size) {
    if (!file || !buffer || size == 0) return 0;
    LightLock_Lock(&file->lock);
    if (!file->io_loading && position >= file->cache_position &&
        position - file->cache_position < file->cache_size) {
        size_t offset = (size_t)(position - file->cache_position);
        size_t available = file->cache_size - offset;
        if (size > available) size = available;
        memcpy(buffer, file->cache + offset, size);
        LightLock_Unlock(&file->lock);
        return size;
    }
    bool signal = false;
    if (!file->io_loading &&
        (!file->io_pending || file->io_position != position)) {
        file->io_position = position;
        file->io_size = size > PROGRESSIVE_CACHE_SIZE ?
                        PROGRESSIVE_CACHE_SIZE : size;
        file->io_pending = true;
        signal = position < file->published;
    }
    LightLock_Unlock(&file->lock);
    if (signal) LightEvent_Signal(&file->io_wake);
    return 0;
}

void progressive_file_snapshot(ProgressiveFile *file,
                               ProgressiveSnapshot *snapshot) {
    if (!snapshot) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (!file) return;
    LightLock_Lock(&file->lock);
    snapshot->published = file->published;
    snapshot->total = file->total;
    snapshot->complete = file->complete;
    snapshot->failed = file->failed;
    snapshot->cancelled = file->cancelled;
    snapshot->prefetch_ready = !file->io_loading &&
                               file->cache_position == 0 &&
                               file->cache_size >= PROGRESSIVE_CACHE_SIZE;
    snapshot->io_pending = file->io_loading || file->io_pending;
    snprintf(snapshot->error, sizeof(snapshot->error), "%s", file->error);
    LightLock_Unlock(&file->lock);
}
