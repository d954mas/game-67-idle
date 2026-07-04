#ifndef RB_DARK_RPG_GAME_CONTENT_H
#define RB_DARK_RPG_GAME_CONTENT_H

#include "game_dialogue.h"

#include <stdbool.h>

typedef enum game_item_kind_t {
    GAME_ITEM_KIND_UNKNOWN = 0,
    GAME_ITEM_KIND_GEAR,
    GAME_ITEM_KIND_QUEST_ITEM,
    GAME_ITEM_KIND_CLUE,
} game_item_kind_t;

typedef enum game_item_slot_t {
    GAME_ITEM_SLOT_NONE = 0,
    GAME_ITEM_SLOT_WEAPON,
    GAME_ITEM_SLOT_ARMOUR,
    GAME_ITEM_SLOT_LEGS,
    GAME_ITEM_SLOT_CHARM,
} game_item_slot_t;

typedef struct game_item_definition_t {
    const char *id;
    const char *display_name;
    game_item_kind_t kind;
    game_item_slot_t slot;
    bool stackable;
    int max_stack;
} game_item_definition_t;

const dialogue_definition_t *game_content_find_dialogue(const char *dialogue_id);
const game_item_definition_t *game_content_find_item(const char *item_id);
const char *game_content_next_quest_step(const char *quest_id, const char *step_id);

#endif
