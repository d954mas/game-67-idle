#include "ui/theme.h"

#include "generated/game_assets.h"

ui_theme_t g_theme;

/* 0xAABBGGRR packed tints. */
void theme_init(nt_resource_t atlas) {
    g_theme.panel_region = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_PANEL.value);
    g_theme.panel_dark_region = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_PANEL_BLUE.value);
    g_theme.panel_brown_region = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_PANEL_BROWN.value);
    g_theme.checkmark_region = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_CHECKMARK.value);
    g_theme.panel_img = nt_ui_image_style_defaults();

    const nt_atlas_region_ref_t btn = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_BUTTON.value);
    const nt_atlas_region_ref_t btn_ok = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_BUTTON_SUCCESS.value);
    const nt_atlas_region_ref_t btn_bad = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_BUTTON_DANGER.value);
    const nt_atlas_region_ref_t btn_sel = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_BUTTON_SELECTED.value);

    nt_ui_button_style_t base = {
        .idle = {.bg = btn, .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 1.0F},
        .hover = {.bg = btn, .bg_tint = 0xFFFFFFFFU, .scale = 1.04F, .opacity = 1.0F},
        .pressed = {.bg = btn, .bg_tint = 0xFFEEEEEEU, .scale = 0.97F, .offset_y = 1.0F, .opacity = 1.0F},
        .disabled = {.bg = btn, .bg_tint = 0xFFFFFFFFU, .scale = 1.0F, .opacity = 0.4F},
        .transition_speed = 16.0F,
        .hit_padding_lrtb = {6, 6, 6, 6},
        .slice9_scale = 1.0F,
    };
    g_theme.button = base;

    nt_ui_button_style_t ok = base;
    ok.idle.bg = btn_ok;
    ok.hover.bg = btn_ok;
    ok.pressed.bg = btn_ok;
    ok.disabled.bg = btn_ok;
    g_theme.button_success = ok;

    nt_ui_button_style_t danger = base;
    danger.idle.bg = btn_bad;
    danger.hover.bg = btn_bad;
    danger.pressed.bg = btn_bad;
    danger.disabled.bg = btn_bad;
    g_theme.button_danger = danger;

    nt_ui_button_style_t sel = base;
    sel.idle.bg = btn_sel;
    sel.hover.bg = btn_sel;
    sel.pressed.bg = btn_sel;
    sel.disabled.bg = btn_sel;
    g_theme.button_selected = sel;

    nt_ui_slider_style_t s = nt_ui_slider_style_defaults();
    s.track_w = 340.0F;
    s.track_h = 16.0F;
    s.thumb_w = 28.0F;
    s.thumb_h = 28.0F;
    s.value_speed = 18.0F;
    s.hit_padding_lrtb[2] = 12;
    s.hit_padding_lrtb[3] = 12;
    s.states[NT_UI_SLIDER_IDLE].track = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_TRACK.value);
    s.states[NT_UI_SLIDER_IDLE].fill = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_FILL.value);
    s.states[NT_UI_SLIDER_IDLE].thumb = nt_atlas_ref(atlas, ASSET_ATLAS_REGION_UI_SLIDER_THUMB.value);
    g_theme.slider = s;

    /* Ink on cream / light on rose CTA */
    g_theme.title = (nt_ui_label_style_t){.font_id = 0, .font_size = 26.0F, .color = {72.0F, 32.0F, 52.0F, 255.0F}};
    g_theme.label = (nt_ui_label_style_t){.font_id = 0, .font_size = 17.0F, .color = {88.0F, 42.0F, 62.0F, 255.0F}};
    g_theme.button_label = (nt_ui_label_style_t){.font_id = 0, .font_size = 16.0F, .color = {72.0F, 32.0F, 52.0F, 255.0F}};
    g_theme.button_label_light =
        (nt_ui_label_style_t){.font_id = 0, .font_size = 16.0F, .color = {255.0F, 248.0F, 252.0F, 255.0F}};
    g_theme.hint = (nt_ui_label_style_t){.font_id = 0, .font_size = 14.0F, .color = {150.0F, 100.0F, 120.0F, 255.0F}};
}
