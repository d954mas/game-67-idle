#ifndef RB_DARK_RPG_GAME_ACTIONS_H
#define RB_DARK_RPG_GAME_ACTIONS_H

#include "game_combat.h"
#include "game_content.h"
#include "game_dialogue.h"
#include "game_state.h"

#include <stdbool.h>

typedef enum game_action_gear_slot_t {
  GAME_ACTION_GEAR_SLOT_NONE = 0,
  GAME_ACTION_GEAR_SLOT_WEAPON,
  GAME_ACTION_GEAR_SLOT_OFFHAND,
  GAME_ACTION_GEAR_SLOT_HEAD,
  GAME_ACTION_GEAR_SLOT_ARMOUR,
  GAME_ACTION_GEAR_SLOT_HANDS,
  GAME_ACTION_GEAR_SLOT_WAIST,
  GAME_ACTION_GEAR_SLOT_LEGS,
  GAME_ACTION_GEAR_SLOT_FEET,
  GAME_ACTION_GEAR_SLOT_NECK,
  GAME_ACTION_GEAR_SLOT_RING_LEFT,
  GAME_ACTION_GEAR_SLOT_RING_RIGHT,
  GAME_ACTION_GEAR_SLOT_RELIC,
} game_action_gear_slot_t;

bool game_actions_grant_gear(GameState *state, const char *instance_id,
                             const char *def_id,
                             game_action_gear_slot_t preferred_slot);
bool game_actions_equip_gear(GameState *state, const char *instance_id);
bool game_actions_starter_gear_equipped(const GameState *state);
bool game_actions_needs_starter_gear_onboarding(const GameState *state);
bool game_actions_needs_gate_check_onboarding(const GameState *state);
bool game_actions_location_unlocked(const GameState *state,
                                    const char *location_id);
bool game_actions_can_move_location(const GameState *state,
                                    const char *location_id);
bool game_actions_location_object_available(
    const GameState *state, const game_location_object_t *object);
const game_location_interaction_t *
game_actions_select_location_interaction(const GameState *state,
                                         const game_location_object_t *object);
bool game_actions_move_location(GameState *state, const char *location_id);
bool game_actions_inspect_object(GameState *state, const char *object_id);
bool game_actions_start_quest(GameState *state, const char *quest_id,
                              const char *current_step_id, const char *reason);
bool game_actions_complete_step(GameState *state, const char *quest_id,
                                const char *step_id, const char *next_step_id,
                                const char *reason);
bool game_actions_complete_quest(GameState *state, const char *quest_id,
                                 const char *reason);
bool game_actions_claim_reward_once(GameState *state, const char *reward_id);
bool game_actions_record_choice(GameState *state, const char *choice_id);
bool game_actions_set_flag(GameState *state, const char *flag_id);
bool game_actions_restore_hp(GameState *state);
bool game_actions_shop_item_available(const GameState *state,
                                      const game_shop_item_t *shop_item);
bool game_actions_can_purchase_shop_item(
    const GameState *state, const game_shop_definition_t *shop,
    const game_shop_item_t *shop_item);
bool game_actions_purchase_shop_item(GameState *state, const char *shop_id,
                                     const char *item_id);
bool game_actions_resolve_encounter(GameState *state, const char *encounter_id,
                                    game_combat_result_t *out_result);
bool game_actions_apply_dialogue_choice(
    GameState *state, const char *dialogue_id, const char *choice_id,
    const char *reward_id, const dialogue_effect_t *effects, int effect_count);

#endif
