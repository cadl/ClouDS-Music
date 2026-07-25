#pragma once

#include "model.h"

#include <stdbool.h>

bool song_text_contains_hangul(const Song *song);
void song_text_compose_hangul_nfc(Song *song);
