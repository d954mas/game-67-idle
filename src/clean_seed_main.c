#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#if NT_DEVAPI_ENABLED
#include "devapi/nt_devapi.h"
#include "devapi/nt_devapi_net.h"
#include "game_devapi_ui.h"
#endif
#include "drawable_comp/nt_drawable_comp.h"
#include "entity/nt_entity.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "material/nt_material.h"
#include "material_comp/nt_material_comp.h"
#include "math/nt_math.h"
#include "mesh_comp/nt_mesh_comp.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "render/nt_render_items.h"
#include "renderers/nt_mesh_renderer.h"
#include "renderers/nt_shape_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "time/nt_time.h"
#include "transform_comp/nt_transform_comp.h"
#include "window/nt_window.h"

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NT_PLATFORM_WEB
#include <glad/gl.h>
#endif

#define CLEAN_SEED_DEVAPI_PORT_DEFAULT 9123
#define ARENA_HALF 8.0F
#define MAX_DRONES 8
#define ROCKET_COST 120
#define CAPTURE_PATH_MAX 512
#define MECH_MESH_PARTS 51
#define ROBOT_ENEMY_MESH_PARTS 7
#define ASSAULT_WALKER_MESH_PARTS 13
#define SENTINEL_SHOWCASE_MESH_PARTS 10
#define KENNEY_WORLD_PROP_PARTS 3
#define KENNEY_WORLD_PROP_INSTANCES 8
#define MECH_MESH_ENTITY_CAPACITY 160
#define MECH_MESH_TYPES 50
#define MECH_MESH_RENDER_ITEMS (1 + MECH_MESH_PARTS + ASSAULT_WALKER_MESH_PARTS + SENTINEL_SHOWCASE_MESH_PARTS + KENNEY_WORLD_PROP_INSTANCES + (MAX_DRONES * ROBOT_ENEMY_MESH_PARTS))
#define MECH_PART_ROCKETS_ONLY 0x01U

typedef enum {
  SCREEN_HANGAR = 0,
  SCREEN_BATTLE,
  SCREEN_REWARD,
  SCREEN_UPGRADE,
  SCREEN_RETEST,
} GameScreen;

typedef struct UiBox {
  float x;
  float y;
  float w;
  float h;
} UiBox;

typedef struct Drone {
  float x;
  float z;
  float hp;
  bool alive;
} Drone;

typedef struct MechGame {
  GameScreen screen;
  float mech_x;
  float mech_z;
  float heat;
  float cannon_cd;
  float rocket_cd;
  float dash_cd;
  float dash_flash;
  float rocket_flash;
  float hit_flash;
  float battle_time;
  float mech_vx;
  float mech_vz;
  float mech_facing;
  float mech_walk;
  float mech_recoil;
  int salvage;
  int battle_index;
  bool rockets_equipped;
  bool reward_ready;
  bool second_prompt_seen;
  Drone drones[MAX_DRONES];
} MechGame;

typedef enum {
  MECH_MESH_TORSO = 0,
  MECH_MESH_PELVIS,
  MECH_MESH_HEAD,
  MECH_MESH_SHOULDER,
  MECH_MESH_LIMB,
  MECH_MESH_FOREARM,
  MECH_MESH_WEAPON,
  MECH_MESH_FOOT,
  MECH_MESH_ROCKET_POD,
  MECH_MESH_ROCKET_TUBE,
  MECH_MESH_VENT,
  MECH_MESH_HYDRAULIC,
  MECH_MESH_JOINT,
  MECH_MESH_ARMOR_PLATE,
  MECH_MESH_VISOR,
  MECH_MESH_QUATERNIUS,
  MECH_MESH_ROBOT_ENEMY_MAIN2,
  MECH_MESH_ROBOT_ENEMY_MAIN,
  MECH_MESH_ROBOT_ENEMY_EDGE,
  MECH_MESH_ROBOT_ENEMY_DARK,
  MECH_MESH_ROBOT_ENEMY_EYE,
  MECH_MESH_ROBOT_ENEMY_GREY,
  MECH_MESH_ROBOT_ENEMY_LIGHTGREY,
  MECH_MESH_ASSAULT_WALKER_GREEN,
  MECH_MESH_ASSAULT_WALKER_GREEN_AO,
  MECH_MESH_ASSAULT_WALKER_GREEN_UV,
  MECH_MESH_ASSAULT_WALKER_GREY_A,
  MECH_MESH_ASSAULT_WALKER_GLASS,
  MECH_MESH_ASSAULT_WALKER_BLACK,
  MECH_MESH_ASSAULT_WALKER_BLACK_UV,
  MECH_MESH_ASSAULT_WALKER_GREY_B,
  MECH_MESH_ASSAULT_WALKER_DARK,
  MECH_MESH_ASSAULT_WALKER_GREY_C,
  MECH_MESH_ASSAULT_WALKER_GREY_C_NONE,
  MECH_MESH_ASSAULT_WALKER_GREY_D,
  MECH_MESH_ASSAULT_WALKER_GREY_D_NONE,
  MECH_MESH_WORLD_STUDS_FLOOR,
  MECH_MESH_SENTINEL_MAT18,
  MECH_MESH_SENTINEL_MAT16,
  MECH_MESH_SENTINEL_MAT9,
  MECH_MESH_SENTINEL_MAT17,
  MECH_MESH_SENTINEL_MAT13,
  MECH_MESH_SENTINEL_MAT21,
  MECH_MESH_SENTINEL_MAT20,
  MECH_MESH_SENTINEL_MAT15,
  MECH_MESH_SENTINEL_MAT12,
  MECH_MESH_SENTINEL_MAT23,
  MECH_MESH_KENNEY_SPACE_GATE,
  MECH_MESH_KENNEY_SPACE_CORRIDOR_WIDE,
  MECH_MESH_KENNEY_SPACE_ROOM_SMALL,
} MeshPartMesh;

typedef struct MeshPartSpec {
  float pos[3];
  float size[3];
  float color[4];
  uint8_t flags;
  uint8_t mesh_kind;
} MeshPartSpec;

typedef struct MeshMechRuntime {
  nt_hash32_t pack_id;
  nt_resource_t meshes[MECH_MESH_TYPES];
  nt_resource_t vs;
  nt_resource_t fs;
  nt_resource_t robot_fs;
  nt_resource_t solid_vs;
  nt_resource_t solid_fs;
  nt_resource_t station_fs;
  nt_resource_t text_vs;
  nt_resource_t text_fs;
  nt_resource_t mech_atlas;
  nt_resource_t world_texture;
  nt_resource_t ui_font_res;
  nt_material_t material;
  nt_material_t world_material;
  nt_material_t robot_material;
  nt_material_t solid_material;
  nt_material_t station_material;
  nt_material_t text_material;
  nt_font_t ui_font;
  nt_entity_t world_floor;
  nt_entity_t parts[MECH_MESH_PARTS];
  nt_entity_t assault_hero[ASSAULT_WALKER_MESH_PARTS];
  nt_entity_t sentinel_showcase[SENTINEL_SHOWCASE_MESH_PARTS];
  nt_entity_t kenney_world_props[KENNEY_WORLD_PROP_INSTANCES];
  nt_entity_t enemy_robots[MAX_DRONES][ROBOT_ENEMY_MESH_PARTS];
  nt_render_item_t items[MECH_MESH_RENDER_ITEMS];
  nt_render_item_t sort_scratch[MECH_MESH_RENDER_ITEMS];
  nt_buffer_t frame_ubo;
  bool initialized;
  bool pack_logged;
} MeshMechRuntime;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = CLEAN_SEED_DEVAPI_PORT_DEFAULT;
static int s_window_width = 1280;
static int s_window_height = 720;
static MechGame s_game;
static MeshMechRuntime s_mesh_mech;
static UiBox s_primary_box;
static UiBox s_secondary_box;
static UiBox s_dash_box;
static UiBox s_special_box;
#if NT_DEVAPI_ENABLED
static char s_pending_capture_path[CAPTURE_PATH_MAX];
#endif

static const float COL_BG_FLOOR[4] = {0.38F, 0.78F, 0.35F, 1.0F};
static const float COL_METAL[4] = {0.76F, 0.84F, 0.90F, 1.0F};
static const float COL_METAL_DARK[4] = {0.22F, 0.32F, 0.40F, 1.0F};
static const float COL_METAL_HI[4] = {0.88F, 0.92F, 0.96F, 1.0F};
static const float COL_ARMOR_DEEP[4] = {0.06F, 0.24F, 0.76F, 1.0F};
static const float COL_ARMOR_BLUE[4] = {0.0F, 0.45F, 1.0F, 1.0F};
static const float COL_ARMOR_LIGHT[4] = {0.0F, 0.86F, 1.0F, 1.0F};
static const float COL_EMISSIVE[4] = {0.0F, 0.95F, 1.0F, 1.0F};
static const float COL_AMBER[4] = {1.0F, 0.48F, 0.05F, 1.0F};
static const float COL_WARNING[4] = {1.0F, 0.18F, 0.08F, 1.0F};
static const float COL_MAGENTA[4] = {0.86F, 0.12F, 1.0F, 1.0F};
static const float COL_GREEN[4] = {0.18F, 0.85F, 0.38F, 1.0F};
static const float COL_WHITE[4] = {0.92F, 0.98F, 1.0F, 1.0F};
static const float COL_PANEL[4] = {0.035F, 0.105F, 0.13F, 1.0F};

static bool mesh_mech_ready(void);

static const MeshPartSpec MESH_MECH_PARTS[MECH_MESH_PARTS] = {
    {{0.0F, 1.62F, 0.0F},
     {1.20F, 1.46F, 0.86F},
     {0.04F, 0.34F, 0.70F, 1.0F},
     0,
     MECH_MESH_TORSO},
    {{0.0F, 0.92F, 0.04F},
     {1.06F, 0.42F, 0.68F},
     {0.035F, 0.22F, 0.44F, 1.0F},
     0,
     MECH_MESH_PELVIS},
    {{0.0F, 2.36F, 0.05F},
     {1.76F, 0.38F, 0.86F},
     {0.035F, 0.22F, 0.44F, 1.0F},
     0,
     MECH_MESH_SHOULDER},
    {{0.0F, 1.88F, -0.50F},
     {0.70F, 0.24F, 0.12F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{0.0F, 2.78F, -0.06F},
     {0.66F, 0.54F, 0.46F},
     {0.52F, 0.63F, 0.70F, 1.0F},
     0,
     MECH_MESH_HEAD},
    {{0.0F, 2.84F, -0.44F},
     {0.54F, 0.12F, 0.08F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{0.0F, 2.64F, 0.44F},
     {0.74F, 0.48F, 0.34F},
     {0.13F, 0.17F, 0.20F, 1.0F},
     0,
     MECH_MESH_PELVIS},
    {{-1.18F, 2.48F, 0.02F},
     {0.86F, 0.42F, 0.88F},
     {0.08F, 0.42F, 0.78F, 1.0F},
     0,
     MECH_MESH_SHOULDER},
    {{1.18F, 2.48F, 0.02F},
     {0.86F, 0.42F, 0.88F},
     {0.06F, 0.35F, 0.70F, 1.0F},
     0,
     MECH_MESH_SHOULDER},
    {{-1.56F, 1.78F, -0.02F},
     {0.58F, 0.92F, 0.54F},
     {0.0F, 0.45F, 1.0F, 1.0F},
     0,
     MECH_MESH_FOREARM},
    {{1.56F, 1.78F, -0.02F},
     {0.58F, 0.92F, 0.54F},
     {0.0F, 0.45F, 1.0F, 1.0F},
     0,
     MECH_MESH_FOREARM},
    {{-1.92F, 1.38F, -0.18F},
     {0.56F, 0.72F, 0.56F},
     {0.10F, 0.58F, 1.0F, 1.0F},
     0,
     MECH_MESH_FOREARM},
    {{1.92F, 1.38F, -0.18F},
     {0.56F, 0.72F, 0.56F},
     {0.10F, 0.58F, 1.0F, 1.0F},
     0,
     MECH_MESH_FOREARM},
    {{2.24F, 1.18F, -0.66F},
     {0.26F, 0.30F, 1.16F},
     {1.0F, 0.48F, 0.05F, 1.0F},
     0,
     MECH_MESH_WEAPON},
    {{-0.62F, 0.58F, -0.08F},
     {0.62F, 1.02F, 0.58F},
     {0.0F, 0.45F, 1.0F, 1.0F},
     0,
     MECH_MESH_LIMB},
    {{0.62F, 0.58F, -0.08F},
     {0.62F, 1.02F, 0.58F},
     {0.0F, 0.45F, 1.0F, 1.0F},
     0,
     MECH_MESH_LIMB},
    {{-0.70F, 0.08F, -0.30F},
     {0.92F, 0.34F, 1.06F},
     {0.76F, 0.84F, 0.90F, 1.0F},
     0,
     MECH_MESH_FOOT},
    {{0.70F, 0.08F, -0.30F},
     {0.92F, 0.34F, 1.06F},
     {0.76F, 0.84F, 0.90F, 1.0F},
     0,
     MECH_MESH_FOOT},
    {{-0.56F, 0.20F, 0.34F},
     {0.46F, 0.24F, 0.38F},
     {0.28F, 0.78F, 1.0F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{0.56F, 0.20F, 0.34F},
     {0.46F, 0.24F, 0.38F},
     {0.28F, 0.78F, 1.0F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{-0.62F, 2.92F, 0.02F},
     {0.38F, 0.34F, 0.82F},
     {1.0F, 0.48F, 0.05F, 1.0F},
     MECH_PART_ROCKETS_ONLY,
     MECH_MESH_ROCKET_POD},
    {{0.62F, 2.92F, 0.02F},
     {0.38F, 0.34F, 0.82F},
     {1.0F, 0.48F, 0.05F, 1.0F},
     MECH_PART_ROCKETS_ONLY,
     MECH_MESH_ROCKET_POD},
    {{-0.62F, 3.18F, 0.02F},
     {0.30F, 0.18F, 0.72F},
     {0.78F, 0.88F, 0.92F, 1.0F},
     MECH_PART_ROCKETS_ONLY,
     MECH_MESH_ROCKET_TUBE},
    {{0.62F, 3.18F, 0.02F},
     {0.30F, 0.18F, 0.72F},
     {0.78F, 0.88F, 0.92F, 1.0F},
     MECH_PART_ROCKETS_ONLY,
     MECH_MESH_ROCKET_TUBE},
    {{-0.42F, 2.91F, -0.48F},
     {0.18F, 0.18F, 0.28F},
     {1.0F, 0.18F, 0.08F, 1.0F},
     MECH_PART_ROCKETS_ONLY,
     MECH_MESH_ROCKET_TUBE},
    {{0.42F, 2.91F, -0.48F},
     {0.18F, 0.18F, 0.28F},
     {1.0F, 0.18F, 0.08F, 1.0F},
     MECH_PART_ROCKETS_ONLY,
     MECH_MESH_ROCKET_TUBE},
    {{0.0F, 1.78F, -0.56F},
     {0.82F, 0.52F, 0.18F},
     {0.02F, 0.82F, 1.0F, 1.0F},
     0,
     MECH_MESH_ARMOR_PLATE},
    {{0.0F, 1.50F, -0.62F},
     {0.34F, 0.22F, 0.08F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VISOR},
    {{-0.44F, 1.18F, -0.48F},
     {0.24F, 0.42F, 0.12F},
     {0.035F, 0.22F, 0.44F, 1.0F},
     0,
     MECH_MESH_ARMOR_PLATE},
    {{0.44F, 1.18F, -0.48F},
     {0.24F, 0.42F, 0.12F},
     {0.035F, 0.22F, 0.44F, 1.0F},
     0,
     MECH_MESH_ARMOR_PLATE},
    {{-0.74F, 0.72F, -0.50F},
     {0.46F, 0.22F, 0.18F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VISOR},
    {{0.74F, 0.72F, -0.50F},
     {0.46F, 0.22F, 0.18F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VISOR},
    {{-0.72F, 0.18F, -0.86F},
     {0.42F, 0.12F, 0.30F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VISOR},
    {{0.72F, 0.18F, -0.86F},
     {0.42F, 0.12F, 0.30F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VISOR},
    {{-1.20F, 2.80F, -0.42F},
     {0.52F, 0.16F, 0.16F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VISOR},
    {{1.20F, 2.80F, -0.42F},
     {0.52F, 0.16F, 0.16F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VISOR},
    {{-1.34F, 2.30F, -0.42F},
     {0.58F, 0.18F, 0.14F},
     {0.78F, 0.88F, 0.92F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{1.34F, 2.30F, -0.42F},
     {0.58F, 0.18F, 0.14F},
     {0.78F, 0.88F, 0.92F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{-0.38F, 2.84F, -0.40F},
     {0.24F, 0.10F, 0.14F},
     {1.0F, 0.48F, 0.05F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{0.38F, 2.84F, -0.40F},
     {0.24F, 0.10F, 0.14F},
     {1.0F, 0.48F, 0.05F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{-0.20F, 2.70F, -0.45F},
     {0.24F, 0.14F, 0.08F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{0.20F, 2.70F, -0.45F},
     {0.24F, 0.14F, 0.08F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_VENT},
    {{-0.30F, 1.92F, -0.78F},
     {0.16F, 0.16F, 0.08F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_ROCKET_TUBE},
    {{0.30F, 1.92F, -0.78F},
     {0.16F, 0.16F, 0.08F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_ROCKET_TUBE},
    {{-1.46F, 2.76F, -0.24F},
     {0.18F, 0.08F, 0.18F},
     {0.28F, 0.78F, 1.0F, 1.0F},
     0,
     MECH_MESH_HYDRAULIC},
    {{-0.98F, 2.76F, -0.24F},
     {0.18F, 0.08F, 0.18F},
     {0.28F, 0.78F, 1.0F, 1.0F},
     0,
     MECH_MESH_HYDRAULIC},
    {{0.98F, 2.76F, -0.24F},
     {0.18F, 0.08F, 0.18F},
     {0.28F, 0.78F, 1.0F, 1.0F},
     0,
     MECH_MESH_HYDRAULIC},
    {{1.46F, 2.76F, -0.24F},
     {0.18F, 0.08F, 0.18F},
     {0.28F, 0.78F, 1.0F, 1.0F},
     0,
     MECH_MESH_HYDRAULIC},
    {{-0.70F, 0.38F, -0.42F},
     {0.16F, 0.07F, 0.16F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_HYDRAULIC},
    {{0.70F, 0.38F, -0.42F},
     {0.16F, 0.07F, 0.16F},
     {0.02F, 0.92F, 1.0F, 1.0F},
     0,
     MECH_MESH_HYDRAULIC},
    {{0.0F, 0.0F, 0.0F},
     {126.0F, 126.0F, 126.0F},
     {1.0F, 1.0F, 1.0F, 1.0F},
     0,
     MECH_MESH_QUATERNIUS},
};

static const char *MESH_MECH_RESOURCE_PATHS[MECH_MESH_TYPES] = {
    "assets/meshes/mech_starter_torso.gltf",
    "assets/meshes/mech_starter_pelvis.gltf",
    "assets/meshes/mech_starter_head.gltf",
    "assets/meshes/mech_starter_shoulder.gltf",
    "assets/meshes/mech_starter_limb.gltf",
    "assets/meshes/mech_starter_forearm.gltf",
    "assets/meshes/mech_starter_weapon.gltf",
    "assets/meshes/mech_starter_foot.gltf",
    "assets/meshes/mech_starter_rocket_pod.gltf",
    "assets/meshes/mech_starter_rocket_tube.gltf",
    "assets/meshes/mech_starter_vent.gltf",
    "assets/meshes/mech_starter_hydraulic.gltf",
    "assets/meshes/mech_starter_joint.gltf",
    "assets/meshes/mech_starter_armor_plate.gltf",
    "assets/meshes/mech_starter_visor.gltf",
    "assets/meshes/poly_pizza_quaternius_mech_cc0.glb",
    "assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_main2_static_cc0.gltf",
    "assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_main_static_cc0.gltf",
    "assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_edge_static_cc0.gltf",
    "assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_dark_static_cc0.gltf",
    "assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_eye_static_cc0.gltf",
    "assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_grey_static_cc0.gltf",
    "assets/meshes/poly_pizza_quaternius_robot_enemy_legs_gun_lightgrey_static_cc0.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_005_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_005_ao_2_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_005_uv_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_006_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_001_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_uv_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_002_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_003_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_004_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_004_none_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_007_static_ccby30.gltf",
    "assets/meshes/poly_pizza_alimayo_mech_assault_walker_material_007_none_static_ccby30.gltf",
    "assets/meshes/mech_world_studs_floor.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat18_static_ccby30.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat16_static_ccby30.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat9_static_ccby30.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat17_static_ccby30.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat13_static_ccby30.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat21_static_ccby30.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat20_static_ccby30.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat15_static_ccby30.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat12_static_ccby30.gltf",
    "assets/meshes/poly_pizza_tekano_sentinel_mech_mat23_static_ccby30.gltf",
    "assets/meshes/kenney_modular_space_gate_cc0.glb",
    "assets/meshes/kenney_modular_space_corridor_wide_cc0.glb",
    "assets/meshes/kenney_modular_space_room_small_cc0.glb",
};

static void ortho(float left, float right, float bottom, float top,
                  float near_z, float far_z, float out[16]) {
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

static float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

static float len2(float x, float z) { return sqrtf((x * x) + (z * z)); }

static float approachf(float current, float target, float delta) {
  if (current < target) {
    return current + fminf(delta, target - current);
  }
  return current - fminf(delta, current - target);
}

static float angle_delta(float from, float to) {
  float d = to - from;
  while (d > 3.14159265F) {
    d -= 6.2831853F;
  }
  while (d < -3.14159265F) {
    d += 6.2831853F;
  }
  return d;
}

static float approach_angle(float current, float target, float delta) {
  return current + clampf(angle_delta(current, target), -delta, delta);
}

static const char *screen_name(GameScreen screen) {
  switch (screen) {
  case SCREEN_HANGAR:
    return "hangar";
  case SCREEN_BATTLE:
    return "battle";
  case SCREEN_REWARD:
    return "reward";
  case SCREEN_UPGRADE:
    return "upgrade";
  case SCREEN_RETEST:
    return "retest";
  default:
    return "unknown";
  }
}

static void q_axis(float x, float y, float z, float angle, float out[4]) {
  glm_quatv(out, angle, (vec3){x, y, z});
}

static void q_pose(float yaw, float pitch, float roll, float out[4]) {
  float q_y[4];
  float q_x[4];
  float q_z[4];
  float q_tmp[4];
  q_axis(0.0F, 1.0F, 0.0F, yaw, q_y);
  q_axis(1.0F, 0.0F, 0.0F, pitch, q_x);
  q_axis(0.0F, 0.0F, 1.0F, roll, q_z);
  glm_quat_mul(q_y, q_x, q_tmp);
  glm_quat_mul(q_tmp, q_z, out);
}

static void mech_point(float root_x, float root_z, float yaw, float lx, float ly,
                       float lz, float scale, float out[3]) {
  const float s = sinf(yaw);
  const float c = cosf(yaw);
  out[0] = root_x + (((lx * c) + (lz * s)) * scale);
  out[1] = 0.25F + (ly * scale);
  out[2] = root_z + (((lz * c) - (lx * s)) * scale);
}

static void source_mech_pose(bool hangar, float *root_x, float *root_z,
                             float *root_scale, float *yaw) {
  const float time = (float)nt_time_now();
  *root_x = hangar ? 0.0F : s_game.mech_x;
  *root_z = hangar ? 0.4F : s_game.mech_z;
  *root_scale = hangar ? 1.18F : 0.96F;
  *yaw = hangar ? (2.74F + sinf(time * 0.65F) * 0.05F) : s_game.mech_facing;
}

static bool source_hero_overlay_part(int index, bool rockets) {
  if (index == 26 || index == 27 || (index >= 34 && index <= 45)) {
    return true;
  }
  if (rockets && index >= 20 && index <= 25) {
    return true;
  }
  return false;
}

static bool assault_hero_kitbash_part(int index, bool rockets) {
  if (index == 13 || index == 26 || index == 27 || (index >= 34 && index <= 49)) {
    return true;
  }
  if (rockets && index >= 20 && index <= 25) {
    return true;
  }
  return false;
}

static void source_mech_cannon_points(float root_x, float root_z, float yaw,
                                      float root_scale, float left[3],
                                      float right[3]) {
  const float recoil = s_game.mech_recoil * 0.10F;
  mech_point(root_x, root_z, yaw, -0.78F, 1.70F, -1.24F + recoil, root_scale,
             left);
  mech_point(root_x, root_z, yaw, 0.78F, 1.70F, -1.24F + recoil, root_scale,
             right);
}

static void source_mech_rocket_point(float root_x, float root_z, float yaw,
                                     float root_scale, int side, int tube,
                                     float out[3]) {
  mech_point(root_x, root_z, yaw, (float)side * 1.18F,
             2.80F + ((float)tube * 0.12F), -1.12F, root_scale, out);
}

static void source_mech_vent_point(float root_x, float root_z, float yaw,
                                   float root_scale, int side, float out[3]) {
  mech_point(root_x, root_z, yaw, (float)side * 0.36F, 2.06F, 0.48F,
             root_scale, out);
}

static void source_mech_foot_point(float root_x, float root_z, float yaw,
                                   float root_scale, int side, int row,
                                   float out[3]) {
  const float lz = row < 0 ? -0.34F : 0.70F;
  mech_point(root_x, root_z, yaw, (float)side * 0.58F, 0.18F, lz, root_scale,
             out);
  out[1] = 0.14F;
}

static void rect2(float x, float y, float w, float h, const float color[4]) {
  nt_shape_renderer_rect((float[3]){x + (w * 0.5F), y + (h * 0.5F), 0.0F},
                         (float[2]){w, h}, color);
}

static void capsule2(float x, float y, float w, float h, const float color[4]) {
  const float r = h * 0.5F;
  rect2(x + r, y, w - (r * 2.0F), h, color);
  nt_shape_renderer_circle((float[3]){x + r, y + r, 0.0F}, r, color);
  nt_shape_renderer_circle((float[3]){x + w - r, y + r, 0.0F}, r, color);
}

static void button2(UiBox b, const float color[4], bool enabled) {
  const float shadow[4] = {0.0F, 0.0F, 0.0F, 0.28F};
  const float disabled[4] = {0.22F, 0.26F, 0.30F, 0.88F};
  capsule2(b.x + 5.0F, b.y + 6.0F, b.w, b.h, shadow);
  capsule2(b.x, b.y, b.w, b.h, enabled ? color : disabled);
  rect2(b.x + (b.w * 0.14F), b.y + (b.h * 0.18F), b.w * 0.72F, b.h * 0.16F,
        (float[4]){1.0F, 1.0F, 1.0F, 0.22F});
}

static void draw_segment_digit(float x, float y, float s, int digit,
                               const float color[4]) {
  static const uint8_t segs[10] = {
      0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U, 0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU,
  };
  const float t = s * 0.16F;
  const float w = s * 0.62F;
  const float h = s;
  const uint8_t mask = segs[digit % 10];
  if (mask & 0x01U) {
    rect2(x + t, y, w, t, color);
  }
  if (mask & 0x02U) {
    rect2(x + w + t, y + t, t, (h * 0.5F) - t, color);
  }
  if (mask & 0x04U) {
    rect2(x + w + t, y + (h * 0.5F), t, (h * 0.5F) - t, color);
  }
  if (mask & 0x08U) {
    rect2(x + t, y + h - t, w, t, color);
  }
  if (mask & 0x10U) {
    rect2(x, y + (h * 0.5F), t, (h * 0.5F) - t, color);
  }
  if (mask & 0x20U) {
    rect2(x, y + t, t, (h * 0.5F) - t, color);
  }
  if (mask & 0x40U) {
    rect2(x + t, y + (h * 0.5F) - (t * 0.5F), w, t, color);
  }
}

static const uint8_t *glyph_rows(char c) {
  static const uint8_t blank[7] = {0, 0, 0, 0, 0, 0, 0};
  static const uint8_t glyphs[26][7] = {
      {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
      {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
      {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
      {14, 17, 16, 23, 17, 17, 14}, {17, 17, 17, 31, 17, 17, 17},
      {14, 4, 4, 4, 4, 4, 14},      {7, 2, 2, 2, 18, 18, 12},
      {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
      {17, 27, 21, 21, 17, 17, 17}, {17, 25, 21, 19, 17, 17, 17},
      {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
      {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
      {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},
      {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
      {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
      {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31},
  };
  if (c >= 'a' && c <= 'z') {
    c = (char)(c - 'a' + 'A');
  }
  if (c >= 'A' && c <= 'Z') {
    return glyphs[c - 'A'];
  }
  return blank;
}

static void draw_char(float x, float y, float scale, char c,
                      const float color[4]) {
  if (c >= '0' && c <= '9') {
    draw_segment_digit(x, y, scale * 7.0F, c - '0', color);
    return;
  }
  if (c == '-') {
    rect2(x + scale, y + (scale * 3.0F), scale * 3.0F, scale, color);
    return;
  }
  if (c == '+') {
    rect2(x + scale, y + (scale * 3.0F), scale * 3.0F, scale, color);
    rect2(x + (scale * 2.0F), y + scale, scale, scale * 5.0F, color);
    return;
  }
  if (c == ':') {
    rect2(x + (scale * 2.0F), y + (scale * 1.5F), scale, scale, color);
    rect2(x + (scale * 2.0F), y + (scale * 4.5F), scale, scale, color);
    return;
  }
  const uint8_t *rows = glyph_rows(c);
  for (int row = 0; row < 7; ++row) {
    for (int col = 0; col < 5; ++col) {
      if (rows[row] & (uint8_t)(1U << (4 - col))) {
        rect2(x + ((float)col * scale), y + ((float)row * scale), scale * 0.82F,
              scale * 0.82F, color);
      }
    }
  }
}

static bool ui_text_ready(void) {
  if (!s_mesh_mech.initialized || !nt_font_valid(s_mesh_mech.ui_font)) {
    return false;
  }
  const nt_material_info_t *mat_info =
      nt_material_get_info(s_mesh_mech.text_material);
  return mat_info && mat_info->ready &&
         nt_resource_is_ready(s_mesh_mech.ui_font_res);
}

static void draw_debug_text(float x, float y, float scale, const char *text,
                            const float color[4]) {
  float cursor = x;
  for (const char *p = text; p && *p; ++p) {
    if (*p == ' ') {
      cursor += scale * 4.0F;
    } else {
      draw_char(cursor, y, scale, *p, color);
      cursor += scale * ((*p >= '0' && *p <= '9') ? 6.2F : 6.0F);
    }
  }
}

static void draw_text(float x, float y, float scale, const char *text,
                      const float color[4]) {
  if (!ui_text_ready()) {
    draw_debug_text(x, y, scale, text, color);
    return;
  }

  mat4 model;
  glm_mat4_identity(model);
  glm_translate(model, (vec3){x, y, 0.0F});
  nt_text_renderer_draw(text, (const float *)model, scale * 8.0F, color, 0.0F,
                        0.0F);
}

static void draw_int_text(float x, float y, float scale, const char *prefix,
                          int value, const float color[4]) {
  char buf[64];
  (void)snprintf(buf, sizeof(buf), "%s%d", prefix, value);
  draw_text(x, y, scale, buf, color);
}

static void action_button2(UiBox b, const float color[4], bool enabled,
                           const char *label) {
  if (b.w <= 130.0F) {
    rect2(b.x + 5.0F, b.y + 6.0F, b.w, b.h,
          (float[4]){0.0F, 0.0F, 0.0F, 0.28F});
    rect2(b.x, b.y, b.w, b.h,
          enabled ? color : (float[4]){0.22F, 0.26F, 0.30F, 0.95F});
    rect2(b.x + 9.0F, b.y + 9.0F, b.w - 18.0F, b.h * 0.50F,
          (float[4]){0.01F, 0.04F, 0.05F, 0.82F});
    rect2(b.x + 9.0F, b.y + 9.0F, b.w - 18.0F, 5.0F,
          enabled ? COL_WHITE : (float[4]){0.45F, 0.50F, 0.55F, 0.95F});
    const float text_scale = strlen(label) > 5U ? 2.1F : 2.5F;
    const float text_w = (float)strlen(label) * 6.0F * text_scale;
    draw_text(b.x + ((b.w - text_w) * 0.5F), b.y + (b.h * 0.76F), text_scale,
              label, COL_WHITE);
    return;
  }

  button2(b, color, enabled);
  const float cx = b.x + (b.w * 0.50F);
  const float cy = b.y + (b.h * 0.38F);
  nt_shape_renderer_circle((float[3]){cx, cy, 0.0F}, b.h * 0.27F,
                           (float[4]){0.01F, 0.04F, 0.05F, 0.90F});
  nt_shape_renderer_circle_wire(
      (float[3]){cx, cy, 0.0F}, b.h * 0.30F,
      enabled ? COL_WHITE : (float[4]){0.45F, 0.50F, 0.55F, 0.95F});
  const float text_scale = strlen(label) > 5U ? 2.1F : 2.5F;
  const float text_w = (float)strlen(label) * 6.0F * text_scale;
  draw_text(b.x + ((b.w - text_w) * 0.5F), b.y + (b.h * 0.70F), text_scale,
            label, COL_WHITE);
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
      if (sscanf(argv[++i], "%dx%d", &width, &height) == 2 && width > 0 &&
          height > 0) {
        s_window_width = width;
        s_window_height = height;
      }
    }
  }
}

static void spawn_drones(int battle_index) {
  const int count = battle_index == 0 ? 4 : 6;
  const float spawn_x[MAX_DRONES] = {3.8F, -3.6F, 4.8F, -4.7F,
                                    0.5F, 2.4F,  -2.7F, 0.0F};
  const float spawn_z[MAX_DRONES] = {-0.9F, -1.6F, -2.8F, -3.6F,
                                    -4.2F, -5.1F, -5.4F, -6.2F};
  for (int i = 0; i < MAX_DRONES; ++i) {
    s_game.drones[i].alive = i < count;
    s_game.drones[i].hp = battle_index == 0 ? 38.0F : 44.0F;
    s_game.drones[i].x = spawn_x[i];
    s_game.drones[i].z = spawn_z[i];
  }
}

static void reset_runtime(void) {
  memset(&s_game, 0, sizeof(s_game));
  s_game.screen = SCREEN_HANGAR;
  s_game.mech_z = 2.5F;
  s_game.mech_facing = 0.18F;
  s_game.salvage = 0;
  g_game_state.wallet_soft = 0;
  game_state_mark_dirty();
}

static int alive_count(void) {
  int count = 0;
  for (int i = 0; i < MAX_DRONES; ++i) {
    if (s_game.drones[i].alive) {
      count++;
    }
  }
  return count;
}

static int target_drone(void) {
  int best = -1;
  float best_d = 9999.0F;
  for (int i = 0; i < MAX_DRONES; ++i) {
    if (!s_game.drones[i].alive) {
      continue;
    }
    const float dx = s_game.drones[i].x - s_game.mech_x;
    const float dz = s_game.drones[i].z - s_game.mech_z;
    const float d = (dx * dx) + (dz * dz);
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

static void start_battle(void) {
  s_game.screen = SCREEN_BATTLE;
  s_game.mech_x = 0.0F;
  s_game.mech_z = 2.6F;
  s_game.mech_vx = 0.0F;
  s_game.mech_vz = 0.0F;
  s_game.mech_facing = 0.0F;
  s_game.mech_walk = 0.0F;
  s_game.mech_recoil = 0.0F;
  s_game.heat = 0.05F;
  s_game.cannon_cd = 0.25F;
  s_game.rocket_cd = s_game.rockets_equipped ? 0.5F : 999.0F;
  s_game.dash_cd = 0.0F;
  s_game.dash_flash = 0.0F;
  s_game.rocket_flash = 0.0F;
  s_game.hit_flash = 0.0F;
  s_game.battle_time = 0.0F;
  s_game.reward_ready = false;
  spawn_drones(s_game.battle_index);
}

static void finish_battle(void) {
  s_game.screen = SCREEN_REWARD;
  s_game.reward_ready = true;
  s_game.salvage += s_game.battle_index == 0 ? ROCKET_COST : 75;
  g_game_state.wallet_soft = s_game.salvage;
  game_state_mark_dirty();
}

static void buy_rockets(void) {
  if (!s_game.rockets_equipped && s_game.salvage >= ROCKET_COST) {
    s_game.salvage -= ROCKET_COST;
    g_game_state.wallet_soft = s_game.salvage;
    s_game.rockets_equipped = true;
    s_game.battle_index = 1;
    s_game.screen = SCREEN_RETEST;
    game_state_mark_dirty();
  }
}

static void fire_rockets(void) {
  if (!s_game.rockets_equipped || s_game.rocket_cd > 0.0F ||
      s_game.heat > 0.82F) {
    return;
  }
  s_game.rocket_cd = 3.5F;
  s_game.heat = clampf(s_game.heat + 0.42F, 0.0F, 1.0F);
  s_game.rocket_flash = 1.45F;
  s_game.mech_recoil = 1.0F;
  int hit = 0;
  for (int i = 0; i < MAX_DRONES && hit < 4; ++i) {
    if (!s_game.drones[i].alive) {
      continue;
    }
    s_game.drones[i].hp -= 70.0F;
    s_game.drones[i].z -= 0.45F;
    if (s_game.drones[i].hp <= 0.0F) {
      s_game.drones[i].alive = false;
    }
    hit++;
  }
}

static void dash(void) {
  if (s_game.dash_cd > 0.0F || s_game.heat > 0.92F) {
    return;
  }
  s_game.dash_cd = 1.3F;
  s_game.dash_flash = 0.35F;
  s_game.heat = clampf(s_game.heat + 0.12F, 0.0F, 1.0F);
  s_game.mech_vz -= 5.6F;
  s_game.mech_z = clampf(s_game.mech_z - 0.34F, -0.8F, 5.4F);
}

static void handle_button_click(void) {
  if (!nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
    return;
  }
  for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
    const nt_pointer_t pointer = g_nt_input.pointers[i];
    if (!pointer.active) {
      continue;
    }
    const float pointer_y = (float)g_nt_window.height - pointer.y;
    if (contains(s_primary_box, pointer.x, pointer_y)) {
      if (s_game.screen == SCREEN_HANGAR || s_game.screen == SCREEN_RETEST) {
        start_battle();
      } else if (s_game.screen == SCREEN_REWARD) {
        s_game.screen =
            s_game.rockets_equipped ? SCREEN_RETEST : SCREEN_UPGRADE;
      } else if (s_game.screen == SCREEN_UPGRADE) {
        buy_rockets();
      }
    }
    if (s_game.screen == SCREEN_BATTLE &&
        contains(s_dash_box, pointer.x, pointer_y)) {
      dash();
    }
    if (s_game.screen == SCREEN_BATTLE &&
        contains(s_special_box, pointer.x, pointer_y)) {
      fire_rockets();
    }
  }
}

static void handle_input(void) {
  if (nt_input_key_is_pressed(NT_KEY_ENTER) ||
      nt_input_key_is_pressed(NT_KEY_SPACE)) {
    if (s_game.screen == SCREEN_HANGAR || s_game.screen == SCREEN_RETEST) {
      start_battle();
    } else if (s_game.screen == SCREEN_REWARD) {
      s_game.screen = s_game.rockets_equipped ? SCREEN_RETEST : SCREEN_UPGRADE;
    } else if (s_game.screen == SCREEN_UPGRADE) {
      buy_rockets();
    }
  }
  if (s_game.screen == SCREEN_BATTLE) {
    if (nt_input_key_is_pressed(NT_KEY_Q)) {
      dash();
    }
    if (nt_input_key_is_pressed(NT_KEY_E)) {
      fire_rockets();
    }
  }
  handle_button_click();
}

static void update_battle(float dt) {
  if (s_game.screen != SCREEN_BATTLE) {
    return;
  }
  float dx = 0.0F;
  float dz = 0.0F;
  if (nt_input_key_is_down(NT_KEY_A) ||
      nt_input_key_is_down(NT_KEY_ARROW_LEFT)) {
    dx -= 1.0F;
  }
  if (nt_input_key_is_down(NT_KEY_D) ||
      nt_input_key_is_down(NT_KEY_ARROW_RIGHT)) {
    dx += 1.0F;
  }
  if (nt_input_key_is_down(NT_KEY_W) || nt_input_key_is_down(NT_KEY_ARROW_UP)) {
    dz -= 1.0F;
  }
  if (nt_input_key_is_down(NT_KEY_S) ||
      nt_input_key_is_down(NT_KEY_ARROW_DOWN)) {
    dz += 1.0F;
  }
  const float l = len2(dx, dz);
  float move_speed = 0.0F;
  if (l > 0.001F) {
    dx /= l;
    dz /= l;
    move_speed = 4.0F;
  }
  const float target_vx = dx * move_speed;
  const float target_vz = dz * move_speed;
  s_game.mech_vx = approachf(s_game.mech_vx, target_vx, dt * 14.0F);
  s_game.mech_vz = approachf(s_game.mech_vz, target_vz, dt * 14.0F);
  s_game.mech_x = clampf(s_game.mech_x + (s_game.mech_vx * dt), -5.8F, 5.8F);
  s_game.mech_z = clampf(s_game.mech_z + (s_game.mech_vz * dt), -0.8F, 5.4F);
  const float actual_speed = len2(s_game.mech_vx, s_game.mech_vz);
  if (actual_speed > 0.12F) {
    s_game.mech_facing =
        approach_angle(s_game.mech_facing, atan2f(s_game.mech_vx, -s_game.mech_vz),
                       dt * 7.5F);
  } else {
    const int aim_target = target_drone();
    if (aim_target >= 0) {
      const float ax = s_game.drones[aim_target].x - s_game.mech_x;
      const float az = s_game.drones[aim_target].z - s_game.mech_z;
      s_game.mech_facing =
          approach_angle(s_game.mech_facing, atan2f(ax, -az), dt * 3.0F);
    }
  }
  s_game.mech_walk += actual_speed * dt * 4.4F;

  s_game.battle_time += dt;
  s_game.cannon_cd -= dt;
  s_game.rocket_cd -= dt;
  s_game.dash_cd -= dt;
  s_game.dash_flash = clampf(s_game.dash_flash - dt, 0.0F, 1.0F);
  s_game.rocket_flash = clampf(s_game.rocket_flash - dt, 0.0F, 1.0F);
  s_game.hit_flash = clampf(s_game.hit_flash - dt, 0.0F, 1.0F);
  s_game.mech_recoil = clampf(s_game.mech_recoil - dt * 4.4F, 0.0F, 1.0F);
  s_game.heat = clampf(s_game.heat - (dt * 0.18F), 0.0F, 1.0F);

  for (int i = 0; i < MAX_DRONES; ++i) {
    if (!s_game.drones[i].alive) {
      continue;
    }
    const float ox = s_game.mech_x - s_game.drones[i].x;
    const float oz = s_game.mech_z - s_game.drones[i].z;
    const float l2 = len2(ox, oz);
    if (l2 > 0.2F) {
      const float speed = s_game.battle_index == 0 ? 0.72F : 0.95F;
      s_game.drones[i].x += (ox / l2) * dt * speed;
      s_game.drones[i].z += (oz / l2) * dt * speed;
    }
  }

  const int target = target_drone();
  if (target >= 0 && s_game.cannon_cd <= 0.0F && s_game.heat < 0.96F) {
    s_game.cannon_cd = 0.55F;
    s_game.heat = clampf(s_game.heat + 0.055F, 0.0F, 1.0F);
    s_game.hit_flash = 0.26F;
    s_game.mech_recoil = fmaxf(s_game.mech_recoil, 0.48F);
    s_game.drones[target].hp -= s_game.rockets_equipped ? 26.0F : 24.0F;
    s_game.drones[target].z -= 0.08F;
    if (s_game.drones[target].hp <= 0.0F) {
      s_game.drones[target].alive = false;
    }
  }

  if (s_game.rockets_equipped && s_game.battle_time > 0.8F &&
      s_game.rocket_cd <= 0.0F && alive_count() >= 2) {
    fire_rockets();
  }

  if (alive_count() == 0 || s_game.battle_time > 38.0F) {
    finish_battle();
  }
}

static void layout(float w, float h) {
  const float btn_h = h < 620.0F ? 54.0F : 64.0F;
  s_primary_box = (UiBox){.x = w - 274.0F, .y = 34.0F, .w = 230.0F, .h = btn_h};
  s_secondary_box = (UiBox){.x = 42.0F, .y = 34.0F, .w = 190.0F, .h = btn_h};
  s_dash_box = (UiBox){.x = w - 244.0F, .y = 94.0F, .w = 88.0F, .h = 82.0F};
  s_special_box = (UiBox){.x = w - 132.0F, .y = 96.0F, .w = 96.0F, .h = 96.0F};
}

static void draw_floor_panel(float x, float z, float sx, float sz,
                             const float color[4]) {
  const float floor_rot[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
  nt_shape_renderer_rect_rot((float[3]){x, 0.0F, z}, (float[2]){sx, sz},
                             floor_rot, color);
}

static void draw_baseplate_block(float x, float z, float sx, float sz,
                                 const float color[4]) {
  nt_shape_renderer_cube((float[3]){x, 0.035F, z}, (float[3]){sx, 0.07F, sz},
                         color);
}

static void draw_stud_grid(float x, float z, float y, int cols, int rows,
                           float step, const float color[4]) {
  const float start_x = -0.5F * (float)(cols - 1) * step;
  const float start_z = -0.5F * (float)(rows - 1) * step;
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      nt_shape_renderer_cube(
          (float[3]){x + start_x + ((float)col * step), y,
                     z + start_z + ((float)row * step)},
          (float[3]){0.16F, 0.055F, 0.16F}, color);
    }
  }
}

static void draw_world_stud(float x, float z, float y, float size,
                            const float color[4]) {
  nt_shape_renderer_cube((float[3]){x, y, z},
                         (float[3]){size, size * 0.34F, size}, color);
  nt_shape_renderer_cube((float[3]){x - size * 0.18F, y + size * 0.12F,
                                    z - size * 0.18F},
                         (float[3]){size * 0.32F, size * 0.05F,
                                    size * 0.32F},
                         (float[4]){0.76F, 1.0F, 0.36F, color[3] * 0.45F});
}

static void draw_stylized_grass_motif(float x, float z, float s,
                                      const float color[4]) {
  const float y = 0.075F;
  nt_shape_renderer_line((float[3]){x - 0.42F * s, y, z - 0.24F * s},
                         (float[3]){x + 0.42F * s, y, z + 0.24F * s}, color);
  nt_shape_renderer_line((float[3]){x - 0.18F * s, y, z - 0.08F * s},
                         (float[3]){x - 0.58F * s, y, z + 0.24F * s}, color);
  nt_shape_renderer_line((float[3]){x - 0.02F * s, y, z + 0.02F * s},
                         (float[3]){x - 0.26F * s, y, z + 0.46F * s}, color);
  nt_shape_renderer_line((float[3]){x + 0.14F * s, y, z + 0.10F * s},
                         (float[3]){x + 0.48F * s, y, z + 0.48F * s}, color);
  nt_shape_renderer_line((float[3]){x + 0.24F * s, y, z + 0.16F * s},
                         (float[3]){x + 0.66F * s, y, z - 0.04F * s}, color);
  nt_shape_renderer_line((float[3]){x + 0.02F * s, y + 0.01F, z - 0.18F * s},
                         (float[3]){x + 0.34F * s, y + 0.01F, z - 0.46F * s},
                         (float[4]){0.82F, 1.0F, 0.24F, 0.55F});
}

static void draw_block_pylon(float x, float z, float h, const float body[4],
                             const float cap[4], const float stud[4]) {
  nt_shape_renderer_cube((float[3]){x, h * 0.5F, z},
                         (float[3]){0.52F, h, 0.52F}, body);
  nt_shape_renderer_cube((float[3]){x, h + 0.16F, z},
                         (float[3]){0.78F, 0.30F, 0.78F}, cap);
  draw_stud_grid(x, z, h + 0.34F, 1, 1, 0.42F, stud);
}

static void draw_pad_energy_ring(float x, float z, float radius,
                                 const float color[4]) {
  const float floor_rot[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
  nt_shape_renderer_circle_wire_rot((float[3]){x, 0.16F, z}, radius, floor_rot,
                                    color);
  nt_shape_renderer_circle_wire_rot((float[3]){x, 0.18F, z}, radius * 0.70F,
                                    floor_rot,
                                    (float[4]){1.0F, 1.0F, 1.0F,
                                               color[3] * 0.25F});
}

static void draw_arena_dressing(bool hangar) {
  const float blue_block[4] = {0.06F, 0.42F, 1.0F, 1.0F};
  const float yellow_block[4] = {1.0F, 0.78F, 0.12F, 1.0F};
  const float red_block[4] = {1.0F, 0.16F, 0.16F, 1.0F};
  const float white_block[4] = {0.88F, 0.95F, 1.0F, 1.0F};
  const float dark_block[4] = {0.28F, 0.54F, 0.34F, 1.0F};
  const float blue_stud[4] = {0.03F, 0.25F, 0.82F, 1.0F};
  const float yellow_stud[4] = {0.82F, 0.55F, 0.02F, 1.0F};
  const float red_stud[4] = {0.78F, 0.06F, 0.06F, 1.0F};
  const float cyan_glow[4] = {0.0F, 0.92F, 1.0F, 0.34F};
  const float amber_glow[4] = {1.0F, 0.58F, 0.04F, 0.32F};

  const float rail_z_front = hangar ? -6.10F : -6.20F;
  const float rail_z_back = hangar ? 5.20F : 5.65F;
  draw_baseplate_block(0.0F, rail_z_front, 14.4F, 0.16F, dark_block);
  draw_baseplate_block(0.0F, rail_z_back, 14.4F, 0.16F, dark_block);
  draw_baseplate_block(-7.70F, -0.35F, 0.16F, 10.3F, dark_block);
  draw_baseplate_block(7.70F, -0.35F, 0.16F, 10.3F, dark_block);

  for (int i = -3; i <= 3; ++i) {
    const float x = (float)i * 2.25F;
    draw_baseplate_block(x, rail_z_back - 0.18F, 1.02F, 0.24F,
                         (i & 1) ? yellow_block : blue_block);
    draw_stud_grid(x, rail_z_back - 0.18F, 0.22F, 2, 1, 0.42F,
                   (i & 1) ? yellow_stud : blue_stud);
  }

  const float front_pylon_h = hangar ? 1.35F : 1.22F;
  const float right_front_pylon_x = hangar ? 7.35F : 8.25F;
  draw_block_pylon(-7.35F, rail_z_front + 0.35F, front_pylon_h, blue_block,
                   white_block, blue_stud);
  draw_block_pylon(right_front_pylon_x, rail_z_front + 0.35F, front_pylon_h,
                   red_block, white_block, red_stud);
  draw_block_pylon(-7.35F, rail_z_back - 0.35F, hangar ? 1.20F : 1.42F,
                   yellow_block, white_block, yellow_stud);
  draw_block_pylon(7.35F, rail_z_back - 0.35F, hangar ? 1.20F : 1.42F,
                   blue_block, white_block, blue_stud);

  for (int side = -1; side <= 1; side += 2) {
    const float x = (float)side * 7.10F;
    nt_shape_renderer_line((float[3]){x, 1.18F, rail_z_front + 0.48F},
                           (float[3]){x, 1.18F, rail_z_back - 0.55F},
                           side < 0 ? cyan_glow : amber_glow);
    nt_shape_renderer_line((float[3]){x, 0.82F, rail_z_front + 0.48F},
                           (float[3]){x, 0.82F, rail_z_back - 0.55F},
                           side < 0 ? amber_glow : cyan_glow);
  }

  draw_pad_energy_ring(0.0F, hangar ? 0.15F : 1.45F, hangar ? 2.25F : 2.05F,
                       cyan_glow);
  draw_pad_energy_ring(0.0F, hangar ? 0.15F : 1.45F, hangar ? 1.25F : 1.12F,
                       amber_glow);
}

static void draw_floor_grid(float half, bool hangar) {
  const float floor_rot[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
  const float *grass = COL_BG_FLOOR;
  const float tile_a[4] = {0.58F, 0.86F, 0.45F, 1.0F};
  const float tile_b[4] = {0.42F, 0.74F, 0.32F, 1.0F};
  const float stud[4] = {0.30F, 0.58F, 0.23F, 0.58F};
  const float stud_soft[4] = {0.48F, 0.78F, 0.27F, 0.44F};
  const float motif_shadow[4] = {0.30F, 0.64F, 0.20F, 0.34F};
  const float motif[4] = {0.62F, 0.95F, 0.18F, 0.62F};
  const float blue_block[4] = {0.06F, 0.42F, 1.0F, 1.0F};
  const float yellow_block[4] = {1.0F, 0.78F, 0.12F, 1.0F};
  const float red_block[4] = {1.0F, 0.16F, 0.16F, 1.0F};
  const float white_block[4] = {0.88F, 0.95F, 1.0F, 1.0F};
  const float pad_stud[4] = {0.36F, 0.64F, 0.83F, 1.0F};
  const float blue_stud[4] = {0.03F, 0.25F, 0.82F, 1.0F};
  const float yellow_stud[4] = {0.82F, 0.55F, 0.02F, 1.0F};
  const float red_stud[4] = {0.78F, 0.06F, 0.06F, 1.0F};
  const float white_stud[4] = {0.70F, 0.83F, 0.94F, 1.0F};

  nt_shape_renderer_rect_rot((float[3]){0.0F, -0.03F, 0.0F},
                             (float[2]){half * 2.35F, half * 2.12F}, floor_rot,
                             grass);
  for (int z = -7; z <= 7; ++z) {
    for (int x = -8; x <= 8; ++x) {
      const float fx = (float)x;
      const float fz = (float)z;
      draw_floor_panel(fx, fz, 0.94F, 0.94F,
                       ((x + z) & 1) == 0 ? tile_a : tile_b);
      const int motif_gap =
          ((x + 8) % 6 <= 1 && (z + 7) % 5 <= 1) ||
          ((x + z + 16) % 9 == 0);
      if (motif_gap) {
        draw_floor_panel(fx, fz, 0.70F, 0.28F, motif_shadow);
      } else {
        draw_world_stud(fx - 0.24F, fz - 0.22F, 0.050F, 0.15F, stud);
        draw_world_stud(fx + 0.24F, fz + 0.22F, 0.050F, 0.15F, stud_soft);
        if (((x * 3 + z * 5) & 3) == 0) {
          draw_world_stud(fx + 0.23F, fz - 0.24F, 0.048F, 0.12F, stud_soft);
        }
      }
    }
  }

  for (int z = -6; z <= 6; z += 3) {
    for (int x = -7; x <= 7; x += 4) {
      const float offset = (float)(((x * 17) + (z * 11)) % 5) * 0.13F;
      draw_floor_panel((float)x + offset, (float)z - offset, 0.92F, 0.32F,
                       motif_shadow);
      draw_stylized_grass_motif((float)x + offset, (float)z - offset, 0.92F,
                                motif);
    }
  }

  draw_baseplate_block(0.0F, hangar ? 0.15F : 1.45F, 5.4F, 4.1F,
                       (float[4]){0.54F, 0.76F, 0.95F, 1.0F});
  draw_stud_grid(0.0F, hangar ? 0.15F : 1.45F, 0.125F, 7, 5, 0.72F,
                 pad_stud);
  draw_baseplate_block(0.0F, hangar ? -1.95F : -0.68F, 5.7F, 0.18F,
                       yellow_block);
  draw_baseplate_block(0.0F, hangar ? 2.25F : 3.58F, 5.7F, 0.18F, blue_block);
  draw_arena_dressing(hangar);

  if (hangar) {
    nt_shape_renderer_cube((float[3]){-4.1F, 0.46F, 2.8F},
                           (float[3]){1.7F, 0.9F, 1.2F}, red_block);
    draw_stud_grid(-4.1F, 2.8F, 0.94F, 3, 2, 0.52F, red_stud);
    nt_shape_renderer_cube((float[3]){-4.1F, 1.08F, 2.8F},
                           (float[3]){1.34F, 0.28F, 0.88F}, yellow_block);
    draw_stud_grid(-4.1F, 2.8F, 1.25F, 2, 1, 0.52F, yellow_stud);
    nt_shape_renderer_cube((float[3]){4.2F, 0.42F, 2.35F},
                           (float[3]){1.5F, 0.82F, 1.15F}, blue_block);
    draw_stud_grid(4.2F, 2.35F, 0.86F, 3, 2, 0.48F, blue_stud);
    nt_shape_renderer_cube((float[3]){4.2F, 1.02F, 2.35F},
                           (float[3]){1.04F, 0.28F, 0.82F}, white_block);
    draw_stud_grid(4.2F, 2.35F, 1.19F, 2, 1, 0.42F, white_stud);
    for (int i = -2; i <= 2; ++i) {
      const float x = (float)i * 2.7F;
      nt_shape_renderer_cube((float[3]){x, 0.22F, -4.35F},
                             (float[3]){1.6F, 0.44F, 1.6F},
                             (i & 1) ? yellow_block : blue_block);
      draw_stud_grid(x, -4.35F, 0.47F, 2, 2, 0.56F,
                     (i & 1) ? yellow_stud : blue_stud);
      nt_shape_renderer_cube((float[3]){x, 0.70F, -4.35F},
                             (float[3]){1.12F, 0.36F, 1.12F}, white_block);
      draw_stud_grid(x, -4.35F, 0.91F, 2, 2, 0.42F, white_stud);
    }
  } else {
    for (int i = -2; i <= 2; ++i) {
      const float x = (float)i * 3.0F;
      nt_shape_renderer_cube((float[3]){x, 0.42F, -4.8F},
                             (float[3]){1.8F, 0.84F, 1.2F},
                             (i & 1) ? red_block : yellow_block);
      draw_stud_grid(x, -4.8F, 0.87F, 3, 2, 0.52F,
                     (i & 1) ? red_stud : yellow_stud);
    }
    nt_shape_renderer_cube((float[3]){-6.4F, 0.95F, -1.2F},
                           (float[3]){0.52F, 1.9F, 0.52F}, blue_block);
    draw_stud_grid(-6.4F, -1.2F, 1.93F, 1, 1, 0.48F, blue_stud);
    nt_shape_renderer_cube((float[3]){6.4F, 0.95F, -1.2F},
                           (float[3]){0.52F, 1.9F, 0.52F}, red_block);
    draw_stud_grid(6.4F, -1.2F, 1.93F, 1, 1, 0.48F, red_stud);
  }
}

static void draw_shadow(float x, float z, float sx, float sz, float alpha) {
  const float rot[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
  const float shade = 0.86F - clampf(alpha, 0.0F, 1.0F) * 0.42F;
  nt_shape_renderer_rect_rot((float[3]){x, 0.018F, z}, (float[2]){sx, sz}, rot,
                             (float[4]){0.24F * shade, 0.58F * shade,
                                        0.22F * shade, 1.0F});
}

static void draw_joint_sphere(float x, float y, float z, float r) {
  nt_shape_renderer_sphere((float[3]){x, y, z}, r, COL_METAL_DARK);
  nt_shape_renderer_sphere_wire((float[3]){x, y, z}, r * 1.03F,
                                (float[4]){0.58F, 0.78F, 0.86F, 0.55F});
}

static void draw_mech(float x, float z, float scale, bool rockets,
                      bool hangar_pose) {
  float q_y[4];
  float q_x[4];
  float q_z[4];
  q_axis(0.0F, 1.0F, 0.0F, hangar_pose ? 0.18F : -0.10F, q_y);
  q_axis(1.0F, 0.0F, 0.0F, 1.5708F, q_x);
  q_axis(0.0F, 0.0F, 1.0F, 1.5708F, q_z);

  draw_shadow(x, z + 0.08F, 2.8F * scale, 2.1F * scale, 0.38F);

  const float y0 = 0.25F;
  nt_shape_renderer_cube_rot(
      (float[3]){x, y0 + (1.85F * scale), z},
      (float[3]){1.35F * scale, 1.55F * scale, 0.82F * scale}, q_y,
      COL_ARMOR_BLUE);
  nt_shape_renderer_cube_wire_rot(
      (float[3]){x, y0 + (1.85F * scale), z},
      (float[3]){1.39F * scale, 1.59F * scale, 0.86F * scale}, q_y,
      (float[4]){0.78F, 0.94F, 1.0F, 0.45F});
  nt_shape_renderer_cube_rot(
      (float[3]){x, y0 + (1.16F * scale), z + (0.03F * scale)},
      (float[3]){1.10F * scale, 0.42F * scale, 0.72F * scale}, q_y,
      COL_ARMOR_DEEP);
  nt_shape_renderer_cube_rot(
      (float[3]){x, y0 + (2.48F * scale), z + (0.08F * scale)},
      (float[3]){1.62F * scale, 0.36F * scale, 0.78F * scale}, q_y,
      COL_ARMOR_DEEP);
  nt_shape_renderer_cube_wire_rot(
      (float[3]){x, y0 + (2.48F * scale), z + (0.08F * scale)},
      (float[3]){1.70F * scale, 0.44F * scale, 0.86F * scale}, q_y,
      (float[4]){0.96F, 0.98F, 1.0F, 0.50F});
  nt_shape_renderer_cube_rot(
      (float[3]){x - (0.42F * scale), y0 + (1.96F * scale),
                 z - (0.46F * scale)},
      (float[3]){0.33F * scale, 1.04F * scale, 0.10F * scale}, q_y,
      COL_ARMOR_LIGHT);
  nt_shape_renderer_cube_rot(
      (float[3]){x + (0.42F * scale), y0 + (1.96F * scale),
                 z - (0.46F * scale)},
      (float[3]){0.33F * scale, 1.04F * scale, 0.10F * scale}, q_y,
      COL_ARMOR_LIGHT);
  nt_shape_renderer_cube_rot(
      (float[3]){x, y0 + (2.16F * scale), z - (0.44F * scale)},
      (float[3]){0.72F * scale, 0.28F * scale, 0.13F * scale}, q_y,
      COL_EMISSIVE);
  nt_shape_renderer_cube_rot(
      (float[3]){x, y0 + (1.56F * scale), z - (0.50F * scale)},
      (float[3]){0.34F * scale, 0.70F * scale, 0.12F * scale}, q_y,
      COL_METAL_HI);
  nt_shape_renderer_cube_rot(
      (float[3]){x, y0 + (1.32F * scale), z - (0.55F * scale)},
      (float[3]){0.86F * scale, 0.10F * scale, 0.14F * scale}, q_y, COL_AMBER);
  nt_shape_renderer_sphere(
      (float[3]){x, y0 + (2.82F * scale), z - (0.05F * scale)}, 0.38F * scale,
      COL_METAL);
  nt_shape_renderer_cube(
      (float[3]){x, y0 + (2.86F * scale), z - (0.38F * scale)},
      (float[3]){0.56F * scale, 0.12F * scale, 0.08F * scale}, COL_EMISSIVE);
  nt_shape_renderer_cube_rot(
      (float[3]){x, y0 + (2.74F * scale), z + (0.42F * scale)},
      (float[3]){0.78F * scale, 0.52F * scale, 0.34F * scale}, q_y,
      COL_METAL_DARK);
  nt_shape_renderer_sphere(
      (float[3]){x, y0 + (2.74F * scale), z + (0.67F * scale)}, 0.20F * scale,
      COL_EMISSIVE);

  for (int side = -1; side <= 1; side += 2) {
    const float sx = x + ((float)side * 0.95F * scale);
    draw_joint_sphere(sx, y0 + (2.32F * scale), z, 0.23F * scale);
    nt_shape_renderer_cube_rot(
        (float[3]){x + ((float)side * 1.08F * scale), y0 + (2.58F * scale),
                   z + (0.02F * scale)},
        (float[3]){0.72F * scale, 0.34F * scale, 0.78F * scale}, q_y,
        side < 0 ? COL_ARMOR_LIGHT : COL_ARMOR_BLUE);
    nt_shape_renderer_capsule_rot(
        (float[3]){x + ((float)side * 1.35F * scale), y0 + (1.92F * scale), z},
        0.18F * scale, 0.88F * scale, q_z, COL_METAL);
    nt_shape_renderer_cube(
        (float[3]){x + ((float)side * 1.86F * scale), y0 + (1.72F * scale),
                   z - (0.08F * scale)},
        (float[3]){0.34F * scale, 0.78F * scale, 0.34F * scale},
        COL_ARMOR_LIGHT);
    nt_shape_renderer_cube_wire(
        (float[3]){x + ((float)side * 1.86F * scale), y0 + (1.72F * scale),
                   z - (0.08F * scale)},
        (float[3]){0.40F * scale, 0.84F * scale, 0.40F * scale},
        (float[4]){0.0F, 0.95F, 1.0F, 0.45F});
    nt_shape_renderer_cylinder_rot(
        (float[3]){x + ((float)side * 2.02F * scale), y0 + (1.45F * scale),
                   z - (0.38F * scale)},
        0.15F * scale, 0.92F * scale, q_x, side < 0 ? COL_METAL : COL_AMBER);
    nt_shape_renderer_cylinder_rot(
        (float[3]){x + ((float)side * 2.08F * scale), y0 + (1.45F * scale),
                   z - (0.84F * scale)},
        0.08F * scale, 0.48F * scale, q_x, COL_WARNING);

    draw_joint_sphere(x + ((float)side * 0.48F * scale), y0 + (0.95F * scale),
                      z + (0.02F * scale), 0.20F * scale);
    nt_shape_renderer_cube(
        (float[3]){x + ((float)side * 0.55F * scale), y0 + (0.78F * scale),
                   z - (0.18F * scale)},
        (float[3]){0.50F * scale, 0.38F * scale, 0.46F * scale},
        COL_ARMOR_BLUE);
    nt_shape_renderer_capsule((float[3]){x + ((float)side * 0.52F * scale),
                                         y0 + (0.50F * scale),
                                         z + (0.04F * scale)},
                              0.18F * scale, 0.82F * scale, COL_METAL_DARK);
    nt_shape_renderer_cube(
        (float[3]){x + ((float)side * 0.50F * scale), y0 + (0.18F * scale),
                   z + (0.35F * scale)},
        (float[3]){0.46F * scale, 0.24F * scale, 0.38F * scale},
        COL_ARMOR_LIGHT);
    nt_shape_renderer_cube(
        (float[3]){x + ((float)side * 0.60F * scale), y0 + (0.10F * scale),
                   z - (0.18F * scale)},
        (float[3]){0.54F * scale, 0.24F * scale, 0.86F * scale}, COL_METAL);
    nt_shape_renderer_cube_wire(
        (float[3]){x + ((float)side * 0.60F * scale), y0 + (0.10F * scale),
                   z - (0.18F * scale)},
        (float[3]){0.60F * scale, 0.30F * scale, 0.92F * scale},
        (float[4]){0.88F, 0.96F, 1.0F, 0.38F});

    if (rockets) {
      nt_shape_renderer_cube(
          (float[3]){x + ((float)side * 0.62F * scale), y0 + (2.92F * scale),
                     z + (0.04F * scale)},
          (float[3]){0.36F * scale, 0.34F * scale, 0.82F * scale}, COL_AMBER);
      nt_shape_renderer_cube(
          (float[3]){x + ((float)side * 0.62F * scale), y0 + (3.18F * scale),
                     z + (0.04F * scale)},
          (float[3]){0.30F * scale, 0.18F * scale, 0.72F * scale},
          COL_METAL_HI);
      nt_shape_renderer_cylinder_rot(
          (float[3]){x + ((float)side * 0.62F * scale), y0 + (2.91F * scale),
                     z - (0.45F * scale)},
          0.11F * scale, 0.34F * scale, q_x, COL_WARNING);
      nt_shape_renderer_cylinder_rot(
          (float[3]){x + ((float)side * 0.42F * scale), y0 + (2.91F * scale),
                     z - (0.45F * scale)},
          0.08F * scale, 0.28F * scale, q_x, COL_WARNING);
      nt_shape_renderer_cylinder_rot(
          (float[3]){x + ((float)side * 0.82F * scale), y0 + (2.91F * scale),
                     z - (0.45F * scale)},
          0.08F * scale, 0.28F * scale, q_x, COL_WARNING);
    } else {
      nt_shape_renderer_cube_wire(
          (float[3]){x + ((float)side * 0.62F * scale), y0 + (2.92F * scale),
                     z + (0.04F * scale)},
          (float[3]){0.42F * scale, 0.38F * scale, 0.92F * scale},
          (float[4]){1.0F, 0.62F, 0.18F, 0.8F});
    }
  }

  const float glow = 0.55F + (sinf((float)g_nt_app.frame * 0.08F) * 0.25F);
  nt_shape_renderer_sphere(
      (float[3]){x, y0 + (1.62F * scale), z + (0.48F * scale)}, 0.22F * scale,
      (float[4]){0.0F, 0.95F, 1.0F, glow});
  nt_shape_renderer_circle_wire_rot(
      (float[3]){x, y0 + (1.62F * scale), z + (0.49F * scale)}, 0.34F * scale,
      q_x, (float[4]){0.0F, 0.95F, 1.0F, 0.70F});
  for (int i = -2; i <= 2; ++i) {
    nt_shape_renderer_line(
        (float[3]){x + ((float)i * 0.18F * scale), y0 + (1.05F * scale),
                   z + (0.49F * scale)},
        (float[3]){x + ((float)i * 0.18F * scale), y0 + (0.72F * scale),
                   z + (0.58F * scale)},
        s_game.heat > 0.70F ? COL_WARNING : COL_EMISSIVE);
  }
  if (s_game.dash_flash > 0.0F && !hangar_pose) {
    nt_shape_renderer_line((float[3]){x, 0.25F, z + 0.7F},
                           (float[3]){x, 0.25F, z + 2.6F},
                           (float[4]){0.0F, 0.9F, 1.0F, 1.0F});
    nt_shape_renderer_line((float[3]){x - 0.55F, 0.20F, z + 0.9F},
                           (float[3]){x - 0.55F, 0.20F, z + 2.0F},
                           (float[4]){0.0F, 0.9F, 1.0F, 0.65F});
    nt_shape_renderer_line((float[3]){x + 0.55F, 0.20F, z + 0.9F},
                           (float[3]){x + 0.55F, 0.20F, z + 2.0F},
                           (float[4]){0.0F, 0.9F, 1.0F, 0.65F});
  }
  if (s_game.rocket_flash > 0.0F && !hangar_pose) {
    nt_shape_renderer_sphere((float[3]){x - (0.62F * scale),
                                        y0 + (3.0F * scale),
                                        z - (0.48F * scale)},
                             0.18F * scale, COL_AMBER);
    nt_shape_renderer_sphere((float[3]){x + (0.62F * scale),
                                        y0 + (3.0F * scale),
                                        z - (0.48F * scale)},
                             0.18F * scale, COL_AMBER);
  }
}

static void draw_drone(const Drone *d, bool target) {
  if (!d->alive) {
    return;
  }
  if (mesh_mech_ready()) {
    draw_shadow(d->x, d->z, target ? 1.10F : 0.88F, target ? 0.80F : 0.64F,
                target ? 0.26F : 0.18F);
    if (target) {
      nt_shape_renderer_circle_wire((float[3]){d->x, 0.06F, d->z}, 0.72F,
                                    COL_WARNING);
      nt_shape_renderer_circle_wire((float[3]){d->x, 0.08F, d->z}, 1.05F,
                                    (float[4]){1.0F, 0.48F, 0.05F, 0.45F});
      nt_shape_renderer_sphere((float[3]){d->x, 0.82F, d->z - 0.08F}, 0.14F,
                               COL_EMISSIVE);
    }
    return;
  }
  draw_shadow(d->x, d->z, 0.95F, 0.72F, 0.22F);
  const float bob = sinf((float)g_nt_app.frame * 0.08F + d->x) * 0.08F;
  const float y = 0.88F + bob;
  nt_shape_renderer_sphere((float[3]){d->x, y, d->z}, 0.34F,
                           target ? COL_WARNING : COL_METAL_DARK);
  nt_shape_renderer_sphere_wire((float[3]){d->x, y, d->z}, 0.42F,
                                target ? COL_AMBER : COL_METAL_HI);
  nt_shape_renderer_cube((float[3]){d->x, y, d->z - 0.34F},
                         (float[3]){0.48F, 0.16F, 0.12F}, COL_EMISSIVE);
  nt_shape_renderer_cube((float[3]){d->x, y + 0.18F, d->z + 0.02F},
                         (float[3]){0.38F, 0.08F, 0.46F},
                         target ? COL_WARNING : COL_ARMOR_DEEP);
  nt_shape_renderer_cube((float[3]){d->x - 0.48F, y, d->z},
                         (float[3]){0.48F, 0.08F, 0.16F}, COL_METAL);
  nt_shape_renderer_cube((float[3]){d->x + 0.48F, y, d->z},
                         (float[3]){0.48F, 0.08F, 0.16F}, COL_METAL);
  nt_shape_renderer_line((float[3]){d->x - 0.78F, y, d->z},
                         (float[3]){d->x - 1.05F, y + 0.18F, d->z},
                         COL_MAGENTA);
  nt_shape_renderer_line((float[3]){d->x + 0.78F, y, d->z},
                         (float[3]){d->x + 1.05F, y + 0.18F, d->z},
                         COL_MAGENTA);
  if (target) {
    nt_shape_renderer_circle_wire((float[3]){d->x, 0.06F, d->z}, 0.72F,
                                  COL_WARNING);
    nt_shape_renderer_circle_wire((float[3]){d->x, 0.08F, d->z}, 1.05F,
                                  (float[4]){1.0F, 0.48F, 0.05F, 0.45F});
  }
}

static void draw_movement_feedback(void) {
  if (s_game.screen != SCREEN_BATTLE) {
    return;
  }
  const float speed = len2(s_game.mech_vx, s_game.mech_vz);
  const float move_t = clampf(speed / 4.0F, 0.0F, 1.0F);
  const float dash_t = clampf(s_game.dash_flash / 0.35F, 0.0F, 1.0F);
  if (move_t <= 0.03F && dash_t <= 0.02F) {
    return;
  }

  float root_x = s_game.mech_x;
  float root_z = s_game.mech_z;
  float root_scale = 0.88F;
  float yaw = s_game.mech_facing;
  if (mesh_mech_ready()) {
    source_mech_pose(false, &root_x, &root_z, &root_scale, &yaw);
  }

  float dir_x = 0.0F;
  float dir_z = -1.0F;
  if (speed > 0.01F) {
    dir_x = s_game.mech_vx / speed;
    dir_z = s_game.mech_vz / speed;
  } else {
    dir_x = sinf(yaw);
    dir_z = -cosf(yaw);
  }
  const float side_x = cosf(yaw);
  const float side_z = -sinf(yaw);
  const float strafe_t = clampf(fabsf(s_game.mech_vx) / 4.0F, 0.0F, 1.0F);
  const float phase = fmodf(s_game.mech_walk, 6.2831853F);
  const int active_side = phase < 3.14159265F ? -1 : 1;
  const float dust_alpha = 0.28F + (0.28F * move_t) + (0.25F * dash_t);
  const float dust[4] = {0.82F, 1.0F, 0.72F, dust_alpha};
  const float stomp_hot[4] = {1.0F, 0.74F, 0.10F, 0.68F};
  const float stomp_cool[4] = {0.0F, 0.92F, 1.0F, 0.44F};
  const float trail[4] = {0.0F, 0.88F, 1.0F, 0.34F + (0.32F * dash_t)};
  const float strafe_color[4] = {1.0F, 0.50F, 0.05F, 0.30F + 0.34F * strafe_t};

  for (int side = -1; side <= 1; side += 2) {
    for (int row = -1; row <= 1; row += 2) {
      float foot[3];
      source_mech_foot_point(root_x, root_z, yaw, root_scale, side, row, foot);
      const bool active = side == active_side;
      const float ring = (active ? 0.38F : 0.28F) * root_scale;
      nt_shape_renderer_circle_wire(foot, ring,
                                    active ? stomp_hot : stomp_cool);
      nt_shape_renderer_line(
          (float[3]){foot[0], 0.13F, foot[2]},
          (float[3]){foot[0] - (dir_x * (0.48F + 0.34F * move_t)),
                     0.13F,
                     foot[2] - (dir_z * (0.48F + 0.34F * move_t))},
          trail);
      if (active) {
        nt_shape_renderer_sphere(
            (float[3]){foot[0] - dir_x * 0.18F, 0.18F,
                       foot[2] - dir_z * 0.18F},
            (0.10F + 0.12F * move_t) * root_scale, dust);
      }
    }
  }

  if (strafe_t > 0.05F) {
    const float strafe_sign = s_game.mech_vx < 0.0F ? -1.0F : 1.0F;
    for (int i = 0; i < 3; ++i) {
      const float back = 0.42F + ((float)i * 0.30F);
      const float lift = 0.16F + ((float)i * 0.03F);
      nt_shape_renderer_line(
          (float[3]){root_x - dir_x * back - side_x * strafe_sign * 0.82F,
                     lift,
                     root_z - dir_z * back - side_z * strafe_sign * 0.82F},
          (float[3]){root_x - dir_x * (back + 0.75F) -
                         side_x * strafe_sign * 1.42F,
                     lift + 0.02F,
                     root_z - dir_z * (back + 0.75F) -
                         side_z * strafe_sign * 1.42F},
          strafe_color);
    }
  }

  if (dash_t > 0.02F) {
    nt_shape_renderer_circle_wire((float[3]){root_x, 0.15F, root_z},
                                  1.05F + dash_t * 0.65F,
                                  (float[4]){0.0F, 0.95F, 1.0F, 0.55F});
  }
}

static void draw_projectiles(void) {
  const int target = target_drone();
  const bool use_source_mech = mesh_mech_ready();
  float root_x = s_game.mech_x;
  float root_z = s_game.mech_z;
  float root_scale = 0.88F;
  float yaw = s_game.mech_facing;
  float cannon_l[3];
  float cannon_r[3];
  float rocket_l[3];
  float rocket_r[3];
  if (use_source_mech) {
    source_mech_pose(false, &root_x, &root_z, &root_scale, &yaw);
    source_mech_cannon_points(root_x, root_z, yaw, root_scale, cannon_l,
                              cannon_r);
    source_mech_rocket_point(root_x, root_z, yaw, root_scale, -1, -1,
                             rocket_l);
    source_mech_rocket_point(root_x, root_z, yaw, root_scale, 1, 1, rocket_r);
  } else {
    mech_point(root_x, root_z, yaw, -1.55F, 1.42F, -1.10F, root_scale,
               cannon_l);
    mech_point(root_x, root_z, yaw, 1.55F, 1.42F, -1.10F, root_scale,
               cannon_r);
    mech_point(root_x, root_z, yaw, -0.62F, 3.12F, -0.58F, root_scale,
               rocket_l);
    mech_point(root_x, root_z, yaw, 0.62F, 3.12F, -0.58F, root_scale,
               rocket_r);
  }
  if (target >= 0 && s_game.hit_flash > 0.0F) {
    const float flash = clampf(s_game.hit_flash / 0.16F, 0.0F, 1.0F);
    const float target_pos[3] = {s_game.drones[target].x, 0.9F,
                                 s_game.drones[target].z};
    nt_shape_renderer_line(
        cannon_l,
        (float[3]){target_pos[0] - 0.10F, target_pos[1] + 0.10F,
                   target_pos[2]},
        (float[4]){1.0F, 0.78F, 0.16F, 1.0F});
    nt_shape_renderer_line(
        cannon_r,
        (float[3]){target_pos[0] + 0.10F, target_pos[1] - 0.04F,
                   target_pos[2]},
        (float[4]){0.0F, 0.96F, 1.0F, 0.82F});
    nt_shape_renderer_line(
        (float[3]){cannon_l[0], cannon_l[1] + 0.04F, cannon_l[2]},
        (float[3]){target_pos[0], target_pos[1] + 0.18F, target_pos[2]},
        (float[4]){1.0F, 0.24F, 0.02F, 0.60F});
    nt_shape_renderer_sphere(
        (float[3]){target_pos[0], target_pos[1], target_pos[2]},
        0.24F + (0.18F * flash), COL_AMBER);
    nt_shape_renderer_sphere(cannon_l, 0.14F + (0.16F * flash), COL_WARNING);
    nt_shape_renderer_sphere(cannon_r, 0.14F + (0.16F * flash), COL_EMISSIVE);
    nt_shape_renderer_sphere_wire(cannon_l, 0.28F + (0.20F * flash),
                                  (float[4]){0.0F, 0.92F, 1.0F, 0.75F});
    nt_shape_renderer_sphere_wire(cannon_r, 0.28F + (0.20F * flash),
                                  (float[4]){1.0F, 0.70F, 0.08F, 0.70F});
  }
  if (s_game.rocket_flash > 0.0F) {
    const float fade = clampf(s_game.rocket_flash / 1.45F, 0.0F, 1.0F);
    for (int i = 0; i < MAX_DRONES; ++i) {
      if (s_game.drones[i].alive) {
        float pod_l[3] = {rocket_l[0], rocket_l[1], rocket_l[2]};
        float pod_r[3] = {rocket_r[0], rocket_r[1], rocket_r[2]};
        if (use_source_mech) {
          source_mech_rocket_point(root_x, root_z, yaw, root_scale, -1,
                                   (i & 1) ? 1 : -1, pod_l);
          source_mech_rocket_point(root_x, root_z, yaw, root_scale, 1,
                                   (i & 1) ? -1 : 1, pod_r);
        }
        nt_shape_renderer_line(
            pod_l,
            (float[3]){s_game.drones[i].x, 1.0F, s_game.drones[i].z},
            COL_AMBER);
        nt_shape_renderer_line(
            pod_r,
            (float[3]){s_game.drones[i].x, 1.0F, s_game.drones[i].z},
            (float[4]){0.0F, 0.92F, 1.0F, 1.0F});
        nt_shape_renderer_sphere(
            (float[3]){s_game.drones[i].x, 1.0F, s_game.drones[i].z},
            0.34F + (0.38F * fade), COL_AMBER);
        nt_shape_renderer_sphere_wire(
            (float[3]){s_game.drones[i].x, 1.0F, s_game.drones[i].z},
            0.56F + (0.44F * fade),
            (float[4]){0.0F, 0.92F, 1.0F, 0.62F});
      }
    }
    nt_shape_renderer_line(
        rocket_l,
        (float[3]){rocket_l[0] - 1.4F, rocket_l[1] - 0.42F,
                   rocket_l[2] - 3.2F},
        (float[4]){1.0F, 0.64F, 0.06F, fade});
    nt_shape_renderer_line(
        rocket_r,
        (float[3]){rocket_r[0] + 1.4F, rocket_r[1] - 0.42F,
                   rocket_r[2] - 3.2F},
        (float[4]){0.0F, 0.92F, 1.0F, fade});
    nt_shape_renderer_sphere(rocket_l, 0.20F + fade * 0.20F, COL_AMBER);
    nt_shape_renderer_sphere(rocket_r, 0.20F + fade * 0.20F, COL_EMISSIVE);
  }

  const float vent_power =
      clampf((s_game.heat - 0.45F) * 1.8F, 0.0F, 1.0F) +
      (s_game.hit_flash > 0.0F ? 0.35F : 0.0F) +
      (s_game.rocket_flash > 0.0F ? 0.55F : 0.0F);
  if (use_source_mech && vent_power > 0.02F) {
    for (int side = -1; side <= 1; side += 2) {
      float vent[3];
      source_mech_vent_point(root_x, root_z, yaw, root_scale, side, vent);
      for (int i = 0; i < 3; ++i) {
        const float offset = ((float)i - 1.0F) * 0.10F * root_scale;
        const float rise = (0.42F + (float)i * 0.18F) * root_scale;
        nt_shape_renderer_line(
            (float[3]){vent[0] + offset, vent[1], vent[2]},
            (float[3]){vent[0] + offset + ((float)side * 0.12F * vent_power),
                       vent[1] + rise, vent[2] + (0.10F * vent_power)},
            (float[4]){0.76F, 0.98F, 1.0F, 0.24F + (0.24F * vent_power)});
      }
    }
  }
}

static void draw_assault_motion_effects(void) {
  if (s_game.screen != SCREEN_BATTLE || !mesh_mech_ready()) {
    return;
  }

  float root_x = s_game.mech_x;
  float root_z = s_game.mech_z;
  float root_scale = 0.88F;
  float yaw = s_game.mech_facing;
  source_mech_pose(false, &root_x, &root_z, &root_scale, &yaw);

  const float speed = len2(s_game.mech_vx, s_game.mech_vz);
  const float move_t = clampf(speed / 4.0F, 0.0F, 1.0F);
  const float walk = s_game.mech_walk;
  const float step = sinf(walk);
  const float stomp = move_t * (0.35F + 0.65F * fabsf(step));
  const float attack_t = fmaxf(clampf(s_game.hit_flash / 0.16F, 0.0F, 1.0F),
                               clampf(s_game.rocket_flash / 1.45F, 0.0F, 1.0F));
  const float heat_t = clampf((s_game.heat - 0.25F) * 1.45F, 0.0F, 1.0F);
  const int target = target_drone();
  const float target_charge =
      target >= 0
          ? 0.26F +
                0.08F * (0.5F + 0.5F * sinf((float)nt_time_now() * 9.0F))
          : 0.0F;

  if (stomp > 0.04F) {
    const int active_row = step > 0.0F ? -1 : 1;
    const float stomp_ring[4] = {1.0F, 0.72F, 0.08F, 0.24F + stomp * 0.24F};
    const float cyan_ring[4] = {0.0F, 0.92F, 1.0F, 0.16F + stomp * 0.20F};
    for (int side = -1; side <= 1; side += 2) {
      float foot[3];
      source_mech_foot_point(root_x, root_z, yaw, root_scale, side, active_row,
                             foot);
      nt_shape_renderer_circle_wire(
          (float[3]){foot[0], 0.105F, foot[2]},
          (0.34F + stomp * 0.26F) * root_scale, stomp_ring);
      nt_shape_renderer_circle_wire(
          (float[3]){foot[0], 0.13F, foot[2]},
          (0.20F + stomp * 0.18F) * root_scale, cyan_ring);
      nt_shape_renderer_sphere(
          (float[3]){foot[0], 0.17F + stomp * 0.05F, foot[2]},
          (0.06F + stomp * 0.07F) * root_scale,
          (float[4]){0.86F, 1.0F, 0.62F, 0.22F + stomp * 0.22F});
    }
  }

  if (attack_t > 0.02F || heat_t > 0.05F || target_charge > 0.02F) {
    float cannon_l[3];
    float cannon_r[3];
    source_mech_cannon_points(root_x, root_z, yaw, root_scale, cannon_l,
                              cannon_r);
    const float glow = fmaxf(fmaxf(attack_t, heat_t * 0.55F), target_charge);
    const float orange[4] = {1.0F, 0.55F, 0.04F, 0.28F + glow * 0.34F};
    const float blue[4] = {0.0F, 0.92F, 1.0F, 0.26F + glow * 0.32F};
    nt_shape_renderer_sphere(cannon_l, (0.14F + glow * 0.20F) * root_scale,
                             orange);
    nt_shape_renderer_sphere(cannon_r, (0.14F + glow * 0.20F) * root_scale,
                             blue);
    nt_shape_renderer_circle_wire(cannon_l, (0.26F + glow * 0.18F) * root_scale,
                                  blue);
    nt_shape_renderer_circle_wire(cannon_r, (0.26F + glow * 0.18F) * root_scale,
                                  orange);
    if (target >= 0) {
      const float target_pos[3] = {s_game.drones[target].x, 0.92F,
                                   s_game.drones[target].z};
      nt_shape_renderer_line(cannon_l, target_pos,
                             (float[4]){1.0F, 0.58F, 0.04F,
                                        0.18F + target_charge * 0.30F});
      nt_shape_renderer_line(cannon_r,
                             (float[3]){target_pos[0] + 0.14F,
                                        target_pos[1] + 0.06F,
                                        target_pos[2]},
                             (float[4]){0.0F, 0.92F, 1.0F,
                                        0.16F + target_charge * 0.28F});
    }

    for (int side = -1; side <= 1; side += 2) {
      float vent[3];
      source_mech_vent_point(root_x, root_z, yaw, root_scale, side, vent);
      nt_shape_renderer_line(
          vent,
          (float[3]){vent[0] + (float)side * 0.28F * root_scale,
                     vent[1] + (0.70F + glow * 0.34F) * root_scale,
                     vent[2] + 0.16F * root_scale},
          (float[4]){0.76F, 0.98F, 1.0F, 0.16F + glow * 0.24F});
    }
  }
}

static void build_frame_uniforms(float w, float h, bool hangar,
                                 nt_frame_uniforms_t *uniforms,
                                 float out_vp[16]) {
  const float aspect = h > 0.0F ? w / h : 1.777F;
  const vec3 eye = {hangar ? 3.6F : 5.1F, hangar ? 3.7F : 5.7F,
                    hangar ? 6.8F : 8.8F};
  const vec3 center = {hangar ? 0.0F : 0.10F, hangar ? 1.75F : 1.16F,
                       hangar ? 0.2F : 0.46F};
  const vec3 up = {0.0F, 1.0F, 0.0F};
  mat4 view;
  mat4 proj;
  mat4 vp;
  glm_lookat((vec3){eye[0], eye[1], eye[2]},
             (vec3){center[0], center[1], center[2]},
             (vec3){up[0], up[1], up[2]}, view);
  glm_perspective(glm_rad(hangar ? 48.0F : 51.0F), aspect, 0.1F, 80.0F, proj);
  glm_mat4_mul(proj, view, vp);

  if (out_vp) {
    memcpy(out_vp, vp, sizeof(float) * 16);
  }
  if (uniforms) {
    memset(uniforms, 0, sizeof(*uniforms));
    memcpy(uniforms->view_proj, vp, 64);
    memcpy(uniforms->view, view, 64);
    memcpy(uniforms->proj, proj, 64);
    uniforms->camera_pos[0] = eye[0];
    uniforms->camera_pos[1] = eye[1];
    uniforms->camera_pos[2] = eye[2];
    uniforms->time[0] = (float)nt_time_now();
    uniforms->time[1] = g_nt_app.dt;
    if (w > 0.0F && h > 0.0F) {
      uniforms->resolution[0] = w;
      uniforms->resolution[1] = h;
      uniforms->resolution[2] = 1.0F / w;
      uniforms->resolution[3] = 1.0F / h;
    }
    uniforms->near_far[0] = 0.1F;
    uniforms->near_far[1] = 80.0F;
  }
}

static void setup_perspective(float w, float h, bool hangar) {
  float vp[16];
  build_frame_uniforms(w, h, hangar, NULL, vp);
  const vec3 eye = {hangar ? 3.6F : 5.1F, hangar ? 3.7F : 5.7F,
                    hangar ? 6.8F : 8.8F};
  nt_shape_renderer_set_vp((float *)vp);
  nt_shape_renderer_set_cam_pos((float[3]){eye[0], eye[1], eye[2]});
  nt_shape_renderer_set_depth(true);
}

static void setup_ortho(float w, float h) {
  float vp[16];
  ortho(0.0F, w, 0.0F, h, -1.0F, 1.0F, vp);
  nt_shape_renderer_set_vp(vp);
  nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
  nt_shape_renderer_set_depth(false);
}

static void bind_ui_frame_uniforms(float w, float h) {
  nt_frame_uniforms_t uniforms;
  memset(&uniforms, 0, sizeof(uniforms));
  ortho(0.0F, w, 0.0F, h, -1.0F, 1.0F, uniforms.view_proj);
  glm_mat4_identity((vec4 *)uniforms.view);
  glm_mat4_identity((vec4 *)uniforms.proj);
  uniforms.camera_pos[2] = 1.0F;
  uniforms.time[0] = (float)nt_time_now();
  uniforms.time[1] = g_nt_app.dt;
  if (w > 0.0F && h > 0.0F) {
    uniforms.resolution[0] = w;
    uniforms.resolution[1] = h;
    uniforms.resolution[2] = 1.0F / w;
    uniforms.resolution[3] = 1.0F / h;
  }
  uniforms.near_far[0] = -1.0F;
  uniforms.near_far[1] = 1.0F;
  nt_gfx_update_buffer(s_mesh_mech.frame_ubo, &uniforms, sizeof(uniforms));
  nt_gfx_bind_uniform_buffer(s_mesh_mech.frame_ubo, 0);
}

static bool mesh_mech_ready(void) {
  if (!s_mesh_mech.initialized) {
    return false;
  }
  const nt_material_info_t *mat_info =
      nt_material_get_info(s_mesh_mech.material);
  if (!mat_info || !mat_info->ready) {
    return false;
  }
  const nt_material_info_t *robot_mat_info =
      nt_material_get_info(s_mesh_mech.robot_material);
  if (!robot_mat_info || !robot_mat_info->ready) {
    return false;
  }
  const nt_material_info_t *world_mat_info =
      nt_material_get_info(s_mesh_mech.world_material);
  if (!world_mat_info || !world_mat_info->ready) {
    return false;
  }
  const nt_material_info_t *solid_mat_info =
      nt_material_get_info(s_mesh_mech.solid_material);
  if (!solid_mat_info || !solid_mat_info->ready) {
    return false;
  }
  const nt_material_info_t *station_mat_info =
      nt_material_get_info(s_mesh_mech.station_material);
  if (!station_mat_info || !station_mat_info->ready) {
    return false;
  }
  if (!nt_resource_is_ready(s_mesh_mech.mech_atlas)) {
    return false;
  }
  if (!nt_resource_is_ready(s_mesh_mech.world_texture)) {
    return false;
  }
  for (int i = 0; i < MECH_MESH_TYPES; ++i) {
    if (!nt_resource_is_ready(s_mesh_mech.meshes[i])) {
      return false;
    }
  }
  return true;
}

static void init_mesh_mech(void) {
  if (s_mesh_mech.initialized) {
    return;
  }

  nt_hash_init(&(nt_hash_desc_t){0});
  nt_resource_init(&(nt_resource_desc_t){0});
  nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture,
                            nt_gfx_deactivate_texture);
  nt_resource_set_activator(NT_ASSET_MESH, nt_gfx_activate_mesh,
                            nt_gfx_deactivate_mesh);
  nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader,
                            nt_gfx_deactivate_shader);
  nt_resource_set_activate_time_budget(0);

  nt_font_init(&(nt_font_desc_t){.max_fonts = 2});
  nt_material_desc_t mat_desc = nt_material_desc_defaults();
  nt_material_init(&mat_desc);
  nt_entity_init(&(nt_entity_desc_t){.max_entities = MECH_MESH_ENTITY_CAPACITY});
  nt_transform_comp_init(
      &(nt_transform_comp_desc_t){.capacity = MECH_MESH_ENTITY_CAPACITY});
  nt_mesh_comp_init(&(nt_mesh_comp_desc_t){.capacity = MECH_MESH_ENTITY_CAPACITY});
  nt_material_comp_init(
      &(nt_material_comp_desc_t){.capacity = MECH_MESH_ENTITY_CAPACITY});
  nt_drawable_comp_init(
      &(nt_drawable_comp_desc_t){.capacity = MECH_MESH_ENTITY_CAPACITY});

  nt_mesh_renderer_desc_t mesh_desc = nt_mesh_renderer_desc_defaults();
  nt_mesh_renderer_init(&mesh_desc);
  nt_text_renderer_init();

  s_mesh_mech.pack_id = nt_hash32_str("mech_builder_battler_mesh");
  for (int i = 0; i < MECH_MESH_TYPES; ++i) {
    s_mesh_mech.meshes[i] = nt_resource_request(
        nt_hash64_str(MESH_MECH_RESOURCE_PATHS[i]), NT_ASSET_MESH);
  }
  s_mesh_mech.vs =
      nt_resource_request(nt_hash64_str("assets/shaders/mech_mesh_inst.vert"),
                          NT_ASSET_SHADER_CODE);
  s_mesh_mech.fs =
      nt_resource_request(nt_hash64_str("assets/shaders/mech_mesh_inst.frag"),
                          NT_ASSET_SHADER_CODE);
  s_mesh_mech.robot_fs = nt_resource_request(
      nt_hash64_str("assets/shaders/mech_mesh_color_inst.frag"),
      NT_ASSET_SHADER_CODE);
  s_mesh_mech.solid_vs = nt_resource_request(
      nt_hash64_str("assets/shaders/mech_mesh_solid_inst.vert"),
      NT_ASSET_SHADER_CODE);
  s_mesh_mech.solid_fs = nt_resource_request(
      nt_hash64_str("assets/shaders/mech_mesh_solid_inst.frag"),
      NT_ASSET_SHADER_CODE);
  s_mesh_mech.station_fs = nt_resource_request(
      nt_hash64_str("assets/shaders/mech_mesh_station_inst.frag"),
      NT_ASSET_SHADER_CODE);
  s_mesh_mech.text_vs = nt_resource_request(
      nt_hash64_str("assets/shaders/slug_text.vert"), NT_ASSET_SHADER_CODE);
  s_mesh_mech.text_fs = nt_resource_request(
      nt_hash64_str("assets/shaders/slug_text.frag"), NT_ASSET_SHADER_CODE);
  s_mesh_mech.mech_atlas = nt_resource_request(
      nt_hash64_str("assets/textures/poly_pizza_quaternius_mech_toy_atlas.png"),
      NT_ASSET_TEXTURE);
  s_mesh_mech.world_texture = nt_resource_request(
      nt_hash64_str(
          "assets/textures/mech_builder_battler_stylized_studs_grass_tile_v1.png"),
      NT_ASSET_TEXTURE);
  s_mesh_mech.ui_font_res =
      nt_resource_request(nt_hash64_str("mech/ui_font"), NT_ASSET_FONT);
  s_mesh_mech.material = nt_material_create(&(nt_material_create_desc_t){
      .vs = s_mesh_mech.vs,
      .fs = s_mesh_mech.fs,
      .textures =
          {
              {.name = "u_mech_texture", .resource = s_mesh_mech.mech_atlas},
          },
      .texture_count = 1,
      .attr_map =
          {
              {.stream_name = "position", .location = 0},
              {.stream_name = "normal", .location = 1},
              {.stream_name = "uv0", .location = 2},
          },
      .attr_map_count = 3,
      .depth_test = true,
      .depth_write = true,
      .cull_mode = NT_CULL_NONE,
      .color_mode = NT_COLOR_MODE_FLOAT4,
      .label = "mech_starter_mesh_parts",
  });
  s_mesh_mech.world_material = nt_material_create(&(nt_material_create_desc_t){
      .vs = s_mesh_mech.vs,
      .fs = s_mesh_mech.fs,
      .textures =
          {
              {.name = "u_mech_texture", .resource = s_mesh_mech.world_texture},
          },
      .texture_count = 1,
      .attr_map =
          {
              {.stream_name = "position", .location = 0},
              {.stream_name = "normal", .location = 1},
              {.stream_name = "uv0", .location = 2},
          },
      .attr_map_count = 3,
      .depth_test = true,
      .depth_write = true,
      .cull_mode = NT_CULL_NONE,
      .color_mode = NT_COLOR_MODE_FLOAT4,
      .label = "world_stylized_studs_texture",
  });
  s_mesh_mech.robot_material = nt_material_create(&(nt_material_create_desc_t){
      .vs = s_mesh_mech.vs,
      .fs = s_mesh_mech.robot_fs,
      .attr_map =
          {
              {.stream_name = "position", .location = 0},
              {.stream_name = "normal", .location = 1},
              {.stream_name = "uv0", .location = 2},
          },
      .attr_map_count = 3,
      .depth_test = true,
      .depth_write = true,
      .cull_mode = NT_CULL_NONE,
      .color_mode = NT_COLOR_MODE_FLOAT4,
      .label = "robot_enemy_mesh_parts",
  });
  s_mesh_mech.solid_material = nt_material_create(&(nt_material_create_desc_t){
      .vs = s_mesh_mech.solid_vs,
      .fs = s_mesh_mech.solid_fs,
      .attr_map =
          {
              {.stream_name = "position", .location = 0},
              {.stream_name = "normal", .location = 1},
          },
      .attr_map_count = 2,
      .depth_test = true,
      .depth_write = true,
      .cull_mode = NT_CULL_NONE,
      .color_mode = NT_COLOR_MODE_FLOAT4,
      .label = "hero_modular_overlay_parts",
  });
  s_mesh_mech.station_material = nt_material_create(&(nt_material_create_desc_t){
      .vs = s_mesh_mech.solid_vs,
      .fs = s_mesh_mech.station_fs,
      .attr_map =
          {
              {.stream_name = "position", .location = 0},
              {.stream_name = "normal", .location = 1},
          },
      .attr_map_count = 2,
      .depth_test = true,
      .depth_write = true,
      .cull_mode = NT_CULL_NONE,
      .color_mode = NT_COLOR_MODE_FLOAT4,
      .label = "kenney_station_plastic_props",
  });
  s_mesh_mech.text_material = nt_material_create(&(nt_material_create_desc_t){
      .vs = s_mesh_mech.text_vs,
      .fs = s_mesh_mech.text_fs,
      .blend_mode = NT_BLEND_MODE_ALPHA,
      .depth_test = false,
      .depth_write = false,
      .cull_mode = NT_CULL_NONE,
      .params[0] = {.name = "u_alpha_cutoff",
                    .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
      .param_count = 1,
      .label = "mech_ui_text",
  });
  s_mesh_mech.ui_font = nt_font_create(&(nt_font_create_desc_t){
      .curve_texture_width = 1024,
      .curve_texture_height = 512,
      .band_texture_height = 256,
      .band_count = 8,
      .measure_cache_size = 256,
  });
  nt_font_add(s_mesh_mech.ui_font, s_mesh_mech.ui_font_res);

  s_mesh_mech.world_floor = nt_entity_create();
  nt_transform_comp_add(s_mesh_mech.world_floor);
  nt_mesh_comp_add(s_mesh_mech.world_floor);
  nt_material_comp_add(s_mesh_mech.world_floor);
  nt_drawable_comp_add(s_mesh_mech.world_floor);
  *nt_material_comp_handle(s_mesh_mech.world_floor) = s_mesh_mech.world_material;

  for (int i = 0; i < MECH_MESH_PARTS; ++i) {
    s_mesh_mech.parts[i] = nt_entity_create();
    nt_transform_comp_add(s_mesh_mech.parts[i]);
    nt_mesh_comp_add(s_mesh_mech.parts[i]);
    nt_material_comp_add(s_mesh_mech.parts[i]);
    nt_drawable_comp_add(s_mesh_mech.parts[i]);
    *nt_material_comp_handle(s_mesh_mech.parts[i]) = s_mesh_mech.material;
  }
  for (int part = 0; part < ASSAULT_WALKER_MESH_PARTS; ++part) {
    s_mesh_mech.assault_hero[part] = nt_entity_create();
    nt_transform_comp_add(s_mesh_mech.assault_hero[part]);
    nt_mesh_comp_add(s_mesh_mech.assault_hero[part]);
    nt_material_comp_add(s_mesh_mech.assault_hero[part]);
    nt_drawable_comp_add(s_mesh_mech.assault_hero[part]);
    *nt_material_comp_handle(s_mesh_mech.assault_hero[part]) =
        s_mesh_mech.robot_material;
  }
  for (int part = 0; part < SENTINEL_SHOWCASE_MESH_PARTS; ++part) {
    s_mesh_mech.sentinel_showcase[part] = nt_entity_create();
    nt_transform_comp_add(s_mesh_mech.sentinel_showcase[part]);
    nt_mesh_comp_add(s_mesh_mech.sentinel_showcase[part]);
    nt_material_comp_add(s_mesh_mech.sentinel_showcase[part]);
    nt_drawable_comp_add(s_mesh_mech.sentinel_showcase[part]);
    *nt_material_comp_handle(s_mesh_mech.sentinel_showcase[part]) =
        s_mesh_mech.robot_material;
  }
  for (int part = 0; part < KENNEY_WORLD_PROP_INSTANCES; ++part) {
    s_mesh_mech.kenney_world_props[part] = nt_entity_create();
    nt_transform_comp_add(s_mesh_mech.kenney_world_props[part]);
    nt_mesh_comp_add(s_mesh_mech.kenney_world_props[part]);
    nt_material_comp_add(s_mesh_mech.kenney_world_props[part]);
    nt_drawable_comp_add(s_mesh_mech.kenney_world_props[part]);
    *nt_material_comp_handle(s_mesh_mech.kenney_world_props[part]) =
        s_mesh_mech.station_material;
  }
  for (int i = 0; i < MAX_DRONES; ++i) {
    for (int part = 0; part < ROBOT_ENEMY_MESH_PARTS; ++part) {
      s_mesh_mech.enemy_robots[i][part] = nt_entity_create();
      nt_transform_comp_add(s_mesh_mech.enemy_robots[i][part]);
      nt_mesh_comp_add(s_mesh_mech.enemy_robots[i][part]);
      nt_material_comp_add(s_mesh_mech.enemy_robots[i][part]);
      nt_drawable_comp_add(s_mesh_mech.enemy_robots[i][part]);
      *nt_material_comp_handle(s_mesh_mech.enemy_robots[i][part]) =
          s_mesh_mech.robot_material;
    }
  }

  s_mesh_mech.frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
      .type = NT_BUFFER_UNIFORM,
      .usage = NT_USAGE_DYNAMIC,
      .size = sizeof(nt_frame_uniforms_t),
      .label = "mech_frame_uniforms",
  });

  nt_resource_mount(s_mesh_mech.pack_id, 100);
#ifdef NT_CDN_URL
  nt_resource_load_auto(s_mesh_mech.pack_id,
                        NT_CDN_URL "/mech_builder_battler_mesh.ntpack");
#elif defined(MECH_BUILDER_PACK_PATH)
  nt_resource_load_auto(s_mesh_mech.pack_id, MECH_BUILDER_PACK_PATH);
#else
  nt_resource_load_auto(s_mesh_mech.pack_id,
                        "assets/mech_builder_battler_mesh.ntpack");
#endif

  s_mesh_mech.initialized = true;
}

static void step_mesh_mech(void) {
  if (!s_mesh_mech.initialized) {
    return;
  }
  nt_resource_step();
  nt_material_step();
  nt_font_step();
  if (!s_mesh_mech.pack_logged &&
      nt_resource_pack_state(s_mesh_mech.pack_id) == NT_PACK_STATE_READY) {
    s_mesh_mech.pack_logged = true;
  }
}

static void shutdown_mesh_mech(void) {
  if (!s_mesh_mech.initialized) {
    return;
  }
  nt_text_renderer_shutdown();
  nt_mesh_renderer_shutdown();
  nt_drawable_comp_shutdown();
  nt_material_comp_shutdown();
  nt_mesh_comp_shutdown();
  nt_transform_comp_shutdown();
  nt_entity_shutdown();
  nt_material_destroy(s_mesh_mech.material);
  nt_material_destroy(s_mesh_mech.world_material);
  nt_material_destroy(s_mesh_mech.robot_material);
  nt_material_destroy(s_mesh_mech.solid_material);
  nt_material_destroy(s_mesh_mech.station_material);
  nt_material_destroy(s_mesh_mech.text_material);
  nt_font_destroy(s_mesh_mech.ui_font);
  nt_font_shutdown();
  nt_material_shutdown();
  nt_resource_shutdown();
  nt_hash_shutdown();
  nt_gfx_destroy_buffer(s_mesh_mech.frame_ubo);
  memset(&s_mesh_mech, 0, sizeof(s_mesh_mech));
}

static void append_assault_hero_items(float root_x, float root_z, float root_scale,
                                      float yaw, float move_t, float walk,
                                      float idle_bob, float recoil,
                                      float strafe_lean, float forward_lean,
                                      uint32_t *item_count) {
  const MeshPartMesh assault_meshes[ASSAULT_WALKER_MESH_PARTS] = {
      MECH_MESH_ASSAULT_WALKER_GREEN,
      MECH_MESH_ASSAULT_WALKER_GREEN_AO,
      MECH_MESH_ASSAULT_WALKER_GREEN_UV,
      MECH_MESH_ASSAULT_WALKER_GREY_A,
      MECH_MESH_ASSAULT_WALKER_GLASS,
      MECH_MESH_ASSAULT_WALKER_BLACK,
      MECH_MESH_ASSAULT_WALKER_BLACK_UV,
      MECH_MESH_ASSAULT_WALKER_GREY_B,
      MECH_MESH_ASSAULT_WALKER_DARK,
      MECH_MESH_ASSAULT_WALKER_GREY_C,
      MECH_MESH_ASSAULT_WALKER_GREY_C_NONE,
      MECH_MESH_ASSAULT_WALKER_GREY_D,
      MECH_MESH_ASSAULT_WALKER_GREY_D_NONE,
  };
  const float assault_colors[ASSAULT_WALKER_MESH_PARTS][4] = {
      {0.08F, 0.88F, 0.36F, 1.0F}, {0.05F, 0.66F, 0.25F, 1.0F},
      {0.30F, 1.00F, 0.52F, 1.0F}, {0.88F, 0.95F, 0.92F, 1.0F},
      {0.12F, 0.96F, 1.00F, 0.96F}, {0.035F, 0.045F, 0.052F, 1.0F},
      {0.025F, 0.034F, 0.042F, 1.0F}, {0.76F, 0.86F, 0.86F, 1.0F},
      {0.08F, 0.12F, 0.15F, 1.0F}, {0.98F, 0.92F, 0.70F, 1.0F},
      {0.90F, 0.96F, 0.94F, 1.0F}, {0.92F, 0.70F, 0.24F, 1.0F},
      {0.80F, 0.90F, 0.90F, 1.0F},
  };
  const float assault_scale = root_scale * 0.16F;
  const float source_min_y = -8.9889545F;
  const float step = sinf(walk);
  const float stomp = move_t * fabsf(step);
  const float attack_t = fmaxf(clampf(s_game.hit_flash / 0.16F, 0.0F, 1.0F),
                               clampf(s_game.rocket_flash / 1.45F, 0.0F, 1.0F));
  const float heat_t = clampf((s_game.heat - 0.30F) * 1.35F, 0.0F, 1.0F);
  const float bob =
      idle_bob * root_scale + (move_t * 0.018F) - (stomp * 0.020F);
  const float model_y = 0.05F - (source_min_y * assault_scale) + bob;
  const float model_z =
      root_z + recoil * root_scale * 0.16F - (move_t * step * root_scale * 0.025F);
  const float roll = strafe_lean * 0.46F + step * move_t * 0.055F;
  const float pitch = forward_lean * 0.32F - recoil * 0.14F + stomp * 0.050F;
  const float squash = 1.0F - stomp * 0.035F + attack_t * 0.018F;
  const float spread = 1.0F + stomp * 0.020F + attack_t * 0.012F;
  for (int part = 0; part < ASSAULT_WALKER_MESH_PARTS; ++part) {
    const uint32_t mesh_id = nt_resource_get(s_mesh_mech.meshes[assault_meshes[part]]);
    nt_entity_t entity = s_mesh_mech.assault_hero[part];
    float *pos = nt_transform_comp_position(entity);
    pos[0] = root_x;
    pos[1] = model_y;
    pos[2] = model_z;
    q_pose(yaw + 3.14159265F, pitch, roll, nt_transform_comp_rotation(entity));
    float *scl = nt_transform_comp_scale(entity);
    scl[0] = assault_scale * spread;
    scl[1] = assault_scale * squash;
    scl[2] = assault_scale * (spread + recoil * 0.018F);
    *nt_transform_comp_dirty(entity) = true;
    *nt_mesh_comp_handle(entity) = (nt_mesh_t){.id = mesh_id};
    *nt_material_comp_handle(entity) = s_mesh_mech.robot_material;
    float r = assault_colors[part][0];
    float g = assault_colors[part][1];
    float b = assault_colors[part][2];
    const float combat_glow = attack_t * 0.24F + heat_t * 0.14F;
    const float heat_pulse = heat_t * (0.35F + 0.65F * fabsf(sinf((float)nt_time_now() * 6.0F)));
    if (part == 4) {
      r = fminf(r + combat_glow * 0.38F, 1.0F);
      g = fminf(g + combat_glow * 0.58F, 1.0F);
      b = fminf(b + combat_glow * 0.95F, 1.0F);
    } else if (part == 2 || part == 6) {
      r = fminf(r + combat_glow * 0.30F, 1.0F);
      g = fminf(g + combat_glow * 0.48F, 1.0F);
      b = fminf(b + combat_glow * 0.74F, 1.0F);
    } else if (part == 5 || part == 8) {
      r = fminf(r + heat_pulse * 0.34F, 1.0F);
      g = fminf(g + heat_pulse * 0.16F, 1.0F);
      b = fminf(b + heat_pulse * 0.06F, 1.0F);
    } else if (part == 9 || part == 11) {
      r = fminf(r + attack_t * 0.28F, 1.0F);
      g = fminf(g + attack_t * 0.20F, 1.0F);
      b = fmaxf(b - attack_t * 0.10F, 0.0F);
    } else if (part <= 2) {
      r = fminf(r + combat_glow * 0.12F, 1.0F);
      g = fminf(g + combat_glow * 0.22F, 1.0F);
      b = fminf(b + combat_glow * 0.14F, 1.0F);
    }
    nt_drawable_comp_set_color(entity, r, g, b, assault_colors[part][3]);
    s_mesh_mech.items[*item_count].sort_key =
        nt_sort_key_opaque(s_mesh_mech.robot_material.id, mesh_id);
    s_mesh_mech.items[*item_count].entity = entity.id;
    s_mesh_mech.items[*item_count].batch_key =
        nt_batch_key(s_mesh_mech.robot_material.id, mesh_id);
    (*item_count)++;
  }
}

static void append_sentinel_showcase_items(float time, uint32_t *item_count) {
  const MeshPartMesh sentinel_meshes[SENTINEL_SHOWCASE_MESH_PARTS] = {
      MECH_MESH_SENTINEL_MAT18, MECH_MESH_SENTINEL_MAT16,
      MECH_MESH_SENTINEL_MAT9,  MECH_MESH_SENTINEL_MAT17,
      MECH_MESH_SENTINEL_MAT13, MECH_MESH_SENTINEL_MAT21,
      MECH_MESH_SENTINEL_MAT20, MECH_MESH_SENTINEL_MAT15,
      MECH_MESH_SENTINEL_MAT12, MECH_MESH_SENTINEL_MAT23,
  };
  const float sentinel_colors[SENTINEL_SHOWCASE_MESH_PARTS][4] = {
      {0.88F, 0.92F, 0.96F, 1.0F}, {0.18F, 0.28F, 0.92F, 1.0F},
      {0.04F, 0.08F, 0.15F, 1.0F}, {1.00F, 0.72F, 0.08F, 1.0F},
      {0.20F, 0.86F, 1.00F, 1.0F}, {0.98F, 0.18F, 0.14F, 1.0F},
      {0.55F, 0.62F, 0.70F, 1.0F}, {0.12F, 0.16F, 0.22F, 1.0F},
      {0.72F, 0.82F, 0.88F, 1.0F}, {0.98F, 0.98F, 0.92F, 1.0F},
  };
  const float scale = 0.55F;
  const float min_y = -2.64851F;
  const float bob = sinf(time * 1.7F) * 0.018F;
  const float model_x = -4.35F;
  const float model_y = 0.06F - (min_y * scale) + bob;
  const float model_z = 0.72F;
  const float yaw = -0.72F + sinf(time * 0.45F) * 0.035F;
  for (int part = 0; part < SENTINEL_SHOWCASE_MESH_PARTS; ++part) {
    const uint32_t mesh_id = nt_resource_get(s_mesh_mech.meshes[sentinel_meshes[part]]);
    nt_entity_t entity = s_mesh_mech.sentinel_showcase[part];
    float *pos = nt_transform_comp_position(entity);
    pos[0] = model_x;
    pos[1] = model_y;
    pos[2] = model_z;
    q_pose(yaw, 0.0F, 0.0F, nt_transform_comp_rotation(entity));
    float *scl = nt_transform_comp_scale(entity);
    scl[0] = scale;
    scl[1] = scale;
    scl[2] = scale;
    *nt_transform_comp_dirty(entity) = true;
    *nt_mesh_comp_handle(entity) = (nt_mesh_t){.id = mesh_id};
    *nt_material_comp_handle(entity) = s_mesh_mech.robot_material;
    nt_drawable_comp_set_color(entity, sentinel_colors[part][0],
                               sentinel_colors[part][1],
                               sentinel_colors[part][2],
                               sentinel_colors[part][3]);
    s_mesh_mech.items[*item_count].sort_key =
        nt_sort_key_opaque(s_mesh_mech.robot_material.id, mesh_id);
    s_mesh_mech.items[*item_count].entity = entity.id;
    s_mesh_mech.items[*item_count].batch_key =
        nt_batch_key(s_mesh_mech.robot_material.id, mesh_id);
    (*item_count)++;
  }
}

static void append_kenney_world_prop_items(bool hangar, uint32_t *item_count) {
  const MeshPartMesh prop_meshes[KENNEY_WORLD_PROP_INSTANCES] = {
      MECH_MESH_KENNEY_SPACE_GATE,
      MECH_MESH_KENNEY_SPACE_CORRIDOR_WIDE,
      MECH_MESH_KENNEY_SPACE_ROOM_SMALL,
      MECH_MESH_KENNEY_SPACE_GATE,
      MECH_MESH_KENNEY_SPACE_CORRIDOR_WIDE,
      MECH_MESH_KENNEY_SPACE_ROOM_SMALL,
      MECH_MESH_KENNEY_SPACE_GATE,
      MECH_MESH_KENNEY_SPACE_CORRIDOR_WIDE,
  };
  const float prop_pos[KENNEY_WORLD_PROP_INSTANCES][3] = {
      {4.95F, 0.48F, -3.55F},
      {-5.25F, 0.38F, -4.50F},
      {0.0F, 0.34F, 5.80F},
      {-6.45F, 0.44F, 1.82F},
      {5.75F, 0.36F, 2.92F},
      {7.20F, 0.32F, -1.25F},
      {-7.25F, 0.42F, -1.25F},
      {0.35F, 0.35F, -5.95F},
  };
  const float prop_scale[KENNEY_WORLD_PROP_INSTANCES][3] = {
      {0.40F, 0.40F, 0.40F},
      {0.30F, 0.30F, 0.30F},
      {0.28F, 0.28F, 0.28F},
      {0.33F, 0.33F, 0.33F},
      {0.23F, 0.23F, 0.23F},
      {0.20F, 0.20F, 0.20F},
      {0.28F, 0.28F, 0.28F},
      {0.22F, 0.22F, 0.22F},
  };
  const float prop_yaw[KENNEY_WORLD_PROP_INSTANCES] = {
      0.72F, -0.55F, 3.14159265F, 2.18F, -2.56F, -1.34F, 1.28F, 0.05F};
  const float prop_colors[KENNEY_WORLD_PROP_INSTANCES][4] = {
      {0.84F, 0.94F, 1.0F, 1.0F},
      {0.22F, 0.44F, 0.95F, 1.0F},
      {0.90F, 0.98F, 0.92F, 1.0F},
      {0.98F, 0.92F, 0.70F, 1.0F},
      {0.08F, 0.34F, 1.0F, 1.0F},
      {0.92F, 0.98F, 1.0F, 1.0F},
      {0.72F, 0.98F, 0.86F, 1.0F},
      {0.98F, 0.72F, 0.24F, 1.0F},
  };
  for (int part = 0; part < KENNEY_WORLD_PROP_INSTANCES; ++part) {
    const uint32_t mesh_id = nt_resource_get(s_mesh_mech.meshes[prop_meshes[part]]);
    nt_entity_t entity = s_mesh_mech.kenney_world_props[part];
    float *pos = nt_transform_comp_position(entity);
    pos[0] = prop_pos[part][0];
    pos[1] = prop_pos[part][1] + (hangar ? 0.0F : -0.06F);
    pos[2] = prop_pos[part][2];
    q_pose(prop_yaw[part], 0.0F, 0.0F, nt_transform_comp_rotation(entity));
    float *scl = nt_transform_comp_scale(entity);
    scl[0] = prop_scale[part][0];
    scl[1] = prop_scale[part][1];
    scl[2] = prop_scale[part][2];
    *nt_transform_comp_dirty(entity) = true;
    *nt_mesh_comp_handle(entity) = (nt_mesh_t){.id = mesh_id};
    *nt_material_comp_handle(entity) = s_mesh_mech.station_material;
    nt_drawable_comp_set_color(entity, prop_colors[part][0], prop_colors[part][1],
                               prop_colors[part][2], prop_colors[part][3]);
    s_mesh_mech.items[*item_count].sort_key =
        nt_sort_key_opaque(s_mesh_mech.station_material.id, mesh_id);
    s_mesh_mech.items[*item_count].entity = entity.id;
    s_mesh_mech.items[*item_count].batch_key =
        nt_batch_key(s_mesh_mech.station_material.id, mesh_id);
    (*item_count)++;
  }
}

static void draw_mesh_mech(float w, float h) {
  if (!mesh_mech_ready()) {
    return;
  }

  const bool hangar = s_game.screen != SCREEN_BATTLE;
  const bool rockets = s_game.rockets_equipped;
  const float time = (float)nt_time_now();
  float root_x = 0.0F;
  float root_z = 0.0F;
  float root_scale = 1.0F;
  float yaw = 0.0F;
  source_mech_pose(hangar, &root_x, &root_z, &root_scale, &yaw);
  const float speed = hangar ? 0.0F : len2(s_game.mech_vx, s_game.mech_vz);
  const float move_t = clampf(speed / 4.0F, 0.0F, 1.0F);
  const float walk = hangar ? time * 1.25F : s_game.mech_walk;
  const float recoil = hangar ? 0.0F : s_game.mech_recoil;
  const float yaw_sin = sinf(yaw);
  const float yaw_cos = cosf(yaw);
  const float idle_bob = sinf(walk * 1.55F) * (hangar ? 0.035F : 0.018F);
  const float step = sinf(walk);
  const float step_alt = sinf(walk + 3.14159265F);
  const float strafe_lean =
      hangar ? 0.0F : clampf(s_game.mech_vx * -0.055F, -0.18F, 0.18F);
  const float forward_lean =
      hangar ? 0.0F : clampf(s_game.mech_vz * 0.035F, -0.12F, 0.12F);
  const bool use_assault_hero = true;
  const bool use_source_hero = !use_assault_hero;
  const int source_hero_index = MECH_MESH_PARTS - 1;

  uint32_t item_count = 0;
  const uint32_t floor_mesh_id =
      nt_resource_get(s_mesh_mech.meshes[MECH_MESH_WORLD_STUDS_FLOOR]);
  float *floor_pos = nt_transform_comp_position(s_mesh_mech.world_floor);
  floor_pos[0] = 0.0F;
  floor_pos[1] = 0.012F;
  floor_pos[2] = 0.0F;
  q_pose(0.0F, 0.0F, 0.0F, nt_transform_comp_rotation(s_mesh_mech.world_floor));
  float *floor_scl = nt_transform_comp_scale(s_mesh_mech.world_floor);
  floor_scl[0] = 1.0F;
  floor_scl[1] = 1.0F;
  floor_scl[2] = 1.0F;
  *nt_transform_comp_dirty(s_mesh_mech.world_floor) = true;
  *nt_mesh_comp_handle(s_mesh_mech.world_floor) = (nt_mesh_t){.id = floor_mesh_id};
  *nt_material_comp_handle(s_mesh_mech.world_floor) = s_mesh_mech.world_material;
  nt_drawable_comp_set_color(s_mesh_mech.world_floor, 1.0F, 1.0F, 1.0F, 0.72F);
  s_mesh_mech.items[item_count].sort_key =
      nt_sort_key_opaque(s_mesh_mech.world_material.id, floor_mesh_id);
  s_mesh_mech.items[item_count].entity = s_mesh_mech.world_floor.id;
  s_mesh_mech.items[item_count].batch_key =
      nt_batch_key(s_mesh_mech.world_material.id, floor_mesh_id);
  item_count++;

  append_kenney_world_prop_items(hangar, &item_count);

  if (use_assault_hero) {
    append_assault_hero_items(root_x, root_z, root_scale, yaw, move_t, walk,
                              idle_bob, recoil, strafe_lean, forward_lean,
                              &item_count);
  }
  if (hangar) {
    append_sentinel_showcase_items(time, &item_count);
  }
  for (int i = 0; i < MECH_MESH_PARTS; ++i) {
    const bool assault_overlay = use_assault_hero && assault_hero_kitbash_part(i, rockets);
    if (use_assault_hero && !assault_overlay) {
      continue;
    }
    const MeshPartSpec *spec = &MESH_MECH_PARTS[i];
    const bool hero_overlay =
        use_source_hero && source_hero_overlay_part(i, rockets);
    if (use_source_hero && i != source_hero_index && !hero_overlay) {
      continue;
    }
    if ((spec->flags & MECH_PART_ROCKETS_ONLY) && !rockets) {
      continue;
    }
    const nt_material_t part_material =
        hero_overlay ? s_mesh_mech.solid_material : s_mesh_mech.material;
    uint32_t mesh_id = nt_resource_get(s_mesh_mech.meshes[spec->mesh_kind]);
    nt_entity_t entity = s_mesh_mech.parts[i];
    float lx = spec->pos[0];
    float ly = spec->pos[1] + idle_bob;
    float lz = spec->pos[2];
    float pitch = forward_lean;
    float roll = strafe_lean;
    float scale_x = spec->size[0];
    float scale_y = spec->size[1];
    float scale_z = spec->size[2];

    if (assault_overlay) {
      lx *= 0.92F;
      lz -= 0.34F;
      scale_x *= 0.74F;
      scale_y *= 0.74F;
      scale_z *= 0.74F;
      pitch += forward_lean * 0.35F - recoil * 0.18F;
      roll += strafe_lean * 0.45F;

      if (i == 13) {
        lx = 0.82F;
        ly = 1.70F + idle_bob * 0.25F;
        lz = -1.26F + recoil * 0.42F;
        scale_x = 0.18F;
        scale_y = 0.22F;
        scale_z = 0.86F + recoil * 0.30F;
        pitch -= recoil * 0.22F;
      } else if (i >= 20 && i <= 25) {
        const float side = i == 20 || i == 22 || i == 24 ? -1.0F : 1.0F;
        lx = side * ((i < 24) ? 0.82F : 0.58F);
        ly = (i < 22) ? 2.64F : ((i < 24) ? 2.86F : 2.54F);
        lz = (i < 24) ? -0.62F : -1.02F;
        scale_x *= 0.86F;
        scale_y *= 0.82F;
        scale_z *= 0.92F + recoil * 0.12F;
        pitch -= recoil * 0.10F;
      } else if (i == 26 || i == 27) {
        lx = 0.0F;
        ly = i == 26 ? 1.68F : 1.96F;
        lz = i == 26 ? -0.92F : -1.04F;
        scale_x *= i == 26 ? 1.12F : 0.82F;
        scale_y *= i == 26 ? 0.86F : 0.70F;
        scale_z *= 0.70F;
        pitch += sinf(time * 2.2F) * 0.018F;
      } else if (i >= 34 && i <= 41) {
        const float side = i < 38 ? -1.0F : 1.0F;
        const float row = (float)((i - 34) & 3);
        lx = side * (0.72F + row * 0.13F);
        ly = 2.24F + sinf(time * 3.0F + row) * 0.018F;
        lz = -0.90F - row * 0.035F + recoil * 0.08F;
        scale_x *= 0.72F;
        scale_y *= 0.82F;
        scale_z *= 0.78F;
        roll += side * (0.10F + move_t * 0.07F);
      } else if (i >= 42 && i <= 45) {
        const float side = i < 44 ? -1.0F : 1.0F;
        const float phase = side < 0.0F ? step : step_alt;
        lx = side * 1.05F;
        ly = 1.52F + phase * move_t * 0.08F;
        lz = -0.60F + recoil * 0.06F;
        scale_x *= 0.76F;
        scale_y *= 0.88F + move_t * 0.14F;
        scale_z *= 0.76F;
        roll += side * (0.18F + phase * move_t * 0.12F);
      } else if (i >= 46 && i <= 49) {
        const float side = i < 48 ? -1.0F : 1.0F;
        const float phase = side < 0.0F ? step : step_alt;
        lx = side * 0.62F;
        ly = 0.62F + fmaxf(phase, 0.0F) * move_t * 0.10F;
        lz = -0.74F + phase * move_t * 0.10F;
        scale_x *= 0.82F;
        scale_y *= 0.82F + move_t * 0.10F;
        scale_z *= 0.82F;
        pitch += phase * move_t * 0.20F;
      }
    }

    if (i == source_hero_index) {
      ly += idle_bob * 0.20F + move_t * 0.015F;
      pitch -= 1.5707963F;
      pitch += forward_lean * 0.35F;
      roll += strafe_lean * 0.35F;
      lz += recoil * 0.10F;
      if (hangar) {
        scale_x *= 0.72F;
        scale_y *= 0.72F;
        scale_z *= 0.72F;
      }
    }

    switch (i) {
    case 0:
      ly += move_t * 0.025F;
      scale_x *= 1.08F;
      scale_z *= 1.10F;
      break;
    case 1:
      ly -= move_t * 0.035F;
      roll *= 0.45F;
      break;
    case 2:
      ly += move_t * 0.020F;
      scale_x *= 1.12F;
      break;
    case 4:
      ly += sinf(time * 1.8F) * 0.018F;
      pitch *= 0.35F;
      roll *= 0.25F;
      break;
    case 7:
    case 9:
    case 11:
      ly += step * move_t * 0.045F;
      lz += step_alt * move_t * 0.075F;
      roll += step * move_t * 0.10F;
      break;
    case 8:
    case 10:
    case 12:
      ly += step_alt * move_t * 0.045F;
      lz += step * move_t * 0.075F;
      roll += step_alt * move_t * 0.10F;
      break;
    case 13:
      lz += recoil * 0.46F;
      ly += recoil * 0.05F;
      pitch -= recoil * 0.15F;
      scale_z *= 1.20F;
      break;
    case 14:
      ly += step * move_t * 0.085F;
      lz += step * move_t * 0.13F;
      pitch += step * move_t * 0.18F;
      break;
    case 15:
      ly += step_alt * move_t * 0.085F;
      lz += step_alt * move_t * 0.13F;
      pitch += step_alt * move_t * 0.18F;
      break;
    case 16:
      ly += fmaxf(step, 0.0F) * move_t * 0.12F;
      lz += step * move_t * 0.18F;
      pitch += step * move_t * 0.22F;
      scale_x *= 1.10F;
      break;
    case 17:
      ly += fmaxf(step_alt, 0.0F) * move_t * 0.12F;
      lz += step_alt * move_t * 0.18F;
      pitch += step_alt * move_t * 0.22F;
      scale_x *= 1.10F;
      break;
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
      ly += recoil * 0.04F + sinf(time * 3.2F + (float)i) * 0.012F;
      break;
    case 26:
      ly += recoil * 0.035F + sinf(time * 2.2F) * 0.006F;
      lz += recoil * 0.09F;
      scale_x *= 0.92F;
      scale_y *= 0.72F;
      break;
    case 27:
      ly += sinf(time * 2.8F) * 0.005F;
      scale_x *= 0.72F;
      scale_y *= 0.78F;
      scale_z *= 0.70F;
      break;
    case 28:
    case 30:
    case 32:
      ly += step * move_t * 0.035F;
      lz += step_alt * move_t * 0.055F;
      roll += step * move_t * 0.06F;
      scale_x *= 0.78F;
      break;
    case 29:
    case 31:
    case 33:
      ly += step_alt * move_t * 0.035F;
      lz += step * move_t * 0.055F;
      roll += step_alt * move_t * 0.06F;
      scale_x *= 0.78F;
      break;
    case 34:
    case 35:
      ly += recoil * 0.030F + sinf(time * 2.6F + (float)i) * 0.010F;
      lz += recoil * 0.060F;
      scale_x *= 0.88F;
      break;
    case 36:
    case 37:
      ly += recoil * 0.020F;
      lz += recoil * 0.050F;
      scale_x *= 0.92F;
      scale_y *= 0.84F;
      break;
    case 38:
    case 39:
    case 40:
    case 41:
      ly += sinf(time * 4.1F + (float)i) * 0.008F;
      lz += recoil * 0.070F;
      break;
    case 42:
    case 43:
      lz += recoil * 0.060F;
      roll -= move_t * 0.035F;
      scale_y *= 0.82F + move_t * 0.10F;
      break;
    case 44:
    case 45:
      lz += recoil * 0.060F;
      roll += move_t * 0.035F;
      scale_y *= 0.82F + move_t * 0.10F;
      break;
    case 46:
    case 47:
      ly += step * move_t * 0.032F;
      lz += step * move_t * 0.045F;
      break;
    case 48:
    case 49:
      ly += step_alt * move_t * 0.032F;
      lz += step_alt * move_t * 0.045F;
      break;
    default:
      break;
    }

    const float wx = (lx * yaw_cos) + (lz * yaw_sin);
    const float wz = (lz * yaw_cos) - (lx * yaw_sin);
    float *pos = nt_transform_comp_position(entity);
    pos[0] = root_x + (wx * root_scale);
    pos[1] = 0.25F + (ly * root_scale);
    pos[2] = root_z + (wz * root_scale);
    q_pose(yaw, pitch, roll, nt_transform_comp_rotation(entity));
    float *scl = nt_transform_comp_scale(entity);
    scl[0] = scale_x * root_scale;
    scl[1] = scale_y * root_scale;
    scl[2] = scale_z * root_scale;
    *nt_transform_comp_dirty(entity) = true;
    *nt_mesh_comp_handle(entity) = (nt_mesh_t){.id = mesh_id};
    *nt_material_comp_handle(entity) = assault_overlay ? s_mesh_mech.solid_material : part_material;
    float out_color[4] = {spec->color[0], spec->color[1], spec->color[2], spec->color[3]};
    if (assault_overlay) {
      const float glow = clampf((s_game.heat - 0.24F) * 1.20F, 0.0F, 1.0F) +
                         fmaxf(s_game.hit_flash, s_game.rocket_flash * 0.35F) * 0.12F;
      if (i == 13 || (i >= 20 && i <= 25)) {
        out_color[0] = fminf(1.0F, 0.98F + glow * 0.12F);
        out_color[1] = fminf(1.0F, 0.46F + glow * 0.26F);
        out_color[2] = fminf(1.0F, 0.06F + glow * 0.18F);
      } else if (i == 27 || (i >= 34 && i <= 41) || i >= 46) {
        out_color[0] = fminf(1.0F, 0.05F + glow * 0.16F);
        out_color[1] = fminf(1.0F, 0.86F + glow * 0.14F);
        out_color[2] = 1.0F;
      } else {
        out_color[0] = 0.76F;
        out_color[1] = 0.88F;
        out_color[2] = 0.92F;
      }
      out_color[3] = 1.0F;
    }
    nt_drawable_comp_set_color(entity, out_color[0], out_color[1],
                               out_color[2], out_color[3]);

    s_mesh_mech.items[item_count].sort_key =
        nt_sort_key_opaque((assault_overlay ? s_mesh_mech.solid_material : part_material).id, mesh_id);
    s_mesh_mech.items[item_count].entity = entity.id;
    s_mesh_mech.items[item_count].batch_key =
        nt_batch_key((assault_overlay ? s_mesh_mech.solid_material : part_material).id, mesh_id);
    item_count++;
  }

  if (!hangar) {
    const MeshPartMesh robot_meshes[ROBOT_ENEMY_MESH_PARTS] = {
        MECH_MESH_ROBOT_ENEMY_MAIN2,     MECH_MESH_ROBOT_ENEMY_MAIN,
        MECH_MESH_ROBOT_ENEMY_EDGE,      MECH_MESH_ROBOT_ENEMY_DARK,
        MECH_MESH_ROBOT_ENEMY_EYE,       MECH_MESH_ROBOT_ENEMY_GREY,
        MECH_MESH_ROBOT_ENEMY_LIGHTGREY,
    };
    const float robot_colors[ROBOT_ENEMY_MESH_PARTS][4] = {
        {0.72F, 0.72F, 0.66F, 1.0F}, {1.00F, 0.46F, 0.08F, 1.0F},
        {0.96F, 0.96F, 0.88F, 1.0F}, {0.055F, 0.075F, 0.070F, 1.0F},
        {1.00F, 0.08F, 0.02F, 1.0F}, {0.28F, 0.28F, 0.30F, 1.0F},
        {0.43F, 0.43F, 0.45F, 1.0F},
    };
    const int target = target_drone();
    for (int i = 0; i < MAX_DRONES; ++i) {
      if (!s_game.drones[i].alive) {
        continue;
      }
      const float to_player_x = s_game.mech_x - s_game.drones[i].x;
      const float to_player_z = s_game.mech_z - s_game.drones[i].z;
      const float enemy_yaw = atan2f(to_player_x, to_player_z);
      const bool is_target = i == target;
      const float bob = sinf(time * 4.2F + (float)i) * 0.018F;
      const float enemy_scale = is_target ? 1.75F : 1.52F;
      const float pos[3] = {s_game.drones[i].x,
                            0.05F + (enemy_scale * 0.00379914F) + bob,
                            s_game.drones[i].z};
      for (int part = 0; part < ROBOT_ENEMY_MESH_PARTS; ++part) {
        const uint32_t enemy_mesh_id =
            nt_resource_get(s_mesh_mech.meshes[robot_meshes[part]]);
        nt_entity_t entity = s_mesh_mech.enemy_robots[i][part];
        float *part_pos = nt_transform_comp_position(entity);
        part_pos[0] = pos[0];
        part_pos[1] = pos[1];
        part_pos[2] = pos[2];
        q_pose(enemy_yaw, 0.0F, is_target ? 0.05F : 0.0F,
               nt_transform_comp_rotation(entity));
        float *scl = nt_transform_comp_scale(entity);
        scl[0] = enemy_scale;
        scl[1] = enemy_scale;
        scl[2] = enemy_scale;
        *nt_transform_comp_dirty(entity) = true;
        *nt_mesh_comp_handle(entity) = (nt_mesh_t){.id = enemy_mesh_id};
        const float pulse = is_target && part == 4
                                ? 0.18F * (0.5F + 0.5F * sinf(time * 9.0F))
                                : 0.0F;
        nt_drawable_comp_set_color(
            entity, fminf(robot_colors[part][0] + pulse, 1.0F),
            fminf(robot_colors[part][1] + pulse * 0.25F, 1.0F),
            fminf(robot_colors[part][2] + pulse * 0.10F, 1.0F),
            robot_colors[part][3]);
        s_mesh_mech.items[item_count].sort_key =
            nt_sort_key_opaque(s_mesh_mech.robot_material.id, enemy_mesh_id);
        s_mesh_mech.items[item_count].entity = entity.id;
        s_mesh_mech.items[item_count].batch_key =
            nt_batch_key(s_mesh_mech.robot_material.id, enemy_mesh_id);
        item_count++;
      }
    }
  }
  nt_transform_comp_update();

  nt_frame_uniforms_t uniforms;
  build_frame_uniforms(w, h, hangar, &uniforms, NULL);
  nt_gfx_update_buffer(s_mesh_mech.frame_ubo, &uniforms, sizeof(uniforms));
  nt_gfx_bind_uniform_buffer(s_mesh_mech.frame_ubo, 0);
  nt_sort_by_key(s_mesh_mech.items, item_count, s_mesh_mech.sort_scratch);
  nt_mesh_renderer_draw_list(s_mesh_mech.items, item_count);
}

static void draw_world(float w, float h) {
  const bool hangar = s_game.screen != SCREEN_BATTLE;
  const bool use_mesh_mech = mesh_mech_ready();
  setup_perspective(w, h, hangar);
  draw_floor_grid(ARENA_HALF, hangar);
  if (s_game.screen == SCREEN_BATTLE) {
    if (use_mesh_mech) {
      draw_shadow(s_game.mech_x, s_game.mech_z + 0.08F, 2.8F * 0.88F,
                  2.1F * 0.88F, 0.38F);
    } else {
      draw_mech(s_game.mech_x, s_game.mech_z, 0.88F, s_game.rockets_equipped,
                false);
    }
    draw_movement_feedback();
    const int target = target_drone();
    for (int i = 0; i < MAX_DRONES; ++i) {
      draw_drone(&s_game.drones[i], i == target);
    }
    draw_projectiles();
    draw_assault_motion_effects();
  } else {
    if (use_mesh_mech) {
      draw_shadow(0.0F, 0.48F, 2.8F * 1.18F, 2.1F * 1.18F, 0.38F);
    } else {
      draw_mech(0.0F, 0.4F, 1.18F, s_game.rockets_equipped, true);
    }
    if (s_game.screen == SCREEN_UPGRADE || s_game.screen == SCREEN_REWARD ||
        s_game.screen == SCREEN_RETEST) {
      nt_shape_renderer_cube((float[3]){3.2F, 0.45F, -1.2F},
                             (float[3]){1.2F, 0.28F, 1.2F}, COL_METAL_DARK);
      nt_shape_renderer_cube((float[3]){3.2F, 0.96F, -1.2F},
                             (float[3]){0.9F, 0.62F, 0.9F},
                             s_game.rockets_equipped ? COL_GREEN : COL_AMBER);
    }
  }
}

static void draw_cooling_meter(float x, float y, float w, float h) {
  capsule2(x, y, w, h, (float[4]){0.03F, 0.08F, 0.10F, 0.9F});
  const float cool = 1.0F - s_game.heat;
  const float color_hot[4] = {1.0F, 0.25F, 0.08F, 1.0F};
  const float color_cool[4] = {0.0F, 0.86F, 1.0F, 1.0F};
  capsule2(x, y, w * cool, h, s_game.heat > 0.72F ? color_hot : color_cool);
  draw_text(x, y - 23.0F, 3.0F, "COOLING", COL_WHITE);
}

static void draw_hud(float w, float h) {
  setup_ortho(w, h);
  bind_ui_frame_uniforms(w, h);
  if (ui_text_ready()) {
    nt_text_renderer_set_material(s_mesh_mech.text_material);
    nt_text_renderer_set_font(s_mesh_mech.ui_font);
  }
  rect2(0.0F, h - 78.0F, w, 78.0F, (float[4]){0.0F, 0.02F, 0.03F, 0.72F});
  draw_text(34.0F, h - 46.0F, 4.2F, "MECH BUILDER BATTLER", COL_WHITE);
  draw_int_text(w - 250.0F, h - 46.0F, 4.0F, "SALVAGE ", s_game.salvage,
                COL_AMBER);

  if (s_game.screen == SCREEN_BATTLE) {
    draw_text(42.0F, h - 116.0F, 3.0F, "WASD MOVE  Q DASH  E ROCKETS",
              COL_WHITE);
    draw_int_text(42.0F, h - 154.0F, 3.6F, "DRONES ", alive_count(), COL_GREEN);
    draw_cooling_meter(w * 0.5F - 160.0F, 36.0F, 320.0F, 18.0F);
    action_button2(s_dash_box, COL_ARMOR_BLUE, s_game.dash_cd <= 0.0F, "DASH");
    draw_text(s_dash_box.x + 35.0F, s_dash_box.y + 24.0F, 3.2F, "Q", COL_WHITE);
    action_button2(s_special_box, COL_AMBER,
                   s_game.rockets_equipped && s_game.rocket_cd <= 0.0F,
                   s_game.rockets_equipped ? "ROCKET" : "LOCK");
    draw_text(s_special_box.x + 40.0F, s_special_box.y + 28.0F, 3.3F, "E",
              COL_WHITE);
    nt_shape_renderer_circle((float[3]){117.0F, 111.0F, 0.0F}, 72.0F,
                             (float[4]){0.025F, 0.12F, 0.16F, 1.0F});
    nt_shape_renderer_circle_wire((float[3]){117.0F, 111.0F, 0.0F}, 57.0F,
                                  (float[4]){0.35F, 0.88F, 1.0F, 0.92F});
    nt_shape_renderer_circle_wire((float[3]){117.0F, 111.0F, 0.0F}, 72.0F,
                                  (float[4]){1.0F, 0.48F, 0.05F, 0.35F});
    nt_shape_renderer_circle((float[3]){117.0F, 111.0F, 0.0F}, 18.0F,
                             (float[4]){0.0F, 0.72F, 0.9F, 1.0F});
    draw_text(88.0F, 100.0F, 2.4F, "WASD", COL_WHITE);
  } else if (s_game.screen == SCREEN_HANGAR) {
    draw_text(46.0F, h - 126.0F, 4.0F, "HANGAR", COL_AMBER);
    draw_text(46.0F, h - 164.0F, 3.0F, "ONE MECH  ONE NEXT ACTION", COL_WHITE);
    button2(s_primary_box, COL_GREEN, true);
    draw_text(s_primary_box.x + 48.0F, s_primary_box.y + 23.0F, 4.2F, "BATTLE",
              COL_WHITE);
    rect2(w - 314.0F, h - 208.0F, 270.0F, 104.0F, COL_PANEL);
    draw_text(w - 286.0F, h - 148.0F, 3.0F, "SHOULDER MODULE", COL_AMBER);
    if (s_game.rockets_equipped) {
      draw_text(w - 286.0F, h - 184.0F, 2.7F, "ROCKETS EQUIPPED", COL_WHITE);
    } else {
      draw_text(w - 286.0F, h - 182.0F, 2.6F, "LOCKED", COL_WHITE);
      draw_text(w - 286.0F, h - 204.0F, 2.4F, "WIN SALVAGE", COL_WHITE);
    }
  } else if (s_game.screen == SCREEN_REWARD) {
    rect2(w * 0.5F - 250.0F, h - 272.0F, 500.0F, 170.0F, COL_PANEL);
    draw_text(w * 0.5F - 182.0F, h - 162.0F, 5.0F, "REWARD", COL_AMBER);
    draw_text(w * 0.5F - 188.0F, h - 222.0F, 4.0F,
              s_game.battle_index == 0 ? "SALVAGE +120" : "SALVAGE +75",
              COL_WHITE);
    button2(s_primary_box, COL_GREEN, true);
    draw_text(s_primary_box.x + 30.0F, s_primary_box.y + 23.0F, 3.8F,
              s_game.rockets_equipped ? "CONTINUE" : "UPGRADE", COL_WHITE);
  } else if (s_game.screen == SCREEN_UPGRADE) {
    rect2(w * 0.5F - 292.0F, h - 298.0F, 584.0F, 200.0F, COL_PANEL);
    draw_text(w * 0.5F - 226.0F, h - 160.0F, 4.3F, "BUY SHOULDER ROCKETS",
              COL_AMBER);
    draw_text(w * 0.5F - 188.0F, h - 218.0F, 3.6F, "COST 120 SALVAGE",
              COL_WHITE);
    button2(s_primary_box,
            s_game.salvage >= ROCKET_COST ? COL_GREEN : COL_WARNING,
            s_game.salvage >= ROCKET_COST);
    draw_text(s_primary_box.x + 50.0F, s_primary_box.y + 23.0F, 4.0F, "ATTACH",
              COL_WHITE);
  } else if (s_game.screen == SCREEN_RETEST) {
    rect2(44.0F, h - 230.0F, 520.0F, 126.0F, COL_PANEL);
    draw_text(78.0F, h - 150.0F, 4.0F, "ROCKETS ATTACHED",
              COL_GREEN);
    draw_text(78.0F, h - 196.0F, 3.1F, "TEST THEM AGAINST DRONES",
              COL_WHITE);
    button2(s_primary_box, COL_AMBER, true);
    draw_text(s_primary_box.x + 56.0F, s_primary_box.y + 23.0F, 4.0F, "RETEST",
              COL_WHITE);
  }
}

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static bool write_framebuffer_ppm(const char *path, int width, int height) {
  if (!path || !path[0] || width <= 0 || height <= 0) {
    return false;
  }
  const size_t row_bytes = (size_t)width * 3U;
  const size_t total = row_bytes * (size_t)height;
  unsigned char *pixels = (unsigned char *)malloc(total);
  if (!pixels) {
    return false;
  }
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

  FILE *file = fopen(path, "wb");
  if (!file) {
    free(pixels);
    return false;
  }
  (void)fprintf(file, "P6\n%d %d\n255\n", width, height);
  for (int y = height - 1; y >= 0; --y) {
    const unsigned char *row = pixels + ((size_t)y * row_bytes);
    if (fwrite(row, 1, row_bytes, file) != row_bytes) {
      fclose(file);
      free(pixels);
      return false;
    }
  }
  fclose(file);
  free(pixels);
  return true;
}

static void maybe_capture_framebuffer(void) {
  if (!s_pending_capture_path[0]) {
    return;
  }
  const int width =
      (int)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const int height =
      (int)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
  (void)write_framebuffer_ppm(s_pending_capture_path, width, height);
  s_pending_capture_path[0] = '\0';
}
#else
static void maybe_capture_framebuffer(void) {}
#endif

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static bool emit_state(cJSON *result_obj) {
  cJSON_AddStringToObject(result_obj, "runtime", "mech_builder_battler");
  cJSON_AddStringToObject(result_obj, "screen", screen_name(s_game.screen));
  cJSON_AddNumberToObject(result_obj, "salvage", (double)s_game.salvage);
  cJSON_AddBoolToObject(result_obj, "rockets_equipped",
                        s_game.rockets_equipped);
  cJSON_AddNumberToObject(result_obj, "battle_index",
                          (double)s_game.battle_index);
  cJSON_AddNumberToObject(result_obj, "alive_drones", (double)alive_count());
  cJSON_AddNumberToObject(result_obj, "heat", (double)s_game.heat);
  cJSON_AddNumberToObject(result_obj, "mech_x", (double)s_game.mech_x);
  cJSON_AddNumberToObject(result_obj, "mech_z", (double)s_game.mech_z);
  cJSON_AddBoolToObject(result_obj, "mesh_mech_ready", mesh_mech_ready());
  return true;
}

static bool ep_game_state(const cJSON *params, cJSON *result_obj,
                          nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  return emit_state(result_obj);
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON *result_obj,
                                   nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  reset_runtime();
  return emit_state(result_obj);
}

static bool ep_game_action_start_battle(const cJSON *params, cJSON *result_obj,
                                        nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  start_battle();
  return emit_state(result_obj);
}

static bool ep_game_action_use_special(const cJSON *params, cJSON *result_obj,
                                       nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  fire_rockets();
  return emit_state(result_obj);
}

static bool ep_game_action_buy_rockets(const cJSON *params, cJSON *result_obj,
                                       nt_devapi_error *err, void *user) {
  (void)params;
  (void)err;
  (void)user;
  buy_rockets();
  return emit_state(result_obj);
}

static bool ep_game_capture_framebuffer(const cJSON *params, cJSON *result_obj,
                                        nt_devapi_error *err, void *user) {
  (void)user;
  const cJSON *output = cJSON_GetObjectItemCaseSensitive(params, "output");
  if (!cJSON_IsString(output) || !output->valuestring ||
      !output->valuestring[0]) {
    err->code = "bad_params";
    err->message = "output path is required";
    return false;
  }
#ifdef NT_PLATFORM_WEB
  err->code = "unsupported";
  err->message = "framebuffer capture is native-only in this prototype";
  return false;
#else
  (void)snprintf(s_pending_capture_path, sizeof(s_pending_capture_path), "%s",
                 output->valuestring);
  s_pending_capture_path[sizeof(s_pending_capture_path) - 1] = '\0';
  cJSON_AddStringToObject(result_obj, "output", s_pending_capture_path);
  cJSON_AddStringToObject(result_obj, "status", "scheduled_next_frame");
  return true;
#endif
}

static void register_game_endpoints(void) {
  game_state_register_devapi();
  static const nt_devapi_command_desc descs[] = {
      {"game.state", "game", "Return the mech playable slice state.", "",
       "state object", "immediate", "none"},
      {"game.reset_playtest", "game", "Reset the mech playable slice.", "",
       "state object", "immediate", "mutates state"},
      {"game.action.start_battle", "game", "Start the current mech battle.", "",
       "state object", "immediate", "mutates state"},
      {"game.action.use_special", "game", "Fire shoulder rockets if available.",
       "", "state object", "immediate", "mutates state"},
      {"game.action.buy_rockets", "game",
       "Buy and equip shoulder rockets when affordable.", "", "state object",
       "immediate", "mutates state"},
      {"game.capture.framebuffer", "game",
       "Capture the native framebuffer to a PPM file.", "output",
       "{output,status}", "next-frame", "writes file"},
  };
  (void)nt_devapi_register(&descs[0], ep_game_state, NULL);
  (void)nt_devapi_register(&descs[1], ep_game_reset_playtest, NULL);
  (void)nt_devapi_register(&descs[2], ep_game_action_start_battle, NULL);
  (void)nt_devapi_register(&descs[3], ep_game_action_use_special, NULL);
  (void)nt_devapi_register(&descs[4], ep_game_action_buy_rockets, NULL);
  (void)nt_devapi_register(&descs[5], ep_game_capture_framebuffer, NULL);
  game_devapi_ui_register();
}

static void register_ui_devapi(float w, float h) {
  game_devapi_ui_clear();
  (void)game_devapi_ui_register_node(
      "root", "", "screen", "Mech Builder Battler", screen_name(s_game.screen),
      0.0F, 0.0F, w, h, true, true);
  if (s_game.screen == SCREEN_HANGAR) {
    (void)game_devapi_ui_register_node(
        "action.battle", "root", "button", "Battle", "Start battle",
        s_primary_box.x, s_primary_box.y, s_primary_box.w, s_primary_box.h,
        true, true);
    (void)game_devapi_ui_register_node(
        "slot.shoulder", "root", "slot", "Shoulder Module",
        s_game.rockets_equipped ? "Rockets equipped" : "Locked", w - 314.0F,
        h - 208.0F, 270.0F, 104.0F, true, false);
  } else if (s_game.screen == SCREEN_BATTLE) {
    (void)game_devapi_ui_register_node(
        "action.dash", "root", "button", "Dash", "Q", s_dash_box.x,
        s_dash_box.y, s_dash_box.w, s_dash_box.h, true, s_game.dash_cd <= 0.0F);
    (void)game_devapi_ui_register_node(
        "action.rockets", "root", "button", "Rockets", "E", s_special_box.x,
        s_special_box.y, s_special_box.w, s_special_box.h, true,
        s_game.rockets_equipped);
    (void)game_devapi_ui_register_node(
        "meter.cooling", "root", "meter", "Cooling", "Heat limiter",
        w * 0.5F - 160.0F, 36.0F, 320.0F, 18.0F, true, true);
  } else if (s_game.screen == SCREEN_REWARD) {
    (void)game_devapi_ui_register_node(
        "action.reward_continue", "root", "button", "Continue", "Go to upgrade",
        s_primary_box.x, s_primary_box.y, s_primary_box.w, s_primary_box.h,
        true, true);
  } else if (s_game.screen == SCREEN_UPGRADE) {
    (void)game_devapi_ui_register_node(
        "action.attach_rockets", "root", "button", "Attach Rockets",
        "Cost 120 salvage", s_primary_box.x, s_primary_box.y, s_primary_box.w,
        s_primary_box.h, true, s_game.salvage >= ROCKET_COST);
  } else if (s_game.screen == SCREEN_RETEST) {
    (void)game_devapi_ui_register_node(
        "action.retest", "root", "button", "Retest", "Test rockets",
        s_primary_box.x, s_primary_box.y, s_primary_box.w, s_primary_box.h,
        true, true);
  }
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

  const float w =
      (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                : g_nt_window.height);
  layout(w, h);
  handle_input();
  update_battle(g_nt_app.dt);
  step_mesh_mech();

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
    nt_text_renderer_restore_gpu();
  }
  nt_gfx_begin_pass(&(nt_pass_desc_t){
      .clear_color = {0.56F, 0.82F, 1.0F, 1.0F}, .clear_depth = 1.0F});
  draw_world(w, h);
  nt_shape_renderer_flush();
  draw_mesh_mech(w, h);
  nt_shape_renderer_flush();
  draw_hud(w, h);
  nt_shape_renderer_flush();
  nt_text_renderer_flush();
  maybe_capture_framebuffer();
  nt_gfx_end_pass();
  nt_gfx_end_frame();
  nt_window_swap_buffers();
}

int main(int argc, char **argv) {
  nt_engine_config_t config = {0};
  config.app_name = "Mech Builder Battler";
  config.version = 1;
  if (nt_engine_init(&config) != NT_OK) {
    return 1;
  }

  parse_args(argc, argv);
  reset_runtime();

  g_nt_window.title = "Mech Builder Battler";
  g_nt_window.width = (uint32_t)s_window_width;
  g_nt_window.height = (uint32_t)s_window_height;
  nt_window_init();
  nt_input_init();

  nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
  gfx_desc.depth = true;
  nt_gfx_init(&gfx_desc);
  nt_gfx_register_global_block("Globals", 0);
  nt_shape_renderer_init();
  nt_http_init();
  nt_fs_init();
  init_mesh_mech();

#if NT_DEVAPI_ENABLED
  if (s_devapi_enabled) {
    if (nt_devapi_init() != NT_OK) {
      (void)fprintf(stderr, "Failed to init DevAPI\n");
      s_devapi_enabled = false;
    } else {
      register_game_endpoints();
      if (!nt_devapi_net_start(s_devapi_port)) {
        (void)fprintf(stderr, "Failed to start DevAPI on port %u\n",
                      (unsigned)s_devapi_port);
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
  shutdown_mesh_mech();
  nt_fs_shutdown();
  nt_http_shutdown();
  nt_gfx_shutdown();
  nt_input_shutdown();
  nt_window_shutdown();
  nt_engine_shutdown();
#endif

  return 0;
}
