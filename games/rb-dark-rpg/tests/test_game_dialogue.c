#include "game_actions.h"
#include "game_content.h"
#include "game_dialogue.h"
#include "game_state.h"
#include "world/world.h"

#include <assert.h>
#include <string.h>

static int gear_count(const GameState *state) {
  int count = 0;
  for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
    if (state->inventory_gear_instances[i].used) {
      count += 1;
    }
  }
  return count;
}

static const GameQuestState *find_quest(const GameState *state,
                                        const char *quest_id) {
  for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
    const GameQuestState *quest = &state->quests_quest_states[i];
    if (quest->used && strcmp(quest->key, quest_id) == 0) {
      return quest;
    }
  }
  return 0;
}

static bool has_flag(const GameState *state, const char *flag_id) {
  for (int i = 0; i < state->flags_ids_count; ++i) {
    if (strcmp(state->flags_ids[i], flag_id) == 0) {
      return true;
    }
  }
  return false;
}

static const game_location_object_t *
find_location_object(const game_location_definition_t *location,
                     const char *object_id) {
  if (!location || !object_id) {
    return 0;
  }
  for (int i = 0; i < location->object_count; ++i) {
    if (strcmp(location->objects[i].id, object_id) == 0) {
      return &location->objects[i];
    }
  }
  return 0;
}

static void assert_starter_equipment_empty(const GameState *state) {
  assert(!state->has_equipment_weapon_instance_id);
  assert(!state->has_equipment_offhand_instance_id);
  assert(!state->has_equipment_head_instance_id);
  assert(!state->has_equipment_armour_instance_id);
  assert(!state->has_equipment_hands_instance_id);
  assert(!state->has_equipment_waist_instance_id);
  assert(!state->has_equipment_legs_instance_id);
  assert(!state->has_equipment_feet_instance_id);
  assert(!state->has_equipment_neck_instance_id);
  assert(!state->has_equipment_ring_left_instance_id);
  assert(!state->has_equipment_ring_right_instance_id);
  assert(!state->has_equipment_charm_instance_id);
}

static void assert_starter_equipment_filled(const GameState *state) {
  assert(state->has_equipment_weapon_instance_id);
  assert(strcmp(state->equipment_weapon_instance_id, "gear_old_sword_001") ==
         0);
  assert(state->has_equipment_armour_instance_id);
  assert(strcmp(state->equipment_armour_instance_id,
                "gear_padded_jacket_001") == 0);
  assert(state->has_equipment_legs_instance_id);
  assert(strcmp(state->equipment_legs_instance_id,
                "gear_leather_greaves_001") == 0);
  assert(!state->has_equipment_offhand_instance_id);
  assert(!state->has_equipment_head_instance_id);
  assert(!state->has_equipment_hands_instance_id);
  assert(!state->has_equipment_waist_instance_id);
  assert(!state->has_equipment_feet_instance_id);
  assert(!state->has_equipment_neck_instance_id);
  assert(!state->has_equipment_ring_left_instance_id);
  assert(!state->has_equipment_ring_right_instance_id);
  assert(!state->has_equipment_charm_instance_id);
}

static void assert_gate_accept_state(const GameState *state) {
  assert(gear_count(state) == 3);
  assert(state->inventory_bag_order_count == 3);
  assert(strcmp(state->inventory_bag_order[0], "gear_old_sword_001") == 0);
  assert(strcmp(state->inventory_bag_order[1], "gear_padded_jacket_001") == 0);
  assert(strcmp(state->inventory_bag_order[2], "gear_leather_greaves_001") ==
         0);
  assert_starter_equipment_empty(state);
  assert(state->quests_completed_step_ids_count == 1);
  assert(strcmp(state->quests_completed_step_ids[0], "talk_gate_guard") == 0);
  assert(state->quests_claimed_reward_ids_count == 1);
  assert(strcmp(state->quests_claimed_reward_ids[0],
                "dlg_gate_guard_intro.accept.immediate") == 0);
  assert(state->quests_choice_ids_count == 1);
  assert(strcmp(state->quests_choice_ids[0], "dlg_gate_guard_intro.accept") ==
         0);
  assert(has_flag(state, "gate_guard_intro_seen"));
  assert(has_flag(state, "starter_gear_received"));
  assert(state->has_quests_tracked_quest_id);
  assert(strcmp(state->quests_tracked_quest_id, "q001_gate_pass") == 0);

  const GameQuestState *quest = find_quest(state, "q001_gate_pass");
  assert(quest != 0);
  assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
  assert(quest->has_current_step_id);
  assert(strcmp(quest->current_step_id, "equip_old_sword") == 0);
}

static void assert_gate_equipped_state(const GameState *state) {
  assert(gear_count(state) == 3);
  assert(state->inventory_bag_order_count == 0);
  assert_starter_equipment_filled(state);
  assert(state->quests_completed_step_ids_count == 4);
  assert(strcmp(state->quests_completed_step_ids[0], "talk_gate_guard") == 0);
  assert(strcmp(state->quests_completed_step_ids[1], "equip_old_sword") == 0);
  assert(strcmp(state->quests_completed_step_ids[2], "equip_padded_jacket") ==
         0);
  assert(strcmp(state->quests_completed_step_ids[3], "equip_leather_greaves") ==
         0);
  assert(has_flag(state, "old_sword_equipped"));
  assert(has_flag(state, "padded_jacket_equipped"));
  assert(has_flag(state, "leather_greaves_equipped"));

  const GameQuestState *quest = find_quest(state, "q001_gate_pass");
  assert(quest != 0);
  assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
  assert(quest->has_current_step_id);
  assert(strcmp(quest->current_step_id, "clear_gate_scavenger") == 0);
}

static void
assert_gate_reaccept_preserves_advanced_quest(const GameState *state) {
  assert(gear_count(state) == 3);
  assert_starter_equipment_filled(state);
  assert(state->quests_completed_step_ids_count == 4);
  assert(strcmp(state->quests_completed_step_ids[0], "talk_gate_guard") == 0);
  assert(strcmp(state->quests_completed_step_ids[1], "equip_old_sword") == 0);
  assert(strcmp(state->quests_completed_step_ids[2], "equip_padded_jacket") ==
         0);
  assert(strcmp(state->quests_completed_step_ids[3], "equip_leather_greaves") ==
         0);
  assert(state->quests_claimed_reward_ids_count == 1);
  assert(strcmp(state->quests_claimed_reward_ids[0],
                "dlg_gate_guard_intro.accept.immediate") == 0);
  assert(has_flag(state, "gate_guard_intro_seen"));
  assert(has_flag(state, "starter_gear_received"));

  const GameQuestState *quest = find_quest(state, "q001_gate_pass");
  assert(quest != 0);
  assert(quest->status == GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN);
  assert(!quest->has_current_step_id);
  assert(quest->objective_progress == 7);
}

static void advance_gate_quest_to_turn_in(GameState *state) {
  GameQuestState *quest = 0;
  for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
    if (state->quests_quest_states[i].used &&
        strcmp(state->quests_quest_states[i].key, "q001_gate_pass") == 0) {
      quest = &state->quests_quest_states[i];
      break;
    }
  }
  assert(quest != 0);
  quest->status = GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN;
  quest->has_current_step_id = false;
  quest->current_step_id[0] = '\0';
  quest->objective_progress = 7;
}

static void test_gate_dialogue_progression(void) {
  World w = {0};
  GameState state;
  game_state_init_defaults(&state);
  w.player_state = &state;
  game_dialogue_init(&w);

  assert(game_dialogue_open(&w, "dlg_gate_guard_intro"));
  assert(w.dialogue.open);
  assert(w.dialogue.current_node != 0);
  assert(strcmp(w.dialogue.current_node->id, "start") == 0);
  assert(w.dialogue.current_node->speaker_name &&
         strcmp(w.dialogue.current_node->speaker_name, "Страж у ворот") == 0);
  assert(w.dialogue.current_node->quest_name &&
         strcmp(w.dialogue.current_node->quest_name, "Допуск за ворота") == 0);
  assert(w.dialogue.definition != 0);
  assert(w.dialogue.definition->quest_preview != 0);
  assert(w.dialogue.definition->quest_preview->immediate_reward_count == 3);
  assert(w.dialogue.definition->quest_preview->completion_reward_count == 2);
  const dialogue_reward_t *first_reward =
      &w.dialogue.definition->quest_preview->immediate_rewards[0];
  assert(strcmp(first_reward->id, "old_sword") == 0);
  assert(first_reward->icon_asset_id != 0);
  assert(strcmp(first_reward->icon_asset_id, "asset_icon_old_sword") == 0);
  const dialogue_reward_t *completion_item =
      &w.dialogue.definition->quest_preview->completion_rewards[0];
  assert(strcmp(completion_item->id, "seeker_token") == 0);
  assert(completion_item->icon_asset_id != 0);
  assert(strcmp(completion_item->icon_asset_id, "asset_icon_seeker_token") ==
         0);
  const dialogue_reward_t *completion_xp =
      &w.dialogue.definition->quest_preview->completion_rewards[1];
  assert(completion_xp->kind == DIALOGUE_REWARD_XP);
  assert(completion_xp->amount == 12);
  assert(completion_xp->icon_asset_id == 0);
  const game_item_definition_t *old_sword =
      game_content_find_item("old_sword");
  assert(old_sword != 0);
  assert(old_sword->icon_asset_id != 0);
  assert(strcmp(old_sword->icon_asset_id, "asset_icon_old_sword") == 0);
  assert(w.dialogue.current_node->choice_count == 3);
  const dialogue_choice_t *accept_choice = &w.dialogue.current_node->choices[2];
  assert(strcmp(accept_choice->id, "accept") == 0);
  assert(accept_choice->reward_id &&
         strcmp(accept_choice->reward_id,
                "dlg_gate_guard_intro.accept.immediate") == 0);
  assert(accept_choice->effect_count == 6);
  assert(accept_choice->effects[0].kind == DIALOGUE_EFFECT_GRANT_ITEM);
  assert(strcmp(accept_choice->effects[0].item_id, "old_sword") == 0);
  assert(accept_choice->effects[3].kind == DIALOGUE_EFFECT_SET_FLAG);
  assert(strcmp(accept_choice->effects[3].flag_id, "gate_guard_intro_seen") ==
         0);
  assert(accept_choice->effects[5].kind == DIALOGUE_EFFECT_ADVANCE_QUEST);
  assert(strcmp(accept_choice->effects[5].step_id, "talk_gate_guard") == 0);
  assert(!game_dialogue_select_choice(&w, "close"));
  assert(!game_dialogue_select_choice(&w, "return_later"));
  assert(game_dialogue_select_choice(&w, "ask_what_happened"));
  assert(w.dialogue.current_node != 0);
  assert(strcmp(w.dialogue.current_node->id, "outside_lore") == 0);
  assert(game_dialogue_select_choice(&w, "ask_what_needed"));
  assert(w.dialogue.current_node != 0);
  assert(strcmp(w.dialogue.current_node->id, "explain_check") == 0);
  assert(game_dialogue_select_choice(&w, "accept"));
  assert(!w.dialogue.open);
  assert(w.first_scene.active_quest_id &&
         strcmp(w.first_scene.active_quest_id, "q001_gate_pass") == 0);
  assert(w.first_scene.active_quest_current_step_id &&
         strcmp(w.first_scene.active_quest_current_step_id,
                "equip_old_sword") == 0);
  assert(w.first_scene.active_quest_completed_talk_step);
  assert(w.first_scene.active_quest_gate_guard_intro_seen);
  assert(w.first_scene.tutorial_guard_talk_completed);
  assert(w.first_scene.objective_object_id == 0);
  assert(!w.first_scene.blacksmith_unlocked);
  assert(w.first_scene.gate_locked);
  assert(w.first_scene.contract_board_locked);
  assert(w.first_scene.current_objective_text &&
         strcmp(w.first_scene.current_objective_text,
                "Надеть выданное снаряжение") == 0);
  assert_gate_accept_state(&state);
  assert(!game_actions_starter_gear_equipped(&state));
  assert(game_actions_needs_starter_gear_onboarding(&state));

  assert(game_dialogue_open(&w, "dlg_gate_guard_intro"));
  assert(game_dialogue_select_choice(&w, "accept"));
  assert_gate_accept_state(&state);
  assert(game_actions_needs_starter_gear_onboarding(&state));

  assert(!game_actions_equip_gear(&state, "gear_missing_001"));
  assert(game_actions_equip_gear(&state, "gear_old_sword_001"));
  assert(state.inventory_bag_order_count == 2);
  assert(strcmp(state.inventory_bag_order[0], "gear_padded_jacket_001") == 0);
  assert(strcmp(state.inventory_bag_order[1], "gear_leather_greaves_001") ==
         0);
  assert(!game_actions_starter_gear_equipped(&state));
  assert(game_actions_needs_starter_gear_onboarding(&state));
  assert(!game_actions_can_move_location(&state, "hub_gate_outskirts"));
  assert(game_actions_equip_gear(&state, "gear_padded_jacket_001"));
  assert(state.inventory_bag_order_count == 1);
  assert(strcmp(state.inventory_bag_order[0], "gear_leather_greaves_001") ==
         0);
  assert(!game_actions_starter_gear_equipped(&state));
  assert(game_actions_needs_starter_gear_onboarding(&state));
  assert(!game_actions_can_move_location(&state, "hub_gate_outskirts"));
  assert(game_actions_equip_gear(&state, "gear_leather_greaves_001"));
  assert(state.inventory_bag_order_count == 0);
  assert(game_actions_starter_gear_equipped(&state));
  assert(!game_actions_needs_starter_gear_onboarding(&state));
  assert(game_actions_needs_gate_check_onboarding(&state));
  assert(!game_actions_can_move_location(&state, "hub_gate_outskirts"));
  const game_location_definition_t *last_post =
      game_content_find_location("hub_last_post");
  const game_location_object_t *caged_scavenger =
      find_location_object(last_post, "hub_last_post.caged_scavenger");
  assert(caged_scavenger != 0);
  const game_location_interaction_t *interaction =
      game_actions_select_location_interaction(&state, caged_scavenger);
  assert(interaction != 0);
  assert(strcmp(interaction->interaction_type, "start_encounter") == 0);
  assert(strcmp(interaction->encounter_id, "gate_scavenger") == 0);
  assert_gate_equipped_state(&state);

  assert(game_dialogue_open(&w, "dlg_gate_guard_intro"));
  assert(game_dialogue_select_choice(&w, "accept"));
  assert_gate_equipped_state(&state);

  advance_gate_quest_to_turn_in(&state);
  assert(game_dialogue_open(&w, "dlg_gate_guard_intro"));
  assert(game_dialogue_select_choice(&w, "accept"));
  assert_gate_reaccept_preserves_advanced_quest(&state);

  char err[128] = {0};
  char *saved = game_state_save_json_string(&state, err, (int)sizeof err);
  assert(saved != 0);
  GameState loaded;
  assert(game_state_load_json_string(&loaded, saved, err, (int)sizeof err));
  cJSON_free(saved);
  assert_gate_reaccept_preserves_advanced_quest(&loaded);
}

static void test_starter_gear_progress_does_not_depend_on_equip_order(void) {
  GameState state;
  game_state_init_defaults(&state);
  assert(game_actions_grant_gear(&state, "gear_old_sword_001", "old_sword",
                                 GAME_ACTION_GEAR_SLOT_WEAPON));
  assert(game_actions_grant_gear(&state, "gear_padded_jacket_001",
                                 "padded_jacket",
                                 GAME_ACTION_GEAR_SLOT_ARMOUR));
  assert(game_actions_grant_gear(&state, "gear_leather_greaves_001",
                                 "leather_greaves",
                                 GAME_ACTION_GEAR_SLOT_LEGS));
  assert(game_actions_start_quest(&state, "q001_gate_pass", "equip_old_sword",
                                  "test"));
  assert(game_actions_complete_step(&state, "q001_gate_pass",
                                    "talk_gate_guard", "equip_old_sword",
                                    "test"));
  assert(game_actions_set_flag(&state, "starter_gear_received"));

  assert(game_actions_equip_gear(&state, "gear_leather_greaves_001"));
  assert(game_actions_equip_gear(&state, "gear_padded_jacket_001"));
  assert(game_actions_equip_gear(&state, "gear_old_sword_001"));

  assert(game_actions_starter_gear_equipped(&state));
  assert(!game_actions_needs_starter_gear_onboarding(&state));
  assert(game_actions_needs_gate_check_onboarding(&state));
  const GameQuestState *quest = find_quest(&state, "q001_gate_pass");
  assert(quest != 0);
  assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
  assert(quest->has_current_step_id);
  assert(strcmp(quest->current_step_id, "clear_gate_scavenger") == 0);
  assert(has_flag(&state, "old_sword_equipped"));
  assert(has_flag(&state, "padded_jacket_equipped"));
  assert(has_flag(&state, "leather_greaves_equipped"));
}

static void
test_gate_turn_in_dialogue_completes_quest_and_grants_rewards(void) {
  World w = {0};
  GameState state;
  game_state_init_defaults(&state);
  w.player_state = &state;
  game_dialogue_init(&w);

  assert(game_actions_start_quest(&state, "q001_gate_pass",
                                  "report_to_gate_guard", "test"));
  advance_gate_quest_to_turn_in(&state);
  assert(game_dialogue_open(&w, "dlg_gate_guard_turn_in"));
  assert(game_dialogue_select_choice(&w, "take_token"));
  assert(!w.dialogue.open);

  const GameQuestState *quest = find_quest(&state, "q001_gate_pass");
  assert(quest != 0);
  assert(quest->status == GAME_STATE_QUEST_STATUS_COMPLETED);
  assert(!quest->has_current_step_id);
  assert(has_flag(&state, "seeker_token_owned"));
  assert(has_flag(&state, "map_gate_unlocked"));
  assert(has_flag(&state, "old_mill_unlocked"));
  const GameQuestState *bread_quest = find_quest(&state, "q002_bread_for_post");
  assert(bread_quest != 0);
  assert(bread_quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
  assert(bread_quest->has_current_step_id);
  assert(strcmp(bread_quest->current_step_id, "visit_old_mill") == 0);
  assert(state.hero_xp == 12);
  bool has_token = false;
  for (int i = 0; i < GAME_STATE_MAX_INVENTORY_STACK_INSTANCES; ++i) {
    if (state.inventory_stack_instances[i].used &&
        strcmp(state.inventory_stack_instances[i].key, "seeker_token") == 0) {
      has_token = state.inventory_stack_instances[i].count == 1;
      break;
    }
  }
  assert(has_token);
  assert(state.quests_claimed_reward_ids_count == 1);
  assert(strcmp(state.quests_claimed_reward_ids[0],
                "dlg_gate_guard_turn_in.take_token.completion") == 0);
}

static void test_equipping_replacement_returns_previous_item_to_bag(void) {
  GameState state;
  game_state_init_defaults(&state);

  assert(game_actions_grant_gear(&state, "gear_old_sword_001", "old_sword",
                                 GAME_ACTION_GEAR_SLOT_WEAPON));
  assert(game_actions_grant_gear(&state, "gear_iron_sword_001", "iron_sword",
                                 GAME_ACTION_GEAR_SLOT_WEAPON));
  assert(state.inventory_bag_order_count == 2);

  assert(game_actions_equip_gear(&state, "gear_old_sword_001"));
  assert(state.inventory_bag_order_count == 1);
  assert(strcmp(state.inventory_bag_order[0], "gear_iron_sword_001") == 0);

  assert(game_actions_equip_gear(&state, "gear_iron_sword_001"));
  assert(state.inventory_bag_order_count == 1);
  assert(strcmp(state.inventory_bag_order[0], "gear_old_sword_001") == 0);
  assert(state.has_equipment_weapon_instance_id);
  assert(strcmp(state.equipment_weapon_instance_id, "gear_iron_sword_001") ==
         0);
}

static void test_unequipping_returns_item_to_bag(void) {
  GameState state;
  game_state_init_defaults(&state);

  assert(game_actions_grant_gear(&state, "gear_old_sword_001", "old_sword",
                                 GAME_ACTION_GEAR_SLOT_WEAPON));
  assert(game_actions_equip_gear(&state, "gear_old_sword_001"));
  assert(state.inventory_bag_order_count == 0);
  assert(state.has_equipment_weapon_instance_id);
  assert(strcmp(state.equipment_weapon_instance_id, "gear_old_sword_001") ==
         0);

  assert(game_actions_unequip_gear(&state, "gear_old_sword_001"));
  assert(!state.has_equipment_weapon_instance_id);
  assert(state.equipment_weapon_instance_id[0] == '\0');
  assert(state.inventory_bag_order_count == 1);
  assert(strcmp(state.inventory_bag_order[0], "gear_old_sword_001") == 0);
  assert(!game_actions_unequip_gear(&state, "gear_old_sword_001"));
}

static void test_elder_dialogues_start_and_complete_q002_from_data(void) {
  World w = {0};
  GameState state;
  game_state_init_defaults(&state);
  w.player_state = &state;
  game_dialogue_init(&w);

  assert(game_dialogue_open(&w, "dlg_elder_bread_contract"));
  assert(game_dialogue_select_choice(&w, "accept_bread_contract"));
  assert(!w.dialogue.open);
  const GameQuestState *quest = find_quest(&state, "q002_bread_for_post");
  assert(quest != 0);
  assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
  assert(quest->has_current_step_id);
  assert(strcmp(quest->current_step_id, "visit_old_mill") == 0);
  assert(state.quests_completed_step_ids_count == 1);
  assert(strcmp(state.quests_completed_step_ids[0], "accept_bread_contract") ==
         0);

  assert(game_actions_complete_step(&state, "q002_bread_for_post",
                                    "visit_old_mill", "inspect_old_mill",
                                    "test"));
  assert(game_actions_complete_step(&state, "q002_bread_for_post",
                                    "inspect_old_mill", "report_to_elder",
                                    "test"));
  assert(game_dialogue_open(&w, "dlg_elder_bread_turn_in"));
  assert(game_dialogue_select_choice(&w, "report_bread_contract"));
  assert(!w.dialogue.open);

  quest = find_quest(&state, "q002_bread_for_post");
  assert(quest != 0);
  assert(quest->status == GAME_STATE_QUEST_STATUS_COMPLETED);
  assert(!quest->has_current_step_id);
  assert(state.hero_xp == 10);
  assert(state.wallet_gold == 6);
  assert(state.quests_claimed_reward_ids_count == 1);
  assert(strcmp(state.quests_claimed_reward_ids[0],
                "dlg_elder_bread_turn_in.report.completion") == 0);
}

static void test_legacy_v1_save_without_legs_slot_loads(void) {
  static const char *legacy_save =
      "{\"schema\":\"rb_dark_rpg.player_state\",\"document\":\"player\","
      "\"version\":1,\"state\":{"
      "\"hero\":{\"level\":1,\"xp\":0,\"hp\":30},"
      "\"wallet\":{\"gold\":0},"
      "\"inventory\":{\"stack_instances\":{},\"gear_instances\":{},\"bag_"
      "order\":[]},"
      "\"equipment\":{\"weapon_instance_id\":null,\"armour_instance_id\":null,"
      "\"charm_instance_id\":null},"
      "\"quests\":{\"quest_states\":{},\"tracked_quest_id\":null,\"completed_"
      "step_ids\":[],\"claimed_reward_ids\":[],\"choice_ids\":[]},"
      "\"flags\":{\"ids\":[]},"
      "\"tutorial\":{\"completed_step_ids\":[]},"
      "\"unlocks\":{\"ids\":[]}"
      "}}";
  char err[128] = {0};
  GameState loaded;
  assert(
      game_state_load_json_string(&loaded, legacy_save, err, (int)sizeof err));
  assert(!loaded.has_equipment_legs_instance_id);
}

static void test_equipment_slot_registry_matches_runtime_grid(void) {
  static const char *expected_ids[] = {
      "weapon", "offhand", "head", "armour",    "hands",      "waist",
      "legs",   "feet",    "neck", "ring_left", "ring_right", "relic",
  };
  static const bool expected_mvp[] = {
      true, false, false, true,  false, false,
      true, false, false, false, false, true,
  };
  const int expected_count =
      (int)(sizeof expected_ids / sizeof expected_ids[0]);
  assert(game_content_equipment_slot_count() == expected_count);
  for (int i = 0; i < expected_count; ++i) {
    const game_equipment_slot_definition_t *slot =
        game_content_equipment_slot_at(i);
    assert(slot != 0);
    assert(strcmp(slot->id, expected_ids[i]) == 0);
    assert(slot->mvp == expected_mvp[i]);
    assert(slot->ui_order == i + 1);
  }
  assert(game_content_equipment_slot_at(-1) == 0);
  assert(game_content_equipment_slot_at(expected_count) == 0);
}

int main(void) {
  test_gate_dialogue_progression();
  test_starter_gear_progress_does_not_depend_on_equip_order();
  test_gate_turn_in_dialogue_completes_quest_and_grants_rewards();
  test_equipping_replacement_returns_previous_item_to_bag();
  test_unequipping_returns_item_to_bag();
  test_elder_dialogues_start_and_complete_q002_from_data();
  test_legacy_v1_save_without_legs_slot_loads();
  test_equipment_slot_registry_matches_runtime_grid();
  return 0;
}
