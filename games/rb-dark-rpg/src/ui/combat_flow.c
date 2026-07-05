#include "ui/combat_flow.h"

#include "clay.h"
#include "game_audio.h"
#include "game_actions.h"
#include "game_content.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/equipment_screen.h"
#include "ui/game_modal.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LAYER_COMBAT_SCRIM 40
#define LAYER_COMBAT_BG 41
#define LAYER_COMBAT_FILL 42
#define LAYER_COMBAT_TEXT_SHADOW 43
#define LAYER_COMBAT_TEXT 44
#define COMBAT_MODAL_ID 0xC0BA7001U
#define COMBAT_MODAL_CONTROL_Z (NT_UI_MODAL_ZBAND_STRIDE + 5)
#define COMBAT_VISUAL_EVENT_LEAD_SECONDS 0.38F
#define COMBAT_VISUAL_EVENT_TAIL_SECONDS 0.46F

#if defined(__GNUC__) || defined(__clang__)
#define COMBAT_UNUSED_FN __attribute__((unused))
#else
#define COMBAT_UNUSED_FN
#endif

typedef struct combat_actor_pose_t {
    float x;
    float y;
    float scale_x;
    float scale_y;
    bool flash;
    bool block_flash;
    bool crit_flash;
    bool attacking;
} combat_actor_pose_t;

typedef enum combat_actor_art_t {
    COMBAT_ACTOR_ART_HERO,
    COMBAT_ACTOR_ART_GATE_SCAVENGER,
    COMBAT_ACTOR_ART_MILL_SCAVENGER,
    COMBAT_ACTOR_ART_COUNT,
} combat_actor_art_t;

typedef enum combat_reward_icon_art_t {
    COMBAT_REWARD_ICON_OLD_SWORD = 0,
    COMBAT_REWARD_ICON_PADDED_JACKET,
    COMBAT_REWARD_ICON_LEATHER_GREAVES,
    COMBAT_REWARD_ICON_IRON_SWORD,
    COMBAT_REWARD_ICON_PATCHED_MAIL,
    COMBAT_REWARD_ICON_GUARD_COAT,
    COMBAT_REWARD_ICON_IRON_GREAVES,
    COMBAT_REWARD_ICON_MILITIA_AXE,
    COMBAT_REWARD_ICON_RUNNER_WRAPS,
    COMBAT_REWARD_ICON_BLACK_SUN_CHARM,
    COMBAT_REWARD_ICON_MILLER_HOOK,
    COMBAT_REWARD_ICON_CHAIN_PATCHES,
    COMBAT_REWARD_ICON_SCAVENGER_KNEE_PLATES,
    COMBAT_REWARD_ICON_DRAGON_ASH_TOKEN,
    COMBAT_REWARD_ICON_MILLER_LUCKY_NAIL,
    COMBAT_REWARD_ICON_SEEKER_TOKEN,
    COMBAT_REWARD_ICON_GRAIN_SACKS,
    COMBAT_REWARD_ICON_CONTRACT_PROGRESS,
    COMBAT_REWARD_ICON_CLUE_FRAGMENT,
    COMBAT_REWARD_ICON_BURNED_CHAIN_BRACKET,
    COMBAT_REWARD_ICON_ORDER_SCRAP,
    COMBAT_REWARD_ICON_COUNT,
} combat_reward_icon_art_t;

static const char *FIRST_ENCOUNTER_STEP_ID = "clear_gate_scavenger";
static const char *FIRST_ENCOUNTER_FLAG_ID = "gate_scavenger_defeated";
static const char *FIRST_ENCOUNTER_ID = "gate_scavenger";

static nt_resource_t s_combat_actor_atlas;
static nt_atlas_region_ref_t s_combat_actor_regions[COMBAT_ACTOR_ART_COUNT];
static const nt_hash64_t COMBAT_ACTOR_REGION_HASHES[COMBAT_ACTOR_ART_COUNT] = {
    ASSET_ATLAS_REGION_COMBAT_ACTORS_COMBAT_ACTOR_HERO,
    ASSET_ATLAS_REGION_COMBAT_ACTORS_COMBAT_ACTOR_GATE_SCAVENGER,
    ASSET_ATLAS_REGION_COMBAT_ACTORS_COMBAT_ACTOR_MILL_SCAVENGER,
};

static nt_resource_t s_combat_reward_atlas;
static nt_atlas_region_ref_t s_combat_reward_icon_regions[COMBAT_REWARD_ICON_COUNT];
static nt_atlas_region_ref_t s_combat_reward_xp_region;
static nt_atlas_region_ref_t s_combat_reward_gold_region;

static const char *COMBAT_REWARD_ICON_ASSET_IDS[COMBAT_REWARD_ICON_COUNT] = {
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

static const nt_hash64_t COMBAT_REWARD_ICON_REGION_HASHES[COMBAT_REWARD_ICON_COUNT] = {
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

static const nt_ui_widget_def_t COMBAT_BUTTON_DEF = {
    .name = "combat_button",
    .pill_color = 0xFFE2A75CU,
};

static bool s_cleanup_pending;

static void combat_request_state_cleanup(void) { s_cleanup_pending = true; }

static void combat_clear_transient_ui_state(nt_ui_context_t *ctx) {
    if (!ctx || !s_cleanup_pending) {
        return;
    }
    game_modal_clear_state(ctx, COMBAT_MODAL_ID);
    s_cleanup_pending = false;
}

static Clay_ElementId clay_id_from_text(const char *id_text) {
    return Clay_GetElementId((Clay_String){
        .isStaticallyAllocated = false,
        .length = (int32_t)strlen(id_text),
        .chars = id_text,
    });
}

static nt_ui_label_style_t label_style(float font_size, float r, float g, float b, float a) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = font_size, .color = {r, g, b, a}};
}

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static COMBAT_UNUSED_FN float lerpf(float from, float to, float t) {
    return from + (to - from) * clampf(t, 0.0F, 1.0F);
}

static COMBAT_UNUSED_FN float ease01(float t) {
    t = clampf(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
}

static bool list_contains(const char list[][GAME_STATE_STRING_MAX], int count, const char *id) {
    if (!id) {
        return false;
    }
    for (int i = 0; i < count; ++i) {
        if (strcmp(list[i], id) == 0) {
            return true;
        }
    }
    return false;
}

static const GameQuestState *find_quest(const GameState *state, const char *quest_id) {
    if (!state || !quest_id) {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
        const GameQuestState *quest = &state->quests_quest_states[i];
        if (quest->used && strcmp(quest->key, quest_id) == 0) {
            return quest;
        }
    }
    return NULL;
}

static unsigned int encounter_seed(const char *encounter_id) {
    unsigned int hash = 2166136261U;
    if (!encounter_id) {
        return hash;
    }
    for (const unsigned char *p = (const unsigned char *)encounter_id; *p; ++p) {
        hash ^= (unsigned int)*p;
        hash *= 16777619U;
    }
    return hash;
}

static void ensure_combat_actor_regions(void) {
    if (s_combat_actor_atlas.id != 0U) {
        return;
    }
    s_combat_actor_atlas = nt_resource_request(ASSET_ATLAS_COMBAT_ACTORS, NT_ASSET_ATLAS);
    for (int i = 0; i < COMBAT_ACTOR_ART_COUNT; ++i) {
        s_combat_actor_regions[i] = nt_atlas_ref(s_combat_actor_atlas, COMBAT_ACTOR_REGION_HASHES[i].value);
    }
}

static void ensure_combat_reward_regions(void) {
    if (s_combat_reward_atlas.id != 0U) {
        return;
    }
    s_combat_reward_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    s_combat_reward_xp_region = nt_atlas_ref(s_combat_reward_atlas, ASSET_ATLAS_REGION_UI_ASSET_REWARD_XP.value);
    s_combat_reward_gold_region = nt_atlas_ref(s_combat_reward_atlas, ASSET_ATLAS_REGION_UI_GOLD_COIN_HUD.value);
    for (int i = 0; i < COMBAT_REWARD_ICON_COUNT; ++i) {
        s_combat_reward_icon_regions[i] = nt_atlas_ref(s_combat_reward_atlas, COMBAT_REWARD_ICON_REGION_HASHES[i].value);
    }
}

static combat_actor_art_t combat_enemy_art_for_encounter(const char *encounter_id) {
    if (encounter_id &&
        (strstr(encounter_id, "mill") != NULL || strstr(encounter_id, "night") != NULL)) {
        /* The mill brute and the night-assault boss use the heavier silhouette so the
         * finale reads as a real threat instead of the caged gate scavenger. */
        return COMBAT_ACTOR_ART_MILL_SCAVENGER;
    }
    return COMBAT_ACTOR_ART_GATE_SCAVENGER;
}

static void shadowed_label(nt_ui_context_t *ctx, int slot, const char *text, const nt_ui_label_style_t *style) {
    nt_ui_label_style_t shadow = *style;
    shadow.color = (Clay_Color){7.0F, 4.0F, 2.0F, 190.0F};

    CLAY({.id = CLAY_IDI("combat/shadow_label", slot),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        CLAY({.id = CLAY_IDI("combat/shadow_label_shadow", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                           .offset = {1.0F, 1.0F}},
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT_SHADOW), text, &shadow);
        }
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), text, style);
    }
}

static void stat_line(nt_ui_context_t *ctx, int slot, const char *text) {
    const nt_ui_label_style_t style = label_style(14.0F, 226.0F, 207.0F, 172.0F, 255.0F);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), text, &style);
    (void)slot;
}

typedef struct combat_reward_visual_t {
    nt_atlas_region_ref_t *region;
    char amount[16];
    const char *fallback_label;
} combat_reward_visual_t;

static nt_atlas_region_ref_t *combat_item_icon_region(const game_item_definition_t *item) {
    if (!item || !item->icon_asset_id) {
        return NULL;
    }
    ensure_combat_reward_regions();
    for (int i = 0; i < COMBAT_REWARD_ICON_COUNT; ++i) {
        if (strcmp(item->icon_asset_id, COMBAT_REWARD_ICON_ASSET_IDS[i]) == 0) {
            return &s_combat_reward_icon_regions[i];
        }
    }
    return NULL;
}

static void combat_reward_cell_ui(nt_ui_context_t *ctx, int slot, const combat_reward_visual_t *reward, bool portrait) {
    const float cell_size = portrait ? 56.0F : 58.0F;
    const float icon_size = portrait ? 35.0F : 38.0F;
    const nt_ui_label_style_t fallback = label_style(15.0F, 255.0F, 232.0F, 188.0F, 255.0F);
    const nt_ui_label_style_t amount = label_style(12.0F, 255.0F, 232.0F, 188.0F, 255.0F);

    CLAY({.id = CLAY_IDI("combat/reward_cell", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(cell_size), CLAY_SIZING_FIXED(cell_size)},
                     .padding = {.left = 5, .right = 5, .top = 5, .bottom = 4},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 2,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {34.0F, 25.0F, 18.0F, 232.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {167.0F, 112.0F, 58.0F, 222.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
        CLAY({.id = CLAY_IDI("combat/reward_icon", slot),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {19.0F, 15.0F, 12.0F, 210.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(3),
              .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_FILL)}) {
            if (reward->region) {
                nt_ui_image_style_t image_style = nt_ui_image_style_defaults();
                image_style.color_packed = 0xFFFFFFFFU;
                CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(icon_size), CLAY_SIZING_FIXED(icon_size)}}}) {
                    nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), reward->region, &image_style, NULL);
                }
            } else {
                shadowed_label(ctx, 700 + slot, reward->fallback_label ? reward->fallback_label : "?", &fallback);
            }
        }
        if (reward->amount[0] != '\0') {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), reward->amount, &amount);
        }
    }
}

static void combat_rewards_ui(nt_ui_context_t *ctx, const game_encounter_definition_t *encounter, bool portrait,
                              int id_base, bool show_title) {
    if (!encounter) {
        return;
    }
    ensure_combat_reward_regions();

    combat_reward_visual_t rewards[GAME_CONTENT_MAX_ENCOUNTER_REWARD_ITEMS + 2];
    int count = 0;
    if (encounter->reward_xp > 0) {
        rewards[count].region = &s_combat_reward_xp_region;
        (void)snprintf(rewards[count].amount, sizeof rewards[count].amount, "+%d", encounter->reward_xp);
        rewards[count].fallback_label = "XP";
        count += 1;
    }
    if (encounter->reward_gold > 0) {
        rewards[count].region = &s_combat_reward_gold_region;
        (void)snprintf(rewards[count].amount, sizeof rewards[count].amount, "+%d", encounter->reward_gold);
        rewards[count].fallback_label = "G";
        count += 1;
    }
    for (int i = 0; i < encounter->reward_item_count && count < (int)(sizeof rewards / sizeof rewards[0]); ++i) {
        const game_item_definition_t *item = game_content_find_item(encounter->reward_items[i]);
        rewards[count].region = combat_item_icon_region(item);
        rewards[count].amount[0] = '\0';
        rewards[count].fallback_label = "?";
        count += 1;
    }
    if (count <= 0) {
        return;
    }

    const nt_ui_label_style_t title = label_style(14.0F, 241.0F, 211.0F, 153.0F, 255.0F);
    const int per_row = portrait ? 3 : 6;
    CLAY({.id = CLAY_IDI("combat/rewards_panel", id_base),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 6,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        if (show_title) {
            shadowed_label(ctx, id_base, "Награда", &title);
        }
        for (int start = 0; start < count; start += per_row) {
            CLAY({.id = CLAY_IDI("combat/rewards_row", id_base + start),
                  .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 7,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                const int end = start + per_row < count ? start + per_row : count;
                for (int i = start; i < end; ++i) {
                    combat_reward_cell_ui(ctx, id_base + i, &rewards[i], portrait);
                }
            }
        }
    }
}

static const char *threat_display_name(const char *threat) {
    if (!threat) {
        return "Легко";
    }
    if (strcmp(threat, "fair") == 0) {
        return "Ровно";
    }
    if (strcmp(threat, "risky") == 0) {
        return "Риск";
    }
    if (strcmp(threat, "deadly") == 0) {
        return "Смертельно";
    }
    return "Легко";
}

static const char *threat_reason_line(const char *threat) {
    if (!threat || strcmp(threat, "easy") == 0) {
        return "Твоего снаряжения хватает для этой проверки.";
    }
    if (strcmp(threat, "fair") == 0) {
        return "Победа вероятна, но HP лучше проверить заранее.";
    }
    if (strcmp(threat, "risky") == 0) {
        return "Враг может пережить несколько твоих ударов.";
    }
    return "Сначала стоит усилиться или восстановиться.";
}

static bool combat_button(nt_ui_context_t *ctx, const char *id_text, const char *text, bool primary, float width) {
    const Clay_ElementId clay_id = clay_id_from_text(id_text);
    const uint32_t id = clay_id.id;
    const int16_t hit_pad[4] = {8, 8, 8, 8};
    nt_ui_widget_register(ctx, id, &COMBAT_BUTTON_DEF, hit_pad, true);
    const nt_ui_events_t events = nt_ui_events_padded(ctx, id, NULL, hit_pad);
    const bool hot = events.hovered || events.held;
    const Clay_Color border = primary ? (Clay_Color){225.0F, 166.0F, 84.0F, hot ? 255.0F : 245.0F}
                                      : (Clay_Color){128.0F, 94.0F, 57.0F, hot ? 242.0F : 220.0F};
    Clay_Color bg;
    if (events.held) {
        bg = primary ? (Clay_Color){28.0F, 58.0F, 91.0F, 244.0F} : (Clay_Color){21.0F, 25.0F, 31.0F, 235.0F};
    } else if (hot) {
        bg = primary ? (Clay_Color){39.0F, 85.0F, 133.0F, 244.0F} : (Clay_Color){30.0F, 38.0F, 49.0F, 235.0F};
    } else {
        bg = primary ? (Clay_Color){31.0F, 70.0F, 113.0F, 238.0F} : (Clay_Color){22.0F, 27.0F, 35.0F, 228.0F};
    }
    const nt_ui_label_style_t text_style =
        primary ? label_style(17.0F, 255.0F, 240.0F, 207.0F, 255.0F) : label_style(16.0F, 230.0F, 211.0F, 176.0F, 255.0F);

    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(42.0F)}}}) {
        CLAY({.id = clay_id,
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .zIndex = COMBAT_MODAL_CONTROL_Z},
              .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(42.0F)},
                         .padding = {.left = 14, .right = 14, .top = 6, .bottom = 6},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = bg,
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .border = {.color = border, .width = {1, 1, 1, 1, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
            shadowed_label(ctx, (int)(id & 0x7FFFU), text, &text_style);
        }
    }
    return events.clicked;
}

static void hp_bar(nt_ui_context_t *ctx, int slot, const char *label, float ratio, Clay_Color fill_color, float width) {
    const nt_ui_label_style_t label_s = label_style(12.0F, 255.0F, 238.0F, 210.0F, 255.0F);
    ratio = clampf(ratio, 0.0F, 1.0F);
    CLAY({.id = CLAY_IDI("combat/hp_bar", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(24.0F)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {18.0F, 13.0F, 10.0F, 235.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(3),
          .border = {.color = {106.0F, 77.0F, 48.0F, 230.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
        const float fill_w = (width - 8.0F) * ratio;
        if (fill_w > 1.0F) {
            CLAY({.id = CLAY_IDI("combat/hp_bar_fill", slot),
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_CENTER, .parent = CLAY_ATTACH_POINT_LEFT_CENTER},
                               .offset = {4.0F, 0.0F}},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(fill_w), CLAY_SIZING_FIXED(14.0F)}},
                  .backgroundColor = fill_color,
                  .cornerRadius = CLAY_CORNER_RADIUS(2),
                  .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_FILL)}) {}
        }
        shadowed_label(ctx, 80 + slot, label, &label_s);
    }
}

static void attack_cue(nt_ui_context_t *ctx, int slot, float progress, Clay_Color fill_color, float width) {
    (void)ctx;
    progress = clampf(progress, 0.0F, 1.0F);
    const float marker_w = 18.0F;
    const float travel = width > marker_w ? width - marker_w : 1.0F;
    const float marker_x = -travel * 0.5F + travel * progress;
    CLAY({.id = CLAY_IDI("combat/attack_cue", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(10.0F)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_IDI("combat/attack_rail", slot),
              .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(3.0F)}},
              .backgroundColor = {67.0F, 51.0F, 34.0F, 126.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(2),
              .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
            CLAY({.id = CLAY_IDI("combat/attack_marker", slot),
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                                .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                               .offset = {marker_x, 0.0F},
                               .zIndex = COMBAT_MODAL_CONTROL_Z + 2},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(marker_w), CLAY_SIZING_FIXED(7.0F)}},
                  .backgroundColor = fill_color,
                  .cornerRadius = CLAY_CORNER_RADIUS(3),
                  .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_FILL)}) {}
        }
    }
}

static void damage_badge(nt_ui_context_t *ctx, int slot, const char *text, Clay_Color bg, float width, float font_size) {
    const nt_ui_label_style_t label = label_style(font_size, 255.0F, 232.0F, 188.0F, 255.0F);
    CLAY({.id = CLAY_IDI("combat/damage_badge", slot),
          .layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(36.0F)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = bg,
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {215.0F, 142.0F, 72.0F, 205.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
        shadowed_label(ctx, 60 + slot, text, &label);
    }
}

static void enemy_portrait_ui(nt_ui_context_t *ctx, combat_actor_art_t art, bool portrait) {
    ensure_combat_actor_regions();
    const float size = portrait ? 66.0F : 78.0F;
    const float img_h = size - 6.0F;
    const float img_w = img_h * (512.0F / 704.0F);
    nt_ui_image_style_t sprite_style = nt_ui_image_style_defaults();
    sprite_style.color_packed = 0xFFFFFFFFU;
    CLAY({.id = CLAY_ID("combat/enemy_portrait"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {24.0F, 17.0F, 17.0F, 218.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .border = {.color = {128.0F, 67.0F, 45.0F, 190.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
        CLAY({.id = CLAY_ID("combat/enemy_portrait_image"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(img_w), CLAY_SIZING_FIXED(img_h)}}}) {
            nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_FILL), &s_combat_actor_regions[art], &sprite_style, NULL);
        }
    }
}

static int latest_combat_event_index(const game_combat_result_t *result, float timeline_time) {
    if (!result) {
        return -1;
    }
    int latest = -1;
    for (int i = 0; i < result->event_count; ++i) {
        if (result->events[i].time_seconds <= timeline_time) {
            latest = i;
        } else {
            break;
        }
    }
    return latest;
}

static float actor_charge_progress(const game_combat_result_t *result, game_combat_actor_t actor, float timeline_time) {
    if (!result || actor == GAME_COMBAT_ACTOR_NONE) {
        return 0.0F;
    }
    float previous_time = 0.0F;
    float next_time = result->duration_seconds;
    bool found_next = false;
    for (int i = 0; i < result->event_count; ++i) {
        const game_combat_event_t *event = &result->events[i];
        if (event->actor != actor) {
            continue;
        }
        if (event->time_seconds <= timeline_time) {
            previous_time = event->time_seconds;
        } else {
            next_time = event->time_seconds;
            found_next = true;
            break;
        }
    }
    if (!found_next && timeline_time >= result->duration_seconds) {
        return 1.0F;
    }
    const float span = next_time - previous_time;
    if (span <= 0.01F) {
        return 1.0F;
    }
    return clampf((timeline_time - previous_time) / span, 0.0F, 1.0F);
}

static COMBAT_UNUSED_FN const char *combat_event_mark(const game_combat_result_t *result, int latest_event,
                                                      float progress) {
    if (!result || latest_event < 0) {
        return "ГОТОВ";
    }
    if (progress >= 1.0F) {
        return "ИТОГ";
    }
    const game_combat_event_t *event = &result->events[latest_event];
    if (event->crit) {
        return "КРИТ";
    }
    if (event->block) {
        return "БЛОК";
    }
    return event->actor == GAME_COMBAT_ACTOR_PLAYER ? "УДАР" : "ОТВЕТ";
}

static void combat_event_log_line(const game_combat_event_t *event, char *buffer, size_t buffer_size) {
    if (!event || !buffer || buffer_size == 0U) {
        return;
    }
    const char *actor = event->actor == GAME_COMBAT_ACTOR_PLAYER ? "Ты" : "Враг";
    const char *suffix = event->crit ? " крит" : event->block ? " блок" : "";
    (void)snprintf(buffer, buffer_size, "%s: -%d%s", actor, event->damage, suffix);
}

static COMBAT_UNUSED_FN int active_combat_visual_event_index(const game_combat_result_t *result, float timeline_time) {
    if (!result) {
        return -1;
    }
    int active = -1;
    float best_distance = 9999.0F;
    for (int i = 0; i < result->event_count; ++i) {
        const float event_time = result->events[i].time_seconds;
        if (timeline_time < event_time - COMBAT_VISUAL_EVENT_LEAD_SECONDS ||
            timeline_time > event_time + COMBAT_VISUAL_EVENT_TAIL_SECONDS) {
            continue;
        }
        float distance = timeline_time - event_time;
        if (distance < 0.0F) {
            distance = -distance;
        }
        if (distance < best_distance) {
            active = i;
            best_distance = distance;
        }
    }
    return active;
}

static float combat_visual_event_progress(const game_combat_event_t *event, float timeline_time) {
    if (!event) {
        return 0.0F;
    }
    const float start = event->time_seconds - COMBAT_VISUAL_EVENT_LEAD_SECONDS;
    const float span = COMBAT_VISUAL_EVENT_LEAD_SECONDS + COMBAT_VISUAL_EVENT_TAIL_SECONDS;
    if (span <= 0.01F) {
        return 1.0F;
    }
    return clampf((timeline_time - start) / span, 0.0F, 1.0F);
}

static combat_actor_pose_t combat_actor_pose(game_combat_actor_t actor, const game_combat_event_t *event,
                                             float visual_progress, bool portrait) {
    combat_actor_pose_t pose = {
        .x = 0.0F,
        .y = 0.0F,
        .scale_x = 1.0F,
        .scale_y = 1.0F,
        .flash = false,
        .block_flash = false,
        .crit_flash = false,
        .attacking = false,
    };
    if (!event || actor == GAME_COMBAT_ACTOR_NONE) {
        return pose;
    }

    const float p = clampf(visual_progress, 0.0F, 1.0F);
    const float toward_center = actor == GAME_COMBAT_ACTOR_PLAYER ? 1.0F : -1.0F;
    const float away_from_center = actor == GAME_COMBAT_ACTOR_PLAYER ? -1.0F : 1.0F;
    const float hit_power = event->crit ? 1.24F : (event->block ? 0.78F : 1.0F);
    const float windup = portrait ? 8.0F : 12.0F;
    const float lunge = (portrait ? 30.0F : 42.0F) * hit_power;
    const float rebound = portrait ? 8.0F : 12.0F;
    const float recoil = (portrait ? 18.0F : 26.0F) * hit_power;

    if (event->actor == actor) {
        pose.attacking = p >= 0.30F && p <= 0.70F;
        pose.crit_flash = event->crit && p >= 0.38F && p <= 0.62F;
        if (p < 0.30F) {
            const float t = ease01(p / 0.30F);
            pose.x = -toward_center * windup * t;
            pose.scale_y = lerpf(1.0F, 0.96F, t);
        } else if (p < 0.50F) {
            const float t = ease01((p - 0.30F) / 0.20F);
            pose.x = toward_center * lerpf(-windup, lunge, t);
            pose.y = lerpf(0.0F, -4.0F, t);
            pose.scale_x = lerpf(0.98F, 1.10F, t);
            pose.scale_y = lerpf(1.03F, 0.95F, t);
        } else if (p < 0.72F) {
            const float t = ease01((p - 0.50F) / 0.22F);
            pose.x = toward_center * lerpf(lunge, rebound, t);
            pose.y = lerpf(-4.0F, 0.0F, t);
            pose.scale_x = lerpf(1.10F, 1.02F, t);
            pose.scale_y = lerpf(0.95F, 1.0F, t);
        } else {
            const float t = ease01((p - 0.72F) / 0.28F);
            pose.x = toward_center * lerpf(rebound, 0.0F, t);
        }
        return pose;
    }

    if (p >= 0.42F && p < 0.68F) {
        const float t = ease01((p - 0.42F) / 0.26F);
        pose.x = away_from_center * lerpf(0.0F, recoil, t);
        pose.y = lerpf(0.0F, event->block ? 1.0F : 3.0F, t);
        pose.scale_x = event->block ? lerpf(1.0F, 0.98F, t) : lerpf(1.0F, 1.08F, t);
        pose.scale_y = event->block ? lerpf(1.0F, 1.04F, t) : lerpf(1.0F, 0.94F, t);
        pose.flash = !event->block;
        pose.block_flash = event->block;
        pose.crit_flash = event->crit;
    } else if (p >= 0.68F) {
        const float t = ease01((p - 0.68F) / 0.32F);
        pose.x = away_from_center * lerpf(recoil, 0.0F, t);
        pose.y = lerpf(event->block ? 1.0F : 3.0F, 0.0F, t);
        pose.flash = !event->block && p < 0.82F;
        pose.block_flash = event->block && p < 0.84F;
        pose.crit_flash = event->crit && p < 0.80F;
    }
    return pose;
}

static void combat_actor_sprite(nt_ui_context_t *ctx, int slot, combat_actor_art_t art, combat_actor_pose_t pose,
                                bool portrait) {
    ensure_combat_actor_regions();

    const float root_h = (portrait ? 168.0F : 182.0F) * pose.scale_y;
    const float root_w = root_h * (512.0F / 704.0F) * pose.scale_x;
    const float shadow_w = root_w * (pose.attacking ? 0.78F : 0.64F);
    nt_ui_image_style_t sprite_style = nt_ui_image_style_defaults();
    if (pose.crit_flash) {
        sprite_style.color_packed = 0xFFFFA66CU;
    } else if (pose.block_flash) {
        sprite_style.color_packed = 0xFFB7D8FFU;
    } else {
        sprite_style.color_packed = pose.flash ? 0xFFFFC7B4U : 0xFFFFFFFFU;
    }

    CLAY({.id = CLAY_IDI("combat/actor_sprite", slot),
          .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                       .offset = {pose.x, pose.y},
                       .zIndex = COMBAT_MODAL_CONTROL_Z + 1},
          .layout = {.sizing = {CLAY_SIZING_FIXED(root_w), CLAY_SIZING_FIXED(root_h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_IDI("combat/actor_shadow", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM, .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                           .offset = {0.0F, -8.0F},
                           .zIndex = COMBAT_MODAL_CONTROL_Z + 1},
              .layout = {.sizing = {CLAY_SIZING_FIXED(shadow_w), CLAY_SIZING_FIXED(8.0F)}},
              .backgroundColor = {4.0F, 3.0F, 3.0F, 150.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {}
        CLAY({.id = CLAY_IDI("combat/actor_image", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .offset = {0.0F, 0.0F},
                           .zIndex = COMBAT_MODAL_CONTROL_Z + 2},
              .layout = {.sizing = {CLAY_SIZING_FIXED(root_w), CLAY_SIZING_FIXED(root_h)}}}) {
            nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_FILL), &s_combat_actor_regions[art], &sprite_style, NULL);
        }
    }
}

static void combat_actor_lane(nt_ui_context_t *ctx, int slot, game_combat_actor_t actor, combat_actor_art_t art,
                              combat_actor_pose_t pose, bool portrait, float height, const char *hp_label,
                              float hp_ratio, Clay_Color hp_fill, float attack_progress) {
    const float status_w = portrait ? 142.0F : 188.0F;
    const float body_h = height > 50.0F ? height - 45.0F : height;
    CLAY({.id = CLAY_IDI("combat/actor_lane", slot),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(height)},
                     .padding = {.left = 4, .right = 4, .top = 3, .bottom = 3},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 3,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = actor == GAME_COMBAT_ACTOR_PLAYER ? (Clay_Color){13.0F, 43.0F, 39.0F, 74.0F}
                                                               : (Clay_Color){49.0F, 22.0F, 16.0F, 74.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = actor == GAME_COMBAT_ACTOR_PLAYER ? (Clay_Color){82.0F, 132.0F, 118.0F, 185.0F}
                                                                : (Clay_Color){142.0F, 73.0F, 52.0F, 185.0F},
                     .width = {0, 0, 0, 0, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
        CLAY({.id = CLAY_IDI("combat/actor_body", slot),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(body_h)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.id = CLAY_IDI("combat/actor_lane_floor", slot),
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                                                .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                               .offset = {0.0F, -11.0F},
                               .zIndex = COMBAT_MODAL_CONTROL_Z + 1},
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(2.0F)}},
                  .backgroundColor = {214.0F, 151.0F, 78.0F, 118.0F},
                  .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_FILL)}) {}
            combat_actor_sprite(ctx, slot, art, pose, portrait);
        }
        CLAY({.id = CLAY_IDI("combat/actor_status", slot),
              .layout = {.sizing = {CLAY_SIZING_FIXED(status_w), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 2,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            hp_bar(ctx, slot, hp_label, hp_ratio, hp_fill, status_w);
            attack_cue(ctx, slot, attack_progress,
                       actor == GAME_COMBAT_ACTOR_PLAYER ? (Clay_Color){159.0F, 196.0F, 82.0F, 210.0F}
                                                         : (Clay_Color){190.0F, 78.0F, 55.0F, 205.0F},
                       status_w * 0.72F);
        }
    }
}

static Clay_Color combat_impact_flash_color(const game_combat_event_t *event) {
    if (event && event->crit) {
        return (Clay_Color){255.0F, 92.0F, 62.0F, 230.0F};
    }
    if (event && event->block) {
        return (Clay_Color){142.0F, 205.0F, 255.0F, 220.0F};
    }
    return (Clay_Color){255.0F, 214.0F, 117.0F, 205.0F};
}

static void combat_impact_overlay(nt_ui_context_t *ctx, const game_combat_event_t *event, int active_event,
                                  float event_progress, const char *hit_badge, Clay_Color hit_badge_bg,
                                  bool portrait) {
    const bool show_hit = active_event >= 0 && event_progress >= 0.38F && event_progress <= 0.88F;
    if (!show_hit) {
        return;
    }
    const Clay_Color flash_color = combat_impact_flash_color(event);
    CLAY({.id = CLAY_ID("combat/impact"),
          .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                        .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                       .offset = {0.0F, portrait ? -30.0F : -34.0F},
                       .zIndex = COMBAT_MODAL_CONTROL_Z + 4},
          .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? 118.0F : 144.0F), CLAY_SIZING_FIXED(54.0F)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 6,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("combat/impact_flash"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? 74.0F : 96.0F), CLAY_SIZING_FIXED(4.0F)}},
              .backgroundColor = flash_color,
              .cornerRadius = CLAY_CORNER_RADIUS(3),
              .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_FILL)}) {}
        damage_badge(ctx, 2, hit_badge, hit_badge_bg, portrait ? 96.0F : 124.0F, portrait ? 15.0F : 18.0F);
    }
}

static COMBAT_UNUSED_FN void combat_stage_ui(nt_ui_context_t *ctx, const game_combat_result_t *preview,
                                             const char *encounter_id, int active_event, int latest_event,
                                             float timeline_time, float progress, const char *hit_badge,
                                             Clay_Color hit_badge_bg, bool portrait, const char *player_label,
                                             const char *enemy_label, int player_hp, int enemy_hp,
                                             const game_combat_stats_t *player, const game_combat_stats_t *enemy) {
    (void)latest_event;
    (void)progress;
    const game_combat_event_t *event = active_event >= 0 ? &preview->events[active_event] : NULL;
    const float event_progress = combat_visual_event_progress(event, timeline_time);
    const float height = portrait ? 248.0F : 226.0F;
    const combat_actor_pose_t player_pose = combat_actor_pose(GAME_COMBAT_ACTOR_PLAYER, event, event_progress, portrait);
    const combat_actor_pose_t enemy_pose = combat_actor_pose(GAME_COMBAT_ACTOR_ENEMY, event, event_progress, portrait);

    CLAY({.id = CLAY_ID("combat/stage"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(height)},
                     .padding = {.left = 10, .right = 10, .top = 6, .bottom = 8},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = portrait ? 12 : 20,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {11.0F, 9.0F, 8.0F, 236.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .border = {.color = {126.0F, 85.0F, 49.0F, 174.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
        combat_actor_lane(ctx, 0, GAME_COMBAT_ACTOR_PLAYER, COMBAT_ACTOR_ART_HERO, player_pose, portrait,
                          height - 14.0F, player_label, (float)player_hp / (float)player->vitality,
                          (Clay_Color){134.0F, 181.0F, 66.0F, 240.0F},
                          actor_charge_progress(preview, GAME_COMBAT_ACTOR_PLAYER, timeline_time));
        combat_actor_lane(ctx, 1, GAME_COMBAT_ACTOR_ENEMY, combat_enemy_art_for_encounter(encounter_id), enemy_pose,
                          portrait, height - 14.0F, enemy_label, (float)enemy_hp / (float)enemy->vitality,
                          (Clay_Color){155.0F, 42.0F, 32.0F, 240.0F},
                          actor_charge_progress(preview, GAME_COMBAT_ACTOR_ENEMY, timeline_time));
        combat_impact_overlay(ctx, event, active_event, event_progress, hit_badge, hit_badge_bg, portrait);
    }
}

static COMBAT_UNUSED_FN void combat_meters_ui(nt_ui_context_t *ctx, const game_combat_result_t *preview, const char *player_label,
                                              const char *enemy_label, int player_hp, int enemy_hp,
                                              const game_combat_stats_t *player, const game_combat_stats_t *enemy,
                                              float timeline_time, bool portrait) {
    const float meter_w = portrait ? 132.0F : 258.0F;
    CLAY({.id = CLAY_ID("combat/meters"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 48.0F : 52.0F)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = portrait ? 8 : 14,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("combat/player_meter"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(meter_w), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 6,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            hp_bar(ctx, 0, player_label, (float)player_hp / (float)player->vitality,
                   (Clay_Color){134.0F, 181.0F, 66.0F, 240.0F}, meter_w);
            attack_cue(ctx, 0, actor_charge_progress(preview, GAME_COMBAT_ACTOR_PLAYER, timeline_time),
                       (Clay_Color){159.0F, 196.0F, 82.0F, 230.0F}, meter_w);
        }
        CLAY({.id = CLAY_ID("combat/enemy_meter"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(meter_w), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 6,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            hp_bar(ctx, 1, enemy_label, (float)enemy_hp / (float)enemy->vitality,
                   (Clay_Color){155.0F, 42.0F, 32.0F, 240.0F}, meter_w);
            attack_cue(ctx, 1, actor_charge_progress(preview, GAME_COMBAT_ACTOR_ENEMY, timeline_time),
                       (Clay_Color){185.0F, 72.0F, 52.0F, 225.0F}, meter_w);
        }
    }
}

static void combat_stat_chip(nt_ui_context_t *ctx, int slot, const char *text, Clay_Color bg) {
    const nt_ui_label_style_t style = label_style(13.0F, 232.0F, 214.0F, 180.0F, 255.0F);
    CLAY({.id = CLAY_IDI("combat/stat_chip", slot),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30.0F)},
                     .padding = {.left = 8, .right = 8, .top = 4, .bottom = 4},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = bg,
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {92.0F, 66.0F, 41.0F, 158.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
        shadowed_label(ctx, 130 + slot, text, &style);
    }
}

static COMBAT_UNUSED_FN void combat_stats_summary_ui(nt_ui_context_t *ctx, const game_combat_stats_t *player,
                                                     const game_combat_stats_t *enemy, bool portrait) {
    char player_stats[80];
    char enemy_stats[80];
    (void)snprintf(player_stats, sizeof player_stats, "Ты: удар %d  защ %d",
                   game_combat_attack_power(player), player->protection);
    (void)snprintf(enemy_stats, sizeof enemy_stats, "Враг: удар %d  защ %d",
                   game_combat_attack_power(enemy), enemy->protection);
    CLAY({.id = CLAY_ID("combat/stats_summary"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                     .childGap = portrait ? 6 : 10,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        combat_stat_chip(ctx, 0, player_stats, (Clay_Color){24.0F, 36.0F, 32.0F, 202.0F});
        combat_stat_chip(ctx, 1, enemy_stats, (Clay_Color){42.0F, 26.0F, 22.0F, 202.0F});
    }
}

static COMBAT_UNUSED_FN void combat_log_ui(nt_ui_context_t *ctx, const game_combat_result_t *preview, int latest_event, bool portrait) {
    CLAY({.id = CLAY_ID("combat/log"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 72.0F : 78.0F)},
                     .padding = CLAY_PADDING_ALL(9),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 5},
          .backgroundColor = {14.0F, 11.0F, 9.0F, 205.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = {83.0F, 61.0F, 39.0F, 190.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
        if (latest_event < 0) {
            stat_line(ctx, 10, "Ты входишь в ближний бой.");
        } else {
            int first_event = latest_event - 2;
            if (first_event < 0) {
                first_event = 0;
            }
            for (int i = first_event; i <= latest_event; ++i) {
                char line[96];
                combat_event_log_line(&preview->events[i], line, sizeof line);
                stat_line(ctx, 10 + i - first_event, line);
            }
        }
    }
}

static bool build_preview(World *w, const game_encounter_definition_t *encounter) {
    if (!w || !w->player_state || !encounter) {
        return false;
    }
    if (!game_combat_build_player_stats(w->player_state, &w->combat.player_stats)) {
        return false;
    }
    if (!game_combat_simulate(&w->combat.player_stats, w->player_state->hero_hp, encounter, encounter_seed(encounter->id),
                              &w->combat.preview_result)) {
        return false;
    }
    w->combat.visual_duration_seconds = clampf(w->combat.preview_result.duration_seconds, 4.0F, 10.0F);
    return true;
}

bool combat_flow_is_open(const World *w) { return w && w->combat.phase != COMBAT_FLOW_NONE; }

bool combat_flow_can_start_gate_check(const World *w) {
    if (!w || !w->player_state || w->dialogue.open || equipment_screen_open() || combat_flow_is_open(w)) {
        return false;
    }
    const GameState *state = w->player_state;
    if (list_contains(state->flags_ids, state->flags_ids_count, FIRST_ENCOUNTER_FLAG_ID)) {
        return false;
    }
    if (!state->has_quests_tracked_quest_id) {
        return false;
    }
    const GameQuestState *quest = find_quest(state, state->quests_tracked_quest_id);
    return quest && quest->status == GAME_STATE_QUEST_STATUS_ACTIVE && quest->has_current_step_id &&
           strcmp(quest->current_step_id, FIRST_ENCOUNTER_STEP_ID) == 0;
}

void combat_flow_open_prefight(World *w, const char *encounter_id) {
    if (!w || !w->player_state || !encounter_id || strlen(encounter_id) >= sizeof w->combat.encounter_id) {
        return;
    }
    const game_encounter_definition_t *encounter = game_content_find_encounter(encounter_id);
    if (!encounter || !build_preview(w, encounter)) {
        return;
    }
    w->combat.phase = COMBAT_FLOW_PREFIGHT;
    (void)strcpy(w->combat.encounter_id, encounter_id);
    w->combat.phase_started_at = w->time_seconds;
    w->combat.result = (game_combat_result_t){0};
    w->combat.result_applied = false;
    w->combat.last_audio_event_index = -1;
    game_audio_play(GAME_AUDIO_CUE_COMBAT_START);
}

static float combat_panel_height(CombatFlowPhase phase, bool portrait, float layout_h) {
    if (phase == COMBAT_FLOW_RUNNING) {
        return portrait ? clampf(layout_h * 0.58F, 430.0F, layout_h - 144.0F) : 416.0F;
    }
    if (phase == COMBAT_FLOW_RESULT) {
        return portrait ? clampf(layout_h * 0.48F, 344.0F, layout_h - 176.0F) : 314.0F;
    }
    return portrait ? clampf(layout_h * 0.80F, 468.0F, layout_h - 96.0F) : 430.0F;
}

static float combat_panel_width(bool portrait, float layout_w) {
    return portrait ? clampf(layout_w - 28.0F, 300.0F, 460.0F) : clampf(layout_w * 0.62F, 620.0F, 760.0F);
}

static void panel_frame(nt_ui_context_t *ctx, CombatFlowPhase phase, bool portrait, float layout_w, float layout_h) {
    const float panel_w = combat_panel_width(portrait, layout_w);
    const float panel_h = combat_panel_height(phase, portrait, layout_h);
    const Clay_Color panel_bg = phase == COMBAT_FLOW_RUNNING ? (Clay_Color){27.0F, 19.0F, 14.0F, 232.0F}
                                                            : (Clay_Color){18.0F, 12.0F, 8.0F, 178.0F};
    const Clay_Color panel_border = phase == COMBAT_FLOW_RUNNING ? (Clay_Color){205.0F, 139.0F, 65.0F, 205.0F}
                                                                : (Clay_Color){171.0F, 118.0F, 58.0F, 132.0F};
    CLAY({.id = CLAY_ID("combat/panel_shell"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIXED(panel_h)},
                     .padding = {.left = portrait ? 12 : 16, .right = portrait ? 12 : 16, .top = 12, .bottom = 14},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 8 : 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = panel_bg,
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = panel_border, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
        (void)ctx;
    }
}

static void prefight_ui(nt_ui_context_t *ctx, World *w, const game_encounter_definition_t *encounter, bool portrait, float layout_w) {
    (void)layout_w;
    const nt_ui_label_style_t title = label_style(portrait ? 20.0F : 24.0F, 255.0F, 238.0F, 204.0F, 255.0F);
    const nt_ui_label_style_t hint = label_style(15.0F, 226.0F, 207.0F, 172.0F, 255.0F);
    const nt_ui_label_style_t threat = label_style(16.0F, 244.0F, 190.0F, 88.0F, 255.0F);
    game_combat_stats_t *player = &w->combat.player_stats;
    game_combat_stats_t const *enemy = &encounter->enemy;
    char player_hp[64];
    char player_dmg[64];
    char enemy_hp[64];
    char enemy_dmg[64];
    (void)snprintf(player_hp, sizeof player_hp, "Твои HP: %d/%d", w->player_state->hero_hp, player->vitality);
    (void)snprintf(player_dmg, sizeof player_dmg, "Удар: %d  Защита: %d  Крит: %d", game_combat_attack_power(player), player->protection,
                   player->intuition);
    (void)snprintf(enemy_hp, sizeof enemy_hp, "HP врага: %d", enemy->vitality);
    (void)snprintf(enemy_dmg, sizeof enemy_dmg, "Удар: %d  Защита: %d", game_combat_attack_power(enemy), enemy->protection);

    CLAY({.id = CLAY_ID("combat/prefight"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 9 : 11,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        shadowed_label(ctx, 1, encounter->display_name, &title);
        CLAY({.id = CLAY_ID("combat/prefight_intro"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                         .childGap = 10,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            enemy_portrait_ui(ctx, combat_enemy_art_for_encounter(w->combat.encounter_id), portrait);
            CLAY({.id = CLAY_ID("combat/prefight_intro_text"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 4,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), "Короткая проверка у внутренних ворот.", &hint);
                nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), threat_reason_line(encounter->expected_threat), &hint);
            }
        }
        CLAY({.id = CLAY_ID("combat/threat"),
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(28.0F)},
                         .padding = {.left = 10, .right = 10, .top = 4, .bottom = 4},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {52.0F, 34.0F, 19.0F, 230.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .border = {.color = {184.0F, 126.0F, 58.0F, 230.0F}, .width = {1, 1, 1, 1, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
            char threat_buf[96];
            (void)snprintf(threat_buf, sizeof threat_buf, "Угроза: %s", threat_display_name(encounter->expected_threat));
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), threat_buf, &threat);
        }
        CLAY({.id = CLAY_ID("combat/prefight_columns"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                         .childGap = 10,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
            CLAY({.id = CLAY_ID("combat/player_stats"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .padding = CLAY_PADDING_ALL(10),
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 6},
                  .backgroundColor = {29.0F, 23.0F, 18.0F, 154.0F},
                  .cornerRadius = CLAY_CORNER_RADIUS(4),
                  .border = {.color = {99.0F, 72.0F, 43.0F, 126.0F}, .width = {1, 1, 1, 1, 0}},
                  .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
                stat_line(ctx, 0, "Наемник");
                stat_line(ctx, 1, player_hp);
                stat_line(ctx, 2, player_dmg);
            }
            CLAY({.id = CLAY_ID("combat/enemy_stats"),
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .padding = CLAY_PADDING_ALL(10),
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 6},
                  .backgroundColor = {29.0F, 20.0F, 18.0F, 154.0F},
                  .cornerRadius = CLAY_CORNER_RADIUS(4),
                  .border = {.color = {99.0F, 60.0F, 43.0F, 126.0F}, .width = {1, 1, 1, 1, 0}},
                  .userData = NT_UI_CLAY_DATA(LAYER_COMBAT_BG)}) {
                stat_line(ctx, 3, "Противник");
                stat_line(ctx, 4, enemy_hp);
                stat_line(ctx, 5, enemy_dmg);
            }
        }
        combat_rewards_ui(ctx, encounter, portrait, 1000, true);
        CLAY({.id = CLAY_ID("combat/prefight_buttons"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = portrait ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                         .childGap = 10,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            if (combat_button(ctx, "combat/prefight_start", "В бой", true, portrait ? 260.0F : 180.0F)) {
                w->combat.phase = COMBAT_FLOW_RUNNING;
                w->combat.phase_started_at = w->time_seconds;
                w->combat.result_applied = false;
                w->combat.last_audio_event_index = -1;
            }
            if (combat_button(ctx, "combat/prefight_back", "Назад", false, portrait ? 260.0F : 160.0F)) {
                w->combat.phase = COMBAT_FLOW_NONE;
            }
        }
    }
}

static void running_ui(nt_ui_context_t *ctx, World *w, const game_encounter_definition_t *encounter, bool portrait) {
    const float elapsed = w->time_seconds - w->combat.phase_started_at;
    const float duration = w->combat.visual_duration_seconds > 0.1F ? w->combat.visual_duration_seconds : 4.0F;
    const float progress = clampf(elapsed / duration, 0.0F, 1.0F);
    const game_combat_result_t *preview = &w->combat.preview_result;
    const float timeline_duration = preview->duration_seconds > 0.1F ? preview->duration_seconds : duration;
    const float timeline_time = timeline_duration * progress;
    const int latest_event = latest_combat_event_index(preview, timeline_time);
    const int active_event = active_combat_visual_event_index(preview, timeline_time);
    game_combat_stats_t *player = &w->combat.player_stats;
    game_combat_stats_t const *enemy = &encounter->enemy;
    int player_hp = w->player_state->hero_hp;
    int enemy_hp = enemy->vitality;
    char player_label[64];
    char enemy_label[64];
    char hit_badge[48];
    Clay_Color hit_badge_bg = (Clay_Color){52.0F, 34.0F, 19.0F, 218.0F};
    if (latest_event >= 0) {
        const game_combat_event_t *event = &preview->events[latest_event];
        player_hp = event->player_hp_after;
        enemy_hp = event->enemy_hp_after;
        if (latest_event > w->combat.last_audio_event_index) {
            game_audio_play(GAME_AUDIO_CUE_COMBAT_HIT);
            w->combat.last_audio_event_index = latest_event;
        }
    }
    (void)snprintf(player_label, sizeof player_label, "Наемник  %d/%d", player_hp, player->vitality);
    (void)snprintf(enemy_label, sizeof enemy_label, "%s  %d/%d", encounter->display_name, enemy_hp, enemy->vitality);
    (void)snprintf(hit_badge, sizeof hit_badge, "ожидание");
    if (latest_event >= 0) {
        const game_combat_event_t *event = &preview->events[latest_event];
        if (event->actor == GAME_COMBAT_ACTOR_PLAYER) {
            (void)snprintf(hit_badge, sizeof hit_badge, "по врагу -%d", event->damage);
            hit_badge_bg = (Clay_Color){72.0F, 28.0F, 24.0F, 226.0F};
        } else if (event->actor == GAME_COMBAT_ACTOR_ENEMY) {
            (void)snprintf(hit_badge, sizeof hit_badge, "по тебе -%d", event->damage);
            hit_badge_bg = (Clay_Color){58.0F, 33.0F, 25.0F, 218.0F};
        }
    }

    (void)snprintf(player_label, sizeof player_label, "Ты  %d/%d", player_hp, player->vitality);
    (void)snprintf(enemy_label, sizeof enemy_label, "Враг  %d/%d", enemy_hp, enemy->vitality);
    if (latest_event >= 0) {
        const game_combat_event_t *event = &preview->events[latest_event];
        if (event->crit) {
            (void)snprintf(hit_badge, sizeof hit_badge, "CRIT -%d", event->damage);
            hit_badge_bg = (Clay_Color){101.0F, 31.0F, 24.0F, 232.0F};
        } else if (event->block) {
            (void)snprintf(hit_badge, sizeof hit_badge, "BLOCK -%d", event->damage);
            hit_badge_bg = (Clay_Color){28.0F, 55.0F, 78.0F, 226.0F};
        } else {
            (void)snprintf(hit_badge, sizeof hit_badge, "-%d", event->damage);
        }
    }

    CLAY({.id = CLAY_ID("combat/running"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 8 : 10,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}}) {
        const nt_ui_label_style_t stage_title = label_style(22.0F, 255.0F, 238.0F, 204.0F, 255.0F);
        shadowed_label(ctx, 20, "Автобой", &stage_title);
        combat_stage_ui(ctx, preview, encounter->id, active_event, latest_event, timeline_time, progress, hit_badge,
                        hit_badge_bg, portrait, player_label, enemy_label, player_hp, enemy_hp, player, enemy);
        combat_log_ui(ctx, preview, latest_event, portrait);
    }

    if (progress >= 1.0F && !w->combat.result_applied) {
        if (game_actions_resolve_encounter(w->player_state, w->combat.encounter_id, &w->combat.result)) {
            w->combat.result_applied = true;
            if (w->combat.result.outcome == GAME_COMBAT_OUTCOME_WIN &&
                strcmp(w->combat.encounter_id, FIRST_ENCOUNTER_ID) == 0) {
                w->first_scene.objective_object_id = "hub_last_post.gate_guard";
                w->first_scene.tutorial_guard_talk_completed = false;
                w->first_scene.current_objective_text = "Вернись к стражнику";
            }
            game_audio_play(w->combat.result.outcome == GAME_COMBAT_OUTCOME_WIN ? GAME_AUDIO_CUE_COMBAT_VICTORY
                                                                                 : GAME_AUDIO_CUE_COMBAT_DEFEAT);
            if (w->combat.result.reward_granted) {
                game_audio_play(GAME_AUDIO_CUE_REWARD);
            }
            w->combat.phase = COMBAT_FLOW_RESULT;
            w->combat.phase_started_at = w->time_seconds;
        }
    }
}

static bool is_gate_scavenger_encounter(const game_encounter_definition_t *encounter) {
    return encounter && encounter->id && strcmp(encounter->id, "gate_scavenger") == 0;
}

static const char *result_close_label(bool win, const game_encounter_definition_t *encounter) {
    if (!win) {
        return "Вернуться к посту";
    }
    return is_gate_scavenger_encounter(encounter) ? "К стражу" : "Продолжить";
}

static void result_ui(nt_ui_context_t *ctx, World *w, const game_encounter_definition_t *encounter, bool portrait) {
    const bool win = w->combat.result.outcome == GAME_COMBAT_OUTCOME_WIN;
    const nt_ui_label_style_t title =
        win ? label_style(24.0F, 255.0F, 238.0F, 204.0F, 255.0F) : label_style(24.0F, 255.0F, 202.0F, 188.0F, 255.0F);
    const nt_ui_label_style_t body = label_style(16.0F, 226.0F, 207.0F, 172.0F, 255.0F);
    char summary[128];
    char rewards[192];
    char next_step[160];
    next_step[0] = '\0';
    rewards[0] = '\0';
    if (win) {
        (void)snprintf(summary, sizeof summary, "Осталось HP: %d. Нанесено урона: %d.", w->combat.result.player_hp,
                       w->combat.result.player_damage_done);
    } else {
        (void)snprintf(summary, sizeof summary, "Ты не дожал врага. Нанесено урона: %d.", w->combat.result.player_damage_done);
    }
    if (win && w->combat.result.reward_granted) {
        if (is_gate_scavenger_encounter(encounter)) {
            (void)snprintf(next_step, sizeof next_step, "Задание обновлено: вернись к стражу.");
        } else {
            (void)snprintf(next_step, sizeof next_step, "Награда добавлена. Продолжай исследовать место.");
        }
    } else if (win) {
        (void)snprintf(rewards, sizeof rewards, "Победа засчитана. Награда уже была получена.");
        (void)snprintf(next_step, sizeof next_step, "Можно продолжать.");
    } else {
        (void)snprintf(rewards, sizeof rewards, "Награда не получена. Прогресс задания не изменился.");
        (void)snprintf(next_step, sizeof next_step, "Ты вернулся к посту. HP восстановлены, проверь снаряжение.");
    }

    CLAY({.id = CLAY_ID("combat/result"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 10 : 12,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        shadowed_label(ctx, 30, win ? "Победа" : "Поражение", &title);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), summary, &body);
        if (win && w->combat.result.reward_granted) {
            combat_rewards_ui(ctx, encounter, portrait, 1100, true);
        } else {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), rewards, &body);
        }
        if (next_step[0] != '\0') {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_COMBAT_TEXT), next_step, &body);
        }
        CLAY({.id = CLAY_ID("combat/result_spacer"), .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(6.0F)}}}) {}
        if (combat_button(ctx, "combat/result_close", result_close_label(win, encounter), true, portrait ? 260.0F : 220.0F)) {
            w->combat.phase = COMBAT_FLOW_NONE;
        }
    }
}

void combat_flow_ui(nt_ui_context_t *ctx, World *w) {
    combat_clear_transient_ui_state(ctx);
    if (!ctx || !combat_flow_is_open(w)) {
        return;
    }
    const game_encounter_definition_t *encounter = game_content_find_encounter(w->combat.encounter_id);
    if (!encounter) {
        w->combat.phase = COMBAT_FLOW_NONE;
        combat_request_state_cleanup();
        combat_clear_transient_ui_state(ctx);
        return;
    }

    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    const bool portrait = layout_h > layout_w;
    const bool dismissible = w->combat.phase != COMBAT_FLOW_RUNNING;
    bool modal_open = true;
    nt_ui_modal_style_t modal_style = game_modal_style((nt_ui_layer_t)LAYER_COMBAT_BG, dismissible);
    if (!nt_ui_modal_visible(ctx, COMBAT_MODAL_ID, &modal_style, &modal_open)) {
        if (!modal_open && dismissible) {
            w->combat.phase = COMBAT_FLOW_NONE;
            combat_request_state_cleanup();
            combat_clear_transient_ui_state(ctx);
        }
        return;
    }
    const float panel_w = combat_panel_width(portrait, layout_w);
    const float panel_h = combat_panel_height(w->combat.phase, portrait, layout_h);

    CLAY({.id = CLAY_ID("combat/panel_stack"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIXED(panel_h)}}}) {
        panel_frame(ctx, w->combat.phase, portrait, layout_w, layout_h);
        if (dismissible) {
            CLAY({.id = CLAY_ID("combat/close_anchor"),
                  .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_TOP, .parent = CLAY_ATTACH_POINT_RIGHT_TOP},
                               .offset = {-14.0F, 12.0F},
                               .zIndex = 4},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(38.0F), CLAY_SIZING_FIXED(38.0F)}}}) {
                if (game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_COMBAT_BG, (nt_ui_layer_t)LAYER_COMBAT_TEXT,
                                            "combat/close", portrait)) {
                    modal_open = false;
                }
            }
        }
        CLAY({.id = CLAY_ID("combat/panel_content"),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                           .zIndex = COMBAT_MODAL_CONTROL_Z},
              .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? clampf(layout_w - 52.0F, 276.0F, 436.0F)
                                                               : clampf(layout_w * 0.62F - 32.0F, 588.0F, 728.0F)),
                                    CLAY_SIZING_FIXED(panel_h - 28.0F)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
            if (w->combat.phase == COMBAT_FLOW_PREFIGHT) {
                prefight_ui(ctx, w, encounter, portrait, layout_w);
            } else if (w->combat.phase == COMBAT_FLOW_RUNNING) {
                running_ui(ctx, w, encounter, portrait);
            } else if (w->combat.phase == COMBAT_FLOW_RESULT) {
                result_ui(ctx, w, encounter, portrait);
            }
        }
    }
    nt_ui_modal_end(ctx);
    if (!modal_open && dismissible) {
        w->combat.phase = COMBAT_FLOW_NONE;
    }
    if (!combat_flow_is_open(w)) {
        combat_request_state_cleanup();
    }
    combat_clear_transient_ui_state(ctx);
}
