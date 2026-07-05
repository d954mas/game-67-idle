#include "game_persistence.h"

#include "game_state.h"
#include "game_storage.h"

#include <stdlib.h>

#ifndef GAME_PERSISTENCE_SAVE_KEY
#define GAME_PERSISTENCE_SAVE_KEY "autosave"
#endif

const char *game_persistence_autosave_key(void) {
    return GAME_PERSISTENCE_SAVE_KEY;
}

bool game_persistence_load_autosave(bool fresh_state, char *error, int error_cap) {
    if (fresh_state) {
        game_state_clear_dirty();
        return true;
    }
    if (!game_storage_key_exists(GAME_PERSISTENCE_SAVE_KEY, GAME_STATE_DOCUMENT)) {
        game_state_clear_dirty();
        return true;
    }

    char *data = NULL;
    if (!game_storage_load_json(GAME_PERSISTENCE_SAVE_KEY, GAME_STATE_DOCUMENT, &data, error, error_cap)) {
        return false;
    }
    const bool ok = game_state_load_json_string(&g_game_state, data, error, error_cap);
    free(data);
    if (ok) {
        game_state_clear_dirty();
    }
    return ok;
}

bool game_persistence_save_autosave(char *error, int error_cap) {
    char *data = game_state_save_json_string(&g_game_state, error, error_cap);
    if (!data) {
        return false;
    }
    const bool ok = game_storage_save_json(GAME_PERSISTENCE_SAVE_KEY, GAME_STATE_DOCUMENT, data, error, error_cap);
    cJSON_free(data);
    if (ok) {
        game_state_clear_dirty();
    }
    return ok;
}

bool game_persistence_save_autosave_if_dirty(char *error, int error_cap) {
    if (!game_state_is_dirty()) {
        return true;
    }
    return game_persistence_save_autosave(error, error_cap);
}
