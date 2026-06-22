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
