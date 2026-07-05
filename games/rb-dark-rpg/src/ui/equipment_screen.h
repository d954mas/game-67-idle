#ifndef RB_DARK_RPG_EQUIPMENT_SCREEN_H
#define RB_DARK_RPG_EQUIPMENT_SCREEN_H

#include "ui/nt_ui.h"
#include "world/world.h"

#include <stdbool.h>

bool equipment_screen_open(void);
void equipment_screen_set_open(bool open);
void equipment_screen_toggle(void);
void equipment_screen_ui(nt_ui_context_t *ctx, World *w);

#endif /* RB_DARK_RPG_EQUIPMENT_SCREEN_H */
