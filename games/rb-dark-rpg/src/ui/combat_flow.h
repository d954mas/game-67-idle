#ifndef RB_DARK_RPG_COMBAT_FLOW_H
#define RB_DARK_RPG_COMBAT_FLOW_H

#include "ui/nt_ui.h"
#include "world/world.h"

#include <stdbool.h>

bool combat_flow_is_open(const World *w);
bool combat_flow_can_start_gate_check(const World *w);
void combat_flow_open_prefight(World *w, const char *encounter_id);
void combat_flow_ui(nt_ui_context_t *ctx, World *w);

#endif /* RB_DARK_RPG_COMBAT_FLOW_H */
