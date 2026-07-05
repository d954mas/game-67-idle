#include "ui/journal_screen.h"

#include "clay.h"
#include "game_content.h"
#include "game_state.h"
#include "ui/game_modal.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_scroll.h"
#include "ui/nt_ui_state.h"

#include <stdio.h>
#include <string.h>

#define LAYER_JOURNAL_BG 24
#define LAYER_JOURNAL_TEXT 25
#define JOURNAL_MODAL_ID 0xA0B70105U

static bool s_open;
static int s_dismiss_guard_frames;
static bool s_cleanup_pending;

static void journal_request_state_cleanup(void) { s_cleanup_pending = true; }

static void journal_clear_transient_ui_state(nt_ui_context_t *ctx) {
  if (!ctx || !s_cleanup_pending) {
    return;
  }
  game_modal_clear_state(ctx, JOURNAL_MODAL_ID);
  nt_ui_state_clear(ctx, nt_ui_id("journal_screen/scroll"));
  s_cleanup_pending = false;
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

static bool str_eq(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

static nt_ui_label_style_t label_style(float font_size, float r, float g,
                                       float b, float a) {
  return (nt_ui_label_style_t){
      .font_id = 0, .font_size = font_size, .color = {r, g, b, a}};
}

static void text_label(nt_ui_context_t *ctx, const char *text,
                       const nt_ui_label_style_t *style) {
  nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_JOURNAL_TEXT), text ? text : "",
              style);
}

static const GameQuestState *find_state_quest(const GameState *state,
                                              const char *quest_id) {
  if (!state || !quest_id) {
    return NULL;
  }
  for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
    const GameQuestState *quest = &state->quests_quest_states[i];
    if (quest->used && str_eq(quest->key, quest_id)) {
      return quest;
    }
  }
  return NULL;
}

static const char *quest_status_label(int status) {
  switch (status) {
  case GAME_STATE_QUEST_STATUS_AVAILABLE:
    return "Доступно";
  case GAME_STATE_QUEST_STATUS_ACTIVE:
    return "Активно";
  case GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN:
    return "Сдать";
  case GAME_STATE_QUEST_STATUS_COMPLETED:
    return "Готово";
  case GAME_STATE_QUEST_STATUS_FAILED:
    return "Провал";
  case GAME_STATE_QUEST_STATUS_CONTENT_MISSING:
    return "Ошибка";
  case GAME_STATE_QUEST_STATUS_HIDDEN:
  default:
    return "Скрыто";
  }
}

static Clay_Color quest_status_color(int status) {
  switch (status) {
  case GAME_STATE_QUEST_STATUS_ACTIVE:
    return (Clay_Color){48.0F, 103.0F, 159.0F, 238.0F};
  case GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN:
    return (Clay_Color){55.0F, 132.0F, 79.0F, 238.0F};
  case GAME_STATE_QUEST_STATUS_COMPLETED:
    return (Clay_Color){82.0F, 100.0F, 88.0F, 220.0F};
  case GAME_STATE_QUEST_STATUS_AVAILABLE:
    return (Clay_Color){196.0F, 133.0F, 31.0F, 238.0F};
  default:
    return (Clay_Color){78.0F, 60.0F, 35.0F, 150.0F};
  }
}

static bool quest_visible(const GameQuestState *state_quest) {
  return state_quest &&
         state_quest->status != GAME_STATE_QUEST_STATUS_HIDDEN &&
         state_quest->status != GAME_STATE_QUEST_STATUS_CONTENT_MISSING;
}

static void status_chip(nt_ui_context_t *ctx, int slot, int status,
                        bool portrait) {
  const nt_ui_label_style_t label =
      label_style(portrait ? 10.5F : 11.5F, 255.0F, 235.0F, 199.0F, 255.0F);
  CLAY({.id = CLAY_IDI("journal_screen/status", slot),
        .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? 62.0F : 72.0F),
                              CLAY_SIZING_FIXED(portrait ? 24.0F : 26.0F)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = quest_status_color(status),
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .border = {.color = {222.0F, 155.0F, 82.0F, 180.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_JOURNAL_BG)}) {
    text_label(ctx, quest_status_label(status), &label);
  }
}

static bool completed_step(const GameState *state, const char *step_id) {
  if (!state || !step_id) {
    return false;
  }
  for (int i = 0; i < state->quests_completed_step_ids_count; ++i) {
    if (str_eq(state->quests_completed_step_ids[i], step_id)) {
      return true;
    }
  }
  return false;
}

static int completed_step_count(const GameState *state,
                                const game_quest_definition_t *quest) {
  int count = 0;
  if (!state || !quest) {
    return count;
  }
  for (int i = 0; i < quest->step_count; ++i) {
    if (completed_step(state, quest->steps[i].id)) {
      ++count;
    }
  }
  return count;
}

static void current_step_card(nt_ui_context_t *ctx,
                              const game_quest_step_definition_t *step,
                              bool portrait, int slot) {
  const nt_ui_label_style_t eyebrow =
      label_style(portrait ? 10.5F : 11.5F, 210.0F, 162.0F, 82.0F, 255.0F);
  const nt_ui_label_style_t title =
      label_style(portrait ? 14.0F : 15.5F, 255.0F, 236.0F, 195.0F, 255.0F);
  const nt_ui_label_style_t desc =
      label_style(portrait ? 11.0F : 12.5F, 202.0F, 177.0F, 135.0F, 255.0F);

  CLAY({.id = CLAY_IDI("journal_screen/current_step", slot),
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                   .padding = {.left = 10, .right = 10, .top = 8, .bottom = 9},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = 4,
                   .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
        .backgroundColor = {39.0F, 28.0F, 17.0F, 218.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .border = {.color = {192.0F, 128.0F, 56.0F, 185.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_JOURNAL_BG)}) {
    text_label(ctx, "Текущий шаг", &eyebrow);
    text_label(ctx, step && step->title ? step->title : "Нет текущего шага",
               &title);
    if (step && step->description) {
      text_label(ctx, step->description, &desc);
    }
  }
}

static void quest_row(nt_ui_context_t *ctx, const GameState *state,
                      const game_quest_definition_t *quest,
                      const GameQuestState *state_quest, bool portrait,
                      int slot) {
  const bool tracked =
      state && state->has_quests_tracked_quest_id &&
      str_eq(state->quests_tracked_quest_id, quest ? quest->id : NULL);
  const nt_ui_label_style_t title =
      label_style(portrait ? 15.0F : 16.5F, 255.0F, 235.0F, 196.0F, 255.0F);
  const nt_ui_label_style_t goal =
      label_style(portrait ? 11.0F : 12.5F, 203.0F, 176.0F, 132.0F, 255.0F);
  const nt_ui_label_style_t progress =
      label_style(portrait ? 10.5F : 12.0F, 164.0F, 142.0F, 108.0F, 255.0F);

  CLAY({.id = CLAY_IDI("journal_screen/quest", slot),
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                   .padding = {.left = 10, .right = 10, .top = 8, .bottom = 9},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = portrait ? 6 : 7,
                   .childAlignment = {CLAY_ALIGN_X_LEFT,
                                      CLAY_ALIGN_Y_TOP}},
        .backgroundColor = tracked ? (Clay_Color){35.0F, 25.0F, 15.0F, 232.0F}
                                   : (Clay_Color){21.0F, 15.0F, 11.0F, 210.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(5),
        .border = {.color = tracked ? (Clay_Color){211.0F, 147.0F, 70.0F, 222.0F}
                                    : (Clay_Color){107.0F, 77.0F, 45.0F, 170.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_JOURNAL_BG)}) {
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 8,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_CENTER}}}) {
      CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0),
                                  CLAY_SIZING_FIT(0)}}}) {
        text_label(ctx, quest && quest->title ? quest->title : "Задание",
                   &title);
      }
      status_chip(ctx, slot, state_quest ? state_quest->status
                                         : GAME_STATE_QUEST_STATUS_HIDDEN,
                  portrait);
    }
    if (quest && quest->short_goal) {
      text_label(ctx, quest->short_goal, &goal);
    }

    const game_quest_step_definition_t *active_step =
        state_quest && state_quest->has_current_step_id
            ? game_content_find_quest_step(quest->id,
                                           state_quest->current_step_id)
            : NULL;
    if (state_quest &&
        state_quest->status == GAME_STATE_QUEST_STATUS_COMPLETED) {
      current_step_card(ctx, NULL, portrait, slot);
    } else {
      current_step_card(ctx, active_step, portrait, slot);
    }

    char progress_buf[64];
    const int completed = completed_step_count(state, quest);
    const int total = quest ? quest->step_count : 0;
    (void)snprintf(progress_buf, sizeof progress_buf, "Прогресс: %d/%d",
                   completed, total);
    text_label(ctx, progress_buf, &progress);
  }
}

static void journal_body(nt_ui_context_t *ctx, const GameState *state,
                         bool portrait) {
  const nt_ui_label_style_t empty =
      label_style(13.0F, 190.0F, 163.0F, 122.0F, 255.0F);
  int rendered = 0;
  for (int i = 0; i < game_content_quest_count(); ++i) {
    const game_quest_definition_t *quest = game_content_quest_at(i);
    const GameQuestState *state_quest =
        find_state_quest(state, quest ? quest->id : NULL);
    if (!quest_visible(state_quest)) {
      continue;
    }
    quest_row(ctx, state, quest, state_quest, portrait, rendered);
    ++rendered;
  }
  if (rendered == 0) {
    text_label(ctx, "Записей пока нет.", &empty);
  }
}

bool journal_screen_open(void) { return s_open; }

void journal_screen_set_open(bool open) {
  if (open && !s_open) {
    s_dismiss_guard_frames = 2;
  }
  if (!open && s_open) {
    journal_request_state_cleanup();
  }
  if (!open) {
    s_dismiss_guard_frames = 0;
  }
  s_open = open;
}

void journal_screen_toggle(void) { journal_screen_set_open(!s_open); }

void journal_screen_ui(nt_ui_context_t *ctx, World *w) {
  journal_clear_transient_ui_state(ctx);
  if (!s_open || !ctx || !w || w->dialogue.open || !w->player_state) {
    return;
  }

  float layout_w = 0.0F;
  float layout_h = 0.0F;
  nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
  const bool portrait = layout_h > layout_w;
  const float panel_w =
      portrait ? clamp_f(layout_w - 24.0F, 300.0F, 430.0F)
               : clamp_f(layout_w * 0.58F, 500.0F, layout_w - 116.0F);
  const float panel_h =
      portrait ? clamp_f(layout_h - 112.0F, 430.0F, layout_h - 44.0F)
               : clamp_f(layout_h - 56.0F, 360.0F, layout_h - 36.0F);
  const nt_ui_label_style_t title =
      label_style(portrait ? 20.0F : 22.0F, 255.0F, 238.0F, 202.0F, 255.0F);
  const nt_ui_label_style_t hint =
      label_style(portrait ? 12.0F : 13.0F, 205.0F, 178.0F, 133.0F, 255.0F);

  bool modal_open = true;
  nt_ui_modal_style_t modal_style =
      game_modal_style((nt_ui_layer_t)LAYER_JOURNAL_BG, true);
  const bool ignore_close_request = s_dismiss_guard_frames > 0;
  if (!game_modal_visible(ctx, JOURNAL_MODAL_ID, &modal_style, &modal_open,
                          ignore_close_request)) {
    if (!modal_open) {
      journal_screen_set_open(false);
      journal_clear_transient_ui_state(ctx);
    }
    return;
  }

  CLAY({.id = CLAY_ID("journal_screen/panel_anchor"),
        .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w),
                              CLAY_SIZING_FIXED(panel_h)}}}) {
    CLAY({.id = CLAY_ID("journal_screen/panel"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {13.0F, 9.0F, 7.0F, 232.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .userData = NT_UI_CLAY_DATA(LAYER_JOURNAL_BG)}) {
      nt_ui_image_style_t panel_image = game_modal_panel_image(portrait);
      nt_ui_panel_begin(
          ctx, NT_UI_DATA_LAYER(LAYER_JOURNAL_BG),
          game_modal_art(GAME_MODAL_ART_OUTER_FRAME), &panel_image,
          &(Clay_ElementDeclaration){
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = CLAY_PADDING_ALL(portrait ? 10 : 12),
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = portrait ? 8 : 10,
                         .childAlignment = {CLAY_ALIGN_X_LEFT,
                                            CLAY_ALIGN_Y_TOP}}});
      CLAY({.id = CLAY_ID("journal_screen/header"),
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                       .layoutDirection = CLAY_LEFT_TO_RIGHT,
                       .childGap = 10,
                       .childAlignment = {CLAY_ALIGN_X_LEFT,
                                          CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0),
                                    CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 2,
                         .childAlignment = {CLAY_ALIGN_X_LEFT,
                                            CLAY_ALIGN_Y_CENTER}}}) {
          text_label(ctx, "Дневник", &title);
          text_label(ctx, "Задания и текущие шаги", &hint);
        }
        if (game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_JOURNAL_BG,
                                    (nt_ui_layer_t)LAYER_JOURNAL_TEXT,
                                    "journal/close", portrait)) {
          modal_open = false;
        }
      }

      nt_ui_scroll_style_t scroll_style = game_modal_scroll_style();
      scroll_style.bar_visibility = NT_UI_SCROLLBAR_AUTO;
      nt_ui_scroll_begin(
          ctx, NT_UI_DATA_LAYER(LAYER_JOURNAL_BG),
          nt_ui_id("journal_screen/scroll"), &scroll_style,
          &(Clay_ElementDeclaration){
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = {.left = 0, .right = 12, .top = 0,
                                     .bottom = 2}},
              .cornerRadius = CLAY_CORNER_RADIUS(4)});
      CLAY({.id = CLAY_ID("journal_screen/content"),
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                       .layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .childGap = portrait ? 7 : 8,
                       .childAlignment = {CLAY_ALIGN_X_LEFT,
                                          CLAY_ALIGN_Y_TOP}}}) {
        journal_body(ctx, w->player_state, portrait);
      }
      nt_ui_scroll_end(ctx);
      nt_ui_panel_end(ctx);
    }
  }
  nt_ui_modal_end(ctx);
  if (s_dismiss_guard_frames > 0) {
    --s_dismiss_guard_frames;
  }
  if (!modal_open) {
    journal_screen_set_open(false);
  }
  journal_clear_transient_ui_state(ctx);
}
