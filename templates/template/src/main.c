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
#include "features/game_features.h"
#include "game_input.h"
#include "game_save.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "render/blank_renderer.h"
#include "window/nt_window.h"
#if defined(NT_PLATFORM_WEB)
#include "crash/nt_crash.h"
#include "platform/web/nt_platform_web.h"
#include "runtime_build_marker.h"
#include <emscripten.h>
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static World s_world;
static game_input_frame_t s_input;
#if NT_DEVAPI_ENABLED
static bool s_devapi_requested;
static bool s_devapi_running;
static uint16_t s_devapi_port = 17890U;
static bool parse_devapi_port(const char *raw, uint16_t *out) {
    char *end = NULL;
    const long value = strtol(raw ? raw : "", &end, 10);
    if (end == raw || *end != '\0' || value < 1 || value > 65535) {
        return false;
    }
    *out = (uint16_t)value;
    return true;
}
#endif
#if defined(NT_PLATFORM_WEB)
static bool s_presented_first_frame;
static bool s_gfx_recovery_pending;
EM_JS(void, game_clear_runtime_recovery, (void), {
    if (typeof window.__clearRuntimeRecovery === 'function') {
        window.__clearRuntimeRecovery();
    }
})
#endif

#if NT_DEVAPI_ENABLED
static bool devapi_start(void) {
    if (!s_devapi_requested) {
        return true;
    }
    if (nt_devapi_init() != NT_OK) {
        return false;
    }
#ifndef NT_PLATFORM_WEB
    if (!nt_devapi_net_start(s_devapi_port)) {
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

#ifndef NT_PLATFORM_WEB
static void devapi_shutdown(void) {
    if (s_devapi_running) {
        nt_devapi_net_stop();
        nt_devapi_shutdown();
    }
}
#endif
#else
static bool devapi_start(void) { return true; }
static void devapi_update(void) {}
#ifndef NT_PLATFORM_WEB
static void devapi_shutdown(void) {}
#endif
#endif

static void frame(void) {
    nt_window_poll();
    devapi_update();
    nt_gfx_begin_frame();
#if defined(NT_PLATFORM_WEB)
    if (g_nt_gfx.context_lost) {
        nt_gfx_end_frame();
        return;
    }
    if (g_nt_gfx.context_restored) {
        s_gfx_recovery_pending = true;
    }
#endif
    nt_input_poll();
    game_input_capture(&s_input);
#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || s_input.escape_pressed) {
        nt_app_quit();
    }
#endif

    game_features_update(&s_world, g_nt_app.dt);
    game_features_update_root(&s_world, g_nt_app.dt, &s_input);
    game_features_react(&s_world);
    game_features_record(&s_world);
    game_save_update_playtime(!g_nt_app.paused);
    game_save_tick();

    const bool render_enabled = nt_app_render_enabled();
    if (render_enabled) {
        blank_renderer_begin();
        game_features_draw_world(&s_world);
        game_features_draw_ui(&s_world, &s_input);
        blank_renderer_end();
    }
    nt_gfx_end_frame();
    if (render_enabled) {
        nt_window_swap_buffers();
#if defined(NT_PLATFORM_WEB)
        // The shell may clear recovery only after a restored frame is presented.
        if (s_gfx_recovery_pending) {
            s_gfx_recovery_pending = false;
            game_clear_runtime_recovery();
        }
        if (!s_presented_first_frame) {
            runtime_build_marker_present_ready();
            s_presented_first_frame = true;
        }
#endif
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
#if NT_DEVAPI_ENABLED
        if (strcmp(argv[i], "--devapi") == 0 && i + 1 < argc) {
            s_devapi_requested = parse_devapi_port(argv[++i], &s_devapi_port);
            if (!s_devapi_requested) {
                (void)fprintf(stderr, "invalid --devapi port\n");
                return 2;
            }
        }
#else
        (void)argv;
#endif
    }

    const nt_engine_config_t config = {.app_name = GAME_APP_NAME, .version = 1};
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }
#if defined(NT_PLATFORM_WEB)
    nt_crash_init(GAME_CRASH_STORAGE_KEY, GAME_RUNTIME_BUILD_FINGERPRINT[0] ? GAME_RUNTIME_BUILD_FINGERPRINT : "preview");
#endif
    g_nt_window.title = GAME_WINDOW_TITLE;
    g_nt_window.width = 1280U;
    g_nt_window.height = 720U;
    nt_window_init();
    nt_input_init();
    const nt_gfx_desc_t graphics = nt_gfx_desc_defaults();
    nt_gfx_init(&graphics);

    game_save_register_fragment(&game_state_fragment);
    game_save_init();
    game_save_load_result_t load_result;
    game_save_load(&load_result);
    game_features_init(&s_world);
    if (!devapi_start()) {
        (void)fprintf(stderr, "failed to start DevAPI\n");
        return 1;
    }
#if defined(NT_PLATFORM_WEB)
    runtime_build_marker_publish();
    nt_platform_web_loading_complete();
#endif
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
    game_save_update_playtime(false);
    (void)game_save_flush(NULL, 0);
    game_features_shutdown(&s_world);
    devapi_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif
    return 0;
}
