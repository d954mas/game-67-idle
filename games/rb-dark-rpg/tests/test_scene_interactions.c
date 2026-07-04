#include "scene/scene_interactions.h"
#include "game_dialogue.h"
#include "game_state.h"

#include <assert.h>
#include <string.h>

static void test_first_scene_defaults(void) {
    World w = {0};
    scene_interactions_init_first_scene(&w);
    assert(w.first_scene.interactions_initialized);
    assert(w.first_scene.hovered_object_id == SCENE_OBJECT_ID_NONE);
    assert(w.first_scene.pressed_object_id == SCENE_OBJECT_ID_NONE);
    assert(w.first_scene.activated_object_id == SCENE_OBJECT_ID_NONE);
    assert(w.first_scene.objective_object_id == SCENE_OBJECT_ID_GUARD);
    assert(!w.first_scene.tutorial_guard_talk_completed);
    assert(scene_interactions_should_show_tutorial_finger(&w, SCENE_OBJECT_ID_GUARD));
}

static void test_guard_hit_bounds(void) {
    assert(scene_interactions_hit_test(656.0f, 300.0f) == SCENE_OBJECT_ID_GUARD);
    assert(scene_interactions_hit_test(583.0f, 300.0f) == SCENE_OBJECT_ID_NONE);
    assert(scene_interactions_hit_test(656.0f, 201.0f) == SCENE_OBJECT_ID_NONE);
}

static void test_guard_uses_mask_glow_highlight(void) {
    const scene_interaction_object_t *guard = scene_interactions_find(SCENE_OBJECT_ID_GUARD);
    assert(guard != 0);
    assert(guard->highlight_profile == SCENE_HIGHLIGHT_MASK_GLOW);
}

static void test_visual_flags_compose_states(void) {
    World w = {0};
    scene_interactions_init_first_scene(&w);

    uint32_t flags = scene_interactions_visual_flags(&w, SCENE_OBJECT_ID_GUARD);
    assert((flags & SCENE_INTERACTION_IDLE) != 0U);
    assert((flags & SCENE_INTERACTION_OBJECTIVE) != 0U);
    assert((flags & SCENE_INTERACTION_HOVERED) == 0U);
    assert((flags & SCENE_INTERACTION_PRESSED) == 0U);

    w.first_scene.hovered_object_id = SCENE_OBJECT_ID_GUARD;
    w.first_scene.pressed_object_id = SCENE_OBJECT_ID_GUARD;
    flags = scene_interactions_visual_flags(&w, SCENE_OBJECT_ID_GUARD);
    assert((flags & SCENE_INTERACTION_HOVERED) != 0U);
    assert((flags & SCENE_INTERACTION_PRESSED) != 0U);
}

static void test_pointer_state_captures_pan_while_pressed(void) {
    World w = {0};
    scene_interactions_update_pointer_state(&w, SCENE_OBJECT_ID_GUARD, true, true, false);
    assert(w.first_scene.pressed_object_id == SCENE_OBJECT_ID_GUARD);
    assert(w.first_scene.hovered_object_id == SCENE_OBJECT_ID_GUARD);
    assert(scene_interactions_pointer_captures_pan(&w));

    scene_interactions_update_pointer_state(&w, SCENE_OBJECT_ID_NONE, false, true, false);
    assert(w.first_scene.pressed_object_id == SCENE_OBJECT_ID_GUARD);
    assert(w.first_scene.hovered_object_id == SCENE_OBJECT_ID_GUARD);
    assert(scene_interactions_pointer_captures_pan(&w));

    scene_interactions_update_pointer_state(&w, SCENE_OBJECT_ID_NONE, false, false, true);
    assert(w.first_scene.pressed_object_id == SCENE_OBJECT_ID_NONE);
    assert(w.first_scene.hovered_object_id == SCENE_OBJECT_ID_NONE);
    assert(w.first_scene.activated_object_id == SCENE_OBJECT_ID_NONE);
    assert(!scene_interactions_pointer_captures_pan(&w));
}

static void test_guard_release_opens_dialogue_without_completing_step(void) {
    World w = {0};
    GameState state;
    game_state_init_defaults(&state);
    w.player_state = &state;
    scene_interactions_update_pointer_state(&w, SCENE_OBJECT_ID_GUARD, true, true, false);
    assert(scene_interactions_should_show_tutorial_finger(&w, SCENE_OBJECT_ID_GUARD));

    scene_interactions_update_pointer_state(&w, SCENE_OBJECT_ID_GUARD, false, false, true);
    assert(w.first_scene.activated_object_id == SCENE_OBJECT_ID_GUARD);
    assert(w.first_scene.pressed_object_id == SCENE_OBJECT_ID_NONE);
    assert(w.first_scene.objective_object_id == SCENE_OBJECT_ID_GUARD);
    assert(!w.first_scene.tutorial_guard_talk_completed);
    assert(!w.first_scene.active_quest_completed_talk_step);
    assert(!w.first_scene.active_quest_gate_guard_intro_seen);
    assert(w.dialogue.open);
    assert(w.dialogue.current_node != 0);
    assert(strcmp(w.dialogue.current_node->id, "start") == 0);
    assert(w.first_scene.current_objective_text != 0);
    assert(!scene_interactions_should_show_tutorial_finger(&w, SCENE_OBJECT_ID_GUARD));

    assert(game_dialogue_select_choice(&w, "ask_what_needed"));
    assert(w.dialogue.current_node != 0);
    assert(strcmp(w.dialogue.current_node->id, "explain_check") == 0);
    assert(game_dialogue_select_choice(&w, "accept"));
    assert(!w.dialogue.open);
    assert(w.first_scene.tutorial_guard_talk_completed);
    assert(w.first_scene.active_quest_completed_talk_step);
    assert(w.first_scene.active_quest_gate_guard_intro_seen);
    assert(w.first_scene.objective_object_id == SCENE_OBJECT_ID_NONE);
    assert(!w.first_scene.blacksmith_unlocked);

    scene_interactions_update_pointer_state(&w, SCENE_OBJECT_ID_NONE, false, false, false);
    assert(w.first_scene.activated_object_id == SCENE_OBJECT_ID_NONE);
}

static void test_release_outside_does_not_complete_tutorial(void) {
    World w = {0};
    scene_interactions_update_pointer_state(&w, SCENE_OBJECT_ID_GUARD, true, true, false);
    scene_interactions_update_pointer_state(&w, SCENE_OBJECT_ID_NONE, false, false, true);

    assert(w.first_scene.activated_object_id == SCENE_OBJECT_ID_NONE);
    assert(w.first_scene.objective_object_id == SCENE_OBJECT_ID_GUARD);
    assert(!w.first_scene.tutorial_guard_talk_completed);
    assert(scene_interactions_should_show_tutorial_finger(&w, SCENE_OBJECT_ID_GUARD));
}

static void test_tutorial_finger_requires_current_objective(void) {
    World w = {0};
    scene_interactions_init_first_scene(&w);
    assert(scene_interactions_should_show_tutorial_finger(&w, SCENE_OBJECT_ID_GUARD));
    assert(!scene_interactions_should_show_tutorial_finger(&w, SCENE_OBJECT_ID_NONE));

    w.first_scene.objective_object_id = SCENE_OBJECT_ID_NONE;
    assert(!scene_interactions_should_show_tutorial_finger(&w, SCENE_OBJECT_ID_GUARD));

    w.first_scene.objective_object_id = SCENE_OBJECT_ID_GUARD;
    w.first_scene.tutorial_guard_talk_completed = true;
    assert(!scene_interactions_should_show_tutorial_finger(&w, SCENE_OBJECT_ID_GUARD));
}

static void test_hover_without_press_does_not_capture_pan(void) {
    World w = {0};
    scene_interactions_update_pointer_state(&w, SCENE_OBJECT_ID_GUARD, false, false, false);
    assert(w.first_scene.hovered_object_id == SCENE_OBJECT_ID_GUARD);
    assert(w.first_scene.pressed_object_id == SCENE_OBJECT_ID_NONE);
    assert(!scene_interactions_pointer_captures_pan(&w));
}

static void test_unknown_object_has_no_flags(void) {
    World w = {0};
    scene_interactions_init_first_scene(&w);
    assert(scene_interactions_visual_flags(&w, SCENE_OBJECT_ID_NONE) == 0U);
    assert(scene_interactions_find(SCENE_OBJECT_ID_NONE) == 0);
}

int main(void) {
    test_first_scene_defaults();
    test_guard_hit_bounds();
    test_guard_uses_mask_glow_highlight();
    test_visual_flags_compose_states();
    test_pointer_state_captures_pan_while_pressed();
    test_guard_release_opens_dialogue_without_completing_step();
    test_release_outside_does_not_complete_tutorial();
    test_tutorial_finger_requires_current_objective();
    test_hover_without_press_does_not_capture_pan();
    test_unknown_object_has_no_flags();
    return 0;
}
