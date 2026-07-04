#include "game_actions.h"

#include "game_content.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static bool copy_id(char dst[GAME_STATE_STRING_MAX], const char *src) {
    if (!dst || !src || src[0] == '\0' || strlen(src) >= GAME_STATE_STRING_MAX) {
        return false;
    }
    (void)strcpy(dst, src);
    return true;
}

static bool list_contains(char list[][GAME_STATE_STRING_MAX], int count, const char *id) {
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

static bool list_add_unique(char list[][GAME_STATE_STRING_MAX], int *count, int max_count, const char *id) {
    if (!count || !id || id[0] == '\0' || strlen(id) >= GAME_STATE_STRING_MAX) {
        return false;
    }
    if (list_contains(list, *count, id)) {
        return true;
    }
    if (*count >= max_count) {
        return false;
    }
    (void)strcpy(list[*count], id);
    *count += 1;
    return true;
}

static GameGearInstance *find_gear(GameState *state, const char *instance_id) {
    if (!state || !instance_id) {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
        GameGearInstance *gear = &state->inventory_gear_instances[i];
        if (gear->used && strcmp(gear->key, instance_id) == 0) {
            return gear;
        }
    }
    return NULL;
}

static GameGearInstance *alloc_gear(GameState *state) {
    if (!state) {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
        if (!state->inventory_gear_instances[i].used) {
            return &state->inventory_gear_instances[i];
        }
    }
    return NULL;
}

static GameStackInstance *find_stack(GameState *state, const char *instance_id) {
    if (!state || !instance_id) {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_STACK_INSTANCES; ++i) {
        GameStackInstance *stack = &state->inventory_stack_instances[i];
        if (stack->used && strcmp(stack->key, instance_id) == 0) {
            return stack;
        }
    }
    return NULL;
}

static GameStackInstance *alloc_stack(GameState *state) {
    if (!state) {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_STACK_INSTANCES; ++i) {
        if (!state->inventory_stack_instances[i].used) {
            return &state->inventory_stack_instances[i];
        }
    }
    return NULL;
}

static GameQuestState *find_quest(GameState *state, const char *quest_id) {
    if (!state || !quest_id) {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
        GameQuestState *quest = &state->quests_quest_states[i];
        if (quest->used && strcmp(quest->key, quest_id) == 0) {
            return quest;
        }
    }
    return NULL;
}

static GameQuestState *find_or_alloc_quest(GameState *state, const char *quest_id) {
    GameQuestState *quest = find_quest(state, quest_id);
    if (quest) {
        return quest;
    }
    if (!state || !quest_id || strlen(quest_id) >= GAME_STATE_STRING_MAX) {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
        quest = &state->quests_quest_states[i];
        if (!quest->used) {
            *quest = (GameQuestState){0};
            quest->used = true;
            (void)strcpy(quest->key, quest_id);
            quest->status = GAME_STATE_QUEST_STATUS_HIDDEN;
            return quest;
        }
    }
    return NULL;
}

static bool set_slot(GameState *state, game_action_gear_slot_t slot, const char *instance_id) {
    switch (slot) {
        case GAME_ACTION_GEAR_SLOT_WEAPON:
            state->has_equipment_weapon_instance_id = true;
            return copy_id(state->equipment_weapon_instance_id, instance_id);
        case GAME_ACTION_GEAR_SLOT_ARMOUR:
            state->has_equipment_armour_instance_id = true;
            return copy_id(state->equipment_armour_instance_id, instance_id);
        case GAME_ACTION_GEAR_SLOT_LEGS:
            state->has_equipment_legs_instance_id = true;
            return copy_id(state->equipment_legs_instance_id, instance_id);
        case GAME_ACTION_GEAR_SLOT_CHARM:
            state->has_equipment_charm_instance_id = true;
            return copy_id(state->equipment_charm_instance_id, instance_id);
        case GAME_ACTION_GEAR_SLOT_NONE:
        default:
            return true;
    }
}

bool game_actions_grant_gear(GameState *state, const char *instance_id, const char *def_id, game_action_gear_slot_t preferred_slot) {
    if (!state || !instance_id || !def_id) {
        return false;
    }
    GameGearInstance *gear = find_gear(state, instance_id);
    if (!gear) {
        gear = alloc_gear(state);
        if (!gear) {
            return false;
        }
        *gear = (GameGearInstance){0};
        gear->used = true;
        if (!copy_id(gear->key, instance_id) || !copy_id(gear->def_id, def_id)) {
            *gear = (GameGearInstance){0};
            return false;
        }
        gear->durability = GAME_STATE_GEAR_INSTANCE_DURABILITY_DEFAULT;
        gear->level = GAME_STATE_GEAR_INSTANCE_LEVEL_DEFAULT;
        gear->bind_state = GAME_STATE_BIND_STATE_NONE;
    }
    return set_slot(state, preferred_slot, instance_id);
}

static game_action_gear_slot_t gear_slot_from_item(const game_item_definition_t *item) {
    if (!item) {
        return GAME_ACTION_GEAR_SLOT_NONE;
    }
    switch (item->slot) {
        case GAME_ITEM_SLOT_WEAPON:
            return GAME_ACTION_GEAR_SLOT_WEAPON;
        case GAME_ITEM_SLOT_ARMOUR:
            return GAME_ACTION_GEAR_SLOT_ARMOUR;
        case GAME_ITEM_SLOT_LEGS:
            return GAME_ACTION_GEAR_SLOT_LEGS;
        case GAME_ITEM_SLOT_CHARM:
            return GAME_ACTION_GEAR_SLOT_CHARM;
        case GAME_ITEM_SLOT_NONE:
        default:
            return GAME_ACTION_GEAR_SLOT_NONE;
    }
}

static bool grant_stack_item(GameState *state, const char *item_id, int count) {
    if (!state || !item_id || count <= 0) {
        return false;
    }
    GameStackInstance *stack = find_stack(state, item_id);
    if (!stack) {
        stack = alloc_stack(state);
        if (!stack) {
            return false;
        }
        *stack = (GameStackInstance){0};
        stack->used = true;
        if (!copy_id(stack->key, item_id) || !copy_id(stack->def_id, item_id)) {
            *stack = (GameStackInstance){0};
            return false;
        }
    }
    if (stack->count > 999999 - count) {
        return false;
    }
    stack->count += count;
    return true;
}

static bool grant_gear_item(GameState *state, const game_item_definition_t *item, int count) {
    if (!state || !item || !item->id || count <= 0) {
        return false;
    }
    const game_action_gear_slot_t slot = gear_slot_from_item(item);
    if (slot == GAME_ACTION_GEAR_SLOT_NONE) {
        return false;
    }
    for (int n = 0; n < count; ++n) {
        char instance_id[GAME_STATE_STRING_MAX];
        bool granted = false;
        for (int index = 1; index <= GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++index) {
            if (snprintf(instance_id, sizeof instance_id, "gear_%s_%03d", item->id, index) >= (int)sizeof instance_id) {
                return false;
            }
            if (find_gear(state, instance_id)) {
                continue;
            }
            if (!game_actions_grant_gear(state, instance_id, item->id, slot)) {
                return false;
            }
            granted = true;
            break;
        }
        if (!granted) {
            return false;
        }
    }
    return true;
}

static bool game_actions_grant_item(GameState *state, const char *item_id, int count) {
    const game_item_definition_t *item = game_content_find_item(item_id);
    if (!state || !item || count <= 0) {
        return false;
    }
    if (item->kind == GAME_ITEM_KIND_GEAR) {
        return grant_gear_item(state, item, count);
    }
    if (item->stackable || item->kind == GAME_ITEM_KIND_QUEST_ITEM || item->kind == GAME_ITEM_KIND_CLUE) {
        return grant_stack_item(state, item->id, count);
    }
    return false;
}

bool game_actions_start_quest(GameState *state, const char *quest_id, const char *current_step_id, const char *reason) {
    GameQuestState *quest = find_or_alloc_quest(state, quest_id);
    if (!quest || !current_step_id) {
        return false;
    }
    quest->status = GAME_STATE_QUEST_STATUS_ACTIVE;
    quest->has_current_step_id = true;
    if (!copy_id(quest->current_step_id, current_step_id)) {
        return false;
    }
    quest->objective_progress = 0;
    quest->has_last_update_reason = reason && reason[0] != '\0';
    if (quest->has_last_update_reason && !copy_id(quest->last_update_reason, reason)) {
        return false;
    }
    state->has_quests_tracked_quest_id = true;
    return copy_id(state->quests_tracked_quest_id, quest_id);
}

bool game_actions_complete_step(GameState *state, const char *quest_id, const char *step_id, const char *next_step_id, const char *reason) {
    if (!state || !step_id) {
        return false;
    }
    GameQuestState *quest = find_or_alloc_quest(state, quest_id);
    if (!quest) {
        return false;
    }
    if (!list_add_unique(state->quests_completed_step_ids, &state->quests_completed_step_ids_count,
                         GAME_STATE_MAX_QUESTS_COMPLETED_STEP_IDS, step_id)) {
        return false;
    }
    quest->status = next_step_id ? GAME_STATE_QUEST_STATUS_ACTIVE : GAME_STATE_QUEST_STATUS_READY_TO_TURN_IN;
    quest->has_current_step_id = next_step_id && next_step_id[0] != '\0';
    if (quest->has_current_step_id && !copy_id(quest->current_step_id, next_step_id)) {
        return false;
    }
    quest->has_last_update_reason = reason && reason[0] != '\0';
    if (quest->has_last_update_reason && !copy_id(quest->last_update_reason, reason)) {
        return false;
    }
    return true;
}

bool game_actions_claim_reward_once(GameState *state, const char *reward_id) {
    return state && list_add_unique(state->quests_claimed_reward_ids, &state->quests_claimed_reward_ids_count,
                                    GAME_STATE_MAX_QUESTS_CLAIMED_REWARD_IDS, reward_id);
}

bool game_actions_record_choice(GameState *state, const char *choice_id) {
    return state && list_add_unique(state->quests_choice_ids, &state->quests_choice_ids_count, GAME_STATE_MAX_QUESTS_CHOICE_IDS, choice_id);
}

bool game_actions_set_flag(GameState *state, const char *flag_id) {
    return state && list_add_unique(state->flags_ids, &state->flags_ids_count, GAME_STATE_MAX_FLAGS_IDS, flag_id);
}

bool game_actions_apply_dialogue_choice(GameState *state, const char *dialogue_id, const char *choice_id, const char *authored_reward_id,
                                        const dialogue_effect_t *effects, int effect_count) {
    if (!state || !dialogue_id || !choice_id || effect_count < 0 || (effect_count > 0 && !effects)) {
        return false;
    }
    GameState next = *state;
    const bool live_state = state == &g_game_state;
    char choice_record_id[GAME_STATE_STRING_MAX];
    char fallback_reward_id[GAME_STATE_STRING_MAX];
    if (snprintf(choice_record_id, sizeof choice_record_id, "%s.%s", dialogue_id, choice_id) >= (int)sizeof choice_record_id) {
        return false;
    }
    bool has_grant_effect = false;
    for (int i = 0; i < effect_count; ++i) {
        if (effects[i].kind == DIALOGUE_EFFECT_GRANT_ITEM) {
            has_grant_effect = true;
            break;
        }
    }
    const char *reward_id = authored_reward_id;
    if ((!reward_id || reward_id[0] == '\0') && has_grant_effect) {
        if (snprintf(fallback_reward_id, sizeof fallback_reward_id, "%s.immediate", choice_record_id) >= (int)sizeof fallback_reward_id) {
            return false;
        }
        reward_id = fallback_reward_id;
    }
    const bool already_claimed =
        reward_id && list_contains(next.quests_claimed_reward_ids, next.quests_claimed_reward_ids_count, reward_id);
    if (!game_actions_record_choice(&next, choice_record_id)) {
        return false;
    }
    for (int i = 0; i < effect_count; ++i) {
        const dialogue_effect_t *effect = &effects[i];
        if (effect->kind == DIALOGUE_EFFECT_GRANT_ITEM) {
            if (!already_claimed && !game_actions_grant_item(&next, effect->item_id, effect->count)) {
                return false;
            }
            continue;
        }
        if (effect->kind == DIALOGUE_EFFECT_SET_FLAG) {
            if (!game_actions_set_flag(&next, effect->flag_id)) {
                return false;
            }
            continue;
        }
        if (effect->kind == DIALOGUE_EFFECT_ADVANCE_QUEST) {
            if (!effect->quest_id || !effect->step_id) {
                return false;
            }
            if (!list_contains(next.quests_completed_step_ids, next.quests_completed_step_ids_count, effect->step_id)) {
                const char *next_step_id = game_content_next_quest_step(effect->quest_id, effect->step_id);
                if (!game_actions_start_quest(&next, effect->quest_id, next_step_id ? next_step_id : effect->step_id, choice_record_id) ||
                    !game_actions_complete_step(&next, effect->quest_id, effect->step_id, next_step_id, choice_record_id)) {
                    return false;
                }
            }
            continue;
        }
        return false;
    }
    if (has_grant_effect && (!reward_id || reward_id[0] == '\0')) {
        return false;
    }
    if (has_grant_effect && !already_claimed && !game_actions_claim_reward_once(&next, reward_id)) {
        return false;
    }
    *state = next;
    if (live_state) {
        game_state_mark_dirty();
    }
    return true;
}
