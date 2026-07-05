#include "ui/tutorial_callout.h"

#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"

#define TUTORIAL_CALLOUT_LAYER_BG 28
#define TUTORIAL_CALLOUT_LAYER_SHADOW 29
#define TUTORIAL_CALLOUT_LAYER_TEXT 30
#define TUTORIAL_CALLOUT_LAYER_FINGER 31
#define TUTORIAL_CALLOUT_Z_INDEX (NT_UI_MODAL_ZBAND_STRIDE + 80)

static nt_resource_t s_ui_atlas;
static nt_atlas_region_ref_t s_finger_region;

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static Clay_FloatingAttachPointType clay_anchor(tutorial_callout_anchor_t anchor) {
    switch (anchor) {
        case TUTORIAL_CALLOUT_ANCHOR_LEFT_TOP:
            return CLAY_ATTACH_POINT_LEFT_TOP;
        case TUTORIAL_CALLOUT_ANCHOR_CENTER_TOP:
            return CLAY_ATTACH_POINT_CENTER_TOP;
        case TUTORIAL_CALLOUT_ANCHOR_CENTER_BOTTOM:
            return CLAY_ATTACH_POINT_CENTER_BOTTOM;
        case TUTORIAL_CALLOUT_ANCHOR_RIGHT_BOTTOM:
            return CLAY_ATTACH_POINT_RIGHT_BOTTOM;
        case TUTORIAL_CALLOUT_ANCHOR_CENTER:
        default:
            return CLAY_ATTACH_POINT_CENTER_CENTER;
    }
}

static nt_ui_label_style_t label_style(float font_size, Clay_Color color) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = font_size, .color = color};
}

static void ensure_finger_region(void) {
    if (s_ui_atlas.id != 0U) {
        return;
    }
    s_ui_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    s_finger_region = nt_atlas_ref(s_ui_atlas, ASSET_ATLAS_REGION_UI_TUTORIAL_FINGER.value);
}

static void callout_label(nt_ui_context_t *ctx, uint32_t slot, const char *text, const tutorial_callout_style_t *style) {
    const nt_ui_label_style_t text_style = label_style(style->font_size, style->text);
    const nt_ui_label_style_t shadow_style = label_style(style->font_size, style->shadow);

    CLAY({.id = CLAY_IDI("tutorial_callout/label", slot),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        CLAY({.id = CLAY_IDI("tutorial_callout/label_shadow", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                           .offset = {1.0F, 1.0F}},
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(TUTORIAL_CALLOUT_LAYER_SHADOW), text, &shadow_style);
        }
        nt_ui_label(ctx, NT_UI_DATA_LAYER(TUTORIAL_CALLOUT_LAYER_TEXT), text, &text_style);
    }
}

tutorial_callout_style_t tutorial_callout_default_style(bool compact, float max_width) {
    const float width = compact ? clampf(max_width - 28.0F, 180.0F, 332.0F) : 276.0F;
    return (tutorial_callout_style_t){
        .width = width,
        .height = compact ? 46.0F : 42.0F,
        .font_size = compact ? 18.0F : 17.0F,
        .background = {18.0F, 11.0F, 7.0F, 205.0F},
        .border = {172.0F, 124.0F, 68.0F, 170.0F},
        .text = {250.0F, 240.0F, 220.0F, 255.0F},
        .shadow = {8.0F, 5.0F, 3.0F, 142.0F},
        .padding_left = 16,
        .padding_right = 16,
        .padding_top = 8,
        .padding_bottom = 8,
        .corner_radius = 7.0F,
        .tail_width = 18.0F,
        .tail_height = 8.0F,
        .tail_offset_x = compact ? 64.0F : 42.0F,
        .tail_offset_y = -1.0F,
    };
}

void tutorial_callout_ui(nt_ui_context_t *ctx, const tutorial_callout_desc_t *desc) {
    if (!ctx || !desc || !desc->visible || !desc->text || desc->text[0] == '\0') {
        return;
    }

    const tutorial_callout_style_t *style = &desc->style;
    CLAY({.id = CLAY_IDI("tutorial_callout/root", desc->slot),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = clay_anchor(desc->element_anchor), .parent = clay_anchor(desc->parent_anchor)},
                       .offset = {desc->offset_x, desc->offset_y},
                       .zIndex = TUTORIAL_CALLOUT_Z_INDEX},
          .layout = {.sizing = {CLAY_SIZING_FIXED(style->width), CLAY_SIZING_FIXED(style->height)},
                     .padding = {.left = style->padding_left,
                                 .right = style->padding_right,
                                 .top = style->padding_top,
                                 .bottom = style->padding_bottom},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = style->background,
          .cornerRadius = CLAY_CORNER_RADIUS(style->corner_radius),
          .border = {.color = style->border, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(TUTORIAL_CALLOUT_LAYER_BG)}) {
        CLAY({.id = CLAY_IDI("tutorial_callout/tail", desc->slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_TOP, .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                           .offset = {style->tail_offset_x, style->tail_offset_y}},
              .layout = {.sizing = {CLAY_SIZING_FIXED(style->tail_width), CLAY_SIZING_FIXED(style->tail_height)}},
              .backgroundColor = style->background,
              .cornerRadius = CLAY_CORNER_RADIUS(3),
              .border = {.color = {style->border.r, style->border.g, style->border.b, 130.0F}, .width = {0, 1, 0, 1, 0}},
              .userData = NT_UI_CLAY_DATA(TUTORIAL_CALLOUT_LAYER_BG)}) {}
        callout_label(ctx, desc->slot, desc->text, style);
    }
}

void tutorial_finger_ui(nt_ui_context_t *ctx, const tutorial_finger_desc_t *desc) {
    if (!ctx || !desc || !desc->visible || desc->size <= 0.0F) {
        return;
    }
    ensure_finger_region();

    CLAY({.id = CLAY_IDI("tutorial_callout/finger", desc->slot),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                       .offset = {desc->offset_x, desc->offset_y},
                       .zIndex = TUTORIAL_CALLOUT_Z_INDEX + 1},
          .layout = {.sizing = {CLAY_SIZING_FIXED(desc->size), CLAY_SIZING_FIXED(desc->size)}}}) {
        nt_ui_image_style_t style = nt_ui_image_style_defaults();
        style.flip_bits = desc->flip_bits;
        nt_ui_image(ctx, NT_UI_DATA_LAYER(TUTORIAL_CALLOUT_LAYER_FINGER), &s_finger_region, &style, NULL);
    }
}
