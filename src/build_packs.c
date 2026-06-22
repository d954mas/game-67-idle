/*
 * Build the Cozy Automation runtime pack:
 *   assets/cozy_automation.ntpack -- sprite + slug_text shaders, the UI font,
 *   and a "garden" atlas packing the cozy PNGs in assets/raw/cozy/.
 *
 * Usage: build_cozy_packs <pack_dir>
 * Run from the project root (paths below are project-root relative).
 *
 * Emits assets/cozy_automation.ntpack (after CMake copies it from <pack_dir>)
 * + src/generated/cozy_automation_assets.h (region/font/shader id macros).
 * Model: the engine bunnymark/atlas examples + the Voxelheim pack builder.
 */

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
#define PACK_NAME "cozy_automation.ntpack"

static char s_path_buf[512];

static const char *pack_path(const char *dir, const char *name) {
    (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
    return s_path_buf;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        (void)fprintf(stderr, "Usage: build_cozy_packs <pack_dir>\n");
        return 1;
    }
    const char *out_dir = argv[1];

    (void)printf("=== Build Cozy Automation Pack -> %s ===\n\n", out_dir);

    MKDIR(out_dir);
    MKDIR(HEADER_DIR);

    char cache_dir[512];
    (void)snprintf(cache_dir, sizeof(cache_dir), "%s/_cache", out_dir);
    MKDIR(cache_dir);

    NtBuilderContext *ctx = nt_builder_start_pack(pack_path(out_dir, PACK_NAME));
    if (!ctx) {
        (void)fprintf(stderr, "Failed to start %s\n", PACK_NAME);
        return 1;
    }

    nt_builder_set_header_dir(ctx, HEADER_DIR);
    nt_builder_set_cache_dir(ctx, cache_dir);

    // NOLINTNEXTLINE(concurrency-mt-unsafe) -- single-threaded CLI tool, getenv is fine
    const char *threads_env = getenv("NT_BUILDER_THREADS");
    if (threads_env && threads_env[0] != '\0') {
        uint32_t threads = (uint32_t)strtoul(threads_env, NULL, 10);
        if (threads > 0) {
            nt_builder_set_threads(ctx, threads);
        } else {
            nt_builder_set_threads_auto(ctx);
        }
    } else {
        nt_builder_set_threads_auto(ctx);
    }

    // #region shaders (sprite + slug_text)
    (void)fprintf(stderr, "[packer] adding shaders...\n");
    nt_builder_add_shader(ctx, "assets/shaders/sprite.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/sprite.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);
    (void)printf("  Shaders added: 4 (sprite + slug_text)\n");
    // #endregion

    // #region UI font (ASCII only -- HUD text is Latin)
    (void)fprintf(stderr, "[packer] adding font...\n");
    nt_builder_add_font(ctx, "assets/fonts/cozy_ui.ttf",
                        &(nt_font_opts_t){
                            .charset = NT_CHARSET_ASCII,
                            .resource_name = "cozy/font_ui",
                        });
    (void)printf("  Font (ASCII) added: cozy/font_ui\n");
    // #endregion

    // #region atlas: cozy garden PNGs from assets/raw/cozy/
    nt_atlas_opts_t atlas_opts = nt_atlas_opts_defaults();
    /* RECT shape: simple, fast, predictable. Identity transform only so region
     * UVs are not rotated -- the screen emits them upright at fixed pivots. */
    atlas_opts.shape = NT_ATLAS_SHAPE_RECT;
    atlas_opts.allow_transform = false;
    atlas_opts.pixels_per_unit = 1.0F; /* 1 source pixel == 1 world unit */
    atlas_opts.padding = 2;
    atlas_opts.margin = 2;
    atlas_opts.extrude = 2; /* OK with RECT -- avoids edge bleed at sprite borders */
    atlas_opts.premultiplied = true;
    atlas_opts.max_size = 4096; /* keep all regions on one page */
    /* LINEAR, no mips, clamp -- crisp bright illustration look, no edge bleed. */
    atlas_opts.filter_min = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    atlas_opts.filter_mag = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    atlas_opts.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    atlas_opts.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    atlas_opts.gen_mipmaps = false;

    (void)fprintf(stderr, "[packer] building atlas...\n");
    nt_builder_begin_atlas(ctx, "garden", &atlas_opts);
    /* Glob picks up every cut PNG; each derives its region name from basename
     * (e.g. assets/raw/cozy/bush.png -> garden/bush.png). opts MUST be NULL for
     * glob; centre pivot (0.5, 0.5) applies by default. */
    nt_builder_atlas_add_glob(ctx, "assets/raw/cozy/*.png", NULL);
    nt_builder_end_atlas(ctx);
    (void)printf("  Atlas 'garden' added: assets/raw/cozy/*.png (RECT, ppu=1.0)\n");
    // #endregion

    // #region finish + codegen
    (void)fprintf(stderr, "[packer] finishing pack (encode)...\n");
    nt_build_result_t r = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (r != NT_BUILD_OK) {
        (void)fprintf(stderr, "%s failed: %d\n", PACK_NAME, r);
        return 1;
    }

    char base_hdr[512];
    (void)snprintf(base_hdr, sizeof(base_hdr), "%s/cozy_automation.h", HEADER_DIR);
    const char *headers[] = {base_hdr};
    char combined[512];
    (void)snprintf(combined, sizeof(combined), "%s/cozy_automation_assets.h", HEADER_DIR);
    nt_builder_merge_headers(headers, 1, combined);
    (void)printf("Generated: %s\n", combined);
    // #endregion

    (void)printf("\n=== Pack Size Summary ===\n");
    FILE *f = fopen(pack_path(out_dir, PACK_NAME), "rb");
    if (f) {
        (void)fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        (void)fclose(f);
        (void)printf("  %s    %8.1f KB\n", PACK_NAME, (double)sz / 1024.0);
    }

    (void)printf("\n=== Done ===\n");
    return 0;
}
