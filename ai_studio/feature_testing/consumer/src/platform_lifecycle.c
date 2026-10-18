#include "platform_lifecycle.h"

#include "features/platform_sdk/platform_sdk.h"

static bool s_platform_lifecycle_initialized;
static bool s_gameplay_input_seen;

void platform_lifecycle_init(void) {
    s_platform_lifecycle_initialized = true;
    (void)platform_sdk_game_loading_progress(0.10f);
    (void)platform_sdk_init();
}

bool platform_lifecycle_on_input(bool input_seen) {
    if (!s_platform_lifecycle_initialized) {
        return false;
    }
    if (input_seen) {
        platform_lifecycle_mark_gameplay_input();
    }
    return input_seen;
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

// The pack download owns 0.45..0.90; the tail belongs to asset activation and
// the first presented frame, which only after_frame_present can confirm.
float platform_lifecycle_loading_progress_from_pack(unsigned int received, unsigned int total, bool pack_ready) {
    if (pack_ready) {
        return 0.90f;
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
    return 0.45f + (pack_progress * 0.45f);
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
