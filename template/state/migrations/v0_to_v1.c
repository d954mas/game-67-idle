#include "game_state.h"

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

bool game_state_migrate_v0_to_v1(cJSON *state, char *error, int error_cap) {
    if (!cJSON_IsObject(state)) {
        set_error(error, error_cap, "v0 state must be object");
        return false;
    }

    cJSON *shape = cJSON_GetObjectItemCaseSensitive(state, "shape");
    if (cJSON_IsString(shape) && !cJSON_GetObjectItemCaseSensitive(state, "shape_index")) {
        for (int i = 0; i < GAME_STATE_SHAPE_COUNT; i++) {
            if (strcmp(shape->valuestring, game_state_shape_name(i)) == 0) {
                cJSON_AddNumberToObject(state, "shape_index", i);
                break;
            }
        }
    }
    cJSON_DeleteItemFromObjectCaseSensitive(state, "shape");

    cJSON *render_mode = cJSON_GetObjectItemCaseSensitive(state, "render_mode");
    if (cJSON_IsString(render_mode) && !cJSON_GetObjectItemCaseSensitive(state, "render_mode_index")) {
        for (int i = 0; i < GAME_STATE_RENDER_MODE_COUNT; i++) {
            if (strcmp(render_mode->valuestring, game_state_render_mode_name(i)) == 0) {
                cJSON_AddNumberToObject(state, "render_mode_index", i);
                break;
            }
        }
    }
    cJSON_DeleteItemFromObjectCaseSensitive(state, "render_mode");

    ensure_object(state, "tutorial");
    cJSON *tutorial = cJSON_GetObjectItemCaseSensitive(state, "tutorial");
    if (!cJSON_GetObjectItemCaseSensitive(tutorial, "done")) {
        cJSON_AddBoolToObject(tutorial, "done", false);
    }

    ensure_object(state, "wallet");
    cJSON *wallet = cJSON_GetObjectItemCaseSensitive(state, "wallet");
    if (!cJSON_GetObjectItemCaseSensitive(wallet, "soft")) {
        cJSON_AddNumberToObject(wallet, "soft", 0);
    }
    if (!cJSON_GetObjectItemCaseSensitive(wallet, "hard")) {
        cJSON_AddNumberToObject(wallet, "hard", 0);
    }

    ensure_object(state, "items");
    ensure_object(state, "inventory");
    cJSON *inventory = cJSON_GetObjectItemCaseSensitive(state, "inventory");
    if (!cJSON_GetObjectItemCaseSensitive(inventory, "item_ids")) {
        cJSON_AddItemToObject(inventory, "item_ids", cJSON_CreateArray());
    }

    ensure_object(state, "equipment");
    cJSON *equipment = cJSON_GetObjectItemCaseSensitive(state, "equipment");
    if (!cJSON_GetObjectItemCaseSensitive(equipment, "hand_item_id")) {
        cJSON_AddNullToObject(equipment, "hand_item_id");
    }

    return true;
}
