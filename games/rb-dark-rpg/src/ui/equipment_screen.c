#include "ui/equipment_screen.h"

#include "clay.h"
#include "generated/game_assets.h"
#include "game_audio.h"
#include "game_actions.h"
#include "game_combat.h"
#include "game_content.h"
#include "game_state.h"
#include "ui/game_modal.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_scroll.h"
#include "ui/nt_ui_state.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"

#include <stdio.h>
#include <string.h>

#define LAYER_BG 18
#define LAYER_ART 19
#define LAYER_ICON 20
#define LAYER_TEXT 21
#define EQUIPMENT_MODAL_ID 0xE901F001U
#define EQUIPMENT_ITEM_MODAL_ID 0xE901F002U
#define BACKPACK_TARGET_CELL_SIZE 46.0F
#define BACKPACK_MIN_CELL_SIZE 40.0F
#define BACKPACK_MAX_CELL_SIZE 50.0F
#define BACKPACK_PANEL_PADDING 7.0F

#if defined(__GNUC__) || defined(__clang__)
#define RB_UNUSED_FN __attribute__((unused))
#else
#define RB_UNUSED_FN
#endif

static bool s_open;
static bool s_detail_open;
static char s_selected_instance_id[GAME_STATE_STRING_MAX];
static char s_selected_stack_id[GAME_STATE_STRING_MAX];
static int s_dismiss_guard_frames;
static bool s_main_cleanup_pending;
static bool s_detail_cleanup_pending;

static const nt_ui_widget_def_t EQUIPMENT_WIDGET_DEF = {
    .name = "gear_screen",
    .pill_color = 0xFFB58A45U,
    ._reserved = 0U,
};

typedef enum equipment_inventory_tab_t {
    EQUIPMENT_TAB_GEAR = 0,
    EQUIPMENT_TAB_QUEST,
} equipment_inventory_tab_t;

static equipment_inventory_tab_t s_inventory_tab;

static void equipment_request_state_cleanup(void) {
    s_main_cleanup_pending = true;
    s_detail_cleanup_pending = true;
}

static void equipment_request_detail_state_cleanup(void) { s_detail_cleanup_pending = true; }

static void equipment_detail_set_open(bool open) {
    if (!open && s_detail_open) {
        equipment_request_detail_state_cleanup();
    }
    s_detail_open = open;
}

static void equipment_clear_transient_ui_state(nt_ui_context_t *ctx) {
    if (!ctx) {
        return;
    }
    if (s_main_cleanup_pending) {
        game_modal_clear_state(ctx, EQUIPMENT_MODAL_ID);
        nt_ui_state_clear(ctx, nt_ui_id("gear_screen/backpack_scroll"));
        nt_ui_state_clear(ctx, nt_ui_id("gear_screen/quest_scroll"));
        s_main_cleanup_pending = false;
    }
    if (s_detail_cleanup_pending) {
        game_modal_clear_state(ctx, EQUIPMENT_ITEM_MODAL_ID);
        s_detail_cleanup_pending = false;
    }
}

typedef enum equipment_art_region_t {
    EQUIP_ART_CELL = 0,
    EQUIP_ART_SLOT_WEAPON,
    EQUIP_ART_SLOT_OFFHAND,
    EQUIP_ART_SLOT_HEAD,
    EQUIP_ART_SLOT_ARMOUR,
    EQUIP_ART_SLOT_HANDS,
    EQUIP_ART_SLOT_WAIST,
    EQUIP_ART_SLOT_LEGS,
    EQUIP_ART_SLOT_FEET,
    EQUIP_ART_SLOT_NECK,
    EQUIP_ART_SLOT_RING_LEFT,
    EQUIP_ART_SLOT_RING_RIGHT,
    EQUIP_ART_SLOT_RELIC,
    EQUIP_ART_ITEM_OLD_SWORD,
    EQUIP_ART_ITEM_PADDED_JACKET,
    EQUIP_ART_ITEM_LEATHER_GREAVES,
    EQUIP_ART_ITEM_IRON_SWORD,
    EQUIP_ART_ITEM_PATCHED_MAIL,
    EQUIP_ART_ITEM_GUARD_COAT,
    EQUIP_ART_ITEM_IRON_GREAVES,
    EQUIP_ART_ITEM_MILITIA_AXE,
    EQUIP_ART_ITEM_RUNNER_WRAPS,
    EQUIP_ART_ITEM_BLACK_SUN_CHARM,
    EQUIP_ART_ITEM_MILLER_HOOK,
    EQUIP_ART_ITEM_CHAIN_PATCHES,
    EQUIP_ART_ITEM_SCAVENGER_KNEE_PLATES,
    EQUIP_ART_ITEM_DRAGON_ASH_TOKEN,
    EQUIP_ART_ITEM_MILLER_LUCKY_NAIL,
    EQUIP_ART_ITEM_SEEKER_TOKEN,
    EQUIP_ART_ITEM_GRAIN_SACKS,
    EQUIP_ART_ITEM_CONTRACT_PROGRESS,
    EQUIP_ART_ITEM_CLUE_FRAGMENT,
    EQUIP_ART_ITEM_BURNED_CHAIN_BRACKET,
    EQUIP_ART_ITEM_ORDER_SCRAP,
    EQUIP_ART_HERO,
    EQUIP_ART_COUNT,
} equipment_art_region_t;

static const nt_hash64_t EQUIPMENT_ART_REGION_HASHES[EQUIP_ART_COUNT] = {
    ASSET_ATLAS_REGION_UI_ASSET_EQUIPMENT_SLOT_CELL,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_WEAPON_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_OFFHAND_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_HEAD_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_ARMOUR_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_HANDS_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_WAIST_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_LEGS_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_FEET_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_NECK_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_RING_LEFT_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_RING_RIGHT_EMPTY,
    ASSET_ATLAS_REGION_UI_ASSET_SLOT_ICON_RELIC_EMPTY,
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
    ASSET_ATLAS_REGION_UI_COMBAT_ACTOR_HERO,
};

static nt_resource_t s_equipment_atlas;
static nt_atlas_region_ref_t s_equipment_regions[EQUIP_ART_COUNT];

static nt_ui_label_style_t label_style(float font_size, float r, float g, float b, float a) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = font_size, .color = {r, g, b, a}};
}

static float clamp_f(float value, float lo, float hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

static int clamp_i(int value, int lo, int hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

static float equipment_backpack_available_width(float panel_w, bool portrait) {
    const float outer_padding = portrait ? 8.0F : 12.0F;
    float available_w = panel_w - outer_padding * 2.0F;
    if (!portrait) {
        const float doll_w = 244.0F;
        const float stats_w = 152.0F;
        available_w -= doll_w + 8.0F + stats_w + 8.0F;
    }
    return available_w > BACKPACK_TARGET_CELL_SIZE ? available_w : BACKPACK_TARGET_CELL_SIZE;
}

static float backpack_cell_gap(bool portrait) { return portrait ? 2.0F : 4.0F; }

static float backpack_grid_inset(bool portrait) { return portrait ? 1.0F : 8.0F; }

static float backpack_cell_size_for_layout(float available_w, int columns, bool portrait) {
    const float gap = backpack_cell_gap(portrait);
    const float content_w = available_w - BACKPACK_PANEL_PADDING * 2.0F - backpack_grid_inset(portrait) * 2.0F;
    const float gap_w = gap * (float)(columns > 0 ? columns - 1 : 0);
    const float size = columns > 0 ? (content_w - gap_w) / (float)columns : BACKPACK_TARGET_CELL_SIZE;
    return clamp_f(size, BACKPACK_MIN_CELL_SIZE, BACKPACK_MAX_CELL_SIZE);
}

static int backpack_columns_for_width(float available_w, bool portrait) {
    const float gap = backpack_cell_gap(portrait);
    const float content_w = available_w - BACKPACK_PANEL_PADDING * 2.0F - backpack_grid_inset(portrait) * 2.0F;
    const float stride = BACKPACK_TARGET_CELL_SIZE + gap;
    const int min_columns = portrait ? 4 : 6;
    const int max_columns = portrait ? 9 : 18;
    int columns = (int)(((content_w + gap) / stride) + 0.5F);
    columns = clamp_i(columns, min_columns, max_columns);
    while (columns > min_columns && backpack_cell_size_for_layout(available_w, columns, portrait) < BACKPACK_MIN_CELL_SIZE) {
        --columns;
    }
    while (columns < max_columns && backpack_cell_size_for_layout(available_w, columns, portrait) > BACKPACK_MAX_CELL_SIZE) {
        ++columns;
    }
    return columns;
}

static void text_label(nt_ui_context_t *ctx, const char *text, const nt_ui_label_style_t *style) {
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), text, style);
}

static void ensure_equipment_art_regions(void) {
    if (s_equipment_atlas.id != 0U) {
        return;
    }
    s_equipment_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    for (int i = 0; i < EQUIP_ART_COUNT; ++i) {
        s_equipment_regions[i] = nt_atlas_ref(s_equipment_atlas, EQUIPMENT_ART_REGION_HASHES[i].value);
    }
}

static equipment_art_region_t equipment_slot_empty_art(game_item_slot_t slot) {
    switch (slot) {
        case GAME_ITEM_SLOT_WEAPON:
            return EQUIP_ART_SLOT_WEAPON;
        case GAME_ITEM_SLOT_OFFHAND:
            return EQUIP_ART_SLOT_OFFHAND;
        case GAME_ITEM_SLOT_HEAD:
            return EQUIP_ART_SLOT_HEAD;
        case GAME_ITEM_SLOT_ARMOUR:
            return EQUIP_ART_SLOT_ARMOUR;
        case GAME_ITEM_SLOT_HANDS:
            return EQUIP_ART_SLOT_HANDS;
        case GAME_ITEM_SLOT_WAIST:
            return EQUIP_ART_SLOT_WAIST;
        case GAME_ITEM_SLOT_LEGS:
            return EQUIP_ART_SLOT_LEGS;
        case GAME_ITEM_SLOT_FEET:
            return EQUIP_ART_SLOT_FEET;
        case GAME_ITEM_SLOT_NECK:
            return EQUIP_ART_SLOT_NECK;
        case GAME_ITEM_SLOT_RING_LEFT:
            return EQUIP_ART_SLOT_RING_LEFT;
        case GAME_ITEM_SLOT_RING_RIGHT:
            return EQUIP_ART_SLOT_RING_RIGHT;
        case GAME_ITEM_SLOT_RELIC:
            return EQUIP_ART_SLOT_RELIC;
        case GAME_ITEM_SLOT_NONE:
        default:
            return EQUIP_ART_SLOT_RELIC;
    }
}

static equipment_art_region_t equipment_item_art(const game_item_definition_t *item) {
    if (!item || !item->id) {
        return EQUIP_ART_COUNT;
    }
    if (strcmp(item->id, "old_sword") == 0) {
        return EQUIP_ART_ITEM_OLD_SWORD;
    }
    if (strcmp(item->id, "padded_jacket") == 0) {
        return EQUIP_ART_ITEM_PADDED_JACKET;
    }
    if (strcmp(item->id, "leather_greaves") == 0) {
        return EQUIP_ART_ITEM_LEATHER_GREAVES;
    }
    if (strcmp(item->id, "iron_sword") == 0) {
        return EQUIP_ART_ITEM_IRON_SWORD;
    }
    if (strcmp(item->id, "patched_mail") == 0) {
        return EQUIP_ART_ITEM_PATCHED_MAIL;
    }
    if (strcmp(item->id, "guard_coat") == 0) {
        return EQUIP_ART_ITEM_GUARD_COAT;
    }
    if (strcmp(item->id, "iron_greaves") == 0) {
        return EQUIP_ART_ITEM_IRON_GREAVES;
    }
    if (strcmp(item->id, "militia_axe") == 0) {
        return EQUIP_ART_ITEM_MILITIA_AXE;
    }
    if (strcmp(item->id, "runner_wraps") == 0) {
        return EQUIP_ART_ITEM_RUNNER_WRAPS;
    }
    if (strcmp(item->id, "black_sun_charm") == 0) {
        return EQUIP_ART_ITEM_BLACK_SUN_CHARM;
    }
    if (strcmp(item->id, "miller_hook") == 0) {
        return EQUIP_ART_ITEM_MILLER_HOOK;
    }
    if (strcmp(item->id, "chain_patches") == 0) {
        return EQUIP_ART_ITEM_CHAIN_PATCHES;
    }
    if (strcmp(item->id, "scavenger_knee_plates") == 0) {
        return EQUIP_ART_ITEM_SCAVENGER_KNEE_PLATES;
    }
    if (strcmp(item->id, "dragon_ash_token") == 0) {
        return EQUIP_ART_ITEM_DRAGON_ASH_TOKEN;
    }
    if (strcmp(item->id, "miller_lucky_nail") == 0) {
        return EQUIP_ART_ITEM_MILLER_LUCKY_NAIL;
    }
    if (strcmp(item->id, "seeker_token") == 0 || strcmp(item->id, "seeker_token_unlock") == 0) {
        return EQUIP_ART_ITEM_SEEKER_TOKEN;
    }
    if (strcmp(item->id, "grain_sacks") == 0) {
        return EQUIP_ART_ITEM_GRAIN_SACKS;
    }
    if (strcmp(item->id, "contract_progress") == 0) {
        return EQUIP_ART_ITEM_CONTRACT_PROGRESS;
    }
    if (strcmp(item->id, "clue_fragment") == 0) {
        return EQUIP_ART_ITEM_CLUE_FRAGMENT;
    }
    if (strcmp(item->id, "burned_chain_bracket") == 0) {
        return EQUIP_ART_ITEM_BURNED_CHAIN_BRACKET;
    }
    if (strcmp(item->id, "order_scrap") == 0) {
        return EQUIP_ART_ITEM_ORDER_SCRAP;
    }
    return EQUIP_ART_COUNT;
}

static void equipment_art_image_box_opacity(nt_ui_context_t *ctx, equipment_art_region_t art, float width, float height,
                                            uint32_t tint, nt_ui_layer_t layer, float opacity) {
    if (art >= EQUIP_ART_COUNT) {
        return;
    }
    ensure_equipment_art_regions();
    nt_ui_image_style_t style = nt_ui_image_style_defaults();
    style.color_packed = tint;
    nt_ui_transform_t transform = nt_ui_transform_defaults();
    const nt_ui_element_data_t *data =
        opacity < 1.0F ? NT_UI_DATA_XFORM(layer, &transform, opacity) : NT_UI_DATA_LAYER(layer);
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(height)}}}) {
        nt_ui_image(ctx, data, &s_equipment_regions[art], &style, NULL);
    }
}

static void equipment_art_image_opacity(nt_ui_context_t *ctx, equipment_art_region_t art, float size, uint32_t tint,
                                        nt_ui_layer_t layer, float opacity) {
    equipment_art_image_box_opacity(ctx, art, size, size, tint, layer, opacity);
}

static void equipment_art_image(nt_ui_context_t *ctx, equipment_art_region_t art, float size, uint32_t tint, nt_ui_layer_t layer) {
    equipment_art_image_opacity(ctx, art, size, tint, layer, 1.0F);
}

static void equipment_cell_art_ui(nt_ui_context_t *ctx, float size, const game_item_definition_t *item,
                                  game_item_slot_t empty_slot, bool dim_empty) {
    const equipment_art_region_t item_art = equipment_item_art(item);
    const bool filled = item_art < EQUIP_ART_COUNT;
    const equipment_art_region_t art = filled ? item_art : equipment_slot_empty_art(empty_slot);
    const float icon_size = filled ? size * 0.78F : size * 0.58F;
    const float icon_opacity = filled ? 1.0F : (dim_empty ? 0.27F : 0.36F);
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = filled ? (Clay_Color){15.0F, 11.0F, 8.0F, 190.0F}
                                    : (Clay_Color){11.0F, 8.0F, 6.0F, 142.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(3),
          .border = {.color = filled ? (Clay_Color){126.0F, 88.0F, 48.0F, 160.0F}
                                     : (Clay_Color){92.0F, 66.0F, 40.0F, 116.0F},
                     .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_ART)}) {
        equipment_art_image_opacity(ctx, art, icon_size, filled ? 0xFFFFFFFFU : 0xFFB09A78U, LAYER_ICON, icon_opacity);
    }
}

static void equipment_cell_state_overlay(float size, Clay_Color border, Clay_Color fill) {
    CLAY({.floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                       .offset = {0.0F, 0.0F},
                       .zIndex = 4},
          .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)}},
          .backgroundColor = fill,
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = border, .width = {2, 2, 2, 2, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_ICON)}) {}
}

static const GameGearInstance *find_gear(const GameState *state, const char *instance_id) {
    if (!state || !instance_id || instance_id[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
        const GameGearInstance *gear = &state->inventory_gear_instances[i];
        if (gear->used && strcmp(gear->key, instance_id) == 0) {
            return gear;
        }
    }
    return NULL;
}

static bool bag_contains(const GameState *state, const char *instance_id) {
    if (!state || !instance_id || instance_id[0] == '\0') {
        return false;
    }
    for (int i = 0; i < state->inventory_bag_order_count; ++i) {
        if (strcmp(state->inventory_bag_order[i], instance_id) == 0) {
            return true;
        }
    }
    return false;
}

static const char *equipped_instance_for_slot(const GameState *state, game_item_slot_t slot) {
    if (!state) {
        return NULL;
    }
    switch (slot) {
        case GAME_ITEM_SLOT_WEAPON:
            return state->has_equipment_weapon_instance_id ? state->equipment_weapon_instance_id : NULL;
        case GAME_ITEM_SLOT_OFFHAND:
            return state->has_equipment_offhand_instance_id ? state->equipment_offhand_instance_id : NULL;
        case GAME_ITEM_SLOT_HEAD:
            return state->has_equipment_head_instance_id ? state->equipment_head_instance_id : NULL;
        case GAME_ITEM_SLOT_ARMOUR:
            return state->has_equipment_armour_instance_id ? state->equipment_armour_instance_id : NULL;
        case GAME_ITEM_SLOT_HANDS:
            return state->has_equipment_hands_instance_id ? state->equipment_hands_instance_id : NULL;
        case GAME_ITEM_SLOT_WAIST:
            return state->has_equipment_waist_instance_id ? state->equipment_waist_instance_id : NULL;
        case GAME_ITEM_SLOT_LEGS:
            return state->has_equipment_legs_instance_id ? state->equipment_legs_instance_id : NULL;
        case GAME_ITEM_SLOT_FEET:
            return state->has_equipment_feet_instance_id ? state->equipment_feet_instance_id : NULL;
        case GAME_ITEM_SLOT_NECK:
            return state->has_equipment_neck_instance_id ? state->equipment_neck_instance_id : NULL;
        case GAME_ITEM_SLOT_RING_LEFT:
            return state->has_equipment_ring_left_instance_id ? state->equipment_ring_left_instance_id : NULL;
        case GAME_ITEM_SLOT_RING_RIGHT:
            return state->has_equipment_ring_right_instance_id ? state->equipment_ring_right_instance_id : NULL;
        case GAME_ITEM_SLOT_RELIC:
            return state->has_equipment_charm_instance_id ? state->equipment_charm_instance_id : NULL;
        case GAME_ITEM_SLOT_NONE:
        default:
            return NULL;
    }
}

static bool is_equipped(const GameState *state, const char *instance_id) {
    if (!state || !instance_id || instance_id[0] == '\0') {
        return false;
    }
    for (int i = 0; i < game_content_equipment_slot_count(); ++i) {
        const game_equipment_slot_definition_t *slot = game_content_equipment_slot_at(i);
        const char *equipped = slot ? equipped_instance_for_slot(state, slot->slot) : NULL;
        if (equipped && strcmp(equipped, instance_id) == 0) {
            return true;
        }
    }
    return false;
}

static const game_item_definition_t *gear_definition(const GameState *state, const char *instance_id) {
    const GameGearInstance *gear = find_gear(state, instance_id);
    return gear ? game_content_find_item(gear->def_id) : NULL;
}

static bool item_belongs_to_quest_tab(const game_item_definition_t *item) {
    return item && (item->kind == GAME_ITEM_KIND_QUEST_ITEM || item->kind == GAME_ITEM_KIND_CLUE);
}

static const GameStackInstance *find_stack(const GameState *state, const char *stack_id) {
    if (!state || !stack_id || stack_id[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_STACK_INSTANCES; ++i) {
        const GameStackInstance *stack = &state->inventory_stack_instances[i];
        if (stack->used && strcmp(stack->key, stack_id) == 0) {
            return stack;
        }
    }
    return NULL;
}

static const game_item_definition_t *stack_definition(const GameStackInstance *stack) {
    return stack && stack->def_id[0] != '\0' ? game_content_find_item(stack->def_id) : NULL;
}

static RB_UNUSED_FN int quest_stack_count(const GameState *state) {
    if (!state) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_STACK_INSTANCES; ++i) {
        const GameStackInstance *stack = &state->inventory_stack_instances[i];
        if (stack->used && item_belongs_to_quest_tab(stack_definition(stack))) {
            ++count;
        }
    }
    return count;
}

static RB_UNUSED_FN const GameStackInstance *quest_stack_at(const GameState *state, int visible_index) {
    if (!state || visible_index < 0) {
        return NULL;
    }
    int seen = 0;
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_STACK_INSTANCES; ++i) {
        const GameStackInstance *stack = &state->inventory_stack_instances[i];
        if (!stack->used || !item_belongs_to_quest_tab(stack_definition(stack))) {
            continue;
        }
        if (seen == visible_index) {
            return stack;
        }
        ++seen;
    }
    return NULL;
}

static void stat_line(const game_item_definition_t *item, char *out, size_t out_cap) {
    if (!out || out_cap == 0U) {
        return;
    }
    if (!item) {
        (void)snprintf(out, out_cap, "Выбери предмет в рюкзаке или слот на герое");
        return;
    }
    if (item->stats.weapon_damage != 0) {
        (void)snprintf(out, out_cap, "+%d урон", item->stats.weapon_damage);
    } else if (item->stats.protection != 0) {
        (void)snprintf(out, out_cap, "+%d защита", item->stats.protection);
    } else if (item->stats.vitality != 0) {
        (void)snprintf(out, out_cap, "+%d живучесть", item->stats.vitality);
    } else if (item->stats.strength != 0) {
        (void)snprintf(out, out_cap, "+%d сила", item->stats.strength);
    } else if (item->stats.intuition != 0) {
        (void)snprintf(out, out_cap, "+%d чутье", item->stats.intuition);
    } else {
        (void)snprintf(out, out_cap, "Без боевого бонуса");
    }
}

static void select_first_bag_item(const GameState *state) {
    if (!state || s_inventory_tab != EQUIPMENT_TAB_GEAR || s_selected_instance_id[0] != '\0') {
        return;
    }
    for (int i = 0; i < state->inventory_bag_order_count; ++i) {
        if (find_gear(state, state->inventory_bag_order[i])) {
            (void)snprintf(s_selected_instance_id, sizeof s_selected_instance_id, "%s", state->inventory_bag_order[i]);
            return;
        }
    }
}

static void clear_missing_selection(const GameState *state) {
    if (s_selected_instance_id[0] != '\0' &&
        (!find_gear(state, s_selected_instance_id) ||
         (!bag_contains(state, s_selected_instance_id) && !is_equipped(state, s_selected_instance_id)))) {
        s_selected_instance_id[0] = '\0';
        equipment_detail_set_open(false);
    }
    if (s_selected_stack_id[0] != '\0') {
        const GameStackInstance *stack = find_stack(state, s_selected_stack_id);
        if (!stack || !item_belongs_to_quest_tab(stack_definition(stack))) {
            s_selected_stack_id[0] = '\0';
            equipment_detail_set_open(false);
        }
    }
}

static void slot_token_ui(nt_ui_context_t *ctx, const GameState *state, int index, const game_equipment_slot_definition_t *slot,
                          bool portrait, bool compact) {
    if (!slot) {
        return;
    }
    const char *instance_id = equipped_instance_for_slot(state, slot->slot);
    const bool filled = instance_id && instance_id[0] != '\0';
    const bool selected = filled && strcmp(instance_id, s_selected_instance_id) == 0;
    const game_item_definition_t *selected_item = gear_definition(state, s_selected_instance_id);
    const bool target = selected_item && selected_item->slot == slot->slot && bag_contains(state, s_selected_instance_id);
    const float w = compact ? (portrait ? 40.0F : 48.0F) : (portrait ? 48.0F : 54.0F);
    const float h = compact ? (portrait ? 40.0F : 48.0F) : (portrait ? 48.0F : 54.0F);
    Clay_ElementId clay_id = CLAY_IDI("gear_screen/slot", index);
    CLAY({.id = clay_id,
          .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        const int16_t hit_pad[4] = {2, 2, 2, 2};
        nt_ui_widget_register(ctx, clay_id.id, &EQUIPMENT_WIDGET_DEF, hit_pad, filled);
        const nt_ui_events_t events = nt_ui_events_padded(ctx, clay_id.id, NULL, hit_pad);
        equipment_cell_art_ui(ctx, w, filled ? gear_definition(state, instance_id) : NULL, slot->slot, !filled);
        if (selected) {
            equipment_cell_state_overlay(w, (Clay_Color){255.0F, 203.0F, 101.0F, 245.0F}, (Clay_Color){92.0F, 48.0F, 18.0F, 68.0F});
        } else if (target) {
            equipment_cell_state_overlay(w, (Clay_Color){238.0F, 163.0F, 74.0F, 230.0F}, (Clay_Color){84.0F, 45.0F, 18.0F, 56.0F});
        }
        if (events.clicked && filled) {
            (void)snprintf(s_selected_instance_id, sizeof s_selected_instance_id, "%s", instance_id);
            s_detail_open = true;
            game_audio_play(GAME_AUDIO_CUE_GEAR_SELECT);
        }
    }
}

static void slot_at_ui(nt_ui_context_t *ctx, const GameState *state, int index, bool portrait, bool compact) {
    slot_token_ui(ctx, state, index, game_content_equipment_slot_at(index), portrait, compact);
}

static void hero_silhouette_ui(nt_ui_context_t *ctx, bool portrait) {
    const float doll_w = portrait ? 98.0F : 116.0F;
    const float doll_h = portrait ? 138.0F : 152.0F;
    const float hero_w = portrait ? 94.0F : 106.0F;
    const float hero_h = portrait ? 130.0F : 147.0F;
    const float hero_offset = portrait ? 5.0F : 6.0F;
    CLAY({.id = CLAY_ID("gear_screen/doll"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(doll_w), CLAY_SIZING_FIXED(doll_h)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {12.0F, 9.0F, 7.0F, 230.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .border = {.color = {133.0F, 91.0F, 49.0F, 195.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(doll_w - 8.0F), CLAY_SIZING_FIXED(doll_h - 8.0F)},
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
              .backgroundColor = {20.0F, 14.0F, 10.0F, 132.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .border = {.color = {72.0F, 49.0F, 29.0F, 105.0F}, .width = {1, 1, 1, 1, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_ART)}) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(hero_w + hero_offset), CLAY_SIZING_FIXED(hero_h)},
                             .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER}}}) {
                equipment_art_image_box_opacity(ctx, EQUIP_ART_HERO, hero_w, hero_h, 0xFFFFFFFFU, LAYER_ICON, 1.0F);
            }
        }
        CLAY({.floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                                            .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM},
                           .offset = {hero_offset, -8.0F},
                           .zIndex = 1},
              .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? 72.0F : 80.0F), CLAY_SIZING_FIXED(8.0F)}},
              .backgroundColor = {5.0F, 4.0F, 3.0F, 128.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(4),
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {}
    }
}

static void equipment_doll_ui(nt_ui_context_t *ctx, const GameState *state, bool portrait) {
    const nt_ui_label_style_t title = label_style(portrait ? 15.0F : 17.0F, 246.0F, 222.0F, 176.0F, 255.0F);
    const bool compact = portrait;
    const int count = game_content_equipment_slot_count();
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), portrait ? CLAY_SIZING_FIT(0) : CLAY_SIZING_GROW(0)},
                     .padding = {.left = 6, .right = 6, .top = 6, .bottom = 6},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 4 : 6,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {18.0F, 13.0F, 10.0F, 205.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = {123.0F, 87.0F, 49.0F, 180.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        text_label(ctx, "Герой", &title);
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 4,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            if (count > 2) {
                slot_at_ui(ctx, state, 2, portrait, compact);
            }
            if (count > 8) {
                slot_at_ui(ctx, state, 8, portrait, compact);
            }
        }
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = portrait ? 3 : 6,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = portrait ? 3 : 5,
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                if (count > 0) {
                    slot_at_ui(ctx, state, 0, portrait, compact);
                }
                if (count > 4) {
                    slot_at_ui(ctx, state, 4, portrait, compact);
                }
                if (count > 9) {
                    slot_at_ui(ctx, state, 9, portrait, compact);
                }
            }
            hero_silhouette_ui(ctx, portrait);
            CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = portrait ? 3 : 5,
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
                if (count > 1) {
                    slot_at_ui(ctx, state, 1, portrait, compact);
                }
                if (count > 5) {
                    slot_at_ui(ctx, state, 5, portrait, compact);
                }
                if (count > 10) {
                    slot_at_ui(ctx, state, 10, portrait, compact);
                }
            }
        }
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = portrait ? 3 : 5,
                         .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
            if (count > 3) {
                slot_at_ui(ctx, state, 3, portrait, compact);
            }
            if (count > 6) {
                slot_at_ui(ctx, state, 6, portrait, compact);
            }
            if (count > 7) {
                slot_at_ui(ctx, state, 7, portrait, compact);
            }
            if (count > 11) {
                slot_at_ui(ctx, state, 11, portrait, compact);
            }
        }
    }
}

static RB_UNUSED_FN void equipment_select_tab(equipment_inventory_tab_t tab) {
    if (s_inventory_tab == tab) {
        return;
    }
    s_inventory_tab = tab;
    s_selected_instance_id[0] = '\0';
    s_selected_stack_id[0] = '\0';
    equipment_detail_set_open(false);
    equipment_request_state_cleanup();
}

static void bag_cell_ui(nt_ui_context_t *ctx, const GameState *state, int index, const char *instance_id, float size) {
    const bool selected = instance_id && strcmp(instance_id, s_selected_instance_id) == 0;
    const game_item_definition_t *item = gear_definition(state, instance_id);
    const equipment_art_region_t art = equipment_item_art(item);
    const float icon_size = size * 0.78F;
    Clay_ElementId clay_id = CLAY_IDI("gear_screen/bag_cell", index);
    CLAY({.id = clay_id,
          .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {13.0F, 10.0F, 8.0F, 170.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(2),
          .border = {.color = {105.0F, 75.0F, 42.0F, 118.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_ART)}) {
        const int16_t hit_pad[4] = {2, 2, 2, 2};
        nt_ui_widget_register(ctx, clay_id.id, &EQUIPMENT_WIDGET_DEF, hit_pad, true);
        const nt_ui_events_t events = nt_ui_events_padded(ctx, clay_id.id, NULL, hit_pad);
        if (art < EQUIP_ART_COUNT) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(icon_size), CLAY_SIZING_FIXED(icon_size)}}}) {
                equipment_art_image(ctx, art, icon_size, 0xFFFFFFFFU, LAYER_ICON);
            }
        }
        if (selected) {
            equipment_cell_state_overlay(size, (Clay_Color){255.0F, 203.0F, 101.0F, 245.0F}, (Clay_Color){92.0F, 48.0F, 18.0F, 68.0F});
        }
        if (events.clicked && instance_id) {
            (void)snprintf(s_selected_instance_id, sizeof s_selected_instance_id, "%s", instance_id);
            s_detail_open = true;
            game_audio_play(GAME_AUDIO_CUE_GEAR_SELECT);
        }
    }
}

static void quest_stack_cell_ui(nt_ui_context_t *ctx, int index, const GameStackInstance *stack, float size) {
    const game_item_definition_t *item = stack_definition(stack);
    const bool selected = stack && strcmp(stack->key, s_selected_stack_id) == 0;
    const equipment_art_region_t art = equipment_item_art(item);
    const float icon_size = size * 0.78F;
    Clay_ElementId clay_id = CLAY_IDI("gear_screen/quest_cell", index);
    CLAY({.id = clay_id,
          .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {13.0F, 10.0F, 8.0F, 174.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(2),
          .border = {.color = {124.0F, 90.0F, 48.0F, 132.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_ART)}) {
        const int16_t hit_pad[4] = {2, 2, 2, 2};
        nt_ui_widget_register(ctx, clay_id.id, &EQUIPMENT_WIDGET_DEF, hit_pad, stack != NULL);
        const nt_ui_events_t events = nt_ui_events_padded(ctx, clay_id.id, NULL, hit_pad);
        if (art < EQUIP_ART_COUNT) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(icon_size), CLAY_SIZING_FIXED(icon_size)}}}) {
                equipment_art_image(ctx, art, icon_size, 0xFFFFFFFFU, LAYER_ICON);
            }
        }
        if (stack && stack->count > 1) {
            char count_buf[16];
            (void)snprintf(count_buf, sizeof count_buf, "%d", stack->count);
            const nt_ui_label_style_t count_style = label_style(9.0F, 255.0F, 238.0F, 202.0F, 255.0F);
            CLAY({.floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                               .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_BOTTOM,
                                                .parent = CLAY_ATTACH_POINT_RIGHT_BOTTOM},
                               .offset = {-2.0F, -2.0F},
                               .zIndex = 5},
                  .layout = {.sizing = {CLAY_SIZING_FIXED(20.0F), CLAY_SIZING_FIXED(15.0F)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = {42.0F, 26.0F, 12.0F, 230.0F},
                  .cornerRadius = CLAY_CORNER_RADIUS(3),
                  .border = {.color = {184.0F, 128.0F, 62.0F, 190.0F}, .width = {1, 1, 1, 1, 0}},
                  .userData = NT_UI_CLAY_DATA(LAYER_ICON)}) {
                text_label(ctx, count_buf, &count_style);
            }
        }
        if (selected) {
            equipment_cell_state_overlay(size, (Clay_Color){255.0F, 203.0F, 101.0F, 245.0F}, (Clay_Color){92.0F, 48.0F, 18.0F, 68.0F});
        }
        if (events.clicked && stack) {
            (void)snprintf(s_selected_stack_id, sizeof s_selected_stack_id, "%s", stack->key);
            s_selected_instance_id[0] = '\0';
            s_detail_open = true;
            game_audio_play(GAME_AUDIO_CUE_GEAR_SELECT);
        }
    }
}

static void empty_bag_cell_ui(nt_ui_context_t *ctx, int index, float size) {
    CLAY({.id = CLAY_IDI("gear_screen/bag_empty", index),
          .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {13.0F, 10.0F, 8.0F, 150.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(2),
          .border = {.color = {96.0F, 68.0F, 38.0F, 104.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_ART)}) {}
}

static int item_stat_value(const game_item_definition_t *item, int stat_index) {
    if (!item) {
        return 0;
    }
    switch (stat_index) {
        case 0:
            return item->stats.weapon_damage + item->stats.bonus_attack_power;
        case 1:
            return item->stats.protection;
        case 2:
            return item->stats.vitality;
        case 3:
            return item->stats.strength;
        case 4:
            return item->stats.intuition;
        default:
            return 0;
    }
}

static const char *stat_name_at(int stat_index) {
    switch (stat_index) {
        case 0:
            return "Урон";
        case 1:
            return "Защита";
        case 2:
            return "Жив.";
        case 3:
            return "Сила";
        case 4:
            return "Чутье";
        default:
            return "";
    }
}

static void equipment_apply_selected(World *w, GameState *state) {
    if (!w || !state || s_selected_instance_id[0] == '\0' || !bag_contains(state, s_selected_instance_id)) {
        return;
    }
    const bool equipped = game_actions_equip_gear(state, s_selected_instance_id);
    if (equipped) {
        game_audio_play(GAME_AUDIO_CUE_GEAR_EQUIP);
        s_selected_instance_id[0] = '\0';
        equipment_detail_set_open(false);
    }
    if (equipped && state->has_quests_tracked_quest_id) {
        const GameQuestState *quest = NULL;
        for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
            if (state->quests_quest_states[i].used &&
                strcmp(state->quests_quest_states[i].key, state->quests_tracked_quest_id) == 0) {
                quest = &state->quests_quest_states[i];
                break;
            }
        }
        w->first_scene.active_quest_id = state->quests_tracked_quest_id;
        w->first_scene.active_quest_current_step_id = quest && quest->has_current_step_id ? quest->current_step_id : NULL;
    }
}

static void item_compare_column_ui(nt_ui_context_t *ctx, const char *title_text, const game_item_definition_t *item,
                                   bool portrait) {
    const nt_ui_label_style_t title = label_style(portrait ? 9.5F : 11.0F, 190.0F, 166.0F, 125.0F, 255.0F);
    const nt_ui_label_style_t name = label_style(portrait ? 11.5F : 14.0F, 252.0F, 232.0F, 190.0F, 255.0F);
    const nt_ui_label_style_t meta = label_style(portrait ? 8.5F : 10.0F, 188.0F, 165.0F, 128.0F, 255.0F);
    char stat_buf[96];
    stat_line(item, stat_buf, sizeof stat_buf);
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .padding = {.left = 7, .right = 7, .top = 6, .bottom = 6},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 5,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {22.0F, 16.0F, 12.0F, 218.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .border = {.color = {112.0F, 78.0F, 42.0F, 170.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        text_label(ctx, title_text, &title);
        text_label(ctx, item && item->display_name ? item->display_name : "Пусто", &name);
        text_label(ctx, item ? stat_buf : "Слот свободен", &meta);
    }
}

static bool item_compare_row_ui(nt_ui_context_t *ctx, int stat_index, const game_item_definition_t *next,
                                const game_item_definition_t *current, bool portrait) {
    const int next_value = item_stat_value(next, stat_index);
    const int current_value = item_stat_value(current, stat_index);
    const int delta = next_value - current_value;
    if (delta == 0) {
        return false;
    }
    char row[96];
    (void)snprintf(row, sizeof row, "%s  %d -> %d  (%+d)", stat_name_at(stat_index), current_value, next_value, delta);
    const nt_ui_label_style_t row_style =
        delta > 0 ? label_style(portrait ? 9.5F : 11.0F, 177.0F, 226.0F, 142.0F, 255.0F)
                  : label_style(portrait ? 9.5F : 11.0F, 231.0F, 129.0F, 102.0F, 255.0F);
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 20.0F : 23.0F)},
                     .padding = {.left = 7, .right = 7, .top = 2, .bottom = 2},
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {17.0F, 12.0F, 9.0F, 178.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(3),
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        text_label(ctx, row, &row_style);
    }
    return true;
}

static int item_compare_changed_stat_count(const game_item_definition_t *next, const game_item_definition_t *current) {
    int changed = 0;
    for (int i = 0; i < 5; ++i) {
        if (item_stat_value(next, i) != item_stat_value(current, i)) {
            ++changed;
        }
    }
    return changed;
}

static void selected_stack_item_modal_ui(nt_ui_context_t *ctx, const GameState *state, bool portrait) {
    const GameStackInstance *stack = find_stack(state, s_selected_stack_id);
    const game_item_definition_t *item = stack_definition(stack);
    if (!s_detail_open || !stack || !item_belongs_to_quest_tab(item)) {
        return;
    }

    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    const float modal_w = portrait ? clamp_f(layout_w - 44.0F, 286.0F, 340.0F) : clamp_f(layout_w * 0.44F, 340.0F, 430.0F);
    const float modal_h = portrait ? 272.0F : 248.0F;
    const nt_ui_label_style_t title = label_style(portrait ? 17.0F : 19.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    const nt_ui_label_style_t meta = label_style(portrait ? 11.0F : 12.0F, 211.0F, 180.0F, 136.0F, 255.0F);
    const nt_ui_label_style_t body = label_style(portrait ? 11.0F : 12.0F, 232.0F, 210.0F, 172.0F, 255.0F);
    const nt_ui_label_style_t locked = label_style(portrait ? 11.0F : 12.0F, 230.0F, 156.0F, 103.0F, 255.0F);
    char count_buf[48];
    (void)snprintf(count_buf, sizeof count_buf, "Количество: %d", stack->count);

    bool modal_open = s_detail_open;
    nt_ui_modal_style_t modal_style = game_modal_style((nt_ui_layer_t)LAYER_BG, true);
    modal_style.backdrop_alpha = 0.42F;
    if (!game_modal_visible(ctx, EQUIPMENT_ITEM_MODAL_ID, &modal_style, &modal_open, false)) {
        equipment_detail_set_open(modal_open);
        equipment_clear_transient_ui_state(ctx);
        return;
    }
    CLAY({.id = CLAY_ID("gear_screen/quest_item_modal_scrim"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("gear_screen/quest_item_modal"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(modal_w), CLAY_SIZING_FIXED(modal_h)}},
              .backgroundColor = {16.0F, 10.0F, 7.0F, 252.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(5),
              .border = {.color = {202.0F, 137.0F, 65.0F, 228.0F}, .width = {1, 1, 1, 1, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            nt_ui_image_style_t panel_body = game_modal_body_image(portrait);
            nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), game_modal_art(GAME_MODAL_ART_BODY_PANEL), &panel_body,
                              &(Clay_ElementDeclaration){
                                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                             .padding = CLAY_PADDING_ALL(portrait ? 8 : 10),
                                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                             .childGap = portrait ? 7 : 8,
                                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 8,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
                    text_label(ctx, item->display_name ? item->display_name : item->id, &title);
                }
                if (game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_BG, (nt_ui_layer_t)LAYER_TEXT,
                                            "gear_screen/quest_item_modal_close", portrait)) {
                    equipment_detail_set_open(false);
                }
            }
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 10,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                equipment_cell_art_ui(ctx, portrait ? 56.0F : 62.0F, item, GAME_ITEM_SLOT_NONE, false);
                CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                 .childGap = 4,
                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                    text_label(ctx, item->category_label ? item->category_label : "Квестовый предмет", &meta);
                    text_label(ctx, count_buf, &meta);
                    text_label(ctx, "Не продается", &locked);
                }
            }
            text_label(ctx, item->description ? item->description : "Хранится отдельно от снаряжения.", &body);
            nt_ui_panel_end(ctx);
        }
    }
    nt_ui_modal_end(ctx);
    equipment_detail_set_open(modal_open);
    equipment_clear_transient_ui_state(ctx);
}

static void selected_item_modal_ui(nt_ui_context_t *ctx, World *w, bool portrait) {
    GameState *state = w ? w->player_state : NULL;
    if (s_inventory_tab == EQUIPMENT_TAB_QUEST) {
        selected_stack_item_modal_ui(ctx, state, portrait);
        return;
    }
    const game_item_definition_t *selected_item = gear_definition(state, s_selected_instance_id);
    if (!s_detail_open || !state || !selected_item) {
        return;
    }
    const char *current_instance = equipped_instance_for_slot(state, selected_item->slot);
    const game_item_definition_t *current_item =
        current_instance && strcmp(current_instance, s_selected_instance_id) != 0 ? gear_definition(state, current_instance) : NULL;
    const bool can_equip = bag_contains(state, s_selected_instance_id);
    const int changed_stats = item_compare_changed_stat_count(selected_item, current_item);
    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    const float modal_w = portrait ? clamp_f(layout_w - 44.0F, 286.0F, 340.0F) : clamp_f(layout_w * 0.48F, 340.0F, 430.0F);
    const float modal_h = portrait ? clamp_f(246.0F + (float)changed_stats * 22.0F, 266.0F, 318.0F)
                                   : clamp_f(218.0F + (float)changed_stats * 25.0F, 238.0F, 294.0F);
    const nt_ui_label_style_t title = label_style(portrait ? 17.0F : 19.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    const nt_ui_label_style_t action = label_style(portrait ? 14.0F : 16.0F, 255.0F, 239.0F, 206.0F, 255.0F);
    const nt_ui_label_style_t no_change = label_style(portrait ? 9.5F : 11.0F, 196.0F, 172.0F, 132.0F, 255.0F);

    bool modal_open = s_detail_open;
    nt_ui_modal_style_t modal_style = game_modal_style((nt_ui_layer_t)LAYER_BG, true);
    modal_style.backdrop_alpha = 0.42F;
    if (!game_modal_visible(ctx, EQUIPMENT_ITEM_MODAL_ID, &modal_style, &modal_open, false)) {
        equipment_detail_set_open(modal_open);
        equipment_clear_transient_ui_state(ctx);
        return;
    }
    CLAY({.id = CLAY_ID("gear_screen/item_modal_scrim"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("gear_screen/item_modal"),
              .layout = {.sizing = {CLAY_SIZING_FIXED(modal_w), CLAY_SIZING_FIXED(modal_h)}},
              .backgroundColor = {16.0F, 10.0F, 7.0F, 252.0F},
              .cornerRadius = CLAY_CORNER_RADIUS(5),
              .border = {.color = {202.0F, 137.0F, 65.0F, 228.0F}, .width = {1, 1, 1, 1, 0}},
              .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
            nt_ui_image_style_t body = game_modal_body_image(portrait);
            nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), game_modal_art(GAME_MODAL_ART_BODY_PANEL), &body,
                              &(Clay_ElementDeclaration){
                                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                             .padding = CLAY_PADDING_ALL(portrait ? 8 : 10),
                                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                             .childGap = portrait ? 7 : 8,
                                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 8,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
                    text_label(ctx, "Сравнение", &title);
                }
                if (game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_BG, (nt_ui_layer_t)LAYER_TEXT,
                                            "gear_screen/item_modal_close", portrait)) {
                    equipment_detail_set_open(false);
                }
            }
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 7,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                item_compare_column_ui(ctx, "Выбрано", selected_item, portrait);
                item_compare_column_ui(ctx, "Сейчас", current_item, portrait);
            }
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 3,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                bool drew_change = false;
                for (int i = 0; i < 5; ++i) {
                    drew_change = item_compare_row_ui(ctx, i, selected_item, current_item, portrait) || drew_change;
                }
                if (!drew_change) {
                    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 20.0F : 23.0F)},
                                     .padding = {.left = 7, .right = 7, .top = 2, .bottom = 2},
                                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}},
                          .backgroundColor = {17.0F, 12.0F, 9.0F, 178.0F},
                          .cornerRadius = CLAY_CORNER_RADIUS(3),
                          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                        text_label(ctx, "РҐР°СЂР°РєС‚РµСЂРёСЃС‚РёРєРё РЅРµ РјРµРЅСЏСЋС‚СЃСЏ", &no_change);
                    }
                }
            }
            Clay_ElementId equip_id = CLAY_ID("gear_screen/equip_button");
            CLAY({.id = equip_id,
                  .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 40.0F : 44.0F)},
                             .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
                  .backgroundColor = can_equip ? (Clay_Color){112.0F, 62.0F, 32.0F, 238.0F}
                                               : (Clay_Color){56.0F, 44.0F, 34.0F, 190.0F},
                  .cornerRadius = CLAY_CORNER_RADIUS(5),
                  .border = {.color = can_equip ? (Clay_Color){211.0F, 147.0F, 70.0F, 230.0F}
                                                 : (Clay_Color){100.0F, 80.0F, 58.0F, 150.0F},
                             .width = {1, 1, 1, 1, 0}},
                  .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
                const int16_t hit_pad[4] = {2, 2, 2, 2};
                nt_ui_widget_register(ctx, equip_id.id, &EQUIPMENT_WIDGET_DEF, hit_pad, can_equip);
                const nt_ui_events_t events = nt_ui_events_padded(ctx, equip_id.id, NULL, hit_pad);
                text_label(ctx, can_equip ? "Надеть" : "Надето", &action);
                if (events.clicked && can_equip) {
                    equipment_apply_selected(w, state);
                }
            }
            nt_ui_panel_end(ctx);
        }
    }
    nt_ui_modal_end(ctx);
    equipment_detail_set_open(modal_open);
    equipment_clear_transient_ui_state(ctx);
}

static void stat_row_ui(nt_ui_context_t *ctx, const char *name, int value, const nt_ui_label_style_t *label,
                        const nt_ui_label_style_t *number) {
    char value_buf[32];
    (void)snprintf(value_buf, sizeof value_buf, "%d", value);
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 6,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
            text_label(ctx, name, label);
        }
        text_label(ctx, value_buf, number);
    }
}

static RB_UNUSED_FN void player_stats_ui(nt_ui_context_t *ctx, const GameState *state, bool portrait) {
    game_combat_stats_t stats;
    const bool has_stats = game_combat_build_player_stats(state, &stats);
    const nt_ui_label_style_t title = label_style(portrait ? 14.0F : 17.0F, 246.0F, 222.0F, 176.0F, 255.0F);
    const nt_ui_label_style_t label = label_style(portrait ? 9.5F : 11.0F, 190.0F, 166.0F, 125.0F, 255.0F);
    const nt_ui_label_style_t number = label_style(portrait ? 10.5F : 12.0F, 253.0F, 226.0F, 173.0F, 255.0F);
    const float min_w = portrait ? 96.0F : 150.0F;
    CLAY({.id = CLAY_ID("gear_screen/player_stats"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(min_w), portrait ? CLAY_SIZING_FIT(0) : CLAY_SIZING_GROW(0)},
                     .padding = {.left = 8, .right = 8, .top = 7, .bottom = 7},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 4 : 6,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {20.0F, 14.0F, 10.0F, 214.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = {123.0F, 87.0F, 49.0F, 180.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        text_label(ctx, "Статы", &title);
        if (has_stats) {
            stat_row_ui(ctx, "HP", state ? state->hero_hp : 0, &label, &number);
            stat_row_ui(ctx, "Урон", game_combat_attack_power(&stats), &label, &number);
            stat_row_ui(ctx, "Защита", stats.protection, &label, &number);
            stat_row_ui(ctx, "Жив.", stats.vitality, &label, &number);
            stat_row_ui(ctx, "Сила", stats.strength, &label, &number);
            stat_row_ui(ctx, "Чутье", stats.intuition, &label, &number);
        } else {
            text_label(ctx, "-", &label);
        }
    }
}

static void equipment_right_column_ui(nt_ui_context_t *ctx, World *w, bool portrait) {
    const float width = portrait ? 126.0F : 152.0F;
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 5 : 7,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
        player_stats_ui(ctx, w ? w->player_state : NULL, portrait);
    }
}

static void inventory_tab_button(nt_ui_context_t *ctx, const char *id_suffix, const char *label,
                                 equipment_inventory_tab_t tab, bool portrait) {
    const bool selected = s_inventory_tab == tab;
    Clay_ElementId tab_id =
        tab == EQUIPMENT_TAB_QUEST ? CLAY_ID("gear_screen/tab/quest") : CLAY_ID("gear_screen/tab/gear");
    const nt_ui_label_style_t text = label_style(portrait ? 11.5F : 13.0F, selected ? 255.0F : 190.0F,
                                                 selected ? 230.0F : 166.0F, selected ? 183.0F : 124.0F, 255.0F);
    CLAY({.id = tab_id,
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(portrait ? 32.0F : 34.0F)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = selected ? (Clay_Color){76.0F, 45.0F, 22.0F, 228.0F}
                                      : (Clay_Color){24.0F, 17.0F, 12.0F, 176.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = selected ? (Clay_Color){194.0F, 132.0F, 63.0F, 220.0F}
                                       : (Clay_Color){94.0F, 67.0F, 39.0F, 132.0F},
                     .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        const int16_t hit_pad[4] = {2, 2, 2, 2};
        nt_ui_widget_register(ctx, tab_id.id, &EQUIPMENT_WIDGET_DEF, hit_pad, true);
        const nt_ui_events_t events = nt_ui_events_padded(ctx, tab_id.id, NULL, hit_pad);
        text_label(ctx, label, &text);
        if (events.clicked) {
            equipment_select_tab(tab);
        }
    }
    (void)id_suffix;
}

static void inventory_tabs_ui(nt_ui_context_t *ctx, bool portrait) {
    CLAY({.id = CLAY_ID("gear_screen/tabs"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 5,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        inventory_tab_button(ctx, "gear", "Снаряжение", EQUIPMENT_TAB_GEAR, portrait);
        inventory_tab_button(ctx, "quest", "Квестовое", EQUIPMENT_TAB_QUEST, portrait);
    }
}

static void backpack_grid_ui(nt_ui_context_t *ctx, World *w, bool portrait, float available_w) {
    GameState *state = w ? w->player_state : NULL;
    const nt_ui_label_style_t title = label_style(portrait ? 15.0F : 17.0F, 246.0F, 222.0F, 176.0F, 255.0F);
    const nt_ui_label_style_t meta = label_style(portrait ? 10.0F : 11.0F, 188.0F, 165.0F, 128.0F, 255.0F);
    const int columns = backpack_columns_for_width(available_w, portrait);
    const float cell_size = backpack_cell_size_for_layout(available_w, columns, portrait);
    const float grid_gap = backpack_cell_gap(portrait);
    const float grid_inset = backpack_grid_inset(portrait);
    const bool quest_tab = s_inventory_tab == EQUIPMENT_TAB_QUEST;
    const int count = quest_tab ? quest_stack_count(state) : (state ? state->inventory_bag_order_count : 0);
    const int capacity = quest_tab ? GAME_STATE_MAX_INVENTORY_STACK_INSTANCES : GAME_STATE_MAX_INVENTORY_BAG_ORDER;
    int visible_cells = quest_tab ? columns * 2 : capacity;
    if (count > visible_cells) {
        visible_cells = ((count + columns - 1) / columns) * columns;
    }
    if (visible_cells > capacity) {
        visible_cells = capacity;
    }
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = CLAY_PADDING_ALL((uint16_t)BACKPACK_PANEL_PADDING),
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 6,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {21.0F, 15.0F, 11.0F, 210.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = {125.0F, 88.0F, 48.0F, 175.0F}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 8,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            text_label(ctx, quest_tab ? "Квестовые предметы" : "Рюкзак", &title);
            char cap_buf[32];
            (void)snprintf(cap_buf, sizeof cap_buf, "%d/%d", count, capacity);
            text_label(ctx, cap_buf, &meta);
        }
        inventory_tabs_ui(ctx, portrait);
        nt_ui_scroll_style_t scroll_style = nt_ui_scroll_style_defaults();
        scroll_style.bar_visibility = NT_UI_SCROLLBAR_AUTO_HIDE;
        scroll_style.scroll_x = false;
        scroll_style.scroll_y = true;
        nt_ui_scroll_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG),
                           quest_tab ? nt_ui_id("gear_screen/quest_scroll") : nt_ui_id("gear_screen/backpack_scroll"),
                           &scroll_style,
                           &(Clay_ElementDeclaration){
                               .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                          .padding = {.left = (uint16_t)grid_inset,
                                                      .right = (uint16_t)(grid_inset + 8.0F),
                                                      .top = (uint16_t)grid_inset,
                                                      .bottom = (uint16_t)grid_inset}},
                               .backgroundColor = portrait ? (Clay_Color){0} : (Clay_Color){11.0F, 8.0F, 6.0F, 92.0F},
                               .cornerRadius = CLAY_CORNER_RADIUS(4),
                               .border = {.color = portrait ? (Clay_Color){0} : (Clay_Color){93.0F, 64.0F, 35.0F, 88.0F},
                                          .width = {portrait ? 0 : 1, portrait ? 0 : 1, portrait ? 0 : 1, portrait ? 0 : 1, 0}}});
        CLAY({.id = CLAY_ID("gear_screen/backpack_grid"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = (uint16_t)grid_gap,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
            int drawn = 0;
            for (int row = 0; row * columns < visible_cells; ++row) {
                CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                 .childGap = (uint16_t)grid_gap,
                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                    for (int col = 0; col < columns; ++col) {
                        const int index = row * columns + col;
                        if (index >= visible_cells) {
                            break;
                        }
                        if (quest_tab) {
                            const GameStackInstance *stack = quest_stack_at(state, index);
                            if (stack) {
                                quest_stack_cell_ui(ctx, index, stack, cell_size);
                                ++drawn;
                            } else {
                                empty_bag_cell_ui(ctx, index, cell_size);
                            }
                        } else {
                            const char *instance_id = state && index < state->inventory_bag_order_count ? state->inventory_bag_order[index] : NULL;
                            if (instance_id && find_gear(state, instance_id)) {
                                bag_cell_ui(ctx, state, index, instance_id, cell_size);
                                ++drawn;
                            } else {
                                empty_bag_cell_ui(ctx, index, cell_size);
                            }
                        }
                    }
                }
            }
            (void)drawn;
        }
        nt_ui_scroll_end(ctx);
    }
}

bool equipment_screen_open(void) { return s_open; }

void equipment_screen_set_open(bool open) {
    if (open && !s_open) {
        s_dismiss_guard_frames = 2;
    }
    if (!open && (s_open || s_detail_open)) {
        equipment_request_state_cleanup();
    }
    s_open = open;
    if (!s_open) {
        s_selected_instance_id[0] = '\0';
        s_selected_stack_id[0] = '\0';
        equipment_detail_set_open(false);
        s_dismiss_guard_frames = 0;
    }
}

void equipment_screen_toggle(void) { equipment_screen_set_open(!s_open); }

static bool equipment_state_has_flag(const GameState *state, const char *flag) {
    if (!state) {
        return false;
    }
    for (int i = 0; i < state->flags_ids_count; ++i) {
        if (strcmp(state->flags_ids[i], flag) == 0) {
            return true;
        }
    }
    return false;
}

void equipment_screen_ui(nt_ui_context_t *ctx, World *w) {
    equipment_clear_transient_ui_state(ctx);
    if (!ctx || !w || w->dialogue.open || !w->player_state) {
        return;
    }
    if (!s_open && equipment_state_has_flag(w->player_state, "dev_equipment_open")) {
        equipment_screen_set_open(true);
    }
    if (!s_open) {
        return;
    }
    GameState *state = w->player_state;
    clear_missing_selection(state);
    select_first_bag_item(state);
    if (s_inventory_tab == EQUIPMENT_TAB_GEAR && !s_detail_open && s_selected_instance_id[0] != '\0' &&
        equipment_state_has_flag(state, "dev_equipment_item_modal")) {
        s_detail_open = true;
    }

    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    const bool portrait = layout_h > layout_w;
    const float max_panel_w = layout_w > (portrait ? 20.0F : 36.0F) ? layout_w - (portrait ? 20.0F : 36.0F) : layout_w;
    const float max_panel_h = layout_h > (portrait ? 96.0F : 36.0F) ? layout_h - (portrait ? 96.0F : 36.0F) : layout_h;
    const float min_panel_w = portrait ? 300.0F : 520.0F;
    const float min_panel_h = portrait ? 520.0F : 300.0F;
    const float panel_w = clamp_f(portrait ? layout_w - 20.0F : layout_w * 0.94F,
                                  min_panel_w < max_panel_w ? min_panel_w : max_panel_w, max_panel_w);
    const float panel_h = clamp_f(portrait ? layout_h - 112.0F : layout_h * 0.88F,
                                  min_panel_h < max_panel_h ? min_panel_h : max_panel_h, max_panel_h);
    const float backpack_w = equipment_backpack_available_width(panel_w, portrait);
    const nt_ui_label_style_t title = label_style(portrait ? 20.0F : 23.0F, 255.0F, 238.0F, 202.0F, 255.0F);
    bool modal_open = s_open;
    nt_ui_modal_style_t modal_style = game_modal_style((nt_ui_layer_t)LAYER_BG, true);
    const bool ignore_close_request = s_dismiss_guard_frames > 0;
    if (!game_modal_visible(ctx, EQUIPMENT_MODAL_ID, &modal_style, &modal_open, ignore_close_request)) {
        equipment_screen_set_open(modal_open);
        equipment_clear_transient_ui_state(ctx);
        return;
    }

    CLAY({.id = CLAY_ID("equipment/modal_frame"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIXED(panel_h)}},
          .backgroundColor = {13.0F, 9.0F, 7.0F, 232.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .userData = NT_UI_CLAY_DATA(LAYER_BG)}) {
        nt_ui_image_style_t panel_image = game_modal_panel_image(portrait);
        nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), game_modal_art(GAME_MODAL_ART_OUTER_FRAME), &panel_image,
                          &(Clay_ElementDeclaration){
                              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                         .padding = CLAY_PADDING_ALL(portrait ? 8 : 12),
                                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                         .childGap = portrait ? 7 : 10,
                                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
        CLAY({.id = CLAY_ID("equipment/header"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 10,
                         .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 2,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
                text_label(ctx, "Снаряжение", &title);
            }
            if (game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_BG, (nt_ui_layer_t)LAYER_TEXT, "equipment/close", portrait)) {
                modal_open = false;
            }
        }
        if (portrait) {
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .layoutDirection = CLAY_TOP_TO_BOTTOM,
                             .childGap = 6,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                                 .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                 .childGap = 6,
                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                    equipment_doll_ui(ctx, state, portrait);
                    equipment_right_column_ui(ctx, w, portrait);
                }
                CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
                    backpack_grid_ui(ctx, w, portrait, backpack_w);
                }
            }
        } else {
            CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                             .layoutDirection = CLAY_LEFT_TO_RIGHT,
                             .childGap = 8,
                             .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0)},
                                 .layoutDirection = CLAY_LEFT_TO_RIGHT,
                                 .childGap = 8,
                                 .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}}) {
                    equipment_doll_ui(ctx, state, portrait);
                    equipment_right_column_ui(ctx, w, portrait);
                }
                CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
                    backpack_grid_ui(ctx, w, portrait, backpack_w);
                }
            }
        }
        nt_ui_panel_end(ctx);
    }
    nt_ui_modal_end(ctx);
    selected_item_modal_ui(ctx, w, portrait);
    if (s_dismiss_guard_frames > 0) {
        --s_dismiss_guard_frames;
    }
    if (!modal_open) {
        equipment_screen_set_open(false);
    }
    equipment_clear_transient_ui_state(ctx);
}
