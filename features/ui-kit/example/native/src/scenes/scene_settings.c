#include "scenes.h"

#include "lab.h"

#include "ui/nt_ui_dropdown.h"

#include "features/ui_kit/ui_kit.h"

#include <stdio.h>

// The settings screen the studio's games ship: a scrimmed kit panel that fits
// its content, three volume rows, the language picker, a small neutral reset
// that opens a confirmation, a round cross on the corner and a full-width
// Close at the bottom. One layout at every size: the canvas is 390 units
// across its short edge everywhere, and the column is shorter than that.

static const char *const LANGUAGES[] = {"Русский", "English"};
#define LANGUAGE_COUNT ((int)(sizeof LANGUAGES / sizeof LANGUAGES[0]))
static const char *const LANGUAGE_ROW_IDS[] = {"settings/language/0", "settings/language/1"};

// View state: closing the panel folds the list and the confirmation.
static bool s_language_open;
static bool s_confirm_open;

void scene_settings_enter(nt_ui_context_t *ctx) {
    (void)ctx;
    g_lab.sheet_open = true;
}

void scene_settings_leave(nt_ui_context_t *ctx) {
    g_lab.sheet_open = false;
    s_language_open = false;
    s_confirm_open = false;
    ui_kit_sheet_clear(ctx, "settings/confirm");
}

static void volume_row(nt_ui_context_t *ctx, const char *id, const char *name, float *value, float row_width) {
    char caption[64];
    (void)snprintf(caption, sizeof caption, "%s: %d%%", name, (int)(*value * 100.0F + 0.5F));
    (void)ui_kit_slider_row(ctx, id, caption, value, true, row_width);
}

// The picker: the trigger names the current choice, the list holds them all.
static void language_row(nt_ui_context_t *ctx, const ui_metrics_t *m, float row_width) {
    nt_ui_dropdown_style_t dropdown = g_ui_theme.dropdown;
    dropdown.min_width = (uint16_t)(row_width * 0.56F);
    dropdown.font_size = ui_css(dropdown.font_size);
    dropdown.row_height = (uint16_t)m->hit;
    dropdown.pad = (uint16_t)(m->gap * 0.6F);
    dropdown.max_visible_rows = LANGUAGE_COUNT;
    CLAY({.id = CLAY_ID("settings/language"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = (uint16_t)m->gap,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
            ui_kit_label(ctx, "Язык", &g_ui_theme.label);
        }
        nt_ui_combo_preview_begin(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), UI_LAYER_TEXT, nt_ui_id("settings/language/trigger"),
                                  &dropdown, &s_language_open);
        CLAY({.id = CLAY_ID("settings/language/current"), .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
            ui_kit_label(ctx, LANGUAGES[g_lab.language], &g_ui_theme.button_label);
        }
        if (nt_ui_combo_preview_end(ctx)) {
            for (int i = 0; i < LANGUAGE_COUNT; ++i) {
                nt_ui_combo_selectable_begin(ctx, (uint32_t)i, i == g_lab.language);
                CLAY({.id = (Clay_ElementId){.id = nt_ui_id(LANGUAGE_ROW_IDS[i])},
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
                    ui_kit_label(ctx, LANGUAGES[i], &g_ui_theme.button_label);
                }
                if (nt_ui_combo_selectable_end(ctx)) {
                    g_lab.language = i;
                }
            }
            nt_ui_combo_end(ctx);
        }
    }
}

// The destructive action is two steps: a small red button here, sized to its
// word, and the confirmation with a Cancel that is the default.
static void reset_row(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        if (ui_kit_button(ctx, "settings/reset", "Сбросить прогресс", UI_KIT_BUTTON_DANGER, true,
                          CLAY_SIZING_FIT(.min = m->hit * 2.0F), ui_kit_hit_height(m))) {
            s_confirm_open = true;
        }
    }
}

// Declared at root level after the panel: a sheet is never inside a plate.
static void confirm_sheet(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    if (!ui_kit_sheet_begin(ctx, "settings/confirm", "СБРОСИТЬ ПРОГРЕСС?", NULL, &s_confirm_open, m->panel_w, 0.0F)) {
        return;
    }
    ui_kit_label(ctx, "Уровни, монеты и улучшения будут удалены. Настройки останутся.", &g_ui_theme.label);
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = (uint16_t)m->gap}}) {
        if (ui_kit_button(ctx, "settings/confirm/cancel", "Отмена", UI_KIT_BUTTON_NEUTRAL, true, CLAY_SIZING_GROW(0), ui_kit_hit_height(m))) {
            s_confirm_open = false;
        }
        if (ui_kit_button(ctx, "settings/confirm/reset", "Сбросить", UI_KIT_BUTTON_DANGER, true, CLAY_SIZING_GROW(0), ui_kit_hit_height(m))) {
            // The demo wallet and level go back to their first values; the
            // settings themselves are what the panel is showing, so they stay.
            const lab_state_t keep = g_lab;
            lab_init();
            g_lab.volume_master = keep.volume_master;
            g_lab.volume_music = keep.volume_music;
            g_lab.volume_sfx = keep.volume_sfx;
            g_lab.language = keep.language;
            g_lab.sheet_open = keep.sheet_open;
            s_confirm_open = false;
        }
    }
    ui_kit_sheet_end(ctx);
}

void scene_settings_build(nt_ui_context_t *ctx) {
    // The world stays, the HUD does not: a scrim at zIndex 0 dims only what
    // sits below the panel's own layers, so HUD plates and text under the
    // panel would show through it. A sheet (engine modal) dims everything and
    // may keep the HUD; this screen holds sliders and cannot be a sheet.
    scene_hud_build_world(ctx);
    if (!g_lab.sheet_open) {
        lab_goto(LAB_SCENE_HUD);
        return;
    }

    const ui_metrics_t m = ui_metrics();
    const float row_width = m.panel_w - m.pad * 2.0F - m.gap;

    ui_kit_scrim(ctx);
    nt_ui_block_pointer(ctx, nt_ui_id("ui_kit/scrim"), NULL);
    ui_kit_panel_begin(ctx, &(Clay_ElementDeclaration){
                                .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                                             .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                                              .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                                             .offset = {(m.safe_l - m.safe_r) * 0.5F, (m.safe_t - m.safe_b) * 0.5F}},
                                .layout = {.sizing = {CLAY_SIZING_FIXED(m.panel_w), CLAY_SIZING_FIT(0)},
                                           .padding = {.left = (uint16_t)m.pad, .right = (uint16_t)m.pad,
                                                       .top = (uint16_t)(m.pad * 0.5F), .bottom = (uint16_t)(m.pad * 0.5F)},
                                           .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                           .childGap = (uint16_t)(m.gap * 0.6F),
                                           .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    ui_kit_label(ctx, "НАСТРОЙКИ", &g_ui_theme.title);
    // The cross is what a player looks for; the bottom Close is what a thumb reaches.
    if (ui_kit_close_button(ctx, "settings/cross", "×")) {
        g_lab.sheet_open = false;
    }
    volume_row(ctx, "settings/master", "Общая", &g_lab.volume_master, row_width);
    volume_row(ctx, "settings/music", "Музыка", &g_lab.volume_music, row_width);
    volume_row(ctx, "settings/sfx", "Звуки", &g_lab.volume_sfx, row_width);
    language_row(ctx, &m, row_width);
    reset_row(ctx, &m);
    if (ui_kit_button(ctx, "settings/close", "ЗАКРЫТЬ", UI_KIT_BUTTON_NEUTRAL, true, CLAY_SIZING_GROW(0), ui_kit_hit_height(&m))) {
        g_lab.sheet_open = false;
    }
    ui_kit_panel_end(ctx);

    confirm_sheet(ctx, &m);
}
