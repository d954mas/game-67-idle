#include "scenes.h"

#include "lab.h"
#include "lab_theme.h"
#include "lab_chrome.h"

#include "ui/nt_ui_dropdown.h"
#include "ui/nt_ui_progress.h"
#include "ui/nt_ui_scroll.h"
#include "ui/nt_ui_state.h"

#include "features/ui_kit/ui_kit.h"

#include <stdio.h>

// Catalogue state: the values the demo controls edit. View state only.
static float s_slider = 0.35F;
static float s_progress;
static bool s_toggle_on = true;
static bool s_toggle_off;
static bool s_check = true;
static int s_radio = 1;
static bool s_pick_open;
static int s_pick = 0;
static bool s_dialog_open;

static const char *const PICKS[] = {"Маленький", "Средний", "Большой"};

void scene_components_leave(nt_ui_context_t *ctx) {
    s_pick_open = false;
    s_dialog_open = false;
    nt_ui_state_clear(ctx, nt_ui_id("components/scroll"));
    ui_kit_sheet_clear(ctx, "components/dialog");
}

// Two cells per row on a phone, four across a desk.
#define ROW_BEGIN(m)                                                                                  \
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},                            \
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,                                          \
                     .childGap = (uint16_t)(m).gap,                                                  \
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}})

static void buttons(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    bool portrait = false;
    portrait = m->portrait;
    lab_section(ctx, "Кнопки");
    const Clay_SizingAxis w = CLAY_SIZING_GROW(0);
    const Clay_SizingAxis h = ui_kit_hit_height(m);
    ROW_BEGIN(*m) {
        (void)ui_kit_button(ctx, "components/btn/neutral", "Обычная", UI_KIT_BUTTON_NEUTRAL, true, w, h);
        (void)ui_kit_button(ctx, "components/btn/confirm", "Действие", UI_KIT_BUTTON_CONFIRM, true, w, h);
        if (!portrait) {
            (void)ui_kit_icon_button(ctx, "components/btn/ad", &g_ui_theme.art.icon_play, "Реклама", UI_KIT_BUTTON_AD, true, w, h);
            (void)ui_kit_button(ctx, "components/btn/danger", "Опасно", UI_KIT_BUTTON_DANGER, true, w, h);
        }
    }
    if (portrait) {
        ROW_BEGIN(*m) {
            (void)ui_kit_icon_button(ctx, "components/btn/ad", &g_ui_theme.art.icon_play, "Реклама", UI_KIT_BUTTON_AD, true, w, h);
            (void)ui_kit_button(ctx, "components/btn/danger", "Опасно", UI_KIT_BUTTON_DANGER, true, w, h);
        }
    }
    ROW_BEGIN(*m) {
        (void)ui_kit_button(ctx, "components/btn/disabled", "Выключена", UI_KIT_BUTTON_CONFIRM, false, w, h);
        (void)ui_kit_icon_button(ctx, "components/btn/icon", &g_lab_art.coin, "С иконкой", UI_KIT_BUTTON_NEUTRAL, true, w, h);
    }
}

static void text(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    (void)m;
    lab_section(ctx, "Текст");
    ui_kit_label(ctx, "Заголовок окна", &g_ui_theme.title);
    ui_kit_label(ctx, "Заголовок раздела", &g_ui_theme.heading);
    ui_kit_label(ctx, "Обычный текст на панели, который может переноситься на вторую строку.", &g_ui_theme.label);
    ui_kit_label(ctx, "Подсказка или второстепенная строка", &g_ui_theme.hint);
    ui_kit_tile_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                                                 .padding = CLAY_PADDING_ALL((uint16_t)(m->gap * 0.7F)),
                                                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                                 .childGap = (uint16_t)(m->gap * 0.2F)}});
    ui_kit_label(ctx, "Строка списка на плитке", &g_ui_theme.row_title);
    ui_kit_label(ctx, "Вторая строка ряда", &g_ui_theme.row_sub);
    ui_kit_tile_end(ctx);
}

static void counters(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    lab_section(ctx, "Счетчики и индикаторы");
    char amount[24];
    ROW_BEGIN(*m) {
        lab_format_amount(amount, sizeof amount, g_lab.coins);
        ui_kit_counter(ctx, "components/chip/coins", &g_lab_art.coin, amount);
        lab_format_amount(amount, sizeof amount, g_lab.nuts);
        ui_kit_counter(ctx, "components/chip/nuts", &g_lab_art.xp, amount);
    }
    nt_ui_label_style_t caption = g_ui_theme.row_sub;
    caption.color = ui_kit_color(ui_theme_tokens()->ink);
    const float meter_w = m->panel_w - m->pad * 2.0F;
    ROW_BEGIN(*m) {
        ui_kit_meter_captioned(ctx, "components/meter/xp", meter_w, m->hit * 0.55F, 0.3F, ui_theme_tokens()->go, "30 / 100", &caption);
    }
    ROW_BEGIN(*m) {
        ui_kit_meter_captioned(ctx, "components/meter/hp", meter_w, m->hit * 0.55F, 0.85F, ui_theme_tokens()->danger, "85 / 100", &caption);
    }
    // The engine's own progress widget on the kit's styles, eased toward a
    // value that loops so the ease is visible.
    nt_ui_progress_style_t progress = g_ui_theme.progress;
    progress.track_w = meter_w;
    progress.track_h = ui_css(progress.track_h);
    nt_ui_progress(ctx, NT_UI_DATA_LAYER(UI_LAYER_IMG), UI_LAYER_IMG, nt_ui_id("components/progress"), s_progress, &progress,
                   &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(meter_w), CLAY_SIZING_FIXED(progress.track_h)}}});
}

static void controls(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    lab_section(ctx, "Ползунки и переключатели");
    const float width = m->panel_w - m->pad * 2.0F;
    (void)ui_kit_slider_row(ctx, "components/slider", "Ползунок", &s_slider, true, width);
    (void)ui_kit_slider_row(ctx, "components/slider_off", "Выключен", &s_slider, false, width);
    (void)ui_kit_toggle_row(ctx, "components/toggle_on", "Тумблер включен", &s_toggle_on, true);
    (void)ui_kit_toggle_row(ctx, "components/toggle_off", "Тумблер выключен", &s_toggle_off, true);
    (void)ui_kit_toggle_row(ctx, "components/toggle_disabled", "Тумблер недоступен", &s_toggle_on, false);
    (void)ui_kit_checkbox_row(ctx, "components/check", "Флажок", &s_check, true);
    for (int i = 0; i < 3; ++i) {
        char id[48];
        (void)snprintf(id, sizeof id, "components/radio/%d", i);
        (void)ui_kit_radio_row(ctx, id, PICKS[i], &s_radio, i, true);
    }
}

static void picker(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    lab_section(ctx, "Выпадающий список");
    (void)ui_kit_dropdown_row(ctx, "components/pick", "Размер", PICKS, 3, &s_pick, &s_pick_open,
                              (m->panel_w - m->pad * 2.0F) * 0.56F);
}

static void plates(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    lab_section(ctx, "Панель и плитка");
    ui_kit_panel_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                                                  .padding = CLAY_PADDING_ALL((uint16_t)m->pad),
                                                                  .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                                  .childGap = (uint16_t)m->gap}});
    ui_kit_label(ctx, "Панель: окно, лист, диалог", &g_ui_theme.label);
    ui_kit_tile_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                                                 .padding = CLAY_PADDING_ALL((uint16_t)(m->gap * 0.7F))}});
    ui_kit_label(ctx, "Плитка: ряд списка, карточка, счетчик", &g_ui_theme.row_title);
    ui_kit_tile_end(ctx);
    if (ui_kit_button(ctx, "components/dialog/open", "Открыть окно", UI_KIT_BUTTON_NEUTRAL, true, CLAY_SIZING_GROW(0), ui_kit_hit_height(m))) {
        s_dialog_open = true;
    }
    ui_kit_panel_end(ctx);
}

void scene_components_build(nt_ui_context_t *ctx) {
    const ui_metrics_t m = ui_metrics();
    lab_ground(ctx, "components/ground");
    s_progress += 0.004F;
    if (s_progress > 1.2F) {
        s_progress = -0.2F;
    }
    // Same column width a dialog gets, centred, so the catalogue shows the
    // components at the size a sheet would hold them.
    CLAY({.id = CLAY_ID("components/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = (uint16_t)(m.margin + m.safe_l), .right = (uint16_t)(m.margin + m.safe_r),
                                 .bottom = (uint16_t)(m.margin + m.safe_b)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}}) {
        nt_ui_scroll_begin(ctx, NULL, nt_ui_id("components/scroll"), ui_kit_scroll_style(),
                           &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(m.panel_w), CLAY_SIZING_GROW(0)},
                                                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                                 .childGap = (uint16_t)(m.gap * 0.6F),
                                                                 .padding = {.left = (uint16_t)m.pad, .right = (uint16_t)m.pad, .bottom = (uint16_t)m.pad}}});
        buttons(ctx, &m);
        text(ctx, &m);
        counters(ctx, &m);
        controls(ctx, &m);
        picker(ctx, &m);
        plates(ctx, &m);
        nt_ui_scroll_end(ctx);
    }
    // The dialog is declared after the scroll closed: a modal never lives
    // inside a clipped container.
    if (ui_kit_sheet_begin(ctx, "components/dialog", "ОКНО", "×", &s_dialog_open, m.panel_w, 0.0F)) {
        ui_kit_label(ctx, "Модальное окно: подложка гасит клики, Esc и клик мимо закрывают.", &g_ui_theme.label);
        if (ui_kit_button(ctx, "components/dialog/ok", "ПОНЯТНО", UI_KIT_BUTTON_CONFIRM, true, CLAY_SIZING_GROW(0), ui_kit_hit_height(&m))) {
            s_dialog_open = false;
        }
        ui_kit_sheet_end(ctx);
    }
}
