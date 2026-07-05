#ifndef RB_DARK_RPG_JOURNAL_SCREEN_H
#define RB_DARK_RPG_JOURNAL_SCREEN_H

#include "ui/nt_ui.h"
#include "world/world.h"

#include <stdbool.h>

bool journal_screen_open(void);
void journal_screen_set_open(bool open);
void journal_screen_toggle(void);
void journal_screen_ui(nt_ui_context_t *ctx, World *w);

#endif /* RB_DARK_RPG_JOURNAL_SCREEN_H */
