#include "nt_builder.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool add_music(NtBuilderContext *ctx, const char *path) {
    static const char prefix[] = "assets/audio/music/";
    static const char suffix[] = ".mp3";
    char id[512];
    const size_t path_length = strlen(path);
    if (path_length < sizeof prefix + sizeof suffix - 1U || path_length - 11U >= sizeof id ||
        strncmp(path, prefix, sizeof prefix - 1U) != 0 ||
        strcmp(path + path_length - 4U, ".mp3") != 0) { return false; }
    FILE *file = fopen(path, "rb");
    if (file == NULL) { return false; }
    if (fseek(file, 0, SEEK_END) != 0) { (void)fclose(file); return false; }
    const long length = ftell(file);
    if (length <= 0 || (uint64_t)length > UINT32_MAX || fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        return false;
    }
    uint8_t *bytes = malloc((size_t)length);
    const bool read = bytes != NULL && fread(bytes, 1, (size_t)length, file) == (size_t)length;
    const bool closed = fclose(file) == 0;
    const int written = snprintf(id, sizeof id, "%.*s", (int)(path_length - 11U), path + 7U);
    if (read && closed && written > 0 && (size_t)written < sizeof id) {
        nt_builder_add_blob(ctx, bytes, (uint32_t)length, id);
    }
    free(bytes);
    return read && closed && written > 0 && (size_t)written < sizeof id;
}

static const unsigned char s_foundation_marker[] = {0U};
int main(int argc, char *argv[]) {
    if (argc < 3) {
        (void)fprintf(stderr, "Usage: build_game_packs <pack_dir> <header_dir> [music.mp3 ...]\n");
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
    nt_builder_set_header_dir(ctx, argv[2]);
    // The pack format requires one entry; this marker is not game content.
    nt_builder_add_blob(ctx, s_foundation_marker, (uint32_t)sizeof s_foundation_marker,
        "foundation/marker");
    nt_build_result_t result = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (result != NT_BUILD_OK || argc == 3) { return result == NT_BUILD_OK ? 0 : 1; }
    if (snprintf(path, sizeof path, "%s/music.ntpack", argv[1]) >= (int)sizeof path) { return 1; }
    ctx = nt_builder_start_pack(path);
    if (ctx == NULL) { return 1; }
    nt_builder_set_header_dir(ctx, argv[2]);
    for (int i = 3; i < argc; ++i) {
        if (!add_music(ctx, argv[i])) {
            (void)fprintf(stderr, "Failed to pack music: %s\n", argv[i]);
            nt_builder_free_pack(ctx);
            return 1;
        }
    }
    result = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    return result == NT_BUILD_OK ? 0 : 1;
}
