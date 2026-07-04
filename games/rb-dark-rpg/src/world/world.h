#ifndef GAME_WORLD_H
#define GAME_WORLD_H

#include "entity/nt_entity.h"
#include "scene/scene_interaction_types.h"

#include <stdbool.h>

// The World: the single source of truth that systems read/write — entity handles,
// camera, sim state. Systems never own entities or call each other directly; they
// go through the World. Starts minimal; a game grows it with per-system SoA blocks.
//
// The sample character below shows the rule: its STATE lives here, while MOVEMENT
// (systems/sys_move) and RENDERING (render/render_mesh) are two SEPARATE systems
// that both operate on this state.
typedef struct FirstSceneState {
    float camera_center_x, camera_center_y;
    bool camera_initialized;
    bool interactions_initialized;
    scene_object_id_t hovered_object_id;
    scene_object_id_t pressed_object_id;
    scene_object_id_t objective_object_id;
} FirstSceneState;

typedef struct World {
    float time_seconds;
    FirstSceneState first_scene;

    float player_x, player_z, player_yaw;
    nt_entity_t player_entity;
    bool player_spawned;

    // A static TEXTURED prop next to the player — shows the textured mesh path
    // (same cube mesh, a uv0 + u_texture material) beside the coloured one.
    nt_entity_t prop_entity;
    bool prop_spawned;
} World;

#endif /* GAME_WORLD_H */
