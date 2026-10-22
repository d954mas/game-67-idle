#include "features/ui_kit/ui_kit.h"

#include "core/nt_assert.h"
#include "features/ui_kit/ui_shape.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_modal.h"
#include "ui/nt_ui_dropdown.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_popup.h"
#include "ui/nt_ui_scroll.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *text_of(const char *text) { return text != NULL ? text : ""; }

// Through Clay's own hash, with the string kept: that is what names the
// element in the ui.tree a DevAPI bot reads. Callers pass strings that outlive
// the frame (literals, statics).
static Clay_ElementId element_id(const char *s) {
    return Clay_GetElementId((Clay_String){.length = (int32_t)strlen(s), .chars = s});
}

// "<id>/<suffix>" kept past the frame, where the ui.tree a DevAPI bot reads
// finds it. More derived names than the ring holds in one frame would rename
// the oldest, so that is asserted.
#define UI_KIT_DERIVED_IDS 64
static const char *derived_id(const char *id, const char *suffix) {
    static char ring[UI_KIT_DERIVED_IDS][128];
    static unsigned slot;
    static unsigned in_frame;
    static uint32_t frame;
    if (frame != ui_frame_serial()) {
        frame = ui_frame_serial();
        in_frame = 0U;
    }
    NT_ASSERT(++in_frame <= UI_KIT_DERIVED_IDS && "ui_kit: more derived ids in one frame than the ring holds");
    char *buf = ring[slot++ % UI_KIT_DERIVED_IDS];
    (void)snprintf(buf, sizeof ring[0], "%s/%s", id, suffix);
    return buf;
}

static const char *derived_index(const char *id, int index) {
    char key[16];
    (void)snprintf(key, sizeof key, "%d", index);
    return derived_id(id, key);
}

static Clay_Color clay_color(uint32_t abgr) {
    return (Clay_Color){(float)(abgr & 0xFFU), (float)((abgr >> 8) & 0xFFU), (float)((abgr >> 16) & 0xFFU),
                        (float)((abgr >> 24) & 0xFFU)};
}

// ---- the shape theme ------------------------------------------------------
// With art.shapes each plate below is the SDF shape of what gen_ui_kit.py
// draws: the same corners, rim and ledge, in the same UI units the art's
// slice9 borders land in, and grayscale art times its tint worked out here.

static bool shapes(void) { return g_ui_theme.art.shapes; }

static uint32_t rgb_of(uint32_t abgr) {
    return ((abgr & 0xFFU) << 16U) | (abgr & 0xFF00U) | ((abgr >> 16U) & 0xFFU);
}

static float alpha_of(uint32_t abgr) { return (float)(abgr >> 24U) / 255.0F; }

// Channel by channel, as the sprite shader multiplies a texel by its tint.
static uint32_t times(uint32_t rgb, uint32_t tint_rgb) {
    uint32_t out = 0U;
    for (uint32_t shift = 0U; shift < 24U; shift += 8U) {
        const uint32_t c = (((rgb >> shift) & 0xFFU) * ((tint_rgb >> shift) & 0xFFU) + 127U) / 255U;
        out |= c << shift;
    }
    return out;
}

static uint32_t grey(float level) {
    const uint32_t v = (uint32_t)(level * 255.0F);
    return (v << 16U) | (v << 8U) | v;
}

static float or_default(float value, float fallback) { return value > 0.0F ? value : fallback; }

static ui_kit_panel_style_t flat_plate(float radius, uint32_t fill_abgr, uint32_t rim_abgr, float rim) {
    ui_kit_panel_style_t s = {.top_rgb = rgb_of(fill_abgr), .bottom_rgb = rgb_of(fill_abgr), .outline_w = rim,
        .outline_rgb = rgb_of(rim_abgr), .alpha = alpha_of(fill_abgr)};
    for (int i = 0; i < 4; ++i) { s.radius[i] = radius; }
    return s;
}

// button.png: the shell rim round the whole box, the ledge grey inside it and
// the white body lift units above the bottom rim, all times the tint.
static ui_kit_panel_style_t button_plate(uint32_t tint) {
    const ui_tokens_t *t = ui_theme_tokens();
    const uint32_t face = rgb_of(tint);
    ui_kit_panel_style_t s = flat_plate(or_default(t->r_button, 16.0F), tint, t->shell, t->rim);
    s.outline_rgb = times(rgb_of(t->shell), face);
    s.lip_inside = true;
    s.lip_h = t->lift;
    s.lip_rgb = times(grey(or_default(t->ledge_step, 0.72F)), face);
    return s;
}

// slider_thumb.png: a white disc in the shell rim, 32 units across, stretched
// to the box, so the rim grows with it.
static ui_kit_radial_style_t disc_plate(uint32_t tint, float size) {
    const ui_tokens_t *t = ui_theme_tokens();
    const uint32_t face = rgb_of(tint);
    return (ui_kit_radial_style_t){.angle_start = 0.0F, .angle_end = 6.2831853F, .top_rgb = face, .bottom_rgb = face,
        .outline_w = t->rim * size / 32.0F, .outline_rgb = times(rgb_of(t->shell), face), .alpha = alpha_of(tint)};
}

ui_kit_panel_style_t ui_kit_look_panel(void) {
    const ui_tokens_t *t = ui_theme_tokens();
    return flat_plate(or_default(t->r_panel, 14.0F), t->panel, t->shell, t->rim);
}

ui_kit_panel_style_t ui_kit_look_tile(void) {
    const ui_tokens_t *t = ui_theme_tokens();
    return flat_plate(or_default(t->r_tile, 12.0F), t->tile, t->tile_rim, t->rim);
}

ui_kit_panel_style_t ui_kit_look_button(uint32_t tint) { return button_plate(tint); }

ui_kit_panel_style_t ui_kit_look_header(void) {
    const ui_tokens_t *t = ui_theme_tokens();
    const uint32_t fill = t->header != 0U ? t->header : t->shell;
    const float r = or_default(t->r_header, or_default(t->r_panel, 14.0F) - t->rim);
    return (ui_kit_panel_style_t){.radius = {r, r, 0.0F, 0.0F}, .top_rgb = rgb_of(fill), .bottom_rgb = rgb_of(fill),
        .alpha = alpha_of(fill)};
}

ui_kit_panel_style_t ui_kit_look_track(void) {
    const ui_tokens_t *t = ui_theme_tokens();
    return flat_plate(or_default(t->r_bar, 11.0F), t->inset, t->inset_rim, t->rim);
}

ui_kit_panel_style_t ui_kit_look_fill(uint32_t tint) {
    const ui_tokens_t *t = ui_theme_tokens();
    return flat_plate(or_default(t->r_bar, 11.0F) - t->rim, tint, tint, 0.0F);
}

ui_kit_radial_style_t ui_kit_look_disc(uint32_t tint, float size) { return disc_plate(tint, size); }

// A pill or a box shorter than twice its radius would draw its ends wrong.
static ui_kit_panel_style_t fit_radius(ui_kit_panel_style_t s, float h) {
    for (int i = 0; i < 4; ++i) { s.radius[i] = fminf(s.radius[i], h * 0.5F); }
    return s;
}

// The disc's rim grows with its width, which has to be known before layout.
static float fixed_side(const Clay_ElementDeclaration *decl) {
    NT_ASSERT(decl != NULL && decl->layout.sizing.width.type == CLAY__SIZING_TYPE_FIXED &&
              "ui_kit_disc_begin: the shape theme needs a FIXED width");
    return decl->layout.sizing.width.size.minMax.min;
}

// An engine widget that takes only atlas regions draws nothing under the shape
// theme unless the consumer still packs that art.
#define ART_PACKED(region) (!shapes() || (region).atlas.id != 0U)

// ---- primitives -----------------------------------------------------------

void ui_kit_panel_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl) {
    if (shapes()) {
        const ui_kit_panel_style_t s = ui_kit_look_panel();
        ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, decl);
        return;
    }
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &g_ui_theme.art.panel, &g_ui_theme.plate_img, decl);
}

void ui_kit_panel_end(nt_ui_context_t *ctx) {
    if (shapes()) {
        ui_kit_panel_styled_end(ctx);
        return;
    }
    nt_ui_panel_end(ctx);
}

void ui_kit_tile_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl) {
    if (shapes()) {
        const ui_kit_panel_style_t s = ui_kit_look_tile();
        ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, decl);
        return;
    }
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &g_ui_theme.art.tile, &g_ui_theme.plate_img, decl);
}

void ui_kit_tile_end(nt_ui_context_t *ctx) { ui_kit_panel_end(ctx); }

void ui_kit_scrim(nt_ui_context_t *ctx, bool occludes) {
    const uint32_t c = ui_theme_tokens()->scrim;
    CLAY({.id = CLAY_ID("ui_kit/scrim"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                        .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
          .backgroundColor = clay_color(c),
          .userData = NT_UI_CLAY_DATA(UI_LAYER_SCRIM)}) {}
    if (occludes) {
        nt_ui_block_pointer(ctx, nt_ui_id("ui_kit/scrim"), NULL);
    }
}

nt_ui_label_style_t ui_text(const nt_ui_label_style_t *kit_style) { return ui_text_scaled(kit_style, 1.0F); }

nt_ui_label_style_t ui_text_scaled(const nt_ui_label_style_t *kit_style, float factor) {
    nt_ui_label_style_t style = *kit_style;
    style.font_size = ui_css(kit_style->font_size * factor);
    return style;
}

void ui_kit_label(nt_ui_context_t *ctx, const char *text, const nt_ui_label_style_t *style) {
    ui_kit_label_scaled(ctx, text, style, 1.0F);
}

void ui_kit_label_scaled(nt_ui_context_t *ctx, const char *text, const nt_ui_label_style_t *style, float factor) {
    const nt_ui_label_style_t sized = ui_text_scaled(style, factor);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT), text_of(text), &sized);
}

void ui_kit_label_shadowed(nt_ui_context_t *ctx, const char *id, int slot, const char *text,
                           const nt_ui_label_style_t *style) {
    const nt_ui_label_style_t sized = ui_text(style);
    nt_ui_label_style_t shadow = sized;
    shadow.color = NT_RGBA8(8, 5, 3, 142);
    // The shadow is a fixed CSS offset below the glyph, so it holds its physical
    // depth instead of thinning out as the type scales.
    const float drop = ui_css(ui_theme_tokens()->rim);
    const char *line = text_of(text);

    const Clay_String id_string = {.isStaticallyAllocated = false, .length = (int32_t)strlen(id), .chars = id};
    CLAY({.id = CLAY_SIDI(id_string, (uint32_t)slot),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        CLAY({.floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                            .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                           .offset = {0.0F, drop}},
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT_SHADOW), line, &shadow);
        }
        nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT), line, &sized);
    }
}

// The shape each open kit button drew its plate as, innermost last.
#define UI_KIT_BUTTON_DEPTH 8
enum { BUTTON_ART, BUTTON_PANEL, BUTTON_RADIAL };
static uint8_t s_button_shape[UI_KIT_BUTTON_DEPTH];
static int s_button_depth;

// The state the engine shows a button in this frame: hover counts the hit
// padding, and a press dragged off the button shows idle.
static ui_kit_look_state_t button_state(nt_ui_context_t *ctx, uint32_t id, const nt_ui_button_style_t *style,
                                        bool enabled) {
    if (!enabled) { return UI_KIT_LOOK_DISABLED; }
    const nt_ui_interaction_t in = nt_ui_query_interaction_padded(ctx, id, style->hit_padding_lrtb);
    if (in.pressed && in.hovered) { return UI_KIT_LOOK_PRESSED; }
    return in.hovered ? UI_KIT_LOOK_HOVER : UI_KIT_LOOK_IDLE;
}

// The tint the engine would put on the art for that state.
static uint32_t state_tint(nt_ui_context_t *ctx, uint32_t id, const nt_ui_button_style_t *style, bool enabled) {
    switch (button_state(ctx, id, style, enabled)) {
    case UI_KIT_LOOK_DISABLED: return style->disabled.bg_tint;
    case UI_KIT_LOOK_PRESSED: return style->pressed.bg_tint;
    case UI_KIT_LOOK_HOVER: return style->hover.bg_tint;
    default: return style->idle.bg_tint;
    }
}

// The engine button with no art of its own: it still eases the scale and the
// press offset that carry the shape inside it. An art-less button fills its
// box with its tint unless the tint is white, so it gets white.
static nt_ui_button_style_t motion_of(const nt_ui_button_style_t *style) {
    nt_ui_button_style_t motion = *style;
    nt_ui_btn_state_t *states[] = {&motion.idle, &motion.hover, &motion.pressed, &motion.disabled};
    for (size_t i = 0; i < sizeof states / sizeof states[0]; ++i) {
        states[i]->bg = (nt_atlas_region_ref_t){0};
        states[i]->bg_tint = 0xFFFFFFFFU;
    }
    return motion;
}

static const Clay_ElementDeclaration BUTTON_FILL = {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                               .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}};

static void button_begin(nt_ui_context_t *ctx, uint32_t id, nt_ui_button_style_t *style, bool enabled,
                         const nt_ui_events_cfg_t *cfg, bool round) {
    const Clay_ElementDeclaration fill = {.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}};
    NT_ASSERT(s_button_depth < UI_KIT_BUTTON_DEPTH && "ui_kit_button_begin: buttons nest too deep");
    const bool shape = shapes() && style->idle.bg.atlas.id == 0U;
    s_button_shape[s_button_depth++] = !shape ? BUTTON_ART : round ? BUTTON_RADIAL : BUTTON_PANEL;
    if (!shape) {
        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), id, style, &fill, enabled, cfg);
        return;
    }
    // The state colours go to the shape.
    nt_ui_button_style_t motion = motion_of(style);
    nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), id, &motion, &fill, enabled, cfg);
    const uint32_t tint = state_tint(ctx, id, style, enabled);
    if (round) {
        const ui_kit_radial_style_t s = disc_plate(tint, ui_metrics().hit);
        ui_kit_radial_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, &fill);
    } else {
        const ui_kit_panel_style_t s = button_plate(tint);
        ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, &fill);
    }
}

void ui_kit_button_begin(nt_ui_context_t *ctx, uint32_t id, nt_ui_button_style_t *style, bool enabled,
                         const nt_ui_events_cfg_t *cfg) {
    button_begin(ctx, id, style, enabled, cfg, false);
}

// The engine button under a look: the theme's ease, and hit padding that grows
// the plate's box of the last frame to `reach` each way. The growth is held to
// `cap_x`/`cap_y` so neighbours keep their own zones, and the horizontal one
// is at least `floor_x` so the gap between segments still takes a tap. A
// disabled button grows nothing: its zone would swallow its neighbours' taps.
static nt_ui_button_style_t look_motion(nt_ui_context_t *ctx, uint32_t id, float reach, float cap_x, float cap_y,
                                        float floor_x, bool enabled) {
    nt_ui_button_style_t motion = motion_of(&g_ui_theme.button);
    const nt_ui_bbox_t box = nt_ui_get_bbox(ctx, id);
    const bool grows = enabled && box.found;
    const float x = grows ? fmaxf(fminf(fmaxf((reach - box.width) * 0.5F, 0.0F), cap_x), floor_x) : 0.0F;
    const float y = grows ? fminf(fmaxf((reach - box.height) * 0.5F, 0.0F), cap_y) : 0.0F;
    motion.hit_padding_lrtb[0] = motion.hit_padding_lrtb[1] = (int16_t)ceilf(x);
    motion.hit_padding_lrtb[2] = motion.hit_padding_lrtb[3] = (int16_t)ceilf(y);
    return motion;
}

// A button in the caller's look: one panel or one radial, whichever is given.
// `disabled` NULL derives the disabled plate from `panel`.
static void look_button_begin(nt_ui_context_t *ctx, uint32_t id, nt_ui_button_style_t motion,
                              const ui_kit_panel_style_t *panel, const ui_kit_panel_style_t *disabled,
                              const ui_kit_radial_style_t *radial, bool enabled, const nt_ui_events_cfg_t *cfg) {
    NT_ASSERT(s_button_depth < UI_KIT_BUTTON_DEPTH && "ui_kit_button_begin_styled: buttons nest too deep");
    s_button_shape[s_button_depth++] = panel != NULL ? BUTTON_PANEL : BUTTON_RADIAL;
    // An outside lip is the press: the face sinks inside the shape, so the box
    // must not move as well. A disc has no ledge, so it presses by scale.
    const float lip = panel != NULL && !panel->lip_inside ? panel->lip_h : 0.0F;
    if (lip > 0.0F) { motion.pressed.offset_y = 0.0F; }
    if (radial != NULL) {
        motion.pressed.offset_y = 0.0F;
        motion.pressed.scale = g_ui_theme.button_close.pressed.scale;
    }
    nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), id, &motion, &BUTTON_FILL, enabled, cfg);
    const ui_kit_look_state_t state = button_state(ctx, id, &motion, enabled);
    const uint32_t off = rgb_of(ui_theme_tokens()->off);
    if (panel != NULL) {
        const ui_kit_panel_style_t s =
            state == UI_KIT_LOOK_DISABLED && disabled != NULL ? *disabled : ui_kit_look_state(panel, state, off);
        // The content centres on the face and sinks onto the lip with it.
        Clay_ElementDeclaration face = BUTTON_FILL;
        if (state == UI_KIT_LOOK_PRESSED) {
            face.layout.padding.top = (uint16_t)lip;
        } else {
            face.layout.padding.bottom = (uint16_t)lip;
        }
        ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, &face);
        return;
    }
    ui_kit_radial_style_t s = *radial;
    if (state == UI_KIT_LOOK_PRESSED) {
        s.bottom_rgb = times(s.bottom_rgb, grey(0.88F));
    } else if (state == UI_KIT_LOOK_DISABLED) {
        s.top_rgb = off;
        s.bottom_rgb = off;
    }
    ui_kit_radial_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, &BUTTON_FILL);
}

void ui_kit_button_begin_styled(nt_ui_context_t *ctx, uint32_t id, const ui_kit_button_look_t *look, bool enabled,
                                const nt_ui_events_cfg_t *cfg) {
    NT_ASSERT(look != NULL && "ui_kit_button_begin_styled: look is required");
    look_button_begin(ctx, id, look_motion(ctx, id, look->reach, INFINITY, INFINITY, 0.0F, enabled), &look->idle, look->disabled, NULL, enabled,
                      cfg);
}

bool ui_kit_button_end(nt_ui_context_t *ctx) {
    NT_ASSERT(s_button_depth > 0 && "ui_kit_button_end without ui_kit_button_begin");
    const uint8_t shape = s_button_shape[--s_button_depth];
    if (shape == BUTTON_PANEL) { ui_kit_panel_styled_end(ctx); }
    if (shape == BUTTON_RADIAL) { ui_kit_radial_styled_end(ctx); }
    return nt_ui_button_end(ctx);
}

void ui_kit_meter(nt_ui_context_t *ctx, float w, float h, float ratio, uint32_t fill_tint) {
    if (ratio < 0.0F) {
        ratio = 0.0F;
    }
    if (ratio > 1.0F) {
        ratio = 1.0F;
    }
    // Both pieces float against the element the caller has open, so a meter
    // never disturbs the row's own layout.
    const ui_tokens_t *t = ui_theme_tokens();
    const Clay_ElementDeclaration track = {
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_LEFT_CENTER}},
        .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}}};
    if (shapes()) {
        const ui_kit_panel_style_t s = ui_kit_look_track();
        ui_kit_panel(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, &track);
    } else {
        nt_ui_image_style_t art = g_ui_theme.plate_img;
        nt_ui_image(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &g_ui_theme.art.slider_track, &art, &track);
    }

    // The pill rides inside the track's own rim, so the recess stays visible at
    // a full bar instead of being painted over by it.
    const float inset = ui_css(t->rim);
    const float fill_w = (w - inset * 2.0F) * ratio;
    if (fill_w <= 1.0F) {
        return;
    }
    const Clay_ElementDeclaration pill = {
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_LEFT_CENTER},
                     .offset = {inset, 0.0F}},
        .layout = {.sizing = {CLAY_SIZING_FIXED(fill_w), CLAY_SIZING_FIXED(h - inset * 2.0F)}}};
    if (shapes()) {
        // slider_fill.png: the white pill inside the track's rim, tinted.
        const ui_kit_panel_style_t s = ui_kit_look_fill(fill_tint);
        ui_kit_panel(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, &pill);
        return;
    }
    nt_ui_image_style_t fill = g_ui_theme.plate_img;
    fill.color_packed = fill_tint;
    nt_ui_image(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &g_ui_theme.art.slider_fill, &fill, &pill);
}

static nt_ui_slider_style_t *slider_style_sized(const ui_metrics_t *m) {
    static nt_ui_slider_style_t s;
    static uint32_t generation;
    if (generation != g_ui_theme.generation) {
        s = g_ui_theme.slider;
        generation = g_ui_theme.generation;
    }
    const nt_ui_slider_style_t *d = &g_ui_theme.slider;
    s.track_h = ui_css(d->track_h);
    s.thumb_w = ui_css(d->thumb_w);
    s.thumb_h = ui_css(d->thumb_h);
    // The track is whatever the row gives it; a fixed width would overflow the
    // narrow frame a plate gets on a phone.
    s.track_w = m->panel_w - m->pad * 2.0F;
    return &s;
}

nt_ui_slider_style_t *ui_kit_slider_style(const ui_metrics_t *m) {
    NT_ASSERT(ART_PACKED(g_ui_theme.slider.states[NT_UI_SLIDER_IDLE].track) && "ui_kit_slider_style: the shape theme packs no slider art");
    return slider_style_sized(m);
}

nt_ui_scroll_style_t *ui_kit_scroll_style(void) {
    static nt_ui_scroll_style_t s;
    static uint32_t generation;
    if (generation != g_ui_theme.generation) {
        s = g_ui_theme.scroll;
        generation = g_ui_theme.generation;
    }
    s.bar_thickness = ui_css(g_ui_theme.scroll.bar_thickness);
    return &s;
}

Clay_Color ui_kit_color(uint32_t abgr) { return clay_color(abgr); }

Clay_SizingAxis ui_kit_hit_height(const ui_metrics_t *m) { return CLAY_SIZING_FIXED(m->hit); }

// ---- composites -----------------------------------------------------------

nt_ui_button_style_t *ui_kit_button_style(ui_kit_button_kind_t kind) {
    switch (kind) {
    case UI_KIT_BUTTON_CONFIRM:
        return &g_ui_theme.button_confirm;
    case UI_KIT_BUTTON_AD:
        return &g_ui_theme.button_ad;
    case UI_KIT_BUTTON_DANGER:
        return &g_ui_theme.button_danger;
    default:
        return &g_ui_theme.button;
    }
}

const nt_ui_label_style_t *ui_kit_button_label_style(ui_kit_button_kind_t kind) {
    return kind == UI_KIT_BUTTON_NEUTRAL ? &g_ui_theme.button_label : &g_ui_theme.button_label_action;
}

static bool button_with(nt_ui_context_t *ctx, const char *id, nt_atlas_region_ref_t *icon, const char *label,
                        ui_kit_button_kind_t kind, bool enabled, Clay_SizingAxis w, Clay_SizingAxis h) {
    const ui_metrics_t m = ui_metrics();
    const uint32_t base = nt_ui_id(id);
    bool clicked = false;
    CLAY({.id = element_id(id), .layout = {.sizing = {w, h}}}) {
        ui_kit_button_begin(ctx, nt_ui_child_id(base, "plate"), ui_kit_button_style(kind), enabled, NULL);
        // The content keeps its own side room, so a button sized to its label
        // never sets the glyphs against the rim.
        CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                         .padding = {.left = (uint16_t)m.gap, .right = (uint16_t)m.gap},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = (uint16_t)(m.gap * 0.5F),
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            if (icon != NULL) {
                ui_kit_icon(ctx, icon, m.hit * 0.6F);
            }
            if (label != NULL) {
                ui_kit_label(ctx, label, enabled ? ui_kit_button_label_style(kind) : &g_ui_theme.button_label_disabled);
            }
        }
        clicked = ui_kit_button_end(ctx);
    }
    return clicked;
}

bool ui_kit_button(nt_ui_context_t *ctx, const char *id, const char *label, ui_kit_button_kind_t kind, bool enabled,
                   Clay_SizingAxis w, Clay_SizingAxis h) {
    return button_with(ctx, id, NULL, label, kind, enabled, w, h);
}

bool ui_kit_icon_button(nt_ui_context_t *ctx, const char *id, nt_atlas_region_ref_t *icon, const char *label,
                        ui_kit_button_kind_t kind, bool enabled, Clay_SizingAxis w, Clay_SizingAxis h) {
    return button_with(ctx, id, icon, label, kind, enabled, w, h);
}

bool ui_kit_close_button(nt_ui_context_t *ctx, const char *id, const char *label) {
    const ui_metrics_t m = ui_metrics();
    bool clicked = false;
    nt_atlas_region_ref_t *icon = ui_kit_icon_ref(UI_ICON_CLOSE);
    nt_ui_label_style_t glyph = g_ui_theme.button_label;
    glyph.wrap_mode = CLAY_TEXT_WRAP_NONE;
    // Centred a little inside the corner, so the circle reads as part of the
    // plate and the safe area never clips half of it.
    const float inset = m.gap * 0.4F;
    CLAY({.id = element_id(id),
          .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {-inset, inset}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(m.hit), CLAY_SIZING_FIXED(m.hit)}}}) {
        button_begin(ctx, nt_ui_child_id(nt_ui_id(id), "plate"), &g_ui_theme.button_close, true, NULL, true);
        if (icon->atlas.id != 0U) {
            ui_kit_icon_tinted(ctx, icon, m.hit * 0.42F, ui_theme_tokens()->ink);
        } else {
            ui_kit_label_scaled(ctx, label, &glyph, 1.3F);
        }
        clicked = ui_kit_button_end(ctx);
    }
    return clicked;
}

void ui_kit_icon(nt_ui_context_t *ctx, nt_atlas_region_ref_t *icon, float size) {
    const nt_ui_image_style_t style = nt_ui_image_style_defaults();
    nt_ui_image(ctx, NT_UI_DATA_LAYER(UI_LAYER_ICON), icon, &style,
                &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)}}});
}

void ui_kit_icon_tinted(nt_ui_context_t *ctx, nt_atlas_region_ref_t *icon, float size, uint32_t tint) {
    nt_ui_image_style_t style = nt_ui_image_style_defaults();
    style.color_packed = tint;
    nt_ui_image(ctx, NT_UI_DATA_LAYER(UI_LAYER_ICON), icon, &style,
                &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)}}});
}

void ui_kit_icon_rotated(nt_ui_context_t *ctx, nt_atlas_region_ref_t *icon, float size, uint32_t tint, float angle) {
    nt_ui_image_style_t style = nt_ui_image_style_defaults();
    style.color_packed = tint;
    nt_ui_transform_t turn = nt_ui_transform_defaults();
    // Clay lays out Y-down, so a Y-up counter-clockwise turn is negative there.
    turn.rotation_z = -angle;
    nt_ui_image(ctx, NT_UI_DATA_XFORM(UI_LAYER_ICON, &turn, 1.0F), icon, &style,
                &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)}}});
}

nt_atlas_region_ref_t *ui_kit_icon_ref(ui_icon_t icon) {
    return &g_ui_theme.art.icons[(icon >= 0 && icon < UI_ICON_COUNT) ? icon : UI_ICON_CLOSE];
}

void ui_kit_counter(nt_ui_context_t *ctx, const char *id, nt_atlas_region_ref_t *icon, const char *text) {
    const ui_metrics_t m = ui_metrics();
    // The wrapper carries the ui.tree name; a plate takes no id of its own.
    CLAY({.id = element_id(id), .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        ui_kit_tile_begin(ctx, &(Clay_ElementDeclaration){
                                   .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(m.hit)},
                                              .padding = {.left = (uint16_t)(m.gap * 0.5F), .right = (uint16_t)(m.gap * 0.8F)},
                                              .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                              .childGap = (uint16_t)(m.gap * 0.4F),
                                              .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}});
        if (icon != NULL) {
            ui_kit_icon(ctx, icon, m.hit * 0.6F);
        }
        ui_kit_label(ctx, text, &g_ui_theme.counter);
        ui_kit_tile_end(ctx);
    }
}

void ui_kit_meter_captioned(nt_ui_context_t *ctx, const char *id, float w, float h, float ratio, uint32_t fill_tint,
                            const char *caption, const nt_ui_label_style_t *caption_style) {
    CLAY({.id = element_id(id),
          .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        ui_kit_meter(ctx, w, h, ratio, fill_tint);
        if (caption != NULL && caption_style != NULL) {
            ui_kit_label(ctx, caption, caption_style);
        }
    }
}

// The tinted art sits on the image layer so it covers a tile it is placed on
// and stays under the icon and the text it holds.
static void tinted_begin(nt_ui_context_t *ctx, nt_atlas_region_ref_t *region, const Clay_ElementDeclaration *decl,
                         uint32_t tint) {
    nt_ui_image_style_t art = g_ui_theme.plate_img;
    art.color_packed = tint;
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), region, &art, decl);
}

void ui_kit_plate_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, uint32_t tint) {
    if (shapes()) {
        const ui_kit_panel_style_t s = button_plate(tint);
        ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, decl);
        return;
    }
    tinted_begin(ctx, &g_ui_theme.art.button, decl, tint);
}

void ui_kit_plate_end(nt_ui_context_t *ctx) {
    if (shapes()) {
        ui_kit_panel_styled_end(ctx);
        return;
    }
    nt_ui_panel_end(ctx);
}

void ui_kit_disc_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, uint32_t tint) {
    if (shapes()) {
        const ui_kit_radial_style_t s = disc_plate(tint, fixed_side(decl));
        ui_kit_radial_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, decl);
        return;
    }
    tinted_begin(ctx, &g_ui_theme.art.thumb, decl, tint);
}

void ui_kit_disc_end(nt_ui_context_t *ctx) {
    if (shapes()) {
        ui_kit_radial_styled_end(ctx);
        return;
    }
    nt_ui_panel_end(ctx);
}

void ui_kit_badge(nt_ui_context_t *ctx, const char *text) {
    const ui_metrics_t m = ui_metrics();
    const ui_tokens_t *t = ui_theme_tokens();
    const float d = m.hit * 0.55F;
    nt_ui_label_style_t style = g_ui_theme.hint;
    style.color = t->on_action != 0U ? t->on_action : t->on_panel;
    ui_kit_disc_begin(ctx,
                      &(Clay_ElementDeclaration){
                          .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                                        .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                                       .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH},
                          .layout = {.sizing = {CLAY_SIZING_FIXED(d), CLAY_SIZING_FIXED(d)},
                                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                      t->danger);
    ui_kit_label(ctx, text, &style);
    ui_kit_disc_end(ctx);
}

static nt_ui_modal_style_t sheet_style(void) {
    const ui_tokens_t *t = ui_theme_tokens();
    nt_ui_modal_style_t s = nt_ui_modal_style_defaults();
    s.ease_speed = 14.0F;
    // The scrim token carries its own alpha; the modal wants it as a factor.
    s.backdrop_alpha = (float)((t->scrim >> 24) & 0xFFU) / 255.0F;
    s.backdrop_color = t->scrim | 0xFF000000U;
    s.layer = UI_LAYER_SCRIM;
    s.flags = (uint8_t)(NT_UI_MODAL_LISTEN_ESC | NT_UI_MODAL_CLOSE_ON_BACKDROP);
    s.open = (nt_ui_modal_anim_t){.type = NT_UI_MODAL_ANIM_SCALE_POP, .scale_start = 0.92F};
    s.close = s.open;
    s.backdrop_close_pad = 16;
    return s;
}

// What each open dialog has to close, innermost last.
#define UI_KIT_DIALOG_DEPTH 4
typedef struct {
    bool styled;
    bool scrolls;
} dialog_frame_t;
static dialog_frame_t s_dialog[UI_KIT_DIALOG_DEPTH];
static int s_dialog_depth;

// Whether each recently drawn body scrolled: the hysteresis needs last
// frame's decision. A few dialogs at once is the most a screen shows.
#define UI_KIT_SCROLL_MEMORY 8
static struct {
    uint32_t id;
    bool on;
} s_scroll_memory[UI_KIT_SCROLL_MEMORY];
static unsigned s_scroll_next;

static bool *scroll_flag(uint32_t id) {
    for (int i = 0; i < UI_KIT_SCROLL_MEMORY; ++i) {
        if (s_scroll_memory[i].id == id) { return &s_scroll_memory[i].on; }
    }
    const unsigned slot = s_scroll_next++ % UI_KIT_SCROLL_MEMORY;
    s_scroll_memory[slot].id = id;
    s_scroll_memory[slot].on = false;
    return &s_scroll_memory[slot].on;
}

// The square close at the band's right end, centred on its height, the glyph
// in the title colour on the face.
static bool square_close(nt_ui_context_t *ctx, const char *id, const char *label, const ui_kit_button_look_t *look,
                         const nt_ui_label_style_t *title, float side) {
    const float lip = look->idle.lip_inside ? 0.0F : look->idle.lip_h;
    nt_atlas_region_ref_t *icon = ui_kit_icon_ref(UI_ICON_CLOSE);
    nt_ui_label_style_t glyph = *title;
    glyph.wrap_mode = CLAY_TEXT_WRAP_NONE;
    bool clicked = false;
    CLAY({.id = element_id(id), .layout = {.sizing = {CLAY_SIZING_FIXED(side), CLAY_SIZING_FIXED(side + lip)}}}) {
        ui_kit_button_begin_styled(ctx, nt_ui_child_id(nt_ui_id(id), "plate"), look, true, NULL);
        if (icon->atlas.id != 0U) {
            ui_kit_icon_tinted(ctx, icon, side * 0.5F, title->color);
        } else {
            ui_kit_label(ctx, label, &glyph);
        }
        clicked = ui_kit_button_end(ctx);
    }
    return clicked;
}

ui_kit_dialog_look_t ui_kit_dialog_look(void) {
    return (ui_kit_dialog_look_t){.panel = ui_kit_look_panel(), .header = ui_kit_look_header(), .close = NULL,
        .title = &g_ui_theme.header_title};
}

static bool dialog_open(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label,
                        Clay_SizingAxis w, Clay_SizingAxis h, const ui_kit_dialog_look_t *look) {
    const ui_metrics_t m = ui_metrics();
    NT_ASSERT(s_dialog_depth < UI_KIT_DIALOG_DEPTH && "ui_kit_dialog_begin: dialogs nest too deep");
    dialog_frame_t *frame = &s_dialog[s_dialog_depth++];
    *frame = (dialog_frame_t){.styled = look != NULL};
    const float body_top = title != NULL ? m.gap : m.pad * 0.5F - m.rim;
    const float band_h = look != NULL && look->header_h > 0.0F ? look->header_h : m.hit;
    const float rule_h = look != NULL && title != NULL ? look->rule_h : 0.0F;
    const ui_kit_button_look_t *square = look != NULL && close_label != NULL ? look->close : NULL;
    const uint32_t body_id = nt_ui_child_id(nt_ui_id(id), "body");
    const bool fixed_h = h.type == CLAY__SIZING_TYPE_FIXED;
    float room = 0.0F;
    if (look != NULL) {
        // The body's content of the last frame, its own padding included,
        // against what the safe area leaves it under the rim, the band, the
        // rule and the plate's foot.
        const float screen = m.view_h - m.safe_t - m.safe_b - m.margin * 2.0F;
        const float limit = fixed_h ? fminf(h.size.minMax.min, screen) : screen;
        room = fmaxf(limit - m.rim - (title != NULL ? band_h + rule_h : 0.0F) - m.pad * 0.5F, m.hit);
        bool *on = scroll_flag(body_id);
        const Clay_ElementData last = Clay_GetElementData((Clay_ElementId){.id = body_id});
        if (last.found) { *on = ui_kit_scroll_engaged(*on, last.boundingBox.height, room, m.hit * 0.5F); }
        frame->scrolls = *on;
        if (frame->scrolls && fixed_h) { h = CLAY_SIZING_FIXED(limit); }
    }
    bool closed = false;
    // The named wrapper carries the size; the plate inside it fills it.
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){.id = element_id(id), .layout = {.sizing = {w, h}}});
    // The band sits inside the panel's rim, so the plate keeps only its rim as
    // padding up there; the body brings the kit's padding back.
    const Clay_ElementDeclaration plate = {
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .padding = {.left = (uint16_t)m.rim, .right = (uint16_t)m.rim, .top = (uint16_t)m.rim,
                               .bottom = (uint16_t)(m.pad * 0.5F)},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}};
    if (look != NULL) {
        ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &look->panel, &plate);
    } else {
        ui_kit_panel_begin(ctx, &plate);
    }
    const nt_ui_label_style_t *title_style = look != NULL && look->title != NULL ? look->title : &g_ui_theme.header_title;
    if (title != NULL) {
        const float side = square == NULL ? 0.0F : look->close_size > 0.0F ? look->close_size : band_h * 0.82F;
        const float lip = square == NULL || square->idle.lip_inside ? 0.0F : square->idle.lip_h;
        const float right = square == NULL ? m.hit
                            : look->close_inset > 0.0F ? look->close_inset
                                                       : fmaxf((band_h - side - lip) * 0.5F, 0.0F);
        const float left = look != NULL && look->title_inset > 0.0F ? look->title_inset : m.pad - m.rim;
        const Clay_ElementDeclaration band = {
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(band_h)},
                       .padding = {.left = (uint16_t)left, .right = (uint16_t)right},
                       .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}};
        const bool drawn = look != NULL || shapes();
        if (drawn) {
            // header.png: round on top only, square where it meets the body.
            ui_kit_panel_style_t s = look != NULL ? look->header : ui_kit_look_header();
            if (look != NULL && look->calm_scroll && frame->scrolls) { s.checker_px = 0.0F; }
            ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &s, &band);
        } else {
            nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &g_ui_theme.art.header, &g_ui_theme.plate_img, &band);
        }
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) { ui_kit_label(ctx, title, title_style); }
        if (square != NULL) { closed = square_close(ctx, derived_id(id, "close"), close_label, square, title_style, side); }
        if (drawn) {
            ui_kit_panel_styled_end(ctx);
        } else {
            nt_ui_panel_end(ctx);
        }
        if (rule_h > 0.0F) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(rule_h)}},
                  .backgroundColor = clay_color(0xFF000000U | rgb_of(look->rule_rgb)),
                  .userData = NT_UI_CLAY_DATA(UI_LAYER_SURFACE)}) {}
        }
    }
    // Without a band the square close has nowhere to sit: it floats as the round one does.
    if (close_label != NULL && (square == NULL || title == NULL)) {
        closed = ui_kit_close_button(ctx, derived_id(id, "close"), close_label);
    }
    if (frame->scrolls) {
        const Clay_ElementDeclaration viewport = {
            .layout = {.sizing = {CLAY_SIZING_GROW(0), fixed_h ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIT(0, room)},
                       .layoutDirection = CLAY_TOP_TO_BOTTOM}};
        nt_ui_scroll_begin(ctx, NULL, nt_ui_child_id(nt_ui_id(id), "scroll"), ui_kit_scroll_style(), &viewport);
    }
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .id = {.id = look != NULL ? body_id : 0U},
        .layout = {.sizing = {CLAY_SIZING_GROW(0), frame->scrolls ? CLAY_SIZING_FIT(0) : CLAY_SIZING_GROW(0)},
                   // A scrolled body keeps a gap under its last row, so the row
                   // never ends flush on the clip.
                   .padding = {.left = (uint16_t)(m.pad - m.rim), .right = (uint16_t)(m.pad - m.rim),
                               .top = (uint16_t)body_top, .bottom = (uint16_t)(frame->scrolls ? m.gap * 0.5F : 0.0F)},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = (uint16_t)m.gap,
                   .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    return closed;
}

bool ui_kit_dialog_begin(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label,
                         Clay_SizingAxis w, Clay_SizingAxis h) {
    return dialog_open(ctx, id, title, close_label, w, h, NULL);
}

bool ui_kit_dialog_begin_styled(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label,
                                Clay_SizingAxis w, Clay_SizingAxis h, const ui_kit_dialog_look_t *look) {
    NT_ASSERT(look != NULL && "ui_kit_dialog_begin_styled: look is required");
    return dialog_open(ctx, id, title, close_label, w, h, look);
}

void ui_kit_dialog_end(nt_ui_context_t *ctx) {
    NT_ASSERT(s_dialog_depth > 0 && "ui_kit_dialog_end without ui_kit_dialog_begin");
    const dialog_frame_t frame = s_dialog[--s_dialog_depth];
    Clay__CloseElement();
    if (frame.scrolls) { nt_ui_scroll_end(ctx); }
    if (frame.styled) {
        ui_kit_panel_styled_end(ctx);
    } else {
        ui_kit_panel_end(ctx);
    }
    Clay__CloseElement();
}

static bool sheet_open(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label, bool *open,
                       float w, float h, const ui_kit_dialog_look_t *look) {
    const nt_ui_modal_style_t style = sheet_style();
    if (!nt_ui_modal_visible(ctx, nt_ui_id(id), &style, open)) {
        return false;
    }
    if (dialog_open(ctx, derived_id(id, "panel"), title, close_label, CLAY_SIZING_FIXED(w),
                    h > 0.0F ? CLAY_SIZING_FIXED(h) : CLAY_SIZING_FIT(0), look)) {
        *open = false;
    }
    return true;
}

bool ui_kit_sheet_begin(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label, bool *open,
                        float w, float h) {
    return sheet_open(ctx, id, title, close_label, open, w, h, NULL);
}

bool ui_kit_sheet_begin_styled(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label,
                               bool *open, float w, float h, const ui_kit_dialog_look_t *look) {
    NT_ASSERT(look != NULL && "ui_kit_sheet_begin_styled: look is required");
    return sheet_open(ctx, id, title, close_label, open, w, h, look);
}

void ui_kit_sheet_end(nt_ui_context_t *ctx) {
    ui_kit_dialog_end(ctx);
    nt_ui_modal_end(ctx);
}

void ui_kit_sheet_clear(nt_ui_context_t *ctx, const char *id) { nt_ui_modal_clear_state(ctx, nt_ui_id(id)); }

nt_ui_checkbox_style_t *ui_kit_toggle_style(void) {
    NT_ASSERT(ART_PACKED(g_ui_theme.art.slider_fill_sm) && "ui_kit_toggle_style: the shape theme packs no switch art");
    return &g_ui_theme.toggle;
}
nt_ui_checkbox_style_t *ui_kit_checkbox_style(void) {
    NT_ASSERT(ART_PACKED(g_ui_theme.art.slider_fill_sm) && "ui_kit_checkbox_style: the shape theme packs no switch art");
    return &g_ui_theme.checkbox;
}
nt_ui_checkbox_style_t *ui_kit_radio_style(void) {
    NT_ASSERT(ART_PACKED(g_ui_theme.art.thumb) && "ui_kit_radio_style: the shape theme packs no switch art");
    return &g_ui_theme.radio;
}

// A settings row: the label fills the row, the control sits at one end, and
// the whole row is one touch-height line.
#define UI_KIT_ROW(id_str, mp)                                                 \
    CLAY({.id = element_id(id_str),                                            \
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT((mp)->hit, 0)},\
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,                    \
                     .childGap = (uint16_t)(mp)->gap,                          \
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}})

static void row_label(nt_ui_context_t *ctx, const char *label, bool enabled) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
        ui_kit_label(ctx, label, enabled ? &g_ui_theme.label : &g_ui_theme.hint);
    }
}

// Under a shape theme a switch whose art was not packed is drawn as shapes on
// a kit button; a theme that still packs it keeps the engine switch.
static bool switch_shapes(const nt_atlas_region_ref_t *art) { return shapes() && art->atlas.id == 0U; }

static const Clay_ElementDeclaration *fixed_box(float w, float h) {
    static Clay_ElementDeclaration decl;
    decl = (Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}}};
    return &decl;
}

// The pill in the on or off colour with the white knob at its matching end.
static bool toggle_shape(nt_ui_context_t *ctx, uint32_t id, bool *value, bool enabled) {
    const ui_tokens_t *t = ui_theme_tokens();
    const float w = ui_css(g_ui_theme.toggle.box_w);
    const float h = ui_css(g_ui_theme.toggle.box_h);
    const float knob = ui_css(g_ui_theme.toggle.overlay_w);
    const uint16_t inset = (uint16_t)((h - knob) * 0.5F);
    const ui_kit_panel_style_t pill = fit_radius(flat_plate(h, *value ? t->go : t->off, t->shell, t->rim), h);
    const ui_kit_radial_style_t disc = disc_plate(0xFFFFFFFFU, knob);
    bool clicked = false;
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}}}) {
        look_button_begin(ctx, id, motion_of(&g_ui_theme.button), &pill, NULL, NULL, enabled, NULL);
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = {.left = inset, .right = inset},
                         .childAlignment = {*value ? CLAY_ALIGN_X_RIGHT : CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            ui_kit_radial(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &disc, fixed_box(knob, knob));
        }
        clicked = ui_kit_button_end(ctx);
    }
    if (clicked) { *value = !*value; }
    return clicked;
}

// The rounded box, the action colour with the check glyph when ticked.
static bool checkbox_shape(nt_ui_context_t *ctx, uint32_t id, bool *value, bool enabled) {
    const ui_tokens_t *t = ui_theme_tokens();
    const float side = ui_css(g_ui_theme.checkbox.box_w);
    const ui_kit_panel_style_t box = fit_radius(flat_plate(side * 0.28F, *value ? t->go : t->tile_dim, t->shell, t->rim), side);
    nt_atlas_region_ref_t *check = ui_kit_icon_ref(UI_ICON_CHECK);
    const uint32_t mark = t->on_action != 0U ? t->on_action : t->on_panel;
    bool clicked = false;
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(side), CLAY_SIZING_FIXED(side)}}}) {
        look_button_begin(ctx, id, motion_of(&g_ui_theme.button), &box, NULL, NULL, enabled, NULL);
        if (*value && check->atlas.id != 0U) {
            ui_kit_icon_tinted(ctx, check, side * 0.7F, mark);
        } else if (*value) {
            const ui_kit_panel_style_t dot = fit_radius(flat_plate(side, mark, mark, 0.0F), side * 0.36F);
            ui_kit_panel(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &dot, fixed_box(side * 0.36F, side * 0.36F));
        }
        clicked = ui_kit_button_end(ctx);
    }
    if (clicked) { *value = !*value; }
    return clicked;
}

// The tile-coloured ring, the action-coloured dot inside when picked.
static bool radio_shape(nt_ui_context_t *ctx, uint32_t id, int *selected, int my_value, bool enabled) {
    const ui_tokens_t *t = ui_theme_tokens();
    const float side = ui_css(g_ui_theme.radio.box_w);
    const ui_kit_radial_style_t ring = disc_plate(t->tile, side);
    const ui_kit_radial_style_t dot = {.angle_end = 6.2831853F, .top_rgb = rgb_of(t->go), .bottom_rgb = rgb_of(t->go),
        .alpha = 1.0F};
    bool clicked = false;
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(side), CLAY_SIZING_FIXED(side)}}}) {
        look_button_begin(ctx, id, motion_of(&g_ui_theme.button), NULL, NULL, &ring, enabled, NULL);
        if (*selected == my_value) {
            ui_kit_radial(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &dot, fixed_box(side * 0.5F, side * 0.5F));
        }
        clicked = ui_kit_button_end(ctx);
    }
    if (!clicked || *selected == my_value) { return false; }
    *selected = my_value;
    return true;
}

static const Clay_ElementDeclaration FIT_DECL = {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}};

bool ui_kit_toggle_row(nt_ui_context_t *ctx, const char *id, const char *label, bool *value, bool enabled) {
    const ui_metrics_t m = ui_metrics();
    const uint32_t control = nt_ui_child_id(nt_ui_id(id), "toggle");
    const bool drawn = switch_shapes(&g_ui_theme.art.slider_fill_sm);
    bool changed = false;
    UI_KIT_ROW(id, &m) {
        row_label(ctx, label, enabled);
        changed = drawn ? toggle_shape(ctx, control, value, enabled)
                        : nt_ui_toggle(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), UI_LAYER_TEXT, control, NULL, value,
                                       ui_kit_toggle_style(), &FIT_DECL, enabled);
    }
    return changed;
}

bool ui_kit_checkbox_row(nt_ui_context_t *ctx, const char *id, const char *label, bool *value, bool enabled) {
    const ui_metrics_t m = ui_metrics();
    const uint32_t control = nt_ui_child_id(nt_ui_id(id), "box");
    const bool drawn = switch_shapes(&g_ui_theme.art.slider_fill_sm);
    bool changed = false;
    UI_KIT_ROW(id, &m) {
        changed = drawn ? checkbox_shape(ctx, control, value, enabled)
                        : nt_ui_checkbox(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), UI_LAYER_TEXT, control, NULL, value,
                                         ui_kit_checkbox_style(), &FIT_DECL, enabled);
        row_label(ctx, label, enabled);
    }
    return changed;
}

bool ui_kit_radio_row(nt_ui_context_t *ctx, const char *id, const char *label, int *selected, int my_value, bool enabled) {
    const ui_metrics_t m = ui_metrics();
    const uint32_t control = nt_ui_child_id(nt_ui_id(id), "radio");
    const bool drawn = switch_shapes(&g_ui_theme.art.thumb);
    bool changed = false;
    UI_KIT_ROW(id, &m) {
        changed = drawn ? radio_shape(ctx, control, selected, my_value, enabled)
                        : nt_ui_radio(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), UI_LAYER_TEXT, control, NULL, selected,
                                      my_value, ui_kit_radio_style(), &FIT_DECL, enabled);
        row_label(ctx, label, enabled);
    }
    return changed;
}

// The thumb is slider_thumb.png: a white disc whose rim grows with it.
ui_kit_slider_look_t ui_kit_slider_look(void) {
    const ui_tokens_t *t = ui_theme_tokens();
    const float thumb = ui_css(g_ui_theme.slider.thumb_w);
    return (ui_kit_slider_look_t){.track = ui_kit_look_track(),
        .fill = ui_kit_look_fill(g_ui_theme.slider.states[NT_UI_SLIDER_IDLE].fill_tint),
        .knob = flat_plate(thumb * 0.5F, 0xFFFFFFFFU, t->shell, t->rim * thumb / 32.0F)};
}

// The track, the fill up to the thumb's centre and the thumb, floating over the
// engine slider that holds the input and draws nothing. Each clips to the row's
// clip, so the parts stay inside a scrolling body the way the engine's own
// thumb does, and lets the pointer through to the slider.
static void slider_shapes(nt_ui_context_t *ctx, uint32_t id, const nt_ui_slider_style_t *style, float value,
                          bool enabled, const ui_kit_slider_look_t *look) {
    const float alpha = enabled ? 1.0F : 0.4F;
    const float usable = fmaxf(style->track_w - style->thumb_w, 0.0F);
    float centre = style->thumb_w * 0.5F + value * usable;
    // Called after the slider this frame, the engine's thumb carries the eased
    // value; both reads share last frame's box, so their difference does not.
    const nt_ui_slider_thumb_t thumb = nt_ui_slider_thumb_pos(ctx, id);
    const nt_ui_bbox_t box = nt_ui_get_bbox(ctx, id);
    if (thumb.found && box.found) { centre = thumb.x - box.x; }
    const Clay_FloatingElementConfig at_start = {.attachTo = CLAY_ATTACH_TO_PARENT,
        .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT, .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
        .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_LEFT_CENTER}};

    ui_kit_panel_style_t track = fit_radius(look->track, style->track_h);
    track.alpha *= alpha;
    ui_kit_panel(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &track,
                 &(Clay_ElementDeclaration){.floating = at_start,
                     .layout = {.sizing = {CLAY_SIZING_FIXED(style->track_w), CLAY_SIZING_FIXED(style->track_h)}}});

    const float inset = look->track.outline_w;
    const float fill_w = fminf(centre, style->track_w - inset) - inset;
    if (fill_w > 1.0F) {
        const float fill_h = style->track_h - inset * 2.0F;
        ui_kit_panel_style_t fill = fit_radius(look->fill, fill_h);
        fill.alpha *= alpha;
        Clay_FloatingElementConfig at = at_start;
        at.offset = (Clay_Vector2){inset, 0.0F};
        ui_kit_panel(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &fill,
                     &(Clay_ElementDeclaration){.floating = at,
                         .layout = {.sizing = {CLAY_SIZING_FIXED(fill_w), CLAY_SIZING_FIXED(fill_h)}}});
    }

    ui_kit_panel_style_t knob = fit_radius(look->knob, style->thumb_h);
    knob.alpha *= alpha;
    Clay_FloatingElementConfig at = at_start;
    at.offset = (Clay_Vector2){centre - style->thumb_w * 0.5F, 0.0F};
    ui_kit_panel(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &knob,
                  &(Clay_ElementDeclaration){.floating = at,
                      .layout = {.sizing = {CLAY_SIZING_FIXED(style->thumb_w), CLAY_SIZING_FIXED(style->thumb_h)}}});
}

bool ui_kit_slider_row(nt_ui_context_t *ctx, const char *id, const char *caption, const char *value_text,
                       float *value, bool enabled, float row_width, const ui_kit_slider_look_t *look) {
    const ui_metrics_t m = ui_metrics();
    const float before = *value;
    const bool drawn = look != NULL || (shapes() && g_ui_theme.slider.states[NT_UI_SLIDER_IDLE].track.atlas.id == 0U);
    const ui_kit_slider_look_t own = look != NULL ? *look : drawn ? ui_kit_slider_look() : (ui_kit_slider_look_t){0};
    nt_ui_slider_style_t *slider = drawn ? slider_style_sized(&m) : ui_kit_slider_style(&m);
    const uint32_t control = nt_ui_child_id(nt_ui_id(id), "slider");
    const nt_ui_label_style_t *text = own.text != NULL ? own.text : enabled ? &g_ui_theme.label : &g_ui_theme.hint;
    const float gap = own.gap > 0.0F ? own.gap : m.gap * 0.4F;
    const bool one_line = !m.portrait || own.caption_w > 0.0F;
    const float caption_w = own.caption_w > 0.0F ? own.caption_w : row_width * 0.44F;
    const float value_w = value_text == NULL ? 0.0F : own.value_w > 0.0F ? own.value_w : m.hit * 1.2F;
    slider->track_w = (one_line ? row_width - caption_w - gap : row_width) - (value_text != NULL ? value_w + gap : 0.0F);
    slider->states[NT_UI_SLIDER_DISABLED].opacity = enabled ? 1.0F : 0.4F;
    const Clay_LayoutConfig line = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                    .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
                                    .childGap = (uint16_t)gap};
    Clay_LayoutConfig row = line;
    row.layoutDirection = one_line ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;
    CLAY({.id = element_id(id), .layout = row}) {
        CLAY({.layout = {.sizing = {one_line ? CLAY_SIZING_FIXED(caption_w) : CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
            ui_kit_label(ctx, caption, text);
        }
        CLAY({.layout = line}) {
            // The row is the touch target: the track is thin, and the thumb
            // alone is not something a thumb can find.
            const Clay_ElementDeclaration track = {.layout = {.sizing = {CLAY_SIZING_GROW(0), ui_kit_hit_height(&m)}}};
            if (!drawn) {
                (void)nt_ui_slider_float(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), UI_LAYER_TEXT, control, NULL, value,
                                         0.0F, 1.0F, 0.0F, slider, &track, enabled);
            } else {
                CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), ui_kit_hit_height(&m)},
                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                    (void)nt_ui_slider_float(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), UI_LAYER_TEXT, control, NULL,
                                             value, 0.0F, 1.0F, 0.0F, slider, &track, enabled);
                    slider_shapes(ctx, control, slider, *value, enabled, &own);
                }
            }
            if (value_text != NULL) {
                CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(value_w), CLAY_SIZING_FIT(0)},
                                 .childAlignment = {.x = CLAY_ALIGN_X_RIGHT}}}) {
                    ui_kit_label(ctx, value_text, text);
                }
            }
        }
    }
    return *value != before;
}

// The trigger and the list as kit buttons in `look`; the list is a
// light-dismiss popup over everything, its first option nearest the trigger.
static void dropdown_styled(nt_ui_context_t *ctx, const char *id, const char *const *options, int count,
                            int *selected, bool *open, bool enabled, float list_width,
                            const ui_kit_dropdown_look_t *look) {
    const ui_metrics_t m = ui_metrics();
    const char *current = derived_id(id, "current");
    const uint32_t trigger = nt_ui_child_id(nt_ui_id(current), "plate");
    const float face_h = look->trigger_h > 0.0F ? look->trigger_h : m.hit;
    const float lip = look->trigger.idle.lip_inside ? 0.0F : look->trigger.idle.lip_h;
    const float arrow = look->arrow > 0.0F ? look->arrow : m.hit * 0.4F;
    const float arrow_box = look->arrow_plate != NULL ? fmaxf(look->arrow_box, arrow) : arrow;
    const float arrow_inset = fmaxf((face_h - arrow_box) * 0.5F, 0.0F);
    const float inset = look->inset > 0.0F ? look->inset : m.gap * 0.8F;
    const nt_ui_label_style_t *text = look->text != NULL ? look->text : &g_ui_theme.button_label;
    nt_atlas_region_ref_t *glyph = ui_kit_icon_ref(UI_ICON_ARROW_DOWN);
    const uint32_t tint = look->arrow_tint != 0U ? look->arrow_tint : ui_theme_tokens()->ink;
    if (!enabled) { *open = false; }
    CLAY({.id = element_id(current), .layout = {.sizing = {CLAY_SIZING_FIXED(list_width), CLAY_SIZING_FIXED(face_h + lip)}}}) {
        look_button_begin(ctx, trigger, look_motion(ctx, trigger, look->trigger.reach, INFINITY, INFINITY, 0.0F, enabled), &look->trigger.idle,
                          look->trigger.disabled, NULL, enabled, NULL);
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = {.left = (uint16_t)inset, .right = (uint16_t)arrow_inset},
                         .childGap = (uint16_t)(m.gap * 0.4F),
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
                ui_kit_label(ctx, options[*selected], text);
            }
            // Open, the arrow turns to point the other way.
            const float turn = *open ? 3.14159265F : 0.0F;
            if (look->arrow_plate != NULL) {
                ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), look->arrow_plate,
                                          &(Clay_ElementDeclaration){
                                              .layout = {.sizing = {CLAY_SIZING_FIXED(arrow_box), CLAY_SIZING_FIXED(arrow_box)},
                                                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}});
            }
            if (glyph->atlas.id != 0U) { ui_kit_icon_rotated(ctx, glyph, arrow, tint, turn); }
            if (look->arrow_plate != NULL) { ui_kit_panel_styled_end(ctx); }
        }
        if (ui_kit_button_end(ctx)) { *open = !*open; }
    }

    nt_ui_popup_style_t popup = nt_ui_popup_style_defaults();
    popup.layer = UI_LAYER_SURFACE;
    nt_ui_popup_anchor_t anchor = {.prefer_side = (uint8_t)(look->above ? NT_UI_POPUP_ABOVE : NT_UI_POPUP_BELOW)};
    const nt_ui_bbox_t at = nt_ui_get_bbox(ctx, nt_ui_id(current));
    if (at.found) {
        anchor.x = at.x;
        anchor.y = at.y;
        anchor.w = at.width;
        anchor.h = at.height;
    }
    // Near the screen's edge the popup flips; the order and the gap follow the side it took.
    const nt_ui_popup_result_t shown =
        nt_ui_popup_begin(ctx, nt_ui_child_id(nt_ui_id(id), "list"), &popup, &anchor, *open);
    if (shown.close_requested) { *open = false; }
    if (shown.visible) {
        const bool above = shown.side == NT_UI_POPUP_ABOVE;
        const float space = m.gap * 0.5F;
        const float row_inset = fmaxf(inset - look->list_pad, 0.0F);
        CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(list_width), CLAY_SIZING_FIT(0)},
                         .padding = {.top = (uint16_t)(above ? 0.0F : space), .bottom = (uint16_t)(above ? space : 0.0F)}}}) {
            const float list_lip = look->list.lip_inside ? 0.0F : look->list.lip_h;
            ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &look->list,
                                      &(Clay_ElementDeclaration){
                                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                                     .padding = {(uint16_t)look->list_pad, (uint16_t)look->list_pad,
                                                                 (uint16_t)look->list_pad, (uint16_t)(look->list_pad + list_lip)},
                                                     .childGap = (uint16_t)(look->list_pad * 0.5F),
                                                     .layoutDirection = CLAY_TOP_TO_BOTTOM}});
            for (int k = 0; k < count; ++k) {
                const int i = above ? count - 1 - k : k;
                const bool picked = i == *selected;
                const ui_kit_button_look_t *row = picked ? &look->picked : &look->row;
                const float row_lip = row->idle.lip_inside ? 0.0F : row->idle.lip_h;
                const char *name = derived_index(id, i);
                const uint32_t plate = nt_ui_child_id(nt_ui_id(name), "plate");
                bool clicked = false;
                CLAY({.id = element_id(name),
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED((look->row_h > 0.0F ? look->row_h : m.hit) + row_lip)}}}) {
                    look_button_begin(ctx, plate, look_motion(ctx, plate, row->reach, INFINITY, look->list_pad * 0.25F, 0.0F, true), &row->idle,
                                      row->disabled, NULL, true, NULL);
                    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                     .padding = {(uint16_t)row_inset, (uint16_t)row_inset, 0, 0},
                                     .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}}}) {
                        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) { ui_kit_label(ctx, options[i], text); }
                        if (picked && look->mark != NULL) {
                            ui_kit_label(ctx, look->mark, look->mark_style != NULL ? look->mark_style : text);
                        }
                    }
                    clicked = ui_kit_button_end(ctx);
                }
                if (clicked) {
                    *selected = i;
                    *open = false;
                }
            }
            ui_kit_panel_styled_end(ctx);
        }
    }
    nt_ui_popup_end(ctx);
}

bool ui_kit_dropdown_row_styled(nt_ui_context_t *ctx, const char *id, const char *label, const char *const *options,
                                int count, int *selected, bool *open, bool enabled, float list_width,
                                const ui_kit_dropdown_look_t *look) {
    NT_ASSERT(look != NULL && "ui_kit_dropdown_row_styled: look is required");
    const ui_metrics_t m = ui_metrics();
    const int before = *selected;
    UI_KIT_ROW(id, &m) {
        row_label(ctx, label, enabled);
        dropdown_styled(ctx, id, options, count, selected, open, enabled, list_width, look);
    }
    return *selected != before;
}

bool ui_kit_dropdown_row(nt_ui_context_t *ctx, const char *id, const char *label, const char *const *options,
                         int count, int *selected, bool *open, bool enabled, float list_width) {
    static nt_ui_dropdown_style_t style;
    static uint32_t generation;
    const ui_metrics_t m = ui_metrics();
    const ui_tokens_t *t = ui_theme_tokens();
    const uint32_t base = nt_ui_id(id);
    const uint32_t trigger = nt_ui_child_id(base, "trigger");
    const int before = *selected;
    // Without the button art the engine trigger is its flat baseline, which
    // draws nothing here; the kit's button look and down glyph stand in it.
    const bool drawn = shapes() && g_ui_theme.dropdown.trigger_idle.bg.atlas.id == 0U;
    if (generation != g_ui_theme.generation) {
        style = g_ui_theme.dropdown;
        generation = g_ui_theme.generation;
    }
    style.font_size = ui_css(g_ui_theme.dropdown.font_size);
    style.row_height = (uint16_t)ui_css((float)g_ui_theme.dropdown.row_height);
    style.pad = (uint16_t)ui_css((float)g_ui_theme.dropdown.pad);
    style.chevron_size = drawn ? 0U : (uint16_t)ui_css((float)g_ui_theme.dropdown.chevron_size);
    style.min_width = (uint16_t)list_width;
    style.max_visible_rows = (uint16_t)count;
    if (!enabled) { *open = false; }
    UI_KIT_ROW(id, &m) {
        row_label(ctx, label, enabled);
        if (!enabled) {
            // The engine combo has no disabled state: a locked row names its
            // value and opens nothing.
            CLAY({.id = element_id(derived_id(id, "current")),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(list_width), CLAY_SIZING_FIT(0)}}}) {
                ui_kit_label(ctx, options[*selected], &g_ui_theme.hint);
            }
        } else {
        // The named wrapper is the trigger's ui.tree name; the engine's own
        // element carries only a hash.
        CLAY({.id = element_id(derived_id(id, "current")),
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_combo_preview_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), UI_LAYER_TEXT, trigger, &style, open);
            if (drawn) {
                const nt_ui_interaction_t in = nt_ui_query_interaction(ctx, trigger);
                const ui_kit_panel_style_t idle = button_plate(t->tile);
                const ui_kit_panel_style_t look_now = ui_kit_look_state(
                    &idle, in.pressed && in.hovered ? UI_KIT_LOOK_PRESSED : UI_KIT_LOOK_IDLE, rgb_of(t->off));
                nt_atlas_region_ref_t *arrow = ui_kit_icon_ref(UI_ICON_ARROW_DOWN);
                ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &look_now,
                                          &(Clay_ElementDeclaration){
                                              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                         .padding = {.left = (uint16_t)(m.gap * 0.8F),
                                                                     .right = (uint16_t)(m.gap * 0.6F)},
                                                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                                         .childGap = (uint16_t)(m.gap * 0.4F),
                                                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}});
                CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
                    ui_kit_label(ctx, options[*selected], &g_ui_theme.button_label);
                }
                if (arrow->atlas.id != 0U) { ui_kit_icon_tinted(ctx, arrow, m.hit * 0.4F, t->ink); }
                ui_kit_panel_styled_end(ctx);
            } else {
                CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
                    ui_kit_label(ctx, options[*selected], &g_ui_theme.button_label);
                }
            }
            if (nt_ui_combo_preview_end(ctx)) {
                for (int i = 0; i < count; ++i) {
                    nt_ui_combo_selectable_begin(ctx, (uint32_t)i, i == *selected);
                    CLAY({.id = element_id(derived_index(id, i)),
                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
                        ui_kit_label(ctx, options[i], &g_ui_theme.button_label);
                    }
                    if (nt_ui_combo_selectable_end(ctx)) {
                        *selected = i;
                    }
                }
                nt_ui_combo_end(ctx);
            }
        }
        }
    }
    return *selected != before;
}

#define UI_KIT_CHOICE_MAX 8

bool ui_kit_choice_row(nt_ui_context_t *ctx, const char *id, const char *label, const char *const *options, int count,
                       int *selected, bool enabled, const ui_kit_choice_look_t *look) {
    NT_ASSERT(count > 0 && count <= UI_KIT_CHOICE_MAX && "ui_kit_choice_row: 1..8 options");
    const ui_metrics_t m = ui_metrics();
    const uint32_t cells_id = nt_ui_child_id(nt_ui_id(id), "options");
    const int before = *selected;
    const bool labelled = look != NULL && look->label_w > 0.0F;
    const bool one_line = !m.portrait || labelled;
    const uint16_t gap = (uint16_t)(look != NULL && look->gap > 0.0F ? look->gap : m.gap * 0.4F);
    const float cell_h = look != NULL && look->option_h > 0.0F ? look->option_h : m.hit;
    // Equal cells over last frame's width; the first frame lets Clay share it.
    const Clay_ElementData last = Clay_GetElementData((Clay_ElementId){.id = cells_id});
    ui_kit_cell_t cells[UI_KIT_CHOICE_MAX];
    const float pad = ui_kit_choice_cells(count, last.found ? last.boundingBox.width : 0.0F, (float)gap,
                                          look != NULL ? INFINITY : (float)g_ui_theme.button.hit_padding_lrtb[0], cells);
    CLAY({.id = element_id(id),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = one_line ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
                     .childGap = labelled ? 0U : gap}}) {
        const Clay_SizingAxis label_w = labelled ? CLAY_SIZING_FIXED(look->label_w)
                                        : one_line ? CLAY_SIZING_PERCENT(0.36F)
                                                   : CLAY_SIZING_GROW(0);
        CLAY({.layout = {.sizing = {label_w, CLAY_SIZING_FIT(0)}}}) {
            ui_kit_label(ctx, label, enabled ? &g_ui_theme.label : &g_ui_theme.hint);
        }
        CLAY({.id = {.id = cells_id},
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = gap}}) {
            for (int i = 0; i < count; ++i) {
                const char *name = derived_index(id, i);
                const uint32_t plate = nt_ui_child_id(nt_ui_id(name), "plate");
                const ui_kit_button_kind_t kind = i == *selected ? UI_KIT_BUTTON_CONFIRM : UI_KIT_BUTTON_NEUTRAL;
                const nt_ui_label_style_t *text = look != NULL && look->text != NULL ? look->text
                                                  : enabled ? ui_kit_button_label_style(kind)
                                                            : &g_ui_theme.button_label_disabled;
                const ui_kit_button_look_t *cell_look = look == NULL ? NULL : i == *selected ? &look->picked : &look->option;
                const float lip = cell_look != NULL && !cell_look->idle.lip_inside ? cell_look->idle.lip_h : 0.0F;
                CLAY({.id = element_id(name),
                      .layout = {.sizing = {last.found ? CLAY_SIZING_FIXED(cells[i].w) : CLAY_SIZING_GROW(0),
                                            CLAY_SIZING_FIXED(cell_h + lip)}}}) {
                    if (cell_look != NULL) {
                        look_button_begin(ctx, plate, look_motion(ctx, plate, cell_look->reach, pad, INFINITY, pad, enabled), &cell_look->idle,
                                          cell_look->disabled, NULL, enabled, NULL);
                    } else {
                        nt_ui_button_style_t style = *ui_kit_button_style(kind);
                        style.hit_padding_lrtb[0] = (int16_t)pad;
                        style.hit_padding_lrtb[1] = (int16_t)pad;
                        ui_kit_button_begin(ctx, plate, &style, enabled, NULL);
                    }
                    ui_kit_label(ctx, options[i], text);
                    if (ui_kit_button_end(ctx)) { *selected = i; }
                }
            }
        }
    }
    return *selected != before;
}
