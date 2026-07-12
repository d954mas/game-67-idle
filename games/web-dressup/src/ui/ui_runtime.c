#include "ui/ui_runtime.h"

#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "core/nt_assert.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "input/nt_input.h"
#include "material/nt_material.h"
#include "math/nt_math.h"
#include "memory/nt_mem_scratch.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_sprite_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "ui/nt_ui_scale.h"
#include "window/nt_window.h"

#include "generated/game_assets.h"
#include "ui/theme.h"

#include <string.h>

// Portrait stays phone-first so touch targets remain readable under EXPAND;
// landscape uses a wider game canvas.
#define UI_LANDSCAPE_REF_W 960.0F
#define UI_LANDSCAPE_REF_H 540.0F
#define UI_PORTRAIT_REF_W 390.0F
#define UI_PORTRAIT_REF_H 844.0F
// Headroom for the default 1024-element UI (Clay arena dominates); a game that
// builds a denser UI can raise this. Static storage — cost is negligible.
#define UI_ARENA_SIZE ((size_t)8U * 1024U * 1024U) // 8MB: fits max_elements=4096 + state_slots=1024 (jam-proven scale)

static NT_UI_DECLARE_ARENA(s_ui_arena, UI_ARENA_SIZE);
static nt_ui_context_t *s_ctx;
static nt_buffer_t s_ui_ubo;
static nt_ui_scale_t s_scale;

static nt_resource_t s_sprite_vs, s_sprite_fs, s_atlas, s_atlas_tex;
static nt_material_t s_sprite_material;
static nt_material_t s_text_material; // owned by main; reused for UI text
static nt_font_t s_font;              // owned by main
static nt_resource_t s_font_resource; // owned by main
static bool s_atlas_bound, s_font_bound;

static nt_hash64_t rid(const char *s) { return nt_hash64_str(s); }

static nt_buffer_t make_ubo(void) {
    return nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM, .usage = NT_USAGE_DYNAMIC, .size = sizeof(nt_frame_uniforms_t), .label = "ui_frame_uniforms"});
}

void ui_runtime_init(nt_material_t text_material, nt_font_t font, nt_resource_t font_resource) {
    s_text_material = text_material;
    s_font = font;
    s_font_resource = font_resource;

    // Atlas resource activator (the NT_ASSET_TEXTURE activator is set in main, shared
    // with the mesh texture path).
    nt_atlas_init();

    nt_sprite_renderer_desc_t sr = nt_sprite_renderer_desc_defaults();
    nt_sprite_renderer_init(&sr);

    nt_ui_module_init();
    nt_ui_create_desc_t desc = nt_ui_create_desc_defaults();
    // Engine defaults (256 state slots / probe 4) are sample-sized; a real game with
    // several screens sharing one context exhausts the non-evicting state pool and
    // trips the fail-fast assert (VibeJam #1). Also clear states on screen close
    // (nt_ui_state_clear_all) instead of only raising these.
    desc.max_elements = 4096U;
    desc.state_slots = 1024U;
    desc.state_probe_max = 16U;
    s_ctx = nt_ui_create_context(s_ui_arena, sizeof s_ui_arena, &desc);
    NT_ASSERT(s_ctx != NULL && "ui_runtime: failed to create UI context");

    s_sprite_vs = nt_resource_request(rid("assets/shaders/sprite.vert"), NT_ASSET_SHADER_CODE);
    s_sprite_fs = nt_resource_request(rid("assets/shaders/sprite.frag"), NT_ASSET_SHADER_CODE);
    s_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    s_atlas_tex = nt_resource_request(ASSET_TEXTURE_UI_TEX0, NT_ASSET_TEXTURE);

    s_sprite_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_sprite_vs,
        .fs = s_sprite_fs,
        .textures = {{.name = "u_texture", .resource = s_atlas_tex}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "ui_sprite",
    });
    nt_ui_set_sprite_material(s_ctx, s_sprite_material);
    nt_ui_set_text_material(s_ctx, s_text_material);

    s_ui_ubo = make_ubo();
    theme_init(s_atlas);
}

static void resolve_runtime_bindings(void) {
    if (!s_atlas_bound && nt_resource_is_ready(s_atlas)) {
        const uint32_t white = nt_atlas_find_region(s_atlas, ASSET_ATLAS_REGION_UI__WHITE.value);
        if (white != NT_ATLAS_INVALID_REGION) {
            nt_ui_set_atlas_white_region(s_ctx, s_atlas, white);
            s_atlas_bound = true;
        }
    }
    if (!s_font_bound && nt_resource_is_ready(s_font_resource)) {
        nt_ui_set_font(s_ctx, 0U, s_font);
        s_font_bound = true;
    }
}

bool ui_runtime_ready(void) {
    resolve_runtime_bindings();
    const nt_material_info_t *si = nt_material_get_info(s_sprite_material);
    const nt_material_info_t *ti = nt_material_get_info(s_text_material);
    return s_atlas_bound && s_font_bound && si != NULL && si->ready && ti != NULL && ti->ready;
}

bool ui_runtime_begin(float dt) {
    if (!ui_runtime_ready()) {
        return false;
    }

    const nt_material_info_t *si = nt_material_get_info(s_sprite_material);
    const nt_material_info_t *ti = nt_material_get_info(s_text_material);
    if (!s_atlas_bound || !s_font_bound || !si || !si->ready || !ti || !ti->ready) {
        return false;
    }

    const float fb_w = (float)(g_nt_window.fb_width > 0 ? g_nt_window.fb_width : 1280);
    const float fb_h = (float)(g_nt_window.fb_height > 0 ? g_nt_window.fb_height : 720);
    const bool portrait = fb_h > fb_w;
    nt_ui_scale_desc_t sd = {
        .ref_w = portrait ? UI_PORTRAIT_REF_W : UI_LANDSCAPE_REF_W,
        .ref_h = portrait ? UI_PORTRAIT_REF_H : UI_LANDSCAPE_REF_H,
        .mode = NT_UI_SCALE_EXPAND,
    };
    s_scale = nt_ui_compute_scale(&sd, fb_w, fb_h);

    nt_frame_uniforms_t u;
    memset(&u, 0, sizeof u);
    float vp[16];
    nt_ui_make_screen_view_proj(s_scale.logical_w, s_scale.logical_h, vp);
    memcpy(u.view_proj, vp, 64);
    memcpy(u.proj, vp, 64);
    glm_mat4_identity((vec4 *)u.view);
    u.resolution[0] = fb_w;
    u.resolution[1] = fb_h;
    u.near_far[0] = -1.0F;
    u.near_far[1] = 1.0F;
    nt_gfx_update_buffer(s_ui_ubo, &u, sizeof u);
    nt_gfx_bind_uniform_buffer(s_ui_ubo, 0);

    nt_font_step();
    nt_mem_scratch_reset();
    // Feed ALL pointer slots: a DevAPI-injected click lands in the first FREE
    // slot, which is not 0 when a real mouse already holds slot 0.
    nt_pointer_t pointers[NT_INPUT_MAX_POINTERS];
    for (uint32_t i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
        pointers[i] = nt_ui_scale_apply_pointer(&s_scale, g_nt_input.pointers[i]);
    }
    nt_ui_begin(s_ctx, s_scale.logical_w, s_scale.logical_h, dt, pointers, NT_INPUT_MAX_POINTERS);
    return true;
}

void ui_runtime_end(void) {
    nt_ui_end(s_ctx);
    nt_ui_target_t target = nt_ui_scale_make_target(&s_scale);
    nt_ui_walk(s_ctx, &target);
    nt_sprite_renderer_flush();
    nt_text_renderer_flush();
}

void ui_runtime_restore_gpu(void) {
    nt_gfx_destroy_buffer(s_ui_ubo);
    s_ui_ubo = make_ubo();
    nt_sprite_renderer_restore_gpu();
    s_atlas_bound = false; // re-resolve atlas white region after GL restore
    s_font_bound = false;
}

void ui_runtime_shutdown(void) {
    nt_ui_destroy_context(s_ctx);
    nt_ui_module_shutdown();
    nt_sprite_renderer_shutdown();
    nt_material_destroy(s_sprite_material);
    nt_gfx_destroy_buffer(s_ui_ubo);
}

nt_ui_context_t *ui_runtime_ctx(void) { return s_ctx; }
