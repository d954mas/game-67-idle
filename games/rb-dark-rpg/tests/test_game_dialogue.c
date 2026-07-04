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

static const GameQuestState *find_quest(const GameState *state, const char *quest_id) {
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

static void assert_gate_accept_state(const GameState *state) {
    assert(gear_count(state) == 3);
    assert(state->has_equipment_weapon_instance_id);
    assert(strcmp(state->equipment_weapon_instance_id, "gear_old_sword_001") == 0);
    assert(state->has_equipment_armour_instance_id);
    assert(strcmp(state->equipment_armour_instance_id, "gear_padded_jacket_001") == 0);
    assert(state->has_equipment_legs_instance_id);
    assert(strcmp(state->equipment_legs_instance_id, "gear_leather_greaves_001") == 0);
    assert(state->quests_completed_step_ids_count == 1);
    assert(strcmp(state->quests_completed_step_ids[0], "talk_gate_guard") == 0);
    assert(state->quests_claimed_reward_ids_count == 1);
    assert(strcmp(state->quests_claimed_reward_ids[0], "dlg_gate_guard_intro.accept.immediate") == 0);
    assert(state->quests_choice_ids_count == 1);
    assert(strcmp(state->quests_choice_ids[0], "dlg_gate_guard_intro.accept") == 0);
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

static void assert_gate_reaccept_preserves_advanced_quest(const GameState *state) {
    assert(gear_count(state) == 3);
    assert(state->quests_completed_step_ids_count == 1);
    assert(strcmp(state->quests_completed_step_ids[0], "talk_gate_guard") == 0);
    assert(state->quests_claimed_reward_ids_count == 1);
    assert(strcmp(state->quests_claimed_reward_ids[0], "dlg_gate_guard_intro.accept.immediate") == 0);
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
        if (state->quests_quest_states[i].used && strcmp(state->quests_quest_states[i].key, "q001_gate_pass") == 0) {
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
    assert(w.dialogue.current_node->speaker_name && strcmp(w.dialogue.current_node->speaker_name, "Страж у ворот") == 0);
    assert(w.dialogue.current_node->quest_name && strcmp(w.dialogue.current_node->quest_name, "Допуск за ворота") == 0);
    assert(w.dialogue.definition != 0);
    assert(w.dialogue.definition->quest_preview != 0);
    assert(w.dialogue.definition->quest_preview->immediate_reward_count == 3);
    assert(w.dialogue.definition->quest_preview->completion_reward_count == 3);
    assert(w.dialogue.current_node->choice_count == 3);
    const dialogue_choice_t *accept_choice = &w.dialogue.current_node->choices[2];
    assert(strcmp(accept_choice->id, "accept") == 0);
    assert(accept_choice->reward_id && strcmp(accept_choice->reward_id, "dlg_gate_guard_intro.accept.immediate") == 0);
    assert(accept_choice->effect_count == 6);
    assert(accept_choice->effects[0].kind == DIALOGUE_EFFECT_GRANT_ITEM);
    assert(strcmp(accept_choice->effects[0].item_id, "old_sword") == 0);
    assert(accept_choice->effects[3].kind == DIALOGUE_EFFECT_SET_FLAG);
    assert(strcmp(accept_choice->effects[3].flag_id, "gate_guard_intro_seen") == 0);
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
    assert(w.first_scene.active_quest_id && strcmp(w.first_scene.active_quest_id, "q001_gate_pass") == 0);
    assert(w.first_scene.active_quest_current_step_id && strcmp(w.first_scene.active_quest_current_step_id, "equip_old_sword") == 0);
    assert(w.first_scene.active_quest_completed_talk_step);
    assert(w.first_scene.active_quest_gate_guard_intro_seen);
    assert(w.first_scene.tutorial_guard_talk_completed);
    assert(w.first_scene.objective_object_id == SCENE_OBJECT_ID_NONE);
    assert(!w.first_scene.blacksmith_unlocked);
    assert(w.first_scene.gate_locked);
    assert(w.first_scene.contract_board_locked);
    assert(w.first_scene.current_objective_text && strcmp(w.first_scene.current_objective_text, "Надеть выданное снаряжение") == 0);
    assert_gate_accept_state(&state);

    assert(game_dialogue_open(&w, "dlg_gate_guard_intro"));
    assert(game_dialogue_select_choice(&w, "accept"));
    assert_gate_accept_state(&state);

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

static void test_legacy_v1_save_without_legs_slot_loads(void) {
    static const char *legacy_save =
        "{\"schema\":\"rb_dark_rpg.player_state\",\"document\":\"player\",\"version\":1,\"state\":{"
        "\"hero\":{\"level\":1,\"xp\":0,\"hp\":30},"
        "\"wallet\":{\"gold\":0},"
        "\"inventory\":{\"stack_instances\":{},\"gear_instances\":{},\"bag_order\":[]},"
        "\"equipment\":{\"weapon_instance_id\":null,\"armour_instance_id\":null,\"charm_instance_id\":null},"
        "\"quests\":{\"quest_states\":{},\"tracked_quest_id\":null,\"completed_step_ids\":[],\"claimed_reward_ids\":[],\"choice_ids\":[]},"
        "\"flags\":{\"ids\":[]},"
        "\"tutorial\":{\"completed_step_ids\":[]},"
        "\"unlocks\":{\"ids\":[]}"
        "}}";
    char err[128] = {0};
    GameState loaded;
    assert(game_state_load_json_string(&loaded, legacy_save, err, (int)sizeof err));
    assert(!loaded.has_equipment_legs_instance_id);
}

int main(void) {
    test_gate_dialogue_progression();
    test_legacy_v1_save_without_legs_slot_loads();
    return 0;
}
