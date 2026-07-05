#ifndef RB_DARK_RPG_SCENE_INTERACTIONS_H
#define RB_DARK_RPG_SCENE_INTERACTIONS_H

#include "scene/scene_layout.h"
#include "world/world.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum scene_object_kind_t {
    SCENE_OBJECT_KIND_CHARACTER = 0,
    SCENE_OBJECT_KIND_GATE = 1,
    SCENE_OBJECT_KIND_PROP = 2,
} scene_object_kind_t;

typedef enum scene_highlight_profile_t {
    SCENE_HIGHLIGHT_MASK_GLOW = 0,
} scene_highlight_profile_t;

enum {
    SCENE_INTERACTION_IDLE = 1U << 0U,
    SCENE_INTERACTION_OBJECTIVE = 1U << 1U,
    SCENE_INTERACTION_HOVERED = 1U << 2U,
    SCENE_INTERACTION_PRESSED = 1U << 3U,
};

typedef struct scene_interaction_object_t {
    const char *id;
    scene_object_kind_t kind;
    const char *location_id;
    const char *stable_id;
    const char *sprite_region_name;
    uint64_t sprite_region_hash;
    float sprite_target_h;
    scene_rect_t bounds;
    float anchor_x;
    float anchor_y;
    scene_highlight_profile_t highlight_profile;
    bool enabled;
} scene_interaction_object_t;

void scene_interactions_init_first_scene(World *w);
const scene_interaction_object_t *scene_interactions_all(const World *w, int *out_count);
const scene_interaction_object_t *scene_interactions_find(const World *w, const char *id);
bool scene_interactions_object_visible(const World *w, const scene_interaction_object_t *object);
const char *scene_interactions_hit_test(const World *w, float master_x, float master_y);
void scene_interactions_update_pointer_state(World *w, const char *hit_object_id,
                                             bool pointer_pressed, bool pointer_down, bool pointer_released);
bool scene_interactions_pointer_captures_pan(const World *w);
bool scene_interactions_should_show_tutorial_finger(const World *w, const char *id);
uint32_t scene_interactions_visual_flags(const World *w, const char *id);

#endif /* RB_DARK_RPG_SCENE_INTERACTIONS_H */
