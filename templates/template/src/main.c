// Template entry point — THE CONDUCTOR. It inits the subsystems, runs the frame
// loop (which only CALLS subsystems), and tears down. No game logic lives here.
// A game adds systems (systems/sys_*.c, render/*, ui/*) and registers them in
// frame(); it never grows this file with gameplay rules.
#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "log/nt_log.h"
#include "material/nt_material.h"
#include "memory/nt_mem_scratch.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
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
#endif

#include "features/game_features.h"
#include "features/items/items.h"
#include "loc_strings.gen.h"
#include "features/platform_sdk/platform_sdk.h"
#include "features/platform_sdk/platform_sdk_events.h"
#if defined(__EMSCRIPTEN__)
#include "features/platform_sdk/platform_sdk_web.h"
#endif
#include "features/settings/settings.h"
#include "game_audio.h"
#include "game_input.h"
#include "game_events.h"
#if GAME_EVENTS_LOG_MIRROR
#include "game_events_log_mirror.h"
#endif
#include "game_log.h" /* E4.B: built-in "log" event type (unconditional leaf) */
#if NT_DEVAPI_ENABLED
#include "game_events_devapi.h" /* E3: game.events.tail (tail ring + recorder) */
#include "iteration_proof_devapi.h"
#endif
#if FEATURE_GAME_ANALYTICS
#include "game_analytics.h" /* E4: local analytics NDJSON writer */
#endif
#include "render/capture.h"
#include "render/shader_programs.h"
#include "render/render_mesh.h"
#include "ui/hud.h"
#include "ui/focus_prompt_ui.h"
#include "ui/ui_runtime.h"
#include "world/world.h"
#include "game_save.h"
#include "game_storage.h"
#include "game_state.h"
#include "game_items.h"
#include "game_state_events.gen.h" /* E2: game_ev_register (typed event labels) */
#include "game_asset_paths.h"
#include "platform_lifecycle.h"
#include "settings_state.h"        /* A6: SettingsState + settings_state_fragment (NOT the events header) */
#include "game_scenes.h"
#include "features/scenes/scene_manager_devapi.h"
#include "items_state.h"           /* И2a: ItemsState + items_state_fragment (NOT the events header) */
#include "items_state_events.gen.h" /* И2a: items_ev_register (typed items.txn label, R2: not empty) */
#include "progression_state.h"            /* И3a: ProgressionState + progression_state_fragment (NOT the events header) */
#include "progression_state_events.gen.h" /* И3a: progression_ev_register (typed progression.levelup label) */
#ifdef NT_PLATFORM_WEB
#include "runtime_build_marker.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GAME_ASSET_PACK_PATH
#define GAME_ASSET_PACK_PATH "assets/game.ntpack"
#endif

static nt_hash32_t s_pack_id;
static nt_buffer_t s_frame_ubo;
static nt_material_t s_text_material;
static nt_font_t s_font;
static nt_resource_t s_text_vs, s_text_fs, s_font_resource;
static nt_resource_t s_mesh_vs, s_mesh_fs, s_cube;
static nt_resource_t s_tex_vs, s_tex_fs, s_uv_texture;
static World s_world;
static game_input_frame_t s_input;
/* The template admits every schema-valid save; production games use a measured bound. */
static char s_save_snapshot[GAME_STORAGE_MAX_BYTES + 1U];
static char s_capture_path[260];
static bool s_capture;
static bool s_open_settings_on_start;
static uint64_t s_scene_frame_index;
static bool s_fresh_state;
static bool s_disable_autosave;
static int s_window_width = 1280;
static int s_window_height = 720;
static int s_frame_count;
static bool s_game_runtime_initialized;
static bool s_game_runtime_ready;
static bool s_game_runtime_failed;

#if NT_DEVAPI_ENABLED
#if NT_DEVAPI_ENABLED
/* Bot-only switches. A bot drives this game far faster than a player does, and
   these three are what make that possible; devapi/README.md explains each. */
static bool s_no_vsync;
static bool s_time_manual;
#endif
static bool s_devapi_requested;
static bool s_devapi_running;
static uint16_t s_devapi_port = NT_DEVAPI_DEFAULT_PORT;
#endif

static nt_hash64_t rid(const char *s) { return nt_hash64_str(s); }

#if defined(__EMSCRIPTEN__)
/* clang-format off */
EM_JS(double, game_expected_pack_bytes, (void), {
    var bytes = globalThis.__gameAssetBytes && globalThis.__gameAssetBytes.pack;
    return bytes > 0 ? bytes : 0;
})
/* clang-format on */
#endif

static float initial_pack_loading_progress(void) {
    uint32_t received = 0u;
    uint32_t total = 0u;
    const nt_pack_state_t state = nt_resource_pack_state(s_pack_id);
    nt_resource_pack_progress(s_pack_id, &received, &total);
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

static bool devapi_start(void) {
    if (!s_devapi_requested) {
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
    if (!nt_devapi_net_start(s_devapi_port)) {
        fprintf(stderr, "failed to start DevAPI TCP transport on 127.0.0.1:%u\n", (unsigned)s_devapi_port);
        nt_devapi_shutdown();
        s_devapi_running = false;
        return false;
    }
    fprintf(stderr, "DevAPI listening on 127.0.0.1:%u\n", (unsigned)s_devapi_port);
#endif
    nt_devapi_register_default();
    if (!scene_manager_register_devapi(game_scenes_manager())) {
        fprintf(stderr, "failed to register scene DevAPI\n");
#ifndef NT_PLATFORM_WEB
        nt_devapi_net_stop();
#endif
        nt_devapi_shutdown();
        s_devapi_running = false;
        return false;
    }
    game_iteration_proof_register_devapi();
    game_items_register_devapi();
    game_save_register_devapi(game_runtime_on_devapi_state_change, NULL);
    game_events_register_devapi(); // E3: game.events.tail (+ enables the recorder)
#ifdef NT_DEVAPI_GROUP_UI
    nt_devapi_ui_register_context("hud", ui_runtime_ctx());
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
#ifndef NT_PLATFORM_WEB
    static bool was_connected;
    bool now_connected = nt_devapi_net_has_client();
    if (was_connected && !now_connected) {
        g_nt_app.mode = NT_APP_MODE_RUN;
        g_nt_app.paused = false;
        g_nt_app.pending_steps = 0;
    }
    was_connected = now_connected;
#endif
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
        .draw_calls = nt_gfx_get_frame_draw_calls(),
        .mem_used = mem_used,
        .scratch_hwm = (uint32_t)nt_mem_scratch_high_water_mark(),
        .scratch_used = (uint32_t)nt_mem_scratch_used(),
    };
    nt_metrics_sample(&frame);
#else
    (void)frame_begin;
#endif
}

#ifndef NT_PLATFORM_WEB
static void devapi_shutdown_runtime(void) {
    if (!s_devapi_running) {
        return;
    }
    nt_devapi_net_stop();
    nt_devapi_shutdown();
    s_devapi_running = false;
}
#endif
#else
static bool devapi_start(void) { return true; }
static void devapi_update_frame(void) {}
static void devapi_sample_metrics(double frame_begin) { (void)frame_begin; }
#ifndef NT_PLATFORM_WEB
static void devapi_shutdown_runtime(void) {}
#endif
#endif

static void game_runtime_fail(const char *message, int detail) {
    fprintf(stderr, "%s (%d)\n", message, detail);
    s_game_runtime_failed = true;
    nt_app_quit();
}

static void game_runtime_reset_fragments_without_grants(void) {
    const int fragment_count = game_save_fragment_count();
    for (int i = 0; i < fragment_count; ++i) {
        const GameSaveFragment *fragment = game_save_fragment_at(i);
        if (fragment != NULL && fragment->reset != NULL) {
            fragment->reset();
        }
    }
    game_items_create_defaults(false);
}

static bool game_runtime_apply_pending_new_game(void) {
    const game_save_transition_result_t transition = game_save_apply_pending_new_game();
    if (!transition.state_changed) {
        return false;
    }
    if (!transition.persisted) {
        fprintf(stderr, "new game started, but save will be retried\n");
    }
    s_world.time_seconds = 0.0F;
    s_world.player_x = 0.0F;
    s_world.player_z = 0.0F;
    s_world.player_yaw = 0.0F;
    return true;
}

static void game_runtime_load_state(void) {
    if (!s_fresh_state) {
        game_save_load_result_t load_result;
        game_save_load(&load_result);
        if (load_result.status == GAME_SAVE_LOAD_CORRUPT_RESET) {
            /* load already did reset()+quarantine but NOT on_new_game; this is
               the single on_new_game call on the corrupt-save path. */
            char save_err[128];
            (void)game_save_new_game(save_err, (int)sizeof save_err);
        }
        /* Deliberately NOT reached for NEWER or BLOCKED: those loads leave a
           real session in memory, so redoing the reset would gut it and the
           next autosave would persist the gutted version. */
        return;
    }

    /* --fresh-state skips load and grants, but still creates required game-owned
       containers so every runtime consumer has valid explicit ownership. */
    game_runtime_reset_fragments_without_grants();
}

static void game_runtime_try_start(void) {
    if (s_game_runtime_ready || s_game_runtime_failed) {
        return;
    }

    const nt_pack_state_t pack_state = nt_resource_pack_state(s_pack_id);
    if (pack_state == NT_PACK_STATE_FAILED) {
        game_runtime_fail("game asset pack failed to load", (int)pack_state);
        return;
    }
    if (pack_state != NT_PACK_STATE_READY) {
        return;
    }

    char seed_error[128] = {0};
    if (!game_items_validate_default_seed(seed_error, (int)sizeof seed_error)) {
        game_runtime_fail(seed_error[0] ? seed_error : "items seed contract is incompatible", 0);
        return;
    }

    /* A ready pack is the startup barrier: save reconciliation and every
       feature that reads content run only after this point. */
    game_runtime_load_state();
    /* The persisted language reaches the string table only here: both load
       paths above leave settings_state populated, and every accessor before
       this point renders the corpus fallback. */
    settings_apply_language();
#ifdef NT_PLATFORM_WEB
    /* Do not expose pagehide/visibility flush until live fragments contain
       the loaded save. Fresh-state runs are intentionally non-persistent. */
    if (!s_fresh_state) {
        game_save_install_web_flush();
    }
#endif
    game_features_init(&s_world);
    game_scenes_init(&s_world);
    platform_lifecycle_init();
    s_game_runtime_initialized = true;

    if (!devapi_start()) {
        game_runtime_fail("failed to start game runtime DevAPI", 0);
        return;
    }
    if (s_open_settings_on_start) {
        settings_open();
    }
    s_game_runtime_ready = true;
}

static void game_runtime_update(void) {
    if (!s_game_runtime_ready) {
        game_event_frame_reset();
        return;
    }

    s_world.time_seconds += g_nt_app.dt;
    /* Apply deferred new-game requests before feature updates and rendering. */
    (void)game_runtime_apply_pending_new_game();

    game_scenes_update(g_nt_app.dt, &s_input);
    game_features_update(&s_world, g_nt_app.dt);
    game_events_react_begin();
    do {
        game_features_react(&s_world);
    } while (game_events_react_progressed());
    game_events_set_phase(GAME_EVENT_PHASE_RECORD);
    game_features_record(&s_world);
    if (!s_disable_autosave) {
        game_save_tick();
    }
    game_event_frame_reset();
}

// The frame loop: poll, advance the world, render. Calls subsystems only.
static void frame(void) {
    const double frame_begin = nt_time_now();
    nt_window_poll();
    devapi_update_frame();
    nt_input_poll();
    game_input_capture(&s_input);
    focus_prompt_ui_update(&s_input);
    if (platform_lifecycle_on_input(s_input.any_gesture)) {
        game_audio_on_user_gesture();
    }
#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() ||
        (s_input.escape_pressed && !game_scenes_handle_escape())) {
        nt_app_quit();
    }
#endif
    nt_resource_step();
    nt_material_step();
    shader_programs_update();
    game_runtime_try_start();
    if (s_game_runtime_ready) {
        game_scenes_step(++s_scene_frame_index, g_nt_app.dt);
    }
    const bool playable_shell_ready =
        s_game_runtime_ready && render_mesh_ready(&s_world) && ui_runtime_ready();
    (void)platform_sdk_game_loading_progress(initial_pack_loading_progress());
    platform_lifecycle_update(
        playable_shell_ready, game_scenes_can_process_game_input());
    game_runtime_update();

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        nt_resource_invalidate(NT_ASSET_SHADER_CODE);
        nt_resource_invalidate(NT_ASSET_FONT);
        nt_resource_invalidate(NT_ASSET_MESH);
        nt_resource_invalidate(NT_ASSET_TEXTURE);
        s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
            .type = NT_BUFFER_UNIFORM, .usage = NT_USAGE_DYNAMIC,
            .size = sizeof(nt_frame_uniforms_t), .label = "frame_uniforms"});
        nt_text_renderer_restore_gpu();
        render_mesh_restore_gpu();
        ui_runtime_restore_gpu();
    }

    // The engine's draw-pass gate (devapi render.set_enabled). A bot advancing a
    // run to a world state pays for frames nobody looks at, and the drawing is
    // the larger half of such a tick. The frame still begins, ends and presents
    // with the gate off, so the GL context and the devapi loop are untouched.
    // The UI is immediate mode: while the gate is off no widget exists to be
    // hit, so anything that clicks or photographs must turn rendering back on
    // and let a frame pass.
    if (nt_app_render_enabled()) {
    // render systems read the world
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.50F, 0.75F, 0.96F, 1.0F}, .clear_depth = 1.0F});
    if (game_scenes_should_render_world()) {
        // draw_world stays a direct shell call until render systems become features.
        render_mesh_draw(&s_world, s_frame_ubo);
        hud_draw(s_text_material, s_font_resource, s_font, s_frame_ubo);
    }
    // UI-слой фич: агрегатор владеет ui_runtime-кадром и рисует settings (z-order).
    if (s_game_runtime_ready) {
        game_features_draw_ui(&s_world, &s_input);
    }
    nt_gfx_end_pass();
    }

    // --capture: after a few frames (resources loaded + rendered), grab + quit.
    if (s_game_runtime_ready) {
        s_frame_count += 1;
    }
    if (s_capture && s_game_runtime_ready && s_frame_count >= 10) {
        capture_write_ppm(s_capture_path);
        nt_app_quit();
    }

    nt_gfx_end_frame();
    /* UI may request New Game during draw. Drain it before pagehide can flush
       the previous state; this callback is still one synchronous frame. */
    (void)game_runtime_apply_pending_new_game();
    nt_window_swap_buffers();
    platform_lifecycle_after_frame_present(playable_shell_ready);
    devapi_sample_metrics(frame_begin);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--capture") == 0 && i + 1 < argc) {
            (void)snprintf(s_capture_path, sizeof(s_capture_path), "%s", argv[++i]);
            s_capture = true;
        } else if (strcmp(argv[i], "--settings") == 0) {
            s_open_settings_on_start = true;
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            if (!parse_window_size_arg(argv[++i], &s_window_width, &s_window_height)) {
                fprintf(stderr, "invalid --window-size, expected WIDTHxHEIGHT\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--fresh-state") == 0) {
            s_fresh_state = true; /* skip load, start from reset defaults */
        } else if (strcmp(argv[i], "--disable-autosave") == 0) {
            s_disable_autosave = true; /* keep loading, but never autosave */
#if NT_DEVAPI_ENABLED
        } else if (strcmp(argv[i], "--no-vsync") == 0) {
            /* A window launched from an agent shell never becomes the
               foreground one, so with vsync on it presents at ~166 ms a frame
               and every bot crawls. Also: vsync pins each frame sample to the
               refresh interval, hiding the renderer cost a perf run compares. */
            s_no_vsync = true;
        } else if (strcmp(argv[i], "--time-manual") == 0) {
            /* Lockstep from frame zero. A bot that photographs the game needs
               the run's clock to be a property of the build, not of how long a
               pack took to load: taking manual time over the devapi is always
               too late, because real frames have already advanced the world by
               a dt nobody chose, and a screenshot is a photograph of the
               animation phase that clock landed on. */
            s_time_manual = true;
        } else if (strcmp(argv[i], "--devapi") == 0 && i + 1 < argc) {
            s_devapi_requested = true;
            if (!parse_devapi_port(argv[++i], &s_devapi_port)) {
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
    if (s_fresh_state) {
        s_disable_autosave = true;
    }
#ifdef NT_PLATFORM_WEB
    runtime_build_marker_publish();
#endif
    nt_engine_config_t config = {0};
    config.app_name = "Template";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

#ifndef GAME_WINDOW_TITLE
#define GAME_WINDOW_TITLE "Template" // CMake sets "<GAME_TITLE> [AI]" for DevAPI builds
#endif
    g_nt_window.title = GAME_WINDOW_TITLE;
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
#if NT_DEVAPI_ENABLED
    if (s_no_vsync) {
        nt_window_set_vsync(NT_VSYNC_OFF);
    }
#endif
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    gfx_desc.max_buffers = 256;
    nt_gfx_init(&gfx_desc);
    nt_gfx_register_global_block("Globals", 0);

    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    game_events_init(); // type-hashes/labels need hash init; arena is gfx-independent
    game_ev_register(); // register typed-event debug labels (effect under NT_HASH_LABELS, E3)
    items_ev_register(); // И2a: register items.txn debug label (mirrors game_ev_register)
    progression_ev_register(); // И3a: register progression.levelup debug label (mirrors items_ev_register)
#if NT_DEVAPI_ENABLED
    game_events_devapi_register_descs(game_ev_descs, game_ev_desc_count); // E3: tail descriptors
    game_events_devapi_register_descs(items_ev_descs, items_ev_desc_count); // И2a: items.txn tail descriptor
    game_events_devapi_register_descs(progression_ev_descs, progression_ev_desc_count); // И3a: progression.levelup tail descriptor
#endif
#if FEATURE_GAME_EVENTS
    platform_sdk_events_register(); // platform SDK lifecycle/ad-flow labels
#endif
    game_log_register(); // E4.B: debug label "log" (UNCONDITIONAL; game_log.c is a leaf)
#if NT_DEVAPI_ENABLED
#if FEATURE_GAME_EVENTS
    game_events_devapi_register_descs(platform_sdk_ev_descs, platform_sdk_ev_desc_count); // SDK event descriptors
#endif
    game_events_devapi_register_descs(game_log_descs, game_log_desc_count); // E3 tail: log type (append)
#endif
#if FEATURE_GAME_ANALYTICS
    game_analytics_register_descs(game_ev_descs, game_ev_desc_count);   // E4: fragment descs (append)
    game_analytics_register_descs(items_ev_descs, items_ev_desc_count); // И2a: items.txn (append)
    game_analytics_register_descs(progression_ev_descs, progression_ev_desc_count); // И3a: progression.levelup (append)
#if FEATURE_GAME_EVENTS
    game_analytics_register_descs(platform_sdk_ev_descs, platform_sdk_ev_desc_count); // SDK events
#endif
    game_analytics_register_descs(game_log_descs, game_log_desc_count); // E4: log type (append)
    game_analytics_init();                                             // E4: open stream + header
#endif
#if GAME_EVENTS_LOG_MIRROR
    game_events_log_mirror_register_descs(game_ev_descs, game_ev_desc_count);
    game_events_log_mirror_register_descs(items_ev_descs, items_ev_desc_count);
    game_events_log_mirror_register_descs(progression_ev_descs, progression_ev_desc_count);
#if FEATURE_GAME_EVENTS
    game_events_log_mirror_register_descs(platform_sdk_ev_descs, platform_sdk_ev_desc_count);
#endif
    game_events_log_mirror_register_descs(game_log_descs, game_log_desc_count);
#endif
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);
    nt_resource_set_activator(NT_ASSET_MESH, nt_gfx_activate_mesh, nt_gfx_deactivate_mesh);
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture); // mesh texture + UI atlas page
    nt_mem_scratch_init((size_t)512U * 1024U); // per-frame arena nt_ui builds element data from
    loc_init(); // after the scratch arena: formatted strings allocate from it

    nt_font_init(&(nt_font_desc_t){.max_fonts = 2});
    nt_material_init(&(nt_material_desc_t){.max_materials = 8});
    nt_text_renderer_init();
    game_save_register_fragment(&settings_state_fragment); /* settings first */
    game_save_register_fragment(&items_state_fragment);    /* L1, no deps: between settings and game */
    game_save_register_fragment(&progression_state_fragment); /* L2 depends on items: register after items */
    game_save_register_fragment(&game_state_fragment);     /* `game` last (most dependent) */
    game_items_configure_save();
    game_save_set_hot_snapshot_buffer(s_save_snapshot, sizeof s_save_snapshot);
    game_save_init();

    /* The loading bar is the pack download: the backend must be able to carry
       progress before the pack starts, not after it is ready. SDK init still
       waits for the runtime; installing a backend only wires the callbacks. */
#if defined(__EMSCRIPTEN__)
    platform_sdk_install_web_backend();
#endif

    s_pack_id = nt_hash32_str("game");
    nt_resource_mount(s_pack_id, 100);
    char pack_path[GAME_ASSET_PATH_MAX];
    if (!game_asset_paths_resolve(GAME_ASSET_PACK_PATH, pack_path, sizeof pack_path)) {
        fprintf(stderr, "game asset pack path does not fit\n");
        return 1;
    }
    nt_resource_load_auto(s_pack_id, pack_path);
    nt_resource_set_activate_time_budget(0);

    s_text_vs = nt_resource_request(rid("assets/shaders/slug_text.vert"), NT_ASSET_SHADER_CODE);
    s_text_fs = nt_resource_request(rid("assets/shaders/slug_text.frag"), NT_ASSET_SHADER_CODE);
    s_font_resource = nt_resource_request(rid("game/font"), NT_ASSET_FONT);

    s_text_material = shader_program_material(s_text_vs, s_text_fs, &(nt_material_create_desc_t){
        .blend = nt_blend_alpha(),
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .params[0] = {.name = "u_alpha_cutoff", .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
        .param_count = 1,
        .label = "text",
    });
    s_font = nt_font_create(&(nt_font_create_desc_t){
        .curve_texture_width = 1024,
        .curve_texture_height = 512,
        .band_texture_height = 256,
        .band_count = 8,
        .measure_cache_size = 256,
    });
    nt_font_add(s_font, s_font_resource);

    s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM, .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t), .label = "frame_uniforms"});

    // mesh render system + two sample cubes: a coloured one (mesh_inst) and a
    // textured one (mesh_tex, samples the uv_grid texture). Same cube mesh feeds both.
    s_mesh_vs = nt_resource_request(rid("assets/shaders/mesh_inst.vert"), NT_ASSET_SHADER_CODE);
    s_mesh_fs = nt_resource_request(rid("assets/shaders/mesh_inst.frag"), NT_ASSET_SHADER_CODE);
    s_tex_vs = nt_resource_request(rid("assets/shaders/mesh_tex.vert"), NT_ASSET_SHADER_CODE);
    s_tex_fs = nt_resource_request(rid("assets/shaders/mesh_tex.frag"), NT_ASSET_SHADER_CODE);
    s_uv_texture = nt_resource_request(rid("assets/textures/uv_grid"), NT_ASSET_TEXTURE);
    s_cube = nt_resource_request(rid("assets/meshes/cube.glb"), NT_ASSET_MESH);
    render_mesh_init(s_mesh_vs, s_mesh_fs, s_tex_vs, s_tex_fs, s_uv_texture);
    render_mesh_spawn_player(&s_world, s_cube, (float[4]){0.16F, 0.78F, 0.30F, 1.0F});
    render_mesh_spawn_prop(&s_world, s_cube);

    // engine UI stack (sprite renderer + slice9 atlas + nt_ui ctx); reuses the font + text material
    ui_runtime_init(s_text_material, s_font, s_font_resource);

    g_nt_app.target_dt = 1.0F / 60.0F;
#if NT_DEVAPI_ENABLED
    if (s_time_manual) {
        nt_app_set_mode(NT_APP_MODE_MANUAL);
    }
#endif
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
    devapi_shutdown_runtime();
    if (s_game_runtime_initialized) {
        game_scenes_shutdown();
        nt_resource_step();
        game_features_shutdown(&s_world);
        platform_lifecycle_shutdown();
    }
    ui_runtime_shutdown();
#if FEATURE_GAME_ANALYTICS
    game_analytics_shutdown(); // E4: final flush + close (before event infra teardown)
#endif
    game_events_shutdown();
    nt_mem_scratch_shutdown();
    nt_text_renderer_shutdown();
    nt_font_destroy(s_font);
    nt_font_shutdown();
    nt_material_destroy(s_text_material);
    shader_programs_shutdown();
    nt_material_shutdown();
    nt_resource_shutdown();
    nt_fs_shutdown();
    nt_http_shutdown();
    nt_hash_shutdown();
    nt_gfx_destroy_buffer(s_frame_ubo);
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif
    return 0;
}
