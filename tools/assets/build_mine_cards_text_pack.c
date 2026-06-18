#define NT_BUILD_MAX_ASSETS 128
#include "nt_builder.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

static char s_path_buf[512];

static const char *pack_path(const char *dir, const char *name) {
    (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
    return s_path_buf;
}

int main(int argc, char **argv) {
    const char *out_dir = argc >= 2 ? argv[1] : "assets";
    const char *header_dir = "build/assets/mine_cards_text/generated";
    const char *cache_dir = "build/assets/mine_cards_text/_cache";
    (void)MKDIR("assets");
    (void)MKDIR("build");
    (void)MKDIR("build/assets");
    (void)MKDIR("build/assets/mine_cards_text");
    (void)MKDIR(header_dir);
    (void)MKDIR(cache_dir);
    (void)MKDIR(out_dir);

    NtBuilderContext *ctx = nt_builder_start_pack(pack_path(out_dir, "mine_cards_text.ntpack"));
    if (ctx == NULL) {
        (void)fprintf(stderr, "failed to start mine_cards_text.ntpack\n");
        return 1;
    }
    nt_builder_set_header_dir(ctx, header_dir);
    nt_builder_set_cache_dir(ctx, cache_dir);

    nt_builder_add_shader(ctx, "external/neotolis-engine/assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "external/neotolis-engine/assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_font(ctx,
                        "gamedesign/projects/mine-cards/visual/source_fonts/Kenney Future.ttf",
                        &(nt_font_opts_t){
                            .charset = NT_CHARSET_ASCII,
                            .resource_name = "mine-cards/font_ui",
                        });

    const nt_build_result_t result = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (result != NT_BUILD_OK) {
        (void)fprintf(stderr, "mine_cards_text.ntpack failed: %d\n", (int)result);
        return 1;
    }

    (void)printf("Built %s\n", pack_path(out_dir, "mine_cards_text.ntpack"));
    return 0;
}
