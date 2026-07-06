#ifndef GAME_FEATURES_H
#define GAME_FEATURES_H

#include "world/world.h"

/* L0 агрегатор фич шаблона (feature_architecture §2). СЕМЬ фазовых функций,
   явные вызовы по одной строке на систему/фичу — НИКАКИХ hook-таблиц/шедулера.
   Двухфазный кадр событий гонит шелл (main.c) через game_events API:
     update эмитит -> react до фикспойнта -> record одним проходом -> reset.
   react/record несут World* как остальные фазы; события читаются глобально через
   game_event_log() (event §7). Список draw_ui сверху вниз = z-order.
   Игра добавляет потребителя ОДНОЙ строкой в react- или record-список. */

void game_features_init(World *w);
void game_features_update(World *w, float dt); /* фаза ЭМИССИИ (системы/фичи эмитят) */
void game_features_react(World *w);            /* реакторы-потребители (могут каскадить) */
void game_features_record(World *w);           /* чистые рекордеры (аналитика/лог/DevAPI) */
void game_features_draw_world(World *w);        /* 3D-слой фич (см. §E1.5: пока прямой шелл) */
void game_features_draw_ui(World *w);           /* UI-слой фич, z-order (см. §E1.5) */
void game_features_shutdown(World *w);

#endif /* GAME_FEATURES_H */
