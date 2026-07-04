#ifndef RB_DARK_RPG_GAME_ACTIONS_H
#define RB_DARK_RPG_GAME_ACTIONS_H

#include "game_dialogue.h"
#include "game_state.h"

#include <stdbool.h>

typedef enum game_action_gear_slot_t {
    GAME_ACTION_GEAR_SLOT_NONE = 0,
    GAME_ACTION_GEAR_SLOT_WEAPON,
    GAME_ACTION_GEAR_SLOT_ARMOUR,
    GAME_ACTION_GEAR_SLOT_LEGS,
    GAME_ACTION_GEAR_SLOT_CHARM,
} game_action_gear_slot_t;

bool game_actions_grant_gear(GameState *state, const char *instance_id, const char *def_id, game_action_gear_slot_t preferred_slot);
bool game_actions_start_quest(GameState *state, const char *quest_id, const char *current_step_id, const char *reason);
bool game_actions_complete_step(GameState *state, const char *quest_id, const char *step_id, const char *next_step_id, const char *reason);
bool game_actions_claim_reward_once(GameState *state, const char *reward_id);
bool game_actions_record_choice(GameState *state, const char *choice_id);
bool game_actions_set_flag(GameState *state, const char *flag_id);
bool game_actions_apply_dialogue_choice(GameState *state, const char *dialogue_id, const char *choice_id, const char *reward_id,
                                        const dialogue_effect_t *effects, int effect_count);

#endif
