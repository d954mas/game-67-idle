#include "features/settings/settings.h"

#include "game_save.h" /* Р11 hold-to-reset: game_save_request_new_game (L0 shell) */

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

static bool s_open;
static float s_master = 0.8F, s_music = 0.7F, s_sfx = 0.9F;

void settings_open(void)  { s_open = true; }
void settings_close(void) { s_open = false; }
bool settings_is_open(void) { return s_open; }

// Label + slider stacked; the slider mutates *value in place (engine owns the drag).
// `commit` (nullable) persists a changed value through the settings feature-API,
// which clamps and marks the save dirty.
static void volume_row(nt_ui_context_t *ctx, const char *name, const char *id, float *value,
                       void (*commit)(float)) {
    char buf[48];
    (void)snprintf(buf, sizeof buf, "%s   %d%%", name, (int)(*value * 100.0F + 0.5F));
    const float before = *value;
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}, .layoutDirection = CLAY_TOP_TO_BOTTOM, .childGap = 4}}) {
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), buf, &g_theme.label);
        (void)nt_ui_slider_float(ctx, NT_UI_DATA_LAYER(LAYER_IMG), LAYER_TEXT, nt_ui_id(id), NULL, value, 0.0F, 1.0F, 0.0F, &g_theme.slider,
                                 &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(380), CLAY_SIZING_FIXED(30)}}}, true);
    }
    if (*value != before && commit) {
        commit(*value); // persist (clamps + marks dirty inside the setter)
    }
}

void settings_draw_ui(nt_ui_context_t *ctx, World *w) {
    // Root: full screen; gear button parked top-right.
    CLAY({.id = CLAY_ID("settings_root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP}}}) {
        CLAY({.id = CLAY_ID("settings/gear"), .layout = {.sizing = {CLAY_SIZING_FIXED(150), CLAY_SIZING_FIXED(48)}}}) {
            const uint32_t gear_id = nt_ui_id("settings/gear/button");
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), gear_id, &g_theme.button,
                               &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                               true, NULL);
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Settings", &g_theme.button_label);
            if (nt_ui_button_end(ctx)) {
                s_open = !s_open;
            }
        }
    }

    if (!s_open) {
        return;
    }

    // Centered floating panel with slice9 art background.
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &g_theme.panel_region, &g_theme.panel_img,
                      &(Clay_ElementDeclaration){
                          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT, .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
                          .layout = {.sizing = {CLAY_SIZING_FIXED(460), CLAY_SIZING_FIT(0)},
                                     .padding = CLAY_PADDING_ALL(28),
                                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                     .childGap = 16,
                                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "SETTINGS", &g_theme.title);

    // Authority is the persisted settings state: reseed the slider backing-floats
    // from the feature each frame the panel is open; the commit callback is the
    // single writer back into settings_state.
    s_master = settings_master();
    s_music = settings_music();
    s_sfx = settings_sfx();
    volume_row(ctx, "Master", "settings/master", &s_master, settings_set_master);
    volume_row(ctx, "Music", "settings/music", &s_music, settings_set_music);
    volume_row(ctx, "SFX", "settings/sfx", &s_sfx, settings_set_sfx);

    // Action row: hold-to-reset (long press) + close.
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}, .layoutDirection = CLAY_LEFT_TO_RIGHT, .childGap = 12, .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("settings/reset"), .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(48)}}}) {
            const uint32_t reset_id = nt_ui_id("settings/reset/button");
            const nt_ui_events_cfg_t hold = {.long_press_secs = RESET_HOLD_SECONDS, .double_click = false};
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), reset_id, &g_theme.button_danger,
                               &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                               true, &hold);
            const nt_ui_events_t re = nt_ui_query_events(ctx, reset_id);
            char rlabel[48];
            if (re.hold_progress > 0.0F && re.hold_progress < 1.0F) {
                (void)snprintf(rlabel, sizeof rlabel, "Hold to reset progress  %d%%", (int)(re.hold_progress * 100.0F));
            } else {
                (void)snprintf(rlabel, sizeof rlabel, "Hold to reset progress");
            }
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), rlabel, &g_theme.label);
            (void)nt_ui_button_end(ctx);
            if (re.long_pressed) {
                // New game IN SESSION (Р11): player position is game-composition state,
                // not a save fragment -- safe to reset synchronously right here (plain
                // field writes, no event emission / file I/O). Save-fragment reset (gold,
                // items, progression tracks) is a REQUEST, applied by the shell at the
                // start of the next frame's update (game_save.c: game_save_apply_pending_
                // new_game) -- see that function's comment for why it is deferred.
                // "settings" is skipped: volumes are not this button's business.
                w->player_x = 0.0F;
                w->player_z = 0.0F;
                w->player_yaw = 0.0F;
                game_save_request_new_game("settings");
                s_open = false;
            }
        }

        CLAY({.id = CLAY_ID("settings/close"), .layout = {.sizing = {CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(48)}}}) {
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
