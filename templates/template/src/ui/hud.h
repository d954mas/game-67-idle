#ifndef GAME_UI_HUD_H
#define GAME_UI_HUD_H

#include "font/nt_font.h"
#include "graphics/nt_gfx.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"

// Draws the on-screen HUD/UI text. A separate system from the game loop and the
// 3D render; called from the conductor's frame() after the world is rendered.
void hud_draw(nt_material_t text_material, nt_resource_t font_resource, nt_font_t font, nt_buffer_t frame_ubo);

#endif /* GAME_UI_HUD_H */
