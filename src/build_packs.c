#define NT_BUILD_MAX_ASSETS 2048
#include "nt_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

#define HEADER_DIR "src/generated"

static char s_path_buf[512];

static const char *pack_path(const char *dir, const char *name) {
    (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
    return s_path_buf;
}

static void add_model(NtBuilderContext *ctx, const char *path, const char *rid_prefix) {
    NtStreamLayout layout[] = {
        {"position", "POSITION", NT_STREAM_FLOAT32, 3, false},
    };
    nt_glb_scene_t scene = {0};
    nt_build_result_t parsed = nt_builder_parse_glb_scene(&scene, path);
    if (parsed != NT_BUILD_OK) {
        (void)fprintf(stderr, "Failed to parse scene: %s\n", path);
        exit(1);
    }
    for (uint32_t mi = 0; mi < scene.mesh_count; ++mi) {
        for (uint32_t pi = 0; pi < scene.meshes[mi].primitive_count; ++pi) {
            char rid[160];
            (void)snprintf(rid, sizeof(rid), "%s/%u_%u", rid_prefix, mi, pi);
            nt_builder_add_scene_mesh(ctx, &scene, mi, pi, rid,
                                      &(nt_mesh_opts_t){
                                          .layout = layout,
                                          .stream_count = 1,
                                          .tangent_mode = NT_TANGENT_NONE,
                                      });
        }
    }
    nt_builder_free_glb_scene(&scene);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        (void)fprintf(stderr, "Usage: build_game_packs <pack_dir>\n");
        return 1;
    }

    const char *out_dir = argv[1];
    (void)MKDIR(out_dir);
    (void)MKDIR(HEADER_DIR);

    NtBuilderContext *ctx = nt_builder_start_pack(pack_path(out_dir, "blockside_heat.ntpack"));
    if (!ctx) {
        (void)fprintf(stderr, "Failed to start blockside_heat pack\n");
        return 1;
    }
    nt_builder_set_header_dir(ctx, HEADER_DIR);
    nt_builder_set_cache_dir(ctx, "build/blockside_heat/_cache");

    add_model(ctx, "assets/source/models/polypizza__street-straight-ua4axt3niq__cc0-1-0/street-straight.glb", "blockside/street_straight");
    add_model(ctx, "assets/source/models/polypizza__street-t-xexepq3jdr__cc0-1-0/street-t.glb", "blockside/street_t");
    add_model(ctx, "assets/source/models/blockside_city_base/blockside-city-base.gltf", "blockside/city_base");
    add_model(ctx, "assets/source/models/polypizza__low-building-4ropd9bksx__cc0-1-0/low-building.glb", "blockside/low_building");
    add_model(ctx, "assets/source/models/polypizza__large-building-1bt4yykmuk__cc0-1-0/large-building.glb", "blockside/large_building");
    add_model(ctx, "assets/source/models/polypizza__street-light-0lxf8dl1ju__cc0-1-0/street-light.glb", "blockside/street_light");
    add_model(ctx, "assets/source/models/polypizza__car-cz6ydaucm9__cc0-1-0/car.glb", "blockside/car");
    add_model(ctx, "assets/source/models/polypizza__character-soldier-pplf4rt4ah__cc0-1-0/character-soldier.glb", "blockside/character");
    add_model(ctx, "assets/source/models/polypizza__cardboard-boxes-zt2wlyypml__cc0-1-0/cardboard-boxes.glb", "blockside/package");

    nt_builder_add_shader(ctx, "assets/shaders/blockside_mesh_inst.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/blockside_mesh_inst.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);

    nt_builder_add_font(ctx, "external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf",
                        &(nt_font_opts_t){
                            .charset = NT_CHARSET_ASCII,
                            .resource_name = "blockside/font",
                        });

    nt_build_result_t r = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (r != NT_BUILD_OK) {
        (void)fprintf(stderr, "blockside_heat pack failed: %d\n", r);
        return 1;
    }

    const char *headers[] = {"src/generated/blockside_heat.h"};
    nt_builder_merge_headers(headers, 1, "src/generated/blockside_heat_assets.h");
    (void)printf("Built blockside_heat.ntpack\n");
    return 0;
}
