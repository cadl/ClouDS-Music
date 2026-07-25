#include "unicode_text.h"
#include "song_text.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_hangul_detection(void) {
    assert(!utf8_contains_hangul("English 中文 日本語"));
    assert(utf8_contains_hangul("한국어"));
    assert(utf8_contains_hangul("\xE1\x84\x92\xE1\x85\xA1\xE1\x86\xAB"));
    assert(unicode_codepoint_is_hangul(0xAC00));
    assert(unicode_codepoint_is_hangul(0xD7A3));
    assert(!unicode_codepoint_is_hangul(0x30A2));
}

static void test_hangul_composition(void) {
    char leading_vowel[] = "\xE1\x84\x80\xE1\x85\xA1";
    assert(utf8_compose_hangul_nfc(leading_vowel) == 3);
    assert(strcmp(leading_vowel, "가") == 0);

    char leading_vowel_trailing[] =
        "A\xE1\x84\x92\xE1\x85\xA1\xE1\x86\xAB글";
    assert(utf8_compose_hangul_nfc(leading_vowel_trailing) == 7);
    assert(strcmp(leading_vowel_trailing, "A한글") == 0);

    char syllable_trailing[] = "가\xE1\x86\xA8";
    assert(utf8_compose_hangul_nfc(syllable_trailing) == 3);
    assert(strcmp(syllable_trailing, "각") == 0);
}

static void test_utf8_copy_truncated(void) {
    char output[8];
    assert(utf8_copy_truncated(output, sizeof(output), "가나다") == 6);
    assert(strcmp(output, "가나") == 0);
    assert(utf8_copy_truncated(output, 4, "가나") == 3);
    assert(strcmp(output, "가") == 0);

    const char malformed[] = {'A', (char)0xE3, 'B', '\0'};
    assert(utf8_copy_truncated(output, sizeof(output), malformed) == 3);
    assert(strcmp(output, "A?B") == 0);
}

static void test_song_metadata_hangul(void) {
    Song song = {0};
    snprintf(song.title, sizeof(song.title),
             "A\xE1\x84\x92\xE1\x85\xA1\xE1\x86\xAB글");
    snprintf(song.artist, sizeof(song.artist),
             "\xE1\x84\x80\xE1\x85\xA1수");
    snprintf(song.album, sizeof(song.album), "Album");
    assert(song_text_contains_hangul(&song));

    song_text_compose_hangul_nfc(&song);
    assert(strcmp(song.title, "A한글") == 0);
    assert(strcmp(song.artist, "가수") == 0);

    snprintf(song.title, sizeof(song.title), "Title");
    snprintf(song.artist, sizeof(song.artist), "Artist");
    snprintf(song.album, sizeof(song.album), "Album");
    assert(!song_text_contains_hangul(&song));

    snprintf(song.album, sizeof(song.album), "앨범");
    assert(song_text_contains_hangul(&song));
}

int main(void) {
    test_hangul_detection();
    test_hangul_composition();
    test_utf8_copy_truncated();
    test_song_metadata_hangul();
    puts("unicode text tests: ok");
    return 0;
}
