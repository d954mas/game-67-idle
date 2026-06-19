#include "app/nt_app.h"
#include "backrooms_portal_scene.h"
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
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BACKROOMS_DEVAPI_PORT_DEFAULT 9123
#define UI_W 960
#define UI_H 540
#define WALL_TEX_W 256
#define WALL_TEX_H 256
#define BACKROOMS_PORTAL_MATERIAL_ATLAS_PATH "assets/backrooms-liminal/materials/portal_material_atlas.ppm"

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
static nt_shader_t s_portal_overlay_vs;
static nt_shader_t s_portal_overlay_fs;
static nt_pipeline_t s_pipeline;
static nt_pipeline_t s_portal_solid_pipeline;
static nt_pipeline_t s_portal_overlay_pipeline;
static nt_buffer_t s_quad_vbo;
static nt_buffer_t s_portal_overlay_vbo;
static nt_texture_t s_wall_tex;
static nt_texture_t s_ui_tex;
static BackroomsPortalScene s_portal_scene;
static uint8_t s_wall_pixels[WALL_TEX_W * WALL_TEX_H * 4];
static uint8_t s_ui_pixels[UI_W * UI_H * 4];
static uint32_t s_last_portal_overlay_vertices;
static uint32_t s_last_portal_room_mesh_vertices;
static uint32_t s_last_portal_shell_vertices;
static uint32_t s_last_portal_blended_vertices;
static bool s_material_atlas_loaded_from_asset;
static const float s_route_choice_z[] = {24.0F, 16.2F, 8.3F};
static const int s_route_choice_safe_side[] = {1, -1, 1};

typedef struct PortalOverlayVertex {
    float x;
    float y;
    float z;
    float r;
    float g;
    float b;
    float a;
    float u;
    float v;
    float kind;
} PortalOverlayVertex;

#define PORTAL_OVERLAY_MAX_VERTICES 1536

static PortalOverlayVertex s_portal_overlay_vertices[PORTAL_OVERLAY_MAX_VERTICES];
static float s_portal_overlay_emit_kind;

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

static const char *s_portal_overlay_vs_src =
    "precision mediump float;\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec4 a_color;\n"
    "layout(location = 2) in vec2 a_uv;\n"
    "layout(location = 3) in float a_kind;\n"
    "out vec4 v_color;\n"
    "out vec2 v_uv;\n"
    "out vec3 v_world;\n"
    "out float v_kind;\n"
    "uniform vec4 u_overlay_resolution;\n"
    "uniform vec4 u_overlay_player;\n"
    "void main() {\n"
    "    float yaw = u_overlay_player.z;\n"
    "    vec2 fwd = vec2(sin(yaw), cos(yaw));\n"
    "    vec2 right = vec2(cos(yaw), -sin(yaw));\n"
    "    vec3 ro = vec3(u_overlay_player.x, 1.05 + 0.025 * sin(u_overlay_resolution.z * 7.0 + u_overlay_player.y), u_overlay_player.y);\n"
    "    vec3 rel = a_position - ro;\n"
    "    float cam_x = dot(rel.xz, right);\n"
    "    float cam_z = dot(rel.xz, fwd);\n"
    "    float cam_y = rel.y;\n"
    "    float front = step(0.06, cam_z) * u_overlay_resolution.w;\n"
    "    float aspect = max(0.5, u_overlay_resolution.x / max(1.0, u_overlay_resolution.y));\n"
    "    vec2 ndc = vec2((cam_x / max(0.06, cam_z)) / (0.78 * aspect), (cam_y / max(0.06, cam_z) + 0.03) / 0.86);\n"
    "    ndc = mix(vec2(2.5, 2.5), ndc, front);\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    v_color = a_color * front;\n"
    "    v_uv = a_uv;\n"
    "    v_world = a_position;\n"
    "    v_kind = a_kind;\n"
    "}\n";

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverlength-strings"
#endif
static const char *s_portal_overlay_fs_src =
    "precision mediump float;\n"
    "in vec4 v_color;\n"
    "in vec2 v_uv;\n"
    "in vec3 v_world;\n"
    "in float v_kind;\n"
    "out vec4 frag_color;\n"
    "uniform sampler2D u_overlay_tex;\n"
    "uniform vec4 u_overlay_portal;\n"
    "void main() {\n"
    "    if (v_color.a <= 0.01) { discard; }\n"
    "    float edge = min(min(v_uv.x, 1.0 - v_uv.x), min(v_uv.y, 1.0 - v_uv.y));\n"
    "    float edge_fade = mix(0.76, 1.0, smoothstep(0.0, 0.16, edge));\n"
    "    float grain = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);\n"
    "    vec3 tex = texture(u_overlay_tex, v_uv * vec2(1.8, 1.35)).rgb;\n"
    "    float depth = clamp((v_world.x - u_overlay_portal.x) / 4.2, 0.0, 1.0);\n"
    "    float side_shadow = smoothstep(u_overlay_portal.z * 0.48, u_overlay_portal.z * 0.98, abs(v_world.z - u_overlay_portal.y));\n"
    "    float lamp = exp(-abs(v_world.z - u_overlay_portal.y) * 2.8) * exp(-abs(v_world.y - 0.34) * 0.95) * u_overlay_portal.w;\n"
    "    float ceiling_spill = exp(-abs(v_world.z - u_overlay_portal.y) * 1.42) * exp(-abs(v_world.y - 1.76) * 1.18) * u_overlay_portal.w;\n"
    "    float floor_spill = exp(-abs(v_world.z - u_overlay_portal.y) * 1.12) * exp(-abs(v_world.y - 0.34) * 1.55) * smoothstep(0.08, 1.0, depth) * u_overlay_portal.w;\n"
    "    float return_contact = smoothstep(u_overlay_portal.z * 0.84, u_overlay_portal.z * 1.04, abs(v_world.z - u_overlay_portal.y));\n"
    "    float threshold_wash = (1.0 - smoothstep(0.02, 0.44, depth)) * (1.0 - smoothstep(u_overlay_portal.z * 0.55, u_overlay_portal.z * 1.04, abs(v_world.z - u_overlay_portal.y)));\n"
    "    float center_beam = exp(-abs(v_world.z - u_overlay_portal.y) * 1.08) * smoothstep(0.02, 0.30, depth) * (1.0 - smoothstep(0.78, 1.0, depth));\n"
    "    float wall_wash = (1.0 - smoothstep(u_overlay_portal.z * 0.42, u_overlay_portal.z * 0.98, abs(v_world.z - u_overlay_portal.y))) * smoothstep(0.34, 1.72, v_world.y) * (1.0 - smoothstep(2.12, 2.72, v_world.y));\n"
    "    float surface_kind = step(0.5, v_kind) * (1.0 - step(1.5, v_kind));\n"
    "    float seam_kind = step(1.5, v_kind) * (1.0 - step(2.5, v_kind));\n"
    "    float light_kind = step(2.5, v_kind) * (1.0 - step(3.5, v_kind));\n"
    "    float occluder_kind = step(3.5, v_kind) * (1.0 - step(4.5, v_kind));\n"
    "    float shell_kind = step(4.5, v_kind);\n"
    "    float solid_light_kind = step(5.5, v_kind) * (1.0 - step(6.5, v_kind));\n"
    "    float construction_kind = step(6.5, v_kind);\n"
    "    float floor_pick = shell_kind * (1.0 - smoothstep(0.36, 0.48, v_world.y));\n"
    "    float ceiling_pick = shell_kind * smoothstep(1.70, 1.94, v_world.y);\n"
    "    float trim_pick = shell_kind * max(1.0 - smoothstep(0.015, 0.055, abs(v_uv.y - 0.5)), step(6.5, v_kind));\n"
    "    vec2 mat_uv = fract(v_uv * vec2(2.15, 1.75));\n"
    "    vec2 wall_uv = mat_uv * 0.46 + vec2(0.02, 0.02);\n"
    "    vec2 floor_uv = mat_uv * 0.46 + vec2(0.52, 0.02);\n"
    "    vec2 ceil_uv = mat_uv * 0.46 + vec2(0.02, 0.52);\n"
    "    vec2 trim_uv = mat_uv * 0.46 + vec2(0.52, 0.52);\n"
    "    vec3 wall_tex = texture(u_overlay_tex, wall_uv).rgb;\n"
    "    vec3 floor_tex = texture(u_overlay_tex, floor_uv).rgb;\n"
    "    vec3 ceil_tex = texture(u_overlay_tex, ceil_uv).rgb;\n"
    "    vec3 trim_tex = texture(u_overlay_tex, trim_uv).rgb;\n"
    "    vec3 material_tex = mix(wall_tex, floor_tex, floor_pick);\n"
    "    material_tex = mix(material_tex, ceil_tex, ceiling_pick * (1.0 - floor_pick));\n"
    "    material_tex = mix(material_tex, trim_tex, trim_pick);\n"
    "    vec3 color = v_color.rgb * (0.98 + material_tex * 1.72 + grain * 0.055);\n"
    "    color *= mix(1.0, 0.88, side_shadow * (surface_kind + shell_kind * 0.30 * (1.0 - solid_light_kind * 0.82)));\n"
    "    color *= mix(1.0, 0.94, depth * (surface_kind + shell_kind * 0.32 * (1.0 - solid_light_kind * 0.78)));\n"
    "    color += vec3(0.74, 0.56, 0.23) * lamp * (0.34 + light_kind * 0.48);\n"
    "    color += vec3(1.08, 0.88, 0.40) * ceiling_spill * shell_kind * (0.36 + light_kind * 0.18);\n"
    "    color += vec3(0.66, 0.45, 0.18) * floor_spill * shell_kind * 0.48;\n"
    "    color += vec3(0.66, 0.50, 0.20) * threshold_wash * shell_kind * (0.42 + u_overlay_portal.w * 0.30);\n"
    "    color += vec3(0.48, 0.36, 0.15) * center_beam * shell_kind * (0.62 + u_overlay_portal.w * 0.38);\n"
    "    color += vec3(0.38, 0.28, 0.105) * wall_wash * construction_kind * (0.52 + u_overlay_portal.w * 0.34);\n"
    "    color = mix(color, color * vec3(0.34, 0.31, 0.24), seam_kind * 0.72);\n"
    "    color = mix(color, vec3(0.030, 0.025, 0.015), occluder_kind * (0.48 + side_shadow * 0.16));\n"
    "    color = mix(color, color * vec3(0.96, 0.91, 0.72) + material_tex * 0.34, shell_kind * 0.78);\n"
    "    color = mix(color, color * vec3(0.40, 0.35, 0.23), return_contact * shell_kind * (0.24 - construction_kind * 0.10));\n"
    "    color += vec3(0.108, 0.084, 0.038) * shell_kind;\n"
    "    color += vec3(0.210, 0.156, 0.062) * construction_kind * (0.52 + u_overlay_portal.w * 0.30);\n"
    "    color += vec3(0.78, 0.61, 0.26) * light_kind * (0.24 + u_overlay_portal.w * 0.22);\n"
    "    color += vec3(1.18, 0.90, 0.36) * solid_light_kind * (0.42 + u_overlay_portal.w * 0.34);\n"
    "    color *= 1.34 + shell_kind * 0.42 + light_kind * 0.22 + solid_light_kind * 0.34 + construction_kind * 0.34;\n"
    "    float alpha_boost = 1.0 + surface_kind * 0.58 + seam_kind * 0.20 + light_kind * 0.22 + occluder_kind * 0.34 + shell_kind * 1.30 + solid_light_kind * 0.48;\n"
    "    float material_floor = surface_kind * 0.42 + seam_kind * 0.30 + occluder_kind * 0.22 + shell_kind * 0.98 + solid_light_kind * 0.24;\n"
    "    float alpha = max(v_color.a * alpha_boost, material_floor) * edge_fade * (0.94 + grain * 0.06);\n"
    "    frag_color = vec4(color, min(0.985, alpha));\n"
    "}\n";

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
    "uniform vec4 u_portal_entry;\n"
    "uniform vec4 u_portal_shape;\n"
    "uniform vec4 u_portal_style;\n"
    "uniform vec4 u_portal_bounds;\n"
    "uniform vec4 u_portal_material;\n"
    "uniform vec4 u_portal_light;\n"
    "uniform vec4 u_portal_finish;\n"
    "uniform vec4 u_portal_construction;\n"
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
    "    float portal_z = entry.z - u_portal_entry.y;\n"
    "    vec3 ro2 = vec3(0.18, clamp(entry.y, u_portal_bounds.x, u_portal_bounds.y - 0.04), portal_z * u_portal_style.x);\n"
    "    vec3 rd2 = normalize(vec3(0.82 + abs(rd.x) * 0.34, rd.y * 0.70, rd.z * 2.75 + portal_z * 0.035));\n"
    "    float best2 = 1e20;\n"
    "    int smat = 0;\n"
    "    vec3 hp = vec3(0.0);\n"
    "    vec3 n2 = vec3(-1.0, 0.0, 0.0);\n"
    "    float tx = (u_portal_shape.x - ro2.x) / rd2.x;\n"
    "    vec3 px = ro2 + rd2 * tx;\n"
    "    if (tx > 0.0 && px.y > 0.0 && px.y < u_portal_shape.z && abs(px.z) < u_portal_shape.y) { best2 = tx; smat = 1; hp = px; n2 = vec3(-1.0, 0.0, 0.0); }\n"
    "    if (abs(rd2.y) > 0.001) {\n"
    "        float ty = (0.0 - ro2.y) / rd2.y;\n"
    "        vec3 py = ro2 + rd2 * ty;\n"
    "        if (ty > 0.0 && ty < best2 && py.x > 0.0 && py.x < u_portal_shape.x + 0.4 && abs(py.z) < u_portal_shape.y) { best2 = ty; smat = 2; hp = py; n2 = vec3(0.0, 1.0, 0.0); }\n"
    "        ty = (u_portal_shape.z - ro2.y) / rd2.y;\n"
    "        py = ro2 + rd2 * ty;\n"
    "        if (ty > 0.0 && ty < best2 && py.x > 0.0 && py.x < u_portal_shape.x + 0.4 && abs(py.z) < u_portal_shape.y) { best2 = ty; smat = 3; hp = py; n2 = vec3(0.0, -1.0, 0.0); }\n"
    "    }\n"
    "    if (abs(rd2.z) > 0.001) {\n"
    "        float tz = (u_portal_shape.y * sign(rd2.z) - ro2.z) / rd2.z;\n"
    "        vec3 pz = ro2 + rd2 * tz;\n"
    "        if (tz > 0.0 && tz < best2 && pz.x > 0.0 && pz.x < u_portal_shape.x + 0.4 && pz.y > 0.0 && pz.y < u_portal_shape.z) { best2 = tz; smat = 4; hp = pz; n2 = vec3(0.0, 0.0, -sign(rd2.z)); }\n"
    "    }\n"
    "    vec3 paper = texture(u_wall_tex, fract(vec2(hp.x * 0.14 + hp.z * 0.025, hp.y * 0.52)) * 0.46 + vec2(0.02, 0.02)).rgb;\n"
    "    vec3 carpet = texture(u_wall_tex, fract(hp.xz * 0.11) * 0.46 + vec2(0.52, 0.02)).rgb;\n"
    "    vec3 ceiling = texture(u_wall_tex, fract(hp.xz * 0.16) * 0.46 + vec2(0.02, 0.52)).rgb;\n"
    "    vec3 trim = texture(u_wall_tex, fract(vec2(hp.x * 0.20, hp.y * 0.62)) * 0.46 + vec2(0.52, 0.52)).rgb;\n"
    "    vec3 col = paper * vec3(0.96, 0.90, 0.66);\n"
    "    if (smat == 2) { col = carpet * vec3(0.76, 0.68, 0.52); }\n"
    "    if (smat == 3) { col = ceiling * vec3(0.82, 0.80, 0.68); }\n"
    "    if (smat == 4) { col = trim * vec3(0.70, 0.62, 0.45); }\n"
    "    float panel_x = 1.0 - smoothstep(0.020, 0.070, abs(fract(hp.x * max(0.10, u_portal_material.x + 0.22)) - 0.5));\n"
    "    float panel_z = 1.0 - smoothstep(0.024, 0.080, abs(fract((hp.z + u_portal_shape.y) * max(0.10, u_portal_material.x)) - 0.5));\n"
    "    float wall_panel = (smat == 1 || smat == 4 ? 1.0 : 0.0) * max(panel_x * 0.55, panel_z);\n"
    "    float wall_batten = (smat == 1 || smat == 4 ? 1.0 : 0.0) * (1.0 - smoothstep(0.025, 0.090, abs(fract((hp.x + 0.25) * 0.46) - 0.5))) * smoothstep(0.34, 0.58, hp.y) * (1.0 - smoothstep(u_portal_shape.z - 0.44, u_portal_shape.z - 0.18, hp.y));\n"
    "    float carpet_tile_x = (smat == 2 ? 1.0 : 0.0) * (1.0 - smoothstep(0.018, 0.070, abs(fract(hp.x * max(0.10, u_portal_material.y + 0.10)) - 0.5)));\n"
    "    float carpet_tile_z = (smat == 2 ? 1.0 : 0.0) * (1.0 - smoothstep(0.018, 0.070, abs(fract((hp.z + u_portal_shape.y) * max(0.10, u_portal_material.y)) - 0.5)));\n"
    "    float baseboard = (smat == 1 || smat == 4 ? 1.0 : 0.0) * smoothstep(0.08, 0.16, hp.y) * (1.0 - smoothstep(0.22, 0.34, hp.y));\n"
    "    float stain = smoothstep(0.66, 0.96, hash12(floor(hp.xz * vec2(1.4, 2.0)))) * (1.0 - smoothstep(0.35, 2.30, hp.y)) * u_portal_material.z;\n"
    "    float corner_shadow = max(smoothstep(u_portal_shape.y * 0.68, u_portal_shape.y, abs(hp.z)), smoothstep(u_portal_shape.x * 0.72, u_portal_shape.x, hp.x)) * u_portal_light.z;\n"
    "    float floor_wet = (smat == 2 ? 1.0 : 0.0) * smoothstep(2.0, 9.4, hp.x) * (0.45 + 0.55 * hash12(floor(hp.xz * 1.7))) * u_portal_material.w;\n"
    "    float fixture_spacing = max(1.8, u_portal_finish.y);\n"
    "    float fixture_center = floor((hp.x + fixture_spacing * 0.50) / fixture_spacing) * fixture_spacing;\n"
    "    float fixture_local = hp.x - fixture_center;\n"
    "    float fixture_pulse = 0.72 + 0.28 * step(0.22, sin(fixture_center * 3.1 + ttime * 2.7));\n"
    "    vec3 lamp_pos = vec3(fixture_center, u_portal_shape.z - 0.13, 0.0);\n"
    "    vec3 lamp_vec = lamp_pos - hp;\n"
    "    float lamp_dist = length(lamp_vec);\n"
    "    vec3 lamp_dir = lamp_vec / max(0.001, lamp_dist);\n"
    "    float n_dot_l = max(0.0, dot(n2, lamp_dir));\n"
    "    float lamp_reach = 1.0 / (1.0 + lamp_dist * lamp_dist * 0.46);\n"
    "    float wall_side_occ = smoothstep(u_portal_shape.y * 0.52, u_portal_shape.y, abs(hp.z));\n"
    "    float back_occ = smoothstep(u_portal_shape.x * 0.50, u_portal_shape.x, hp.x);\n"
    "    float floor_contact_occ = smoothstep(u_portal_shape.y * 0.62, u_portal_shape.y, abs(hp.z)) * smoothstep(0.0, 0.32, hp.y + 0.05);\n"
    "    float fixture_cast_shadow = (smat == 1 || smat == 2 || smat == 4 ? 1.0 : 0.0) * (1.0 - smoothstep(0.08, 0.44, abs(hp.z))) * (1.0 - smoothstep(0.42, 1.25, abs(fixture_local))) * smoothstep(0.48, 2.20, hp.x);\n"
    "    float wet_spec = (smat == 2 ? 1.0 : 0.0) * pow(max(0.0, dot(reflect(-lamp_dir, n2), -rd2)), 18.0) * floor_wet;\n"
    "    float fixture_lens = (smat == 3 ? 1.0 : 0.0) * (1.0 - smoothstep(u_portal_light.x * 0.35, u_portal_light.x, abs(hp.z))) * (1.0 - smoothstep(0.46, 0.62, abs(fixture_local)));\n"
    "    float fixture_housing = (smat == 3 ? 1.0 : 0.0) * max(1.0 - smoothstep(0.028, 0.075, abs(abs(hp.z) - u_portal_light.x * 1.22)), 1.0 - smoothstep(0.030, 0.082, abs(abs(fixture_local) - 0.58)));\n"
    "    fixture_housing *= (1.0 - smoothstep(0.74, 0.94, abs(fixture_local))) * (1.0 - smoothstep(u_portal_light.x * 1.6, u_portal_light.x * 2.1, abs(hp.z)));\n"
    "    float ceiling_panel = (smat == 3 ? 1.0 : 0.0) * max(1.0 - smoothstep(0.014, 0.060, abs(fract((hp.x + 0.33) * max(0.18, u_portal_finish.z)) - 0.5)), 1.0 - smoothstep(0.014, 0.060, abs(fract((hp.z + u_portal_shape.y) * max(0.18, u_portal_finish.z * 0.72)) - 0.5)));\n"
    "    float ceiling_strip = fixture_lens * (0.66 + 0.34 * fixture_pulse);\n"
    "    float wall_light_spill = (smat == 1 || smat == 4 ? 1.0 : 0.0) * (1.0 - smoothstep(0.34, 1.72, abs(hp.z))) * smoothstep(1.18, 2.38, hp.y) * (0.48 + 0.52 * fixture_pulse) * u_portal_finish.w;\n"
    "    float side_wall_bounce = (smat == 4 ? 1.0 : 0.0) * (1.0 - smoothstep(0.16, 0.95, abs(fixture_local))) * smoothstep(0.44, 1.34, hp.y) * (1.0 - smoothstep(2.24, 2.82, hp.y)) * u_portal_finish.w;\n"
    "    float cove_trim = (smat == 1 || smat == 4 ? 1.0 : 0.0) * max(1.0 - smoothstep(0.030, 0.095, abs(hp.y - (u_portal_shape.z - 0.16))), 1.0 - smoothstep(0.026, 0.080, abs(hp.y - 0.42)));\n"
    "    cove_trim *= u_portal_finish.x;\n"
    "    float floor_light_pool = (smat == 2 ? 1.0 : 0.0) * (1.0 - smoothstep(0.42, 1.70, abs(hp.z))) * (1.0 - smoothstep(0.62, 1.95, abs(fixture_local))) * u_portal_finish.w;\n"
    "    float floor_long_pool = (smat == 2 ? 1.0 : 0.0) * exp(-abs(hp.z) * 0.62) * smoothstep(0.48, 2.10, hp.x) * (1.0 - smoothstep(u_portal_shape.x - 1.15, u_portal_shape.x + 0.10, hp.x)) * u_portal_light.y;\n"
    "    float floor_wall_shadow = (smat == 2 ? 1.0 : 0.0) * smoothstep(u_portal_shape.y * 0.58, u_portal_shape.y, abs(hp.z)) * u_portal_light.z;\n"
    "    float front_return = (smat == 4 ? 1.0 : 0.0) * (1.0 - smoothstep(0.18, 0.82, hp.x)) * smoothstep(0.28, 0.54, hp.y) * (1.0 - smoothstep(u_portal_shape.z - 0.64, u_portal_shape.z - 0.28, hp.y)) * u_portal_construction.x;\n"
    "    float threshold_lip = (smat == 2 ? 1.0 : 0.0) * (1.0 - smoothstep(0.28, 0.82, hp.x)) * (1.0 - smoothstep(u_portal_shape.y * 0.62, u_portal_shape.y * 0.96, abs(hp.z))) * u_portal_construction.y;\n"
    "    float ceiling_conduit = (smat == 3 ? 1.0 : 0.0) * max(1.0 - smoothstep(0.018, 0.058, abs(abs(hp.z) - max(0.38, u_portal_light.x * 2.8))), 1.0 - smoothstep(0.020, 0.070, abs(hp.z))) * u_portal_construction.z;\n"
    "    ceiling_conduit *= smoothstep(0.34, 0.74, hp.x) * (1.0 - smoothstep(u_portal_shape.x - 0.80, u_portal_shape.x - 0.28, hp.x));\n"
    "    float landmark_column = (smat == 1 ? 1.0 : 0.0) * max(1.0 - smoothstep(0.055, 0.16, abs(abs(hp.z) - u_portal_shape.y * 0.44)), 1.0 - smoothstep(0.045, 0.14, abs(hp.z))) * smoothstep(0.36, 0.60, hp.y) * (1.0 - smoothstep(u_portal_shape.z - 0.42, u_portal_shape.z - 0.20, hp.y)) * u_portal_construction.w;\n"
    "    float nested_body = (smat == 1 ? 1.0 : 0.0) * (1.0 - smoothstep(1.12, 1.58, abs(hp.z))) * smoothstep(0.44, 0.62, hp.y) * (1.0 - smoothstep(1.82, 2.08, hp.y));\n"
    "    float nested_half = u_portal_style.y;\n"
    "    float nested_frame = (smat == 1 ? 1.0 : 0.0) * max(1.0 - smoothstep(0.035, 0.12, abs(abs(hp.z) - nested_half)), max(1.0 - smoothstep(0.035, 0.12, abs(hp.y - 0.50)), 1.0 - smoothstep(0.035, 0.12, abs(hp.y - 1.98))));\n"
    "    nested_frame *= u_portal_shape.w * (1.0 - smoothstep(nested_half + 0.18, nested_half + 0.50, abs(hp.z))) * smoothstep(0.40, 0.56, hp.y) * (1.0 - smoothstep(1.98, 2.18, hp.y));\n"
    "    vec2 nuv = vec2(hp.z / max(0.4, nested_half + 0.03), (hp.y - 1.22) / 0.82);\n"
    "    float nwall = smoothstep(0.48, 0.98, abs(nuv.x));\n"
    "    float nfloor = smoothstep(-0.10, -0.72, nuv.y);\n"
    "    float nceil = smoothstep(0.32, 0.92, nuv.y);\n"
    "    float nlight = (1.0 - smoothstep(0.04, 0.18, abs(nuv.x))) * smoothstep(0.30, 0.72, nuv.y);\n"
    "    float ndepth = 1.0 / (0.50 + abs(nuv.x) * 0.72 + abs(nuv.y + 0.08) * 0.45);\n"
    "    vec3 nested_col = mix(vec3(0.18, 0.14, 0.075), vec3(0.62, 0.56, 0.30), ndepth);\n"
    "    nested_col = mix(nested_col, vec3(0.030, 0.026, 0.016), max(nwall, max(nfloor, nceil)) * (0.46 + 0.20 * u_portal_light.z));\n"
    "    nested_col += vec3(1.05, 0.90, 0.46) * nlight * (0.68 + 0.18 * u_portal_light.y);\n"
    "    float copied_mark = (smat == 1 ? 1.0 : 0.0) * (1.0 - smoothstep(0.075, 0.16, abs((hp.y - 1.14) - hp.z * 0.18)));\n"
    "    copied_mark *= (1.0 - smoothstep(0.075, 0.16, abs((hp.y - 1.14) + hp.z * 0.18))) * smoothstep(0.42, 0.66, hp.y) * (1.0 - smoothstep(1.58, 1.84, hp.y));\n"
    "    float copied_mark_small = nested_body * (1.0 - smoothstep(0.040, 0.095, abs((nuv.y + 0.18) - nuv.x * 0.34))) * (1.0 - smoothstep(0.040, 0.095, abs((nuv.y + 0.18) + nuv.x * 0.34)));\n"
    "    float threshold_light = exp(-ro2.x * 0.34) * (0.36 + 0.32 * smoothstep(u_portal_entry.z * 0.80, 0.0, abs(portal_z)));\n"
    "    float direct_light = (0.26 + n_dot_l * 1.34) * lamp_reach * u_portal_light.y * fixture_pulse;\n"
    "    float center_volume = exp(-abs(hp.z) * 0.56) * smoothstep(0.24, 1.64, hp.x) * (1.0 - smoothstep(u_portal_shape.x - 0.90, u_portal_shape.x + 0.18, hp.x));\n"
    "    float bounce_light = 0.20 + threshold_light + center_volume * 0.22 + ceiling_strip * u_portal_light.y * 0.68 + wall_light_spill * 0.48 + side_wall_bounce * 0.34 + floor_light_pool * 0.34 + floor_long_pool * 0.10 + u_portal_style.z / (1.0 + length(hp - vec3(u_portal_shape.x * 0.49, u_portal_shape.z - 0.29, 0.0)) * 0.96);\n"
    "    float light = bounce_light + direct_light;\n"
    "    col *= light;\n"
    "    col = mix(col, col * vec3(0.46, 0.40, 0.28), stain * 0.64);\n"
    "    col = mix(col, col * vec3(0.24, 0.22, 0.17), corner_shadow * 0.58);\n"
    "    col = mix(col, col * vec3(0.18, 0.17, 0.13), max(wall_side_occ, back_occ) * 0.30);\n"
    "    col = mix(col, col * vec3(0.12, 0.11, 0.085), floor_contact_occ * 0.34);\n"
    "    col = mix(col, col * 0.36, fixture_cast_shadow * 0.28);\n"
    "    col = mix(col, col * 0.48, wall_panel * 0.32);\n"
    "    col = mix(col, vec3(0.18, 0.12, 0.050), wall_batten * 0.56);\n"
    "    col = mix(col, vec3(0.13, 0.09, 0.045), baseboard * u_portal_light.w);\n"
    "    col = mix(col, vec3(0.10, 0.075, 0.038), cove_trim * 0.70);\n"
    "    col = mix(col, col * 0.56, ceiling_panel * 0.26);\n"
    "    col = mix(col, vec3(0.030, 0.026, 0.018), fixture_housing * 0.86);\n"
    "    col = mix(col, col * 0.62, max(carpet_tile_x, carpet_tile_z) * 0.40);\n"
    "    col = mix(col, col * 0.50, floor_wall_shadow * 0.46);\n"
    "    col = mix(col, vec3(0.095, 0.065, 0.030), front_return * 0.66);\n"
    "    col = mix(col, vec3(0.18, 0.12, 0.060), threshold_lip * 0.56);\n"
    "    col = mix(col, vec3(0.035, 0.031, 0.022), ceiling_conduit * 0.76);\n"
    "    col = mix(col, vec3(0.11, 0.075, 0.036), landmark_column * 0.68);\n"
    "    col += vec3(0.20, 0.15, 0.07) * floor_wet;\n"
    "    col += vec3(0.80, 0.66, 0.32) * wet_spec * 0.42;\n"
    "    col += vec3(1.00, 0.88, 0.46) * ceiling_strip * (0.42 + 0.12 * u_portal_light.y);\n"
    "    col += vec3(0.76, 0.58, 0.25) * wall_light_spill * 0.30;\n"
    "    col += vec3(0.52, 0.39, 0.16) * side_wall_bounce * 0.26;\n"
    "    col += vec3(0.36, 0.23, 0.09) * floor_light_pool * 0.34;\n"
    "    col += vec3(0.46, 0.31, 0.12) * floor_long_pool * 0.20;\n"
    "    col += vec3(0.34, 0.24, 0.10) * threshold_lip * 0.22;\n"
    "    col += vec3(0.72, 0.55, 0.24) * landmark_column * 0.12;\n"
    "    col = mix(col, nested_col, nested_body * 0.92);\n"
    "    col = mix(col, vec3(0.016, 0.012, 0.006), nested_frame * 0.84);\n"
    "    col += vec3(0.95, 0.68, 0.30) * nested_frame * 0.28;\n"
    "    col += vec3(1.0, 0.04, 0.0) * copied_mark * 0.92;\n"
    "    col += vec3(1.0, 0.02, 0.0) * copied_mark_small * 0.70;\n"
    "    float fog2 = smoothstep(4.2, 12.4, best2);\n"
    "    return mix(col, vec3(0.048, 0.044, 0.027), fog2 * 0.38);\n"
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
    "    if (mat == 1) { albedo = texture(u_wall_tex, fract(hit.xz * 0.19) * 0.46 + vec2(0.52, 0.02)).rgb * vec3(0.72, 0.64, 0.48); }\n"
    "    if (mat == 2) { albedo = texture(u_wall_tex, fract(hit.xz * 0.21) * 0.46 + vec2(0.02, 0.52)).rgb * vec3(0.78, 0.76, 0.64); tuv = hit.xz * 0.42; }\n"
    "    if (mat == 3) { albedo = texture(u_wall_tex, fract(vec2(hit.z * 0.18, hit.y * 0.55)) * 0.46 + vec2(0.02, 0.02)).rgb * vec3(0.98, 0.92, 0.68); }\n"
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
    "    float impossible_rim = 0.0;\n"
    "    float impossible_occlusion = 0.0;\n"
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
    "        float cut_band = room_band(hit.z, u_portal_entry.y, u_portal_entry.z) * smoothstep(u_portal_bounds.z, u_portal_bounds.w, hit.x);\n"
    "        float cut_y = smoothstep(u_portal_bounds.x - 0.08, u_portal_bounds.x + 0.08, hit.y) * (1.0 - smoothstep(u_portal_bounds.y - 0.08, u_portal_bounds.y + 0.14, hit.y));\n"
    "        impossible_cut = u_portal_entry.w * cut_band * cut_y;\n"
    "        float side_edge = abs(abs(hit.z - u_portal_entry.y) - max(0.0, u_portal_entry.z - 0.02));\n"
    "        float top_edge = abs(hit.y - u_portal_bounds.y);\n"
    "        float bottom_edge = abs(hit.y - u_portal_bounds.x);\n"
    "        float frame_z = 1.0 - smoothstep(0.024, 0.085, side_edge);\n"
    "        float frame_y = max(1.0 - smoothstep(0.026, 0.090, bottom_edge), 1.0 - smoothstep(0.026, 0.090, top_edge));\n"
    "        impossible_frame = u_portal_entry.w * cut_band * max(frame_z * cut_y, frame_y * room_band(hit.z, u_portal_entry.y, u_portal_entry.z + 0.10));\n"
    "        impossible_rim = u_portal_entry.w * cut_band * max(1.0 - smoothstep(0.0, 0.035, min(side_edge, min(top_edge, bottom_edge))), 0.0);\n"
    "        impossible_occlusion = u_portal_entry.w * cut_band * cut_y * (1.0 - smoothstep(0.0, 0.34, min(side_edge, min(top_edge, bottom_edge))));\n"
    "    }\n"
    "    float portal_near_z = u_portal_entry.w * (1.0 - smoothstep(u_portal_entry.z + 0.25, u_portal_entry.z + 1.55, abs(hit.z - u_portal_entry.y)));\n"
    "    float portal_near_x = smoothstep(u_portal_bounds.z - 1.15, u_portal_bounds.z + 0.15, hit.x) * (1.0 - smoothstep(u_portal_bounds.w + 0.35, u_portal_bounds.w + 1.35, hit.x));\n"
    "    float portal_area = portal_near_z * portal_near_x;\n"
    "    float portal_wall_bounce = portal_area * (mat == 3 ? 1.0 : 0.0) * smoothstep(0.26, 0.78, hit.y) * (1.0 - smoothstep(2.05, 2.42, hit.y));\n"
    "    float portal_floor_bounce = portal_area * (mat == 1 ? 1.0 : 0.0) * (1.0 - smoothstep(1.15, 3.70, abs(hit.x - u_portal_entry.x))) * (1.0 - smoothstep(0.15, 0.90, abs(hit.z - u_portal_entry.y)));\n"
    "    float fixture_shape = 0.0;\n"
    "    if (mat == 2) {\n"
    "        float fixture_w = mix(0.13, 0.34, room_mix);\n"
    "        fixture_shape = (1.0 - smoothstep(fixture_w, fixture_w + 0.07, abs(hit.x))) * (1.0 - smoothstep(0.72, 0.92, abs(hit.z - fixture_z)));\n"
    "    }\n"
    "\n"
    "    float wall_damp = (mat == 3 ? 1.0 : 0.0) * (1.0 - smoothstep(0.10, 1.85, hit.y)) * smoothstep(0.72, 0.96, hash12(floor(hit.zy * vec2(2.0, 4.0))));\n"
    "    float wall_panel_v = (mat == 3 ? 1.0 : 0.0) * (1.0 - smoothstep(0.020, 0.070, abs(fract((hit.z + 0.35) * 0.42) - 0.5))) * smoothstep(0.24, 0.46, hit.y) * (1.0 - smoothstep(2.02, 2.28, hit.y));\n"
    "    float wall_baseboard = (mat == 3 ? 1.0 : 0.0) * smoothstep(0.065, 0.13, hit.y) * (1.0 - smoothstep(0.20, 0.32, hit.y));\n"
    "    float carpet_wear = (mat == 1 ? 1.0 : 0.0) * smoothstep(0.54, 0.98, hash12(floor(hit.xz * 2.4))) * smoothstep(3.5, 18.0, hit.z);\n"
    "    float carpet_seam_x = (mat == 1 ? 1.0 : 0.0) * (1.0 - smoothstep(0.016, 0.060, abs(fract((hit.x + 0.12) * 0.58) - 0.5)));\n"
    "    float carpet_seam_z = (mat == 1 ? 1.0 : 0.0) * (1.0 - smoothstep(0.016, 0.060, abs(fract((hit.z + 0.28) * 0.40) - 0.5)));\n"
    "    float ceiling_panel = (mat == 2 ? 1.0 : 0.0) * max(1.0 - smoothstep(0.018, 0.075, abs(fract((hit.x + 0.20) * 0.46) - 0.5)), 1.0 - smoothstep(0.018, 0.075, abs(fract((hit.z + 0.40) * 0.37) - 0.5)));\n"
    "    float fixture_shadow = (mat == 2 ? 1.0 : 0.0) * (1.0 - smoothstep(0.18, 0.52, abs(hit.x))) * (1.0 - smoothstep(0.45, 1.10, abs(hit.z - fixture_z)));\n"
    "    float cut_screw_a = 1.0 - smoothstep(0.018, 0.052, length(vec2(abs(hit.z - u_portal_entry.y) - u_portal_entry.z * 0.87, hit.y - (u_portal_bounds.y - 0.18))));\n"
    "    float cut_screw_b = 1.0 - smoothstep(0.018, 0.052, length(vec2(abs(hit.z - u_portal_entry.y) - u_portal_entry.z * 0.87, hit.y - (u_portal_bounds.x + 0.18))));\n"
    "    float cut_screws = impossible_frame * max(cut_screw_a, cut_screw_b);\n"
    "    vec3 color = albedo * (0.13 + ceiling_light + exit_light + fuse_light + flashlight) * contact;\n"
    "    color = mix(color, color * vec3(0.48, 0.42, 0.30), wall_damp * 0.56);\n"
    "    color = mix(color, color * 0.58, wall_panel_v * 0.34);\n"
    "    color = mix(color, vec3(0.16, 0.10, 0.045), wall_baseboard * 0.74);\n"
    "    color = mix(color, color * vec3(0.56, 0.45, 0.32), carpet_wear * 0.42);\n"
    "    color = mix(color, color * 0.62, max(carpet_seam_x, carpet_seam_z) * 0.34);\n"
    "    color = mix(color, color * 0.64, ceiling_panel * 0.20);\n"
    "    color = mix(color, color * 0.38, fixture_shadow * 0.22);\n"
    "    color += vec3(1.0, 0.92, 0.66) * fixture_shape * (1.05 + 0.75 * flicker);\n"
    "    color += vec3(1.15, 0.04, 0.0) * fixture_shape * u_horror.x * (0.7 + 0.5 * step(0.45, sin(ttime * 15.0)));\n"
    "    color = mix(color, color * vec3(0.36, 0.32, 0.20), side_opening * 0.38);\n"
    "    color += vec3(0.16, 0.13, 0.055) * side_opening * 0.16;\n"
    "    color = mix(color, vec3(0.006, 0.006, 0.004), dead_end_shadow * 0.62);\n"
    "    color = mix(color, vec3(0.16, 0.0, 0.0), red_room * (0.52 + u_pressure.x * 0.25));\n"
    "    color = mix(color, vec3(0.018, 0.014, 0.010), exit_door * (0.82 + 0.15 * u_puzzle.w));\n"
    "    color = mix(color, vec3(0.04, 0.0, 0.0), exit_door * (1.0 - u_puzzle.z) * 0.35);\n"
    "    color += vec3(0.55, 0.16, 0.04) * exit_door * (1.0 - u_puzzle.z) * 0.34;\n"
    "    color += vec3(1.0, 0.58, 0.18) * exit_frame * (0.10 + 0.46 * u_puzzle.w);\n"
    "    color += vec3(0.20, 0.15, 0.055) * portal_wall_bounce * (0.60 + u_portal_light.y * 0.18);\n"
    "    color += vec3(0.18, 0.12, 0.050) * portal_floor_bounce * (0.45 + u_portal_light.y * 0.12);\n"
    "    color = mix(color, color * vec3(1.14, 1.10, 0.96), portal_wall_bounce * 0.34 + portal_floor_bounce * 0.24);\n"
    "    vec3 impossible_col = impossible_room_color(hit, rd, ttime);\n"
    "    vec3 portal_matte = mix(vec3(0.052, 0.043, 0.024), impossible_col * vec3(0.55, 0.46, 0.26), 0.50);\n"
    "    portal_matte += impossible_col * (0.060 + impossible_rim * 0.026 + impossible_frame * 0.014);\n"
    "    color = mix(color, portal_matte, impossible_cut);\n"
    "    color = mix(color, vec3(0.030, 0.023, 0.013), impossible_frame * 0.30);\n"
    "    color += vec3(0.70, 0.52, 0.22) * impossible_frame * 0.12;\n"
    "    color = mix(color, color * 0.58, impossible_frame * smoothstep(0.0, 1.0, abs(hit.z - u_portal_entry.y)) * 0.06);\n"
    "    color = mix(color, color * vec3(0.38, 0.32, 0.21), impossible_occlusion * 0.34);\n"
    "    color += vec3(0.92, 0.70, 0.30) * impossible_rim * 0.26;\n"
    "    color = mix(color, vec3(0.010, 0.007, 0.004), cut_screws * 0.92);\n"
    "    color += vec3(0.80, 0.55, 0.24) * cut_screws * 0.28;\n"
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
    "    float scene_luma = dot(color, vec3(0.2126, 0.7152, 0.0722));\n"
    "    color = max(mix(vec3(scene_luma), color, 1.09) * 1.035, vec3(0.0));\n"
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

static float clamp_absf(float v, float max_abs) { return clampf(v, -max_abs, max_abs); }

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

static float mouse_look_dx(void) {
    float dx = 0.0F;
    for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
        const nt_pointer_t *pointer = &g_nt_input.pointers[i];
        if (pointer->active && pointer->type == (uint8_t)NT_POINTER_MOUSE) {
            dx += pointer->dx;
        }
    }
    return clamp_absf(dx, 180.0F);
}

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
    backrooms_portal_scene_build_t0010(&s_portal_scene);
    s_game = (BackroomsState){
        .x = 0.0F,
        .z = 2.75F,
        .yaw = 0.0F,
        .fear = 10.0F,
        .battery = 1.0F,
        .flashlight_on = true,
    };
    set_message(backrooms_portal_scene_validate(&s_portal_scene) ? "MARK WALL. FIND HANDLE" : "PORTAL SCENE INVALID", 3.0F);
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
            const int tile_x = x & 127;
            const int tile_y = y & 127;
            const int zone = (x >= 128 ? 1 : 0) + (y >= 128 ? 2 : 0);
            const int hash = (tile_x * 37 + tile_y * 73 + ((tile_x * tile_y) % 97)) & 255;
            int r = 0;
            int g = 0;
            int b = 0;
            if (zone == 0) {
                const int seam = (tile_x % 32 == 0) || (tile_y % 46 == 0);
                const int fleck = ((tile_x * 17 + tile_y * 31 + ((tile_x * tile_y) % 19)) & 23) == 0;
                const int stain = (((tile_x * 3 + tile_y * 11) & 63) < 7 && ((tile_x + tile_y * 5) & 15) < 5);
                const int vertical_wear = (tile_x % 32 == 30 || tile_x % 32 == 1) && ((tile_y * 7 + tile_x) & 7) < 5;
                r = 168 + ((tile_x * 5 + tile_y * 3) & 23);
                g = 151 + ((tile_x * 7 + tile_y * 11) & 19);
                b = 78 + ((tile_x * 13 + tile_y * 2) & 15);
                if (seam) {
                    r -= 44;
                    g -= 40;
                    b -= 24;
                }
                if (fleck) {
                    r -= 54;
                    g -= 45;
                    b -= 25;
                }
                if (stain) {
                    r -= 42;
                    g -= 38;
                    b -= 18;
                }
                if (vertical_wear) {
                    r += 22;
                    g += 16;
                    b += 7;
                }
            } else if (zone == 1) {
                const int seam = (tile_x % 26 == 0) || (tile_y % 30 == 0);
                const int fiber = ((tile_x * 19 + tile_y * 5) & 15) < 4;
                const int damp = (((tile_x - 54) * (tile_x - 54) + (tile_y - 76) * (tile_y - 76)) < 680) || (hash > 236);
                r = 86 + ((tile_x * 3 + tile_y * 9) & 19);
                g = 69 + ((tile_x * 11 + tile_y * 5) & 15);
                b = 38 + ((tile_x * 7 + tile_y * 13) & 11);
                if (fiber) {
                    r += 18;
                    g += 13;
                    b += 5;
                }
                if (seam) {
                    r -= 34;
                    g -= 27;
                    b -= 16;
                }
                if (damp) {
                    r -= 31;
                    g -= 27;
                    b -= 14;
                }
            } else if (zone == 2) {
                const int grid = (tile_x % 38 < 2) || (tile_y % 34 < 2);
                const int speckle = hash > 224;
                const int water_ring = (((tile_x - 84) * (tile_x - 84) + (tile_y - 38) * (tile_y - 38)) > 360 &&
                                        ((tile_x - 84) * (tile_x - 84) + (tile_y - 38) * (tile_y - 38)) < 610);
                r = 156 + ((tile_x * 5 + tile_y * 2) & 17);
                g = 148 + ((tile_x * 2 + tile_y * 7) & 15);
                b = 102 + ((tile_x * 13 + tile_y * 3) & 13);
                if (grid) {
                    r -= 55;
                    g -= 51;
                    b -= 36;
                }
                if (speckle) {
                    r -= 39;
                    g -= 36;
                    b -= 26;
                }
                if (water_ring) {
                    r -= 30;
                    g -= 32;
                    b -= 18;
                }
            } else {
                const int rib = (tile_x % 18 < 3) || (tile_y % 42 < 3);
                const int scratch = ((tile_x * 29 + tile_y * 17) & 31) < 3;
                r = 116 + ((tile_x * 9 + tile_y * 4) & 15);
                g = 92 + ((tile_x * 4 + tile_y * 11) & 13);
                b = 46 + ((tile_x * 7 + tile_y * 2) & 9);
                if (rib) {
                    r -= 44;
                    g -= 34;
                    b -= 18;
                }
                if (scratch) {
                    r += 42;
                    g += 31;
                    b += 12;
                }
            }
            s_wall_pixels[i + 0] = (uint8_t)clampf((float)r, 0.0F, 255.0F);
            s_wall_pixels[i + 1] = (uint8_t)clampf((float)g, 0.0F, 255.0F);
            s_wall_pixels[i + 2] = (uint8_t)clampf((float)b, 0.0F, 255.0F);
            s_wall_pixels[i + 3] = 255;
        }
    }
}

static int ppm_next_byte(FILE *file) {
    int ch = fgetc(file);
    while (isspace(ch) || ch == '#') {
        if (ch == '#') {
            do {
                ch = fgetc(file);
            } while (ch != '\n' && ch != '\r' && ch != EOF);
        }
        ch = fgetc(file);
    }
    return ch;
}

static bool ppm_read_int(FILE *file, int *out_value) {
    int ch = ppm_next_byte(file);
    if (!isdigit(ch)) {
        return false;
    }
    int value = 0;
    while (isdigit(ch)) {
        value = value * 10 + (ch - '0');
        ch = fgetc(file);
    }
    *out_value = value;
    return true;
}

static bool load_portal_material_atlas(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    const int magic_p = fgetc(file);
    const int magic_6 = fgetc(file);
    int width = 0;
    int height = 0;
    int max_value = 0;
    if (magic_p != 'P' || magic_6 != '6' || !ppm_read_int(file, &width) || !ppm_read_int(file, &height) || !ppm_read_int(file, &max_value) || width != WALL_TEX_W || height != WALL_TEX_H ||
        max_value != 255) {
        fclose(file);
        return false;
    }
    const size_t rgb_size = (size_t)WALL_TEX_W * (size_t)WALL_TEX_H * 3U;
    uint8_t *rgb = (uint8_t *)malloc(rgb_size);
    if (rgb == NULL) {
        fclose(file);
        return false;
    }
    const size_t read_size = fread(rgb, 1, rgb_size, file);
    fclose(file);
    if (read_size != rgb_size) {
        free(rgb);
        return false;
    }
    for (int y = 0; y < WALL_TEX_H; ++y) {
        for (int x = 0; x < WALL_TEX_W; ++x) {
            const int dst = (y * WALL_TEX_W + x) * 4;
            const int src = (y * WALL_TEX_W + x) * 3;
            s_wall_pixels[dst + 0] = rgb[src + 0];
            s_wall_pixels[dst + 1] = rgb[src + 1];
            s_wall_pixels[dst + 2] = rgb[src + 2];
            s_wall_pixels[dst + 3] = 255;
        }
    }
    free(rgb);
    return true;
}

static void load_or_generate_wall_texture(void) {
    generate_wall_texture();
    s_material_atlas_loaded_from_asset = load_portal_material_atlas(BACKROOMS_PORTAL_MATERIAL_ATLAS_PATH);
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
    ui_rect(x + 3, y + 4, w, h, 0, 0, 0, danger ? 104 : 66);
    ui_rect(x, y, w, h, danger ? 22 : 12, danger ? 3 : 10, danger ? 3 : 7, danger ? 166 : 126);
    ui_rect(x, y, w, 2, danger ? 144 : 116, danger ? 35 : 92, danger ? 28 : 52, danger ? 158 : 104);
    ui_rect(x, y + h - 2, w, 2, 0, 0, 0, 72);
    ui_rect(x, y, 2, h, 0, 0, 0, 48);
    ui_rect(x + w - 2, y, 2, h, 0, 0, 0, 48);
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

static void portal_overlay_emit_vertex(uint32_t *count, float x, float y, float z, float r, float g, float b, float a, float u, float v) {
    if (*count >= PORTAL_OVERLAY_MAX_VERTICES) {
        return;
    }
    s_portal_overlay_vertices[*count] =
        (PortalOverlayVertex){.x = x, .y = y, .z = z, .r = r, .g = g, .b = b, .a = a, .u = u, .v = v, .kind = s_portal_overlay_emit_kind};
    *count += 1U;
}

static void portal_overlay_emit_quad(uint32_t *count,
                                     float ax,
                                     float ay,
                                     float az,
                                     float bx,
                                     float by,
                                     float bz,
                                     float cx,
                                     float cy,
                                     float cz,
                                     float dx,
                                     float dy,
                                     float dz,
                                     float r,
                                     float g,
                                     float b,
                                     float a) {
    portal_overlay_emit_vertex(count, ax, ay, az, r, g, b, a, 0.0F, 0.0F);
    portal_overlay_emit_vertex(count, bx, by, bz, r, g, b, a, 1.0F, 0.0F);
    portal_overlay_emit_vertex(count, cx, cy, cz, r, g, b, a, 1.0F, 1.0F);
    portal_overlay_emit_vertex(count, ax, ay, az, r, g, b, a, 0.0F, 0.0F);
    portal_overlay_emit_vertex(count, cx, cy, cz, r, g, b, a, 1.0F, 1.0F);
    portal_overlay_emit_vertex(count, dx, dy, dz, r, g, b, a, 0.0F, 1.0F);
}

static void portal_overlay_emit_yz_quad(uint32_t *count,
                                        float x,
                                        float y0,
                                        float y1,
                                        float z0,
                                        float z1,
                                        float r,
                                        float g,
                                        float b,
                                        float a) {
    portal_overlay_emit_quad(count, x, y0, z0, x, y1, z0, x, y1, z1, x, y0, z1, r, g, b, a);
}

static void portal_overlay_emit_floor_quad(uint32_t *count,
                                           float x0,
                                           float x1,
                                           float y,
                                           float z0,
                                           float z1,
                                           float r,
                                           float g,
                                           float b,
                                           float a) {
    portal_overlay_emit_quad(count, x0, y, z0, x1, y, z0, x1, y, z1, x0, y, z1, r, g, b, a);
}

static void portal_overlay_emit_xy_quad(uint32_t *count,
                                        float x0,
                                        float x1,
                                        float y0,
                                        float y1,
                                        float z,
                                        float r,
                                        float g,
                                        float b,
                                        float a) {
    portal_overlay_emit_quad(count, x0, y0, z, x1, y0, z, x1, y1, z, x0, y1, z, r, g, b, a);
}

static void portal_overlay_emit_box(uint32_t *count,
                                    float x0,
                                    float x1,
                                    float y0,
                                    float y1,
                                    float z0,
                                    float z1,
                                    float r,
                                    float g,
                                    float b,
                                    float a) {
    const float top_r = r * 1.12F;
    const float top_g = g * 1.08F;
    const float top_b = b * 0.94F;
    const float side_r = r * 0.82F;
    const float side_g = g * 0.80F;
    const float side_b = b * 0.76F;
    const float back_r = r * 0.66F;
    const float back_g = g * 0.64F;
    const float back_b = b * 0.62F;
    portal_overlay_emit_yz_quad(count, x0, y0, y1, z0, z1, back_r, back_g, back_b, a);
    portal_overlay_emit_yz_quad(count, x1, y0, y1, z0, z1, r, g, b, a);
    portal_overlay_emit_floor_quad(count, x0, x1, y0, z0, z1, side_r, side_g, side_b, a);
    portal_overlay_emit_floor_quad(count, x0, x1, y1, z0, z1, top_r, top_g, top_b, a);
    portal_overlay_emit_xy_quad(count, x0, x1, y0, y1, z0, side_r, side_g, side_b, a);
    portal_overlay_emit_xy_quad(count, x0, x1, y0, y1, z1, side_r * 0.92F, side_g * 0.92F, side_b * 0.92F, a);
}

static void portal_overlay_emit_floor_z_strip(uint32_t *count,
                                              float x0,
                                              float x1,
                                              float y,
                                              float z,
                                              float width,
                                              float r,
                                              float g,
                                              float b,
                                              float a) {
    portal_overlay_emit_floor_quad(count, x0, x1, y, z - width, z + width, r, g, b, a);
}

static void portal_overlay_emit_floor_x_strip(uint32_t *count,
                                              float x,
                                              float y,
                                              float z0,
                                              float z1,
                                              float width,
                                              float r,
                                              float g,
                                              float b,
                                              float a) {
    portal_overlay_emit_floor_quad(count, x - width, x + width, y, z0, z1, r, g, b, a);
}

static float portal_panel_variation(int x, int z) {
    const float s = sinf((float)(x * 37 + z * 19) * 12.9898F) * 43758.546875F;
    return s - floorf(s);
}

static void portal_overlay_emit_aperture_occlusion(uint32_t *count, float wall_x, float z0, float z1, float min_y, float max_y, float jamb) {
    const float side = 0.034F + jamb * 0.026F;
    const float inner_x = wall_x + 0.040F;
    s_portal_overlay_emit_kind = 4.0F;
    portal_overlay_emit_yz_quad(count, inner_x, min_y + 0.015F, max_y + 0.010F, z0 + 0.028F, z0 + side, 0.036F, 0.028F, 0.014F, 0.25F);
    portal_overlay_emit_yz_quad(count, inner_x, min_y + 0.015F, max_y + 0.010F, z1 - side, z1 - 0.028F, 0.034F, 0.026F, 0.013F, 0.26F);
    portal_overlay_emit_yz_quad(count, inner_x + 0.010F, max_y - 0.050F, max_y + 0.018F, z0 + 0.075F, z1 - 0.075F, 0.034F, 0.026F, 0.012F, 0.24F);
    s_portal_overlay_emit_kind = 0.0F;
}

static void portal_overlay_emit_room_mesh_layer(uint32_t *count,
                                                const BackroomsPortalGpuParams *portal,
                                                float wall_x,
                                                float center_z,
                                                float z0,
                                                float z1,
                                                float min_y,
                                                float max_y) {
    const float depth = clampf(portal->shape[0], 3.2F, 12.0F);
    const float room_x0 = wall_x + 0.22F;
    const float room_x1 = wall_x + 3.25F + depth * 0.055F;
    const float inner_z0 = z0 + 0.13F;
    const float inner_z1 = z1 - 0.13F;
    const float ceiling_y = max_y + 0.42F;
    const float panel_y0 = min_y + 0.08F;
    const float panel_y1 = max_y + 0.26F;
    const float grime = clampf(portal->material[2], 0.0F, 1.0F);
    const float wet = clampf(portal->material[3], 0.0F, 1.0F);
    const float light = clampf(portal->light[1] * 0.28F, 0.22F, 0.72F);
    const int floor_cols = 5;
    const int floor_rows = 4;
    const float seam_y = min_y + 0.018F;
    const uint32_t shell_start = *count;

    s_portal_overlay_emit_kind = 5.0F;
    portal_overlay_emit_floor_quad(count, room_x0 + 0.18F, room_x1 - 0.18F, min_y + 0.010F, inner_z0 + 0.08F, inner_z1 - 0.08F, 0.070F, 0.058F, 0.035F, 0.46F + wet * 0.10F);
    portal_overlay_emit_xy_quad(count, room_x0 + 0.02F, room_x1 - 0.10F, panel_y0 - 0.02F, panel_y1 + 0.04F, inner_z0 - 0.034F, 0.118F, 0.104F, 0.058F, 0.50F + grime * 0.12F);
    portal_overlay_emit_xy_quad(count, room_x0 + 0.02F, room_x1 - 0.10F, panel_y0 - 0.02F, panel_y1 + 0.04F, inner_z1 + 0.034F, 0.102F, 0.091F, 0.051F, 0.52F + grime * 0.12F);
    portal_overlay_emit_yz_quad(count, room_x1 + 0.18F, panel_y0 - 0.04F, panel_y1 + 0.05F, inner_z0 + 0.05F, inner_z1 - 0.05F, 0.094F, 0.083F, 0.046F, 0.56F + grime * 0.12F);
    portal_overlay_emit_floor_quad(count, room_x0 + 0.25F, room_x1 - 0.25F, ceiling_y - 0.012F, inner_z0 + 0.07F, inner_z1 - 0.07F, 0.145F, 0.128F, 0.072F, 0.42F + light * 0.10F);
    const float return_x0 = wall_x + 0.055F;
    const float return_x1 = room_x0 + 0.68F;
    portal_overlay_emit_xy_quad(count, return_x0, return_x1, min_y - 0.012F, ceiling_y + 0.006F, inner_z0 - 0.072F, 0.052F, 0.044F, 0.026F, 0.92F);
    portal_overlay_emit_xy_quad(count, return_x0, return_x1, min_y - 0.012F, ceiling_y + 0.006F, inner_z1 + 0.072F, 0.047F, 0.040F, 0.024F, 0.94F);
    portal_overlay_emit_floor_quad(count, return_x0, return_x1, ceiling_y + 0.004F, inner_z0 - 0.060F, inner_z1 + 0.060F, 0.062F, 0.052F, 0.030F, 0.90F);
    portal_overlay_emit_floor_quad(count, return_x0 - 0.010F, return_x1 + 0.12F, min_y - 0.024F, inner_z0 - 0.056F, inner_z1 + 0.056F, 0.040F, 0.032F, 0.020F, 0.86F);
    portal_overlay_emit_yz_quad(count, return_x1 + 0.018F, min_y + 0.010F, ceiling_y - 0.020F, inner_z0 - 0.060F, inner_z0 + 0.016F, 0.026F, 0.021F, 0.013F, 0.96F);
    portal_overlay_emit_yz_quad(count, return_x1 + 0.018F, min_y + 0.010F, ceiling_y - 0.020F, inner_z1 - 0.016F, inner_z1 + 0.060F, 0.024F, 0.020F, 0.012F, 0.96F);
    s_portal_overlay_emit_kind = 7.0F;
    portal_overlay_emit_yz_quad(count, wall_x + 0.020F, min_y - 0.040F, ceiling_y + 0.020F, z0 - 0.150F, z0 + 0.060F, 0.110F, 0.086F, 0.040F, 0.98F);
    portal_overlay_emit_yz_quad(count, wall_x + 0.020F, min_y - 0.040F, ceiling_y + 0.020F, z1 - 0.060F, z1 + 0.150F, 0.102F, 0.080F, 0.038F, 0.98F);
    portal_overlay_emit_yz_quad(count, wall_x + 0.016F, max_y - 0.020F, max_y + 0.180F, z0 - 0.130F, z1 + 0.130F, 0.138F, 0.108F, 0.048F, 0.96F);
    portal_overlay_emit_floor_quad(count, wall_x - 0.030F, return_x1 + 0.170F, min_y - 0.036F, z0 - 0.120F, z1 + 0.120F, 0.104F, 0.080F, 0.040F, 0.98F);
    portal_overlay_emit_yz_quad(count, wall_x + 0.052F, min_y + 0.050F, max_y + 0.080F, z0 + 0.050F, z0 + 0.118F, 0.040F, 0.031F, 0.017F, 0.99F);
    portal_overlay_emit_yz_quad(count, wall_x + 0.052F, min_y + 0.050F, max_y + 0.080F, z1 - 0.118F, z1 - 0.050F, 0.038F, 0.030F, 0.016F, 0.99F);
    s_portal_overlay_emit_kind = 7.0F;
    portal_overlay_emit_box(count, return_x0 + 0.06F, return_x1 + 0.34F, min_y + 0.020F, min_y + 0.118F, inner_z0 - 0.100F, inner_z0 + 0.055F, 0.112F, 0.082F, 0.034F, 0.99F);
    portal_overlay_emit_box(count, return_x0 + 0.06F, return_x1 + 0.34F, min_y + 0.020F, min_y + 0.118F, inner_z1 - 0.055F, inner_z1 + 0.100F, 0.104F, 0.076F, 0.032F, 0.99F);
    portal_overlay_emit_box(count, return_x0 + 0.04F, return_x1 + 0.28F, ceiling_y - 0.120F, ceiling_y + 0.014F, inner_z0 - 0.095F, inner_z0 + 0.070F, 0.134F, 0.104F, 0.046F, 0.98F);
    portal_overlay_emit_box(count, return_x0 + 0.04F, return_x1 + 0.28F, ceiling_y - 0.120F, ceiling_y + 0.014F, inner_z1 - 0.070F, inner_z1 + 0.095F, 0.124F, 0.096F, 0.043F, 0.98F);
    portal_overlay_emit_box(count, room_x0 + 0.16F, room_x1 - 0.24F, min_y + 0.010F, min_y + 0.100F, inner_z0 - 0.020F, inner_z0 + 0.120F, 0.062F, 0.047F, 0.022F, 0.99F);
    portal_overlay_emit_box(count, room_x0 + 0.16F, room_x1 - 0.24F, min_y + 0.010F, min_y + 0.100F, inner_z1 - 0.120F, inner_z1 + 0.020F, 0.058F, 0.044F, 0.021F, 0.99F);
    portal_overlay_emit_box(count, room_x0 + 0.10F, room_x1 - 0.18F, ceiling_y - 0.115F, ceiling_y + 0.018F, inner_z0 + 0.045F, inner_z0 + 0.180F, 0.078F, 0.062F, 0.030F, 0.98F);
    portal_overlay_emit_box(count, room_x0 + 0.10F, room_x1 - 0.18F, ceiling_y - 0.115F, ceiling_y + 0.018F, inner_z1 - 0.180F, inner_z1 - 0.045F, 0.074F, 0.058F, 0.028F, 0.98F);
    portal_overlay_emit_box(count, room_x0 + 0.42F, room_x1 - 0.50F, ceiling_y - 0.085F, ceiling_y + 0.006F, center_z - 0.090F, center_z + 0.090F, 0.095F, 0.078F, 0.038F, 0.96F);
    portal_overlay_emit_box(count, room_x1 - 0.20F, room_x1 + 0.34F, panel_y0 - 0.020F, panel_y1 + 0.055F, inner_z0 + 0.045F, inner_z1 - 0.045F, 0.086F, 0.076F, 0.042F, 0.99F);
    portal_overlay_emit_box(count, room_x0 + 0.34F, room_x1 - 0.36F, min_y + 0.008F, min_y + 0.065F, center_z - 0.240F, center_z + 0.240F, 0.088F, 0.064F, 0.026F, 0.98F);
    portal_overlay_emit_box(count, room_x0 + 0.38F, room_x1 - 0.46F, ceiling_y - 0.040F, ceiling_y + 0.052F, center_z - 0.170F, center_z + 0.170F, 0.120F, 0.098F, 0.048F, 0.97F);
    s_portal_overlay_emit_kind = 5.0F;
    portal_overlay_emit_yz_quad(count, room_x0 + 0.70F, panel_y1 - 0.16F, ceiling_y + 0.01F, inner_z0 + 0.08F, inner_z1 - 0.08F, 0.048F, 0.039F, 0.021F, 0.44F);
    portal_overlay_emit_yz_quad(count, room_x1 - 0.42F, panel_y0 - 0.02F, panel_y1 + 0.08F, center_z - 0.12F, center_z + 0.12F, 0.032F, 0.026F, 0.015F, 0.55F);
    for (int ix = 0; ix < floor_cols; ++ix) {
        const float fx0 = room_x0 + (room_x1 - room_x0) * ((float)ix / (float)floor_cols);
        const float fx1 = room_x0 + (room_x1 - room_x0) * ((float)(ix + 1) / (float)floor_cols) - 0.018F;
        for (int iz = 0; iz < floor_rows; ++iz) {
            const float fz0 = inner_z0 + (inner_z1 - inner_z0) * ((float)iz / (float)floor_rows);
            const float fz1 = inner_z0 + (inner_z1 - inner_z0) * ((float)(iz + 1) / (float)floor_rows) - 0.014F;
            const float v = portal_panel_variation(ix, iz);
            const float base = 0.058F + v * 0.026F;
            portal_overlay_emit_floor_quad(count, fx0, fx1, min_y + 0.008F, fz0, fz1, base * 0.92F, base * 0.78F, base * 0.48F, 0.62F + wet * 0.12F);
        }
    }
    for (int ix = 0; ix < 4; ++ix) {
        const float sx0 = room_x0 + (room_x1 - room_x0) * ((float)ix / 4.0F);
        const float sx1 = room_x0 + (room_x1 - room_x0) * ((float)(ix + 1) / 4.0F) - 0.020F;
        const float v = portal_panel_variation(ix, 7);
        const float r = 0.134F + v * 0.032F;
        const float g = 0.119F + v * 0.028F;
        const float b = 0.064F + v * 0.018F;
        portal_overlay_emit_xy_quad(count, sx0, sx1, panel_y0, panel_y1, inner_z0 - 0.040F, r * 0.84F, g * 0.78F, b * 0.70F, 0.68F + grime * 0.10F);
        portal_overlay_emit_xy_quad(count, sx0, sx1, panel_y0, panel_y1, inner_z1 + 0.040F, r * 0.76F, g * 0.71F, b * 0.64F, 0.70F + grime * 0.10F);
    }
    for (int ix = 1; ix < 4; ++ix) {
        const float sx = room_x0 + (room_x1 - room_x0) * ((float)ix / 4.0F);
        portal_overlay_emit_xy_quad(count, sx - 0.026F, sx + 0.026F, panel_y0 - 0.035F, panel_y1 + 0.050F, inner_z0 - 0.058F, 0.030F, 0.024F, 0.014F, 0.92F);
        portal_overlay_emit_xy_quad(count, sx - 0.026F, sx + 0.026F, panel_y0 - 0.035F, panel_y1 + 0.050F, inner_z1 + 0.058F, 0.027F, 0.022F, 0.013F, 0.94F);
    }
    portal_overlay_emit_xy_quad(count, room_x0 + 0.05F, room_x1 - 0.10F, panel_y0 + 0.62F, panel_y0 + 0.70F, inner_z0 - 0.064F, 0.034F, 0.027F, 0.015F, 0.90F);
    portal_overlay_emit_xy_quad(count, room_x0 + 0.05F, room_x1 - 0.10F, panel_y0 + 0.62F, panel_y0 + 0.70F, inner_z1 + 0.064F, 0.031F, 0.025F, 0.014F, 0.92F);
    const float solid_back_x = room_x1 + 0.20F;
    for (int iz = 0; iz < 3; ++iz) {
        const float bz0 = inner_z0 + (inner_z1 - inner_z0) * ((float)iz / 3.0F);
        const float bz1 = inner_z0 + (inner_z1 - inner_z0) * ((float)(iz + 1) / 3.0F) - 0.020F;
        const float v = portal_panel_variation(11, iz);
        portal_overlay_emit_yz_quad(count, solid_back_x, panel_y0, panel_y1, bz0, bz1, 0.108F + v * 0.030F, 0.096F + v * 0.024F, 0.052F + v * 0.014F, 0.72F + grime * 0.12F);
    }
    portal_overlay_emit_yz_quad(count, solid_back_x - 0.018F, panel_y0 - 0.050F, panel_y0 + 0.140F, inner_z0 + 0.030F, inner_z1 - 0.030F, 0.038F, 0.030F, 0.016F, 0.96F);
    portal_overlay_emit_yz_quad(count, solid_back_x - 0.024F, panel_y1 - 0.095F, panel_y1 + 0.010F, inner_z0 + 0.060F, inner_z1 - 0.060F, 0.050F, 0.040F, 0.020F, 0.88F);
    for (int ix = 0; ix < 4; ++ix) {
        const float cx0 = room_x0 + (room_x1 - room_x0) * ((float)ix / 4.0F);
        const float cx1 = room_x0 + (room_x1 - room_x0) * ((float)(ix + 1) / 4.0F) - 0.026F;
        portal_overlay_emit_floor_quad(count, cx0, cx1, ceiling_y - 0.015F, inner_z0, inner_z1, 0.150F, 0.132F, 0.072F, 0.58F + light * 0.10F);
    }
    s_portal_overlay_emit_kind = 3.0F;
    for (int ix = 0; ix < 3; ++ix) {
        const float lx0 = room_x0 + 0.35F + (room_x1 - room_x0 - 0.70F) * ((float)ix / 3.0F);
        const float lx1 = lx0 + (room_x1 - room_x0) * 0.16F;
        portal_overlay_emit_floor_quad(count, lx0, lx1, ceiling_y - 0.034F, center_z - 0.155F, center_z + 0.155F, 0.80F, 0.64F, 0.28F, 0.62F + light * 0.22F);
        portal_overlay_emit_floor_quad(count, lx0 + 0.05F, lx1 - 0.05F, min_y + 0.018F, center_z - 0.245F, center_z + 0.245F, 0.34F, 0.23F, 0.095F, 0.30F + wet * 0.10F);
    }
    s_portal_overlay_emit_kind = 5.0F;
    const float nested_x = solid_back_x - 0.030F;
    const float nested_z0 = center_z - 0.46F;
    const float nested_z1 = center_z + 0.46F;
    const float nested_y0 = panel_y0 + 0.22F;
    const float nested_y1 = panel_y1 - 0.18F;
    s_portal_overlay_emit_kind = 7.0F;
    portal_overlay_emit_yz_quad(count, nested_x, nested_y0, nested_y1, nested_z0, nested_z1, 0.018F, 0.014F, 0.007F, 0.94F);
    portal_overlay_emit_yz_quad(count, nested_x - 0.014F, nested_y0 - 0.055F, nested_y1 + 0.055F, nested_z0 - 0.050F, nested_z0 + 0.020F, 0.112F, 0.082F, 0.038F, 0.96F);
    portal_overlay_emit_yz_quad(count, nested_x - 0.014F, nested_y0 - 0.055F, nested_y1 + 0.055F, nested_z1 - 0.020F, nested_z1 + 0.050F, 0.102F, 0.075F, 0.034F, 0.96F);
    portal_overlay_emit_yz_quad(count, nested_x - 0.018F, nested_y1 - 0.020F, nested_y1 + 0.070F, nested_z0 - 0.040F, nested_z1 + 0.040F, 0.135F, 0.102F, 0.046F, 0.94F);
    portal_overlay_emit_yz_quad(count, nested_x - 0.018F, nested_y0 - 0.070F, nested_y0 + 0.020F, nested_z0 - 0.040F, nested_z1 + 0.040F, 0.074F, 0.054F, 0.026F, 0.94F);
    s_portal_overlay_emit_kind = 3.0F;
    portal_overlay_emit_yz_quad(count, nested_x - 0.026F, nested_y1 + 0.075F, nested_y1 + 0.135F, center_z - 0.34F, center_z + 0.34F, 0.78F, 0.62F, 0.26F, 0.92F);
    portal_overlay_emit_yz_quad(count, room_x0 + 0.46F, panel_y1 + 0.02F, ceiling_y - 0.018F, center_z - 0.16F, center_z + 0.16F, 0.66F, 0.52F, 0.22F, 0.80F);
    s_portal_overlay_emit_kind = 6.0F;
    portal_overlay_emit_yz_quad(count, solid_back_x - 0.060F, panel_y0 + 0.46F, panel_y1 + 0.030F, center_z - 0.075F, center_z + 0.075F, 1.00F, 0.78F, 0.32F, 0.94F);
    portal_overlay_emit_yz_quad(count, solid_back_x - 0.070F, panel_y0 + 0.38F, panel_y1 + 0.110F, center_z - 0.145F, center_z + 0.145F, 0.48F, 0.34F, 0.13F, 0.48F);
    portal_overlay_emit_floor_quad(count, return_x0 + 0.10F, room_x1 - 0.42F, ceiling_y - 0.030F, center_z - 0.24F, center_z + 0.24F, 0.82F, 0.66F, 0.30F, 0.46F + light * 0.18F);
    portal_overlay_emit_floor_quad(count, return_x0 + 0.18F, room_x1 - 0.78F, min_y + 0.018F, center_z - 0.28F, center_z + 0.28F, 0.40F, 0.28F, 0.12F, 0.34F + wet * 0.12F);
    portal_overlay_emit_floor_quad(count, room_x0 + 0.46F, room_x1 - 0.48F, min_y + 0.030F, center_z - 0.185F, center_z + 0.185F, 0.58F, 0.39F, 0.15F, 0.46F + wet * 0.12F);
    portal_overlay_emit_floor_quad(count, return_x0 + 0.06F, return_x1 + 0.44F, ceiling_y - 0.024F, center_z - 0.44F, center_z + 0.44F, 0.92F, 0.73F, 0.32F, 0.44F + light * 0.18F);
    portal_overlay_emit_floor_quad(count, return_x0 + 0.02F, return_x1 + 0.52F, min_y + 0.024F, center_z - 0.54F, center_z + 0.54F, 0.46F, 0.31F, 0.13F, 0.32F + wet * 0.12F);
    s_portal_overlay_emit_kind = 5.0F;
    s_last_portal_shell_vertices = *count - shell_start;

    s_portal_overlay_emit_kind = 1.0F;
    for (int ix = 0; ix < floor_cols; ++ix) {
        const float fx0 = room_x0 + (room_x1 - room_x0) * ((float)ix / (float)floor_cols);
        const float fx1 = room_x0 + (room_x1 - room_x0) * ((float)(ix + 1) / (float)floor_cols) - 0.018F;
        for (int iz = 0; iz < floor_rows; ++iz) {
            const float fz0 = inner_z0 + (inner_z1 - inner_z0) * ((float)iz / (float)floor_rows);
            const float fz1 = inner_z0 + (inner_z1 - inner_z0) * ((float)(iz + 1) / (float)floor_rows) - 0.014F;
            const float v = portal_panel_variation(ix, iz);
            const float base = 0.050F + v * 0.024F;
            portal_overlay_emit_floor_quad(count, fx0, fx1, min_y + 0.006F, fz0, fz1, base * 0.90F, base * 0.78F, base * 0.50F, 0.245F + wet * 0.105F);
        }
    }
    s_portal_overlay_emit_kind = 2.0F;
    for (int ix = 1; ix < floor_cols; ++ix) {
        const float x = room_x0 + (room_x1 - room_x0) * ((float)ix / (float)floor_cols);
        portal_overlay_emit_floor_x_strip(count, x, seam_y, inner_z0, inner_z1, 0.010F, 0.018F, 0.015F, 0.010F, 0.30F);
    }
    for (int iz = 1; iz < floor_rows; ++iz) {
        const float z = inner_z0 + (inner_z1 - inner_z0) * ((float)iz / (float)floor_rows);
        portal_overlay_emit_floor_z_strip(count, room_x0, room_x1, seam_y, z, 0.010F, 0.018F, 0.015F, 0.010F, 0.28F);
    }

    s_portal_overlay_emit_kind = 1.0F;
    for (int ix = 0; ix < 4; ++ix) {
        const float sx0 = room_x0 + (room_x1 - room_x0) * ((float)ix / 4.0F);
        const float sx1 = room_x0 + (room_x1 - room_x0) * ((float)(ix + 1) / 4.0F) - 0.020F;
        const float v = portal_panel_variation(ix, 7);
        const float r = 0.145F + v * 0.035F;
        const float g = 0.130F + v * 0.030F;
        const float b = 0.070F + v * 0.020F;
        portal_overlay_emit_xy_quad(count, sx0, sx1, panel_y0, panel_y1, inner_z0 - 0.018F, r * 0.82F, g * 0.78F, b * 0.72F, 0.335F + grime * 0.120F);
        portal_overlay_emit_xy_quad(count, sx0, sx1, panel_y0, panel_y1, inner_z1 + 0.018F, r * 0.74F, g * 0.70F, b * 0.66F, 0.355F + grime * 0.130F);
    }
    s_portal_overlay_emit_kind = 2.0F;
    for (int ix = 1; ix < 4; ++ix) {
        const float sx = room_x0 + (room_x1 - room_x0) * ((float)ix / 4.0F);
        portal_overlay_emit_xy_quad(count, sx - 0.012F, sx + 0.012F, panel_y0, panel_y1, inner_z0 - 0.024F, 0.050F, 0.042F, 0.024F, 0.26F);
        portal_overlay_emit_xy_quad(count, sx - 0.012F, sx + 0.012F, panel_y0, panel_y1, inner_z1 + 0.024F, 0.045F, 0.038F, 0.022F, 0.28F);
    }
    portal_overlay_emit_xy_quad(count, room_x0, room_x1, panel_y0 + 0.66F, panel_y0 + 0.69F, inner_z0 - 0.026F, 0.040F, 0.034F, 0.020F, 0.26F);
    portal_overlay_emit_xy_quad(count, room_x0, room_x1, panel_y0 + 0.66F, panel_y0 + 0.69F, inner_z1 + 0.026F, 0.038F, 0.032F, 0.019F, 0.28F);

    const float back_x = room_x1 + 0.16F;
    s_portal_overlay_emit_kind = 1.0F;
    for (int iz = 0; iz < 3; ++iz) {
        const float bz0 = inner_z0 + (inner_z1 - inner_z0) * ((float)iz / 3.0F);
        const float bz1 = inner_z0 + (inner_z1 - inner_z0) * ((float)(iz + 1) / 3.0F) - 0.020F;
        const float v = portal_panel_variation(11, iz);
        portal_overlay_emit_yz_quad(count, back_x, panel_y0, panel_y1, bz0, bz1, 0.115F + v * 0.035F, 0.103F + v * 0.028F, 0.058F + v * 0.016F, 0.370F + grime * 0.115F);
    }
    s_portal_overlay_emit_kind = 2.0F;
    for (int iz = 1; iz < 3; ++iz) {
        const float z = inner_z0 + (inner_z1 - inner_z0) * ((float)iz / 3.0F);
        portal_overlay_emit_yz_quad(count, back_x - 0.014F, panel_y0, panel_y1, z - 0.010F, z + 0.010F, 0.040F, 0.034F, 0.020F, 0.32F);
    }
    portal_overlay_emit_yz_quad(count, back_x - 0.018F, panel_y0 + 0.76F, panel_y0 + 0.80F, inner_z0, inner_z1, 0.050F, 0.042F, 0.024F, 0.30F);

    s_portal_overlay_emit_kind = 1.0F;
    for (int ix = 0; ix < 4; ++ix) {
        const float cx0 = room_x0 + (room_x1 - room_x0) * ((float)ix / 4.0F);
        const float cx1 = room_x0 + (room_x1 - room_x0) * ((float)(ix + 1) / 4.0F) - 0.026F;
        portal_overlay_emit_floor_quad(count, cx0, cx1, ceiling_y, inner_z0, inner_z1, 0.19F, 0.17F, 0.09F, 0.220F + light * 0.090F);
    }
    s_portal_overlay_emit_kind = 2.0F;
    for (int ix = 1; ix < 4; ++ix) {
        const float x = room_x0 + (room_x1 - room_x0) * ((float)ix / 4.0F);
        portal_overlay_emit_floor_x_strip(count, x, ceiling_y - 0.004F, inner_z0, inner_z1, 0.008F, 0.060F, 0.052F, 0.030F, 0.28F);
    }
    portal_overlay_emit_floor_z_strip(count, room_x0, room_x1, ceiling_y - 0.006F, center_z, 0.010F, 0.064F, 0.056F, 0.032F, 0.26F);

    const float spill_x0 = room_x0 + 0.35F;
    const float spill_x1 = room_x1 - 0.35F;
    s_portal_overlay_emit_kind = 3.0F;
    portal_overlay_emit_floor_quad(count, spill_x0, spill_x1, min_y + 0.022F, center_z - 0.34F, center_z + 0.34F, 0.66F, 0.53F, 0.24F, 0.110F + light * 0.115F);
    portal_overlay_emit_floor_quad(count, spill_x0 + 0.34F, spill_x1 - 0.34F, min_y + 0.026F, center_z - 0.16F, center_z + 0.16F, 0.78F, 0.64F, 0.31F, 0.090F + light * 0.075F);
    portal_overlay_emit_yz_quad(count, back_x - 0.035F, panel_y0 + 0.40F, panel_y0 + 0.56F, center_z - 0.52F, center_z + 0.52F, 0.60F, 0.48F, 0.22F, 0.105F + light * 0.105F);
    s_portal_overlay_emit_kind = 2.0F;
    portal_overlay_emit_yz_quad(count, back_x - 0.040F, panel_y0, panel_y0 + 0.18F, inner_z0, inner_z1, 0.014F, 0.012F, 0.008F, 0.32F + grime * 0.16F);
    s_portal_overlay_emit_kind = 0.0F;
}

static uint32_t build_portal_overlay_vertices(const BackroomsPortalGpuParams *portal) {
    if (portal == NULL || portal->entry[3] <= 0.5F) {
        s_last_portal_room_mesh_vertices = 0U;
        s_last_portal_shell_vertices = 0U;
        return 0U;
    }

    uint32_t count = 0U;
    const float wall_x = portal->entry[0] - 0.035F;
    const float center_z = portal->entry[1];
    const float half_z = portal->entry[2];
    const float min_y = portal->bounds[0];
    const float max_y = portal->bounds[1];
    const float jamb = clampf(portal->construction[0], 0.18F, 1.10F);
    const float lip = clampf(portal->construction[1], 0.18F, 1.15F);
    const float trim = clampf(portal->finish[0], 0.2F, 1.0F);
    const float conduit = clampf(portal->construction[2], 0.0F, 1.0F);
    const float column = clampf(portal->construction[3], 0.0F, 1.0F);
    const float z0 = center_z - half_z;
    const float z1 = center_z + half_z;
    const float side = 0.075F + jamb * 0.045F;
    const float top = 0.065F + trim * 0.055F;
    const float glow_a = 0.18F + 0.18F * portal->light[1];

    const uint32_t room_mesh_start = count;
    portal_overlay_emit_room_mesh_layer(&count, portal, wall_x, center_z, z0, z1, min_y, max_y);
    s_last_portal_room_mesh_vertices = count - room_mesh_start;

    portal_overlay_emit_aperture_occlusion(&count, wall_x, z0, z1, min_y, max_y, jamb);

    s_portal_overlay_emit_kind = 0.0F;
    portal_overlay_emit_yz_quad(&count, wall_x - 0.010F, min_y - 0.04F, max_y + 0.04F, z0 - side, z0 + side, 0.18F, 0.14F, 0.076F, 0.035F);
    portal_overlay_emit_yz_quad(&count, wall_x - 0.010F, min_y - 0.04F, max_y + 0.04F, z1 - side, z1 + side, 0.17F, 0.13F, 0.072F, 0.036F);
    portal_overlay_emit_yz_quad(&count, wall_x - 0.020F, max_y - top, max_y + top, z0 - side, z1 + side, 0.24F, 0.19F, 0.095F, 0.040F);
    portal_overlay_emit_yz_quad(&count, wall_x - 0.020F, min_y - top * 0.85F, min_y + top * 0.45F, z0 - side, z1 + side, 0.12F, 0.090F, 0.048F, 0.034F);

    portal_overlay_emit_floor_quad(&count, wall_x - 0.10F, wall_x + lip, min_y - 0.035F, z0 - side * 0.7F, z1 + side * 0.7F, 0.19F, 0.15F, 0.078F, 0.052F);
    portal_overlay_emit_floor_quad(&count, wall_x + 0.10F, wall_x + 1.35F + lip * 0.35F, min_y + 0.012F, z0 + 0.15F, z1 - 0.15F, 0.065F, 0.054F, 0.037F, 0.066F);

    const float inner_x = wall_x + 0.92F;
    const float fixture_y0 = max_y + 0.10F;
    const float fixture_y1 = max_y + 0.18F;
    s_portal_overlay_emit_kind = 3.0F;
    portal_overlay_emit_yz_quad(&count, inner_x, fixture_y0, fixture_y1, center_z - 0.54F, center_z + 0.54F, 1.00F, 0.86F, 0.50F, clampf(glow_a, 0.28F, 0.66F));
    s_portal_overlay_emit_kind = 2.0F;
    portal_overlay_emit_yz_quad(&count, inner_x - 0.05F, fixture_y0 - 0.035F, fixture_y0 + 0.025F, center_z - 0.62F, center_z + 0.62F, 0.16F, 0.13F, 0.080F, 0.72F);
    s_portal_overlay_emit_kind = 0.0F;

    if (conduit > 0.05F) {
        const float cy = max_y - 0.24F;
        portal_overlay_emit_yz_quad(&count, wall_x - 0.055F, cy - 0.018F, cy + 0.018F, z0 - 0.72F, z1 + 0.30F, 0.35F, 0.30F, 0.18F, 0.22F + conduit * 0.18F);
    }

    if (column > 0.05F) {
        const float column_z = z1 + 0.42F;
        const float column_w = 0.10F + column * 0.08F;
        portal_overlay_emit_yz_quad(&count, wall_x + 0.18F, min_y - 0.02F, max_y + 0.26F, column_z - column_w, column_z + column_w, 0.16F, 0.13F, 0.080F, 0.34F + column * 0.14F);
        portal_overlay_emit_yz_quad(&count, wall_x + 0.22F, max_y + 0.18F, max_y + 0.26F, column_z - column_w * 1.25F, column_z + column_w * 1.25F, 0.42F, 0.33F, 0.15F, 0.26F);
    }

    return count;
}

static void init_render_resources(void) {
    const float verts[] = {
        -1.0F, -1.0F, 1.0F, -1.0F, 1.0F, 1.0F,
        -1.0F, -1.0F, 1.0F, 1.0F,  -1.0F, 1.0F,
    };

    s_vs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_VERTEX, .source = s_vs_src, .label = "backrooms_fullscreen_vs"});
    s_fs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_FRAGMENT, .source = s_fs_src, .label = "backrooms_liminal_fs"});
    s_portal_overlay_vs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_VERTEX, .source = s_portal_overlay_vs_src, .label = "backrooms_portal_overlay_vs"});
    s_portal_overlay_fs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_FRAGMENT, .source = s_portal_overlay_fs_src, .label = "backrooms_portal_overlay_fs"});
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
    s_portal_solid_pipeline = nt_gfx_make_pipeline(&(nt_pipeline_desc_t){
        .vertex_shader = s_portal_overlay_vs,
        .fragment_shader = s_portal_overlay_fs,
        .layout =
            {
                .attr_count = 4,
                .stride = sizeof(PortalOverlayVertex),
                .attrs =
                    {
                        {.location = 0, .format = NT_FORMAT_FLOAT3, .offset = (uint16_t)offsetof(PortalOverlayVertex, x)},
                        {.location = 1, .format = NT_FORMAT_FLOAT4, .offset = (uint16_t)offsetof(PortalOverlayVertex, r)},
                        {.location = 2, .format = NT_FORMAT_FLOAT2, .offset = (uint16_t)offsetof(PortalOverlayVertex, u)},
                        {.location = 3, .format = NT_FORMAT_FLOAT, .offset = (uint16_t)offsetof(PortalOverlayVertex, kind)},
                    },
            },
        .depth_test = false,
        .depth_write = false,
        .depth_func = NT_DEPTH_ALWAYS,
        .cull_mode = 0,
        .blend = false,
        .label = "backrooms_portal_solid_pipeline",
    });
    s_portal_overlay_pipeline = nt_gfx_make_pipeline(&(nt_pipeline_desc_t){
        .vertex_shader = s_portal_overlay_vs,
        .fragment_shader = s_portal_overlay_fs,
        .layout =
            {
                .attr_count = 4,
                .stride = sizeof(PortalOverlayVertex),
                .attrs =
                    {
                        {.location = 0, .format = NT_FORMAT_FLOAT3, .offset = (uint16_t)offsetof(PortalOverlayVertex, x)},
                        {.location = 1, .format = NT_FORMAT_FLOAT4, .offset = (uint16_t)offsetof(PortalOverlayVertex, r)},
                        {.location = 2, .format = NT_FORMAT_FLOAT2, .offset = (uint16_t)offsetof(PortalOverlayVertex, u)},
                        {.location = 3, .format = NT_FORMAT_FLOAT, .offset = (uint16_t)offsetof(PortalOverlayVertex, kind)},
                    },
            },
        .depth_test = false,
        .depth_write = false,
        .depth_func = NT_DEPTH_ALWAYS,
        .cull_mode = 0,
        .blend = true,
        .blend_src = NT_BLEND_SRC_ALPHA,
        .blend_dst = NT_BLEND_ONE_MINUS_SRC_ALPHA,
        .label = "backrooms_portal_overlay_pipeline",
    });
    s_quad_vbo = nt_gfx_make_buffer(&(nt_buffer_desc_t){.type = NT_BUFFER_VERTEX, .usage = NT_USAGE_IMMUTABLE, .data = verts, .size = sizeof(verts), .label = "backrooms_quad"});
    s_portal_overlay_vbo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_VERTEX,
        .usage = NT_USAGE_DYNAMIC,
        .data = NULL,
        .size = sizeof(s_portal_overlay_vertices),
        .label = "backrooms_portal_overlay_vbo",
    });

    load_or_generate_wall_texture();
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
    nt_gfx_destroy_buffer(s_portal_overlay_vbo);
    nt_gfx_destroy_buffer(s_quad_vbo);
    nt_gfx_destroy_pipeline(s_portal_overlay_pipeline);
    nt_gfx_destroy_pipeline(s_portal_solid_pipeline);
    nt_gfx_destroy_pipeline(s_pipeline);
    nt_gfx_destroy_shader(s_portal_overlay_fs);
    nt_gfx_destroy_shader(s_portal_overlay_vs);
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
    const float mouse_turn = mouse_look_dx() * 0.0022F;
    if (fabsf(mouse_turn) > 0.0001F) {
        s_game.yaw += mouse_turn;
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
    const BackroomsPortalGpuParams portal = backrooms_portal_scene_gpu_params(&s_portal_scene, 0U, s_game.mark_placed);
    nt_gfx_set_uniform_vec4("u_portal_entry", portal.entry);
    nt_gfx_set_uniform_vec4("u_portal_shape", portal.shape);
    nt_gfx_set_uniform_vec4("u_portal_style", portal.style);
    nt_gfx_set_uniform_vec4("u_portal_bounds", portal.bounds);
    nt_gfx_set_uniform_vec4("u_portal_material", portal.material);
    nt_gfx_set_uniform_vec4("u_portal_light", portal.light);
    nt_gfx_set_uniform_vec4("u_portal_finish", portal.finish);
    nt_gfx_set_uniform_vec4("u_portal_construction", portal.construction);
    nt_gfx_draw(0, 6);

    s_last_portal_overlay_vertices = build_portal_overlay_vertices(&portal);
    s_last_portal_blended_vertices = s_last_portal_overlay_vertices > s_last_portal_shell_vertices ? s_last_portal_overlay_vertices - s_last_portal_shell_vertices : 0U;
    if (s_last_portal_overlay_vertices > 0U) {
        nt_gfx_orphan_buffer(s_portal_overlay_vbo, s_portal_overlay_vertices, s_last_portal_overlay_vertices * sizeof(s_portal_overlay_vertices[0]));
        nt_gfx_bind_vertex_buffer(s_portal_overlay_vbo);
        nt_gfx_bind_texture(s_wall_tex, 0);
        if (s_last_portal_shell_vertices > 0U) {
            nt_gfx_bind_pipeline(s_portal_solid_pipeline);
            nt_gfx_set_uniform_int("u_overlay_tex", 0);
            nt_gfx_set_uniform_vec4("u_overlay_resolution", (float[4]){fb_w, fb_h, g_nt_app.time, portal.entry[3]});
            nt_gfx_set_uniform_vec4("u_overlay_player", (float[4]){s_game.x, s_game.z, s_game.yaw, 0.0F});
            nt_gfx_set_uniform_vec4("u_overlay_portal", (float[4]){portal.entry[0], portal.entry[1], portal.entry[2], clampf(portal.light[1] * 0.32F, 0.25F, 0.80F)});
            nt_gfx_draw(0, s_last_portal_shell_vertices);
        }
        if (s_last_portal_blended_vertices > 0U) {
            nt_gfx_bind_pipeline(s_portal_overlay_pipeline);
            nt_gfx_set_uniform_int("u_overlay_tex", 0);
            nt_gfx_set_uniform_vec4("u_overlay_resolution", (float[4]){fb_w, fb_h, g_nt_app.time, portal.entry[3]});
            nt_gfx_set_uniform_vec4("u_overlay_player", (float[4]){s_game.x, s_game.z, s_game.yaw, 0.0F});
            nt_gfx_set_uniform_vec4("u_overlay_portal", (float[4]){portal.entry[0], portal.entry[1], portal.entry[2], clampf(portal.light[1] * 0.32F, 0.25F, 0.80F)});
            nt_gfx_draw(s_last_portal_shell_vertices, s_last_portal_blended_vertices);
        }
    }
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
    cJSON *portal = cJSON_CreateObject();
    const BackroomsPortalGpuParams portal_params = backrooms_portal_scene_gpu_params(&s_portal_scene, 0U, s_game.mark_placed);
    cJSON_AddNumberToObject(portal, "room_count", (double)s_portal_scene.room_count);
    cJSON_AddNumberToObject(portal, "portal_count", (double)s_portal_scene.portal_count);
    cJSON_AddBoolToObject(portal, "visible", portal_params.entry[3] > 0.5F);
    cJSON_AddNumberToObject(portal, "target_half_width", (double)portal_params.shape[1]);
    cJSON_AddNumberToObject(portal, "target_half_depth", (double)portal_params.shape[0]);
    cJSON_AddNumberToObject(portal, "wall_panel_scale", (double)portal_params.material[0]);
    cJSON_AddNumberToObject(portal, "carpet_tile_scale", (double)portal_params.material[1]);
    cJSON_AddNumberToObject(portal, "grime_strength", (double)portal_params.material[2]);
    cJSON_AddNumberToObject(portal, "wetness_strength", (double)portal_params.material[3]);
    cJSON_AddNumberToObject(portal, "fluorescent_intensity", (double)portal_params.light[1]);
    cJSON_AddNumberToObject(portal, "trim_strength", (double)portal_params.finish[0]);
    cJSON_AddNumberToObject(portal, "fixture_spacing", (double)portal_params.finish[1]);
    cJSON_AddNumberToObject(portal, "ceiling_panel_scale", (double)portal_params.finish[2]);
    cJSON_AddNumberToObject(portal, "shadow_spill_strength", (double)portal_params.finish[3]);
    cJSON_AddNumberToObject(portal, "jamb_depth", (double)portal_params.construction[0]);
    cJSON_AddNumberToObject(portal, "threshold_lip", (double)portal_params.construction[1]);
    cJSON_AddNumberToObject(portal, "conduit_strength", (double)portal_params.construction[2]);
    cJSON_AddNumberToObject(portal, "landmark_column_strength", (double)portal_params.construction[3]);
    cJSON *overlay = cJSON_CreateObject();
    cJSON_AddBoolToObject(overlay, "enabled", true);
    cJSON_AddNumberToObject(overlay, "last_vertex_count", (double)s_last_portal_overlay_vertices);
    cJSON_AddNumberToObject(overlay, "room_mesh_vertex_count", (double)s_last_portal_room_mesh_vertices);
    cJSON_AddNumberToObject(overlay, "solid_shell_vertex_count", (double)s_last_portal_shell_vertices);
    cJSON_AddNumberToObject(overlay, "solid_pass_vertex_count", (double)s_last_portal_shell_vertices);
    cJSON_AddNumberToObject(overlay, "blended_detail_vertex_count", (double)s_last_portal_blended_vertices);
    cJSON_AddNumberToObject(overlay, "vertex_capacity", (double)PORTAL_OVERLAY_MAX_VERTICES);
    cJSON_AddStringToObject(overlay, "path", "native_nt_gfx_solid_shell_plus_blended_detail_layer");
    cJSON_AddStringToObject(overlay, "material_source", s_material_atlas_loaded_from_asset ? "asset_ppm_backrooms_material_atlas_wall_carpet_ceiling_trim" : "fallback_runtime_backrooms_material_atlas_wall_carpet_ceiling_trim");
    cJSON_AddNumberToObject(overlay, "material_atlas_width", (double)WALL_TEX_W);
    cJSON_AddNumberToObject(overlay, "material_atlas_height", (double)WALL_TEX_H);
    cJSON_AddBoolToObject(overlay, "material_atlas_loaded_from_asset", s_material_atlas_loaded_from_asset);
    cJSON_AddStringToObject(overlay, "material_asset_path", BACKROOMS_PORTAL_MATERIAL_ATLAS_PATH);
    cJSON_AddItemToObject(portal, "native_overlay", overlay);
    cJSON_AddItemToObject(root, "portal_render", portal);
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

static bool ep_game_perf_stats(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "frame", (double)g_nt_app.frame);
    cJSON_AddNumberToObject(root, "time", (double)g_nt_app.time);
    cJSON_AddNumberToObject(root, "target_dt_ms", (double)(g_nt_app.target_dt * 1000.0F));
    cJSON_AddNumberToObject(root, "framebuffer_width", (double)g_nt_window.fb_width);
    cJSON_AddNumberToObject(root, "framebuffer_height", (double)g_nt_window.fb_height);

    cJSON *gfx = cJSON_CreateObject();
    cJSON_AddNumberToObject(gfx, "draw_calls", (double)g_nt_gfx.frame_stats.draw_calls);
    cJSON_AddNumberToObject(gfx, "draw_calls_instanced", (double)g_nt_gfx.frame_stats.draw_calls_instanced);
    cJSON_AddNumberToObject(gfx, "vertices", (double)g_nt_gfx.frame_stats.vertices);
    cJSON_AddNumberToObject(gfx, "indices", (double)g_nt_gfx.frame_stats.indices);
    cJSON_AddNumberToObject(gfx, "instances", (double)g_nt_gfx.frame_stats.instances);
    cJSON_AddItemToObject(root, "gfx", gfx);

    cJSON *portal = cJSON_CreateObject();
    cJSON_AddNumberToObject(portal, "overlay_vertices", (double)s_last_portal_overlay_vertices);
    cJSON_AddNumberToObject(portal, "room_mesh_vertices", (double)s_last_portal_room_mesh_vertices);
    cJSON_AddNumberToObject(portal, "solid_shell_vertices", (double)s_last_portal_shell_vertices);
    cJSON_AddNumberToObject(portal, "blended_detail_vertices", (double)s_last_portal_blended_vertices);
    cJSON_AddNumberToObject(portal, "vertex_capacity", (double)PORTAL_OVERLAY_MAX_VERTICES);
    cJSON_AddItemToObject(root, "portal", portal);

    *result = root;
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
    nt_devapi_register("game.perf.stats", ep_game_perf_stats, NULL);
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
