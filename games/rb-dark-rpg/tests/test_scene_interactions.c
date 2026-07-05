#include "scene/scene_interactions.h"
#include "game_dialogue.h"
#include "game_state.h"

#include <assert.h>
#include <string.h>

static const char *GUARD_ID = "hub_last_post.gate_guard";

static void test_first_scene_defaults(void) {
    World w = {0};
    scene_interactions_init_first_scene(&w);
    assert(w.first_scene.interactions_initialized);
    assert(w.first_scene.hovered_object_id == 0);
    assert(w.first_scene.pressed_object_id == 0);
    assert(w.first_scene.activated_object_id == 0);
    assert(strcmp(w.first_scene.objective_object_id, GUARD_ID) == 0);
    assert(!w.first_scene.tutorial_guard_talk_completed);
    assert(scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));
}

static void test_guard_comes_from_location_scene_data(void) {
    World w = {0};
    const scene_interaction_object_t *guard = scene_interactions_find(&w, GUARD_ID);
    assert(guard != 0);
    assert(strcmp(guard->id, GUARD_ID) == 0);
    assert(strcmp(guard->stable_id, GUARD_ID) == 0);
    assert(strcmp(guard->location_id, "hub_last_post") == 0);
    assert(strcmp(guard->sprite_region_name, "hub_scene/last_post_guard") == 0);
    assert(guard->sprite_region_hash == 0xBA1197C949937606ULL);
    assert(guard->sprite_target_h == 196.0f);
    assert(guard->bounds.x == 584);
    assert(guard->bounds.y == 202);
    assert(guard->bounds.w == 144);
    assert(guard->bounds.h == 210);
    assert(guard->anchor_x == 656.0f);
    assert(guard->anchor_y == 212.0f);
}

static void test_disabled_scene_objects_are_not_shown(void) {
    World w = {0};
    int count = 0;
    const scene_interaction_object_t *objects = scene_interactions_all(&w, &count);
    assert(objects != 0);
    assert(count == 1);
    assert(strcmp(objects[0].id, GUARD_ID) == 0);
    assert(scene_interactions_find(&w, "hub_last_post.blacksmith") == 0);
    assert(scene_interactions_hit_test(&w, 448.0f, 280.0f) == 0);
}

static void test_guard_hit_bounds(void) {
    World w = {0};
    assert(strcmp(scene_interactions_hit_test(&w, 656.0f, 300.0f), GUARD_ID) == 0);
    assert(scene_interactions_hit_test(&w, 583.0f, 300.0f) == 0);
    assert(scene_interactions_hit_test(&w, 656.0f, 201.0f) == 0);
}

static void test_guard_hidden_outside_last_post(void) {
    World w = {0};
    GameState state;
    game_state_init_defaults(&state);
    w.player_state = &state;

    strncpy(state.world_current_location_id, "hub_gate_outskirts",
            sizeof state.world_current_location_id - 1);
    state.world_current_location_id[sizeof state.world_current_location_id - 1] = '\0';

    assert(scene_interactions_find(&w, GUARD_ID) == 0);
    assert(scene_interactions_hit_test(&w, 656.0f, 300.0f) == 0);
    assert(scene_interactions_visual_flags(&w, GUARD_ID) == 0U);
}

static void test_guard_uses_mask_glow_highlight(void) {
    World w = {0};
    const scene_interaction_object_t *guard = scene_interactions_find(&w, GUARD_ID);
    assert(guard != 0);
    assert(guard->highlight_profile == SCENE_HIGHLIGHT_MASK_GLOW);
}

static void test_visual_flags_compose_states(void) {
    World w = {0};
    scene_interactions_init_first_scene(&w);

    uint32_t flags = scene_interactions_visual_flags(&w, GUARD_ID);
    assert((flags & SCENE_INTERACTION_IDLE) != 0U);
    assert((flags & SCENE_INTERACTION_OBJECTIVE) != 0U);
    assert((flags & SCENE_INTERACTION_HOVERED) == 0U);
    assert((flags & SCENE_INTERACTION_PRESSED) == 0U);

    w.first_scene.hovered_object_id = GUARD_ID;
    w.first_scene.pressed_object_id = GUARD_ID;
    flags = scene_interactions_visual_flags(&w, GUARD_ID);
    assert((flags & SCENE_INTERACTION_HOVERED) != 0U);
    assert((flags & SCENE_INTERACTION_PRESSED) != 0U);
}

static void test_pointer_state_captures_pan_while_pressed(void) {
    World w = {0};
    scene_interactions_update_pointer_state(&w, GUARD_ID, true, true, false);
    assert(strcmp(w.first_scene.pressed_object_id, GUARD_ID) == 0);
    assert(strcmp(w.first_scene.hovered_object_id, GUARD_ID) == 0);
    assert(scene_interactions_pointer_captures_pan(&w));

    scene_interactions_update_pointer_state(&w, 0, false, true, false);
    assert(strcmp(w.first_scene.pressed_object_id, GUARD_ID) == 0);
    assert(strcmp(w.first_scene.hovered_object_id, GUARD_ID) == 0);
    assert(scene_interactions_pointer_captures_pan(&w));

    scene_interactions_update_pointer_state(&w, 0, false, false, true);
    assert(w.first_scene.pressed_object_id == 0);
    assert(w.first_scene.hovered_object_id == 0);
    assert(w.first_scene.activated_object_id == 0);
    assert(!scene_interactions_pointer_captures_pan(&w));
}

static void test_guard_release_opens_dialogue_without_completing_step(void) {
    World w = {0};
    GameState state;
    game_state_init_defaults(&state);
    w.player_state = &state;
    scene_interactions_update_pointer_state(&w, GUARD_ID, true, true, false);
    assert(scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));

    scene_interactions_update_pointer_state(&w, GUARD_ID, false, false, true);
    assert(strcmp(w.first_scene.activated_object_id, GUARD_ID) == 0);
    assert(w.first_scene.pressed_object_id == 0);
    assert(strcmp(w.first_scene.objective_object_id, GUARD_ID) == 0);
    assert(!w.first_scene.tutorial_guard_talk_completed);
    assert(!w.first_scene.active_quest_completed_talk_step);
    assert(!w.first_scene.active_quest_gate_guard_intro_seen);
    assert(w.dialogue.open);
    assert(w.dialogue.current_node != 0);
    assert(strcmp(w.dialogue.current_node->id, "start") == 0);
    assert(w.first_scene.current_objective_text != 0);
    assert(!scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));

    assert(game_dialogue_select_choice(&w, "ask_what_needed"));
    assert(w.dialogue.current_node != 0);
    assert(strcmp(w.dialogue.current_node->id, "explain_check") == 0);
    assert(game_dialogue_select_choice(&w, "accept"));
    assert(!w.dialogue.open);
    assert(w.first_scene.tutorial_guard_talk_completed);
    assert(w.first_scene.active_quest_completed_talk_step);
    assert(w.first_scene.active_quest_gate_guard_intro_seen);
    assert(w.first_scene.objective_object_id == 0);
    assert(!w.first_scene.blacksmith_unlocked);

    scene_interactions_update_pointer_state(&w, 0, false, false, false);
    assert(w.first_scene.activated_object_id == 0);
}

static void test_release_outside_does_not_complete_tutorial(void) {
    World w = {0};
    scene_interactions_update_pointer_state(&w, GUARD_ID, true, true, false);
    scene_interactions_update_pointer_state(&w, 0, false, false, true);

    assert(w.first_scene.activated_object_id == 0);
    assert(strcmp(w.first_scene.objective_object_id, GUARD_ID) == 0);
    assert(!w.first_scene.tutorial_guard_talk_completed);
    assert(scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));
}

static void test_modal_suppression_clears_press_before_release(void) {
    World w = {0};
    GameState state;
    game_state_init_defaults(&state);
    w.player_state = &state;

    scene_interactions_update_pointer_state(&w, GUARD_ID, true, true, false);
    assert(strcmp(w.first_scene.pressed_object_id, GUARD_ID) == 0);

    scene_interactions_update_pointer_state(&w, 0, false, false, false);
    assert(w.first_scene.pressed_object_id == 0);

    scene_interactions_update_pointer_state(&w, GUARD_ID, false, false, true);
    assert(w.first_scene.activated_object_id == 0);
    assert(!w.dialogue.open);
}

static void test_tutorial_finger_requires_current_objective(void) {
    World w = {0};
    scene_interactions_init_first_scene(&w);
    assert(scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));
    assert(!scene_interactions_should_show_tutorial_finger(&w, 0));

    w.first_scene.objective_object_id = 0;
    assert(!scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));

    w.first_scene.objective_object_id = GUARD_ID;
    w.first_scene.tutorial_guard_talk_completed = true;
    assert(!scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));
}

static void test_tutorial_finger_is_hidden_outside_last_post(void) {
    World w = {0};
    GameState state;
    game_state_init_defaults(&state);
    w.player_state = &state;
    scene_interactions_init_first_scene(&w);
    assert(scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));

    strncpy(state.world_current_location_id, "hub_gate_outskirts",
            sizeof state.world_current_location_id - 1);
    state.world_current_location_id[sizeof state.world_current_location_id - 1] = '\0';
    assert(!scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));

    strncpy(state.world_current_location_id, "old_mill",
            sizeof state.world_current_location_id - 1);
    state.world_current_location_id[sizeof state.world_current_location_id - 1] = '\0';
    assert(!scene_interactions_should_show_tutorial_finger(&w, GUARD_ID));
}

static void test_hover_without_press_does_not_capture_pan(void) {
    World w = {0};
    scene_interactions_update_pointer_state(&w, GUARD_ID, false, false, false);
    assert(strcmp(w.first_scene.hovered_object_id, GUARD_ID) == 0);
    assert(w.first_scene.pressed_object_id == 0);
    assert(!scene_interactions_pointer_captures_pan(&w));
}

static void test_unknown_object_has_no_flags(void) {
    World w = {0};
    scene_interactions_init_first_scene(&w);
    assert(scene_interactions_visual_flags(&w, 0) == 0U);
    assert(scene_interactions_find(&w, "missing.object") == 0);
}

int main(void) {
    test_first_scene_defaults();
    test_guard_comes_from_location_scene_data();
    test_disabled_scene_objects_are_not_shown();
    test_guard_hit_bounds();
    test_guard_hidden_outside_last_post();
    test_guard_uses_mask_glow_highlight();
    test_visual_flags_compose_states();
    test_pointer_state_captures_pan_while_pressed();
    test_guard_release_opens_dialogue_without_completing_step();
    test_release_outside_does_not_complete_tutorial();
    test_modal_suppression_clears_press_before_release();
    test_tutorial_finger_requires_current_objective();
    test_tutorial_finger_is_hidden_outside_last_post();
    test_hover_without_press_does_not_capture_pan();
    test_unknown_object_has_no_flags();
    return 0;
}
