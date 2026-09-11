#include "scenes.h"

#include "lab.h"
#include "lab_theme.h"

#include "ui/nt_ui_scroll.h"
#include "ui/nt_ui_state.h"

#include "features/ui_kit/ui_kit.h"

#include <math.h>
#include <stdio.h>

void scene_upgrade_enter(nt_ui_context_t *ctx) {
    (void)ctx;
    g_lab.sheet_open = true;
}

void scene_upgrade_leave(nt_ui_context_t *ctx) {
    g_lab.sheet_open = false;
    nt_ui_state_clear(ctx, nt_ui_id("upgrade/scroll"));
    ui_kit_sheet_clear(ctx, "upgrade/sheet");
}

// One card: what the player buys, what it does now and next, what it costs.
// The price button is the only interactive part; its state IS the card's state.
static void upgrade_card(nt_ui_context_t *ctx, int index) {
    const ui_metrics_t m = ui_metrics();
    const ui_tokens_t *t = ui_theme_tokens();
    lab_upgrade_t *u = &g_lab.upgrades[index];
    const int price = lab_upgrade_price(u);
    const bool unlocked = lab_upgrade_unlocked(u);
    const bool maxed = u->level >= u->max_level;
    const bool affordable = unlocked && !maxed && g_lab.coins >= price;
    char id[48];
    char line[96];
    char amount[24];

    ui_kit_tile_begin(ctx, &(Clay_ElementDeclaration){
                               .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                          .padding = CLAY_PADDING_ALL((uint16_t)(m.gap * 0.7F)),
                                          .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                          .childGap = (uint16_t)(m.gap * 0.7F),
                                          .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}});
    // The icon sits on a recessed plate: a socket, not a second buy button.
    const float socket = m.hit * 1.1F;
    ui_kit_plate_begin(ctx,
                       &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(socket), CLAY_SIZING_FIXED(socket)},
                                                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                       unlocked ? t->inset : t->tile_dim);
    ui_kit_icon(ctx, u->icon, m.hit * 0.7F);
    ui_kit_plate_end(ctx);
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = (uint16_t)(m.gap * 0.2F)}}) {
        ui_kit_label(ctx, u->name, &g_ui_theme.row_title);
        if (!unlocked) {
            (void)snprintf(line, sizeof line, "Откроется на уровне %d", u->unlock_level);
        } else if (maxed) {
            (void)snprintf(line, sizeof line, "Максимальный уровень");
        } else {
            char now[16];
            char next[16];
            (void)snprintf(now, sizeof now, "%.*f", u->decimals, (double)lab_upgrade_value(u, u->level));
            (void)snprintf(next, sizeof next, "%.*f", u->decimals, (double)lab_upgrade_value(u, u->level + 1));
            (void)snprintf(line, sizeof line, "%s → %s", now, next);
        }
        ui_kit_label(ctx, line, &g_ui_theme.row_sub);
    }
    (void)snprintf(id, sizeof id, "upgrade/buy/%d", index);
    if (!unlocked || maxed) {
        (void)ui_kit_button(ctx, id, unlocked ? "Макс" : "Закрыто", UI_KIT_BUTTON_NEUTRAL, false,
                         CLAY_SIZING_FIXED(m.hit * 2.6F), CLAY_SIZING_FIXED(m.hit));
    } else {
        lab_format_amount(amount, sizeof amount, price);
        if (ui_kit_icon_button(ctx, id, &g_lab_art.coin, amount, affordable ? UI_KIT_BUTTON_CONFIRM : UI_KIT_BUTTON_NEUTRAL, affordable,
                            CLAY_SIZING_FIXED(m.hit * 2.6F), CLAY_SIZING_FIXED(m.hit))) {
            (void)lab_try_buy(index);
        }
    }
    ui_kit_tile_end(ctx);
}

void scene_upgrade_build(nt_ui_context_t *ctx) {
    scene_hud_build_world(ctx);
    scene_hud_build_overlay(ctx);

    const ui_metrics_t m = ui_metrics();
    const float available_h = m.view_h - m.safe_t - m.safe_b;
    const float panel_h = fminf(available_h * 0.82F, m.hit * 11.0F);
    if (ui_kit_sheet_begin(ctx, "upgrade/sheet", "УЛУЧШЕНИЯ", "×", &g_lab.sheet_open, m.panel_w, panel_h)) {
        // The list scrolls inside the sheet; the continue action stays outside it.
        nt_ui_scroll_begin(ctx, NULL, nt_ui_id("upgrade/scroll"), ui_kit_scroll_style(),
                           &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                                 .childGap = (uint16_t)(m.gap * 0.7F),
                                                                 .padding = {.right = (uint16_t)(m.gap * 0.6F)}}});
        for (int i = 0; i < LAB_UPGRADE_COUNT; ++i) {
            upgrade_card(ctx, i);
        }
        nt_ui_scroll_end(ctx);
        // Every sheet dismisses with the same word; "continue" would read as a
        // different action on a screen that only closes.
        if (ui_kit_button(ctx, "upgrade/close", "ЗАКРЫТЬ", UI_KIT_BUTTON_NEUTRAL, true, CLAY_SIZING_GROW(0), ui_kit_hit_height(&m))) {
            g_lab.sheet_open = false;
        }
        ui_kit_sheet_end(ctx);
    } else if (!g_lab.sheet_open) {
        lab_goto(LAB_SCENE_HUD);
    }
}
