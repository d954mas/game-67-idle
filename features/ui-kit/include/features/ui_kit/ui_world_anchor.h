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

// A world point that stands on screen: inside the view frustum on all axes,
// false (outputs untouched) otherwise. `ui_depth` is its clip depth moved 0.2 m
// toward the eye, for ui_world_depth_offset.
bool ui_world_project(const float view_proj[16], const float point[3], float view_w, float view_h, float *out_x,
    float *out_y, float *ui_depth);
// The same for a point that may lie off to a side of the frame, as the anchor of an object still partly on screen does;
// false only behind the eye or outside the depth range.
bool ui_world_project_any(const float view_proj[16], const float point[3], float view_w, float view_h, float *out_x,
    float *out_y, float *ui_depth);

// The clip depth the screen view projection puts fixed HUD on, just inside the
// near plane; a depth-testing HUD view_proj sets its [14] to this.
#define UI_WORLD_SCREEN_NEAR_NDC -0.999F
// The z offset that lands an element laid out on the HUD's depth at `ui_depth`.
float ui_world_depth_offset(float ui_depth);

// A plate fixed in the world, without a depth test. A camera-facing plane under
// perspective projects as exactly a translate and a uniform scale, so this is a
// billboard and its text stays the UI's: scale 1 at `focus_m` from the eye.
typedef struct ui_world_billboard_t {
    float x, y, scale;
    float distance; // metres from the eye along the view, for ordering
} ui_world_billboard_t;
// False nearer than `near_m`, or past the canvas's edge by more than `margin` x scale.
bool ui_world_billboard(const float view_proj[16], const float point[3], float view_w, float view_h, float near_m,
    float focus_m, float margin, ui_world_billboard_t *out);

#endif /* FEATURES_UI_KIT_UI_WORLD_ANCHOR_H */
