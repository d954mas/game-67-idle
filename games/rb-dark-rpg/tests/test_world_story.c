#include "game_actions.h"
#include "game_combat.h"
#include "game_content.h"
#include "game_state.h"

#include <assert.h>
#include <string.h>

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

static bool has_visit(const GameState *state, const char *location_id) {
  for (int i = 0; i < state->world_visited_location_ids_count; ++i) {
    if (strcmp(state->world_visited_location_ids[i], location_id) == 0) {
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

static void
assert_valid_map_position(const game_location_definition_t *location) {
  assert(location != 0);
  assert(location->has_map_position);
  assert(location->map_x >= 0.0F);
  assert(location->map_x <= 1.0F);
  assert(location->map_y >= 0.0F);
  assert(location->map_y <= 1.0F);
}

static void setup_gate_ready_state(GameState *state) {
  game_state_init_defaults(state);
  assert(game_actions_start_quest(state, "q001_gate_pass",
                                  "clear_gate_scavenger", "test"));
  assert(game_actions_grant_gear(state, "gear_old_sword_001", "old_sword",
                                 GAME_ACTION_GEAR_SLOT_WEAPON));
  assert(game_actions_grant_gear(state, "gear_padded_jacket_001",
                                 "padded_jacket",
                                 GAME_ACTION_GEAR_SLOT_ARMOUR));
  assert(game_actions_grant_gear(state, "gear_leather_greaves_001",
                                 "leather_greaves",
                                 GAME_ACTION_GEAR_SLOT_LEGS));
  assert(game_actions_equip_gear(state, "gear_old_sword_001"));
  assert(game_actions_equip_gear(state, "gear_padded_jacket_001"));
  assert(game_actions_equip_gear(state, "gear_leather_greaves_001"));
}

static void test_world_content_registry(void) {
  assert(game_content_location_count() >= 3);
  assert(game_content_location_at(0) != 0);
  assert(game_content_location_at(-1) == 0);
  assert(game_content_location_at(game_content_location_count()) == 0);

  assert(game_content_quest_count() >= 2);
  assert(game_content_quest_at(0) != 0);
  assert(game_content_quest_at(-1) == 0);
  assert(game_content_quest_at(game_content_quest_count()) == 0);
  const game_quest_definition_t *gate_quest =
      game_content_find_quest("q001_gate_pass");
  assert(gate_quest != 0);
  assert(strcmp(gate_quest->title, "Допуск за ворота") == 0);
  assert(gate_quest->step_count >= 6);
  const game_quest_step_definition_t *gate_step =
      game_content_find_quest_step("q001_gate_pass", "clear_gate_scavenger");
  assert(gate_step != 0);
  assert(strcmp(gate_step->title,
                "Проверить себя на падальщике в клетке") == 0);
  assert(strcmp(gate_step->location_id, "hub_last_post") == 0);
  assert(game_content_find_quest("missing_quest") == 0);
  assert(game_content_find_quest_step("q001_gate_pass", "missing_step") == 0);

  const game_location_definition_t *last_post =
      game_content_find_location("hub_last_post");
  assert(last_post != 0);
  assert(last_post->display_name != 0);
  assert(strcmp(last_post->screen_id, "hub_last_post") == 0);
  assert(last_post->object_count >= 1);
  assert(last_post->objects != 0);
  assert(strcmp(last_post->objects[0].id, "hub_last_post.gate_guard") == 0);
  assert(strcmp(last_post->objects[0].kind, "npc") == 0);
  assert(strcmp(last_post->objects[0].character_id, "gate_guard") == 0);
  assert(last_post->objects[0].interaction_count >= 3);
  assert(strcmp(last_post->objects[0].interactions[0].dialogue_id,
                "dlg_gate_guard_turn_in") == 0);
  assert(last_post->objects[0].interactions[0].requirement_count == 1);
  assert(last_post->objects[0].interactions[0].requirements[0].kind ==
         GAME_LOCATION_REQUIREMENT_QUEST_STEP);
  assert(strcmp(last_post->objects[0].interactions[0].requirements[0].id,
                "q001_gate_pass") == 0);
  assert(strcmp(last_post->objects[0].interactions[0].requirements[0].step_id,
                "report_to_gate_guard") == 0);
  assert(strcmp(last_post->objects[0].interactions[1].dialogue_id,
                "dlg_gate_guard_completed") == 0);
  assert(last_post->objects[0].interactions[1].requirement_count == 1);
  assert(last_post->objects[0].interactions[1].requirements[0].kind ==
         GAME_LOCATION_REQUIREMENT_QUEST_STATUS);
  assert(strcmp(last_post->objects[0].interactions[1].requirements[0].id,
                "q001_gate_pass") == 0);
  assert(strcmp(last_post->objects[0].interactions[1].requirements[0].status,
                "completed") == 0);
  assert(strcmp(last_post->objects[0].interactions[2].dialogue_id,
                "dlg_gate_guard_intro") == 0);
  assert(last_post->objects[0].interactions[2].requirement_count == 1);
  assert(last_post->objects[0].interactions[2].requirements[0].kind ==
         GAME_LOCATION_REQUIREMENT_FLAG);
  assert(strcmp(last_post->objects[0].interactions[2].requirements[0].id,
                "gate_guard_intro_seen") == 0);
  assert(!last_post->objects[0].interactions[2].requirements[0].value);
  const game_location_object_t *elder =
      find_location_object(last_post, "hub_last_post.elder");
  assert(elder != 0);
  assert(elder->scene_enabled);
  const game_location_object_t *caged_scavenger =
      find_location_object(last_post, "hub_last_post.caged_scavenger");
  assert(caged_scavenger != 0);
  assert(strcmp(caged_scavenger->kind, "combat") == 0);
  assert(strcmp(caged_scavenger->encounter_id, "gate_scavenger") == 0);
  assert(caged_scavenger->interaction_count == 1);
  assert(strcmp(caged_scavenger->interactions[0].interaction_type,
                "start_encounter") == 0);
  assert(strcmp(caged_scavenger->interactions[0].encounter_id,
                "gate_scavenger") == 0);
  assert(caged_scavenger->requirement_count == 1);
  assert(caged_scavenger->requirements[0].kind ==
         GAME_LOCATION_REQUIREMENT_QUEST_STEP);
  assert(strcmp(caged_scavenger->requirements[0].id, "q001_gate_pass") == 0);
  assert(strcmp(caged_scavenger->requirements[0].step_id,
                "clear_gate_scavenger") == 0);

  const game_location_object_t *trader =
      find_location_object(last_post, "hub_last_post.town_trader");
  assert(trader != 0);
  assert(strcmp(trader->kind, "npc") == 0);
  assert(strcmp(trader->character_id, "town_trader") == 0);
  assert(trader->interaction_count == 1);
  assert(strcmp(trader->interactions[0].interaction_type, "shop") == 0);
  assert(strcmp(trader->interactions[0].shop_id, "shop_post_trader_basic") ==
         0);

  const game_shop_definition_t *shop =
      game_content_find_shop("shop_post_trader_basic");
  assert(shop != 0);
  assert(strcmp(shop->keeper_character_id, "town_trader") == 0);
  assert(strcmp(shop->location_id, "hub_last_post") == 0);
  assert(shop->item_count >= 3);
  assert(strcmp(shop->items[0].item_id, "iron_sword") == 0);
  assert(shop->items[0].price_gold == 12);
  assert(shop->items[0].requirement_count == 1);
  assert(shop->items[0].requirements[0].kind ==
         GAME_SERVICE_REQUIREMENT_QUEST_COMPLETED);
  assert(strcmp(shop->items[0].requirements[0].id, "q001_gate_pass") == 0);
  assert_valid_map_position(last_post);

  const game_location_definition_t *gate_outskirts =
      game_content_find_location("hub_gate_outskirts");
  assert_valid_map_position(gate_outskirts);
  assert(gate_outskirts->map_x > last_post->map_x);

  const game_location_definition_t *equipment =
      game_content_find_location("equipment");
  assert(equipment != 0);
  assert(!equipment->has_map_position);

  const game_location_definition_t *old_mill =
      game_content_find_location("old_mill");
  assert(old_mill != 0);
  assert(old_mill->unlock_kind == GAME_LOCATION_UNLOCK_FLAG);
  assert(strcmp(old_mill->unlock_flag_id, "old_mill_unlocked") == 0);
  assert(old_mill->object_count == 5);
  assert(strcmp(old_mill->objects[1].asset_id,
                "asset_object_black_sun_clue_wall") == 0);
  assert(old_mill->objects[1].interaction_count == 1);
  assert(strcmp(old_mill->objects[1].interactions[0].object_id,
                "black_sun_mark") == 0);
  assert(old_mill->objects[1].requirement_count == 1);
  assert_valid_map_position(old_mill);
  assert(old_mill->map_x > gate_outskirts->map_x);
  assert(old_mill->map_y > gate_outskirts->map_y);

  const game_quest_definition_t *bread_quest =
      game_content_find_quest("q002_bread_for_post");
  assert(bread_quest != 0);
  assert(strcmp(bread_quest->short_goal,
                "Открой карту, дойди до Старой мельницы и зачисти двор.") ==
         0);
  const game_quest_step_definition_t *mill_yard_step =
      game_content_find_quest_step("q002_bread_for_post",
                                   "q002_clear_mill_yard");
  assert(mill_yard_step != 0);
  assert(strcmp(mill_yard_step->location_id, "old_mill") == 0);
  assert(strcmp(mill_yard_step->title, "Зачистить двор мельницы") == 0);
  assert(strstr(mill_yard_step->description, "Открой «Здесь»") != 0);
  const game_quest_step_definition_t *mill_brute_step =
      game_content_find_quest_step("q002_bread_for_post",
                                   "q002_clear_mill_brute");
  assert(mill_brute_step != 0);
  assert(strcmp(mill_brute_step->location_id, "old_mill") == 0);
  assert(strcmp(mill_brute_step->title, "Добить главаря у мельницы") == 0);
  assert(game_content_find_location("missing_location") == 0);
}

static void test_location_object_interaction_selection_uses_state(void) {
  GameState state;
  game_state_init_defaults(&state);
  const game_location_definition_t *last_post =
      game_content_find_location("hub_last_post");
  assert(last_post != 0);
  const game_location_object_t *guard = &last_post->objects[0];

  const game_location_interaction_t *interaction =
      game_actions_select_location_interaction(&state, guard);
  assert(interaction != 0);
  assert(strcmp(interaction->interaction_type, "dialogue") == 0);
  assert(strcmp(interaction->dialogue_id, "dlg_gate_guard_intro") == 0);

  /* After the gate fight the quest rests ACTIVE on the return step; the guard
   * must offer the turn-in dialogue at that resting state (not only during the
   * transient READY_TO_TURN_IN flip that the turn-in choice itself performs). */
  assert(game_actions_start_quest(&state, "q001_gate_pass",
                                  "report_to_gate_guard", "test"));
  interaction = game_actions_select_location_interaction(&state, guard);
  assert(interaction != 0);
  assert(strcmp(interaction->interaction_type, "dialogue") == 0);
  assert(strcmp(interaction->dialogue_id, "dlg_gate_guard_turn_in") == 0);

  assert(game_actions_complete_step(&state, "q001_gate_pass",
                                    "report_to_gate_guard", 0, "test"));
  assert(game_actions_complete_quest(&state, "q001_gate_pass", "test"));
  interaction = game_actions_select_location_interaction(&state, guard);
  assert(interaction != 0);
  assert(strcmp(interaction->interaction_type, "dialogue") == 0);
  assert(strcmp(interaction->dialogue_id, "dlg_gate_guard_completed") == 0);
}

static void test_elder_interaction_selection_tracks_q002_story_state(void) {
  GameState state;
  game_state_init_defaults(&state);
  assert(game_actions_set_flag(&state, "map_gate_unlocked"));

  const game_location_definition_t *last_post =
      game_content_find_location("hub_last_post");
  assert(last_post != 0);
  const game_location_object_t *elder = 0;
  for (int i = 0; i < last_post->object_count; ++i) {
    if (strcmp(last_post->objects[i].id, "hub_last_post.elder") == 0) {
      elder = &last_post->objects[i];
      break;
    }
  }
  assert(elder != 0);

  const game_location_interaction_t *interaction =
      game_actions_select_location_interaction(&state, elder);
  assert(interaction != 0);
  assert(strcmp(interaction->dialogue_id, "dlg_elder_bread_contract") == 0);

  assert(game_actions_start_quest(&state, "q002_bread_for_post",
                                  "visit_old_mill", "test"));
  interaction = game_actions_select_location_interaction(&state, elder);
  assert(interaction != 0);
  assert(strcmp(interaction->dialogue_id, "dlg_elder_bread_in_progress") == 0);

  assert(game_actions_complete_step(&state, "q002_bread_for_post",
                                    "visit_old_mill", "inspect_old_mill",
                                    "test"));
  assert(game_actions_complete_step(&state, "q002_bread_for_post",
                                    "inspect_old_mill", "report_to_elder",
                                    "test"));
  interaction = game_actions_select_location_interaction(&state, elder);
  assert(interaction != 0);
  assert(strcmp(interaction->dialogue_id, "dlg_elder_bread_turn_in") == 0);

  assert(game_actions_complete_quest(&state, "q002_bread_for_post", "test"));
  interaction = game_actions_select_location_interaction(&state, elder);
  assert(interaction != 0);
  assert(strcmp(interaction->dialogue_id, "dlg_q005_night_visitors_start") == 0);

  /* Accepting the finale moves the elder into the night-assault in-progress line. */
  assert(game_actions_start_quest(&state, "q005_night_visitors",
                                  "q005_defeat_attacker", "test"));
  interaction = game_actions_select_location_interaction(&state, elder);
  assert(interaction != 0);
  assert(strcmp(interaction->dialogue_id,
                "dlg_q005_night_visitors_in_progress") == 0);

  /* Once the boss step is cleared, the elder offers the finale turn-in. */
  assert(game_actions_complete_step(&state, "q005_night_visitors",
                                    "q005_defeat_attacker", "q005_report_attack",
                                    "test"));
  interaction = game_actions_select_location_interaction(&state, elder);
  assert(interaction != 0);
  assert(strcmp(interaction->dialogue_id, "dlg_q005_night_visitors_report") == 0);

  /* After the finale completes, the elder never re-offers the contract. */
  assert(game_actions_complete_quest(&state, "q005_night_visitors", "test"));
  interaction = game_actions_select_location_interaction(&state, elder);
  assert(interaction != 0);
  assert(strcmp(interaction->dialogue_id, "dlg_elder_bread_completed") == 0);
}

static void test_move_rejects_missing_or_locked_locations(void) {
  GameState state;
  game_state_init_defaults(&state);
  assert(!game_actions_location_unlocked(&state, "missing_location"));
  assert(game_actions_location_unlocked(&state, "hub_last_post"));
  assert(!game_actions_location_unlocked(&state, "old_mill"));
  assert(!game_actions_can_move_location(&state, "missing_location"));
  assert(strcmp(state.world_current_location_id, "hub_last_post") == 0);
  assert(has_visit(&state, "hub_last_post"));

  assert(!game_actions_move_location(&state, "missing_location"));
  assert(strcmp(state.world_current_location_id, "hub_last_post") == 0);
  assert(!game_actions_move_location(&state, "old_mill"));
  assert(strcmp(state.world_current_location_id, "hub_last_post") == 0);
}

static void test_first_gate_check_stays_inside_last_post(void) {
  GameState state;
  game_state_init_defaults(&state);
  assert(!game_actions_can_move_location(&state, "hub_gate_outskirts"));
  assert(!game_actions_move_location(&state, "hub_gate_outskirts"));
  assert(strcmp(state.world_current_location_id, "hub_last_post") == 0);

  setup_gate_ready_state(&state);
  assert(!game_actions_can_move_location(&state, "hub_gate_outskirts"));
  assert(!game_actions_move_location(&state, "hub_gate_outskirts"));
  assert(strcmp(state.world_current_location_id, "hub_last_post") == 0);
  assert(has_visit(&state, "hub_last_post"));

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

  game_combat_result_t result;
  assert(game_actions_resolve_encounter(&state, "gate_scavenger", &result));
  assert(result.outcome == GAME_COMBAT_OUTCOME_WIN);
  assert(strcmp(state.world_current_location_id, "hub_last_post") == 0);
  assert(!has_visit(&state, "hub_gate_outskirts"));
  assert(!game_actions_location_object_available(&state, caged_scavenger));

  char err[128] = {0};
  char *saved = game_state_save_json_string(&state, err, (int)sizeof err);
  assert(saved != 0);
  GameState loaded;
  assert(game_state_load_json_string(&loaded, saved, err, (int)sizeof err));
  cJSON_free(saved);
  assert(strcmp(loaded.world_current_location_id, "hub_last_post") == 0);
  assert(has_visit(&loaded, "hub_last_post"));
  assert(!has_visit(&loaded, "hub_gate_outskirts"));
}

static void test_visit_location_advances_quest_step(void) {
  GameState state;
  game_state_init_defaults(&state);
  assert(game_actions_set_flag(&state, "map_gate_unlocked"));
  assert(game_actions_set_flag(&state, "old_mill_unlocked"));
  assert(game_actions_start_quest(&state, "q002_bread_for_post",
                                  "visit_old_mill", "test"));

  assert(game_actions_move_location(&state, "hub_gate_outskirts"));
  assert(strcmp(state.world_current_location_id, "hub_gate_outskirts") == 0);
  assert(game_actions_move_location(&state, "old_mill"));
  assert(strcmp(state.world_current_location_id, "old_mill") == 0);
  assert(has_visit(&state, "hub_gate_outskirts"));
  assert(has_visit(&state, "old_mill"));
  assert(state.quests_completed_step_ids_count == 1);
  assert(strcmp(state.quests_completed_step_ids[0], "visit_old_mill") == 0);

  const GameQuestState *quest = find_quest(&state, "q002_bread_for_post");
  assert(quest != 0);
  assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
  assert(quest->has_current_step_id);
  assert(strcmp(quest->current_step_id, "q002_clear_mill_yard") == 0);
}

static void test_location_object_requirements_and_inspect_step(void) {
  GameState state;
  game_state_init_defaults(&state);
  assert(game_actions_set_flag(&state, "old_mill_unlocked"));

  const game_location_definition_t *old_mill =
      game_content_find_location("old_mill");
  assert(old_mill != 0);
  const game_location_object_t *mark = &old_mill->objects[1];
  assert(strcmp(mark->id, "old_mill.black_sun_mark") == 0);
  assert(!game_actions_location_object_available(&state, mark));
  assert(!game_actions_inspect_object(&state, "old_mill.black_sun_mark"));

  assert(game_actions_start_quest(&state, "q002_bread_for_post",
                                  "inspect_old_mill", "test"));
  assert(game_actions_location_object_available(&state, mark));
  const game_quest_inspect_step_t *step = game_content_find_inspect_step(
      "q002_bread_for_post", "old_mill.black_sun_mark");
  assert(step != 0);
  assert(strcmp(step->step_id, "inspect_old_mill") == 0);

  assert(game_actions_inspect_object(&state, "old_mill.black_sun_mark"));
  assert(state.quests_completed_step_ids_count == 1);
  assert(strcmp(state.quests_completed_step_ids[0], "inspect_old_mill") == 0);

  const GameQuestState *quest = find_quest(&state, "q002_bread_for_post");
  assert(quest != 0);
  assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
  assert(quest->has_current_step_id);
  assert(strcmp(quest->current_step_id, "report_to_elder") == 0);
}

int main(void) {
  test_world_content_registry();
  test_move_rejects_missing_or_locked_locations();
  test_location_object_interaction_selection_uses_state();
  test_elder_interaction_selection_tracks_q002_story_state();
  test_first_gate_check_stays_inside_last_post();
  test_visit_location_advances_quest_step();
  test_location_object_requirements_and_inspect_step();
  return 0;
}
