#pragma once

#include "model.h"
#include "player.h"

#include <3ds.h>
#include <citro2d.h>

typedef struct Ui Ui;

typedef enum {
    UI_IME_NONE = 0,
    UI_IME_SUBMIT,
    UI_IME_CANCEL
} UiImeAction;

typedef enum {
    UI_PLAYER_TOUCH_NONE = 0,
    UI_PLAYER_TOUCH_PREVIOUS,
    UI_PLAYER_TOUCH_PLAY_PAUSE,
    UI_PLAYER_TOUCH_NEXT,
    UI_PLAYER_TOUCH_PLAY_MODE,
    UI_PLAYER_TOUCH_ALBUM,
    UI_PLAYER_TOUCH_SEEK,
    UI_PLAYER_TOUCH_QUEUE_ITEM,
    UI_PLAYER_TOUCH_PLAYLIST_FOCUS
} UiPlayerTouchAction;

Ui *ui_create(C3D_RenderTarget *top_left, C3D_RenderTarget *top_right,
              C3D_RenderTarget *bottom);
void ui_destroy(Ui *ui);
void ui_draw_startup(Ui *ui, unsigned int step, unsigned int total,
                     const char *status);
void ui_draw(Ui *ui, const AppState *app, const Player *player);
void ui_draw_once(Ui *ui, const AppState *app, const Player *player);
bool ui_menu_font_ready(const Ui *ui);
int ui_prepare_immersive_font(Ui *ui, char *error, size_t error_size);

bool ui_ime_begin(Ui *ui, const char *initial_text);
bool ui_ime_active(const Ui *ui);
const char *ui_ime_text(const Ui *ui);
UiImeAction ui_ime_handle(Ui *ui, u32 down, u32 repeat,
                          const touchPosition *touch);
UiPlayerTouchAction ui_player_touch(const AppState *app,
                                    const touchPosition *touch,
                                    int *queue_index, float *seek_ratio);
bool ui_player_seek_ratio(const touchPosition *touch, float *seek_ratio);

int ui_load_cover(Ui *ui, const char *path, int64_t song_id,
                  char *error, size_t error_size);
int ui_upload_cover(Ui *ui, const uint32_t *pixels, size_t pixel_count,
                    int64_t song_id, char *error, size_t error_size);
void ui_clear_cover(Ui *ui);
bool ui_set_login_qr(Ui *ui, const char *key);
void ui_clear_login_qr(Ui *ui);
