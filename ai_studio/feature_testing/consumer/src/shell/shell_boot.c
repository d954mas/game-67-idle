#include "shell/shell.h"

#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "color/nt_color.h"
#include "core/nt_core.h"
#ifndef NT_PLATFORM_WEB
#include "fs/nt_fs.h"
#endif
#include "graphics/nt_gfx.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "memory/nt_mem_scratch.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_text_renderer.h"
#include "window/nt_window.h"

#include "features/platform_sdk/platform_sdk_events.h"
#if defined(__EMSCRIPTEN__)
#include "features/platform_sdk/platform_sdk_web.h"
#endif
#include "game_events.h"
#if GAME_EVENTS_LOG_MIRROR
#include "game_events_log_mirror.h"
#endif
#if NT_DEVAPI_ENABLED
#include "game_events_devapi.h"
#endif
#if FEATURE_GAME_ANALYTICS
#include "game_analytics.h"
#endif
#include "game_asset_paths.h"
#include "game_log.h"
#include "game_save.h"
#include "game_save_policy.h"
#include "game_state.h"
#include "game_state_events.gen.h"
#include "game_storage.h"
#include "items_state.h"
#include "items_state_events.gen.h"
#include "loc_strings.gen.h"
#include "progression_state.h"
#include "progression_state_events.gen.h"
#include "render/render_mesh.h"
#include "render/shader_programs.h"
#include "settings_state.h"
#include "systems/sys_cloud_save.h"
#include "ui/ui_runtime.h"

#include <stdio.h>

#ifndef GAME_ASSET_PACK_PATH
#define GAME_ASSET_PACK_PATH "assets/game.ntpack"
#endif

// UI quads and glyphs one frame can record. Vertex = quads x 4 x 20 B + glyphs x 4 x 48 B + mesh instances x 52 B;
// index = (quads + glyphs) x 6 x 4 B; uniform = view-block binds per frame (mesh, HUD, UI) x (sizeof block + 256).
#define FRAME_QUADS 2048U
#define FRAME_GLYPHS 2048U
#define FRAME_MESH_INSTANCES 16U
#define FRAME_VERTEX_BYTES (FRAME_QUADS * 4U * 20U + FRAME_GLYPHS * 4U * 48U + FRAME_MESH_INSTANCES * 52U)
#define FRAME_INDEX_BYTES ((FRAME_QUADS + FRAME_GLYPHS) * 6U * 4U)
#define FRAME_UNIFORM_BYTES (3U * ((uint32_t)sizeof(nt_frame_uniforms_t) + 256U))

static nt_hash32_t s_pack_id;
static shell_text_t s_text;
static nt_resource_t s_text_vs, s_text_fs;
static nt_resource_t s_mesh_vs, s_mesh_fs, s_cube;
static nt_resource_t s_tex_vs, s_tex_fs, s_uv_texture;
/* The template admits every schema-valid save; production games use a measured bound. */
static char s_save_snapshot[GAME_STORAGE_MAX_BYTES + 1U];

static nt_hash64_t rid(const char *s) { return nt_hash64_str(s); }

nt_hash32_t shell_boot_pack_id(void) { return s_pack_id; }

const shell_text_t *shell_boot_text(void) { return &s_text; }

/* Debug labels, the DevAPI tail, analytics and the log mirror each read the
   same descriptor sets; every set registers after hash init. */
static void register_events(void) {
    game_events_init(); // type-hashes/labels need hash init; arena is gfx-independent
    game_ev_register();
    items_ev_register();
    progression_ev_register();
#if NT_DEVAPI_ENABLED
    game_events_devapi_register_descs(game_ev_descs, game_ev_desc_count);
    game_events_devapi_register_descs(items_ev_descs, items_ev_desc_count);
    game_events_devapi_register_descs(progression_ev_descs, progression_ev_desc_count);
#endif
#if FEATURE_GAME_EVENTS
    platform_sdk_events_register();
#endif
    game_log_register(); // unconditional: game_log.c is a leaf every build links
#if NT_DEVAPI_ENABLED
#if FEATURE_GAME_EVENTS
    game_events_devapi_register_descs(platform_sdk_ev_descs, platform_sdk_ev_desc_count);
#endif
    game_events_devapi_register_descs(game_log_descs, game_log_desc_count);
#endif
#if FEATURE_GAME_ANALYTICS
    game_analytics_register_descs(game_ev_descs, game_ev_desc_count);
    game_analytics_register_descs(items_ev_descs, items_ev_desc_count);
    game_analytics_register_descs(progression_ev_descs, progression_ev_desc_count);
#if FEATURE_GAME_EVENTS
    game_analytics_register_descs(platform_sdk_ev_descs, platform_sdk_ev_desc_count);
#endif
    game_analytics_register_descs(game_log_descs, game_log_desc_count);
    game_analytics_init(); // opens the stream once every set is known
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
}

static void register_saves(void) {
    game_save_register_fragment(&settings_state_fragment); /* settings first */
    game_save_register_fragment(&items_state_fragment);    /* no deps: between settings and game */
    game_save_register_fragment(&progression_state_fragment); /* depends on items: register after items */
    game_save_register_fragment(&game_state_fragment);     /* `game` last (most dependent) */
    game_configure_save();
    game_save_set_hot_snapshot_buffer(s_save_snapshot, sizeof s_save_snapshot);
    game_save_init();
    cloud_save_init(game_save_policy_decide, game_save_policy_same_features, 0.0);
}

bool shell_boot_init(const shell_options_t *options, World *world) {
    g_nt_window.width = (uint32_t)options->window_width;
    g_nt_window.height = (uint32_t)options->window_height;
    nt_window_init();
    if (options->no_vsync) {
        nt_window_set_vsync(NT_VSYNC_OFF);
    }
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    gfx_desc.max_buffers = 256;
#ifdef NT_DEVAPI_GROUP_RECORDER
    gfx_desc.max_buffers += 5;
#endif
    gfx_desc.frame_capacity[NT_GFX_FRAME_VERTEX] = FRAME_VERTEX_BYTES;
    gfx_desc.frame_capacity[NT_GFX_FRAME_INDEX] = FRAME_INDEX_BYTES;
    gfx_desc.frame_capacity[NT_GFX_FRAME_UNIFORM] = FRAME_UNIFORM_BYTES;
    nt_gfx_init(&gfx_desc);

    nt_http_init();
#ifndef NT_PLATFORM_WEB
    nt_fs_init();
#endif
    nt_hash_init(&(nt_hash_desc_t){0});
    register_events();
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_register_type(NT_ASSET_SHADER_CODE, &(nt_resource_type_desc_t){
        .activate = nt_gfx_activate_shader, .deactivate = nt_gfx_deactivate_shader});
    nt_resource_register_type(NT_ASSET_MESH, &(nt_resource_type_desc_t){
        .activate = nt_gfx_activate_mesh, .deactivate = nt_gfx_deactivate_mesh});
    nt_resource_register_type(NT_ASSET_TEXTURE, &(nt_resource_type_desc_t){
        .activate = nt_gfx_activate_texture, .deactivate = nt_gfx_deactivate_texture}); // mesh texture + UI atlas page
    nt_atlas_init(); // resource types register before the first mount
    nt_mem_scratch_init((size_t)512U * 1024U); // per-frame arena nt_ui builds element data from
    loc_init(); // after the scratch arena: formatted strings allocate from it

    nt_font_init(&(nt_font_desc_t){.max_fonts = 2});
    nt_material_init(&(nt_material_desc_t){.max_materials = 8});
    register_saves();

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
        return false;
    }
    nt_resource_load_auto(s_pack_id, pack_path);
    nt_resource_set_activate_time_budget(0);

    s_text_vs = nt_resource_request(rid("assets/shaders/slug_text.vert"), NT_ASSET_SHADER_CODE);
    s_text_fs = nt_resource_request(rid("assets/shaders/slug_text.frag"), NT_ASSET_SHADER_CODE);
    s_text.font_resource = nt_resource_request(rid("game/font"), NT_ASSET_FONT);

    s_text.material = shader_program_material(s_text_vs, s_text_fs, &(nt_material_create_desc_t){
        .blend = nt_blend_alpha_premultiplied(),
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "text",
    });
    s_text.font = nt_font_create(&(nt_font_create_desc_t){
        .max_glyphs = 256,
        .measure_cache_size = 256,
    });
    nt_font_add(s_text.font, s_text.font_resource);

    // mesh render system + two sample cubes: a coloured one (mesh_inst) and a
    // textured one (mesh_tex, samples the uv_grid texture). Same cube mesh feeds both.
    s_mesh_vs = nt_resource_request(rid("assets/shaders/mesh_inst.vert"), NT_ASSET_SHADER_CODE);
    s_mesh_fs = nt_resource_request(rid("assets/shaders/mesh_inst.frag"), NT_ASSET_SHADER_CODE);
    s_tex_vs = nt_resource_request(rid("assets/shaders/mesh_tex.vert"), NT_ASSET_SHADER_CODE);
    s_tex_fs = nt_resource_request(rid("assets/shaders/mesh_tex.frag"), NT_ASSET_SHADER_CODE);
    s_uv_texture = nt_resource_request(rid("assets/textures/uv_grid"), NT_ASSET_TEXTURE);
    s_cube = nt_resource_request(rid("assets/meshes/cube.glb"), NT_ASSET_MESH);
    render_mesh_init(s_mesh_vs, s_mesh_fs, s_tex_vs, s_tex_fs, s_uv_texture);
    render_mesh_spawn_player(world, s_cube, NT_RGBA8(41, 199, 77, 255));
    render_mesh_spawn_prop(world, s_cube);

    // engine UI stack (sprite renderer + slice9 atlas + nt_ui ctx); reuses the font + text material
    ui_runtime_init(s_text.material, s_text.font, s_text.font_resource);

    g_nt_app.target_dt = 1.0F / 60.0F;
    if (options->time_manual) {
        nt_app_set_mode(NT_APP_MODE_MANUAL);
    }
    return true;
}

/* Native only: the web runtime never returns from nt_app_run. */
void shell_boot_shutdown(void) {
    ui_runtime_shutdown();
#if FEATURE_GAME_ANALYTICS
    game_analytics_shutdown(); // final flush and close, before the event infrastructure goes
#endif
    game_events_shutdown();
    nt_mem_scratch_shutdown();
    nt_text_renderer_shutdown();
    nt_font_destroy(s_text.font);
    nt_font_shutdown();
    nt_material_destroy(s_text.material);
    shader_programs_shutdown();
    nt_material_shutdown();
    nt_resource_shutdown();
#ifndef NT_PLATFORM_WEB
    nt_fs_shutdown();
#endif
    nt_http_shutdown();
    nt_hash_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
}
