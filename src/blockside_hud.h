#ifndef BLOCKSIDE_HUD_H
#define BLOCKSIDE_HUD_H

#include "blockside_game_types.h"
#include "font/nt_font.h"
#include "graphics/nt_gfx.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"

void blockside_draw_hud(const GameRuntime *game,
                        nt_material_t text_material,
                        nt_resource_t font_resource,
                        nt_font_t font,
                        nt_buffer_t frame_ubo);

#endif /* BLOCKSIDE_HUD_H */
