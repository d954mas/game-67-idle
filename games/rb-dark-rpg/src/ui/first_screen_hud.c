#include "ui/first_screen_hud.h"

#include "clay.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/theme.h"
#include "window/nt_window.h"

#define LAYER_BG 0
#define LAYER_TEXT 2

static void player_card(nt_ui_context_t *ctx, float width, bool compact) {
    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &g_theme.panel_region, &g_theme.panel_img,
                      &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(compact ? 74 : 62)},
                                                            .padding = CLAY_PADDING_ALL(10),
                                                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                            .childGap = 4,
                                                            .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}});
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), compact ? "Искатель" : "Искатель  HP 100/100", &g_theme.label);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), compact ? "HP 100/100  XP 0/20" : "XP 0/20", &g_theme.hint);
    nt_ui_panel_end(ctx);
}

void first_screen_hud_ui(nt_ui_context_t *ctx, const World *w) {
    (void)w;
    const bool portrait = g_nt_window.fb_height > g_nt_window.fb_width;

    CLAY({.id = CLAY_ID("first_screen/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = CLAY_PADDING_ALL(14),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        CLAY({.id = CLAY_ID("first_screen/top_ui"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(72)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 12,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.id = CLAY_ID("first_screen/poki_reserve"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(112), CLAY_SIZING_FIXED(72)}}}) {}

            if (!portrait) {
                player_card(ctx, 318.0F, false);
            }
        }

        if (portrait) {
            CLAY({.id = CLAY_ID("first_screen/portrait_stats_row"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}}) {
                player_card(ctx, 238.0F, true);
            }
        }

        const float hint_width = portrait ? 360.0F : 430.0F;
        nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &g_theme.panel_region, &g_theme.panel_img,
                          &(Clay_ElementDeclaration){
                              .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                                           .offset = {0.0F, -18.0F}},
                              .layout = {.sizing = {CLAY_SIZING_FIXED(hint_width), CLAY_SIZING_FIXED(64)},
                                         .padding = CLAY_PADDING_ALL(12),
                                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}});
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Поговори со стражником", &g_theme.label);
        nt_ui_panel_end(ctx);
    }
}
