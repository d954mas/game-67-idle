#ifndef GAME_UI_DEMO_HUD_H
#define GAME_UI_DEMO_HUD_H

#include "ui/nt_ui.h"

/* КОМПОЗИЦИЯ ИГРЫ (game-owned), не фича: единственный TU разрешено включать
   СРАЗУ resource_panel.h + items.h + progression.h -- см. demo_hud.c и
   game_features.c зовёт эти две
   функции ОДНОЙ строкой каждую. */
void demo_hud_update(float dt);
void demo_hud_draw_ui(nt_ui_context_t *ctx);

#endif /* GAME_UI_DEMO_HUD_H */
