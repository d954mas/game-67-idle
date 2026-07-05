#include "ui/end_screen.h"

#include "clay.h"
#include "game_actions.h"
#include "game_state.h"
#include "generated/game_assets.h"
#include "ui/game_modal.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_state.h"

#include <stdint.h>
#include <string.h>

#define LAYER_END_BG 60
#define LAYER_END_TEXT 62
#define END_MODAL_ID 0xE0D50011U

#define END_FLAG_TRIGGER "act_i_completed"
#define END_FLAG_SEEN "act_i_ending_seen"

static const nt_ui_widget_def_t END_BUTTON_DEF = {
    .name = "end_screen_button",
    .pill_color = 0xFFE2A75CU,
};

static nt_ui_label_style_t label_style(float size, float r, float g, float b,
                                       float a) {
  return (nt_ui_label_style_t){
      .font_id = 0, .font_size = size, .color = {r, g, b, a}};
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

static bool has_flag(const GameState *state, const char *flag_id) {
  if (!state || !flag_id) {
    return false;
  }
  for (int i = 0; i < state->flags_ids_count; ++i) {
    if (strcmp(state->flags_ids[i], flag_id) == 0) {
      return true;
    }
  }
  return false;
}

static void text_label(nt_ui_context_t *ctx, const char *text,
                       const nt_ui_label_style_t *style) {
  nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_END_TEXT), text ? text : "", style);
}

static bool end_continue_button(nt_ui_context_t *ctx, bool portrait) {
  const Clay_ElementId id = CLAY_ID("end_screen/continue");
  const int16_t hit_pad[4] = {8, 8, 8, 8};
  nt_ui_widget_register(ctx, id.id, &END_BUTTON_DEF, hit_pad, true);
  const nt_ui_events_t events = nt_ui_events_padded(ctx, id.id, NULL, hit_pad);
  const bool hot = events.hovered || events.held;
  const nt_ui_label_style_t label =
      label_style(portrait ? 16.0F : 17.0F, 255.0F, 240.0F, 207.0F, 255.0F);
  CLAY({.id = id,
        .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? 220.0F : 240.0F),
                              CLAY_SIZING_FIXED(44.0F)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = hot ? (Clay_Color){39.0F, 85.0F, 133.0F, 244.0F}
                               : (Clay_Color){31.0F, 70.0F, 113.0F, 238.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .border = {.color = {225.0F, 166.0F, 84.0F, 245.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_END_BG)}) {
    text_label(ctx, "Продолжить", &label);
  }
  return events.clicked;
}

static void end_screen_dismiss(GameState *state, nt_ui_context_t *ctx) {
  (void)game_actions_set_flag(state, END_FLAG_SEEN);
  game_modal_clear_state(ctx, END_MODAL_ID);
}

void end_screen_ui(nt_ui_context_t *ctx, World *w) {
  if (!ctx || !w || !w->player_state || w->dialogue.open) {
    return;
  }
  GameState *state = w->player_state;
  if (!has_flag(state, END_FLAG_TRIGGER) || has_flag(state, END_FLAG_SEEN)) {
    return;
  }

  float layout_w = 0.0F;
  float layout_h = 0.0F;
  nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
  const bool portrait = layout_h > layout_w;
  const float panel_w = portrait ? clamp_f(layout_w - 24.0F, 300.0F, 440.0F)
                                 : clamp_f(layout_w * 0.60F, 520.0F, 720.0F);
  const float panel_h = portrait ? clamp_f(layout_h - 80.0F, 420.0F, layout_h - 32.0F)
                                 : clamp_f(layout_h * 0.72F, 360.0F, layout_h - 40.0F);

  bool modal_open = true;
  nt_ui_modal_style_t modal_style =
      game_modal_style((nt_ui_layer_t)LAYER_END_BG, true);
  if (!game_modal_visible(ctx, END_MODAL_ID, &modal_style, &modal_open, false)) {
    if (!modal_open) {
      end_screen_dismiss(state, ctx);
    }
    return;
  }

  const nt_ui_label_style_t title =
      label_style(portrait ? 26.0F : 30.0F, 255.0F, 236.0F, 196.0F, 255.0F);
  const nt_ui_label_style_t beat =
      label_style(portrait ? 15.0F : 17.0F, 236.0F, 205.0F, 150.0F, 255.0F);
  const nt_ui_label_style_t body =
      label_style(portrait ? 13.5F : 15.0F, 220.0F, 200.0F, 166.0F, 255.0F);
  const nt_ui_label_style_t thanks =
      label_style(portrait ? 17.0F : 19.0F, 255.0F, 232.0F, 180.0F, 255.0F);
  const nt_ui_label_style_t credit =
      label_style(portrait ? 12.0F : 13.0F, 175.0F, 152.0F, 116.0F, 255.0F);

  CLAY({.id = CLAY_ID("end_screen/anchor"),
        .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w),
                              CLAY_SIZING_FIXED(panel_h)}}}) {
    CLAY({.id = CLAY_ID("end_screen/panel"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {13.0F, 9.0F, 7.0F, 236.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .userData = NT_UI_CLAY_DATA(LAYER_END_BG)}) {
      nt_ui_image_style_t panel_image = game_modal_panel_image(portrait);
      nt_ui_panel_begin(
          ctx, NT_UI_DATA_LAYER(LAYER_END_BG),
          game_modal_art(GAME_MODAL_ART_OUTER_FRAME), &panel_image,
          &(Clay_ElementDeclaration){
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = CLAY_PADDING_ALL(portrait ? 18 : 26),
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = portrait ? 8 : 10,
                         .childAlignment = {CLAY_ALIGN_X_CENTER,
                                            CLAY_ALIGN_Y_CENTER}}});
      text_label(ctx, "Мир без тебя", &title);
      text_label(ctx, "Застава выстояла ночь.", &beat);
      CLAY({.id = CLAY_ID("end_screen/gap_a"),
            .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                  CLAY_SIZING_FIXED(6.0F)}}}) {}
      text_label(ctx, "Застава встретила рассвет.", &body);
      text_label(ctx, "Но этой ночью Последний Пост встретил рассвет сам.",
                 &body);
      text_label(ctx, "Мир без своего защитника учится стоять сам,", &body);
      text_label(ctx, "и начинает с тех, кто остался держать ворота.", &body);
      CLAY({.id = CLAY_ID("end_screen/gap_b"),
            .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                  CLAY_SIZING_FIXED(10.0F)}}}) {}
      text_label(ctx, "Спасибо за игру!", &thanks);
      text_label(ctx, "Made for VibeJam #1", &credit);
      CLAY({.id = CLAY_ID("end_screen/gap_c"),
            .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                  CLAY_SIZING_FIXED(8.0F)}}}) {}
      if (end_continue_button(ctx, portrait)) {
        modal_open = false;
      }
      nt_ui_panel_end(ctx);
    }
  }
  nt_ui_modal_end(ctx);
  if (!modal_open) {
    end_screen_dismiss(state, ctx);
  }
}
