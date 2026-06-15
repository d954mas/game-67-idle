#include "nt_builder.h"

#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

#define HEADER_DIR "src/generated"

static char s_path_buf[512];
static char s_source_path_buf[512];

static const char *pack_path(const char *dir, const char *name) {
    (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
    return s_path_buf;
}

static const char *source_path(const char *dir, const char *name) {
    (void)snprintf(s_source_path_buf, sizeof(s_source_path_buf), "%s/%s", dir, name);
    return s_source_path_buf;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        (void)fprintf(stderr, "Usage: roblox_fishing_build_packs <output_assets_dir> <model_source_dir>\n");
        return 1;
    }

    const char *out_dir = argv[1];
    const char *model_dir = argv[2];
    (void)MKDIR(out_dir);
    (void)MKDIR(HEADER_DIR);
    (void)MKDIR("build");
    (void)MKDIR("build/roblox_fishing_models");
    (void)MKDIR("build/roblox_fishing_models/_cache");

    NtStreamLayout layout[] = {
        {"position", "POSITION", NT_STREAM_FLOAT32, 3, false},
        {"uv0", "TEXCOORD_0", NT_STREAM_FLOAT32, 2, false},
    };
    static const uint8_t white_rgba[4] = {255, 255, 255, 255};

    NtBuilderContext *ctx = nt_builder_start_pack(pack_path(out_dir, "roblox_fishing_models.ntpack"));
    if (!ctx) {
        (void)fprintf(stderr, "Failed to start roblox_fishing_models pack\n");
        return 1;
    }

    nt_builder_set_header_dir(ctx, HEADER_DIR);
    nt_builder_set_cache_dir(ctx, "build/roblox_fishing_models/_cache");
    nt_builder_add_mesh(ctx, "external/neotolis-engine/assets/meshes/cube.glb", &(nt_mesh_opts_t){.layout = layout, .stream_count = 2});
    nt_builder_rename(ctx, "external/neotolis-engine/assets/meshes/cube.glb", "roblox_fishing/cube_model");
    nt_builder_add_mesh(ctx, source_path(model_dir, "fish_trophy.gltf"), &(nt_mesh_opts_t){.layout = layout, .stream_count = 2});
    nt_builder_rename(ctx, source_path(model_dir, "fish_trophy.gltf"), "roblox_fishing/fish_trophy");
    nt_builder_add_mesh(ctx, source_path(model_dir, "toy_boat_hull.gltf"), &(nt_mesh_opts_t){.layout = layout, .stream_count = 2});
    nt_builder_rename(ctx, source_path(model_dir, "toy_boat_hull.gltf"), "roblox_fishing/toy_boat_hull");
    nt_builder_add_mesh(ctx, source_path(model_dir, "shop_sign.gltf"), &(nt_mesh_opts_t){.layout = layout, .stream_count = 2});
    nt_builder_rename(ctx, source_path(model_dir, "shop_sign.gltf"), "roblox_fishing/shop_sign");
    nt_builder_add_mesh(ctx, source_path(model_dir, "palm_leaf_chunk.gltf"), &(nt_mesh_opts_t){.layout = layout, .stream_count = 2});
    nt_builder_rename(ctx, source_path(model_dir, "palm_leaf_chunk.gltf"), "roblox_fishing/palm_leaf_chunk");
    nt_builder_add_mesh(ctx, source_path(model_dir, "bobber_diamond.gltf"), &(nt_mesh_opts_t){.layout = layout, .stream_count = 2});
    nt_builder_rename(ctx, source_path(model_dir, "bobber_diamond.gltf"), "roblox_fishing/bobber_diamond");
    nt_builder_add_shader(ctx, "external/neotolis-engine/assets/shaders/mesh_inst.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_rename(ctx, "external/neotolis-engine/assets/shaders/mesh_inst.vert", "roblox_fishing/mesh_inst.vert");
    nt_builder_add_shader(ctx, "external/neotolis-engine/assets/shaders/mesh_inst.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_rename(ctx, "external/neotolis-engine/assets/shaders/mesh_inst.frag", "roblox_fishing/mesh_inst.frag");
    nt_builder_add_shader(ctx, "external/neotolis-engine/assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_rename(ctx, "external/neotolis-engine/assets/shaders/slug_text.vert", "roblox_fishing/slug_text.vert");
    nt_builder_add_shader(ctx, "external/neotolis-engine/assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_rename(ctx, "external/neotolis-engine/assets/shaders/slug_text.frag", "roblox_fishing/slug_text.frag");
    nt_builder_add_font(ctx,
                        "external/neotolis-engine/examples/ui_theme_demo/raw/font.ttf",
                        &(nt_font_opts_t){
                            .charset = NT_CHARSET_ASCII,
                            .resource_name = "roblox_fishing/font",
                        });
    nt_builder_add_texture_raw(ctx, white_rgba, 1, 1, "roblox_fishing/white", NULL);

    nt_build_result_t result = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (result != NT_BUILD_OK) {
        (void)fprintf(stderr, "roblox_fishing_models pack failed: %d\n", (int)result);
        return 1;
    }

    (void)printf("Built roblox_fishing_models.ntpack in %s\n", out_dir);
    return 0;
}
