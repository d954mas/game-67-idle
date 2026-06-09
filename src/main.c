#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "drawable_comp/nt_drawable_comp.h"
#include "entity/nt_entity.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "math/nt_math.h"
#include "renderers/nt_shape_renderer.h"
#include "transform_comp/nt_transform_comp.h"
#include "window/nt_window.h"

#include <math.h>

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#define ROOM_W 18.0F
#define ROOM_H 9.0F
#define ROOM_D 18.0F
#define GRID_STEP 1.0F

#define MOUSE_SENS 0.005F
#define AUTO_SPIN_SPEED 0.55F
#define INERTIA_DECAY 0.94F
#define ZOOM_MIN 2.5F
#define ZOOM_MAX 10.0F
#define ZOOM_SPEED 0.01F
#define FOV_DEG 70.0F
#define VEL_THRESHOLD 0.0001F

enum { SHAPE_CUBE = 0, SHAPE_SPHERE, SHAPE_CYLINDER, SHAPE_CAPSULE, SHAPE_COUNT };
enum { MODE_SOLID_WIRE = 0, MODE_SOLID, MODE_WIRE, MODE_COUNT };

static nt_entity_t s_shape_entity;
static float s_vel_yaw;
static float s_vel_pitch;
static float s_cam_dist = 6.0F;
static int s_current_shape;
static int s_render_mode;
static bool s_grabbed;

static const float s_shape_colors[SHAPE_COUNT][4] = {
    {0.1F, 0.8F, 1.0F, 1.0F},
    {0.8F, 0.25F, 1.0F, 1.0F},
    {1.0F, 0.55F, 0.15F, 1.0F},
    {0.92F, 0.92F, 0.88F, 1.0F},
};

static const float s_wire_color[4] = {0.0F, 0.0F, 0.0F, 1.0F};
static const float s_shape_y = ROOM_H * 0.5F;

static void draw_room(void) {
    float hw = ROOM_W * 0.5F;
    float hd = ROOM_D * 0.5F;

    float floor_col[4] = {0.14F, 0.15F, 0.18F, 1.0F};
    float floor_pos[3] = {0, 0, 0};
    float floor_size[2] = {ROOM_W, ROOM_D};
    float floor_rot[4] = {0.7071068F, 0, 0, 0.7071068F};
    nt_shape_renderer_rect_rot(floor_pos, floor_size, floor_rot, floor_col);

    float grid_col[4] = {0.28F, 0.30F, 0.34F, 1.0F};
    int grid_nx = (int)(ROOM_W / GRID_STEP) + 1;
    int grid_nz = (int)(ROOM_D / GRID_STEP) + 1;
    for (int ix = 0; ix < grid_nx; ix++) {
        float x = -hw + ((float)ix * GRID_STEP);
        float a[3] = {x, 0.001F, -hd};
        float b[3] = {x, 0.001F, hd};
        nt_shape_renderer_line(a, b, grid_col);
    }
    for (int iz = 0; iz < grid_nz; iz++) {
        float z = -hd + ((float)iz * GRID_STEP);
        float a[3] = {-hw, 0.001F, z};
        float b[3] = {hw, 0.001F, z};
        nt_shape_renderer_line(a, b, grid_col);
    }

    float ceil_col[4] = {0.12F, 0.12F, 0.17F, 1.0F};
    float ceil_pos[3] = {0, ROOM_H, 0};
    nt_shape_renderer_rect_rot(ceil_pos, floor_size, floor_rot, ceil_col);

    float back_col[4] = {0.18F, 0.16F, 0.20F, 1.0F};
    float side_col[4] = {0.14F, 0.18F, 0.20F, 1.0F};
    {
        float pos[3] = {0, ROOM_H * 0.5F, -hd};
        float size[2] = {ROOM_W, ROOM_H};
        nt_shape_renderer_rect(pos, size, back_col);
    }
    {
        float pos[3] = {0, ROOM_H * 0.5F, hd};
        float size[2] = {ROOM_W, ROOM_H};
        nt_shape_renderer_rect(pos, size, back_col);
    }
    {
        float pos[3] = {-hw, ROOM_H * 0.5F, 0};
        float size[2] = {ROOM_D, ROOM_H};
        float rot[4] = {0, 0.7071068F, 0, 0.7071068F};
        nt_shape_renderer_rect_rot(pos, size, rot, side_col);
    }
    {
        float pos[3] = {hw, ROOM_H * 0.5F, 0};
        float size[2] = {ROOM_D, ROOM_H};
        float rot[4] = {0, 0.7071068F, 0, 0.7071068F};
        nt_shape_renderer_rect_rot(pos, size, rot, side_col);
    }
}

static void draw_shape(void) {
    if (!*nt_drawable_comp_visible(s_shape_entity)) {
        return;
    }

    float *pos = nt_transform_comp_position(s_shape_entity);
    float *rot = nt_transform_comp_rotation(s_shape_entity);
    const float *col = nt_drawable_comp_color(s_shape_entity);
    float *scale = nt_transform_comp_scale(s_shape_entity);

    bool draw_solid = (s_render_mode == MODE_SOLID_WIRE) || (s_render_mode == MODE_SOLID);
    bool draw_wire = (s_render_mode == MODE_SOLID_WIRE) || (s_render_mode == MODE_WIRE);

    switch (s_current_shape) {
    case SHAPE_CUBE: {
        float size[3] = {scale[0], scale[1], scale[2]};
        if (draw_solid) {
            nt_shape_renderer_cube_rot(pos, size, rot, col);
        }
        if (draw_wire) {
            nt_shape_renderer_cube_wire_rot(pos, size, rot, s_wire_color);
        }
        break;
    }
    case SHAPE_SPHERE:
        if (draw_solid) {
            nt_shape_renderer_sphere_rot(pos, scale[0], rot, col);
        }
        if (draw_wire) {
            nt_shape_renderer_sphere_wire_rot(pos, scale[0], rot, s_wire_color);
        }
        break;
    case SHAPE_CYLINDER:
        if (draw_solid) {
            nt_shape_renderer_cylinder_rot(pos, scale[0], scale[1], rot, col);
        }
        if (draw_wire) {
            nt_shape_renderer_cylinder_wire_rot(pos, scale[0], scale[1], rot, s_wire_color);
        }
        break;
    case SHAPE_CAPSULE:
        if (draw_solid) {
            nt_shape_renderer_capsule_rot(pos, scale[0], scale[1], rot, col);
        }
        if (draw_wire) {
            nt_shape_renderer_capsule_wire_rot(pos, scale[0], scale[1], rot, s_wire_color);
        }
        break;
    default:
        break;
    }
}

static void apply_rotation(float yaw, float pitch) {
    float *local_rot = nt_transform_comp_rotation(s_shape_entity);
    versor q_yaw;
    versor q_pitch;
    versor tmp;
    vec3 axis_y = {0, 1, 0};
    vec3 axis_x = {1, 0, 0};

    glm_quatv(q_yaw, yaw, axis_y);
    glm_quatv(q_pitch, pitch, axis_x);
    glm_quat_mul(q_yaw, local_rot, tmp);
    glm_quat_mul(q_pitch, tmp, local_rot);
    glm_quat_normalize(local_rot);
    *nt_transform_comp_dirty(s_shape_entity) = true;
}

static void set_shape_scale(void) {
    float *scale = nt_transform_comp_scale(s_shape_entity);
    switch (s_current_shape) {
    case SHAPE_CUBE:
        glm_vec3_copy((vec3){1.5F, 1.5F, 1.5F}, scale);
        break;
    case SHAPE_SPHERE:
        glm_vec3_copy((vec3){1.05F, 1.05F, 1.05F}, scale);
        break;
    case SHAPE_CYLINDER:
        glm_vec3_copy((vec3){0.65F, 2.0F, 0.65F}, scale);
        break;
    case SHAPE_CAPSULE:
        glm_vec3_copy((vec3){0.45F, 1.6F, 0.45F}, scale);
        break;
    default:
        break;
    }
    *nt_transform_comp_dirty(s_shape_entity) = true;
}

static void set_shape_color(void) {
    const float *src = s_shape_colors[s_current_shape];
    nt_drawable_comp_set_color(s_shape_entity, src[0], src[1], src[2], src[3]);
}

static void frame(void) {
    nt_window_poll();
    nt_input_poll();

    float dt = g_nt_app.dt;

    if (nt_input_key_is_pressed(NT_KEY_A)) {
        s_current_shape = (s_current_shape + SHAPE_COUNT - 1) % SHAPE_COUNT;
        set_shape_scale();
        set_shape_color();
    }
    if (nt_input_key_is_pressed(NT_KEY_D)) {
        s_current_shape = (s_current_shape + 1) % SHAPE_COUNT;
        set_shape_scale();
        set_shape_color();
    }
    if (nt_input_key_is_pressed(NT_KEY_W)) {
        s_render_mode = (s_render_mode + 1) % MODE_COUNT;
    }
    if (nt_input_key_is_pressed(NT_KEY_R)) {
        glm_quat_identity(nt_transform_comp_rotation(s_shape_entity));
        *nt_transform_comp_dirty(s_shape_entity) = true;
        s_vel_yaw = 0;
        s_vel_pitch = 0;
    }
#ifndef NT_PLATFORM_WEB
    if (nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif

    if (nt_input_mouse_is_down(NT_BUTTON_LEFT)) {
        s_grabbed = true;
        float dx = g_nt_input.pointers[0].dx;
        float dy = g_nt_input.pointers[0].dy;
        float new_yaw = dx * MOUSE_SENS;
        float new_pitch = dy * MOUSE_SENS;
        s_vel_yaw = (new_yaw * 0.6F) + (s_vel_yaw * 0.4F);
        s_vel_pitch = (new_pitch * 0.6F) + (s_vel_pitch * 0.4F);
        apply_rotation(s_vel_yaw, s_vel_pitch);
    } else if ((fabsf(s_vel_yaw) > VEL_THRESHOLD) || (fabsf(s_vel_pitch) > VEL_THRESHOLD)) {
        apply_rotation(s_vel_yaw, s_vel_pitch);
        float decay = powf(INERTIA_DECAY, dt * 60.0F);
        s_vel_yaw *= decay;
        s_vel_pitch *= decay;
        if (fabsf(s_vel_yaw) < VEL_THRESHOLD) {
            s_vel_yaw = 0;
        }
        if (fabsf(s_vel_pitch) < VEL_THRESHOLD) {
            s_vel_pitch = 0;
        }
    } else if (!s_grabbed) {
        apply_rotation(AUTO_SPIN_SPEED * dt, 0);
    }

    float wheel = g_nt_input.pointers[0].wheel_dy;
    if (fabsf(wheel) > 0.001F) {
        s_cam_dist += wheel * ZOOM_SPEED;
        if (s_cam_dist < ZOOM_MIN) {
            s_cam_dist = ZOOM_MIN;
        }
        if (s_cam_dist > ZOOM_MAX) {
            s_cam_dist = ZOOM_MAX;
        }
    }

    nt_transform_comp_update();

    float aspect = 1.0F;
    if (g_nt_window.fb_height > 0) {
        aspect = (float)g_nt_window.fb_width / (float)g_nt_window.fb_height;
    }

    vec3 eye = {0, s_shape_y + 0.5F, s_cam_dist};
    vec3 center = {0, s_shape_y, 0};
    vec3 up = {0, 1, 0};

    mat4 view;
    mat4 proj;
    mat4 vp;
    glm_lookat(eye, center, up, view);
    glm_perspective(glm_rad(FOV_DEG), aspect, 0.1F, 50.0F, proj);
    glm_mat4_mul(proj, view, vp);

    float cam_pos[3] = {eye[0], eye[1], eye[2]};

    nt_gfx_begin_frame();
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.045F, 0.05F, 0.07F, 1.0F}, .clear_depth = 1.0F});

    nt_shape_renderer_set_vp((float *)vp);
    nt_shape_renderer_set_cam_pos(cam_pos);
    nt_shape_renderer_set_depth(true);

    draw_room();
    draw_shape();

    nt_shape_renderer_flush();
    nt_gfx_end_pass();
    nt_gfx_end_frame();

    nt_window_swap_buffers();
}

int main(void) {
    nt_engine_config_t config = {0};
    config.app_name = "game_67_idle";
    config.version = 1;

    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    g_nt_window.width = 960;
    g_nt_window.height = 640;
    nt_window_init();
    nt_input_init();
    nt_gfx_init(&(nt_gfx_desc_t){.max_shaders = 32, .max_pipelines = 16, .max_buffers = 128, .max_textures = 16, .max_meshes = 64, .depth = true});
    nt_shape_renderer_init();

    nt_entity_init(&(nt_entity_desc_t){.max_entities = 64});
    nt_transform_comp_init(&(nt_transform_comp_desc_t){.capacity = 64});
    nt_drawable_comp_init(&(nt_drawable_comp_desc_t){.capacity = 64});

    s_shape_entity = nt_entity_create();
    nt_transform_comp_add(s_shape_entity);
    nt_transform_comp_position(s_shape_entity)[1] = s_shape_y;
    nt_drawable_comp_add(s_shape_entity);

#ifdef NT_PLATFORM_WEB
    nt_platform_web_loading_complete();
#endif

    s_current_shape = SHAPE_CUBE;
    s_render_mode = MODE_SOLID_WIRE;
    set_shape_scale();
    set_shape_color();

    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
    nt_drawable_comp_shutdown();
    nt_transform_comp_shutdown();
    nt_entity_shutdown();
    nt_shape_renderer_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif
    return 0;
}

