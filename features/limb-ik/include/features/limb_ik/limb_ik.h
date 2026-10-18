#ifndef FEATURES_LIMB_IK_LIMB_IK_H
#define FEATURES_LIMB_IK_LIMB_IK_H

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct limb_ik_point {
    float x;
    float y;
} limb_ik_point_t;

typedef struct limb_ik_solution {
    limb_ik_point_t joint;
    limb_ik_point_t end;
} limb_ik_solution_t;

/* Coordinates are root-local Y-up. bend chooses a side of the root-to-target axis. */
static inline bool limb_ik_solve_2link(limb_ik_point_t root, limb_ik_point_t target,
                                       float upper_length, float lower_length, int bend,
                                       limb_ik_solution_t *out) {
    if (out == NULL || !isfinite(root.x) || !isfinite(root.y) ||
        !isfinite(target.x) || !isfinite(target.y) ||
        !isfinite(upper_length) || !isfinite(lower_length) ||
        upper_length <= 0.0F || lower_length <= 0.0F ||
        (bend != -1 && bend != 1)) {
        return false;
    }

    const double upper = (double)upper_length;
    const double lower = (double)lower_length;
    const double dx = (double)target.x - (double)root.x;
    const double dy = (double)target.y - (double)root.y;
    const double target_distance = hypot(dx, dy);
    const double axis_x = target_distance > 0.0 ? dx / target_distance : 0.0;
    const double axis_y = target_distance > 0.0 ? dy / target_distance : -1.0;
    const double reach = fmax(fabs(upper - lower), fmin(upper + lower, target_distance));
    const double along = reach > 0.0
        ? (upper * upper - lower * lower + reach * reach) / (2.0 * reach)
        : 0.0;
    const double across = sqrt(fmax(0.0, upper * upper - along * along)) * (double)bend;
    const double joint_x = (double)root.x + axis_x * along - axis_y * across;
    const double joint_y = (double)root.y + axis_y * along + axis_x * across;
    const double end_x = (double)root.x + axis_x * reach;
    const double end_y = (double)root.y + axis_y * reach;

    if (!isfinite(joint_x) || !isfinite(joint_y) ||
        !isfinite(end_x) || !isfinite(end_y) ||
        fabs(joint_x) > (double)FLT_MAX || fabs(joint_y) > (double)FLT_MAX ||
        fabs(end_x) > (double)FLT_MAX || fabs(end_y) > (double)FLT_MAX) {
        return false;
    }

    *out = (limb_ik_solution_t){
        .joint = {(float)joint_x, (float)joint_y},
        .end = {(float)end_x, (float)end_y},
    };
    return true;
}

#endif
