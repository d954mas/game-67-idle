// The conductor: it sequences the shell phases (shell/shell.h) and owns
// nothing else. A game adds systems (systems/sys_*.c, render/*, ui/*) and grows
// the phase that runs them, never this file.
#include "app/nt_app.h"
#include "core/nt_core.h"
#include "shell/shell.h"
#include "window/nt_window.h"
#if defined(__EMSCRIPTEN__)
#include "crash/nt_crash.h"
#endif
#ifdef NT_PLATFORM_WEB
#include "runtime_build_marker.h"
#endif

static World s_world;
static game_input_frame_t s_input;
static shell_options_t s_options;

static void frame(void) {
    shell_platform_frame_begin(&s_input);
    shell_present_upload();
    const shell_tick_status_t tick = shell_tick(&s_world, &s_input);
    const shell_shown_t shown = shell_present(&s_world, &s_input, tick);
    if (shown.complete) {
        shell_tick_after_draw(&s_world);
    }
    shell_platform_frame_end(shown, tick.playable);
}

int main(int argc, char **argv) {
    const int args_status = shell_platform_parse_args(argc, argv, &s_options);
    if (args_status != 0) {
        return args_status;
    }
#ifdef NT_PLATFORM_WEB
    runtime_build_marker_publish();
#endif
    nt_engine_config_t config = {0};
    config.app_name = "Template";
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }
#if defined(__EMSCRIPTEN__)
    nt_crash_init(GAME_CRASH_STORAGE_KEY, GAME_RUNTIME_BUILD_FINGERPRINT[0] ? GAME_RUNTIME_BUILD_FINGERPRINT : "preview");
#endif

#ifndef GAME_WINDOW_TITLE
#define GAME_WINDOW_TITLE "Template" // CMake sets "<GAME_TITLE> [AI]" for DevAPI builds
#endif
    g_nt_window.title = GAME_WINDOW_TITLE;
    if (!shell_boot_init(&s_options, &s_world)) {
        return 1;
    }
    shell_tick_init(&s_options);
    shell_present_init(&s_options);
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
    shell_platform_shutdown();
    shell_tick_shutdown(&s_world);
    shell_boot_shutdown();
    nt_engine_shutdown();
#endif
    return 0;
}
