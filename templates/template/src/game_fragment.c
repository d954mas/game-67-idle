/* game_fragment.c — transitional adapter (§A3.7). Defines the single `game`
   fragment whose hooks drive the generated GameState monolith (g_game_state).
   In A4 the `game` fragment is regenerated as v2 and this adapter goes away.

   The generated game_state_to_json / game_state_from_json operate on the INNER
   state object (a flat payload without the "v"/envelope); game_save now builds
   the envelope and stamps "v", so the adapter hands off / takes a flat payload. */

#include "game_save.h"

#include "game_state.h" /* generated monolith: g_game_state + the game_state_* API */

static cJSON *game_to_json(void) { return game_state_to_json(&g_game_state); }

static bool game_from_json(const cJSON *json, char *error, int error_cap) {
    return game_state_from_json(&g_game_state, json, error, error_cap);
}

static void game_reset(void) { game_state_init_defaults(&g_game_state); }

/* on_new_game = NULL: the monolith defaults ARE the template's starting state; a
   game fills this hook with content choices (starting gold, sword, first quest). */

static cJSON *game_get_path(const char *sub, char *error, int error_cap) {
    return game_state_get_path_json(&g_game_state, sub, error, error_cap); /* ready for A5 */
}

static bool game_set_path(const char *sub, const cJSON *value, char *error, int error_cap) {
    return game_state_set_path_json(&g_game_state, sub, value, error, error_cap); /* ready for A5 */
}

static cJSON *game_schema(void) { return game_state_schema_json(); }

const GameSaveFragment game_fragment = {
    .id = "game",
    .version = GAME_STATE_VERSION,
    .steps = NULL,
    .reset = game_reset,
    .on_new_game = NULL,
    .to_json = game_to_json,
    .from_json = game_from_json,
    .reconcile = NULL,
    .get_path_json = game_get_path,
    .set_path_json = game_set_path,
    .schema_json = game_schema,
};
