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
    float footstep_timer;
    float heartbeat_timer;
    float run_time;
    float last_run_time;
    float last_fear;
    float last_battery;
    float route_choice_feedback_timer;
    float blackout_timer;
    float ambush_timer;
    float relief_timer;
    float dynamo_stall_timer;
    int visited_rooms_mask;
    int side_room_visits;
    int layout_shift_count;
    int maze_zone;
    int route_choice_stage;
    int route_choice_correct;
    int route_choice_wrong;
    bool mark_placed;
    bool door_handle_collected;
    bool door_handle_placed;
    bool portal_exit_revealed;
    bool threat_visible;
    bool caught_audio_played;
    bool sprinting;
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

#define FUSE_X (-3.45F)
#define FUSE_Z (25.15F)
#define MARK_X (-3.45F)
#define MARK_Z (10.8F)
#define HANDLE_X (3.35F)
#define HANDLE_Z (18.6F)
#define MAZE_ZONE_MAIN 0
#define MAZE_ZONE_LEFT_DEADEND 1
#define MAZE_ZONE_RED_ROOM 2
#define MAZE_ZONE_FUSE_ROOM 3

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
    "uniform vec4 u_horror;\n"
    "uniform vec4 u_puzzle;\n"
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
    "float room_band(float z, float center, float half_span) {\n"
    "    return 1.0 - step(half_span, abs(z - center));\n"
    "}\n"
    "\n"
    "float corridor_room_mix(float z) {\n"
    "    return max(max(room_band(z, 10.8, 1.85), room_band(z, 18.6, 2.2)), room_band(z, 25.1, 1.95));\n"
    "}\n"
    "\n"
    "float corridor_half_width(float z) {\n"
    "    float w = 1.36;\n"
    "    w = max(w, mix(1.36, 4.25, room_band(z, 10.8, 1.85)));\n"
    "    w = max(w, mix(1.36, 5.10, room_band(z, 18.6, 2.2)));\n"
    "    w = max(w, mix(1.36, 4.65, room_band(z, 25.1, 1.95)));\n"
    "    return w;\n"
    "}\n"
    "\n"
    "vec3 impossible_room_color(vec3 entry, vec3 rd, float ttime) {\n"
    "    vec3 ro2 = vec3(0.10, entry.y, (entry.z - 10.8) * 2.55);\n"
    "    vec3 rd2 = normalize(vec3(max(0.10, abs(rd.x)), rd.y * 0.82, rd.z * 2.30));\n"
    "    float best2 = 1e20;\n"
    "    int smat = 0;\n"
    "    vec3 hp = vec3(0.0);\n"
    "    float tx = (8.8 - ro2.x) / rd2.x;\n"
    "    vec3 px = ro2 + rd2 * tx;\n"
    "    if (tx > 0.0 && px.y > 0.0 && px.y < 2.75 && abs(px.z) < 5.9) { best2 = tx; smat = 1; hp = px; }\n"
    "    if (abs(rd2.y) > 0.001) {\n"
    "        float ty = (0.0 - ro2.y) / rd2.y;\n"
    "        vec3 py = ro2 + rd2 * ty;\n"
    "        if (ty > 0.0 && ty < best2 && py.x > 0.0 && py.x < 9.2 && abs(py.z) < 5.9) { best2 = ty; smat = 2; hp = py; }\n"
    "        ty = (2.75 - ro2.y) / rd2.y;\n"
    "        py = ro2 + rd2 * ty;\n"
    "        if (ty > 0.0 && ty < best2 && py.x > 0.0 && py.x < 9.2 && abs(py.z) < 5.9) { best2 = ty; smat = 3; hp = py; }\n"
    "    }\n"
    "    if (abs(rd2.z) > 0.001) {\n"
    "        float tz = (5.9 * sign(rd2.z) - ro2.z) / rd2.z;\n"
    "        vec3 pz = ro2 + rd2 * tz;\n"
    "        if (tz > 0.0 && tz < best2 && pz.x > 0.0 && pz.x < 9.2 && pz.y > 0.0 && pz.y < 2.75) { best2 = tz; smat = 4; hp = pz; }\n"
    "    }\n"
    "    vec3 paper = texture(u_wall_tex, vec2(hp.x * 0.18 + hp.z * 0.035, hp.y * 0.55)).rgb;\n"
    "    vec3 col = paper * vec3(0.86, 0.78, 0.48);\n"
    "    if (smat == 2) { col = vec3(0.26, 0.20, 0.12) * (0.70 + texture(u_wall_tex, hp.xz * 0.13).r * 0.36); }\n"
    "    if (smat == 3) { col = vec3(0.40, 0.38, 0.30); }\n"
    "    if (smat == 4) { col *= vec3(0.52, 0.48, 0.34); }\n"
    "    float seam_x = 1.0 - smoothstep(0.025, 0.070, abs(fract(hp.x * 0.82) - 0.5));\n"
    "    float seam_z = 1.0 - smoothstep(0.030, 0.085, abs(fract((hp.z + 5.9) * 0.48) - 0.5));\n"
    "    float stain = smoothstep(0.70, 0.96, hash12(floor(hp.xz * vec2(1.7, 2.3)))) * (1.0 - smoothstep(0.30, 2.20, hp.y));\n"
    "    float corner_shadow = max(smoothstep(4.1, 5.9, abs(hp.z)), smoothstep(6.5, 8.8, hp.x));\n"
    "    float floor_wet = (smat == 2 ? 1.0 : 0.0) * smoothstep(2.2, 7.4, hp.x) * (0.55 + 0.45 * hash12(floor(hp.xz * 2.0)));\n"
    "    float ceiling_strip = (smat == 3 ? 1.0 : 0.0) * (1.0 - smoothstep(0.10, 0.22, abs(hp.z))) * (0.65 + 0.35 * step(0.25, sin(hp.x * 2.6 + ttime * 0.5)));\n"
    "    float back_door = (smat == 1 ? 1.0 : 0.0) * (1.0 - smoothstep(0.70, 1.05, abs(hp.z))) * smoothstep(0.12, 0.22, hp.y) * (1.0 - smoothstep(1.64, 1.92, hp.y));\n"
    "    float copied_mark = (smat == 1 ? 1.0 : 0.0) * (1.0 - smoothstep(0.075, 0.16, abs((hp.y - 1.14) - hp.z * 0.20)));\n"
    "    copied_mark *= (1.0 - smoothstep(0.075, 0.16, abs((hp.y - 1.14) + hp.z * 0.20))) * smoothstep(0.40, 0.65, hp.y) * (1.0 - smoothstep(1.62, 1.88, hp.y));\n"
    "    float light = 0.18 + ceiling_strip * 1.72 + 1.05 / (1.0 + length(hp - vec3(4.8, 2.52, 0.0)) * 0.95);\n"
    "    col *= light;\n"
    "    col = mix(col, col * vec3(0.48, 0.43, 0.30), stain * 0.62);\n"
    "    col = mix(col, col * vec3(0.28, 0.25, 0.20), corner_shadow * 0.50);\n"
    "    col += vec3(0.20, 0.15, 0.07) * floor_wet;\n"
    "    col = mix(col, vec3(0.020, 0.018, 0.012), back_door * 0.76);\n"
    "    col += vec3(1.00, 0.88, 0.46) * ceiling_strip * 0.58;\n"
    "    col = mix(col, col * 0.58, max(seam_x, seam_z) * 0.36);\n"
    "    col += vec3(1.0, 0.04, 0.0) * copied_mark * 0.90;\n"
    "    float fog2 = smoothstep(3.2, 9.0, best2);\n"
    "    return mix(col, vec3(0.055, 0.050, 0.030), fog2 * 0.42);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 frag = v_uv * u_resolution_time.xy;\n"
    "    vec2 p = (frag - 0.5 * u_resolution_time.xy) / max(u_resolution_time.y, 1.0);\n"
    "    float ttime = u_resolution_time.z;\n"
    "    p.x += (u_pressure.x + u_horror.y * 0.7) * (0.035 * sin(ttime * 1.7 + u_player.y * 0.55) + 0.018 * sin(ttime * 5.1));\n"
    "    p.y += (u_pressure.x + u_horror.y * 0.6) * 0.014 * sin(ttime * 2.3 + u_player.x * 3.0);\n"
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
    "    float texit = (0.12 - ro.z) / rd.z;\n"
    "    vec3 pexit = ro + rd * texit;\n"
    "    if (texit > 0.0 && abs(pexit.x) < 0.68 && pexit.y > 0.0 && pexit.y < 2.15 && texit < best) { best = texit; normal = vec3(0.0, 0.0, 1.0); mat = 4; }\n"
    "    float tmaze = 0.08;\n"
    "    float prev_tmaze = tmaze;\n"
    "    for (int i = 0; i < 118; ++i) {\n"
    "        vec3 pm = ro + rd * tmaze;\n"
    "        if (pm.y >= 0.0 && pm.y <= 2.55) {\n"
    "            float hw = corridor_half_width(pm.z);\n"
    "            if (abs(pm.x) > hw || pm.z < 0.0 || pm.z > 34.0) {\n"
    "                if (tmaze < best) {\n"
    "                    float lo = prev_tmaze;\n"
    "                    float hi = tmaze;\n"
    "                    for (int j = 0; j < 6; ++j) {\n"
    "                        float mid = 0.5 * (lo + hi);\n"
    "                        vec3 bm = ro + rd * mid;\n"
    "                        float bhw = corridor_half_width(bm.z);\n"
    "                        if (abs(bm.x) > bhw || bm.z < 0.0 || bm.z > 34.0) { hi = mid; } else { lo = mid; }\n"
    "                    }\n"
    "                    best = hi;\n"
    "                    pm = ro + rd * best;\n"
    "                    hw = corridor_half_width(pm.z);\n"
    "                    normal = abs(pm.x) > hw ? vec3(-sign(pm.x), 0.0, 0.0) : vec3(0.0, 0.0, pm.z < 0.0 ? 1.0 : -1.0);\n"
    "                    mat = 3;\n"
    "                }\n"
    "                break;\n"
    "            }\n"
    "        }\n"
    "        prev_tmaze = tmaze;\n"
    "        tmaze += 0.055 + tmaze * 0.012;\n"
    "    }\n"
    "\n"
    "    float thandle = sphere_hit(ro, rd, vec3(3.35, 0.44, 18.6), 0.22);\n"
    "    if (u_puzzle.y < 0.5 && thandle > 0.0 && thandle < best) {\n"
    "        best = thandle; normal = normalize(ro + rd * thandle - vec3(3.35, 0.44, 18.6)); mat = 7;\n"
    "    }\n"
    "    float tplaced_handle = sphere_hit(ro, rd, vec3(-4.16, 1.05, 25.15), 0.055);\n"
    "    if (u_puzzle.z > 0.5 && tplaced_handle > 0.0 && tplaced_handle < best) {\n"
    "        best = tplaced_handle; normal = normalize(ro + rd * tplaced_handle - vec3(-4.16, 1.05, 25.15)); mat = 8;\n"
    "    }\n"
    "\n"
    "    float entity_dist = max(2.55, mix(8.6, 4.4, u_pressure.y) - u_horror.y * 2.2);\n"
    "    float entity_z = max(u_player.y - entity_dist, 2.9);\n"
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
    "    if (mat == 5) { albedo = vec3(0.05, 0.04, 0.025); }\n"
    "    if (mat == 6) { albedo = vec3(0.004, 0.006, 0.008); }\n"
    "    if (mat == 7) { albedo = vec3(1.05, 0.72, 0.34); }\n"
    "    if (mat == 8) { albedo = vec3(0.95, 0.78, 0.42); }\n"
    "\n"
    "    float fixture_z = floor((hit.z + 2.6) / 5.2) * 5.2;\n"
    "    float flicker = 0.76 + 0.24 * step(0.18, hash12(vec2(floor(ttime * 13.0), fixture_z)));\n"
    "    float light_dist = length(vec3(hit.x, hit.y - 2.42, hit.z - fixture_z));\n"
    "    float blackout_flicker = mix(1.0, 0.10 + 0.22 * step(0.34, hash12(vec2(floor(ttime * 19.0), fixture_z + u_player.y))), u_horror.x);\n"
    "    float ceiling_light = 1.55 / (1.0 + light_dist * light_dist * 1.1) * flicker * blackout_flicker;\n"
    "    float exit_light = u_state.x * 3.2 / (1.0 + length(hit - vec3(0.0, 1.25, 0.28)) * 1.7);\n"
    "    float fuse_light = u_horror.w * (1.0 - u_state.x) * 1.55 / (1.0 + length(hit - vec3(-4.25, 1.0, 25.15)) * 1.65);\n"
    "    float cone = smoothstep(0.72, 0.98, dot(rd, normalize(vec3(fwd.x, -0.03, fwd.y)))) * u_state.z;\n"
    "    float flashlight = cone * mix(2.5, 3.25, u_horror.x) / (1.0 + best * best * 0.035);\n"
    "    float contact = 1.0 - 0.34 * exp(-abs(abs(hit.x) - corridor_half_width(hit.z)) * 8.0) * smoothstep(0.0, 0.22, hit.y);\n"
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
    "    float room_mix = corridor_room_mix(hit.z);\n"
    "    float red_room = room_band(hit.z, 18.6, 1.6) * smoothstep(2.15, 3.1, hit.x);\n"
    "    float dead_end_shadow = room_band(hit.z, 10.8, 1.35) * smoothstep(1.8, 2.65, -hit.x);\n"
    "    float exit_wall = room_band(hit.z, 25.1, 1.5) * smoothstep(3.4, 4.1, -hit.x);\n"
    "    float exit_door_w = abs(hit.z - 25.15);\n"
    "    float exit_door_h = hit.y;\n"
    "    float exit_door = exit_wall * (1.0 - smoothstep(0.62, 0.92, exit_door_w)) * smoothstep(0.10, 0.18, exit_door_h) * (1.0 - smoothstep(1.58, 1.82, exit_door_h));\n"
    "    float exit_frame = exit_wall * max(1.0 - smoothstep(0.02, 0.08, abs(exit_door_w - 0.68)), 1.0 - smoothstep(0.02, 0.08, min(abs(exit_door_h - 0.12), abs(exit_door_h - 1.62))));\n"
    "    float mark_wall = 0.0;\n"
    "    float forged_mark = 0.0;\n"
    "    float impossible_cut = 0.0;\n"
    "    float impossible_frame = 0.0;\n"
    "    if (mat == 3) {\n"
    "        float mark_side = smoothstep(3.20, 3.95, -hit.x);\n"
    "        float mark_a = 1.0 - smoothstep(0.025, 0.075, abs((hit.y - 1.13) - (hit.z - 10.8) * 0.42));\n"
    "        float mark_b = 1.0 - smoothstep(0.025, 0.075, abs((hit.y - 1.13) + (hit.z - 10.8) * 0.42));\n"
    "        float mark_box = room_band(hit.z, 10.8, 0.62) * smoothstep(0.58, 0.78, hit.y) * (1.0 - smoothstep(1.54, 1.74, hit.y));\n"
    "        mark_wall = u_puzzle.x * mark_side * mark_box * max(mark_a, mark_b);\n"
    "        float forged_box = room_band(hit.z, 25.15, 0.72) * smoothstep(0.58, 0.78, hit.y) * (1.0 - smoothstep(1.54, 1.74, hit.y));\n"
    "        float forged_a = 1.0 - smoothstep(0.03, 0.09, abs((hit.y - 1.10) - (hit.z - 25.15) * 0.37));\n"
    "        float forged_b = 1.0 - smoothstep(0.03, 0.09, abs((hit.y - 1.10) + (hit.z - 25.15) * 0.37));\n"
    "        forged_mark = u_puzzle.x * exit_wall * forged_box * max(forged_a, forged_b);\n"
    "        float cut_band = room_band(hit.z, 10.8, 1.10) * smoothstep(3.36, 4.12, hit.x);\n"
    "        float cut_y = smoothstep(0.22, 0.38, hit.y) * (1.0 - smoothstep(1.70, 1.92, hit.y));\n"
    "        impossible_cut = u_puzzle.x * cut_band * cut_y;\n"
    "        float frame_z = 1.0 - smoothstep(0.045, 0.13, abs(abs(hit.z - 10.8) - 1.08));\n"
    "        float frame_y = max(1.0 - smoothstep(0.035, 0.12, abs(hit.y - 0.30)), 1.0 - smoothstep(0.035, 0.12, abs(hit.y - 1.78)));\n"
    "        impossible_frame = u_puzzle.x * cut_band * max(frame_z * cut_y, frame_y * room_band(hit.z, 10.8, 1.20));\n"
    "    }\n"
    "    float fixture_shape = 0.0;\n"
    "    if (mat == 2) {\n"
    "        float fixture_w = mix(0.13, 0.34, room_mix);\n"
    "        fixture_shape = (1.0 - smoothstep(fixture_w, fixture_w + 0.07, abs(hit.x))) * (1.0 - smoothstep(0.72, 0.92, abs(hit.z - fixture_z)));\n"
    "    }\n"
    "\n"
    "    float wall_damp = (mat == 3 ? 1.0 : 0.0) * (1.0 - smoothstep(0.10, 1.85, hit.y)) * smoothstep(0.72, 0.96, hash12(floor(hit.zy * vec2(2.0, 4.0))));\n"
    "    float carpet_wear = (mat == 1 ? 1.0 : 0.0) * smoothstep(0.54, 0.98, hash12(floor(hit.xz * 2.4))) * smoothstep(3.5, 18.0, hit.z);\n"
    "    float fixture_shadow = (mat == 2 ? 1.0 : 0.0) * (1.0 - smoothstep(0.18, 0.52, abs(hit.x))) * (1.0 - smoothstep(0.45, 1.10, abs(hit.z - fixture_z)));\n"
    "    vec3 color = albedo * (0.13 + ceiling_light + exit_light + fuse_light + flashlight) * contact;\n"
    "    color = mix(color, color * vec3(0.48, 0.42, 0.30), wall_damp * 0.56);\n"
    "    color = mix(color, color * vec3(0.56, 0.45, 0.32), carpet_wear * 0.42);\n"
    "    color = mix(color, color * 0.38, fixture_shadow * 0.22);\n"
    "    color += vec3(1.0, 0.92, 0.66) * fixture_shape * (1.05 + 0.75 * flicker);\n"
    "    color += vec3(1.15, 0.04, 0.0) * fixture_shape * u_horror.x * (0.7 + 0.5 * step(0.45, sin(ttime * 15.0)));\n"
    "    color *= 1.0 - side_opening * 0.86;\n"
    "    color = mix(color, vec3(0.006, 0.006, 0.004), dead_end_shadow * 0.62);\n"
    "    color = mix(color, vec3(0.16, 0.0, 0.0), red_room * (0.52 + u_pressure.x * 0.25));\n"
    "    color = mix(color, vec3(0.018, 0.014, 0.010), exit_door * (0.82 + 0.15 * u_puzzle.w));\n"
    "    color = mix(color, vec3(0.04, 0.0, 0.0), exit_door * (1.0 - u_puzzle.z) * 0.35);\n"
    "    color += vec3(0.55, 0.16, 0.04) * exit_door * (1.0 - u_puzzle.z) * 0.34;\n"
    "    color += vec3(1.0, 0.58, 0.18) * exit_frame * (0.10 + 0.46 * u_puzzle.w);\n"
    "    vec3 impossible_col = impossible_room_color(hit, rd, ttime);\n"
    "    color = mix(color, impossible_col, impossible_cut * 0.96);\n"
    "    color = mix(color, vec3(0.018, 0.014, 0.008), impossible_frame * 0.76);\n"
    "    color += vec3(0.92, 0.70, 0.34) * impossible_frame * 0.35;\n"
    "    color = mix(color, color * 0.34, impossible_frame * smoothstep(0.0, 1.0, abs(hit.z - 10.8)) * 0.18);\n"
    "    color += vec3(1.0, 0.08, 0.03) * mark_wall * (0.85 + 0.15 * sin(ttime * 9.0));\n"
    "    color += vec3(1.0, 0.02, 0.0) * forged_mark * (0.55 + 0.25 * step(0.45, sin(ttime * 11.0)));\n"
    "    color = mix(color, vec3(0.005, 0.012, 0.008), false_exit * 0.92);\n"
    "    color += vec3(0.1, 1.1, 0.34) * false_exit * (0.42 + 0.58 * step(0.5, sin(ttime * 6.0 + hit.z)));\n"
    "    color = mix(color, vec3(0.11, 0.0, 0.0), route_bad_glow * 0.46);\n"
    "    color += vec3(0.08, 1.05, 0.42) * route_safe_glow * (0.75 + u_pressure.x);\n"
    "    color += vec3(1.0, 0.08, 0.0) * route_bad_glow * 0.22;\n"
    "    color = mix(color, color * vec3(0.34, 0.24, 0.18), u_horror.x * 0.62);\n"
    "    color += vec3(0.72, 0.015, 0.0) * u_horror.y * (0.11 + 0.09 * sin(ttime * 18.0));\n"
    "    color += vec3(0.05, 0.75, 0.34) * u_horror.z * max(0.0, 1.0 - best / 30.0) * 0.16;\n"
    "    if (mat == 5) color += vec3(0.12, 0.06, 0.02);\n"
    "    if (mat == 6) {\n"
    "        float eye_l = 1.0 - smoothstep(0.015, 0.05, length(hit.xy - vec2(-0.08, 1.55)));\n"
    "        float eye_r = 1.0 - smoothstep(0.015, 0.05, length(hit.xy - vec2(0.08, 1.55)));\n"
    "        color *= 0.035;\n"
    "        color += vec3(1.2, 0.03, 0.0) * max(eye_l, eye_r) * (0.4 + u_pressure.y);\n"
    "    }\n"
    "    if (mat == 7) { color += vec3(1.2, 0.76, 0.22) * 0.75; }\n"
    "    if (mat == 8) { color += vec3(1.1, 0.86, 0.35) * (0.45 + 0.25 * u_puzzle.w); }\n"
    "    float fog = smoothstep(13.0, 31.0, best);\n"
    "    vec3 fog_col = mix(vec3(0.18, 0.15, 0.06), vec3(0.03, 0.035, 0.045), u_state.x * 0.55);\n"
    "    fog_col = mix(fog_col, vec3(0.06, 0.018, 0.015), u_pressure.y * 0.35);\n"
    "    color = mix(color, fog_col, fog);\n"
    "    float vignette = smoothstep(0.95, 0.22, length(p));\n"
    "    color *= 0.55 + 0.45 * vignette;\n"
    "    float fear_pulse = u_state.y * (0.08 + 0.07 * sin(ttime * 9.0));\n"
    "    color = mix(color, vec3(0.07, 0.0, 0.0), fear_pulse);\n"
    "    color = mix(color, vec3(0.12, 0.0, 0.0), u_horror.y * (0.10 + 0.08 * sin(ttime * 20.0)));\n"
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

static bool in_z_band(float z, float center, float half_span) { return absf(z - center) <= half_span; }

static float maze_walk_half_width(float z) {
    float width = 1.05F;
    if (in_z_band(z, 10.8F, 2.0F)) {
        width = fmaxf(width, 4.0F);
    }
    if (in_z_band(z, 18.6F, 2.35F)) {
        width = fmaxf(width, 4.85F);
    }
    if (in_z_band(z, 25.1F, 2.05F)) {
        width = fmaxf(width, 4.4F);
    }
    return width;
}

static bool maze_walkable(float x, float z) {
    if (z < 0.45F || z > 31.8F) {
        return false;
    }
    return absf(x) <= maze_walk_half_width(z);
}

static int maze_zone_for_pos(float x, float z) {
    if (in_z_band(z, 10.8F, 2.0F) && x < -1.35F) {
        return MAZE_ZONE_LEFT_DEADEND;
    }
    if (in_z_band(z, 18.6F, 2.35F) && x > 1.45F) {
        return MAZE_ZONE_RED_ROOM;
    }
    if (in_z_band(z, 25.1F, 2.05F) && x < -1.45F) {
        return MAZE_ZONE_FUSE_ROOM;
    }
    return MAZE_ZONE_MAIN;
}

static int count_room_bits(int mask) {
    int count = 0;
    for (int bit = 0; bit < 8; ++bit) {
        count += (mask & (1 << bit)) != 0 ? 1 : 0;
    }
    return count;
}

static bool near_mark_surface(void) { return !s_game.mark_placed && dist_to(s_game.x, s_game.z, MARK_X, MARK_Z) < 1.55F; }

static bool near_door_handle(void) { return !s_game.door_handle_collected && dist_to(s_game.x, s_game.z, HANDLE_X, HANDLE_Z) < 1.35F; }

static bool near_locked_door(void) { return s_game.side_room_visits >= 3 && dist_to(s_game.x, s_game.z, FUSE_X, FUSE_Z) < 1.45F; }

static bool near_fuse(void) { return !s_game.fuse_found && s_game.portal_exit_revealed && near_locked_door(); }

static bool near_exit(void) { return s_game.fuse_found && s_game.z < 1.85F && absf(s_game.x) < 0.9F; }

static bool can_use_context(void) {
    return !s_game.won && !s_game.caught && (near_mark_surface() || near_door_handle() || near_locked_door() || near_exit());
}

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

static bool blackout_active(void) { return s_game.blackout_timer > 0.0F || s_game.ambush_timer > 0.0F; }

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

static void update_movement_audio(bool moving) {
    if (moving && s_game.footstep_timer <= 0.0F) {
        if (s_game.sprinting) {
            game_audio_play(GAME_AUDIO_CUE_SPRINT_STEP);
            s_game.footstep_timer = blackout_active() ? 0.17F : 0.21F;
        } else {
            game_audio_play(GAME_AUDIO_CUE_FOOTSTEP);
            s_game.footstep_timer = 0.38F;
        }
    }
    if (blackout_active() && s_game.heartbeat_timer <= 0.0F) {
        game_audio_play(GAME_AUDIO_CUE_HEARTBEAT);
        s_game.heartbeat_timer = s_game.sprinting ? 0.42F : 0.58F;
        if (s_game.stalker_pressure > 0.72F) {
            s_game.heartbeat_timer *= 0.78F;
        }
    }
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
    set_message("MARK WALL. FIND HANDLE", 3.0F);
}

static void record_run_result(void) {
    s_game.last_run_time = s_game.run_time;
    s_game.last_fear = s_game.fear;
    s_game.last_battery = s_game.battery;
}

static void update_maze_lostness(void) {
    const int next_zone = maze_zone_for_pos(s_game.x, s_game.z);
    if (next_zone == s_game.maze_zone) {
        return;
    }
    s_game.maze_zone = next_zone;
    if (next_zone == MAZE_ZONE_MAIN || s_game.won || s_game.caught) {
        return;
    }

    const int bit = 1 << (next_zone - 1);
    if ((s_game.visited_rooms_mask & bit) == 0) {
        s_game.visited_rooms_mask |= bit;
        s_game.side_room_visits = count_room_bits(s_game.visited_rooms_mask);
        s_game.layout_shift_count += 1;
        s_game.route_shift = clampf(s_game.route_shift + 0.12F, 0.0F, 1.0F);
        s_game.fear = clampf(s_game.fear + 3.0F, 0.0F, 100.0F);

        if (next_zone == MAZE_ZONE_LEFT_DEADEND) {
            set_message("THE HALL REPEATS", 1.7F);
        } else if (next_zone == MAZE_ZONE_RED_ROOM) {
            s_game.blackout_timer = fmaxf(s_game.blackout_timer, 1.1F);
            s_game.ambush_timer = fmaxf(s_game.ambush_timer, 0.8F);
            s_game.yaw += 0.38F;
            set_message(s_game.door_handle_collected ? "THIS ROOM MOVED" : "HANDLE ON THE FLOOR", 1.9F);
        } else if (next_zone == MAZE_ZONE_FUSE_ROOM) {
            set_message(s_game.door_handle_collected ? "DOOR HAS NO PLACE OUTSIDE" : "LOCKED - NO HANDLE", 1.8F);
            game_audio_play(GAME_AUDIO_CUE_FUSE_HUM);
        }
    } else if (next_zone == MAZE_ZONE_RED_ROOM && s_game.message_timer <= 0.0F) {
        set_message("RED ROOM - BACK OUT", 1.2F);
    }
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
        s_game.relief_timer = 2.0F;
        set_message("GOOD TURN", 1.4F);
    } else {
        s_game.route_choice_wrong += 1;
        s_game.fear = clampf(s_game.fear + 16.0F, 0.0F, 100.0F);
        s_game.stalker_pressure = clampf(s_game.stalker_pressure + 0.23F, 0.0F, 1.0F);
        s_game.route_shift = clampf(s_game.route_shift + 0.18F, 0.0F, 1.0F);
        s_game.blackout_timer = 3.2F;
        s_game.ambush_timer = 2.4F;
        s_game.relief_timer = 0.0F;
        game_audio_play(GAME_AUDIO_CUE_STALKER);
        set_message(chosen_side == 0 ? "LIGHTS OUT - MOVE" : "WRONG TURN - RUN", 2.2F);
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
            const int fleck = ((x * 17 + y * 31 + ((x * y) % 19)) & 23) == 0;
            const int stain = (((x * 3 + y * 11) & 63) < 6 && ((x + y * 5) & 15) < 5);
            const int vertical_wear = (x % 32 == 30 || x % 32 == 1) && ((y * 7 + x) & 7) < 5;
            int r = 176 + ((x * 5 + y * 3) & 17);
            int g = 156 + ((x * 7 + y * 11) & 15);
            int b = 76 + ((x * 13 + y * 2) & 11);
            if (seam) {
                r -= 42;
                g -= 39;
                b -= 22;
            }
            if (fleck) {
                r -= 50;
                g -= 42;
                b -= 24;
            }
            if (stain) {
                r -= 36;
                g -= 32;
                b -= 14;
            }
            if (vertical_wear) {
                r += 18;
                g += 14;
                b += 6;
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

static void ui_panel(int x, int y, int w, int h, bool danger) {
    ui_rect(x + 3, y + 4, w, h, 0, 0, 0, danger ? 118 : 88);
    ui_rect(x, y, w, h, danger ? 22 : 11, danger ? 3 : 9, danger ? 3 : 7, danger ? 178 : 146);
    ui_rect(x, y, w, 2, danger ? 144 : 104, danger ? 35 : 84, danger ? 28 : 46, danger ? 168 : 112);
    ui_rect(x, y + h - 2, w, 2, 0, 0, 0, 86);
    ui_rect(x, y, 2, h, 0, 0, 0, 64);
    ui_rect(x + w - 2, y, 2, h, 0, 0, 0, 64);
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

static void build_ui(void) {
    ui_clear();
    const float fuse_dist = dist_to(s_game.x, s_game.z, FUSE_X, FUSE_Z);
    char line[96];

    ui_rect(476, 268, 8, 2, 230, 235, 220, 220);
    ui_rect(479, 265, 2, 8, 230, 235, 220, 220);

    if (!s_game.won && !s_game.caught && blackout_active()) {
        ui_panel(284, 396, 392, 42, true);
        ui_text(290, 406, "LIGHTS OUT - SPRINT", 3, 255, 138, 112, 255);
    } else if (!s_game.won && !s_game.caught && route_choice_active()) {
        ui_panel(260, 396, 440, 42, false);
        (void)snprintf(line, sizeof(line), "MOVE %s - TRUST HUM", route_choice_side_name(route_choice_safe_side_for_stage(s_game.route_choice_stage)));
        ui_text(288, 406, line, 3, 132, 255, 184, 255);
    } else if (!s_game.won && !s_game.caught && s_game.relief_timer > 0.0F) {
        ui_panel(306, 396, 348, 42, false);
        ui_text(340, 406, "SAFE TURN - MOVE", 3, 132, 255, 184, 255);
    } else if (!s_game.won && !s_game.caught && near_mark_surface()) {
        ui_panel(306, 396, 348, 42, false);
        ui_text(338, 406, "PRESS E - DRAW MARK", 3, 255, 226, 130, 255);
    } else if (!s_game.won && !s_game.caught && near_door_handle()) {
        ui_panel(314, 396, 332, 42, false);
        ui_text(348, 406, "PRESS E - TAKE HANDLE", 3, 255, 226, 130, 255);
    } else if (!s_game.won && !s_game.caught && near_locked_door() && !s_game.portal_exit_revealed) {
        ui_panel(310, 396, 340, 42, false);
        ui_text(338, 406, s_game.door_handle_collected ? "PRESS E - FIT HANDLE" : "PRESS E - TRY DOOR", 3, 255, 226, 130, 255);
    } else if (!s_game.won && !s_game.caught && near_fuse()) {
        ui_panel(318, 396, 324, 42, false);
        ui_text(352, 406, "PRESS E - ENTER", 3, 255, 226, 130, 255);
    } else if (!s_game.won && !s_game.caught && near_exit()) {
        ui_panel(334, 396, 292, 42, false);
        ui_text(358, 406, "PRESS E - ESCAPE", 3, 255, 226, 130, 255);
    }

    if (!s_game.won && !s_game.caught) {
        ui_panel(24, 458, 350, 58, false);
        ui_text(42, 470, "JOURNAL", 2, 246, 226, 146, 225);
        if (!s_game.mark_placed) {
            ui_text(42, 496, "TASK: DRAW A WALL MARK", 2, 230, 238, 208, 238);
        } else if (!s_game.door_handle_collected) {
            ui_text(42, 496, "TASK: FIND DOOR HANDLE", 2, 230, 238, 208, 238);
        } else if (!s_game.portal_exit_revealed) {
            ui_text(42, 496, "TASK: FIT HANDLE TO DOOR", 2, 230, 238, 208, 238);
        } else if (!s_game.fuse_found) {
            ui_text(42, 496, "TASK: ENTER REAL EXIT", 2, 230, 238, 208, 238);
        } else {
            (void)snprintf(line, sizeof(line), "TASK: CROSS %d MORE ROOMS", 3 - s_game.side_room_visits);
            ui_text(42, 496, line, 2, 230, 238, 208, 238);
        }
        if (s_game.battery < 0.18F || !s_game.flashlight_on) {
            ui_text(252, 470, "DYNAMO", 1, 255, 190, 128, 220);
        } else if (s_game.side_room_visits >= 3 && !s_game.fuse_found) {
            (void)snprintf(line, sizeof(line), "HUM %.0fM", (double)fuse_dist);
            ui_text(252, 470, line, 1, 230, 218, 156, 210);
        }
    }

    if (!s_game.won && !s_game.caught && s_game.message_timer > 0.0F) {
        ui_panel(302, 344, 356, 34, false);
        ui_text(326, 354, s_game.message, 2, 255, 236, 170, 240);
    }
    if (s_game.won) {
        ui_panel(220, 210, 520, 132, false);
        ui_text(338, 206, "ESCAPED", 4, 255, 228, 128, 255);
        (void)snprintf(line, sizeof(line), "TIME:%02dS  ROOMS:%d  SHIFTS:%d", (int)s_game.last_run_time, s_game.side_room_visits, s_game.layout_shift_count);
        ui_text(260, 258, line, 2, 220, 238, 210, 255);
        ui_text(292, 296, "PRESS E - NEW RUN", 3, 132, 255, 184, 255);
    }
    if (s_game.caught) {
        ui_panel(208, 210, 544, 132, true);
        ui_text(292, 226, "LOST IN THE LIGHTS", 3, 255, 130, 112, 255);
        (void)snprintf(line, sizeof(line), "TIME:%02dS  ROOMS:%d  SHIFTS:%d", (int)s_game.last_run_time, s_game.side_room_visits, s_game.layout_shift_count);
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
    if (near_mark_surface()) {
        s_game.mark_placed = true;
        s_game.route_shift = clampf(s_game.route_shift + 0.18F, 0.0F, 1.0F);
        s_game.fear = clampf(s_game.fear + 4.0F, 0.0F, 100.0F);
        set_message("YOUR MARK APPEARS AHEAD", 2.2F);
        return;
    }
    if (near_door_handle()) {
        s_game.door_handle_collected = true;
        s_game.blackout_timer = fmaxf(s_game.blackout_timer, 0.65F);
        s_game.ambush_timer = fmaxf(s_game.ambush_timer, 0.35F);
        set_message("TOOK A DOOR HANDLE", 1.8F);
        return;
    }
    if (near_locked_door() && !s_game.portal_exit_revealed) {
        if (!s_game.door_handle_collected) {
            set_message("LOCKED - HANDLE MISSING", 1.6F);
            s_game.fear = clampf(s_game.fear + 2.0F, 0.0F, 100.0F);
            return;
        }
        s_game.door_handle_placed = true;
        s_game.portal_exit_revealed = true;
        s_game.relief_timer = fmaxf(s_game.relief_timer, 1.2F);
        s_game.route_shift = clampf(s_game.route_shift + 0.22F, 0.0F, 1.0F);
        game_audio_play(GAME_AUDIO_CUE_FUSE_HUM);
        set_message("THE ROOM OPENS INWARD", 2.4F);
        return;
    }
    if (near_fuse()) {
        s_game.fuse_found = true;
        s_game.fear = clampf(s_game.fear + 18.0F, 0.0F, 100.0F);
        s_game.route_shift = 0.18F;
        s_game.stalker_pressure = fmaxf(s_game.stalker_pressure, 0.22F);
        s_game.won = true;
        record_run_result();
        game_audio_play(GAME_AUDIO_CUE_ESCAPE);
        set_message("FOUND A WAY OUT", 8.0F);
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
    if (s_game.footstep_timer > 0.0F) {
        s_game.footstep_timer -= dt;
    }
    if (s_game.heartbeat_timer > 0.0F) {
        s_game.heartbeat_timer -= dt;
    }
    if (s_game.route_choice_feedback_timer > 0.0F) {
        s_game.route_choice_feedback_timer -= dt;
    }
    if (s_game.blackout_timer > 0.0F) {
        s_game.blackout_timer -= dt;
    }
    if (s_game.ambush_timer > 0.0F) {
        s_game.ambush_timer -= dt;
    }
    if (s_game.relief_timer > 0.0F) {
        s_game.relief_timer -= dt;
    }
    if (s_game.dynamo_stall_timer > 0.0F) {
        s_game.dynamo_stall_timer -= dt;
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
        set_message(s_game.flashlight_on ? "DYNAMO ON" : "DYNAMO OFF", 0.9F);
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
    const bool moving = len > 0.001F;
    if (moving) {
        move_x /= len;
        move_z /= len;
        const bool wants_sprint = nt_input_key_is_down(NT_KEY_LSHIFT) || nt_input_key_is_down(NT_KEY_RSHIFT);
        s_game.sprinting = wants_sprint;
        const float speed = s_game.sprinting ? 5.05F : 3.15F;
        const float next_x = s_game.x + move_x * speed * dt;
        const float next_z = s_game.z + move_z * speed * dt;
        if (maze_walkable(next_x, s_game.z)) {
            s_game.x = next_x;
        }
        if (maze_walkable(s_game.x, next_z)) {
            s_game.z = next_z;
        }
    } else {
        s_game.sprinting = false;
    }
    s_game.z = clampf(s_game.z, 0.45F, 31.8F);
    s_game.x = clampf(s_game.x, -maze_walk_half_width(s_game.z), maze_walk_half_width(s_game.z));
    s_game.threat_visible = looking_at_stalker();
    update_maze_lostness();
    update_route_choice();
    update_movement_audio(moving);

    if (s_game.flashlight_on) {
        const float movement_charge = moving ? (s_game.sprinting ? 0.105F : 0.066F) : 0.0F;
        const float stall_drain = moving ? 0.024F : 0.086F;
        const float blackout_drain = blackout_active() ? 0.07F : 0.0F;
        s_game.battery = clampf(s_game.battery + dt * (movement_charge - stall_drain - blackout_drain), 0.0F, 1.0F);
        if (s_game.battery <= 0.035F) {
            s_game.flashlight_on = false;
            s_game.dynamo_stall_timer = 1.6F;
            game_audio_play(GAME_AUDIO_CUE_FLASHLIGHT);
            set_message("DYNAMO STALLED - MOVE", 1.5F);
        }
    } else {
        s_game.battery = clampf(s_game.battery + dt * (moving ? 0.18F : 0.018F), 0.0F, 1.0F);
        if (moving && s_game.battery > 0.22F && s_game.dynamo_stall_timer <= 0.0F) {
            s_game.flashlight_on = true;
            game_audio_play(GAME_AUDIO_CUE_FLASHLIGHT);
            set_message("DYNAMO CATCHES", 0.95F);
        }
    }

    if (s_game.fuse_found) {
        const float return_progress = clampf((FUSE_Z - s_game.z) / FUSE_Z, 0.0F, 1.0F);
        const float target_shift = clampf(0.24F + return_progress * 0.58F + (s_game.fear / 100.0F) * 0.16F, 0.0F, 1.0F);
        s_game.route_shift = approachf(s_game.route_shift, target_shift, dt * 0.55F);

        float stalker_delta = 0.045F + (s_game.fear / 100.0F) * 0.07F + (s_game.flashlight_on ? -0.016F : 0.052F);
        if (s_game.threat_visible) {
            stalker_delta += s_game.flashlight_on ? -0.055F : 0.12F;
        }
        if (near_exit()) {
            stalker_delta += 0.18F;
        }
        if (blackout_active()) {
            stalker_delta += 0.28F;
            if (s_game.sprinting) {
                stalker_delta -= 0.46F;
            }
        } else if (s_game.sprinting) {
            stalker_delta -= 0.045F;
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
        const float fuse_dist = dist_to(s_game.x, s_game.z, FUSE_X, FUSE_Z);
        if (fuse_dist < 11.0F && s_game.fuse_hum_timer <= 0.0F) {
            game_audio_play(GAME_AUDIO_CUE_FUSE_HUM);
            s_game.fuse_hum_timer = clampf(0.45F + fuse_dist * 0.10F, 0.55F, 1.45F);
        }
    }

    float fear_rate = 1.15F + s_game.z * 0.035F + (s_game.flashlight_on ? -0.55F : 1.1F);
    if (s_game.fuse_found) {
        fear_rate += 3.6F + s_game.route_shift * 1.2F + s_game.stalker_pressure * 3.4F;
        if (blackout_active()) {
            fear_rate += 2.0F;
            if (s_game.sprinting) {
                fear_rate -= 1.35F;
            }
        }
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
    nt_gfx_set_uniform_vec4("u_horror", (float[4]){clampf(s_game.blackout_timer / 3.2F, 0.0F, 1.0F), clampf(s_game.ambush_timer / 2.4F, 0.0F, 1.0F), clampf(s_game.relief_timer / 2.0F, 0.0F, 1.0F), clampf((float)s_game.side_room_visits / 3.0F, 0.0F, 1.0F)});
    nt_gfx_set_uniform_vec4("u_puzzle", (float[4]){s_game.mark_placed ? 1.0F : 0.0F, s_game.door_handle_collected ? 1.0F : 0.0F, s_game.door_handle_placed ? 1.0F : 0.0F, s_game.portal_exit_revealed ? 1.0F : 0.0F});
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
    cJSON_AddNumberToObject(root, "visited_rooms_mask", (double)s_game.visited_rooms_mask);
    cJSON_AddNumberToObject(root, "side_room_visits", (double)s_game.side_room_visits);
    cJSON_AddNumberToObject(root, "layout_shift_count", (double)s_game.layout_shift_count);
    cJSON_AddNumberToObject(root, "maze_zone", (double)s_game.maze_zone);
    cJSON_AddNumberToObject(root, "route_choice_stage", (double)s_game.route_choice_stage);
    cJSON_AddNumberToObject(root, "route_choice_total", (double)route_choice_total());
    cJSON_AddBoolToObject(root, "route_choice_active", route_choice_active());
    cJSON_AddStringToObject(root, "route_choice_safe_side", route_choice_active() ? route_choice_side_name(route_choice_safe_side_for_stage(s_game.route_choice_stage)) : "NONE");
    cJSON_AddNumberToObject(root, "route_choice_correct", (double)s_game.route_choice_correct);
    cJSON_AddNumberToObject(root, "route_choice_wrong", (double)s_game.route_choice_wrong);
    cJSON_AddBoolToObject(root, "blackout_active", blackout_active());
    cJSON_AddNumberToObject(root, "blackout_timer", (double)fmaxf(0.0F, s_game.blackout_timer));
    cJSON_AddNumberToObject(root, "ambush_timer", (double)fmaxf(0.0F, s_game.ambush_timer));
    cJSON_AddNumberToObject(root, "relief_timer", (double)fmaxf(0.0F, s_game.relief_timer));
    cJSON_AddNumberToObject(root, "footstep_timer", (double)fmaxf(0.0F, s_game.footstep_timer));
    cJSON_AddNumberToObject(root, "heartbeat_timer", (double)fmaxf(0.0F, s_game.heartbeat_timer));
    cJSON_AddNumberToObject(root, "run_time", (double)s_game.run_time);
    cJSON_AddNumberToObject(root, "last_run_time", (double)s_game.last_run_time);
    cJSON_AddNumberToObject(root, "last_fear", (double)s_game.last_fear);
    cJSON_AddNumberToObject(root, "last_battery", (double)s_game.last_battery);
    cJSON_AddBoolToObject(root, "flashlight_on", s_game.flashlight_on);
    cJSON_AddBoolToObject(root, "sprinting", s_game.sprinting);
    cJSON_AddBoolToObject(root, "fuse_found", s_game.fuse_found);
    cJSON_AddBoolToObject(root, "mark_placed", s_game.mark_placed);
    cJSON_AddBoolToObject(root, "door_handle_collected", s_game.door_handle_collected);
    cJSON_AddBoolToObject(root, "door_handle_placed", s_game.door_handle_placed);
    cJSON_AddBoolToObject(root, "portal_exit_revealed", s_game.portal_exit_revealed);
    cJSON_AddBoolToObject(root, "exit_powered", s_game.portal_exit_revealed);
    cJSON_AddBoolToObject(root, "won", s_game.won);
    cJSON_AddBoolToObject(root, "caught", s_game.caught);
    cJSON_AddBoolToObject(root, "threat_visible", s_game.threat_visible);
    cJSON_AddBoolToObject(root, "can_mark", near_mark_surface());
    cJSON_AddBoolToObject(root, "can_take_handle", near_door_handle());
    cJSON_AddBoolToObject(root, "can_try_locked_door", near_locked_door() && !s_game.portal_exit_revealed);
    cJSON_AddBoolToObject(root, "can_use", can_use_context());
    cJSON_AddBoolToObject(root, "can_restart", s_game.won || s_game.caught);
    cJSON_AddStringToObject(root,
                            "objective",
                            s_game.caught ? "caught"
                                          : (s_game.won              ? "escaped"
                                             : (!s_game.mark_placed  ? "draw_mark"
                                                : (!s_game.door_handle_collected ? "find_handle"
                                                   : (!s_game.portal_exit_revealed ? "fit_handle" : "enter_exit")))));
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

static bool ep_game_action_place_mark(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    if (!s_game.won && !s_game.caught && near_mark_surface()) {
        interact();
    }
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
    const float requested_z = clampf((float)json_number(params, "z", (double)s_game.z), 0.45F, 31.8F);
    const float requested_x =
        clampf((float)json_number(params, "x", (double)s_game.x), -maze_walk_half_width(requested_z), maze_walk_half_width(requested_z));
    s_game.x = requested_x;
    s_game.z = requested_z;
    if (!maze_walkable(s_game.x, s_game.z)) {
        s_game.x = clampf(s_game.x, -1.05F, 1.05F);
    }
    s_game.yaw = (float)json_number(params, "yaw", (double)s_game.yaw);
    update_maze_lostness();
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
    const cJSON *mark = cJSON_IsObject(params) ? cJSON_GetObjectItemCaseSensitive(params, "mark_placed") : NULL;
    const cJSON *handle_collected = cJSON_IsObject(params) ? cJSON_GetObjectItemCaseSensitive(params, "door_handle_collected") : NULL;
    const cJSON *handle_placed = cJSON_IsObject(params) ? cJSON_GetObjectItemCaseSensitive(params, "door_handle_placed") : NULL;
    const cJSON *exit_revealed = cJSON_IsObject(params) ? cJSON_GetObjectItemCaseSensitive(params, "portal_exit_revealed") : NULL;
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
    if (cJSON_IsBool(mark)) {
        s_game.mark_placed = cJSON_IsTrue(mark);
    }
    if (cJSON_IsBool(handle_collected)) {
        s_game.door_handle_collected = cJSON_IsTrue(handle_collected);
    }
    if (cJSON_IsBool(handle_placed)) {
        s_game.door_handle_placed = cJSON_IsTrue(handle_placed);
    }
    if (cJSON_IsBool(exit_revealed)) {
        s_game.portal_exit_revealed = cJSON_IsTrue(exit_revealed);
    }
    s_game.fear = clampf((float)json_number(params, "fear", (double)s_game.fear), 0.0F, 100.0F);
    s_game.battery = clampf((float)json_number(params, "battery", (double)s_game.battery), 0.0F, 1.0F);
    s_game.route_shift = clampf((float)json_number(params, "route_shift", (double)s_game.route_shift), 0.0F, 1.0F);
    s_game.stalker_pressure = clampf((float)json_number(params, "stalker_pressure", (double)s_game.stalker_pressure), 0.0F, 1.0F);
    s_game.route_choice_stage = (int)clampf((float)json_number(params, "route_choice_stage", (double)s_game.route_choice_stage), 0.0F, (float)route_choice_total());
    s_game.route_choice_correct = (int)fmaxf(0.0F, (float)json_number(params, "route_choice_correct", (double)s_game.route_choice_correct));
    s_game.route_choice_wrong = (int)fmaxf(0.0F, (float)json_number(params, "route_choice_wrong", (double)s_game.route_choice_wrong));
    s_game.visited_rooms_mask = (int)fmaxf(0.0F, (float)json_number(params, "visited_rooms_mask", (double)s_game.visited_rooms_mask));
    s_game.side_room_visits = count_room_bits(s_game.visited_rooms_mask);
    s_game.layout_shift_count = (int)fmaxf(0.0F, (float)json_number(params, "layout_shift_count", (double)s_game.layout_shift_count));
    s_game.maze_zone = maze_zone_for_pos(s_game.x, s_game.z);
    s_game.blackout_timer = fmaxf(0.0F, (float)json_number(params, "blackout_timer", (double)s_game.blackout_timer));
    s_game.ambush_timer = fmaxf(0.0F, (float)json_number(params, "ambush_timer", (double)s_game.ambush_timer));
    s_game.relief_timer = fmaxf(0.0F, (float)json_number(params, "relief_timer", (double)s_game.relief_timer));
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
    nt_devapi_register("game.action.place_mark", ep_game_action_place_mark, NULL);
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
    (void)nt_devapi_register_ui_node("root", "", "screen", "Backrooms Liminal", "Cross three rooms and find the exit.", 0.0F, 0.0F, (float)UI_W, (float)UI_H, true, true);
    const char *objective_label = s_game.caught ? "Caught"
                                                : (s_game.won              ? "Escaped"
                                                   : (!s_game.mark_placed  ? "Draw a wall mark"
                                                      : (!s_game.door_handle_collected ? "Find the handle"
                                                         : (!s_game.portal_exit_revealed ? "Fit handle to locked door" : "Enter the real exit"))));
    const bool use_active = can_use_context();
    (void)nt_devapi_register_ui_node("backrooms.objective", "root", "label", "Objective", objective_label, 18.0F, 18.0F, 462.0F, 118.0F, true, true);
    (void)nt_devapi_register_ui_node("backrooms.fear", "root", "meter", "Hidden director pressure", "Internal only", 682.0F, 18.0F, 258.0F, 52.0F, false, false);
    (void)nt_devapi_register_ui_node("backrooms.battery", "root", "meter", "Dynamo", "Flashlight dynamo charge", 682.0F, 70.0F, 258.0F, 66.0F, false, false);
    (void)nt_devapi_register_ui_node("backrooms.threat", "root", "label", "Rooms", s_game.portal_exit_revealed ? "Exit is real" : "Rooms copy evidence", 682.0F, 118.0F, 258.0F, 28.0F, false, false);
    const char *use_text = near_mark_surface()      ? "Press E to draw mark"
                           : (near_door_handle()    ? "Press E to take handle"
                              : (near_locked_door() ? (s_game.portal_exit_revealed ? "Press E to enter exit"
                                                                                   : (s_game.door_handle_collected ? "Press E to fit handle" : "Press E to try door"))
                                                    : (near_exit() ? "Press E to escape" : "")));
    (void)nt_devapi_register_ui_node("backrooms.use_prompt", "root", "prompt", "Use", use_text, 322.0F, 392.0F, 316.0F, 46.0F, use_active, use_active);
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
