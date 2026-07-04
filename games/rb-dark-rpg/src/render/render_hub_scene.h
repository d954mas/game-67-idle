#ifndef RB_DARK_RPG_RENDER_HUB_SCENE_H
#define RB_DARK_RPG_RENDER_HUB_SCENE_H

#include "graphics/nt_gfx.h"
#include "world/world.h"

void render_hub_scene_init(void);
void render_hub_scene_draw(const World *w, nt_buffer_t frame_ubo);
void render_hub_scene_shutdown(void);

#endif /* RB_DARK_RPG_RENDER_HUB_SCENE_H */
