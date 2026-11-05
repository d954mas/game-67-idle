#include "features/ui_kit/ui_look.h"

#include "ui/nt_ui_fit.h"

#include <math.h>
#include <string.h>

#define UI_LOOK_PI 3.14159265F

static const nt_ui_element_data_t *surface(void) { return NT_UI_DATA_LAYER(UI_LAYER_SURFACE); }

static const Clay_Sizing k_grow = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
static const Clay_ChildAlignment k_centre = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};

static void open(const Clay_ElementDeclaration *decl) {
    Clay__OpenElement();
    Clay__ConfigureOpenElement(*decl);
}

static Clay_ElementId named(const char *id) {
    return Clay_GetElementId((Clay_String){.length = (int32_t)strlen(id), .chars = id});
}

Clay_Color ui_rgb_color(uint32_t rgb, float alpha) {
    return (Clay_Color){(float)((rgb >> 16U) & 0xFFU), (float)((rgb >> 8U) & 0xFFU), (float)(rgb & 0xFFU),
        ui_rgb_clamp01(alpha) * 255.0F};
}

nt_ui_label_style_t ui_text_outlined(float size, uint32_t rgb, uint32_t stroke_rgb, float stroke_em) {
    nt_ui_label_style_t style;
    memset(&style, 0, sizeof style);
    style.font_size = size;
    style.color = ui_rgb_tint(rgb, 1.0F);
    style.wrap_mode = CLAY_TEXT_WRAP_NONE;
    if (stroke_em > 0.0F) {
        style.outline_w = stroke_em;
        style.outline_color = ui_rgb_tint(stroke_rgb, 1.0F);
    }
    return style;
}

static uint32_t faded(uint32_t abgr, float alpha) { return nt_color_scale_alpha(abgr, ui_rgb_clamp01(alpha)); }

void ui_text_fade(nt_ui_label_style_t *style, float alpha) {
    style->color = faded(style->color, alpha);
    style->outline_color = faded(style->outline_color, alpha);
    style->shadow_color = faded(style->shadow_color, alpha);
}

void ui_text_fit_width(nt_ui_context_t *ctx, const char *text, nt_ui_label_style_t *style, float width, float min_size) {
    const float outline = 2.0F * style->outline_w * style->font_size;
    // A view narrower than the widget leaves no room at all: nothing fits, so
    // the answer is the floor, as for any overflow.
    if (!(width - outline > 0.0F)) {
        style->font_size = min_size;
        return;
    }
    style->font_size = (float)nt_ui_fit_width(ctx, style->font_id, text != NULL ? text : "", width - outline,
        (uint16_t)min_size, (uint16_t)style->font_size, (float)style->letter_tracking);
}

static float squeeze(float radius, float height) {
    const float corners = 2.0F * (radius + 2.0F);
    return height > 0.0F && height < corners ? height / corners : 1.0F;
}

ui_kit_panel_style_t ui_look_face(const ui_look_family_t *family, uint32_t top, uint32_t bottom, float height,
    float radius, float outline) {
    const float k = squeeze(family->radius, height);
    const float r = (radius > 0.0F ? radius : family->radius) * k;
    return (ui_kit_panel_style_t){.radius = {r, r, r, r}, .top_rgb = family->gradient ? top : bottom,
        .bottom_rgb = bottom, .outline_w = (outline > 0.0F ? outline : family->border) * k,
        .outline_rgb = family->ink, .lip_rgb = family->ink, .gloss_cap = family->gloss_cap, .alpha = 1.0F};
}

ui_kit_panel_style_t ui_look_framed(const ui_look_family_t *family, uint32_t fill, uint32_t frame, float height) {
    const float k = squeeze(family->frame_r, height);
    const float r = family->frame_r * k;
    return (ui_kit_panel_style_t){.radius = {r, r, r, r}, .top_rgb = fill, .bottom_rgb = fill,
        .outline_w = family->border * k, .outline_rgb = frame, .face_radius = family->face_r * k,
        .lip_rgb = family->ink, .gloss_cap = family->gloss_cap, .alpha = 1.0F};
}

ui_kit_radial_style_t ui_look_disc(uint32_t top, uint32_t bottom, float rim, float lift, uint32_t ink) {
    return (ui_kit_radial_style_t){.angle_end = 2.0F * UI_LOOK_PI, .top_rgb = top, .bottom_rgb = bottom, .outline_w = rim,
        .outline_rgb = ink, .lip_h = lift, .lip_rgb = ink, .alpha = 1.0F};
}

// The caller's sizing with the lift added under a fixed height.
static Clay_Sizing with_lift(Clay_Sizing sizing, float lift) {
    if (sizing.height.type == CLAY__SIZING_TYPE_FIXED) {
        sizing.height.size.minMax.min += lift;
        sizing.height.size.minMax.max += lift;
    }
    return sizing;
}

void ui_kit_lifted_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, const ui_kit_panel_style_t *look) {
    const float lift = look->lip_h;
    ui_kit_panel_begin_styled(ctx, surface(), look,
        &(Clay_ElementDeclaration){.id = decl->id, .floating = decl->floating,
            .layout = {.sizing = with_lift(decl->layout.sizing, lift), .padding = {.bottom = (uint16_t)lift}}});
    Clay_LayoutConfig face = decl->layout;
    face.sizing = k_grow;
    open(&(Clay_ElementDeclaration){.layout = face});
}

void ui_kit_lifted_end(nt_ui_context_t *ctx) {
    Clay__CloseElement();
    ui_kit_panel_styled_end(ctx);
}

void ui_kit_lifted_disc_begin(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, const ui_kit_radial_style_t *look) {
    const uint16_t rim = (uint16_t)ceilf(look->outline_w);
    ui_kit_radial_begin_styled(ctx, surface(), look,
        &(Clay_ElementDeclaration){.id = decl->id, .floating = decl->floating,
            .layout = {.sizing = with_lift(decl->layout.sizing, look->lip_h),
                       .padding = {rim, rim, rim, (uint16_t)(rim + (uint16_t)look->lip_h)}}});
    open(&(Clay_ElementDeclaration){.layout = {.sizing = k_grow, .childAlignment = k_centre}});
}

void ui_kit_lifted_disc_end(nt_ui_context_t *ctx) {
    Clay__CloseElement();
    ui_kit_radial_styled_end(ctx);
}

// A flat disc or ring `size` across, centred on the open element.
static void centred_radial(nt_ui_context_t *ctx, uint32_t rgb, float alpha, float inner, float from, float to, float size) {
    const ui_kit_radial_style_t style = {.angle_start = from, .angle_end = to, .inner_norm = inner, .top_rgb = rgb,
        .bottom_rgb = rgb, .alpha = alpha};
    ui_kit_radial(ctx, surface(), &style,
        &(Clay_ElementDeclaration){
            .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                         .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                         .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH},
            .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)}}});
}

void ui_kit_arc(nt_ui_context_t *ctx, float outer, float thickness, float start, float share, uint32_t rgb, float alpha) {
    const float full = 2.0F * UI_LOOK_PI;
    const float top = UI_LOOK_PI * 0.5F - start * full;
    centred_radial(ctx, rgb, alpha, 1.0F - thickness * 2.0F / outer, top - ui_rgb_clamp01(share) * full, top, outer);
}

void ui_kit_well(nt_ui_context_t *ctx, const Clay_ElementDeclaration *decl, uint32_t fill, float fill_alpha,
    uint32_t rim_rgb, float rim_alpha, float rim) {
    const float full = 2.0F * UI_LOOK_PI;
    const float d = decl->layout.sizing.width.size.minMax.min;
    open(decl);
    centred_radial(ctx, fill, fill_alpha, 0.0F, 0.0F, full, d - rim * 2.0F);
    centred_radial(ctx, rim_rgb, rim_alpha, d > 0.0F ? 1.0F - rim * 2.0F / d : 0.9F, 0.0F, full, d);
    Clay__CloseElement();
}

void ui_kit_bar(nt_ui_context_t *ctx, const char *id, const ui_bar_t *bar) {
    ui_kit_panel_style_t track = ui_look_face(bar->family, bar->track, bar->track, bar->h, bar->radius, bar->outline);
    track.alpha = bar->alpha;
    ui_kit_panel_begin_styled(ctx, surface(), &track,
        &(Clay_ElementDeclaration){.id = named(id),
                                   .layout = {.sizing = {CLAY_SIZING_FIXED(bar->w), CLAY_SIZING_FIXED(bar->h)}}});
    const float fill_w = bar->w * ui_rgb_clamp01(bar->ratio);
    const float least = bar->family->radius * 2.0F;
    if (fill_w >= least * 0.5F || (bar->radius > 0.0F && fill_w > 0.0F)) {
        const float drawn_w = bar->radius > 0.0F ? fill_w : fmaxf(fill_w, least);
        ui_kit_panel_style_t fill = ui_look_face(bar->family, bar->top, bar->bottom, bar->h,
            bar->radius > 0.0F ? fminf(bar->radius, drawn_w * 0.5F) : 0.0F, bar->outline);
        fill.alpha = bar->alpha;
        ui_kit_panel(ctx, surface(), &fill,
            &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(drawn_w), CLAY_SIZING_GROW(0)}}});
    }
    if (bar->caption != NULL) {
        CLAY({.floating = {.attachTo = CLAY_ATTACH_TO_PARENT, .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT), bar->caption, bar->caption_style);
        }
    }
    ui_kit_panel_styled_end(ctx);
}

bool ui_kit_text_button_styled(nt_ui_context_t *ctx, const char *id, const ui_kit_button_look_t *look, Clay_SizingAxis w,
    float h, const char *text, const nt_ui_label_style_t *style, const nt_ui_label_style_t *disabled_style, float pad,
    bool enabled) {
    bool clicked = false;
    const float lip = look->idle.lip_inside ? 0.0F : look->idle.lip_h;
    CLAY({.id = named(id), .layout = {.sizing = {w, CLAY_SIZING_FIXED(h + lip)}}}) {
        ui_kit_button_begin_styled(ctx, nt_ui_child_id(nt_ui_id(id), "plate"), look, enabled, NULL);
        CLAY({.layout = {.padding = {(uint16_t)pad, (uint16_t)pad, 0, 0}}}) {
            const bool off = !enabled && disabled_style != NULL && style->outline_w > 0.0F;
            nt_ui_label(ctx, NT_UI_DATA_LAYER(UI_LAYER_TEXT), text != NULL ? text : "", off ? disabled_style : style);
        }
        clicked = ui_kit_button_end(ctx);
    }
    return clicked;
}
