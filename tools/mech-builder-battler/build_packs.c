/*
 * Build the Mech Builder Battler starter mesh pack.
 *
 * The first production path is intentionally small: one packed cube glTF mesh
 * with normals plus a game-owned instanced shader. Runtime composes the starter
 * mech from mesh-renderer instances instead of shape-renderer boxes, so the
 * slice proves the engine asset/material path before larger authored mech
 * assets arrive.
 */

#include "nt_builder.h"

#include <stdio.h>

static char s_path_buf[512];

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

  NtBuilderContext *ctx = nt_builder_start_pack(
      pack_path(out_dir, "mech_builder_battler_mesh.ntpack"));
  if (!ctx) {
    (void)fprintf(stderr, "Failed to start mech_builder_battler_mesh pack\n");
    return 1;
  }

  nt_builder_add_mesh(ctx, "assets/meshes/starter_cube_normals.gltf",
                      &(nt_mesh_opts_t){.layout = layout, .stream_count = 2});
  nt_builder_add_shader(ctx, "assets/shaders/mech_mesh_inst.vert",
                        NT_BUILD_SHADER_VERTEX);
  nt_builder_add_shader(ctx, "assets/shaders/mech_mesh_inst.frag",
                        NT_BUILD_SHADER_FRAGMENT);

  nt_build_result_t result = nt_builder_finish_pack(ctx);
  nt_builder_free_pack(ctx);
  if (result != NT_BUILD_OK) {
    (void)fprintf(stderr, "Pack failed: %d\n", result);
    return 1;
  }

  (void)printf("Built: mech_builder_battler_mesh.ntpack\n");
  return 0;
}
