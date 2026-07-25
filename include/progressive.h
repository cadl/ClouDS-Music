#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ProgressiveFile ProgressiveFile;

typedef struct {
    uint64_t published;
    uint64_t total;
    bool complete;
    bool failed;
    bool cancelled;
    bool prefetch_ready;
    bool io_pending;
    char error[192];
} ProgressiveSnapshot;

ProgressiveFile *progressive_file_create(const char *final_path,
                                         char *error, size_t error_size);
void progressive_file_retain(ProgressiveFile *file);
void progressive_file_release(ProgressiveFile *file);
void progressive_file_publish(ProgressiveFile *file,
                              uint64_t published, uint64_t total);
int progressive_file_commit(ProgressiveFile *file,
                            char *error, size_t error_size);
void progressive_file_fail(ProgressiveFile *file, bool cancelled,
                           const char *error);
size_t progressive_file_read_at(ProgressiveFile *file, uint64_t position,
                                void *buffer, size_t size);
void progressive_file_snapshot(ProgressiveFile *file,
                               ProgressiveSnapshot *snapshot);
