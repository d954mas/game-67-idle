#ifndef FEATURE_UI_KIT_METRICS_H
#define FEATURE_UI_KIT_METRICS_H

#include "features/ui_kit/ui_scale_policy.h"
#include "features/ui_kit/ui_reach.h"
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

    // What the screen is. NEITHER of these changes a size: the canvas above is
    // decided by the viewport alone, so the same window draws the same interface
    // on a phone and on a monitor and a reviewer can see both. They are here for
    // the decisions a size cannot make -- a row that becomes a column, a control
    // that moves under the thumb, a hover-only affordance that has to become a
    // visible button, a little more room around something a finger has to hit.
    bool in_hand;
    bool portrait;
} ui_metrics_t;

// Opens a frame: the consumer's UI runtime calls this once per frame with the
// framebuffer it is about to draw into, and lays out on the canvas it returns.
// The canvas is a proportion -- the sheet's `ref_short` authored units span the
// viewport's short edge, safe area excluded -- and the viewport is the ONLY
// input: the same window yields the same interface on any device.
// Everything below reads the frame this stored.
UiScaleFit ui_frame_begin(float fb_w, float fb_h, float dpr);

ui_metrics_t ui_metrics(void);

// The reference short edge this frame is laying out against.
float ui_reference_short(void);

// An authored size, in the units the frame lays out in. The conversion is the
// identity: a size IS a share of the reference short edge. It stays a call so
// game sources keep one spelling for "this number is an authored size", and so
// device-pixel input and CSS-pixel safe areas remain visibly different things.
float ui_css(float css_px);

// The authored-unit conversion factor.
float ui_css_unit(void);

#endif /* FEATURE_UI_KIT_METRICS_H */
