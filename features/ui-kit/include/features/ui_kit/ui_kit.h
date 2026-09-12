#ifndef FEATURE_UI_KIT_WIDGETS_H
#define FEATURE_UI_KIT_WIDGETS_H

// The widget half of the design system: ui_theme.h holds the styles,
// ui_metrics.h holds the sizes, and this turns the two into the pieces a
// screen is built from. A screen that reaches past this to the engine
// widgets has to restate the layer order, the CSS conversion and the touch
// floor, and that is exactly how two screens start looking like two different
// games.
//
// Two tiers live here. The primitives (panel, tile, scrim, label, button
// plate, meter) are the surfaces; the composites below them (a labelled
// button, a counter, a badge, a sheet, a settings row) are the things a
// screen writes first.
//
// Text here takes `const char *`, not a localized handle: the kit does not know
// how its consumer localizes. A consumer with a localization wrapper keeps ONE
// place where its string type becomes a raw pointer and calls these from there.

#include "clay.h"
#include "ui/nt_ui.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_checkbox.h"
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
// An icon sits between the plate and the label, sharing the shadow slot: a
// button's art order is plate, icon, text without a second Clay zIndex.
#define UI_LAYER_ICON UI_LAYER_TEXT_SHADOW

// ---- primitives -----------------------------------------------------------

// A plate: the panel art with the kit's slice9 scale. The caller owns the
// declaration (position, size, padding); everything about the surface is here.
// A plate takes no id of its own (the engine assigns one); wrap it when the
// ui.tree needs a name.
void ui_kit_panel_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl);
void ui_kit_panel_end(nt_ui_context_t *ctx);

// The light card surface a list row or an item sits on.
void ui_kit_tile_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl);
void ui_kit_tile_end(nt_ui_context_t *ctx);

// The dim under a modal. Emit it before the plate. `occludes` makes it swallow
// the pointer, so the world under it stops reacting; without it the caller
// decides what stays interactive.
//
// It floats at zIndex 0 on purpose: the walker sorts one zIndex by UI layer,
// so plates the screen draws under it on a higher layer stay undimmed, but a
// panel at a higher zIndex is laid out after any engine slider thumb inside
// it, which the walker rejects. A sheet (ui_kit_sheet_begin) dims everything
// and holds no slider; a scrimmed panel holds the slider and dims the ground.
void ui_kit_scrim(nt_ui_context_t *ctx, bool occludes);

// Text at the kit's ramp. `style` is a theme label style, whose font_size is in
// CSS pixels; this is the only conversion point a screen needs.
void ui_kit_label(nt_ui_context_t *ctx, const char *text, const nt_ui_label_style_t *style);
void ui_kit_label_scaled(nt_ui_context_t *ctx, const char *text, const nt_ui_label_style_t *style, float factor);

// The same line with the kit's drop shadow under it: for text that has to stay
// readable over the world rather than over a plate. `id` plus `slot` only has to
// be unique among the shadowed labels of one frame. The `on_world` role is the
// outlined alternative, one label and no floating child.
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

// A static tinted container: the button plate (ledge and all) or the round
// thumb in `tint` 0xAABBGGRR, holding the caller's children like a tile does.
// Where a screen would reach for a Clay rectangle with a border: the engine
// draws those without antialiasing, the art is antialiased at every scale.
void ui_kit_plate_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, uint32_t tint);
void ui_kit_plate_end(nt_ui_context_t *ctx);
void ui_kit_disc_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, uint32_t tint);
void ui_kit_disc_end(nt_ui_context_t *ctx);

// The theme slider and scroll bar at this frame's size: the theme carries CSS
// pixels, the engine wants UI units. Pointers into a kit-owned copy that keeps
// the engine's memoized regions between frames; the copy follows a repaint.
nt_ui_slider_style_t *ui_kit_slider_style(const ui_metrics_t *m);
nt_ui_scroll_style_t *ui_kit_scroll_style(void);

// A packed 0xAABBGGRR token as the colour Clay takes.
Clay_Color ui_kit_color(uint32_t abgr);

// A row height no interactive element goes below, in UI units.
Clay_SizingAxis ui_kit_hit_height(const ui_metrics_t *m);

// ---- composites -----------------------------------------------------------

// The four button roles. A screen picks a role, never a colour.
typedef enum {
    UI_KIT_BUTTON_NEUTRAL = 0, // the light tile surface, dark ink
    UI_KIT_BUTTON_CONFIRM,     // yes, the main action, a purchase for coins (green)
    UI_KIT_BUTTON_AD,          // a rewarded ad, and nothing else (blue)
    UI_KIT_BUTTON_DANGER       // destructive (red)
} ui_kit_button_kind_t;

nt_ui_button_style_t *ui_kit_button_style(ui_kit_button_kind_t kind);
const nt_ui_label_style_t *ui_kit_button_label_style(ui_kit_button_kind_t kind);

// A labelled button in an element the caller sizes. `id` is the ui.tree name
// of that element; the plate underneath derives its own from it. Returns
// clicked.
bool ui_kit_button(nt_ui_context_t *ctx, const char *id, const char *label, ui_kit_button_kind_t kind, bool enabled,
                   Clay_SizingAxis w, Clay_SizingAxis h);
// The same with an icon before the label; `label` NULL makes it icon-only.
bool ui_kit_icon_button(nt_ui_context_t *ctx, const char *id, nt_atlas_region_ref_t *icon, const char *label,
                        ui_kit_button_kind_t kind, bool enabled, Clay_SizingAxis w, Clay_SizingAxis h);

// The round dismiss mobile players look for: a `hit`-sized neutral circle
// with the kit's close glyph in ink, or `label` (the cross the consumer's font
// packs) when no glyph is bound, floating on the top-right corner of the
// element that is open. A floating's zIndex is relative to the floating it
// sits in, so it lands in its panel's own band. Returns clicked.
bool ui_kit_close_button(nt_ui_context_t *ctx, const char *id, const char *label);

// A square icon on the icon layer, `size` UI units. The ref is by pointer
// because the engine memoizes the resolved region into it.
void ui_kit_icon(nt_ui_context_t *ctx, nt_atlas_region_ref_t *icon, float size);
// The same in a colour: the kit's glyphs are white masks, so ink on a light
// plate and white on an action fill are one region.
void ui_kit_icon_tinted(nt_ui_context_t *ctx, nt_atlas_region_ref_t *icon, float size, uint32_t tint);
// The theme's glyph by name, for `ui_kit_icon*` and the icon button. The
// pointer is into g_ui_theme, where the engine memoizes the resolved region.
nt_atlas_region_ref_t *ui_kit_icon_ref(ui_icon_t icon);

// A counter: tile plate, icon, number in the `counter` role. A wallet, a
// resource, a score.
void ui_kit_counter(nt_ui_context_t *ctx, const char *id, nt_atlas_region_ref_t *icon, const char *text);

// A meter with a caption riding on it, in an element of the given size.
// `caption` may be NULL.
void ui_kit_meter_captioned(nt_ui_context_t *ctx, const char *id, float w, float h, float ratio, uint32_t fill_tint,
                            const char *caption, const nt_ui_label_style_t *caption_style);

// A small round count pinned to the top-right corner of the open element.
void ui_kit_badge(nt_ui_context_t *ctx, const char *text);

// A titled plate: the panel with the header band across its top carrying
// `title` in the header role, the round close on the corner when `close_label`
// is given, and the body under it with the kit's padding. `id` names the
// plate in the ui.tree; the close derives "<id>/close". Returns whether the
// close was clicked this frame. A sheet is this inside the engine modal.
bool ui_kit_dialog_begin(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label,
                         Clay_SizingAxis w, Clay_SizingAxis h);
void ui_kit_dialog_end(nt_ui_context_t *ctx);

// A sheet: the engine modal (backdrop, occluder, Esc and backdrop close,
// open/close tween) carrying the kit's panel with a title row. Declare it at
// ROOT level, never inside a scroll or clip. `open` is the caller's bool; the
// sheet only ever clears it. Returns whether the body is declared this frame;
// then emit the body and call ui_kit_sheet_end. `title` NULL = no title row;
// `close_label` NULL = no round close on the corner (the caller's actions
// close it).
// `h` <= 0 fits the content.
bool ui_kit_sheet_begin(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label, bool *open,
                        float w, float h);
void ui_kit_sheet_end(nt_ui_context_t *ctx);
// Drops the sheet's tween state after it closed for good (a screen transition).
void ui_kit_sheet_clear(nt_ui_context_t *ctx, const char *id);

// The engine toggle, checkbox and radio in the kit's own art (theme fields
// `toggle`, `checkbox`, `radio`). Pointers, not copies: the engine memoizes
// the resolved atlas regions into the style it is handed.
nt_ui_checkbox_style_t *ui_kit_toggle_style(void);
nt_ui_checkbox_style_t *ui_kit_checkbox_style(void);
nt_ui_checkbox_style_t *ui_kit_radio_style(void);

// Settings rows, one touch height each. The label fills the row; the control
// sits at its end (toggle) or its start (checkbox, radio). Each returns true
// the frame its value changed.
bool ui_kit_toggle_row(nt_ui_context_t *ctx, const char *id, const char *label, bool *value, bool enabled);
bool ui_kit_checkbox_row(nt_ui_context_t *ctx, const char *id, const char *label, bool *value, bool enabled);
bool ui_kit_radio_row(nt_ui_context_t *ctx, const char *id, const char *label, int *selected, int my_value, bool enabled);

// A slider row the way the studio's settings screens lay it out: caption
// beside the track across a desk, above it in a hand. The caller formats the
// caption ("Музыка: 70%"); `row_width` is the row's own width in UI units.
// Returns true the frame the value changed.
bool ui_kit_slider_row(nt_ui_context_t *ctx, const char *id, const char *caption, float *value, bool enabled,
                       float row_width);

// A pick-one row: the label fills the row, the trigger names the current
// option and opens the list of all of them. `open` is the caller's bool for the
// list; `list_width` is the trigger's width in UI units. Returns true the frame
// `*selected` changed.
bool ui_kit_dropdown_row(nt_ui_context_t *ctx, const char *id, const char *label, const char *const *options,
                         int count, int *selected, bool *open, float list_width);

#endif /* FEATURE_UI_KIT_WIDGETS_H */
