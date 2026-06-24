#ifndef BLOCKSIDE_VEHICLE_H
#define BLOCKSIDE_VEHICLE_H

typedef struct BlocksideVehicleState {
    float speed;
} BlocksideVehicleState;

typedef struct BlocksideVehiclePose {
    float x;
    float z;
    float yaw;
} BlocksideVehiclePose;

void blockside_vehicle_reset(BlocksideVehicleState *vehicle);
void blockside_vehicle_step(BlocksideVehicleState *vehicle,
                            BlocksideVehiclePose *pose,
                            float throttle,
                            float brake,
                            float steer,
                            float dt);
void blockside_vehicle_drive_toward(BlocksideVehicleState *vehicle,
                                    BlocksideVehiclePose *pose,
                                    float target_x,
                                    float target_z,
                                    unsigned frames,
                                    float dt);

#endif /* BLOCKSIDE_VEHICLE_H */
