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
