// Template pack builder: packs the starter-shell assets into game.ntpack + a
// generated asset-id header. Runs natively at build time (CMake custom command).
// A game extends this with its own meshes/textures; the shell below is what every
// game starts with: a font (text), the text shader, the instanced-mesh shader, and
// a 1x1 white texture (neutral u_texture for per-instance-coloured meshes).
#define NT_BUILD_MAX_ASSETS 4096
#include "nt_builder.h"

#include <stdint.h>
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
static const char *pack_path(const char *dir, const char *name) {
    (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
    return s_path_buf;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        (void)fprintf(stderr, "Usage: build_game_packs <pack_dir>\n");
        return 1;
    }
    const char *out_dir = argv[1];
    (void)MKDIR(out_dir);
    (void)MKDIR("src");
    (void)MKDIR(HEADER_DIR);

    NtBuilderContext *ctx = nt_builder_start_pack(pack_path(out_dir, "game.ntpack"));
    if (!ctx) {
        (void)fprintf(stderr, "Failed to start game pack\n");
        return 1;
    }
    nt_builder_set_header_dir(ctx, HEADER_DIR);
    (void)MKDIR("build");
    nt_builder_set_cache_dir(ctx, "build/_cache");

    // text shell
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_font(ctx, "../external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf",
                        &(nt_font_opts_t){.charset = NT_CHARSET_ASCII, .resource_name = "game/font"});

    // instanced-mesh shell (coloured via per-instance colour + neutral white texture;
    // a game packs its own meshes/textures on top)
    nt_builder_add_shader(ctx, "assets/shaders/mesh_inst.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/mesh_inst.frag", NT_BUILD_SHADER_FRAGMENT);
    static const uint8_t white_px[4] = {255, 255, 255, 255};
    nt_tex_opts_t white_opts = nt_tex_opts_defaults();
    nt_builder_add_texture_raw(ctx, white_px, 1, 1, "game/white", &white_opts);

    nt_build_result_t r = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (r != NT_BUILD_OK) {
        (void)fprintf(stderr, "game pack failed: %d\n", r);
        return 1;
    }
    const char *headers[] = {HEADER_DIR "/game.h"};
    nt_builder_merge_headers(headers, 1, HEADER_DIR "/game_assets.h");
    (void)printf("Built game.ntpack\n");
    return 0;
}
