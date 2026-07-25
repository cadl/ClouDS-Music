#include "song_text.h"

#include "unicode_text.h"

bool song_text_contains_hangul(const Song *song) {
    return song &&
           (utf8_contains_hangul(song->title) ||
            utf8_contains_hangul(song->artist) ||
            utf8_contains_hangul(song->album));
}

void song_text_compose_hangul_nfc(Song *song) {
    if (!song) return;
    (void)utf8_compose_hangul_nfc(song->title);
    (void)utf8_compose_hangul_nfc(song->artist);
    (void)utf8_compose_hangul_nfc(song->album);
}
