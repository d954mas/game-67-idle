#include "ui/world_map_screen.h"

#include "clay.h"
#include "game_audio.h"
#include "game_actions.h"
#include "game_content.h"
#include "game_dialogue.h"
#include "game_state.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/combat_flow.h"
#include "ui/game_modal.h"
#include "ui/location_screen.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_scroll.h"
#include "ui/nt_ui_state.h"
#include "ui/shop_screen.h"
#include "ui/world_map_viewport.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LAYER_WORLD_SCRIM 24
#define LAYER_WORLD_BG 25
#define LAYER_WORLD_MAP_TERRAIN 26
#define LAYER_WORLD_MAP_MARKER 27
#define LAYER_WORLD_TEXT 28
#define WORLD_MAP_MODAL_ID 0xA0B70101U
#define WORLD_SEMANTIC_ID_SLOTS 64
#define WORLD_SEMANTIC_ID_LEN 96
#define WORLD_MAP_Z_TERRAIN 1010
#define WORLD_MAP_Z_MARKER 1014
#define WORLD_MAP_Z_TEXT 1016
#define WORLD_MAP_TRAVEL_MAX_STEPS 4
#define WORLD_MAP_TRAVEL_MAX_PATH_POINTS 8
#define WORLD_MAP_TRAVEL_SEGMENT_SECONDS 5.0F
typedef enum world_map_mode_t {
  WORLD_MAP_MODE_CLOSED = 0,
  WORLD_MAP_MODE_MAP,
} world_map_mode_t;

static world_map_mode_t s_mode = WORLD_MAP_MODE_CLOSED;
static int s_dismiss_guard_frames;
static bool s_cleanup_pending;
static char s_semantic_id_storage[WORLD_SEMANTIC_ID_SLOTS]
                                 [WORLD_SEMANTIC_ID_LEN];
static int s_semantic_id_cursor;
static nt_resource_t s_world_map_atlas;
static nt_atlas_region_ref_t s_ash_border_map_region;
static bool s_map_center_on_hero_pending;
static bool s_map_travel_active;
static int s_map_travel_step_count;
static int s_map_travel_step_index;
static int s_map_travel_frame_count;
static float s_map_travel_segment_started_at;
static char s_map_travel_from_id[GAME_STATE_STRING_MAX];
static char s_map_travel_steps[WORLD_MAP_TRAVEL_MAX_STEPS]
                              [GAME_STATE_STRING_MAX];
static int s_map_travel_path_count;
static float s_map_travel_path_x[WORLD_MAP_TRAVEL_MAX_PATH_POINTS];
static float s_map_travel_path_y[WORLD_MAP_TRAVEL_MAX_PATH_POINTS];

static const nt_ui_widget_def_t WORLD_ROW_DEF = {
    .name = "world_map_row",
    .pill_color = 0xFFB58A45U,
};

static nt_ui_label_style_t label_style(float font_size, float r, float g,
                                       float b, float a) {
  return (nt_ui_label_style_t){
      .font_id = 0, .font_size = font_size, .color = {r, g, b, a}};
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

static void world_map_request_state_cleanup(void) { s_cleanup_pending = true; }

static void world_map_clear_transient_ui_state(nt_ui_context_t *ctx) {
  if (!ctx || !s_cleanup_pending) {
    return;
  }
  game_modal_clear_state(ctx, WORLD_MAP_MODAL_ID);
  nt_ui_state_clear(ctx, nt_ui_id("world_map/viewport_scroll"));
  s_cleanup_pending = false;
}

static bool str_eq(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

static void semantic_ids_begin_frame(void) { s_semantic_id_cursor = 0; }

static Clay_ElementId semantic_clay_id(const char *prefix, const char *suffix) {
  char *buffer =
      s_semantic_id_storage[s_semantic_id_cursor % WORLD_SEMANTIC_ID_SLOTS];
  s_semantic_id_cursor += 1;
  (void)snprintf(buffer, WORLD_SEMANTIC_ID_LEN, "%s%s", prefix ? prefix : "",
                 suffix ? suffix : "");
  return Clay_GetElementId((Clay_String){.isStaticallyAllocated = false,
                                         .length = (int32_t)strlen(buffer),
                                         .chars = buffer});
}

static void text_label(nt_ui_context_t *ctx, const char *text,
                       const nt_ui_label_style_t *style) {
  nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_WORLD_TEXT), text ? text : "", style);
}

static bool is_system_location(const game_location_definition_t *location) {
  return location && str_eq(location->kind, "system_screen");
}

static const char *current_location_id(const GameState *state) {
  return state && state->world_current_location_id[0] != '\0'
             ? state->world_current_location_id
             : "hub_last_post";
}

static const game_location_definition_t *
current_location(const GameState *state) {
  return game_content_find_location(current_location_id(state));
}

static bool map_copy_id(char dst[GAME_STATE_STRING_MAX], const char *src) {
  if (!dst || !src || src[0] == '\0' || strlen(src) >= GAME_STATE_STRING_MAX) {
    return false;
  }
  (void)strcpy(dst, src);
  return true;
}

static int map_route_steps_for(const GameState *state, const char *from_id,
                               const char *target_id,
                               const char *out_steps[],
                               int out_step_capacity) {
  if (!state || !from_id || !target_id || !out_steps ||
      out_step_capacity <= 0 || str_eq(from_id, target_id)) {
    return 0;
  }
  if (game_actions_can_move_location(state, target_id)) {
    out_steps[0] = target_id;
    return 1;
  }
  if (out_step_capacity < 2) {
    return 0;
  }

  if (str_eq(from_id, "hub_last_post") && str_eq(target_id, "old_mill") &&
      game_actions_location_unlocked(state, "hub_gate_outskirts") &&
      game_actions_location_unlocked(state, "old_mill")) {
    out_steps[0] = "hub_gate_outskirts";
    out_steps[1] = "old_mill";
    return 2;
  }
  if (str_eq(from_id, "old_mill") && str_eq(target_id, "hub_last_post") &&
      game_actions_location_unlocked(state, "hub_gate_outskirts") &&
      game_actions_location_unlocked(state, "hub_last_post")) {
    out_steps[0] = "hub_gate_outskirts";
    out_steps[1] = "hub_last_post";
    return 2;
  }
  return 0;
}

static bool map_location_route_available(const GameState *state,
                                         const char *target_id) {
  const char *steps[WORLD_MAP_TRAVEL_MAX_STEPS] = {0};
  return map_route_steps_for(state, current_location_id(state), target_id, steps,
                             WORLD_MAP_TRAVEL_MAX_STEPS) > 0;
}

static bool close_button(nt_ui_context_t *ctx, bool portrait) {
  return game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_WORLD_BG,
                                 (nt_ui_layer_t)LAYER_WORLD_TEXT,
                                 "world_map/close", portrait);
}

static bool location_has_map_node(const game_location_definition_t *location) {
  return location && !is_system_location(location) &&
         location->has_map_position;
}

static float map_node_center_x(const game_location_definition_t *location,
                               float map_w) {
  return world_map_viewport_location_point(location->map_x, location->map_y,
                                           map_w, 720.0F)
      .x;
}

static float map_node_center_y(const game_location_definition_t *location,
                               float map_h) {
  return world_map_viewport_location_point(location->map_x, location->map_y,
                                           1280.0F, map_h)
      .y;
}

static int16_t map_layer_z(nt_ui_layer_t layer) {
  if (layer == (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER) {
    return WORLD_MAP_Z_MARKER;
  }
  if (layer == (nt_ui_layer_t)LAYER_WORLD_TEXT) {
    return WORLD_MAP_Z_TEXT;
  }
  return WORLD_MAP_Z_TERRAIN;
}

static void map_box_on_layer(Clay_ElementId id, nt_ui_layer_t layer, float x,
                             float y, float w, float h, Clay_Color bg,
                             float radius, Clay_Color border,
                             int border_width) {
  if (w <= 0.0F || h <= 0.0F) {
    return;
  }
  const float cr = radius;
  uint16_t bw = 0;
  if (border_width > 0) {
    bw = (uint16_t)border_width;
  }
  CLAY({.id = id,
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                      .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                     .offset = {x, y},
                     .zIndex = map_layer_z(layer)},
        .layout = {.sizing = {CLAY_SIZING_FIXED(w), CLAY_SIZING_FIXED(h)}},
        .backgroundColor = bg,
        .cornerRadius = CLAY_CORNER_RADIUS(cr),
        .border = {.color = border,
                   .width = {bw, bw, bw, bw, 0}},
        .userData = NT_UI_CLAY_DATA(layer)}) {}
}

static void map_box(Clay_ElementId id, float x, float y, float w, float h,
                    Clay_Color bg, float radius, Clay_Color border,
                    int border_width) {
  map_box_on_layer(id, (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER, x, y, w, h, bg,
                   radius, border, border_width);
}

static void ensure_world_map_art(void) {
  if (s_world_map_atlas.id != 0U) {
    return;
  }
  s_world_map_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
  s_ash_border_map_region =
      nt_atlas_ref(s_world_map_atlas, ASSET_ATLAS_REGION_UI_ASH_BORDER_MAP.value);
}

static void draw_ash_border_map_art(nt_ui_context_t *ctx, float map_w,
                                    float map_h) {
  ensure_world_map_art();
  nt_ui_image_style_t art = nt_ui_image_style_defaults();
  art.color_packed = 0xFFFFFFFFU;
  CLAY({.id = CLAY_ID("world_map/art"),
        .layout = {.sizing = {CLAY_SIZING_FIXED(map_w),
                              CLAY_SIZING_FIXED(map_h)}}}) {
    nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_WORLD_MAP_TERRAIN),
                &s_ash_border_map_region, &art, NULL);
  }
}

static bool map_location_visible(GameState *state,
                                 const game_location_definition_t *location) {
  if (!state || !location_has_map_node(location)) {
    return false;
  }
  if (str_eq(current_location_id(state), location->id)) {
    return true;
  }
  return game_actions_location_unlocked(state, location->id) ||
         game_actions_can_move_location(state, location->id) ||
         map_location_route_available(state, location->id) ||
         str_eq(location->id, "old_mill");
}

static const char *
location_marker_text(const game_location_definition_t *location) {
  if (!location || !location->kind) {
    return "?";
  }
  if (str_eq(location->kind, "combat_spot")) {
    return "!";
  }
  if (str_eq(location->kind, "quest_location")) {
    return "*";
  }
  return "P";
}

static Clay_Color location_marker_color(const game_location_definition_t *loc,
                                        bool current, bool hot,
                                        bool unlocked, bool can_move) {
  if (current) {
    return (Clay_Color){147.0F, 77.0F, 29.0F, 242.0F};
  }
  if (!unlocked) {
    return (Clay_Color){43.0F, 38.0F, 33.0F, 180.0F};
  }
  if (hot) {
    return (Clay_Color){104.0F, 76.0F, 38.0F, 236.0F};
  }
  if (can_move) {
    return (Clay_Color){143.0F, 101.0F, 28.0F, 236.0F};
  }
  if (loc && str_eq(loc->kind, "combat_spot")) {
    return (Clay_Color){105.0F, 29.0F, 24.0F, 224.0F};
  }
  if (loc && str_eq(loc->kind, "quest_location")) {
    return (Clay_Color){119.0F, 83.0F, 32.0F, 226.0F};
  }
  return (Clay_Color){47.0F, 72.0F, 52.0F, 226.0F};
}

static bool activate_location_object(World *w,
                                     const game_location_object_t *object,
                                     const game_location_interaction_t
                                         *interaction) {
  if (!w || !w->player_state || !object || !interaction) {
    return false;
  }
  GameState *state = w->player_state;
  if (str_eq(interaction->interaction_type, "dialogue") &&
      interaction->dialogue_id &&
      game_dialogue_open(w, interaction->dialogue_id)) {
    s_mode = WORLD_MAP_MODE_CLOSED;
    return true;
  }
  if (str_eq(interaction->interaction_type, "inspect") && object->id &&
      game_actions_inspect_object(state, object->id)) {
    game_audio_play(GAME_AUDIO_CUE_LOCATION_INSPECT);
    s_mode = WORLD_MAP_MODE_CLOSED;
    location_screen_open_screen();
    return true;
  }
  if (str_eq(interaction->interaction_type, "healer") &&
      game_actions_restore_hp(state)) {
    game_audio_play(GAME_AUDIO_CUE_HEAL);
    s_mode = WORLD_MAP_MODE_CLOSED;
    location_screen_open_screen();
    return true;
  }
  if (str_eq(interaction->interaction_type, "shop") && interaction->shop_id &&
      shop_screen_open_shop(interaction->shop_id)) {
    s_mode = WORLD_MAP_MODE_CLOSED;
    return true;
  }
  if (str_eq(interaction->interaction_type, "start_encounter") &&
      interaction->encounter_id) {
    combat_flow_open_prefight(w, interaction->encounter_id);
    s_mode = WORLD_MAP_MODE_CLOSED;
    return true;
  }
  return false;
}

static float map_travel_elapsed_seconds(const World *w) {
  if (!s_map_travel_active) {
    return 0.0F;
  }
  const float time_elapsed =
      w ? w->time_seconds - s_map_travel_segment_started_at : 0.0F;
  const float frame_elapsed = (float)s_map_travel_frame_count / 60.0F;
  return time_elapsed > frame_elapsed ? time_elapsed : frame_elapsed;
}

static float map_travel_segment_progress(const World *w) {
  return clamp_f(map_travel_elapsed_seconds(w) /
                     WORLD_MAP_TRAVEL_SEGMENT_SECONDS,
                 0.0F, 1.0F);
}

static void map_travel_path_clear(void) {
  s_map_travel_path_count = 0;
  for (int i = 0; i < WORLD_MAP_TRAVEL_MAX_PATH_POINTS; ++i) {
    s_map_travel_path_x[i] = 0.0F;
    s_map_travel_path_y[i] = 0.0F;
  }
}

static bool map_travel_path_add(float design_x, float design_y) {
  if (s_map_travel_path_count >= WORLD_MAP_TRAVEL_MAX_PATH_POINTS) {
    return false;
  }
  s_map_travel_path_x[s_map_travel_path_count] = design_x;
  s_map_travel_path_y[s_map_travel_path_count] = design_y;
  s_map_travel_path_count += 1;
  return true;
}

static bool map_travel_path_add_location(const char *location_id) {
  const game_location_definition_t *location =
      game_content_find_location(location_id);
  if (!location_has_map_node(location)) {
    return false;
  }
  return map_travel_path_add(location->map_x * 1280.0F,
                             location->map_y * 720.0F);
}

static bool map_prepare_travel_leg_path(const char *from_id,
                                        const char *to_id) {
  map_travel_path_clear();
  if (!from_id || !to_id || !map_travel_path_add_location(from_id)) {
    return false;
  }

  if (str_eq(from_id, "hub_last_post") &&
      str_eq(to_id, "hub_gate_outskirts")) {
    return map_travel_path_add(410.0F, 304.0F) &&
           map_travel_path_add(482.0F, 448.0F) &&
           map_travel_path_add(590.0F, 382.0F) &&
           map_travel_path_add_location(to_id);
  }
  if (str_eq(from_id, "hub_gate_outskirts") &&
      str_eq(to_id, "hub_last_post")) {
    return map_travel_path_add(590.0F, 382.0F) &&
           map_travel_path_add(482.0F, 448.0F) &&
           map_travel_path_add(410.0F, 304.0F) &&
           map_travel_path_add_location(to_id);
  }
  if (str_eq(from_id, "hub_gate_outskirts") && str_eq(to_id, "old_mill")) {
    return map_travel_path_add(780.0F, 315.0F) &&
           map_travel_path_add(902.0F, 390.0F) &&
           map_travel_path_add(995.0F, 410.0F) &&
           map_travel_path_add_location(to_id);
  }
  if (str_eq(from_id, "old_mill") && str_eq(to_id, "hub_gate_outskirts")) {
    return map_travel_path_add(995.0F, 410.0F) &&
           map_travel_path_add(902.0F, 390.0F) &&
           map_travel_path_add(780.0F, 315.0F) &&
           map_travel_path_add_location(to_id);
  }
  return map_travel_path_add_location(to_id);
}

static bool map_travel_path_point_at(float progress, float map_w, float map_h,
                                     float *out_x, float *out_y) {
  if (s_map_travel_path_count <= 0) {
    return false;
  }
  if (s_map_travel_path_count == 1) {
    if (out_x) {
      *out_x = s_map_travel_path_x[0] * map_w / 1280.0F;
    }
    if (out_y) {
      *out_y = s_map_travel_path_y[0] * map_h / 720.0F;
    }
    return true;
  }

  float total = 0.0F;
  for (int i = 1; i < s_map_travel_path_count; ++i) {
    const float dx = s_map_travel_path_x[i] - s_map_travel_path_x[i - 1];
    const float dy = s_map_travel_path_y[i] - s_map_travel_path_y[i - 1];
    total += sqrtf(dx * dx + dy * dy);
  }
  if (total <= 0.01F) {
    return false;
  }

  float remaining = clamp_f(progress, 0.0F, 1.0F) * total;
  for (int i = 1; i < s_map_travel_path_count; ++i) {
    const float sx = s_map_travel_path_x[i - 1];
    const float sy = s_map_travel_path_y[i - 1];
    const float ex = s_map_travel_path_x[i];
    const float ey = s_map_travel_path_y[i];
    const float dx = ex - sx;
    const float dy = ey - sy;
    const float len = sqrtf(dx * dx + dy * dy);
    if (remaining > len && i < s_map_travel_path_count - 1) {
      remaining -= len;
      continue;
    }
    const float t = len > 0.01F ? clamp_f(remaining / len, 0.0F, 1.0F) : 1.0F;
    if (out_x) {
      *out_x = (sx + dx * t) * map_w / 1280.0F;
    }
    if (out_y) {
      *out_y = (sy + dy * t) * map_h / 720.0F;
    }
    return true;
  }
  return false;
}

static bool map_begin_travel(World *w, const char *target_id) {
  if (!w || !w->player_state || !target_id) {
    return false;
  }
  GameState *state = w->player_state;
  const char *route_steps[WORLD_MAP_TRAVEL_MAX_STEPS] = {0};
  const int route_count =
      map_route_steps_for(state, current_location_id(state), target_id,
                          route_steps, WORLD_MAP_TRAVEL_MAX_STEPS);
  if (route_count <= 0 ||
      !map_copy_id(s_map_travel_from_id, current_location_id(state))) {
    return false;
  }
  if (!map_prepare_travel_leg_path(current_location_id(state), route_steps[0])) {
    return false;
  }
  for (int i = 0; i < route_count; ++i) {
    if (!map_copy_id(s_map_travel_steps[i], route_steps[i])) {
      return false;
    }
  }
  s_map_travel_active = true;
  s_map_travel_step_count = route_count;
  s_map_travel_step_index = 0;
  s_map_travel_frame_count = 0;
  s_map_travel_segment_started_at = w->time_seconds;
  s_map_center_on_hero_pending = true;
  return true;
}

static void map_cancel_travel(void) {
  s_map_travel_active = false;
  s_map_travel_step_count = 0;
  s_map_travel_step_index = 0;
  s_map_travel_frame_count = 0;
  s_map_travel_from_id[0] = '\0';
  map_travel_path_clear();
  for (int i = 0; i < WORLD_MAP_TRAVEL_MAX_STEPS; ++i) {
    s_map_travel_steps[i][0] = '\0';
  }
}

static void map_update_travel(World *w) {
  if (!s_map_travel_active) {
    return;
  }
  if (!w || !w->player_state || s_map_travel_step_index < 0 ||
      s_map_travel_step_index >= s_map_travel_step_count) {
    map_cancel_travel();
    return;
  }
  s_map_travel_frame_count += 1;
  if (map_travel_segment_progress(w) < 1.0F) {
    return;
  }

  GameState *state = w->player_state;
  const char *next_id = s_map_travel_steps[s_map_travel_step_index];
  if (!game_actions_move_location(state, next_id) ||
      !map_copy_id(s_map_travel_from_id, next_id)) {
    map_cancel_travel();
    return;
  }
  s_map_travel_step_index += 1;
  s_map_travel_frame_count = 0;
  s_map_travel_segment_started_at = w->time_seconds;
  s_map_center_on_hero_pending = true;
  if (s_map_travel_step_index >= s_map_travel_step_count) {
    game_audio_play(GAME_AUDIO_CUE_LOCATION_MOVE);
    map_cancel_travel();
    s_mode = WORLD_MAP_MODE_CLOSED;
    location_screen_open_screen();
  } else if (!map_prepare_travel_leg_path(
                 next_id, s_map_travel_steps[s_map_travel_step_index])) {
    map_cancel_travel();
  }
}

static void map_center_scroll_to_hero(nt_ui_context_t *ctx,
                                      const GameState *state,
                                      uint32_t scroll_id,
                                      world_map_viewport_desc_t viewport) {
  const game_location_definition_t *location = current_location(state);
  if (!ctx || !location_has_map_node(location)) {
    return;
  }
  const world_map_point_t hero = world_map_viewport_location_point(
      location->map_x, location->map_y, viewport.content_w, viewport.content_h);
  const world_map_point_t offset = world_map_viewport_center_offset(
      hero.x, hero.y, viewport.viewport_w, viewport.viewport_h,
      viewport.content_w, viewport.content_h);
  nt_ui_scroll_to(ctx, scroll_id, offset.x, offset.y);
}

static bool map_floating_tool_button(nt_ui_context_t *ctx, const char *id_text,
                                     const char *label, float x, float y,
                                     float size) {
  const Clay_ElementId clay_id =
      Clay_GetElementId((Clay_String){.isStaticallyAllocated = false,
                                      .length = (int32_t)strlen(id_text),
                                      .chars = id_text});
  const int16_t hit_pad[4] = {5, 5, 5, 5};
  nt_ui_widget_register(ctx, clay_id.id, &WORLD_ROW_DEF, hit_pad, true);
  const nt_ui_events_t events =
      nt_ui_events_padded(ctx, clay_id.id, NULL, hit_pad);
  const bool hot = events.hovered || events.held;
  const nt_ui_label_style_t icon =
      label_style(13.0F, 255.0F, 232.0F, 184.0F, 255.0F);
  CLAY({.id = clay_id,
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                      .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                     .offset = {x, y},
                     .zIndex = WORLD_MAP_Z_TEXT + 4},
        .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor =
            hot ? (Clay_Color){91.0F, 58.0F, 28.0F, 238.0F}
                : (Clay_Color){47.0F, 34.0F, 23.0F, 232.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(6),
        .border = {.color = {218.0F, 157.0F, 78.0F, 220.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_WORLD_BG)}) {
    text_label(ctx, label, &icon);
  }
  return events.clicked;
}

static const char *
object_marker_text(const game_location_object_t *object,
                   const game_location_interaction_t *interaction) {
  if (!object) {
    return "?";
  }
  if (str_eq(object->kind, "combat")) {
    return "!";
  }
  if (str_eq(object->kind, "quest_board")) {
    return "*";
  }
  if (str_eq(object->kind, "exit")) {
    return ">";
  }
  if (str_eq(object->kind, "hotspot")) {
    return "?";
  }
  if (interaction && str_eq(interaction->interaction_type, "healer")) {
    return "+";
  }
  return "i";
}

static Clay_Color object_marker_color(const game_location_object_t *object,
                                      const game_location_interaction_t
                                          *interaction,
                                      bool enabled) {
  if (!enabled) {
    return (Clay_Color){52.0F, 45.0F, 39.0F, 170.0F};
  }
  if (object && str_eq(object->kind, "combat")) {
    return (Clay_Color){172.0F, 38.0F, 32.0F, 236.0F};
  }
  if (object && str_eq(object->kind, "quest_board")) {
    return (Clay_Color){190.0F, 139.0F, 38.0F, 238.0F};
  }
  if (object && str_eq(object->kind, "exit")) {
    return (Clay_Color){171.0F, 141.0F, 42.0F, 230.0F};
  }
  if (object && str_eq(object->kind, "hotspot")) {
    return (Clay_Color){69.0F, 103.0F, 118.0F, 228.0F};
  }
  if (interaction && str_eq(interaction->interaction_type, "healer")) {
    return (Clay_Color){51.0F, 119.0F, 88.0F, 228.0F};
  }
  return (Clay_Color){44.0F, 92.0F, 147.0F, 228.0F};
}

typedef struct map_region_gate_t {
  const char *from_id;
  const char *target_id;
  float design_x;
  float design_y;
  float width;
  float height;
} map_region_gate_t;

static const map_region_gate_t MAP_REGION_GATES[] = {
    {"hub_last_post", "hub_gate_outskirts", 482.0F, 448.0F, 34.0F, 72.0F},
    {"hub_gate_outskirts", "hub_last_post", 482.0F, 448.0F, 34.0F, 72.0F},
    {"hub_gate_outskirts", "old_mill", 902.0F, 390.0F, 80.0F, 28.0F},
    {"old_mill", "hub_gate_outskirts", 902.0F, 390.0F, 80.0F, 28.0F},
};

static float map_design_x(float x, float map_w) { return x * map_w / 1280.0F; }

static float map_design_y(float y, float map_h) { return y * map_h / 720.0F; }

static void map_region_gate_ui(const map_region_gate_t *gate, bool open,
                               bool current_region_gate, int route_index,
                               float map_w, float map_h) {
  if (!gate) {
    return;
  }
  const float cx = map_design_x(gate->design_x, map_w);
  const float cy = map_design_y(gate->design_y, map_h);
  const float seg_w = gate->width * map_w / 1280.0F;
  const float seg_h = gate->height * map_h / 720.0F;
  const float halo_pad = current_region_gate ? 7.0F : 4.0F;
  const bool vertical = seg_h > seg_w;
  const float mark_w = vertical ? seg_w * 0.72F : seg_w * 0.50F;
  const float mark_h = vertical ? seg_h * 0.50F : seg_h * 0.72F;
  const float slit_w = vertical ? mark_w * 0.28F : mark_w * 0.62F;
  const float slit_h = vertical ? mark_h * 0.62F : mark_h * 0.28F;
  const Clay_Color shadow =
      open ? (Clay_Color){42.0F, 28.0F, 9.0F, 86.0F}
           : (Clay_Color){36.0F, 31.0F, 27.0F, 72.0F};
  const Clay_Color halo =
      open ? (Clay_Color){255.0F, 216.0F, 58.0F,
                          current_region_gate ? 48.0F : 28.0F}
           : (Clay_Color){86.0F, 72.0F, 46.0F, 26.0F};
  const Clay_Color fill =
      open ? (Clay_Color){247.0F, 181.0F, 34.0F,
                          current_region_gate ? 96.0F : 72.0F}
           : (Clay_Color){89.0F, 75.0F, 52.0F, 70.0F};
  const Clay_Color border =
      open ? (Clay_Color){255.0F, 230.0F, 106.0F, 190.0F}
           : (Clay_Color){138.0F, 115.0F, 71.0F, 90.0F};
  const Clay_Color slit =
      open ? (Clay_Color){255.0F, 241.0F, 148.0F, 170.0F}
           : (Clay_Color){126.0F, 107.0F, 70.0F, 90.0F};

  map_box_on_layer(CLAY_IDI("world_map/route_shadow", route_index),
                   (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER,
                   cx - mark_w * 0.5F + 1.5F, cy - mark_h * 0.5F + 2.0F,
                   mark_w, mark_h, shadow, 5.0F, (Clay_Color){0}, 0);
  map_box_on_layer(CLAY_IDI("world_map/route_halo", route_index),
                   (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER,
                   cx - mark_w * 0.5F - halo_pad,
                   cy - mark_h * 0.5F - halo_pad, mark_w + halo_pad * 2.0F,
                   mark_h + halo_pad * 2.0F, halo, 7.0F, (Clay_Color){0}, 0);
  map_box_on_layer(CLAY_IDI("world_map/route_bridge", route_index),
                   (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER, cx - mark_w * 0.5F,
                   cy - mark_h * 0.5F, mark_w, mark_h, fill, 5.0F, border,
                   open ? 2 : 1);
  map_box_on_layer(CLAY_IDI("world_map/route_slit", route_index),
                   (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER, cx - slit_w * 0.5F,
                   cy - slit_h * 0.5F, slit_w, slit_h, slit, 2.0F,
                   (Clay_Color){0}, 0);
}

static bool map_gate_center_already_rendered(const map_region_gate_t *gate,
                                             const map_region_gate_t *const *rendered,
                                             int rendered_count) {
  if (!gate) {
    return false;
  }
  for (int i = 0; i < rendered_count; ++i) {
    const map_region_gate_t *other = rendered[i];
    if (other && other->design_x == gate->design_x &&
        other->design_y == gate->design_y) {
      return true;
    }
  }
  return false;
}

static void map_routes_ui(GameState *state, float map_w, float map_h) {
  const game_location_definition_t *from = current_location(state);
  if (!state || !location_has_map_node(from)) {
    return;
  }
  int rendered = 0;
  const map_region_gate_t *rendered_gates[16] = {0};
  const int gate_count =
      (int)(sizeof MAP_REGION_GATES / sizeof MAP_REGION_GATES[0]);
  for (int i = 0; i < gate_count; ++i) {
    const map_region_gate_t *gate = &MAP_REGION_GATES[i];
    const bool from_current = str_eq(from->id, gate->from_id);
    const bool target_current = str_eq(from->id, gate->target_id);
    if (!from_current && !target_current) {
      continue;
    }
    if (map_gate_center_already_rendered(gate, rendered_gates, rendered)) {
      continue;
    }
    const char *other_id = from_current ? gate->target_id : gate->from_id;
    const bool target_is_real_location =
        game_content_find_location(other_id) != NULL;
    const bool open = target_is_real_location &&
                      game_actions_can_move_location(state, other_id);
    map_region_gate_ui(gate, open, true, rendered, map_w, map_h);
    rendered_gates[rendered] = gate;
    rendered += 1;
  }
}

static void map_object_offset(const game_location_definition_t *location,
                              const game_location_object_t *object,
                              int object_slot, float *out_x, float *out_y) {
  static const float fallback_offsets[][2] = {
      {-34.0F, -26.0F}, {0.0F, -38.0F},  {34.0F, -24.0F},
      {-44.0F, 8.0F},   {44.0F, 8.0F},   {-22.0F, 34.0F},
      {22.0F, 34.0F},   {0.0F, 48.0F},
  };
  const int fallback_count =
      (int)(sizeof fallback_offsets / sizeof fallback_offsets[0]);
  float x = fallback_offsets[object_slot % fallback_count][0];
  float y = fallback_offsets[object_slot % fallback_count][1];

  if (location && object && str_eq(location->id, "hub_last_post")) {
    if (str_eq(object->id, "hub_last_post.gate_guard")) {
      x = -82.0F;
      y = -8.0F;
    } else if (str_eq(object->id, "hub_last_post.map_gate")) {
      x = -98.0F;
      y = 20.0F;
    } else if (str_eq(object->id, "hub_last_post.blacksmith")) {
      x = 62.0F;
      y = -4.0F;
    } else if (str_eq(object->id, "hub_last_post.town_trader")) {
      x = 82.0F;
      y = 18.0F;
    } else if (str_eq(object->id, "hub_last_post.contract_board")) {
      x = -12.0F;
      y = -48.0F;
    } else if (str_eq(object->id, "hub_last_post.elder")) {
      x = 20.0F;
      y = -38.0F;
    } else if (str_eq(object->id, "hub_last_post.healer")) {
      x = 58.0F;
      y = 34.0F;
    } else if (str_eq(object->id, "hub_last_post.dragon_memorial")) {
      x = 12.0F;
      y = -70.0F;
    } else if (str_eq(object->id, "hub_last_post.caged_scavenger")) {
      x = 88.0F;
      y = -36.0F;
    }
  } else if (location && str_eq(location->id, "hub_gate_outskirts")) {
    x = 0.0F;
    y = -38.0F;
  } else if (location && object && str_eq(location->id, "old_mill")) {
    if (str_eq(object->kind, "combat")) {
      x = -22.0F;
      y = 28.0F;
    } else {
      x = 30.0F;
      y = 18.0F;
    }
  }

  if (out_x) {
    *out_x = x;
  }
  if (out_y) {
    *out_y = y;
  }
}

static void
map_object_marker_ui(nt_ui_context_t *ctx, World *w,
                     const game_location_definition_t *location,
                     const game_location_object_t *object, float origin_x,
                     float origin_y, int object_slot, bool parent_current,
                     bool portrait, float clip_w, float clip_h) {
  if (!ctx || !w || !w->player_state || !location || !object) {
    return;
  }
  GameState *state = w->player_state;
  const game_location_interaction_t *interaction =
      game_actions_select_location_interaction(state, object);
  const bool available =
      interaction != NULL && game_actions_location_object_available(state, object);
  const bool enabled = !s_map_travel_active && parent_current && available;
  const float size = portrait ? 20.0F : 22.0F;
  float offset_x = 0.0F;
  float offset_y = 0.0F;
  map_object_offset(location, object, object_slot, &offset_x, &offset_y);
  const float x = origin_x + offset_x;
  const float y = origin_y + offset_y;
  if (x + size < 0.0F || x - size > clip_w || y + size < 0.0F ||
      y - size > clip_h) {
    return;
  }
  const Clay_ElementId marker_id =
      semantic_clay_id("world_map/object/", object->id);
  const int16_t hit_pad[4] = {7, 7, 7, 7};
  nt_ui_events_t events = {0};
  if (enabled) {
    nt_ui_widget_register(ctx, marker_id.id, &WORLD_ROW_DEF, hit_pad, true);
    events = nt_ui_events_padded(ctx, marker_id.id, NULL, hit_pad);
  }
  const Clay_Color bg = object_marker_color(object, interaction, enabled);
  const nt_ui_label_style_t icon = label_style(
      portrait ? 10.0F : 11.0F, enabled ? 255.0F : 158.0F,
      enabled ? 235.0F : 145.0F, enabled ? 196.0F : 124.0F, 255.0F);

  CLAY({.id = CLAY_IDI("world_map/object_shell", object_slot),
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                      .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                     .offset = {x - size * 0.5F, y - size * 0.5F},
                     .zIndex = WORLD_MAP_Z_MARKER},
        .layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}}}) {
    CLAY({.id = marker_id,
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER,
                                        CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = bg,
          .cornerRadius = CLAY_CORNER_RADIUS(8),
          .border = {.color = {236.0F, 188.0F, 95.0F,
                               enabled ? 205.0F : 82.0F},
                     .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_WORLD_MAP_MARKER)}) {
      text_label(ctx, object_marker_text(object, interaction), &icon);
    }
  }
  if (enabled && events.clicked && activate_location_object(w, object, interaction)) {
    game_audio_play(GAME_AUDIO_CUE_UI_CLICK);
  }
}

static void map_region_hit_size(const game_location_definition_t *location,
                                bool portrait, float *out_w, float *out_h) {
  float w = portrait ? 152.0F : 184.0F;
  float h = portrait ? 118.0F : 136.0F;
  if (location && str_eq(location->id, "hub_last_post")) {
    w = portrait ? 218.0F : 260.0F;
    h = portrait ? 172.0F : 206.0F;
  } else if (location && str_eq(location->id, "hub_gate_outskirts")) {
    w = portrait ? 190.0F : 232.0F;
    h = portrait ? 126.0F : 156.0F;
  } else if (location && str_eq(location->id, "old_mill")) {
    w = portrait ? 206.0F : 252.0F;
    h = portrait ? 140.0F : 174.0F;
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
}

static void map_region_hit_area_ui(nt_ui_context_t *ctx, World *w,
                                   const game_location_definition_t *location,
                                   bool portrait, int index, float map_w,
                                   float map_h) {
  if (!ctx || !w || !w->player_state || !location_has_map_node(location)) {
    return;
  }
  GameState *state = w->player_state;
  const bool current = str_eq(current_location_id(state), location->id);
  const bool can_route =
      !current && map_location_route_available(state, location->id);
  const bool can_move =
      !current && game_actions_can_move_location(state, location->id);
  const bool enabled =
      !s_map_travel_active && (current || can_route || can_move);
  if (!enabled) {
    return;
  }

  float hit_w = 0.0F;
  float hit_h = 0.0F;
  map_region_hit_size(location, portrait, &hit_w, &hit_h);
  const float x = map_node_center_x(location, map_w) - hit_w * 0.5F;
  const float y = map_node_center_y(location, map_h) - hit_h * 0.5F;
  const Clay_ElementId hit_id =
      semantic_clay_id("world_map/region/", location->id);
  const int16_t hit_pad[4] = {0, 0, 0, 0};
  nt_ui_events_t events = {0};

  CLAY({.id = CLAY_IDI("world_map/region_hit_shell", index),
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                      .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                     .offset = {x, y},
                     .zIndex = WORLD_MAP_Z_TERRAIN + 1},
        .layout = {.sizing = {CLAY_SIZING_FIXED(hit_w),
                              CLAY_SIZING_FIXED(hit_h)}}}) {
    CLAY({.id = hit_id,
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
          .backgroundColor = {255.0F, 216.0F, 72.0F,
                              current ? 8.0F : 3.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(18),
          .userData = NT_UI_CLAY_DATA(LAYER_WORLD_MAP_TERRAIN)}) {
      nt_ui_widget_register(ctx, hit_id.id, &WORLD_ROW_DEF, hit_pad, true);
      events = nt_ui_events_padded(ctx, hit_id.id, NULL, hit_pad);
    }
  }

  if (events.clicked) {
    if (current) {
      game_audio_play(GAME_AUDIO_CUE_UI_CLICK);
      s_mode = WORLD_MAP_MODE_CLOSED;
      location_screen_open_screen();
    } else if (map_begin_travel(w, location->id)) {
      game_audio_play(GAME_AUDIO_CUE_LOCATION_MOVE);
    }
  }
}

static void map_location_marker_ui(nt_ui_context_t *ctx, World *w,
                                   const game_location_definition_t *location,
                                   bool portrait, float map_w, float map_h,
                                   int index, float clip_w, float clip_h) {
  if (!ctx || !w || !w->player_state || !location_has_map_node(location)) {
    return;
  }
  GameState *state = w->player_state;
  const bool current = str_eq(current_location_id(state), location->id);
  const bool unlocked = game_actions_location_unlocked(state, location->id);
  const bool can_move =
      current || game_actions_can_move_location(state, location->id);
  const bool can_route =
      !current && map_location_route_available(state, location->id);
  const bool enabled =
      !s_map_travel_active && (current || can_move || can_route);
  const float marker_size =
      current ? (portrait ? 34.0F : 38.0F) : (portrait ? 28.0F : 32.0F);
  const float x = map_node_center_x(location, map_w);
  const float y = map_node_center_y(location, map_h);
  if (x + marker_size < 0.0F || x - marker_size > clip_w ||
      y + marker_size < 0.0F || y - marker_size > clip_h) {
    return;
  }
  const Clay_ElementId marker_id =
      semantic_clay_id("world_map/location/", location->id);
  const int16_t hit_pad[4] = {8, 8, 8, 8};
  nt_ui_events_t events = {0};
  const bool hot = false;
  const Clay_Color bg = location_marker_color(location, current, hot, unlocked,
                                              can_move || can_route);
  const Clay_Color border =
      current    ? (Clay_Color){255.0F, 201.0F, 70.0F, 238.0F}
      : hot      ? (Clay_Color){240.0F, 181.0F, 80.0F, 224.0F}
      : (can_move || can_route) ? (Clay_Color){255.0F, 211.0F, 68.0F, 222.0F}
      : unlocked ? (Clay_Color){184.0F, 147.0F, 69.0F, 190.0F}
                 : (Clay_Color){97.0F, 84.0F, 69.0F, 118.0F};
  const nt_ui_label_style_t icon = label_style(
      portrait ? 13.0F : 14.0F, unlocked ? 255.0F : 139.0F,
      unlocked ? 237.0F : 128.0F, unlocked ? 196.0F : 108.0F, 255.0F);
  const nt_ui_label_style_t label = label_style(
      portrait ? 9.5F : 10.5F, unlocked ? 247.0F : 150.0F,
      unlocked ? 222.0F : 132.0F, unlocked ? 181.0F : 107.0F, 255.0F);
  const nt_ui_label_style_t meta = label_style(
      portrait ? 8.5F : 9.5F, unlocked ? 199.0F : 121.0F,
      unlocked ? 169.0F : 105.0F, unlocked ? 121.0F : 84.0F, 255.0F);
  char meta_text[64];
  (void)snprintf(meta_text, sizeof meta_text, "%s",
                 current    ? "здесь"
                 : can_move ? "путь открыт"
                            : "в тумане");

  CLAY({.id = CLAY_IDI("world_map/location_shell", index),
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                      .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                     .offset = {x - marker_size * 0.5F,
                                y - marker_size * 0.5F},
                     .zIndex = WORLD_MAP_Z_MARKER},
        .layout = {.sizing = {CLAY_SIZING_FIXED(marker_size),
                              CLAY_SIZING_FIXED(marker_size)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}}}) {
    CLAY({.id = marker_id,
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER,
                                        CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = bg,
          .cornerRadius = CLAY_CORNER_RADIUS(10),
          .border = {.color = border, .width = {2, 2, 2, 2, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_WORLD_MAP_MARKER)}) {
      nt_ui_widget_register(ctx, marker_id.id, &WORLD_ROW_DEF, hit_pad,
                            enabled);
      events = nt_ui_events_padded(ctx, marker_id.id, NULL, hit_pad);
      text_label(ctx, location_marker_text(location), &icon);
    }
  }

  CLAY({.id = CLAY_IDI("world_map/location_label", index),
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                      .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                     .offset = {clamp_f(x - 56.0F, 4.0F, map_w - 112.0F),
                                y + marker_size * 0.5F + 4.0F},
                     .zIndex = WORLD_MAP_Z_TEXT},
        .layout = {.sizing = {CLAY_SIZING_FIXED(112.0F),
                              CLAY_SIZING_FIT(0)},
                   .layoutDirection = CLAY_TOP_TO_BOTTOM,
                   .childGap = 0,
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}}}) {
    text_label(ctx, location->display_name, &label);
    text_label(ctx, meta_text, &meta);
  }

  if (enabled && events.clicked) {
    if (current) {
      game_audio_play(GAME_AUDIO_CUE_UI_CLICK);
      s_mode = WORLD_MAP_MODE_CLOSED;
      location_screen_open_screen();
    } else if (map_begin_travel(w, location->id)) {
      game_audio_play(GAME_AUDIO_CUE_LOCATION_MOVE);
    }
  }
}

static bool map_travel_point(const World *w, float map_w, float map_h,
                             float *out_x, float *out_y) {
  if (!s_map_travel_active || s_map_travel_step_index < 0 ||
      s_map_travel_step_index >= s_map_travel_step_count) {
    return false;
  }
  return map_travel_path_point_at(map_travel_segment_progress(w), map_w, map_h,
                                  out_x, out_y);
}

static void map_travel_route_ui(GameState *state, float map_w, float map_h) {
  (void)state;
  if (!s_map_travel_active || s_map_travel_path_count < 2) {
    return;
  }
  int dot_index = 0;
  for (int dot = 1; dot <= 18; ++dot) {
    float x = 0.0F;
    float y = 0.0F;
    if (map_travel_path_point_at((float)dot / 19.0F, map_w, map_h, &x, &y)) {
      map_box_on_layer(
          CLAY_IDI("world_map/travel_dot", dot_index++),
          (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER, x - 3.0F, y - 3.0F, 6.0F,
          6.0F, (Clay_Color){255.0F, 218.0F, 74.0F, 205.0F}, 3.0F,
          (Clay_Color){94.0F, 48.0F, 14.0F, 150.0F}, 1);
    }
  }
}

static void map_travel_status_ui(nt_ui_context_t *ctx, const World *w,
                                 bool portrait, float map_w) {
  if (!ctx || !s_map_travel_active) {
    return;
  }
  const int remaining_steps = s_map_travel_step_count - s_map_travel_step_index;
  float remaining = WORLD_MAP_TRAVEL_SEGMENT_SECONDS -
                    map_travel_elapsed_seconds(w);
  if (remaining < 0.0F) {
    remaining = 0.0F;
  }
  remaining += (float)(remaining_steps - 1) * WORLD_MAP_TRAVEL_SEGMENT_SECONDS;
  int seconds = (int)(remaining + 0.999F);
  if (seconds < 0) {
    seconds = 0;
  }
  char text[32];
  (void)snprintf(text, sizeof text, "Travel 00:%02d", seconds);
  const float plaque_w = portrait ? 118.0F : 132.0F;
  const float plaque_h = portrait ? 28.0F : 30.0F;
  const float plaque_x = clamp_f((map_w - plaque_w) * 0.5F, 8.0F,
                                 map_w - plaque_w - 8.0F);
  const float plaque_y = 10.0F;
  map_box_on_layer(CLAY_ID("world_map/travel_timer_bg"),
                   (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER, plaque_x, plaque_y,
                   plaque_w, plaque_h,
                   (Clay_Color){52.0F, 30.0F, 14.0F, 236.0F}, 6.0F,
                   (Clay_Color){244.0F, 184.0F, 72.0F, 220.0F}, 2);
  CLAY({.id = CLAY_ID("world_map/travel_timer"),
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                      .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                     .offset = {plaque_x + 8.0F, plaque_y + 6.0F},
                     .zIndex = WORLD_MAP_Z_TEXT},
        .layout = {.sizing = {CLAY_SIZING_FIXED(plaque_w - 16.0F),
                              CLAY_SIZING_FIT(0)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}}}) {
    const nt_ui_label_style_t timer =
        label_style(portrait ? 10.0F : 11.0F, 255.0F, 232.0F, 175.0F, 255.0F);
    text_label(ctx, text, &timer);
  }
}

static void map_travel_curtain_ui(float viewport_w, float viewport_h) {
  if (!s_map_travel_active || viewport_w <= 0.0F || viewport_h <= 0.0F) {
    return;
  }
  const float side_w = clamp_f(viewport_w * 0.16F, 34.0F, 96.0F);
  const float top_h = clamp_f(viewport_h * 0.12F, 28.0F, 56.0F);
  const float bottom_h = clamp_f(viewport_h * 0.18F, 42.0F, 92.0F);
  const Clay_Color shade = {11.0F, 7.0F, 4.0F, 116.0F};
  const Clay_Color edge = {246.0F, 182.0F, 64.0F, 42.0F};
  map_box_on_layer(CLAY_ID("world_map/travel_curtain_left"),
                   (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER, 0.0F, 0.0F,
                   side_w, viewport_h, shade, 0.0F, edge, 0);
  map_box_on_layer(CLAY_ID("world_map/travel_curtain_right"),
                   (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER,
                   viewport_w - side_w, 0.0F, side_w, viewport_h, shade,
                   0.0F, edge, 0);
  map_box_on_layer(CLAY_ID("world_map/travel_curtain_top"),
                   (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER, 0.0F, 0.0F,
                   viewport_w, top_h, shade, 0.0F, edge, 0);
  map_box_on_layer(CLAY_ID("world_map/travel_curtain_bottom"),
                   (nt_ui_layer_t)LAYER_WORLD_MAP_MARKER, 0.0F,
                   viewport_h - bottom_h, viewport_w, bottom_h, shade, 0.0F,
                   edge, 0);
}

static void map_hero_marker(nt_ui_context_t *ctx, const World *w,
                            GameState *state, bool portrait, float map_w,
                            float map_h, float clip_w, float clip_h) {
  const game_location_definition_t *location = current_location(state);
  if (!ctx || !state || !location_has_map_node(location)) {
    return;
  }
  float x = map_node_center_x(location, map_w);
  float y = map_node_center_y(location, map_h);
  (void)map_travel_point(w, map_w, map_h, &x, &y);
  if (x + 42.0F < 0.0F || x - 42.0F > clip_w || y + 46.0F < 0.0F ||
      y - 34.0F > clip_h) {
    return;
  }
  const nt_ui_label_style_t hero_label =
      label_style(portrait ? 8.5F : 9.5F, 255.0F, 232.0F, 175.0F, 255.0F);
  map_box(CLAY_ID("world_map/hero/ring"), x - 24.0F, y + 8.0F, 48.0F, 15.0F,
          (Clay_Color){248.0F, 211.0F, 71.0F, 112.0F}, 8,
          (Clay_Color){255.0F, 226.0F, 88.0F, 195.0F}, 2);
  map_box(CLAY_ID("world_map/hero/shadow"), x - 9.0F, y - 20.0F, 18.0F,
          30.0F, (Clay_Color){40.0F, 18.0F, 17.0F, 230.0F}, 9,
          (Clay_Color){246.0F, 183.0F, 70.0F, 180.0F}, 1);
  map_box(CLAY_ID("world_map/hero/cloak"), x - 6.0F, y - 28.0F, 13.0F,
          26.0F, (Clay_Color){154.0F, 32.0F, 29.0F, 236.0F}, 6,
          (Clay_Color){84.0F, 24.0F, 20.0F, 150.0F}, 1);
  map_box(CLAY_ID("world_map/hero/flag_pole"), x + 10.0F, y - 37.0F, 3.0F,
          28.0F, (Clay_Color){98.0F, 58.0F, 32.0F, 220.0F}, 2,
          (Clay_Color){0}, 0);
  map_box(CLAY_ID("world_map/hero/flag"), x + 12.0F, y - 38.0F, 22.0F, 13.0F,
          (Clay_Color){174.0F, 34.0F, 27.0F, 230.0F}, 3,
          (Clay_Color){95.0F, 22.0F, 17.0F, 150.0F}, 1);
  CLAY({.id = CLAY_ID("world_map/hero/label"),
        .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                     .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
                     .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP,
                                      .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                     .offset = {x - 25.0F, y + 25.0F},
                     .zIndex = WORLD_MAP_Z_TEXT},
        .layout = {.sizing = {CLAY_SIZING_FIXED(50.0F),
                              CLAY_SIZING_FIT(0)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}}}) {
    text_label(ctx, "герой", &hero_label);
  }
}

static void map_body(nt_ui_context_t *ctx, World *w, bool portrait,
                     float panel_w, float panel_h) {
  const nt_ui_label_style_t empty =
      label_style(13.0F, 190.0F, 163.0F, 122.0F, 255.0F);
  if (!w || !w->player_state) {
    text_label(ctx, "РќРµС‚ РѕС‚РєСЂС‹С‚С‹С… РјРµСЃС‚.", &empty);
    return;
  }

  const world_map_viewport_desc_t viewport =
      world_map_viewport_compute(panel_w, panel_h, portrait);
  const float map_w = viewport.content_w;
  const float map_h = viewport.content_h;
  const uint32_t scroll_id = nt_ui_id("world_map/viewport_scroll");
  int rendered = 0;
  int region_slot = 0;
  int object_slot = 0;
  GameState *state = w->player_state;

  CLAY(
      {.id = CLAY_ID("world_map/atlas_wrap"),
       .layout = {.sizing = {CLAY_SIZING_GROW(0),
                             CLAY_SIZING_FIXED(viewport.viewport_h + 8.0F)},
                  .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}}) {
    CLAY({.id = CLAY_ID("world_map/viewport_shell"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(viewport.viewport_w),
                                CLAY_SIZING_FIXED(viewport.viewport_h)}}}) {
      nt_ui_scroll_style_t map_scroll = nt_ui_scroll_style_defaults();
      map_scroll.scroll_x = true;
      map_scroll.scroll_y = true;
      map_scroll.bar_visibility = NT_UI_SCROLLBAR_AUTO_HIDE;
      nt_ui_scroll_begin(
          ctx, NT_UI_DATA_LAYER(LAYER_WORLD_BG), scroll_id, &map_scroll,
          &(Clay_ElementDeclaration){
              .layout = {.sizing = {CLAY_SIZING_FIXED(viewport.viewport_w),
                                    CLAY_SIZING_FIXED(viewport.viewport_h)}},
              .cornerRadius = CLAY_CORNER_RADIUS(7)});
      if (s_map_center_on_hero_pending) {
        map_center_scroll_to_hero(ctx, state, scroll_id, viewport);
        s_map_center_on_hero_pending = false;
      }
    CLAY({.id = CLAY_ID("world_map/atlas_canvas"),
          .layout = {.sizing = {CLAY_SIZING_FIXED(map_w),
                                CLAY_SIZING_FIXED(map_h)}},
          .backgroundColor = {20.0F, 17.0F, 12.0F, 238.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(7),
          .border = {.color = {178.0F, 137.0F, 63.0F, 198.0F},
                     .width = {2, 2, 2, 2, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_WORLD_BG)}) {
      draw_ash_border_map_art(ctx, map_w, map_h);
      map_routes_ui(state, map_w, map_h);
      map_travel_route_ui(state, map_w, map_h);

      for (int i = 0; i < game_content_location_count(); ++i) {
        const game_location_definition_t *location =
            game_content_location_at(i);
        if (!map_location_visible(state, location)) {
          continue;
        }
        map_region_hit_area_ui(ctx, w, location, portrait, region_slot++,
                               map_w, map_h);
      }

      for (int i = 0; i < game_content_location_count(); ++i) {
        const game_location_definition_t *location =
            game_content_location_at(i);
        if (!map_location_visible(state, location)) {
          continue;
        }
        const bool parent_current =
            str_eq(current_location_id(state), location->id);
        const float x = map_node_center_x(location, map_w);
        const float y = map_node_center_y(location, map_h);
        for (int object_index = 0; object_index < location->object_count;
             ++object_index) {
          map_object_marker_ui(ctx, w, location, &location->objects[object_index],
                               x, y, object_slot++, parent_current, portrait,
                               map_w, map_h);
        }
      }

      for (int i = 0; i < game_content_location_count(); ++i) {
        const game_location_definition_t *location =
            game_content_location_at(i);
        if (!map_location_visible(state, location)) {
          continue;
        }
        map_location_marker_ui(ctx, w, location, portrait, map_w, map_h,
                               rendered, map_w, map_h);
        rendered += 1;
      }

      map_hero_marker(ctx, w, state, portrait, map_w, map_h, map_w, map_h);

#if 0
      const float plaque_w = portrait ? 190.0F : 240.0F;
      const float plaque_h = portrait ? 30.0F : 34.0F;
      const float plaque_x = (map_w - plaque_w) * 0.5F;
      const float plaque_y = map_h - plaque_h - 8.0F;
      map_box(CLAY_ID("world_map/region_plaque"), plaque_x, plaque_y,
              plaque_w, plaque_h, (Clay_Color){86.0F, 42.0F, 22.0F, 232.0F},
              6, (Clay_Color){221.0F, 154.0F, 72.0F, 220.0F}, 2);
      CLAY({.id = CLAY_ID("world_map/region_plaque_label"),
            .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                         .attachPoints = {.element =
                                              CLAY_ATTACH_POINT_LEFT_TOP,
                                          .parent =
                                              CLAY_ATTACH_POINT_LEFT_TOP},
                         .offset = {plaque_x + 8.0F, plaque_y + 6.0F},
                         .zIndex = WORLD_MAP_Z_TEXT},
            .layout = {.sizing = {CLAY_SIZING_FIXED(plaque_w - 16.0F),
                                  CLAY_SIZING_FIT(0)},
                       .childAlignment = {CLAY_ALIGN_X_CENTER,
                                          CLAY_ALIGN_Y_CENTER}}}) {
        const nt_ui_label_style_t plaque =
            label_style(portrait ? 13.0F : 15.0F, 255.0F, 232.0F, 180.0F,
                        255.0F);
        text_label(ctx, "Пепельная граница", &plaque);
      }
#endif
      }
      nt_ui_scroll_end(ctx);
      map_travel_curtain_ui(viewport.viewport_w, viewport.viewport_h);
      map_travel_status_ui(ctx, w, portrait, viewport.viewport_w);
      if (map_floating_tool_button(ctx, "world_map/center_current", "C",
                                   viewport.viewport_w - 42.0F, 10.0F,
                                   32.0F)) {
        s_map_center_on_hero_pending = true;
      }
    }
  }
  if (rendered == 0) {
    text_label(ctx, "Нет открытых мест.", &empty);
  }
}

bool world_map_screen_open(void) { return s_mode != WORLD_MAP_MODE_CLOSED; }

#if defined(NT_DEVAPI_ENABLED) && NT_DEVAPI_ENABLED
static world_map_mode_t world_map_dev_requested_mode(const GameState *state) {
  if (!state) {
    return WORLD_MAP_MODE_CLOSED;
  }
  for (int i = 0; i < state->flags_ids_count; ++i) {
    if (strcmp(state->flags_ids[i], "dev_world_map_open") == 0) {
      return WORLD_MAP_MODE_MAP;
    }
  }
  return WORLD_MAP_MODE_CLOSED;
}

static bool world_map_dev_auto_travel_requested(const GameState *state) {
  if (!state || s_map_travel_active) {
    return false;
  }
  for (int i = 0; i < state->flags_ids_count; ++i) {
    if (strcmp(state->flags_ids[i], "dev_world_map_auto_travel_old_mill") == 0) {
      return true;
    }
  }
  return false;
}
#endif

static void set_world_map_mode(world_map_mode_t mode) {
  const world_map_mode_t previous = s_mode;
  if (mode != WORLD_MAP_MODE_CLOSED && s_mode == WORLD_MAP_MODE_CLOSED) {
    s_dismiss_guard_frames = 2;
  }
  if (mode == WORLD_MAP_MODE_CLOSED) {
    if (s_mode != WORLD_MAP_MODE_CLOSED) {
      world_map_request_state_cleanup();
    }
    s_dismiss_guard_frames = 0;
  }
  s_mode = mode;
  if (mode == WORLD_MAP_MODE_MAP && previous != WORLD_MAP_MODE_MAP) {
    s_map_center_on_hero_pending = true;
  }
}

void world_map_screen_set_open(bool open) {
  if (!open && s_map_travel_active) {
    return;
  }
  set_world_map_mode(open ? WORLD_MAP_MODE_MAP : WORLD_MAP_MODE_CLOSED);
}

void world_map_screen_open_map(void) { set_world_map_mode(WORLD_MAP_MODE_MAP); }

void world_map_screen_toggle_map(void) {
  if (s_map_travel_active) {
    return;
  }
  set_world_map_mode(s_mode == WORLD_MAP_MODE_MAP ? WORLD_MAP_MODE_CLOSED
                                                  : WORLD_MAP_MODE_MAP);
}

void world_map_screen_ui(nt_ui_context_t *ctx, World *w) {
  world_map_clear_transient_ui_state(ctx);
#if defined(NT_DEVAPI_ENABLED) && NT_DEVAPI_ENABLED
  if (s_mode == WORLD_MAP_MODE_CLOSED && w && w->player_state) {
    const world_map_mode_t dev_mode =
        world_map_dev_requested_mode(w->player_state);
    if (dev_mode != WORLD_MAP_MODE_CLOSED) {
      set_world_map_mode(dev_mode);
    }
  }
#endif
  if (s_mode == WORLD_MAP_MODE_CLOSED || !ctx || !w || w->dialogue.open ||
      !w->player_state) {
    return;
  }
  map_update_travel(w);
  if (s_mode == WORLD_MAP_MODE_CLOSED) {
    return;
  }
#if defined(NT_DEVAPI_ENABLED) && NT_DEVAPI_ENABLED
  if (world_map_dev_auto_travel_requested(w->player_state)) {
    (void)map_begin_travel(w, "old_mill");
  }
#endif
  semantic_ids_begin_frame();

  float layout_w = 0.0F;
  float layout_h = 0.0F;
  nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
  const bool portrait = layout_h > layout_w;
  const float panel_w =
      portrait ? clamp_f(layout_w - 24.0F, 300.0F, 430.0F)
               : clamp_f(layout_w * 0.74F, 620.0F, layout_w - 64.0F);
  const float panel_h =
      portrait ? clamp_f(layout_h - 72.0F, 500.0F, layout_h - 28.0F)
               : clamp_f(layout_h * 0.74F, 360.0F, layout_h - 54.0F);
  const nt_ui_label_style_t title =
      label_style(portrait ? 20.0F : 22.0F, 255.0F, 238.0F, 202.0F, 255.0F);
  const nt_ui_label_style_t hint =
      label_style(portrait ? 12.0F : 13.0F, 205.0F, 178.0F, 133.0F, 255.0F);
  const char *display_title_text = "Карта";
  const char *display_hint_text = "Выбери место на карте.";

  bool modal_open = true;
  nt_ui_modal_style_t modal_style =
      game_modal_style((nt_ui_layer_t)LAYER_WORLD_BG, true);
  const bool ignore_close_request =
      s_dismiss_guard_frames > 0 || s_map_travel_active;
  if (!game_modal_visible(ctx, WORLD_MAP_MODAL_ID, &modal_style, &modal_open,
                          ignore_close_request)) {
    if (!modal_open) {
      set_world_map_mode(WORLD_MAP_MODE_CLOSED);
      world_map_clear_transient_ui_state(ctx);
    }
    return;
  }

  CLAY(
      {.id = CLAY_ID("world_map/panel_anchor"),
       .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w),
                             CLAY_SIZING_FIXED(panel_h)}}}) {
    CLAY({.id = CLAY_ID("world_map/panel"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {13.0F, 9.0F, 7.0F, 232.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .userData = NT_UI_CLAY_DATA(LAYER_WORLD_BG)}) {
      nt_ui_image_style_t panel_image = game_modal_panel_image(portrait);
      nt_ui_panel_begin(
          ctx, NT_UI_DATA_LAYER(LAYER_WORLD_BG),
          game_modal_art(GAME_MODAL_ART_OUTER_FRAME), &panel_image,
          &(Clay_ElementDeclaration){
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = CLAY_PADDING_ALL(portrait ? 10 : 12),
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = portrait ? 8 : 10,
                         .childAlignment = {CLAY_ALIGN_X_LEFT,
                                            CLAY_ALIGN_Y_TOP}}});
      CLAY({.id = CLAY_ID("world_map/header"),
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 10,
                .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 2,
                         .childAlignment = {CLAY_ALIGN_X_LEFT,
                                            CLAY_ALIGN_Y_CENTER}}}) {
          text_label(ctx, display_title_text, &title);
          text_label(ctx, display_hint_text, &hint);
        }
        if (!s_map_travel_active && close_button(ctx, portrait)) {
          modal_open = false;
        }
      }

      CLAY({.id = CLAY_ID("world_map/content"),
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .childGap = portrait ? 6 : 7,
                .childAlignment = {CLAY_ALIGN_X_LEFT,
                                   CLAY_ALIGN_Y_TOP}}}) {
        map_body(ctx, w, portrait, panel_w, panel_h);
      }
      nt_ui_panel_end(ctx);
    }
  }
  nt_ui_modal_end(ctx);
  if (s_dismiss_guard_frames > 0) {
    --s_dismiss_guard_frames;
  }
  if (!modal_open) {
    set_world_map_mode(WORLD_MAP_MODE_CLOSED);
  }
  world_map_clear_transient_ui_state(ctx);
}
