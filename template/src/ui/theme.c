#include "ui/theme.h"

#include "generated/game_assets.h"

ui_theme_t g_theme;

// 0xAABBGGRR packed tints (engine convention). Clay_Color is {r,g,b,a} 0..255.
void theme_init(nt_resource_t atlas) {
    g_theme.panel_region = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_PANEL.value);
    g_theme.panel_img = nt_ui_image_style_defaults();

    const nt_atlas_region_ref_t btn = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_BUTTON.value);
    nt_ui_button_style_t base = {
        .idle = {.bg = btn, .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 1.0F},
        .hover = {.bg = btn, .bg_tint = 0xFFFFFFFFU, .scale = 1.04F, .opacity = 1.0F},
        .pressed = {.bg = btn, .bg_tint = 0xFFCFCFCFU, .scale = 0.96F, .offset_y = 2.0F, .opacity = 1.0F},
        .disabled = {.bg = btn, .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 0.4F},
        .transition_speed = 12.0F,
        .hit_padding_lrtb = {8, 8, 8, 8},
        .slice9_scale = 1.0F,
    };
    g_theme.button = base;

    // Reset button: warm/red tint so it reads as a destructive action.
    nt_ui_button_style_t danger = base;
    danger.idle.bg_tint = 0xFF5260E6U;
    danger.hover.bg_tint = 0xFF6173F2U;
    danger.pressed.bg_tint = 0xFF4150C0U;
    danger.disabled.bg_tint = 0xFF5260E6U;
    g_theme.button_danger = danger;

    nt_ui_slider_style_t s = nt_ui_slider_style_defaults();
    s.track_w = 380.0F;
    s.track_h = 18.0F;
    s.thumb_w = 26.0F;
    s.thumb_h = 26.0F;
    s.value_speed = 18.0F;
    s.hit_padding_lrtb[2] = 12;
    s.hit_padding_lrtb[3] = 12;
    s.states[NT_UI_SLIDER_IDLE].track = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_TRACK.value);
    s.states[NT_UI_SLIDER_IDLE].fill = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_FILL.value);
    s.states[NT_UI_SLIDER_IDLE].thumb = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_THUMB.value);
    g_theme.slider = s;

    g_theme.title = (nt_ui_label_style_t){.font_id = 0, .font_size = 30.0F, .color = {255.0F, 255.0F, 255.0F, 255.0F}};
    g_theme.label = (nt_ui_label_style_t){.font_id = 0, .font_size = 20.0F, .color = {236.0F, 240.0F, 248.0F, 255.0F}};
    g_theme.button_label = (nt_ui_label_style_t){.font_id = 0, .font_size = 20.0F, .color = {22.0F, 34.0F, 64.0F, 255.0F}};
    g_theme.hint = (nt_ui_label_style_t){.font_id = 0, .font_size = 16.0F, .color = {206.0F, 212.0F, 224.0F, 255.0F}};
}
