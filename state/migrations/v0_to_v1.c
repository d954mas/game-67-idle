#include "game_state.h"

#include <stdio.h>

// Little Lives starts at schema version 1; there are no real v0 saves to carry
// forward, so this is a safe no-op. Missing fields fall back to defaults during
// game_state_from_json, and unknown legacy fields are ignored.
bool game_state_migrate_v0_to_v1(cJSON *state, char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    if (!cJSON_IsObject(state)) {
        if (error && error_cap > 0) {
            (void)snprintf(error, (size_t)error_cap, "v0 state must be object");
        }
        return false;
    }
    return true;
}
