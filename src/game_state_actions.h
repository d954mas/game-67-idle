#ifndef GAME_STATE_ACTIONS_H
#define GAME_STATE_ACTIONS_H

#include <stdbool.h>

#include "generated/game_state.h"

#define GAME_67_VARIANT_COUNT 30
#define GAME_67_BOARD_SLOTS 12
#define GAME_67_PASSIVE_INTERVAL_FRAMES 300
#define GAME_67_FASTER_SPAWN_COST 25
#define GAME_67_BETTER_CRATE_MAX_LEVEL (GAME_67_VARIANT_COUNT - 2)

typedef struct Game67VariantDef {
    const char *id;
    const char *name;
    int order;
    int passive_coins_per_tick;
    int discovery_bonus;
} Game67VariantDef;

const Game67VariantDef *game_67_variants(void);
int game_67_variant_count(const GameState *state, int index);
int game_67_total_on_board(const GameState *state);
int game_67_passive_income_per_tick(const GameState *state);
bool game_67_can_spawn(const GameState *state);
bool game_67_can_merge(const GameState *state);
bool game_67_can_recycle_lowest(const GameState *state);
bool game_67_can_buy_faster_spawn(const GameState *state);
bool game_67_can_buy_better_crate(const GameState *state);
int game_67_faster_spawn_cost_remaining(const GameState *state);
int game_67_better_crate_next_cost(const GameState *state);
int game_67_better_crate_cost_remaining(const GameState *state);
int game_67_spawn_variant_index(const GameState *state);
const char *game_67_faster_spawn_state(const GameState *state);
const char *game_67_better_crate_state(const GameState *state);
const char *game_67_ftue_step(const GameState *state);
const char *game_67_ftue_prompt(const GameState *state);
const char *game_67_next_goal(const GameState *state);
void game_67_reset_playtest(GameState *state);
bool game_67_spawn(GameState *state);
bool game_67_recycle_lowest(GameState *state);
bool game_67_merge_lowest(GameState *state);
bool game_67_merge_variant(GameState *state, int index);
bool game_67_buy_faster_spawn(GameState *state);
bool game_67_buy_better_crate(GameState *state);
bool game_67_tick_passive(GameState *state, int frames);

#endif
