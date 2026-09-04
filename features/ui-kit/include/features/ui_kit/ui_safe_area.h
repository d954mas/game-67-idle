#ifndef FEATURE_UI_KIT_SAFE_AREA_H
#define FEATURE_UI_KIT_SAFE_AREA_H

// The strip of the canvas the device itself owns: the notch, the rounded
// corners and the iOS gesture bar. The browser reports it as
// env(safe-area-inset-*), the shell forwards it, and this is the only place the
// interface reads it. In CSS pixels, left/right/top/bottom, zero where there is
// none.
void ui_safe_area_insets_css(float out_lrtb[4]);

#endif /* FEATURE_UI_KIT_SAFE_AREA_H */
