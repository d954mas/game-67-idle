#include "ui/game_modal.h"

#include "clay.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"

#include <stdint.h>
#include <string.h>

static nt_resource_t s_ui_atlas;
static nt_atlas_region_ref_t s_outer_frame_region;
static nt_atlas_region_ref_t s_body_panel_region;
static nt_atlas_region_ref_t s_header_plaque_region;
static nt_atlas_region_ref_t s_objective_panel_region;
static nt_atlas_region_ref_t s_answer_normal_region;
static nt_atlas_region_ref_t s_answer_primary_region;
static nt_atlas_region_ref_t s_reward_cell_region;
static nt_atlas_region_ref_t s_white_region;

static void ensure_regions(void) {
    if (s_ui_atlas.id != 0U) {
        return;
    }

    s_ui_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    s_outer_frame_region = nt_atlas_ref(s_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_OUTER_FRAME.value);
    s_body_panel_region = nt_atlas_ref(s_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_BODY_PANEL.value);
    s_header_plaque_region = nt_atlas_ref(s_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_HEADER_PLAQUE.value);
    s_objective_panel_region = nt_atlas_ref(s_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_OBJECTIVE_PANEL.value);
    s_answer_normal_region = nt_atlas_ref(s_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_ANSWER_NORMAL.value);
    s_answer_primary_region = nt_atlas_ref(s_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_ANSWER_PRIMARY.value);
    s_reward_cell_region = nt_atlas_ref(s_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_REWARD_CELL.value);
    s_white_region = nt_atlas_ref(s_ui_atlas, ASSET_ATLAS_REGION_UI__WHITE.value);
}

static Clay_ElementId clay_id_from_text(const char *id_text) {
    return Clay_GetElementId((Clay_String){
        .isStaticallyAllocated = false,
        .length = (int32_t)strlen(id_text),
        .chars = id_text,
    });
}

nt_atlas_region_ref_t *game_modal_art(game_modal_art_t art) {
    ensure_regions();
    switch (art) {
        case GAME_MODAL_ART_OUTER_FRAME:
            return &s_outer_frame_region;
        case GAME_MODAL_ART_BODY_PANEL:
            return &s_body_panel_region;
        case GAME_MODAL_ART_HEADER_PLAQUE:
            return &s_header_plaque_region;
        case GAME_MODAL_ART_OBJECTIVE_PANEL:
            return &s_objective_panel_region;
        case GAME_MODAL_ART_ANSWER_PRIMARY:
            return &s_answer_primary_region;
        case GAME_MODAL_ART_REWARD_CELL:
            return &s_reward_cell_region;
        case GAME_MODAL_ART_WHITE:
            return &s_white_region;
        case GAME_MODAL_ART_ANSWER_NORMAL:
        default:
            return &s_answer_normal_region;
    }
}

nt_ui_modal_style_t game_modal_style(nt_ui_layer_t layer, bool dismissible) {
    nt_ui_modal_style_t s = nt_ui_modal_style_defaults();
    s.backdrop_alpha = 0.72F;
    s.backdrop_color = 0xFF050302U;
    s.layer = layer;
    s.flags = dismissible ? (uint8_t)(NT_UI_MODAL_CLOSE_ON_BACKDROP | NT_UI_MODAL_LISTEN_ESC) : 0U;
    s.open = (nt_ui_modal_anim_t){.type = NT_UI_MODAL_ANIM_FADE};
    s.close = (nt_ui_modal_anim_t){.type = NT_UI_MODAL_ANIM_FADE};
    s.ease_speed = 18.0F;
    return s;
}

bool game_modal_visible(nt_ui_context_t *ctx, uint32_t id,
                        const nt_ui_modal_style_t *style, bool *open,
                        bool ignore_close_request) {
    const nt_ui_modal_result_t result = nt_ui_modal_begin(ctx, id, style, open ? *open : false);
    if (result.close_requested && !ignore_close_request && open) {
        *open = false;
    }
    if (!result.visible) {
        nt_ui_modal_end(ctx);
        return false;
    }
    return true;
}

void game_modal_clear_state(nt_ui_context_t *ctx, uint32_t id) {
    if (!ctx) {
        return;
    }
    nt_ui_modal_clear_state(ctx, id);
}

nt_ui_image_style_t game_modal_panel_image(bool portrait) {
    nt_ui_image_style_t s = nt_ui_image_style_defaults();
    s.slice9_scale = portrait ? 0.42F : 0.50F;
    return s;
}

nt_ui_image_style_t game_modal_body_image(bool portrait) {
    nt_ui_image_style_t s = nt_ui_image_style_defaults();
    s.slice9_scale = portrait ? 0.42F : 0.50F;
    return s;
}

nt_ui_image_style_t game_modal_header_image(bool portrait) {
    nt_ui_image_style_t s = nt_ui_image_style_defaults();
    s.slice9_scale = portrait ? 0.44F : 0.52F;
    return s;
}

nt_ui_image_style_t game_modal_small_panel_image(bool portrait) {
    nt_ui_image_style_t s = nt_ui_image_style_defaults();
    s.slice9_scale = portrait ? 0.34F : 0.40F;
    return s;
}

nt_ui_button_style_t game_modal_button_style(bool primary) {
    ensure_regions();
    nt_ui_button_style_t s = {0};
    s.idle.bg = primary ? s_answer_primary_region : s_answer_normal_region;
    s.hover.bg = s.idle.bg;
    s.pressed.bg = s.idle.bg;
    s.disabled.bg = s.idle.bg;
    s.idle.bg_tint = 0xFFFFFFFFU;
    s.hover.bg_tint = primary ? 0xFFFFE7B5U : 0xFFE9F3F8U;
    s.pressed.bg_tint = primary ? 0xFFD19A67U : 0xFFB5C8D2U;
    s.disabled.bg_tint = 0xFFFFFFFFU;
    s.idle.scale = 1.0F;
    s.hover.scale = 1.02F;
    s.pressed.scale = 0.98F;
    s.disabled.scale = 1.0F;
    s.idle.opacity = 1.0F;
    s.hover.opacity = 1.0F;
    s.pressed.opacity = 1.0F;
    s.disabled.opacity = 0.55F;
    s.transition_speed = 16.0F;
    s.hit_padding_lrtb[0] = 8;
    s.hit_padding_lrtb[1] = 8;
    s.hit_padding_lrtb[2] = 8;
    s.hit_padding_lrtb[3] = 8;
    s.slice9_scale = 0.48F;
    return s;
}

nt_ui_button_style_t game_modal_close_button_style(void) {
    nt_ui_button_style_t s = game_modal_button_style(false);
    s.idle.bg_tint = 0xFFE2C69CU;
    s.hover.bg_tint = 0xFFFFE3B4U;
    s.pressed.bg_tint = 0xFFC18C5AU;
    s.disabled.bg_tint = 0xFFE2C69CU;
    s.slice9_scale = 0.34F;
    return s;
}

nt_ui_scroll_style_t game_modal_scroll_style(void) {
    ensure_regions();
    nt_ui_scroll_style_t s = nt_ui_scroll_style_defaults();
    s.bar_visibility = NT_UI_SCROLLBAR_AUTO_HIDE;
    s.bar_thickness = 8.0F;
    s.bar_thumb_min_px = 34.0F;
    s.track_ref = s_white_region;
    s.thumb_ref = s_white_region;
    s.track_tint = 0xA0243456U;
    s.thumb_tint = 0xF044BCECU;
    return s;
}

nt_ui_label_style_t game_modal_label(float size, float r, float g, float b, float a) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = size, .color = {r, g, b, a}};
}

bool game_modal_close_button(nt_ui_context_t *ctx, nt_ui_layer_t image_layer,
                             nt_ui_layer_t text_layer, const char *id_text,
                             bool portrait) {
    const Clay_ElementId wrapper_id = clay_id_from_text(id_text);
    const uint32_t button_id = nt_ui_child_id(wrapper_id.id, "button");
    const float size = portrait ? 34.0F : 36.0F;
    nt_ui_button_style_t button = game_modal_close_button_style();
    const nt_ui_label_style_t label = game_modal_label(portrait ? 18.0F : 19.0F, 56.0F, 28.0F, 18.0F, 255.0F);

    bool clicked = false;
    CLAY({.id = wrapper_id,
          .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {43.0F, 25.0F, 17.0F, 230.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .userData = NT_UI_CLAY_DATA(image_layer)}) {
        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(image_layer), button_id, &button,
                           &(Clay_ElementDeclaration){
                               .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                          .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                           true, NULL);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(text_layer), "x", &label);
        clicked = nt_ui_button_end(ctx);
    }
    return clicked;
}
