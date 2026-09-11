#include "lab_ui_runtime.h"

#include "atlas/nt_atlas.h"
#include "core/nt_assert.h"
#include "graphics/nt_gfx.h"
#include "math/nt_math.h"
#include "memory/nt_mem_scratch.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_sprite_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "ui/nt_ui_scale.h"
#include "window/nt_window.h"

#include "features/ui_kit/ui_metrics.h"
#include "ui_lab_assets.h"

#include <string.h>

// Six scenes share one context; the kit's default pool would trip the
// non-evicting state assert once every scroll, modal and slider has a cell.
#define UI_ARENA_SIZE ((size_t)8U * 1024U * 1024U)

static NT_UI_DECLARE_ARENA(s_ui_arena, UI_ARENA_SIZE);
static nt_ui_context_t *s_ctx;
static nt_buffer_t s_ubo;
static nt_ui_scale_t s_scale;

static nt_material_t s_sprite_material;
static nt_material_t s_text_material;
static nt_font_t s_font;
static nt_resource_t s_font_resource;
static nt_resource_t s_atlas;
static bool s_atlas_bound;
static bool s_font_bound;

static nt_buffer_t make_ubo(void) {
    return nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM, .usage = NT_USAGE_DYNAMIC, .size = sizeof(nt_frame_uniforms_t), .label = "ui_lab_frame_uniforms"});
}

static bool material_ready(nt_material_t material) {
    const nt_material_info_t *info = nt_material_get_info(material);
    return info != NULL && nt_gfx_program_ready(info->program);
}

void lab_ui_runtime_init(nt_material_t sprite_material, nt_material_t text_material, nt_font_t font,
                         nt_resource_t font_resource, nt_resource_t ui_atlas) {
    s_sprite_material = sprite_material;
    s_text_material = text_material;
    s_font = font;
    s_font_resource = font_resource;
    s_atlas = ui_atlas;

    nt_ui_module_init();
    nt_ui_create_desc_t desc = nt_ui_create_desc_defaults();
    desc.max_elements = 4096U;
    desc.state_slots = 1024U;
    desc.state_probe_max = 16U;
    s_ctx = nt_ui_create_context(s_ui_arena, sizeof s_ui_arena, &desc);
    NT_ASSERT(s_ctx != NULL && "ui_lab: failed to create UI context");

    nt_ui_set_sprite_material(s_ctx, s_sprite_material);
    nt_ui_set_text_material(s_ctx, s_text_material);
    s_ubo = make_ubo();
}

static void resolve_bindings(void) {
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

bool lab_ui_runtime_ready(void) {
    resolve_bindings();
    return s_atlas_bound && s_font_bound && material_ready(s_sprite_material) && material_ready(s_text_material);
}

bool lab_ui_runtime_begin(float dt, const nt_pointer_t *pointers, uint32_t count) {
    if (!lab_ui_runtime_ready()) {
        return false;
    }
    const float fb_w = (float)(g_nt_window.fb_width > 0 ? g_nt_window.fb_width : 1280);
    const float fb_h = (float)(g_nt_window.fb_height > 0 ? g_nt_window.fb_height : 720);
    const UiScaleFit fit = ui_frame_begin(fb_w, fb_h, g_nt_window.dpr);
    s_scale = (nt_ui_scale_t){
        .logical_w = fit.logical_w,
        .logical_h = fit.logical_h,
        .scale_x = fit.scale,
        .scale_y = fit.scale,
        .offset_x = 0.0F,
        .offset_y = 0.0F,
        .fb_w = fb_w,
        .fb_h = fb_h,
    };

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
    nt_gfx_update_buffer(s_ubo, 0, &u, sizeof u);
    nt_gfx_bind_uniform_buffer(s_ubo, 0);

    nt_ui_begin(s_ctx, s_scale.logical_w, s_scale.logical_h, dt, pointers, count);
    nt_ui_set_viewport(s_ctx, nt_ui_viewport_from_scale(&s_scale));
    return true;
}

void lab_ui_runtime_end(void) {
    nt_ui_end(s_ctx);
    const nt_ui_target_t target = nt_ui_scale_make_target(&s_scale);
    nt_ui_walk(s_ctx, &target);
    nt_sprite_renderer_flush();
    nt_text_renderer_flush();
}

void lab_ui_runtime_restore_gpu(void) {
    nt_gfx_destroy_buffer(s_ubo);
    s_ubo = make_ubo();
    s_atlas_bound = false;
    s_font_bound = false;
}

void lab_ui_runtime_shutdown(void) {
    nt_ui_destroy_context(s_ctx);
    nt_ui_module_shutdown();
    nt_gfx_destroy_buffer(s_ubo);
}

nt_ui_context_t *lab_ui_runtime_ctx(void) { return s_ctx; }
