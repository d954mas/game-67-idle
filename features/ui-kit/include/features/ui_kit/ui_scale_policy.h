#ifndef FEATURE_UI_KIT_SCALE_POLICY_H
#define FEATURE_UI_KIT_SCALE_POLICY_H

// How big one UI unit is on the glass. Pure arithmetic, no engine types, so the
// rule that decides whether the interface is readable in a hand is testable
// without a window.
//
// The engine's NT_UI_SCALE_EXPAND fits a REFERENCE RECTANGLE inside the window,
// which is the wrong question for a phone: a 9:16 window fits 1280 reference
// units across 360 pixels and every widget arrives at a quarter of its intended
// physical size. The right question is how much of the SHORT edge a widget
// should own, because the short edge is what the hand spans in either
// orientation.
//
// Three terms, in order:
//   expand - the window's own share of the reference short edge; it is what
//            grows the UI on a large monitor.
//   dpr    - a floor. A dense display reports more device pixels for the same
//            physical size, and without this floor the UI would shrink exactly
//            as the display got sharper.
//   cap    - the ceiling that holds the logical short edge at short_edge_max,
//            so "a third of the short edge" is a third of the phone whatever
//            its pixel count.

// A canvas is never scaled below this. A window small enough to need the floor
// is already unplayable; the floor exists so the projection stays finite.
#define UI_SCALE_MIN 0.25F

typedef struct UiScaleFit {
    float scale;     // device pixels per UI unit
    float logical_w; // the canvas a UI system lays out on
    float logical_h;
} UiScaleFit;

static inline UiScaleFit ui_scale_fit(float fb_w, float fb_h, float dpr, float ref_short,
                                      float short_edge_max) {
    if (!(fb_w > 0.0F)) {
        fb_w = 1280.0F;
    }
    if (!(fb_h > 0.0F)) {
        fb_h = 720.0F;
    }
    if (!(dpr > 0.0F)) {
        dpr = 1.0F;
    }
    if (!(ref_short > 0.0F)) {
        ref_short = 720.0F;
    }
    if (!(short_edge_max > 0.0F)) {
        short_edge_max = 480.0F;
    }
    const float short_side = fb_w < fb_h ? fb_w : fb_h;
    const float expand = short_side / ref_short;
    float scale = expand > dpr ? expand : dpr;
    const float cap = short_side / short_edge_max;
    if (scale > cap) {
        scale = cap;
    }
    if (!(scale > UI_SCALE_MIN)) {
        scale = UI_SCALE_MIN;
    }
    UiScaleFit fit;
    fit.scale = scale;
    fit.logical_w = fb_w / scale;
    fit.logical_h = fb_h / scale;
    return fit;
}

// UI units one CSS pixel is worth. A CSS pixel is the only unit that means the
// same PHYSICAL size on a phone and on a monitor, so every readability floor and
// every touch target is stated in CSS pixels and converted through this. Device
// pixels are not that unit: on a 3x phone a 24-device-pixel glyph is 8 CSS
// pixels tall, which is what makes a HUD unreadable in the hand.
static inline float ui_scale_css_unit(float scale, float dpr) {
    if (!(scale > 0.0F)) {
        scale = 1.0F;
    }
    if (!(dpr > 0.0F)) {
        dpr = 1.0F;
    }
    return dpr / scale;
}

#endif /* FEATURE_UI_KIT_SCALE_POLICY_H */
