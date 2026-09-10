#ifndef FEATURE_UI_KIT_METRICS_H
#define FEATURE_UI_KIT_METRICS_H

#include "features/ui_kit/ui_scale_policy.h"
#include "features/ui_kit/ui_tokens.h"

// The ONE place the interface's sizes come from. Every screen reads this struct
// instead of inventing its own share of the canvas, which is what keeps a HUD
// chip and a dialog button looking like two pieces of one surface.
//
// Legacy frames convert authored CSS sizes into canvas units. Relative frames
// use reference-design units for every authored size, including text and hits.
// Platform safe-area insets always keep their physical CSS-pixel meaning.

typedef struct {
    float view_w; // the logical canvas
    float view_h;
    float shortest; // its short edge: everything relative is a share of this
    float css;      // UI units per authored unit (CSS in legacy, reference in relative)

    // Type ramp, already in UI units.
    float t_display;
    float t_title;
    float t_body;
    float t_num;
    float t_badge;

    float rim;    // the kit's outline, on every plate
    float lift;   // the kit's button lift: the dark step under a plate
    float gap;    // rhythm between siblings
    float pad;    // rhythm inside a plate
    float margin; // from a plate to the frame edge
    float hit;    // minimum touch target; no interactive element is shorter

    // A dialog's width, already fitted to this canvas.
    float panel_w;

    // Insets nothing readable may cross, carrying the device's own safe area.
    float safe_l;
    float safe_r;
    float safe_t;
    float safe_b;
} ui_metrics_t;

// Opens a frame: the consumer's UI runtime calls this once per frame with the
// framebuffer it is about to draw into, and lays out on the canvas it returns.
// Everything below reads the frame this stored.
UiScaleFit ui_frame_begin(float fb_w, float fb_h, float dpr);

// Opt-in proportional layout. The available short edge fits reference_short;
// panel_min_w, panel_max_w and the legacy density floor/cap do not apply.
UiScaleFit ui_frame_begin_relative(float fb_w, float fb_h, float dpr, float reference_short);

ui_metrics_t ui_metrics(void);

// Converts an authored size: CSS pixels in legacy frames, reference units in
// relative frames. Device-pixel input and safe areas use the frame scale instead.
float ui_css(float css_px);

// Raw authored-unit conversion factor, following the active frame mode.
float ui_css_unit(void);

#endif /* FEATURE_UI_KIT_METRICS_H */
