#include "ui/first_screen_hud.h"

#include "clay.h"
#include "game_actions.h"
#include "game_combat.h"
#include "game_content.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "scene/scene_interactions.h"
#include "scene/scene_layout.h"
#include "ui/bottom_nav.h"
#include "ui/combat_flow.h"
#include "ui/equipment_screen.h"
#include "ui/location_screen.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/shop_screen.h"
#include "ui/tutorial_callout.h"
#include "ui/world_map_screen.h"
#include "window/nt_window.h"

#include <stdint.h>
#include <stdio.h>

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
    TOP_HUD_GOLD_COIN,
    TOP_HUD_PORTRAIT_FRAME,
    TOP_HUD_STATUS_PLAQUE,
    TOP_HUD_HP_FRAME,
    TOP_HUD_XP_FRAME,
    TOP_HUD_RESOURCE_COIN_CHIP,
    TOP_HUD_RESOURCE_SUPPLIES_CHIP,
    TOP_HUD_LOCATION_PLAQUE,
    TOP_HUD_LEVEL_BADGE,
    TOP_HUD_ICON_COIN,
    TOP_HUD_REGION_COUNT,
} top_hud_region_t;

static const nt_hash64_t TOP_HUD_REGION_HASHES[TOP_HUD_REGION_COUNT] = {
    ASSET_ATLAS_REGION_UI_SEEKER_PORTRAIT_HUD,
    ASSET_ATLAS_REGION_UI_GOLD_COIN_HUD,
    ASSET_ATLAS_REGION_UI_TOP_HUD_PORTRAIT_FRAME,
    ASSET_ATLAS_REGION_UI_TOP_HUD_STATUS_PLAQUE,
    ASSET_ATLAS_REGION_UI_TOP_HUD_HP_FRAME,
    ASSET_ATLAS_REGION_UI_TOP_HUD_XP_FRAME,
    ASSET_ATLAS_REGION_UI_TOP_HUD_RESOURCE_COIN_CHIP,
    ASSET_ATLAS_REGION_UI_TOP_HUD_RESOURCE_SUPPLIES_CHIP,
    ASSET_ATLAS_REGION_UI_TOP_HUD_LOCATION_PLAQUE,
    ASSET_ATLAS_REGION_UI_TOP_HUD_LEVEL_BADGE,
    ASSET_ATLAS_REGION_UI_TOP_HUD_ICON_COIN,
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

typedef struct top_hud_xp_progress_t {
    int level;
    int current_xp;
    int needed_xp;
    float ratio;
    bool capped;
} top_hud_xp_progress_t;

static top_hud_xp_progress_t top_hud_xp_progress(int total_xp) {
    static const int level_thresholds[] = {0, 20, 45};
    const int threshold_count = (int)(sizeof level_thresholds / sizeof level_thresholds[0]);
    if (total_xp < 0) {
        total_xp = 0;
    }

    int level_index = 0;
    for (int i = 1; i < threshold_count; ++i) {
        if (total_xp >= level_thresholds[i]) {
            level_index = i;
        }
    }

    if (level_index >= threshold_count - 1) {
        return (top_hud_xp_progress_t){.level = threshold_count, .current_xp = 0, .needed_xp = 0, .ratio = 1.0F, .capped = true};
    }

    const int floor_xp = level_thresholds[level_index];
    const int next_xp = level_thresholds[level_index + 1];
    const int needed_xp = next_xp - floor_xp;
    int current_xp = total_xp - floor_xp;
    if (current_xp < 0) {
        current_xp = 0;
    }
    if (current_xp > needed_xp) {
        current_xp = needed_xp;
    }

    return (top_hud_xp_progress_t){.level = level_index + 1,
                                   .current_xp = current_xp,
                                   .needed_xp = needed_xp,
                                   .ratio = needed_xp > 0 ? (float)current_xp / (float)needed_xp : 1.0F,
                                   .capped = false};
}

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
    const nt_ui_label_style_t bar_label = label_style(13.0F, 255.0F, 240.0F, 218.0F, 255.0F);
    ratio = ratio < 0.0F ? 0.0F : (ratio > 1.0F ? 1.0F : ratio);
    CLAY({.id = CLAY_IDI("first_screen/top_hud_bar", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {18.0F, 13.0F, 9.0F, 218.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {105.0F, 76.0F, 43.0F, 182.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_TOP_ART)}) {
        const float inset_x = 4.0F;
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

static void top_hud_gold_counter(nt_ui_context_t *ctx, const World *w, bool portrait) {
    int gold = 0;
    if (w && w->player_state) {
        gold = w->player_state->wallet_gold;
    }

    char gold_label[32];
    (void)snprintf(gold_label, sizeof gold_label, "%d", gold);

    const nt_ui_label_style_t value_style = label_style(portrait ? 17.0F : 18.0F, 250.0F, 232.0F, 190.0F, 255.0F);
    const float counter_w = portrait ? 116.0F : 128.0F;
    const float counter_h = portrait ? 34.0F : 36.0F;
    const float coin_size = portrait ? 24.0F : 27.0F;
    const float portrait_size = portrait ? 58.0F : 64.0F;
    const float status_w = portrait ? 178.0F : 206.0F;
    const float cluster_w = portrait_size + status_w + 18.0F;
    const float cluster_h = portrait ? 74.0F : 76.0F;
    const float cluster_right_gap = portrait ? 70.0F : 72.0F;
    const float cluster_top = 14.0F;
    const float portrait_left_inset = 4.0F;
    const float counter_x = counter_w - cluster_w - cluster_right_gap + portrait_left_inset;
    const float counter_y = cluster_top + cluster_h + 2.0F;
    CLAY({.id = CLAY_ID("first_screen/gold_counter"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                       .offset = {counter_x, counter_y}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(counter_w), CLAY_SIZING_FIXED(counter_h)},
                     .padding = {.left = 3, .right = 9, .top = 0, .bottom = 1},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 7,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {10.0F, 7.0F, 4.0F, 72.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .userData = NT_UI_CLAY_DATA(LAYER_TOP_ART)}) {
        CLAY({.id = CLAY_ID("first_screen/gold_coin"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(coin_size), CLAY_SIZING_FIXED(coin_size)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            top_hud_image_on_layer(ctx, TOP_HUD_GOLD_COIN, LAYER_TOP_OVERLAY);
        }
        shadowed_label(ctx, 20, gold_label, &value_style);
    }
}

static void top_hud_level_badge(nt_ui_context_t *ctx, bool portrait, int level_value) {
    const float badge_size = portrait ? 20.0F : 22.0F;
    const nt_ui_label_style_t level = label_style(portrait ? 12.0F : 13.0F, 255.0F, 240.0F, 208.0F, 255.0F);
    char level_label[8];
    (void)snprintf(level_label, sizeof level_label, "%d", level_value);
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
        shadowed_label(ctx, 12, level_label, &level);
    }
}

static void top_hud_player_cluster(nt_ui_context_t *ctx, const World *w, bool portrait) {
    const float portrait_size = portrait ? 58.0F : 64.0F;
    const float portrait_art_size = portrait_size - 8.0F;
    const float status_w = portrait ? 178.0F : 206.0F;
    const float bar_h = 24.0F;
    const float bar_gap = 4.0F;
    const float status_h = bar_h * 2.0F + bar_gap;
    const float cluster_h = portrait ? 74.0F : 76.0F;
    const float cluster_w = portrait_size + status_w + 18.0F;
    int hero_hp = 0;
    int max_hp = 1;
    int hero_xp = 0;
    if (w && w->player_state) {
        game_combat_stats_t stats;
        if (game_combat_build_player_stats(w->player_state, &stats) && stats.vitality > 0) {
            max_hp = stats.vitality;
        }
        hero_hp = w->player_state->hero_hp;
        if (hero_hp < 0) {
            hero_hp = 0;
        }
        if (hero_hp > max_hp) {
            hero_hp = max_hp;
        }
        hero_xp = w->player_state->hero_xp;
    }
    const top_hud_xp_progress_t xp = top_hud_xp_progress(hero_xp);
    char hp_label[32];
    char xp_label[32];
    (void)snprintf(hp_label, sizeof hp_label, "HP %d/%d", hero_hp, max_hp);
    if (xp.capped) {
        (void)snprintf(xp_label, sizeof xp_label, "XP MAX");
    } else {
        (void)snprintf(xp_label, sizeof xp_label, "XP %d/%d", xp.current_xp, xp.needed_xp);
    }

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
            top_hud_level_badge(ctx, portrait, xp.level);
        }

        CLAY({.id = CLAY_ID("first_screen/top_status"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(status_w), CLAY_SIZING_FIXED(status_h)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = (uint16_t)bar_gap,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            top_hud_bar(ctx, 0, (float)hero_hp / (float)max_hp, (Clay_Color){164.0F, 41.0F, 32.0F, 238.0F}, status_w, bar_h, hp_label);
            top_hud_bar(ctx, 1, xp.ratio, (Clay_Color){62.0F, 118.0F, 184.0F, 232.0F}, status_w, bar_h, xp_label);
        }
    }
}

static void top_hud_location(nt_ui_context_t *ctx, const World *w, bool portrait) {
    const char *location_name = "Последний Пост";
    if (w && w->player_state) {
        const game_location_definition_t *definition = game_content_find_location(w->player_state->world_current_location_id);
        if (definition && definition->display_name) {
            location_name = definition->display_name;
        }
    }
    const float plaque_w = portrait ? 150.0F : 184.0F;
    const float plaque_h = portrait ? 28.0F : 32.0F;
    const nt_ui_label_style_t location = label_style(portrait ? 11.0F : 12.0F, 220.0F, 210.0F, 184.0F, 232.0F);
    CLAY({.id = CLAY_ID("first_screen/top_location"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_TOP, .parent = CLAY_ATTACH_POINT_CENTER_TOP},
                       .offset = {0.0F, portrait ? 136.0F : 16.0F}},
          .layout = {.sizing = {CLAY_SIZING_FIXED(plaque_w), CLAY_SIZING_FIXED(plaque_h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("first_screen/top_location_art"),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
              .layout = {.sizing = {CLAY_SIZING_FIXED(plaque_w), CLAY_SIZING_FIXED(plaque_h)}}}) {
            top_hud_image(ctx, TOP_HUD_LOCATION_PLAQUE);
        }
        shadowed_label(ctx, 3, location_name, &location);
    }
}

static void top_hud_ui(nt_ui_context_t *ctx, const World *w, bool portrait) {
    ensure_top_hud_regions();

    CLAY({.id = CLAY_ID("first_screen/top_ui"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 166 : 124)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        CLAY({.id = CLAY_ID("first_screen/poki_reserve"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(112), CLAY_SIZING_FIXED(72)}}}) {}

        top_hud_player_cluster(ctx, w, portrait);
        top_hud_gold_counter(ctx, w, portrait);
        top_hud_location(ctx, w, portrait);
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
    if (bottom_nav_sheet_open() || combat_flow_is_open(w) || !w || w->dialogue.open) {
        return;
    }

    scene_point_t anchor = {0.0F, 0.0F};
    tutorial_callout_style_t style = tutorial_callout_default_style(portrait, layout_w);
    if (objective_callout_screen_anchor(w, layout_w, layout_h, &anchor)) {
        const char *text = w->first_scene.current_objective_text ? w->first_scene.current_objective_text : "РџРѕРіРѕРІРѕСЂРё СЃРѕ СЃС‚СЂР°Р¶РЅРёРєРѕРј";
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
        return;
    }

    const bool needs_starter_gear = game_actions_needs_starter_gear_onboarding(w->player_state);
    const bool needs_gate_check = game_actions_needs_gate_check_onboarding(w->player_state);
    if (!needs_starter_gear && !needs_gate_check) {
        return;
    }
    const int target_slot = needs_starter_gear ? 0 : 3;
    const uint32_t overlay_slot = needs_starter_gear ? 1U : 3U;

    const float side_margin = portrait ? 14.0F : 18.0F;
    const float gap = portrait ? 8.0F : 12.0F;
    const float max_btn_w = portrait ? 66.0F : 78.0F;
    const float fit_btn_w = (layout_w - side_margin * 2.0F - gap * 4.0F) / 5.0F;
    const float btn_w = min_f(max_btn_w, fit_btn_w);
    const float btn_h = btn_w * (365.0F / 299.0F);
    const float bottom_gap = portrait ? 12.0F : 10.0F;
    const float nav_w = btn_w * 5.0F + gap * 4.0F;
    const float target_x = layout_w * 0.5F - nav_w * 0.5F +
                           (float)target_slot * (btn_w + gap) + btn_w * 0.5F;
    const float target_y = layout_h - bottom_gap - btn_h * 0.50F;
    style.width = portrait ? clampf(layout_w - 34.0F, 220.0F, 332.0F) : 288.0F;
    anchor.x = clampf(target_x + style.width * 0.20F, style.width * 0.5F + 14.0F, layout_w - style.width * 0.5F - 14.0F);
    anchor.y = layout_h - bottom_gap - btn_h - (portrait ? 18.0F : 16.0F);
    tutorial_callout_ui(ctx,
                        &(tutorial_callout_desc_t){.visible = true,
                                                   .slot = overlay_slot,
                                                   .text = needs_starter_gear ? "Открой снаряжение" : "Здесь",
                                                   .element_anchor = TUTORIAL_CALLOUT_ANCHOR_CENTER_BOTTOM,
                                                   .parent_anchor = TUTORIAL_CALLOUT_ANCHOR_LEFT_TOP,
                                                   .offset_x = anchor.x,
                                                   .offset_y = anchor.y,
                                                   .style = style});
    const float finger = portrait ? 66.0F : 72.0F;
    tutorial_finger_ui(ctx,
                       &(tutorial_finger_desc_t){.visible = true,
                                                .slot = overlay_slot,
                                                .offset_x = clampf(target_x + btn_w * 0.12F, 0.0F, layout_w - finger),
                                                .offset_y = clampf(target_y - finger * 0.18F, 0.0F, layout_h - finger),
                                                .size = finger,
                                                .flip_bits = 0U});
}

void first_screen_hud_ui(nt_ui_context_t *ctx, World *w) {
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
        top_hud_ui(ctx, w, portrait);

        first_screen_tutorial_hint_ui(ctx, w, portrait, layout_w, layout_h);

        combat_flow_ui(ctx, w);

        if (!combat_flow_is_open(w)) {
            equipment_screen_ui(ctx, w);
            location_screen_ui(ctx, w);
            world_map_screen_ui(ctx, w);
            shop_screen_ui(ctx, w);
            if (!world_map_screen_open() && !location_screen_open() &&
                !shop_screen_open()) {
                bottom_nav_ui(ctx, w);
            }
        }
    }
}
