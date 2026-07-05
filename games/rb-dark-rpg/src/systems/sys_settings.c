#include "systems/sys_settings.h"

#include "clay.h"
#include "game_audio.h"
#include "game_dialogue.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "scene/scene_interactions.h"
#include "ui/equipment_screen.h"
#include "ui/game_modal.h"
#include "ui/journal_screen.h"
#include "ui/location_screen.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_slider.h"
#include "ui/shop_screen.h"
#include "ui/theme.h"
#include "ui/world_map_screen.h"
#if FEATURE_GAME_STATE
#include "game_persistence.h"
#endif

#include <stdio.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
/* clang-format off */
EM_JS_DEPS(sys_settings_web, "$UTF8ToString")
EM_JS(void, sys_settings_web_open_url, (const char *url_ptr), {
    try {
        window.open(UTF8ToString(url_ptr), "_blank");
    } catch (e) {}
})
/* clang-format on */
#endif
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#endif

// Walker batches RECTs/IMAGEs first, then TEXT, within each Clay zIndex - so a
// lower layer draws behind: panel bg (BG) < widget art (IMG) < labels (TEXT).
#define LAYER_BG 0
#define LAYER_IMG 1
#define LAYER_TEXT 2

#define RESET_HOLD_SECONDS 1.5F
#define SETTINGS_MARGIN 18.0F
#define SETTINGS_MAX_W 460.0F
#define SETTINGS_MIN_W 304.0F
#define SETTINGS_PADDING_LANDSCAPE 8.0F
#define SETTINGS_PADDING_PORTRAIT 6.0F
#define SETTINGS_SLIDER_MAX_W 380.0F
#define SETTINGS_SLIDER_MIN_W 232.0F
#define SETTINGS_MODAL_ID 0x5E771001U

// VibeJam #1 attribution (jam rule: "Made for VibeJam #1" + link to the jam
// channel must be reachable from the game).
#define SYS_SETTINGS_JAM_CHANNEL_URL "https://t.me/VibeYura"
#define SYS_SETTINGS_AUTHOR_TG_URL "https://t.me/d954mas_make_games"
#define SYS_SETTINGS_URL_SCHEME_LEN 8 /* strlen("https://"), used to show the bare handle */

static bool s_open;
static int s_dismiss_guard_frames;
static bool s_cleanup_pending;
static float s_master = 0.8F, s_music = 0.45F, s_sfx = 0.9F;
static nt_resource_t s_settings_ui_atlas;
static nt_atlas_region_ref_t s_settings_gear_region;

void sys_settings_force_open(void) {
    if (!s_open) {
        s_dismiss_guard_frames = 2;
    }
    journal_screen_set_open(false);
    s_open = true;
}
bool sys_settings_is_open(void) { return s_open; }
float sys_settings_master(void) { return s_master; }
float sys_settings_music(void) { return s_music; }
float sys_settings_sfx(void) { return s_sfx; }

static void settings_request_state_cleanup(void) { s_cleanup_pending = true; }

static void settings_clear_transient_ui_state(nt_ui_context_t *ctx) {
    if (!ctx || !s_cleanup_pending) {
        return;
    }
    game_modal_clear_state(ctx, SETTINGS_MODAL_ID);
    s_cleanup_pending = false;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static bool settings_reset_game(World *w) {
    if (!w) {
        return false;
    }
#if FEATURE_GAME_STATE
    char error[256];
    if (!game_persistence_reset_autosave(error, (int)sizeof(error))) {
        return false;
    }
#endif
    w->first_scene = (FirstSceneState){0};
    game_dialogue_init(w);
    w->combat = (CombatFlowState){.last_audio_event_index = -1};
    w->player_x = 0.0F;
    w->player_z = 0.0F;
    w->player_yaw = 0.0F;
    scene_interactions_init_first_scene(w);
    equipment_screen_set_open(false);
    journal_screen_set_open(false);
    location_screen_set_open(false);
    world_map_screen_set_open(false);
    shop_screen_set_open(false);
    return true;
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
    nt_ui_slider_style_t slider_style = g_theme.slider;
    for (int i = 1; i < 4; ++i) {
        slider_style.states[i] = slider_style.states[NT_UI_SLIDER_IDLE];
    }
    slider_style.states[NT_UI_SLIDER_IDLE].track_tint = 0xFFFFFFFFU;
    slider_style.states[NT_UI_SLIDER_IDLE].fill_tint = 0xFFFFFFFFU;
    slider_style.states[NT_UI_SLIDER_IDLE].thumb_tint = 0xFFFFFFFFU;
    slider_style.states[NT_UI_SLIDER_HOVER].track_tint = 0xFFFFFFFFU;
    slider_style.states[NT_UI_SLIDER_HOVER].fill_tint = 0xFFFFFFFFU;
    slider_style.states[NT_UI_SLIDER_HOVER].thumb_tint = 0xFFFFFFFFU;
    slider_style.states[NT_UI_SLIDER_PRESSED].track_tint = 0xFFFFFFFFU;
    slider_style.states[NT_UI_SLIDER_PRESSED].fill_tint = 0xFFFFFFFFU;
    slider_style.states[NT_UI_SLIDER_PRESSED].thumb_tint = 0xFFFFFFFFU;
    slider_style.track_h = 18.0F;
    slider_style.thumb_w = 26.0F;
    slider_style.thumb_h = 26.0F;
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

// Opens an external URL from the running game: EM_ASM window.open() on web,
// ShellExecuteA on native Windows. No-op (link stays visible-only) elsewhere.
static void settings_open_url(const char *url) {
    if (!url) {
        return;
    }
#if defined(__EMSCRIPTEN__)
    sys_settings_web_open_url(url);
#elif defined(_WIN32)
    (void)ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#else
    (void)url;
#endif
}

// Full-width clickable row: label shows the bare handle (URL without the
// "https://" scheme), click opens the URL. Reuses the same button look as the
// reset/close action row below.
static void jam_link_row(nt_ui_context_t *ctx, const char *id, const char *prefix, const char *url) {
    char label_text[96];
    (void)snprintf(label_text, sizeof label_text, "%s%s", prefix, url + SYS_SETTINGS_URL_SCHEME_LEN);
    nt_ui_button_style_t link_style = game_modal_button_style(true);
    const nt_ui_label_style_t link_label = game_modal_label(15.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    const uint32_t link_id = nt_ui_id(id);
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(40)}}}) {
        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), link_id, &link_style,
                           &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                           true, NULL);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), label_text, &link_label);
        if (nt_ui_button_end(ctx)) {
            game_audio_play(GAME_AUDIO_CUE_SETTINGS);
            settings_open_url(url);
        }
    }
}

void sys_settings_ui(nt_ui_context_t *ctx, World *w) {
    settings_clear_transient_ui_state(ctx);
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
                    journal_screen_set_open(false);
                    s_open = true;
                } else {
                    s_open = false;
                    s_dismiss_guard_frames = 0;
                    settings_request_state_cleanup();
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
    const nt_ui_label_style_t danger_label = game_modal_label(16.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    nt_ui_button_style_t reset_button = game_modal_button_style(false);
    reset_button.idle.bg_tint = 0xFF5864B4U;
    reset_button.hover.bg_tint = 0xFF687CD5U;
    reset_button.pressed.bg_tint = 0xFF394288U;
    bool modal_open = s_open;
    nt_ui_modal_style_t modal_style = game_modal_style((nt_ui_layer_t)LAYER_BG, true);
    const bool ignore_close_request = s_dismiss_guard_frames > 0;
    if (!game_modal_visible(ctx, SETTINGS_MODAL_ID, &modal_style, &modal_open, ignore_close_request)) {
        s_open = modal_open;
        if (!s_open) {
            s_dismiss_guard_frames = 0;
            settings_request_state_cleanup();
            settings_clear_transient_ui_state(ctx);
        }
        return;
    }
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), game_modal_art(GAME_MODAL_ART_OUTER_FRAME), &panel_image,
                      &(Clay_ElementDeclaration){
                          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIT(0)},
                                     .padding = CLAY_PADDING_ALL(panel_padding),
                                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                     .childGap = 10,
                                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    CLAY({.id = CLAY_ID("settings/body_fill"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .padding = CLAY_PADDING_ALL(portrait_panel ? 6 : 8),
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

    // VibeJam #1 attribution: required jam notice + reachable links to the
    // jam channel and the author's Telegram (see SYS_SETTINGS_*_URL above).
    CLAY({.id = CLAY_ID("settings/jam_section"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 8}}) {
        const nt_ui_label_style_t jam_label = game_modal_label(13.5F, 205.0F, 184.0F, 144.0F, 255.0F);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Сделано для VibeJam #1 - тема \"Мир без тебя\"", &jam_label);
        jam_link_row(ctx, "settings/jam_channel", "Канал джема: ", SYS_SETTINGS_JAM_CHANNEL_URL);
        jam_link_row(ctx, "settings/jam_author", "Автор: ", SYS_SETTINGS_AUTHOR_TG_URL);
    }

    // Reset is the only footer action; the modal already has a header close button.
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 0,
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
                if (settings_reset_game(w)) {
                    modal_open = false;
                    s_open = false;
                    settings_request_state_cleanup();
                }
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
        settings_request_state_cleanup();
    }
    settings_clear_transient_ui_state(ctx);
}
