#include "render/render_hub_scene.h"

#include "atlas/nt_atlas.h"
#include "color/nt_color.h"
#include "generated/game_assets.h"
#include "hash/nt_hash.h"
#include "material/nt_material.h"
#include "math/nt_math.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_sprite_renderer.h"
#include "resource/nt_resource.h"
#include "scene/scene_interactions.h"
#include "scene/scene_layout.h"
#include "window/nt_window.h"

#include <math.h>
#include <string.h>

static nt_resource_t s_sprite_vs, s_sprite_fs, s_sprite_mask_glow_fs, s_sprite_fade_vs, s_sprite_fade_fs;
static nt_resource_t s_overlay_atlas, s_overlay_atlas_tex;
static nt_resource_t s_background_atlas, s_background_atlas_tex;
static nt_material_t s_overlay_material, s_background_material, s_mask_glow_material, s_hud_fade_material;
static uint32_t s_white_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_tutorial_finger_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_background_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_guard_region = NT_ATLAS_INVALID_REGION;

static nt_hash64_t rid(const char *s) { return nt_hash64_str(s); }

static uint32_t rgba(float r, float g, float b, float a) {
    const float c[4] = {r, g, b, a};
    return nt_color_pack(c);
}

static void draw_quad(float x, float y, float w, float h, uint32_t color) {
    const float p[4][2] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    static const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
    nt_sprite_renderer_emit_geometry(s_overlay_atlas, s_white_region, p, 4, idx, 6, NT_MATH_MAT4_IDENTITY, color);
}

static void draw_tri(float ax, float ay, float bx, float by, float cx, float cy, uint32_t color) {
    const float p[3][2] = {{ax, ay}, {bx, by}, {cx, cy}};
    static const uint16_t idx[3] = {0, 1, 2};
    nt_sprite_renderer_emit_geometry(s_overlay_atlas, s_white_region, p, 3, idx, 3, NT_MATH_MAT4_IDENTITY, color);
}

static void draw_soft_oval(float center_x, float y, float w, float h, uint32_t color) {
    static const float widths[] = {0.36F, 0.68F, 0.92F, 1.0F, 0.92F, 0.68F, 0.36F};
    const int count = (int)(sizeof widths / sizeof widths[0]);
    const float step = h / (float)count;
    for (int i = 0; i < count; ++i) {
        const float band_w = w * widths[i];
        draw_quad(center_x - band_w * 0.5F, y + step * (float)i, band_w, step + 0.5F, color);
    }
}

static bool material_ready(nt_material_t material) {
    const nt_material_info_t *info = nt_material_get_info(material);
    return info && info->ready;
}

static bool overlay_ready(void) {
    if (!nt_resource_is_ready(s_overlay_atlas)) {
        return false;
    }
    if (s_white_region == NT_ATLAS_INVALID_REGION) {
        s_white_region = nt_atlas_find_region(s_overlay_atlas, ASSET_ATLAS_REGION_UI__WHITE.value);
    }
    return s_white_region != NT_ATLAS_INVALID_REGION && material_ready(s_overlay_material);
}

static bool tutorial_finger_ready(void) {
    if (!nt_resource_is_ready(s_overlay_atlas) || !material_ready(s_overlay_material)) {
        return false;
    }
    if (s_tutorial_finger_region == NT_ATLAS_INVALID_REGION) {
        s_tutorial_finger_region = nt_atlas_find_region(s_overlay_atlas, ASSET_ATLAS_REGION_UI_TUTORIAL_FINGER.value);
    }
    return s_tutorial_finger_region != NT_ATLAS_INVALID_REGION;
}

static bool background_ready(void) {
    if (!nt_resource_is_ready(s_background_atlas)) {
        return false;
    }
    if (s_background_region == NT_ATLAS_INVALID_REGION) {
        s_background_region = nt_atlas_find_region(s_background_atlas, ASSET_ATLAS_REGION_HUB_SCENE_LAST_POST_BACKGROUND.value);
    }
    return s_background_region != NT_ATLAS_INVALID_REGION && material_ready(s_background_material);
}

static bool guard_ready(void) {
    if (!nt_resource_is_ready(s_background_atlas)) {
        return false;
    }
    if (s_guard_region == NT_ATLAS_INVALID_REGION) {
        s_guard_region = nt_atlas_find_region(s_background_atlas, ASSET_ATLAS_REGION_HUB_SCENE_LAST_POST_GUARD.value);
    }
    return s_guard_region != NT_ATLAS_INVALID_REGION && material_ready(s_background_material);
}

void render_hub_scene_init(void) {
    s_sprite_vs = nt_resource_request(rid("assets/shaders/sprite.vert"), NT_ASSET_SHADER_CODE);
    s_sprite_fs = nt_resource_request(rid("assets/shaders/sprite.frag"), NT_ASSET_SHADER_CODE);
    s_sprite_mask_glow_fs = nt_resource_request(rid("assets/shaders/sprite_mask_glow.frag"), NT_ASSET_SHADER_CODE);
    s_sprite_fade_vs = nt_resource_request(rid("assets/shaders/sprite_ui_fade.vert"), NT_ASSET_SHADER_CODE);
    s_sprite_fade_fs = nt_resource_request(rid("assets/shaders/sprite_ui_fade.frag"), NT_ASSET_SHADER_CODE);
    s_overlay_atlas = nt_resource_request(ASSET_ATLAS_UI, NT_ASSET_ATLAS);
    s_overlay_atlas_tex = nt_resource_request(ASSET_TEXTURE_UI_TEX0, NT_ASSET_TEXTURE);
    s_background_atlas = nt_resource_request(ASSET_ATLAS_HUB_SCENE, NT_ASSET_ATLAS);
    s_background_atlas_tex = nt_resource_request(ASSET_TEXTURE_HUB_SCENE_TEX0, NT_ASSET_TEXTURE);
    s_overlay_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_sprite_vs,
        .fs = s_sprite_fs,
        .textures = {{.name = "u_texture", .resource = s_overlay_atlas_tex}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "hub_scene_overlay_sprite",
    });
    s_background_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_sprite_vs,
        .fs = s_sprite_fs,
        .textures = {{.name = "u_texture", .resource = s_background_atlas_tex}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "hub_scene_background_sprite",
    });
    s_mask_glow_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_sprite_vs,
        .fs = s_sprite_mask_glow_fs,
        .textures = {{.name = "u_texture", .resource = s_background_atlas_tex}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "hub_scene_mask_glow_sprite",
    });
    s_hud_fade_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_sprite_fade_vs,
        .fs = s_sprite_fade_fs,
        .textures = {{.name = "u_texture", .resource = s_overlay_atlas_tex}},
        .texture_count = 1,
        .attr_map = {{.stream_name = "fade", .location = 4}},
        .attr_map_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "hub_scene_hud_fade_sprite",
    });
}

static void bind_scene_view(scene_view_t view, nt_buffer_t frame_ubo) {
    float view_m[16];
    float proj_m[16];
    float vp[16];
    glm_mat4_identity((vec4 *)view_m);
    glm_ortho(view.view_x, view.view_x + view.view_w, view.view_y, view.view_y + view.view_h, -1.0F, 1.0F, (vec4 *)proj_m);
    glm_mat4_mul((vec4 *)proj_m, (vec4 *)view_m, (vec4 *)vp);

    nt_frame_uniforms_t u;
    memset(&u, 0, sizeof u);
    memcpy(u.view_proj, vp, 64);
    memcpy(u.view, view_m, 64);
    memcpy(u.proj, proj_m, 64);
    u.resolution[0] = (float)g_nt_window.fb_width;
    u.resolution[1] = (float)g_nt_window.fb_height;
    u.near_far[0] = -1.0F;
    u.near_far[1] = 1.0F;
    nt_gfx_update_buffer(frame_ubo, &u, sizeof u);
    nt_gfx_bind_uniform_buffer(frame_ubo, 0);
}

static void draw_background_image(void) {
    float m[16];
    glm_mat4_identity((vec4 *)m);
    nt_sprite_renderer_set_material(s_background_material);
    nt_sprite_renderer_emit_region(s_background_atlas, s_background_region, m, 0.0F, 0.0F, 0xFFFFFFFFU, 0U);
    nt_sprite_renderer_flush();
}

static void draw_scene_backdrop(scene_view_t view) {
    draw_quad(view.view_x, view.view_y, view.view_w, view.view_h, rgba(0.018F, 0.021F, 0.030F, 1.0F));
    draw_quad(view.view_x, view.view_y, view.view_w, view.view_h * 0.34F, rgba(0.030F, 0.028F, 0.032F, 1.0F));
    draw_quad(view.view_x, view.view_y + view.view_h * 0.66F, view.view_w, view.view_h * 0.34F, rgba(0.018F, 0.023F, 0.034F, 1.0F));
}

static void draw_fallback_background(void) {
    draw_quad(0.0F, 0.0F, (float)SCENE_LAYOUT_MASTER_W, (float)SCENE_LAYOUT_MASTER_H, rgba(0.035F, 0.043F, 0.060F, 1.0F));
    draw_quad(0.0F, 0.0F, (float)SCENE_LAYOUT_MASTER_W, 190.0F, rgba(0.055F, 0.052F, 0.050F, 1.0F));
    draw_quad(160.0F, 80.0F, 960.0F, 540.0F, rgba(0.070F, 0.083F, 0.105F, 1.0F));

    draw_quad(0.0F, 80.0F, 160.0F, 540.0F, rgba(0.030F, 0.036F, 0.050F, 1.0F));
    draw_quad(1120.0F, 80.0F, 160.0F, 540.0F, rgba(0.030F, 0.036F, 0.050F, 1.0F));
    draw_quad(0.0F, 620.0F, 1280.0F, 80.0F, rgba(0.025F, 0.030F, 0.044F, 1.0F));
    draw_quad(0.0F, 0.0F, 1280.0F, 80.0F, rgba(0.026F, 0.024F, 0.026F, 1.0F));

    draw_quad(300.0F, 330.0F, 680.0F, 150.0F, rgba(0.110F, 0.115F, 0.125F, 1.0F));
    draw_quad(430.0F, 245.0F, 420.0F, 200.0F, rgba(0.150F, 0.145F, 0.130F, 1.0F));
    draw_quad(570.0F, 190.0F, 140.0F, 220.0F, rgba(0.045F, 0.040F, 0.042F, 1.0F));
    draw_quad(520.0F, 160.0F, 240.0F, 35.0F, rgba(0.100F, 0.085F, 0.065F, 1.0F));

    draw_tri(520.0F, 80.0F, 760.0F, 80.0F, 670.0F, 230.0F, rgba(0.130F, 0.105F, 0.075F, 1.0F));
    draw_tri(760.0F, 80.0F, 980.0F, 80.0F, 670.0F, 230.0F, rgba(0.075F, 0.066F, 0.060F, 1.0F));
}

static void draw_hud_fade_quad(float x, float y, float w, float h, float transparent_y, float dense_y, float max_alpha) {
    const float p[4][2] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    static const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
    const float fade_attrs[4] = {transparent_y, dense_y, max_alpha, 0.0F};
    nt_sprite_renderer_set_custom_attrs(fade_attrs, (uint8_t)sizeof fade_attrs);
    nt_sprite_renderer_emit_geometry(s_overlay_atlas,
                                     s_white_region,
                                     p,
                                     4,
                                     idx,
                                     6,
                                     NT_MATH_MAT4_IDENTITY,
                                     rgba(0.039F, 0.027F, 0.020F, 1.0F));
}

static void draw_hud_scrims(scene_view_t view) {
    if (!material_ready(s_hud_fade_material)) {
        return;
    }

    const bool portrait = view.framebuffer_h > view.framebuffer_w;
    const float top_tail_h = (portrait ? 172.0F : 156.0F) / view.scale;
    const float top_dense_h = (portrait ? 96.0F : 84.0F) / view.scale;
    const float bottom_tail_h = (portrait ? 206.0F : 190.0F) / view.scale;
    const float bottom_dense_h = (portrait ? 132.0F : 124.0F) / view.scale;
    const float top_edge_y = view.view_y + view.view_h;
    const float bottom_edge_y = view.view_y;

    nt_sprite_renderer_set_material(s_hud_fade_material);
    draw_hud_fade_quad(view.view_x,
                       top_edge_y - top_tail_h,
                       view.view_w,
                       top_tail_h,
                       top_edge_y - top_tail_h,
                       top_edge_y - top_dense_h,
                       0.78F);
    draw_hud_fade_quad(view.view_x,
                       bottom_edge_y,
                       view.view_w,
                       bottom_tail_h,
                       bottom_edge_y + bottom_tail_h,
                       bottom_edge_y + bottom_dense_h,
                       0.76F);
}

static void draw_character_contact_shadow(const scene_interaction_object_t *object, uint32_t flags) {
    const float center_x = object->anchor_x;
    const float y = object->anchor_y - 6.0F;
    float alpha = 0.15F;
    if ((flags & SCENE_INTERACTION_OBJECTIVE) != 0U) {
        alpha += 0.04F;
    }
    if ((flags & SCENE_INTERACTION_HOVERED) != 0U) {
        alpha += 0.04F;
    }

    draw_quad(center_x - 42.0F, y, 84.0F, 2.0F, rgba(0.0F, 0.0F, 0.0F, alpha * 0.35F));
    draw_quad(center_x - 33.0F, y + 2.0F, 27.0F, 5.0F, rgba(0.0F, 0.0F, 0.0F, alpha));
    draw_quad(center_x + 9.0F, y + 2.0F, 32.0F, 5.0F, rgba(0.0F, 0.0F, 0.0F, alpha * 0.90F));
    draw_quad(center_x - 24.0F, y + 7.0F, 54.0F, 2.0F, rgba(0.0F, 0.0F, 0.0F, alpha * 0.28F));
}

static void draw_interaction_ground_shadow(const scene_interaction_object_t *object, uint32_t flags, bool has_sprite_outline) {
    if (object->kind == SCENE_OBJECT_KIND_CHARACTER && has_sprite_outline) {
        draw_character_contact_shadow(object, flags);
        return;
    }

    const float center_x = object->anchor_x;
    const float shadow_w = (float)object->bounds.w * 0.62F;
    const float shadow_h = 18.0F;
    const float shadow_y = object->anchor_y - 10.0F;
    float alpha = 0.18F;
    if ((flags & SCENE_INTERACTION_OBJECTIVE) != 0U) {
        alpha += 0.06F;
    }
    if ((flags & SCENE_INTERACTION_HOVERED) != 0U) {
        alpha += 0.06F;
    }
    draw_soft_oval(center_x, shadow_y, shadow_w, shadow_h, rgba(0.0F, 0.0F, 0.0F, alpha));
}

static void draw_scene_object_ground_affordance(const scene_interaction_object_t *object, uint32_t flags, float pulse, bool has_sprite_outline) {
    if ((flags & SCENE_INTERACTION_IDLE) == 0U) {
        return;
    }
    (void)pulse;
    draw_interaction_ground_shadow(object, flags, has_sprite_outline);
}

static void draw_scene_object_fallback(const scene_interaction_object_t *object) {
    if (object->id != SCENE_OBJECT_ID_GUARD) {
        return;
    }
    draw_quad(626.0F, 250.0F, 48.0F, 86.0F, rgba(0.180F, 0.170F, 0.150F, 1.0F));
    draw_quad(632.0F, 336.0F, 36.0F, 34.0F, rgba(0.210F, 0.185F, 0.140F, 1.0F));
    draw_quad(618.0F, 246.0F, 64.0F, 8.0F, rgba(0.95F, 0.70F, 0.26F, 1.0F));
}

static void draw_scene_object_affordances(const World *w, bool has_guard) {
    const float pulse = 0.5F + 0.5F * sinf(w->time_seconds * 4.0F);
    int count = 0;
    const scene_interaction_object_t *objects = scene_interactions_all(&count);
    for (int i = 0; i < count; ++i) {
        const uint32_t flags = scene_interactions_visual_flags(w, objects[i].id);
        const bool has_sprite_outline = objects[i].id == SCENE_OBJECT_ID_GUARD && has_guard;
        draw_scene_object_ground_affordance(&objects[i], flags, pulse, has_sprite_outline);
        if (objects[i].id == SCENE_OBJECT_ID_GUARD && !has_guard) {
            draw_scene_object_fallback(&objects[i]);
        }
    }
}

typedef struct scene_object_sprite_visual_t {
    uint32_t region;
    float target_h;
} scene_object_sprite_visual_t;

static bool scene_object_sprite_visual(const scene_interaction_object_t *object, scene_object_sprite_visual_t *visual) {
    if (object->id == SCENE_OBJECT_ID_GUARD && s_guard_region != NT_ATLAS_INVALID_REGION) {
        visual->region = s_guard_region;
        visual->target_h = (float)object->bounds.h - 14.0F;
        return true;
    }
    return false;
}

static void emit_scene_object_sprite(const scene_interaction_object_t *object,
                                     const scene_object_sprite_visual_t *visual,
                                     float offset_x,
                                     float offset_y,
                                     float scale_mul,
                                     uint32_t color) {
    nt_atlas_region_handles_t handles;
    nt_atlas_get_region_handles(s_background_atlas, visual->region, &handles);
    if (!handles.region || handles.region->source_h == 0U) {
        return;
    }

    const float source_h = (float)handles.region->source_h * handles.ipu;
    const float scale = (visual->target_h / source_h) * scale_mul;
    float m[16];
    glm_mat4_identity((vec4 *)m);
    m[0] = scale;
    m[5] = scale;
    m[12] = object->anchor_x + offset_x;
    m[13] = object->anchor_y + offset_y;

    nt_sprite_renderer_emit_region(s_background_atlas, visual->region, m, 0.5F, 0.0F, color, 0U);
}

static void draw_scene_object_sprite_mask_glow(const scene_interaction_object_t *object,
                                               const scene_object_sprite_visual_t *visual,
                                               uint32_t flags,
                                               float pulse) {
    if ((flags & SCENE_INTERACTION_IDLE) == 0U || object->highlight_profile != SCENE_HIGHLIGHT_MASK_GLOW) {
        return;
    }

    const bool hovered = (flags & SCENE_INTERACTION_HOVERED) != 0U;
    const bool pressed = (flags & SCENE_INTERACTION_PRESSED) != 0U;
    const bool objective = (flags & SCENE_INTERACTION_OBJECTIVE) != 0U;
    const float base_alpha = objective ? 0.068F + 0.012F * pulse : 0.048F;
    const float glow_alpha = pressed ? 0.145F : (hovered ? 0.130F : base_alpha);
    const float radius = hovered || pressed ? 6.0F : 3.5F;

    const uint32_t outer_warm = rgba(1.0F, 0.66F, 0.30F, glow_alpha * 0.54F);
    const uint32_t mid_warm = rgba(1.0F, 0.82F, 0.48F, glow_alpha * 0.66F);
    const uint32_t soft_core = rgba(1.0F, 0.92F, 0.70F, glow_alpha * 0.26F);

    emit_scene_object_sprite(object, visual, 0.0F, 0.0F, hovered ? 1.150F : 1.095F, outer_warm);
    emit_scene_object_sprite(object, visual, -radius, 0.0F, hovered ? 1.088F : 1.048F, mid_warm);
    emit_scene_object_sprite(object, visual, radius, 0.0F, hovered ? 1.088F : 1.048F, mid_warm);
    emit_scene_object_sprite(object, visual, 0.0F, -radius, hovered ? 1.088F : 1.048F, mid_warm);
    emit_scene_object_sprite(object, visual, 0.0F, radius, hovered ? 1.088F : 1.048F, mid_warm);

    if (hovered || pressed) {
        const float diagonal = radius * 0.70F;
        emit_scene_object_sprite(object, visual, -diagonal, -diagonal, 1.045F, mid_warm);
        emit_scene_object_sprite(object, visual, diagonal, -diagonal, 1.045F, mid_warm);
        emit_scene_object_sprite(object, visual, -diagonal, diagonal, 1.045F, mid_warm);
        emit_scene_object_sprite(object, visual, diagonal, diagonal, 1.045F, mid_warm);
        emit_scene_object_sprite(object, visual, 0.0F, 0.0F, 1.030F, soft_core);
    }
}

static void draw_scene_object_sprite(const scene_interaction_object_t *object, const scene_object_sprite_visual_t *visual, uint32_t flags) {
    const bool hovered = (flags & SCENE_INTERACTION_HOVERED) != 0U;
    const bool pressed = (flags & SCENE_INTERACTION_PRESSED) != 0U;
    const float scale_mul = pressed ? 1.012F : (hovered ? 1.035F : 1.0F);
    const uint32_t tint = hovered ? rgba(1.0F, 0.97F, 0.90F, 1.0F) : 0xFFFFFFFFU;
    emit_scene_object_sprite(object, visual, 0.0F, 0.0F, scale_mul, tint);
}

static void draw_scene_object_sprites(const World *w) {
    if (!material_ready(s_mask_glow_material) || !material_ready(s_background_material)) {
        return;
    }

    const float pulse = 0.5F + 0.5F * sinf(w->time_seconds * 4.0F);
    int count = 0;
    const scene_interaction_object_t *objects = scene_interactions_all(&count);
    nt_sprite_renderer_set_material(s_mask_glow_material);
    for (int i = 0; i < count; ++i) {
        scene_object_sprite_visual_t visual;
        if (!scene_object_sprite_visual(&objects[i], &visual)) {
            continue;
        }
        const uint32_t flags = scene_interactions_visual_flags(w, objects[i].id);
        draw_scene_object_sprite_mask_glow(&objects[i], &visual, flags, pulse);
    }
    nt_sprite_renderer_flush();

    nt_sprite_renderer_set_material(s_background_material);
    for (int i = 0; i < count; ++i) {
        scene_object_sprite_visual_t visual;
        if (!scene_object_sprite_visual(&objects[i], &visual)) {
            continue;
        }
        const uint32_t flags = scene_interactions_visual_flags(w, objects[i].id);
        draw_scene_object_sprite(&objects[i], &visual, flags);
    }
    nt_sprite_renderer_flush();
}

static void draw_tutorial_finger_for_object(const World *w, const scene_interaction_object_t *object) {
    nt_atlas_region_handles_t handles;
    nt_atlas_get_region_handles(s_overlay_atlas, s_tutorial_finger_region, &handles);
    if (!handles.region || handles.region->source_h == 0U) {
        return;
    }

    const float pulse = 0.5F + 0.5F * sinf(w->time_seconds * 4.0F);
    const float source_h = (float)handles.region->source_h * handles.ipu;
    const float target_h = 128.0F;
    const float scale = (target_h / source_h) * (1.0F + 0.025F * pulse);
    const float tip_x = object->anchor_x + 16.0F + 4.0F * pulse;
    const float tip_y = (float)object->bounds.y + (float)object->bounds.h * 0.64F + 4.0F * pulse;
    const uint32_t color = rgba(1.0F, 0.96F, 0.86F, 0.88F + 0.08F * pulse);

    float m[16];
    glm_mat4_identity((vec4 *)m);
    m[0] = scale;
    m[5] = scale;
    m[12] = tip_x;
    m[13] = tip_y;

    nt_sprite_renderer_emit_region(s_overlay_atlas, s_tutorial_finger_region, m, 0.16F, 0.92F, color, 0U);
}

static void draw_tutorial_fingers(const World *w) {
    if (!tutorial_finger_ready()) {
        return;
    }

    int count = 0;
    const scene_interaction_object_t *objects = scene_interactions_all(&count);
    for (int i = 0; i < count; ++i) {
        if (scene_interactions_should_show_tutorial_finger(w, objects[i].id)) {
            draw_tutorial_finger_for_object(w, &objects[i]);
        }
    }
}

void render_hub_scene_draw(const World *w, nt_buffer_t frame_ubo) {
    const bool has_background = background_ready();
    const bool has_guard = guard_ready();
    const bool has_overlay = overlay_ready();
    if (!has_background && !has_overlay) {
        return;
    }
    const float center_x = w->first_scene.camera_initialized ? w->first_scene.camera_center_x : scene_layout_default_center_x();
    const float center_y = w->first_scene.camera_initialized ? w->first_scene.camera_center_y : scene_layout_default_center_y();
    const scene_view_t view = scene_layout_compute_view((int)g_nt_window.fb_width, (int)g_nt_window.fb_height, center_x, center_y);
    if (view.scale <= 0.0F) {
        return;
    }

    bind_scene_view(view, frame_ubo);

    if (has_overlay) {
        nt_sprite_renderer_set_material(s_overlay_material);
        draw_scene_backdrop(view);
        nt_sprite_renderer_flush();
    }
    if (has_background) {
        draw_background_image();
    }
    if (has_overlay) {
        nt_sprite_renderer_set_material(s_overlay_material);
        if (!has_background) {
            draw_fallback_background();
        }
        draw_scene_object_affordances(w, has_guard);
        nt_sprite_renderer_flush();
    }
    if (has_guard) {
        draw_scene_object_sprites(w);
    }
    if (has_overlay) {
        draw_hud_scrims(view);
        nt_sprite_renderer_flush();
    }
    if (has_overlay) {
        nt_sprite_renderer_set_material(s_overlay_material);
        draw_tutorial_fingers(w);
        nt_sprite_renderer_flush();
    }
}

void render_hub_scene_shutdown(void) {
    nt_material_destroy(s_overlay_material);
    nt_material_destroy(s_background_material);
    nt_material_destroy(s_mask_glow_material);
    nt_material_destroy(s_hud_fade_material);
}
