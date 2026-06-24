#ifndef GAME_WORLD_H
#define GAME_WORLD_H

// The World: the single source of truth that systems read/write — entity handles,
// camera, sim time. Systems never own entities or talk to each other directly;
// they go through the World. Starts minimal; a game grows it with per-system SoA
// blocks (positions, velocities, …) and handle pools.
typedef struct World {
    float time_seconds;
} World;

#endif /* GAME_WORLD_H */
