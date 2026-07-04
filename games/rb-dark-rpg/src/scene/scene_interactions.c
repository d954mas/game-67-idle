#include "scene/scene_interactions.h"

#include <stddef.h>

static const scene_interaction_object_t SCENE_OBJECTS[] = {
    {
        .id = SCENE_OBJECT_ID_GUARD,
        .kind = SCENE_OBJECT_KIND_CHARACTER,
        .stable_id = "last_post/guard",
        .bounds = {.x = 584, .y = 202, .w = 144, .h = 210},
        .anchor_x = 656.0F,
        .anchor_y = 212.0F,
        .highlight_profile = SCENE_HIGHLIGHT_MASK_GLOW,
        .enabled = true,
    },
};

static bool contains(scene_rect_t r, float x, float y) {
    return x >= (float)r.x && x <= (float)(r.x + r.w) && y >= (float)r.y && y <= (float)(r.y + r.h);
}

void scene_interactions_init_first_scene(World *w) {
    if (!w || w->first_scene.interactions_initialized) {
        return;
    }
    w->first_scene.hovered_object_id = SCENE_OBJECT_ID_NONE;
    w->first_scene.pressed_object_id = SCENE_OBJECT_ID_NONE;
    w->first_scene.objective_object_id = SCENE_OBJECT_ID_GUARD;
    w->first_scene.interactions_initialized = true;
}

const scene_interaction_object_t *scene_interactions_all(int *out_count) {
    if (out_count) {
        *out_count = (int)(sizeof SCENE_OBJECTS / sizeof SCENE_OBJECTS[0]);
    }
    return SCENE_OBJECTS;
}

const scene_interaction_object_t *scene_interactions_find(scene_object_id_t id) {
    int count = 0;
    const scene_interaction_object_t *objects = scene_interactions_all(&count);
    for (int i = 0; i < count; ++i) {
        if (objects[i].id == id) {
            return &objects[i];
        }
    }
    return NULL;
}

scene_object_id_t scene_interactions_hit_test(float master_x, float master_y) {
    int count = 0;
    const scene_interaction_object_t *objects = scene_interactions_all(&count);
    for (int i = count - 1; i >= 0; --i) {
        if (objects[i].enabled && contains(objects[i].bounds, master_x, master_y)) {
            return objects[i].id;
        }
    }
    return SCENE_OBJECT_ID_NONE;
}

void scene_interactions_update_pointer_state(World *w, scene_object_id_t hit_object_id,
                                             bool pointer_pressed, bool pointer_down, bool pointer_released) {
    if (!w) {
        return;
    }
    scene_interactions_init_first_scene(w);

    if (pointer_pressed) {
        w->first_scene.pressed_object_id = hit_object_id;
        w->first_scene.hovered_object_id = hit_object_id;
        return;
    }

    if (pointer_down) {
        w->first_scene.hovered_object_id =
            (w->first_scene.pressed_object_id != SCENE_OBJECT_ID_NONE) ? w->first_scene.pressed_object_id : hit_object_id;
        return;
    }

    if (pointer_released) {
        w->first_scene.pressed_object_id = SCENE_OBJECT_ID_NONE;
        w->first_scene.hovered_object_id = hit_object_id;
        return;
    }

    w->first_scene.pressed_object_id = SCENE_OBJECT_ID_NONE;
    w->first_scene.hovered_object_id = hit_object_id;
}

bool scene_interactions_pointer_captures_pan(const World *w) {
    return w && w->first_scene.pressed_object_id != SCENE_OBJECT_ID_NONE;
}

uint32_t scene_interactions_visual_flags(const World *w, scene_object_id_t id) {
    if (!w || id == SCENE_OBJECT_ID_NONE || !scene_interactions_find(id)) {
        return 0U;
    }
    uint32_t flags = SCENE_INTERACTION_IDLE;
    if (w->first_scene.objective_object_id == id) {
        flags |= SCENE_INTERACTION_OBJECTIVE;
    }
    if (w->first_scene.hovered_object_id == id) {
        flags |= SCENE_INTERACTION_HOVERED;
    }
    if (w->first_scene.pressed_object_id == id) {
        flags |= SCENE_INTERACTION_PRESSED;
    }
    return flags;
}
