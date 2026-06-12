#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "game_storage.h"
#include "generated/game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "renderers/nt_shape_renderer.h"
#include "window/nt_window.h"

#include "cJSON.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#else
#include <glad/gl.h>
#endif

#define DEFAULT_SAVE_KEY "autosave"

static bool s_devapi_enabled;
static uint16_t s_devapi_port = 9123;
static bool s_fresh_state_requested;
static bool s_autosave_enabled = true;
static int s_window_width = 960;
static int s_window_height = 540;
static float s_button_x;
static float s_button_y;
static float s_button_w;
static float s_button_h;

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static char s_pending_capture_path[512];
#endif

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

static void rect(float x, float y, float w, float h, const float color[4]) {
    const float pos[3] = {x, y, 0.0F};
    const float size[2] = {w, h};
    nt_shape_renderer_rect(pos, size, color);
}

static void circle(float x, float y, float radius, const float color[4]) {
    const float center[3] = {x, y, 0.0F};
    nt_shape_renderer_circle(center, radius, color);
}

static void set_seed_label(void) {
    (void)snprintf(g_game_state.test_label_text, sizeof(g_game_state.test_label_text), "Seed clicks: %d", g_game_state.test_ui_clicks);
    (void)snprintf(g_game_state.test_button_text, sizeof(g_game_state.test_button_text), "Cycle color");
}

static void seed_click(void) {
    if (g_game_state.test_ui_clicks < GAME_STATE_TEST_UI_CLICKS_MAX) {
        g_game_state.test_ui_clicks++;
    }
    g_game_state.wallet_soft = g_game_state.test_ui_clicks;
    set_seed_label();
    game_state_mark_dirty();
}

static void register_ui_devapi(float w, float h) {
#if NT_DEVAPI_ENABLED
    nt_devapi_set_view(w, h, w, h);
    nt_devapi_clear_ui_elements();
    (void)nt_devapi_register_ui_node("root", "", "screen", "Game Seed", "Clean project seed", 0.0F, 0.0F, w, h, true, true);
    (void)nt_devapi_register_ui_node("seed.panel", "root", "panel", "Seed Panel", g_game_state.test_label_text, (w - fminf(w - 48.0F, 560.0F)) * 0.5F,
                                     (h - fminf(h - 72.0F, 300.0F)) * 0.5F, fminf(w - 48.0F, 560.0F), fminf(h - 72.0F, 300.0F), true, true);
    (void)nt_devapi_register_ui_node("seed.cycle", "seed.panel", "button", "Cycle", g_game_state.test_button_text, s_button_x, s_button_y, s_button_w, s_button_h, true, true);
#else
    (void)w;
    (void)h;
#endif
}

static void draw_seed_screen(float w, float h) {
    float vp[16];
    ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, vp);

    const int palette = g_game_state.test_ui_clicks % 3;
    const float bg_colors[3][4] = {
        {0.12F, 0.62F, 0.92F, 1.0F},
        {0.45F, 0.82F, 0.38F, 1.0F},
        {0.98F, 0.54F, 0.24F, 1.0F},
    };
    const float *bg = bg_colors[palette];
    const float cloud[4] = {1.00F, 0.96F, 0.82F, 1.0F};
    const float hill[4] = {0.16F, 0.56F, 0.35F, 1.0F};
    const float panel[4] = {0.12F, 0.16F, 0.24F, 1.0F};
    const float panel_top[4] = {1.00F, 0.86F, 0.20F, 1.0F};
    const float button[4] = {0.20F, 0.78F, 0.96F, 1.0F};
    const float button_shadow[4] = {0.06F, 0.33F, 0.48F, 1.0F};
    const float toy_red[4] = {1.00F, 0.25F, 0.34F, 1.0F};
    const float toy_yellow[4] = {1.00F, 0.84F, 0.16F, 1.0F};
    const float toy_blue[4] = {0.20F, 0.44F, 1.00F, 1.0F};
    const float swatches[12][4] = {
        {1.00F, 0.20F, 0.30F, 1.0F},
        {1.00F, 0.52F, 0.18F, 1.0F},
        {1.00F, 0.86F, 0.18F, 1.0F},
        {0.42F, 0.92F, 0.24F, 1.0F},
        {0.16F, 0.78F, 0.52F, 1.0F},
        {0.12F, 0.84F, 0.94F, 1.0F},
        {0.20F, 0.46F, 1.00F, 1.0F},
        {0.50F, 0.32F, 1.00F, 1.0F},
        {0.86F, 0.26F, 0.94F, 1.0F},
        {1.00F, 0.42F, 0.68F, 1.0F},
        {0.96F, 0.96F, 0.92F, 1.0F},
        {0.22F, 0.24F, 0.34F, 1.0F},
    };

    nt_gfx_begin_frame();
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {bg[0], bg[1], bg[2], bg[3]}, .clear_depth = 1.0F});

    nt_shape_renderer_set_vp(vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);

    rect(0.0F, h * 0.60F, w, h * 0.40F, hill);
    circle(w * 0.16F, h * 0.20F, 38.0F, cloud);
    circle(w * 0.22F, h * 0.18F, 52.0F, cloud);
    circle(w * 0.75F, h * 0.17F, 48.0F, cloud);
    circle(w * 0.82F, h * 0.19F, 34.0F, cloud);

    const float panel_w = fminf(w - 48.0F, 560.0F);
    const float panel_h = fminf(h - 72.0F, 300.0F);
    const float panel_x = (w - panel_w) * 0.5F;
    const float panel_y = (h - panel_h) * 0.5F;
    rect(panel_x, panel_y, panel_w, panel_h, panel);
    rect(panel_x, panel_y, panel_w, 10.0F, panel_top);

    const float cx = panel_x + panel_w * 0.5F;
    const float cy = panel_y + panel_h * 0.48F;
    circle(cx - 92.0F, cy + 12.0F, 44.0F, toy_red);
    circle(cx, cy - 18.0F, 58.0F, toy_yellow);
    circle(cx + 98.0F, cy + 18.0F, 44.0F, toy_blue);
    for (int i = 0; i < 12; i++) {
        const float x = panel_x + 34.0F + (float)(i % 6) * 38.0F;
        const float y = panel_y + 34.0F + (float)(i / 6) * 34.0F;
        rect(x, y, 24.0F, 18.0F, swatches[i]);
    }

    s_button_w = 264.0F;
    s_button_h = 46.0F;
    s_button_x = cx - (s_button_w * 0.5F);
    s_button_y = panel_y + panel_h - 88.0F;
    rect(s_button_x, s_button_y + 6.0F, s_button_w, s_button_h, button_shadow);
    rect(s_button_x, s_button_y, s_button_w, s_button_h, button);

    nt_shape_renderer_flush();
    nt_gfx_end_pass();
    nt_gfx_end_frame();

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
    if (s_pending_capture_path[0] != '\0') {
        char path[sizeof(s_pending_capture_path)];
        (void)snprintf(path, sizeof(path), "%s", s_pending_capture_path);
        s_pending_capture_path[0] = '\0';
        FILE *file = fopen(path, "wb");
        if (file) {
            const int width = (int)w;
            const int height = (int)h;
            const size_t bytes = (size_t)width * (size_t)height * 3U;
            unsigned char *pixels = (unsigned char *)malloc(bytes);
            if (pixels) {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadBuffer(GL_BACK);
                glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
                (void)fprintf(file, "P6\n%d %d\n255\n", width, height);
                for (int y = height - 1; y >= 0; y--) {
                    (void)fwrite(pixels + ((size_t)y * (size_t)width * 3U), 1, (size_t)width * 3U, file);
                }
                free(pixels);
            }
            (void)fclose(file);
        }
    }
#endif

    nt_window_swap_buffers();
}

static void handle_clicks(void) {
    const nt_pointer_t pointer = g_nt_input.pointers[0];
    const bool inside = pointer.x >= s_button_x && pointer.x <= s_button_x + s_button_w && pointer.y >= s_button_y && pointer.y <= s_button_y + s_button_h;
    if ((nt_input_mouse_is_pressed(NT_BUTTON_LEFT) && inside) || nt_input_key_is_pressed(NT_KEY_SPACE)) {
        seed_click();
    }
}

#if NT_DEVAPI_ENABLED
static cJSON *state_json(void) {
    cJSON *state = game_state_to_json(&g_game_state);
    cJSON_AddNumberToObject(state, "frame", (double)g_nt_app.frame);
    cJSON_AddBoolToObject(state, "state_dirty", game_state_is_dirty());
    return state;
}

static bool ep_game_state(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    *result = state_json();
    return true;
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)user;
    if (!game_state_reset(&g_game_state, error, error_cap)) {
        return false;
    }
    set_seed_label();
    *result = state_json();
    return true;
}

static bool ep_game_capture_framebuffer(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
#ifdef NT_PLATFORM_WEB
    (void)params;
    (void)result;
    (void)error;
    (void)error_cap;
    return false;
#else
    const cJSON *output = cJSON_GetObjectItemCaseSensitive(params, "output");
    if (!cJSON_IsString(output) || output->valuestring[0] == '\0' || strlen(output->valuestring) >= sizeof(s_pending_capture_path)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "valid output path is required");
        return false;
    }
    (void)snprintf(s_pending_capture_path, sizeof(s_pending_capture_path), "%s", output->valuestring);
    *result = cJSON_CreateObject();
    cJSON_AddStringToObject(*result, "output", s_pending_capture_path);
    cJSON_AddBoolToObject(*result, "pending", true);
    return true;
#endif
}

static void register_game_endpoints(void) {
    nt_devapi_register_builtins();
    game_state_register_devapi();
    nt_devapi_register("game.state", ep_game_state, NULL);
    nt_devapi_register("game.reset_playtest", ep_game_reset_playtest, NULL);
    nt_devapi_register("game.capture.framebuffer", ep_game_capture_framebuffer, NULL);
}
#endif

static void load_default_save_if_available(void) {
    if (!s_autosave_enabled || s_fresh_state_requested) {
        return;
    }
    char error[128];
    char *data = NULL;
    if (!game_storage_load_json(DEFAULT_SAVE_KEY, GAME_STATE_DOCUMENT, &data, error, (int)sizeof(error))) {
        return;
    }
    if (!game_state_load_json_string(&g_game_state, data, error, (int)sizeof(error))) {
        (void)fprintf(stderr, "Autosave load failed: %s\n", error);
    }
    free(data);
}

static void autosave_if_dirty(void) {
    if (!s_autosave_enabled || !game_state_is_dirty()) {
        return;
    }
    char error[128];
    char *data = game_state_save_json_string(&g_game_state, error, (int)sizeof(error));
    if (!data) {
        (void)fprintf(stderr, "Autosave failed: %s\n", error);
        return;
    }
    if (game_storage_save_json(DEFAULT_SAVE_KEY, GAME_STATE_DOCUMENT, data, error, (int)sizeof(error))) {
        game_state_clear_dirty();
    } else {
        (void)fprintf(stderr, "Autosave failed: %s\n", error);
    }
    cJSON_free(data);
}

static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--devapi") == 0 && i + 1 < argc) {
            s_devapi_enabled = true;
            s_devapi_port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            const char *size_arg = argv[++i];
            char *end = NULL;
            long parsed_w = strtol(size_arg, &end, 10);
            if (end && (*end == 'x' || *end == 'X')) {
                char *height_end = NULL;
                long parsed_h = strtol(end + 1, &height_end, 10);
                if (height_end && *height_end == '\0' && parsed_w >= 320 && parsed_h >= 320 && parsed_w <= 4096 && parsed_h <= 4096) {
                    s_window_width = (int)parsed_w;
                    s_window_height = (int)parsed_h;
                }
            }
        } else if (strcmp(argv[i], "--fresh-state") == 0) {
            s_fresh_state_requested = true;
        } else if (strcmp(argv[i], "--disable-autosave") == 0) {
            s_autosave_enabled = false;
        }
    }
}

static void frame(void) {
    nt_window_poll();
    nt_input_poll();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_set_frame((uint64_t)g_nt_app.frame);
        nt_devapi_net_poll();
        nt_devapi_apply_pending();
    }
#endif

    const float w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    handle_clicks();
    register_ui_devapi(w, h);
    draw_seed_screen(w, h);
    autosave_if_dirty();

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Game Seed";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    game_state_init();
    set_seed_label();
    parse_args(argc, argv);
    load_default_save_if_available();

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
        nt_devapi_init();
        register_game_endpoints();
        if (!nt_devapi_net_start(s_devapi_port)) {
            (void)fprintf(stderr, "Failed to start DevAPI on port %u\n", (unsigned)s_devapi_port);
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
