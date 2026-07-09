#include "platform_lifecycle.h"

#include "features/platform_sdk/platform_sdk.h"
#include "input/nt_input.h"

static bool s_platform_lifecycle_initialized;
static bool s_gameplay_input_seen;

static bool has_pointer_input_edge(void) {
    for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
        const nt_pointer_t *pointer = &g_nt_input.pointers[i];
        if (!pointer->active) {
            continue;
        }
        if (pointer->wheel_dx != 0.0F || pointer->wheel_dy != 0.0F) {
            return true;
        }
        for (int button = 0; button < NT_BUTTON_MAX; ++button) {
            if (pointer->buttons[button].is_pressed) {
                return true;
            }
        }
    }
    return false;
}

static bool has_gameplay_input_edge(void) {
    return nt_input_key_is_pressed(NT_KEY_W) || nt_input_key_is_pressed(NT_KEY_A) ||
           nt_input_key_is_pressed(NT_KEY_S) || nt_input_key_is_pressed(NT_KEY_D);
}

void platform_lifecycle_init(void) {
    s_platform_lifecycle_initialized = true;
    (void)platform_sdk_game_loading_progress(0.10f);
    (void)platform_sdk_init();
}

void platform_lifecycle_after_input_poll(void) {
    if (!s_platform_lifecycle_initialized) {
        return;
    }
    if (!platform_sdk_has_input() && (nt_input_any_key_pressed() || has_pointer_input_edge())) {
        platform_sdk_mark_input();
    }
    if (has_gameplay_input_edge()) {
        platform_lifecycle_mark_gameplay_input();
    }
}

void platform_lifecycle_mark_gameplay_input(void) {
    s_gameplay_input_seen = true;
    platform_sdk_mark_input();
}

void platform_lifecycle_update(bool playable_shell_ready, bool gameplay_allowed) {
    if (!s_platform_lifecycle_initialized) {
        return;
    }

    const bool should_play = playable_shell_ready && gameplay_allowed && platform_sdk_has_input() &&
                             (s_gameplay_input_seen || platform_sdk_has_gameplay_started());
    if (should_play) {
        (void)platform_sdk_gameplay_start();
    } else if (platform_sdk_gameplay_active()) {
        (void)platform_sdk_gameplay_stop();
    }
}

void platform_lifecycle_after_frame_present(bool playable_shell_ready) {
    if (!s_platform_lifecycle_initialized || !playable_shell_ready) {
        return;
    }
    (void)platform_sdk_game_loading_progress(1.0f);
    if (platform_sdk_game_loading_finished() == PLATFORM_SDK_RESULT_OK) {
        (void)platform_sdk_game_ready();
    }
}

float platform_lifecycle_loading_progress_from_pack(unsigned int received, unsigned int total, bool pack_ready) {
    if (pack_ready) {
        return 1.0f;
    }
    if (total == 0u) {
        return 0.45f;
    }
    float pack_progress = (float)received / (float)total;
    if (pack_progress < 0.0f) {
        pack_progress = 0.0f;
    } else if (pack_progress > 1.0f) {
        pack_progress = 1.0f;
    }
    return 0.45f + (pack_progress * 0.55f);
}

void platform_lifecycle_shutdown(void) {
    if (!s_platform_lifecycle_initialized) {
        return;
    }
    if (platform_sdk_gameplay_active()) {
        (void)platform_sdk_gameplay_stop();
    }
    platform_sdk_destroy();
    s_platform_lifecycle_initialized = false;
    s_gameplay_input_seen = false;
}
