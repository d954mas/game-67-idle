#include "generated/game_state.h"

#include <stdio.h>
#include <string.h>

static void set_error(char *error, int error_cap, const char *message) {
    if (error && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
}

static void ensure_object(cJSON *state, const char *name) {
    if (!cJSON_GetObjectItemCaseSensitive(state, name)) {
        cJSON_AddItemToObject(state, name, cJSON_CreateObject());
    }
}

static void ensure_number(cJSON *obj, const char *name, double value) {
    if (!cJSON_GetObjectItemCaseSensitive(obj, name)) {
        cJSON_AddNumberToObject(obj, name, value);
    }
}

/* v1 -> v2: introduce the casual-RPG "run" progression block. Pre-v2 saves had
 * no run state, so seed the defaults that match the schema. */
bool game_state_migrate_v1_to_v2(cJSON *state, char *error, int error_cap) {
    if (!cJSON_IsObject(state)) {
        set_error(error, error_cap, "v1 state must be object");
        return false;
    }

    ensure_object(state, "run");
    cJSON *run = cJSON_GetObjectItemCaseSensitive(state, "run");
    ensure_number(run, "hero_hp", 100);
    ensure_number(run, "hero_max_hp", 100);
    ensure_number(run, "level", 1);
    ensure_number(run, "xp", 0);
    ensure_number(run, "xp_to_next", 60);
    ensure_number(run, "enemies_defeated", 0);
    if (!cJSON_GetObjectItemCaseSensitive(run, "keep_reached")) {
        cJSON_AddBoolToObject(run, "keep_reached", false);
    }
    ensure_number(run, "ftue_step", 0);

    return true;
}
