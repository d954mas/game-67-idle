#ifndef FEATURES_SETTINGS_H
#define FEATURES_SETTINGS_H
// feature-layer: L2
/* Единственный публичный хедер фичи settings — вся её публичная поверхность;
   всё остальное в папке static. Слой L2 (surface): зависит только от L0-шелла
   (ui_runtime / theme / World / game_save), не от других фич. */
#include "ui/nt_ui.h"    /* nt_ui_context_t */
#include "world/world.h" /* World */

#include <stdbool.h>

/* Фазовая отрисовка. Зовётся из game_features_draw_ui() внутри ui_runtime-кадра;
   ctx даёт агрегатор. Рисует гир-кнопку всегда + панель, когда is_open. */
void settings_draw_ui(nt_ui_context_t *ctx, World *w);

/* Навигация: фича экспортирует open/close/is_open; КОГДА открыть решает игра
   (main.c, флаг --settings). Гир-кнопка панели — собственный аффорданс фичи. */
void settings_open(void);
void settings_close(void);
bool settings_is_open(void);

/* Текущие громкости 0..1 — игра подключает их к своему аудио-микшеру. */
float settings_master(void);
float settings_music(void);
float settings_sfx(void);

#if FEATURE_GAME_STATE
/* clamp [0,1] -> запись в фрагмент settings_state -> game_save_mark_dirty().
   Существуют только под FEATURE_GAME_STATE (persist-путь); экран передаёт их
   как commit-колбэки. Под !FEATURE_GAME_STATE commit = NULL (сессионный store). */
void settings_set_master(float value);
void settings_set_music(float value);
void settings_set_sfx(float value);
#endif

#endif /* FEATURES_SETTINGS_H */
