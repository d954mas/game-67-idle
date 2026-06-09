#ifndef GAME_STATE_ACTIONS_H
#define GAME_STATE_ACTIONS_H

#include <stdbool.h>

#include "generated/game_state.h"

bool game_state_action_shape_next(GameState *state, char *error, int error_cap);
bool game_state_action_shape_prev(GameState *state, char *error, int error_cap);
bool game_state_action_render_mode_next(GameState *state, char *error, int error_cap);
bool game_state_action_camera_zoom(GameState *state, float wheel_delta, float zoom_speed, char *error, int error_cap);
bool game_state_action_test_ui_click(GameState *state, char *error, int error_cap);
bool game_state_action_set_master_volume(GameState *state, float volume, char *error, int error_cap);
bool game_state_action_set_sfx_volume(GameState *state, float volume, char *error, int error_cap);

#endif
