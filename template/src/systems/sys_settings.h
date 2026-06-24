#ifndef GAME_SYS_SETTINGS_H
#define GAME_SYS_SETTINGS_H

#include "font/nt_font.h"
#include "graphics/nt_gfx.h"
#include "material/nt_material.h"
#include "resource/nt_resource.h"
#include "world/world.h"

// Settings system: a gear button (top-right) opens a panel with volume sliders
// (master/music/SFX), a Close button, and a Reset button that resets game state
// on a LONG-PRESS (hold to confirm). Owns its own UI state; resets the World.
// A separate system from gameplay/render. (Drawn with text for now; swap in the
// CC0 GUI art — 9-slice panel + buttons — when packed.)
void sys_settings_update(World *w, float dt);
void sys_settings_force_open(void); // debug/screenshot: open the panel programmatically
void sys_settings_draw(nt_material_t text_material, nt_resource_t font_resource, nt_font_t font, nt_buffer_t frame_ubo);

// Current volumes (0..1) — a game wires these to its audio system.
float sys_settings_master(void);
float sys_settings_music(void);
float sys_settings_sfx(void);

#endif /* GAME_SYS_SETTINGS_H */
