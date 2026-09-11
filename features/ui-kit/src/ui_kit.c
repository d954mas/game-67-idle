#include "features/ui_kit/ui_kit.h"

#include "ui/nt_ui_image.h"
#include "ui/nt_ui_modal.h"
#include "ui/nt_ui_dropdown.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_scroll.h"

#include <stdio.h>
#include <string.h>

static const char *text_of(const char *text) { return text != NULL ? text : ""; }

// Through Clay's own hash, with the string kept: that is what names the
// element in the ui.tree a DevAPI bot reads. Callers pass strings that outlive
// the frame (literals, statics).
static Clay_ElementId element_id(const char *s) {
    return Clay_GetElementId((Clay_String){.length = (int32_t)strlen(s), .chars = s});
}

static Clay_Color clay_color(uint32_t abgr) {
    return (Clay_Color){(float)(abgr & 0xFFU), (float)((abgr >> 8) & 0xFFU), (float)((abgr >> 16) & 0xFFU),
                        (float)((abgr >> 24) & 0xFFU)};
}

// ---- primitives -----------------------------------------------------------

void ui_kit_panel_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl) {
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_BG), &g_ui_theme.art.panel, &g_ui_theme.plate_img, decl);
}

void ui_kit_panel_end(nt_ui_context_t *ctx) { nt_ui_panel_end(ctx); }

void ui_kit_tile_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl) {
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_BG), &g_ui_theme.art.tile, &g_ui_theme.plate_img, decl);
}

void ui_kit_tile_end(nt_ui_context_t *ctx) { nt_ui_panel_end(ctx); }

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
    shadow.color = (Clay_Color){8.0F, 5.0F, 3.0F, 142.0F};
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

void ui_kit_button_begin(nt_ui_context_t *ctx, uint32_t id, nt_ui_button_style_t *style, bool enabled,
                         const nt_ui_events_cfg_t *cfg) {
    nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), id, style,
                       &(Clay_ElementDeclaration){
                           .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                      .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                       enabled, cfg);
}

bool ui_kit_button_end(nt_ui_context_t *ctx) { return nt_ui_button_end(ctx); }

void ui_kit_meter(nt_ui_context_t *ctx, float w, float h, float ratio, uint32_t fill_tint) {
    if (ratio < 0.0F) {
        ratio = 0.0F;
    }
    if (ratio > 1.0F) {
        ratio = 1.0F;
    }
    // Both pieces float against the element the caller has open, so a meter
    // never disturbs the row's own layout.
    nt_ui_image_style_t art = g_ui_theme.plate_img;
    nt_ui_image(ctx, NT_UI_DATA_LAYER(UI_LAYER_BG), &g_ui_theme.art.slider_track, &art,
                &(Clay_ElementDeclaration){
                    .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                                 .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER,
                                                  .parent = CLAY_ATTACH_POINT_LEFT_CENTER}},
                    .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}}});

    // The pill rides inside the track's own rim, so the recess stays visible at
    // a full bar instead of being painted over by it.
    const float inset = ui_css(ui_theme_tokens()->rim);
    const float fill_w = (w - inset * 2.0F) * ratio;
    if (fill_w <= 1.0F) {
        return;
    }
    nt_ui_image_style_t fill = g_ui_theme.plate_img;
    fill.color_packed = fill_tint;
    nt_ui_image(ctx, NT_UI_DATA_LAYER(UI_LAYER_FILL), &g_ui_theme.art.slider_fill, &fill,
                &(Clay_ElementDeclaration){
                    .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                                 .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER,
                                                  .parent = CLAY_ATTACH_POINT_LEFT_CENTER},
                                 .offset = {inset, 0.0F}},
                    .layout = {.sizing = {CLAY_SIZING_FIXED(fill_w), CLAY_SIZING_FIXED(h - inset * 2.0F)}}});
}

nt_ui_slider_style_t *ui_kit_slider_style(const ui_metrics_t *m) {
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
    nt_ui_label_style_t glyph = g_ui_theme.button_label_action;
    glyph.wrap_mode = CLAY_TEXT_WRAP_NONE;
    // Centred a little inside the corner, so the circle reads as part of the
    // plate and the safe area never clips half of it.
    const float inset = m.gap * 0.4F;
    CLAY({.id = element_id(id),
          .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {-inset, inset}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(m.hit), CLAY_SIZING_FIXED(m.hit)}}}) {
        ui_kit_button_begin(ctx, nt_ui_child_id(nt_ui_id(id), "plate"), &g_ui_theme.button_close, true, NULL);
        ui_kit_label_scaled(ctx, label, &glyph, 1.3F);
        clicked = ui_kit_button_end(ctx);
    }
    return clicked;
}

void ui_kit_icon(nt_ui_context_t *ctx, nt_atlas_region_ref_t *icon, float size) {
    const nt_ui_image_style_t style = nt_ui_image_style_defaults();
    nt_ui_image(ctx, NT_UI_DATA_LAYER(UI_LAYER_ICON), icon, &style,
                &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)}}});
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
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), region, &art, decl);
}

void ui_kit_plate_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, uint32_t tint) {
    tinted_begin(ctx, &g_ui_theme.art.button, decl, tint);
}

void ui_kit_plate_end(nt_ui_context_t *ctx) { nt_ui_panel_end(ctx); }

void ui_kit_disc_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, uint32_t tint) {
    tinted_begin(ctx, &g_ui_theme.art.thumb, decl, tint);
}

void ui_kit_disc_end(nt_ui_context_t *ctx) { nt_ui_panel_end(ctx); }

void ui_kit_badge(nt_ui_context_t *ctx, const char *text) {
    const ui_metrics_t m = ui_metrics();
    const ui_tokens_t *t = ui_theme_tokens();
    const float d = m.hit * 0.55F;
    nt_ui_label_style_t style = g_ui_theme.hint;
    style.color = clay_color(t->on_action != 0U ? t->on_action : t->on_panel);
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

// The close's derived name has to outlive the frame for the ui.tree; a few
// dialogs per frame is the most a screen opens.
static const char *derived_id(const char *id, const char *suffix) {
    static char ring[4][128];
    static unsigned slot;
    char *buf = ring[slot++ % 4U];
    (void)snprintf(buf, sizeof ring[0], "%s/%s", id, suffix);
    return buf;
}

bool ui_kit_dialog_begin(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label,
                         Clay_SizingAxis w, Clay_SizingAxis h) {
    const ui_metrics_t m = ui_metrics();
    bool closed = false;
    // The named wrapper carries the size; the plate inside it fills it.
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){.id = element_id(id), .layout = {.sizing = {w, h}}});
    // The band sits inside the panel's rim, so the plate keeps only its rim as
    // padding up there; the body brings the kit's padding back.
    ui_kit_panel_begin(ctx, &(Clay_ElementDeclaration){
                                .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                           .padding = {.left = (uint16_t)m.rim, .right = (uint16_t)m.rim,
                                                       .top = (uint16_t)m.rim, .bottom = (uint16_t)(m.pad * 0.5F)},
                                           .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                           .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    if (title != NULL) {
        nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_FILL), &g_ui_theme.art.header, &g_ui_theme.plate_img,
                          &(Clay_ElementDeclaration){
                              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(m.hit)},
                                         .padding = {.left = (uint16_t)(m.pad - m.rim), .right = (uint16_t)m.hit},
                                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}});
        ui_kit_label(ctx, title, &g_ui_theme.header_title);
        nt_ui_panel_end(ctx);
    }
    if (close_label != NULL) {
        closed = ui_kit_close_button(ctx, derived_id(id, "close"), close_label);
    }
    Clay__OpenElement();
    Clay__ConfigureOpenElement((Clay_ElementDeclaration){
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .padding = {.left = (uint16_t)(m.pad - m.rim), .right = (uint16_t)(m.pad - m.rim),
                               .top = (uint16_t)(title != NULL ? m.gap : m.pad * 0.5F - m.rim)},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = (uint16_t)m.gap,
                   .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    return closed;
}

void ui_kit_dialog_end(nt_ui_context_t *ctx) {
    Clay__CloseElement();
    ui_kit_panel_end(ctx);
    Clay__CloseElement();
}

bool ui_kit_sheet_begin(nt_ui_context_t *ctx, const char *id, const char *title, const char *close_label, bool *open,
                        float w, float h) {
    const nt_ui_modal_style_t style = sheet_style();
    if (!nt_ui_modal_visible(ctx, nt_ui_id(id), &style, open)) {
        return false;
    }
    if (ui_kit_dialog_begin(ctx, derived_id(id, "panel"), title, close_label, CLAY_SIZING_FIXED(w),
                            h > 0.0F ? CLAY_SIZING_FIXED(h) : CLAY_SIZING_FIT(0))) {
        *open = false;
    }
    return true;
}

void ui_kit_sheet_end(nt_ui_context_t *ctx) {
    ui_kit_dialog_end(ctx);
    nt_ui_modal_end(ctx);
}

void ui_kit_sheet_clear(nt_ui_context_t *ctx, const char *id) { nt_ui_modal_clear_state(ctx, nt_ui_id(id)); }

nt_ui_checkbox_style_t *ui_kit_toggle_style(void) { return &g_ui_theme.toggle; }
nt_ui_checkbox_style_t *ui_kit_checkbox_style(void) { return &g_ui_theme.checkbox; }
nt_ui_checkbox_style_t *ui_kit_radio_style(void) { return &g_ui_theme.radio; }

// A settings row: the label fills the row, the control sits at one end, and
// the whole row is one touch-height line.
#define UI_KIT_ROW(id_str, mp)                                                 \
    CLAY({.id = element_id(id_str),                                            \
          .layout = {.sizing = {CLAY_SIZING_GROW(0), ui_kit_hit_height(mp)},   \
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,                    \
                     .childGap = (uint16_t)(mp)->gap,                          \
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}})

static void row_label(nt_ui_context_t *ctx, const char *label, bool enabled) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
        ui_kit_label(ctx, label, enabled ? &g_ui_theme.label : &g_ui_theme.hint);
    }
}

static const Clay_ElementDeclaration FIT_DECL = {.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}};

bool ui_kit_toggle_row(nt_ui_context_t *ctx, const char *id, const char *label, bool *value, bool enabled) {
    const ui_metrics_t m = ui_metrics();
    nt_ui_checkbox_style_t *style = ui_kit_toggle_style();
    bool changed = false;
    UI_KIT_ROW(id, &m) {
        row_label(ctx, label, enabled);
        changed = nt_ui_toggle(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), UI_LAYER_TEXT, nt_ui_child_id(nt_ui_id(id), "toggle"),
                               NULL, value, style, &FIT_DECL, enabled);
    }
    return changed;
}

bool ui_kit_checkbox_row(nt_ui_context_t *ctx, const char *id, const char *label, bool *value, bool enabled) {
    const ui_metrics_t m = ui_metrics();
    nt_ui_checkbox_style_t *style = ui_kit_checkbox_style();
    bool changed = false;
    UI_KIT_ROW(id, &m) {
        changed = nt_ui_checkbox(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), UI_LAYER_TEXT, nt_ui_child_id(nt_ui_id(id), "box"),
                                 NULL, value, style, &FIT_DECL, enabled);
        row_label(ctx, label, enabled);
    }
    return changed;
}

bool ui_kit_radio_row(nt_ui_context_t *ctx, const char *id, const char *label, int *selected, int my_value, bool enabled) {
    const ui_metrics_t m = ui_metrics();
    nt_ui_checkbox_style_t *style = ui_kit_radio_style();
    bool changed = false;
    UI_KIT_ROW(id, &m) {
        changed = nt_ui_radio(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), UI_LAYER_TEXT, nt_ui_child_id(nt_ui_id(id), "radio"),
                              NULL, selected, my_value, style, &FIT_DECL, enabled);
        row_label(ctx, label, enabled);
    }
    return changed;
}

bool ui_kit_slider_row(nt_ui_context_t *ctx, const char *id, const char *caption, float *value, bool enabled,
                       float row_width) {
    const ui_metrics_t m = ui_metrics();
    const float before = *value;
    const bool landscape = !m.portrait;
    nt_ui_slider_style_t *slider = ui_kit_slider_style(&m);
    slider->track_w = landscape ? row_width * 0.56F - m.gap * 0.4F : row_width;
    slider->states[NT_UI_SLIDER_DISABLED].opacity = enabled ? 1.0F : 0.4F;
    CLAY({.id = element_id(id),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = landscape ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
                     .childGap = (uint16_t)(m.gap * 0.4F)}}) {
        CLAY({.layout = {.sizing = {landscape ? CLAY_SIZING_PERCENT(0.44F) : CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
            ui_kit_label(ctx, caption, enabled ? &g_ui_theme.label : &g_ui_theme.hint);
        }
        // The row is the touch target: the track is thin, and the thumb alone
        // is not something a thumb can find.
        (void)nt_ui_slider_float(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), UI_LAYER_TEXT, nt_ui_child_id(nt_ui_id(id), "slider"),
                                 NULL, value, 0.0F, 1.0F, 0.0F, slider,
                                 &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), ui_kit_hit_height(&m)}}},
                                 enabled);
    }
    return *value != before;
}

bool ui_kit_dropdown_row(nt_ui_context_t *ctx, const char *id, const char *label, const char *const *options,
                         int count, int *selected, bool *open, float list_width) {
    static nt_ui_dropdown_style_t style;
    static uint32_t generation;
    const ui_metrics_t m = ui_metrics();
    const uint32_t base = nt_ui_id(id);
    const int before = *selected;
    if (generation != g_ui_theme.generation) {
        style = g_ui_theme.dropdown;
        generation = g_ui_theme.generation;
    }
    style.font_size = ui_css(g_ui_theme.dropdown.font_size);
    style.row_height = (uint16_t)ui_css((float)g_ui_theme.dropdown.row_height);
    style.pad = (uint16_t)ui_css((float)g_ui_theme.dropdown.pad);
    style.min_width = (uint16_t)list_width;
    style.max_visible_rows = (uint16_t)count;
    UI_KIT_ROW(id, &m) {
        row_label(ctx, label, true);
        nt_ui_combo_preview_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), UI_LAYER_TEXT, nt_ui_child_id(base, "trigger"),
                                  &style, open);
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
            ui_kit_label(ctx, options[*selected], &g_ui_theme.button_label);
        }
        if (nt_ui_combo_preview_end(ctx)) {
            for (int i = 0; i < count; ++i) {
                char key[16];
                (void)snprintf(key, sizeof key, "%d", i);
                nt_ui_combo_selectable_begin(ctx, (uint32_t)i, i == *selected);
                CLAY({.id = (Clay_ElementId){.id = nt_ui_child_id(base, key)},
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
    return *selected != before;
}
