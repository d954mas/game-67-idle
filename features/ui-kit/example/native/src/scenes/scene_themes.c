#include "scenes.h"

#include "lab.h"
#include "lab_theme.h"
#include "lab_chrome.h"

#include "ui/nt_ui_scroll.h"
#include "ui/nt_ui_state.h"

#include "features/ui_kit/ui_kit.h"

#include <stdio.h>

static bool s_sample_toggle = true;

static Clay_Color clay_color(uint32_t abgr) {
    return (Clay_Color){(float)(abgr & 0xFFU), (float)((abgr >> 8) & 0xFFU), (float)((abgr >> 16) & 0xFFU),
                        (float)((abgr >> 24) & 0xFFU)};
}

void scene_themes_leave(nt_ui_context_t *ctx) { nt_ui_state_clear(ctx, nt_ui_id("themes/scroll")); }

// One theme row: three swatches (plate, action, coin) and the name; the active
// one wears the action colour like the nav.
static void theme_row(nt_ui_context_t *ctx, int index, const ui_metrics_t *m) {
    const lab_theme_desc_t *theme = lab_theme_at(index);
    const bool active = index == lab_theme_index();
    char id[48];
    (void)snprintf(id, sizeof id, "themes/pick/%s", theme->id);
    CLAY({.id = (Clay_ElementId){.id = nt_ui_id(id)},
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(m->hit * 1.2F)}}}) {
        ui_kit_button_begin(ctx, nt_ui_child_id(nt_ui_id(id), "plate"), ui_kit_button_style(active ? UI_KIT_BUTTON_CONFIRM : UI_KIT_BUTTON_NEUTRAL), true, NULL);
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .padding = {.left = (uint16_t)m->gap, .right = (uint16_t)m->gap},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = (uint16_t)(m->gap * 0.5F),
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            lab_swatch(ctx, theme->tokens->panel, m->hit * 0.5F);
            lab_swatch(ctx, theme->tokens->go, m->hit * 0.5F);
            lab_swatch(ctx, theme->tokens->coin, m->hit * 0.5F);
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}, .padding = {.left = (uint16_t)(m->gap * 0.5F)}}}) {
                ui_kit_label(ctx, theme->title, ui_kit_button_label_style(active ? UI_KIT_BUTTON_CONFIRM : UI_KIT_BUTTON_NEUTRAL));
            }
        }
        if (ui_kit_button_end(ctx)) {
            lab_request_theme(index);
        }
    }
}

// The same composition under every theme: what a repaint has to keep readable.
static void sample(nt_ui_context_t *ctx, const ui_metrics_t *m) {
    char amount[24];
    nt_ui_label_style_t caption = g_ui_theme.row_sub;
    caption.color = clay_color(ui_theme_tokens()->ink);
    ui_kit_panel_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                                                  .padding = CLAY_PADDING_ALL((uint16_t)m->pad),
                                                                  .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                                  .childGap = (uint16_t)m->gap}});
    ui_kit_label(ctx, "Пример", &g_ui_theme.title);
    ui_kit_label(ctx, "Те же компоненты, другие цвета. Состояние полигона не сбрасывается.", &g_ui_theme.label);
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = (uint16_t)m->gap,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        lab_format_amount(amount, sizeof amount, g_lab.coins);
        ui_kit_counter(ctx, "themes/sample/chip", &g_lab_art.coin, amount);
        ui_kit_meter_captioned(ctx, "themes/sample/meter", m->hit * 3.5F, m->hit * 0.55F, 0.64F, ui_theme_tokens()->go, "64%", &caption);
    }
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = (uint16_t)m->gap}}) {
        (void)ui_kit_button(ctx, "themes/sample/neutral", "Отмена", UI_KIT_BUTTON_NEUTRAL, true, CLAY_SIZING_GROW(0), ui_kit_hit_height(m));
        (void)ui_kit_button(ctx, "themes/sample/confirm", "Купить", UI_KIT_BUTTON_CONFIRM, true, CLAY_SIZING_GROW(0), ui_kit_hit_height(m));
    }
    (void)ui_kit_toggle_row(ctx, "themes/sample/toggle", "Тумблер", &s_sample_toggle, true);
    ui_kit_panel_end(ctx);
}

void scene_themes_build(nt_ui_context_t *ctx) {
    const ui_metrics_t m = ui_metrics();
    lab_ground(ctx, "themes/ground");
    CLAY({.id = CLAY_ID("themes/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = (uint16_t)(m.margin + m.safe_l), .right = (uint16_t)(m.margin + m.safe_r),
                                 .bottom = (uint16_t)(m.margin + m.safe_b)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}}) {
        nt_ui_scroll_style_t scroll = nt_ui_scroll_style_defaults();
        scroll.bar_visibility = NT_UI_SCROLLBAR_AUTO;
        scroll.bar_thickness = m.gap * 0.3F;
        scroll.thumb_ref = g_ui_theme.art.slider_fill;
        scroll.thumb_tint = ui_theme_tokens()->ink_soft;
        nt_ui_scroll_begin(ctx, NULL, nt_ui_id("themes/scroll"), &scroll,
                           &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(m.panel_w), CLAY_SIZING_GROW(0)},
                                                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                                 .childGap = (uint16_t)(m.gap * 0.6F),
                                                                 .padding = {.left = (uint16_t)m.pad, .right = (uint16_t)m.pad, .bottom = (uint16_t)m.pad}}});
        lab_section(ctx, "Темы");
        ui_kit_label(ctx, "Тема = лист токенов + папка арта. Клавиша T листает.", &g_ui_theme.hint);
        for (int i = 0; i < lab_theme_count(); ++i) {
            theme_row(ctx, i, &m);
        }
        lab_section(ctx, "Как выглядит");
        sample(ctx, &m);
        nt_ui_scroll_end(ctx);
    }
}
