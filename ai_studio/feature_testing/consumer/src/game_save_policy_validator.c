#include "game_save_policy.h"

#include <stdio.h>

#include "game_items.h"
#include "game_save.h"
#include "progression_state.h"
#include "settings_state.h"

static bool required_fragment(const cJSON *features, const char *name,
                              const cJSON **fragment_out,
                              char *error, int error_cap) {
    const cJSON *fragment = cJSON_GetObjectItemCaseSensitive(features, name);
    if (cJSON_IsObject(fragment)) {
        *fragment_out = fragment;
        return true;
    }
    if (error != NULL && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s fragment is required", name);
    }
    return false;
}

static bool validate_document(const cJSON *features,
                              char *error, int error_cap) {
    if (!game_items_validate_save_document(features, error, error_cap)) return false;

    const cJSON *settings_json = cJSON_GetObjectItemCaseSensitive(features, "settings");
    const cJSON *progression_json = cJSON_GetObjectItemCaseSensitive(features, "progression");
    if ((settings_json != NULL && !required_fragment(features, "settings", &settings_json,
                                                      error, error_cap)) ||
        (progression_json != NULL && !required_fragment(features, "progression",
                                                         &progression_json, error, error_cap))) {
        return false;
    }

    if (settings_json != NULL) {
        SettingsState staged_settings;
        settings_state_init_defaults(&staged_settings);
        if (!settings_state_from_json(&staged_settings, settings_json, error, error_cap) ||
            !settings_state_validate(&staged_settings, error, error_cap)) return false;
    }
    if (progression_json != NULL) {
        ProgressionState staged_progression;
        progression_state_init_defaults(&staged_progression);
        if (!progression_state_from_json(&staged_progression, progression_json,
                                         error, error_cap) ||
            !progression_state_validate(&staged_progression, error, error_cap)) return false;
    }
    return true;
}

void game_configure_save(void) {
    static const GameSaveDocumentMigrateFn migrations[] = {
        game_items_migrate_document_v1_to_v2,
    };
    game_save_set_document_migrations(migrations, 1);
    game_save_set_document_validator(validate_document);
    game_save_set_live_validator(game_items_validate_live_save);
}
