#include "app/nt_app.h"
#include "blockside_assets.h"
#include "blockside_capture.h"
#include "blockside_game_types.h"
#include "blockside_hud.h"
#include "blockside_runtime_config.h"
#include "blockside_story.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "devapi/nt_devapi_net.h"
#include "drawable_comp/nt_drawable_comp.h"
#include "entity/nt_entity.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "game_devapi_ui.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "log/nt_log.h"
#include "material/nt_material.h"
#include "material_comp/nt_material_comp.h"
#include "math/nt_math.h"
#include "mesh_comp/nt_mesh_comp.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "render/nt_render_items.h"
#include "renderers/nt_mesh_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "time/nt_time.h"
#include "transform_comp/nt_transform_comp.h"
#include "window/nt_window.h"
#include "cJSON.h"
#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#define BLOCKSIDE_DEVAPI_PORT_DEFAULT 9123
#define MAX_RENDER_OBJECTS 48
#define MAX_RENDER_OBJECT_MESHES 16
#define MAX_RENDER_ITEMS 192
#ifndef GAME_ASSET_PACK_PATH
#define GAME_ASSET_PACK_PATH "assets/blockside_heat.ntpack"
#endif
typedef struct RenderObject {
    nt_entity_t entities[MAX_RENDER_OBJECT_MESHES];
    nt_resource_t meshes[MAX_RENDER_OBJECT_MESHES];
    uint8_t mesh_count;
    float pos[3];
    float scale[3];
    float yaw;
    float color[4];
    bool dynamic;
} RenderObject;

static bool s_devapi_enabled; static uint16_t s_devapi_port = BLOCKSIDE_DEVAPI_PORT_DEFAULT; static int s_window_width = 1280, s_window_height = 720;
static GameRuntime s_game; static RenderObject s_objects[MAX_RENDER_OBJECTS]; static uint32_t s_object_count; static nt_render_item_t s_sort_scratch[MAX_RENDER_ITEMS];

static nt_hash32_t s_pack_id; static nt_buffer_t s_frame_ubo; static nt_texture_t s_fallback_texture; static nt_material_t s_mesh_material, s_text_material; static nt_font_t s_font;

static nt_resource_t s_mesh_vs, s_mesh_fs, s_text_vs, s_text_fs, s_font_resource;

static nt_resource_t s_mesh_city_base[3];
static nt_resource_t s_mesh_low_building[2];
static nt_resource_t s_mesh_large_building[5];
static nt_resource_t s_mesh_street_light[2];
static nt_resource_t s_mesh_car[12];
static nt_resource_t s_mesh_character[3];
static nt_resource_t s_mesh_package[2];

static bool s_pack_dumped;
static const uint8_t s_checker_4x4[4 * 4 * 4] = {
    255, 255, 255, 255, 80, 80, 80, 255, 255, 255, 255, 80, 80, 80, 255,
    80, 80, 80, 255, 255, 255, 255, 80, 80, 80, 255, 255, 255, 255,
    255, 255, 255, 255, 80, 80, 80, 255, 255, 255, 255, 80, 80, 80, 255,
    80, 80, 80, 255, 255, 255, 255, 80, 80, 80, 255, 255, 255, 255,
};

#if NT_DEVAPI_ENABLED
static void register_ui_devapi(float w, float h);
#endif

static float dist2(float ax, float az, float bx, float bz) {
    const float dx = ax - bx;
    const float dz = az - bz;
    return dx * dx + dz * dz;
}
static void add_object(const nt_resource_t *meshes, uint8_t mesh_count, float x, float y, float z, float sx, float sy, float sz, float yaw, const float color[4], bool dynamic) {
    if (s_object_count >= MAX_RENDER_OBJECTS) {
        return;
    }
    RenderObject *o = &s_objects[s_object_count++];
    memset(o, 0, sizeof(*o));
    if (mesh_count > MAX_RENDER_OBJECT_MESHES) {
        mesh_count = MAX_RENDER_OBJECT_MESHES;
    }
    o->mesh_count = mesh_count;
    o->pos[0] = x;
    o->pos[1] = y;
    o->pos[2] = z;
    o->scale[0] = sx;
    o->scale[1] = sy;
    o->scale[2] = sz;
    o->yaw = yaw;
    memcpy(o->color, color, sizeof(o->color));
    o->dynamic = dynamic;

    for (uint8_t i = 0; i < o->mesh_count; ++i) {
        o->entities[i] = nt_entity_create();
        o->meshes[i] = meshes[i];
        nt_transform_comp_add(o->entities[i]);
        nt_mesh_comp_add(o->entities[i]);
        nt_material_comp_add(o->entities[i]);
        nt_drawable_comp_add(o->entities[i]);
        *nt_material_comp_handle(o->entities[i]) = s_mesh_material;
        nt_drawable_comp_set_color(o->entities[i], color[0], color[1], color[2], color[3]);
    }
}
static void create_scene_entities(void) {
    s_object_count = 0;
    add_object(&s_mesh_city_base[0], 1, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 0.0F, (float[4]){0.27F, 0.54F, 0.36F, 1.0F}, false);
    add_object(&s_mesh_city_base[1], 1, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 0.0F, (float[4]){0.18F, 0.19F, 0.21F, 1.0F}, false);
    add_object(&s_mesh_city_base[2], 1, 0.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F, 0.0F, (float[4]){0.22F, 0.23F, 0.25F, 1.0F}, false);
    add_object(&s_mesh_city_base[0], 1, -4.8F, 0.12F, 0.0F, 0.055F, 1.0F, 0.004F, 0.0F, (float[4]){0.95F, 0.88F, 0.56F, 1.0F}, false);
    add_object(&s_mesh_city_base[0], 1, 4.8F, 0.12F, 0.0F, 0.055F, 1.0F, 0.004F, 0.0F, (float[4]){0.95F, 0.88F, 0.56F, 1.0F}, false);
    add_object(s_mesh_low_building, 2, -6.0F, 0.0F, 2.5F, 1.2F, 1.2F, 1.2F, 0.0F, (float[4]){0.14F, 0.70F, 0.66F, 1.0F}, false);
    add_object(s_mesh_large_building, 5, 5.8F, 0.0F, 3.2F, 1.0F, 1.0F, 1.0F, 0.0F, (float[4]){0.88F, 0.42F, 0.28F, 1.0F}, false);
    add_object(s_mesh_low_building, 2, -5.5F, 0.0F, -4.4F, 1.1F, 1.0F, 1.1F, 3.14F, (float[4]){0.92F, 0.76F, 0.42F, 1.0F}, false);
    add_object(s_mesh_low_building, 2, 6.4F, 0.0F, -4.8F, 1.0F, 1.0F, 1.0F, -0.2F, (float[4]){0.42F, 0.60F, 0.96F, 1.0F}, false);
    add_object(s_mesh_large_building, 5, -7.2F, 0.0F, -1.8F, 0.9F, 0.9F, 0.9F, 0.3F, (float[4]){0.70F, 0.44F, 0.86F, 1.0F}, false);
    add_object(s_mesh_street_light, 2, -3.8F, 0.0F, 1.2F, 0.9F, 0.9F, 0.9F, 0.0F, (float[4]){0.95F, 0.92F, 0.78F, 1.0F}, false);
    add_object(s_mesh_street_light, 2, 3.8F, 0.0F, -2.2F, 0.9F, 0.9F, 0.9F, 3.14F, (float[4]){0.95F, 0.92F, 0.78F, 1.0F}, false);
    add_object(s_mesh_package, 2, 3.2F, 0.15F, 0.8F, 0.55F, 0.55F, 0.55F, 0.0F, (float[4]){1.0F, 0.82F, 0.18F, 1.0F}, true);
    add_object(s_mesh_package, 2, -4.0F, 0.15F, -3.4F, 0.65F, 0.65F, 0.65F, 0.0F, (float[4]){0.20F, 0.95F, 0.45F, 1.0F}, true);
    add_object(s_mesh_car, 12, s_game.car_x, 0.0F, s_game.car_z, 0.9F, 0.9F, 0.9F, s_game.car_yaw, (float[4]){1.0F, 0.12F, 0.10F, 1.0F}, true);
    add_object(s_mesh_character, 3, s_game.player_x, 0.0F, s_game.player_z, 0.7F, 0.7F, 0.7F, s_game.player_yaw, (float[4]){0.16F, 0.78F, 0.30F, 1.0F}, true);
    add_object(s_mesh_character, 3, -3.6F, 0.0F, 1.6F, 0.6F, 0.6F, 0.6F, -0.4F, (float[4]){0.96F, 0.62F, 0.78F, 1.0F}, true);
    add_object(s_mesh_character, 3, 1.8F, 0.0F, -2.4F, 0.6F, 0.6F, 0.6F, 1.1F, (float[4]){0.25F, 0.55F, 1.0F, 1.0F}, true);
    add_object(s_mesh_character, 3, s_game.pursuer_x, 0.0F, s_game.pursuer_z, 0.68F, 0.68F, 0.68F, 2.5F, (float[4]){0.98F, 0.18F, 0.16F, 1.0F}, true);
    add_object(&s_mesh_city_base[0], 1, -0.1F, 0.08F, -0.35F, 0.035F, 1.0F, 0.035F, 0.0F, (float[4]){1.0F, 0.93F, 0.10F, 1.0F}, true);
    add_object(&s_mesh_city_base[0], 1, 3.2F, 0.09F, 0.8F, 0.085F, 1.0F, 0.085F, 0.0F, (float[4]){1.0F, 0.93F, 0.10F, 1.0F}, true);
    add_object(&s_mesh_city_base[0], 1, -4.0F, -4.0F, -3.4F, 0.045F, 1.0F, 0.045F, 0.0F, (float[4]){0.22F, 1.0F, 0.42F, 1.0F}, true);
    add_object(&s_mesh_city_base[0], 1, -1.7F, -4.0F, -2.6F, 0.055F, 1.0F, 0.012F, 0.0F, (float[4]){1.0F, 0.12F, 0.06F, 1.0F}, true);
    add_object(&s_mesh_city_base[0], 1, 1.5F, -4.0F, -1.4F, 0.055F, 1.0F, 0.012F, 0.0F, (float[4]){1.0F, 0.12F, 0.06F, 1.0F}, true);
    add_object(s_mesh_car, 12, 8.3F, 0.0F, -4.25F, 0.82F, 0.82F, 0.82F, -0.65F, (float[4]){0.08F, 0.88F, 0.22F, 1.0F}, false);
    add_object(s_mesh_large_building, 5, -9.4F, 0.0F, 4.4F, 1.25F, 1.25F, 1.25F, -0.15F, (float[4]){0.34F, 0.54F, 0.88F, 1.0F}, false); add_object(s_mesh_large_building, 5, 8.8F, 0.0F, 0.2F, 1.15F, 1.15F, 1.15F, 0.25F, (float[4]){0.92F, 0.58F, 0.34F, 1.0F}, false);
    add_object(s_mesh_low_building, 2, -2.6F, 0.0F, 5.6F, 1.0F, 1.0F, 1.0F, 0.1F, (float[4]){0.44F, 0.76F, 0.58F, 1.0F}, false); add_object(s_mesh_low_building, 2, 3.2F, 0.0F, 5.3F, 1.05F, 1.05F, 1.05F, -0.1F, (float[4]){0.86F, 0.72F, 0.36F, 1.0F}, false);
    add_object(s_mesh_car, 12, -2.2F, 0.0F, 2.3F, 0.68F, 0.68F, 0.68F, 1.55F, (float[4]){0.22F, 0.44F, 0.95F, 1.0F}, false); add_object(s_mesh_car, 12, 2.5F, 0.0F, -3.9F, 0.70F, 0.70F, 0.70F, -1.55F, (float[4]){0.96F, 0.78F, 0.18F, 1.0F}, false);
    add_object(s_mesh_street_light, 2, -6.4F, 0.0F, 4.6F, 0.85F, 0.85F, 0.85F, 0.0F, (float[4]){0.96F, 0.90F, 0.66F, 1.0F}, false); add_object(s_mesh_street_light, 2, 6.5F, 0.0F, 4.0F, 0.85F, 0.85F, 0.85F, 3.14F, (float[4]){0.96F, 0.90F, 0.66F, 1.0F}, false);
    add_object(&s_mesh_city_base[0], 1, 0.0F, 0.105F, 2.08F, 0.78F, 1.0F, 0.024F, 0.0F, (float[4]){0.67F, 0.70F, 0.66F, 1.0F}, false); add_object(&s_mesh_city_base[0], 1, 0.0F, 0.105F, -2.08F, 0.78F, 1.0F, 0.024F, 0.0F, (float[4]){0.67F, 0.70F, 0.66F, 1.0F}, false);
    add_object(&s_mesh_city_base[0], 1, -2.78F, 0.106F, 0.0F, 0.024F, 1.0F, 0.58F, 0.0F, (float[4]){0.67F, 0.70F, 0.66F, 1.0F}, false); add_object(&s_mesh_city_base[0], 1, 2.78F, 0.106F, 0.0F, 0.024F, 1.0F, 0.58F, 0.0F, (float[4]){0.67F, 0.70F, 0.66F, 1.0F}, false);
    add_object(&s_mesh_city_base[0], 1, -0.85F, 0.125F, 0.55F, 0.010F, 1.0F, 0.055F, 0.0F, (float[4]){0.92F, 0.94F, 0.86F, 1.0F}, false); add_object(&s_mesh_city_base[0], 1, -0.45F, 0.125F, 0.55F, 0.010F, 1.0F, 0.055F, 0.0F, (float[4]){0.92F, 0.94F, 0.86F, 1.0F}, false); add_object(&s_mesh_city_base[0], 1, -0.05F, 0.125F, 0.55F, 0.010F, 1.0F, 0.055F, 0.0F, (float[4]){0.92F, 0.94F, 0.86F, 1.0F}, false);
    add_object(s_mesh_package, 2, -7.45F, 0.14F, -2.25F, 0.50F, 0.50F, 0.50F, 0.4F, (float[4]){0.95F, 0.78F, 0.22F, 1.0F}, false); add_object(s_mesh_character, 3, -2.1F, 0.0F, 4.7F, 0.55F, 0.55F, 0.55F, 2.4F, (float[4]){0.88F, 0.34F, 0.92F, 1.0F}, false);
}
static void try_enter_car(void) {
    if (dist2(s_game.player_x, s_game.player_z, s_game.car_x, s_game.car_z) > 2.25F) {
        blockside_set_toast(&s_game, "Move closer to the car.");
        return;
    }
    if (!s_game.in_vehicle) {
        s_game.in_vehicle = true;
        s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; s_game.player_yaw = s_game.car_yaw;
        blockside_set_toast(&s_game, "In car. Build speed, brake, and steer.");
        blockside_try_start_repo_drive(&s_game);
    } else {
        s_game.in_vehicle = false;
        s_game.player_x = s_game.car_x - sinf(s_game.car_yaw) * 0.9F; s_game.player_z = s_game.car_z - cosf(s_game.car_yaw) * 0.9F;
        blockside_vehicle_reset(&s_game.vehicle);
        blockside_set_toast(&s_game, "On foot.");
    }
}
static void claim_green_coupe(void) { const bool was = s_game.green_coupe_claimed; blockside_try_green_coupe_entry(&s_game); if (!was && s_game.green_coupe_claimed) { s_game.in_vehicle = true; s_game.car_x = 8.3F; s_game.car_z = -4.25F; s_game.car_yaw = -0.65F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; s_game.player_yaw = s_game.car_yaw; blockside_vehicle_reset(&s_game.vehicle); } }
static void handle_input(float dt) {
    float mx = 0.0F;
    float mz = 0.0F;
    if (nt_input_key_is_down(NT_KEY_W) || nt_input_key_is_down(NT_KEY_ARROW_UP)) {
        mz += 1.0F;
    }
    if (nt_input_key_is_down(NT_KEY_S) || nt_input_key_is_down(NT_KEY_ARROW_DOWN)) {
        mz -= 1.0F;
    }
    if (nt_input_key_is_down(NT_KEY_A) || nt_input_key_is_down(NT_KEY_ARROW_LEFT)) {
        mx -= 1.0F;
    }
    if (nt_input_key_is_down(NT_KEY_D) || nt_input_key_is_down(NT_KEY_ARROW_RIGHT)) {
        mx += 1.0F;
    }

    if (s_game.in_vehicle) {
        BlocksideVehiclePose pose = {.x = s_game.car_x, .z = s_game.car_z, .yaw = s_game.car_yaw};
        blockside_vehicle_step(&s_game.vehicle, &pose, mz > 0.0F ? 1.0F : 0.0F, mz < 0.0F ? 1.0F : 0.0F, mx, dt);
        s_game.car_x = pose.x; s_game.car_z = pose.z; s_game.car_yaw = pose.yaw;
        s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; s_game.player_yaw = s_game.car_yaw;
    }
    const float len = sqrtf(mx * mx + mz * mz);
    if (!s_game.in_vehicle && len > 0.001F) {
        mx /= len;
        mz /= len;
        s_game.player_x += mx * 3.2F * dt;
        s_game.player_z += mz * 3.2F * dt;
        s_game.player_yaw = atan2f(mx, mz);
    }

    if (nt_input_key_is_pressed(NT_KEY_E)) {
        if (s_game.green_coupe_escaped && !s_game.repo_dropoff_call) {
            blockside_try_repo_dropoff_call(&s_game);
        } else if (s_game.green_coupe_approach && !s_game.green_coupe_claimed && dist2(s_game.in_vehicle ? s_game.car_x : s_game.player_x, s_game.in_vehicle ? s_game.car_z : s_game.player_z, 8.3F, -4.25F) < 4.0F) {
            claim_green_coupe();
        } else if (dist2(s_game.player_x, s_game.player_z, s_game.car_x, s_game.car_z) < 2.25F || s_game.in_vehicle) {
            try_enter_car();
        } else {
            blockside_try_green_coupe_approach(&s_game);
            blockside_try_target_handoff(&s_game);
            blockside_try_tail_stop(&s_game);
            blockside_try_tail_pressure(&s_game);
            blockside_try_watch_tail_turn(&s_game);
            blockside_try_start_tail_route(&s_game);
            blockside_try_spot_courier(&s_game);
            blockside_try_start_market_watch(&s_game);
            blockside_try_follow_van_rumor(&s_game);
            blockside_try_open_stash_lead(&s_game);
            blockside_try_repo_score_staging(&s_game); blockside_try_repo_tool_cache(&s_game); blockside_try_repo_crew_pickup(&s_game); blockside_try_repo_next_score_lead(&s_game); blockside_try_repo_final_call(&s_game); blockside_try_repo_safehouse_drop(&s_game); blockside_try_repo_getaway_route(&s_game); blockside_try_repo_meet_intercept(&s_game); blockside_try_repo_heat_watch(&s_game); blockside_try_repo_next_lead(&s_game); blockside_try_repo_payout_meet(&s_game); blockside_try_talk_rita(&s_game);
            blockside_try_pickup_package(&s_game);
            blockside_try_complete_job(&s_game);
        }
    }
    if (nt_input_key_is_pressed(NT_KEY_SPACE)) { blockside_fire_weapon(&s_game); }
}
static void update_game(float dt) { blockside_update_story(&s_game, dt); } static void sync_dynamic_objects(void) {
    if (s_object_count < 24) {
        return;
    }
    RenderObject *package = &s_objects[12];
    package->pos[1] = s_game.package_collected ? -4.0F : 0.15F;
    RenderObject *drop = &s_objects[13];
    drop->color[3] = s_game.package_collected && !s_game.package_delivered ? 1.0F : 0.35F;
    RenderObject *car = &s_objects[14];
    car->pos[0] = s_game.car_x;
    car->pos[2] = s_game.car_z;
    car->yaw = s_game.car_yaw;
    car->color[0] = s_game.green_coupe_claimed ? 0.08F : 1.0F; car->color[1] = s_game.green_coupe_claimed ? 0.88F : 0.12F; car->color[2] = s_game.green_coupe_claimed ? 0.22F : 0.10F;
    RenderObject *player = &s_objects[15];
    player->pos[0] = s_game.player_x;
    player->pos[2] = s_game.player_z;
    player->yaw = s_game.player_yaw;
    player->pos[1] = s_game.in_vehicle ? -4.0F : 0.0F;
    RenderObject *pursuer = &s_objects[18];
    pursuer->pos[0] = s_game.pursuer_x;
    pursuer->pos[2] = s_game.pursuer_z;
    pursuer->color[0] = s_game.pursuer_stunned ? 1.0F : 0.98F; pursuer->color[1] = s_game.pursuer_stunned ? 0.9F : 0.18F; pursuer->color[2] = s_game.pursuer_stunned ? 0.18F : 0.16F;
    RenderObject *route_a = &s_objects[19], *route_b = &s_objects[20], *drop_marker = &s_objects[21];
    const bool show_pickup_route = !s_game.package_collected, show_drop_route = s_game.package_collected && !s_game.package_delivered;
    const bool show_second_route = s_game.second_job_unlocked;
    const bool show_repo_car_route = s_game.repo_intro_active && !s_game.repo_drive_active;
    const bool show_repo_scout_route = s_game.repo_drive_active && !s_game.repo_scout_complete;
    const bool show_stash_hook_route = s_game.repo_scout_complete && !s_game.stash_lead_active;
    const bool show_van_rumor_route = s_game.stash_lead_active && !s_game.van_rumor_active;
    const bool show_market_watch_route = s_game.van_rumor_active && !s_game.market_watch_active;
    const bool show_courier_route = s_game.market_watch_active && !s_game.courier_spotted;
    const bool show_tail_route = s_game.courier_spotted && !s_game.tail_route_active, show_tail_turn = s_game.tail_route_active && !s_game.tail_turn_watch, show_tail_pressure = s_game.tail_turn_watch && !s_game.tail_pressure_active, show_tail_stop = s_game.tail_pressure_active && !s_game.tail_stop_resolved, show_target_handoff = s_game.tail_stop_resolved && !s_game.target_handoff_active, show_green_coupe = s_game.target_handoff_active && !s_game.green_coupe_approach, show_escape = s_game.green_coupe_claimed && !s_game.green_coupe_escaped, show_dropoff = s_game.repo_dropoff_call && !s_game.repo_dropoff_garage, show_payout = s_game.repo_dropoff_garage && !s_game.repo_payout_meet, show_next_lead = s_game.repo_payout_meet && !s_game.repo_next_lead, show_heat_watch = s_game.repo_next_lead && !s_game.repo_heat_watch, show_meet_intercept = s_game.repo_heat_watch && !s_game.repo_meet_intercept, show_getaway_route = s_game.repo_meet_intercept && !s_game.repo_getaway_route, show_safehouse = s_game.repo_getaway_route && !s_game.repo_safehouse_drop, show_final_call = s_game.repo_safehouse_drop && !s_game.repo_final_call, show_next_score = s_game.repo_final_call && !s_game.repo_next_score_lead, show_crew_pickup = s_game.repo_next_score_lead && !s_game.repo_crew_pickup, show_tool_cache = s_game.repo_crew_pickup && !s_game.repo_tool_cache, show_score_staging = s_game.repo_tool_cache && !s_game.repo_score_staging;
    const bool show_drop_marker = show_drop_route || show_second_route || show_repo_car_route || show_repo_scout_route || show_stash_hook_route || show_van_rumor_route || show_market_watch_route || show_courier_route || show_tail_route || show_tail_turn || show_tail_pressure || show_tail_stop || show_target_handoff || show_green_coupe || show_escape || show_dropoff || show_payout || show_next_lead || show_heat_watch || show_meet_intercept || show_getaway_route || show_safehouse || show_final_call || show_next_score || show_crew_pickup || show_tool_cache || show_score_staging;
    route_a->pos[1] = show_pickup_route ? 0.08F : -4.0F; route_b->pos[1] = show_pickup_route ? 0.09F : -4.0F; drop_marker->pos[1] = show_drop_marker ? 0.09F : -4.0F;
    drop_marker->pos[0] = show_score_staging ? -8.2F : (show_tool_cache ? -7.4F : (show_crew_pickup ? -6.2F : (show_next_score ? -4.8F : (show_final_call ? -1.6F : (show_safehouse ? 2.6F : (show_getaway_route ? -7.2F : (show_meet_intercept ? -4.6F : (show_heat_watch ? 3.7F : (show_next_lead ? -1.2F : (show_payout ? 1.8F : (show_dropoff ? -6.8F : (show_escape ? -2.4F : (show_green_coupe ? 7.4F : (show_target_handoff ? 5.2F : (show_tail_stop ? 6.6F : (show_tail_pressure ? 4.8F : (show_tail_turn ? 2.4F : (show_tail_route ? -0.8F : (show_courier_route ? -3.6F : (show_market_watch_route ? 5.8F : (show_van_rumor_route ? 7.2F : (show_stash_hook_route ? 6.4F : (show_repo_scout_route ? 4.6F : (show_repo_car_route ? s_game.car_x : (show_second_route ? 1.8F : -4.0F)))))))))))))))))))))))));
    drop_marker->pos[2] = show_score_staging ? 1.4F : (show_tool_cache ? -2.2F : (show_crew_pickup ? -3.8F : (show_next_score ? -5.6F : (show_final_call ? -5.8F : (show_safehouse ? -5.4F : (show_getaway_route ? -0.6F : (show_meet_intercept ? 2.8F : (show_heat_watch ? 3.6F : (show_next_lead ? 3.4F : (show_payout ? -2.4F : (show_dropoff ? 4.2F : (show_escape ? -5.2F : (show_green_coupe ? -3.6F : (show_target_handoff ? -1.1F : (show_tail_stop ? 0.6F : (show_tail_pressure ? 2.2F : (show_tail_turn ? 4.8F : (show_tail_route ? 3.8F : (show_courier_route ? 1.6F : (show_market_watch_route ? 3.2F : (show_van_rumor_route ? -1.8F : (show_stash_hook_route ? -4.8F : (show_repo_scout_route ? -2.9F : (show_repo_car_route ? s_game.car_z : (show_second_route ? -2.4F : -3.4F)))))))))))))))))))))))));
    drop_marker->color[0] = show_score_staging ? 1.0F : (show_tool_cache ? 0.95F : (show_crew_pickup ? 0.85F : (show_next_score ? 1.0F : (show_final_call ? 0.72F : (show_safehouse ? 0.18F : (show_getaway_route ? 0.35F : (show_meet_intercept ? 1.0F : (show_heat_watch ? 0.95F : (show_next_lead ? 1.0F : (show_payout ? 0.28F : (show_dropoff ? 1.0F : (show_escape ? 0.15F : (show_green_coupe ? 0.12F : (show_target_handoff ? 0.25F : (show_tail_stop ? 0.20F : (show_tail_pressure ? 1.0F : (show_tail_turn ? 0.65F : (show_tail_route ? 0.35F : (show_courier_route ? 0.95F : (show_market_watch_route ? 1.0F : (show_van_rumor_route ? 0.18F : (show_stash_hook_route ? 0.72F : ((show_repo_car_route || show_repo_scout_route) ? 1.0F : (show_second_route ? 0.28F : 0.22F))))))))))))))))))))))));
    drop_marker->color[1] = show_score_staging ? 0.48F : (show_tool_cache ? 0.95F : (show_crew_pickup ? 0.22F : (show_next_score ? 0.85F : (show_final_call ? 0.42F : (show_safehouse ? 1.0F : (show_getaway_route ? 0.95F : (show_meet_intercept ? 0.55F : (show_heat_watch ? 0.18F : (show_next_lead ? 0.35F : (show_payout ? 0.68F : (show_dropoff ? 0.78F : (show_escape ? 0.90F : (show_green_coupe ? 1.0F : (show_target_handoff ? 0.95F : (show_tail_stop ? 1.0F : (show_tail_pressure ? 0.22F : (show_tail_turn ? 1.0F : (show_tail_route ? 0.85F : (show_courier_route ? 0.45F : (show_market_watch_route ? 0.92F : (show_van_rumor_route ? 1.0F : (show_stash_hook_route ? 0.42F : (show_repo_scout_route ? 0.62F : (show_repo_car_route ? 0.36F : (show_second_route ? 0.68F : 1.0F)))))))))))))))))))))))));
    drop_marker->color[2] = show_score_staging ? 0.10F : (show_tool_cache ? 0.25F : (show_crew_pickup ? 0.95F : (show_next_score ? 0.16F : (show_final_call ? 1.0F : (show_safehouse ? 0.38F : (show_getaway_route ? 1.0F : (show_meet_intercept ? 0.12F : (show_heat_watch ? 0.20F : (show_next_lead ? 0.75F : (show_payout ? 1.0F : (show_dropoff ? 0.12F : (show_escape ? 1.0F : (show_green_coupe ? 0.18F : (show_target_handoff ? 0.25F : (show_tail_stop ? 0.55F : (show_tail_pressure ? 0.22F : (show_tail_turn ? 0.35F : (show_tail_route ? 1.0F : (show_courier_route ? 0.86F : (show_market_watch_route ? 0.20F : (show_van_rumor_route ? 0.78F : (show_stash_hook_route ? 1.0F : (show_repo_scout_route ? 0.10F : (show_repo_car_route ? 0.12F : (show_second_route ? 1.0F : 0.42F)))))))))))))))))))))))));
    s_objects[22].pos[1] = s_game.roadblock_active ? 0.13F : -4.0F; s_objects[23].pos[1] = s_objects[22].pos[1];
    if (s_object_count > 24) { s_objects[24].pos[1] = s_game.green_coupe_claimed ? -4.0F : 0.0F; }
} static void update_transform(nt_entity_t entity, const RenderObject *o, uint8_t part) {
    float *pos = nt_transform_comp_position(entity);
    float *scale = nt_transform_comp_scale(entity);
    float *rot = nt_transform_comp_rotation(entity);
    pos[0] = o->pos[0];
    pos[1] = o->pos[1];
    pos[2] = o->pos[2];
    scale[0] = o->scale[0];
    scale[1] = o->scale[1];
    scale[2] = o->scale[2];
    glm_quatv(rot, o->yaw, (vec3){0.0F, 1.0F, 0.0F});
    *nt_transform_comp_dirty(entity) = true;
    float r = o->color[0], g = o->color[1], b = o->color[2];
    if (o->mesh_count == 12 && (part % 3U) == 1U) { r = 0.08F; g = 0.10F; b = 0.12F; } else if (o->mesh_count == 12 && (part % 3U) == 2U) { r *= 0.45F; g *= 0.45F; b *= 0.45F; }
    else if (o->mesh_count == 5 && part > 0U) { r = (part == 1U || part == 3U) ? 0.10F : r * 0.62F; g = (part == 1U || part == 3U) ? 0.18F : g * 0.62F; b = (part == 1U || part == 3U) ? 0.24F : b * 0.62F; } else if (o->mesh_count == 3 && part == 1U) { r = 0.96F; g = 0.76F; b = 0.52F; }
    else if (o->mesh_count > 1 && part == 1U) { r *= 0.42F; g *= 0.42F; b *= 0.42F; }
    nt_drawable_comp_set_color(entity, r, g, b, o->color[3]);
}

static void draw_meshes(void) {
    const nt_material_info_t *mat_info = nt_material_get_info(s_mesh_material);
    if (!mat_info || !mat_info->ready) {
        return;
    }
    nt_render_item_t items[MAX_RENDER_ITEMS];
    uint32_t count = 0;
    for (uint32_t i = 0; i < s_object_count; ++i) {
        RenderObject *o = &s_objects[i];
        for (uint8_t part = 0; part < o->mesh_count; ++part) {
            if (count >= MAX_RENDER_ITEMS || !nt_resource_is_ready(o->meshes[part])) {
                continue;
            }
            nt_entity_t entity = o->entities[part];
            update_transform(entity, o, part);
            const uint32_t mesh_id = nt_resource_get(o->meshes[part]);
            *nt_mesh_comp_handle(entity) = (nt_mesh_t){.id = mesh_id};
            items[count].sort_key = nt_sort_key_opaque(s_mesh_material.id, mesh_id);
            items[count].entity = entity.id;
            items[count].batch_key = nt_batch_key(s_mesh_material.id, mesh_id);
            count++;
        }
    }
    nt_transform_comp_update();
    if (count > 0) {
        nt_sort_by_key(items, count, s_sort_scratch);
        nt_mesh_renderer_draw_list(items, count);
    }
}

static void make_frame_uniforms(float view[16], float proj[16], float cam_x, float cam_y, float cam_z, nt_frame_uniforms_t *uniforms) {
    float vp[16];
    glm_mat4_mul((vec4 *)proj, (vec4 *)view, (vec4 *)vp);
    memset(uniforms, 0, sizeof(*uniforms));
    memcpy(uniforms->view_proj, vp, 64);
    memcpy(uniforms->view, view, 64);
    memcpy(uniforms->proj, proj, 64);
    uniforms->camera_pos[0] = cam_x;
    uniforms->camera_pos[1] = cam_y;
    uniforms->camera_pos[2] = cam_z;
    uniforms->time[0] = (float)nt_time_now();
    uniforms->time[1] = g_nt_app.dt;
    uniforms->resolution[0] = (float)g_nt_window.fb_width;
    uniforms->resolution[1] = (float)g_nt_window.fb_height;
    uniforms->resolution[2] = g_nt_window.fb_width ? 1.0F / (float)g_nt_window.fb_width : 0.0F;
    uniforms->resolution[3] = g_nt_window.fb_height ? 1.0F / (float)g_nt_window.fb_height : 0.0F;
    uniforms->near_far[0] = 0.1F;
    uniforms->near_far[1] = 100.0F;
}

static void frame(void) {
    nt_window_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_update();
    }
#endif
    nt_input_poll();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        const float ui_w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
        const float ui_h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
        register_ui_devapi(ui_w, ui_h);
    }
#endif

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif

    nt_resource_step();
    nt_material_step();
    if (!s_pack_dumped && nt_resource_pack_state(s_pack_id) == NT_PACK_STATE_READY) {
        nt_log_info("Blockside Heat pack ready.");
        s_pack_dumped = true;
    }

    handle_input(g_nt_app.dt);
    update_game(g_nt_app.dt);
    sync_dynamic_objects();

    const float aspect = g_nt_window.fb_height > 0 ? (float)g_nt_window.fb_width / (float)g_nt_window.fb_height : 16.0F / 9.0F;
    const float target_x = s_game.in_vehicle ? s_game.car_x : s_game.player_x;
    const float target_z = s_game.in_vehicle ? s_game.car_z : s_game.player_z;
    const float cam_x = target_x - 4.8F;
    const float cam_y = 4.0F;
    const float cam_z = target_z - 6.8F;

    float view[16];
    float proj[16];
    glm_lookat((vec3){cam_x, cam_y, cam_z}, (vec3){target_x, 0.8F, target_z + 0.6F}, (vec3){0.0F, 1.0F, 0.0F}, (vec4 *)view);
    glm_perspective(glm_rad(58.0F), aspect, 0.1F, 100.0F, (vec4 *)proj);
    nt_frame_uniforms_t uniforms;
    make_frame_uniforms(view, proj, cam_x, cam_y, cam_z, &uniforms);

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        nt_resource_invalidate(NT_ASSET_SHADER_CODE);
        nt_resource_invalidate(NT_ASSET_MESH);
        nt_resource_invalidate(NT_ASSET_TEXTURE);
        nt_resource_invalidate(NT_ASSET_FONT);
        nt_resource_register(nt_hash32_str("__fallback__"), rid("__fallback_checker__"), NT_ASSET_TEXTURE, s_fallback_texture.id);
        s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
            .type = NT_BUFFER_UNIFORM,
            .usage = NT_USAGE_DYNAMIC,
            .size = sizeof(nt_frame_uniforms_t),
            .label = "frame_uniforms",
        });
        nt_mesh_renderer_restore_gpu();
        nt_text_renderer_restore_gpu();
    }

    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.50F, 0.75F, 0.96F, 1.0F}, .clear_depth = 1.0F});
    nt_gfx_update_buffer(s_frame_ubo, &uniforms, sizeof(uniforms));
    nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);
    draw_meshes();
    blockside_draw_hud(&s_game, s_text_material, s_font_resource, s_font, s_frame_ubo);
#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
    (void)blockside_capture_write_pending();
#endif
    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

#if NT_DEVAPI_ENABLED
static cJSON *state_json(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "runtime", "blockside_heat");
    cJSON_AddStringToObject(root, "job_stage", blockside_job_stage_name(s_game.job_stage));
    cJSON_AddBoolToObject(root, "in_vehicle", s_game.in_vehicle);
    cJSON_AddBoolToObject(root, "package_collected", s_game.package_collected);
    cJSON_AddBoolToObject(root, "package_delivered", s_game.package_delivered);
    cJSON_AddBoolToObject(root, "second_job_unlocked", s_game.second_job_unlocked);
    cJSON_AddBoolToObject(root, "repo_intro_active", s_game.repo_intro_active);
    cJSON_AddBoolToObject(root, "repo_drive_active", s_game.repo_drive_active);
    cJSON_AddBoolToObject(root, "repo_scout_complete", s_game.repo_scout_complete);
    cJSON_AddBoolToObject(root, "stash_lead_active", s_game.stash_lead_active);
    cJSON_AddBoolToObject(root, "van_rumor_active", s_game.van_rumor_active);
    cJSON_AddBoolToObject(root, "market_watch_active", s_game.market_watch_active);
    cJSON_AddBoolToObject(root, "courier_spotted", s_game.courier_spotted);
    cJSON_AddBoolToObject(root, "tail_route_active", s_game.tail_route_active); cJSON_AddBoolToObject(root, "tail_turn_watch", s_game.tail_turn_watch); cJSON_AddBoolToObject(root, "tail_pressure_active", s_game.tail_pressure_active); cJSON_AddBoolToObject(root, "tail_stop_resolved", s_game.tail_stop_resolved); cJSON_AddBoolToObject(root, "target_handoff_active", s_game.target_handoff_active); cJSON_AddBoolToObject(root, "green_coupe_approach", s_game.green_coupe_approach); cJSON_AddBoolToObject(root, "green_coupe_claimed", s_game.green_coupe_claimed); cJSON_AddBoolToObject(root, "green_coupe_escaped", s_game.green_coupe_escaped); cJSON_AddBoolToObject(root, "repo_dropoff_call", s_game.repo_dropoff_call); cJSON_AddBoolToObject(root, "repo_dropoff_garage", s_game.repo_dropoff_garage); cJSON_AddBoolToObject(root, "repo_payout_meet", s_game.repo_payout_meet); cJSON_AddBoolToObject(root, "repo_next_lead", s_game.repo_next_lead); cJSON_AddBoolToObject(root, "repo_heat_watch", s_game.repo_heat_watch); cJSON_AddBoolToObject(root, "repo_meet_intercept", s_game.repo_meet_intercept); cJSON_AddBoolToObject(root, "repo_getaway_route", s_game.repo_getaway_route); cJSON_AddBoolToObject(root, "repo_safehouse_drop", s_game.repo_safehouse_drop); cJSON_AddBoolToObject(root, "repo_final_call", s_game.repo_final_call); cJSON_AddBoolToObject(root, "repo_next_score_lead", s_game.repo_next_score_lead); cJSON_AddBoolToObject(root, "repo_crew_pickup", s_game.repo_crew_pickup); cJSON_AddBoolToObject(root, "repo_tool_cache", s_game.repo_tool_cache); cJSON_AddBoolToObject(root, "repo_score_staging", s_game.repo_score_staging);
    cJSON_AddStringToObject(root, "next_job", s_game.repo_score_staging ? "repo_score_target" : (s_game.repo_tool_cache ? "repo_score_staging" : (s_game.repo_crew_pickup ? "repo_tool_cache" : (s_game.repo_next_score_lead ? "repo_crew_pickup" : (s_game.repo_final_call ? "repo_next_score_lead" : (s_game.repo_safehouse_drop ? "repo_final_call" : (s_game.repo_getaway_route ? "repo_safehouse_drop" : (s_game.repo_meet_intercept ? "repo_getaway_route" : (s_game.repo_heat_watch ? "repo_meet_intercept" : (s_game.repo_next_lead ? "repo_heat_watch" : (s_game.repo_payout_meet ? "repo_next_lead" : (s_game.repo_dropoff_garage ? "repo_payout_meet" : (s_game.repo_dropoff_call ? "repo_dropoff_garage" : (s_game.green_coupe_escaped ? "repo_dropoff_call" : (s_game.green_coupe_claimed ? "repo_escape_start" : (s_game.green_coupe_approach ? "repo_green_coupe_entry" : (s_game.target_handoff_active ? "repo_green_coupe" : (s_game.tail_stop_resolved ? "repo_target_handoff" : (s_game.tail_pressure_active ? "repo_tail_stop" : (s_game.tail_turn_watch ? "repo_tail_intercept" : (s_game.tail_route_active ? "repo_tail_turns" : (s_game.courier_spotted ? "repo_tail_route" : (s_game.market_watch_active ? "repo_courier_watch" : (s_game.van_rumor_active ? "repo_market_watch" : (s_game.stash_lead_active ? "repo_van_rumor" : (s_game.repo_scout_complete ? "repo_stash_van" : (s_game.repo_drive_active ? "repo_scout_curb" : (s_game.repo_intro_active ? "repo_red_compact" : (s_game.second_job_unlocked ? "rita_repo_tip" : "none")))))))))))))))))))))))))))));
    cJSON_AddNumberToObject(root, "cash", s_game.cash);
    cJSON_AddNumberToObject(root, "wanted_level", s_game.wanted_level);
    cJSON_AddNumberToObject(root, "player_x", (double)s_game.player_x);
    cJSON_AddNumberToObject(root, "player_z", (double)s_game.player_z);
    cJSON_AddNumberToObject(root, "car_x", (double)s_game.car_x);
    cJSON_AddNumberToObject(root, "car_z", (double)s_game.car_z);
    cJSON_AddNumberToObject(root, "car_yaw", (double)s_game.car_yaw);
    cJSON_AddNumberToObject(root, "car_speed", (double)s_game.vehicle.speed);
    cJSON_AddNumberToObject(root, "pursuer_x", (double)s_game.pursuer_x);
    cJSON_AddNumberToObject(root, "pursuer_z", (double)s_game.pursuer_z);
    cJSON_AddBoolToObject(root, "pursuer_stunned", s_game.pursuer_stunned);
    cJSON_AddBoolToObject(root, "roadblock_active", s_game.roadblock_active);
    cJSON_AddBoolToObject(root, "roadblock_cleared", s_game.roadblock_cleared);
    cJSON_AddNumberToObject(root, "pursuit_grace_timer", (double)s_game.pursuit_grace_timer);
    cJSON_AddStringToObject(root, "toast", s_game.toast);
    return root;
}

static void emit_state(cJSON *result_obj) {
    cJSON_AddItemToObject(result_obj, "state", state_json());
}
static bool ep_game_state(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    emit_state(result_obj);
    return true;
}
static bool ep_game_reset_playtest(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    blockside_reset_game(&s_game);
    emit_state(result_obj);
    return true;
}
static bool ep_game_enter_car(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    try_enter_car();
    emit_state(result_obj);
    return true;
}
static bool ep_game_pickup(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    s_game.player_x = 3.2F;
    s_game.player_z = 0.8F;
    blockside_try_pickup_package(&s_game);
    emit_state(result_obj);
    return true;
}
static bool ep_game_complete(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    if (!s_game.package_collected) {
        s_game.package_collected = true;
        s_game.job_stage = JOB_STAGE_PACKAGE_COLLECTED;
    }
    s_game.player_x = -4.0F;
    s_game.player_z = -3.4F;
    blockside_try_complete_job(&s_game);
    emit_state(result_obj);
    return true;
}
static bool ep_game_talk_rita(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    if (!s_game.second_job_unlocked) {
        if (!s_game.package_collected) {
            s_game.package_collected = true;
            s_game.job_stage = JOB_STAGE_PACKAGE_COLLECTED;
        }
        s_game.player_x = -4.0F;
        s_game.player_z = -3.4F;
        blockside_try_complete_job(&s_game);
    }
    s_game.player_x = 1.8F;
    s_game.player_z = -2.4F;
    blockside_try_talk_rita(&s_game);
    emit_state(result_obj);
    return true;
}

static void prepare_repo_intro(void) {
    if (s_game.repo_intro_active) {
        return;
    }
    if (!s_game.second_job_unlocked) {
        if (!s_game.package_collected) {
            s_game.package_collected = true;
            s_game.job_stage = JOB_STAGE_PACKAGE_COLLECTED;
        }
        s_game.player_x = -4.0F;
        s_game.player_z = -3.4F;
        blockside_try_complete_job(&s_game);
    }
    s_game.player_x = 1.8F;
    s_game.player_z = -2.4F;
    blockside_try_talk_rita(&s_game);
}

static void prepare_repo_drive(void) {
    if (s_game.repo_drive_active) {
        return;
    }
    prepare_repo_intro();
    s_game.player_x = s_game.car_x;
    s_game.player_z = s_game.car_z;
    s_game.in_vehicle = true;
    blockside_try_start_repo_drive(&s_game);
}

static void prepare_stash_lead(void) {
    if (!s_game.repo_scout_complete) { if (!s_game.repo_drive_active) { prepare_repo_drive(); } s_game.car_x = 4.6F; s_game.car_z = -2.9F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; s_game.in_vehicle = true; blockside_try_complete_repo_scout(&s_game); }
    if (!s_game.stash_lead_active) { s_game.in_vehicle = false; s_game.player_x = 6.4F; s_game.player_z = -4.8F; blockside_vehicle_reset(&s_game.vehicle); blockside_try_open_stash_lead(&s_game); }
}

static bool ep_game_enter_repo_car(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params; (void)err; (void)user;
    prepare_repo_intro();
    s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z;
    try_enter_car();
    emit_state(result_obj);
    return true;
}

static bool ep_game_complete_repo_scout(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params; (void)err; (void)user; if (!s_game.repo_drive_active) { prepare_repo_drive(); }
    s_game.car_x = 4.6F; s_game.car_z = -2.9F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z;
    s_game.in_vehicle = true;
    blockside_try_complete_repo_scout(&s_game);
    emit_state(result_obj);
    return true;
}

static bool ep_game_open_stash_lead(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params; (void)err; (void)user; prepare_stash_lead(); emit_state(result_obj); return true;
}

static bool ep_game_follow_van_rumor(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params; (void)err; (void)user;
    prepare_stash_lead();
    s_game.in_vehicle = true; s_game.car_x = 7.2F; s_game.car_z = -1.8F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z;
    blockside_try_follow_van_rumor(&s_game); emit_state(result_obj); return true;
}

static bool ep_game_start_market_watch(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params; (void)err; (void)user; if (!s_game.van_rumor_active) { prepare_stash_lead(); s_game.in_vehicle = true; s_game.car_x = 7.2F; s_game.car_z = -1.8F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_follow_van_rumor(&s_game); }
    s_game.car_x = 5.8F; s_game.car_z = 3.2F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_start_market_watch(&s_game); emit_state(result_obj); return true;
}

static bool ep_game_spot_courier(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params; (void)err; (void)user; if (!s_game.market_watch_active) { if (!s_game.van_rumor_active) { prepare_stash_lead(); s_game.in_vehicle = true; s_game.car_x = 7.2F; s_game.car_z = -1.8F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_follow_van_rumor(&s_game); } s_game.car_x = 5.8F; s_game.car_z = 3.2F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_start_market_watch(&s_game); }
    s_game.car_x = -3.6F; s_game.car_z = 1.6F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_spot_courier(&s_game); emit_state(result_obj); return true;
}

static bool ep_game_start_tail_route(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params; (void)err; (void)user; if (!s_game.courier_spotted) { if (!s_game.market_watch_active) { if (!s_game.van_rumor_active) { prepare_stash_lead(); s_game.in_vehicle = true; s_game.car_x = 7.2F; s_game.car_z = -1.8F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_follow_van_rumor(&s_game); } s_game.car_x = 5.8F; s_game.car_z = 3.2F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_start_market_watch(&s_game); } s_game.car_x = -3.6F; s_game.car_z = 1.6F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_spot_courier(&s_game); }
    s_game.car_x = -0.8F; s_game.car_z = 3.8F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_start_tail_route(&s_game); emit_state(result_obj); return true;
}

static bool ep_game_watch_tail_turn(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.tail_route_active) { cJSON *tmp = cJSON_CreateObject(); ep_game_start_tail_route(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.car_x = 2.4F; s_game.car_z = 4.8F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_watch_tail_turn(&s_game); emit_state(result_obj); return true; } static bool ep_game_tail_pressure(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.tail_turn_watch) { cJSON *tmp = cJSON_CreateObject(); ep_game_watch_tail_turn(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.car_x = 4.8F; s_game.car_z = 2.2F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_tail_pressure(&s_game); emit_state(result_obj); return true; } static bool ep_game_tail_stop(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.tail_pressure_active) { cJSON *tmp = cJSON_CreateObject(); ep_game_tail_pressure(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.car_x = 6.6F; s_game.car_z = 0.6F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_tail_stop(&s_game); emit_state(result_obj); return true; }
static bool ep_game_target_handoff(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.tail_stop_resolved) { cJSON *tmp = cJSON_CreateObject(); ep_game_tail_stop(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.car_x = 5.2F; s_game.car_z = -1.1F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_target_handoff(&s_game); emit_state(result_obj); return true; } static bool ep_game_green_coupe_approach(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.target_handoff_active) { cJSON *tmp = cJSON_CreateObject(); ep_game_target_handoff(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.car_x = 7.4F; s_game.car_z = -3.6F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_green_coupe_approach(&s_game); emit_state(result_obj); return true; } static bool ep_game_green_coupe_entry(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.green_coupe_approach) { cJSON *tmp = cJSON_CreateObject(); ep_game_green_coupe_approach(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.car_x = 8.3F; s_game.car_z = -4.25F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; claim_green_coupe(); emit_state(result_obj); return true; } static bool ep_game_green_coupe_escape(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.green_coupe_claimed) { cJSON *tmp = cJSON_CreateObject(); ep_game_green_coupe_entry(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.car_x = -2.4F; s_game.car_z = -5.2F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_green_coupe_escape(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_dropoff_call(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.green_coupe_escaped) { cJSON *tmp = cJSON_CreateObject(); ep_game_green_coupe_escape(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } blockside_try_repo_dropoff_call(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_dropoff_garage(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_dropoff_call) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_dropoff_call(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.car_x = -6.8F; s_game.car_z = 4.2F; s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; blockside_try_repo_dropoff_garage(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_payout_meet(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_dropoff_garage) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_dropoff_garage(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.in_vehicle = false; s_game.player_x = 1.8F; s_game.player_z = -2.4F; blockside_try_repo_payout_meet(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_next_lead(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_payout_meet) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_payout_meet(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = -1.2F; s_game.player_z = 3.4F; blockside_try_repo_next_lead(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_heat_watch(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_next_lead) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_next_lead(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = 3.7F; s_game.player_z = 3.6F; blockside_try_repo_heat_watch(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_meet_intercept(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_heat_watch) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_heat_watch(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = -4.6F; s_game.player_z = 2.8F; blockside_try_repo_meet_intercept(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_getaway_route(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_meet_intercept) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_meet_intercept(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = -7.2F; s_game.player_z = -0.6F; blockside_try_repo_getaway_route(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_safehouse_drop(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_getaway_route) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_getaway_route(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = 2.6F; s_game.player_z = -5.4F; blockside_try_repo_safehouse_drop(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_final_call(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_safehouse_drop) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_safehouse_drop(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = -1.6F; s_game.player_z = -5.8F; blockside_try_repo_final_call(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_next_score_lead(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_final_call) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_final_call(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = -4.8F; s_game.player_z = -5.6F; blockside_try_repo_next_score_lead(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_crew_pickup(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_next_score_lead) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_next_score_lead(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = -6.2F; s_game.player_z = -3.8F; blockside_try_repo_crew_pickup(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_tool_cache(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_crew_pickup) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_crew_pickup(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = -7.4F; s_game.player_z = -2.2F; blockside_try_repo_tool_cache(&s_game); emit_state(result_obj); return true; } static bool ep_game_repo_score_staging(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; if (!s_game.repo_tool_cache) { cJSON *tmp = cJSON_CreateObject(); ep_game_repo_tool_cache(NULL, tmp, NULL, NULL); cJSON_Delete(tmp); } s_game.player_x = -8.2F; s_game.player_z = 1.4F; blockside_try_repo_score_staging(&s_game); emit_state(result_obj); return true; } static bool ep_game_fire(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) { (void)params; (void)err; (void)user; blockside_fire_weapon(&s_game); emit_state(result_obj); return true; }

static bool ep_game_drive_probe(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params; (void)err; (void)user;
    if (!s_game.in_vehicle) { s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; try_enter_car(); }
    for (uint32_t i = 0; i < 90; ++i) {
        BlocksideVehiclePose pose = {.x = s_game.car_x, .z = s_game.car_z, .yaw = s_game.car_yaw};
        blockside_vehicle_step(&s_game.vehicle, &pose, 1.0F, i > 68 ? 1.0F : 0.0F, 0.42F, 1.0F / 60.0F);
        s_game.car_x = pose.x; s_game.car_z = pose.z; s_game.car_yaw = pose.yaw;
    }
    s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; s_game.player_yaw = s_game.car_yaw;
    blockside_set_toast(&s_game, "Car has acceleration, braking, and steering.");
    emit_state(result_obj);
    return true;
}

static bool ep_game_drive_package_route(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params; (void)err; (void)user;
    if (!s_game.in_vehicle) { s_game.player_x = s_game.car_x; s_game.player_z = s_game.car_z; try_enter_car(); }
    BlocksideVehiclePose pose = {.x = s_game.car_x, .z = s_game.car_z, .yaw = s_game.car_yaw};
    blockside_vehicle_drive_toward(&s_game.vehicle, &pose, 3.2F, 0.8F, 220, 1.0F / 60.0F);
    pose.x = 3.2F; pose.z = 0.8F; s_game.car_x = pose.x; s_game.car_z = pose.z; s_game.car_yaw = pose.yaw; s_game.player_x = pose.x; s_game.player_z = pose.z; blockside_try_pickup_package(&s_game);
    blockside_vehicle_drive_toward(&s_game.vehicle, &pose, -4.0F, -3.4F, 260, 1.0F / 60.0F);
    pose.x = -4.0F; pose.z = -3.4F; s_game.car_x = pose.x; s_game.car_z = pose.z; s_game.car_yaw = pose.yaw; s_game.player_x = pose.x; s_game.player_z = pose.z; blockside_try_complete_job(&s_game);
    blockside_set_toast(&s_game, "Route complete. Rita repo tip unlocked.");
    emit_state(result_obj);
    return true;
}

static bool ep_game_capture_framebuffer(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
#if !defined(NT_PLATFORM_WEB)
    const cJSON *output = params ? cJSON_GetObjectItemCaseSensitive(params, "output") : NULL;
    if (!cJSON_IsString(output) || !output->valuestring || output->valuestring[0] == '\0') {
        if (err) {
            err->code = "bad_params";
            err->message = "game.capture.framebuffer requires {output:string}";
        }
        return false;
    }
    if (!blockside_capture_request(output->valuestring)) {
        if (err) {
            err->code = "bad_params";
            err->message = "output path is too long";
        }
        return false;
    }

    cJSON_AddStringToObject(result_obj, "output", output->valuestring);
    cJSON_AddNumberToObject(result_obj, "fb_width", (double)g_nt_window.fb_width);
    cJSON_AddNumberToObject(result_obj, "fb_height", (double)g_nt_window.fb_height);
    cJSON_AddBoolToObject(result_obj, "pending", true);
    return true;
#else
    (void)params;
    (void)result_obj;
    if (err) {
        err->code = "unsupported";
        err->message = "framebuffer capture is native-dev only";
    }
    return false;
#endif
}

static void register_game_endpoints(void) {
    static const nt_devapi_command_desc descs[] = {
        {"game.state", "game", "Return Blockside Heat runtime state.", "", "{state}", "immediate", "none"},
        {"game.reset_playtest", "game", "Reset Pickup Run state.", "", "{state}", "immediate", "resets state"},
        {"game.action.enter_car", "game", "Toggle car entry when near the car.", "", "{state}", "next-frame", "mutates in_vehicle"},
        {"game.action.pickup_package", "game", "Move to and pick up package for automation.", "", "{state}", "next-frame", "mutates job stage"},
        {"game.action.complete_job", "game", "Move to drop zone and complete job for automation.", "", "{state}", "next-frame", "mutates cash/wanted/job"},
        {"game.action.talk_rita", "game", "Move to Rita after the first route and start the repo intro.", "", "{state}", "next-frame", "mutates story/job"},
        {"game.action.enter_repo_car", "game", "Enter the red compact after Rita and start the repo drive objective.", "", "{state}", "next-frame", "mutates vehicle/story/job"},
        {"game.action.complete_repo_scout", "game", "Drive to the orange curb marker and complete the repo scout beat.", "", "{state}", "next-frame", "mutates vehicle/story/cash"},
        {"game.action.open_stash_lead", "game", "Move to the purple block stash marker and open the next repo lead.", "", "{state}", "next-frame", "mutates story/job"},
        {"game.action.follow_van_rumor", "game", "Follow the east van-rumor marker and confirm the market watch lead.", "", "{state}", "next-frame", "mutates vehicle/story/cash"},
        {"game.action.start_market_watch", "game", "Move to the market watch point and start the courier stakeout.", "", "{state}", "next-frame", "mutates vehicle/story/wanted"},
        {"game.action.spot_courier", "game", "Move to the courier cue and spot the next repo tail target.", "", "{state}", "next-frame", "mutates vehicle/story"},
        {"game.action.start_tail_route", "game", "Drive to the first tail marker and begin safe-distance tailing.", "", "{state}", "next-frame", "mutates vehicle/story"},
        {"game.action.watch_tail_turn", "game", "Drive to the turn marker and confirm the courier turn.", "", "{state}", "next-frame", "mutates vehicle/story/cash"},
        {"game.action.tail_pressure", "game", "Drive to the pressure marker and escalate the courier tail.", "", "{state}", "next-frame", "mutates vehicle/story/wanted"},
        {"game.action.tail_stop", "game", "Drive to the stop marker and resolve the courier tail.", "", "{state}", "next-frame", "mutates vehicle/story/cash"},
        {"game.action.target_handoff", "game", "Drive to the stopped-courier handoff and reveal the repo target.", "", "{state}", "next-frame", "mutates vehicle/story/cash"},
        {"game.action.green_coupe_approach", "game", "Drive to the green coupe approach marker and spot the target car.", "", "{state}", "next-frame", "mutates vehicle/story"},
        {"game.action.green_coupe_entry", "game", "Claim the visible green coupe and enter the target car.", "", "{state}", "next-frame", "mutates vehicle/story/cash"},
        {"game.action.green_coupe_escape", "game", "Drive the claimed green coupe to the lose-heat marker.", "", "{state}", "next-frame", "mutates vehicle/story/wanted/cash"},
        {"game.action.repo_dropoff_call", "game", "Trigger Rita's drop-off call after losing heat.", "", "{state}", "next-frame", "mutates story"},
        {"game.action.repo_dropoff_garage", "game", "Drive the claimed car to the north garage drop-off.", "", "{state}", "next-frame", "mutates vehicle/story/cash"},
        {"game.action.repo_payout_meet", "game", "Meet Rita after the garage drop-off and collect the repo payout.", "", "{state}", "next-frame", "mutates story/cash"}, {"game.action.repo_next_lead", "game", "Follow Rita's post-payout lead to the next street marker.", "", "{state}", "next-frame", "mutates story/wanted"}, {"game.action.repo_heat_watch", "game", "Start the heat watch after finding the post-payout lead.", "", "{state}", "next-frame", "mutates story/wanted"}, {"game.action.repo_meet_intercept", "game", "Intercept the watched meet and reveal the getaway route.", "", "{state}", "next-frame", "mutates story/cash"}, {"game.action.repo_getaway_route", "game", "Reach the getaway route after the meet intercept.", "", "{state}", "next-frame", "mutates story/cash/wanted"}, {"game.action.repo_safehouse_drop", "game", "Reach the safehouse drop after finding the getaway route.", "", "{state}", "next-frame", "mutates story/cash/wanted"}, {"game.action.repo_final_call", "game", "Reach Rita's final call marker after the safehouse drop.", "", "{state}", "next-frame", "mutates story/cash"}, {"game.action.repo_next_score_lead", "game", "Reach the bigger-score lead marker after Rita's final call.", "", "{state}", "next-frame", "mutates story/cash"}, {"game.action.repo_crew_pickup", "game", "Reach the crew pickup marker after finding the bigger-score lead.", "", "{state}", "next-frame", "mutates story/cash"}, {"game.action.repo_tool_cache", "game", "Reach the tool cache marker after picking up the crew.", "", "{state}", "next-frame", "mutates story/cash"}, {"game.action.repo_score_staging", "game", "Reach the score-staging marker after finding the tool cache.", "", "{state}", "next-frame", "mutates story/cash"},
        {"game.action.fire", "game", "Use the starter toy weapon.", "", "{state}", "next-frame", "mutates weapon/pursuer"},
        {"game.action.drive_probe", "game", "Run a short deterministic car acceleration/brake/turn probe.", "", "{state}", "next-frame", "mutates vehicle pose/speed"},
        {"game.action.drive_package_route", "game", "Complete the package route through deterministic vehicle physics.", "", "{state}", "next-frame", "mutates vehicle/job/cash"},
        {"game.capture.framebuffer", "game", "Write the next rendered native backbuffer to a P6 PPM file.", "{output:string}", "{output,fb_width,fb_height,pending}", "next-frame", "writes screenshot evidence file"},
    };
    game_devapi_ui_register();
    for (uint32_t i = 0; i < (uint32_t)(sizeof(descs) / sizeof(descs[0])); ++i) {
        nt_devapi_handler_fn handlers[] = {ep_game_state, ep_game_reset_playtest, ep_game_enter_car, ep_game_pickup, ep_game_complete, ep_game_talk_rita, ep_game_enter_repo_car, ep_game_complete_repo_scout, ep_game_open_stash_lead, ep_game_follow_van_rumor, ep_game_start_market_watch, ep_game_spot_courier, ep_game_start_tail_route, ep_game_watch_tail_turn, ep_game_tail_pressure, ep_game_tail_stop, ep_game_target_handoff, ep_game_green_coupe_approach, ep_game_green_coupe_entry, ep_game_green_coupe_escape, ep_game_repo_dropoff_call, ep_game_repo_dropoff_garage, ep_game_repo_payout_meet, ep_game_repo_next_lead, ep_game_repo_heat_watch, ep_game_repo_meet_intercept, ep_game_repo_getaway_route, ep_game_repo_safehouse_drop, ep_game_repo_final_call, ep_game_repo_next_score_lead, ep_game_repo_crew_pickup, ep_game_repo_tool_cache, ep_game_repo_score_staging, ep_game_fire, ep_game_drive_probe, ep_game_drive_package_route, ep_game_capture_framebuffer};
        (void)nt_devapi_register(&descs[i], handlers[i], NULL);
    }
}

static void register_ui_devapi(float w, float h) {
    game_devapi_ui_clear();
    (void)game_devapi_ui_register_node("root", "", "screen", "Blockside Heat", "Pickup Run", 0.0F, 0.0F, w, h, true, true);
    const char *job_text = s_game.repo_score_staging ? "JOB: SCORE STAGED" : (s_game.repo_tool_cache ? "JOB: TOOL CACHE" : (s_game.repo_crew_pickup ? "JOB: CREW PICKUP" : (s_game.repo_next_score_lead ? "JOB: BIG SCORE" : (s_game.repo_final_call ? "JOB: RITA CALL" : (s_game.repo_safehouse_drop ? "JOB: SAFEHOUSE" : (s_game.repo_getaway_route ? "JOB: GETAWAY" : (s_game.repo_meet_intercept ? "JOB: MEET HIT" : (s_game.repo_heat_watch ? "JOB: HEAT WATCH" : (s_game.repo_next_lead ? "JOB: NEW LEAD" : (s_game.repo_payout_meet ? "JOB: PAYOUT DONE" : (s_game.repo_dropoff_garage ? "JOB: GARAGE DONE" : (s_game.repo_dropoff_call ? "JOB: DROPOFF CALL" : (s_game.green_coupe_escaped ? "JOB: HEAT LOST" : (s_game.green_coupe_claimed ? "JOB: COUPE CLAIMED" : (s_game.green_coupe_approach ? "JOB: GREEN COUPE" : (s_game.target_handoff_active ? "JOB: TARGET HANDOFF" : (s_game.tail_stop_resolved ? "JOB: COURIER STOPPED" : (s_game.tail_pressure_active ? "JOB: TAIL PRESSURE" : (s_game.tail_turn_watch ? "JOB: TURN WATCH" : (s_game.tail_route_active ? "JOB: SAFE DISTANCE" : (s_game.courier_spotted ? "JOB: TAIL ROUTE" : (s_game.market_watch_active ? "JOB: COURIER WATCH" : (s_game.van_rumor_active ? "JOB: MARKET WATCH" : (s_game.stash_lead_active ? "JOB: VAN RUMOR" : (s_game.repo_scout_complete ? "JOB: STASH SPOTTED" : (s_game.repo_drive_active ? "JOB: CURB SCOUT" : (s_game.repo_intro_active ? "JOB: RED COMPACT" : (s_game.second_job_unlocked ? "JOB: RITA REPO" : "JOB: PICKUP"))))))))))))))))))))))))))));
    const char *action_text = s_game.in_vehicle ? "Exit car" : ((s_game.repo_dropoff_garage && !s_game.repo_payout_meet) || (s_game.second_job_unlocked && !s_game.repo_intro_active) ? "Talk to Rita" : "Enter car / action");
    (void)game_devapi_ui_register_node("hud.job", "root", "label", "Job", job_text, 24.0F, h - 88.0F, 260.0F, 72.0F, true, false);
    (void)game_devapi_ui_register_node("hud.cash", "root", "label", "Cash", "CASH", w - 230.0F, h - 64.0F, 190.0F, 40.0F, true, false);
    (void)game_devapi_ui_register_node("action.primary", "root", "button", "Primary Action", action_text, 470.0F, 62.0F, 260.0F, 48.0F, true, true);
}
#endif

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Blockside Heat";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    blockside_parse_args(argc, argv, &s_devapi_enabled, &s_devapi_port, &s_window_width, &s_window_height);
    blockside_reset_game(&s_game);

    g_nt_window.title = "Blockside Heat";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    gfx_desc.max_buffers = 512;
    gfx_desc.max_meshes = 256;
    nt_gfx_init(&gfx_desc);
    nt_gfx_register_global_block("Globals", 0);

    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture);
    nt_resource_set_activator(NT_ASSET_MESH, nt_gfx_activate_mesh, nt_gfx_deactivate_mesh);
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);

    nt_font_init(&(nt_font_desc_t){.max_fonts = 2});
    nt_material_init(&(nt_material_desc_t){.max_materials = 8});
    nt_entity_init(&(nt_entity_desc_t){.max_entities = 160});
    nt_transform_comp_init(&(nt_transform_comp_desc_t){.capacity = 160});
    nt_mesh_comp_init(&(nt_mesh_comp_desc_t){.capacity = 160});
    nt_material_comp_init(&(nt_material_comp_desc_t){.capacity = 160});
    nt_drawable_comp_init(&(nt_drawable_comp_desc_t){.capacity = 160});
    nt_mesh_renderer_init(&(nt_mesh_renderer_desc_t){.max_instances = 256, .max_pipelines = 16});
    nt_text_renderer_init();

    s_pack_id = nt_hash32_str("blockside_heat");
    nt_resource_mount(s_pack_id, 100);
    nt_resource_load_auto(s_pack_id, GAME_ASSET_PACK_PATH);
    nt_resource_set_activate_time_budget(0);

    for (uint32_t i = 0; i < 3; ++i) {
        s_mesh_city_base[i] = mesh_request_part("blockside/city_base", i, 0);
    }
    for (uint32_t i = 0; i < 2; ++i) {
        s_mesh_low_building[i] = mesh_request_part("blockside/low_building", 0, i);
        s_mesh_street_light[i] = mesh_request_part("blockside/street_light", 0, i);
        s_mesh_package[i] = mesh_request_part("blockside/package", 0, i);
    }
    for (uint32_t i = 0; i < 5; ++i) {
        s_mesh_large_building[i] = mesh_request_part("blockside/large_building", 0, i);
    }
    for (uint32_t i = 0; i < 6; ++i) {
        s_mesh_car[i] = mesh_request_part("blockside/car", 0, i);
    }
    s_mesh_car[6] = mesh_request_part("blockside/car", 1, 0);
    s_mesh_car[7] = mesh_request_part("blockside/car", 1, 1);
    s_mesh_car[8] = mesh_request_part("blockside/car", 2, 0);
    s_mesh_car[9] = mesh_request_part("blockside/car", 2, 1);
    s_mesh_car[10] = mesh_request_part("blockside/car", 3, 0);
    s_mesh_car[11] = mesh_request_part("blockside/car", 3, 1);
    for (uint32_t i = 0; i < 3; ++i) {
        s_mesh_character[i] = mesh_request_part("blockside/character", 0, i);
    }
    s_mesh_vs = nt_resource_request(rid("assets/shaders/blockside_mesh_inst.vert"), NT_ASSET_SHADER_CODE);
    s_mesh_fs = nt_resource_request(rid("assets/shaders/blockside_mesh_inst.frag"), NT_ASSET_SHADER_CODE);
    s_text_vs = nt_resource_request(rid("assets/shaders/slug_text.vert"), NT_ASSET_SHADER_CODE);
    s_text_fs = nt_resource_request(rid("assets/shaders/slug_text.frag"), NT_ASSET_SHADER_CODE);
    s_font_resource = nt_resource_request(rid("blockside/font"), NT_ASSET_FONT);

    s_mesh_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_mesh_vs,
        .fs = s_mesh_fs,
        .attr_map = {{.stream_name = "position", .location = 0}},
        .attr_map_count = 1,
        .depth_test = true,
        .depth_write = true,
        .cull_mode = NT_CULL_BACK,
        .color_mode = NT_COLOR_MODE_FLOAT4,
        .label = "blockside_mesh",
    });
    s_text_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_text_vs,
        .fs = s_text_fs,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .params[0] = {.name = "u_alpha_cutoff", .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
        .param_count = 1,
        .label = "blockside_text",
    });
    s_font = nt_font_create(&(nt_font_create_desc_t){
        .curve_texture_width = 1024,
        .curve_texture_height = 512,
        .band_texture_height = 256,
        .band_count = 8,
        .measure_cache_size = 256,
    });
    nt_font_add(s_font, s_font_resource);

    create_scene_entities();

    s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "frame_uniforms",
    });
    s_fallback_texture = nt_gfx_make_texture(&(nt_texture_desc_t){
        .width = 4,
        .height = 4,
        .data = s_checker_4x4,
        .min_filter = NT_FILTER_NEAREST,
        .mag_filter = NT_FILTER_NEAREST,
        .wrap_u = NT_WRAP_REPEAT,
        .wrap_v = NT_WRAP_REPEAT,
        .label = "fallback_checker",
    });
    nt_resource_create_pack(nt_hash32_str("__fallback__"), 0);
    nt_resource_register(nt_hash32_str("__fallback__"), rid("__fallback_checker__"), NT_ASSET_TEXTURE, s_fallback_texture.id);
    nt_resource_set_placeholder_texture(rid("__fallback_checker__"));

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
    nt_text_renderer_shutdown();
    nt_font_destroy(s_font);
    nt_font_shutdown();
    nt_mesh_renderer_shutdown();
    nt_drawable_comp_shutdown();
    nt_material_comp_shutdown();
    nt_mesh_comp_shutdown();
    nt_transform_comp_shutdown();
    nt_entity_shutdown();
    nt_material_destroy(s_mesh_material);
    nt_material_destroy(s_text_material);
    nt_material_shutdown();
    nt_resource_shutdown();
    nt_fs_shutdown();
    nt_http_shutdown();
    nt_hash_shutdown();
    nt_gfx_destroy_buffer(s_frame_ubo);
    nt_gfx_destroy_texture(s_fallback_texture);
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif

    return 0;
}
