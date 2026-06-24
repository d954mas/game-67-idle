#ifndef GAME_SYS_MOVE_H
#define GAME_SYS_MOVE_H

#include "world/world.h"

// Movement system: turns input (WASD) into the character's position/yaw in the
// World. Knows nothing about rendering — a separate system draws it.
void sys_move(World *w, float dt);

#endif /* GAME_SYS_MOVE_H */
