#ifndef FEATURE_UI_KIT_WORLD_LABEL_H
#define FEATURE_UI_KIT_WORLD_LABEL_H

// Plates standing in the world, on the UI's text stack. Two kinds exist and
// they differ in what can hide them. A billboard floats under the HUD in a low
// zIndex band and nothing in the scene hides it, so it stays above its object by
// placement. An anchored element tests scene depth: the UI's material must fetch
// it, and a frame without it must not show the element, or it would show through
// walls.

#include "clay.h"
#include "features/ui_kit/ui_metrics.h"
#include "features/ui_kit/ui_world_anchor.h"
#include "ui/nt_ui.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct ui_world_label_t {
    float near_m;   // nearer than this it is gone
    float focus_m;  // the distance scale 1 draws at, usually the camera's focus
    float margin;   // how far past the canvas's edge it survives, scaled
    int16_t z_near; // the zIndex at the eye; farther sinks by `z_per_m` a metre
    float z_per_m;
    float z_span;   // the deepest it sinks
} ui_world_label_t;

// Opens a floating, pointer-transparent element carried by a render transform to
// where `at` projects. Laid out at the canvas's centre, so layout culling never
// pops it. Declare the plate inside, then call ui_world_label_end; false means
// nothing was opened.
bool ui_world_label_begin(const ui_metrics_t *m, const float view_proj[16], const float at[3], const ui_world_label_t *label);
void ui_world_label_end(void);

// The element data placing an anchored element at `ndc_depth` (ui_world_project),
// visible only when `shown`: pass whether this frame's scene depth is live.
void *ui_world_depth_data(float ndc_depth, bool shown);

#endif /* FEATURE_UI_KIT_WORLD_LABEL_H */
