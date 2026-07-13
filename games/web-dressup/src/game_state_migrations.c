#include "game_state_migrations.h"

#include "game_state_json.h"

bool game_migrate_1_to_2(cJSON *fragment, char *error, int error_cap) {
    if (!cJSON_IsObject(fragment)) {
        gsj_set_error(error, error_cap, "game v1 fragment must be an object");
        return false;
    }
    const cJSON *saved = gsj_object_item(fragment, "saved_looks");
    if (saved) {
        if (!cJSON_IsObject(saved)) {
            gsj_set_error(error, error_cap, "saved_looks must be an object");
            return false;
        }
        return true;
    }
    cJSON *empty = cJSON_CreateObject();
    if (!empty || !cJSON_AddItemToObject(fragment, "saved_looks", empty)) {
        cJSON_Delete(empty);
        gsj_set_error(error, error_cap, "failed to add saved_looks");
        return false;
    }
    return true;
}
