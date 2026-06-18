#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "game_state.h"
#include "game_audio.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "window/nt_window.h"

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#else
#include <glad/gl.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKROOMS_DEVAPI_PORT_DEFAULT 9123
#define UI_W 960
#define UI_H 540
#define WALL_TEX_W 128
#define WALL_TEX_H 128

typedef struct UiBox {
    float x;
    float y;
    float w;
    float h;
} UiBox;

typedef struct BackroomsState {
    float x;
    float z;
    float yaw;
    float fear;
    float battery;
    float message_timer;
    float caught_timer;
    float route_shift;
    float stalker_pressure;
    float fuse_hum_timer;
    float stalker_audio_timer;
    float run_time;
    float last_run_time;
    float last_fear;
    float last_battery;
    float route_choice_feedback_timer;
    int route_choice_stage;
    int route_choice_correct;
    int route_choice_wrong;
    bool threat_visible;
    bool caught_audio_played;
    bool flashlight_on;
    bool fuse_found;
    bool won;
    bool caught;
    char message[64];
} BackroomsState;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = BACKROOMS_DEVAPI_PORT_DEFAULT;
static int s_window_width = 1280;
static int s_window_height = 720;

static BackroomsState s_game;
static nt_shader_t s_vs;
static nt_shader_t s_fs;
static nt_pipeline_t s_pipeline;
static nt_buffer_t s_quad_vbo;
static nt_texture_t s_wall_tex;
static nt_texture_t s_ui_tex;
static uint8_t s_wall_pixels[WALL_TEX_W * WALL_TEX_H * 4];
static uint8_t s_ui_pixels[UI_W * UI_H * 4];
static const float s_route_choice_z[] = {24.0F, 16.2F, 8.3F};
static const int s_route_choice_safe_side[] = {1, -1, 1};

static const char *s_vs_src = "precision mediump float;\n"
                              "layout(location = 0) in vec2 a_position;\n"
                              "out vec2 v_uv;\n"
                              "void main() {\n"
                              "    v_uv = a_position * 0.5 + 0.5;\n"
                              "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
                              "}\n";

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverlength-strings"
#endif
static const char *s_fs_src =
    "precision mediump float;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "uniform vec4 u_resolution_time;\n"
    "uniform vec4 u_player;\n"
    "uniform vec4 u_state;\n"
    "uniform vec4 u_pressure;\n"
    "uniform vec4 u_route;\n"
    "uniform sampler2D u_wall_tex;\n"
    "uniform sampler2D u_ui_tex;\n"
    "\n"
    "float hash12(vec2 p) {\n"
    "    vec3 p3 = fract(vec3(p.xyx) * 0.1031);\n"
    "    p3 += dot(p3, p3.yzx + 33.33);\n"
    "    return fract((p3.x + p3.y) * p3.z);\n"
    "}\n"
    "\n"
    "vec3 tonemap(vec3 c) {\n"
    "    c = max(c, vec3(0.0));\n"
    "    c = c / (vec3(1.0) + c);\n"
    "    return pow(c, vec3(0.88));\n"
    "}\n"
    "\n"
    "float sphere_hit(vec3 ro, vec3 rd, vec3 c, float r) {\n"
    "    vec3 oc = ro - c;\n"
    "    float b = dot(oc, rd);\n"
    "    float h = b * b - dot(oc, oc) + r * r;\n"
    "    if (h < 0.0) return 1e20;\n"
    "    float t = -b - sqrt(h);\n"
    "    return t > 0.0 ? t : 1e20;\n"
    "}\n"
    "\n"
    "float box2(vec2 p, vec2 half_size) {\n"
    "    vec2 d = abs(p) - half_size;\n"
    "    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 frag = v_uv * u_resolution_time.xy;\n"
    "    vec2 p = (frag - 0.5 * u_resolution_time.xy) / max(u_resolution_time.y, 1.0);\n"
    "    float ttime = u_resolution_time.z;\n"
    "    p.x += u_pressure.x * (0.035 * sin(ttime * 1.7 + u_player.y * 0.55) + 0.018 * sin(ttime * 5.1));\n"
    "    p.y += u_pressure.x * 0.014 * sin(ttime * 2.3 + u_player.x * 3.0);\n"
    "    float yaw = u_player.z;\n"
    "    vec2 fwd = vec2(sin(yaw), cos(yaw));\n"
    "    vec2 right = vec2(cos(yaw), -sin(yaw));\n"
    "    vec3 ro = vec3(u_player.x, 1.05 + 0.025 * sin(ttime * 7.0 + u_player.y), u_player.y);\n"
    "    vec3 rd = normalize(vec3(right.x * p.x * 1.55 + fwd.x, p.y * 0.86 - 0.04, right.y * p.x * 1.55 + fwd.y));\n"
    "\n"
    "    float best = 1e20;\n"
    "    vec3 normal = vec3(0.0, 1.0, 0.0);\n"
    "    int mat = 0;\n"
    "\n"
    "    float tfloor = (0.0 - ro.y) / rd.y;\n"
    "    if (tfloor > 0.0) { best = tfloor; normal = vec3(0.0, 1.0, 0.0); mat = 1; }\n"
    "    float tceil = (2.55 - ro.y) / rd.y;\n"
    "    if (tceil > 0.0 && tceil < best) { best = tceil; normal = vec3(0.0, -1.0, 0.0); mat = 2; }\n"
    "    float twall_l = (-1.36 - ro.x) / rd.x;\n"
    "    vec3 pwall_l = ro + rd * twall_l;\n"
    "    if (twall_l > 0.0 && pwall_l.y >= 0.0 && pwall_l.y <= 2.55 && twall_l < best) { best = twall_l; normal = vec3(1.0, 0.0, 0.0); mat = 3; }\n"
    "    float twall_r = (1.36 - ro.x) / rd.x;\n"
    "    vec3 pwall_r = ro + rd * twall_r;\n"
    "    if (twall_r > 0.0 && pwall_r.y >= 0.0 && pwall_r.y <= 2.55 && twall_r < best) { best = twall_r; normal = vec3(-1.0, 0.0, 0.0); mat = 3; }\n"
    "    float texit = (0.12 - ro.z) / rd.z;\n"
    "    vec3 pexit = ro + rd * texit;\n"
    "    if (texit > 0.0 && abs(pexit.x) < 0.68 && pexit.y > 0.0 && pexit.y < 2.15 && texit < best) { best = texit; normal = vec3(0.0, 0.0, 1.0); mat = 4; }\n"
    "    float tfar = (34.0 - ro.z) / rd.z;\n"
    "    vec3 pfar = ro + rd * tfar;\n"
    "    if (tfar > 0.0 && abs(pfar.x) < 1.5 && pfar.y >= 0.0 && pfar.y <= 2.55 && tfar < best) { best = tfar; normal = vec3(0.0, 0.0, -1.0); mat = 3; }\n"
    "\n"
    "    float tfuse = sphere_hit(ro, rd, vec3(0.36, 1.05, 29.4), 0.24);\n"
    "    if (u_state.x < 0.5 && tfuse < best) { best = tfuse; normal = normalize(ro + rd * tfuse - vec3(0.36, 1.05, 29.4)); mat = 5; }\n"
    "\n"
    "    float entity_z = max(u_player.y - mix(8.6, 4.4, u_pressure.y), 2.9);\n"
    "    float tent = (entity_z - ro.z) / rd.z;\n"
    "    vec3 pent = ro + rd * tent;\n"
    "    float ent_x = pent.x - 0.24 * sin(ttime * 1.7);\n"
    "    float ent_head = 1.0 - smoothstep(0.16, 0.24, length(vec2(ent_x, (pent.y - 1.66) * 0.88)));\n"
    "    float ent_torso = (1.0 - smoothstep(0.18, mix(0.30, 0.44, u_pressure.y), abs(ent_x))) * smoothstep(0.44, 0.72, pent.y) * (1.0 - smoothstep(1.28, 1.56, pent.y));\n"
    "    float ent_legs = (1.0 - smoothstep(0.10, 0.19, abs(abs(ent_x) - 0.10))) * smoothstep(0.08, 0.34, pent.y) * (1.0 - smoothstep(0.74, 1.05, pent.y));\n"
    "    float ent_shape = max(max(ent_head, ent_torso), ent_legs);\n"
    "    if (u_state.x > 0.5 && tent > 0.0 && ent_shape > 0.22 && pent.y > 0.06 && pent.y < 2.05 && tent < best) {\n"
    "        best = tent; normal = vec3(0.0, 0.0, 1.0); mat = 6;\n"
    "    }\n"
    "\n"
    "    vec3 hit = ro + rd * best;\n"
    "    vec3 albedo = vec3(0.77, 0.68, 0.34);\n"
    "    vec2 tuv = hit.xz * 0.33;\n"
    "    if (mat == 1) { albedo = vec3(0.34, 0.25, 0.14) * (0.75 + texture(u_wall_tex, hit.xz * 0.19).r * 0.35); }\n"
    "    if (mat == 2) { albedo = vec3(0.42, 0.40, 0.33); tuv = hit.xz * 0.42; }\n"
    "    if (mat == 3) { albedo = texture(u_wall_tex, vec2(hit.z * 0.18, hit.y * 0.55)).rgb; }\n"
    "    if (mat == 4) { albedo = mix(vec3(0.18, 0.12, 0.06), vec3(1.1, 0.84, 0.28), u_state.x); }\n"
    "    if (mat == 5) { albedo = vec3(0.15, 1.25, 0.72); }\n"
    "    if (mat == 6) { albedo = vec3(0.004, 0.006, 0.008); }\n"
    "\n"
    "    float fixture_z = floor((hit.z + 2.6) / 5.2) * 5.2;\n"
    "    float flicker = 0.76 + 0.24 * step(0.18, hash12(vec2(floor(ttime * 13.0), fixture_z)));\n"
    "    float light_dist = length(vec3(hit.x, hit.y - 2.42, hit.z - fixture_z));\n"
    "    float ceiling_light = 1.8 / (1.0 + light_dist * light_dist * 0.9) * flicker;\n"
    "    float exit_light = u_state.x * 3.2 / (1.0 + length(hit - vec3(0.0, 1.25, 0.28)) * 1.7);\n"
    "    float fuse_light = (1.0 - u_state.x) * 3.4 / (1.0 + length(hit - vec3(0.36, 1.0, 29.4)) * 1.35);\n"
    "    float cone = smoothstep(0.72, 0.98, dot(rd, normalize(vec3(fwd.x, -0.03, fwd.y)))) * u_state.z;\n"
    "    float flashlight = cone * 2.5 / (1.0 + best * best * 0.035);\n"
    "    float contact = 1.0 - 0.34 * exp(-abs(hit.x - sign(hit.x) * 1.36) * 8.0) * smoothstep(0.0, 0.22, hit.y);\n"
    "    contact *= 1.0 - 0.22 * exp(-hit.y * 12.0);\n"
    "    float side_opening = 0.0;\n"
    "    float false_exit = 0.0;\n"
    "    float route_safe_glow = 0.0;\n"
    "    float route_bad_glow = 0.0;\n"
    "    if (mat == 3) {\n"
    "        float bay = abs(fract((hit.z + 1.2) / 7.7) - 0.5);\n"
    "        side_opening = (1.0 - smoothstep(0.065, 0.12, bay)) * smoothstep(0.18, 0.42, hit.y) * (1.0 - smoothstep(1.85, 2.22, hit.y));\n"
    "        side_opening *= 0.55 + u_pressure.x * 1.05;\n"
    "        float slot = abs(fract((hit.z - 2.4) / 6.9) - 0.5);\n"
    "        float door_body = (1.0 - smoothstep(0.07, 0.16, slot)) * smoothstep(0.22, 0.38, hit.y) * (1.0 - smoothstep(1.58, 1.86, hit.y));\n"
    "        float sign_band = (1.0 - smoothstep(0.09, 0.18, slot)) * (1.0 - smoothstep(0.04, 0.08, abs(hit.y - 1.72)));\n"
    "        false_exit = u_state.x * u_pressure.x * max(door_body, sign_band * 0.8);\n"
    "        float anomaly = u_route.x * (1.0 - smoothstep(1.1, 2.35, abs(hit.z - u_route.z)));\n"
    "        float lane_side = step(0.0, hit.x * u_route.y);\n"
    "        float lane_band = smoothstep(0.34, 0.62, hit.y) * (1.0 - smoothstep(1.78, 2.10, hit.y));\n"
    "        lane_band *= 0.62 + 0.38 * step(0.5, sin(ttime * 8.0 + hit.z * 1.4));\n"
    "        route_safe_glow = anomaly * lane_side * lane_band;\n"
    "        route_bad_glow = anomaly * (1.0 - lane_side) * lane_band;\n"
    "    }\n"
    "    float fixture_shape = 0.0;\n"
    "    if (mat == 2) {\n"
    "        fixture_shape = (1.0 - smoothstep(0.13, 0.19, abs(hit.x))) * (1.0 - smoothstep(0.72, 0.92, abs(hit.z - fixture_z)));\n"
    "    }\n"
    "\n"
    "    vec3 color = albedo * (0.16 + ceiling_light + exit_light + fuse_light + flashlight) * contact;\n"
    "    color += vec3(1.1, 1.0, 0.73) * fixture_shape * (1.6 + 1.1 * flicker);\n"
    "    color *= 1.0 - side_opening * 0.86;\n"
    "    color = mix(color, vec3(0.005, 0.012, 0.008), false_exit * 0.92);\n"
    "    color += vec3(0.1, 1.1, 0.34) * false_exit * (0.42 + 0.58 * step(0.5, sin(ttime * 6.0 + hit.z)));\n"
    "    color = mix(color, vec3(0.11, 0.0, 0.0), route_bad_glow * 0.46);\n"
    "    color += vec3(0.08, 1.05, 0.42) * route_safe_glow * (0.75 + u_pressure.x);\n"
    "    color += vec3(1.0, 0.08, 0.0) * route_bad_glow * 0.22;\n"
    "    if (mat == 5) color += vec3(0.1, 2.4, 1.1);\n"
    "    if (mat == 6) {\n"
    "        float eye_l = 1.0 - smoothstep(0.015, 0.05, length(hit.xy - vec2(-0.08, 1.55)));\n"
    "        float eye_r = 1.0 - smoothstep(0.015, 0.05, length(hit.xy - vec2(0.08, 1.55)));\n"
    "        color *= 0.035;\n"
    "        color += vec3(1.2, 0.03, 0.0) * max(eye_l, eye_r) * (0.4 + u_pressure.y);\n"
    "    }\n"
    "    float fog = smoothstep(13.0, 31.0, best);\n"
    "    vec3 fog_col = mix(vec3(0.18, 0.15, 0.06), vec3(0.03, 0.035, 0.045), u_state.x * 0.55);\n"
    "    fog_col = mix(fog_col, vec3(0.06, 0.018, 0.015), u_pressure.y * 0.35);\n"
    "    color = mix(color, fog_col, fog);\n"
    "    float vignette = smoothstep(0.95, 0.22, length(p));\n"
    "    color *= 0.55 + 0.45 * vignette;\n"
    "    float fear_pulse = u_state.y * (0.08 + 0.07 * sin(ttime * 9.0));\n"
    "    color = mix(color, vec3(0.07, 0.0, 0.0), fear_pulse);\n"
    "\n"
    "    vec4 ui = texture(u_ui_tex, vec2(v_uv.x, 1.0 - v_uv.y));\n"
    "    vec3 final_col = mix(tonemap(color), ui.rgb, ui.a);\n"
    "    frag_color = vec4(final_col, 1.0);\n"
    "}\n";
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static float absf(float v) { return v < 0.0F ? -v : v; }

static float approachf(float value, float target, float step) {
    if (value < target) {
        return value + fminf(step, target - value);
    }
    return value - fminf(step, value - target);
}

static float dist_to(float x, float z, float tx, float tz) {
    const float dx = x - tx;
    const float dz = z - tz;
    return sqrtf((dx * dx) + (dz * dz));
}

static bool near_fuse(void) { return !s_game.fuse_found && dist_to(s_game.x, s_game.z, 0.36F, 29.4F) < 1.25F; }

static bool near_exit(void) { return s_game.fuse_found && s_game.z < 1.85F && absf(s_game.x) < 0.9F; }

static int route_choice_total(void) { return (int)(sizeof(s_route_choice_z) / sizeof(s_route_choice_z[0])); }

static int route_choice_safe_side_for_stage(int stage) {
    if (stage < 0 || stage >= route_choice_total()) {
        return 0;
    }
    return s_route_choice_safe_side[stage];
}

static float route_choice_z_for_stage(int stage) {
    if (stage < 0 || stage >= route_choice_total()) {
        return 0.0F;
    }
    return s_route_choice_z[stage];
}

static const char *route_choice_side_name(int side) {
    if (side > 0) {
        return "LEFT";
    }
    if (side < 0) {
        return "RIGHT";
    }
    return "NONE";
}

static bool route_choice_active(void) {
    if (!s_game.fuse_found || s_game.won || s_game.caught || s_game.route_choice_stage >= route_choice_total()) {
        return false;
    }
    const float route_z = route_choice_z_for_stage(s_game.route_choice_stage);
    return s_game.z <= route_z + 1.8F && s_game.z >= route_z - 1.8F;
}

static float stalker_z(void) {
    const float close_distance = 8.6F - 4.2F * clampf(s_game.stalker_pressure, 0.0F, 1.0F);
    return fmaxf(s_game.z - close_distance, 2.9F);
}

static bool looking_at_stalker(void) {
    if (!s_game.fuse_found || s_game.won || s_game.caught) {
        return false;
    }
    const float tx = 0.24F * sinf(g_nt_app.time * 1.7F);
    const float tz = stalker_z();
    const float dx = tx - s_game.x;
    const float dz = tz - s_game.z;
    const float len = sqrtf(dx * dx + dz * dz);
    if (len < 0.01F || len > 13.0F) {
        return false;
    }
    const float fwd_x = sinf(s_game.yaw);
    const float fwd_z = cosf(s_game.yaw);
    const float dot = (dx / len) * fwd_x + (dz / len) * fwd_z;
    return dot > 0.86F;
}

static void set_message(const char *text, float seconds) {
    (void)snprintf(s_game.message, sizeof(s_game.message), "%s", text);
    s_game.message[sizeof(s_game.message) - 1] = '\0';
    s_game.message_timer = seconds;
}

static void reset_backrooms(void) {
    game_state_init_defaults(&g_game_state);
    s_game = (BackroomsState){
        .x = 0.0F,
        .z = 2.75F,
        .yaw = 0.0F,
        .fear = 10.0F,
        .battery = 1.0F,
        .flashlight_on = true,
    };
    set_message("FIND THE HUMMING FUSE", 3.0F);
}

static void record_run_result(void) {
    s_game.last_run_time = s_game.run_time;
    s_game.last_fear = s_game.fear;
    s_game.last_battery = s_game.battery;
}

static void update_route_choice(void) {
    if (!s_game.fuse_found || s_game.won || s_game.caught || s_game.route_choice_stage >= route_choice_total()) {
        return;
    }

    const float route_z = route_choice_z_for_stage(s_game.route_choice_stage);
    const int safe_side = route_choice_safe_side_for_stage(s_game.route_choice_stage);
    if (route_choice_active() && s_game.message_timer <= 0.0F) {
        char line[64];
        (void)snprintf(line, sizeof(line), "HUM POINTS %s", route_choice_side_name(safe_side));
        set_message(line, 0.75F);
    }
    if (s_game.z > route_z - 1.8F) {
        return;
    }

    int chosen_side = 0;
    if (s_game.x < -0.24F) {
        chosen_side = -1;
    } else if (s_game.x > 0.24F) {
        chosen_side = 1;
    }

    if (chosen_side == safe_side) {
        s_game.route_choice_correct += 1;
        s_game.fear = clampf(s_game.fear - 5.0F, 0.0F, 100.0F);
        s_game.stalker_pressure = clampf(s_game.stalker_pressure - 0.14F, 0.0F, 1.0F);
        s_game.route_shift = clampf(s_game.route_shift - 0.08F, 0.0F, 1.0F);
        set_message("GOOD TURN", 1.4F);
    } else {
        s_game.route_choice_wrong += 1;
        s_game.fear = clampf(s_game.fear + 16.0F, 0.0F, 100.0F);
        s_game.stalker_pressure = clampf(s_game.stalker_pressure + 0.23F, 0.0F, 1.0F);
        s_game.route_shift = clampf(s_game.route_shift + 0.18F, 0.0F, 1.0F);
        game_audio_play(GAME_AUDIO_CUE_STALKER);
        set_message(chosen_side == 0 ? "YOU HESITATED" : "WRONG TURN", 1.8F);
    }
    s_game.route_choice_feedback_timer = 2.0F;
    s_game.route_choice_stage += 1;
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

static void generate_wall_texture(void) {
    for (int y = 0; y < WALL_TEX_H; ++y) {
        for (int x = 0; x < WALL_TEX_W; ++x) {
            const int i = (y * WALL_TEX_W + x) * 4;
            const int seam = (x % 32 == 0) || (y % 42 == 0);
            const int fleck = ((x * 17 + y * 31 + ((x * y) % 19)) & 31) == 0;
            int r = 184 + ((x * 5 + y * 3) & 15);
            int g = 161 + ((x * 7 + y * 11) & 13);
            int b = 74 + ((x * 13 + y * 2) & 9);
            if (seam) {
                r -= 36;
                g -= 34;
                b -= 20;
            }
            if (fleck) {
                r -= 50;
                g -= 42;
                b -= 24;
            }
            s_wall_pixels[i + 0] = (uint8_t)clampf((float)r, 0.0F, 255.0F);
            s_wall_pixels[i + 1] = (uint8_t)clampf((float)g, 0.0F, 255.0F);
            s_wall_pixels[i + 2] = (uint8_t)clampf((float)b, 0.0F, 255.0F);
            s_wall_pixels[i + 3] = 255;
        }
    }
}

static void ui_clear(void) { memset(s_ui_pixels, 0, sizeof(s_ui_pixels)); }

static void ui_px(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || y < 0 || x >= UI_W || y >= UI_H) {
        return;
    }
    uint8_t *p = &s_ui_pixels[(y * UI_W + x) * 4];
    const float alpha = (float)a / 255.0F;
    const float inv = 1.0F - alpha;
    p[0] = (uint8_t)((float)p[0] * inv + (float)r * alpha);
    p[1] = (uint8_t)((float)p[1] * inv + (float)g * alpha);
    p[2] = (uint8_t)((float)p[2] * inv + (float)b * alpha);
    p[3] = (uint8_t)clampf((float)p[3] + (float)a * (1.0F - (float)p[3] / 255.0F), 0.0F, 255.0F);
}

static void ui_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            ui_px(xx, yy, r, g, b, a);
        }
    }
}

static void glyph_rows(char c, uint8_t out[7]) {
    memset(out, 0, 7);
#define GLYPH(a, b, c0, d, e, f, g)                                                                                                                       \
    do {                                                                                                                                                   \
        out[0] = (a);                                                                                                                                       \
        out[1] = (b);                                                                                                                                       \
        out[2] = (c0);                                                                                                                                      \
        out[3] = (d);                                                                                                                                       \
        out[4] = (e);                                                                                                                                       \
        out[5] = (f);                                                                                                                                       \
        out[6] = (g);                                                                                                                                       \
    } while (0)
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - ('a' - 'A'));
    }
    switch (c) {
    case 'A': GLYPH(14, 17, 17, 31, 17, 17, 17); break;
    case 'B': GLYPH(30, 17, 17, 30, 17, 17, 30); break;
    case 'C': GLYPH(14, 17, 16, 16, 16, 17, 14); break;
    case 'D': GLYPH(30, 17, 17, 17, 17, 17, 30); break;
    case 'E': GLYPH(31, 16, 16, 30, 16, 16, 31); break;
    case 'F': GLYPH(31, 16, 16, 30, 16, 16, 16); break;
    case 'G': GLYPH(14, 17, 16, 23, 17, 17, 15); break;
    case 'H': GLYPH(17, 17, 17, 31, 17, 17, 17); break;
    case 'I': GLYPH(14, 4, 4, 4, 4, 4, 14); break;
    case 'J': GLYPH(7, 2, 2, 2, 18, 18, 12); break;
    case 'K': GLYPH(17, 18, 20, 24, 20, 18, 17); break;
    case 'L': GLYPH(16, 16, 16, 16, 16, 16, 31); break;
    case 'M': GLYPH(17, 27, 21, 21, 17, 17, 17); break;
    case 'N': GLYPH(17, 25, 21, 19, 17, 17, 17); break;
    case 'O': GLYPH(14, 17, 17, 17, 17, 17, 14); break;
    case 'P': GLYPH(30, 17, 17, 30, 16, 16, 16); break;
    case 'Q': GLYPH(14, 17, 17, 17, 21, 18, 13); break;
    case 'R': GLYPH(30, 17, 17, 30, 20, 18, 17); break;
    case 'S': GLYPH(15, 16, 16, 14, 1, 1, 30); break;
    case 'T': GLYPH(31, 4, 4, 4, 4, 4, 4); break;
    case 'U': GLYPH(17, 17, 17, 17, 17, 17, 14); break;
    case 'V': GLYPH(17, 17, 17, 17, 17, 10, 4); break;
    case 'W': GLYPH(17, 17, 17, 21, 21, 21, 10); break;
    case 'X': GLYPH(17, 17, 10, 4, 10, 17, 17); break;
    case 'Y': GLYPH(17, 17, 10, 4, 4, 4, 4); break;
    case 'Z': GLYPH(31, 1, 2, 4, 8, 16, 31); break;
    case '0': GLYPH(14, 17, 19, 21, 25, 17, 14); break;
    case '1': GLYPH(4, 12, 4, 4, 4, 4, 14); break;
    case '2': GLYPH(14, 17, 1, 2, 4, 8, 31); break;
    case '3': GLYPH(30, 1, 1, 14, 1, 1, 30); break;
    case '4': GLYPH(2, 6, 10, 18, 31, 2, 2); break;
    case '5': GLYPH(31, 16, 16, 30, 1, 1, 30); break;
    case '6': GLYPH(14, 16, 16, 30, 17, 17, 14); break;
    case '7': GLYPH(31, 1, 2, 4, 8, 8, 8); break;
    case '8': GLYPH(14, 17, 17, 14, 17, 17, 14); break;
    case '9': GLYPH(14, 17, 17, 15, 1, 1, 14); break;
    case '-': GLYPH(0, 0, 0, 31, 0, 0, 0); break;
    case ':': GLYPH(0, 4, 4, 0, 4, 4, 0); break;
    case '.': GLYPH(0, 0, 0, 0, 0, 12, 12); break;
    case '/': GLYPH(1, 1, 2, 4, 8, 16, 16); break;
    case '!': GLYPH(4, 4, 4, 4, 4, 0, 4); break;
    case '>': GLYPH(16, 8, 4, 2, 4, 8, 16); break;
    default: break;
    }
#undef GLYPH
}

static void ui_text(int x, int y, const char *text, int scale, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int pen_x = x;
    for (const char *it = text; *it; ++it) {
        if (*it == ' ') {
            pen_x += 4 * scale;
            continue;
        }
        uint8_t rows[7];
        glyph_rows(*it, rows);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((rows[row] & (1U << (4 - col))) == 0U) {
                    continue;
                }
                ui_rect(pen_x + col * scale, y + row * scale, scale, scale, r, g, b, a);
            }
        }
        pen_x += 6 * scale;
    }
}

static void ui_bar(int x, int y, int w, int h, float value, uint8_t r, uint8_t g, uint8_t b) {
    ui_rect(x, y, w, h, 8, 10, 12, 205);
    ui_rect(x + 2, y + 2, (int)((float)(w - 4) * clampf(value, 0.0F, 1.0F)), h - 4, r, g, b, 235);
}

static void build_ui(void) {
    ui_clear();
    const float fuse_dist = dist_to(s_game.x, s_game.z, 0.36F, 29.4F);
    char line[96];
    const char *objective = s_game.caught ? "CAUGHT" : (s_game.won ? "ESCAPED" : (s_game.fuse_found ? "RETURN TO EXIT" : "FIND THE HUMMING FUSE"));

    ui_rect(18, 18, 462, 118, 4, 6, 8, 190);
    ui_text(30, 30, "BACKROOMS LIMINAL", 3, 246, 226, 146, 255);
    (void)snprintf(line, sizeof(line), "OBJECTIVE: %s", objective);
    ui_text(30, 64, line, 2, 230, 238, 208, 255);
    ui_text(30, 92, "WASD MOVE  ARROWS LOOK  E USE  F LIGHT", 2, 190, 205, 184, 240);

    ui_rect(682, 18, 258, 118, 4, 6, 8, 190);
    ui_text(700, 32, "FEAR", 2, 240, 210, 176, 250);
    ui_bar(770, 30, 150, 16, s_game.fear / 100.0F, 190, 30, 36);
    ui_text(700, 62, "BATTERY", 2, 240, 210, 176, 250);
    ui_bar(806, 60, 114, 16, s_game.battery, 238, 210, 86);
    (void)snprintf(line, sizeof(line), "FUSE:%s  EXIT:%s", s_game.fuse_found ? "YES" : "NO", s_game.fuse_found ? "ON" : "OFF");
    ui_text(700, 94, line, 2, 198, 230, 196, 245);
    if (route_choice_active()) {
        (void)snprintf(line, sizeof(line), "TURN:%s  CHOICE:%d/%d", route_choice_side_name(route_choice_safe_side_for_stage(s_game.route_choice_stage)), s_game.route_choice_stage + 1, route_choice_total());
        ui_text(700, 122, line, 1, 132, 255, 184, 255);
    } else if (s_game.fuse_found && !s_game.won) {
        (void)snprintf(line, sizeof(line), "ROUTE:SHIFT  THREAT:%d", (int)(s_game.stalker_pressure * 100.0F));
        ui_text(700, 122, line, 1, 255, 184, 132, 245);
    } else {
        ui_text(700, 122, "ROUTE:STABLE", 1, 175, 205, 170, 220);
    }

    ui_rect(476, 268, 8, 2, 230, 235, 220, 220);
    ui_rect(479, 265, 2, 8, 230, 235, 220, 220);

    if (!s_game.won && !s_game.caught && route_choice_active()) {
        ui_rect(260, 392, 440, 46, 5, 8, 8, 215);
        (void)snprintf(line, sizeof(line), "MOVE %s - TRUST HUM", route_choice_side_name(route_choice_safe_side_for_stage(s_game.route_choice_stage)));
        ui_text(288, 406, line, 3, 132, 255, 184, 255);
    } else if (!s_game.won && !s_game.caught && near_fuse()) {
        ui_rect(322, 392, 316, 46, 5, 8, 8, 215);
        ui_text(346, 406, "PRESS E - TAKE FUSE", 3, 132, 255, 184, 255);
    } else if (!s_game.won && !s_game.caught && near_exit()) {
        ui_rect(334, 392, 292, 46, 5, 8, 8, 215);
        ui_text(358, 406, "PRESS E - ESCAPE", 3, 255, 226, 130, 255);
    } else if (!s_game.won && !s_game.caught && !s_game.fuse_found && s_game.message_timer <= 0.0F) {
        (void)snprintf(line, sizeof(line), "FUSE HUM %.0fM", (double)fuse_dist);
        ui_text(390, 466, line, 2, 230, 218, 156, 230);
    }

    if (!s_game.won && !s_game.caught && s_game.message_timer > 0.0F) {
        ui_rect(254, 470, 452, 42, 3, 4, 5, 190);
        ui_text(282, 484, s_game.message, 2, 255, 236, 170, 255);
    }
    if (s_game.won) {
        ui_rect(188, 184, 584, 158, 4, 6, 7, 235);
        ui_text(338, 206, "ESCAPED", 4, 255, 228, 128, 255);
        (void)snprintf(line, sizeof(line), "TIME:%02dS  FEAR:%02d  BAT:%02d", (int)s_game.last_run_time, (int)s_game.last_fear, (int)(s_game.last_battery * 100.0F));
        ui_text(260, 258, line, 2, 220, 238, 210, 255);
        ui_text(292, 296, "PRESS E - NEW RUN", 3, 132, 255, 184, 255);
    }
    if (s_game.caught) {
        ui_rect(178, 184, 604, 166, 8, 0, 0, 232);
        ui_text(242, 206, "THE LIGHTS FOUND YOU", 3, 255, 130, 112, 255);
        (void)snprintf(line, sizeof(line), "TIME:%02dS  FEAR:%02d  BAT:%02d", (int)s_game.last_run_time, (int)s_game.last_fear, (int)(s_game.last_battery * 100.0F));
        ui_text(260, 258, line, 2, 255, 220, 190, 255);
        ui_text(302, 296, "PRESS E - RETRY", 3, 255, 230, 160, 255);
    }
}

static void init_render_resources(void) {
    const float verts[] = {
        -1.0F, -1.0F, 1.0F, -1.0F, 1.0F, 1.0F,
        -1.0F, -1.0F, 1.0F, 1.0F,  -1.0F, 1.0F,
    };

    s_vs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_VERTEX, .source = s_vs_src, .label = "backrooms_fullscreen_vs"});
    s_fs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_FRAGMENT, .source = s_fs_src, .label = "backrooms_liminal_fs"});
    s_pipeline = nt_gfx_make_pipeline(&(nt_pipeline_desc_t){
        .vertex_shader = s_vs,
        .fragment_shader = s_fs,
        .layout =
            {
                .attr_count = 1,
                .stride = sizeof(float) * 2,
                .attrs = {{.location = 0, .format = NT_FORMAT_FLOAT2, .offset = 0}},
            },
        .depth_test = false,
        .depth_write = false,
        .depth_func = NT_DEPTH_ALWAYS,
        .cull_mode = 0,
        .label = "backrooms_fullscreen_pipeline",
    });
    s_quad_vbo = nt_gfx_make_buffer(&(nt_buffer_desc_t){.type = NT_BUFFER_VERTEX, .usage = NT_USAGE_IMMUTABLE, .data = verts, .size = sizeof(verts), .label = "backrooms_quad"});

    generate_wall_texture();
    s_wall_tex = nt_gfx_make_texture(&(nt_texture_desc_t){
        .width = WALL_TEX_W,
        .height = WALL_TEX_H,
        .data = s_wall_pixels,
        .format = NT_PIXEL_RGBA8,
        .min_filter = NT_FILTER_LINEAR,
        .mag_filter = NT_FILTER_LINEAR,
        .wrap_u = NT_WRAP_REPEAT,
        .wrap_v = NT_WRAP_REPEAT,
        .gen_mipmaps = true,
        .label = "backrooms_wallpaper_texture",
    });
    ui_clear();
    s_ui_tex = nt_gfx_make_texture(&(nt_texture_desc_t){
        .width = UI_W,
        .height = UI_H,
        .data = s_ui_pixels,
        .format = NT_PIXEL_RGBA8,
        .min_filter = NT_FILTER_LINEAR,
        .mag_filter = NT_FILTER_LINEAR,
        .wrap_u = NT_WRAP_CLAMP_TO_EDGE,
        .wrap_v = NT_WRAP_CLAMP_TO_EDGE,
        .label = "backrooms_ui_texture",
    });
}

static void shutdown_render_resources(void) {
    nt_gfx_destroy_texture(s_ui_tex);
    nt_gfx_destroy_texture(s_wall_tex);
    nt_gfx_destroy_buffer(s_quad_vbo);
    nt_gfx_destroy_pipeline(s_pipeline);
    nt_gfx_destroy_shader(s_fs);
    nt_gfx_destroy_shader(s_vs);
}

static void interact(void) {
    if (s_game.won || s_game.caught) {
        reset_backrooms();
        return;
    }
    if (near_fuse()) {
        s_game.fuse_found = true;
        s_game.fear = clampf(s_game.fear + 18.0F, 0.0F, 100.0F);
        s_game.route_shift = 0.18F;
        s_game.stalker_pressure = fmaxf(s_game.stalker_pressure, 0.22F);
        game_audio_play(GAME_AUDIO_CUE_FUSE_PICKUP);
        set_message("THE LIGHTS HEARD YOU", 3.0F);
    } else if (near_exit()) {
        s_game.won = true;
        record_run_result();
        game_audio_play(GAME_AUDIO_CUE_ESCAPE);
        set_message("ESCAPED - ROUTE COMPLETE", 8.0F);
    } else {
        set_message("TOO FAR FROM ANYTHING", 1.2F);
    }
}

static void update_game(void) {
    float dt = g_nt_app.dt > 0.0F ? g_nt_app.dt : (1.0F / 60.0F);
    dt = clampf(dt, 0.0F, 0.05F);
    if (s_game.message_timer > 0.0F) {
        s_game.message_timer -= dt;
    }
    if (s_game.fuse_hum_timer > 0.0F) {
        s_game.fuse_hum_timer -= dt;
    }
    if (s_game.stalker_audio_timer > 0.0F) {
        s_game.stalker_audio_timer -= dt;
    }
    if (s_game.route_choice_feedback_timer > 0.0F) {
        s_game.route_choice_feedback_timer -= dt;
    }
    game_audio_update();
    game_audio_set_volume(g_game_state.settings_master_volume, g_game_state.settings_sfx_volume);
    if (s_game.caught) {
        if (!s_game.caught_audio_played) {
            game_audio_play(GAME_AUDIO_CUE_CAUGHT);
            s_game.caught_audio_played = true;
        }
        if (nt_input_key_is_pressed(NT_KEY_E) || nt_input_key_is_pressed(NT_KEY_ENTER)) {
            reset_backrooms();
        }
        return;
    }
    if (s_game.won) {
        if (nt_input_key_is_pressed(NT_KEY_E) || nt_input_key_is_pressed(NT_KEY_ENTER)) {
            reset_backrooms();
            return;
        }
        s_game.route_shift = approachf(s_game.route_shift, 0.0F, dt * 0.7F);
        s_game.stalker_pressure = approachf(s_game.stalker_pressure, 0.0F, dt * 0.45F);
        s_game.fear = clampf(s_game.fear - dt * 14.0F, 0.0F, 100.0F);
        return;
    }

    if (nt_input_key_is_pressed(NT_KEY_F)) {
        s_game.flashlight_on = !s_game.flashlight_on;
        game_audio_play(GAME_AUDIO_CUE_FLASHLIGHT);
        set_message(s_game.flashlight_on ? "FLASHLIGHT ON" : "FLASHLIGHT OFF", 0.9F);
    }
    if (nt_input_key_is_pressed(NT_KEY_E) || nt_input_key_is_pressed(NT_KEY_ENTER)) {
        interact();
    }
    s_game.run_time += dt;

    const float turn = 1.95F * dt;
    if (nt_input_key_is_down(NT_KEY_ARROW_LEFT) || nt_input_key_is_down(NT_KEY_Q)) {
        s_game.yaw -= turn;
    }
    if (nt_input_key_is_down(NT_KEY_ARROW_RIGHT) || nt_input_key_is_down(NT_KEY_R)) {
        s_game.yaw += turn;
    }

    const float fwd_x = sinf(s_game.yaw);
    const float fwd_z = cosf(s_game.yaw);
    const float right_x = cosf(s_game.yaw);
    const float right_z = -sinf(s_game.yaw);
    float move_x = 0.0F;
    float move_z = 0.0F;
    if (nt_input_key_is_down(NT_KEY_W) || nt_input_key_is_down(NT_KEY_ARROW_UP)) {
        move_x += fwd_x;
        move_z += fwd_z;
    }
    if (nt_input_key_is_down(NT_KEY_S) || nt_input_key_is_down(NT_KEY_ARROW_DOWN)) {
        move_x -= fwd_x;
        move_z -= fwd_z;
    }
    if (nt_input_key_is_down(NT_KEY_A)) {
        move_x -= right_x;
        move_z -= right_z;
    }
    if (nt_input_key_is_down(NT_KEY_D)) {
        move_x += right_x;
        move_z += right_z;
    }
    const float len = sqrtf(move_x * move_x + move_z * move_z);
    if (len > 0.001F) {
        move_x /= len;
        move_z /= len;
        const float speed = 3.15F;
        s_game.x += move_x * speed * dt;
        s_game.z += move_z * speed * dt;
    }
    s_game.x = clampf(s_game.x, -1.05F, 1.05F);
    s_game.z = clampf(s_game.z, 0.45F, 31.8F);
    s_game.threat_visible = looking_at_stalker();
    update_route_choice();

    if (s_game.flashlight_on && s_game.battery > 0.0F) {
        s_game.battery = clampf(s_game.battery - dt * 0.026F, 0.0F, 1.0F);
        if (s_game.battery <= 0.001F) {
            s_game.flashlight_on = false;
            set_message("BATTERY DEAD", 1.5F);
        }
    } else {
        s_game.battery = clampf(s_game.battery + dt * 0.006F, 0.0F, 1.0F);
    }

    if (s_game.fuse_found) {
        const float return_progress = clampf((29.4F - s_game.z) / 29.4F, 0.0F, 1.0F);
        const float target_shift = clampf(0.24F + return_progress * 0.58F + (s_game.fear / 100.0F) * 0.16F, 0.0F, 1.0F);
        s_game.route_shift = approachf(s_game.route_shift, target_shift, dt * 0.55F);

        float stalker_delta = 0.045F + (s_game.fear / 100.0F) * 0.07F + (s_game.flashlight_on ? -0.016F : 0.052F);
        if (s_game.threat_visible) {
            stalker_delta += s_game.flashlight_on ? -0.055F : 0.12F;
        }
        if (near_exit()) {
            stalker_delta += 0.18F;
        }
        s_game.stalker_pressure = clampf(s_game.stalker_pressure + stalker_delta * dt, 0.0F, 1.0F);
        if (s_game.stalker_pressure > 0.62F && s_game.message_timer <= 0.0F) {
            set_message(s_game.threat_visible ? "DON'T STARE AT IT" : "IT IS BETWEEN YOU AND EXIT", 1.35F);
        }
        if (s_game.stalker_pressure > 0.52F && s_game.stalker_audio_timer <= 0.0F) {
            game_audio_play(GAME_AUDIO_CUE_STALKER);
            s_game.stalker_audio_timer = s_game.threat_visible ? 1.1F : 1.9F;
        }
    } else {
        s_game.route_shift = approachf(s_game.route_shift, 0.0F, dt * 0.8F);
        s_game.stalker_pressure = approachf(s_game.stalker_pressure, 0.0F, dt * 0.8F);
        s_game.threat_visible = false;
        const float fuse_dist = dist_to(s_game.x, s_game.z, 0.36F, 29.4F);
        if (fuse_dist < 11.0F && s_game.fuse_hum_timer <= 0.0F) {
            game_audio_play(GAME_AUDIO_CUE_FUSE_HUM);
            s_game.fuse_hum_timer = clampf(0.45F + fuse_dist * 0.10F, 0.55F, 1.45F);
        }
    }

    float fear_rate = 1.15F + s_game.z * 0.035F + (s_game.flashlight_on ? -0.55F : 1.1F);
    if (s_game.fuse_found) {
        fear_rate += 3.6F + s_game.route_shift * 1.2F + s_game.stalker_pressure * 3.4F;
        if (s_game.threat_visible && !s_game.flashlight_on) {
            fear_rate += 2.4F;
        }
    }
    if (near_exit() || near_fuse()) {
        fear_rate -= 1.1F;
    }
    s_game.fear = clampf(s_game.fear + fear_rate * dt, 0.0F, 100.0F);
    if (s_game.fear >= 100.0F) {
        s_game.caught = true;
        record_run_result();
        s_game.caught_timer = 2.0F;
        s_game.caught_audio_played = false;
        set_message("THE LIGHTS FOUND YOU", 2.0F);
    }
}

static void draw_frame(float fb_w, float fb_h) {
    build_ui();
    nt_gfx_update_texture(s_ui_tex, 0, 0, UI_W, UI_H, s_ui_pixels);

    nt_gfx_bind_pipeline(s_pipeline);
    nt_gfx_bind_vertex_buffer(s_quad_vbo);
    nt_gfx_bind_texture(s_wall_tex, 0);
    nt_gfx_bind_texture(s_ui_tex, 1);
    nt_gfx_set_uniform_int("u_wall_tex", 0);
    nt_gfx_set_uniform_int("u_ui_tex", 1);
    nt_gfx_set_uniform_vec4("u_resolution_time", (float[4]){fb_w, fb_h, g_nt_app.time, 0.0F});
    nt_gfx_set_uniform_vec4("u_player", (float[4]){s_game.x, s_game.z, s_game.yaw, 0.0F});
    nt_gfx_set_uniform_vec4("u_state", (float[4]){s_game.fuse_found ? 1.0F : 0.0F, s_game.fear / 100.0F, (s_game.flashlight_on && s_game.battery > 0.0F) ? 1.0F : 0.0F, s_game.won ? 1.0F : 0.0F});
    nt_gfx_set_uniform_vec4("u_pressure", (float[4]){s_game.route_shift, s_game.stalker_pressure, s_game.threat_visible ? 1.0F : 0.0F, s_game.caught ? 1.0F : 0.0F});
    nt_gfx_set_uniform_vec4("u_route", (float[4]){route_choice_active() ? 1.0F : 0.0F, (float)route_choice_safe_side_for_stage(s_game.route_choice_stage), route_choice_z_for_stage(s_game.route_choice_stage), (float)s_game.route_choice_wrong});
    nt_gfx_draw(0, 6);
}

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static cJSON *state_json(void) {
    cJSON *root = game_state_to_json(&g_game_state);
    cJSON_AddStringToObject(root, "runtime", "backrooms_liminal");
    cJSON_AddNumberToObject(root, "x", (double)s_game.x);
    cJSON_AddNumberToObject(root, "z", (double)s_game.z);
    cJSON_AddNumberToObject(root, "yaw", (double)s_game.yaw);
    cJSON_AddNumberToObject(root, "fear", (double)s_game.fear);
    cJSON_AddNumberToObject(root, "battery", (double)s_game.battery);
    cJSON_AddNumberToObject(root, "route_shift", (double)s_game.route_shift);
    cJSON_AddNumberToObject(root, "stalker_pressure", (double)s_game.stalker_pressure);
    cJSON_AddNumberToObject(root, "route_choice_stage", (double)s_game.route_choice_stage);
    cJSON_AddNumberToObject(root, "route_choice_total", (double)route_choice_total());
    cJSON_AddBoolToObject(root, "route_choice_active", route_choice_active());
    cJSON_AddStringToObject(root, "route_choice_safe_side", route_choice_active() ? route_choice_side_name(route_choice_safe_side_for_stage(s_game.route_choice_stage)) : "NONE");
    cJSON_AddNumberToObject(root, "route_choice_correct", (double)s_game.route_choice_correct);
    cJSON_AddNumberToObject(root, "route_choice_wrong", (double)s_game.route_choice_wrong);
    cJSON_AddNumberToObject(root, "run_time", (double)s_game.run_time);
    cJSON_AddNumberToObject(root, "last_run_time", (double)s_game.last_run_time);
    cJSON_AddNumberToObject(root, "last_fear", (double)s_game.last_fear);
    cJSON_AddNumberToObject(root, "last_battery", (double)s_game.last_battery);
    cJSON_AddBoolToObject(root, "flashlight_on", s_game.flashlight_on);
    cJSON_AddBoolToObject(root, "fuse_found", s_game.fuse_found);
    cJSON_AddBoolToObject(root, "exit_powered", s_game.fuse_found);
    cJSON_AddBoolToObject(root, "won", s_game.won);
    cJSON_AddBoolToObject(root, "caught", s_game.caught);
    cJSON_AddBoolToObject(root, "threat_visible", s_game.threat_visible);
    cJSON_AddBoolToObject(root, "can_use", !s_game.won && !s_game.caught && (near_fuse() || near_exit()));
    cJSON_AddBoolToObject(root, "can_restart", s_game.won || s_game.caught);
    cJSON_AddStringToObject(root, "objective", s_game.caught ? "caught" : (s_game.won ? "escaped" : (s_game.fuse_found ? "return_to_exit" : "find_fuse")));
    cJSON_AddStringToObject(root, "outcome", s_game.caught ? "caught" : (s_game.won ? "escaped" : "running"));
    cJSON_AddStringToObject(root, "message", s_game.message_timer > 0.0F ? s_game.message : "");
    return root;
}

static double json_number(const cJSON *params, const char *name, double fallback) {
    const cJSON *item = cJSON_IsObject(params) ? cJSON_GetObjectItemCaseSensitive(params, name) : NULL;
    return cJSON_IsNumber(item) ? item->valuedouble : fallback;
}

static const char *json_string(const cJSON *params, const char *name, const char *fallback) {
    const cJSON *item = cJSON_IsObject(params) ? cJSON_GetObjectItemCaseSensitive(params, name) : NULL;
    return cJSON_IsString(item) ? item->valuestring : fallback;
}

static cJSON *audio_status_json(void) {
    const GameAudioStatus status = game_audio_status();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "implemented", status.implemented);
    cJSON_AddBoolToObject(root, "initialized", status.initialized);
    cJSON_AddBoolToObject(root, "device_enabled", status.device_enabled);
    cJSON_AddStringToObject(root, "backend", status.backend ? status.backend : "unknown");
    cJSON_AddNumberToObject(root, "total_play_count", status.total_play_count);
    cJSON *counts = cJSON_CreateObject();
    for (int i = 0; i < GAME_AUDIO_CUE_COUNT; ++i) {
        cJSON_AddNumberToObject(counts, game_audio_cue_name((GameAudioCue)i), status.cue_play_count[i]);
    }
    cJSON_AddItemToObject(root, "cue_play_count", counts);
    return root;
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
    reset_backrooms();
    *result = state_json();
    return true;
}

static bool ep_game_action_use(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    interact();
    *result = state_json();
    return true;
}

static bool ep_game_action_toggle_flashlight(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    s_game.flashlight_on = !s_game.flashlight_on;
    game_audio_play(GAME_AUDIO_CUE_FLASHLIGHT);
    *result = state_json();
    return true;
}

static bool ep_game_action_set_pose(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    s_game.x = clampf((float)json_number(params, "x", (double)s_game.x), -1.05F, 1.05F);
    s_game.z = clampf((float)json_number(params, "z", (double)s_game.z), 0.45F, 31.8F);
    s_game.yaw = (float)json_number(params, "yaw", (double)s_game.yaw);
    *result = state_json();
    return true;
}

static bool ep_game_debug_set_progress(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    const cJSON *fuse = cJSON_IsObject(params) ? cJSON_GetObjectItemCaseSensitive(params, "fuse_found") : NULL;
    const cJSON *won = cJSON_IsObject(params) ? cJSON_GetObjectItemCaseSensitive(params, "won") : NULL;
    const cJSON *caught = cJSON_IsObject(params) ? cJSON_GetObjectItemCaseSensitive(params, "caught") : NULL;
    if (cJSON_IsBool(fuse)) {
        s_game.fuse_found = cJSON_IsTrue(fuse);
    }
    if (cJSON_IsBool(won)) {
        s_game.won = cJSON_IsTrue(won);
        if (s_game.won) {
            record_run_result();
        }
    }
    if (cJSON_IsBool(caught)) {
        s_game.caught = cJSON_IsTrue(caught);
        if (s_game.caught) {
            record_run_result();
            set_message("THE LIGHTS FOUND YOU", 2.0F);
        }
    }
    s_game.fear = clampf((float)json_number(params, "fear", (double)s_game.fear), 0.0F, 100.0F);
    s_game.route_shift = clampf((float)json_number(params, "route_shift", (double)s_game.route_shift), 0.0F, 1.0F);
    s_game.stalker_pressure = clampf((float)json_number(params, "stalker_pressure", (double)s_game.stalker_pressure), 0.0F, 1.0F);
    s_game.route_choice_stage = (int)clampf((float)json_number(params, "route_choice_stage", (double)s_game.route_choice_stage), 0.0F, (float)route_choice_total());
    s_game.route_choice_correct = (int)fmaxf(0.0F, (float)json_number(params, "route_choice_correct", (double)s_game.route_choice_correct));
    s_game.route_choice_wrong = (int)fmaxf(0.0F, (float)json_number(params, "route_choice_wrong", (double)s_game.route_choice_wrong));
    s_game.run_time = fmaxf(0.0F, (float)json_number(params, "run_time", (double)s_game.run_time));
    if (s_game.won || s_game.caught) {
        record_run_result();
    }
    *result = state_json();
    return true;
}

static bool ep_game_audio_status(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    *result = audio_status_json();
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
    const char *output = json_string(params, "output", "");
    if (!output || !output[0]) {
        (void)snprintf(error, (size_t)error_cap, "output is required");
        return false;
    }
    const int width = (int)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const int height = (int)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    if (width <= 0 || height <= 0) {
        (void)snprintf(error, (size_t)error_cap, "framebuffer is empty");
        return false;
    }
    uint8_t *pixels = (uint8_t *)malloc((size_t)width * (size_t)height * 3U);
    if (!pixels) {
        (void)snprintf(error, (size_t)error_cap, "capture allocation failed");
        return false;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    FILE *file = fopen(output, "wb");
    if (!file) {
        free(pixels);
        (void)snprintf(error, (size_t)error_cap, "could not open capture output");
        return false;
    }
    (void)fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; --y) {
        (void)fwrite(pixels + ((size_t)y * (size_t)width * 3U), 1, (size_t)width * 3U, file);
    }
    fclose(file);
    free(pixels);
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "output", output);
    cJSON_AddNumberToObject(obj, "width", width);
    cJSON_AddNumberToObject(obj, "height", height);
    *result = obj;
    return true;
#endif
}

static void register_game_endpoints(void) {
    nt_devapi_register_builtins();
    game_state_register_devapi();
    nt_devapi_register("game.state", ep_game_state, NULL);
    nt_devapi_register("game.reset_playtest", ep_game_reset_playtest, NULL);
    nt_devapi_register("game.action.use", ep_game_action_use, NULL);
    nt_devapi_register("game.action.toggle_flashlight", ep_game_action_toggle_flashlight, NULL);
    nt_devapi_register("game.action.set_pose", ep_game_action_set_pose, NULL);
    nt_devapi_register("game.debug.set_progress", ep_game_debug_set_progress, NULL);
    nt_devapi_register("game.audio.status", ep_game_audio_status, NULL);
    nt_devapi_register("game.capture.framebuffer", ep_game_capture_framebuffer, NULL);
}

static void register_ui_devapi(void) {
    nt_devapi_set_frame(g_nt_app.frame);
    nt_devapi_set_view((float)g_nt_window.fb_width, (float)g_nt_window.fb_height, (float)UI_W, (float)UI_H);
    nt_devapi_clear_ui_elements();
    (void)nt_devapi_register_ui_node("root", "", "screen", "Backrooms Liminal", "Find the fuse and escape.", 0.0F, 0.0F, (float)UI_W, (float)UI_H, true, true);
    const char *objective_label = s_game.caught ? "Caught" : (s_game.won ? "Escaped" : (s_game.fuse_found ? "Return to exit" : "Find the humming fuse"));
    const bool use_active = !s_game.won && !s_game.caught && (near_fuse() || near_exit());
    (void)nt_devapi_register_ui_node("backrooms.objective", "root", "label", "Objective", objective_label, 18.0F, 18.0F, 462.0F, 118.0F, true, true);
    (void)nt_devapi_register_ui_node("backrooms.fear", "root", "meter", "Fear", "Fear pressure", 682.0F, 18.0F, 258.0F, 52.0F, true, true);
    (void)nt_devapi_register_ui_node("backrooms.battery", "root", "meter", "Battery", "Flashlight battery", 682.0F, 70.0F, 258.0F, 66.0F, true, true);
    (void)nt_devapi_register_ui_node("backrooms.threat", "root", "label", "Route threat", s_game.fuse_found ? "Route shifting and stalker pressure" : "Route stable", 682.0F, 118.0F, 258.0F, 28.0F, true, true);
    (void)nt_devapi_register_ui_node("backrooms.use_prompt", "root", "prompt", "Use", near_fuse() ? "Press E to take fuse" : (near_exit() ? "Press E to escape" : ""), 322.0F, 392.0F, 316.0F, 46.0F, use_active, use_active);
}
#endif

static void frame(void) {
    nt_window_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_net_poll();
    }
#endif
    nt_input_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_apply_pending();
    }
#endif

    update_game();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        register_ui_devapi();
    }
#endif

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif

    const float fb_w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const float fb_h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        init_render_resources();
    }
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.035F, 0.030F, 0.020F, 1.0F}, .clear_depth = 1.0F});
    draw_frame(fb_w, fb_h);
    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Backrooms Liminal";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    parse_args(argc, argv);
    reset_backrooms();

    g_nt_window.title = "Backrooms Liminal";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();
    game_audio_init();
    game_audio_set_volume(g_game_state.settings_master_volume, g_game_state.settings_sfx_volume);

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = false;
    gfx_desc.max_shaders = 8;
    gfx_desc.max_pipelines = 8;
    gfx_desc.max_buffers = 16;
    gfx_desc.max_textures = 16;
    nt_gfx_init(&gfx_desc);
    init_render_resources();

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
    shutdown_render_resources();
    nt_gfx_shutdown();
    game_audio_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif

    return 0;
}
