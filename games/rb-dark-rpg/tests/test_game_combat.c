#include "game_actions.h"
#include "game_combat.h"
#include "game_content.h"
#include "game_state.h"

#include <assert.h>
#include <string.h>

static const GameQuestState *find_quest(const GameState *state, const char *quest_id) {
    for (int i = 0; i < GAME_STATE_MAX_QUESTS_QUEST_STATES; ++i) {
        const GameQuestState *quest = &state->quests_quest_states[i];
        if (quest->used && strcmp(quest->key, quest_id) == 0) {
            return quest;
        }
    }
    return 0;
}

static bool has_flag(const GameState *state, const char *flag_id) {
    for (int i = 0; i < state->flags_ids_count; ++i) {
        if (strcmp(state->flags_ids[i], flag_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_reward(const GameState *state, const char *reward_id) {
    for (int i = 0; i < state->quests_claimed_reward_ids_count; ++i) {
        if (strcmp(state->quests_claimed_reward_ids[i], reward_id) == 0) {
            return true;
        }
    }
    return false;
}

static int stack_count(const GameState *state, const char *item_id) {
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_STACK_INSTANCES; ++i) {
        const GameStackInstance *stack = &state->inventory_stack_instances[i];
        if (stack->used && strcmp(stack->key, item_id) == 0) {
            return stack->count;
        }
    }
    return 0;
}

static int gear_count(const GameState *state, const char *def_id) {
    int count = 0;
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
        const GameGearInstance *gear = &state->inventory_gear_instances[i];
        if (gear->used && strcmp(gear->def_id, def_id) == 0) {
            ++count;
        }
    }
    return count;
}

static bool has_gear_instance(const GameState *state, const char *instance_id) {
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
        const GameGearInstance *gear = &state->inventory_gear_instances[i];
        if (gear->used && strcmp(gear->key, instance_id) == 0) {
            return true;
        }
    }
    return false;
}

static bool bag_has_instance(const GameState *state, const char *instance_id) {
    for (int i = 0; i < state->inventory_bag_order_count; ++i) {
        if (strcmp(state->inventory_bag_order[i], instance_id) == 0) {
            return true;
        }
    }
    return false;
}

static float test_absf(float value) { return value < 0.0F ? -value : value; }

static void prepare_shop_unlocked_state(GameState *state, int gold) {
    game_state_init_defaults(state);
    state->wallet_gold = gold;
    assert(game_actions_start_quest(state, "q001_gate_pass", "report_to_gate_guard", "test"));
    assert(game_actions_complete_step(state, "q001_gate_pass", "report_to_gate_guard", 0, "test"));
    assert(game_actions_complete_quest(state, "q001_gate_pass", "test"));
}

static void prepare_gate_check_state(GameState *state) {
    game_state_init_defaults(state);
    assert(game_actions_grant_gear(state, "gear_old_sword_001", "old_sword", GAME_ACTION_GEAR_SLOT_WEAPON));
    assert(game_actions_grant_gear(state, "gear_padded_jacket_001", "padded_jacket", GAME_ACTION_GEAR_SLOT_ARMOUR));
    assert(game_actions_grant_gear(state, "gear_leather_greaves_001", "leather_greaves", GAME_ACTION_GEAR_SLOT_LEGS));
    assert(game_actions_start_quest(state, "q001_gate_pass", "equip_old_sword", "test"));
    assert(game_actions_complete_step(state, "q001_gate_pass", "talk_gate_guard", "equip_old_sword", "test"));
    assert(game_actions_equip_gear(state, "gear_old_sword_001"));
    assert(game_actions_equip_gear(state, "gear_padded_jacket_001"));
    assert(game_actions_equip_gear(state, "gear_leather_greaves_001"));
    const GameQuestState *quest = find_quest(state, "q001_gate_pass");
    assert(quest != 0);
    assert(quest->has_current_step_id);
    assert(strcmp(quest->current_step_id, "clear_gate_scavenger") == 0);
}

static void test_shop_purchase_requires_completed_quest_and_spends_gold(void) {
    GameState state;
    game_state_init_defaults(&state);
    state.wallet_gold = 20;

    assert(!game_actions_purchase_shop_item(&state, "shop_post_trader_basic", "iron_sword"));
    assert(state.wallet_gold == 20);
    assert(gear_count(&state, "iron_sword") == 0);
    assert(state.inventory_bag_order_count == 0);

    prepare_shop_unlocked_state(&state, 20);
    assert(game_actions_purchase_shop_item(&state, "shop_post_trader_basic", "iron_sword"));
    assert(state.wallet_gold == 8);
    assert(gear_count(&state, "iron_sword") == 1);
    assert(state.inventory_bag_order_count == 1);
    assert(strcmp(state.inventory_bag_order[0], "gear_iron_sword_001") == 0);
}

static void test_shop_purchase_rejects_insufficient_gold_without_mutation(void) {
    GameState state;
    prepare_shop_unlocked_state(&state, 4);

    assert(!game_actions_purchase_shop_item(&state, "shop_post_trader_basic", "runner_wraps"));
    assert(state.wallet_gold == 4);
    assert(gear_count(&state, "runner_wraps") == 0);
    assert(state.inventory_bag_order_count == 0);
}

static void test_shop_item_content_exposes_price_and_sellable_contract(void) {
    const game_item_definition_t *shop_item = game_content_find_item("iron_sword");
    const game_item_definition_t *starter_item = game_content_find_item("old_sword");
    const game_item_definition_t *clue_item = game_content_find_item("clue_fragment");
    const game_item_definition_t *quest_item = game_content_find_item("seeker_token");

    assert(shop_item != 0);
    assert(shop_item->price_gold == 12);
    assert(shop_item->sellable);
    assert(starter_item != 0);
    assert(starter_item->price_gold == 0);
    assert(!starter_item->sellable);
    assert(clue_item != 0);
    assert(!clue_item->sellable);
    assert(clue_item->category_label != 0);
    assert(strcmp(clue_item->category_label, "Улика") == 0);
    assert(clue_item->description != 0);
    assert(quest_item != 0);
    assert(!quest_item->sellable);
    assert(quest_item->category_label != 0);
    assert(strcmp(quest_item->category_label, "Квестовый предмет") == 0);
    assert(quest_item->description != 0);
}

static void test_shop_sell_rejects_quest_stack_items(void) {
    GameState state;
    prepare_shop_unlocked_state(&state, 0);
    state.inventory_stack_instances[0].used = true;
    (void)strcpy(state.inventory_stack_instances[0].key, "seeker_token");
    (void)strcpy(state.inventory_stack_instances[0].def_id, "seeker_token");
    state.inventory_stack_instances[0].count = 1;

    game_shop_buyback_t buyback;
    game_actions_shop_buyback_init(&buyback);

    int sell_price = 99;
    assert(!game_actions_can_sell_inventory_item(&state, "seeker_token", &sell_price));
    assert(sell_price == 0);
    assert(!game_actions_sell_inventory_item(&state, "seeker_token", &buyback));
    assert(state.wallet_gold == 0);
    assert(state.inventory_stack_instances[0].used);
    assert(state.inventory_stack_instances[0].count == 1);
    assert(buyback.count == 0);
}

static void test_shop_sell_bag_gear_records_buyback_snapshot(void) {
    GameState state;
    prepare_shop_unlocked_state(&state, 0);
    assert(game_actions_grant_gear(&state, "gear_iron_sword_001", "iron_sword", GAME_ACTION_GEAR_SLOT_WEAPON));

    game_shop_buyback_t buyback;
    game_actions_shop_buyback_init(&buyback);

    int sell_price = 0;
    assert(game_actions_can_sell_inventory_item(&state, "gear_iron_sword_001", &sell_price));
    assert(sell_price == 6);
    assert(game_actions_sell_inventory_item(&state, "gear_iron_sword_001", &buyback));

    assert(state.wallet_gold == 6);
    assert(!has_gear_instance(&state, "gear_iron_sword_001"));
    assert(!bag_has_instance(&state, "gear_iron_sword_001"));
    assert(buyback.count == 1);
    assert(strcmp(buyback.entries[0].entry_id, "gear_iron_sword_001") == 0);
    assert(strcmp(buyback.entries[0].gear.def_id, "iron_sword") == 0);
    assert(buyback.entries[0].price_gold == 6);
}

static void test_shop_sell_rejects_equipped_or_unsellable_items_without_mutation(void) {
    GameState state;
    prepare_shop_unlocked_state(&state, 0);
    assert(game_actions_grant_gear(&state, "gear_iron_sword_001", "iron_sword", GAME_ACTION_GEAR_SLOT_WEAPON));
    assert(game_actions_equip_gear(&state, "gear_iron_sword_001"));
    assert(game_actions_grant_gear(&state, "gear_old_sword_001", "old_sword", GAME_ACTION_GEAR_SLOT_WEAPON));

    game_shop_buyback_t buyback;
    game_actions_shop_buyback_init(&buyback);

    assert(!game_actions_sell_inventory_item(&state, "gear_iron_sword_001", &buyback));
    assert(!game_actions_sell_inventory_item(&state, "gear_old_sword_001", &buyback));
    assert(state.wallet_gold == 0);
    assert(has_gear_instance(&state, "gear_iron_sword_001"));
    assert(has_gear_instance(&state, "gear_old_sword_001"));
    assert(buyback.count == 0);
}

static void test_shop_buyback_restores_original_gear_instance_for_sale_price(void) {
    GameState state;
    prepare_shop_unlocked_state(&state, 0);
    assert(game_actions_grant_gear(&state, "gear_runner_wraps_001", "runner_wraps", GAME_ACTION_GEAR_SLOT_LEGS));
    GameGearInstance *sold = 0;
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
        if (state.inventory_gear_instances[i].used &&
            strcmp(state.inventory_gear_instances[i].key, "gear_runner_wraps_001") == 0) {
            sold = &state.inventory_gear_instances[i];
            break;
        }
    }
    assert(sold != 0);
    sold->durability = 0.42F;
    sold->level = 3;

    game_shop_buyback_t buyback;
    game_actions_shop_buyback_init(&buyback);
    assert(game_actions_sell_inventory_item(&state, "gear_runner_wraps_001", &buyback));
    assert(state.wallet_gold == 2);

    assert(game_actions_rebuy_inventory_item(&state, &buyback, "gear_runner_wraps_001"));
    assert(state.wallet_gold == 0);
    assert(buyback.count == 0);
    assert(has_gear_instance(&state, "gear_runner_wraps_001"));
    assert(bag_has_instance(&state, "gear_runner_wraps_001"));
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
        const GameGearInstance *gear = &state.inventory_gear_instances[i];
        if (gear->used && strcmp(gear->key, "gear_runner_wraps_001") == 0) {
            assert(strcmp(gear->def_id, "runner_wraps") == 0);
            assert(gear->durability > 0.41F && gear->durability < 0.43F);
            assert(gear->level == 3);
            return;
        }
    }
    assert(false);
}

static void prepare_mill_contract_state(GameState *state) {
    prepare_gate_check_state(state);
    assert(game_actions_resolve_encounter(state, "gate_scavenger", &(game_combat_result_t){0}));
    assert(game_actions_complete_step(state, "q001_gate_pass", "report_to_gate_guard", 0, "test"));
    assert(game_actions_set_flag(state, "map_gate_unlocked"));
    assert(game_actions_set_flag(state, "old_mill_unlocked"));
    assert(game_actions_grant_gear(state, "gear_iron_sword_001", "iron_sword", GAME_ACTION_GEAR_SLOT_WEAPON));
    assert(game_actions_grant_gear(state, "gear_patched_mail_001", "patched_mail", GAME_ACTION_GEAR_SLOT_ARMOUR));
    assert(game_actions_grant_gear(state, "gear_iron_greaves_001", "iron_greaves", GAME_ACTION_GEAR_SLOT_LEGS));
    assert(game_actions_equip_gear(state, "gear_iron_sword_001"));
    assert(game_actions_equip_gear(state, "gear_patched_mail_001"));
    assert(game_actions_equip_gear(state, "gear_iron_greaves_001"));
    assert(game_actions_start_quest(state, "q002_bread_for_post", "visit_old_mill", "test"));
    state->hero_hp = 48;
}

static void test_generated_gate_encounter_is_available(void) {
    const game_encounter_definition_t *encounter = game_content_find_encounter("gate_scavenger");
    assert(encounter != 0);
    assert(strcmp(encounter->id, "gate_scavenger") == 0);
    assert(encounter->enemy.vitality == 20);
    assert(encounter->enemy.weapon_damage == 4);
    assert(encounter->reward_xp == 8);
    assert(encounter->reward_gold == 5);
    assert(encounter->reward_item_count == 1);
    assert(strcmp(encounter->reward_items[0], "seeker_token_unlock") == 0);
    const game_item_definition_t *token = game_content_find_item(encounter->reward_items[0]);
    assert(token != 0);
    assert(token->icon_asset_id != 0);
    assert(strcmp(token->icon_asset_id, "asset_icon_seeker_token") == 0);
}

static void test_player_stats_use_equipped_starter_gear(void) {
    GameState state;
    prepare_gate_check_state(&state);

    game_combat_stats_t stats;
    assert(game_combat_build_player_stats(&state, &stats));
    assert(stats.vitality == 33);
    assert(stats.strength == 20);
    assert(stats.protection == 15);
    assert(stats.intuition == 3);
    assert(stats.weapon_damage == 4);
    assert(game_combat_attack_power(&stats) == 6);
}

static void test_gate_scavenger_combat_records_event_timeline(void) {
    GameState state;
    prepare_gate_check_state(&state);
    const game_encounter_definition_t *encounter = game_content_find_encounter("gate_scavenger");
    assert(encounter != 0);

    game_combat_stats_t stats;
    game_combat_result_t result;
    assert(game_combat_build_player_stats(&state, &stats));
    assert(game_combat_simulate(&stats, state.hero_hp, encounter, 12345U, &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_WIN);
    assert(result.event_count > 0);
    assert(result.event_count == result.player_hits + result.enemy_hits);
    assert(result.event_count <= GAME_COMBAT_MAX_EVENTS);

    const game_combat_event_t *first = &result.events[0];
    assert(first->time_seconds > 0.0F);
    assert(first->time_seconds < stats.attack_interval);
    assert(first->actor == GAME_COMBAT_ACTOR_PLAYER || first->actor == GAME_COMBAT_ACTOR_ENEMY);
    assert(first->damage > 0);
    assert(first->player_hp_after >= 0);
    assert(first->enemy_hp_after >= 0);

    for (int i = 1; i < result.event_count; ++i) {
        assert(result.events[i].time_seconds >= result.events[i - 1].time_seconds);
        assert(result.events[i].damage > 0);
        assert(result.events[i].player_hp_after >= 0);
        assert(result.events[i].enemy_hp_after >= 0);
    }

    const game_combat_event_t *last = &result.events[result.event_count - 1];
    assert(last->player_hp_after == result.player_hp);
    assert(last->enemy_hp_after == result.enemy_hp);
    assert(last->enemy_hp_after == 0);
}

static void test_mill_combat_timeline_avoids_same_timestamp_exchange(void) {
    GameState state;
    prepare_mill_contract_state(&state);
    const game_encounter_definition_t *encounter = game_content_find_encounter("mill_scavenger");
    assert(encounter != 0);

    game_combat_stats_t stats;
    game_combat_result_t result;
    assert(game_combat_build_player_stats(&state, &stats));
    assert(game_combat_simulate(&stats, state.hero_hp, encounter, 67890U, &result));
    assert(result.event_count > 2);

    for (int i = 1; i < result.event_count; ++i) {
        const float spacing = test_absf(result.events[i].time_seconds - result.events[i - 1].time_seconds);
        assert(spacing >= 0.25F);
    }
}

static void test_gate_scavenger_win_grants_rewards_once_and_advances_quest(void) {
    GameState state;
    prepare_gate_check_state(&state);

    game_combat_result_t result;
    assert(game_actions_resolve_encounter(&state, "gate_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_WIN);
    assert(result.reward_granted);
    assert(result.player_hp > 0);
    assert(result.enemy_hp == 0);
    assert(result.event_count > 0);

    assert(state.hero_xp == 8);
    assert(state.wallet_gold == 5);
    assert(stack_count(&state, "seeker_token_unlock") == 1);
    assert(has_reward(&state, "encounter.gate_scavenger.win"));
    assert(has_flag(&state, "gate_scavenger_defeated"));

    const GameQuestState *quest = find_quest(&state, "q001_gate_pass");
    assert(quest != 0);
    assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
    assert(quest->has_current_step_id);
    assert(strcmp(quest->current_step_id, "report_to_gate_guard") == 0);

    const int xp_after_first = state.hero_xp;
    const int gold_after_first = state.wallet_gold;
    const int token_after_first = stack_count(&state, "seeker_token_unlock");
    assert(game_actions_resolve_encounter(&state, "gate_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_WIN);
    assert(!result.reward_granted);
    assert(state.hero_xp == xp_after_first);
    assert(state.wallet_gold == gold_after_first);
    assert(stack_count(&state, "seeker_token_unlock") == token_after_first);
}

static void test_zero_hp_cannot_win_or_claim_rewards(void) {
    GameState state;
    prepare_gate_check_state(&state);
    state.hero_hp = 0;

    game_combat_result_t result;
    assert(game_actions_resolve_encounter(&state, "gate_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_LOSS);
    assert(!result.reward_granted);
    assert(result.event_count == 0);
    assert(state.hero_hp == 1);
    assert(state.hero_xp == 0);
    assert(state.wallet_gold == 0);
    assert(stack_count(&state, "seeker_token_unlock") == 0);
    assert(!has_reward(&state, "encounter.gate_scavenger.win"));
    assert(!has_flag(&state, "gate_scavenger_defeated"));

    const GameQuestState *quest = find_quest(&state, "q001_gate_pass");
    assert(quest != 0);
    assert(quest->has_current_step_id);
    assert(strcmp(quest->current_step_id, "clear_gate_scavenger") == 0);
}

static void test_positive_hp_loss_records_fight_but_grants_no_rewards(void) {
    GameState state;
    prepare_gate_check_state(&state);
    state.hero_hp = 1;
    assert(!game_actions_move_location(&state, "hub_gate_outskirts"));

    game_combat_result_t result;
    assert(game_actions_resolve_encounter(&state, "gate_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_LOSS);
    assert(!result.reward_granted);
    assert(result.event_count > 0);
    assert(result.enemy_damage_done > 0);
    assert(result.player_damage_done > 0);
    assert(result.player_hp == 0);
    assert(state.hero_hp == 1);
    assert(state.hero_xp == 0);
    assert(state.wallet_gold == 0);
    assert(stack_count(&state, "seeker_token_unlock") == 0);
    assert(!has_reward(&state, "encounter.gate_scavenger.win"));
    assert(!has_flag(&state, "gate_scavenger_defeated"));
    assert(strcmp(state.world_current_location_id, "hub_last_post") == 0);

    const GameQuestState *quest = find_quest(&state, "q001_gate_pass");
    assert(quest != 0);
    assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
    assert(quest->has_current_step_id);
    assert(strcmp(quest->current_step_id, "clear_gate_scavenger") == 0);
}

static void test_restore_hp_to_equipped_max_without_rewards_or_quest_changes(void) {
    GameState state;
    prepare_gate_check_state(&state);
    state.hero_hp = 1;

    assert(game_actions_restore_hp(&state));
    assert(state.hero_hp == 33);
    assert(state.hero_xp == 0);
    assert(state.wallet_gold == 0);
    assert(stack_count(&state, "seeker_token_unlock") == 0);
    assert(!has_reward(&state, "encounter.gate_scavenger.win"));
    assert(!has_flag(&state, "gate_scavenger_defeated"));

    const GameQuestState *quest = find_quest(&state, "q001_gate_pass");
    assert(quest != 0);
    assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
    assert(quest->has_current_step_id);
    assert(strcmp(quest->current_step_id, "clear_gate_scavenger") == 0);
}

static void test_loss_restore_retry_can_win_and_claim_rewards_once(void) {
    GameState state;
    prepare_gate_check_state(&state);
    state.hero_hp = 1;
    assert(!game_actions_move_location(&state, "hub_gate_outskirts"));

    game_combat_result_t result;
    assert(game_actions_resolve_encounter(&state, "gate_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_LOSS);
    assert(state.hero_hp == 1);
    assert(strcmp(state.world_current_location_id, "hub_last_post") == 0);

    assert(game_actions_restore_hp(&state));
    assert(state.hero_hp == 33);
    assert(!game_actions_move_location(&state, "hub_gate_outskirts"));
    assert(game_actions_resolve_encounter(&state, "gate_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_WIN);
    assert(result.reward_granted);
    assert(state.hero_xp == 8);
    assert(state.wallet_gold == 5);
    assert(stack_count(&state, "seeker_token_unlock") == 1);
    assert(has_reward(&state, "encounter.gate_scavenger.win"));
    assert(has_flag(&state, "gate_scavenger_defeated"));

    const int xp_after_first_win = state.hero_xp;
    const int gold_after_first_win = state.wallet_gold;
    const int token_after_first_win = stack_count(&state, "seeker_token_unlock");
    assert(game_actions_resolve_encounter(&state, "gate_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_WIN);
    assert(!result.reward_granted);
    assert(state.hero_xp == xp_after_first_win);
    assert(state.wallet_gold == gold_after_first_win);
    assert(stack_count(&state, "seeker_token_unlock") == token_after_first_win);
}

static void test_active_untracked_encounter_quest_advances(void) {
    GameState state;
    prepare_gate_check_state(&state);
    state.has_quests_tracked_quest_id = false;
    state.quests_tracked_quest_id[0] = '\0';

    game_combat_result_t result;
    assert(game_actions_resolve_encounter(&state, "gate_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_WIN);
    assert(result.reward_granted);

    const GameQuestState *quest = find_quest(&state, "q001_gate_pass");
    assert(quest != 0);
    assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
    assert(quest->has_current_step_id);
    assert(strcmp(quest->current_step_id, "report_to_gate_guard") == 0);
    assert(has_flag(&state, "gate_scavenger_defeated"));
}

static void test_multi_item_encounter_rewards_grant_once(void) {
    GameState state;
    prepare_mill_contract_state(&state);

    game_combat_result_t result;
    assert(game_actions_move_location(&state, "hub_gate_outskirts"));
    assert(game_actions_move_location(&state, "old_mill"));
    const GameQuestState *visited_quest = find_quest(&state, "q002_bread_for_post");
    assert(visited_quest != 0);
    assert(visited_quest->has_current_step_id);
    assert(strcmp(visited_quest->current_step_id, "inspect_old_mill") == 0);
    assert(game_actions_inspect_object(&state, "old_mill.black_sun_mark"));
    const GameQuestState *inspected_quest = find_quest(&state, "q002_bread_for_post");
    assert(inspected_quest != 0);
    assert(inspected_quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
    assert(inspected_quest->has_current_step_id);
    assert(strcmp(inspected_quest->current_step_id, "report_to_elder") == 0);

    assert(game_actions_resolve_encounter(&state, "mill_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_WIN);
    assert(result.reward_granted);
    assert(state.hero_xp == 18);
    assert(state.wallet_gold == 12);
    assert(stack_count(&state, "contract_progress") == 1);
    assert(gear_count(&state, "scavenger_knee_plates") == 1);
    assert(has_reward(&state, "encounter.mill_scavenger.win"));
    const GameQuestState *quest = find_quest(&state, "q002_bread_for_post");
    assert(quest != 0);
    assert(quest->status == GAME_STATE_QUEST_STATUS_ACTIVE);
    assert(quest->has_current_step_id);
    assert(strcmp(quest->current_step_id, "report_to_elder") == 0);

    const int xp_after_first = state.hero_xp;
    const int gold_after_first = state.wallet_gold;
    const int contract_after_first = stack_count(&state, "contract_progress");
    const int gear_after_first = gear_count(&state, "scavenger_knee_plates");
    assert(game_actions_resolve_encounter(&state, "mill_scavenger", &result));
    assert(result.outcome == GAME_COMBAT_OUTCOME_WIN);
    assert(!result.reward_granted);
    assert(state.hero_xp == xp_after_first);
    assert(state.wallet_gold == gold_after_first);
    assert(stack_count(&state, "contract_progress") == contract_after_first);
    assert(gear_count(&state, "scavenger_knee_plates") == gear_after_first);
}

int main(void) {
    test_shop_item_content_exposes_price_and_sellable_contract();
    test_generated_gate_encounter_is_available();
    test_player_stats_use_equipped_starter_gear();
    test_shop_purchase_requires_completed_quest_and_spends_gold();
    test_shop_purchase_rejects_insufficient_gold_without_mutation();
    test_shop_sell_rejects_quest_stack_items();
    test_shop_sell_bag_gear_records_buyback_snapshot();
    test_shop_sell_rejects_equipped_or_unsellable_items_without_mutation();
    test_shop_buyback_restores_original_gear_instance_for_sale_price();
    test_gate_scavenger_combat_records_event_timeline();
    test_mill_combat_timeline_avoids_same_timestamp_exchange();
    test_gate_scavenger_win_grants_rewards_once_and_advances_quest();
    test_zero_hp_cannot_win_or_claim_rewards();
    test_positive_hp_loss_records_fight_but_grants_no_rewards();
    test_restore_hp_to_equipped_max_without_rewards_or_quest_changes();
    test_loss_restore_retry_can_win_and_claim_rewards_once();
    test_active_untracked_encounter_quest_advances();
    test_multi_item_encounter_rewards_grant_once();
    return 0;
}
