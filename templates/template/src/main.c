#include "app/nt_app.h"
#include "core/nt_core.h"
#include "shell/shell.h"
#include "window/nt_window.h"
#if defined(NT_PLATFORM_WEB)
#include "crash/nt_crash.h"
#include "runtime_build_marker.h"
#endif

static World s_world;
static game_input_frame_t s_input;

// The phase order is the contract in shell/shell.h; nothing else runs here.
static void frame(void) {
    shell_platform_frame_begin(&s_input);
    shell_tick(&s_world, &s_input);
    shell_platform_frame_end(shell_present(&s_world, &s_input));
}

int main(int argc, char **argv) {
    shell_options_t options;
    const int args_status = shell_platform_parse_args(argc, argv, &options);
    if (args_status != 0) {
        return args_status;
    }

    const nt_engine_config_t config = {.app_name = GAME_APP_NAME};
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }
#if defined(NT_PLATFORM_WEB)
    nt_crash_init(GAME_CRASH_STORAGE_KEY, GAME_RUNTIME_BUILD_FINGERPRINT[0] ? GAME_RUNTIME_BUILD_FINGERPRINT : "preview");
#endif
    g_nt_window.title = GAME_WINDOW_TITLE;
    shell_boot_init();
    shell_tick_init(&s_world);
    if (!shell_platform_start(&options)) {
        return 1;
    }
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
    shell_platform_shutdown();
    shell_tick_shutdown(&s_world);
    shell_boot_shutdown();
    nt_engine_shutdown();
#endif
    return 0;
}
