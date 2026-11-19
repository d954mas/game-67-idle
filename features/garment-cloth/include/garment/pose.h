#ifndef GARMENT_POSE_H
#define GARMENT_POSE_H

#include <stdbool.h>

/* Sample points in one space. Distances are meters, angles are degrees at the pivot. */
bool cloth_pose_distance(const float a[3], const float b[3], float inactive_m, float full_m, float *weight);
bool cloth_pose_angle(const float a[3], const float pivot[3], const float b[3],
                      float inactive_deg, float full_deg, float *weight);

typedef struct {
    const float *weights; /* Immutable mask in [0,1] per proxy vertex. */
    float offset_m, activation;
} ClothPoseOffset;

/* The sum is clamped once after blending. Invalid inputs leave the destination untouched. */
bool cloth_pose_offsets(int vertices, const ClothPoseOffset *layers, int layer_count, float max_abs_m, float *offsets_m);

#endif
