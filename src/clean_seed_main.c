#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#if NT_DEVAPI_ENABLED
#include "devapi/nt_devapi.h"
#include "devapi/nt_devapi_net.h"
#include "game_devapi_ui.h"
#endif
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "renderers/nt_shape_renderer.h"
#include "window/nt_window.h"

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLEAN_SEED_DEVAPI_PORT_DEFAULT 9123

typedef struct UiBox {
    float x;
    float y;
    float w;
    float h;
} UiBox;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = CLEAN_SEED_DEVAPI_PORT_DEFAULT;
static int s_window_width = 960;
static int s_window_height = 540;
static UiBox s_cycle_box;

static void ortho(float left, float right, float bottom, float top, float near_z, float far_z, float out[16]) {
    memset(out, 0, sizeof(float) * 16);
    out[0] = 2.0F / (right - left);
    out[5] = 2.0F / (top - bottom);
    out[10] = -2.0F / (far_z - near_z);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(far_z + near_z) / (far_z - near_z);
    out[15] = 1.0F;
}

static bool contains(UiBox box, float x, float y) {
    return x >= box.x && x <= box.x + box.w && y >= box.y && y <= box.y + box.h;
}

static void set_text(char *target, size_t cap, const char *text) {
    if (cap == 0) {
        return;
    }
    (void)snprintf(target, cap, "%s", text);
    target[cap - 1] = '\0';
}

static void sync_seed_labels(void) {
    char label[GAME_STATE_STRING_MAX];
    (void)snprintf(label, sizeof(label), "Clean seed: %s", game_state_shape_name(g_game_state.shape_index));
    set_text(g_game_state.test_label_text, sizeof(g_game_state.test_label_text), label);
    set_text(g_game_state.test_button_text, sizeof(g_game_state.test_button_text), "Cycle seed");
}

static void reset_seed(void) {
    game_state_init_defaults(&g_game_state);
    sync_seed_labels();
}

static void cycle_seed(void) {
    g_game_state.shape_index = (g_game_state.shape_index + 1) % GAME_STATE_SHAPE_COUNT;
    if (g_game_state.test_ui_clicks < GAME_STATE_TEST_UI_CLICKS_MAX) {
        g_game_state.test_ui_clicks += 1;
    }
    sync_seed_labels();
    game_state_mark_dirty();
}

static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--devapi") == 0) {
            s_devapi_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                s_devapi_port = (uint16_t)strtoul(argv[++i], NULL, 10);
            }
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            int width = 0;
            int height = 0;
            if (sscanf(argv[++i], "%dx%d", &width, &height) == 2 && width > 0 && height > 0) {
                s_window_width = width;
                s_window_height = height;
            }
        }
    }
}

static void layout(float w, float h) {
    const float button_w = w < 620.0F ? w * 0.54F : 260.0F;
    const float button_h = 58.0F;
    s_cycle_box = (UiBox){
        .x = (w - button_w) * 0.5F,
        .y = h - 96.0F,
        .w = button_w,
        .h = button_h,
    };
}

static void rect(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void rect_wire(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect_wire((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void circle(float x, float y, float radius, const float color[4]) {
    nt_shape_renderer_circle((float[3]){x, y, 0.0F}, radius, color);
}

static void capsule(float x, float y, float w, float h, const float color[4]) {
    const float r = h * 0.5F;
    rect(x + r, y, w - r * 2.0F, h, color);
    circle(x + r, y + r, r, color);
    circle(x + w - r, y + r, r, color);
}

static void draw_seed_shape(float w, float h) {
    const float cx = w * 0.5F;
    const float cy = h * 0.40F;
    const float size = h < w ? h * 0.18F : w * 0.18F;
    const float shadow[4] = {0.08F, 0.13F, 0.18F, 0.28F};
    const float colors[][4] = {
        {0.08F, 0.58F, 0.88F, 1.0F},
        {0.10F, 0.72F, 0.36F, 1.0F},
        {0.96F, 0.58F, 0.12F, 1.0F},
        {0.92F, 0.24F, 0.32F, 1.0F},
    };
    const float *accent = colors[g_game_state.shape_index % 4];

    capsule(cx - size * 1.18F, cy + size * 0.68F, size * 2.36F, size * 0.26F, shadow);
    switch (g_game_state.shape_index) {
    case GAME_STATE_SHAPE_CUBE:
        rect(cx - size * 0.5F, cy - size * 0.5F, size, size, accent);
        rect_wire(cx - size * 0.5F, cy - size * 0.5F, size, size, (float[4]){1.0F, 1.0F, 1.0F, 0.45F});
        break;
    case GAME_STATE_SHAPE_SPHERE:
        circle(cx, cy, size * 0.58F, accent);
        circle(cx - size * 0.20F, cy - size * 0.20F, size * 0.18F, (float[4]){1.0F, 1.0F, 1.0F, 0.35F});
        break;
    case GAME_STATE_SHAPE_CYLINDER:
        capsule(cx - size * 0.42F, cy - size * 0.70F, size * 0.84F, size * 1.40F, accent);
        rect(cx - size * 0.42F, cy - size * 0.16F, size * 0.84F, size * 0.32F, (float[4]){1.0F, 1.0F, 1.0F, 0.18F});
        break;
    case GAME_STATE_SHAPE_CAPSULE:
    default:
        capsule(cx - size * 0.86F, cy - size * 0.36F, size * 1.72F, size * 0.72F, accent);
        break;
    }
}

static void draw_button(UiBox box) {
    const bool hot = nt_input_mouse_is_down(NT_BUTTON_LEFT) && contains(box, g_nt_input.pointers[0].x, g_nt_input.pointers[0].y);
    const float base[4] = {0.06F, 0.46F, 0.76F, 1.0F};
    const float down[4] = {0.04F, 0.36F, 0.62F, 1.0F};
    const float shine[4] = {1.0F, 1.0F, 1.0F, 0.22F};
    capsule(box.x, box.y + 5.0F, box.w, box.h, (float[4]){0.05F, 0.11F, 0.16F, 0.28F});
    capsule(box.x, box.y, box.w, box.h, hot ? down : base);
    rect(box.x + box.w * 0.14F, box.y + box.h * 0.18F, box.w * 0.72F, box.h * 0.18F, shine);
}

static void draw_scene(float w, float h) {
    float vp[16];
    ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, vp);
    nt_shape_renderer_set_vp(vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(3.0F);

    rect(0.0F, 0.0F, w, h, (float[4]){0.90F, 0.97F, 1.0F, 1.0F});
    rect(0.0F, h * 0.62F, w, h * 0.38F, (float[4]){0.74F, 0.92F, 0.66F, 1.0F});
    rect(w * 0.10F, h * 0.16F, w * 0.80F, h * 0.54F, (float[4]){1.0F, 1.0F, 1.0F, 0.22F});
    rect(w * 0.10F, h * 0.16F, w * 0.80F, 8.0F, (float[4]){0.08F, 0.58F, 0.88F, 1.0F});
    draw_seed_shape(w, h);
    draw_button(s_cycle_box);

    const float progress = (float)(g_game_state.test_ui_clicks % 8) / 7.0F;
    const float meter_w = w < 620.0F ? w * 0.62F : 320.0F;
    const float meter_x = (w - meter_w) * 0.5F;
    const float meter_y = s_cycle_box.y - 34.0F;
    capsule(meter_x, meter_y, meter_w, 14.0F, (float[4]){0.05F, 0.16F, 0.22F, 0.22F});
    capsule(meter_x, meter_y, meter_w * progress, 14.0F, (float[4]){0.10F, 0.72F, 0.36F, 1.0F});
}

static void handle_input(void) {
    if (nt_input_key_is_pressed(NT_KEY_SPACE) || nt_input_key_is_pressed(NT_KEY_ENTER)) {
        cycle_seed();
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
            const nt_pointer_t pointer = g_nt_input.pointers[i];
            if (pointer.active && contains(s_cycle_box, pointer.x, pointer.y)) {
                cycle_seed();
                return;
            }
        }
    }
}

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static cJSON *state_json(void) {
    cJSON *root = game_state_to_json(&g_game_state);
    cJSON_AddStringToObject(root, "runtime", "clean_seed");
    cJSON_AddStringToObject(root, "shape", game_state_shape_name(g_game_state.shape_index));
    return root;
}

/* The engine handler ABI fills a pre-created result_obj; state_json() builds a
   fresh object, so transplant its members. */
static bool seed_emit(cJSON *result_obj, cJSON *src) {
    if (!src) {
        return false;
    }
    cJSON *child = src->child;
    while (child) {
        cJSON *next = child->next;
        cJSON_DetachItemViaPointer(src, child);
        cJSON_AddItemToObject(result_obj, child->string, child);
        child = next;
    }
    cJSON_Delete(src);
    return true;
}

static bool ep_game_state(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    return seed_emit(result_obj, state_json());
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    reset_seed();
    return seed_emit(result_obj, state_json());
}

static bool ep_game_action_cycle(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    cycle_seed();
    return seed_emit(result_obj, state_json());
}

static void register_game_endpoints(void) {
    game_state_register_devapi();
    static const nt_devapi_command_desc descs[] = {
        {"game.state", "game", "Return the current seed state.", "", "state object", "immediate", "none"},
        {"game.reset_playtest", "game", "Reset the seed to defaults.", "", "state object", "immediate", "mutates state"},
        {"game.action.cycle", "game", "Cycle the seed shape.", "", "state object", "immediate", "mutates state"},
    };
    (void)nt_devapi_register(&descs[0], ep_game_state, NULL);
    (void)nt_devapi_register(&descs[1], ep_game_reset_playtest, NULL);
    (void)nt_devapi_register(&descs[2], ep_game_action_cycle, NULL);
    game_devapi_ui_register();
}

static void register_ui_devapi(float w, float h) {
    game_devapi_ui_clear();
    (void)game_devapi_ui_register_node("root", "", "screen", "Game Seed", "Clean seed runtime", 0.0F, 0.0F, w, h, true, true);
    (void)game_devapi_ui_register_node("seed.cycle", "root", "button", "Cycle Seed", g_game_state.test_button_text, s_cycle_box.x, s_cycle_box.y, s_cycle_box.w, s_cycle_box.h, true, true);
    (void)game_devapi_ui_register_node("seed.progress", "root", "meter", "Seed Progress", g_game_state.test_label_text, w * 0.5F - 160.0F, s_cycle_box.y - 34.0F, 320.0F, 14.0F, true, true);
}
#endif

static void frame(void) {
    nt_window_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_update();
    }
#endif
    nt_input_poll();

    const float w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    layout(w, h);
    handle_input();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        register_ui_devapi(w, h);
    }
#endif

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        nt_shape_renderer_restore_gpu();
    }
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.90F, 0.97F, 1.0F, 1.0F}, .clear_depth = 1.0F});
    draw_scene(w, h);
    nt_shape_renderer_flush();
    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Game Seed";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    parse_args(argc, argv);
    reset_seed();

    g_nt_window.title = "Game Seed";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    nt_gfx_init(&gfx_desc);
    nt_shape_renderer_init();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        if (nt_devapi_init() != NT_OK) {
            (void)fprintf(stderr, "Failed to init DevAPI\n");
            s_devapi_enabled = false;
        } else {
            register_game_endpoints();
            if (!nt_devapi_net_start(s_devapi_port)) {
                (void)fprintf(stderr, "Failed to start DevAPI on port %u\n", (unsigned)s_devapi_port);
            }
        }
    }
#endif

#ifdef NT_PLATFORM_WEB
    nt_platform_web_loading_complete();
#endif

    g_nt_app.target_dt = 1.0F / 60.0F;
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_net_stop();
        nt_devapi_shutdown();
    }
#endif
    nt_shape_renderer_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif

    return 0;
}
