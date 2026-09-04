#ifndef FEATURE_UI_KIT_WIDGETS_H
#define FEATURE_UI_KIT_WIDGETS_H

// The widget half of the design system: ui_theme.h holds the styles,
// ui_metrics.h holds the sizes, and this turns the two into the handful of
// pieces a screen is built from. A screen that reaches past this to the engine
// widgets has to restate the layer order, the CSS conversion and the touch
// floor, and that is exactly how two screens start looking like two different
// games.
//
// Text here takes `const char *`, not a localized handle: the kit does not know
// how its consumer localizes. A consumer with a localization wrapper keeps ONE
// place where its string type becomes a raw pointer and calls these from there.

#include "clay.h"
#include "ui/nt_ui.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_slider.h"

#include "features/ui_kit/ui_metrics.h"
#include "features/ui_kit/ui_theme.h"

#include <stdbool.h>

// Z-order inside one Clay zIndex: the walker batches rects and images first,
// then text, so a lower layer draws behind. Every surface uses THESE numbers;
// a screen with its own layer constants sorts against a different scale and
// cannot be stacked predictably against one that uses the kit's.
#define UI_LAYER_SCRIM 0
#define UI_LAYER_BG 1
#define UI_LAYER_FILL 2
#define UI_LAYER_IMG 3
#define UI_LAYER_TEXT_SHADOW 4
#define UI_LAYER_TEXT 5

// A plate: the panel art with the kit's slice9 scale. The caller owns the
// declaration (position, size, padding); everything about the surface is here.
void ui_kit_panel_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl);
void ui_kit_panel_end(nt_ui_context_t *ctx);

// The light card surface a list row or an item sits on.
void ui_kit_tile_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl);
void ui_kit_tile_end(nt_ui_context_t *ctx);

// The dim under a modal. Emit it before the plate; it blocks nothing, so the
// caller still decides what stays interactive.
void ui_kit_scrim(nt_ui_context_t *ctx);

// Text at the kit's ramp. `style` is a theme label style, whose font_size is in
// CSS pixels; this is the only conversion point a screen needs.
void ui_kit_label(nt_ui_context_t *ctx, const char *text, const nt_ui_label_style_t *style);
void ui_kit_label_scaled(nt_ui_context_t *ctx, const char *text, const nt_ui_label_style_t *style, float factor);

// The same line with the kit's drop shadow under it: for text that has to stay
// readable over the world rather than over a plate. `id` plus `slot` only has to
// be unique among the shadowed labels of one frame.
void ui_kit_label_shadowed(nt_ui_context_t *ctx, const char *id, int slot, const char *text,
                           const nt_ui_label_style_t *style);

// A label style at the size it was authored in. Use when a caller must adjust a
// style (a colour lerp, an alignment) before emitting it.
nt_ui_label_style_t ui_text(const nt_ui_label_style_t *kit_style);
nt_ui_label_style_t ui_text_scaled(const nt_ui_label_style_t *kit_style, float factor);

// The button plate. The caller owns the outer element that carries the stable
// ui.tree id and the outer size; this fills it, centres the content, and is the
// one place the press feel lives. Emit the label between begin and end.
void ui_kit_button_begin(nt_ui_context_t *ctx, uint32_t id, nt_ui_button_style_t *style, bool enabled,
                         const nt_ui_events_cfg_t *cfg);
bool ui_kit_button_end(nt_ui_context_t *ctx);

// A meter: the kit's recessed track with a tinted pill riding inside it, drawn
// to fill the element the caller has open. `ratio` is clamped to 0..1,
// `fill_tint` is 0xAABBGGRR, and the two sizes are UI units.
void ui_kit_meter(nt_ui_context_t *ctx, float w, float h, float ratio, uint32_t fill_tint);

// The theme slider at this frame's size: the theme carries CSS pixels, the
// engine wants UI units, and the track grows to whatever row holds it.
nt_ui_slider_style_t ui_kit_slider_style(const ui_metrics_t *m);

// A row height no interactive element goes below, in UI units.
Clay_SizingAxis ui_kit_hit_height(const ui_metrics_t *m);

#endif /* FEATURE_UI_KIT_WIDGETS_H */
