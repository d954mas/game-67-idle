#include "features/settings/settings.h"
#include "game_scenes.h"
#include "settings_state.h" /* generated: SettingsStateLanguage */

#include "game_save.h" /* Р11 hold-to-reset: game_save_request_new_game (L0 shell) */

#include "clay.h"
#include "ui/nt_ui_slider.h"
#include "ui/theme.h"
#include "ui/loc_widgets.h"

#include "loc_strings.gen.h"

#define RESET_HOLD_SECONDS 1.5F

static float s_master = 0.8F, s_music = 0.7F, s_sfx = 0.9F;

void settings_open(void)  { (void)game_scenes_show_settings(); }
void settings_close(void) { (void)game_scenes_close_settings(); }
bool settings_is_open(void) {
    return game_scenes_is_presented(GAME_SCENE_SETTINGS);
}

// Label + slider stacked; the slider mutates *value in place (engine owns the drag).
// `commit` (nullable) persists a changed value through the settings feature-API,
// which clamps and marks the save dirty.
// `name` is the row's own key: nesting one localized string inside another is a
// `str` argument fed by loc_by_key, never a concatenation.
static void volume_row(nt_ui_context_t *ctx, const ui_metrics_t *m, LocKey0 name, const char *id,
                       float *value, void (*commit)(float), bool interactive) {
    const LocStr row = loc_settings_volume_row(loc_by_key(name), (int64_t)(*value * 100.0F + 0.5F));
    const float before = *value;
    nt_ui_slider_style_t slider = ui_kit_slider_style(m);
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = (uint16_t)(m->gap * 0.4F)}}) {
        loc_kit_label(ctx, row, &g_ui_theme.label);
        // The slider's own label is NULL here, so no text reaches the engine
        // through an unwrapped entry point. The row is a full touch target: the
        // track is thin, and the thumb alone is not something a thumb can find.
        (void)nt_ui_slider_float(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), UI_LAYER_TEXT, nt_ui_id(id), NULL, value,
                                 0.0F, 1.0F, 0.0F, &slider,
                                 &(Clay_ElementDeclaration){
                                     .layout = {.sizing = {CLAY_SIZING_GROW(0), ui_kit_hit_height(m)}}},
                                 interactive);
    }
    if (*value != before && commit) {
        commit(*value); // persist (clamps + marks dirty inside the setter)
    }
}

/* Endonyms indexed by SettingsStateLanguage: a language names itself the same
   way whatever the UI is set to, so the picker never renames what it offers. */
static const LocKey0 LANGUAGE_NAMES[] = {LOC0_SETTINGS_LANG_EN, LOC0_SETTINGS_LANG_RU};
_Static_assert((int)(sizeof LANGUAGE_NAMES / sizeof LANGUAGE_NAMES[0]) == SETTINGS_STATE_LANGUAGE_COUNT,
               "a language in state/settings.schema.json has no endonym key");

// One button cycling the languages: label + the CURRENT language in its own
// name. The switch is immediate -- every accessor reads the active language on
// the next call, so the next frame is already translated.
static void language_row(nt_ui_context_t *ctx, const ui_metrics_t *m, bool interactive) {
    const int current = settings_language();
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = (uint16_t)m->gap,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        loc_kit_label(ctx, loc_settings_language(), &g_ui_theme.label);
        CLAY({.id = CLAY_ID("settings/language"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), ui_kit_hit_height(m)}}}) {
            ui_kit_button_begin(ctx, nt_ui_id("settings/language/button"), &g_ui_theme.button, interactive, NULL);
            loc_kit_label(ctx, loc_by_key(LANGUAGE_NAMES[current]), &g_ui_theme.button_label);
            if (ui_kit_button_end(ctx) && interactive) {
                settings_set_language((current + 1) % SETTINGS_STATE_LANGUAGE_COUNT);
            }
        }
    }
}

void settings_draw_launcher(nt_ui_context_t *ctx, bool interactive) {
    const ui_metrics_t m = ui_metrics();
    // Root: full screen; gear button parked top-right, clear of the device's
    // own safe area.
    CLAY({.id = CLAY_ID("settings_root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = (uint16_t)(m.margin + m.safe_l),
                                 .right = (uint16_t)(m.margin + m.safe_r),
                                 .top = (uint16_t)(m.margin + m.safe_t),
                                 .bottom = (uint16_t)(m.margin + m.safe_b)},
                     .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP}}}) {
        CLAY({.id = CLAY_ID("settings/gear"),
              .layout = {.sizing = {CLAY_SIZING_FIT(.min = m.hit * 2.4F), ui_kit_hit_height(&m)}}}) {
            ui_kit_button_begin(ctx, nt_ui_id("settings/gear/button"), &g_ui_theme.button, interactive, NULL);
            loc_kit_label(ctx, loc_settings_open(), &g_ui_theme.button_label);
            if (ui_kit_button_end(ctx) && interactive) {
                settings_open();
            }
        }
    }
}

void settings_draw_panel(nt_ui_context_t *ctx, World *w, bool interactive) {
    const ui_metrics_t m = ui_metrics();
    // Centred plate over a scrim: the dim is what tells the player the world
    // behind is not taking input right now.
    ui_kit_scrim(ctx);
    ui_kit_panel_begin(ctx, &(Clay_ElementDeclaration){
                                .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                                             .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                                              .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
                                .layout = {.sizing = {CLAY_SIZING_FIXED(m.panel_w), CLAY_SIZING_FIT(0)},
                                           .padding = CLAY_PADDING_ALL((uint16_t)m.pad),
                                           .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                           .childGap = (uint16_t)m.gap,
                                           .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    loc_kit_label(ctx, loc_settings_title(), &g_ui_theme.title);

    // Authority is the persisted settings state: reseed the slider backing-floats
    // from the feature each frame the panel is open; the commit callback is the
    // single writer back into settings_state.
    s_master = settings_master();
    s_music = settings_music();
    s_sfx = settings_sfx();
    volume_row(ctx, &m, LOC0_SETTINGS_MASTER, "settings/master", &s_master, settings_set_master, interactive);
    volume_row(ctx, &m, LOC0_SETTINGS_MUSIC, "settings/music", &s_music, settings_set_music, interactive);
    volume_row(ctx, &m, LOC0_SETTINGS_SFX, "settings/sfx", &s_sfx, settings_set_sfx, interactive);
    language_row(ctx, &m, interactive);

    // Action row: hold-to-reset (long press) + close.
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = (uint16_t)m.gap,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("settings/reset"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), ui_kit_hit_height(&m)}}}) {
            const uint32_t reset_id = nt_ui_id("settings/reset/button");
            const nt_ui_events_cfg_t hold = {.long_press_secs = RESET_HOLD_SECONDS, .double_click = false};
            ui_kit_button_begin(ctx, reset_id, &g_ui_theme.button_danger, interactive, &hold);
            const nt_ui_events_t re = nt_ui_query_events(ctx, reset_id);
            const bool holding = re.hold_progress > 0.0F && re.hold_progress < 1.0F;
            const LocStr rlabel = holding ? loc_settings_reset_holding((int64_t)(re.hold_progress * 100.0F))
                                          : loc_settings_reset();
            loc_kit_label(ctx, rlabel, &g_ui_theme.button_label_action);
            (void)ui_kit_button_end(ctx);
            if (interactive && re.long_pressed) {
                // New Game is one composition-level transition. The UI records
                // intent only; main applies persistent and transient resets together
                // at the nearest safe frame boundary.
                // "settings" is skipped: volumes are not this button's business.
                game_save_request_new_game("settings");
                settings_close();
            }
        }

        CLAY({.id = CLAY_ID("settings/close"),
              .layout = {.sizing = {CLAY_SIZING_FIT(.min = m.hit * 2.0F), ui_kit_hit_height(&m)}}}) {
            ui_kit_button_begin(ctx, nt_ui_id("settings/close/button"), &g_ui_theme.button, interactive, NULL);
            loc_kit_label(ctx, loc_settings_close(), &g_ui_theme.button_label);
            if (ui_kit_button_end(ctx) && interactive) {
                settings_close();
            }
        }
    }
    ui_kit_panel_end(ctx);
}
