/* Minimal game pack builder.
 * Usage: build_game_67_idle_pack <pack_dir>
 */

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

static char s_path[512];

static const char *join_path(const char *dir, const char *name) {
    (void)snprintf(s_path, sizeof(s_path), "%s/%s", dir, name);
    return s_path;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        (void)fprintf(stderr, "Usage: build_game_67_idle_pack <pack_dir>\n");
        return 1;
    }

    const char *out_dir = argv[1];
    (void)MKDIR(out_dir);
    (void)MKDIR(HEADER_DIR);

    char cache_dir[512];
    (void)snprintf(cache_dir, sizeof(cache_dir), "%s/_cache", out_dir);
    (void)MKDIR(cache_dir);

    NtBuilderContext *ctx = nt_builder_start_pack(join_path(out_dir, "game_67_idle.ntpack"));
    if (!ctx) {
        (void)fprintf(stderr, "Failed to start game_67_idle.ntpack\n");
        return 1;
    }

    nt_builder_set_header_dir(ctx, HEADER_DIR);
    nt_builder_set_cache_dir(ctx, cache_dir);

    static const char k_manifest[] =
        "{\n"
        "  \"game\": \"game_67_idle\",\n"
        "  \"scene\": \"neotolis_smoke_test\",\n"
        "  \"purpose\": \"submodule and pack-builder smoke test\"\n"
        "}\n";
    nt_builder_add_blob(ctx, k_manifest, (uint32_t)(sizeof(k_manifest) - 1U), "game_67_idle/manifest");

    nt_build_result_t result = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (result != NT_BUILD_OK) {
        (void)fprintf(stderr, "game_67_idle.ntpack failed: %d\n", result);
        return 1;
    }

    const char *headers[] = {"src/generated/game_67_idle.h"};
    nt_builder_merge_headers(headers, 1, "src/generated/game_67_idle_assets.h");

    (void)printf("Generated %s\n", join_path(out_dir, "game_67_idle.ntpack"));
    return 0;
}

