#ifndef RB_DARK_RPG_END_SCREEN_H
#define RB_DARK_RPG_END_SCREEN_H

#include "ui/nt_ui.h"
#include "world/world.h"

/* Full-screen end-of-act beat. Shows once when the finale flag is set, then
 * persists a "seen" flag so it survives save/load and never re-triggers. */
void end_screen_ui(nt_ui_context_t *ctx, World *w);

#endif /* RB_DARK_RPG_END_SCREEN_H */
