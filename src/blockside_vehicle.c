#include "blockside_vehicle.h"

#include <math.h>

void blockside_vehicle_reset(BlocksideVehicleState *vehicle) {
    vehicle->speed = 0.0F;
}

void blockside_vehicle_step(BlocksideVehicleState *vehicle,
                            BlocksideVehiclePose *pose,
                            float throttle,
                            float brake,
                            float steer,
                            float dt) {
    const float accel = 8.0F;
    const float reverse_accel = 3.6F;
    const float brake_power = 11.0F;
    const float drag = 1.8F;
    const float max_forward = 9.0F;
    const float max_reverse = -3.2F;

    vehicle->speed += throttle * accel * dt;
    vehicle->speed -= brake * reverse_accel * dt;
    if (brake > 0.0F && vehicle->speed > 0.0F) {
        vehicle->speed -= brake_power * brake * dt;
    }
    vehicle->speed -= vehicle->speed * drag * dt;
    if (vehicle->speed > max_forward) {
        vehicle->speed = max_forward;
    } else if (vehicle->speed < max_reverse) {
        vehicle->speed = max_reverse;
    }
    if (fabsf(vehicle->speed) < 0.02F) {
        vehicle->speed = 0.0F;
    }

    const float speed01 = fminf(1.0F, fabsf(vehicle->speed) / max_forward);
    pose->yaw += steer * (0.85F + speed01 * 1.9F) * dt;
    pose->x += sinf(pose->yaw) * vehicle->speed * dt;
    pose->z += cosf(pose->yaw) * vehicle->speed * dt;
}

void blockside_vehicle_drive_toward(BlocksideVehicleState *vehicle,
                                    BlocksideVehiclePose *pose,
                                    float target_x,
                                    float target_z,
                                    unsigned frames,
                                    float dt) {
    for (unsigned i = 0; i < frames; ++i) {
        const float dx = target_x - pose->x;
        const float dz = target_z - pose->z;
        const float desired = atan2f(dx, dz);
        float diff = desired - pose->yaw;
        while (diff > 3.1415926F) { diff -= 6.2831853F; }
        while (diff < -3.1415926F) { diff += 6.2831853F; }
        const float steer = fmaxf(-1.0F, fminf(1.0F, diff * 1.8F));
        const float dist2 = dx * dx + dz * dz;
        blockside_vehicle_step(vehicle, pose, dist2 > 1.4F ? 1.0F : 0.0F, dist2 < 2.6F ? 0.8F : 0.0F, steer, dt);
    }
}
