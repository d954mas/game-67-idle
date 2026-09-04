#ifndef FEATURE_UI_KIT_METRICS_H
#define FEATURE_UI_KIT_METRICS_H

#include "features/ui_kit/ui_scale_policy.h"
#include "features/ui_kit/ui_tokens.h"

// The ONE place the interface's sizes come from. Every screen reads this struct
// instead of inventing its own share of the canvas, which is what keeps a HUD
// chip and a dialog button looking like two pieces of one surface.
//
// Two units live here and they must not be mixed up:
//   UI units - what Clay lays out in. The logical short edge is held near
//              tokens->short_edge_max, so a size written as a share of
//              `shortest` is the same share of any screen.
//   CSS px   - the only unit that is the same PHYSICAL size on a phone and on a
//              monitor. Readability floors and touch targets are stated in CSS
//              px and converted with `css`.
//
// The split that matters: TYPE and TOUCH TARGETS are physical, so they come
// from CSS px. PLATES and their rhythm are relative, so they come from a share
// of the canvas with a CSS-px clamp. Sizing a plate in raw CSS px overflows a
// phone; sizing type as a share of the canvas makes it unreadable in a hand.

typedef struct {
    float view_w; // the logical canvas
    float view_h;
    float shortest; // its short edge: everything relative is a share of this
    float css;      // UI units per CSS pixel

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

ui_metrics_t ui_metrics(void);

// A size stated in CSS pixels, in UI units. Use for anything the player has to
// read or hit; never state such a size in device pixels.
float ui_css(float css_px);

// UI units one CSS pixel is worth, for a caller that needs the raw factor.
float ui_css_unit(void);

#endif /* FEATURE_UI_KIT_METRICS_H */
