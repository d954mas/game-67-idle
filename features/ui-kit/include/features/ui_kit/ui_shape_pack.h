#ifndef FEATURES_UI_KIT_UI_SHAPE_PACK_H
#define FEATURES_UI_KIT_UI_SHAPE_PACK_H

// The styles of the kit's SDF shapes and their packing into the uber shader's
// per-vertex block, plus the few layout rules the widgets derive from them.
// Pure arithmetic with no engine types, so all of it is testable without a
// window. The drawing half is ui_shape.h.
//
// Sizes are UI units, the same units a Clay layout is in; colours are 0xRRGGBB
// and opaque, the shape's opacity being `alpha` alone, so a fade stays one
// channel. A zero-initialised style draws nothing: set `alpha`.

#include <stdbool.h>
#include <stdint.h>

typedef struct ui_kit_panel_style_t {
    float radius[4];      // corner radii: tl, tr, br, bl
    uint32_t top_rgb;     // the fill's top colour
    uint32_t bottom_rgb;  // its bottom colour; equal to top for a flat fill
    float outline_w;      // the contour inside the edge; 0 for none
    uint32_t outline_rgb;
    float face_radius;    // the fill's corner radius inside the contour; 0 = radius - outline_w
    float lip_h;          // the step under the face, inside the box; 0 for none
    uint32_t lip_rgb;
    // The lip inside the contour, as the kit's button art draws it: the
    // contour runs round the whole box and the face stands lip_h above its
    // bottom rim. `press` does not apply; the engine button moves the box.
    bool lip_inside;
    float press;          // 0..1: the face sinks onto the lip
    float highlight_w;    // a white band just inside the contour
    float highlight_a;
    float gloss_h;        // a white band across the top of the fill; 0 for none
    float gloss_inset;    // from the fill's sides
    float gloss_top;      // down from the fill's top
    float gloss_cap;      // how far each rounded end reaches across; 0 = gloss_h / 2
    float gloss_a;
    float checker_px;     // a white checker's cell side over the fill; 0 for none
    float checker_a;
    bool overlay;         // only the checker, gloss and highlight, over what is beneath
    float alpha;
} ui_kit_panel_style_t;

// An ellipse in the element's box, or a ring or wedge of it.
typedef struct ui_kit_radial_style_t {
    float angle_start;    // y-up radians, 0 = +X, counter-clockwise
    float angle_end;      // a full turn from start draws the whole shape
    float inner_norm;     // 0 = a disc; (0, 1) = a ring of that inner radius
    uint32_t top_rgb;
    uint32_t bottom_rgb;
    float outline_w;      // a rim inside the edge
    uint32_t outline_rgb;
    float lip_h;          // the step under the face: the box's own ellipse
    uint32_t lip_rgb;
    float gloss_a;        // an ellipse 72% x 30% of the inside, 6% from its top
    bool overlay;
    float alpha;
} ui_kit_radial_style_t;

#define UI_KIT_SHAPE_MODE_PANEL 1U
#define UI_KIT_SHAPE_MODE_RADIAL 2U
// Lengths travel as quarter units in 12 bits.
#define UI_KIT_SHAPE_MAX_LENGTH 1023.75F

// The three attributes after a_layout: a_shape, a_paint, a_fx.
typedef struct ui_kit_shape_block_t {
    float shape[4];
    float paint[4];
    float fx[4];
} ui_kit_shape_block_t;

void ui_kit_pack_panel(const ui_kit_panel_style_t *style, ui_kit_shape_block_t *out);
void ui_kit_pack_radial(const ui_kit_radial_style_t *style, ui_kit_shape_block_t *out);
// The top colour at the style's alpha as the engine's packed tint (0xAABBGGRR).
uint32_t ui_kit_shape_tint(uint32_t top_rgb, float alpha);

// ---- rules the ui_kit.h widgets build on, kept here to stay testable ----

typedef enum {
    UI_KIT_LOOK_IDLE = 0,
    UI_KIT_LOOK_HOVER,
    UI_KIT_LOOK_PRESSED,
    UI_KIT_LOOK_DISABLED
} ui_kit_look_state_t;

// A button's look in one state, derived from its idle look. Pressed sinks the
// face onto a lip drawn outside it (one shape, no second plate); with the lip
// inside, or none, the engine button moves the box and the bottom colour drops
// 12%. Disabled is the flat `off_rgb` (0xRRGGBB) with its lip a step darker.
// Hover is the idle look: the engine button eases the scale.
ui_kit_panel_style_t ui_kit_look_state(const ui_kit_panel_style_t *look, ui_kit_look_state_t state, uint32_t off_rgb);

// Whether a body scrolls this frame, from its content height of the last one.
// It starts past `room` and stops only below `room - band`: a scroll bar or a
// rewrap that moves the content by less than `band` cannot flip the scissor
// on and off every frame.
bool ui_kit_scroll_engaged(bool was, float content_h, float room, float band);

// The cells of a segmented row: `count` equal cells across `width`, `gap`
// apart. Returns the horizontal hit padding each cell may keep, `hit_pad` at
// most, so no cell's touch zone reaches into its neighbour's.
typedef struct {
    float x;
    float w;
} ui_kit_cell_t;
float ui_kit_choice_cells(int count, float width, float gap, float hit_pad, ui_kit_cell_t *out);

#endif
