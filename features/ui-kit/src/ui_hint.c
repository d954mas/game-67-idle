#include "features/ui_kit/ui_hint.h"

#include <math.h>
#include <string.h>

// A line's rounded bars: each starts further along and is wider.
#define HINT_STROKE_STEPS 4U

ui_hint_t ui_hint_defaults(void) {
    return (ui_hint_t){.alpha = 1.0F, .hand_tip_x = 0.23F, .hand_tip_y = 0.11F, .stroke_em = 0.15F,
        .breath_hz = 0.6F, .breath_scale = 0.06F, .drag_s = 1.8F,
        .label_top = 0x293C60U, .label_bottom = 0x16233DU, .lift = 4.0F, .label_min_w = 120.0F, .label_h = 48.0F,
        .label_h_desk = 32.0F, .label_text = 30.0F, .label_text_desk = 18.0F, .label_text_min = 14.0F,
        .key_top = 0x234A72U, .key_bottom = 0x041127U, .key_frame = 0x7296B4U, .key = 64.0F, .key_gap = 6.0F,
        .keys_from_hero = 110.0F, .keys_below = 60.0F, .keys_below_narrow = 110.0F, .keys_label_gap = 14.0F,
        .keys_label_w = 160.0F, .edge_px = 40.0F, .foot_clear = 30.0F, .edge = 12.0F,
        .line_rim = 0x16233DU, .line_rim_w = 4.0F, .line_tip = 4.0F, .line_head = 14.0F, .knob_share = 0.42F,
        .stick_label_gap = 18.0F, .stick_label_w = 140.0F, .hand_size = 81.0F, .hand_max_px = 134.0F, .pad_bottom = 0xC9CDD8U,
        .ring_rgb = 0xFFE15AU, .click_dim = 0.6F, .click_gap = 8.0F, .click_ring = 48.0F, .click_ring_px = 5.0F,
        .click_label_w = 560.0F, .click_label_h = 64.0F, .click_text = 36.0F, .click_text_min = 20.0F,
        .click_hand = 140.0F, .click_hand_max_px = 240.0F, .click_hand_min = 80.0F, .click_beat_s = 1.0F,
        .click_press_s = 0.12F, .click_press = 10.0F, .click_ring_s = 0.8F, .click_z = 100};
}

// A slow, small breath: the hint's scale and brightness.
static float breath(const ui_hint_t *h) { return 0.5F + 0.5F * sinf(h->clock * h->breath_hz * 6.2831853F); }

static Clay_String clay_str(const char *id) { return (Clay_String){.length = (int32_t)strlen(id), .chars = id}; }

static void float_at(const char *id, float x, float y, float w, float h, int16_t z) {
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){.id = Clay_GetElementId(clay_str(id)),
        .floating = {.attachTo = CLAY_ATTACH_TO_ROOT, .offset = {x, y}, .zIndex = z, .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH},
        .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}});
}

static void float_end(void) { Clay__CloseElement(); }

static nt_ui_label_style_t text_style(const ui_hint_t *h, float size) { return ui_text_outlined(size, 0xFFFFFFU, h->ink, h->stroke_em); }

static float label_height(const ui_hint_t *h) { return h->desktop ? h->label_h_desk : h->label_h; }

static void text_label(nt_ui_context_t *ctx, const char *text, const nt_ui_label_style_t *style) {
    nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT), text != NULL ? text : "", style);
}

// A short line on the label plate centred on (x, y), breathing with the hint.
static void label_at(nt_ui_context_t *ctx, const ui_hint_t *h, const char *id, const char *text, float x, float y, float width) {
    width = fmaxf(width, h->label_min_w);
    const float height = label_height(h);
    float_at(id, x - width * 0.5F, y - height * 0.5F, width, height + h->lift, 0);
    const float shade = 0.92F + 0.12F * breath(h);
    ui_kit_panel_style_t look = ui_look_face(h->label_family, ui_rgb_shaded(h->label_top, shade),
        ui_rgb_shaded(h->label_bottom, shade), height, 0.0F, 0.0F);
    look.lip_h = h->lift;
    look.alpha = h->alpha;
    ui_kit_lifted_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(height)}, .padding = {16, 16, 0, 0},
        .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}, &look);
    nt_ui_label_style_t style = text_style(h, h->desktop ? h->label_text_desk : h->label_text);
    ui_text_fade(&style, h->alpha);
    ui_text_fit_width(ctx, text, &style, width - 32.0F, h->label_text_min);
    text_label(ctx, text, &style);
    ui_kit_lifted_end(ctx);
    float_end();
}

// A key cap, breathing with the hint.
static void key_cap(nt_ui_context_t *ctx, const ui_hint_t *h, const char *id, const char *letter, float x, float y) {
    const float size = h->key * (1.0F + h->breath_scale * breath(h));
    float_at(id, x + (h->key - size) * 0.5F, y + (h->key - size) * 0.5F, size, size + h->lift, 0);
    const float shade = 0.9F + 0.15F * breath(h);
    ui_kit_panel_style_t cap = ui_look_face(h->key_family, ui_rgb_shaded(h->key_top, shade), ui_rgb_shaded(h->key_bottom, shade), size, 0.0F, 0.0F);
    cap.outline_rgb = h->key_frame;
    cap.lip_h = h->lift;
    cap.alpha = h->alpha;
    ui_kit_lifted_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)},
        .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}, &cap);
    // The letter fills about 60% of the cap so it reads at a glance.
    nt_ui_label_style_t style = text_style(h, size * 0.6F);
    ui_text_fade(&style, h->alpha);
    text_label(ctx, letter, &style);
    ui_kit_lifted_end(ctx);
    float_end();
}

// A hint's margin from the window's edges.
static float window_margin(const ui_hint_t *h) {
    return h->device_per_unit > 0.0F ? fmaxf(h->edge_px / h->device_per_unit, h->edge) : h->edge_px;
}

void ui_hint_move_keys(nt_ui_context_t *ctx, const ui_hint_t *h, const ui_metrics_t *m, float hero_x, float hero_y,
    const char *label, const char *const keys[4]) {
    const float cluster_w = 3.0F * h->key + 2.0F * h->key_gap;
    const float cluster_h = 2.0F * h->key + h->key_gap;
    const float foot = m->view_h - m->safe_b - h->edge - h->foot_clear - cluster_h * 0.5F;
    float cx = hero_x - h->keys_from_hero - cluster_w * 0.5F;
    float cy = fminf(foot, hero_y + h->keys_below);
    if (cx - cluster_w * 0.5F < m->safe_l + window_margin(h)) {
        cx = hero_x;
        cy = fminf(foot, hero_y + h->keys_below_narrow + cluster_h * 0.5F);
    }
    label_at(ctx, h, "hint/keys_label", label, cx, cy - cluster_h * 0.5F - h->keys_label_gap - label_height(h) * 0.5F, h->keys_label_w);
    const float x = cx - cluster_w * 0.5F;
    const float y = cy - cluster_h * 0.5F;
    const float mid = x + h->key + h->key_gap;
    const float low = y + h->key + h->key_gap;
    key_cap(ctx, h, "hint/key_0", keys[0], mid, y);
    key_cap(ctx, h, "hint/key_1", keys[1], x, low);
    key_cap(ctx, h, "hint/key_2", keys[2], mid, low);
    key_cap(ctx, h, "hint/key_3", keys[3], mid + h->key + h->key_gap, low);
}

// The hand's drawn size for an art `size` across shown at most `max_px` device pixels.
static float hand_units(const ui_hint_t *h, float size, float max_px) {
    return fminf(size, h->device_per_unit > 0.0F ? max_px / h->device_per_unit : size);
}

// A fingertip pressing at (x, y): the hand's art when given, else a white pad.
static void finger_at(nt_ui_context_t *ctx, const ui_hint_t *h, const char *id, float x, float y, float hand, int16_t z) {
    hand *= 1.0F + h->breath_scale * breath(h);
    float_at(id, x - hand * h->hand_tip_x, y - hand * h->hand_tip_y, hand, hand, z);
    if (h->hand != NULL) {
        ui_kit_icon_tinted(ctx, h->hand, hand, ui_rgb_tint(0xFFFFFFU, h->alpha));
    } else {
        const float pad = hand * 0.5F;
        ui_kit_radial_style_t look = ui_look_disc(0xFFFFFFU, h->pad_bottom, 3.0F, 3.0F, h->ink);
        look.alpha = h->alpha;
        ui_kit_lifted_disc_begin(ctx, &(Clay_ElementDeclaration){.floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
            .offset = {hand * h->hand_tip_x - pad * 0.5F, hand * h->hand_tip_y - pad * 0.5F},
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH},
            .layout = {.sizing = {CLAY_SIZING_FIXED(pad), CLAY_SIZING_FIXED(pad)}}}, &look);
        ui_kit_lifted_disc_end(ctx);
    }
    float_end();
}

// 0..1..0 over a drag cycle, eased at both ends.
static float drag_phase(const ui_hint_t *h) {
    const float t = fmodf(h->clock, h->drag_s) / h->drag_s;
    const float out = t < 0.5F ? t * 2.0F : 2.0F - t * 2.0F;
    return out * out * (3.0F - 2.0F * out);
}

static void stroke_bar(const ui_hint_t *h, uint32_t index, float x, float y0, float y1, float width, uint32_t rgb) {
    const float left = x - width * 0.5F, top = fminf(y0, y1) - width * 0.5F;
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){.id = Clay_GetElementIdWithIndex(clay_str("hint/line"), index),
        .floating = {.attachTo = CLAY_ATTACH_TO_ROOT, .offset = {left, top}, .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH},
        .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(fabsf(y1 - y0) + width)}},
        .backgroundColor = ui_rgb_color(rgb, h->alpha), .cornerRadius = CLAY_CORNER_RADIUS(width * 0.5F)});
    Clay__CloseElement();
}

// An upright line from (x, y0) down to (x, y1), `tail` wide at the start to
// `wide` at the end; the rims all go under the cores, which reads as one outline.
static void stroke(const ui_hint_t *h, float x, float y0, float y1, float tail, float wide) {
    for (uint32_t pass = 0U; pass < 2U; ++pass) {
        for (uint32_t index = 0U; index < HINT_STROKE_STEPS; ++index) {
            const float from = (float)index / (float)HINT_STROKE_STEPS;
            const float width = tail + (wide - tail) * (float)index / (float)(HINT_STROKE_STEPS - 1U);
            const float sy = y0 + (y1 - y0) * from;
            if (pass == 0U) { stroke_bar(h, index, x, sy, y1, width + 2.0F * h->line_rim_w, h->line_rim); }
            else { stroke_bar(h, HINT_STROKE_STEPS + index, x, sy, y1, width, 0xFFFFFFU); }
        }
    }
}

void ui_hint_stick_drag(nt_ui_context_t *ctx, const ui_hint_t *h, const ui_metrics_t *m, float x, float y, float radius,
    const char *label) {
    const float p = drag_phase(h);
    const float knob = radius * h->knob_share;
    const float tip = y - knob - radius;
    stroke(h, x, tip, y, h->line_tip, h->line_head);
    const float ky = y - radius * 0.7F * p;
    float_at("hint/knob", x - knob, ky - knob, 2.0F * knob, 2.0F * knob + h->lift, 0);
    ui_kit_radial_style_t ghost = ui_look_disc(0xFFFFFFU, h->pad_bottom, 4.0F, h->lift, h->ink);
    ghost.alpha = h->alpha;
    ui_kit_lifted_disc_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(2.0F * knob), CLAY_SIZING_FIXED(2.0F * knob)}}}, &ghost);
    ui_kit_lifted_disc_end(ctx);
    float_end();
    label_at(ctx, h, "hint/stick_label", label, fmaxf(x, m->safe_l + h->edge + h->stick_label_w * 0.5F),
        tip - h->line_rim_w - h->stick_label_gap - h->label_h * 0.5F, h->stick_label_w);
    finger_at(ctx, h, "hint/finger", x, ky, hand_units(h, h->hand_size, h->hand_max_px), 0);
}

void ui_hint_click_prompt(nt_ui_context_t *ctx, const ui_hint_t *h, const ui_metrics_t *m, const char *text) {
    const int16_t over = (int16_t)(h->click_z + 1);
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){.id = CLAY_ID("hint/click_dim"),
        .floating = {.attachTo = CLAY_ATTACH_TO_ROOT, .zIndex = h->click_z, .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH},
        .layout = {.sizing = {CLAY_SIZING_FIXED(m->view_w), CLAY_SIZING_FIXED(m->view_h)}}, .backgroundColor = ui_rgb_color(h->ink, h->click_dim)});
    Clay__CloseElement();
    // The group, top to bottom: the ring's reach over the fingertip, the hand under its tip, the gap, the label.
    const float fixed = h->click_ring + h->click_gap + h->click_label_h + h->lift;
    const float hand_share = 1.0F - h->hand_tip_y;
    const float column = m->view_h - m->safe_t - m->safe_b - 2.0F * h->edge;
    const float hand = fminf(hand_units(h, h->click_hand, h->click_hand_max_px), fmaxf(h->click_hand_min, (column - fixed) / hand_share));
    const float w = fmaxf(h->label_min_w + 48.0F, fminf(h->click_label_w, m->view_w - m->safe_l - m->safe_r - 2.0F * h->edge));
    const float cx = m->safe_l + (m->view_w - m->safe_l - m->safe_r) * 0.5F;
    const float top = m->safe_t + (m->view_h - m->safe_t - m->safe_b - fixed - hand_share * hand) * 0.5F;
    const float beat = fmodf(h->clock, h->click_beat_s);
    const float press = beat < h->click_press_s ? h->click_press : 0.0F;
    // The hand's middle over the label's: its fingertip sits left of the art's middle.
    const float tip_x = cx - hand * (0.5F - h->hand_tip_x), tip_y = top + h->click_ring;
    const float spread = fminf(1.0F, beat / h->click_ring_s);
    const float thick = fmaxf(6.0F, h->device_per_unit > 0.0F ? h->click_ring_px / h->device_per_unit : 6.0F);
    const float radius = 16.0F + (h->click_ring - 20.0F) * spread;
    finger_at(ctx, h, "hint/click_hand", tip_x, tip_y + press, hand, over);
    // Over the hand: the ring reads as the fingertip's tap, its dark edge keeping it clear on a light floor.
    float_at("hint/click_ring", tip_x - 2.0F, tip_y - 2.0F, 4.0F, 4.0F, over);
    if (spread < 1.0F) {
        const float fade = 1.0F - spread * spread;
        ui_kit_arc(ctx, 2.0F * (radius + 2.0F), thick + 4.0F, 0.0F, 1.0F, h->line_rim, fade);
        ui_kit_arc(ctx, 2.0F * radius, thick, 0.0F, 1.0F, h->ring_rgb, fade);
    }
    float_end();
    const float label_y = tip_y + hand_share * hand + h->click_gap;
    float_at("hint/click_label", cx - w * 0.5F, label_y, w, h->click_label_h + h->lift, over);
    ui_kit_panel_style_t look = ui_look_face(h->label_family, h->label_top, h->label_bottom, h->click_label_h, 0.0F, 0.0F);
    look.lip_h = h->lift;
    ui_kit_lifted_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(h->click_label_h)}, .padding = {24, 24, 0, 0},
        .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}, &look);
    nt_ui_label_style_t style = text_style(h, h->click_text);
    ui_text_fit_width(ctx, text, &style, w - 48.0F, h->click_text_min);
    text_label(ctx, text, &style);
    ui_kit_lifted_end(ctx);
    float_end();
}
