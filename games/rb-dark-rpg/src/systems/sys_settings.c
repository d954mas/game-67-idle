#include "systems/sys_settings.h"

#include "clay.h"
#include "game_audio.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/game_modal.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_slider.h"
#include "ui/theme.h"

#include <stdio.h>

// Walker batches RECTs/IMAGEs first, then TEXT, within each Clay zIndex - so a
// lower layer draws behind: panel bg (BG) < widget art (IMG) < labels (TEXT).
#define LAYER_BG 0
#define LAYER_IMG 1
#define LAYER_TEXT 2

#define RESET_HOLD_SECONDS 1.5F
#define SETTINGS_MARGIN 18.0F
#define SETTINGS_MAX_W 460.0F
#define SETTINGS_MIN_W 304.0F
#define SETTINGS_PADDING_LANDSCAPE 28.0F
#define SETTINGS_PADDING_PORTRAIT 18.0F
#define SETTINGS_SLIDER_MAX_W 380.0F
#define SETTINGS_SLIDER_MIN_W 232.0F
#define SETTINGS_MODAL_ID 0x5E771001U

static bool s_open;
static int s_dismiss_guard_frames;
static float s_master = 0.8F, s_music = 0.45F, s_sfx = 0.9F;
static nt_resource_t s_settings_ui_atlas;
static nt_atlas_region_ref_t s_settings_gear_region;

void sys_settings_force_open(void) {
    if (!s_open) {
        s_dismiss_guard_frames = 2;
    }
    s_open = true;
}
bool sys_settings_is_open(void) { return s_open; }
float sys_settings_master(void) { return s_master; }
float sys_settings_music(void) { return s_music; }
float sys_settings_sfx(void) { return s_sfx; }

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static float settings_panel_width(nt_ui_context_t *ctx) {
    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    (void)layout_h;
    const float available = layout_w > (SETTINGS_MARGIN * 2.0F) ? layout_w - (SETTINGS_MARGIN * 2.0F) : SETTINGS_MIN_W;
    return clampf(available, SETTINGS_MIN_W, SETTINGS_MAX_W);
}

static void ensure_settings_gear_region(void) {
    if (s_settings_ui_atlas.id != 0U) {
        return;
    }

    s_settings_ui_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    s_settings_gear_region = nt_atlas_ref(s_settings_ui_atlas, ASSET_ATLAS_REGION_UI_TOP_HUD_SETTINGS_BUTTON.value);
}

static nt_ui_button_style_t settings_gear_button_style(void) {
    nt_ui_button_style_t style = {0};
    style.idle.bg = s_settings_gear_region;
    style.idle.bg_tint = 0xFFFFFFFFU;
    style.idle.scale = 1.0F;
    style.idle.opacity = 1.0F;
    style.hover.bg_tint = 0xFFFFFFFFU;
    style.hover.scale = 1.05F;
    style.hover.opacity = 1.0F;
    style.pressed.bg_tint = 0xFFE3E3E3U;
    style.pressed.scale = 0.96F;
    style.pressed.opacity = 1.0F;
    style.disabled.bg_tint = 0xFF808080U;
    style.disabled.scale = 1.0F;
    style.disabled.opacity = 0.55F;
    style.transition_speed = 14.0F;
    style.hit_padding_lrtb[0] = 6;
    style.hit_padding_lrtb[1] = 6;
    style.hit_padding_lrtb[2] = 6;
    style.hit_padding_lrtb[3] = 6;
    style.slice9_scale = 1.0F;
    return style;
}

// Label + slider stacked; the slider mutates *value in place (engine owns the drag).
static void volume_row(nt_ui_context_t *ctx, const char *name, const char *id, float *value, float slider_w) {
    char buf[48];
    (void)snprintf(buf, sizeof buf, "%s   %d%%", name, (int)(*value * 100.0F + 0.5F));
    nt_ui_slider_style_t slider_style = nt_ui_slider_style_defaults();
    nt_atlas_region_ref_t *white = game_modal_art(GAME_MODAL_ART_WHITE);
    if (white) {
        slider_style.states[NT_UI_SLIDER_IDLE].track = *white;
        slider_style.states[NT_UI_SLIDER_IDLE].fill = *white;
        slider_style.states[NT_UI_SLIDER_IDLE].thumb = *white;
    }
    slider_style.states[NT_UI_SLIDER_IDLE].track_tint = 0xFF0F1722U;
    slider_style.states[NT_UI_SLIDER_IDLE].fill_tint = 0xFF3879B7U;
    slider_style.states[NT_UI_SLIDER_IDLE].thumb_tint = 0xFF94C9E8U;
    slider_style.track_h = 12.0F;
    slider_style.thumb_w = 18.0F;
    slider_style.thumb_h = 24.0F;
    slider_style.track_w = slider_w;
    slider_style.state_speed = 16.0F;
    slider_style.value_speed = 18.0F;
    const nt_ui_label_style_t label_style = game_modal_label(16.0F, 246.0F, 222.0F, 176.0F, 255.0F);
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 4}}) {
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), buf, &label_style);
        (void)nt_ui_slider_float(ctx, NT_UI_DATA_LAYER(LAYER_IMG), LAYER_TEXT, nt_ui_id(id), NULL, value, 0.0F, 1.0F, 0.0F, &slider_style,
                                 &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(slider_w), CLAY_SIZING_FIXED(30)}}}, true);
    }
}

void sys_settings_ui(nt_ui_context_t *ctx, World *w) {
    ensure_settings_gear_region();

    // Floating gear button: anchor to the visible root, independent from other
    // full-screen UI siblings and portrait logical-width expansion.
    CLAY({.id = CLAY_ID("settings_root"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {-16.0F, 16.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(48), CLAY_SIZING_FIXED(48)}}}) {
        CLAY({.id = CLAY_ID("settings/gear"), .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
            const uint32_t gear_id = nt_ui_id("settings/gear/button");
            nt_ui_button_style_t gear_style = settings_gear_button_style();
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), gear_id, &gear_style,
                               &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                               true, NULL);
            if (nt_ui_button_end(ctx)) {
                game_audio_play(GAME_AUDIO_CUE_SETTINGS);
                if (!s_open) {
                    s_dismiss_guard_frames = 2;
                    s_open = true;
                } else {
                    s_open = false;
                    s_dismiss_guard_frames = 0;
                }
            }
        }
    }

    if (!s_open) {
        return;
    }

    // Centered floating panel with slice9 art background.
    const float panel_w = settings_panel_width(ctx);
    const bool portrait_panel = panel_w < SETTINGS_MAX_W;
    const float panel_padding = portrait_panel ? SETTINGS_PADDING_PORTRAIT : SETTINGS_PADDING_LANDSCAPE;
    const float content_w = panel_w - (panel_padding * 2.0F);
    const float slider_w = clampf(content_w, SETTINGS_SLIDER_MIN_W, SETTINGS_SLIDER_MAX_W);
    nt_ui_image_style_t panel_image = game_modal_panel_image(portrait_panel);
    const nt_ui_label_style_t title_style = game_modal_label(24.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    const nt_ui_label_style_t button_label = game_modal_label(16.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    const nt_ui_label_style_t danger_label = game_modal_label(16.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    nt_ui_button_style_t reset_button = game_modal_button_style(false);
    reset_button.idle.bg_tint = 0xFF5864B4U;
    reset_button.hover.bg_tint = 0xFF687CD5U;
    reset_button.pressed.bg_tint = 0xFF394288U;
    nt_ui_button_style_t close_button = game_modal_button_style(true);
    bool modal_open = s_open;
    nt_ui_modal_style_t modal_style = game_modal_style((nt_ui_layer_t)LAYER_BG, true);
    const bool ignore_close_request = s_dismiss_guard_frames > 0;
    if (!game_modal_visible(ctx, SETTINGS_MODAL_ID, &modal_style, &modal_open, ignore_close_request)) {
        s_open = modal_open;
        if (!s_open) {
            s_dismiss_guard_frames = 0;
        }
        return;
    }
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), game_modal_art(GAME_MODAL_ART_OUTER_FRAME), &panel_image,
                      &(Clay_ElementDeclaration){
                          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIT(0)},
                                     .padding = CLAY_PADDING_ALL(panel_padding),
                                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                     .childGap = 16,
                                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    CLAY({.id = CLAY_ID("settings/body_fill"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .padding = CLAY_PADDING_ALL(portrait_panel ? 8 : 10),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 14,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {18.0F, 12.0F, 9.0F, 255.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
    CLAY({.id = CLAY_ID("settings/header"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 12,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("settings/header_title"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Настройки", &title_style);
        }
        if (game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_IMG, (nt_ui_layer_t)LAYER_TEXT,
                                    "settings/close_x", portrait_panel)) {
            game_audio_play(GAME_AUDIO_CUE_SETTINGS);
            modal_open = false;
        }
    }

    volume_row(ctx, "Master", "settings/master", &s_master, slider_w);
    volume_row(ctx, "Music", "settings/music", &s_music, slider_w);
    volume_row(ctx, "SFX", "settings/sfx", &s_sfx, slider_w);

    // Action row: hold-to-reset (long press) + close.
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = portrait_panel ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                     .childGap = 12,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("settings/reset"), .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(48)}}}) {
            const uint32_t reset_id = nt_ui_id("settings/reset/button");
            const nt_ui_events_cfg_t hold = {.long_press_secs = RESET_HOLD_SECONDS, .double_click = false};
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), reset_id, &reset_button,
                               &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                               true, &hold);
            const nt_ui_events_t re = nt_ui_query_events(ctx, reset_id);
            char rlabel[48];
            if (re.hold_progress > 0.0F && re.hold_progress < 1.0F) {
                (void)snprintf(rlabel, sizeof rlabel, "Reset  %d%%", (int)(re.hold_progress * 100.0F));
            } else {
                (void)snprintf(rlabel, sizeof rlabel, "Reset");
            }
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), rlabel, &danger_label);
            (void)nt_ui_button_end(ctx);
            if (re.long_pressed) {
                game_audio_play(GAME_AUDIO_CUE_SETTINGS);
                w->player_x = 0.0F;
                w->player_z = 0.0F;
                w->player_yaw = 0.0F;
                modal_open = false;
                s_open = false;
            }
        }

        CLAY({.id = CLAY_ID("settings/close"),
              .layout = {.sizing = {portrait_panel ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(48)}}}) {
            const uint32_t close_id = nt_ui_id("settings/close/button");
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), close_id, &close_button,
                               &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                               true, NULL);
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Закрыть", &button_label);
            if (nt_ui_button_end(ctx)) {
                game_audio_play(GAME_AUDIO_CUE_SETTINGS);
                modal_open = false;
            }
        }
    }
    }
    nt_ui_panel_end(ctx);
    nt_ui_modal_end(ctx);
    if (s_dismiss_guard_frames > 0) {
        --s_dismiss_guard_frames;
    }
    if (!modal_open) {
        s_open = false;
        s_dismiss_guard_frames = 0;
    }
}
