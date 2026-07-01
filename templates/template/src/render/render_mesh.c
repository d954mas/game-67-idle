#include "render/render_mesh.h"

#include "app/nt_app.h"
#include "drawable_comp/nt_drawable_comp.h"
#include "entity/nt_entity.h"
#include "material/nt_material.h"
#include "material_comp/nt_material_comp.h"
#include "math/nt_math.h"
#include "mesh_comp/nt_mesh_comp.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "render/nt_render_items.h"
#include "renderers/nt_mesh_renderer.h"
#include "transform_comp/nt_transform_comp.h"
#include "window/nt_window.h"

#include <string.h>

// The mesh render system. Owns the shared ECS + mesh renderer + the follow camera,
// and TWO materials a game learns from: s_material (coloured, mesh_inst) and
// s_material_tex (textured, mesh_tex + u_texture). Draws every mesh entity in the
// World; it only reads the World's state.
static nt_material_t s_material;     // coloured path (per-instance colour, no texture)
static nt_material_t s_material_tex; // textured path (samples u_texture at uv0)
static nt_resource_t s_cube_mesh;

void render_mesh_init(nt_resource_t mesh_vs, nt_resource_t mesh_fs, nt_resource_t tex_vs, nt_resource_t tex_fs, nt_resource_t texture) {
    nt_entity_init(&(nt_entity_desc_t){.max_entities = 128});
    nt_transform_comp_init(&(nt_transform_comp_desc_t){.capacity = 128});
    nt_mesh_comp_init(&(nt_mesh_comp_desc_t){.capacity = 128});
    nt_material_comp_init(&(nt_material_comp_desc_t){.capacity = 128});
    nt_drawable_comp_init(&(nt_drawable_comp_desc_t){.capacity = 128});
    nt_mesh_renderer_init(&(nt_mesh_renderer_desc_t){.max_instances = 256, .max_pipelines = 16});

    s_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = mesh_vs,
        .fs = mesh_fs,
        .attr_map = {{.stream_name = "position", .location = 0}},
        .attr_map_count = 1,
        .depth_test = true,
        .depth_write = true,
        .cull_mode = NT_CULL_BACK,
        .color_mode = NT_COLOR_MODE_FLOAT4,
        .label = "mesh",
    });

    // Textured material: position + uv0 streams, samples u_texture; per-instance
    // colour still multiplies (white = texture verbatim).
    s_material_tex = nt_material_create(&(nt_material_create_desc_t){
        .vs = tex_vs,
        .fs = tex_fs,
        .attr_map = {{.stream_name = "position", .location = 0}, {.stream_name = "uv0", .location = 1}},
        .attr_map_count = 2,
        .textures = {{.name = "u_texture", .resource = texture}},
        .texture_count = 1,
        .depth_test = true,
        .depth_write = true,
        .cull_mode = NT_CULL_BACK,
        .color_mode = NT_COLOR_MODE_FLOAT4,
        .label = "mesh_tex",
    });
}

void render_mesh_spawn_player(World *w, nt_resource_t cube_mesh, const float color[4]) {
    s_cube_mesh = cube_mesh;
    w->player_entity = nt_entity_create();
    nt_transform_comp_add(w->player_entity);
    nt_mesh_comp_add(w->player_entity);
    nt_material_comp_add(w->player_entity);
    nt_drawable_comp_add(w->player_entity);
    *nt_material_comp_handle(w->player_entity) = s_material;
    nt_drawable_comp_set_color(w->player_entity, color[0], color[1], color[2], color[3]);
    w->player_spawned = true;
}

void render_mesh_spawn_prop(World *w, nt_resource_t cube_mesh) {
    w->prop_entity = nt_entity_create();
    nt_transform_comp_add(w->prop_entity);
    nt_mesh_comp_add(w->prop_entity);
    nt_material_comp_add(w->prop_entity);
    nt_drawable_comp_add(w->prop_entity);
    *nt_material_comp_handle(w->prop_entity) = s_material_tex;
    nt_drawable_comp_set_color(w->prop_entity, 1.0F, 1.0F, 1.0F, 1.0F); // white = texture verbatim
    // Static, parked to the player's right; set once (transform persists).
    float *pos = nt_transform_comp_position(w->prop_entity);
    float *scale = nt_transform_comp_scale(w->prop_entity);
    pos[0] = 3.0F;
    pos[1] = 0.5F;
    pos[2] = 0.0F;
    scale[0] = scale[1] = scale[2] = 1.0F;
    *nt_transform_comp_dirty(w->prop_entity) = true;
    w->prop_spawned = true;
}

void render_mesh_restore_gpu(void) {
    nt_mesh_renderer_restore_gpu();
}

void render_mesh_draw(World *w, nt_buffer_t frame_ubo) {
    const nt_material_info_t *info = nt_material_get_info(s_material);
    const nt_material_info_t *info_tex = nt_material_get_info(s_material_tex);
    if (!info || !info->ready || !w->player_spawned || !nt_resource_is_ready(s_cube_mesh)) {
        return;
    }

    // follow camera on the player (3/4 view)
    const float aspect = g_nt_window.fb_height > 0 ? (float)g_nt_window.fb_width / (float)g_nt_window.fb_height : 16.0F / 9.0F;
    const float cx = w->player_x - 4.0F;
    const float cy = 4.0F;
    const float cz = w->player_z - 6.0F;
    float view[16];
    float proj[16];
    glm_lookat((vec3){cx, cy, cz}, (vec3){w->player_x, 0.5F, w->player_z}, (vec3){0.0F, 1.0F, 0.0F}, (vec4 *)view);
    glm_perspective(glm_rad(58.0F), aspect, 0.1F, 100.0F, (vec4 *)proj);

    nt_frame_uniforms_t u;
    memset(&u, 0, sizeof(u));
    float vp[16];
    glm_mat4_mul((vec4 *)proj, (vec4 *)view, (vec4 *)vp);
    memcpy(u.view_proj, vp, 64);
    memcpy(u.view, view, 64);
    memcpy(u.proj, proj, 64);
    u.camera_pos[0] = cx;
    u.camera_pos[1] = cy;
    u.camera_pos[2] = cz;
    u.resolution[0] = (float)g_nt_window.fb_width;
    u.resolution[1] = (float)g_nt_window.fb_height;
    u.near_far[0] = 0.1F;
    u.near_far[1] = 100.0F;
    nt_gfx_update_buffer(frame_ubo, &u, sizeof(u));
    nt_gfx_bind_uniform_buffer(frame_ubo, 0);

    // set the player transform from the World, then draw
    float *pos = nt_transform_comp_position(w->player_entity);
    float *scale = nt_transform_comp_scale(w->player_entity);
    float *rot = nt_transform_comp_rotation(w->player_entity);
    pos[0] = w->player_x;
    pos[1] = 0.5F;
    pos[2] = w->player_z;
    scale[0] = scale[1] = scale[2] = 1.0F;
    glm_quatv(rot, w->player_yaw, (vec3){0.0F, 1.0F, 0.0F});
    *nt_transform_comp_dirty(w->player_entity) = true;
    nt_transform_comp_update();

    const uint32_t mesh_id = nt_resource_get(s_cube_mesh);
    nt_render_item_t items[2];
    uint32_t count = 0;

    *nt_mesh_comp_handle(w->player_entity) = (nt_mesh_t){.id = mesh_id};
    items[count].sort_key = nt_sort_key_opaque(s_material.id, mesh_id);
    items[count].entity = w->player_entity.id;
    items[count].batch_key = nt_batch_key(s_material.id, mesh_id);
    count += 1;

    // textured prop (once its material + mesh are ready)
    if (w->prop_spawned && info_tex && info_tex->ready) {
        *nt_mesh_comp_handle(w->prop_entity) = (nt_mesh_t){.id = mesh_id};
        items[count].sort_key = nt_sort_key_opaque(s_material_tex.id, mesh_id);
        items[count].entity = w->prop_entity.id;
        items[count].batch_key = nt_batch_key(s_material_tex.id, mesh_id);
        count += 1;
    }

    nt_mesh_renderer_draw_list(items, count);
}
