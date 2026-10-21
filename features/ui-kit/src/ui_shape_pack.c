#include "features/ui_kit/ui_shape_pack.h"

#include <math.h>

static float quarters(float units) {
    if (!(units > 0.0F)) { return 0.0F; }
    if (units > UI_KIT_SHAPE_MAX_LENGTH) { units = UI_KIT_SHAPE_MAX_LENGTH; }
    return floorf(units * 4.0F + 0.5F);
}

static float pair12(float lo, float hi) { return lo + hi * 4096.0F; }

static float rgb24(uint32_t rgb) {
    return (float)((rgb >> 16U) & 0xFFU) + (float)((rgb >> 8U) & 0xFFU) * 256.0F + (float)(rgb & 0xFFU) * 65536.0F;
}

static float byte01(float v) {
    if (!(v > 0.0F)) { return 0.0F; }
    return v >= 1.0F ? 255.0F : floorf(v * 255.0F + 0.5F);
}

static float bytes3(float a, float b, float c) { return byte01(a) + byte01(b) * 256.0F + byte01(c) * 65536.0F; }

void ui_kit_pack_panel(const ui_kit_panel_style_t *s, ui_kit_shape_block_t *out) {
    const float outline = quarters(s->outline_w);
    *out = (ui_kit_shape_block_t){
        .shape = {pair12(outline, (float)UI_KIT_SHAPE_MODE_PANEL), pair12(quarters(s->radius[0]), quarters(s->radius[1])),
            pair12(quarters(s->radius[2]), quarters(s->radius[3])), pair12(quarters(s->face_radius), quarters(s->lip_h))},
        .paint = {rgb24(s->bottom_rgb), rgb24(s->outline_rgb), rgb24(s->lip_rgb),
            bytes3(s->gloss_a, s->checker_a, s->highlight_a)},
        .fx = {pair12(quarters(s->gloss_inset), quarters(s->gloss_top)), pair12(quarters(s->gloss_h), quarters(s->gloss_cap)),
            pair12(quarters(s->checker_px), quarters(s->highlight_w)),
            pair12(byte01(s->press), (s->overlay ? 1.0F : 0.0F) + (s->lip_inside ? 2.0F : 0.0F))},
    };
}

void ui_kit_pack_radial(const ui_kit_radial_style_t *s, ui_kit_shape_block_t *out) {
    *out = (ui_kit_shape_block_t){
        .shape = {pair12(quarters(s->outline_w), (float)UI_KIT_SHAPE_MODE_RADIAL), s->angle_start, s->angle_end,
            s->inner_norm},
        .paint = {rgb24(s->bottom_rgb), rgb24(s->outline_rgb), rgb24(s->lip_rgb), bytes3(s->gloss_a, 0.0F, 0.0F)},
        .fx = {quarters(s->lip_h), 0.0F, 0.0F, pair12(0.0F, s->overlay ? 1.0F : 0.0F)},
    };
}

uint32_t ui_kit_shape_tint(uint32_t rgb, float alpha) {
    const uint32_t a = (uint32_t)byte01(alpha);
    const uint32_t packed = (a << 24U) | ((rgb & 0xFFU) << 16U) | (rgb & 0xFF00U) | ((rgb >> 16U) & 0xFFU);
    // The walker reads an all-zero tint as "untinted" and draws opaque white;
    // one unit of red at zero alpha still draws nothing.
    return packed != 0U ? packed : 1U;
}

static uint32_t scale_rgb(uint32_t rgb, float k) {
    uint32_t out = 0U;
    for (uint32_t shift = 0U; shift < 24U; shift += 8U) {
        const float c = (float)((rgb >> shift) & 0xFFU) * k;
        out |= (uint32_t)(c + 0.5F) << shift;
    }
    return out;
}

ui_kit_panel_style_t ui_kit_look_state(const ui_kit_panel_style_t *look, ui_kit_look_state_t state, uint32_t off_rgb) {
    ui_kit_panel_style_t s = *look;
    if (state == UI_KIT_LOOK_PRESSED) {
        if (s.lip_h > 0.0F && !s.lip_inside) {
            s.press = 1.0F;
        } else {
            s.bottom_rgb = scale_rgb(s.bottom_rgb, 0.88F);
        }
    } else if (state == UI_KIT_LOOK_DISABLED) {
        s.top_rgb = off_rgb;
        s.bottom_rgb = off_rgb;
        s.lip_rgb = scale_rgb(off_rgb, 0.72F);
        s.press = 0.0F;
    }
    return s;
}

bool ui_kit_scroll_engaged(bool was, float content_h, float room, float band) {
    return was ? content_h > room - band : content_h > room;
}

float ui_kit_choice_cells(int count, float width, float gap, float hit_pad, ui_kit_cell_t *out) {
    if (count <= 0) { return 0.0F; }
    const float w = (width - gap * (float)(count - 1)) / (float)count;
    for (int i = 0; i < count; ++i) {
        out[i] = (ui_kit_cell_t){.x = (w + gap) * (float)i, .w = w > 0.0F ? w : 0.0F};
    }
    const float half = gap * 0.5F;
    const float pad = hit_pad < half ? hit_pad : half;
    return pad > 0.0F ? floorf(pad) : 0.0F;
}
