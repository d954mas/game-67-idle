#ifndef FEATURE_UI_KIT_LOOK_H
#define FEATURE_UI_KIT_LOOK_H

// A game's face built from the kit's shapes: corner families, plates standing
// on a lift, rings, bars, labelled buttons. The colours, sizes and families are
// the game's data; what lives here is how they turn into shapes and layout.

#include "clay.h"
#include "features/ui_kit/ui_kit.h"
#include "features/ui_kit/ui_rgb.h"
#include "features/ui_kit/ui_shape.h"
#include "ui/nt_ui.h"
#include "ui/nt_ui_label.h"

#include <stdbool.h>
#include <stdint.h>

// ---- colour and text ------------------------------------------------------

// 0xRRGGBB at an opacity as the colour Clay takes.
Clay_Color ui_rgb_color(uint32_t rgb, float alpha);

// One line, no wrap, `rgb` in an outline of `stroke_em` x size each side
// (0 = no outline).
nt_ui_label_style_t ui_text_outlined(float size, uint32_t rgb, uint32_t stroke_rgb, float stroke_em);
// The same style at an opacity, its outline and shadow faded with it.
void ui_text_fade(nt_ui_label_style_t *style, float alpha);
// Shrinks style->font_size, never below `min_size`, until `text` fits one line
// `width` wide with its outline; no room at all (width <= 0) gives `min_size`.
void ui_text_fit_width(nt_ui_context_t *ctx, const char *text, nt_ui_label_style_t *style, float width, float min_size);

// ---- corner families ------------------------------------------------------

// One family of plates: a corner and contour, UI units. A consumer keeps a
// table of them, one per shape in its look.
typedef struct ui_look_family_t {
    float radius, border;
    bool gradient;         // false: the face is one colour, its `bottom`
    float lift;            // the lift a plate of the family stands on
    float frame_r, face_r; // framed: the frame's outer corners, the face inside it
    uint32_t ink;          // contour and lift, 0xRRGGBB
    float gloss_cap;       // how far a gloss band's rounded ends reach
} ui_look_family_t;

// A face `height` tall in the family's corners and ink, no lift. `radius` and
// `outline` above 0 replace the family's. A face shorter than its two corners
// squeezes them, contour and all, so a thin bar stays a bar.
ui_kit_panel_style_t ui_look_face(const ui_look_family_t *family, uint32_t top, uint32_t bottom, float height,
    float radius, float outline);
// `fill` inside a contour of `frame`: the family's outer corners round a tighter face.
ui_kit_panel_style_t ui_look_framed(const ui_look_family_t *family, uint32_t fill, uint32_t frame, float height);
// A round plate: a `rim` wide ring of `ink` round the fill, the lift a crescent under it.
ui_kit_radial_style_t ui_look_disc(uint32_t top, uint32_t bottom, float rim, float lift, uint32_t ink);

// ---- plates ---------------------------------------------------------------

// A styled panel whose fixed height in `decl` is the face: the look's lift adds
// under it and the children centre on the face, not the whole. `decl` may carry
// the id and the floating.
void ui_kit_lifted_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, const ui_kit_panel_style_t *look);
void ui_kit_lifted_end(nt_ui_context_t *ctx);
// The same for a round plate: `decl` sizes the face square.
void ui_kit_lifted_disc_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, const ui_kit_radial_style_t *look);
void ui_kit_lifted_disc_end(nt_ui_context_t *ctx);

// An arc `thickness` wide on a circle `outer` across, centred on the open
// element: `share` of a turn from `start` turns clockwise from twelve o'clock.
void ui_kit_arc(nt_ui_context_t *ctx, float outer, float thickness, float start, float share, uint32_t rgb, float alpha);

// A flat round well: `fill` inside a `rim` wide ring. A leaf; `decl` sizes it
// with a fixed width and places it.
void ui_kit_well(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, uint32_t fill, float fill_alpha,
    uint32_t rim_rgb, float rim_alpha, float rim);

// A bar in a family's contour: `track` under a fill from the left to `ratio` in
// the `top` to `bottom` gradient. `radius` 0 keeps the family's corners, and a
// fill shorter than them starts at their width, never squeezed out of round.
typedef struct ui_bar_t {
    const ui_look_family_t *family;
    float w, h, ratio;
    uint32_t track, top, bottom;
    float radius, outline, alpha;
    const char *caption; // centred on the bar; NULL = none
    const nt_ui_label_style_t *caption_style;
} ui_bar_t;
void ui_kit_bar(nt_ui_context_t *ctx, const char *id, const ui_bar_t *bar);

// ---- buttons --------------------------------------------------------------

// A labelled button in `look`: an element named `id`, `w` wide and `h` plus the
// lift tall, `text` `pad` from each side. Disabled draws `disabled_style` when
// `style` has an outline and it is given. Returns clicked.
bool ui_kit_text_button_styled(nt_ui_context_t *ctx, const char *id, const ui_kit_button_look_t *look, Clay_SizingAxis w,
    float h, const char *text, const nt_ui_label_style_t *style, const nt_ui_label_style_t *disabled_style, float pad,
    bool enabled);

#endif /* FEATURE_UI_KIT_LOOK_H */
