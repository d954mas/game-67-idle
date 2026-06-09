#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "drawable_comp/nt_drawable_comp.h"
#include "entity/nt_entity.h"
#include "game_storage.h"
#include "game_state_actions.h"
#include "generated/game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "math/nt_math.h"
#include "renderers/nt_shape_renderer.h"
#include "transform_comp/nt_transform_comp.h"
#include "window/nt_window.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define TEST_UI_X 24.0F
#define TEST_UI_Y 24.0F
#define TEST_UI_W 260.0F
#define TEST_UI_H 116.0F
#define TEST_UI_LABEL_X 40.0F
#define TEST_UI_LABEL_Y 42.0F
#define TEST_UI_LABEL_W 228.0F
#define TEST_UI_LABEL_H 28.0F
#define TEST_UI_BUTTON_X 40.0F
#define TEST_UI_BUTTON_Y 80.0F
#define TEST_UI_BUTTON_W 168.0F
#define TEST_UI_BUTTON_H 42.0F
#define SETTINGS_BUTTON_W 132.0F
#define SETTINGS_BUTTON_H 42.0F
#define SETTINGS_BUTTON_MARGIN 24.0F
#define SETTINGS_MODAL_W 460.0F
#define SETTINGS_MODAL_H 280.0F
#define SETTINGS_CLOSE_W 42.0F
#define SETTINGS_CLOSE_H 34.0F
#define SETTINGS_SLIDER_X_OFFSET 76.0F
#define SETTINGS_SLIDER_W 308.0F
#define SETTINGS_SLIDER_H 34.0F
#define SETTINGS_SLIDER_MASTER_Y_OFFSET 104.0F
#define SETTINGS_SLIDER_SFX_Y_OFFSET 176.0F
#define GAME_STORAGE_NAMESPACE "game_67_idle"
#define GAME_STORAGE_NATIVE_ROOT "build/saves"
#define GAME_AUTOSAVE_KEY "autosave"

enum { SHAPE_CUBE = 0, SHAPE_SPHERE, SHAPE_CYLINDER, SHAPE_CAPSULE, SHAPE_COUNT };
enum { MODE_SOLID_WIRE = 0, MODE_SOLID, MODE_WIRE, MODE_COUNT };

static nt_entity_t s_shape_entity;
static float s_vel_yaw;
static float s_vel_pitch;
static bool s_grabbed;
static bool s_settings_open;
static int s_active_settings_slider;
static bool s_fresh_state;
static bool s_autosave_disabled;
static bool s_autosave_ready;
static bool s_autosave_load_done;
static bool s_autosave_sync_pending;
static bool s_autosave_sync_failed;
#ifndef NT_PLATFORM_WEB
static bool s_devapi_started;
#endif

static const float s_shape_colors[SHAPE_COUNT][4] = {
    {0.1F, 0.8F, 1.0F, 1.0F},
    {0.8F, 0.25F, 1.0F, 1.0F},
    {1.0F, 0.55F, 0.15F, 1.0F},
    {0.92F, 0.92F, 0.88F, 1.0F},
};

static const float s_wire_color[4] = {0.0F, 0.0F, 0.0F, 1.0F};
static const float s_shape_y = ROOM_H * 0.5F;

static void set_shape_scale(void);
static void set_shape_color(void);

static bool has_arg(int argc, char **argv, const char *name) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static void autosave_init(bool fresh_state, bool disabled) {
    s_fresh_state = fresh_state;
    s_autosave_disabled = disabled;
    s_autosave_load_done = fresh_state;
    if (disabled) {
        s_autosave_ready = false;
        s_autosave_sync_pending = false;
        s_autosave_sync_failed = false;
        return;
    }
#ifdef NT_PLATFORM_WEB
    s_autosave_ready = true;
    s_autosave_sync_pending = false;
    s_autosave_sync_failed = false;
#else
    s_autosave_ready = true;
    s_autosave_sync_pending = false;
    s_autosave_sync_failed = false;
#endif
}

static void autosave_try_load(void) {
    if (s_autosave_disabled) {
        return;
    }
    if (!s_autosave_ready || s_autosave_load_done) {
        return;
    }
    s_autosave_load_done = true;
    if (s_fresh_state || game_state_is_dirty()) {
        return;
    }
    char error[128] = {0};
    char *data = NULL;
    if (!game_storage_load_json(GAME_AUTOSAVE_KEY, GAME_STATE_DOCUMENT, &data, error, (int)sizeof(error))) {
        return;
    }
    if (game_state_load_json_string(&g_game_state, data, error, (int)sizeof(error))) {
        game_state_clear_dirty();
        set_shape_scale();
        set_shape_color();
    }
    free(data);
}

static void autosave_flush_if_dirty(void) {
    if (s_autosave_disabled) {
        return;
    }
    autosave_try_load();
    if (!s_autosave_ready || s_autosave_sync_pending || !game_state_is_dirty()) {
        return;
    }
    char error[128] = {0};
#ifdef NT_PLATFORM_WEB
    char *data = game_state_save_json_string(&g_game_state, error, (int)sizeof(error));
    if (!data) {
        return;
    }
    if (!game_storage_save_json(GAME_AUTOSAVE_KEY, GAME_STATE_DOCUMENT, data, error, (int)sizeof(error))) {
        cJSON_free(data);
        s_autosave_sync_failed = true;
        return;
    }
    cJSON_free(data);
    s_autosave_sync_failed = false;
#else
    char *data = game_state_save_json_string(&g_game_state, error, (int)sizeof(error));
    if (!data) {
        return;
    }
    if (!game_storage_save_json(GAME_AUTOSAVE_KEY, GAME_STATE_DOCUMENT, data, error, (int)sizeof(error))) {
        cJSON_free(data);
        return;
    }
    cJSON_free(data);
#endif
    game_state_clear_dirty();
}

#if NT_DEVAPI_ENABLED
static bool parse_devapi_port(int argc, char **argv, uint16_t *out_port) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--devapi") == 0) {
            long port = 9123;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                port = strtol(argv[i + 1], NULL, 10);
            }
            if (port <= 0 || port > 65535) {
                port = 9123;
            }
            *out_port = (uint16_t)port;
            return true;
        }
        if (strncmp(argv[i], "--devapi=", 9) == 0) {
            long port = strtol(argv[i] + 9, NULL, 10);
            if (port <= 0 || port > 65535) {
                port = 9123;
            }
            *out_port = (uint16_t)port;
            return true;
        }
    }
    return false;
}

static bool ep_game_state(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    float *pos = nt_transform_comp_position(s_shape_entity);
    float *scale = nt_transform_comp_scale(s_shape_entity);
    cJSON *obj = game_state_to_json(&g_game_state);
    cJSON_AddBoolToObject(obj, "grabbed", s_grabbed);
    cJSON_AddNumberToObject(obj, "time", (double)g_nt_app.time);
    cJSON_AddNumberToObject(obj, "frame", (double)g_nt_app.frame);
    cJSON_AddBoolToObject(obj, "state_dirty", game_state_is_dirty());
    cJSON_AddBoolToObject(obj, "autosave_ready", s_autosave_ready);
    cJSON_AddBoolToObject(obj, "autosave_sync_pending", s_autosave_sync_pending);
    cJSON_AddBoolToObject(obj, "autosave_sync_failed", s_autosave_sync_failed);

    cJSON *shape_pos = cJSON_AddArrayToObject(obj, "shape_pos");
    cJSON_AddItemToArray(shape_pos, cJSON_CreateNumber((double)pos[0]));
    cJSON_AddItemToArray(shape_pos, cJSON_CreateNumber((double)pos[1]));
    cJSON_AddItemToArray(shape_pos, cJSON_CreateNumber((double)pos[2]));

    cJSON *shape_scale = cJSON_AddArrayToObject(obj, "shape_scale");
    cJSON_AddItemToArray(shape_scale, cJSON_CreateNumber((double)scale[0]));
    cJSON_AddItemToArray(shape_scale, cJSON_CreateNumber((double)scale[1]));
    cJSON_AddItemToArray(shape_scale, cJSON_CreateNumber((double)scale[2]));

    *result = obj;
    return true;
}

static bool on_game_state_changed(const char *path, void *user, char *error, int error_cap) {
    (void)path;
    (void)user;
    (void)error;
    (void)error_cap;
    set_shape_scale();
    set_shape_color();
    return true;
}
#endif

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

    bool draw_solid = (g_game_state.render_mode_index == MODE_SOLID_WIRE) || (g_game_state.render_mode_index == MODE_SOLID);
    bool draw_wire = (g_game_state.render_mode_index == MODE_SOLID_WIRE) || (g_game_state.render_mode_index == MODE_WIRE);

    switch (g_game_state.shape_index) {
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
    switch (g_game_state.shape_index) {
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
    const float *src = s_shape_colors[g_game_state.shape_index];
    nt_drawable_comp_set_color(s_shape_entity, src[0], src[1], src[2], src[3]);
}

static bool point_in_rect(float px, float py, float x, float y, float w, float h) { return px >= x && px <= x + w && py >= y && py <= y + h; }

static float clamp01(float value) {
    if (value < 0.0F) {
        return 0.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

static float settings_button_x(void) { return (float)g_nt_window.fb_width - SETTINGS_BUTTON_MARGIN - SETTINGS_BUTTON_W; }
static float settings_button_y(void) { return SETTINGS_BUTTON_MARGIN; }
static float settings_modal_x(void) { return ((float)g_nt_window.fb_width - SETTINGS_MODAL_W) * 0.5F; }
static float settings_modal_y(void) { return ((float)g_nt_window.fb_height - SETTINGS_MODAL_H) * 0.5F; }
static float settings_slider_x(void) { return settings_modal_x() + SETTINGS_SLIDER_X_OFFSET; }
static float settings_master_slider_y(void) { return settings_modal_y() + SETTINGS_SLIDER_MASTER_Y_OFFSET; }
static float settings_sfx_slider_y(void) { return settings_modal_y() + SETTINGS_SLIDER_SFX_Y_OFFSET; }

static void set_master_volume_from_x(float x) {
    char error[128] = {0};
    float volume = clamp01((x - settings_slider_x()) / SETTINGS_SLIDER_W);
    (void)game_state_action_set_master_volume(&g_game_state, volume, error, (int)sizeof(error));
}

static void set_sfx_volume_from_x(float x) {
    char error[128] = {0};
    float volume = clamp01((x - settings_slider_x()) / SETTINGS_SLIDER_W);
    (void)game_state_action_set_sfx_volume(&g_game_state, volume, error, (int)sizeof(error));
}

static void register_test_ui(void) {
    char text[64];
    nt_devapi_register_ui_element("scene.viewport", "Scene viewport", 0.0F, 0.0F, (float)g_nt_window.fb_width, (float)g_nt_window.fb_height);
    nt_devapi_register_ui_node("test.ui", "", "panel", "Test UI", "", TEST_UI_X, TEST_UI_Y, TEST_UI_W, TEST_UI_H, true, true);
    nt_devapi_register_ui_node("test.label", "test.ui", "label", "Test Label", g_game_state.test_label_text, TEST_UI_LABEL_X, TEST_UI_LABEL_Y, TEST_UI_LABEL_W, TEST_UI_LABEL_H, true, false);
    nt_devapi_register_ui_node("test.button", "test.ui", "button", "Test Button", g_game_state.test_button_text, TEST_UI_BUTTON_X, TEST_UI_BUTTON_Y, TEST_UI_BUTTON_W, TEST_UI_BUTTON_H, true, true);
    nt_devapi_register_ui_node("settings.open", "", "button", "Settings", "Settings", settings_button_x(), settings_button_y(), SETTINGS_BUTTON_W, SETTINGS_BUTTON_H, true, true);
    if (!s_settings_open) {
        return;
    }
    nt_devapi_register_ui_node("settings.overlay", "", "overlay", "Settings Overlay", "", 0.0F, 0.0F, (float)g_nt_window.fb_width, (float)g_nt_window.fb_height, true, true);
    nt_devapi_register_ui_node("settings.modal", "settings.overlay", "dialog", "Settings", "", settings_modal_x(), settings_modal_y(), SETTINGS_MODAL_W, SETTINGS_MODAL_H, true, true);
    nt_devapi_register_ui_node("settings.close", "settings.modal", "button", "Close", "X", settings_modal_x() + SETTINGS_MODAL_W - SETTINGS_CLOSE_W - 18.0F, settings_modal_y() + 18.0F, SETTINGS_CLOSE_W, SETTINGS_CLOSE_H, true, true);
    (void)snprintf(text, sizeof(text), "%.2f", (double)g_game_state.settings_master_volume);
    nt_devapi_register_ui_node("settings.master_volume", "settings.modal", "slider", "Master Volume", text, settings_slider_x(), settings_master_slider_y(), SETTINGS_SLIDER_W, SETTINGS_SLIDER_H, true, true);
    (void)snprintf(text, sizeof(text), "%.2f", (double)g_game_state.settings_sfx_volume);
    nt_devapi_register_ui_node("settings.sfx_volume", "settings.modal", "slider", "SFX Volume", text, settings_slider_x(), settings_sfx_slider_y(), SETTINGS_SLIDER_W, SETTINGS_SLIDER_H, true, true);
}

static void update_test_ui_interaction(void) {
    if (!nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        return;
    }
    const float px = g_nt_input.pointers[0].x;
    const float py = g_nt_input.pointers[0].y;
    if (!point_in_rect(px, py, TEST_UI_BUTTON_X, TEST_UI_BUTTON_Y, TEST_UI_BUTTON_W, TEST_UI_BUTTON_H)) {
        return;
    }
    char error[128] = {0};
    (void)game_state_action_test_ui_click(&g_game_state, error, (int)sizeof(error));
}

static bool update_settings_ui_interaction(void) {
    const float px = g_nt_input.pointers[0].x;
    const float py = g_nt_input.pointers[0].y;
    const float open_x = settings_button_x();
    const float open_y = settings_button_y();
    const float modal_x = settings_modal_x();
    const float modal_y = settings_modal_y();
    const float close_x = modal_x + SETTINGS_MODAL_W - SETTINGS_CLOSE_W - 18.0F;
    const float close_y = modal_y + 18.0F;
    bool blocks_scene = s_settings_open || point_in_rect(px, py, open_x, open_y, SETTINGS_BUTTON_W, SETTINGS_BUTTON_H);

    if (nt_input_key_is_pressed(NT_KEY_S)) {
        s_settings_open = !s_settings_open;
        s_active_settings_slider = 0;
        return true;
    }
    if (nt_input_mouse_is_released(NT_BUTTON_LEFT)) {
        s_active_settings_slider = 0;
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT) && point_in_rect(px, py, open_x, open_y, SETTINGS_BUTTON_W, SETTINGS_BUTTON_H)) {
        s_settings_open = true;
        return true;
    }
    if (!s_settings_open) {
        return blocks_scene;
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT) && point_in_rect(px, py, close_x, close_y, SETTINGS_CLOSE_W, SETTINGS_CLOSE_H)) {
        s_settings_open = false;
        s_active_settings_slider = 0;
        return true;
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT) && point_in_rect(px, py, settings_slider_x(), settings_master_slider_y(), SETTINGS_SLIDER_W, SETTINGS_SLIDER_H)) {
        s_active_settings_slider = 1;
    } else if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT) && point_in_rect(px, py, settings_slider_x(), settings_sfx_slider_y(), SETTINGS_SLIDER_W, SETTINGS_SLIDER_H)) {
        s_active_settings_slider = 2;
    }
    if (nt_input_mouse_is_down(NT_BUTTON_LEFT)) {
        if (s_active_settings_slider == 1) {
            set_master_volume_from_x(px);
        } else if (s_active_settings_slider == 2) {
            set_sfx_volume_from_x(px);
        }
    }
    return blocks_scene;
}

static void draw_screen_rect(float x, float y, float w, float h, const float color[4]) {
    float px = x + (w * 0.5F);
    float py = (float)g_nt_window.fb_height - y - (h * 0.5F);
    float pos[3] = {px, py, 0.0F};
    float size[2] = {w, h};
    nt_shape_renderer_rect(pos, size, color);
}

static void draw_screen_rect_wire(float x, float y, float w, float h, const float color[4]) {
    float px = x + (w * 0.5F);
    float py = (float)g_nt_window.fb_height - y - (h * 0.5F);
    float pos[3] = {px, py, 0.0F};
    float size[2] = {w, h};
    nt_shape_renderer_rect_wire(pos, size, color);
}

static void draw_slider(float x, float y, float w, float value, const float accent[4]) {
    const float track_col[4] = {0.18F, 0.20F, 0.24F, 1.0F};
    const float knob_col[4] = {0.94F, 0.96F, 0.90F, 1.0F};
    draw_screen_rect(x, y + 14.0F, w, 6.0F, track_col);
    draw_screen_rect(x, y + 14.0F, w * clamp01(value), 6.0F, accent);
    draw_screen_rect(x + (w * clamp01(value)) - 7.0F, y + 5.0F, 14.0F, 24.0F, knob_col);
}

static void draw_settings_ui(void) {
    mat4 ortho;
    float cam_pos[3] = {0.0F, 0.0F, 1.0F};
    glm_ortho(0.0F, (float)g_nt_window.fb_width, 0.0F, (float)g_nt_window.fb_height, -1.0F, 1.0F, ortho);
    nt_shape_renderer_set_vp((float *)ortho);
    nt_shape_renderer_set_cam_pos(cam_pos);
    nt_shape_renderer_set_depth(false);

    const float button_col[4] = {0.14F, 0.32F, 0.42F, 1.0F};
    const float button_wire[4] = {0.55F, 0.88F, 0.92F, 1.0F};
    draw_screen_rect(settings_button_x(), settings_button_y(), SETTINGS_BUTTON_W, SETTINGS_BUTTON_H, button_col);
    draw_screen_rect_wire(settings_button_x(), settings_button_y(), SETTINGS_BUTTON_W, SETTINGS_BUTTON_H, button_wire);

    if (!s_settings_open) {
        return;
    }

    const float overlay_col[4] = {0.02F, 0.02F, 0.025F, 0.82F};
    const float modal_col[4] = {0.08F, 0.09F, 0.11F, 1.0F};
    const float modal_wire[4] = {0.76F, 0.80F, 0.68F, 1.0F};
    const float close_col[4] = {0.42F, 0.14F, 0.16F, 1.0F};
    const float accent_master[4] = {0.20F, 0.78F, 0.66F, 1.0F};
    const float accent_sfx[4] = {0.92F, 0.58F, 0.22F, 1.0F};
    float modal_x = settings_modal_x();
    float modal_y = settings_modal_y();

    draw_screen_rect(0.0F, 0.0F, (float)g_nt_window.fb_width, (float)g_nt_window.fb_height, overlay_col);
    draw_screen_rect(modal_x, modal_y, SETTINGS_MODAL_W, SETTINGS_MODAL_H, modal_col);
    draw_screen_rect_wire(modal_x, modal_y, SETTINGS_MODAL_W, SETTINGS_MODAL_H, modal_wire);
    draw_screen_rect(modal_x + SETTINGS_MODAL_W - SETTINGS_CLOSE_W - 18.0F, modal_y + 18.0F, SETTINGS_CLOSE_W, SETTINGS_CLOSE_H, close_col);
    draw_slider(settings_slider_x(), settings_master_slider_y(), SETTINGS_SLIDER_W, g_game_state.settings_master_volume, accent_master);
    draw_slider(settings_slider_x(), settings_sfx_slider_y(), SETTINGS_SLIDER_W, g_game_state.settings_sfx_volume, accent_sfx);
}

static void frame(void) {
    nt_window_poll();
    nt_input_poll();
    nt_devapi_set_frame(g_nt_app.frame);
    nt_devapi_set_view((float)g_nt_window.fb_width, (float)g_nt_window.fb_height, (float)g_nt_window.width, (float)g_nt_window.height);
    nt_devapi_clear_ui_elements();
    register_test_ui();
    nt_devapi_net_poll();
    nt_devapi_apply_pending();
    autosave_try_load();

    float dt = g_nt_app.dt;
    update_test_ui_interaction();
    bool ui_blocks_pointer = update_settings_ui_interaction();

    if (nt_input_key_is_pressed(NT_KEY_A)) {
        char error[128] = {0};
        (void)game_state_action_shape_prev(&g_game_state, error, (int)sizeof(error));
        set_shape_scale();
        set_shape_color();
    }
    if (nt_input_key_is_pressed(NT_KEY_D)) {
        char error[128] = {0};
        (void)game_state_action_shape_next(&g_game_state, error, (int)sizeof(error));
        set_shape_scale();
        set_shape_color();
    }
    if (nt_input_key_is_pressed(NT_KEY_W)) {
        char error[128] = {0};
        (void)game_state_action_render_mode_next(&g_game_state, error, (int)sizeof(error));
    }
    if (nt_input_key_is_pressed(NT_KEY_R)) {
        glm_quat_identity(nt_transform_comp_rotation(s_shape_entity));
        *nt_transform_comp_dirty(s_shape_entity) = true;
        s_vel_yaw = 0;
        s_vel_pitch = 0;
    }
#ifndef NT_PLATFORM_WEB
    if (nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        if (s_settings_open) {
            s_settings_open = false;
            s_active_settings_slider = 0;
        } else {
            nt_app_quit();
        }
    }
#endif

    s_grabbed = nt_input_mouse_is_down(NT_BUTTON_LEFT) && !ui_blocks_pointer;
    if (s_grabbed) {
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
    } else {
        apply_rotation(AUTO_SPIN_SPEED * dt, 0);
    }

    float wheel = g_nt_input.pointers[0].wheel_dy;
    if (fabsf(wheel) > 0.001F) {
        char error[128] = {0};
        (void)game_state_action_camera_zoom(&g_game_state, wheel, ZOOM_SPEED, error, (int)sizeof(error));
    }

    nt_transform_comp_update();

    float aspect = 1.0F;
    if (g_nt_window.fb_height > 0) {
        aspect = (float)g_nt_window.fb_width / (float)g_nt_window.fb_height;
    }

    vec3 eye = {0, s_shape_y + 0.5F, g_game_state.camera_distance};
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
    draw_settings_ui();
    nt_shape_renderer_flush();
    nt_gfx_end_pass();
    nt_gfx_end_frame();

    nt_window_swap_buffers();
    autosave_flush_if_dirty();
}

int main(int argc, char **argv) {
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
    game_state_init();
    game_storage_init(&(GameStorageConfig){
        .namespace_name = GAME_STORAGE_NAMESPACE,
        .native_root = GAME_STORAGE_NATIVE_ROOT,
    });

    s_shape_entity = nt_entity_create();
    nt_transform_comp_add(s_shape_entity);
    nt_transform_comp_position(s_shape_entity)[1] = s_shape_y;
    nt_drawable_comp_add(s_shape_entity);

    set_shape_scale();
    set_shape_color();
    autosave_init(has_arg(argc, argv, "--fresh-state"), has_arg(argc, argv, "--disable-autosave"));

#ifdef NT_PLATFORM_WEB
    nt_platform_web_loading_complete();
#endif

#if NT_DEVAPI_ENABLED
    game_state_set_changed_callback(on_game_state_changed, NULL);
    uint16_t devapi_port = 0;
    if (parse_devapi_port(argc, argv, &devapi_port)) {
        nt_devapi_init();
        nt_devapi_register_builtins();
        nt_devapi_register("game.state", ep_game_state, NULL);
        game_state_register_devapi();
        s_devapi_started = nt_devapi_net_start(devapi_port);
    }
#else
    (void)argc;
    (void)argv;
#endif

    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
    if (s_devapi_started) {
        nt_devapi_net_stop();
    }
    nt_devapi_shutdown();
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
