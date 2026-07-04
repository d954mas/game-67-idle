#ifndef GAME_WORLD_H
#define GAME_WORLD_H

#include "entity/nt_entity.h"
#include "game_dialogue.h"
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
    scene_object_id_t activated_object_id;
    scene_object_id_t objective_object_id;
    bool tutorial_guard_talk_completed;
    const char *active_quest_id;
    int active_quest_status;
    const char *active_quest_current_step_id;
    const char *active_quest_completed_step_id;
    bool active_quest_completed_talk_step;
    bool active_quest_gate_guard_intro_seen;
    bool blacksmith_unlocked;
    bool gate_locked;
    bool contract_board_locked;
    const char *current_objective_text;
} FirstSceneState;

typedef struct GameState GameState;

typedef struct World {
    float time_seconds;
    FirstSceneState first_scene;
    dialogue_runtime_t dialogue;
    GameState *player_state;

    float player_x, player_z, player_yaw;
    nt_entity_t player_entity;
    bool player_spawned;

    // A static TEXTURED prop next to the player — shows the textured mesh path
    // (same cube mesh, a uv0 + u_texture material) beside the coloured one.
    nt_entity_t prop_entity;
    bool prop_spawned;
} World;

#endif /* GAME_WORLD_H */
