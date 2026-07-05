#ifndef RB_DARK_RPG_LOCATION_SCREEN_H
#define RB_DARK_RPG_LOCATION_SCREEN_H

#include "ui/nt_ui.h"
#include "world/world.h"

#include <stdbool.h>

bool location_screen_open(void);
void location_screen_set_open(bool open);
void location_screen_open_screen(void);
void location_screen_toggle(void);
void location_screen_ui(nt_ui_context_t *ctx, World *w);

#endif /* RB_DARK_RPG_LOCATION_SCREEN_H */
