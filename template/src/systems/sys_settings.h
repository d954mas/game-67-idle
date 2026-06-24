#ifndef GAME_SYS_SETTINGS_H
#define GAME_SYS_SETTINGS_H

#include "ui/nt_ui.h"
#include "world/world.h"

// Settings system: a gear button + a centered panel built from nt_ui WIDGETS
// (slice9 panel + sliders + buttons), NOT hand-drawn text. Only the LOGIC lives
// here (volumes, open state, reset); styling lives in ui/theme. Build it between
// ui_runtime_begin and ui_runtime_end.
void sys_settings_ui(nt_ui_context_t *ctx, World *w);

void sys_settings_force_open(void); // debug/screenshot: open the panel programmatically

// Current volumes (0..1) — a game wires these to its audio system.
float sys_settings_master(void);
float sys_settings_music(void);
float sys_settings_sfx(void);

#endif /* GAME_SYS_SETTINGS_H */
