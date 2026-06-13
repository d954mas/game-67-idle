#include "app/nt_app.h"
#include "core/nt_core.h"
#include "devapi/nt_devapi.h"
#include "game_audio.h"
#include "game_state_actions.h"
#include "game_storage.h"
#include "generated/game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "renderers/nt_shape_renderer.h"
#include "window/nt_window.h"

#include "cJSON.h"

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

typedef struct UiBox {
    float x;
    float y;
    float w;
    float h;
} UiBox;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = 9123;
static bool s_fresh_state_requested;
static bool s_autosave_enabled = true;
static int s_window_width = 960;
static int s_window_height = 540;
static UiBox s_cycle_box;

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

static void rect_wire(float x, float y, float w, float h, const float color[4]) {
    const float pos[3] = {x, y, 0.0F};
    const float size[2] = {w, h};
    nt_shape_renderer_rect_wire(pos, size, color);
}

static void circle(float x, float y, float radius, const float color[4]) {
    const float center[3] = {x, y, 0.0F};
    nt_shape_renderer_circle(center, radius, color);
}

static bool contains(UiBox box, float x, float y) {
    return x >= box.x && x <= box.x + box.w && y >= box.y && y <= box.y + box.h;
}

static void layout(float w, float h) {
    const float button_w = w < 560.0F ? w * 0.58F : 280.0F;
    const float button_h = h < 420.0F ? 72.0F : 86.0F;
    s_cycle_box = (UiBox){
        w * 0.5F - button_w * 0.5F,
        h * 0.66F,
        button_w,
        button_h,
    };
}

static void seed_cycle(void) {
    game_seed_cycle(&g_game_state);
    game_audio_play(GAME_AUDIO_CUE_CLICK);
    game_audio_set_volume(g_game_state.settings_master_volume, g_game_state.settings_sfx_volume);
}

static void handle_input(void) {
    if (nt_input_key_is_pressed(NT_KEY_SPACE) || nt_input_key_is_pressed(NT_KEY_ENTER)) {
        seed_cycle();
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
            const nt_pointer_t pointer = g_nt_input.pointers[i];
            if (pointer.active && pointer.buttons[NT_BUTTON_LEFT].is_pressed && contains(s_cycle_box, pointer.x, pointer.y)) {
                seed_cycle();
                break;
            }
        }
    }
}

static void draw_seed_shape(float w, float h) {
    const float cx = w * 0.5F;
    const float cy = h * 0.40F;
    const float size = h < w ? h * 0.18F : w * 0.18F;
    const float colors[][4] = {
        {0.25F, 0.52F, 0.92F, 1.0F},
        {0.10F, 0.72F, 0.48F, 1.0F},
        {0.96F, 0.55F, 0.16F, 1.0F},
        {0.82F, 0.28F, 0.50F, 1.0F},
    };
    const float *accent = colors[g_game_state.shape_index % 4];
    const float shadow[4] = {0.06F, 0.08F, 0.11F, 0.28F};
    const float line[4] = {0.95F, 0.97F, 1.0F, 0.85F};

    circle(cx + size * 0.08F, cy + size * 0.12F, size * 0.86F, shadow);
    switch (g_game_state.shape_index) {
    case GAME_STATE_SHAPE_SPHERE:
        circle(cx, cy, size * 0.70F, accent);
        circle(cx - size * 0.20F, cy - size * 0.22F, size * 0.18F, line);
        break;
    case GAME_STATE_SHAPE_CYLINDER:
        rect(cx - size * 0.62F, cy - size * 0.48F, size * 1.24F, size * 0.96F, accent);
        circle(cx, cy - size * 0.48F, size * 0.62F, accent);
        circle(cx, cy + size * 0.48F, size * 0.62F, line);
        break;
    case GAME_STATE_SHAPE_CAPSULE:
        rect(cx - size * 0.46F, cy - size * 0.72F, size * 0.92F, size * 1.44F, accent);
        circle(cx, cy - size * 0.72F, size * 0.46F, accent);
        circle(cx, cy + size * 0.72F, size * 0.46F, line);
        break;
    case GAME_STATE_SHAPE_CUBE:
    default:
        rect(cx - size * 0.62F, cy - size * 0.62F, size * 1.24F, size * 1.24F, accent);
        rect_wire(cx - size * 0.50F, cy - size * 0.50F, size, size, line);
        break;
    }
}

static void draw_game(float w, float h) {
    const float vp[16] = {0};
    float mutable_vp[16];
    ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, mutable_vp);

    const float bg_a = 0.08F + 0.03F * (float)(g_game_state.shape_index % 2);
    nt_gfx_begin_frame();
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {bg_a, 0.10F, 0.13F, 1.0F}, .clear_depth = 1.0F});
    (void)vp;

    nt_shape_renderer_set_vp(mutable_vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(3.0F);

    rect(w * 0.08F, h * 0.12F, w * 0.84F, h * 0.72F, (float[4]){0.12F, 0.15F, 0.20F, 1.0F});
    rect(w * 0.08F, h * 0.12F, w * 0.84F, 8.0F, (float[4]){0.34F, 0.63F, 0.96F, 1.0F});
    for (int i = 0; i < 12; ++i) {
        const float t = (float)i / 11.0F;
        const float x = w * 0.13F + t * w * 0.74F;
        const float y = h * 0.18F + (float)(i % 3) * 18.0F;
        const float swatch[4] = {0.18F + 0.62F * t, 0.34F + 0.22F * (float)(i % 4), 0.86F - 0.48F * t, 1.0F};
        rect(x, y, 18.0F, 10.0F + (float)(i % 4) * 5.0F, swatch);
    }
    draw_seed_shape(w, h);

    const bool pressed = nt_input_mouse_is_down(NT_BUTTON_LEFT);
    rect(s_cycle_box.x, s_cycle_box.y + 5.0F, s_cycle_box.w, s_cycle_box.h, (float[4]){0.04F, 0.05F, 0.07F, 0.7F});
    rect(s_cycle_box.x, s_cycle_box.y + (pressed ? 4.0F : 0.0F), s_cycle_box.w, s_cycle_box.h, (float[4]){0.18F, 0.45F, 0.84F, 1.0F});
    rect_wire(s_cycle_box.x + 8.0F, s_cycle_box.y + 8.0F, s_cycle_box.w - 16.0F, s_cycle_box.h - 16.0F, (float[4]){0.92F, 0.96F, 1.0F, 0.85F});

    const float meter_w = s_cycle_box.w;
    const float filled = meter_w * ((float)(g_game_state.test_ui_clicks % 8) / 7.0F);
    rect(s_cycle_box.x, s_cycle_box.y + s_cycle_box.h + 18.0F, meter_w, 10.0F, (float[4]){0.05F, 0.06F, 0.08F, 1.0F});
    rect(s_cycle_box.x, s_cycle_box.y + s_cycle_box.h + 18.0F, filled, 10.0F, (float[4]){0.90F, 0.72F, 0.25F, 1.0F});

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

static void autosave_if_dirty(void) {
    if (!s_autosave_enabled || !game_state_is_dirty()) {
        return;
    }
    char error[256] = {0};
    char *json = game_state_save_json_string(&g_game_state, error, (int)sizeof(error));
    if (!json) {
        (void)fprintf(stderr, "autosave serialize failed: %s\n", error);
        return;
    }
    if (game_storage_save_json(DEFAULT_SAVE_KEY, GAME_STATE_DOCUMENT, json, error, (int)sizeof(error))) {
        game_state_clear_dirty();
    } else {
        (void)fprintf(stderr, "autosave failed: %s\n", error);
    }
    free(json);
}

static void load_default_save_if_available(void) {
    if (s_fresh_state_requested || !s_autosave_enabled) {
        return;
    }
    char error[256] = {0};
    char *json = NULL;
    if (!game_storage_load_json(DEFAULT_SAVE_KEY, GAME_STATE_DOCUMENT, &json, error, (int)sizeof(error))) {
        return;
    }
    if (!game_state_load_json_string(&g_game_state, json, error, (int)sizeof(error))) {
        (void)fprintf(stderr, "autosave load failed: %s\n", error);
    }
    free(json);
    game_state_clear_dirty();
}

static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--devapi") == 0) {
            s_devapi_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                s_devapi_port = (uint16_t)strtoul(argv[++i], NULL, 10);
            }
        } else if (strcmp(argv[i], "--fresh-state") == 0) {
            s_fresh_state_requested = true;
        } else if (strcmp(argv[i], "--disable-autosave") == 0) {
            s_autosave_enabled = false;
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

#if NT_DEVAPI_ENABLED
static cJSON *state_json(void) {
    cJSON *state = game_state_to_json(&g_game_state);
    cJSON_AddNumberToObject(state, "frame", (double)g_nt_app.frame);
    cJSON_AddBoolToObject(state, "state_dirty", game_state_is_dirty());
    cJSON *seed = cJSON_AddObjectToObject(state, "seed");
    cJSON_AddStringToObject(seed, "shape", game_seed_shape_label(&g_game_state));
    cJSON_AddBoolToObject(seed, "can_cycle", true);
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
    (void)error;
    (void)error_cap;
    (void)user;
    game_seed_reset_playtest(&g_game_state);
    *result = state_json();
    return true;
}

static bool ep_game_action_cycle(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    seed_cycle();
    *result = state_json();
    return true;
}

static bool ep_game_audio_status(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    const GameAudioStatus status = game_audio_status();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "implemented", status.implemented);
    cJSON_AddBoolToObject(root, "initialized", status.initialized);
    cJSON_AddBoolToObject(root, "device_enabled", status.device_enabled);
    cJSON_AddStringToObject(root, "backend", status.backend ? status.backend : "unknown");
    cJSON_AddNumberToObject(root, "total_play_count", status.total_play_count);
    cJSON *cues = cJSON_AddObjectToObject(root, "cue_play_counts");
    for (int i = 0; i < GAME_AUDIO_CUE_COUNT; ++i) {
        cJSON_AddNumberToObject(cues, game_audio_cue_name((GameAudioCue)i), status.cue_play_count[i]);
    }
    *result = root;
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

static void register_ui_devapi(float w, float h) {
    nt_devapi_set_view(w, h, w, h);
    nt_devapi_clear_ui_elements();
    (void)nt_devapi_register_ui_node("root", "", "screen", "Game Seed", "Neutral template screen", 0.0F, 0.0F, w, h, true, true);
    (void)nt_devapi_register_ui_node("seed.preview", "root", "panel", "Preview", g_game_state.test_label_text, w * 0.08F, h * 0.12F, w * 0.84F, h * 0.72F, true, true);
    (void)nt_devapi_register_ui_node("seed.cycle", "root", "button", "Cycle", g_game_state.test_button_text, s_cycle_box.x, s_cycle_box.y, s_cycle_box.w, s_cycle_box.h, true, true);
}

static void register_game_endpoints(void) {
    nt_devapi_register_builtins();
    game_state_register_devapi();
    nt_devapi_register("game.state", ep_game_state, NULL);
    nt_devapi_register("game.reset_playtest", ep_game_reset_playtest, NULL);
    nt_devapi_register("game.action.cycle", ep_game_action_cycle, NULL);
    nt_devapi_register("game.audio.status", ep_game_audio_status, NULL);
    nt_devapi_register("game.capture.framebuffer", ep_game_capture_framebuffer, NULL);
}
#endif

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
    layout(w, h);
    handle_input();
    game_audio_update();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        register_ui_devapi(w, h);
    }
#endif

    draw_game(w, h);
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
    parse_args(argc, argv);
    load_default_save_if_available();
    game_audio_init();
    game_audio_set_device_enabled(!s_devapi_enabled);
    game_audio_set_volume(g_game_state.settings_master_volume, g_game_state.settings_sfx_volume);

    g_nt_window.title = "Game Seed";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    nt_gfx_init(&gfx_desc);
    nt_gfx_register_global_block("Globals", 0);
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
    game_audio_shutdown();
    nt_shape_renderer_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif

    return 0;
}
