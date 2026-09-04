#ifndef FEATURE_UI_KIT_TOKENS_H
#define FEATURE_UI_KIT_TOKENS_H

#include <stdint.h>

// The design tokens the whole interface is built from: colour, type, geometry.
// One struct, because three consumers have to agree on the same numbers — the
// art generator (tools/gen_ui_kit.py), the engine styles (ui_theme.c) and the
// per-frame metrics (ui_metrics.c). Anything that disagrees is a repaint that
// only half happened.
//
// Colours are packed 0xAABBGGRR, the engine's tint convention.
//
// SIZES ARE CSS PIXELS. A CSS pixel is the only unit that is the same PHYSICAL
// size on a phone and on a monitor, so a readability floor or a touch target
// stated here means the same thing on both. ui_metrics converts them into the
// UI units Clay lays out in.

typedef struct {
    // Base surfaces: one hue, three steps.
    uint32_t shell;     // outer rim of a panel
    uint32_t panel;     // panel body
    uint32_t inset;     // recessed areas: slider track, preview wells
    uint32_t inset_rim;
    uint32_t scrim; // dimming under a modal

    // The light card surface a list row or an item sits on.
    uint32_t tile;
    uint32_t tile_dim;
    uint32_t tile_rim;

    // Text.
    uint32_t ink;      // on a light tile
    uint32_t ink_soft;
    uint32_t on_panel;
    uint32_t on_panel_soft;

    // Currency and actions.
    uint32_t coin;
    uint32_t go;
    uint32_t info;
    uint32_t danger;
    uint32_t off;

    // Type ramp, CSS pixels.
    float t_display; // modal title
    float t_title;   // section heading, tab
    float t_body;    // label, item name
    float t_num;     // price, balance
    float t_badge;   // badge, state caption
    float t_row;     // list row name
    float t_row_sub; // list row second line

    // Geometry rhythm, CSS pixels.
    float rim;  // one outline thickness across the system
    float lift; // the pressable ledge under a button
    float gap;  // rhythm between siblings
    float pad;  // rhythm inside a plate
    float hit;  // minimum touch target

    // A dialog is as wide as the frame allows minus its margins, but never
    // wider than one comfortable reading measure nor narrower than its
    // controls. CSS pixels.
    float panel_min_w;
    float panel_max_w;

    // Canvas rule (ui_scale_policy.h): the reference short edge the window's
    // own share is measured against, and the cap the logical short edge is held
    // at. UI units, not CSS pixels — these two DEFINE the unit.
    float ref_short;
    float short_edge_max;

    // The kit's slice9 art ships above its on-screen size, so a style drawing
    // it must shrink the baked borders by this factor. 1 / export scale.
    float slice9_scale;
} ui_tokens_t;

// The studio's default look: what a new prototype wears before anyone repaints
// it. Mirrors tokens/studio_default.json, which the art generator reads; the
// two are held together by tests/tokens_parity.test.mjs.
const ui_tokens_t *ui_tokens_studio_default(void);

#endif /* FEATURE_UI_KIT_TOKENS_H */
