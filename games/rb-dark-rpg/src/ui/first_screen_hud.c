#include "ui/first_screen_hud.h"

#include "clay.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/bottom_nav.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/tutorial_callout.h"
#include "window/nt_window.h"

#define LAYER_TEXT_SHADOW 1
#define LAYER_TEXT 2
#define LAYER_TOP_FILL 4
#define LAYER_TOP_ART 5

typedef enum top_hud_region_t {
    TOP_HUD_PORTRAIT_FRAME = 0,
    TOP_HUD_STATUS_PLAQUE,
    TOP_HUD_HP_FRAME,
    TOP_HUD_XP_FRAME,
    TOP_HUD_RESOURCE_COIN_CHIP,
    TOP_HUD_RESOURCE_SUPPLIES_CHIP,
    TOP_HUD_LOCATION_PLAQUE,
    TOP_HUD_REGION_COUNT,
} top_hud_region_t;

static const nt_hash64_t TOP_HUD_REGION_HASHES[TOP_HUD_REGION_COUNT] = {
    ASSET_ATLAS_REGION_UI_TOP_HUD_PORTRAIT_FRAME,
    ASSET_ATLAS_REGION_UI_TOP_HUD_STATUS_PLAQUE,
    ASSET_ATLAS_REGION_UI_TOP_HUD_HP_FRAME,
    ASSET_ATLAS_REGION_UI_TOP_HUD_XP_FRAME,
    ASSET_ATLAS_REGION_UI_TOP_HUD_RESOURCE_COIN_CHIP,
    ASSET_ATLAS_REGION_UI_TOP_HUD_RESOURCE_SUPPLIES_CHIP,
    ASSET_ATLAS_REGION_UI_TOP_HUD_LOCATION_PLAQUE,
};

static nt_resource_t s_top_hud_atlas;
static nt_atlas_region_ref_t s_top_hud_regions[TOP_HUD_REGION_COUNT];

static nt_ui_label_style_t label_style(float font_size, float r, float g, float b, float a) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = font_size, .color = {r, g, b, a}};
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

static float min_f(float a, float b) { return a < b ? a : b; }

static void ensure_top_hud_regions(void) {
    if (s_top_hud_atlas.id != 0U) {
        return;
    }

    s_top_hud_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    for (int i = 0; i < TOP_HUD_REGION_COUNT; ++i) {
        s_top_hud_regions[i] = nt_atlas_ref(s_top_hud_atlas, TOP_HUD_REGION_HASHES[i].value);
    }
}

static void top_hud_image(nt_ui_context_t *ctx, top_hud_region_t region) {
    nt_ui_image_style_t style = nt_ui_image_style_defaults();
    nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_TOP_ART), &s_top_hud_regions[region], &style, NULL);
}

static void top_hud_bar(nt_ui_context_t *ctx, int slot, top_hud_region_t frame, float ratio, Clay_Color fill_color, float w, float h) {
    ratio = ratio < 0.0F ? 0.0F : (ratio > 1.0F ? 1.0F : ratio);
    CLAY({.id = CLAY_IDI("first_screen/top_hud_bar", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}}}) {
        const float inset_x = min_f(10.0F, w * 0.08F);
        const float fill_w = (w - inset_x * 2.0F) * ratio;
        if (fill_w > 1.0F) {
            CLAY({.id = CLAY_IDI("first_screen/top_hud_bar_fill", slot),
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_LEFT_CENTER},
                               .offset = {inset_x, 0.0F}},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(fill_w), CLAY_SIZING_FIXED(h * 0.34F)}},
                  .backgroundColor = fill_color,
                  .cornerRadius = CLAY_CORNER_RADIUS(2),
                  .userData = NT_UI_CLAY_DATA(LAYER_TOP_FILL)}) {}
        }

        CLAY({.id = CLAY_IDI("first_screen/top_hud_bar_frame", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
              .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}}}) {
            top_hud_image(ctx, frame);
        }
    }
}

static void top_hud_resource_chip(nt_ui_context_t *ctx, int slot, top_hud_region_t region, const char *value, float w, float h) {
    const nt_ui_label_style_t value_style = label_style(13.0F, 246.0F, 231.0F, 198.0F, 255.0F);
    CLAY({.id = CLAY_IDI("first_screen/resource_chip", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)},
                     .padding = {.left = 28, .right = 8, .top = 0, .bottom = 1},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_IDI("first_screen/resource_chip_art", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
              .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}}}) {
            top_hud_image(ctx, region);
        }
        shadowed_label(ctx, 20 + slot, value, &value_style);
    }
}

static void top_hud_player_cluster(nt_ui_context_t *ctx, bool portrait) {
    const float portrait_size = portrait ? 42.0F : 48.0F;
    const float status_w = portrait ? 142.0F : 154.0F;
    const float status_h = portrait ? 58.0F : 62.0F;
    const nt_ui_label_style_t name_style = label_style(portrait ? 14.0F : 15.0F, 248.0F, 239.0F, 213.0F, 255.0F);
    const nt_ui_label_style_t hint_style = label_style(portrait ? 11.0F : 12.0F, 217.0F, 207.0F, 184.0F, 245.0F);

    CLAY({.id = CLAY_ID("first_screen/top_player_cluster"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {portrait ? -64.0F : -68.0F, portrait ? 14.0F : 13.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(portrait_size + status_w + 7.0F), CLAY_SIZING_FIXED(status_h)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 7,
                     .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("first_screen/top_portrait"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(portrait_size), CLAY_SIZING_FIXED(portrait_size)}}}) {
            top_hud_image(ctx, TOP_HUD_PORTRAIT_FRAME);
        }

        CLAY({.id = CLAY_ID("first_screen/top_status"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(status_w), CLAY_SIZING_FIXED(status_h)},
                         .padding = {.left = 13, .right = 13, .top = 6, .bottom = 6},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = portrait ? 2 : 3,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.id = CLAY_ID("first_screen/top_status_art"),
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(status_w), CLAY_SIZING_FIXED(status_h)}}}) {
                top_hud_image(ctx, TOP_HUD_STATUS_PLAQUE);
            }
            shadowed_label(ctx, 10, "Искатель", &name_style);
            top_hud_bar(ctx, 0, TOP_HUD_HP_FRAME, 1.0F, (Clay_Color){151.0F, 37.0F, 28.0F, 238.0F}, status_w - 26.0F, portrait ? 9.0F : 10.0F);
            top_hud_bar(ctx, 1, TOP_HUD_XP_FRAME, 0.0F, (Clay_Color){64.0F, 105.0F, 132.0F, 232.0F}, status_w - 26.0F, portrait ? 8.0F : 9.0F);
            if (!portrait) {
                shadowed_label(ctx, 11, "Ур. 1", &hint_style);
            }
        }
    }
}

static void top_hud_resource_row(nt_ui_context_t *ctx) {
    CLAY({.id = CLAY_ID("first_screen/top_resource_row"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {-68.0F, 78.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(164), CLAY_SIZING_FIXED(29)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 8,
                     .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER}}}) {
        top_hud_resource_chip(ctx, 0, TOP_HUD_RESOURCE_COIN_CHIP, "1 250", 78.0F, 28.0F);
        top_hud_resource_chip(ctx, 1, TOP_HUD_RESOURCE_SUPPLIES_CHIP, "81/100", 78.0F, 28.0F);
    }
}

static void top_hud_location(nt_ui_context_t *ctx) {
    const nt_ui_label_style_t location = label_style(16.0F, 238.0F, 224.0F, 194.0F, 250.0F);
    CLAY({.id = CLAY_ID("first_screen/top_location"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_TOP, .parent = CLAY_ATTACH_POINT_CENTER_TOP},
                       .offset = {0.0F, 17.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(258), CLAY_SIZING_FIXED(44)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("first_screen/top_location_art"),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
              .layout = {.sizing = {CLAY_SIZING_FIXED(258), CLAY_SIZING_FIXED(44)}}}) {
            top_hud_image(ctx, TOP_HUD_LOCATION_PLAQUE);
        }
        shadowed_label(ctx, 3, "Последний Пост", &location);
    }
}

static void top_hud_ui(nt_ui_context_t *ctx, bool portrait) {
    ensure_top_hud_regions();

    CLAY({.id = CLAY_ID("first_screen/top_ui"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 86 : 96)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        CLAY({.id = CLAY_ID("first_screen/poki_reserve"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(112), CLAY_SIZING_FIXED(72)}}}) {}

        top_hud_player_cluster(ctx, portrait);
        if (!portrait) {
            top_hud_location(ctx);
            top_hud_resource_row(ctx);
        }
    }
}

static void first_screen_tutorial_hint_ui(nt_ui_context_t *ctx, const World *w, bool portrait, float layout_w) {
    if (bottom_nav_sheet_open() || !w || w->dialogue.open || w->first_scene.tutorial_guard_talk_completed) {
        return;
    }

    const char *text = w->first_scene.current_objective_text ? w->first_scene.current_objective_text : "Поговори со стражником";
    tutorial_callout_style_t style = tutorial_callout_default_style(portrait, layout_w);
    tutorial_callout_ui(ctx,
                        &(tutorial_callout_desc_t){.visible = true,
                                                   .slot = 0,
                                                   .text = text,
                                                   .element_anchor = TUTORIAL_CALLOUT_ANCHOR_CENTER,
                                                   .parent_anchor = TUTORIAL_CALLOUT_ANCHOR_CENTER,
                                                   .offset_x = portrait ? 0.0F : 86.0F,
                                                   .offset_y = portrait ? -56.0F : -54.0F,
                                                   .style = style});
}

void first_screen_hud_ui(nt_ui_context_t *ctx, const World *w) {
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
        top_hud_ui(ctx, portrait);

        first_screen_tutorial_hint_ui(ctx, w, portrait, layout_w);

        bottom_nav_ui(ctx, w);
    }
}
