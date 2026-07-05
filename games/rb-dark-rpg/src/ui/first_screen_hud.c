#include "ui/first_screen_hud.h"

#include "clay.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "scene/scene_interactions.h"
#include "scene/scene_layout.h"
#include "ui/bottom_nav.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/tutorial_callout.h"
#include "window/nt_window.h"

#define LAYER_TOP_FILL 4
#define LAYER_TOP_ART 5
#define LAYER_TOP_BAR_FILL 6
#define LAYER_TOP_PORTRAIT 7
#define LAYER_TOP_OVERLAY 8
#define LAYER_TEXT_SHADOW 9
#define LAYER_TEXT 10

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

typedef enum top_hud_region_t {
    TOP_HUD_SEEKER_PORTRAIT = 0,
    TOP_HUD_PORTRAIT_FRAME,
    TOP_HUD_STATUS_PLAQUE,
    TOP_HUD_HP_FRAME,
    TOP_HUD_XP_FRAME,
    TOP_HUD_RESOURCE_COIN_CHIP,
    TOP_HUD_RESOURCE_SUPPLIES_CHIP,
    TOP_HUD_LOCATION_PLAQUE,
    TOP_HUD_LEVEL_BADGE,
    TOP_HUD_REGION_COUNT,
} top_hud_region_t;

static const nt_hash64_t TOP_HUD_REGION_HASHES[TOP_HUD_REGION_COUNT] = {
    ASSET_ATLAS_REGION_UI_SEEKER_PORTRAIT_HUD,
    ASSET_ATLAS_REGION_UI_TOP_HUD_PORTRAIT_FRAME,
    ASSET_ATLAS_REGION_UI_TOP_HUD_STATUS_PLAQUE,
    ASSET_ATLAS_REGION_UI_TOP_HUD_HP_FRAME,
    ASSET_ATLAS_REGION_UI_TOP_HUD_XP_FRAME,
    ASSET_ATLAS_REGION_UI_TOP_HUD_RESOURCE_COIN_CHIP,
    ASSET_ATLAS_REGION_UI_TOP_HUD_RESOURCE_SUPPLIES_CHIP,
    ASSET_ATLAS_REGION_UI_TOP_HUD_LOCATION_PLAQUE,
    ASSET_ATLAS_REGION_UI_TOP_HUD_LEVEL_BADGE,
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

static void top_hud_image_on_layer(nt_ui_context_t *ctx, top_hud_region_t region, nt_ui_layer_t layer) {
    nt_ui_image_style_t style = nt_ui_image_style_defaults();
    nt_ui_image(ctx, NT_UI_DATA_LAYER(layer), &s_top_hud_regions[region], &style, NULL);
}

static void top_hud_image(nt_ui_context_t *ctx, top_hud_region_t region) {
    top_hud_image_on_layer(ctx, region, LAYER_TOP_ART);
}

static void top_hud_bar(nt_ui_context_t *ctx, int slot, float ratio, Clay_Color fill_color, float w, float h, const char *label) {
    const nt_ui_label_style_t bar_label = label_style(12.0F, 255.0F, 240.0F, 218.0F, 255.0F);
    ratio = ratio < 0.0F ? 0.0F : (ratio > 1.0F ? 1.0F : ratio);
    CLAY({.id = CLAY_IDI("first_screen/top_hud_bar", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {18.0F, 13.0F, 9.0F, 218.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {105.0F, 76.0F, 43.0F, 182.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_TOP_ART)}) {
        const float inset_x = min_f(4.0F, w * 0.08F);
        const float inset_y = 4.0F;
        const float fill_w = (w - inset_x * 2.0F) * ratio;
        if (fill_w > 1.0F) {
            CLAY({.id = CLAY_IDI("first_screen/top_hud_bar_fill", slot),
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_LEFT_CENTER},
                               .offset = {inset_x, 0.0F}},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(fill_w), CLAY_SIZING_FIXED(h - inset_y * 2.0F)}},
                  .backgroundColor = fill_color,
                  .cornerRadius = CLAY_CORNER_RADIUS(2),
                  .userData = NT_UI_CLAY_DATA(LAYER_TOP_BAR_FILL)}) {}
        }
        if (label) {
            shadowed_label(ctx, 30 + slot, label, &bar_label);
        }
    }
}

static void top_hud_gold_counter(nt_ui_context_t *ctx, bool portrait) {
    const nt_ui_label_style_t value_style = label_style(13.0F, 250.0F, 232.0F, 190.0F, 255.0F);
    const float coin_size = portrait ? 13.0F : 14.0F;
    CLAY({.id = CLAY_ID("first_screen/gold_counter"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {portrait ? -78.0F : -82.0F, portrait ? 86.0F : 82.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? 72.0F : 80.0F), CLAY_SIZING_FIXED(20.0F)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 5,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("first_screen/gold_coin"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(coin_size), CLAY_SIZING_FIXED(coin_size)}},
              .backgroundColor = {209.0F, 151.0F, 61.0F, 245.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(7),
              .border = {.color = {87.0F, 55.0F, 19.0F, 190.0F}, .width = {1, 1, 1, 1, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_TOP_OVERLAY)}) {}
        shadowed_label(ctx, 20, "1 250", &value_style);
    }
}

static void top_hud_level_badge(nt_ui_context_t *ctx, bool portrait) {
    const float badge_size = portrait ? 20.0F : 22.0F;
    const nt_ui_label_style_t level = label_style(portrait ? 12.0F : 13.0F, 255.0F, 240.0F, 208.0F, 255.0F);
    CLAY({.id = CLAY_ID("first_screen/top_level_badge"),
          .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_BOTTOM, .parent = CLAY_ATTACH_POINT_RIGHT_BOTTOM},
                       .offset = {1.0F, 1.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(badge_size), CLAY_SIZING_FIXED(badge_size)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("first_screen/top_level_badge_art"),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
              .layout = {.sizing = {CLAY_SIZING_FIXED(badge_size), CLAY_SIZING_FIXED(badge_size)}}}) {
            top_hud_image_on_layer(ctx, TOP_HUD_LEVEL_BADGE, LAYER_TOP_OVERLAY);
        }
        shadowed_label(ctx, 12, "1", &level);
    }
}

static void top_hud_player_cluster(nt_ui_context_t *ctx, bool portrait) {
    const float portrait_size = portrait ? 58.0F : 64.0F;
    const float portrait_art_size = portrait_size - 8.0F;
    const float status_w = portrait ? 178.0F : 206.0F;
    const float status_h = portrait ? 48.0F : 50.0F;
    const float cluster_h = portrait ? 68.0F : 72.0F;
    const float cluster_w = portrait_size + status_w + 18.0F;

    CLAY({.id = CLAY_ID("first_screen/top_player_cluster"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {portrait ? -70.0F : -72.0F, portrait ? 14.0F : 14.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(cluster_w), CLAY_SIZING_FIXED(cluster_h)},
                     .padding = {.left = 4, .right = 8, .top = 4, .bottom = 4},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 6,
                     .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {14.0F, 10.0F, 7.0F, 168.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = {115.0F, 79.0F, 42.0F, 150.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_TOP_FILL)}) {
        CLAY({.id = CLAY_ID("first_screen/top_portrait"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(portrait_size), CLAY_SIZING_FIXED(portrait_size)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.id = CLAY_ID("first_screen/top_portrait_art"),
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(portrait_art_size), CLAY_SIZING_FIXED(portrait_art_size)}}}) {
                top_hud_image_on_layer(ctx, TOP_HUD_SEEKER_PORTRAIT, LAYER_TOP_PORTRAIT);
            }
            CLAY({.id = CLAY_ID("first_screen/top_portrait_frame"),
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(portrait_size), CLAY_SIZING_FIXED(portrait_size)}}}) {
                top_hud_image(ctx, TOP_HUD_PORTRAIT_FRAME);
            }
            top_hud_level_badge(ctx, portrait);
        }

        CLAY({.id = CLAY_ID("first_screen/top_status"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(status_w), CLAY_SIZING_FIXED(status_h)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 4,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            top_hud_bar(ctx, 0, 0.82F, (Clay_Color){164.0F, 41.0F, 32.0F, 238.0F}, status_w, 22.0F, "HP 245/300");
            top_hud_bar(ctx, 1, 0.60F, (Clay_Color){62.0F, 118.0F, 184.0F, 232.0F}, status_w, 22.0F, "XP 12/20");
        }
    }
}

static void top_hud_location(nt_ui_context_t *ctx, bool portrait) {
    const float plaque_w = portrait ? 150.0F : 184.0F;
    const float plaque_h = portrait ? 28.0F : 32.0F;
    const nt_ui_label_style_t location = label_style(portrait ? 11.0F : 12.0F, 220.0F, 210.0F, 184.0F, 232.0F);
    CLAY({.id = CLAY_ID("first_screen/top_location"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_TOP, .parent = CLAY_ATTACH_POINT_CENTER_TOP},
                       .offset = {0.0F, portrait ? 106.0F : 16.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(plaque_w), CLAY_SIZING_FIXED(plaque_h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("first_screen/top_location_art"),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
              .layout = {.sizing = {CLAY_SIZING_FIXED(plaque_w), CLAY_SIZING_FIXED(plaque_h)}}}) {
            top_hud_image(ctx, TOP_HUD_LOCATION_PLAQUE);
        }
        shadowed_label(ctx, 3, "Последний Пост", &location);
    }
}

static void top_hud_ui(nt_ui_context_t *ctx, bool portrait) {
    ensure_top_hud_regions();

    CLAY({.id = CLAY_ID("first_screen/top_ui"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 112 : 110)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        CLAY({.id = CLAY_ID("first_screen/poki_reserve"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(112), CLAY_SIZING_FIXED(72)}}}) {}

        top_hud_player_cluster(ctx, portrait);
        top_hud_gold_counter(ctx, portrait);
        top_hud_location(ctx, portrait);
    }
}

static bool objective_callout_screen_anchor(const World *w, float layout_w, float layout_h, scene_point_t *out_anchor) {
    if (!w || !out_anchor || w->first_scene.objective_object_id == SCENE_OBJECT_ID_NONE) {
        return false;
    }
    const scene_interaction_object_t *object = scene_interactions_find(w->first_scene.objective_object_id);
    if (!object || !scene_interactions_should_show_tutorial_finger(w, object->id)) {
        return false;
    }

    const float center_x = w->first_scene.camera_initialized ? w->first_scene.camera_center_x : scene_layout_default_center_x();
    const float center_y = w->first_scene.camera_initialized ? w->first_scene.camera_center_y : scene_layout_default_center_y();
    const scene_view_t view = scene_layout_compute_view((int)layout_w, (int)layout_h, center_x, center_y);
    if (view.scale <= 0.0F) {
        return false;
    }

    const float master_x = object->anchor_x + 16.0F;
    const float master_y = (float)object->bounds.y + (float)object->bounds.h * 0.64F;
    *out_anchor = scene_layout_master_to_screen(view, master_x, master_y);
    return true;
}

static void first_screen_tutorial_hint_ui(nt_ui_context_t *ctx, const World *w, bool portrait, float layout_w, float layout_h) {
    if (bottom_nav_sheet_open() || !w || w->dialogue.open || w->first_scene.tutorial_guard_talk_completed) {
        return;
    }

    scene_point_t anchor = {0.0F, 0.0F};
    if (!objective_callout_screen_anchor(w, layout_w, layout_h, &anchor)) {
        return;
    }

    const char *text = w->first_scene.current_objective_text ? w->first_scene.current_objective_text : "Поговори со стражником";
    tutorial_callout_style_t style = tutorial_callout_default_style(portrait, layout_w);
    const float margin_x = 14.0F + style.width * 0.5F;
    const float min_y = 108.0F + style.height;
    const float max_y = layout_h - (portrait ? 134.0F : 116.0F);
    anchor.x = clampf(anchor.x, margin_x, layout_w - margin_x);
    anchor.y = clampf(anchor.y - (portrait ? 72.0F : 58.0F), min_y, max_y);
    tutorial_callout_ui(ctx,
                        &(tutorial_callout_desc_t){.visible = true,
                                                   .slot = 0,
                                                   .text = text,
                                                   .element_anchor = TUTORIAL_CALLOUT_ANCHOR_CENTER_BOTTOM,
                                                   .parent_anchor = TUTORIAL_CALLOUT_ANCHOR_LEFT_TOP,
                                                   .offset_x = anchor.x,
                                                   .offset_y = anchor.y,
                                                   .style = style});
}

void first_screen_hud_ui(nt_ui_context_t *ctx, const World *w) {
    const bool portrait = g_nt_window.fb_height > g_nt_window.fb_width;
    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    CLAY({.id = CLAY_ID("first_screen/root"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = CLAY_PADDING_ALL(14),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        top_hud_ui(ctx, portrait);

        first_screen_tutorial_hint_ui(ctx, w, portrait, layout_w, layout_h);

        bottom_nav_ui(ctx, w);
    }
}
