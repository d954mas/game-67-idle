#include "systems/sys_move.h"

#include <math.h>

void sys_move(World *w, float dt, const game_input_frame_t *input) {
    float dx = 0.0F;
    float dz = 0.0F;
    if (input == NULL) { return; }
    if (input->move_up) { dz -= 1.0F; }
    if (input->move_down) { dz += 1.0F; }
    if (input->move_left) { dx -= 1.0F; }
    if (input->move_right) { dx += 1.0F; }

    if (dx != 0.0F || dz != 0.0F) {
        const float len = sqrtf(dx * dx + dz * dz);
        dx /= len;
        dz /= len;
        const float speed = 4.0F;
        w->player_x += dx * speed * dt;
        w->player_z += dz * speed * dt;
        w->player_yaw = atan2f(dx, dz);
    }
}
