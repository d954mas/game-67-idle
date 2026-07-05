#ifndef RB_DARK_RPG_GAME_COMBAT_H
#define RB_DARK_RPG_GAME_COMBAT_H

#include "game_content.h"
#include "game_state.h"

#include <stdbool.h>

typedef enum game_combat_outcome_t {
    GAME_COMBAT_OUTCOME_NONE = 0,
    GAME_COMBAT_OUTCOME_WIN,
    GAME_COMBAT_OUTCOME_LOSS,
} game_combat_outcome_t;

typedef enum game_combat_actor_t {
    GAME_COMBAT_ACTOR_NONE = 0,
    GAME_COMBAT_ACTOR_PLAYER,
    GAME_COMBAT_ACTOR_ENEMY,
} game_combat_actor_t;

#define GAME_COMBAT_MAX_EVENTS 256

typedef struct game_combat_event_t {
    float time_seconds;
    game_combat_actor_t actor;
    int damage;
    int player_hp_after;
    int enemy_hp_after;
    bool crit;
    bool block;
} game_combat_event_t;

typedef struct game_combat_result_t {
    game_combat_outcome_t outcome;
    game_combat_event_t events[GAME_COMBAT_MAX_EVENTS];
    int event_count;
    int player_hp;
    int enemy_hp;
    int player_damage_done;
    int enemy_damage_done;
    int player_hits;
    int enemy_hits;
    int player_crits;
    int enemy_crits;
    int player_blocks;
    int enemy_blocks;
    float duration_seconds;
    bool reward_granted;
} game_combat_result_t;

int game_combat_attack_power(const game_combat_stats_t *stats);
bool game_combat_build_player_stats(const GameState *state, game_combat_stats_t *out_stats);
bool game_combat_simulate(const game_combat_stats_t *player_stats, int player_current_hp,
                          const game_encounter_definition_t *encounter, unsigned int seed,
                          game_combat_result_t *out_result);

#endif
