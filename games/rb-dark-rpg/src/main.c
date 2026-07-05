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

#include "render/capture.h"
#include "game_audio.h"
#include "render/render_2d_runtime.h"
#include "render/render_hub_scene.h"
#include "render/render_mesh.h"
#include "systems/sys_scene_interactions.h"
#include "systems/sys_scene_pan.h"
#include "systems/sys_move.h"
#include "systems/sys_settings.h"
#include "ui/dialogue_panel.h"
#include "ui/end_screen.h"
#include "ui/first_screen_hud.h"
#include "ui/hud.h"
#include "ui/ui_runtime.h"
#include "world/world.h"
#if FEATURE_GAME_STATE
#include "game_persistence.h"
#include "game_state.h"
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
static char s_capture_path[260];
static bool s_capture;
static bool s_open_settings_on_start;
static int s_window_width = 1280;
static int s_window_height = 720;
static int s_frame_count;
#if FEATURE_GAME_STATE
static bool s_fresh_state;
static bool s_autosave_enabled = true;
static bool s_autosave_error_reported;
#endif

#if NT_DEVAPI_ENABLED
static bool s_devapi_requested;
static bool s_devapi_running;
static uint16_t s_devapi_port = NT_DEVAPI_DEFAULT_PORT;
#endif

static nt_hash64_t rid(const char *s) { return nt_hash64_str(s); }

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
#if FEATURE_GAME_STATE
    game_state_register_devapi();
#endif
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

static void devapi_shutdown_runtime(void) {
    if (!s_devapi_running) {
        return;
    }
#ifndef NT_PLATFORM_WEB
    nt_devapi_net_stop();
#endif
    nt_devapi_shutdown();
    s_devapi_running = false;
}
#else
static bool devapi_start(void) { return true; }
static void devapi_update_frame(void) {}
static void devapi_sample_metrics(double frame_begin) { (void)frame_begin; }
#ifndef NT_PLATFORM_WEB
static void devapi_shutdown_runtime(void) {}
#endif
#endif

#if FEATURE_GAME_STATE
static void autosave_update(void) {
    if (!s_autosave_enabled || !game_state_is_dirty()) {
        return;
    }
    char error[256];
    if (!game_persistence_save_autosave_if_dirty(error, (int)sizeof(error))) {
        if (!s_autosave_error_reported) {
            fprintf(stderr, "autosave failed: %s\n", error);
            s_autosave_error_reported = true;
        }
        return;
    }
    s_autosave_error_reported = false;
}
#endif

// The frame loop: poll, advance the world, render. Calls subsystems only.
static void frame(void) {
    const double frame_begin = nt_time_now();
    nt_window_poll();
    devapi_update_frame();
    nt_input_poll();
#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif
    nt_resource_step();
    nt_material_step();
    game_audio_set_volume(sys_settings_master(), sys_settings_music(), sys_settings_sfx());
    game_audio_update();
    s_world.time_seconds += g_nt_app.dt;

    // game systems update the world (settings logic now lives in its nt_ui build)
    sys_move(&s_world, g_nt_app.dt);
    sys_scene_interactions_update(&s_world);
    sys_scene_pan_update(&s_world);

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
        render_2d_runtime_restore_gpu();
        render_mesh_restore_gpu();
        ui_runtime_restore_gpu();
    }

    // render systems read the world
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.035F, 0.043F, 0.060F, 1.0F}, .clear_depth = 1.0F});
    render_hub_scene_draw(&s_world, s_frame_ubo);

    // settings UI: real nt_ui widgets (panel + sliders + buttons), built + walked here
    if (ui_runtime_begin(g_nt_app.dt)) {
        first_screen_hud_ui(ui_runtime_ctx(), &s_world);
        dialogue_panel_ui(ui_runtime_ctx(), &s_world);
        end_screen_ui(ui_runtime_ctx(), &s_world);
        sys_settings_ui(ui_runtime_ctx(), &s_world);
        ui_runtime_end();
    }
#if FEATURE_GAME_STATE
    autosave_update();
#endif
    nt_gfx_end_pass();

    // --capture: after a few frames (resources loaded + rendered), grab + quit.
    s_frame_count += 1;
    if (s_capture && s_frame_count >= 10) {
        capture_write_ppm(s_capture_path);
        nt_app_quit();
    }

    nt_gfx_end_frame();
    nt_window_swap_buffers();
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
#if FEATURE_GAME_STATE
            s_fresh_state = true;
            s_autosave_enabled = false;
#endif
        } else if (strcmp(argv[i], "--disable-autosave") == 0) {
#if FEATURE_GAME_STATE
            s_autosave_enabled = false;
#endif
#if NT_DEVAPI_ENABLED
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
    nt_engine_config_t config = {0};
    config.app_name = "RB Dark RPG";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    g_nt_window.title = "RB Dark RPG";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    gfx_desc.max_buffers = 256;
    nt_gfx_init(&gfx_desc);
    nt_gfx_register_global_block("Globals", 0);

    nt_http_init();
    nt_fs_init();
    game_audio_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);
    nt_resource_set_activator(NT_ASSET_MESH, nt_gfx_activate_mesh, nt_gfx_deactivate_mesh);
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture); // mesh texture + UI atlas page
    nt_mem_scratch_init((size_t)512U * 1024U); // per-frame arena nt_ui builds element data from

    nt_font_init(&(nt_font_desc_t){.max_fonts = 2});
    nt_material_init(&(nt_material_desc_t){.max_materials = 8});
    nt_text_renderer_init();
    render_2d_runtime_init();
#if FEATURE_GAME_STATE
    game_state_init();
    char state_error[256];
    if (!game_persistence_load_autosave(s_fresh_state, state_error, (int)sizeof(state_error))) {
        fprintf(stderr, "failed to load autosave: %s\n", state_error);
    }
    s_world.player_state = &g_game_state;
#endif

    s_pack_id = nt_hash32_str("game");
    nt_resource_mount(s_pack_id, 100);
    nt_resource_load_auto(s_pack_id, GAME_ASSET_PACK_PATH);
    nt_resource_set_activate_time_budget(0);

    s_text_vs = nt_resource_request(rid("assets/shaders/slug_text.vert"), NT_ASSET_SHADER_CODE);
    s_text_fs = nt_resource_request(rid("assets/shaders/slug_text.frag"), NT_ASSET_SHADER_CODE);
    s_font_resource = nt_resource_request(rid("game/font"), NT_ASSET_FONT);

    s_text_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_text_vs,
        .fs = s_text_fs,
        .blend_mode = NT_BLEND_MODE_ALPHA,
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
    render_hub_scene_init();
    ui_runtime_init(s_text_material, s_font, s_font_resource);
    game_audio_set_volume(sys_settings_master(), sys_settings_music(), sys_settings_sfx());
    game_audio_start_music();

    if (!devapi_start()) {
        return 1;
    }

    if (s_open_settings_on_start) {
        sys_settings_force_open();
    }

    g_nt_app.target_dt = 1.0F / 60.0F;
    nt_app_run(frame);

#if FEATURE_GAME_STATE
    autosave_update();
#endif

#ifndef NT_PLATFORM_WEB
    devapi_shutdown_runtime();
    render_hub_scene_shutdown();
    ui_runtime_shutdown();
    render_2d_runtime_shutdown();
    game_audio_shutdown();
    nt_mem_scratch_shutdown();
    nt_text_renderer_shutdown();
    nt_font_destroy(s_font);
    nt_font_shutdown();
    nt_material_destroy(s_text_material);
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
