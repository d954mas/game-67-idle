#include "nt_builder.h"

#include <stdint.h>
#include <stdio.h>

static const unsigned char s_foundation_marker[] = {0U};
int main(int argc, char *argv[]) {
    if (argc != 2) {
        (void)fprintf(stderr, "Usage: build_game_packs <pack_dir>\n");
        return 1;
    }
    char path[512];
    if (snprintf(path, sizeof path, "%s/game.ntpack", argv[1]) >= (int)sizeof path) {
        return 1;
    }
    NtBuilderContext *ctx = nt_builder_start_pack(path);
    if (ctx == NULL) {
        (void)fprintf(stderr, "failed to start empty game pack\n");
        return 1;
    }
    // The pack format requires one entry; this marker is not game content.
    nt_builder_add_blob(ctx, s_foundation_marker, (uint32_t)sizeof s_foundation_marker,
        "foundation/marker");
    const nt_build_result_t result = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    return result == NT_BUILD_OK ? 0 : 1;
}
