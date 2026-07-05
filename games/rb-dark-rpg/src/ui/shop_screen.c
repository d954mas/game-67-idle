#include "ui/shop_screen.h"

#include "clay.h"
#include "game_actions.h"
#include "game_audio.h"
#include "game_content.h"
#include "game_state.h"
#include "ui/game_modal.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_state.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LAYER_SHOP_BG 34
#define LAYER_SHOP_ICON 35
#define LAYER_SHOP_TEXT 36
#define SHOP_MODAL_ID 0x5100F001U
#define SHOP_SEMANTIC_ID_SLOTS 128
#define SHOP_SEMANTIC_ID_LEN 96
#define SHOP_CELL_W_DESKTOP 112.0F
#define SHOP_CELL_W_PHONE 106.0F
#define SHOP_CELL_H_DESKTOP 132.0F
#define SHOP_CELL_H_PHONE 128.0F

#if defined(__GNUC__) || defined(__clang__)
#define SHOP_UNUSED_FN __attribute__((unused))
#else
#define SHOP_UNUSED_FN
#endif

#if defined(__GNUC__) || defined(__clang__)
#define RB_DARK_RPG_UNUSED __attribute__((unused))
#else
#define RB_DARK_RPG_UNUSED
#endif

typedef enum shop_mode_t {
  SHOP_MODE_BUY = 0,
  SHOP_MODE_SELL,
  SHOP_MODE_BUYBACK,
} shop_mode_t;

typedef enum shop_list_kind_t {
  SHOP_LIST_BUY = 0,
  SHOP_LIST_SELL,
  SHOP_LIST_BUYBACK,
} shop_list_kind_t;

typedef enum shop_art_region_t {
  SHOP_ART_SLOT_CELL = 0,
  SHOP_ART_GOLD_COIN,
  SHOP_ART_ITEM_OLD_SWORD,
  SHOP_ART_ITEM_PADDED_JACKET,
  SHOP_ART_ITEM_LEATHER_GREAVES,
  SHOP_ART_ITEM_IRON_SWORD,
  SHOP_ART_ITEM_PATCHED_MAIL,
  SHOP_ART_ITEM_GUARD_COAT,
  SHOP_ART_ITEM_IRON_GREAVES,
  SHOP_ART_ITEM_MILITIA_AXE,
  SHOP_ART_ITEM_RUNNER_WRAPS,
  SHOP_ART_ITEM_BLACK_SUN_CHARM,
  SHOP_ART_ITEM_MILLER_HOOK,
  SHOP_ART_ITEM_CHAIN_PATCHES,
  SHOP_ART_ITEM_SCAVENGER_KNEE_PLATES,
  SHOP_ART_ITEM_DRAGON_ASH_TOKEN,
  SHOP_ART_ITEM_MILLER_LUCKY_NAIL,
  SHOP_ART_COUNT,
} shop_art_region_t;

static bool s_open;
static char s_shop_id[GAME_STATE_STRING_MAX];
static char s_feedback[128];
static int s_dismiss_guard_frames;
static bool s_cleanup_pending;
static shop_mode_t s_mode = SHOP_MODE_BUY;
static game_shop_buyback_t s_buyback;
static char s_semantic_id_storage[SHOP_SEMANTIC_ID_SLOTS]
                                 [SHOP_SEMANTIC_ID_LEN];
static int s_semantic_id_cursor;
static nt_resource_t s_shop_atlas RB_DARK_RPG_UNUSED;
static nt_atlas_region_ref_t s_shop_regions[SHOP_ART_COUNT] RB_DARK_RPG_UNUSED;

static const nt_ui_widget_def_t SHOP_WIDGET_DEF RB_DARK_RPG_UNUSED = {
    .name = "shop",
    .pill_color = 0xFFE0A34FU,
    ._reserved = 0U,
};

static const nt_hash64_t SHOP_ART_REGION_HASHES[SHOP_ART_COUNT] RB_DARK_RPG_UNUSED = {
    ASSET_ATLAS_REGION_UI_ASSET_EQUIPMENT_SLOT_CELL,
    ASSET_ATLAS_REGION_UI_GOLD_COIN_HUD,
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
};

static float clamp_f(float value, float lo, float hi) {
  if (value < lo) {
    return lo;
  }
  if (value > hi) {
    return hi;
  }
  return value;
}

static void shop_request_state_cleanup(void) { s_cleanup_pending = true; }

static void shop_clear_transient_ui_state(nt_ui_context_t *ctx) {
  if (!ctx || !s_cleanup_pending) {
    return;
  }
  game_modal_clear_state(ctx, SHOP_MODAL_ID);
  nt_ui_state_clear(ctx, nt_ui_id("shop/scroll"));
  nt_ui_state_clear(ctx, nt_ui_id("shop/buy_scroll"));
  nt_ui_state_clear(ctx, nt_ui_id("shop/player_scroll"));
  s_cleanup_pending = false;
}

static void semantic_ids_begin_frame(void) { s_semantic_id_cursor = 0; }

static Clay_ElementId semantic_clay_id(const char *prefix,
                                       const char *suffix) {
  char *buffer =
      s_semantic_id_storage[s_semantic_id_cursor % SHOP_SEMANTIC_ID_SLOTS];
  s_semantic_id_cursor += 1;
  (void)snprintf(buffer, SHOP_SEMANTIC_ID_LEN, "%s%s", prefix ? prefix : "",
                 suffix ? suffix : "");
  return Clay_GetElementId((Clay_String){.isStaticallyAllocated = false,
                                         .length = (int32_t)strlen(buffer),
                                         .chars = buffer});
}

static void text_label(nt_ui_context_t *ctx, const char *text,
                       const nt_ui_label_style_t *style) {
  nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_SHOP_TEXT), text ? text : "", style);
}

static const char *shop_mode_suffix(shop_mode_t mode) {
  switch (mode) {
  case SHOP_MODE_BUY:
    return "buy";
  case SHOP_MODE_SELL:
    return "sell";
  case SHOP_MODE_BUYBACK:
    return "buyback";
  default:
    return "buy";
  }
}

static const GameGearInstance *shop_find_gear(const GameState *state,
                                              const char *instance_id) {
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

static bool shop_equipped_matches(bool has_equipped, const char *equipped_id,
                                  const char *instance_id) {
  return has_equipped && equipped_id && instance_id &&
         strcmp(equipped_id, instance_id) == 0;
}

static bool shop_is_equipped(const GameState *state, const char *instance_id) {
  if (!state || !instance_id || instance_id[0] == '\0') {
    return false;
  }
  return shop_equipped_matches(state->has_equipment_weapon_instance_id,
                               state->equipment_weapon_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_offhand_instance_id,
                               state->equipment_offhand_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_head_instance_id,
                               state->equipment_head_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_armour_instance_id,
                               state->equipment_armour_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_hands_instance_id,
                               state->equipment_hands_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_waist_instance_id,
                               state->equipment_waist_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_charm_instance_id,
                               state->equipment_charm_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_feet_instance_id,
                               state->equipment_feet_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_neck_instance_id,
                               state->equipment_neck_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_ring_left_instance_id,
                               state->equipment_ring_left_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_ring_right_instance_id,
                               state->equipment_ring_right_instance_id,
                               instance_id) ||
         shop_equipped_matches(state->has_equipment_legs_instance_id,
                               state->equipment_legs_instance_id,
                               instance_id);
}

static const char *shop_item_name(const game_item_definition_t *item,
                                  const char *fallback) {
  return item && item->display_name ? item->display_name
                                    : fallback ? fallback : "-";
}

static void ensure_shop_art_regions(void) {
  if (s_shop_atlas.id != 0U) {
    return;
  }
  s_shop_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
  for (int i = 0; i < SHOP_ART_COUNT; ++i) {
    s_shop_regions[i] =
        nt_atlas_ref(s_shop_atlas, SHOP_ART_REGION_HASHES[i].value);
  }
}

static shop_art_region_t shop_item_art(const game_item_definition_t *item) {
  if (!item || !item->id) {
    return SHOP_ART_COUNT;
  }
  if (strcmp(item->id, "old_sword") == 0) {
    return SHOP_ART_ITEM_OLD_SWORD;
  }
  if (strcmp(item->id, "padded_jacket") == 0) {
    return SHOP_ART_ITEM_PADDED_JACKET;
  }
  if (strcmp(item->id, "leather_greaves") == 0) {
    return SHOP_ART_ITEM_LEATHER_GREAVES;
  }
  if (strcmp(item->id, "iron_sword") == 0) {
    return SHOP_ART_ITEM_IRON_SWORD;
  }
  if (strcmp(item->id, "patched_mail") == 0) {
    return SHOP_ART_ITEM_PATCHED_MAIL;
  }
  if (strcmp(item->id, "guard_coat") == 0) {
    return SHOP_ART_ITEM_GUARD_COAT;
  }
  if (strcmp(item->id, "iron_greaves") == 0) {
    return SHOP_ART_ITEM_IRON_GREAVES;
  }
  if (strcmp(item->id, "militia_axe") == 0) {
    return SHOP_ART_ITEM_MILITIA_AXE;
  }
  if (strcmp(item->id, "runner_wraps") == 0) {
    return SHOP_ART_ITEM_RUNNER_WRAPS;
  }
  if (strcmp(item->id, "black_sun_charm") == 0) {
    return SHOP_ART_ITEM_BLACK_SUN_CHARM;
  }
  if (strcmp(item->id, "miller_hook") == 0) {
    return SHOP_ART_ITEM_MILLER_HOOK;
  }
  if (strcmp(item->id, "chain_patches") == 0) {
    return SHOP_ART_ITEM_CHAIN_PATCHES;
  }
  if (strcmp(item->id, "scavenger_knee_plates") == 0) {
    return SHOP_ART_ITEM_SCAVENGER_KNEE_PLATES;
  }
  if (strcmp(item->id, "dragon_ash_token") == 0) {
    return SHOP_ART_ITEM_DRAGON_ASH_TOKEN;
  }
  if (strcmp(item->id, "miller_lucky_nail") == 0) {
    return SHOP_ART_ITEM_MILLER_LUCKY_NAIL;
  }
  return SHOP_ART_COUNT;
}

static void shop_art_image(nt_ui_context_t *ctx, shop_art_region_t art,
                           float size, uint32_t tint, float opacity) {
  if (art >= SHOP_ART_COUNT) {
    return;
  }
  ensure_shop_art_regions();
  nt_ui_image_style_t style = nt_ui_image_style_defaults();
  style.color_packed = tint;
  nt_ui_transform_t transform = nt_ui_transform_defaults();
  const nt_ui_element_data_t *data =
      opacity < 1.0F ? NT_UI_DATA_XFORM(LAYER_SHOP_ICON, &transform, opacity)
                     : NT_UI_DATA_LAYER(LAYER_SHOP_ICON);
  CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(size),
                              CLAY_SIZING_FIXED(size)}}}) {
    nt_ui_image(ctx, data, &s_shop_regions[art], &style, NULL);
  }
}

static void shop_item_icon(nt_ui_context_t *ctx,
                           const game_item_definition_t *item, float size,
                           bool enabled) {
  const shop_art_region_t item_art = shop_item_art(item);
  const bool has_icon = item_art < SHOP_ART_COUNT;
  const float slot_size = size;
  const float icon_size = has_icon ? size * 0.64F : size * 0.48F;
  const float opacity = enabled ? 1.0F : 0.45F;
  CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(slot_size),
                              CLAY_SIZING_FIXED(slot_size)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = enabled ? (Clay_Color){15.0F, 11.0F, 8.0F, 212.0F}
                                   : (Clay_Color){11.0F, 9.0F, 7.0F, 172.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(3),
        .border = {.color = enabled ? (Clay_Color){124.0F, 86.0F, 45.0F, 178.0F}
                                    : (Clay_Color){82.0F, 66.0F, 47.0F, 128.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_SHOP_BG)}) {
    shop_art_image(ctx, has_icon ? item_art : SHOP_ART_SLOT_CELL, icon_size,
                   enabled ? 0xFFFFFFFFU : 0xFF9F8866U, opacity);
  }
}

static float shop_cell_w(bool portrait) {
  return portrait ? SHOP_CELL_W_PHONE : SHOP_CELL_W_DESKTOP;
}

static float shop_cell_h(bool portrait) {
  return portrait ? SHOP_CELL_H_PHONE : SHOP_CELL_H_DESKTOP;
}

static void shop_money_icon(nt_ui_context_t *ctx, bool enabled, bool portrait) {
  shop_art_image(ctx, SHOP_ART_GOLD_COIN, portrait ? 11.0F : 12.0F,
                 enabled ? 0xFFFFFFFFU : 0xFF9F8866U,
                 enabled ? 1.0F : 0.55F);
}

static bool shop_cell_button(nt_ui_context_t *ctx, uint32_t button_id,
                             const char *action_text, const char *price_text,
                             bool enabled, bool portrait) {
  nt_ui_button_style_t button = game_modal_button_style(true);
  button.slice9_scale = portrait ? 0.34F : 0.38F;
  button.hit_padding_lrtb[0] = 6;
  button.hit_padding_lrtb[1] = 6;
  button.hit_padding_lrtb[2] = 6;
  button.hit_padding_lrtb[3] = 6;
  const nt_ui_label_style_t action =
      game_modal_label(portrait ? 9.0F : 10.0F, enabled ? 255.0F : 154.0F,
                       enabled ? 226.0F : 119.0F, enabled ? 176.0F : 88.0F,
                       255.0F);
  const nt_ui_label_style_t price =
      game_modal_label(portrait ? 9.5F : 10.5F, enabled ? 255.0F : 154.0F,
                       enabled ? 238.0F : 119.0F, enabled ? 196.0F : 88.0F,
                       255.0F);
  nt_ui_button_begin(
      ctx, NT_UI_DATA_LAYER(LAYER_SHOP_BG), button_id, &button,
      &(Clay_ElementDeclaration){
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 3,
                     .padding = {.left = 4, .right = 4, .top = 3, .bottom = 4},
                     .childAlignment = {CLAY_ALIGN_X_CENTER,
                                        CLAY_ALIGN_Y_CENTER}}},
      enabled, NULL);
  text_label(ctx, action_text ? action_text : "", &action);
  shop_money_icon(ctx, enabled, portrait);
  text_label(ctx, price_text ? price_text : "", &price);
  return nt_ui_button_end(ctx);
}

static bool shop_trade_cell(nt_ui_context_t *ctx, Clay_ElementId item_id,
                            Clay_ElementId action_id,
                            const game_item_definition_t *item,
                            const char *fallback_name, const char *price_text,
                            const char *action_text, bool enabled,
                            bool portrait) {
  const float cell_w = shop_cell_w(portrait);
  const float cell_h = shop_cell_h(portrait);
  const float icon_size = portrait ? 52.0F : 56.0F;
  const nt_ui_label_style_t name =
      game_modal_label(portrait ? 10.0F : 11.0F, enabled ? 252.0F : 155.0F,
                       enabled ? 230.0F : 139.0F, enabled ? 190.0F : 112.0F,
                       255.0F);
  bool clicked = false;
  CLAY({.id = item_id,
        .layout = {.sizing = {CLAY_SIZING_FIXED(cell_w),
                              CLAY_SIZING_FIXED(cell_h)}}}) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = 7, .right = 7, .top = 7, .bottom = 6},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 4,
                     .childAlignment = {CLAY_ALIGN_X_CENTER,
                                        CLAY_ALIGN_Y_TOP}},
          .backgroundColor = enabled ? (Clay_Color){30.0F, 21.0F, 14.0F, 224.0F}
                                     : (Clay_Color){20.0F, 17.0F, 13.0F, 184.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = enabled ? (Clay_Color){150.0F, 101.0F, 49.0F, 210.0F}
                                      : (Clay_Color){88.0F, 71.0F, 50.0F, 138.0F},
                     .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_SHOP_BG)}) {
      CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(icon_size),
                                  CLAY_SIZING_FIXED(icon_size)},
                       .childAlignment = {CLAY_ALIGN_X_CENTER,
                                          CLAY_ALIGN_Y_CENTER}}}) {
        shop_item_icon(ctx, item, icon_size, enabled);
      }
      CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0),
                                  CLAY_SIZING_FIXED(portrait ? 24.0F : 26.0F)},
                       .childAlignment = {CLAY_ALIGN_X_CENTER,
                                          CLAY_ALIGN_Y_CENTER}}}) {
        text_label(ctx, shop_item_name(item, fallback_name), &name);
      }
      CLAY({.id = action_id,
            .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                  CLAY_SIZING_FIXED(portrait ? 28.0F : 30.0F)},
                       .childAlignment = {CLAY_ALIGN_X_CENTER,
                                          CLAY_ALIGN_Y_CENTER}}}) {
        clicked = shop_cell_button(ctx, nt_ui_child_id(action_id.id, "button"),
                                   action_text, price_text, enabled, portrait);
      }
    }
  }
  return clicked;
}

static void item_stat_line(const game_item_definition_t *item, char *out,
                           size_t out_cap) {
  if (!out || out_cap == 0U) {
    return;
  }
  if (!item) {
    (void)snprintf(out, out_cap, "-");
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
    (void)snprintf(out, out_cap, "без боевого бонуса");
  }
}

static bool shop_button(nt_ui_context_t *ctx, uint32_t button_id,
                        const char *text, bool enabled, bool portrait) {
  nt_ui_button_style_t button = game_modal_button_style(true);
  const nt_ui_label_style_t label = game_modal_label(
      portrait ? 13.0F : 14.0F, enabled ? 255.0F : 142.0F,
      enabled ? 226.0F : 112.0F, enabled ? 176.0F : 86.0F, 255.0F);
  nt_ui_button_begin(
      ctx, NT_UI_DATA_LAYER(LAYER_SHOP_BG), button_id, &button,
      &(Clay_ElementDeclaration){
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = 12, .right = 12, .top = 6,
                                 .bottom = 6},
                     .childAlignment = {CLAY_ALIGN_X_CENTER,
                                        CLAY_ALIGN_Y_CENTER}}},
      enabled, NULL);
  text_label(ctx, text, &label);
  return nt_ui_button_end(ctx);
}

static const char *locked_reason(const GameState *state,
                                 const game_shop_item_t *shop_item) {
  if (!state || !shop_item) {
    return "недоступно";
  }
  if (!game_actions_shop_item_available(state, shop_item)) {
    return "после задания";
  }
  if (state->wallet_gold < shop_item->price_gold) {
    return "не хватает золота";
  }
  return "можно купить";
}

static SHOP_UNUSED_FN void shop_item_row(nt_ui_context_t *ctx, World *w,
                          const game_shop_definition_t *shop,
                          const game_shop_item_t *shop_item, bool portrait,
                          int index) {
  if (!ctx || !w || !w->player_state || !shop_item || !shop_item->item_id) {
    return;
  }
  GameState *state = w->player_state;
  const game_item_definition_t *item =
      game_content_find_item(shop_item->item_id);
  const Clay_ElementId row_id = semantic_clay_id("shop/item/",
                                                 shop_item->item_id);
  const Clay_ElementId buy_id = semantic_clay_id("shop/buy/",
                                                 shop_item->item_id);

  const bool can_buy =
      game_actions_can_purchase_shop_item(state, shop, shop_item);
  const bool available = game_actions_shop_item_available(state, shop_item);
  const float row_h = portrait ? 78.0F : 72.0F;
  const float button_w = portrait ? 82.0F : 96.0F;
  const nt_ui_label_style_t title = game_modal_label(
      portrait ? 13.0F : 15.0F, available ? 250.0F : 154.0F,
      available ? 231.0F : 137.0F, available ? 190.0F : 112.0F, 255.0F);
  const nt_ui_label_style_t meta = game_modal_label(
      portrait ? 10.5F : 12.0F, available ? 201.0F : 126.0F,
      available ? 173.0F : 108.0F, available ? 130.0F : 88.0F, 255.0F);
  const nt_ui_label_style_t price = game_modal_label(
      portrait ? 11.0F : 12.5F, can_buy ? 248.0F : 182.0F,
      can_buy ? 218.0F : 139.0F, can_buy ? 151.0F : 103.0F, 255.0F);
  char stat_buf[96];
  char price_buf[96];
  item_stat_line(item, stat_buf, sizeof stat_buf);
  (void)snprintf(price_buf, sizeof price_buf, "%d зол. - %s",
                 shop_item->price_gold, locked_reason(state, shop_item));

  CLAY({.id = row_id,
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(row_h)},
                   .padding = {.left = 10, .right = 10, .top = 8, .bottom = 8},
                   .layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .childGap = 10,
                   .childAlignment = {CLAY_ALIGN_X_LEFT,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = can_buy ? (Clay_Color){31.0F, 22.0F, 15.0F, 226.0F}
                                   : (Clay_Color){21.0F, 17.0F, 13.0F, 184.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .border = {.color = can_buy ? (Clay_Color){149.0F, 99.0F, 47.0F, 210.0F}
                                    : (Clay_Color){92.0F, 71.0F, 49.0F, 135.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_SHOP_BG)}) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 2,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_CENTER}}}) {
      text_label(ctx, item && item->display_name ? item->display_name
                                                 : shop_item->item_id,
                 &title);
      text_label(ctx, stat_buf, &meta);
      text_label(ctx, price_buf, &price);
    }
    CLAY({.id = buy_id,
          .layout = {.sizing = {CLAY_SIZING_FIXED(button_w),
                                CLAY_SIZING_FIXED(portrait ? 36.0F : 38.0F)}}}) {
      if (shop_button(ctx, nt_ui_child_id(buy_id.id, "button"), "Купить",
                      can_buy, portrait)) {
        if (game_actions_purchase_shop_item(state, shop->id,
                                            shop_item->item_id)) {
          (void)snprintf(s_feedback, sizeof s_feedback, "Куплено: %s",
                         item && item->display_name ? item->display_name
                                                    : shop_item->item_id);
          game_audio_play(GAME_AUDIO_CUE_REWARD);
        }
      }
    }
  }
  (void)index;
}

static bool shop_mode_button(nt_ui_context_t *ctx, shop_mode_t mode,
                             const char *text, bool portrait) {
  const Clay_ElementId mode_id =
      semantic_clay_id("shop/mode/", shop_mode_suffix(mode));
  const bool active = s_mode == mode;
  nt_ui_button_style_t button = game_modal_button_style(active);
  const nt_ui_label_style_t label =
      game_modal_label(portrait ? 12.0F : 13.0F, active ? 255.0F : 218.0F,
                       active ? 231.0F : 191.0F, active ? 184.0F : 146.0F,
                       255.0F);
  bool clicked = false;
  CLAY({.id = mode_id,
        .layout = {.sizing = {CLAY_SIZING_GROW(0),
                              CLAY_SIZING_FIXED(portrait ? 32.0F : 34.0F)}}}) {
    nt_ui_button_begin(
        ctx, NT_UI_DATA_LAYER(LAYER_SHOP_BG),
        nt_ui_child_id(mode_id.id, "button"), &button,
        &(Clay_ElementDeclaration){
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                       .padding = {.left = 8, .right = 8, .top = 5,
                                   .bottom = 5},
                       .childAlignment = {CLAY_ALIGN_X_CENTER,
                                          CLAY_ALIGN_Y_CENTER}}},
        true, NULL);
    text_label(ctx, text, &label);
    clicked = nt_ui_button_end(ctx);
  }
  if (clicked && s_mode != mode) {
    s_mode = mode;
    nt_ui_state_clear(ctx, nt_ui_id("shop/scroll"));
    nt_ui_state_clear(ctx, nt_ui_id("shop/player_scroll"));
    game_audio_play(GAME_AUDIO_CUE_UI_CLICK);
  }
  return clicked;
}

static void shop_mode_tabs_ui(nt_ui_context_t *ctx, bool portrait) {
  CLAY({.id = CLAY_ID("shop/modes"),
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                   .layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .childGap = portrait ? 5 : 7,
                   .childAlignment = {CLAY_ALIGN_X_LEFT,
                                      CLAY_ALIGN_Y_CENTER}}}) {
    (void)shop_mode_button(ctx, SHOP_MODE_BUY, "Купить", portrait);
    (void)shop_mode_button(ctx, SHOP_MODE_SELL, "Продать", portrait);
    (void)shop_mode_button(ctx, SHOP_MODE_BUYBACK, "Выкуп", portrait);
  }
}

static const char *shop_sell_locked_reason(const GameState *state,
                                           const char *instance_id,
                                           const game_item_definition_t *item) {
  if (!state || !instance_id || instance_id[0] == '\0') {
    return "недоступно";
  }
  if (shop_is_equipped(state, instance_id)) {
    return "сначала снять";
  }
  if (!item || !item->sellable || game_actions_item_sell_price(item) <= 0) {
    return "не продается";
  }
  return "недоступно";
}

static SHOP_UNUSED_FN void shop_sell_row(nt_ui_context_t *ctx, World *w, bool portrait,
                          int index) {
  if (!ctx || !w || !w->player_state || index < 0 ||
      index >= w->player_state->inventory_bag_order_count) {
    return;
  }
  GameState *state = w->player_state;
  const char *instance_id = state->inventory_bag_order[index];
  const GameGearInstance *gear = shop_find_gear(state, instance_id);
  if (!gear) {
    return;
  }
  const game_item_definition_t *item = game_content_find_item(gear->def_id);
  const Clay_ElementId row_id =
      semantic_clay_id("shop/player_item/", instance_id);
  const Clay_ElementId sell_id = semantic_clay_id("shop/sell/", instance_id);
  int price_gold = 0;
  const bool can_sell =
      game_actions_can_sell_inventory_item(state, instance_id, &price_gold);
  const int preview_price =
      price_gold > 0 ? price_gold : game_actions_item_sell_price(item);
  const float row_h = portrait ? 78.0F : 72.0F;
  const float button_w = portrait ? 82.0F : 96.0F;
  const nt_ui_label_style_t title =
      game_modal_label(portrait ? 13.0F : 15.0F, can_sell ? 250.0F : 154.0F,
                       can_sell ? 231.0F : 137.0F,
                       can_sell ? 190.0F : 112.0F, 255.0F);
  const nt_ui_label_style_t meta =
      game_modal_label(portrait ? 10.5F : 12.0F, can_sell ? 201.0F : 126.0F,
                       can_sell ? 173.0F : 108.0F,
                       can_sell ? 130.0F : 88.0F, 255.0F);
  const nt_ui_label_style_t price =
      game_modal_label(portrait ? 11.0F : 12.5F, can_sell ? 248.0F : 182.0F,
                       can_sell ? 218.0F : 139.0F,
                       can_sell ? 151.0F : 103.0F, 255.0F);
  char stat_buf[96];
  char price_buf[96];
  item_stat_line(item, stat_buf, sizeof stat_buf);
  if (preview_price > 0) {
    (void)snprintf(price_buf, sizeof price_buf, "%d зол. - %s", preview_price,
                   can_sell ? "можно продать"
                            : shop_sell_locked_reason(state, instance_id, item));
  } else {
    (void)snprintf(price_buf, sizeof price_buf, "%s",
                   shop_sell_locked_reason(state, instance_id, item));
  }

  CLAY({.id = row_id,
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(row_h)},
                   .padding = {.left = 10, .right = 10, .top = 8, .bottom = 8},
                   .layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .childGap = 10,
                   .childAlignment = {CLAY_ALIGN_X_LEFT,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = can_sell ? (Clay_Color){29.0F, 24.0F, 17.0F, 226.0F}
                                   : (Clay_Color){20.0F, 18.0F, 14.0F, 184.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .border = {.color = can_sell ? (Clay_Color){132.0F, 105.0F, 58.0F, 205.0F}
                                    : (Clay_Color){86.0F, 74.0F, 52.0F, 135.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_SHOP_BG)}) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 2,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_CENTER}}}) {
      text_label(ctx, shop_item_name(item, instance_id), &title);
      text_label(ctx, stat_buf, &meta);
      text_label(ctx, price_buf, &price);
    }
    CLAY({.id = sell_id,
          .layout = {.sizing = {CLAY_SIZING_FIXED(button_w),
                                CLAY_SIZING_FIXED(portrait ? 36.0F : 38.0F)}}}) {
      if (shop_button(ctx, nt_ui_child_id(sell_id.id, "button"), "Продать",
                      can_sell, portrait)) {
        char name_buf[GAME_STATE_STRING_MAX];
        (void)snprintf(name_buf, sizeof name_buf, "%s",
                       shop_item_name(item, instance_id));
        if (game_actions_sell_inventory_item(state, instance_id, &s_buyback)) {
          (void)snprintf(s_feedback, sizeof s_feedback, "Продано: %s",
                         name_buf);
          game_audio_play(GAME_AUDIO_CUE_REWARD);
        }
      }
    }
  }
}

static const char *shop_buyback_locked_reason(const GameState *state,
                                              int price_gold) {
  if (!state) {
    return "недоступно";
  }
  if (state->wallet_gold < price_gold) {
    return "не хватает золота";
  }
  return "нет места";
}

static SHOP_UNUSED_FN void shop_buyback_row(nt_ui_context_t *ctx, World *w,
                             const game_shop_buyback_entry_t *entry,
                             bool portrait) {
  if (!ctx || !w || !w->player_state || !entry || !entry->used ||
      entry->entry_id[0] == '\0' || entry->gear.def_id[0] == '\0') {
    return;
  }
  GameState *state = w->player_state;
  const game_item_definition_t *item =
      game_content_find_item(entry->gear.def_id);
  const Clay_ElementId row_id =
      semantic_clay_id("shop/buyback_item/", entry->entry_id);
  const Clay_ElementId buyback_id =
      semantic_clay_id("shop/buyback/", entry->entry_id);
  int price_gold = 0;
  const bool can_rebuy = game_actions_can_rebuy_inventory_item(
      state, &s_buyback, entry->entry_id, &price_gold);
  const int preview_price = price_gold > 0 ? price_gold : entry->price_gold;
  const float row_h = portrait ? 78.0F : 72.0F;
  const float button_w = portrait ? 82.0F : 96.0F;
  const nt_ui_label_style_t title = game_modal_label(
      portrait ? 13.0F : 15.0F, can_rebuy ? 250.0F : 154.0F,
      can_rebuy ? 231.0F : 137.0F, can_rebuy ? 190.0F : 112.0F, 255.0F);
  const nt_ui_label_style_t meta = game_modal_label(
      portrait ? 10.5F : 12.0F, can_rebuy ? 201.0F : 126.0F,
      can_rebuy ? 173.0F : 108.0F, can_rebuy ? 130.0F : 88.0F, 255.0F);
  const nt_ui_label_style_t price = game_modal_label(
      portrait ? 11.0F : 12.5F, can_rebuy ? 248.0F : 182.0F,
      can_rebuy ? 218.0F : 139.0F, can_rebuy ? 151.0F : 103.0F, 255.0F);
  char stat_buf[96];
  char price_buf[96];
  item_stat_line(item, stat_buf, sizeof stat_buf);
  (void)snprintf(price_buf, sizeof price_buf, "%d зол. - %s", preview_price,
                 can_rebuy ? "можно выкупить"
                            : shop_buyback_locked_reason(state, preview_price));

  CLAY({.id = row_id,
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(row_h)},
                   .padding = {.left = 10, .right = 10, .top = 8, .bottom = 8},
                   .layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .childGap = 10,
                   .childAlignment = {CLAY_ALIGN_X_LEFT,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = can_rebuy ? (Clay_Color){31.0F, 24.0F, 17.0F, 226.0F}
                                     : (Clay_Color){21.0F, 18.0F, 14.0F, 184.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .border = {.color = can_rebuy ? (Clay_Color){145.0F, 106.0F, 55.0F, 210.0F}
                                      : (Clay_Color){91.0F, 74.0F, 52.0F, 135.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_SHOP_BG)}) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 2,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_CENTER}}}) {
      text_label(ctx, shop_item_name(item, entry->entry_id), &title);
      text_label(ctx, stat_buf, &meta);
      text_label(ctx, price_buf, &price);
    }
    CLAY({.id = buyback_id,
          .layout = {.sizing = {CLAY_SIZING_FIXED(button_w),
                                CLAY_SIZING_FIXED(portrait ? 36.0F : 38.0F)}}}) {
      if (shop_button(ctx, nt_ui_child_id(buyback_id.id, "button"), "Выкуп",
                      can_rebuy, portrait)) {
        char name_buf[GAME_STATE_STRING_MAX];
        (void)snprintf(name_buf, sizeof name_buf, "%s",
                       shop_item_name(item, entry->entry_id));
        if (game_actions_rebuy_inventory_item(state, &s_buyback,
                                              entry->entry_id)) {
          (void)snprintf(s_feedback, sizeof s_feedback, "Выкуплено: %s",
                         name_buf);
          game_audio_play(GAME_AUDIO_CUE_REWARD);
        }
      }
    }
  }
}

static void shop_buy_cell(nt_ui_context_t *ctx, World *w,
                          const game_shop_definition_t *shop,
                          const game_shop_item_t *shop_item, bool portrait,
                          int index) {
  if (!ctx || !w || !w->player_state || !shop_item || !shop_item->item_id) {
    return;
  }
  GameState *state = w->player_state;
  const game_item_definition_t *item =
      game_content_find_item(shop_item->item_id);
  const Clay_ElementId item_id =
      semantic_clay_id("shop/item/", shop_item->item_id);
  const Clay_ElementId buy_id =
      semantic_clay_id("shop/buy/", shop_item->item_id);
  const bool can_buy =
      game_actions_can_purchase_shop_item(state, shop, shop_item);
  char price_buf[24];
  (void)snprintf(price_buf, sizeof price_buf, "%d", shop_item->price_gold);
  if (shop_trade_cell(ctx, item_id, buy_id, item, shop_item->item_id, price_buf,
                      "Купить", can_buy, portrait)) {
    if (game_actions_purchase_shop_item(state, shop->id, shop_item->item_id)) {
      (void)snprintf(s_feedback, sizeof s_feedback, "Куплено: %s",
                     shop_item_name(item, shop_item->item_id));
      game_audio_play(GAME_AUDIO_CUE_REWARD);
    }
  }
  (void)index;
}

static void shop_sell_cell(nt_ui_context_t *ctx, World *w, bool portrait,
                           int index) {
  if (!ctx || !w || !w->player_state || index < 0 ||
      index >= w->player_state->inventory_bag_order_count) {
    return;
  }
  GameState *state = w->player_state;
  const char *instance_id = state->inventory_bag_order[index];
  const GameGearInstance *gear = shop_find_gear(state, instance_id);
  if (!gear) {
    return;
  }
  const game_item_definition_t *item = game_content_find_item(gear->def_id);
  const Clay_ElementId item_id =
      semantic_clay_id("shop/player_item/", instance_id);
  const Clay_ElementId sell_id = semantic_clay_id("shop/sell/", instance_id);
  int price_gold = 0;
  const bool can_sell =
      game_actions_can_sell_inventory_item(state, instance_id, &price_gold);
  const int preview_price =
      price_gold > 0 ? price_gold : game_actions_item_sell_price(item);
  char price_buf[24];
  (void)snprintf(price_buf, sizeof price_buf, preview_price > 0 ? "+%d" : "-",
                 preview_price);
  if (shop_trade_cell(ctx, item_id, sell_id, item, instance_id, price_buf,
                      "Продать", can_sell, portrait)) {
    char name_buf[GAME_STATE_STRING_MAX];
    (void)snprintf(name_buf, sizeof name_buf, "%s",
                   shop_item_name(item, instance_id));
    if (game_actions_sell_inventory_item(state, instance_id, &s_buyback)) {
      (void)snprintf(s_feedback, sizeof s_feedback, "Продано: %s", name_buf);
      game_audio_play(GAME_AUDIO_CUE_REWARD);
    }
  }
}

static void shop_buyback_cell(nt_ui_context_t *ctx, World *w,
                              const game_shop_buyback_entry_t *entry,
                              bool portrait) {
  if (!ctx || !w || !w->player_state || !entry || !entry->used ||
      entry->entry_id[0] == '\0' || entry->gear.def_id[0] == '\0') {
    return;
  }
  GameState *state = w->player_state;
  const game_item_definition_t *item =
      game_content_find_item(entry->gear.def_id);
  const Clay_ElementId item_id =
      semantic_clay_id("shop/buyback_item/", entry->entry_id);
  const Clay_ElementId buyback_id =
      semantic_clay_id("shop/buyback/", entry->entry_id);
  int price_gold = 0;
  const bool can_rebuy = game_actions_can_rebuy_inventory_item(
      state, &s_buyback, entry->entry_id, &price_gold);
  const int preview_price = price_gold > 0 ? price_gold : entry->price_gold;
  char price_buf[24];
  (void)snprintf(price_buf, sizeof price_buf, "%d", preview_price);
  if (shop_trade_cell(ctx, item_id, buyback_id, item, entry->entry_id,
                      price_buf, "Выкуп", can_rebuy, portrait)) {
    char name_buf[GAME_STATE_STRING_MAX];
    (void)snprintf(name_buf, sizeof name_buf, "%s",
                   shop_item_name(item, entry->entry_id));
    if (game_actions_rebuy_inventory_item(state, &s_buyback,
                                          entry->entry_id)) {
      (void)snprintf(s_feedback, sizeof s_feedback, "Выкуплено: %s",
                     name_buf);
      game_audio_play(GAME_AUDIO_CUE_REWARD);
    }
  }
}

static int shop_columns_sane(int columns) { return columns > 0 ? columns : 1; }

static int shop_buy_grid_ui(nt_ui_context_t *ctx, World *w,
                            const game_shop_definition_t *shop, bool portrait,
                            int columns) {
  int drawn = 0;
  columns = shop_columns_sane(columns);
  for (int cursor = 0; cursor < shop->item_count;) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = portrait ? 6 : 7,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_TOP}}}) {
      for (int col = 0; col < columns && cursor < shop->item_count; ++col) {
        shop_buy_cell(ctx, w, shop, &shop->items[cursor], portrait, cursor);
        cursor += 1;
        drawn += 1;
      }
    }
  }
  return drawn;
}

static int shop_sell_grid_ui(nt_ui_context_t *ctx, World *w, bool portrait,
                             int columns) {
  if (!w || !w->player_state) {
    return 0;
  }
  int drawn = 0;
  int cursor = 0;
  columns = shop_columns_sane(columns);
  while (cursor < w->player_state->inventory_bag_order_count) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = portrait ? 6 : 7,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_TOP}}}) {
      for (int col = 0;
           col < columns &&
           cursor < w->player_state->inventory_bag_order_count;) {
        const int item_index = cursor;
        cursor += 1;
        const char *instance_id = w->player_state->inventory_bag_order[item_index];
        if (!shop_find_gear(w->player_state, instance_id)) {
          continue;
        }
        shop_sell_cell(ctx, w, portrait, item_index);
        drawn += 1;
        col += 1;
      }
    }
  }
  return drawn;
}

static int shop_buyback_grid_ui(nt_ui_context_t *ctx, World *w, bool portrait,
                                int columns) {
  int drawn = 0;
  int cursor = 0;
  columns = shop_columns_sane(columns);
  while (cursor < s_buyback.count) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = portrait ? 6 : 7,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_TOP}}}) {
      for (int col = 0; col < columns && cursor < s_buyback.count;) {
        const int entry_index = cursor;
        cursor += 1;
        if (!s_buyback.entries[entry_index].used) {
          continue;
        }
        shop_buyback_cell(ctx, w, &s_buyback.entries[entry_index], portrait);
        drawn += 1;
        col += 1;
      }
    }
  }
  return drawn;
}

static void shop_list_content_ui(nt_ui_context_t *ctx, World *w,
                                 const game_shop_definition_t *shop,
                                 shop_list_kind_t kind, bool portrait,
                                 int columns) {
  const nt_ui_label_style_t empty =
      game_modal_label(13.0F, 196.0F, 166.0F, 123.0F, 255.0F);
  if (kind == SHOP_LIST_SELL) {
    if (!w || !w->player_state) {
      text_label(ctx, "Сумка недоступна.", &empty);
      return;
    }
    const int rows = shop_sell_grid_ui(ctx, w, portrait, columns);
    if (rows == 0) {
      text_label(ctx, "В сумке нет предметов для продажи.", &empty);
    }
    return;
  }
  if (kind == SHOP_LIST_BUYBACK) {
    if (s_buyback.count <= 0) {
      text_label(ctx, "Выкуп пуст.", &empty);
      return;
    }
    const int rows = shop_buyback_grid_ui(ctx, w, portrait, columns);
    if (rows == 0) {
      text_label(ctx, "Выкуп пуст.", &empty);
    }
    return;
  }
  if (!shop) {
    text_label(ctx, "Магазин не найден.", &empty);
    return;
  }
  if (shop->item_count <= 0 || !shop->items) {
    text_label(ctx, "Товаров нет.", &empty);
    return;
  }
  (void)shop_buy_grid_ui(ctx, w, shop, portrait, columns);
}

static void shop_scroll_list_ui(nt_ui_context_t *ctx, World *w,
                                const game_shop_definition_t *shop,
                                shop_list_kind_t kind, const char *scroll_id,
                                const char *list_id, bool portrait,
                                int columns) {
  nt_ui_scroll_style_t scroll_style = game_modal_scroll_style();
  nt_ui_scroll_begin(
      ctx, NT_UI_DATA_LAYER(LAYER_SHOP_BG), nt_ui_id(scroll_id), &scroll_style,
      &(Clay_ElementDeclaration){
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = 0, .right = 9, .top = 0,
                                 .bottom = 2}},
          .cornerRadius = CLAY_CORNER_RADIUS(4)});
  const Clay_ElementId clay_list_id = semantic_clay_id("", list_id);
  CLAY({.id = clay_list_id,
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = portrait ? 6 : 7,
                   .childAlignment = {CLAY_ALIGN_X_LEFT,
                                      CLAY_ALIGN_Y_TOP}}}) {
    shop_list_content_ui(ctx, w, shop, kind, portrait, columns);
  }
  nt_ui_scroll_end(ctx);
}

static void shop_list_panel_ui(nt_ui_context_t *ctx, World *w,
                               const game_shop_definition_t *shop,
                               shop_list_kind_t kind, const char *panel_id,
                               const char *scroll_id, const char *list_id,
                               const char *title_text, bool portrait,
                               int columns) {
  const Clay_ElementId clay_panel_id = semantic_clay_id("", panel_id);
  const nt_ui_label_style_t heading =
      game_modal_label(portrait ? 12.0F : 13.0F, 233.0F, 205.0F, 158.0F,
                       255.0F);
  CLAY({.id = clay_panel_id,
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                   .padding = {.left = 8, .right = 6, .top = 7, .bottom = 6},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = 6,
                   .childAlignment = {CLAY_ALIGN_X_LEFT,
                                      CLAY_ALIGN_Y_TOP}},
        .backgroundColor = {18.0F, 13.0F, 9.0F, 186.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .border = {.color = {89.0F, 67.0F, 42.0F, 150.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_SHOP_BG)}) {
    text_label(ctx, title_text, &heading);
    shop_scroll_list_ui(ctx, w, shop, kind, scroll_id, list_id, portrait,
                        columns);
  }
}

bool shop_screen_open(void) { return s_open; }

void shop_screen_set_open(bool open) {
  if (!open && s_open) {
    shop_request_state_cleanup();
  }
  s_open = open;
  if (!s_open) {
    s_shop_id[0] = '\0';
    s_feedback[0] = '\0';
    s_dismiss_guard_frames = 0;
    s_mode = SHOP_MODE_BUY;
    game_actions_shop_buyback_init(&s_buyback);
  } else {
    s_dismiss_guard_frames = 2;
  }
}

bool shop_screen_open_shop(const char *shop_id) {
  if (!shop_id || !game_content_find_shop(shop_id) ||
      strlen(shop_id) >= sizeof s_shop_id) {
    return false;
  }
  (void)strcpy(s_shop_id, shop_id);
  s_feedback[0] = '\0';
  s_mode = SHOP_MODE_BUY;
  game_actions_shop_buyback_init(&s_buyback);
  shop_screen_set_open(true);
  return true;
}

void shop_screen_ui(nt_ui_context_t *ctx, World *w) {
  shop_clear_transient_ui_state(ctx);
  if (!s_open || !ctx || !w || w->dialogue.open || !w->player_state) {
    return;
  }
  semantic_ids_begin_frame();
  const game_shop_definition_t *shop = game_content_find_shop(s_shop_id);
  float layout_w = 0.0F;
  float layout_h = 0.0F;
  nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
  const bool portrait = layout_h > layout_w;
  const float panel_w =
      portrait ? clamp_f(layout_w - 24.0F, 300.0F, 430.0F)
               : clamp_f(layout_w * 0.84F, 680.0F, layout_w - 72.0F);
  const float panel_h =
      portrait ? clamp_f(layout_h - 108.0F, 430.0F, layout_h - 42.0F)
               : clamp_f(layout_h * 0.76F, 370.0F, layout_h - 64.0F);
  const nt_ui_label_style_t title =
      game_modal_label(portrait ? 20.0F : 23.0F, 255.0F, 238.0F, 202.0F,
                       255.0F);
  const nt_ui_label_style_t hint =
      game_modal_label(portrait ? 12.0F : 13.0F, 205.0F, 178.0F, 133.0F,
                       255.0F);
  const nt_ui_label_style_t feedback =
      game_modal_label(portrait ? 12.0F : 13.0F, 225.0F, 189.0F, 109.0F,
                       255.0F);
  char wallet_buf[64];
  (void)snprintf(wallet_buf, sizeof wallet_buf, "Золото: %d",
                 w->player_state->wallet_gold);

  bool modal_open = true;
  nt_ui_modal_style_t modal_style =
      game_modal_style((nt_ui_layer_t)LAYER_SHOP_BG, true);
  const bool ignore_close_request = s_dismiss_guard_frames > 0;
  if (!game_modal_visible(ctx, SHOP_MODAL_ID, &modal_style, &modal_open,
                          ignore_close_request)) {
    if (!modal_open) {
      shop_screen_set_open(false);
      shop_clear_transient_ui_state(ctx);
    }
    return;
  }

  CLAY({.id = CLAY_ID("shop/modal_frame"),
        .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w),
                              CLAY_SIZING_FIXED(panel_h)}},
        .backgroundColor = {13.0F, 9.0F, 7.0F, 232.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(5),
        .userData = NT_UI_CLAY_DATA(LAYER_SHOP_BG)}) {
    nt_ui_image_style_t panel_image = game_modal_panel_image(portrait);
    nt_ui_panel_begin(
        ctx, NT_UI_DATA_LAYER(LAYER_SHOP_BG),
        game_modal_art(GAME_MODAL_ART_OUTER_FRAME), &panel_image,
        &(Clay_ElementDeclaration){
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                       .padding = CLAY_PADDING_ALL(portrait ? 9 : 12),
                       .layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .childGap = portrait ? 7 : 9,
                       .childAlignment = {CLAY_ALIGN_X_LEFT,
                                          CLAY_ALIGN_Y_TOP}}});
    CLAY({.id = CLAY_ID("shop/header"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_CENTER}}}) {
      CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                       .layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .childGap = 2,
                       .childAlignment = {CLAY_ALIGN_X_LEFT,
                                          CLAY_ALIGN_Y_CENTER}}}) {
        text_label(ctx, shop && shop->display_name ? shop->display_name
                                                   : "Магазин",
                   &title);
        text_label(ctx, wallet_buf, &hint);
      }
      if (game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_SHOP_BG,
                                  (nt_ui_layer_t)LAYER_SHOP_TEXT,
                                  "shop/close", portrait)) {
        modal_open = false;
      }
    }
    shop_mode_tabs_ui(ctx, portrait);
    if (s_feedback[0] != '\0') {
      CLAY({.id = CLAY_ID("shop/feedback"),
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30.0F)},
                       .padding = {.left = 9, .right = 9, .top = 4,
                                   .bottom = 4},
                       .childAlignment = {CLAY_ALIGN_X_LEFT,
                                          CLAY_ALIGN_Y_CENTER}},
            .backgroundColor = {48.0F, 34.0F, 18.0F, 218.0F},
            .cornerRadius = CLAY_CORNER_RADIUS(4),
            .border = {.color = {166.0F, 112.0F, 46.0F, 190.0F},
                       .width = {1, 1, 1, 1, 0}},
            .userData = NT_UI_CLAY_DATA(LAYER_SHOP_BG)}) {
        text_label(ctx, s_feedback, &feedback);
      }
    }
    if (portrait) {
      const shop_list_kind_t active_kind =
          s_mode == SHOP_MODE_SELL
              ? SHOP_LIST_SELL
              : (s_mode == SHOP_MODE_BUYBACK ? SHOP_LIST_BUYBACK
                                             : SHOP_LIST_BUY);
      const int columns = panel_w >= 356.0F ? 3 : 2;
      shop_scroll_list_ui(ctx, w, shop, active_kind, "shop/scroll",
                          "shop/items", portrait, columns);
    } else {
      const shop_list_kind_t player_kind =
          s_mode == SHOP_MODE_BUYBACK ? SHOP_LIST_BUYBACK : SHOP_LIST_SELL;
      const char *player_title =
          s_mode == SHOP_MODE_BUYBACK ? "Выкуп" : "Моя сумка";
      const int columns = panel_w >= 760.0F ? 3 : 2;
      CLAY({.id = CLAY_ID("shop/columns"),
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                       .layoutDirection = CLAY_LEFT_TO_RIGHT,
                       .childGap = 9,
                       .childAlignment = {CLAY_ALIGN_X_LEFT,
                                          CLAY_ALIGN_Y_TOP}}}) {
        shop_list_panel_ui(ctx, w, shop, SHOP_LIST_BUY, "shop/buy_column",
                           "shop/buy_scroll", "shop/items", "Торговец",
                           portrait, columns);
        shop_list_panel_ui(ctx, w, shop, player_kind, "shop/player_column",
                           "shop/player_scroll", "shop/player_items",
                           player_title, portrait, columns);
      }
    }
    nt_ui_panel_end(ctx);
  }
  nt_ui_modal_end(ctx);
  if (s_dismiss_guard_frames > 0) {
    --s_dismiss_guard_frames;
  }
  if (!modal_open) {
    shop_screen_set_open(false);
  }
  shop_clear_transient_ui_state(ctx);
}
