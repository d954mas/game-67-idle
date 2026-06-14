#ifndef GAME_STATE_ACTIONS_H
#define GAME_STATE_ACTIONS_H

#include "generated/game_state.h"

#include <stdbool.h>

void game_seed_reset_playtest(GameState *state);
void game_seed_cycle(GameState *state);
const char *game_seed_shape_label(const GameState *state);

void game_rune_reset_playtest(GameState *state);
void game_rune_scout(GameState *state);
void game_rune_scout_east(GameState *state);
void game_rune_scout_greenfen(GameState *state);
bool game_rune_can_scout_greenfen(const GameState *state);
void game_rune_primary_action(GameState *state);
void game_rune_strike(GameState *state);
void game_rune_spark(GameState *state);
void game_rune_guard(GameState *state);
void game_rune_retreat(GameState *state);
void game_rune_rest(GameState *state);
void game_rune_buy_spark_ward(GameState *state);
void game_rune_study_rune_lore(GameState *state);
bool game_rune_can_study_rune_lore(const GameState *state);
void game_rune_choose_bell_rope_silver(GameState *state);
void game_rune_choose_bell_rope_kindness(GameState *state);
void game_rune_inspect_tower(GameState *state);
void game_rune_light_moss_shrine(GameState *state);
bool game_rune_can_light_moss_shrine(const GameState *state);
void game_rune_open_causeway(GameState *state);
bool game_rune_can_open_causeway(const GameState *state);
void game_rune_choose_briar_gate(GameState *state);
void game_rune_choose_moonwell(GameState *state);
bool game_rune_can_choose_next_route(const GameState *state);
void game_rune_scout_briar_gate(GameState *state);
bool game_rune_can_scout_briar_gate(const GameState *state);
void game_rune_scout_moonwell(GameState *state);
bool game_rune_can_scout_moonwell(const GameState *state);
void game_rune_discover_ashen_cairn(GameState *state);
bool game_rune_can_discover_ashen_cairn(const GameState *state);
void game_rune_discover_starfall_grotto(GameState *state);
bool game_rune_can_discover_starfall_grotto(const GameState *state);
int game_rune_spark_damage(const GameState *state);
int game_rune_enemy_max_hp(const GameState *state);
int game_rune_hp_max(const GameState *state);

#endif
