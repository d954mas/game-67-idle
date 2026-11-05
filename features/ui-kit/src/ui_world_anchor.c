#include "features/ui_kit/ui_world_anchor.h"

#include <math.h>

bool ui_world_to_canvas(const float vp[16], const float point[3], float view_w, float view_h, float *out_x,
    float *out_y) {
    const float cx = vp[0] * point[0] + vp[4] * point[1] + vp[8] * point[2] + vp[12];
    const float cy = vp[1] * point[0] + vp[5] * point[1] + vp[9] * point[2] + vp[13];
    const float cw = vp[3] * point[0] + vp[7] * point[1] + vp[11] * point[2] + vp[15];
    if (!(cw > 1e-4F)) { return false; }
    *out_x = (cx / cw * 0.5F + 0.5F) * view_w;
    *out_y = (0.5F - cy / cw * 0.5F) * view_h;
    return isfinite(*out_x) && isfinite(*out_y);
}

bool ui_figure_box(const float view_proj[16], float x, float z, float height, float half_width, float view_w,
    float view_h, ui_figure_box_t *out) {
    float fx = 0.0F;
    float fy = 0.0F;
    float tx = 0.0F;
    float ty = 0.0F;
    if (!ui_world_to_canvas(view_proj, (const float[3]){x, 0.0F, z}, view_w, view_h, &fx, &fy) ||
        !ui_world_to_canvas(view_proj, (const float[3]){x, height, z}, view_w, view_h, &tx, &ty)) {
        return false;
    }
    *out = (ui_figure_box_t){.l = fminf(fx, tx) - half_width, .r = fmaxf(fx, tx) + half_width, .t = fminf(fy, ty),
        .b = fmaxf(fy, ty)};
    return true;
}

// Moves a world anchor forward in depth while keeping its projected X/Y, m.
#define WORLD_DEPTH_BIAS_M 0.2F
#define WORLD_DEPTH_FALLBACK_NDC 0.0002F
#define WORLD_NEAR_GUARD_NDC 0.0001F

static float biased_depth(const float vp[16], float ndc_depth, float clip_w) {
    float row_w = 0.0F;
    float row_z = 0.0F;
    for (int axis = 0; axis < 3; ++axis) {
        const int index = axis * 4;
        if (fabsf(vp[index + 3]) > fabsf(row_w)) {
            row_w = vp[index + 3];
            row_z = vp[index + 2];
        }
    }
    if (fabsf(row_w) < 1e-4F || clip_w <= WORLD_DEPTH_BIAS_M) { return ndc_depth - WORLD_DEPTH_FALLBACK_NDC; }
    // Perspective clip depth is affine in clip w plus a constant term.
    const float slope = row_z / row_w;
    const float intercept = vp[14] - slope * vp[15];
    if (!isfinite(slope) || !isfinite(intercept) || intercept >= 0.0F) { return ndc_depth - WORLD_DEPTH_FALLBACK_NDC; }
    const float biased = slope + intercept / (clip_w - WORLD_DEPTH_BIAS_M);
    return isfinite(biased) ? biased : ndc_depth - WORLD_DEPTH_FALLBACK_NDC;
}

bool ui_world_project(const float vp[16], const float point[3], float view_w, float view_h, float *out_x, float *out_y,
    float *ui_depth) {
    const float cx = vp[0] * point[0] + vp[4] * point[1] + vp[8] * point[2] + vp[12];
    const float cy = vp[1] * point[0] + vp[5] * point[1] + vp[9] * point[2] + vp[13];
    const float cz = vp[2] * point[0] + vp[6] * point[1] + vp[10] * point[2] + vp[14];
    const float cw = vp[3] * point[0] + vp[7] * point[1] + vp[11] * point[2] + vp[15];
    if (!isfinite(cw) || cw <= 1e-4F) { return false; }
    const float nx = cx / cw;
    const float ny = cy / cw;
    const float nz = cz / cw;
    if (!isfinite(nx) || !isfinite(ny) || !isfinite(nz) || fabsf(nx) > 1.0F || fabsf(ny) > 1.0F || nz < -1.0F || nz > 1.0F) {
        return false;
    }
    *out_x = (nx * 0.5F + 0.5F) * view_w;
    *out_y = (0.5F - ny * 0.5F) * view_h;
    *ui_depth = biased_depth(vp, nz, cw);
    return true;
}

float ui_world_depth_offset(float ui_depth) {
    const float target = fmaxf(ui_depth, UI_WORLD_SCREEN_NEAR_NDC + WORLD_NEAR_GUARD_NDC);
    // The standard 2D screen matrix negates UI z when mapping it to clip depth.
    return UI_WORLD_SCREEN_NEAR_NDC - target;
}

bool ui_world_billboard(const float vp[16], const float point[3], float view_w, float view_h, float near_m, float focus_m,
    float margin, ui_world_billboard_t *out) {
    const float cx = vp[0] * point[0] + vp[4] * point[1] + vp[8] * point[2] + vp[12];
    const float cy = vp[1] * point[0] + vp[5] * point[1] + vp[9] * point[2] + vp[13];
    const float cw = vp[3] * point[0] + vp[7] * point[1] + vp[11] * point[2] + vp[15];
    if (!(cw > near_m)) { return false; }
    const float scale = focus_m / cw;
    const float x = (cx / cw * 0.5F + 0.5F) * view_w;
    const float y = (0.5F - cy / cw * 0.5F) * view_h;
    if (!isfinite(x) || !isfinite(y) || !isfinite(scale) || !(scale > 0.0F)) { return false; }
    const float reach = margin * scale;
    if (x < -reach || x > view_w + reach || y < -reach || y > view_h + reach) { return false; }
    *out = (ui_world_billboard_t){.x = x, .y = y, .scale = scale, .distance = cw};
    return true;
}
