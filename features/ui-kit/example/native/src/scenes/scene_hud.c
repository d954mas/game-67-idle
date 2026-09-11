#include "scenes.h"

#include "lab.h"
#include "lab_theme.h"

#include "ui/nt_ui_image.h"

#include "features/ui_kit/ui_kit.h"

#include <math.h>
#include <stdio.h>

// The world backdrop is 3:2; it covers the canvas the way a camera would, so
// the HUD is laid over a picture rather than beside one.
#define WORLD_W 1152.0F
#define WORLD_H 768.0F

static Clay_Color clay_color(uint32_t abgr) {
    return (Clay_Color){(float)(abgr & 0xFFU), (float)((abgr >> 8) & 0xFFU), (float)((abgr >> 16) & 0xFFU),
                        (float)((abgr >> 24) & 0xFFU)};
}

void scene_hud_build_world(nt_ui_context_t *ctx) {
    const ui_metrics_t m = ui_metrics();
    const float cover = fmaxf(m.view_w / WORLD_W, m.view_h / WORLD_H);
    const nt_ui_image_style_t style = nt_ui_image_style_defaults();
    // zIndex -1 puts the picture under the main tree, whatever the walker does
    // with root-attached floats at zero.
    nt_ui_image(ctx, NT_UI_DATA_LAYER(UI_LAYER_SCRIM), &g_lab_art.world, &style,
                &(Clay_ElementDeclaration){
                    .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                                 .zIndex = -1,
                                 .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                                  .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                                 .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH},
                    .layout = {.sizing = {CLAY_SIZING_FIXED(WORLD_W * cover), CLAY_SIZING_FIXED(WORLD_H * cover)}}});
}

static int affordable_upgrades(void) {
    int n = 0;
    for (int i = 0; i < LAB_UPGRADE_COUNT; ++i) {
        const lab_upgrade_t *u = &g_lab.upgrades[i];
        if (lab_upgrade_unlocked(u) && u->level < u->max_level && g_lab.coins >= lab_upgrade_price(u)) {
            ++n;
        }
    }
    return n;
}

void scene_hud_build_overlay(nt_ui_context_t *ctx) {
    const ui_metrics_t m = ui_metrics();
    const ui_tokens_t *t = ui_theme_tokens();
    char buf[48];

    const nt_ui_label_style_t on_world = g_ui_theme.on_world;
    nt_ui_label_style_t meter_caption = g_ui_theme.row_sub;
    meter_caption.color = clay_color(t->ink);

    // The top inset carries no safe_t here because the lab's scene list above
    // already does; a game copying this HUD adds m.safe_t to the top padding.
    CLAY({.id = CLAY_ID("hud/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = (uint16_t)(m.margin + m.safe_l),
                                 .right = (uint16_t)(m.margin + m.safe_r),
                                 .top = (uint16_t)m.margin,
                                 .bottom = (uint16_t)(m.margin + m.safe_b)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = (uint16_t)m.gap}}) {
        // Top edge: level and progress on the left, wallet and options on the right.
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = (uint16_t)m.gap,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = (uint16_t)(m.gap * 0.4F)}}) {
                (void)snprintf(buf, sizeof buf, "УРОВЕНЬ %d", g_lab.level);
                ui_kit_label(ctx, buf, &on_world);
                (void)snprintf(buf, sizeof buf, "%d / %d", g_lab.xp, g_lab.xp_next);
                ui_kit_meter_captioned(ctx, "hud/xp", m.hit * 4.0F, m.hit * 0.55F,
                          g_lab.xp_next > 0 ? (float)g_lab.xp / (float)g_lab.xp_next : 0.0F, t->go, buf, &meter_caption);
            }
            CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = (uint16_t)(m.gap * 0.5F),
                             .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP}}}) {
                lab_format_amount(buf, sizeof buf, g_lab.coins);
                ui_kit_counter(ctx, "hud/wallet", &g_lab_art.coin, buf);
                if (ui_kit_button(ctx, "hud/settings", "Опции", UI_KIT_BUTTON_NEUTRAL, true, CLAY_SIZING_FIXED(m.hit * 2.2F), CLAY_SIZING_FIXED(m.hit))) {
                    lab_goto(LAB_SCENE_SETTINGS);
                }
                // The one blue button: a rewarded ad, with the play glyph the kit ships.
                if (ui_kit_icon_button(ctx, "hud/ad", &g_ui_theme.art.icon_play, "+240", UI_KIT_BUTTON_AD, true, CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(m.hit))) {
                    g_lab.coins += 240;
                }
            }
        }

        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {}

        // The hint: a tile so the line stays readable on any part of the picture.
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            ui_kit_tile_begin(ctx, &(Clay_ElementDeclaration){
                                       .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                                                  .padding = {.left = (uint16_t)m.gap, .right = (uint16_t)m.gap,
                                                              .top = (uint16_t)(m.gap * 0.5F), .bottom = (uint16_t)(m.gap * 0.5F)}}});
            ui_kit_label(ctx, "Удерживай для удара", &g_ui_theme.row_title);
            ui_kit_tile_end(ctx);
        }

        // Bottom edge: abilities under the thumb, the main action at the far end.
        const float slot = m.hit * 1.5F;
        const bool cooling = g_lab.cooldown_left > 0.0F;
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = (uint16_t)m.gap,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_BOTTOM}}}) {
            if (ui_kit_icon_button(ctx, "hud/ability/0", &g_lab_art.sword, NULL,
                                g_lab.ability == 0 ? UI_KIT_BUTTON_CONFIRM : UI_KIT_BUTTON_NEUTRAL, true,
                                CLAY_SIZING_FIXED(slot), CLAY_SIZING_FIXED(slot))) {
                g_lab.ability = 0;
            }
            (void)snprintf(buf, sizeof buf, "%dс", (int)ceilf(g_lab.cooldown_left));
            if (ui_kit_icon_button(ctx, "hud/ability/1", &g_lab_art.energy, cooling ? buf : NULL,
                                g_lab.ability == 1 ? UI_KIT_BUTTON_CONFIRM : UI_KIT_BUTTON_NEUTRAL, !cooling,
                                CLAY_SIZING_FIXED(cooling ? slot * 1.35F : slot), CLAY_SIZING_FIXED(slot))) {
                g_lab.ability = 1;
            }
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {}
            CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(m.hit * 3.4F), CLAY_SIZING_FIXED(slot)}}}) {
                if (ui_kit_button(ctx, "hud/upgrade", "УЛУЧШИТЬ", UI_KIT_BUTTON_CONFIRM, true, CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0))) {
                    lab_goto(LAB_SCENE_UPGRADE);
                }
                const int n = affordable_upgrades();
                if (n > 0) {
                    (void)snprintf(buf, sizeof buf, "%d", n);
                    ui_kit_badge(ctx, buf);
                }
            }
        }
    }
}

void scene_hud_build(nt_ui_context_t *ctx) {
    scene_hud_build_world(ctx);
    scene_hud_build_overlay(ctx);
}
