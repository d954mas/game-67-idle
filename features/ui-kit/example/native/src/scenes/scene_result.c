#include "scenes.h"

#include "lab.h"
#include "lab_theme.h"

#include "features/ui_kit/ui_kit.h"

#include <stdio.h>

void scene_result_enter(nt_ui_context_t *ctx) {
    (void)ctx;
    g_lab.sheet_open = true;
}

void scene_result_leave(nt_ui_context_t *ctx) {
    g_lab.sheet_open = false;
    ui_kit_sheet_clear(ctx, "result/sheet");
}

// A reward tile: a big icon with the delta under it, the size of a prize,
// nothing else.
static void reward(nt_ui_context_t *ctx, const char *id, nt_atlas_region_ref_t *icon, const char *text) {
    const ui_metrics_t m = ui_metrics();
    const float side = m.hit * 2.2F;
    nt_ui_label_style_t amount = g_ui_theme.amount;
    amount.color = g_ui_theme.row_title.color;
    CLAY({.id = (Clay_ElementId){.id = nt_ui_id(id)}, .layout = {.sizing = {CLAY_SIZING_FIXED(side), CLAY_SIZING_FIT(0)}}}) {
        ui_kit_tile_begin(ctx, &(Clay_ElementDeclaration){
                                   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(side)},
                                              .childGap = (uint16_t)(m.gap * 0.3F),
                                              .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                              .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}});
        ui_kit_icon(ctx, icon, m.hit * 1.2F);
        ui_kit_label(ctx, text, &amount);
        ui_kit_tile_end(ctx);
    }
}

void scene_result_build(nt_ui_context_t *ctx) {
    scene_hud_build_world(ctx);
    scene_hud_build_overlay(ctx);

    const ui_metrics_t m = ui_metrics();
    char line[64];
    // No title row: the result names itself in the middle, and there is no
    // close cross because the reward is taken by one of the two actions.
    if (ui_kit_sheet_begin(ctx, "result/sheet", NULL, NULL, &g_lab.sheet_open, m.panel_w, 0.0F)) {
        nt_ui_label_style_t title = g_ui_theme.title;
        title.align = CLAY_TEXT_ALIGN_CENTER;
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = (uint16_t)(m.gap * 0.3F),
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}}) {
            ui_kit_label(ctx, "ПОБЕДА!", &title);
            (void)snprintf(line, sizeof line, "Уровень %d пройден", g_lab.level);
            ui_kit_label(ctx, line, &g_ui_theme.hint);
        }
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = (uint16_t)m.gap,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            reward(ctx, "result/reward/coins", &g_lab_art.coin, "+240");
            reward(ctx, "result/reward/nuts", &g_lab_art.xp, "+15");
        }
        if (ui_kit_button(ctx, "result/next", "ДАЛЬШЕ", UI_KIT_BUTTON_CONFIRM, true, CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(m.hit * 1.2F))) {
            lab_claim_result();
            g_lab.sheet_open = false;
        }
        if (ui_kit_button(ctx, "result/retry", "ПОВТОР", UI_KIT_BUTTON_NEUTRAL, true, CLAY_SIZING_GROW(0), ui_kit_hit_height(&m))) {
            g_lab.sheet_open = false;
        }
        ui_kit_sheet_end(ctx);
    } else if (!g_lab.sheet_open) {
        lab_goto(LAB_SCENE_HUD);
    }
}
