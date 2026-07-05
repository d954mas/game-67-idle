#ifndef RB_DARK_RPG_WORLD_MAP_SCREEN_H
#define RB_DARK_RPG_WORLD_MAP_SCREEN_H

#include "ui/nt_ui.h"
#include "world/world.h"

#include <stdbool.h>

bool world_map_screen_open(void);
void world_map_screen_set_open(bool open);
void world_map_screen_open_map(void);
void world_map_screen_toggle_map(void);
void world_map_screen_ui(nt_ui_context_t *ctx, World *w);

#endif /* RB_DARK_RPG_WORLD_MAP_SCREEN_H */
