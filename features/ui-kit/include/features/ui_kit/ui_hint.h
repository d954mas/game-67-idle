#ifndef FEATURE_UI_KIT_HINT_H
#define FEATURE_UI_KIT_HINT_H

// First-minute hints drawn over a running game: the move keys, a finger pushing
// the stick, and the prompt to click a page that has no keyboard focus. Only the
// drawing is here. When a hint shows, how it fades and where the hero and the
// stick stand are the game's: it passes them in every frame.

#include "clay.h"
#include "features/ui_kit/ui_look.h"
#include "features/ui_kit/ui_metrics.h"
#include "ui/nt_ui.h"

#include <stdbool.h>
#include <stdint.h>

// Sizes are UI units unless named _px (device pixels) or _s (seconds).
typedef struct ui_hint_t {
    // This frame.
    float alpha;           // the whole hint's fade, 0..1
    float clock;           // running seconds, for the breath and the beats
    float device_per_unit; // device pixels per UI unit; 0 = not known
    bool desktop;          // keys and smaller labels instead of the stick's
    // The game's art and face.
    nt_atlas_region_ref_t *hand; // a pointing hand; NULL draws a white pad at the fingertip
    float hand_tip_x, hand_tip_y; // the fingertip in the art, shares of its size from the top left
    const ui_look_family_t *label_family, *key_family;
    uint32_t ink;
    float stroke_em;
    // The breath and the drag.
    float breath_hz, breath_scale, drag_s;
    // The label plate.
    uint32_t label_top, label_bottom;
    float lift, label_min_w, label_h, label_h_desk, label_text, label_text_desk, label_text_min;
    // The key caps and where they stand from the hero.
    uint32_t key_top, key_bottom, key_frame;
    float key, key_gap, keys_from_hero, keys_below, keys_below_narrow, keys_label_gap, keys_label_w;
    float edge_px, foot_clear, edge;
    // The stick.
    uint32_t line_rim;
    float line_rim_w, line_tip, line_head, knob_share, stick_label_gap, stick_label_w, hand_size, hand_max_px;
    uint32_t pad_bottom;
    // The click prompt.
    uint32_t ring_rgb;
    float click_dim, click_gap, click_ring, click_ring_px, click_label_w, click_label_h, click_text, click_text_min;
    float click_hand, click_hand_max_px, click_hand_min, click_beat_s, click_press_s, click_press, click_ring_s;
    int16_t click_z; // over every HUD layer
} ui_hint_t;

// The sizes, beats and colours the studio's hints share. The consumer sets what
// has no default: this frame's values, `hand`, the two families, `ink`.
ui_hint_t ui_hint_defaults(void);

// `label` over a W / A S D cluster (`keys` in that order) left of the hero's
// canvas point, level with it; where that leaves the window, centred under it.
void ui_hint_move_keys(nt_ui_context_t *ctx, const ui_hint_t *hint, const ui_metrics_t *m, float hero_x, float hero_y,
    const char *label, const char *const keys[4]);

// A knob dragged up a line by a fingertip, from the stick's centre at (x, y) on
// its ring of `radius`, with `label` over the line's tip.
void ui_hint_stick_drag(nt_ui_context_t *ctx, const ui_hint_t *hint, const ui_metrics_t *m, float x, float y, float radius,
    const char *label);

// The whole window dimmed, HUD included, and one group in its middle: a hand
// tapping over `text`. Everything lets the click through, since the click is what
// gives the canvas focus. On a short window the hand shrinks, to `click_hand_min`.
void ui_hint_click_prompt(nt_ui_context_t *ctx, const ui_hint_t *hint, const ui_metrics_t *m, const char *text);

#endif /* FEATURE_UI_KIT_HINT_H */
