#include "features/ui_kit/ui_kit.h"

#include "ui/nt_ui_image.h"
#include "ui/nt_ui_panel.h"

#include <string.h>

static const char *text_of(const char *text) { return text != NULL ? text : ""; }

void ui_kit_panel_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl) {
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_BG), &g_ui_theme.art.panel, &g_ui_theme.plate_img, decl);
}

void ui_kit_panel_end(nt_ui_context_t *ctx) { nt_ui_panel_end(ctx); }

void ui_kit_tile_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl) {
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_BG), &g_ui_theme.art.tile, &g_ui_theme.plate_img, decl);
}

void ui_kit_tile_end(nt_ui_context_t *ctx) { nt_ui_panel_end(ctx); }

void ui_kit_scrim(nt_ui_context_t *ctx) {
    (void)ctx;
    const uint32_t c = ui_theme_tokens()->scrim;
    CLAY({.id = CLAY_ID("ui_kit/scrim"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                        .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
          .backgroundColor = {(float)(c & 0xFFU), (float)((c >> 8) & 0xFFU), (float)((c >> 16) & 0xFFU),
                              (float)((c >> 24) & 0xFFU)},
          .userData = NT_UI_CLAY_DATA(UI_LAYER_SCRIM)}) {}
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

nt_ui_slider_style_t ui_kit_slider_style(const ui_metrics_t *m) {
    nt_ui_slider_style_t s = g_ui_theme.slider;
    s.track_h = ui_css(s.track_h);
    s.thumb_w = ui_css(s.thumb_w);
    s.thumb_h = ui_css(s.thumb_h);
    // The track is whatever the row gives it; a fixed width would overflow the
    // narrow frame a plate gets on a phone.
    s.track_w = m->panel_w - m->pad * 2.0F;
    return s;
}

Clay_SizingAxis ui_kit_hit_height(const ui_metrics_t *m) { return CLAY_SIZING_FIXED(m->hit); }
