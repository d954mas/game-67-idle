#include "shell/shell.h"

#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "log/nt_log.h"
#include "memory/nt_mem_scratch.h"
#include "time/nt_time.h"
#include "window/nt_window.h"

#if NT_DEVAPI_ENABLED
#include "devapi/nt_devapi.h"
#ifdef NT_PLATFORM_WEB
#include "devapi/nt_devapi_web.h"
#else
#include "devapi/nt_devapi_net.h"
#endif
#ifdef NT_DEVAPI_GROUP_CAPTURE
#include "devapi/nt_devapi_capture.h"
#endif
#ifdef NT_DEVAPI_GROUP_OBS
#include "log/nt_log_ring.h"
#include "metrics/nt_metrics.h"
#endif
#ifndef NT_DEVAPI_DEFAULT_PORT
#define NT_DEVAPI_DEFAULT_PORT 17890
#endif
#include "features/scenes/scene_manager_devapi.h"
#include "game_events_devapi.h"
#include "game_items.h"
#include "iteration_proof_devapi.h"
#include "features/platform_sdk/platform_sdk_devapi.h"
#include "features/platform_sdk/platform_hooks.h"
#include "ui/ui_runtime.h"
#endif

#include "game_audio.h"
#include "game_save.h"
#include "game_scenes.h"
#include "game_storage.h"
#include "features/platform_sdk/platform_lifecycle.h"
#include "ui/focus_prompt_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

static const shell_options_t *s_options;
static double s_frame_begin;
#if NT_DEVAPI_ENABLED
static bool s_devapi_running;
#endif

#if defined(__EMSCRIPTEN__)
/* clang-format off */
EM_JS(double, game_expected_pack_bytes, (void), {
    var bytes = globalThis.__gameAssetBytes && globalThis.__gameAssetBytes.pack;
    return bytes > 0 ? bytes : 0;
})

EM_JS(void, game_clear_runtime_recovery, (void), {
    if (typeof window.__clearRuntimeRecovery === "function") {
        window.__clearRuntimeRecovery();
    }
})
/* clang-format on */
#endif

static void game_report_gfx_recovery_complete(void) {
#if defined(__EMSCRIPTEN__)
    game_clear_runtime_recovery();
#endif
}

float shell_platform_pack_loading_progress(nt_hash32_t pack_id) {
    uint32_t received = 0u;
    uint32_t total = 0u;
    const nt_pack_state_t state = nt_resource_pack_state(pack_id);
    nt_resource_pack_progress(pack_id, &received, &total);
#if defined(__EMSCRIPTEN__)
    /* A CDN that compresses on the fly sends no Content-Length, so the engine's
       total stays 0 while the bytes it counts are already decompressed. The
       build wrote the real size into the shell; the header is only a fallback. */
    const uint32_t expected = (uint32_t)game_expected_pack_bytes();
    if (expected > 0u) {
        total = expected;
    }
#endif
    return platform_lifecycle_loading_progress_from_pack(received, total, state == NT_PACK_STATE_READY);
}

static bool parse_window_size_arg(const char *raw, int *out_w, int *out_h) {
    const char *sep = raw ? strchr(raw, 'x') : NULL;
    if (!sep) {
        sep = raw ? strchr(raw, 'X') : NULL;
    }
    if (!sep) {
        return false;
    }
    char *end = NULL;
    long w = strtol(raw, &end, 10);
    if (end != sep) {
        return false;
    }
    long h = strtol(sep + 1, &end, 10);
    if (*end != '\0' || w <= 0 || h <= 0 || w > 16384 || h > 16384) {
        return false;
    }
    *out_w = (int)w;
    *out_h = (int)h;
    return true;
}

#if NT_DEVAPI_ENABLED
static bool parse_devapi_port(const char *raw, uint16_t *out) {
    char *end = NULL;
    long v = strtol(raw ? raw : "", &end, 10);
    if (end == raw || *end != '\0' || v < 1 || v > 65535) {
        return false;
    }
    *out = (uint16_t)v;
    return true;
}

static void game_runtime_on_devapi_state_change(
    game_save_devapi_change_t change,
    const char *fragment_id, void *user) {
    (void)user;
    if (change == GAME_SAVE_DEVAPI_EDIT && fragment_id) {
        (void)game_save_reconcile_from(fragment_id);
    }
}

// A bot that drops mid-step would leave the game frozen; the engine resets only its own state.
static void recover_on_client_reset(void) {
    g_nt_app.mode = NT_APP_MODE_RUN;
    g_nt_app.paused = false;
    g_nt_app.pending_steps = 0;
}

bool shell_platform_start(World *world) {
    (void)world;
    if (!s_options->devapi_requested) {
        return true;
    }
#ifdef NT_DEVAPI_GROUP_OBS
    nt_log_ring_init();
    nt_log_add_sink(nt_log_ring_sink, NULL);
    nt_metrics_init();
#endif
    if (nt_devapi_init() != NT_OK) {
        fprintf(stderr, "failed to initialize DevAPI\n");
        return false;
    }
    s_devapi_running = true;
#ifndef NT_PLATFORM_WEB
    if (!nt_devapi_net_start(s_options->devapi_port)) {
        fprintf(stderr, "failed to start DevAPI TCP transport on 127.0.0.1:%u\n", (unsigned)s_options->devapi_port);
        nt_devapi_shutdown();
        s_devapi_running = false;
        return false;
    }
    fprintf(stderr, "DevAPI listening on 127.0.0.1:%u\n", (unsigned)s_options->devapi_port);
#endif
    nt_devapi_register_default();
    nt_devapi_register_reset(recover_on_client_reset);
    scene_manager_register_devapi(game_scenes_manager());
    game_iteration_proof_register_devapi();
    game_items_register_devapi();
    platform_sdk_register_devapi();
    platform_hooks_register_devapi();
    game_save_register_devapi(game_runtime_on_devapi_state_change, NULL);
    game_events_register_devapi(); // also arms the recorder game.events.tail reads
#ifdef NT_DEVAPI_GROUP_UI
    static nt_devapi_ui_context s_ui_contexts[1];
    s_ui_contexts[0] = (nt_devapi_ui_context){.name = "hud", .ctx = ui_runtime_ctx()};
    nt_devapi_ui_set_contexts(s_ui_contexts, NT_COUNTOF(s_ui_contexts));
#endif
#ifdef NT_PLATFORM_WEB
    nt_devapi_web_install_shim();
#endif
#ifdef NT_DEVAPI_GROUP_CAPTURE
    nt_devapi_capture_install_seam();
#endif
#ifndef NT_PLATFORM_WEB
    (void)nt_devapi_net_wait_for_client(2000);
#endif
    return true;
}

static void devapi_update_frame(void) {
    if (!s_devapi_running) {
        return;
    }
    nt_devapi_update();
}

static void devapi_sample_metrics(double frame_begin) {
#ifdef NT_DEVAPI_GROUP_OBS
    if (!s_devapi_running) {
        return;
    }
    static double last_frame_begin;
    static uint64_t mem_used;
    static uint32_t mem_tick;
    const float frame_ms = last_frame_begin > 0.0 ? (float)((frame_begin - last_frame_begin) * 1000.0) : -1.0F;
    last_frame_begin = frame_begin;
    if ((mem_tick++ % 30U) == 0U) {
        mem_used = (uint64_t)nt_platform_memory_usage().used;
    }
    nt_metrics_frame_t frame = {
        .frame_ms = frame_ms,
        .cpu_ms = (float)((nt_time_now() - frame_begin) * 1000.0),
        .gpu_ms = -1.0F,
        .draw_calls = nt_gfx_draw_calls(&g_nt_gfx.counters),
        .mem_used = mem_used,
        .scratch_hwm = (uint32_t)nt_mem_scratch_high_water_mark(),
        .scratch_used = (uint32_t)nt_mem_scratch_used(),
    };
    nt_metrics_sample(&frame);
#else
    (void)frame_begin;
#endif
}

void shell_platform_shutdown(void) {
#ifndef NT_PLATFORM_WEB
    if (!s_devapi_running) {
        return;
    }
    nt_devapi_net_stop();
    nt_devapi_shutdown();
    s_devapi_running = false;
#endif
}
#else
bool shell_platform_start(World *world) {
    (void)world;
    return true;
}
static void devapi_update_frame(void) {}
static void devapi_sample_metrics(double frame_begin) { (void)frame_begin; }
void shell_platform_shutdown(void) {}
#endif

int shell_platform_parse_args(int argc, char **argv, shell_options_t *options) {
    *options = (shell_options_t){.window_width = 1280, .window_height = 720};
#if NT_DEVAPI_ENABLED
    options->devapi_port = NT_DEVAPI_DEFAULT_PORT;
#endif
    s_options = options;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc) {
            (void)snprintf(options->capture_path, sizeof(options->capture_path), "%s", argv[++i]);
            options->capture = true;
        } else if (strcmp(argv[i], "--settings") == 0) {
            options->open_settings_on_start = true;
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            if (!parse_window_size_arg(argv[++i], &options->window_width, &options->window_height)) {
                fprintf(stderr, "invalid --window-size, expected WIDTHxHEIGHT\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--fresh-state") == 0) {
            options->fresh_state = true; /* skip load, start from reset defaults */
        } else if (strcmp(argv[i], "--disable-autosave") == 0) {
            options->disable_autosave = true; /* keep loading, but never autosave */
        } else if (strcmp(argv[i], "--save-root") == 0 && i + 1 < argc) {
            /* A tool that seeds or captures saves must never reach the one
               someone is playing, so a bad root is a refused launch. */
            if (!game_storage_set_root(argv[++i])) {
                fprintf(stderr, "invalid --save-root, expected an absolute path\n");
                return 2;
            }
#if NT_DEVAPI_ENABLED
        } else if (strcmp(argv[i], "--no-vsync") == 0) {
            /* A window launched from an agent shell never becomes the
               foreground one, so with vsync on it presents at ~166 ms a frame
               and every bot crawls. Also: vsync pins each frame sample to the
               refresh interval, hiding the renderer cost a perf run compares. */
            options->no_vsync = true;
        } else if (strcmp(argv[i], "--time-manual") == 0) {
            /* Lockstep from frame zero. A bot that photographs the game needs
               the run's clock to be a property of the build, not of how long a
               pack took to load: taking manual time over the devapi is always
               too late, because real frames have already advanced the world by
               a dt nobody chose, and a screenshot is a photograph of the
               animation phase that clock landed on. */
            options->time_manual = true;
        } else if (strcmp(argv[i], "--devapi") == 0 && i + 1 < argc) {
            options->devapi_requested = true;
            if (!parse_devapi_port(argv[++i], &options->devapi_port)) {
                fprintf(stderr, "invalid --devapi port, expected 1..65535\n");
                return 2;
            }
#else
        } else if (strcmp(argv[i], "--devapi") == 0) {
            fprintf(stderr, "--devapi requires configuring with GAME_DEVAPI_ENABLED=ON\n");
            return 2;
#endif
        }
    }
    // --fresh-state implies --disable-autosave (no opt-out flag exists, none is added):
    // otherwise the 2s debounce would silently overwrite a REAL save on disk with the
    // throwaway fresh session a few seconds later (automation already
    // always passes both, ai_studio/runtime_automation/devapi_client.py running_game()).
    if (options->fresh_state) {
        options->disable_autosave = true;
    }
    return 0;
}

void shell_platform_frame_begin(game_input_frame_t *input) {
    s_frame_begin = nt_time_now();
    nt_window_poll();
    // DevAPI handlers run between frames, where every gfx call (timing toggle included) is allowed.
    devapi_update_frame();
    nt_gfx_begin_frame();
    nt_input_poll();
    game_input_capture(input);
    focus_prompt_ui_update(input);
    if (platform_lifecycle_on_input(input->any_gesture)) {
        game_audio_on_user_gesture();
    }
#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() ||
        (input->escape_pressed && !game_scenes_handle_escape())) {
        nt_app_quit();
    }
#endif
}

void shell_platform_frame_end(shell_shown_t shown, bool playable) {
    if (!shown.presented) {
        return;
    }
    nt_window_swap_buffers();
    if (shown.complete) {
        if (shown.recovered) {
            game_report_gfx_recovery_complete();
        }
        platform_lifecycle_after_frame_present(playable);
        if (playable) game_audio_on_game_ready();
    }
    devapi_sample_metrics(s_frame_begin);
}
