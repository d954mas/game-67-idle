#include "scenes.h"

#include "lab.h"

#include "ui/nt_ui_state.h"

#include "features/ui_kit/ui_kit.h"
#include "features/ui_kit/ui_shape.h"

#include <stdio.h>

// The look seam: a dialog in looks the screen composed from the theme's own
// (a gradient band under a checker, a square close), buttons in a look with
// their derived states, glyphs turned by angle, and the settings rows. Under
// the `shape` theme the rows are the kit's shapes; under an art theme the same
// calls keep the art. The column is taller than a landscape canvas, so there
// the body scrolls under the band; in portrait it fits and has no scissor.

static const char *const SIZES[] = {"Мал.", "Сред.", "Бол."};
static const char *const QUALITY[] = {"Низкое", "Среднее", "Высокое"};

static bool s_open = true;
static bool s_music = true;
static bool s_hints;
static bool s_vibration = true;
static int s_hand = 1;
static float s_volume = 0.65F;
static int s_size = 1;
static int s_quality;
static bool s_quality_open;

void scene_looks_leave(nt_ui_context_t *ctx) {
    s_open = true;
    s_quality_open = false;
    nt_ui_state_clear(ctx, nt_ui_child_id(nt_ui_id("looks/panel"), "scroll"));
}

static uint32_t rgb(uint32_t abgr) { return ((abgr & 0xFFU) << 16U) | (abgr & 0xFF00U) | ((abgr >> 16U) & 0xFFU); }

static uint32_t mix(uint32_t a, uint32_t b, float k) {
    uint32_t out = 0U;
    for (uint32_t shift = 0U; shift < 24U; shift += 8U) {
        const float x = (float)((a >> shift) & 0xFFU);
        const float y = (float)((b >> shift) & 0xFFU);
        out |= (uint32_t)(x + (y - x) * k + 0.5F) << shift;
    }
    return out;
}

// A button with its ledge outside the face and a gloss band: the look a game
// builds on top of the theme's plate.
static ui_kit_panel_style_t raised(uint32_t tint) {
    ui_kit_panel_style_t s = ui_kit_look_button(tint);
    s.lip_inside = false;
    s.top_rgb = mix(rgb(tint), 0xFFFFFFU, 0.18F);
    s.gloss_h = 7.0F;
    s.gloss_inset = 7.0F;
    s.gloss_top = 3.0F;
    s.gloss_a = 0.35F;
    return s;
}

static void caption(nt_ui_context_t *ctx, const char *text) { ui_kit_label(ctx, text, &g_ui_theme.hint); }

static void button_states(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    const ui_tokens_t *t = ui_theme_tokens();
    const ui_kit_button_look_t look = {.idle = raised(t->go)};
    const ui_kit_panel_style_t pressed = ui_kit_look_state(&look.idle, UI_KIT_LOOK_PRESSED, rgb(t->off));
    caption(ctx, "Кнопка в облике: обычная, нажатая, выключенная");
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = (uint16_t)m->gap}}) {
        CLAY({.id = CLAY_ID("looks/go"), .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(m->hit)}}}) {
            ui_kit_button_begin_styled(ctx, nt_ui_id("looks/go/plate"), &look, true, NULL);
            ui_kit_label(ctx, "Вперед", &g_ui_theme.button_label_action);
            (void)ui_kit_button_end(ctx);
        }
        // The pressed look drawn still, so a screenshot shows what a held
        // press looks like next to the idle one.
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(m->hit)}}}) {
            ui_kit_panel_begin_styled(ctx, NT_UI_DATA_LAYER(UI_LAYER_SURFACE), &pressed,
                                      &(Clay_ElementDeclaration){
                                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                     .padding = {.top = (uint16_t)look.idle.lip_h},
                                                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}});
            ui_kit_label(ctx, "Вперед", &g_ui_theme.button_label_action);
            ui_kit_panel_styled_end(ctx);
        }
        CLAY({.id = CLAY_ID("looks/off"), .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(m->hit)}}}) {
            ui_kit_button_begin_styled(ctx, nt_ui_id("looks/off/plate"), &look, false, NULL);
            ui_kit_label(ctx, "Вперед", &g_ui_theme.button_label_disabled);
            (void)ui_kit_button_end(ctx);
        }
    }
}

// One glyph at a quarter turn each: 0 points right, a positive angle turns it
// counter-clockwise, as Y-up angles do.
static void turned_icons(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    caption(ctx, "Значок с поворотом: 0, 90, 180, 270 градусов");
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = (uint16_t)m->gap,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        for (int i = 0; i < 4; ++i) {
            ui_kit_icon_rotated(ctx, ui_kit_icon_ref(UI_ICON_ARROW_RIGHT), m->hit * 0.7F, ui_theme_tokens()->ink,
                                (float)i * 1.5707963F);
        }
    }
}

void scene_looks_build(nt_ui_context_t *ctx) {
    scene_hud_build_world(ctx);
    if (!s_open) {
        s_open = true;
        lab_goto(LAB_SCENE_HUD);
        return;
    }
    const ui_metrics_t m = ui_metrics();
    const ui_tokens_t *t = ui_theme_tokens();
    const float row_width = m.panel_w - m.pad * 2.0F - m.gap;

    ui_kit_dialog_look_t look = ui_kit_dialog_look();
    look.header.top_rgb = mix(rgb(t->go), 0xFFFFFFU, 0.15F);
    look.header.bottom_rgb = rgb(t->go);
    look.header.checker_px = 7.0F;
    look.header.checker_a = 0.14F;
    const ui_kit_button_look_t close = {.idle = ui_kit_look_button(t->danger)};
    look.close = &close;

    // A scrimmed panel, not a sheet: the body holds a slider and may scroll.
    ui_kit_scrim(ctx, true);
    CLAY({.floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                        .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                       .offset = {(m.safe_l - m.safe_r) * 0.5F, (m.safe_t - m.safe_b) * 0.5F}},
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        if (ui_kit_dialog_begin_styled(ctx, "looks/panel", "ОБЛИКИ", "×", CLAY_SIZING_FIXED(m.panel_w),
                                       CLAY_SIZING_FIT(0), &look)) {
            s_open = false;
        }
        button_states(ctx, &m);
        turned_icons(ctx, &m);
        caption(ctx, "Строки настроек");
        (void)ui_kit_toggle_row(ctx, "looks/music", "Музыка", &s_music, true);
        (void)ui_kit_toggle_row(ctx, "looks/hints", "Подсказки", &s_hints, true);
        (void)ui_kit_checkbox_row(ctx, "looks/vibration", "Вибрация", &s_vibration, true);
        (void)ui_kit_radio_row(ctx, "looks/left", "Левая рука", &s_hand, 0, true);
        (void)ui_kit_radio_row(ctx, "looks/right", "Правая рука", &s_hand, 1, true);
        char volume[48];
        (void)snprintf(volume, sizeof volume, "%d%%", (int)(s_volume * 100.0F + 0.5F));
        (void)ui_kit_slider_row(ctx, "looks/volume", "Громкость", volume, &s_volume, true, row_width, NULL);
        (void)ui_kit_choice_row(ctx, "looks/size", "Размер", SIZES, 3, &s_size, true, NULL);
        (void)ui_kit_choice_row(ctx, "looks/locked", "Режим", SIZES, 3, &s_size, false, NULL);
        (void)ui_kit_dropdown_row(ctx, "looks/quality", "Качество", QUALITY, 3, &s_quality, &s_quality_open, true,
                                  row_width * 0.56F);
        ui_kit_dialog_end(ctx);
    }
}
