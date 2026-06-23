// Builds little_lives.ntpack (UI font + Slug text shaders) and the generated
// asset-id header. Runs natively at build time; the runtime loads the pack.
// Usage: little_lives_build_packs <pack_dir> <header_dir>   (run from repo root)
/* clang-format off */
#include "nt_builder.h"
/* clang-format on */

#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static char s_buf[512];
static const char *join(const char *dir, const char *name) {
    (void)snprintf(s_buf, sizeof(s_buf), "%s/%s", dir, name);
    return s_buf;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        (void)fprintf(stderr, "usage: little_lives_build_packs <pack_dir> <header_dir>\n");
        return 1;
    }
    const char *out_dir = argv[1];
    const char *hdr_dir = argv[2];
    MKDIR(out_dir);
    MKDIR(hdr_dir);

    char cache_dir[512];
    (void)snprintf(cache_dir, sizeof(cache_dir), "%s/_cache", out_dir);
    MKDIR(cache_dir);

    NtBuilderContext *ctx = nt_builder_start_pack(join(out_dir, "little_lives.ntpack"));
    if (!ctx) {
        (void)fprintf(stderr, "failed to start little_lives.ntpack\n");
        return 1;
    }
    nt_builder_set_header_dir(ctx, hdr_dir);
    nt_builder_set_cache_dir(ctx, cache_dir);
    nt_builder_set_threads_auto(ctx);

    nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);

    nt_builder_add_font(ctx, "assets/fonts/LilitaOne.ttf",
                        &(nt_font_opts_t){
                            .charset = NT_CHARSET_ASCII,
                            .resource_name = "little_lives/ui_font",
                        });

    // 3D mesh reuse loop: instanced mesh shaders + a neutral 1x1 white texture
    // (mesh_inst.frag samples u_texture * v_color; library glbs are untextured,
    // so colour is delivered per-instance and the texture stays white). The cube
    // is the single-primitive proving mesh; real furniture uses the scene API.
    nt_builder_add_shader(ctx, "assets/shaders/mesh_inst.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/mesh_inst.frag", NT_BUILD_SHADER_FRAGMENT);

    static const uint8_t white_px[4] = {255, 255, 255, 255};
    nt_tex_opts_t white_opts = nt_tex_opts_defaults();
    nt_builder_add_texture_raw(ctx, white_px, 1, 1, "little_lives/white", &white_opts);

    NtStreamLayout mesh_layout[] = {
        {"position", "POSITION", NT_STREAM_FLOAT32, 3, false},
        {"uv0", "TEXCOORD_0", NT_STREAM_FLOAT32, 2, false},
    };
    nt_builder_add_mesh(ctx, "assets/meshes/cube.glb",
                        &(nt_mesh_opts_t){.layout = mesh_layout, .stream_count = 2, .tangent_mode = NT_TANGENT_NONE});

    nt_build_result_t r = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (r != NT_BUILD_OK) {
        (void)fprintf(stderr, "little_lives.ntpack failed: %d\n", r);
        return 1;
    }

    char base_hdr[512];
    (void)snprintf(base_hdr, sizeof(base_hdr), "%s/little_lives.h", hdr_dir);
    const char *headers[] = {base_hdr};
    char combined[512];
    (void)snprintf(combined, sizeof(combined), "%s/little_lives_assets.h", hdr_dir);
    nt_builder_merge_headers(headers, 1, combined);
    (void)printf("generated %s\n", combined);
    return 0;
}
