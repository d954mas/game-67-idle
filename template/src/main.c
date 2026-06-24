// Template entry point — THE CONDUCTOR. It inits the subsystems, runs the frame
// loop (which only CALLS subsystems), and tears down. No game logic lives here.
// A game adds systems (systems/sys_*.c, render/*, ui/*) and registers them in
// frame(); it never grows this file with gameplay rules.
#include "app/nt_app.h"
#include "core/nt_core.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "log/nt_log.h"
#include "material/nt_material.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "time/nt_time.h"
#include "window/nt_window.h"

#include "ui/hud.h"
#include "world/world.h"

#include <stdio.h>

#ifndef GAME_ASSET_PACK_PATH
#define GAME_ASSET_PACK_PATH "assets/game.ntpack"
#endif

static nt_hash32_t s_pack_id;
static nt_buffer_t s_frame_ubo;
static nt_material_t s_text_material;
static nt_font_t s_font;
static nt_resource_t s_text_vs, s_text_fs, s_font_resource;
static World s_world;

static nt_hash64_t rid(const char *s) { return nt_hash64_str(s); }

// The frame loop: poll, advance the world, render. Calls subsystems only.
static void frame(void) {
    nt_window_poll();
    nt_input_poll();
#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif
    nt_resource_step();
    nt_material_step();
    s_world.time_seconds += g_nt_app.dt;

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        nt_resource_invalidate(NT_ASSET_SHADER_CODE);
        nt_resource_invalidate(NT_ASSET_FONT);
        s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
            .type = NT_BUFFER_UNIFORM, .usage = NT_USAGE_DYNAMIC,
            .size = sizeof(nt_frame_uniforms_t), .label = "frame_uniforms"});
        nt_text_renderer_restore_gpu();
    }

    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.50F, 0.75F, 0.96F, 1.0F}, .clear_depth = 1.0F});
    hud_draw(s_text_material, s_font_resource, s_font, s_frame_ubo);
    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    nt_engine_config_t config = {0};
    config.app_name = "Template";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    g_nt_window.title = "Template";
    g_nt_window.width = 1280;
    g_nt_window.height = 720;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    gfx_desc.max_buffers = 256;
    nt_gfx_init(&gfx_desc);
    nt_gfx_register_global_block("Globals", 0);

    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);

    nt_font_init(&(nt_font_desc_t){.max_fonts = 2});
    nt_material_init(&(nt_material_desc_t){.max_materials = 8});
    nt_text_renderer_init();

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

    g_nt_app.target_dt = 1.0F / 60.0F;
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
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
