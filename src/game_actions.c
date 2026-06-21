#include "game_actions.h"

#include <stdio.h>
#include <string.h>

#define ROAD_WOLF_XP 10
#define ROAD_WOLF_GOLD 6
#define QUEST_XP 12
#define QUEST_GOLD 18
#define LEVEL_2_XP 20
#define OLD_MINE_SCOUT_XP 4
#define OLD_MINE_SCOUT_SHARDS 3
#define OLD_MINE_DEPTH_XP 6
#define OLD_MINE_DEPTH_GOLD 4
#define OLD_MINE_DEPTH_SHARDS 2
#define OLD_MINE_BAT_DAMAGE 3
#define OLD_MINE_CACHE_XP 3
#define OLD_MINE_CACHE_GOLD 2
#define OLD_MINE_CACHE_SHARDS 1
#define MINE_LANTERN_SHARD_COST 6

static void write_msg(char *message, int cap, const char *text) {
    if (!message || cap <= 0) {
        return;
    }
    (void)snprintf(message, (size_t)cap, "%s", text ? text : "");
    message[cap - 1] = '\0';
}

static int max_int(int a, int b) {
    return a > b ? a : b;
}

static int damage(int attack, int defense) {
    return max_int(1, attack - defense);
}

static void sync_legacy_seed_labels(void) {
    (void)snprintf(g_game_state.test_label_text, sizeof(g_game_state.test_label_text), "Ember Road: %s", game_actions_objective_text());
    (void)snprintf(g_game_state.test_button_text, sizeof(g_game_state.test_button_text), "%s", game_actions_primary_action_label());
}

static void apply_level_if_ready(void) {
    if (g_game_state.hero_level < 2 && g_game_state.hero_xp >= LEVEL_2_XP) {
        g_game_state.hero_level = 2;
        g_game_state.hero_hp_max = 36;
        g_game_state.hero_attack += 1;
        g_game_state.hero_hp = g_game_state.hero_hp_max;
    }
}

static void finish_mutation(void) {
    sync_legacy_seed_labels();
    game_state_mark_dirty();
}

void game_actions_reset_ember_road(void) {
    game_state_init_defaults(&g_game_state);
    g_game_state.location_index = GAME_STATE_LOCATION_OLD_GATE;
    g_game_state.quest_stage_index = GAME_STATE_QUEST_STAGE_NOT_STARTED;
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_NONE;
    g_game_state.battle_enemy_hp = g_game_state.battle_enemy_hp_max;
    g_game_state.inventory_item_ids_count = 0;
    g_game_state.reward_pending = false;
    g_game_state.reward_item_ready = false;
    g_game_state.gear_ring_equipped = false;
    g_game_state.old_mine_scouted = false;
    g_game_state.old_mine_depth = 0;
    g_game_state.old_mine_ember_shards = 0;
    g_game_state.old_mine_depth_resolved = false;
    g_game_state.old_mine_bat_defeated = false;
    g_game_state.old_mine_bat_damage = 0;
    g_game_state.old_mine_depth_gold = 0;
    g_game_state.old_mine_delve_count = 0;
    g_game_state.old_mine_cache_claimed = false;
    g_game_state.old_mine_last_delve_shards = 0;
    g_game_state.old_mine_last_delve_gold = 0;
    g_game_state.old_mine_last_delve_xp = 0;
    g_game_state.gear_mine_lantern = false;
    g_game_state.old_mine_depth2_unlocked = false;
    sync_legacy_seed_labels();
    game_state_mark_dirty();
}

GameActionResult game_actions_accept_quest(char *message, int message_cap) {
    if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_NOT_STARTED) {
        write_msg(message, message_cap, "Quest already active.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.quest_stage_index = GAME_STATE_QUEST_STAGE_ACCEPTED;
    g_game_state.location_index = GAME_STATE_LOCATION_OLD_GATE;
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_NONE;
    write_msg(message, message_cap, "Quest accepted: Wolves at the North Road.");
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_travel_north_road(char *message, int message_cap) {
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_NOT_STARTED) {
        write_msg(message, message_cap, "Accept the town quest before leaving.");
        return GAME_ACTION_BLOCKED;
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED) {
        write_msg(message, message_cap, "Old Mine is the next lock; this slice ends here.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.location_index = GAME_STATE_LOCATION_NORTH_ROAD;
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_READY;
    g_game_state.battle_round = 0;
    g_game_state.battle_enemy_hp = g_game_state.battle_enemy_hp_max;
    g_game_state.battle_last_damage = 0;
    g_game_state.battle_last_enemy_damage = 0;
    write_msg(message, message_cap, "Travelled to North Road. Road Wolf sighted.");
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_auto_battle(char *message, int message_cap) {
    if (g_game_state.location_index != GAME_STATE_LOCATION_NORTH_ROAD) {
        write_msg(message, message_cap, "Travel to North Road first.");
        return GAME_ACTION_BLOCKED;
    }
    if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_ACCEPTED) {
        write_msg(message, message_cap, "No active wolf objective.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_RUNNING;
    g_game_state.battle_round = 0;
    g_game_state.battle_enemy_hp = g_game_state.battle_enemy_hp_max;
    while (g_game_state.hero_hp > 0 && g_game_state.battle_enemy_hp > 0 && g_game_state.battle_round < 20) {
        g_game_state.battle_round += 1;
        g_game_state.battle_last_damage = damage(g_game_state.hero_attack, g_game_state.battle_enemy_defense);
        g_game_state.battle_enemy_hp = max_int(0, g_game_state.battle_enemy_hp - g_game_state.battle_last_damage);
        if (g_game_state.battle_enemy_hp > 0) {
            g_game_state.battle_last_enemy_damage = damage(g_game_state.battle_enemy_attack, g_game_state.hero_defense);
            g_game_state.hero_hp = max_int(0, g_game_state.hero_hp - g_game_state.battle_last_enemy_damage);
        } else {
            g_game_state.battle_last_enemy_damage = 0;
        }
    }
    if (g_game_state.hero_hp <= 0) {
        g_game_state.hero_hp = 1;
        g_game_state.location_index = GAME_STATE_LOCATION_OLD_GATE;
        g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_DEFEAT;
        write_msg(message, message_cap, "Defeat. The hero retreats to Old Gate at 1 HP.");
    } else {
        g_game_state.battle_wins += 1;
        g_game_state.quest_wolf_kills = 1;
        g_game_state.hero_xp += ROAD_WOLF_XP;
        g_game_state.hero_gold += ROAD_WOLF_GOLD;
        g_game_state.reward_pending = true;
        g_game_state.reward_item_ready = true;
        g_game_state.quest_stage_index = GAME_STATE_QUEST_STAGE_WOLF_DEFEATED;
        g_game_state.battle_state_index = g_game_state.hero_hp <= 8 ? GAME_STATE_BATTLE_STATE_LOW_HEALTH : GAME_STATE_BATTLE_STATE_VICTORY;
        apply_level_if_ready();
        write_msg(message, message_cap, "Victory. Loot found: Rusty Iron Ring.");
    }
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_equip_ring(char *message, int message_cap) {
    if (!g_game_state.reward_item_ready) {
        write_msg(message, message_cap, "No ring reward is ready.");
        return GAME_ACTION_BLOCKED;
    }
    if (g_game_state.gear_ring_equipped) {
        write_msg(message, message_cap, "Rusty Iron Ring is already equipped.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.gear_ring_equipped = true;
    g_game_state.hero_attack += 1;
    write_msg(message, message_cap, "Equipped Rusty Iron Ring: attack +1.");
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_claim_reward(char *message, int message_cap) {
    if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_WOLF_DEFEATED) {
        write_msg(message, message_cap, "Defeat the Road Wolf before claiming the quest.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.location_index = GAME_STATE_LOCATION_OLD_GATE;
    g_game_state.hero_xp += QUEST_XP;
    g_game_state.hero_gold += QUEST_GOLD;
    g_game_state.reward_pending = false;
    g_game_state.quest_stage_index = GAME_STATE_QUEST_STAGE_COMPLETED;
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_NONE;
    apply_level_if_ready();
    write_msg(message, message_cap, "Quest complete. Old Mine lock is now visible.");
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_enter_old_mine(char *message, int message_cap) {
    if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED || g_game_state.hero_level < 2) {
        write_msg(message, message_cap, "Complete the wolf quest and reach level 2 first.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.location_index = GAME_STATE_LOCATION_OLD_MINE;
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_NONE;
    write_msg(message, message_cap, "Old Mine entrance reached. Choose scout or return.");
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_scout_old_mine(char *message, int message_cap) {
    if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED ||
        g_game_state.location_index != GAME_STATE_LOCATION_OLD_MINE ||
        g_game_state.hero_level < 2) {
        write_msg(message, message_cap, "Reach the Old Mine entrance first.");
        return GAME_ACTION_BLOCKED;
    }
    if (g_game_state.old_mine_scouted) {
        write_msg(message, message_cap, "Scout report already complete.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.old_mine_scouted = true;
    g_game_state.old_mine_depth = 1;
    g_game_state.old_mine_ember_shards += OLD_MINE_SCOUT_SHARDS;
    g_game_state.hero_xp += OLD_MINE_SCOUT_XP;
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_NONE;
    write_msg(message, message_cap, "Scout complete: Cave Bat signs and ember shards found.");
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_resolve_old_mine_depth(char *message, int message_cap) {
    if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED ||
        g_game_state.location_index != GAME_STATE_LOCATION_OLD_MINE ||
        !g_game_state.old_mine_scouted) {
        write_msg(message, message_cap, "Scout the Old Mine entrance first.");
        return GAME_ACTION_BLOCKED;
    }
    if (g_game_state.old_mine_depth_resolved) {
        write_msg(message, message_cap, "Depth 1 is already clear.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.old_mine_depth = 1;
    g_game_state.old_mine_depth_resolved = true;
    g_game_state.old_mine_bat_defeated = true;
    g_game_state.old_mine_bat_damage = OLD_MINE_BAT_DAMAGE;
    g_game_state.old_mine_depth_gold = OLD_MINE_DEPTH_GOLD;
    g_game_state.old_mine_ember_shards += OLD_MINE_DEPTH_SHARDS;
    g_game_state.hero_xp += OLD_MINE_DEPTH_XP;
    g_game_state.hero_gold += OLD_MINE_DEPTH_GOLD;
    g_game_state.hero_hp = max_int(1, g_game_state.hero_hp - OLD_MINE_BAT_DAMAGE);
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_VICTORY;
    g_game_state.battle_round = 2;
    g_game_state.battle_enemy_hp_max = 12;
    g_game_state.battle_enemy_hp = 0;
    g_game_state.battle_enemy_attack = 3;
    g_game_state.battle_last_damage = g_game_state.hero_attack;
    g_game_state.battle_last_enemy_damage = OLD_MINE_BAT_DAMAGE;
    g_game_state.battle_wins += 1;
    apply_level_if_ready();
    write_msg(message, message_cap, "Depth 1 cleared. Cave Bat defeated; ember cache found.");
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_delve_old_mine(char *message, int message_cap) {
    if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED ||
        g_game_state.location_index != GAME_STATE_LOCATION_OLD_MINE ||
        !g_game_state.old_mine_depth_resolved) {
        write_msg(message, message_cap, "Clear Depth 1 before delving the cache.");
        return GAME_ACTION_BLOCKED;
    }
    if (g_game_state.old_mine_cache_claimed) {
        write_msg(message, message_cap, "The first ember cache is already recovered.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.old_mine_delve_count += 1;
    g_game_state.old_mine_cache_claimed = true;
    g_game_state.old_mine_last_delve_shards = OLD_MINE_CACHE_SHARDS;
    g_game_state.old_mine_last_delve_gold = OLD_MINE_CACHE_GOLD;
    g_game_state.old_mine_last_delve_xp = OLD_MINE_CACHE_XP;
    g_game_state.old_mine_ember_shards += OLD_MINE_CACHE_SHARDS;
    g_game_state.hero_gold += OLD_MINE_CACHE_GOLD;
    g_game_state.hero_xp += OLD_MINE_CACHE_XP;
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_NONE;
    apply_level_if_ready();
    write_msg(message, message_cap, "Delve complete: first ember cache recovered.");
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_return_old_gate(char *message, int message_cap) {
    if (g_game_state.location_index != GAME_STATE_LOCATION_OLD_MINE) {
        write_msg(message, message_cap, "Old Gate is already the current route hub.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.location_index = GAME_STATE_LOCATION_OLD_GATE;
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_NONE;
    write_msg(message, message_cap, "Returned to Old Gate.");
    finish_mutation();
    return GAME_ACTION_OK;
}

GameActionResult game_actions_forge_mine_lantern(char *message, int message_cap) {
    if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED ||
        g_game_state.location_index != GAME_STATE_LOCATION_OLD_GATE ||
        !g_game_state.old_mine_cache_claimed) {
        write_msg(message, message_cap, "Return to Old Gate with the first ember cache.");
        return GAME_ACTION_BLOCKED;
    }
    if (g_game_state.gear_mine_lantern) {
        write_msg(message, message_cap, "Mine Lantern is already forged.");
        return GAME_ACTION_BLOCKED;
    }
    if (g_game_state.old_mine_ember_shards < MINE_LANTERN_SHARD_COST) {
        write_msg(message, message_cap, "Need 6 ember shards to forge the Mine Lantern.");
        return GAME_ACTION_BLOCKED;
    }
    g_game_state.old_mine_ember_shards -= MINE_LANTERN_SHARD_COST;
    g_game_state.gear_mine_lantern = true;
    g_game_state.old_mine_depth2_unlocked = true;
    g_game_state.battle_state_index = GAME_STATE_BATTLE_STATE_NONE;
    write_msg(message, message_cap, "Mine Lantern forged. Depth 2 route is now lit.");
    finish_mutation();
    return GAME_ACTION_OK;
}

const char *game_actions_primary_action_id(void) {
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_NOT_STARTED) {
        return "ember.accept_quest";
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_ACCEPTED && g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE) {
        return "ember.travel_north_road";
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_ACCEPTED && g_game_state.location_index == GAME_STATE_LOCATION_NORTH_ROAD) {
        return "ember.auto_battle";
    }
    if (g_game_state.reward_item_ready && !g_game_state.gear_ring_equipped && g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED) {
        return "ember.equip_ring";
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_WOLF_DEFEATED) {
        return "ember.claim_reward";
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED && g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE) {
        if (!g_game_state.old_mine_scouted) return "ember.scout_old_mine";
        if (!g_game_state.old_mine_depth_resolved) return "ember.resolve_old_mine_depth";
        if (!g_game_state.old_mine_cache_claimed) return "ember.delve_old_mine";
        return "ember.return_old_gate";
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
        g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE &&
        g_game_state.old_mine_cache_claimed &&
        !g_game_state.gear_mine_lantern) {
        return "ember.forge_mine_lantern";
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED) {
        return "ember.enter_old_mine";
    }
    return "ember.completed";
}

const char *game_actions_primary_action_label(void) {
    const char *id = game_actions_primary_action_id();
    if (strcmp(id, "ember.accept_quest") == 0) return "Accept quest";
    if (strcmp(id, "ember.travel_north_road") == 0) return "Travel to North Road";
    if (strcmp(id, "ember.auto_battle") == 0) return "Start auto battle";
    if (strcmp(id, "ember.equip_ring") == 0) return "Equip ring";
    if (strcmp(id, "ember.claim_reward") == 0) return "Claim reward";
    if (strcmp(id, "ember.enter_old_mine") == 0) return "Enter Old Mine";
    if (strcmp(id, "ember.scout_old_mine") == 0) return "Scout entrance";
    if (strcmp(id, "ember.resolve_old_mine_depth") == 0) return "Clear depth 1";
    if (strcmp(id, "ember.delve_old_mine") == 0) return "Delve cache";
    if (strcmp(id, "ember.return_old_gate") == 0) return "Back to Old Gate";
    if (strcmp(id, "ember.forge_mine_lantern") == 0) return "Forge lantern";
    return "Old Mine next";
}

const char *game_actions_objective_text(void) {
    switch (g_game_state.quest_stage_index) {
    case GAME_STATE_QUEST_STAGE_NOT_STARTED:
        return "Talk to the Gate Warden and take the wolf quest.";
    case GAME_STATE_QUEST_STAGE_ACCEPTED:
        return g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE ? "Travel to North Road." : "Defeat the Road Wolf.";
    case GAME_STATE_QUEST_STAGE_WOLF_DEFEATED:
        return g_game_state.gear_ring_equipped ? "Return to town and claim the reward." : "Equip or keep the Rusty Iron Ring.";
    case GAME_STATE_QUEST_STAGE_COMPLETED:
        if (g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE) {
            if (g_game_state.old_mine_cache_claimed) return "Cache recovered. Return to town with the Old Mine proof.";
            if (g_game_state.old_mine_depth_resolved) return "Depth 1 is clear. Delve the ember cache or return.";
            return g_game_state.old_mine_scouted ? "Clear the Cave Bat at depth 1." : "Scout the Old Mine entrance.";
        }
        if (g_game_state.old_mine_cache_claimed && !g_game_state.gear_mine_lantern) {
            return "Forge a Mine Lantern from ember shards.";
        }
        if (g_game_state.gear_mine_lantern) {
            return "Mine Lantern ready. Depth 2 route is lit.";
        }
        return "Enter the Old Mine route from the map.";
    default:
        return "Choose the next quest step.";
    }
}

const char *game_actions_location_title(void) {
    switch (g_game_state.location_index) {
    case GAME_STATE_LOCATION_NORTH_ROAD:
        return "North Road";
    case GAME_STATE_LOCATION_OLD_MINE:
        return "Old Mine Entrance";
    case GAME_STATE_LOCATION_OLD_GATE:
    default:
        return "Old Gate Town Square";
    }
}

const char *game_actions_battle_text(void) {
    switch (g_game_state.battle_state_index) {
    case GAME_STATE_BATTLE_STATE_READY:
        return "Road Wolf is blocking the path.";
    case GAME_STATE_BATTLE_STATE_RUNNING:
        return "Auto battle resolving...";
    case GAME_STATE_BATTLE_STATE_VICTORY:
        return "Victory: loot and XP gained.";
    case GAME_STATE_BATTLE_STATE_LOW_HEALTH:
        return "Victory, but HP is low.";
    case GAME_STATE_BATTLE_STATE_DEFEAT:
        return "Defeat: retreated to town.";
    case GAME_STATE_BATTLE_STATE_NONE:
    default:
        if (g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE) {
            if (g_game_state.old_mine_cache_claimed) return "First ember cache recovered; next depth remains locked.";
            if (g_game_state.old_mine_depth_resolved) return "Depth 1 clear: delve the visible ember cache next.";
            return g_game_state.old_mine_scouted ? "Cave Bat signs ahead. Depth 1 is now mapped." : "Old Mine awaits: scout for threat and ember shards.";
        }
        if (g_game_state.gear_mine_lantern) return "Lantern ready: Depth 2 route is unlocked for the next slice.";
        if (g_game_state.old_mine_cache_claimed) return "Town forge: spend 6 ember shards for a Mine Lantern.";
        return "No battle active.";
    }
}
