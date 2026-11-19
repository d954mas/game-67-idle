#include "garment/pose.h"
#include "garment/cloth.h"

#include <math.h>

static bool finite_point(const float p[3]) {
    return p && isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2]);
}

static bool valid_range(float inactive, float full) {
    return isfinite(inactive) && isfinite(full) && inactive >= 0 && full >= 0 && inactive != full;
}

static float smooth_weight(double value, float inactive, float full) {
    double u = fmin(1.0, fmax(0.0, (value - (double)inactive) / ((double)full - (double)inactive)));
    return (float)(u * u * (3.0 - 2.0 * u));
}

bool cloth_pose_distance(const float a[3], const float b[3], float inactive_m, float full_m, float *weight) {
    if (!weight || !finite_point(a) || !finite_point(b) || !valid_range(inactive_m, full_m)) return false;
    double x = (double)a[0] - (double)b[0], y = (double)a[1] - (double)b[1], z = (double)a[2] - (double)b[2];
    *weight = smooth_weight(sqrt(x*x + y*y + z*z), inactive_m, full_m);
    return true;
}

bool cloth_pose_angle(const float a[3], const float pivot[3], const float b[3],
                      float inactive_deg, float full_deg, float *weight) {
    if (!weight || !finite_point(a) || !finite_point(pivot) || !finite_point(b) ||
        !valid_range(inactive_deg, full_deg) || inactive_deg > 180 || full_deg > 180) return false;
    double u[3], v[3];
    for (int i = 0; i < 3; ++i) {
        u[i] = (double)a[i] - (double)pivot[i];
        v[i] = (double)b[i] - (double)pivot[i];
    }
    double cx = u[1]*v[2] - u[2]*v[1], cy = u[2]*v[0] - u[0]*v[2], cz = u[0]*v[1] - u[1]*v[0];
    double cross_length = sqrt(cx*cx + cy*cy + cz*cz);
    double dot = u[0]*v[0] + u[1]*v[1] + u[2]*v[2];
    if (dot == 0 && cross_length == 0) return false;
    double degrees = atan2(cross_length, dot) * 57.29577951308232;
    *weight = smooth_weight(degrees, inactive_deg, full_deg);
    return true;
}

bool cloth_pose_offsets(int vertices, const ClothPoseOffset *layers, int layer_count, float max_abs_m, float *offsets_m) {
    if (vertices < 1 || vertices > CLOTH_VERTICES || layer_count < 0 || (layer_count && !layers) ||
        !isfinite(max_abs_m) || max_abs_m < 0 || !offsets_m) return false;
    double sums[CLOTH_VERTICES] = {0};
    for (int i = 0; i < layer_count; ++i) {
        const ClothPoseOffset *layer = &layers[i];
        if (!layer->weights || !isfinite(layer->offset_m) || !isfinite(layer->activation) ||
            layer->activation < 0 || layer->activation > 1) return false;
        double amplitude = (double)layer->offset_m * (double)layer->activation;
        for (int v = 0; v < vertices; ++v) {
            float mask = layer->weights[v];
            if (!isfinite(mask) || mask < 0 || mask > 1) return false;
            sums[v] += amplitude * (double)mask;
        }
    }
    for (int v = 0; v < vertices; ++v)
        offsets_m[v] = (float)fmin((double)max_abs_m, fmax(-(double)max_abs_m, sums[v]));
    return true;
}
