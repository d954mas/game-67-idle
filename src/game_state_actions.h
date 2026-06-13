#ifndef GAME_STATE_ACTIONS_H
#define GAME_STATE_ACTIONS_H

#include "generated/game_state.h"

void game_seed_reset_playtest(GameState *state);
void game_seed_cycle(GameState *state);
const char *game_seed_shape_label(const GameState *state);

#endif
