#ifndef RB_DARK_RPG_GAME_CONTENT_H
#define RB_DARK_RPG_GAME_CONTENT_H

#include "game_dialogue.h"

#include <stdbool.h>

typedef enum game_item_kind_t {
  GAME_ITEM_KIND_UNKNOWN = 0,
  GAME_ITEM_KIND_GEAR,
  GAME_ITEM_KIND_QUEST_ITEM,
  GAME_ITEM_KIND_CLUE,
  GAME_ITEM_KIND_CONSUMABLE,
  GAME_ITEM_KIND_MATERIAL,
} game_item_kind_t;

typedef enum game_item_slot_t {
  GAME_ITEM_SLOT_NONE = 0,
  GAME_ITEM_SLOT_WEAPON,
  GAME_ITEM_SLOT_OFFHAND,
  GAME_ITEM_SLOT_HEAD,
  GAME_ITEM_SLOT_ARMOUR,
  GAME_ITEM_SLOT_HANDS,
  GAME_ITEM_SLOT_WAIST,
  GAME_ITEM_SLOT_LEGS,
  GAME_ITEM_SLOT_FEET,
  GAME_ITEM_SLOT_NECK,
  GAME_ITEM_SLOT_RING_LEFT,
  GAME_ITEM_SLOT_RING_RIGHT,
  GAME_ITEM_SLOT_RELIC,
} game_item_slot_t;

typedef struct game_combat_stats_t {
  int vitality;
  int strength;
  int protection;
  int intuition;
  int weapon_damage;
  int bonus_attack_power;
  float attack_interval;
} game_combat_stats_t;

typedef struct game_item_definition_t {
  const char *id;
  const char *display_name;
  const char *icon_asset_id;
  game_item_kind_t kind;
  game_item_slot_t slot;
  bool stackable;
  int max_stack;
  game_combat_stats_t stats;
} game_item_definition_t;

typedef struct game_equipment_slot_definition_t {
  const char *id;
  const char *display_name;
  game_item_slot_t slot;
  bool mvp;
  int ui_order;
} game_equipment_slot_definition_t;

typedef struct game_quest_equip_step_t {
  const char *quest_id;
  const char *step_id;
  const char *item_id;
  const char *complete_flag_id;
  const char *unlock_id;
} game_quest_equip_step_t;

typedef enum game_location_unlock_kind_t {
  GAME_LOCATION_UNLOCK_ALWAYS = 0,
  GAME_LOCATION_UNLOCK_FLAG,
  GAME_LOCATION_UNLOCK_QUEST_STEP,
} game_location_unlock_kind_t;

typedef enum game_location_requirement_kind_t {
  GAME_LOCATION_REQUIREMENT_NONE = 0,
  GAME_LOCATION_REQUIREMENT_FLAG,
  GAME_LOCATION_REQUIREMENT_EQUIPPED,
  GAME_LOCATION_REQUIREMENT_QUEST_ACTIVE,
  GAME_LOCATION_REQUIREMENT_QUEST_STATUS,
  GAME_LOCATION_REQUIREMENT_QUEST_STEP,
} game_location_requirement_kind_t;

typedef struct game_location_requirement_t {
  game_location_requirement_kind_t kind;
  const char *id;
  const char *step_id;
  const char *status;
  bool value;
} game_location_requirement_t;

typedef struct game_location_exit_t {
  const char *target_location_id;
  const game_location_requirement_t *requirements;
  int requirement_count;
} game_location_exit_t;

typedef struct game_location_interaction_t {
  const char *interaction_type;
  const char *dialogue_id;
  const char *shop_id;
  const char *service_id;
  const char *quest_id;
  const char *encounter_id;
  const char *object_id;
  const game_location_requirement_t *requirements;
  int requirement_count;
} game_location_interaction_t;

typedef struct game_location_object_t {
  const char *id;
  const char *kind;
  const char *display_name;
  const char *character_id;
  const char *asset_id;
  const char *encounter_id;
  const game_location_interaction_t *interactions;
  int interaction_count;
  const game_location_requirement_t *requirements;
  int requirement_count;
} game_location_object_t;

typedef struct game_location_definition_t {
  const char *id;
  const char *display_name;
  const char *kind;
  const char *screen_id;
  bool has_map_position;
  float map_x;
  float map_y;
  game_location_unlock_kind_t unlock_kind;
  const char *unlock_flag_id;
  const char *unlock_quest_id;
  const char *unlock_step_id;
  const game_location_object_t *objects;
  int object_count;
  const game_location_exit_t *exits;
  int exit_count;
} game_location_definition_t;

typedef struct game_quest_visit_step_t {
  const char *quest_id;
  const char *step_id;
  const char *location_id;
} game_quest_visit_step_t;

typedef struct game_quest_inspect_step_t {
  const char *quest_id;
  const char *step_id;
  const char *object_id;
} game_quest_inspect_step_t;

typedef enum game_service_requirement_kind_t {
  GAME_SERVICE_REQUIREMENT_NONE = 0,
  GAME_SERVICE_REQUIREMENT_FLAG,
  GAME_SERVICE_REQUIREMENT_QUEST_COMPLETED,
  GAME_SERVICE_REQUIREMENT_QUEST_STATUS,
  GAME_SERVICE_REQUIREMENT_QUEST_STEP,
} game_service_requirement_kind_t;

typedef struct game_service_requirement_t {
  game_service_requirement_kind_t kind;
  const char *id;
  const char *step_id;
  const char *status;
  bool value;
} game_service_requirement_t;

typedef struct game_shop_item_t {
  const char *item_id;
  int price_gold;
  const game_service_requirement_t *requirements;
  int requirement_count;
} game_shop_item_t;

typedef struct game_shop_definition_t {
  const char *id;
  const char *display_name;
  const char *keeper_character_id;
  const char *location_id;
  const game_shop_item_t *items;
  int item_count;
} game_shop_definition_t;

#define GAME_CONTENT_MAX_ENCOUNTER_REWARD_ITEMS 4

typedef struct game_encounter_definition_t {
  const char *id;
  const char *display_name;
  game_combat_stats_t enemy;
  int reward_xp;
  int reward_gold;
  const char *reward_items[GAME_CONTENT_MAX_ENCOUNTER_REWARD_ITEMS];
  int reward_item_count;
  const char *expected_threat;
} game_encounter_definition_t;

typedef struct game_quest_encounter_step_t {
  const char *quest_id;
  const char *step_id;
  const char *encounter_id;
  const char *complete_flag_id;
  const char *unlock_id;
} game_quest_encounter_step_t;

const dialogue_definition_t *
game_content_find_dialogue(const char *dialogue_id);
const game_item_definition_t *game_content_find_item(const char *item_id);
const game_location_definition_t *
game_content_find_location(const char *location_id);
int game_content_location_count(void);
const game_location_definition_t *game_content_location_at(int index);
const game_shop_definition_t *game_content_find_shop(const char *shop_id);
int game_content_shop_count(void);
const game_shop_definition_t *game_content_shop_at(int index);
int game_content_equipment_slot_count(void);
const game_equipment_slot_definition_t *
game_content_equipment_slot_at(int index);
const game_combat_stats_t *game_content_base_player_stats(void);
const game_encounter_definition_t *
game_content_find_encounter(const char *encounter_id);
const char *game_content_next_quest_step(const char *quest_id,
                                         const char *step_id);
const game_quest_equip_step_t *
game_content_find_equip_step(const char *quest_id, const char *item_id);
const game_quest_visit_step_t *
game_content_find_visit_step(const char *quest_id, const char *location_id);
const game_quest_inspect_step_t *
game_content_find_inspect_step(const char *quest_id, const char *object_id);
const game_quest_encounter_step_t *
game_content_find_encounter_step(const char *quest_id,
                                 const char *encounter_id);

#endif
