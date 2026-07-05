#include "scene/scene_interactions.h"

#include "game_actions.h"
#include "game_content.h"
#include "game_dialogue.h"
#include "game_state.h"

#include <stddef.h>
#include <string.h>

enum { SCENE_INTERACTION_MAX_OBJECTS = 32 };

bool shop_screen_open_shop(const char *shop_id);
void world_map_screen_open_map(void);

static scene_interaction_object_t s_scene_objects[SCENE_INTERACTION_MAX_OBJECTS];

static bool str_eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static bool contains(scene_rect_t r, float x, float y) {
    return x >= (float)r.x && x <= (float)(r.x + r.w) && y >= (float)r.y && y <= (float)(r.y + r.h);
}

static const char *current_location_id(const World *w) {
    return w && w->player_state && w->player_state->world_current_location_id[0] != '\0'
               ? w->player_state->world_current_location_id
               : "hub_last_post";
}

static const game_location_object_t *current_location_object(const World *w, const char *object_id) {
    const game_location_definition_t *location = game_content_find_location(current_location_id(w));
    if (!location || !object_id) {
        return NULL;
    }
    for (int i = 0; i < location->object_count; ++i) {
        if (str_eq(location->objects[i].id, object_id)) {
            return &location->objects[i];
        }
    }
    return NULL;
}

static bool activate_scene_object(World *w, const char *object_id) {
    if (!w || !w->player_state || !object_id) {
        return false;
    }
    const game_location_object_t *object = current_location_object(w, object_id);
    const game_location_interaction_t *interaction =
        game_actions_select_location_interaction(w->player_state, object);
    if (!interaction) {
        return false;
    }
    if (str_eq(interaction->interaction_type, "dialogue") && interaction->dialogue_id) {
        return game_dialogue_open(w, interaction->dialogue_id);
    }
    if (str_eq(interaction->interaction_type, "shop") && interaction->shop_id) {
        return shop_screen_open_shop(interaction->shop_id);
    }
    if (str_eq(interaction->interaction_type, "open_screen")) {
        world_map_screen_open_map();
        return true;
    }
    return false;
}

static scene_object_kind_t scene_kind_for_location_object(const game_location_object_t *object) {
    if (!object || !object->kind) {
        return SCENE_OBJECT_KIND_PROP;
    }
    if (str_eq(object->kind, "npc") || str_eq(object->kind, "combat")) {
        return SCENE_OBJECT_KIND_CHARACTER;
    }
    if (str_eq(object->kind, "exit")) {
        return SCENE_OBJECT_KIND_GATE;
    }
    return SCENE_OBJECT_KIND_PROP;
}

static bool location_object_has_scene(const game_location_object_t *object) {
    return object && object->id && object->scene_enabled && object->scene_bounds.w > 0 &&
           object->scene_bounds.h > 0;
}

static int build_scene_objects(const World *w) {
    const game_location_definition_t *location = game_content_find_location(current_location_id(w));
    if (!location || !location->objects || location->object_count <= 0) {
        return 0;
    }
    GameState fallback_state;
    const GameState *state = w ? w->player_state : NULL;
    if (!state) {
        game_state_init_defaults(&fallback_state);
        state = &fallback_state;
    }

    int out_count = 0;
    for (int i = 0; i < location->object_count && out_count < SCENE_INTERACTION_MAX_OBJECTS; ++i) {
        const game_location_object_t *source = &location->objects[i];
        if (!location_object_has_scene(source) ||
            !game_actions_location_object_visible(state, source)) {
            continue;
        }

        scene_interaction_object_t *object = &s_scene_objects[out_count++];
        *object = (scene_interaction_object_t){
            .id = source->id,
            .kind = scene_kind_for_location_object(source),
            .location_id = location->id,
            .stable_id = source->id,
            .sprite_region_name = source->scene_sprite_region_name,
            .sprite_region_hash = source->scene_sprite_region_hash,
            .sprite_target_h = source->scene_sprite_target_h,
            .bounds = source->scene_bounds,
            .anchor_x = source->scene_anchor_x,
            .anchor_y = source->scene_anchor_y,
            .highlight_profile = SCENE_HIGHLIGHT_MASK_GLOW,
            .enabled = source->scene_enabled &&
                       game_actions_location_object_available(state, source),
        };
    }
    return out_count;
}

void scene_interactions_init_first_scene(World *w) {
    if (!w || w->first_scene.interactions_initialized) {
        return;
    }
    w->first_scene.hovered_object_id = NULL;
    w->first_scene.pressed_object_id = NULL;
    w->first_scene.activated_object_id = NULL;
    if (!w->first_scene.current_objective_text) {
        w->first_scene.current_objective_text = "Поговори со стражником";
    }
    w->first_scene.objective_object_id = "hub_last_post.gate_guard";
    w->first_scene.tutorial_guard_talk_completed = false;
    w->first_scene.interactions_initialized = true;
}

const scene_interaction_object_t *scene_interactions_all(const World *w, int *out_count) {
    const int count = build_scene_objects(w);
    if (out_count) {
        *out_count = count;
    }
    return s_scene_objects;
}

const scene_interaction_object_t *scene_interactions_find(const World *w, const char *id) {
    if (!id) {
        return NULL;
    }
    int count = 0;
    const scene_interaction_object_t *objects = scene_interactions_all(w, &count);
    for (int i = 0; i < count; ++i) {
        if (str_eq(objects[i].id, id)) {
            return &objects[i];
        }
    }
    return NULL;
}

bool scene_interactions_object_visible(const World *w, const scene_interaction_object_t *object) {
    return object && object->location_id && str_eq(current_location_id(w), object->location_id);
}

const char *scene_interactions_hit_test(const World *w, float master_x, float master_y) {
    int count = 0;
    const scene_interaction_object_t *objects = scene_interactions_all(w, &count);
    for (int i = count - 1; i >= 0; --i) {
        if (objects[i].enabled && scene_interactions_object_visible(w, &objects[i]) &&
            contains(objects[i].bounds, master_x, master_y)) {
            return objects[i].id;
        }
    }
    return NULL;
}

void scene_interactions_update_pointer_state(World *w, const char *hit_object_id,
                                             bool pointer_pressed, bool pointer_down, bool pointer_released) {
    if (!w) {
        return;
    }
    scene_interactions_init_first_scene(w);
    w->first_scene.activated_object_id = NULL;

    if (pointer_pressed) {
        w->first_scene.pressed_object_id = hit_object_id;
        w->first_scene.hovered_object_id = hit_object_id;
        return;
    }

    if (pointer_down) {
        w->first_scene.hovered_object_id = w->first_scene.pressed_object_id ? w->first_scene.pressed_object_id
                                                                            : hit_object_id;
        return;
    }

    if (pointer_released) {
        const char *pressed_object_id = w->first_scene.pressed_object_id;
        w->first_scene.pressed_object_id = NULL;
        w->first_scene.hovered_object_id = hit_object_id;
        if (pressed_object_id && str_eq(pressed_object_id, hit_object_id)) {
            w->first_scene.activated_object_id = hit_object_id;
            (void)activate_scene_object(w, hit_object_id);
        }
        return;
    }

    w->first_scene.pressed_object_id = NULL;
    w->first_scene.hovered_object_id = hit_object_id;
}

bool scene_interactions_pointer_captures_pan(const World *w) {
    return w && w->first_scene.pressed_object_id;
}

bool scene_interactions_should_show_tutorial_finger(const World *w, const char *id) {
    return w && id && str_eq(current_location_id(w), "hub_last_post") &&
           str_eq(w->first_scene.objective_object_id, id) &&
           !w->first_scene.tutorial_guard_talk_completed && !w->dialogue.open;
}

uint32_t scene_interactions_visual_flags(const World *w, const char *id) {
    if (!w || !id || !scene_interactions_find(w, id)) {
        return 0U;
    }
    const scene_interaction_object_t *object = scene_interactions_find(w, id);
    if (!object || !object->enabled || !scene_interactions_object_visible(w, object)) {
        return 0U;
    }
    uint32_t flags = SCENE_INTERACTION_IDLE;
    if (str_eq(w->first_scene.objective_object_id, id)) {
        flags |= SCENE_INTERACTION_OBJECTIVE;
    }
    if (str_eq(w->first_scene.hovered_object_id, id)) {
        flags |= SCENE_INTERACTION_HOVERED;
    }
    if (str_eq(w->first_scene.pressed_object_id, id)) {
        flags |= SCENE_INTERACTION_PRESSED;
    }
    return flags;
}
