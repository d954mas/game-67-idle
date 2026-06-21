#ifndef GAME_ACTIONS_H
#define GAME_ACTIONS_H

#include <stdbool.h>

#include "game_state.h"

typedef enum GameActionResult {
    GAME_ACTION_OK = 0,
    GAME_ACTION_BLOCKED,
    GAME_ACTION_ERROR,
} GameActionResult;

void game_actions_reset_ember_road(void);
GameActionResult game_actions_accept_quest(char *message, int message_cap);
GameActionResult game_actions_travel_north_road(char *message, int message_cap);
GameActionResult game_actions_auto_battle(char *message, int message_cap);
GameActionResult game_actions_equip_ring(char *message, int message_cap);
GameActionResult game_actions_claim_reward(char *message, int message_cap);
GameActionResult game_actions_enter_old_mine(char *message, int message_cap);
GameActionResult game_actions_scout_old_mine(char *message, int message_cap);
GameActionResult game_actions_resolve_old_mine_depth(char *message, int message_cap);
GameActionResult game_actions_delve_old_mine(char *message, int message_cap);
GameActionResult game_actions_return_old_gate(char *message, int message_cap);
GameActionResult game_actions_forge_mine_lantern(char *message, int message_cap);

const char *game_actions_primary_action_id(void);
const char *game_actions_primary_action_label(void);
const char *game_actions_objective_text(void);
const char *game_actions_location_title(void);
const char *game_actions_battle_text(void);

#endif
