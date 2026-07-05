#include "ui/location_screen.h"

#include "atlas/nt_atlas.h"
#include "clay.h"
#include "game_actions.h"
#include "game_audio.h"
#include "game_combat.h"
#include "game_content.h"
#include "game_dialogue.h"
#include "game_state.h"
#include "generated/game_assets.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"
#include "ui/combat_flow.h"
#include "ui/game_modal.h"
#include "ui/nt_ui_image.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_panel.h"
#include "ui/nt_ui_scroll.h"
#include "ui/shop_screen.h"
#include "ui/tutorial_callout.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LAYER_LOCATION_BG 25
#define LAYER_LOCATION_ART 27
#define LAYER_LOCATION_TEXT 28
#define LOCATION_MODAL_ID 0xA0B70102U
#define LOCATION_SEMANTIC_ID_SLOTS 64
#define LOCATION_SEMANTIC_ID_LEN 96

typedef enum location_tab_t {
  LOCATION_TAB_ENEMIES = 0,
  LOCATION_TAB_POINTS,
} location_tab_t;

typedef enum location_art_t {
  LOCATION_ART_GATE_GUARD,
  LOCATION_ART_GATE_SCAVENGER,
  LOCATION_ART_MILL_SCAVENGER,
  LOCATION_ART_COUNT,
} location_art_t;

typedef enum location_quest_badge_t {
  LOCATION_QUEST_BADGE_NONE = 0,
  LOCATION_QUEST_BADGE_CONTRACT,
  LOCATION_QUEST_BADGE_AVAILABLE,
  LOCATION_QUEST_BADGE_ACTIVE,
  LOCATION_QUEST_BADGE_READY,
  LOCATION_QUEST_BADGE_COMPLETED,
} location_quest_badge_t;

static bool s_open;
static location_tab_t s_tab = LOCATION_TAB_POINTS;
static bool s_tab_needs_default = true;
static int s_dismiss_guard_frames;
static char s_semantic_id_storage[LOCATION_SEMANTIC_ID_SLOTS]
                                 [LOCATION_SEMANTIC_ID_LEN];
static int s_semantic_id_cursor;
static nt_resource_t s_location_atlas;
static nt_atlas_region_ref_t s_location_regions[LOCATION_ART_COUNT];

static const nt_hash64_t LOCATION_ART_HASHES[LOCATION_ART_COUNT] = {
    ASSET_ATLAS_REGION_UI_GATE_GUARD_PORTRAIT,
    ASSET_ATLAS_REGION_UI_COMBAT_ACTOR_GATE_SCAVENGER,
    ASSET_ATLAS_REGION_UI_COMBAT_ACTOR_MILL_SCAVENGER,
};

static const nt_ui_widget_def_t LOCATION_ROW_DEF = {
    .name = "location_row",
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

static bool str_eq(const char *a, const char *b) {
  return a && b && strcmp(a, b) == 0;
}

static const GameQuestState *find_location_quest(const GameState *state,
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

static bool quest_current_step_matches(const GameQuestState *quest,
                                       const char *step_id) {
  return quest && step_id &&
         quest->status == GAME_STATE_QUEST_STATUS_ACTIVE &&
         quest->has_current_step_id && str_eq(quest->current_step_id, step_id);
}

static bool quest_status_matches(const GameQuestState *quest,
                                 const char *status) {
  if (!quest || !status) {
    return false;
  }
  if (str_eq(status, "hidden")) {
    return quest->status == GAME_STATE_QUEST_STATUS_HIDDEN;
  }
  if (str_eq(status, "available")) {
    return quest->status == GAME_STATE_QUEST_STATUS_AVAILABLE;
  }
  if (str_eq(status, "active")) {
    return quest->status == GAME_STATE_QUEST_STATUS_ACTIVE;
  }
  if (str_eq(status, "ready_to_turn_in")) {
    return quest->status == GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN;
  }
  if (str_eq(status, "completed")) {
    return quest->status == GAME_STATE_QUEST_STATUS_COMPLETED;
  }
  if (str_eq(status, "failed")) {
    return quest->status == GAME_STATE_QUEST_STATUS_FAILED;
  }
  if (str_eq(status, "content_missing")) {
    return quest->status == GAME_STATE_QUEST_STATUS_CONTENT_MISSING;
  }
  return false;
}

static int quest_badge_priority(location_quest_badge_t badge) {
  switch (badge) {
  case LOCATION_QUEST_BADGE_READY:
    return 5;
  case LOCATION_QUEST_BADGE_ACTIVE:
    return 4;
  case LOCATION_QUEST_BADGE_AVAILABLE:
    return 3;
  case LOCATION_QUEST_BADGE_CONTRACT:
    return 2;
  case LOCATION_QUEST_BADGE_COMPLETED:
    return 1;
  case LOCATION_QUEST_BADGE_NONE:
  default:
    return 0;
  }
}

static location_quest_badge_t merge_quest_badge(location_quest_badge_t current,
                                                location_quest_badge_t next) {
  return quest_badge_priority(next) > quest_badge_priority(current) ? next
                                                                    : current;
}

static location_quest_badge_t quest_status_badge(const GameQuestState *quest) {
  if (!quest) {
    return LOCATION_QUEST_BADGE_AVAILABLE;
  }
  switch (quest->status) {
  case GAME_STATE_QUEST_STATUS_AVAILABLE:
  case GAME_STATE_QUEST_STATUS_HIDDEN:
    return LOCATION_QUEST_BADGE_AVAILABLE;
  case GAME_STATE_QUEST_STATUS_ACTIVE:
    return LOCATION_QUEST_BADGE_ACTIVE;
  case GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN:
    return LOCATION_QUEST_BADGE_READY;
  case GAME_STATE_QUEST_STATUS_COMPLETED:
    return LOCATION_QUEST_BADGE_COMPLETED;
  default:
    return LOCATION_QUEST_BADGE_NONE;
  }
}

static location_quest_badge_t requirement_quest_badge(
    const GameState *state, const game_location_requirement_t *requirement) {
  if (!requirement) {
    return LOCATION_QUEST_BADGE_NONE;
  }
  const GameQuestState *quest = find_location_quest(state, requirement->id);
  switch (requirement->kind) {
  case GAME_LOCATION_REQUIREMENT_QUEST_ACTIVE:
    return quest && quest->status == GAME_STATE_QUEST_STATUS_ACTIVE
               ? LOCATION_QUEST_BADGE_ACTIVE
               : LOCATION_QUEST_BADGE_NONE;
  case GAME_LOCATION_REQUIREMENT_QUEST_STATUS:
    return quest_status_matches(quest, requirement->status)
               ? quest_status_badge(quest)
               : LOCATION_QUEST_BADGE_NONE;
  case GAME_LOCATION_REQUIREMENT_QUEST_STEP:
    return quest_current_step_matches(quest, requirement->step_id)
               ? LOCATION_QUEST_BADGE_ACTIVE
               : LOCATION_QUEST_BADGE_NONE;
  default:
    return LOCATION_QUEST_BADGE_NONE;
  }
}

static location_quest_badge_t requirements_quest_badge(
    const GameState *state, const game_location_requirement_t *requirements,
    int requirement_count) {
  location_quest_badge_t badge = LOCATION_QUEST_BADGE_NONE;
  if (!requirements || requirement_count <= 0) {
    return badge;
  }
  for (int i = 0; i < requirement_count; ++i) {
    badge = merge_quest_badge(
        badge, requirement_quest_badge(state, &requirements[i]));
  }
  return badge;
}

static location_quest_badge_t interaction_quest_badge(
    const GameState *state, const game_location_interaction_t *interaction) {
  if (!interaction) {
    return LOCATION_QUEST_BADGE_NONE;
  }
  location_quest_badge_t badge = requirements_quest_badge(
      state, interaction->requirements, interaction->requirement_count);
  if (str_eq(interaction->interaction_type, "quest_list")) {
    badge = merge_quest_badge(badge, LOCATION_QUEST_BADGE_CONTRACT);
  }
  if (interaction->quest_id || str_eq(interaction->interaction_type, "quest")) {
    badge = merge_quest_badge(
        badge, quest_status_badge(find_location_quest(state,
                                                      interaction->quest_id)));
  }
  return badge;
}

static location_quest_badge_t
object_quest_badge(const GameState *state,
                   const game_location_object_t *object) {
  if (!object) {
    return LOCATION_QUEST_BADGE_NONE;
  }
  location_quest_badge_t badge =
      str_eq(object->kind, "quest_board") ? LOCATION_QUEST_BADGE_CONTRACT
                                          : LOCATION_QUEST_BADGE_NONE;
  badge = merge_quest_badge(
      badge, requirements_quest_badge(state, object->requirements,
                                      object->requirement_count));
  for (int i = 0; i < object->interaction_count; ++i) {
    badge = merge_quest_badge(
        badge, interaction_quest_badge(state, &object->interactions[i]));
  }
  return badge;
}

static void ensure_location_art_regions(void) {
  if (s_location_atlas.id != 0U) {
    return;
  }
  s_location_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
  for (int i = 0; i < LOCATION_ART_COUNT; ++i) {
    s_location_regions[i] = nt_atlas_ref(s_location_atlas,
                                         LOCATION_ART_HASHES[i].value);
  }
}

static void semantic_ids_begin_frame(void) { s_semantic_id_cursor = 0; }

static Clay_ElementId semantic_clay_id(const char *prefix, const char *suffix) {
  char *buffer =
      s_semantic_id_storage[s_semantic_id_cursor % LOCATION_SEMANTIC_ID_SLOTS];
  s_semantic_id_cursor += 1;
  (void)snprintf(buffer, LOCATION_SEMANTIC_ID_LEN, "%s%s", prefix ? prefix : "",
                 suffix ? suffix : "");
  return Clay_GetElementId((Clay_String){.isStaticallyAllocated = false,
                                         .length = (int32_t)strlen(buffer),
                                         .chars = buffer});
}

static void text_label(nt_ui_context_t *ctx, const char *text,
                       const nt_ui_label_style_t *style) {
  nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_LOCATION_TEXT), text ? text : "",
              style);
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

static bool is_combat_object(const game_location_object_t *object) {
  if (!object) {
    return false;
  }
  if (str_eq(object->kind, "combat")) {
    return true;
  }
  for (int i = 0; i < object->interaction_count; ++i) {
    if (str_eq(object->interactions[i].interaction_type, "start_encounter")) {
      return true;
    }
  }
  return false;
}

static bool is_person_object(const game_location_object_t *object,
                             const game_location_interaction_t *interaction) {
  return (object && str_eq(object->kind, "npc")) ||
         (interaction && (str_eq(interaction->interaction_type, "dialogue") ||
                          str_eq(interaction->interaction_type, "healer")));
}

static bool object_matches_tab(const GameState *state,
                               const game_location_object_t *object,
                               location_tab_t tab) {
  const game_location_interaction_t *interaction =
      game_actions_select_location_interaction(state, object);
  if (!interaction || !object) {
    return false;
  }
  const bool combat = is_combat_object(object) ||
                      str_eq(interaction->interaction_type, "start_encounter");
  return tab == LOCATION_TAB_ENEMIES ? combat : !combat;
}

static location_tab_t
default_tab(const GameState *state, const game_location_definition_t *location) {
  if (!location) {
    return LOCATION_TAB_POINTS;
  }
  for (int i = 0; i < location->object_count; ++i) {
    if (object_matches_tab(state, &location->objects[i],
                           LOCATION_TAB_ENEMIES)) {
      return LOCATION_TAB_ENEMIES;
    }
  }
  return LOCATION_TAB_POINTS;
}

static const char *kind_label(const char *kind) {
  if (str_eq(kind, "npc")) {
    return "NPC";
  }
  if (str_eq(kind, "combat")) {
    return "Враг";
  }
  if (str_eq(kind, "hotspot")) {
    return "Осмотр";
  }
  if (str_eq(kind, "quest_board")) {
    return "Доска";
  }
  if (str_eq(kind, "exit")) {
    return "Переход";
  }
  return "Точка";
}

static const char *interaction_label(const char *interaction_type) {
  if (str_eq(interaction_type, "dialogue")) {
    return "разговор";
  }
  if (str_eq(interaction_type, "quest")) {
    return "задание";
  }
  if (str_eq(interaction_type, "quest_list")) {
    return "контракты";
  }
  if (str_eq(interaction_type, "healer")) {
    return "лечение";
  }
  if (str_eq(interaction_type, "shop")) {
    return "торговля";
  }
  if (str_eq(interaction_type, "service")) {
    return "услуга";
  }
  if (str_eq(interaction_type, "inspect")) {
    return "осмотр";
  }
  if (str_eq(interaction_type, "start_encounter")) {
    return "бой";
  }
  return interaction_type ? interaction_type : "осмотр";
}

static const char *localized_threat(const char *threat) {
  if (str_eq(threat, "easy")) {
    return "Легко";
  }
  if (str_eq(threat, "fair")) {
    return "Ровно";
  }
  if (str_eq(threat, "risky")) {
    return "Риск";
  }
  if (str_eq(threat, "deadly")) {
    return "Смертельно";
  }
  return "Опасность";
}

static int simple_attack_power(const game_combat_stats_t *stats) {
  if (!stats) {
    return 0;
  }
  return stats->weapon_damage + stats->bonus_attack_power + stats->strength / 10;
}

static const char *quest_badge_text(location_quest_badge_t badge) {
  switch (badge) {
  case LOCATION_QUEST_BADGE_READY:
  case LOCATION_QUEST_BADGE_AVAILABLE:
    return "!";
  case LOCATION_QUEST_BADGE_ACTIVE:
    return ">";
  case LOCATION_QUEST_BADGE_COMPLETED:
    return "OK";
  case LOCATION_QUEST_BADGE_CONTRACT:
    return "?";
  case LOCATION_QUEST_BADGE_NONE:
  default:
    return "";
  }
}

static Clay_Color quest_badge_color(location_quest_badge_t badge,
                                    bool enabled) {
  if (!enabled) {
    return (Clay_Color){78.0F, 60.0F, 35.0F, 150.0F};
  }
  switch (badge) {
  case LOCATION_QUEST_BADGE_READY:
    return (Clay_Color){55.0F, 132.0F, 79.0F, 238.0F};
  case LOCATION_QUEST_BADGE_ACTIVE:
    return (Clay_Color){48.0F, 103.0F, 159.0F, 238.0F};
  case LOCATION_QUEST_BADGE_COMPLETED:
    return (Clay_Color){82.0F, 100.0F, 88.0F, 220.0F};
  case LOCATION_QUEST_BADGE_CONTRACT:
  case LOCATION_QUEST_BADGE_AVAILABLE:
    return (Clay_Color){196.0F, 133.0F, 31.0F, 238.0F};
  case LOCATION_QUEST_BADGE_NONE:
  default:
    return (Clay_Color){78.0F, 60.0F, 35.0F, 150.0F};
  }
}

static const char *object_icon_text(const game_location_object_t *object,
                                    const game_location_interaction_t
                                        *interaction,
                                    location_quest_badge_t quest_badge_kind) {
  if (!object) {
    return "?";
  }
  if (is_combat_object(object)) {
    return "!";
  }
  if (quest_badge_kind != LOCATION_QUEST_BADGE_NONE &&
      str_eq(object->kind, "quest_board")) {
    return quest_badge_text(quest_badge_kind);
  }
  if (interaction && str_eq(interaction->interaction_type, "healer")) {
    return "+";
  }
  if (interaction && str_eq(interaction->interaction_type, "shop")) {
    return "$";
  }
  if (is_person_object(object, interaction)) {
    return "NPC";
  }
  if (str_eq(object->kind, "hotspot")) {
    return "?";
  }
  return "i";
}

static Clay_Color object_icon_color(const game_location_object_t *object,
                                    const game_location_interaction_t
                                        *interaction,
                                    location_quest_badge_t quest_badge_kind,
                                    bool enabled) {
  if (!enabled) {
    return (Clay_Color){52.0F, 45.0F, 39.0F, 170.0F};
  }
  if (object && is_combat_object(object)) {
    return (Clay_Color){172.0F, 38.0F, 32.0F, 236.0F};
  }
  if (object && quest_badge_kind != LOCATION_QUEST_BADGE_NONE &&
      str_eq(object->kind, "quest_board")) {
    return quest_badge_color(quest_badge_kind, enabled);
  }
  if (interaction && str_eq(interaction->interaction_type, "healer")) {
    return (Clay_Color){51.0F, 119.0F, 88.0F, 228.0F};
  }
  if (interaction && str_eq(interaction->interaction_type, "shop")) {
    return (Clay_Color){158.0F, 104.0F, 36.0F, 230.0F};
  }
  if (is_person_object(object, interaction)) {
    return (Clay_Color){45.0F, 91.0F, 146.0F, 230.0F};
  }
  if (object && str_eq(object->kind, "hotspot")) {
    return (Clay_Color){69.0F, 103.0F, 118.0F, 228.0F};
  }
  return (Clay_Color){44.0F, 92.0F, 147.0F, 228.0F};
}

static const char *object_encounter_id(
    const game_location_object_t *object,
    const game_location_interaction_t *interaction) {
  if (object && object->encounter_id) {
    return object->encounter_id;
  }
  return interaction ? interaction->encounter_id : NULL;
}

static nt_atlas_region_ref_t *enemy_region_for_encounter(
    const char *encounter_id) {
  if (!encounter_id) {
    return NULL;
  }
  ensure_location_art_regions();
  if (strstr(encounter_id, "mill") != NULL) {
    return &s_location_regions[LOCATION_ART_MILL_SCAVENGER];
  }
  return &s_location_regions[LOCATION_ART_GATE_SCAVENGER];
}

static nt_atlas_region_ref_t *
person_region_for_object(const game_location_object_t *object) {
  if (object && str_eq(object->character_id, "gate_guard")) {
    ensure_location_art_regions();
    return &s_location_regions[LOCATION_ART_GATE_GUARD];
  }
  return NULL;
}

static void close_location_screen(void) {
  s_open = false;
  s_dismiss_guard_frames = 0;
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
    close_location_screen();
    return true;
  }
  if (str_eq(interaction->interaction_type, "inspect") && object->id &&
      game_actions_inspect_object(state, object->id)) {
    game_audio_play(GAME_AUDIO_CUE_LOCATION_INSPECT);
    s_tab_needs_default = true;
    return true;
  }
  if (str_eq(interaction->interaction_type, "healer") &&
      game_actions_restore_hp(state)) {
    game_audio_play(GAME_AUDIO_CUE_HEAL);
    s_tab_needs_default = true;
    return true;
  }
  if (str_eq(interaction->interaction_type, "shop") && interaction->shop_id &&
      shop_screen_open_shop(interaction->shop_id)) {
    close_location_screen();
    return true;
  }

  const char *encounter_id = object_encounter_id(object, interaction);
  if (str_eq(interaction->interaction_type, "start_encounter") &&
      encounter_id) {
    combat_flow_open_prefight(w, encounter_id);
    close_location_screen();
    return true;
  }
  return false;
}

static bool close_button(nt_ui_context_t *ctx, bool portrait) {
  return game_modal_close_button(ctx, (nt_ui_layer_t)LAYER_LOCATION_BG,
                                 (nt_ui_layer_t)LAYER_LOCATION_TEXT,
                                 "location_screen/close", portrait);
}

static void icon_chip(nt_ui_context_t *ctx, int slot, const char *text,
                      Clay_Color bg, bool enabled, float size) {
  const bool compact_text = text && strlen(text) > 1U;
  const nt_ui_label_style_t icon = label_style(
      compact_text ? (size > 30.0F ? 10.0F : 9.0F)
                   : (size > 28.0F ? 14.0F : 12.0F),
      enabled ? 255.0F : 158.0F,
      enabled ? 235.0F : 145.0F, enabled ? 196.0F : 124.0F, 255.0F);
  CLAY({.id = CLAY_IDI("location_screen/icon_chip", slot),
        .layout = {.sizing = {CLAY_SIZING_FIXED(size),
                              CLAY_SIZING_FIXED(size)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = bg,
        .cornerRadius = CLAY_CORNER_RADIUS(6),
        .border = {.color = {236.0F, 188.0F, 95.0F, enabled ? 205.0F : 82.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_LOCATION_BG)}) {
    text_label(ctx, text, &icon);
  }
}

static void image_media(nt_ui_context_t *ctx, int slot,
                        nt_atlas_region_ref_t *region, bool enabled,
                        float width, float height) {
  nt_ui_image_style_t style = nt_ui_image_style_defaults();
  style.color_packed = enabled ? 0xFFFFFFFFU : 0x99FFFFFFU;
  CLAY({.id = CLAY_IDI("location_screen/image_frame", slot),
        .layout = {.sizing = {CLAY_SIZING_FIXED(width),
                              CLAY_SIZING_FIXED(height)},
                   .padding = {.left = 3, .right = 3, .top = 3, .bottom = 3},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = enabled ? (Clay_Color){22.0F, 16.0F, 12.0F, 235.0F}
                                   : (Clay_Color){23.0F, 19.0F, 16.0F, 160.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(5),
        .border = {.color = enabled ? (Clay_Color){173.0F, 112.0F, 59.0F, 205.0F}
                                    : (Clay_Color){90.0F, 70.0F, 48.0F, 118.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_LOCATION_BG)}) {
    CLAY({.id = CLAY_IDI("location_screen/image", slot),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}}) {
      nt_ui_image(ctx, NT_UI_DATA_LAYER(LAYER_LOCATION_ART), region, &style,
                  NULL);
    }
  }
}

static void quest_badge(nt_ui_context_t *ctx, int slot,
                        location_quest_badge_t badge, bool enabled,
                        float size) {
  if (badge == LOCATION_QUEST_BADGE_NONE) {
    return;
  }
  const char *text = quest_badge_text(badge);
  const bool compact_text = text && strlen(text) > 1U;
  const nt_ui_label_style_t icon =
      label_style(compact_text ? 8.0F : (size > 18.0F ? 12.0F : 10.0F),
                  enabled ? 255.0F : 158.0F, enabled ? 238.0F : 145.0F,
                  enabled ? 176.0F : 108.0F, 255.0F);
  CLAY({.id = CLAY_IDI("location_screen/quest_badge", slot),
        .layout = {.sizing = {CLAY_SIZING_FIXED(size),
                              CLAY_SIZING_FIXED(size)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = quest_badge_color(badge, enabled),
        .cornerRadius = CLAY_CORNER_RADIUS(5),
        .border = {.color = enabled ? (Clay_Color){246.0F, 194.0F, 86.0F, 215.0F}
                                    : (Clay_Color){118.0F, 92.0F, 55.0F, 120.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_LOCATION_BG)}) {
    text_label(ctx, text, &icon);
  }
}

static void action_chip(nt_ui_context_t *ctx, int slot, const char *label,
                        bool enabled, bool portrait) {
  const nt_ui_label_style_t text = label_style(
      portrait ? 11.0F : 12.0F, enabled ? 255.0F : 142.0F,
      enabled ? 226.0F : 124.0F, enabled ? 178.0F : 96.0F, 255.0F);
  CLAY({.id = CLAY_IDI("location_screen/action_chip", slot),
        .layout = {.sizing = {CLAY_SIZING_FIXED(portrait ? 48.0F : 54.0F),
                              CLAY_SIZING_FIXED(portrait ? 26.0F : 28.0F)},
                   .childAlignment = {CLAY_ALIGN_X_CENTER,
                                      CLAY_ALIGN_Y_CENTER}},
        .backgroundColor = enabled ? (Clay_Color){102.0F, 54.0F, 28.0F, 232.0F}
                                   : (Clay_Color){50.0F, 39.0F, 32.0F, 150.0F},
        .cornerRadius = CLAY_CORNER_RADIUS(4),
        .border = {.color = enabled ? (Clay_Color){220.0F, 143.0F, 68.0F, 215.0F}
                                    : (Clay_Color){94.0F, 75.0F, 54.0F, 118.0F},
                   .width = {1, 1, 1, 1, 0}},
        .userData = NT_UI_CLAY_DATA(LAYER_LOCATION_BG)}) {
    text_label(ctx, label ? label : ">", &text);
  }
}

static void object_media(nt_ui_context_t *ctx,
                         const game_location_object_t *object,
                         const game_location_interaction_t *interaction,
                         location_quest_badge_t quest_badge_kind,
                         bool enabled, bool portrait, int index) {
  const bool combat = object && is_combat_object(object);
  const bool quest_signal = quest_badge_kind != LOCATION_QUEST_BADGE_NONE &&
                            object && !str_eq(object->kind, "quest_board");
  nt_atlas_region_ref_t *region =
      combat ? enemy_region_for_encounter(object_encounter_id(object, interaction))
             : person_region_for_object(object);
  const float image_w = combat ? (portrait ? 58.0F : 64.0F)
                               : (portrait ? 34.0F : 36.0F);
  const float image_h = combat ? (portrait ? 76.0F : 82.0F)
                               : (portrait ? 34.0F : 36.0F);
  const float chip_size = combat ? (portrait ? 46.0F : 50.0F)
                                 : (portrait ? 30.0F : 32.0F);

  CLAY({.id = CLAY_IDI("location_screen/object_media", index),
        .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                   .layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .childGap = portrait ? 4 : 5,
                   .childAlignment = {CLAY_ALIGN_X_LEFT,
                                      CLAY_ALIGN_Y_CENTER}}}) {
    if (region) {
      image_media(ctx, 500 + index, region, enabled, image_w, image_h);
    } else {
      icon_chip(ctx, 100 + index,
                object_icon_text(object, interaction, quest_badge_kind),
                object_icon_color(object, interaction, quest_badge_kind,
                                  enabled),
                enabled,
                chip_size);
    }
    if (quest_signal) {
      quest_badge(ctx, 700 + index, quest_badge_kind, enabled,
                  portrait ? 20.0F : 22.0F);
    }
  }
}

static bool row_frame(nt_ui_context_t *ctx, Clay_ElementId row_id, bool enabled,
                      bool selected, float height, Clay_Color *out_bg,
                      Clay_Color *out_border) {
  const uint32_t id = row_id.id;
  const int16_t hit_pad[4] = {3, 3, 3, 3};
  nt_ui_widget_register(ctx, id, &LOCATION_ROW_DEF, hit_pad, enabled);
  const nt_ui_events_t events = nt_ui_events_padded(ctx, id, NULL, hit_pad);
  const bool hot = enabled && (events.hovered || events.held);
  if (out_bg) {
    *out_bg =
        selected ? (Clay_Color){84.0F, 50.0F, 25.0F, 238.0F}
        : hot    ? (Clay_Color){57.0F, 39.0F, 25.0F, 230.0F}
                 : (Clay_Color){29.0F, 21.0F, 15.0F,
                                enabled ? 214.0F : 150.0F};
  }
  if (out_border) {
    *out_border =
        selected ? (Clay_Color){220.0F, 154.0F, 74.0F, 232.0F}
        : hot    ? (Clay_Color){167.0F, 114.0F, 58.0F, 215.0F}
                 : (Clay_Color){107.0F, 77.0F, 45.0F,
                                enabled ? 170.0F : 118.0F};
  }
  (void)height;
  return enabled && events.clicked;
}

static void object_row(nt_ui_context_t *ctx, World *w,
                       const game_location_object_t *object, bool portrait,
                       int index) {
  if (!object || !w) {
    return;
  }
  GameState *state = w->player_state;
  const game_location_interaction_t *interaction =
      game_actions_select_location_interaction(state, object);
  const bool available = interaction != NULL;
  const bool combat = is_combat_object(object);
  const location_quest_badge_t quest_badge_kind =
      object_quest_badge(state, object);
  const nt_ui_label_style_t title = label_style(
      portrait ? 13.0F : 14.0F, available ? 248.0F : 150.0F,
      available ? 229.0F : 132.0F, available ? 190.0F : 110.0F, 255.0F);
  const nt_ui_label_style_t meta = label_style(
      portrait ? 11.0F : 12.0F, available ? 197.0F : 120.0F,
      available ? 172.0F : 106.0F, available ? 132.0F : 88.0F, 255.0F);
  const Clay_ElementId row_id =
      semantic_clay_id("world_place/object/", object->id);
  char meta_text[96];
  const char *action =
      interaction_label(interaction ? interaction->interaction_type : "inspect");
  (void)snprintf(meta_text, sizeof meta_text, "%s - %s",
                 kind_label(object->kind), available ? action : "закрыто");

  if (interaction && str_eq(interaction->interaction_type, "inspect")) {
    (void)snprintf(meta_text, sizeof meta_text, "%s", kind_label(object->kind));
  }

  const char *encounter_id = object_encounter_id(object, interaction);
  if (interaction && str_eq(interaction->interaction_type, "start_encounter") &&
      encounter_id) {
    const game_encounter_definition_t *encounter =
        game_content_find_encounter(encounter_id);
    if (encounter) {
      (void)snprintf(meta_text, sizeof meta_text,
                     "%s - HP %d - Урон %d - %d XP, %d зол.",
                     localized_threat(encounter->expected_threat),
                     encounter->enemy.vitality,
                     simple_attack_power(&encounter->enemy),
                     encounter->reward_xp, encounter->reward_gold);
    }
  }

  const float row_h = combat ? (portrait ? 96.0F : 102.0F)
                             : (portrait ? 56.0F : 58.0F);
  Clay_Color bg = {0};
  Clay_Color border = {0};
  const bool clicked =
      row_frame(ctx, row_id, available, false, row_h, &bg, &border);

  CLAY({.id = CLAY_IDI("world_place/object_shell", index),
        .layout = {.sizing = {CLAY_SIZING_GROW(0),
                              CLAY_SIZING_FIXED(row_h)}}}) {
    CLAY({.id = row_id,
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = 10, .right = 10, .top = 7, .bottom = 7},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = bg,
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = border, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_LOCATION_BG)}) {
      object_media(ctx, object, interaction, quest_badge_kind, available,
                   portrait, index);
      CLAY({.id = CLAY_IDI("location_screen/object_text", index),
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                       .layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .childGap = 2,
                       .childAlignment = {CLAY_ALIGN_X_LEFT,
                                          CLAY_ALIGN_Y_CENTER}}}) {
        text_label(ctx, object->display_name, &title);
        text_label(ctx, meta_text, &meta);
      }
      if (combat) {
        action_chip(ctx, 900 + index, action, available, portrait);
      }
    }
  }

  if (clicked && activate_location_object(w, object, interaction)) {
    game_audio_play(GAME_AUDIO_CUE_UI_CLICK);
  }
}

static void exit_row(nt_ui_context_t *ctx, World *w,
                     const game_location_exit_t *exit_def, bool portrait,
                     int index) {
  if (!w || !w->player_state || !exit_def || !exit_def->target_location_id) {
    return;
  }
  GameState *state = w->player_state;
  const game_location_definition_t *target =
      game_content_find_location(exit_def->target_location_id);
  const bool can_move =
      game_actions_can_move_location(state, exit_def->target_location_id);
  const Clay_ElementId row_id =
      semantic_clay_id("world_place/exit/", exit_def->target_location_id);
  const nt_ui_label_style_t title = label_style(
      portrait ? 13.0F : 14.0F, can_move ? 248.0F : 150.0F,
      can_move ? 229.0F : 132.0F, can_move ? 190.0F : 110.0F, 255.0F);
  const nt_ui_label_style_t meta = label_style(
      portrait ? 11.0F : 12.0F, can_move ? 197.0F : 120.0F,
      can_move ? 172.0F : 106.0F, can_move ? 132.0F : 88.0F, 255.0F);
  const float row_h = portrait ? 54.0F : 56.0F;
  Clay_Color bg = {0};
  Clay_Color border = {0};
  const bool clicked =
      row_frame(ctx, row_id, can_move, false, row_h, &bg, &border);

  CLAY({.id = CLAY_IDI("world_place/exit_shell", index),
        .layout = {.sizing = {CLAY_SIZING_GROW(0),
                              CLAY_SIZING_FIXED(row_h)}}}) {
    CLAY({.id = row_id,
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = 10, .right = 10, .top = 7, .bottom = 7},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 10,
                     .childAlignment = {CLAY_ALIGN_X_LEFT,
                                        CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = bg,
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = border, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_LOCATION_BG)}) {
      icon_chip(ctx, 300 + index, ">", (Clay_Color){171.0F, 141.0F, 42.0F,
                                                    can_move ? 230.0F : 130.0F},
                can_move, portrait ? 28.0F : 30.0F);
      CLAY({.id = CLAY_IDI("location_screen/exit_text", index),
            .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                       .layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .childGap = 2,
                       .childAlignment = {CLAY_ALIGN_X_LEFT,
                                          CLAY_ALIGN_Y_CENTER}}}) {
        text_label(ctx,
                   target && target->display_name ? target->display_name
                                                  : exit_def->target_location_id,
                   &title);
        text_label(ctx, can_move ? "Перейти" : "Недоступно", &meta);
      }
    }
  }

  if (clicked && game_actions_move_location(state, exit_def->target_location_id)) {
    game_audio_play(GAME_AUDIO_CUE_LOCATION_MOVE);
    s_tab_needs_default = true;
  }
}

static void tab_button(nt_ui_context_t *ctx, const char *id_suffix,
                       const char *icon, const char *label, location_tab_t tab,
                       bool portrait, int index) {
  const bool selected = s_tab == tab;
  const Clay_ElementId row_id =
      semantic_clay_id("world_place/tab/", id_suffix);
  const float tab_h = portrait ? 38.0F : 40.0F;
  Clay_Color bg = {0};
  Clay_Color border = {0};
  const bool clicked = row_frame(ctx, row_id, true, selected, tab_h, &bg, &border);
  const nt_ui_label_style_t text = label_style(
      portrait ? 13.0F : 14.0F, selected ? 255.0F : 213.0F,
      selected ? 235.0F : 190.0F, selected ? 196.0F : 150.0F, 255.0F);

  CLAY({.id = CLAY_IDI("world_place/tab_shell", index),
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(tab_h)}}}) {
    CLAY({.id = row_id,
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .padding = {.left = 10, .right = 10, .top = 5, .bottom = 5},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 7,
                     .childAlignment = {CLAY_ALIGN_X_CENTER,
                                        CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = bg,
          .cornerRadius = CLAY_CORNER_RADIUS(4),
          .border = {.color = border, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_LOCATION_BG)}) {
      text_label(ctx, icon, &text);
      text_label(ctx, label, &text);
    }
  }

  if (clicked) {
    s_tab = tab;
    s_tab_needs_default = false;
    game_audio_play(GAME_AUDIO_CUE_UI_CLICK);
  }
}

static void tabs_ui(nt_ui_context_t *ctx, bool portrait) {
  CLAY({.id = CLAY_ID("world_place/tabs"),
        .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                   .layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .childGap = 7,
                   .childAlignment = {CLAY_ALIGN_X_LEFT,
                                      CLAY_ALIGN_Y_CENTER}}}) {
    tab_button(ctx, "enemies", "!", "Враги", LOCATION_TAB_ENEMIES, portrait, 0);
    tab_button(ctx, "environment", "?", "Точки", LOCATION_TAB_POINTS, portrait, 1);
  }
}

static void gate_check_hint_ui(nt_ui_context_t *ctx, const GameState *state,
                               bool portrait, float layout_w, float layout_h,
                               float panel_w, float panel_h) {
  if (!ctx || !state || !game_actions_needs_gate_check_onboarding(state)) {
    return;
  }
  const float panel_left = (layout_w - panel_w) * 0.5F;
  const float panel_top = (layout_h - panel_h) * 0.5F;
  const bool target_tab = s_tab != LOCATION_TAB_ENEMIES;
  const float target_x =
      target_tab ? panel_left + panel_w * 0.22F : panel_left + panel_w * 0.78F;
  const float target_y =
      target_tab ? panel_top + (portrait ? 92.0F : 96.0F)
                 : panel_top + (portrait ? 148.0F : 154.0F);
  const float finger = portrait ? 62.0F : 68.0F;
  tutorial_callout_style_t style =
      tutorial_callout_default_style(portrait, layout_w);
  style.width = portrait ? clamp_f(layout_w - 34.0F, 220.0F, 332.0F) : 276.0F;
  tutorial_callout_ui(ctx,
                      &(tutorial_callout_desc_t){
                          .visible = true,
                          .slot = 4,
                          .text = target_tab ? "Враги" : "Проверка боя",
                          .element_anchor = TUTORIAL_CALLOUT_ANCHOR_CENTER_BOTTOM,
                          .parent_anchor = TUTORIAL_CALLOUT_ANCHOR_LEFT_TOP,
                          .offset_x = clamp_f(target_x, style.width * 0.5F + 12.0F,
                                              layout_w - style.width * 0.5F - 12.0F),
                          .offset_y = clamp_f(target_y - 46.0F, 82.0F,
                                              layout_h - 122.0F),
                          .style = style});
  tutorial_finger_ui(ctx,
                     &(tutorial_finger_desc_t){
                         .visible = true,
                         .slot = 4,
                         .offset_x = clamp_f(target_x + 12.0F, 0.0F,
                                             layout_w - finger),
                         .offset_y = clamp_f(target_y - finger * 0.32F, 0.0F,
                                             layout_h - finger),
                         .size = finger,
                         .flip_bits = 0U});
}

static void body_ui(nt_ui_context_t *ctx, World *w, bool portrait,
                    float layout_w, float layout_h, float panel_w,
                    float panel_h) {
  GameState *state = w ? w->player_state : NULL;
  const game_location_definition_t *location = current_location(state);
  const nt_ui_label_style_t empty =
      label_style(13.0F, 190.0F, 163.0F, 122.0F, 255.0F);
  if (!location) {
    text_label(ctx, "Локация не найдена.", &empty);
    return;
  }
  if (s_tab_needs_default) {
    s_tab = default_tab(state, location);
    s_tab_needs_default = false;
  }

  tabs_ui(ctx, portrait);

  int rendered = 0;
  for (int i = 0; i < location->object_count; ++i) {
    const game_location_object_t *object = &location->objects[i];
    if (!object_matches_tab(state, object, s_tab)) {
      continue;
    }
    object_row(ctx, w, object, portrait, rendered);
    rendered += 1;
  }

  if (s_tab == LOCATION_TAB_POINTS) {
    for (int i = 0; i < location->exit_count; ++i) {
      exit_row(ctx, w, &location->exits[i], portrait, rendered);
      rendered += 1;
    }
  }

  if (rendered == 0) {
    text_label(ctx, s_tab == LOCATION_TAB_ENEMIES ? "Врагов нет."
                                                  : "Точек нет.",
               &empty);
  }
  gate_check_hint_ui(ctx, state, portrait, layout_w, layout_h, panel_w,
                     panel_h);
}

bool location_screen_open(void) { return s_open; }

void location_screen_set_open(bool open) {
  if (open && !s_open) {
    s_dismiss_guard_frames = 2;
    s_tab_needs_default = true;
  }
  if (!open) {
    s_dismiss_guard_frames = 0;
  }
  s_open = open;
}

void location_screen_open_screen(void) { location_screen_set_open(true); }

void location_screen_toggle(void) { location_screen_set_open(!s_open); }

#if defined(NT_DEVAPI_ENABLED) && NT_DEVAPI_ENABLED
static bool dev_requested_open(const GameState *state) {
  if (!state) {
    return false;
  }
  for (int i = 0; i < state->flags_ids_count; ++i) {
    if (strcmp(state->flags_ids[i], "dev_world_place_open") == 0) {
      return true;
    }
  }
  return false;
}
#endif

void location_screen_ui(nt_ui_context_t *ctx, World *w) {
#if defined(NT_DEVAPI_ENABLED) && NT_DEVAPI_ENABLED
  if (!s_open && w && w->player_state && dev_requested_open(w->player_state)) {
    location_screen_set_open(true);
  }
#endif
  if (!s_open || !ctx || !w || w->dialogue.open || !w->player_state) {
    return;
  }
  semantic_ids_begin_frame();

  float layout_w = 0.0F;
  float layout_h = 0.0F;
  nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
  const bool portrait = layout_h > layout_w;
  const float panel_w =
      portrait ? clamp_f(layout_w - 24.0F, 300.0F, 430.0F)
               : clamp_f(layout_w * 0.64F, 520.0F, layout_w - 96.0F);
  const float panel_h =
      portrait ? clamp_f(layout_h - 92.0F, 430.0F, layout_h - 38.0F)
               : clamp_f(layout_h - 40.0F, 390.0F, layout_h - 28.0F);
  const nt_ui_label_style_t title =
      label_style(portrait ? 20.0F : 22.0F, 255.0F, 238.0F, 202.0F, 255.0F);
  const nt_ui_label_style_t hint =
      label_style(portrait ? 12.0F : 13.0F, 205.0F, 178.0F, 133.0F, 255.0F);
  const game_location_definition_t *location =
      current_location(w->player_state);
  const char *display_title_text =
      location && location->display_name ? location->display_name : "Здесь";

  bool modal_open = true;
  nt_ui_modal_style_t modal_style =
      game_modal_style((nt_ui_layer_t)LAYER_LOCATION_BG, true);
  const bool ignore_close_request = s_dismiss_guard_frames > 0;
  if (!game_modal_visible(ctx, LOCATION_MODAL_ID, &modal_style, &modal_open,
                          ignore_close_request)) {
    if (!modal_open) {
      location_screen_set_open(false);
    }
    return;
  }

  CLAY({.id = CLAY_ID("location_screen/panel_anchor"),
        .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w),
                              CLAY_SIZING_FIXED(panel_h)}}}) {
    CLAY({.id = CLAY_ID("location_screen/panel"),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}},
          .backgroundColor = {13.0F, 9.0F, 7.0F, 232.0F},
          .cornerRadius = CLAY_CORNER_RADIUS(5),
          .userData = NT_UI_CLAY_DATA(LAYER_LOCATION_BG)}) {
      nt_ui_image_style_t panel_image = game_modal_panel_image(portrait);
      nt_ui_panel_begin(
          ctx, NT_UI_DATA_LAYER(LAYER_LOCATION_BG),
          game_modal_art(GAME_MODAL_ART_OUTER_FRAME), &panel_image,
          &(Clay_ElementDeclaration){
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = CLAY_PADDING_ALL(portrait ? 10 : 12),
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = portrait ? 8 : 10,
                         .childAlignment = {CLAY_ALIGN_X_LEFT,
                                            CLAY_ALIGN_Y_TOP}}});
      CLAY({.id = CLAY_ID("location_screen/header"),
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 10,
                .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        CLAY({.id = CLAY_ID("location_screen/header_text"),
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM,
                         .childGap = 2,
                         .childAlignment = {CLAY_ALIGN_X_LEFT,
                                            CLAY_ALIGN_Y_CENTER}}}) {
          text_label(ctx, display_title_text, &title);
          text_label(ctx, "Враги и точки интереса", &hint);
        }
        if (close_button(ctx, portrait)) {
          modal_open = false;
        }
      }

      nt_ui_scroll_style_t scroll_style = game_modal_scroll_style();
      scroll_style.bar_visibility = NT_UI_SCROLLBAR_AUTO;
      const uint32_t scroll_id = nt_ui_id("world_place/scroll");
      nt_ui_scroll_begin(
          ctx, NT_UI_DATA_LAYER(LAYER_LOCATION_BG), scroll_id, &scroll_style,
          &(Clay_ElementDeclaration){
              .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                         .padding = {.left = 0, .right = 12, .top = 0,
                                     .bottom = 2}},
              .cornerRadius = CLAY_CORNER_RADIUS(4)});
      CLAY({.id = CLAY_ID("location_screen/content"),
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .childGap = portrait ? 6 : 6,
                .childAlignment = {CLAY_ALIGN_X_LEFT,
                                   CLAY_ALIGN_Y_TOP}}}) {
        body_ui(ctx, w, portrait, layout_w, layout_h, panel_w, panel_h);
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
    location_screen_set_open(false);
  }
}
