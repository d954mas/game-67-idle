#ifndef FEATURE_UI_KIT_SCALE_POLICY_H
#define FEATURE_UI_KIT_SCALE_POLICY_H

// How big one UI unit is on the glass. Pure arithmetic, no engine types, so the
// rule that decides how much of the screen the interface owns is testable
// without a window.
//
// One rule, and it is a proportion: `reference_short` units span the SHORT EDGE
// of the viewport, whatever that viewport is. The short edge is what the hand
// spans in either orientation and what a window loses first, so a size stated
// against it keeps its share of a phone, a laptop and a monitor alike.
//
// There is deliberately no density floor and no ceiling. A floor hands a small
// window a LARGER share of itself than a big one -- the same button eating a
// quarter of a short window and a sixth of a tall one -- which is the defect
// this rule exists to remove. A game that wants to be read closer states that
// as its own reference, not as a clamp in here.
//
// The engine's NT_UI_SCALE_EXPAND answers a different question: it fits a
// reference RECTANGLE inside the window, so a 9:16 window fits 1280 reference
// units across 360 pixels and every widget arrives at a quarter of its size.

typedef struct UiScaleFit {
    float scale;     // device pixels per UI unit
    float logical_w; // the canvas a UI system lays out on
    float logical_h;
} UiScaleFit;

// Safe-area sizes are platform CSS pixels; authored sizes use reference units.
// A positive viewport has no scale floor, so its layout stays proportional.
static inline UiScaleFit ui_scale_fit(float fb_w, float fb_h, float dpr, float reference_short,
                                      float safe_w_css, float safe_h_css) {
    if (!(dpr > 0.0F)) dpr = 1.0F;
    if (!(reference_short > 0.0F)) reference_short = 400.0F;
    if (!(fb_w > 0.0F)) fb_w = reference_short * dpr;
    if (!(fb_h > 0.0F)) fb_h = reference_short * dpr;
    float available_w = fb_w - (safe_w_css > 0.0F ? safe_w_css * dpr : 0.0F);
    float available_h = fb_h - (safe_h_css > 0.0F ? safe_h_css * dpr : 0.0F);
    // A fully occluded or unsized surface still needs a finite projection.
    if (!(available_w > 0.0F)) available_w = fb_w;
    if (!(available_h > 0.0F)) available_h = fb_h;
    const float scale = (available_w < available_h ? available_w : available_h) / reference_short;
    return (UiScaleFit){scale, fb_w / scale, fb_h / scale};
}

// UI units one CSS pixel is worth. A CSS pixel is the only unit that means the
// same PHYSICAL size on a phone and on a monitor, which is what the platform
// states safe-area insets in; authored sizes are reference units and need no
// conversion.
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
