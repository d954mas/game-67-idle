#include "ui/first_screen_hud.h"

#include "clay.h"
#include "ui/nt_ui_label.h"
#include "window/nt_window.h"

#define LAYER_TEXT_SHADOW 1
#define LAYER_TEXT 2

static nt_ui_label_style_t label_style(float font_size, float r, float g, float b, float a) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = font_size, .color = {r, g, b, a}};
}

static float fit_width(float desired, float layout_w, float margin) {
    const float max_w = layout_w > margin * 2.0F ? layout_w - margin * 2.0F : layout_w;
    return desired < max_w ? desired : max_w;
}

static void shadowed_label(nt_ui_context_t *ctx, int slot, const char *text, const nt_ui_label_style_t *style) {
    nt_ui_label_style_t shadow = *style;
    shadow.color = (Clay_Color){8.0F, 5.0F, 3.0F, 142.0F};

    CLAY({.id = CLAY_IDI("first_screen/shadowed_label", slot),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        CLAY({.id = CLAY_IDI("first_screen/shadowed_label_shadow", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                           .offset = {1.0F, 1.0F}},
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT_SHADOW), text, &shadow);
        }
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), text, style);
    }
}

static void player_stats_text(nt_ui_context_t *ctx, bool compact) {
    const nt_ui_label_style_t label = label_style(compact ? 17.0F : 20.0F, 248.0F, 244.0F, 232.0F, 255.0F);
    const nt_ui_label_style_t hint = label_style(compact ? 14.0F : 16.0F, 218.0F, 211.0F, 196.0F, 255.0F);

    CLAY({.id = CLAY_ID("first_screen/player_stats_text"),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = compact ? 1 : 2,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        if (compact) {
            shadowed_label(ctx, 1, "Искатель", &label);
            shadowed_label(ctx, 2, "HP 100/100", &hint);
            shadowed_label(ctx, 5, "XP 0/20", &hint);
        } else {
            shadowed_label(ctx, 1, "Искатель  HP 100/100", &label);
            shadowed_label(ctx, 2, "XP 0/20", &hint);
        }
    }
}

void first_screen_hud_ui(nt_ui_context_t *ctx, const World *w) {
    (void)w;
    const bool portrait = g_nt_window.fb_height > g_nt_window.fb_width;
    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    (void)layout_h;

    CLAY({.id = CLAY_ID("first_screen/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = CLAY_PADDING_ALL(14),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        CLAY({.id = CLAY_ID("first_screen/top_ui"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 86 : 72)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = portrait ? 10 : 12,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.id = CLAY_ID("first_screen/poki_reserve"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(112), CLAY_SIZING_FIXED(72)}}}) {}

            player_stats_text(ctx, portrait);
        }

        if (!portrait) {
            CLAY({.id = CLAY_ID("first_screen/location_label"),
                  .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_TOP, .parent = CLAY_ATTACH_POINT_CENTER_TOP},
                               .offset = {0.0F, 20.0F}},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(260), CLAY_SIZING_FIXED(28)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                const nt_ui_label_style_t location = label_style(16.0F, 235.0F, 221.0F, 192.0F, 245.0F);
                shadowed_label(ctx, 3, "Последний Пост", &location);
            }
        }

        const float hint_width = fit_width(portrait ? 360.0F : 430.0F, layout_w, 14.0F);
        CLAY({.id = CLAY_ID("first_screen/bottom_hint"),
              .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                           .offset = {0.0F, -38.0F}},
              .layout = {.sizing = {CLAY_SIZING_FIXED(hint_width), CLAY_SIZING_FIXED(34)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            const nt_ui_label_style_t prompt = label_style(20.0F, 248.0F, 244.0F, 232.0F, 255.0F);
            shadowed_label(ctx, 4, "Поговори со стражником", &prompt);
        }
    }
}
