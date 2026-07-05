#include "game_actions.h"

#include "game_content.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static bool copy_id(char dst[GAME_STATE_STRING_MAX], const char *src) {
  if (!dst || !src || src[0] == '\0' || strlen(src) >= GAME_STATE_STRING_MAX) {
    return false;
  }
  (void)strcpy(dst, src);
  return true;
}

static bool list_contains(const char list[][GAME_STATE_STRING_MAX], int count,
                          const char *id) {
  if (!id) {
    return false;
  }
  for (int i = 0; i < count; ++i) {
    if (strcmp(list[i], id) == 0) {
      return true;
    }
  }
  return false;
}

static bool list_add_unique(char list[][GAME_STATE_STRING_MAX], int *count,
                            int max_count, const char *id) {
  if (!count || !id || id[0] == '\0' || strlen(id) >= GAME_STATE_STRING_MAX) {
    return false;
  }
  if (list_contains(list, *count, id)) {
    return true;
  }
  if (*count >= max_count) {
    return false;
  }
  (void)strcpy(list[*count], id);
  *count += 1;
  return true;
}

static bool list_remove(char list[][GAME_STATE_STRING_MAX], int *count,
                        const char *id) {
  if (!list || !count || !id || id[0] == '\0') {
    return false;
  }
  for (int i = 0; i < *count; ++i) {
    if (strcmp(list[i], id) != 0) {
      continue;
    }
    for (int j = i + 1; j < *count; ++j) {
      (void)strcpy(list[j - 1], list[j]);
    }
    *count -= 1;
    if (*count >= 0) {
      list[*count][0] = '\0';
    }
    return true;
  }
  return true;
}

static GameGearInstance *find_gear(GameState *state, const char *instance_id) {
  if (!state || !instance_id) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
    GameGearInstance *gear = &state->inventory_gear_instances[i];
    if (gear->used && strcmp(gear->key, instance_id) == 0) {
      return gear;
    }
  }
  return NULL;
}

static const GameGearInstance *find_gear_const(const GameState *state,
                                               const char *instance_id) {
  if (!state || !instance_id) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
    const GameGearInstance *gear = &state->inventory_gear_instances[i];
    if (gear->used && strcmp(gear->key, instance_id) == 0) {
      return gear;
    }
  }
  return NULL;
}

static bool bag_contains_instance(const GameState *state,
                                  const char *instance_id) {
  return state && list_contains(state->inventory_bag_order,
                                state->inventory_bag_order_count,
                                instance_id);
}

static GameGearInstance *alloc_gear(GameState *state) {
  if (!state) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
    if (!state->inventory_gear_instances[i].used) {
      return &state->inventory_gear_instances[i];
    }
  }
  return NULL;
}

static GameStackInstance *find_stack(GameState *state,
                                     const char *instance_id) {
  if (!state || !instance_id) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_INVENTORY_STACK_INSTANCES; ++i) {
    GameStackInstance *stack = &state->inventory_stack_instances[i];
    if (stack->used && strcmp(stack->key, instance_id) == 0) {
      return stack;
    }
  }
  return NULL;
}

static GameStackInstance *alloc_stack(GameState *state) {
  if (!state) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_INVENTORY_STACK_INSTANCES; ++i) {
    if (!state->inventory_stack_instances[i].used) {
      return &state->inventory_stack_instances[i];
    }
  }
  return NULL;
}

static GameQuestState *find_quest(GameState *state, const char *quest_id) {
  if (!state || !quest_id) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
    GameQuestState *quest = &state->quests_quest_states[i];
    if (quest->used && strcmp(quest->key, quest_id) == 0) {
      return quest;
    }
  }
  return NULL;
}

static const GameQuestState *find_quest_const(const GameState *state,
                                              const char *quest_id) {
  if (!state || !quest_id) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
    const GameQuestState *quest = &state->quests_quest_states[i];
    if (quest->used && strcmp(quest->key, quest_id) == 0) {
      return quest;
    }
  }
  return NULL;
}

static GameQuestState *find_or_alloc_quest(GameState *state,
                                           const char *quest_id) {
  GameQuestState *quest = find_quest(state, quest_id);
  if (quest) {
    return quest;
  }
  if (!state || !quest_id || strlen(quest_id) >= GAME_STATE_STRING_MAX) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
    quest = &state->quests_quest_states[i];
    if (!quest->used) {
      *quest = (GameQuestState){0};
      quest->used = true;
      (void)strcpy(quest->key, quest_id);
      quest->status = GAME_STATE_QUEST_STATUS_HIDDEN;
      return quest;
    }
  }
  return NULL;
}

static GameQuestState *
find_active_encounter_quest(GameState *state, const char *encounter_id,
                            const game_quest_encounter_step_t **out_step) {
  if (out_step) {
    *out_step = NULL;
  }
  if (!state || !encounter_id) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
    GameQuestState *quest = &state->quests_quest_states[i];
    if (!quest->used || quest->status != GAME_STATE_QUEST_STATUS_ACTIVE ||
        !quest->has_current_step_id) {
      continue;
    }
    const game_quest_encounter_step_t *step =
        game_content_find_encounter_step(quest->key, encounter_id);
    if (!step || strcmp(quest->current_step_id, step->step_id) != 0 ||
        list_contains(state->quests_completed_step_ids,
                      state->quests_completed_step_ids_count, step->step_id)) {
      continue;
    }
    if (out_step) {
      *out_step = step;
    }
    return quest;
  }
  return NULL;
}

static bool quest_current_step_matches(const GameState *state,
                                       const char *quest_id,
                                       const char *step_id) {
  const GameQuestState *quest = find_quest_const(state, quest_id);
  return quest && quest->status == GAME_STATE_QUEST_STATUS_ACTIVE &&
         quest->has_current_step_id && step_id &&
         strcmp(quest->current_step_id, step_id) == 0;
}

static bool quest_status_matches(const GameQuestState *quest,
                                 const char *status) {
  if (!quest || !status) {
    return false;
  }
  if (strcmp(status, "hidden") == 0) {
    return quest->status == GAME_STATE_QUEST_STATUS_HIDDEN;
  }
  if (strcmp(status, "available") == 0) {
    return quest->status == GAME_STATE_QUEST_STATUS_AVAILABLE;
  }
  if (strcmp(status, "active") == 0) {
    return quest->status == GAME_STATE_QUEST_STATUS_ACTIVE;
  }
  if (strcmp(status, "ready_to_turn_in") == 0) {
    return quest->status == GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN;
  }
  if (strcmp(status, "completed") == 0) {
    return quest->status == GAME_STATE_QUEST_STATUS_COMPLETED;
  }
  if (strcmp(status, "failed") == 0) {
    return quest->status == GAME_STATE_QUEST_STATUS_FAILED;
  }
  if (strcmp(status, "content_missing") == 0) {
    return quest->status == GAME_STATE_QUEST_STATUS_CONTENT_MISSING;
  }
  return false;
}

static bool is_item_equipped(const GameState *state, const char *item_id) {
  if (!state || !item_id) {
    return false;
  }
  const char *slots[] = {
      state->has_equipment_weapon_instance_id
          ? state->equipment_weapon_instance_id
          : NULL,
      state->has_equipment_offhand_instance_id
          ? state->equipment_offhand_instance_id
          : NULL,
      state->has_equipment_head_instance_id ? state->equipment_head_instance_id
                                            : NULL,
      state->has_equipment_armour_instance_id
          ? state->equipment_armour_instance_id
          : NULL,
      state->has_equipment_hands_instance_id
          ? state->equipment_hands_instance_id
          : NULL,
      state->has_equipment_waist_instance_id
          ? state->equipment_waist_instance_id
          : NULL,
      state->has_equipment_legs_instance_id ? state->equipment_legs_instance_id
                                            : NULL,
      state->has_equipment_feet_instance_id ? state->equipment_feet_instance_id
                                            : NULL,
      state->has_equipment_neck_instance_id ? state->equipment_neck_instance_id
                                            : NULL,
      state->has_equipment_ring_left_instance_id
          ? state->equipment_ring_left_instance_id
          : NULL,
      state->has_equipment_ring_right_instance_id
          ? state->equipment_ring_right_instance_id
          : NULL,
      state->has_equipment_charm_instance_id
          ? state->equipment_charm_instance_id
          : NULL,
  };
  for (int i = 0; i < (int)(sizeof slots / sizeof slots[0]); ++i) {
    if (!slots[i]) {
      continue;
    }
    GameGearInstance *gear = find_gear((GameState *)state, slots[i]);
    if (gear && strcmp(gear->def_id, item_id) == 0) {
      return true;
    }
  }
  return false;
}

bool game_actions_starter_gear_equipped(const GameState *state) {
  return state && is_item_equipped(state, "old_sword") &&
         is_item_equipped(state, "padded_jacket") &&
         is_item_equipped(state, "leather_greaves");
}

static bool is_starter_gear_step(const char *step_id) {
  return step_id &&
         (strcmp(step_id, "equip_old_sword") == 0 ||
          strcmp(step_id, "equip_padded_jacket") == 0 ||
          strcmp(step_id, "equip_leather_greaves") == 0);
}

bool game_actions_needs_starter_gear_onboarding(const GameState *state) {
  if (!state ||
      !list_contains(state->flags_ids, state->flags_ids_count,
                     "starter_gear_received") ||
      game_actions_starter_gear_equipped(state)) {
    return false;
  }
  const GameQuestState *quest = find_quest_const(state, "q001_gate_pass");
  return quest && quest->status == GAME_STATE_QUEST_STATUS_ACTIVE &&
         quest->has_current_step_id &&
         is_starter_gear_step(quest->current_step_id);
}

bool game_actions_needs_gate_check_onboarding(const GameState *state) {
  if (!state ||
      list_contains(state->flags_ids, state->flags_ids_count,
                    "gate_scavenger_defeated")) {
    return false;
  }
  const GameQuestState *quest = find_quest_const(state, "q001_gate_pass");
  return quest && quest->status == GAME_STATE_QUEST_STATUS_ACTIVE &&
         quest->has_current_step_id &&
         strcmp(quest->current_step_id, "clear_gate_scavenger") == 0;
}

static bool
location_requirement_met(const GameState *state,
                         const game_location_requirement_t *requirement) {
  if (!state || !requirement) {
    return false;
  }
  switch (requirement->kind) {
  case GAME_LOCATION_REQUIREMENT_NONE:
    return true;
  case GAME_LOCATION_REQUIREMENT_FLAG: {
    const bool has = list_contains(state->flags_ids, state->flags_ids_count,
                                   requirement->id);
    return has == requirement->value;
  }
  case GAME_LOCATION_REQUIREMENT_EQUIPPED:
    return is_item_equipped(state, requirement->id);
  case GAME_LOCATION_REQUIREMENT_QUEST_ACTIVE: {
    const GameQuestState *quest = find_quest_const(state, requirement->id);
    return quest && quest->status == GAME_STATE_QUEST_STATUS_ACTIVE;
  }
  case GAME_LOCATION_REQUIREMENT_QUEST_STATUS: {
    const GameQuestState *quest = find_quest_const(state, requirement->id);
    return quest_status_matches(quest, requirement->status);
  }
  case GAME_LOCATION_REQUIREMENT_QUEST_STEP:
    return quest_current_step_matches(state, requirement->id,
                                      requirement->step_id);
  default:
    return false;
  }
}

static bool
location_requirements_met(const GameState *state,
                          const game_location_requirement_t *requirements,
                          int requirement_count) {
  if (requirement_count <= 0) {
    return true;
  }
  if (!state || !requirements) {
    return false;
  }
  for (int i = 0; i < requirement_count; ++i) {
    if (!location_requirement_met(state, &requirements[i])) {
      return false;
    }
  }
  return true;
}

static bool
shop_requirement_met(const GameState *state,
                     const game_service_requirement_t *requirement) {
  if (!state || !requirement) {
    return false;
  }
  switch (requirement->kind) {
  case GAME_SERVICE_REQUIREMENT_NONE:
    return true;
  case GAME_SERVICE_REQUIREMENT_FLAG: {
    const bool has = list_contains(state->flags_ids, state->flags_ids_count,
                                   requirement->id);
    return has == requirement->value;
  }
  case GAME_SERVICE_REQUIREMENT_QUEST_COMPLETED: {
    const GameQuestState *quest = find_quest_const(state, requirement->id);
    return quest && quest->status == GAME_STATE_QUEST_STATUS_COMPLETED;
  }
  case GAME_SERVICE_REQUIREMENT_QUEST_STATUS: {
    const GameQuestState *quest = find_quest_const(state, requirement->id);
    return quest_status_matches(quest, requirement->status);
  }
  case GAME_SERVICE_REQUIREMENT_QUEST_STEP:
    return quest_current_step_matches(state, requirement->id,
                                      requirement->step_id);
  default:
    return false;
  }
}

static bool
shop_requirements_met(const GameState *state,
                      const game_service_requirement_t *requirements,
                      int requirement_count) {
  if (requirement_count <= 0) {
    return true;
  }
  if (!state || !requirements) {
    return false;
  }
  for (int i = 0; i < requirement_count; ++i) {
    if (!shop_requirement_met(state, &requirements[i])) {
      return false;
    }
  }
  return true;
}

static bool location_unlocked(const GameState *state,
                              const game_location_definition_t *location) {
  if (!state || !location) {
    return false;
  }
  switch (location->unlock_kind) {
  case GAME_LOCATION_UNLOCK_ALWAYS:
    return true;
  case GAME_LOCATION_UNLOCK_FLAG:
    return list_contains(state->flags_ids, state->flags_ids_count,
                         location->unlock_flag_id);
  case GAME_LOCATION_UNLOCK_QUEST_STEP:
    return quest_current_step_matches(state, location->unlock_quest_id,
                                      location->unlock_step_id);
  default:
    return false;
  }
}

static bool can_exit_to_location(const GameState *state,
                                 const game_location_definition_t *from,
                                 const char *target_location_id) {
  if (!state || !from || !target_location_id) {
    return false;
  }
  for (int i = 0; i < from->exit_count; ++i) {
    const game_location_exit_t *exit_def = &from->exits[i];
    if (!exit_def->target_location_id ||
        strcmp(exit_def->target_location_id, target_location_id) != 0) {
      continue;
    }
    for (int r = 0; r < exit_def->requirement_count; ++r) {
      if (!location_requirement_met(state, &exit_def->requirements[r])) {
        return false;
      }
    }
    return true;
  }
  return false;
}

static bool set_slot(GameState *state, game_action_gear_slot_t slot,
                     const char *instance_id) {
  switch (slot) {
  case GAME_ACTION_GEAR_SLOT_WEAPON:
    state->has_equipment_weapon_instance_id = true;
    return copy_id(state->equipment_weapon_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_OFFHAND:
    state->has_equipment_offhand_instance_id = true;
    return copy_id(state->equipment_offhand_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_HEAD:
    state->has_equipment_head_instance_id = true;
    return copy_id(state->equipment_head_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_ARMOUR:
    state->has_equipment_armour_instance_id = true;
    return copy_id(state->equipment_armour_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_HANDS:
    state->has_equipment_hands_instance_id = true;
    return copy_id(state->equipment_hands_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_WAIST:
    state->has_equipment_waist_instance_id = true;
    return copy_id(state->equipment_waist_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_LEGS:
    state->has_equipment_legs_instance_id = true;
    return copy_id(state->equipment_legs_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_FEET:
    state->has_equipment_feet_instance_id = true;
    return copy_id(state->equipment_feet_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_NECK:
    state->has_equipment_neck_instance_id = true;
    return copy_id(state->equipment_neck_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_RING_LEFT:
    state->has_equipment_ring_left_instance_id = true;
    return copy_id(state->equipment_ring_left_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_RING_RIGHT:
    state->has_equipment_ring_right_instance_id = true;
    return copy_id(state->equipment_ring_right_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_RELIC:
    state->has_equipment_charm_instance_id = true;
    return copy_id(state->equipment_charm_instance_id, instance_id);
  case GAME_ACTION_GEAR_SLOT_NONE:
  default:
    return true;
  }
}

static void clear_slot(GameState *state, game_action_gear_slot_t slot) {
  if (!state) {
    return;
  }
  switch (slot) {
  case GAME_ACTION_GEAR_SLOT_WEAPON:
    state->has_equipment_weapon_instance_id = false;
    state->equipment_weapon_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_OFFHAND:
    state->has_equipment_offhand_instance_id = false;
    state->equipment_offhand_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_HEAD:
    state->has_equipment_head_instance_id = false;
    state->equipment_head_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_ARMOUR:
    state->has_equipment_armour_instance_id = false;
    state->equipment_armour_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_HANDS:
    state->has_equipment_hands_instance_id = false;
    state->equipment_hands_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_WAIST:
    state->has_equipment_waist_instance_id = false;
    state->equipment_waist_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_LEGS:
    state->has_equipment_legs_instance_id = false;
    state->equipment_legs_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_FEET:
    state->has_equipment_feet_instance_id = false;
    state->equipment_feet_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_NECK:
    state->has_equipment_neck_instance_id = false;
    state->equipment_neck_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_RING_LEFT:
    state->has_equipment_ring_left_instance_id = false;
    state->equipment_ring_left_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_RING_RIGHT:
    state->has_equipment_ring_right_instance_id = false;
    state->equipment_ring_right_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_RELIC:
    state->has_equipment_charm_instance_id = false;
    state->equipment_charm_instance_id[0] = '\0';
    return;
  case GAME_ACTION_GEAR_SLOT_NONE:
  default:
    return;
  }
}

static const char *equipped_instance_for_slot(
    const GameState *state, game_action_gear_slot_t slot) {
  if (!state) {
    return NULL;
  }
  switch (slot) {
  case GAME_ACTION_GEAR_SLOT_WEAPON:
    return state->has_equipment_weapon_instance_id
               ? state->equipment_weapon_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_OFFHAND:
    return state->has_equipment_offhand_instance_id
               ? state->equipment_offhand_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_HEAD:
    return state->has_equipment_head_instance_id
               ? state->equipment_head_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_ARMOUR:
    return state->has_equipment_armour_instance_id
               ? state->equipment_armour_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_HANDS:
    return state->has_equipment_hands_instance_id
               ? state->equipment_hands_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_WAIST:
    return state->has_equipment_waist_instance_id
               ? state->equipment_waist_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_LEGS:
    return state->has_equipment_legs_instance_id
               ? state->equipment_legs_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_FEET:
    return state->has_equipment_feet_instance_id
               ? state->equipment_feet_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_NECK:
    return state->has_equipment_neck_instance_id
               ? state->equipment_neck_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_RING_LEFT:
    return state->has_equipment_ring_left_instance_id
               ? state->equipment_ring_left_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_RING_RIGHT:
    return state->has_equipment_ring_right_instance_id
               ? state->equipment_ring_right_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_RELIC:
    return state->has_equipment_charm_instance_id
               ? state->equipment_charm_instance_id
               : NULL;
  case GAME_ACTION_GEAR_SLOT_NONE:
  default:
    return NULL;
  }
}

static bool is_instance_equipped(const GameState *state,
                                 const char *instance_id) {
  if (!state || !instance_id || instance_id[0] == '\0') {
    return false;
  }
  const game_action_gear_slot_t slots[] = {
      GAME_ACTION_GEAR_SLOT_WEAPON,     GAME_ACTION_GEAR_SLOT_OFFHAND,
      GAME_ACTION_GEAR_SLOT_HEAD,       GAME_ACTION_GEAR_SLOT_ARMOUR,
      GAME_ACTION_GEAR_SLOT_HANDS,      GAME_ACTION_GEAR_SLOT_WAIST,
      GAME_ACTION_GEAR_SLOT_LEGS,       GAME_ACTION_GEAR_SLOT_FEET,
      GAME_ACTION_GEAR_SLOT_NECK,       GAME_ACTION_GEAR_SLOT_RING_LEFT,
      GAME_ACTION_GEAR_SLOT_RING_RIGHT, GAME_ACTION_GEAR_SLOT_RELIC,
  };
  for (int i = 0; i < (int)(sizeof slots / sizeof slots[0]); ++i) {
    const char *equipped = equipped_instance_for_slot(state, slots[i]);
    if (equipped && strcmp(equipped, instance_id) == 0) {
      return true;
    }
  }
  return false;
}

bool game_actions_grant_gear(GameState *state, const char *instance_id,
                             const char *def_id,
                             game_action_gear_slot_t preferred_slot) {
  (void)preferred_slot;
  if (!state || !instance_id || !def_id) {
    return false;
  }
  GameGearInstance *gear = find_gear(state, instance_id);
  if (!gear) {
    gear = alloc_gear(state);
    if (!gear) {
      return false;
    }
    *gear = (GameGearInstance){0};
    gear->used = true;
    if (!copy_id(gear->key, instance_id) || !copy_id(gear->def_id, def_id)) {
      *gear = (GameGearInstance){0};
      return false;
    }
    gear->durability = GAME_STATE_GEAR_INSTANCE_DURABILITY_DEFAULT;
    gear->level = GAME_STATE_GEAR_INSTANCE_LEVEL_DEFAULT;
    gear->bind_state = GAME_STATE_BIND_STATE_NONE;
  }
  if (is_instance_equipped(state, instance_id)) {
    return true;
  }
  return list_add_unique(state->inventory_bag_order,
                         &state->inventory_bag_order_count,
                         GAME_STATE_MAX_INVENTORY_BAG_ORDER, instance_id);
}

static game_action_gear_slot_t
gear_slot_from_item(const game_item_definition_t *item) {
  if (!item) {
    return GAME_ACTION_GEAR_SLOT_NONE;
  }
  switch (item->slot) {
  case GAME_ITEM_SLOT_WEAPON:
    return GAME_ACTION_GEAR_SLOT_WEAPON;
  case GAME_ITEM_SLOT_OFFHAND:
    return GAME_ACTION_GEAR_SLOT_OFFHAND;
  case GAME_ITEM_SLOT_HEAD:
    return GAME_ACTION_GEAR_SLOT_HEAD;
  case GAME_ITEM_SLOT_ARMOUR:
    return GAME_ACTION_GEAR_SLOT_ARMOUR;
  case GAME_ITEM_SLOT_HANDS:
    return GAME_ACTION_GEAR_SLOT_HANDS;
  case GAME_ITEM_SLOT_WAIST:
    return GAME_ACTION_GEAR_SLOT_WAIST;
  case GAME_ITEM_SLOT_LEGS:
    return GAME_ACTION_GEAR_SLOT_LEGS;
  case GAME_ITEM_SLOT_FEET:
    return GAME_ACTION_GEAR_SLOT_FEET;
  case GAME_ITEM_SLOT_NECK:
    return GAME_ACTION_GEAR_SLOT_NECK;
  case GAME_ITEM_SLOT_RING_LEFT:
    return GAME_ACTION_GEAR_SLOT_RING_LEFT;
  case GAME_ITEM_SLOT_RING_RIGHT:
    return GAME_ACTION_GEAR_SLOT_RING_RIGHT;
  case GAME_ITEM_SLOT_RELIC:
    return GAME_ACTION_GEAR_SLOT_RELIC;
  case GAME_ITEM_SLOT_NONE:
  default:
    return GAME_ACTION_GEAR_SLOT_NONE;
  }
}

static bool grant_stack_item(GameState *state,
                             const game_item_definition_t *item, int count) {
  if (!state || !item || !item->id || count <= 0) {
    return false;
  }
  const int max_stack = item->stackable ? item->max_stack : 1;
  if (max_stack <= 0) {
    return false;
  }
  GameStackInstance *stack = find_stack(state, item->id);
  if (!stack) {
    stack = alloc_stack(state);
    if (!stack) {
      return false;
    }
    *stack = (GameStackInstance){0};
    stack->used = true;
    if (!copy_id(stack->key, item->id) || !copy_id(stack->def_id, item->id)) {
      *stack = (GameStackInstance){0};
      return false;
    }
  }
  if (stack->count > max_stack - count) {
    return false;
  }
  stack->count += count;
  return true;
}

static bool grant_gear_item(GameState *state,
                            const game_item_definition_t *item, int count) {
  if (!state || !item || !item->id || count <= 0) {
    return false;
  }
  const game_action_gear_slot_t slot = gear_slot_from_item(item);
  if (slot == GAME_ACTION_GEAR_SLOT_NONE) {
    return false;
  }
  for (int n = 0; n < count; ++n) {
    char instance_id[GAME_STATE_STRING_MAX];
    bool granted = false;
    for (int index = 1; index <= GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES;
         ++index) {
      if (snprintf(instance_id, sizeof instance_id, "gear_%s_%03d", item->id,
                   index) >= (int)sizeof instance_id) {
        return false;
      }
      if (find_gear(state, instance_id)) {
        continue;
      }
      if (!game_actions_grant_gear(state, instance_id, item->id, slot)) {
        return false;
      }
      granted = true;
      break;
    }
    if (!granted) {
      return false;
    }
  }
  return true;
}

bool game_actions_equip_gear(GameState *state, const char *instance_id) {
  if (!state || !instance_id) {
    return false;
  }
  GameState next = *state;
  const bool live_state = state == &g_game_state;
  GameGearInstance *gear = find_gear(&next, instance_id);
  if (!gear) {
    return false;
  }
  const game_item_definition_t *item = game_content_find_item(gear->def_id);
  if (!item || item->kind != GAME_ITEM_KIND_GEAR) {
    return false;
  }
  const game_action_gear_slot_t slot = gear_slot_from_item(item);
  if (slot == GAME_ACTION_GEAR_SLOT_NONE) {
    return false;
  }
  char previous_instance_id[GAME_STATE_STRING_MAX] = {0};
  const char *previous = equipped_instance_for_slot(&next, slot);
  if (previous && strcmp(previous, instance_id) != 0 &&
      !copy_id(previous_instance_id, previous)) {
    return false;
  }
  if (!set_slot(&next, slot, instance_id)) {
    return false;
  }
  if (!list_remove(next.inventory_bag_order, &next.inventory_bag_order_count,
                   instance_id)) {
    return false;
  }
  if (previous_instance_id[0] != '\0' &&
      !list_add_unique(next.inventory_bag_order,
                       &next.inventory_bag_order_count,
                       GAME_STATE_MAX_INVENTORY_BAG_ORDER,
                       previous_instance_id)) {
    return false;
  }

  if (next.has_quests_tracked_quest_id) {
    GameQuestState *quest = find_quest(&next, next.quests_tracked_quest_id);
    const game_quest_equip_step_t *step =
        game_content_find_equip_step(next.quests_tracked_quest_id, item->id);
    if (quest && quest->status == GAME_STATE_QUEST_STATUS_ACTIVE &&
        quest->has_current_step_id && step &&
        strcmp(quest->current_step_id, step->step_id) == 0 &&
        !list_contains(next.quests_completed_step_ids,
                       next.quests_completed_step_ids_count, step->step_id)) {
      const char *next_step_id =
          game_content_next_quest_step(step->quest_id, step->step_id);
      char reason[GAME_STATE_STRING_MAX];
      if (snprintf(reason, sizeof reason, "equip.%s", item->id) >=
          (int)sizeof reason) {
        return false;
      }
      if (!game_actions_complete_step(&next, step->quest_id, step->step_id,
                                      next_step_id, reason)) {
        return false;
      }
      if (step->complete_flag_id &&
          !game_actions_set_flag(&next, step->complete_flag_id)) {
        return false;
      }
      if (step->unlock_id &&
          !list_add_unique(next.unlocks_ids, &next.unlocks_ids_count,
                           GAME_STATE_MAX_UNLOCKS_IDS, step->unlock_id)) {
        return false;
      }
    }
  }

  *state = next;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}

bool game_actions_unequip_gear(GameState *state, const char *instance_id) {
  if (!state || !instance_id || instance_id[0] == '\0') {
    return false;
  }
  GameState next = *state;
  const bool live_state = state == &g_game_state;
  GameGearInstance *gear = find_gear(&next, instance_id);
  if (!gear) {
    return false;
  }
  const game_item_definition_t *item = game_content_find_item(gear->def_id);
  if (!item || item->kind != GAME_ITEM_KIND_GEAR) {
    return false;
  }
  const game_action_gear_slot_t slot = gear_slot_from_item(item);
  if (slot == GAME_ACTION_GEAR_SLOT_NONE) {
    return false;
  }
  const char *equipped = equipped_instance_for_slot(&next, slot);
  if (!equipped || strcmp(equipped, instance_id) != 0) {
    return false;
  }
  if (!list_add_unique(next.inventory_bag_order, &next.inventory_bag_order_count,
                       GAME_STATE_MAX_INVENTORY_BAG_ORDER, instance_id)) {
    return false;
  }
  clear_slot(&next, slot);
  *state = next;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}

bool game_actions_location_unlocked(const GameState *state,
                                    const char *location_id) {
  if (!state || !location_id) {
    return false;
  }
  const game_location_definition_t *target =
      game_content_find_location(location_id);
  return target && location_unlocked(state, target);
}

bool game_actions_can_move_location(const GameState *state,
                                    const char *location_id) {
  if (!state || !location_id) {
    return false;
  }
  const game_location_definition_t *target =
      game_content_find_location(location_id);
  if (!target || !location_unlocked(state, target)) {
    return false;
  }
  if (strcmp(state->world_current_location_id, location_id) != 0) {
    const game_location_definition_t *from =
        game_content_find_location(state->world_current_location_id);
    if (!can_exit_to_location(state, from, location_id)) {
      return false;
    }
  }
  return true;
}

bool game_actions_location_object_available(
    const GameState *state, const game_location_object_t *object) {
  return game_actions_select_location_interaction(state, object) != NULL;
}

bool game_actions_location_object_visible(
    const GameState *state, const game_location_object_t *object) {
  return state && object &&
         location_requirements_met(state, object->requirements,
                                   object->requirement_count);
}

const game_location_interaction_t *
game_actions_select_location_interaction(const GameState *state,
                                         const game_location_object_t *object) {
  if (!state || !object ||
      !location_requirements_met(state, object->requirements,
                                 object->requirement_count)) {
    return NULL;
  }
  if (object->interaction_count <= 0 || !object->interactions) {
    return NULL;
  }
  for (int i = 0; i < object->interaction_count; ++i) {
    const game_location_interaction_t *interaction = &object->interactions[i];
    if (location_requirements_met(state, interaction->requirements,
                                  interaction->requirement_count)) {
      return interaction;
    }
  }
  return NULL;
}

bool game_actions_move_location(GameState *state, const char *location_id) {
  if (!state || !location_id ||
      !game_actions_can_move_location(state, location_id)) {
    return false;
  }
  GameState next = *state;
  const bool live_state = state == &g_game_state;
  if (!copy_id(next.world_current_location_id, location_id)) {
    return false;
  }
  if (!list_add_unique(next.world_visited_location_ids,
                       &next.world_visited_location_ids_count,
                       GAME_STATE_MAX_WORLD_VISITED_LOCATION_IDS,
                       location_id)) {
    return false;
  }
  if (next.has_quests_tracked_quest_id) {
    GameQuestState *quest = find_quest(&next, next.quests_tracked_quest_id);
    const game_quest_visit_step_t *step =
        game_content_find_visit_step(next.quests_tracked_quest_id, location_id);
    if (quest && quest->status == GAME_STATE_QUEST_STATUS_ACTIVE &&
        quest->has_current_step_id && step &&
        strcmp(quest->current_step_id, step->step_id) == 0 &&
        !list_contains(next.quests_completed_step_ids,
                       next.quests_completed_step_ids_count, step->step_id)) {
      const char *next_step_id =
          game_content_next_quest_step(step->quest_id, step->step_id);
      char reason[GAME_STATE_STRING_MAX];
      if (snprintf(reason, sizeof reason, "visit.%s", location_id) >=
          (int)sizeof reason) {
        return false;
      }
      if (!game_actions_complete_step(&next, step->quest_id, step->step_id,
                                      next_step_id, reason)) {
        return false;
      }
    }
  }

  *state = next;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}

bool game_actions_inspect_object(GameState *state, const char *object_id) {
  if (!state || !object_id) {
    return false;
  }
  GameState next = *state;
  const bool live_state = state == &g_game_state;
  if (!next.has_quests_tracked_quest_id) {
    return false;
  }
  GameQuestState *quest = find_quest(&next, next.quests_tracked_quest_id);
  const game_quest_inspect_step_t *step =
      game_content_find_inspect_step(next.quests_tracked_quest_id, object_id);
  if (!quest || quest->status != GAME_STATE_QUEST_STATUS_ACTIVE ||
      !quest->has_current_step_id || !step ||
      strcmp(quest->current_step_id, step->step_id) != 0 ||
      list_contains(next.quests_completed_step_ids,
                    next.quests_completed_step_ids_count, step->step_id)) {
    return false;
  }
  const char *next_step_id =
      game_content_next_quest_step(step->quest_id, step->step_id);
  char reason[GAME_STATE_STRING_MAX];
  if (snprintf(reason, sizeof reason, "inspect.%s", object_id) >=
      (int)sizeof reason) {
    return false;
  }
  if (!game_actions_complete_step(&next, step->quest_id, step->step_id,
                                  next_step_id, reason)) {
    return false;
  }

  *state = next;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}

static bool game_actions_grant_item(GameState *state, const char *item_id,
                                    int count) {
  const game_item_definition_t *item = game_content_find_item(item_id);
  if (!state || !item || count <= 0) {
    return false;
  }
  if (item->kind == GAME_ITEM_KIND_GEAR) {
    return grant_gear_item(state, item, count);
  }
  if (item->stackable || item->kind == GAME_ITEM_KIND_QUEST_ITEM ||
      item->kind == GAME_ITEM_KIND_CLUE) {
    return grant_stack_item(state, item, count);
  }
  return false;
}

bool game_actions_start_quest(GameState *state, const char *quest_id,
                              const char *current_step_id, const char *reason) {
  GameQuestState *quest = find_or_alloc_quest(state, quest_id);
  if (!quest || !current_step_id) {
    return false;
  }
  quest->status = GAME_STATE_QUEST_STATUS_ACTIVE;
  quest->has_current_step_id = true;
  if (!copy_id(quest->current_step_id, current_step_id)) {
    return false;
  }
  quest->objective_progress = 0;
  quest->has_last_update_reason = reason && reason[0] != '\0';
  if (quest->has_last_update_reason &&
      !copy_id(quest->last_update_reason, reason)) {
    return false;
  }
  state->has_quests_tracked_quest_id = true;
  return copy_id(state->quests_tracked_quest_id, quest_id);
}

bool game_actions_complete_step(GameState *state, const char *quest_id,
                                const char *step_id, const char *next_step_id,
                                const char *reason) {
  if (!state || !step_id) {
    return false;
  }
  GameQuestState *quest = find_or_alloc_quest(state, quest_id);
  if (!quest) {
    return false;
  }
  if (!list_add_unique(state->quests_completed_step_ids,
                       &state->quests_completed_step_ids_count,
                       GAME_STATE_MAX_QUESTS_COMPLETED_STEP_IDS, step_id)) {
    return false;
  }
  quest->status = next_step_id ? GAME_STATE_QUEST_STATUS_ACTIVE
                               : GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN;
  quest->has_current_step_id = next_step_id && next_step_id[0] != '\0';
  if (quest->has_current_step_id &&
      !copy_id(quest->current_step_id, next_step_id)) {
    return false;
  }
  quest->has_last_update_reason = reason && reason[0] != '\0';
  if (quest->has_last_update_reason &&
      !copy_id(quest->last_update_reason, reason)) {
    return false;
  }
  return true;
}

bool game_actions_complete_quest(GameState *state, const char *quest_id,
                                 const char *reason) {
  if (!state || !quest_id) {
    return false;
  }
  GameQuestState *quest = find_quest(state, quest_id);
  if (!quest) {
    return false;
  }
  if (quest->status == GAME_STATE_QUEST_STATUS_COMPLETED) {
    return true;
  }
  if (quest->status != GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN &&
      quest->status != GAME_STATE_QUEST_STATUS_ACTIVE) {
    return false;
  }
  quest->status = GAME_STATE_QUEST_STATUS_COMPLETED;
  quest->has_current_step_id = false;
  quest->current_step_id[0] = '\0';
  quest->has_last_update_reason = reason && reason[0] != '\0';
  if (quest->has_last_update_reason &&
      !copy_id(quest->last_update_reason, reason)) {
    return false;
  }
  return true;
}

bool game_actions_claim_reward_once(GameState *state, const char *reward_id) {
  return state &&
         list_add_unique(state->quests_claimed_reward_ids,
                         &state->quests_claimed_reward_ids_count,
                         GAME_STATE_MAX_QUESTS_CLAIMED_REWARD_IDS, reward_id);
}

bool game_actions_record_choice(GameState *state, const char *choice_id) {
  return state && list_add_unique(state->quests_choice_ids,
                                  &state->quests_choice_ids_count,
                                  GAME_STATE_MAX_QUESTS_CHOICE_IDS, choice_id);
}

bool game_actions_set_flag(GameState *state, const char *flag_id) {
  return state && list_add_unique(state->flags_ids, &state->flags_ids_count,
                                  GAME_STATE_MAX_FLAGS_IDS, flag_id);
}

bool game_actions_has_flag(const GameState *state, const char *flag_id) {
  return state && flag_id &&
         list_contains(state->flags_ids, state->flags_ids_count, flag_id);
}

bool game_actions_restore_hp(GameState *state) {
  if (!state) {
    return false;
  }
  game_combat_stats_t player_stats;
  if (!game_combat_build_player_stats(state, &player_stats) ||
      player_stats.vitality <= 0) {
    return false;
  }
  GameState next = *state;
  const bool live_state = state == &g_game_state;
  next.hero_hp = player_stats.vitality;
  *state = next;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}

static const game_shop_item_t *
find_shop_item(const game_shop_definition_t *shop, const char *item_id) {
  if (!shop || !item_id) {
    return NULL;
  }
  for (int i = 0; i < shop->item_count; ++i) {
    if (shop->items[i].item_id && strcmp(shop->items[i].item_id, item_id) == 0) {
      return &shop->items[i];
    }
  }
  return NULL;
}

bool game_actions_shop_item_available(const GameState *state,
                                      const game_shop_item_t *shop_item) {
  return state && shop_item && shop_item->item_id &&
         game_content_find_item(shop_item->item_id) &&
         shop_requirements_met(state, shop_item->requirements,
                               shop_item->requirement_count);
}

bool game_actions_can_purchase_shop_item(
    const GameState *state, const game_shop_definition_t *shop,
    const game_shop_item_t *shop_item) {
  (void)shop;
  return state && shop_item && shop_item->price_gold >= 0 &&
         game_actions_shop_item_available(state, shop_item) &&
         state->wallet_gold >= shop_item->price_gold;
}

bool game_actions_purchase_shop_item(GameState *state, const char *shop_id,
                                     const char *item_id) {
  if (!state || !shop_id || !item_id) {
    return false;
  }
  const game_shop_definition_t *shop = game_content_find_shop(shop_id);
  const game_shop_item_t *shop_item = find_shop_item(shop, item_id);
  if (!game_actions_can_purchase_shop_item(state, shop, shop_item)) {
    return false;
  }

  GameState next = *state;
  const bool live_state = state == &g_game_state;
  next.wallet_gold -= shop_item->price_gold;
  if (!game_actions_grant_item(&next, shop_item->item_id, 1)) {
    return false;
  }

  *state = next;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}

void game_actions_shop_buyback_init(game_shop_buyback_t *buyback) {
  if (buyback) {
    *buyback = (game_shop_buyback_t){0};
  }
}

int game_actions_item_sell_price(const game_item_definition_t *item) {
  if (!item || !item->sellable || item->price_gold <= 0) {
    return 0;
  }
  const int price = item->price_gold / 2;
  return price > 0 ? price : 1;
}

bool game_actions_can_sell_inventory_item(const GameState *state,
                                          const char *instance_id,
                                          int *out_price_gold) {
  if (out_price_gold) {
    *out_price_gold = 0;
  }
  if (!state || !instance_id || instance_id[0] == '\0' ||
      !bag_contains_instance(state, instance_id) ||
      is_instance_equipped(state, instance_id)) {
    return false;
  }
  const GameGearInstance *gear = find_gear_const(state, instance_id);
  if (!gear || !gear->def_id[0]) {
    return false;
  }
  const game_item_definition_t *item = game_content_find_item(gear->def_id);
  if (!item || item->kind != GAME_ITEM_KIND_GEAR) {
    return false;
  }
  const int price = game_actions_item_sell_price(item);
  if (price <= 0) {
    return false;
  }
  if (out_price_gold) {
    *out_price_gold = price;
  }
  return true;
}

static int buyback_find_index(const game_shop_buyback_t *buyback,
                              const char *entry_id) {
  if (!buyback || !entry_id || entry_id[0] == '\0') {
    return -1;
  }
  for (int i = 0; i < buyback->count; ++i) {
    if (buyback->entries[i].used &&
        strcmp(buyback->entries[i].entry_id, entry_id) == 0) {
      return i;
    }
  }
  return -1;
}

static bool buyback_append(game_shop_buyback_t *buyback,
                           const game_shop_buyback_entry_t *entry) {
  if (!buyback || !entry || !entry->used || entry->entry_id[0] == '\0' ||
      !entry->gear.used || entry->gear.key[0] == '\0' ||
      entry->gear.def_id[0] == '\0' || entry->price_gold <= 0 ||
      buyback->count < 0 || buyback->count > GAME_ACTIONS_SHOP_BUYBACK_MAX) {
    return false;
  }
  if (buyback->count == GAME_ACTIONS_SHOP_BUYBACK_MAX) {
    for (int i = 1; i < GAME_ACTIONS_SHOP_BUYBACK_MAX; ++i) {
      buyback->entries[i - 1] = buyback->entries[i];
    }
    buyback->count -= 1;
  }
  buyback->entries[buyback->count] = *entry;
  buyback->count += 1;
  return true;
}

static bool buyback_remove_at(game_shop_buyback_t *buyback, int index) {
  if (!buyback || index < 0 || index >= buyback->count) {
    return false;
  }
  for (int i = index + 1; i < buyback->count; ++i) {
    buyback->entries[i - 1] = buyback->entries[i];
  }
  buyback->count -= 1;
  if (buyback->count >= 0 &&
      buyback->count < GAME_ACTIONS_SHOP_BUYBACK_MAX) {
    buyback->entries[buyback->count] = (game_shop_buyback_entry_t){0};
  }
  return true;
}

bool game_actions_sell_inventory_item(GameState *state,
                                      const char *instance_id,
                                      game_shop_buyback_t *buyback) {
  int price = 0;
  if (!buyback ||
      !game_actions_can_sell_inventory_item(state, instance_id, &price)) {
    return false;
  }
  GameState next = *state;
  const bool live_state = state == &g_game_state;
  GameGearInstance *gear = find_gear(&next, instance_id);
  if (!gear || next.wallet_gold > INT_MAX - price) {
    return false;
  }

  game_shop_buyback_entry_t entry = {0};
  entry.used = true;
  entry.price_gold = price;
  entry.gear = *gear;
  if (!copy_id(entry.entry_id, instance_id)) {
    return false;
  }

  game_shop_buyback_t next_buyback = *buyback;
  if (!buyback_append(&next_buyback, &entry)) {
    return false;
  }
  if (!list_remove(next.inventory_bag_order, &next.inventory_bag_order_count,
                   instance_id)) {
    return false;
  }
  *gear = (GameGearInstance){0};
  next.wallet_gold += price;

  *state = next;
  *buyback = next_buyback;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}

bool game_actions_can_rebuy_inventory_item(const GameState *state,
                                           const game_shop_buyback_t *buyback,
                                           const char *entry_id,
                                           int *out_price_gold) {
  if (out_price_gold) {
    *out_price_gold = 0;
  }
  const int index = buyback_find_index(buyback, entry_id);
  if (!state || index < 0) {
    return false;
  }
  const game_shop_buyback_entry_t *entry = &buyback->entries[index];
  if (!entry->gear.used || entry->gear.key[0] == '\0' ||
      entry->gear.def_id[0] == '\0' || entry->price_gold <= 0 ||
      state->wallet_gold < entry->price_gold ||
      state->inventory_bag_order_count >= GAME_STATE_MAX_INVENTORY_BAG_ORDER ||
      find_gear_const(state, entry->gear.key) ||
      bag_contains_instance(state, entry->gear.key) ||
      !game_content_find_item(entry->gear.def_id)) {
    return false;
  }
  if (out_price_gold) {
    *out_price_gold = entry->price_gold;
  }
  return true;
}

bool game_actions_rebuy_inventory_item(GameState *state,
                                       game_shop_buyback_t *buyback,
                                       const char *entry_id) {
  int price = 0;
  const int index = buyback_find_index(buyback, entry_id);
  if (index < 0 ||
      !game_actions_can_rebuy_inventory_item(state, buyback, entry_id,
                                             &price)) {
    return false;
  }

  GameState next = *state;
  const bool live_state = state == &g_game_state;
  GameGearInstance *slot = alloc_gear(&next);
  if (!slot) {
    return false;
  }
  const game_shop_buyback_entry_t entry = buyback->entries[index];
  next.wallet_gold -= price;
  *slot = entry.gear;
  slot->used = true;
  if (!list_add_unique(next.inventory_bag_order, &next.inventory_bag_order_count,
                       GAME_STATE_MAX_INVENTORY_BAG_ORDER, slot->key)) {
    return false;
  }

  game_shop_buyback_t next_buyback = *buyback;
  if (!buyback_remove_at(&next_buyback, index)) {
    return false;
  }

  *state = next;
  *buyback = next_buyback;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}

static unsigned int encounter_seed(const char *encounter_id) {
  unsigned int hash = 2166136261U;
  if (!encounter_id) {
    return hash;
  }
  for (const unsigned char *p = (const unsigned char *)encounter_id; *p; ++p) {
    hash ^= (unsigned int)*p;
    hash *= 16777619U;
  }
  return hash;
}

bool game_actions_resolve_encounter(GameState *state, const char *encounter_id,
                                    game_combat_result_t *out_result) {
  if (!state || !encounter_id || !out_result) {
    return false;
  }
  const game_encounter_definition_t *encounter =
      game_content_find_encounter(encounter_id);
  if (!encounter) {
    return false;
  }
  GameState next = *state;
  const bool live_state = state == &g_game_state;
  game_combat_stats_t player_stats;
  if (!game_combat_build_player_stats(&next, &player_stats)) {
    return false;
  }
  game_combat_result_t result;
  if (!game_combat_simulate(&player_stats, next.hero_hp, encounter,
                            encounter_seed(encounter_id), &result)) {
    return false;
  }

  next.hero_hp = player_stats.vitality;
  if (result.outcome == GAME_COMBAT_OUTCOME_LOSS) {
    if (!copy_id(next.world_current_location_id, "hub_last_post")) {
      return false;
    }
    if (!list_add_unique(next.world_visited_location_ids,
                         &next.world_visited_location_ids_count,
                         GAME_STATE_MAX_WORLD_VISITED_LOCATION_IDS,
                         "hub_last_post")) {
      return false;
    }
  }

  if (result.outcome == GAME_COMBAT_OUTCOME_WIN) {
    char reward_id[GAME_STATE_STRING_MAX];
    if (snprintf(reward_id, sizeof reward_id, "encounter.%s.win",
                 encounter_id) >= (int)sizeof reward_id) {
      return false;
    }
    // Repeatable farm encounters pay gold/xp on every win and are never
    // recorded as claimed; unique item rewards stay one-shot only.
    const bool already_claimed =
        !encounter->repeatable &&
        list_contains(next.quests_claimed_reward_ids,
                      next.quests_claimed_reward_ids_count, reward_id);
    if (!already_claimed) {
      if (encounter->reward_xp > 0) {
        if (next.hero_xp > 999999 - encounter->reward_xp) {
          return false;
        }
        next.hero_xp += encounter->reward_xp;
      }
      if (encounter->reward_gold > 0) {
        if (next.wallet_gold > 2147483647 - encounter->reward_gold) {
          return false;
        }
        next.wallet_gold += encounter->reward_gold;
      }
      if (!encounter->repeatable) {
        for (int i = 0; i < encounter->reward_item_count; ++i) {
          if (!game_actions_grant_item(&next, encounter->reward_items[i], 1)) {
            return false;
          }
        }
        if (!game_actions_claim_reward_once(&next, reward_id)) {
          return false;
        }
      }
      result.reward_granted = true;
    }

    const game_quest_encounter_step_t *step = NULL;
    GameQuestState *quest = NULL;
    const char *tracked =
        next.has_quests_tracked_quest_id ? next.quests_tracked_quest_id : NULL;
    if (tracked) {
      step = game_content_find_encounter_step(tracked, encounter_id);
      quest = find_quest(&next, tracked);
    }
    if (!step || !quest || quest->status != GAME_STATE_QUEST_STATUS_ACTIVE ||
        !quest->has_current_step_id ||
        strcmp(quest->current_step_id, step->step_id) != 0 ||
        list_contains(next.quests_completed_step_ids,
                      next.quests_completed_step_ids_count, step->step_id)) {
      quest = find_active_encounter_quest(&next, encounter_id, &step);
    }
    if (step && quest && quest->status == GAME_STATE_QUEST_STATUS_ACTIVE &&
        quest->has_current_step_id &&
        strcmp(quest->current_step_id, step->step_id) == 0 &&
        !list_contains(next.quests_completed_step_ids,
                       next.quests_completed_step_ids_count, step->step_id)) {
      const char *next_step_id =
          game_content_next_quest_step(step->quest_id, step->step_id);
      char reason[GAME_STATE_STRING_MAX];
      if (snprintf(reason, sizeof reason, "encounter.%s", encounter_id) >=
          (int)sizeof reason) {
        return false;
      }
      if (!game_actions_complete_step(&next, step->quest_id, step->step_id,
                                      next_step_id, reason)) {
        return false;
      }
      if (step->complete_flag_id &&
          !game_actions_set_flag(&next, step->complete_flag_id)) {
        return false;
      }
      if (step->unlock_id &&
          !list_add_unique(next.unlocks_ids, &next.unlocks_ids_count,
                           GAME_STATE_MAX_UNLOCKS_IDS, step->unlock_id)) {
        return false;
      }
    }
  }

  *state = next;
  *out_result = result;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}

bool game_actions_apply_dialogue_choice(GameState *state,
                                        const char *dialogue_id,
                                        const char *choice_id,
                                        const char *authored_reward_id,
                                        const dialogue_effect_t *effects,
                                        int effect_count) {
  if (!state || !dialogue_id || !choice_id || effect_count < 0 ||
      (effect_count > 0 && !effects)) {
    return false;
  }
  GameState next = *state;
  const bool live_state = state == &g_game_state;
  char choice_record_id[GAME_STATE_STRING_MAX];
  char fallback_reward_id[GAME_STATE_STRING_MAX];
  if (snprintf(choice_record_id, sizeof choice_record_id, "%s.%s", dialogue_id,
               choice_id) >= (int)sizeof choice_record_id) {
    return false;
  }
  bool has_grant_effect = false;
  for (int i = 0; i < effect_count; ++i) {
    if (effects[i].kind == DIALOGUE_EFFECT_GRANT_ITEM ||
        effects[i].kind == DIALOGUE_EFFECT_GRANT_XP ||
        effects[i].kind == DIALOGUE_EFFECT_GRANT_GOLD) {
      has_grant_effect = true;
      break;
    }
  }
  const char *reward_id = authored_reward_id;
  if ((!reward_id || reward_id[0] == '\0') && has_grant_effect) {
    if (snprintf(fallback_reward_id, sizeof fallback_reward_id, "%s.immediate",
                 choice_record_id) >= (int)sizeof fallback_reward_id) {
      return false;
    }
    reward_id = fallback_reward_id;
  }
  const bool already_claimed =
      reward_id &&
      list_contains(next.quests_claimed_reward_ids,
                    next.quests_claimed_reward_ids_count, reward_id);
  if (!game_actions_record_choice(&next, choice_record_id)) {
    return false;
  }
  for (int i = 0; i < effect_count; ++i) {
    const dialogue_effect_t *effect = &effects[i];
    if (effect->kind == DIALOGUE_EFFECT_GRANT_ITEM) {
      if (!already_claimed &&
          !game_actions_grant_item(&next, effect->item_id, effect->count)) {
        return false;
      }
      continue;
    }
    if (effect->kind == DIALOGUE_EFFECT_GRANT_XP) {
      if (!already_claimed) {
        if (effect->count < 0 || next.hero_xp > 999999 - effect->count) {
          return false;
        }
        next.hero_xp += effect->count;
      }
      continue;
    }
    if (effect->kind == DIALOGUE_EFFECT_GRANT_GOLD) {
      if (!already_claimed) {
        if (effect->count < 0 ||
            next.wallet_gold > 2147483647 - effect->count) {
          return false;
        }
        next.wallet_gold += effect->count;
      }
      continue;
    }
    if (effect->kind == DIALOGUE_EFFECT_SET_FLAG) {
      if (!game_actions_set_flag(&next, effect->flag_id)) {
        return false;
      }
      continue;
    }
    if (effect->kind == DIALOGUE_EFFECT_ADVANCE_QUEST) {
      if (!effect->quest_id || !effect->step_id) {
        return false;
      }
      if (!list_contains(next.quests_completed_step_ids,
                         next.quests_completed_step_ids_count,
                         effect->step_id)) {
        const char *next_step_id =
            game_content_next_quest_step(effect->quest_id, effect->step_id);
        if (!game_actions_start_quest(&next, effect->quest_id,
                                      next_step_id ? next_step_id
                                                   : effect->step_id,
                                      choice_record_id) ||
            !game_actions_complete_step(&next, effect->quest_id,
                                        effect->step_id, next_step_id,
                                        choice_record_id)) {
          return false;
        }
      }
      continue;
    }
    if (effect->kind == DIALOGUE_EFFECT_COMPLETE_QUEST) {
      if (!game_actions_complete_quest(&next, effect->quest_id,
                                       choice_record_id)) {
        return false;
      }
      continue;
    }
    return false;
  }
  if (has_grant_effect && (!reward_id || reward_id[0] == '\0')) {
    return false;
  }
  if (has_grant_effect && !already_claimed &&
      !game_actions_claim_reward_once(&next, reward_id)) {
    return false;
  }
  *state = next;
  if (live_state) {
    game_state_mark_dirty();
  }
  return true;
}
