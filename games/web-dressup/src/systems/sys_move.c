#include "systems/sys_move.h"

#include "input/nt_input.h"

#include <math.h>

void sys_move(World *w, float dt) {
    float dx = 0.0F;
    float dz = 0.0F;
    if (nt_input_key_is_down(NT_KEY_W)) { dz -= 1.0F; }
    if (nt_input_key_is_down(NT_KEY_S)) { dz += 1.0F; }
    if (nt_input_key_is_down(NT_KEY_A)) { dx -= 1.0F; }
    if (nt_input_key_is_down(NT_KEY_D)) { dx += 1.0F; }

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
