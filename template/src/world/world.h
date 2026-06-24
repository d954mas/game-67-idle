#ifndef GAME_WORLD_H
#define GAME_WORLD_H

#include "entity/nt_entity.h"

#include <stdbool.h>

// The World: the single source of truth that systems read/write — entity handles,
// camera, sim state. Systems never own entities or call each other directly; they
// go through the World. Starts minimal; a game grows it with per-system SoA blocks.
//
// The sample character below shows the rule: its STATE lives here, while MOVEMENT
// (systems/sys_move) and RENDERING (render/render_mesh) are two SEPARATE systems
// that both operate on this state.
typedef struct World {
    float time_seconds;

    float player_x, player_z, player_yaw;
    nt_entity_t player_entity;
    bool player_spawned;
} World;

#endif /* GAME_WORLD_H */
