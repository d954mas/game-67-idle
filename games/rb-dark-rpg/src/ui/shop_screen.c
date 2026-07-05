#include "ui/shop_screen.h"

#include "clay.h"
#include "game_actions.h"
#include "game_audio.h"
#include "game_content.h"
#include "game_state.h"
#include "ui/game_modal.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LAYER_SHOP_BG 34
#define LAYER_SHOP_TEXT 35
#define SHOP_MODAL_ID 0x5100F001U
#define SHOP_SEMANTIC_ID_SLOTS 64
#define SHOP_SEMANTIC_ID_LEN 96

static bool s_open;
static char s_shop_id[GAME_STATE_STRING_MAX];
static char s_feedback[128];
static int s_dismiss_guard_frames;
static char s_semantic_id_storage[SHOP_SEMANTIC_ID_SLOTS]
                                 [SHOP_SEMANTIC_ID_LEN];
static int s_semantic_id_cursor;

static float clamp_f(float value, float lo, float hi) {
  if (value < lo) {
    return lo;
  }
  if (value > hi) {
    return hi;
  }
  return value;
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

static void shop_item_row(nt_ui_context_t *ctx, World *w,
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

static void shop_body_ui(nt_ui_context_t *ctx, World *w,
                         const game_shop_definition_t *shop, bool portrait) {
  const nt_ui_label_style_t empty =
      game_modal_label(13.0F, 196.0F, 166.0F, 123.0F, 255.0F);
  if (!shop) {
    text_label(ctx, "Магазин не найден.", &empty);
    return;
  }
  if (shop->item_count <= 0 || !shop->items) {
    text_label(ctx, "Товаров нет.", &empty);
    return;
  }
  for (int i = 0; i < shop->item_count; ++i) {
    shop_item_row(ctx, w, shop, &shop->items[i], portrait, i);
  }
}

bool shop_screen_open(void) { return s_open; }

void shop_screen_set_open(bool open) {
  s_open = open;
  if (!s_open) {
    s_shop_id[0] = '\0';
    s_feedback[0] = '\0';
    s_dismiss_guard_frames = 0;
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
  shop_screen_set_open(true);
  return true;
}

void shop_screen_ui(nt_ui_context_t *ctx, World *w) {
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
               : clamp_f(layout_w * 0.66F, 540.0F, layout_w - 96.0F);
  const float panel_h =
      portrait ? clamp_f(layout_h - 108.0F, 430.0F, layout_h - 42.0F)
               : clamp_f(layout_h * 0.72F, 350.0F, layout_h - 76.0F);
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
    nt_ui_scroll_style_t scroll_style = game_modal_scroll_style();
    nt_ui_scroll_begin(
        ctx, NT_UI_DATA_LAYER(LAYER_SHOP_BG), nt_ui_id("shop/scroll"),
        &scroll_style,
        &(Clay_ElementDeclaration){
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                       .padding = {.left = 0, .right = 9, .top = 0,
                                   .bottom = 2}},
            .cornerRadius = CLAY_CORNER_RADIUS(4)});
    CLAY({.id = CLAY_ID("shop/items"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = portrait ? 6 : 7,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_TOP}}}) {
      shop_body_ui(ctx, w, shop, portrait);
    }
    nt_ui_scroll_end(ctx);
    nt_ui_panel_end(ctx);
  }
  nt_ui_modal_end(ctx);
  if (s_dismiss_guard_frames > 0) {
    --s_dismiss_guard_frames;
  }
  if (!modal_open) {
    shop_screen_set_open(false);
  }
}
