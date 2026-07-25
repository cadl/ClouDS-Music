/*
 * Compact on-device pinyin lookup, adapted from Fishason/DSSH.
 * Original implementation copyright (c) 2026 exdekotive, MIT licensed.
 */
#include "ime_pinyin.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IME_GATHER_MAX 8192

typedef struct __attribute__((packed)) {
    char magic[4];
    uint32_t version;
    uint32_t entry_count;
    uint32_t pinyin_offset;
    uint32_t pinyin_size;
    uint32_t word_offset;
    uint32_t word_size;
} DictionaryHeader;

typedef struct __attribute__((packed)) {
    uint32_t pinyin_offset;
    uint32_t word_offset;
    uint32_t frequency;
} DictionaryEntry;

struct PinyinIme {
    uint8_t *data;
    size_t size;
    const DictionaryHeader *header;
    const DictionaryEntry *entries;
    const char *pinyin_pool;
    const char *word_pool;
    char buffer[IME_BUFFER_MAX + 1];
    int buffer_length;
    int matched_length;
    const DictionaryEntry *gathered[IME_GATHER_MAX];
    int gathered_count;
    const char *candidates[IME_MAX_CANDIDATES];
    int candidate_count;
};

static const char *entry_pinyin(const PinyinIme *ime, int index) {
    return ime->pinyin_pool + ime->entries[index].pinyin_offset;
}

static int compare_frequency(const void *left, const void *right) {
    const DictionaryEntry *a = *(const DictionaryEntry *const *)left;
    const DictionaryEntry *b = *(const DictionaryEntry *const *)right;
    if (a->frequency > b->frequency) return -1;
    if (a->frequency < b->frequency) return 1;
    return 0;
}

static int lower_bound(const PinyinIme *ime, const char *prefix) {
    int low = 0;
    int high = (int)ime->header->entry_count;
    while (low < high) {
        int middle = low + (high - low) / 2;
        if (strcmp(entry_pinyin(ime, middle), prefix) < 0)
            low = middle + 1;
        else high = middle;
    }
    return low;
}

static int gather(PinyinIme *ime, int length) {
    ime->gathered_count = 0;
    if (length <= 0) return 0;
    char prefix[IME_BUFFER_MAX + 1];
    memcpy(prefix, ime->buffer, (size_t)length);
    prefix[length] = '\0';
    int total = (int)ime->header->entry_count;
    for (int i = lower_bound(ime, prefix);
         i < total && ime->gathered_count < IME_GATHER_MAX; i++) {
        const char *pinyin = entry_pinyin(ime, i);
        if (strncmp(pinyin, prefix, (size_t)length) != 0) break;
        ime->gathered[ime->gathered_count++] = &ime->entries[i];
    }
    return ime->gathered_count;
}

static void refresh(PinyinIme *ime) {
    ime->gathered_count = 0;
    ime->candidate_count = 0;
    ime->matched_length = 0;
    for (int length = ime->buffer_length; length > 0; length--) {
        if (gather(ime, length) > 0) {
            ime->matched_length = length;
            break;
        }
    }
    if (ime->gathered_count == 0) return;
    int exact_count = 0;
    while (exact_count < ime->gathered_count) {
        const DictionaryEntry *entry = ime->gathered[exact_count];
        const char *pinyin = ime->pinyin_pool + entry->pinyin_offset;
        if (strlen(pinyin) != (size_t)ime->matched_length) break;
        exact_count++;
    }
    /* Entries are stored lexicographically by pinyin, so exact matches form
     * the leading span. Rank that span ahead of longer prefix completions,
     * while preserving frequency order within both groups. */
    if (exact_count > 1)
        qsort(ime->gathered, (size_t)exact_count,
              sizeof(ime->gathered[0]), compare_frequency);
    int completion_count = ime->gathered_count - exact_count;
    if (completion_count > 1)
        qsort(ime->gathered + exact_count, (size_t)completion_count,
              sizeof(ime->gathered[0]), compare_frequency);
    int take = ime->gathered_count < IME_MAX_CANDIDATES ?
               ime->gathered_count : IME_MAX_CANDIDATES;
    for (int i = 0; i < take; i++)
        ime->candidates[i] = ime->word_pool + ime->gathered[i]->word_offset;
    ime->candidate_count = take;
}

static int dictionary_valid(const PinyinIme *ime) {
    const DictionaryHeader *header = ime->header;
    if (memcmp(header->magic, "PYIN", 4) != 0 || header->version != 1)
        return 0;
    uint64_t entries_end = sizeof(*header) +
        (uint64_t)header->entry_count * sizeof(DictionaryEntry);
    uint64_t pinyin_end = (uint64_t)header->pinyin_offset + header->pinyin_size;
    uint64_t word_end = (uint64_t)header->word_offset + header->word_size;
    if (entries_end > ime->size || pinyin_end > ime->size ||
        word_end > ime->size || header->pinyin_offset < entries_end ||
        header->word_offset < pinyin_end) return 0;
    const DictionaryEntry *entries =
        (const DictionaryEntry *)(ime->data + sizeof(*header));
    for (uint32_t i = 0; i < header->entry_count; i++) {
        if (entries[i].pinyin_offset >= header->pinyin_size ||
            entries[i].word_offset >= header->word_size) return 0;
    }
    return 1;
}

PinyinIme *ime_create(const char *path) {
    if (!path) return NULL;
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    long length = ftell(file);
    if (length <= (long)sizeof(DictionaryHeader)) { fclose(file); return NULL; }
    rewind(file);
    PinyinIme *ime = (PinyinIme *)calloc(1, sizeof(*ime));
    if (!ime) { fclose(file); return NULL; }
    ime->data = (uint8_t *)malloc((size_t)length);
    if (!ime->data) { fclose(file); free(ime); return NULL; }
    if (fread(ime->data, 1, (size_t)length, file) != (size_t)length) {
        fclose(file); ime_destroy(ime); return NULL;
    }
    fclose(file);
    ime->size = (size_t)length;
    ime->header = (const DictionaryHeader *)ime->data;
    if (!dictionary_valid(ime)) { ime_destroy(ime); return NULL; }
    ime->entries =
        (const DictionaryEntry *)(ime->data + sizeof(DictionaryHeader));
    ime->pinyin_pool = (const char *)ime->data + ime->header->pinyin_offset;
    ime->word_pool = (const char *)ime->data + ime->header->word_offset;
    return ime;
}

void ime_destroy(PinyinIme *ime) {
    if (!ime) return;
    free(ime->data);
    free(ime);
}

void ime_input(PinyinIme *ime, char letter) {
    if (!ime || letter < 'a' || letter > 'z' ||
        ime->buffer_length >= IME_BUFFER_MAX) return;
    ime->buffer[ime->buffer_length++] = letter;
    ime->buffer[ime->buffer_length] = '\0';
    refresh(ime);
}

void ime_backspace(PinyinIme *ime) {
    if (!ime || ime->buffer_length == 0) return;
    ime->buffer[--ime->buffer_length] = '\0';
    refresh(ime);
}

void ime_clear(PinyinIme *ime) {
    if (!ime) return;
    ime->buffer[0] = '\0';
    ime->buffer_length = 0;
    refresh(ime);
}

const char *ime_buffer(const PinyinIme *ime) { return ime ? ime->buffer : ""; }
int ime_active(const PinyinIme *ime) { return ime && ime->buffer_length > 0; }
int ime_matched_length(const PinyinIme *ime) {
    return ime ? ime->matched_length : 0;
}

int ime_candidate_count(const PinyinIme *ime) {
    return ime ? ime->candidate_count : 0;
}

const char *ime_candidate(const PinyinIme *ime, int index) {
    if (!ime || index < 0 || index >= ime_candidate_count(ime)) return NULL;
    return ime->candidates[index];
}

const char *ime_commit(PinyinIme *ime, int index) {
    const char *value = ime_candidate(ime, index);
    if (value) ime_clear(ime);
    return value;
}
