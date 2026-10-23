#include "shell/shell.h"

#include "app/nt_app.h"
#include "core/nt_core.h"
#include "devapi/nt_devapi.h"
#if NT_DEVAPI_ENABLED
#include "devapi/nt_devapi_capture.h"
#ifndef NT_PLATFORM_WEB
#include "devapi/nt_devapi_net.h"
#else
#include "devapi/nt_devapi_web.h"
#endif
#endif
#include "game_save.h"
#include "game_music_pack.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "window/nt_window.h"
#if defined(NT_PLATFORM_WEB)
#include "platform/web/nt_platform_web.h"
#include "runtime_build_marker.h"
#include <emscripten.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if NT_DEVAPI_ENABLED
static bool s_devapi_running;

static bool parse_devapi_port(const char *raw, uint16_t *out) {
    char *end = NULL;
    const long value = strtol(raw ? raw : "", &end, 10);
    if (end == raw || *end != '\0' || value < 1 || value > 65535) {
        return false;
    }
    *out = (uint16_t)value;
    return true;
}

static bool devapi_start(const shell_options_t *options) {
    if (!options->devapi_requested) {
        return true;
    }
    if (nt_devapi_init() != NT_OK) {
        return false;
    }
#ifndef NT_PLATFORM_WEB
    if (!nt_devapi_net_start(options->devapi_port)) {
        nt_devapi_shutdown();
        return false;
    }
#endif
    nt_devapi_register_default();
    game_save_register_devapi(NULL, NULL);
#ifdef NT_PLATFORM_WEB
    nt_devapi_web_install_shim();
#endif
    nt_devapi_capture_install_seam();
    s_devapi_running = true;
    return true;
}

static void devapi_update(void) {
    if (s_devapi_running) {
        nt_devapi_update();
    }
}

static void devapi_shutdown(void) {
#ifndef NT_PLATFORM_WEB
    if (s_devapi_running) {
        nt_devapi_net_stop();
        nt_devapi_shutdown();
    }
#endif
}
#else
static bool devapi_start(const shell_options_t *options) {
    (void)options;
    return true;
}
static void devapi_update(void) {}
static void devapi_shutdown(void) {}
#endif

#if defined(NT_PLATFORM_WEB)
static bool s_presented_first_frame;
EM_JS(void, game_clear_runtime_recovery, (void), {
    if (typeof window.__clearRuntimeRecovery === 'function') {
        window.__clearRuntimeRecovery();
    }
})
#endif

int shell_platform_parse_args(int argc, char **argv, shell_options_t *options) {
    *options = (shell_options_t){.devapi_port = 17890U};
    for (int i = 1; i < argc; ++i) {
#if NT_DEVAPI_ENABLED
        if (strcmp(argv[i], "--devapi") == 0 && i + 1 < argc) {
            options->devapi_requested = parse_devapi_port(argv[++i], &options->devapi_port);
            if (!options->devapi_requested) {
                (void)fprintf(stderr, "invalid --devapi port\n");
                return 2;
            }
        }
#else
        (void)argv;
#endif
    }
    return 0;
}

bool shell_platform_start(const shell_options_t *options) {
    if (!devapi_start(options)) {
        (void)fprintf(stderr, "failed to start DevAPI\n");
        return false;
    }
#if defined(NT_PLATFORM_WEB)
    runtime_build_marker_publish();
    nt_platform_web_loading_complete();
#endif
    return true;
}

void shell_platform_frame_begin(game_input_frame_t *input) {
    nt_window_poll();
    devapi_update();
    nt_gfx_begin_frame();
    nt_input_poll();
    game_input_capture(input);
#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || input->escape_pressed) {
        nt_app_quit();
    }
#endif
}

void shell_platform_frame_end(shell_shown_t shown) {
    if (!shown.presented) {
        return;
    }
    nt_window_swap_buffers();
    if (shown.complete) { game_music_pack_after_present(); }
#if defined(NT_PLATFORM_WEB)
    // The shell may clear recovery only after a restored frame is presented.
    if (shown.recovered) {
        game_clear_runtime_recovery();
    }
    if (shown.complete && !s_presented_first_frame) {
        runtime_build_marker_present_ready();
        s_presented_first_frame = true;
    }
#else
    (void)shown;
#endif
}

void shell_platform_shutdown(void) { devapi_shutdown(); }
