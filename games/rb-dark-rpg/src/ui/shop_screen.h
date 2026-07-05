#ifndef RB_DARK_RPG_SHOP_SCREEN_H
#define RB_DARK_RPG_SHOP_SCREEN_H

#include "ui/nt_ui.h"
#include "world/world.h"

#include <stdbool.h>

bool shop_screen_open(void);
void shop_screen_set_open(bool open);
bool shop_screen_open_shop(const char *shop_id);
void shop_screen_ui(nt_ui_context_t *ctx, World *w);

#endif /* RB_DARK_RPG_SHOP_SCREEN_H */
