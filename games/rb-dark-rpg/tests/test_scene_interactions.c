#include "scene/scene_interactions.h"

#include <assert.h>

static void test_first_scene_defaults(void) {
    World w = {0};
    scene_interactions_init_first_scene(&w);
    assert(w.first_scene.interactions_initialized);
    assert(w.first_scene.hovered_object_id == SCENE_OBJECT_ID_NONE);
    assert(w.first_scene.pressed_object_id == SCENE_OBJECT_ID_NONE);
    assert(w.first_scene.objective_object_id == SCENE_OBJECT_ID_GUARD);
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
    assert(!scene_interactions_pointer_captures_pan(&w));
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
    test_hover_without_press_does_not_capture_pan();
    test_unknown_object_has_no_flags();
    return 0;
}
