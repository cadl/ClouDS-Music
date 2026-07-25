#pragma once

#include "model.h"

#include <stdbool.h>
#include <stdint.h>

#define IMMERSIVE_CONTROLS_HOLD_MS 3000U
#define IMMERSIVE_CONTROLS_FADE_MS 1000U
#define IMMERSIVE_FLIP_DURATION_MS 600U
#define IMMERSIVE_LYRIC_FADE_DURATION_MS 400U

/* Immersive lyrics are only valid when the lyrics belong to the song shown on
 * Now Playing. During an initial prebuffer that may be the pending song; once
 * playback exists, the currently audible song remains the displayed song. */
bool immersive_lyrics_available(const AppState *app);
bool immersive_lyrics_style_valid(ImmersiveLyricStyle style);
ImmersiveLyricStyle immersive_lyrics_next_style(ImmersiveLyricStyle style);
const char *immersive_lyrics_style_name(ImmersiveLyricStyle style);
float immersive_lyrics_controls_alpha(uint64_t now_ms,
                                      uint64_t shown_since_ms);
float immersive_lyrics_flip_progress(uint32_t playback_ms,
                                     uint32_t line_start_ms);
float immersive_lyrics_fade_alpha(uint32_t playback_ms,
                                  uint32_t line_start_ms,
                                  uint32_t line_end_ms);
float immersive_lyrics_crawl_progress(uint32_t playback_ms,
                                      uint32_t line_start_ms,
                                      uint32_t line_end_ms);
