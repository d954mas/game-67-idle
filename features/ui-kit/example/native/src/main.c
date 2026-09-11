/* UI Lab: the studio UI kit in the real renderer. Six scenes, four themes, a
 * command line that can open any of them at any window size, drive a scripted
 * pointer through them and write the framebuffer out, so a review is a set of
 * frames rather than a description. */
#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "core/nt_core.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "log/nt_log.h"
#include "material/nt_material.h"
#include "material/nt_program_ref.h"
#include "memory/nt_mem_scratch.h"
#include "nt_pack_format.h"
#include "renderers/nt_sprite_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui.h"
#include "ui/nt_ui_modal.h"
#include "window/nt_window.h"

#include "features/ui_kit/ui_theme.h"

#include "lab.h"
#include "lab_theme.h"
#include "lab_ui_runtime.h"
#include "ui_lab_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#define SCRATCH_ARENA_SIZE ((size_t)512U * 1024U)
#define TAP_MAX 8
// A scripted tap lands after the first frames settled and the next one waits
// for the previous press to be handled and animated.
#define TAP_FIRST 20
#define TAP_PERIOD 15
#define SHOT_SETTLE 30

static struct {
    const char *scene;
    const char *theme;
    const char *shot;
    int width;
    int height;
    int frames;
    int tap_count;
    float taps[TAP_MAX][2];
    bool hover;
    float hover_x;
    float hover_y;
    float wheel; // notches delivered once at the hover point, before any tap
} s_cli = {.width = 1280, .height = 800};

static nt_program_ref_t s_sprite_program;
static nt_program_ref_t s_text_program;
static nt_program_ref_t s_radial_program;
static nt_material_t s_sprite_material;
static nt_material_t s_text_material;
static nt_material_t s_radial_material;
static nt_resource_t s_atlas;
static nt_resource_t s_atlas_tex;
static nt_resource_t s_font_resource;
static nt_font_t s_font;
static nt_hash32_t s_pack_id;
static uint32_t s_ready_frames;
static bool s_regions_checked;

static void usage(void) {
    (void)printf("ui_lab [--scene hud|upgrade|result|settings|components|themes] [--theme b|forest|ember|night]\n"
                 "       [--size WxH] [--frames N] [--shot file.ppm] [--tap X,Y]... [--hover X,Y] [--wheel N]\n"
                 "Keys: 1-6 scenes, T next theme, Esc closes a sheet or returns to the HUD.\n");
}

static bool parse_pair(const char *text, char sep, float *a, float *b) {
    char *end = NULL;
    *a = strtof(text, &end);
    if (end == text || *end != sep) {
        return false;
    }
    const char *rest = end + 1;
    *b = strtof(rest, &end);
    return end != rest;
}

static bool parse_args(int argc, char *argv[]) {
    // Every option takes one value, so the cursor moves two at a time.
    for (int i = 1; i < argc; i += 2) {
        const char *arg = argv[i];
        const char *value = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (strcmp(arg, "--help") == 0) {
            usage();
            return false;
        }
        if (value == NULL) {
            (void)fprintf(stderr, "%s needs a value\n", arg);
            return false;
        }
        if (strcmp(arg, "--scene") == 0) {
            s_cli.scene = value;
        } else if (strcmp(arg, "--theme") == 0) {
            s_cli.theme = value;
        } else if (strcmp(arg, "--shot") == 0) {
            s_cli.shot = value;
        } else if (strcmp(arg, "--frames") == 0) {
            s_cli.frames = atoi(value);
        } else if (strcmp(arg, "--size") == 0) {
            float w = 0.0F;
            float h = 0.0F;
            if (!parse_pair(value, 'x', &w, &h) || w < 64.0F || h < 64.0F) {
                (void)fprintf(stderr, "--size wants WxH\n");
                return false;
            }
            s_cli.width = (int)w;
            s_cli.height = (int)h;
        } else if (strcmp(arg, "--tap") == 0) {
            if (s_cli.tap_count >= TAP_MAX || !parse_pair(value, ',', &s_cli.taps[s_cli.tap_count][0], &s_cli.taps[s_cli.tap_count][1])) {
                (void)fprintf(stderr, "--tap wants X,Y (at most %d taps)\n", TAP_MAX);
                return false;
            }
            s_cli.tap_count++;
        } else if (strcmp(arg, "--wheel") == 0) {
            s_cli.wheel = strtof(value, NULL);
        } else if (strcmp(arg, "--hover") == 0) {
            if (!parse_pair(value, ',', &s_cli.hover_x, &s_cli.hover_y)) {
                (void)fprintf(stderr, "--hover wants X,Y\n");
                return false;
            }
            s_cli.hover = true;
        } else {
            (void)fprintf(stderr, "unknown option %s\n", arg);
            usage();
            return false;
        }
    }
    return true;
}

// The pack sits beside the executable, which is how bin/assets is laid out.
static void pack_path(char *out, size_t cap) {
    const char *dir_end = NULL;
#ifdef _WIN32
    char *exe = NULL;
    if (_get_pgmptr(&exe) == 0 && exe != NULL) {
        const char *a = strrchr(exe, '\\');
        const char *b = strrchr(exe, '/');
        dir_end = a > b ? a : b;
        if (dir_end != NULL) {
            (void)snprintf(out, cap, "%.*s/%s", (int)(dir_end - exe), exe, UI_LAB_PACK_PATH);
            return;
        }
    }
#endif
    (void)dir_end;
    (void)snprintf(out, cap, "%s", UI_LAB_PACK_PATH);
}

static void link_programs(void) {
    if (nt_program_ref_update(&s_sprite_program)) {
        nt_material_set_program(s_sprite_material, s_sprite_program.program);
    }
    if (nt_program_ref_update(&s_text_program)) {
        nt_material_set_program(s_text_material, s_text_program.program);
    }
    if (nt_program_ref_update(&s_radial_program)) {
        nt_material_set_program(s_radial_material, s_radial_program.program);
    }
}

// The scripted pointer: a hover held from the first frame, and taps that
// press, hold and release on three consecutive frames each.
static void scripted_pointer(nt_pointer_t *p) {
    memset(p, 0, sizeof *p);
    p->type = NT_POINTER_MOUSE;
    if (s_cli.hover) {
        p->active = true;
        p->x = s_cli.hover_x;
        p->y = s_cli.hover_y;
        if (s_cli.wheel != 0.0F && s_ready_frames == TAP_FIRST / 2) {
            p->wheel_dy = s_cli.wheel;
        }
    }
    for (int i = 0; i < s_cli.tap_count; ++i) {
        const int phase = (int)s_ready_frames - (TAP_FIRST + i * TAP_PERIOD);
        if (phase < 0 || phase > 2) {
            continue;
        }
        p->active = true;
        p->x = s_cli.taps[i][0];
        p->y = s_cli.taps[i][1];
        if (phase == 0) {
            p->buttons[NT_BUTTON_LEFT].is_pressed = true;
            p->buttons[NT_BUTTON_LEFT].is_down = true;
        } else if (phase == 1) {
            p->buttons[NT_BUTTON_LEFT].is_down = true;
        } else {
            p->buttons[NT_BUTTON_LEFT].is_released = true;
        }
    }
}

static bool scripted(void) { return s_cli.hover || s_cli.tap_count > 0; }

static uint32_t shot_frame(void) {
    uint32_t frame = (uint32_t)(s_cli.frames > 0 ? s_cli.frames : 60);
    if (s_cli.tap_count > 0) {
        const uint32_t after_taps = (uint32_t)(TAP_FIRST + s_cli.tap_count * TAP_PERIOD + SHOT_SETTLE);
        frame = frame > after_taps ? frame : after_taps;
    }
    return frame;
}

// Binary PPM: no dependency, and the lab tool turns it into PNG.
static bool write_ppm(const char *path) {
    const int w = (int)g_nt_window.fb_width;
    const int h = (int)g_nt_window.fb_height;
    if (w <= 0 || h <= 0) {
        return false;
    }
    const size_t count = (size_t)w * (size_t)h;
    uint8_t *rgba = (uint8_t *)malloc(count * 4U);
    uint8_t *rgb = (uint8_t *)malloc(count * 3U);
    bool ok = rgba != NULL && rgb != NULL && nt_gfx_read_pixels(0, 0, w, h, rgba, (uint32_t)(count * 4U));
    if (ok) {
        for (size_t i = 0; i < count; ++i) {
            rgb[i * 3U] = rgba[i * 4U];
            rgb[i * 3U + 1U] = rgba[i * 4U + 1U];
            rgb[i * 3U + 2U] = rgba[i * 4U + 2U];
        }
        FILE *f = fopen(path, "wb");
        ok = f != NULL;
        if (ok) {
            (void)fprintf(f, "P6\n%d %d\n255\n", w, h);
            ok = fwrite(rgb, 1, count * 3U, f) == count * 3U;
            (void)fclose(f);
        }
    }
    free(rgba);
    free(rgb);
    if (ok) {
        nt_log_info("ui_lab: wrote %s (%dx%d)", path, w, h);
    } else {
        nt_log_error("ui_lab: could not write %s", path);
    }
    return ok;
}

static void handle_keys(nt_ui_context_t *ctx) {
    // A sheet takes Esc first; the engine reports last frame's presence, which
    // is the one-frame gate immediate mode always has.
    if (nt_ui_modal_active(ctx)) {
        return;
    }
    static const nt_key_t SCENE_KEYS[LAB_SCENE_COUNT] = {NT_KEY_1, NT_KEY_2, NT_KEY_3, NT_KEY_4, NT_KEY_5, NT_KEY_6};
    for (int i = 0; i < LAB_SCENE_COUNT; ++i) {
        if (nt_input_key_is_pressed(SCENE_KEYS[i])) {
            lab_goto((lab_scene_id_t)i);
        }
    }
    if (nt_input_key_is_pressed(NT_KEY_T)) {
        lab_request_theme((lab_theme_index() + 1) % lab_theme_count());
    }
    if (nt_input_key_is_pressed(NT_KEY_ESCAPE) && lab_scene() != LAB_SCENE_HUD) {
        lab_goto(LAB_SCENE_HUD);
    }
}

static void frame(void) {
    nt_window_poll();
    nt_input_poll();
    nt_mem_scratch_reset();

    nt_ui_context_t *ctx = lab_ui_runtime_ctx();
    handle_keys(ctx);

    nt_resource_step();
    link_programs();

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        nt_resource_invalidate(NT_ASSET_TEXTURE);
        nt_resource_invalidate(NT_ASSET_FONT);
        (void)nt_sprite_renderer_restore_gpu();
        (void)nt_text_renderer_restore_gpu();
        nt_program_ref_drop(&s_sprite_program);
        nt_program_ref_drop(&s_text_program);
        nt_program_ref_drop(&s_radial_program);
        nt_resource_invalidate(NT_ASSET_SHADER_CODE);
        lab_ui_runtime_restore_gpu();
    }

    const ui_tokens_t *t = ui_theme_tokens();
    nt_gfx_begin_pass(&(nt_pass_desc_t){
        .clear_color = {(float)(t->shell & 0xFFU) / 255.0F, (float)((t->shell >> 8) & 0xFFU) / 255.0F,
                        (float)((t->shell >> 16) & 0xFFU) / 255.0F, 1.0F},
        .clear_depth = 1.0F,
    });
    nt_font_step();

    bool drew = false;
    if (lab_ui_runtime_ready()) {
        if (!s_regions_checked && lab_theme_regions_ready()) {
            s_regions_checked = true;
        }
        lab_update(ctx, g_nt_app.dt);
        nt_pointer_t pointers[NT_INPUT_MAX_POINTERS];
        if (scripted()) {
            memset(pointers, 0, sizeof pointers);
            scripted_pointer(&pointers[0]);
        } else {
            memcpy(pointers, g_nt_input.pointers, sizeof pointers);
        }
        if (lab_ui_runtime_begin(g_nt_app.dt, pointers, NT_INPUT_MAX_POINTERS)) {
            lab_build(ctx);
            lab_ui_runtime_end();
            drew = true;
        }
    }
    nt_gfx_end_pass();
    nt_gfx_end_frame();

    if (drew) {
        s_ready_frames++;
        const bool capture = s_cli.shot != NULL && s_ready_frames == shot_frame();
        const bool done = s_cli.shot == NULL && s_cli.frames > 0 && s_ready_frames >= (uint32_t)s_cli.frames;
        if (capture) {
            (void)write_ppm(s_cli.shot);
        }
        if (capture || done) {
            nt_app_quit();
        }
    }
    nt_window_swap_buffers();
}

int main(int argc, char *argv[]) {
    if (!parse_args(argc, argv)) {
        return 2;
    }

    nt_engine_config_t config = {0};
    config.app_name = "ui_lab";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    g_nt_window.title = "UI Lab";
    g_nt_window.width = (uint32_t)s_cli.width;
    g_nt_window.height = (uint32_t)s_cli.height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    nt_gfx_init(&gfx_desc);
    nt_gfx_register_global_block("Globals", 0);

    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_mem_scratch_init(SCRATCH_ARENA_SIZE);
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture);
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);
    nt_atlas_init();
    nt_material_init(&(nt_material_desc_t){.max_materials = 4});
    nt_font_init(&(nt_font_desc_t){.max_fonts = 1});
    nt_sprite_renderer_desc_t sprite_desc = nt_sprite_renderer_desc_defaults();
    nt_sprite_renderer_init(&sprite_desc);
    nt_text_renderer_init();

    s_pack_id = nt_hash32_str("ui_lab");
    nt_resource_mount(s_pack_id, 100);
    char path[1024];
    pack_path(path, sizeof path);
    nt_resource_load_auto(s_pack_id, path);

    s_sprite_program.vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_VERT, NT_ASSET_SHADER_CODE);
    s_sprite_program.fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_FRAG, NT_ASSET_SHADER_CODE);
    s_text_program.vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_VERT, NT_ASSET_SHADER_CODE);
    s_text_program.fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_FRAG, NT_ASSET_SHADER_CODE);
    s_radial_program.vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_RADIAL_VERT, NT_ASSET_SHADER_CODE);
    s_radial_program.fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_RADIAL_FRAG, NT_ASSET_SHADER_CODE);
    s_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    s_atlas_tex = nt_resource_request(ASSET_TEXTURE_UI_TEX0, NT_ASSET_TEXTURE);
    s_font_resource = nt_resource_request(ASSET_FONT_UI_LAB_FONT, NT_ASSET_FONT);

    // Premultiplied atlas and premultiplied text output: the kit's alpha contract.
    s_sprite_material = nt_material_create(&(nt_material_create_desc_t){
        .textures = {{.name = "u_texture", .resource = s_atlas_tex}},
        .texture_count = 1,
        .blend = nt_blend_alpha_premultiplied(),
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "ui_lab_sprite",
    });
    s_text_material = nt_material_create(&(nt_material_create_desc_t){
        .blend = nt_blend_alpha_premultiplied(),
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .params[0] = {.name = "u_alpha_cutoff", .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
        .param_count = 1,
        .label = "ui_lab_text",
    });
    // The radial SDF: the extended sprite layout the kit draws its discs with.
    s_radial_material = nt_material_create(&(nt_material_create_desc_t){
        .blend = nt_blend_alpha_premultiplied(),
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .attr_map[0] = {.stream_name = "a_radial", .location = 4},
        .attr_map[1] = {.stream_name = "a_layout", .location = 7},
        .attr_map_count = 2,
        .label = "ui_lab_radial",
    });
    s_font = nt_font_create(&(nt_font_create_desc_t){
        .curve_texture_width = 1024,
        .curve_texture_height = 512,
        .band_texture_height = 256,
        .band_count = 8,
        .measure_cache_size = 256,
    });
    nt_font_add(s_font, s_font_resource);
    nt_resource_set_activate_time_budget(0);

    lab_ui_runtime_init(s_sprite_material, s_text_material, s_font, s_font_resource, s_atlas);
    lab_theme_bind(s_atlas, s_radial_material);
    lab_init();
    if (s_cli.theme != NULL && !lab_theme_apply_id(s_cli.theme)) {
        (void)fprintf(stderr, "unknown theme %s\n", s_cli.theme);
        return 2;
    }
    if (s_cli.scene != NULL && !lab_goto_id(s_cli.scene)) {
        (void)fprintf(stderr, "unknown scene %s\n", s_cli.scene);
        return 2;
    }

    g_nt_app.target_dt = 0.0F;
#ifdef NT_PLATFORM_WEB
    nt_platform_web_loading_complete();
#endif
    nt_log_info("ui_lab: %s build (%s)", nt_engine_build_string(), nt_engine_preset_string());
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
    lab_ui_runtime_shutdown();
    nt_text_renderer_shutdown();
    nt_sprite_renderer_shutdown();
    nt_font_destroy(s_font);
    nt_font_shutdown();
    nt_material_destroy(s_sprite_material);
    nt_material_destroy(s_text_material);
    nt_material_destroy(s_radial_material);
    nt_program_ref_drop(&s_sprite_program);
    nt_program_ref_drop(&s_text_program);
    nt_program_ref_drop(&s_radial_program);
    nt_material_shutdown();
    nt_mem_scratch_shutdown();
    // Releases the pins the destroys above queued, so the pack unmounts clean.
    nt_resource_step();
    nt_resource_shutdown();
    nt_fs_shutdown();
    nt_http_shutdown();
    nt_hash_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif
    return 0;
}
