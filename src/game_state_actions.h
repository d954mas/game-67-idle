#ifndef GAME_STATE_ACTIONS_H
#define GAME_STATE_ACTIONS_H

#include <stdbool.h>

#include "generated/game_state.h"

bool game_state_action_do67(GameState *state, char *error, int error_cap);
bool game_state_action_buy_first_upgrade(GameState *state, char *error, int error_cap);
bool game_state_action_buy_second_upgrade(GameState *state, char *error, int error_cap);
bool game_state_action_buy_third_upgrade(GameState *state, char *error, int error_cap);
bool game_state_action_buy_fourth_upgrade(GameState *state, char *error, int error_cap);
bool game_state_action_buy_fifth_upgrade(GameState *state, char *error, int error_cap);
bool game_state_action_start_first_job(GameState *state, char *error, int error_cap);
bool game_state_action_claim_first_job(GameState *state, char *error, int error_cap);
bool game_state_action_tick(GameState *state, int delta_ms, char *error, int error_cap);
bool game_state_action_reset_playtest(GameState *state, char *error, int error_cap);
bool game_state_action_set_master_volume(GameState *state, float volume, char *error, int error_cap);
bool game_state_action_set_sfx_volume(GameState *state, float volume, char *error, int error_cap);

#endif
