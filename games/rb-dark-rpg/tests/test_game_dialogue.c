#include "game_dialogue.h"
#include "world/world.h"

#include <assert.h>
#include <string.h>

static void test_gate_dialogue_progression(void) {
    World w = {0};
    game_dialogue_init(&w);
    assert(game_dialogue_open(&w, "dlg_gate_guard_intro"));
    assert(w.dialogue.open);
    assert(w.dialogue.current_node != 0);
    assert(strcmp(w.dialogue.current_node->id, "start") == 0);
    assert(w.dialogue.definition != 0);
    assert(w.dialogue.definition->quest_preview != 0);
    assert(w.dialogue.definition->quest_preview->immediate_reward_count == 3);
    assert(w.dialogue.definition->quest_preview->completion_reward_count == 3);
    assert(w.dialogue.current_node->choice_count == 3);
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
    assert(w.first_scene.current_objective_text && strcmp(w.first_scene.current_objective_text, "Надеть выданное снаряжение") == 0);
}

int main(void) {
    test_gate_dialogue_progression();
    return 0;
}
