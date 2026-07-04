#include "systems/sys_settings.h"

#include "clay.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_slider.h"
#include "ui/theme.h"

#include <stdio.h>

// Walker batches RECTs/IMAGEs first, then TEXT, within each Clay zIndex — so a
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

static bool s_open;
static float s_master = 0.8F, s_music = 0.7F, s_sfx = 0.9F;

void sys_settings_force_open(void) { s_open = true; }
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

// Label + slider stacked; the slider mutates *value in place (engine owns the drag).
static void volume_row(nt_ui_context_t *ctx, const char *name, const char *id, float *value, float slider_w) {
    char buf[48];
    (void)snprintf(buf, sizeof buf, "%s   %d%%", name, (int)(*value * 100.0F + 0.5F));
    nt_ui_slider_style_t slider_style = g_theme.slider;
    slider_style.track_w = slider_w;
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 4}}) {
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), buf, &g_theme.label);
        (void)nt_ui_slider_float(ctx, NT_UI_DATA_LAYER(LAYER_IMG), LAYER_TEXT, nt_ui_id(id), NULL, value, 0.0F, 1.0F, 0.0F, &slider_style,
                                 &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(slider_w), CLAY_SIZING_FIXED(30)}}}, true);
    }
}

void sys_settings_ui(nt_ui_context_t *ctx, World *w) {
    // Floating gear button: anchor to the visible root, independent from other
    // full-screen UI siblings and portrait logical-width expansion.
    CLAY({.id = CLAY_ID("settings_root"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {-16.0F, 16.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(132), CLAY_SIZING_FIXED(48)}}}) {
        CLAY({.id = CLAY_ID("settings/gear"), .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
            const uint32_t gear_id = nt_ui_id("settings/gear/button");
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), gear_id, &g_theme.button,
                               &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                               true, NULL);
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Опции", &g_theme.button_label);
            if (nt_ui_button_end(ctx)) {
                s_open = !s_open;
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
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &g_theme.panel_region, &g_theme.panel_img,
                      &(Clay_ElementDeclaration){
                          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT, .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
                          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIT(0)},
                                     .padding = CLAY_PADDING_ALL(panel_padding),
                                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                     .childGap = 16,
                                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "SETTINGS", &g_theme.title);

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
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), reset_id, &g_theme.button_danger,
                               &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                               true, &hold);
            const nt_ui_events_t re = nt_ui_query_events(ctx, reset_id);
            char rlabel[48];
            if (re.hold_progress > 0.0F && re.hold_progress < 1.0F) {
                (void)snprintf(rlabel, sizeof rlabel, "Hold to reset  %d%%", (int)(re.hold_progress * 100.0F));
            } else {
                (void)snprintf(rlabel, sizeof rlabel, "Hold to reset");
            }
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), rlabel, &g_theme.label);
            (void)nt_ui_button_end(ctx);
            if (re.long_pressed) {
                w->player_x = 0.0F;
                w->player_z = 0.0F;
                w->player_yaw = 0.0F;
                s_open = false;
            }
        }

        CLAY({.id = CLAY_ID("settings/close"),
              .layout = {.sizing = {portrait_panel ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(48)}}}) {
            const uint32_t close_id = nt_ui_id("settings/close/button");
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), close_id, &g_theme.button,
                               &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                               true, NULL);
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Close", &g_theme.button_label);
            if (nt_ui_button_end(ctx)) {
                s_open = false;
            }
        }
    }
    nt_ui_panel_end(ctx);
}
