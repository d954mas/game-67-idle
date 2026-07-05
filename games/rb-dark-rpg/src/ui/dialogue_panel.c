#include "ui/dialogue_panel.h"

#include "clay.h"
#include "game_dialogue.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/game_modal.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_modal.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_scroll.h"
#include "ui/nt_ui_state.h"
#include "ui/nt_ui_tooltip.h"
#include "ui/theme.h"

#include <stdio.h>
#include <string.h>

#define LAYER_BG 30
#define LAYER_IMG 31
#define LAYER_TEXT 32

#define DIALOGUE_MODAL_ID 0xD1A106U
#define DIALOGUE_REWARD_DETAIL_MODAL_ID 0xD1A107U

#if defined(__GNUC__) || defined(__clang__)
#define DIALOGUE_UNUSED_VAR __attribute__((unused))
#else
#define DIALOGUE_UNUSED_VAR
#endif

static const dialogue_reward_t *s_selected_reward;
static bool s_reward_detail_open;
static bool s_dialogue_was_open;
static bool s_reward_detail_was_open;
static int s_dialogue_dismiss_guard_frames;
static int s_reward_detail_dismiss_guard_frames;
static bool s_dialogue_cleanup_pending;
static bool s_reward_detail_cleanup_pending;
static nt_resource_t s_dialogue_ui_atlas;
static nt_atlas_region_ref_t s_gate_guard_portrait_region;
static nt_atlas_region_ref_t s_scrollbar_white_region;
static nt_atlas_region_ref_t s_dialogue_outer_frame_region;
static nt_atlas_region_ref_t s_dialogue_body_panel_region;
static nt_atlas_region_ref_t s_dialogue_header_plaque_region;
static nt_atlas_region_ref_t s_dialogue_portrait_frame_region;
static nt_atlas_region_ref_t s_dialogue_objective_panel_region;
static nt_atlas_region_ref_t s_dialogue_reward_cell_region;
static nt_atlas_region_ref_t s_dialogue_answer_normal_region;
static nt_atlas_region_ref_t s_dialogue_answer_primary_region;
static nt_atlas_region_ref_t s_dialogue_divider_region;
static bool s_dialogue_regions_requested;

typedef enum dialogue_reward_icon_art_t {
    DIALOGUE_REWARD_ICON_OLD_SWORD = 0,
    DIALOGUE_REWARD_ICON_PADDED_JACKET,
    DIALOGUE_REWARD_ICON_LEATHER_GREAVES,
    DIALOGUE_REWARD_ICON_IRON_SWORD,
    DIALOGUE_REWARD_ICON_PATCHED_MAIL,
    DIALOGUE_REWARD_ICON_GUARD_COAT,
    DIALOGUE_REWARD_ICON_IRON_GREAVES,
    DIALOGUE_REWARD_ICON_MILITIA_AXE,
    DIALOGUE_REWARD_ICON_RUNNER_WRAPS,
    DIALOGUE_REWARD_ICON_BLACK_SUN_CHARM,
    DIALOGUE_REWARD_ICON_MILLER_HOOK,
    DIALOGUE_REWARD_ICON_CHAIN_PATCHES,
    DIALOGUE_REWARD_ICON_SCAVENGER_KNEE_PLATES,
    DIALOGUE_REWARD_ICON_DRAGON_ASH_TOKEN,
    DIALOGUE_REWARD_ICON_MILLER_LUCKY_NAIL,
    DIALOGUE_REWARD_ICON_SEEKER_TOKEN,
    DIALOGUE_REWARD_ICON_GRAIN_SACKS,
    DIALOGUE_REWARD_ICON_CONTRACT_PROGRESS,
    DIALOGUE_REWARD_ICON_CLUE_FRAGMENT,
    DIALOGUE_REWARD_ICON_BURNED_CHAIN_BRACKET,
    DIALOGUE_REWARD_ICON_ORDER_SCRAP,
    DIALOGUE_REWARD_ICON_COUNT,
} dialogue_reward_icon_art_t;

static void dialogue_request_state_cleanup(void) {
    s_dialogue_cleanup_pending = true;
    s_reward_detail_cleanup_pending = true;
}

static void dialogue_request_reward_detail_cleanup(void) { s_reward_detail_cleanup_pending = true; }

static void dialogue_clear_transient_ui_state(nt_ui_context_t *ctx) {
    if (!ctx) {
        return;
    }
    if (s_dialogue_cleanup_pending) {
        game_modal_clear_state(ctx, DIALOGUE_MODAL_ID);
        nt_ui_state_clear(ctx, nt_ui_id("dialogue/content_scroll"));
        s_dialogue_cleanup_pending = false;
    }
    if (s_reward_detail_cleanup_pending) {
        game_modal_clear_state(ctx, DIALOGUE_REWARD_DETAIL_MODAL_ID);
        s_reward_detail_cleanup_pending = false;
    }
}

static const char *DIALOGUE_REWARD_ICON_ASSET_IDS[DIALOGUE_REWARD_ICON_COUNT] DIALOGUE_UNUSED_VAR = {
    "asset_icon_old_sword",
    "asset_icon_padded_jacket",
    "asset_icon_leather_greaves",
    "asset_icon_iron_sword",
    "asset_icon_patched_mail",
    "asset_icon_guard_coat",
    "asset_icon_iron_greaves",
    "asset_icon_militia_axe",
    "asset_icon_runner_wraps",
    "asset_icon_black_sun_charm",
    "asset_icon_miller_hook",
    "asset_icon_chain_patches",
    "asset_icon_scavenger_knee_plates",
    "asset_icon_dragon_ash_token",
    "asset_icon_miller_lucky_nail",
    "asset_icon_seeker_token",
    "asset_icon_grain_sacks",
    "asset_icon_contract_progress",
    "asset_icon_clue_fragment",
    "asset_icon_burned_chain_bracket",
    "asset_icon_order_scrap",
};

static const nt_hash64_t DIALOGUE_REWARD_ICON_REGION_HASHES[DIALOGUE_REWARD_ICON_COUNT] = {
    ASSET_ATLAS_REGION_UI_ASSET_ICON_OLD_SWORD,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_PADDED_JACKET,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_LEATHER_GREAVES,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_IRON_SWORD,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_PATCHED_MAIL,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_GUARD_COAT,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_IRON_GREAVES,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_MILITIA_AXE,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_RUNNER_WRAPS,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_BLACK_SUN_CHARM,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_MILLER_HOOK,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_CHAIN_PATCHES,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_SCAVENGER_KNEE_PLATES,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_DRAGON_ASH_TOKEN,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_MILLER_LUCKY_NAIL,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_SEEKER_TOKEN,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_GRAIN_SACKS,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_CONTRACT_PROGRESS,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_CLUE_FRAGMENT,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_BURNED_CHAIN_BRACKET,
    ASSET_ATLAS_REGION_UI_ASSET_ICON_ORDER_SCRAP,
};

static nt_atlas_region_ref_t s_dialogue_reward_icon_regions[DIALOGUE_REWARD_ICON_COUNT];
static nt_atlas_region_ref_t s_dialogue_reward_xp_region;
static nt_atlas_region_ref_t s_dialogue_reward_gold_region;
static nt_atlas_region_ref_t s_dialogue_reward_unlock_region;

static nt_ui_label_style_t make_label(float size, float r, float g, float b, float a) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = size, .color = {r, g, b, a}};
}

static Clay_Color dialogue_underlay_outer(void) {
    return (Clay_Color){20.0F, 17.0F, 15.0F, 255.0F};
}

static Clay_Color dialogue_underlay_body(void) {
    return (Clay_Color){39.0F, 29.0F, 23.0F, 255.0F};
}

static Clay_Color dialogue_underlay_header(void) {
    return (Clay_Color){76.0F, 27.0F, 20.0F, 255.0F};
}

static Clay_Color dialogue_underlay_objective(void) {
    return (Clay_Color){82.0F, 42.0F, 24.0F, 255.0F};
}

static Clay_Color dialogue_underlay_choice(void) {
    return (Clay_Color){35.0F, 28.0F, 25.0F, 255.0F};
}

static Clay_Color dialogue_underlay_reward(void) {
    return (Clay_Color){43.0F, 31.0F, 23.0F, 255.0F};
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static void ensure_dialogue_regions(void) {
    if (s_dialogue_regions_requested) {
        return;
    }

    s_dialogue_ui_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    s_gate_guard_portrait_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_GATE_GUARD_PORTRAIT.value);
    s_scrollbar_white_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI__WHITE.value);
    s_dialogue_outer_frame_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_OUTER_FRAME.value);
    s_dialogue_body_panel_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_BODY_PANEL.value);
    s_dialogue_header_plaque_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_HEADER_PLAQUE.value);
    s_dialogue_portrait_frame_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_PORTRAIT_FRAME.value);
    s_dialogue_objective_panel_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_OBJECTIVE_PANEL.value);
    s_dialogue_reward_cell_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_REWARD_CELL.value);
    s_dialogue_answer_normal_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_ANSWER_NORMAL.value);
    s_dialogue_answer_primary_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_ANSWER_PRIMARY.value);
    s_dialogue_divider_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_DIALOGUE_DIVIDER.value);
    s_dialogue_reward_xp_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_ASSET_REWARD_XP.value);
    s_dialogue_reward_gold_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_GOLD_COIN_HUD.value);
    s_dialogue_reward_unlock_region = nt_atlas_ref(s_dialogue_ui_atlas, ASSET_ATLAS_REGION_UI_NAV_V11_MAP.value);
    for (int i = 0; i < DIALOGUE_REWARD_ICON_COUNT; ++i) {
        s_dialogue_reward_icon_regions[i] = nt_atlas_ref(s_dialogue_ui_atlas, DIALOGUE_REWARD_ICON_REGION_HASHES[i].value);
    }
    s_dialogue_regions_requested = true;
}

static nt_ui_button_style_t dialogue_choice_button(void) {
    nt_ui_button_style_t s = g_theme.button;
    s.idle.bg = (nt_atlas_region_ref_t){0};
    s.hover.bg = (nt_atlas_region_ref_t){0};
    s.pressed.bg = (nt_atlas_region_ref_t){0};
    s.disabled.bg = (nt_atlas_region_ref_t){0};
    s.idle.bg_tint = 0xFF7CB8D7U;
    s.hover.bg_tint = 0xFF8FCAE7U;
    s.pressed.bg_tint = 0xFF528BB2U;
    s.disabled.bg_tint = 0xFF7CB8D7U;
    s.transition_speed = 16.0F;
    return s;
}

static nt_ui_button_style_t dialogue_topic_button(void) {
    ensure_dialogue_regions();
    nt_ui_button_style_t s = dialogue_choice_button();
    s.idle.bg = s_dialogue_answer_normal_region;
    s.hover.bg = s_dialogue_answer_normal_region;
    s.pressed.bg = s_dialogue_answer_normal_region;
    s.disabled.bg = s_dialogue_answer_normal_region;
    s.idle.bg_tint = 0xFFFFFFFFU;
    s.hover.bg_tint = 0xFFE9F3F8U;
    s.pressed.bg_tint = 0xFFB5C8D2U;
    s.disabled.bg_tint = 0xFFFFFFFFU;
    s.slice9_scale = 0.48F;
    return s;
}

static nt_ui_button_style_t dialogue_primary_button(void) {
    ensure_dialogue_regions();
    nt_ui_button_style_t s = dialogue_choice_button();
    s.idle.bg = s_dialogue_answer_primary_region;
    s.hover.bg = s_dialogue_answer_primary_region;
    s.pressed.bg = s_dialogue_answer_primary_region;
    s.disabled.bg = s_dialogue_answer_primary_region;
    s.idle.bg_tint = 0xFFFFFFFFU;
    s.hover.bg_tint = 0xFFFFE7B5U;
    s.pressed.bg_tint = 0xFFD19A67U;
    s.disabled.bg_tint = 0xFFFFFFFFU;
    s.slice9_scale = 0.48F;
    return s;
}

static nt_ui_button_style_t dialogue_reward_button(void) {
    ensure_dialogue_regions();
    nt_ui_button_style_t s = dialogue_choice_button();
    s.idle.bg = s_dialogue_reward_cell_region;
    s.hover.bg = s_dialogue_reward_cell_region;
    s.pressed.bg = s_dialogue_reward_cell_region;
    s.disabled.bg = s_dialogue_reward_cell_region;
    s.idle.bg_tint = 0xFFFFFFFFU;
    s.hover.bg_tint = 0xFFFFE6B8U;
    s.pressed.bg_tint = 0xFFD5A66FU;
    s.disabled.bg_tint = 0xFFFFFFFFU;
    s.slice9_scale = 0.46F;
    return s;
}

static nt_ui_scroll_style_t dialogue_scroll_style(void) {
    ensure_dialogue_regions();
    nt_ui_scroll_style_t s = nt_ui_scroll_style_defaults();
    s.bar_visibility = NT_UI_SCROLLBAR_AUTO;
    s.bar_thickness = 8.0F;
    s.bar_thumb_min_px = 34.0F;
    s.track_ref = s_scrollbar_white_region;
    s.thumb_ref = s_scrollbar_white_region;
    s.track_tint = 0xA0243456U;
    s.thumb_tint = 0xF044BCECU;
    return s;
}

static nt_ui_button_style_t dialogue_close_button(void) {
    nt_ui_button_style_t s = dialogue_choice_button();
    s.idle.bg_tint = 0xFF835339U;
    s.hover.bg_tint = 0xFFA66845U;
    s.pressed.bg_tint = 0xFF5A3325U;
    s.disabled.bg_tint = 0xFF835339U;
    return s;
}

static nt_ui_modal_style_t dialogue_modal_style(void) {
    return game_modal_style((nt_ui_layer_t)LAYER_BG, true);
}

static nt_ui_modal_style_t reward_detail_modal_style(void) {
    nt_ui_modal_style_t s = game_modal_style((nt_ui_layer_t)LAYER_BG, true);
    s.backdrop_alpha = 0.28F;
    return s;
}

static nt_ui_tooltip_style_t reward_tooltip_style(void) {
    nt_ui_tooltip_style_t s = nt_ui_tooltip_style_defaults();
    s.delay_secs = 0.15F;
    s.font_size = 14.0F;
    s.max_width = 260U;
    s.pad = 8U;
    s.panel_bg = 0xFF1A2028U;
    s.text_color = 0xFFCFE7F6U;
    s.border_color = 0xFF659ED6U;
    s.border_px = 1U;
    s.corner_radius = 4U;
    return s;
}

static void reward_tooltip_text(const dialogue_reward_t *reward, char *buf, int cap) {
    if (!reward || !buf || cap <= 0) {
        return;
    }
    if (reward->amount > 1 && reward->kind == DIALOGUE_REWARD_ITEM) {
        (void)snprintf(buf, (size_t)cap, "%s x%d: %s", reward->name, reward->amount, reward->summary);
        return;
    }
    (void)snprintf(buf, (size_t)cap, "%s: %s", reward->name, reward->summary);
}

static nt_atlas_region_ref_t *dialogue_reward_icon_region(const dialogue_reward_t *reward) {
    if (!reward) {
        return NULL;
    }
    ensure_dialogue_regions();
    if (reward->kind == DIALOGUE_REWARD_XP) {
        return &s_dialogue_reward_xp_region;
    }
    if (reward->kind == DIALOGUE_REWARD_GOLD) {
        return &s_dialogue_reward_gold_region;
    }
    if (reward->kind == DIALOGUE_REWARD_UNLOCK) {
        return &s_dialogue_reward_unlock_region;
    }
    if (!reward->icon_asset_id) {
        return NULL;
    }
    for (int i = 0; i < DIALOGUE_REWARD_ICON_COUNT; ++i) {
        if (strcmp(reward->icon_asset_id, DIALOGUE_REWARD_ICON_ASSET_IDS[i]) == 0) {
            return &s_dialogue_reward_icon_regions[i];
        }
    }
    return NULL;
}

static void reward_icon_visual_ui(nt_ui_context_t *ctx, const dialogue_reward_t *reward, float size,
                                  const nt_ui_label_style_t *fallback_style) {
    nt_atlas_region_ref_t *region = dialogue_reward_icon_region(reward);
    if (region) {
        nt_ui_image_style_t icon_image = nt_ui_image_style_defaults();
        icon_image.color_packed = 0xFFFFFFFFU;
        CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)}}}) {
            nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_IMG), region, &icon_image, NULL);
        }
        return;
    }
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), reward ? reward->icon_label : "?", fallback_style);
}

static void reward_detail_modal_ui(nt_ui_context_t *ctx, bool portrait) {
    if (!s_reward_detail_open || !s_selected_reward) {
        if (s_reward_detail_was_open) {
            dialogue_request_reward_detail_cleanup();
        }
        s_reward_detail_was_open = false;
        s_reward_detail_dismiss_guard_frames = 0;
        return;
    }
    if (!s_reward_detail_was_open) {
        s_reward_detail_dismiss_guard_frames = 2;
        s_reward_detail_was_open = true;
    }

    bool detail_open = s_reward_detail_open;
    nt_ui_modal_style_t modal_style = reward_detail_modal_style();
    const bool ignore_close_request = s_reward_detail_dismiss_guard_frames > 0;
    if (!game_modal_visible(ctx, DIALOGUE_REWARD_DETAIL_MODAL_ID, &modal_style,
                            &detail_open, ignore_close_request)) {
        s_reward_detail_open = detail_open;
        if (!s_reward_detail_open) {
            dialogue_request_reward_detail_cleanup();
            s_selected_reward = NULL;
            s_reward_detail_was_open = false;
            s_reward_detail_dismiss_guard_frames = 0;
        }
        return;
    }

    const float panel_w = portrait ? 282.0F : 360.0F;
    const dialogue_reward_t *reward = s_selected_reward;
    const nt_ui_label_style_t title_style = make_label(22.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    const nt_ui_label_style_t icon_style = make_label(28.0F, 246.0F, 220.0F, 170.0F, 255.0F);
    const nt_ui_label_style_t body_style = make_label(16.0F, 236.0F, 211.0F, 166.0F, 255.0F);
    const nt_ui_label_style_t close_label = make_label(18.0F, 255.0F, 239.0F, 206.0F, 255.0F);
    nt_ui_button_style_t close_button = dialogue_close_button();

    CLAY({.id = CLAY_ID("dialogue/reward_detail_frame"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIT(0)},
                     .padding = CLAY_PADDING_ALL(14),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 12,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {49.0F, 31.0F, 22.0F, 255.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = {224.0F, 169.0F, 93.0F, 255.0F}, .width = {2, 2, 2, 2, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_ID("dialogue/reward_detail_head"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 12,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.id = CLAY_ID("dialogue/reward_detail_icon"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(62.0F), CLAY_SIZING_FIXED(62.0F)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = {27.0F, 22.0F, 18.0F, 255.0F},
                  .cornerRadius = CLAY_CORNER_RADIUS(4),
                  .border = {.color = {176.0F, 122.0F, 64.0F, 255.0F}, .width = {1, 1, 1, 1, 0}},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                reward_icon_visual_ui(ctx, reward, 52.0F, &icon_style);
            }
            CLAY({.id = CLAY_ID("dialogue/reward_detail_title"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 4,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), reward->name, &title_style);
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), reward->summary, &body_style);
            }
            if (game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_IMG, (nt_ui_layer_t)LAYER_TEXT,
                                        "dialogue/reward_detail_x", portrait)) {
                detail_open = false;
            }
        }

        CLAY({.id = CLAY_ID("dialogue/reward_detail_text"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .padding = {.left = 2, .right = 2, .top = 2, .bottom = 2}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), reward->detail, &body_style);
        }

        CLAY({.id = CLAY_ID("dialogue/reward_detail_close"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(42.0F)}}}) {
            const uint32_t close_id = nt_ui_id("dialogue/reward_detail_close/button");
            nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), close_id, &close_button,
                               &(Clay_ElementDeclaration){
                                   .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                              .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                                   .cornerRadius = CLAY_CORNER_RADIUS(4),
                                   .border = {.color = {176.0F, 122.0F, 64.0F, 255.0F}, .width = {1, 1, 1, 1, 0}}},
                               true, NULL);
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Закрыть", &close_label);
            if (nt_ui_button_end(ctx)) {
                detail_open = false;
            }
        }
    }
    nt_ui_modal_end(ctx);
    if (s_reward_detail_dismiss_guard_frames > 0) {
        --s_reward_detail_dismiss_guard_frames;
    }

    s_reward_detail_open = detail_open;
    if (!s_reward_detail_open) {
        dialogue_request_reward_detail_cleanup();
        s_selected_reward = NULL;
        s_reward_detail_was_open = false;
        s_reward_detail_dismiss_guard_frames = 0;
    }
}

static void reward_cell_ui(nt_ui_context_t *ctx, const char *scope, int slot, const dialogue_reward_t *reward, bool portrait,
                           const nt_ui_label_style_t *icon_style, const nt_ui_label_style_t *amount_style) {
    if (!reward) {
        return;
    }

    const float cell_size = portrait ? 50.0F : 44.0F;
    const uint32_t group_id = nt_ui_child_id(nt_ui_id("dialogue/reward_group"), scope);
    const uint32_t button_id = nt_ui_child_id(group_id, reward->id);
    nt_ui_button_style_t cell_button = dialogue_reward_button();
    char amount_buf[16];
    amount_buf[0] = '\0';
    if (reward->kind == DIALOGUE_REWARD_XP || reward->kind == DIALOGUE_REWARD_GOLD) {
        (void)snprintf(amount_buf, sizeof amount_buf, "+%d", reward->amount);
    } else if (reward->amount > 1) {
        (void)snprintf(amount_buf, sizeof amount_buf, "x%d", reward->amount);
    }

    CLAY({.id = CLAY_IDI("dialogue/reward_cell_box", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(cell_size), CLAY_SIZING_FIXED(cell_size)}},
          .backgroundColor = dialogue_underlay_reward(),
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), button_id, &cell_button,
                           &(Clay_ElementDeclaration){
                               .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                          .padding = CLAY_PADDING_ALL(4),
                                          .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                          .childGap = 2,
                                          .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                           true, NULL);
        CLAY({.id = CLAY_IDI("dialogue/reward_icon", slot),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {36.0F, 27.0F, 20.0F, 245.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(3),
              .border = {.color = {209.0F, 154.0F, 82.0F, 255.0F}, .width = {1, 1, 1, 1, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            reward_icon_visual_ui(ctx, reward, cell_size * 0.66F, icon_style);
        }
        if (amount_buf[0] != '\0') {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), amount_buf, amount_style);
        }
        if (nt_ui_button_end(ctx)) {
            s_selected_reward = reward;
            s_reward_detail_open = true;
        }
    }

    char tip[192];
    reward_tooltip_text(reward, tip, (int)sizeof tip);
    nt_ui_tooltip_style_t tip_style = reward_tooltip_style();
    (void)nt_ui_tooltip(ctx, NT_UI_DATA_LAYER(LAYER_IMG), LAYER_TEXT, button_id, tip, &tip_style);
}

static void speaker_reward_ui(nt_ui_context_t *ctx, const dialogue_quest_preview_t *preview, bool portrait,
                              const nt_ui_label_style_t *section_style, const nt_ui_label_style_t *icon_style,
                              const nt_ui_label_style_t *amount_style) {
    if (!preview || !preview->completion_rewards || preview->completion_reward_count <= 0) {
        return;
    }
    CLAY({.id = CLAY_ID("dialogue/speaker_reward"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .padding = {.left = 6, .right = 6, .top = 5, .bottom = 6},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 4,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {42.0F, 30.0F, 23.0F, 205.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {157.0F, 106.0F, 56.0F, 190.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "После выполнения", section_style);
        CLAY({.id = CLAY_ID("dialogue/speaker_reward_row"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = portrait ? 5 : 4,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            for (int i = 0; i < preview->completion_reward_count; ++i) {
                reward_cell_ui(ctx, "speaker_completion", 96 + i, &preview->completion_rewards[i], portrait, icon_style, amount_style);
            }
        }
    }
}

static void dialogue_grant_ui(nt_ui_context_t *ctx, const dialogue_quest_preview_t *preview, bool portrait,
                              const nt_ui_label_style_t *section_style, const nt_ui_label_style_t *icon_style,
                              const nt_ui_label_style_t *amount_style) {
    if (!preview || !preview->immediate_rewards || preview->immediate_reward_count <= 0) {
        return;
    }

    CLAY({.id = CLAY_ID("dialogue/grant_now"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .padding = {.left = 8, .right = 8, .top = 6, .bottom = 6},
                     .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                     .childGap = portrait ? 5 : 8,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {47.0F, 33.0F, 25.0F, 225.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {142.0F, 93.0F, 47.0F, 210.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.id = CLAY_ID("dialogue/grant_now_label"),
              .layout = {.sizing = {portrait ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(142.0F), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Страж выдаст сейчас", section_style);
        }
        CLAY({.id = CLAY_ID("dialogue/grant_now_row"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = portrait ? 5 : 6,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            for (int i = 0; i < preview->immediate_reward_count; ++i) {
                reward_cell_ui(ctx, "grant_now", 64 + i, &preview->immediate_rewards[i], portrait, icon_style, amount_style);
            }
        }
    }
}

static const char *dialogue_window_title(const dialogue_definition_t *definition, const dialogue_node_t *node) {
    if (definition && definition->quest_preview && node && node->quest_name) {
        return node->quest_name;
    }
    return definition ? definition->title : "";
}

static const char *speaker_bio_text(const dialogue_node_t *node) {
    if (node && node->speaker_id && strcmp(node->speaker_id, "gate_guard") == 0) {
        return "Старший у ворот. Не выпускает новичков без снаряжения и проверки.";
    }
    return "Персонаж, связанный с текущим заданием.";
}

static nt_atlas_region_ref_t *speaker_portrait_region(const dialogue_node_t *node) {
    if (!node || !node->speaker_id || strcmp(node->speaker_id, "gate_guard") != 0) {
        return NULL;
    }
    ensure_dialogue_regions();
    return &s_gate_guard_portrait_region;
}

static void task_section_ui(nt_ui_context_t *ctx, const char *objective, bool portrait, const nt_ui_label_style_t *section_style,
                            const nt_ui_label_style_t *objective_style) {
    if (!objective) {
        return;
    }
    ensure_dialogue_regions();
    nt_ui_image_style_t objective_panel_style = nt_ui_image_style_defaults();
    objective_panel_style.slice9_scale = portrait ? 0.42F : 0.50F;
    CLAY({.id = CLAY_ID("dialogue/task_section"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 98.0F : 70.0F)}},
          .backgroundColor = dialogue_underlay_objective(),
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &s_dialogue_header_plaque_region, &objective_panel_style,
                          &(Clay_ElementDeclaration){
                              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                         .padding = {.left = 11, .right = 12, .top = 9, .bottom = 9},
                                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                         .childGap = 9,
                                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
        CLAY({.id = CLAY_ID("dialogue/task_icon"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? 34.0F : 36.0F), CLAY_SIZING_FIXED(portrait ? 34.0F : 36.0F)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {229.0F, 169.0F, 41.0F, 255.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(18),
              .border = {.color = {117.0F, 50.0F, 24.0F, 255.0F}, .width = {2, 2, 2, 2, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "!", section_style);
        }
        CLAY({.id = CLAY_ID("dialogue/current_objective_column"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 3,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Цель", section_style);
            CLAY({.id = CLAY_ID("dialogue/current_objective"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .padding = {.left = 0, .right = 4, .top = 0, .bottom = 0},
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), objective, objective_style);
            }
        }
        nt_ui_panel_end(ctx);
    }
}

static bool dialogue_choices_ui(nt_ui_context_t *ctx, World *w, const dialogue_node_t *node, bool portrait,
                                const nt_ui_label_style_t *topic_label, const nt_ui_label_style_t *primary_label,
                                nt_ui_button_style_t *topic_button, nt_ui_button_style_t *primary_button) {
    if (!node) {
        return false;
    }
    int topic_count = 0;
    const dialogue_choice_t *primary = NULL;
    for (int i = 0; i < node->choice_count; ++i) {
        if (node->choices[i].kind == DIALOGUE_CHOICE_BRANCH) {
            ++topic_count;
        } else if (node->choices[i].kind == DIALOGUE_CHOICE_PROGRESS) {
            primary = &node->choices[i];
        }
    }
    if (topic_count <= 0 && !primary) {
        return false;
    }

    bool selected = false;
    CLAY({.id = CLAY_ID("dialogue/choices"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 7 : 8,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        if (topic_count > 0) {
            CLAY({.id = CLAY_ID("dialogue/topic_choices"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                             .childGap = portrait ? 7 : 10,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                for (int i = 0; i < node->choice_count; ++i) {
                    const dialogue_choice_t *choice = &node->choices[i];
                    if (choice->kind != DIALOGUE_CHOICE_BRANCH) {
                        continue;
                    }
                    CLAY({.id = CLAY_IDI("dialogue/topic_choice", i),
                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 38.0F : 30.0F)}},
                          .backgroundColor = dialogue_underlay_choice(),
                          .cornerRadius = CLAY_CORNER_RADIUS(4),
                          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                        const uint32_t button_id = nt_ui_id(choice->id);
                        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), button_id, topic_button,
                                           &(Clay_ElementDeclaration){
                                               .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                          .padding = {.left = 12, .right = 12, .top = 5, .bottom = 5},
                                                          .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                                           true, NULL);
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), choice->text, topic_label);
                        if (nt_ui_button_end(ctx)) {
                            (void)game_dialogue_select_choice(w, choice->id);
                            selected = true;
                        }
                    }
                    if (selected) {
                        break;
                    }
                }
            }
        }

        if (!selected && primary) {
            CLAY({.id = CLAY_ID("dialogue/primary_choice_row"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .padding = {.left = 0, .right = 0, .top = portrait ? 1 : 2, .bottom = 0},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                CLAY({.id = CLAY_ID("dialogue/primary_choice_inline"),
                      .layout = {.sizing = {portrait ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(340.0F),
                                            CLAY_SIZING_FIXED(portrait ? 42.0F : 38.0F)}},
                      .backgroundColor = dialogue_underlay_choice(),
                      .cornerRadius = CLAY_CORNER_RADIUS(4),
                      .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                    const uint32_t button_id = nt_ui_id(primary->id);
                    nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), button_id, primary_button,
                                       &(Clay_ElementDeclaration){
                                           .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                      .padding = {.left = 16, .right = 16, .top = 6, .bottom = 6},
                                                      .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                                       true, NULL);
                    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), primary->text, primary_label);
                    if (nt_ui_button_end(ctx)) {
                        (void)game_dialogue_select_choice(w, primary->id);
                        selected = true;
                    }
                }
            }
        }
    }
    return selected;
}

void dialogue_panel_ui(nt_ui_context_t *ctx, World *w) {
    if (!w || !w->dialogue.open || !w->dialogue.current_node) {
        if (s_dialogue_was_open || s_reward_detail_was_open || s_reward_detail_open) {
            dialogue_request_state_cleanup();
        }
        s_selected_reward = NULL;
        s_reward_detail_open = false;
        s_dialogue_was_open = false;
        s_dialogue_dismiss_guard_frames = 0;
        s_reward_detail_was_open = false;
        s_reward_detail_dismiss_guard_frames = 0;
        dialogue_clear_transient_ui_state(ctx);
        return;
    }
    dialogue_clear_transient_ui_state(ctx);
    if (!s_dialogue_was_open) {
        s_dialogue_dismiss_guard_frames = 2;
        s_dialogue_was_open = true;
    }
    const dialogue_node_t *node = w->dialogue.current_node;
    const dialogue_definition_t *definition = w->dialogue.definition;
    const dialogue_quest_preview_t *preview = definition ? definition->quest_preview : NULL;
    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    const bool portrait = layout_h > layout_w;
    const float panel_w = portrait ? clampf(layout_w - 28.0F, 292.0F, 520.0F)
                                   : clampf(layout_w * 0.84F, 660.0F, layout_w - 52.0F);
    const float panel_h = portrait ? clampf(layout_h * 0.94F, 390.0F, layout_h - 12.0F)
                                   : clampf(layout_h * 0.80F, 390.0F, layout_h - 8.0F);
    const float portrait_w = portrait ? panel_w - 56.0F : clampf(panel_w * 0.22F, 170.0F, 205.0F);
    const float portrait_badge_w = portrait ? 54.0F : portrait_w - 22.0F;
    const float portrait_badge_h = portrait ? 60.0F : 132.0F;

    nt_ui_modal_style_t modal_style = dialogue_modal_style();
    const bool ignore_close_request = s_dialogue_dismiss_guard_frames > 0;
    if (!game_modal_visible(ctx, DIALOGUE_MODAL_ID, &modal_style,
                            &w->dialogue.open, ignore_close_request)) {
        if (!w->dialogue.open) {
            dialogue_request_state_cleanup();
            s_dialogue_was_open = false;
            s_dialogue_dismiss_guard_frames = 0;
            dialogue_clear_transient_ui_state(ctx);
        }
        return;
    }

    ensure_dialogue_regions();
    const nt_ui_label_style_t title_style = make_label(portrait ? 20.0F : 22.0F, 255.0F, 240.0F, 205.0F, 255.0F);
    const nt_ui_label_style_t body_style = make_label(portrait ? 16.0F : 17.0F, 232.0F, 208.0F, 166.0F, 255.0F);
    const nt_ui_label_style_t portrait_style = make_label(portrait ? 17.0F : 18.0F, 246.0F, 220.0F, 170.0F, 255.0F);
    const nt_ui_label_style_t portrait_hint_style = make_label(portrait ? 11.0F : 12.0F, 218.0F, 188.0F, 136.0F, 255.0F);
    const nt_ui_label_style_t objective_style = make_label(portrait ? 15.0F : 16.0F, 255.0F, 229.0F, 184.0F, 255.0F);
    const nt_ui_label_style_t reward_section_style = make_label(14.0F, 245.0F, 190.0F, 94.0F, 255.0F);
    const nt_ui_label_style_t speaker_reward_style = make_label(portrait ? 12.0F : 13.0F, 238.0F, 205.0F, 145.0F, 255.0F);
    const nt_ui_label_style_t reward_icon_style = make_label(portrait ? 16.0F : 18.0F, 248.0F, 226.0F, 184.0F, 255.0F);
    const nt_ui_label_style_t reward_amount_style = make_label(12.0F, 255.0F, 230.0F, 182.0F, 255.0F);
    const nt_ui_label_style_t choice_label = make_label(portrait ? 15.0F : 16.0F, 232.0F, 208.0F, 166.0F, 255.0F);
    const nt_ui_label_style_t primary_label = make_label(portrait ? 16.0F : 17.0F, 255.0F, 239.0F, 206.0F, 255.0F);
    nt_ui_button_style_t topic_button = dialogue_topic_button();
    nt_ui_button_style_t primary_button = dialogue_primary_button();
    nt_ui_scroll_style_t scroll_style = dialogue_scroll_style();
    nt_ui_image_style_t outer_frame_image = nt_ui_image_style_defaults();
    outer_frame_image.slice9_scale = portrait ? 0.42F : 0.50F;
    nt_ui_image_style_t body_panel_image = nt_ui_image_style_defaults();
    body_panel_image.slice9_scale = portrait ? 0.42F : 0.50F;
    nt_ui_image_style_t header_plaque_image = nt_ui_image_style_defaults();
    header_plaque_image.slice9_scale = portrait ? 0.44F : 0.52F;
    nt_ui_image_style_t portrait_frame_image = nt_ui_image_style_defaults();
    portrait_frame_image.slice9_scale = portrait ? 0.40F : 0.48F;
    nt_ui_image_style_t choice_panel_image = nt_ui_image_style_defaults();
    choice_panel_image.slice9_scale = portrait ? 0.34F : 0.40F;

    CLAY({.id = CLAY_ID("dialogue/modal_frame"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIXED(panel_h)}},
          .backgroundColor = dialogue_underlay_outer(),
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &s_dialogue_outer_frame_region, &outer_frame_image,
                          &(Clay_ElementDeclaration){
                              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                         .padding = {.left = portrait ? 12 : 14,
                                                     .right = portrait ? 12 : 14,
                                                     .top = portrait ? 10 : 12,
                                                     .bottom = portrait ? 12 : 14},
                                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                         .childGap = portrait ? 5 : 6,
                                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
        CLAY({.id = CLAY_ID("dialogue/title_bar"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 36.0F : 40.0F)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 8,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = dialogue_underlay_header(),
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &s_dialogue_header_plaque_region, &header_plaque_image,
                              &(Clay_ElementDeclaration){
                                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                             .padding = {.left = 18, .right = 8, .top = 5, .bottom = 5},
                                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                             .childGap = 8,
                                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}});
            CLAY({.id = CLAY_ID("dialogue/title_text"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), dialogue_window_title(definition, node), &title_style);
            }
            if (game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_IMG, (nt_ui_layer_t)LAYER_TEXT,
                                        "dialogue/close", portrait)) {
                w->dialogue.open = false;
            }
            nt_ui_panel_end(ctx);
        }

        CLAY({.id = CLAY_ID("dialogue/parchment"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
              .backgroundColor = dialogue_underlay_body(),
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &s_dialogue_body_panel_region, &body_panel_image,
                              &(Clay_ElementDeclaration){
                                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                             .padding = CLAY_PADDING_ALL(portrait ? 9 : 11),
                                             .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                                             .childGap = portrait ? 8 : 10,
                                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
            CLAY({.id = CLAY_ID("dialogue/portrait_column"),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(portrait_w), CLAY_SIZING_FIT(0)},
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = {22.0F, 24.0F, 25.0F, 235.0F},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), &s_dialogue_portrait_frame_region, &portrait_frame_image,
                                  &(Clay_ElementDeclaration){
                                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                 .padding = CLAY_PADDING_ALL(portrait ? 8 : 9),
                                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                 .childGap = portrait ? 7 : 8,
                                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}});
                CLAY({.id = CLAY_ID("dialogue/portrait_summary"),
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .layoutDirection = portrait ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM,
                                 .childGap = portrait ? 10 : 8,
                                 .childAlignment = {portrait ? CLAY_ALIGN_X_LEFT : CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                    CLAY({.id = CLAY_ID("dialogue/portrait_badge"),
                          .layout = {.sizing = {CLAY_SIZING_FIXED(portrait_badge_w),
                                                CLAY_SIZING_FIXED(portrait_badge_h)},
                                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                          .backgroundColor = {28.0F, 23.0F, 20.0F, 255.0F},
                          .cornerRadius = CLAY_CORNER_RADIUS(4),
                          .border = {.color = {178.0F, 126.0F, 66.0F, 255.0F}, .width = {1, 1, 1, 1, 0}},
                          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                        nt_atlas_region_ref_t *portrait_region = speaker_portrait_region(node);
                        if (portrait_region) {
                            nt_ui_image_style_t portrait_image = nt_ui_image_style_defaults();
                            portrait_image.color_packed = 0xFFFFFFFFU;
                            nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_IMG), portrait_region, &portrait_image, NULL);
                        } else {
                            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), portrait ? "NPC" : "СТРАЖ", &portrait_style);
                        }
                    }
                    if (portrait) {
                        CLAY({.id = CLAY_ID("dialogue/portrait_identity"),
                              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                         .childGap = 4,
                                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), node->speaker_name, &portrait_style);
                            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), speaker_bio_text(node), &portrait_hint_style);
                        }
                    } else {
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), node->speaker_name, &portrait_style);
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), speaker_bio_text(node), &portrait_hint_style);
                    }
                }
                speaker_reward_ui(ctx, preview, portrait, &speaker_reward_style, &reward_icon_style, &reward_amount_style);
                nt_ui_panel_end(ctx);
            }

            CLAY({.id = CLAY_ID("dialogue/content_column"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .padding = {0, 0, 0, 0},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 6,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                const uint32_t scroll_id = nt_ui_id("dialogue/content_scroll");
                nt_ui_scroll_begin(ctx, NULL, scroll_id, &scroll_style,
                                   &(Clay_ElementDeclaration){
                                       .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                                  .padding = {.left = 0, .right = 13, .top = 0, .bottom = 2}},
                                       .cornerRadius = CLAY_CORNER_RADIUS(4)});
                CLAY({.id = CLAY_ID("dialogue/content_inner"),
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                 .childGap = portrait ? 6 : 7,
                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                    CLAY({.id = CLAY_ID("dialogue/body_text"),
                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                     .padding = {.left = 2, .right = 6, .top = 1, .bottom = 3}}}) {
                        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), node->text, &body_style);
                    }

                    dialogue_grant_ui(ctx, preview, portrait, &reward_section_style, &reward_icon_style, &reward_amount_style);

                    const char *objective = preview && preview->goal ? preview->goal : game_dialogue_current_objective(w);
                    task_section_ui(ctx, objective, portrait, &reward_section_style, &objective_style);
                }
                nt_ui_scroll_end(ctx);

                CLAY({.id = CLAY_ID("dialogue/choice_tray"),
                      .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
                      .backgroundColor = dialogue_underlay_choice(),
                      .cornerRadius = CLAY_CORNER_RADIUS(4),
                      .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &s_dialogue_body_panel_region, &choice_panel_image,
                                      &(Clay_ElementDeclaration){
                                          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                                     .padding = {.left = portrait ? 9 : 10,
                                                                 .right = portrait ? 9 : 10,
                                                                 .top = portrait ? 7 : 7,
                                                                 .bottom = portrait ? 7 : 7},
                                                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                                     .childGap = 0,
                                                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
                    (void)dialogue_choices_ui(ctx, w, node, portrait, &choice_label, &primary_label, &topic_button, &primary_button);
                    nt_ui_panel_end(ctx);
                }
            }
            nt_ui_panel_end(ctx);
        }
        nt_ui_panel_end(ctx);
    }
    reward_detail_modal_ui(ctx, portrait);
    nt_ui_modal_end(ctx);
    if (s_dialogue_dismiss_guard_frames > 0) {
        --s_dialogue_dismiss_guard_frames;
    }
    if (!w->dialogue.open) {
        dialogue_request_state_cleanup();
        s_dialogue_was_open = false;
        s_dialogue_dismiss_guard_frames = 0;
    }
    dialogue_clear_transient_ui_state(ctx);
}
