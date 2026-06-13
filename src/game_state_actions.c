#include "game_state_actions.h"

#include <stdio.h>

void game_seed_reset_playtest(GameState *state) {
    game_state_init_defaults(state);
    game_state_mark_dirty();
}

void game_seed_cycle(GameState *state) {
    state->test_ui_clicks += 1;
    state->shape_index = (state->shape_index + 1) % GAME_STATE_SHAPE_COUNT;
    state->wallet_soft += 1;
    state->tutorial_done = true;
    (void)snprintf(state->test_label_text, sizeof(state->test_label_text), "Template clicks: %d", state->test_ui_clicks);
    game_state_mark_dirty();
}

const char *game_seed_shape_label(const GameState *state) {
    return game_state_shape_name(state->shape_index);
}
