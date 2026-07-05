#include "game_combat.h"

#include <stddef.h>
#include <string.h>

#define GAME_COMBAT_DEFAULT_ATTACK_INTERVAL 2.1F
#define GAME_COMBAT_PLAYER_OPENING_FACTOR 0.36F
#define GAME_COMBAT_ENEMY_OPENING_FACTOR 0.62F
#define GAME_COMBAT_MIN_EVENT_SPACING 0.32F

static float combat_attack_interval(const game_combat_stats_t *stats) {
    return stats && stats->attack_interval > 0.0F ? stats->attack_interval : GAME_COMBAT_DEFAULT_ATTACK_INTERVAL;
}

static float combat_spaced_event_time(float scheduled_time, float *last_event_time) {
    if (!last_event_time) {
        return scheduled_time;
    }
    const float earliest = *last_event_time + GAME_COMBAT_MIN_EVENT_SPACING;
    const float resolved_time = scheduled_time < earliest ? earliest : scheduled_time;
    *last_event_time = resolved_time;
    return resolved_time;
}

static const GameGearInstance *find_gear(const GameState *state, const char *instance_id) {
    if (!state || !instance_id || instance_id[0] == '\0') {
        return NULL;
    }
    for (int i = 0; i < GAME_STATE_MAX_INVENTORY_GEAR_INSTANCES; ++i) {
        const GameGearInstance *gear = &state->inventory_gear_instances[i];
        if (gear->used && strcmp(gear->key, instance_id) == 0) {
            return gear;
        }
    }
    return NULL;
}

static void add_stats(game_combat_stats_t *dst, const game_combat_stats_t *src) {
    if (!dst || !src) {
        return;
    }
    dst->vitality += src->vitality;
    dst->strength += src->strength;
    dst->protection += src->protection;
    dst->intuition += src->intuition;
    dst->weapon_damage += src->weapon_damage;
    dst->bonus_attack_power += src->bonus_attack_power;
    if (src->attack_interval > 0.0F && (dst->attack_interval <= 0.0F || src->attack_interval < dst->attack_interval)) {
        dst->attack_interval = src->attack_interval;
    }
}

static void add_equipped_item(GameState const *state, game_combat_stats_t *stats, const char *instance_id) {
    const GameGearInstance *gear = find_gear(state, instance_id);
    const game_item_definition_t *item = gear ? game_content_find_item(gear->def_id) : NULL;
    if (item) {
        add_stats(stats, &item->stats);
    }
}

int game_combat_attack_power(const game_combat_stats_t *stats) {
    if (!stats) {
        return 0;
    }
    return stats->weapon_damage + stats->strength / 10 + stats->bonus_attack_power;
}

bool game_combat_build_player_stats(const GameState *state, game_combat_stats_t *out_stats) {
    const game_combat_stats_t *base = game_content_base_player_stats();
    if (!state || !out_stats || !base) {
        return false;
    }
    *out_stats = *base;
    if (state->has_equipment_weapon_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_weapon_instance_id);
    }
    if (state->has_equipment_offhand_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_offhand_instance_id);
    }
    if (state->has_equipment_head_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_head_instance_id);
    }
    if (state->has_equipment_armour_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_armour_instance_id);
    }
    if (state->has_equipment_hands_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_hands_instance_id);
    }
    if (state->has_equipment_waist_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_waist_instance_id);
    }
    if (state->has_equipment_legs_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_legs_instance_id);
    }
    if (state->has_equipment_feet_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_feet_instance_id);
    }
    if (state->has_equipment_neck_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_neck_instance_id);
    }
    if (state->has_equipment_ring_left_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_ring_left_instance_id);
    }
    if (state->has_equipment_ring_right_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_ring_right_instance_id);
    }
    if (state->has_equipment_charm_instance_id) {
        add_equipped_item(state, out_stats, state->equipment_charm_instance_id);
    }
    out_stats->attack_interval = combat_attack_interval(out_stats);
    return true;
}

static unsigned int next_random(unsigned int *seed) {
    *seed = (*seed * 1664525U) + 1013904223U;
    return *seed;
}

static bool roll_chance(unsigned int *seed, int stat, int cap_percent) {
    if (stat <= 0 || cap_percent <= 0) {
        return false;
    }
    int chance_percent = stat;
    if (chance_percent > cap_percent) {
        chance_percent = cap_percent;
    }
    const unsigned int roll = (next_random(seed) >> 8U) % 100U;
    return roll < (unsigned int)chance_percent;
}

static int resolve_hit(const game_combat_stats_t *attacker, const game_combat_stats_t *defender,
                       unsigned int *seed, bool *out_crit, bool *out_block) {
    if (out_crit) {
        *out_crit = false;
    }
    if (out_block) {
        *out_block = false;
    }

    const int attack = game_combat_attack_power(attacker);
    if (attack <= 0) {
        return 1;
    }
    if (roll_chance(seed, attacker->intuition, 30)) {
        if (out_crit) {
            *out_crit = true;
        }
        return attack * 2;
    }
    if (roll_chance(seed, defender->protection, 35)) {
        if (out_block) {
            *out_block = true;
        }
        return (attack + 1) / 2;
    }
    return attack;
}

static int clamped_hp(int hp) { return hp > 0 ? hp : 0; }

static void append_event(game_combat_result_t *result, float time_seconds, game_combat_actor_t actor, int damage, int player_hp,
                         int enemy_hp, bool crit, bool block) {
    if (!result || result->event_count >= GAME_COMBAT_MAX_EVENTS) {
        return;
    }
    game_combat_event_t *event = &result->events[result->event_count++];
    event->time_seconds = time_seconds;
    event->actor = actor;
    event->damage = damage;
    event->player_hp_after = clamped_hp(player_hp);
    event->enemy_hp_after = clamped_hp(enemy_hp);
    event->crit = crit;
    event->block = block;
}

bool game_combat_simulate(const game_combat_stats_t *player_stats, int player_current_hp,
                          const game_encounter_definition_t *encounter, unsigned int seed,
                          game_combat_result_t *out_result) {
    if (!player_stats || !encounter || !out_result) {
        return false;
    }
    game_combat_result_t result = {0};
    const game_combat_stats_t *enemy_stats = &encounter->enemy;
    if (player_current_hp <= 0) {
        result.outcome = GAME_COMBAT_OUTCOME_LOSS;
        result.player_hp = 0;
        result.enemy_hp = enemy_stats->vitality;
        result.duration_seconds = 0.0F;
        *out_result = result;
        return true;
    }
    int player_hp = player_current_hp;
    if (player_hp > player_stats->vitality) {
        player_hp = player_stats->vitality;
    }
    int enemy_hp = enemy_stats->vitality;
    const float player_interval = combat_attack_interval(player_stats);
    const float enemy_interval = combat_attack_interval(enemy_stats);
    float player_next = player_interval * GAME_COMBAT_PLAYER_OPENING_FACTOR;
    float enemy_next = enemy_interval * GAME_COMBAT_ENEMY_OPENING_FACTOR;
    float now = 0.0F;
    float last_event_time = -GAME_COMBAT_MIN_EVENT_SPACING;

    for (int guard = 0; guard < 120 && player_hp > 0 && enemy_hp > 0; ++guard) {
        const bool player_first = player_next <= enemy_next;
        now = combat_spaced_event_time(player_first ? player_next : enemy_next, &last_event_time);
        if (player_first) {
            bool crit = false;
            bool block = false;
            const int damage = resolve_hit(player_stats, enemy_stats, &seed, &crit, &block);
            enemy_hp -= damage;
            result.player_damage_done += damage;
            result.player_hits += 1;
            result.player_crits += crit ? 1 : 0;
            result.enemy_blocks += block ? 1 : 0;
            append_event(&result, now, GAME_COMBAT_ACTOR_PLAYER, damage, player_hp, enemy_hp, crit, block);
            player_next = now + player_interval;
        } else {
            bool enemy_crit = false;
            bool player_block = false;
            const int enemy_damage = resolve_hit(enemy_stats, player_stats, &seed, &enemy_crit, &player_block);
            player_hp -= enemy_damage;
            result.enemy_damage_done += enemy_damage;
            result.enemy_hits += 1;
            result.enemy_crits += enemy_crit ? 1 : 0;
            result.player_blocks += player_block ? 1 : 0;
            append_event(&result, now, GAME_COMBAT_ACTOR_ENEMY, enemy_damage, player_hp, enemy_hp, enemy_crit, player_block);
            enemy_next = now + enemy_interval;
        }
    }

    if (player_hp <= 0 && enemy_hp <= 0) {
        player_hp = 1;
        enemy_hp = 0;
    }
    result.outcome = enemy_hp <= 0 ? GAME_COMBAT_OUTCOME_WIN : GAME_COMBAT_OUTCOME_LOSS;
    result.player_hp = player_hp > 0 ? player_hp : 0;
    result.enemy_hp = enemy_hp > 0 ? enemy_hp : 0;
    result.duration_seconds = now;
    *out_result = result;
    return true;
}
