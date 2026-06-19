/*
 * Build the Mech Builder Battler starter mesh pack.
 *
 * The first production path packs a small authored glTF mesh set with normals
 * plus a game-owned instanced shader. Runtime composes the starter mech from
 * distinct mesh-renderer part silhouettes instead of one stretched cube.
 */

#include "nt_builder.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static char s_path_buf[512];

static const char *MESH_ASSETS[] = {
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
};

static const char *pack_path(const char *dir, const char *name) {
  (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
  return s_path_buf;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    (void)fprintf(stderr,
                  "Usage: build_mech_builder_battler_packs <pack_dir>\n");
    return 1;
  }
  const char *out_dir = argv[1];

  (void)printf("=== Build Mech Builder Battler Packs -> %s ===\n", out_dir);

  NtStreamLayout layout[] = {
      {"position", "POSITION", NT_STREAM_FLOAT32, 3, false},
      {"normal", "NORMAL", NT_STREAM_FLOAT32, 3, false},
  };
  NtStreamLayout source_mech_layout[] = {
      {"position", "POSITION", NT_STREAM_FLOAT32, 3, false},
      {"normal", "NORMAL", NT_STREAM_FLOAT32, 3, false},
      {"uv0", "TEXCOORD_0", NT_STREAM_FLOAT32, 2, false},
  };

  NtBuilderContext *ctx = nt_builder_start_pack(
      pack_path(out_dir, "mech_builder_battler_mesh.ntpack"));
  if (!ctx) {
    (void)fprintf(stderr, "Failed to start mech_builder_battler_mesh pack\n");
    return 1;
  }

  const size_t mesh_count = sizeof(MESH_ASSETS) / sizeof(MESH_ASSETS[0]);
  for (size_t i = 0; i < mesh_count; ++i) {
    const bool source_mech =
        strcmp(MESH_ASSETS[i], "assets/meshes/poly_pizza_quaternius_mech_cc0.glb") ==
        0;
    nt_builder_add_mesh(ctx, MESH_ASSETS[i],
                        &(nt_mesh_opts_t){
                            .layout = source_mech ? source_mech_layout : layout,
                            .stream_count = source_mech ? 3 : 2,
                        });
  }
  nt_builder_add_texture(
      ctx, "assets/textures/poly_pizza_quaternius_mech_toy_atlas.png", NULL);
  nt_builder_add_shader(ctx, "assets/shaders/mech_mesh_inst.vert",
                        NT_BUILD_SHADER_VERTEX);
  nt_builder_add_shader(ctx, "assets/shaders/mech_mesh_inst.frag",
                        NT_BUILD_SHADER_FRAGMENT);
  nt_builder_add_shader(ctx, "assets/shaders/mech_mesh_color_inst.frag",
                        NT_BUILD_SHADER_FRAGMENT);
  nt_builder_add_shader(ctx, "assets/shaders/mech_mesh_solid_inst.vert",
                        NT_BUILD_SHADER_VERTEX);
  nt_builder_add_shader(ctx, "assets/shaders/mech_mesh_solid_inst.frag",
                        NT_BUILD_SHADER_FRAGMENT);
  nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert",
                        NT_BUILD_SHADER_VERTEX);
  nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag",
                        NT_BUILD_SHADER_FRAGMENT);
  nt_builder_add_font(
      ctx,
      "external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf",
      &(nt_font_opts_t){.charset = NT_CHARSET_ASCII,
                        .resource_name = "mech/ui_font"});

  nt_build_result_t result = nt_builder_finish_pack(ctx);
  nt_builder_free_pack(ctx);
  if (result != NT_BUILD_OK) {
    (void)fprintf(stderr, "Pack failed: %d\n", result);
    return 1;
  }

  (void)printf("Built: mech_builder_battler_mesh.ntpack\n");
  return 0;
}
