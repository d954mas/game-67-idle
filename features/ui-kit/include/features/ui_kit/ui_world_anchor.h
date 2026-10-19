#ifndef FEATURES_UI_KIT_UI_WORLD_ANCHOR_H
#define FEATURES_UI_KIT_UI_WORLD_ANCHOR_H

#include <stdbool.h>

// A world point on the canvas (y down) through the camera render drew with;
// false behind the camera. UI that stands text over the world anchors here.
bool ui_world_to_canvas(const float view_proj[16], const float point[3], float view_w, float view_h, float *out_x,
    float *out_y);

// The canvas box of an upright figure standing at (x, z), `height` metres
// tall, widened by `half_width` canvas units each side; false behind the camera.
typedef struct ui_figure_box_t {
    float l, t, r, b;
} ui_figure_box_t;
bool ui_figure_box(const float view_proj[16], float x, float z, float height, float half_width, float view_w,
    float view_h, ui_figure_box_t *out);

#endif /* FEATURES_UI_KIT_UI_WORLD_ANCHOR_H */
