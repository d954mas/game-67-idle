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

static void ensure_bool(cJSON *obj, const char *name, bool value) {
    if (!cJSON_GetObjectItemCaseSensitive(obj, name)) {
        cJSON_AddBoolToObject(obj, name, value);
    }
}

/* v2 -> v3: introduce the idle / incremental RPG "idle" progression block.
 * Pre-v3 saves had only the real-time "run" block, so seed schema defaults. */
bool game_state_migrate_v2_to_v3(cJSON *state, char *error, int error_cap) {
    if (!cJSON_IsObject(state)) {
        set_error(error, error_cap, "v2 state must be object");
        return false;
    }

    ensure_object(state, "idle");
    cJSON *idle = cJSON_GetObjectItemCaseSensitive(state, "idle");
    ensure_number(idle, "gold", 0);
    ensure_number(idle, "stage", 1);
    ensure_number(idle, "highest_stage", 1);
    ensure_number(idle, "kills_in_stage", 0);
    ensure_number(idle, "up_sword", 0);
    ensure_number(idle, "up_boots", 0);
    ensure_number(idle, "up_armor", 0);
    ensure_number(idle, "up_luck", 0);
    ensure_number(idle, "frost_shards", 0);
    ensure_number(idle, "shard_global_damage", 0);
    ensure_number(idle, "shard_global_gold", 0);
    ensure_number(idle, "shard_start_stage", 0);
    ensure_number(idle, "shard_offline_rate", 0);
    ensure_number(idle, "last_seen_unix", 0);
    ensure_bool(idle, "offline_unlocked", false);
    ensure_bool(idle, "boss_active", false);

    return true;
}
