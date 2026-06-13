/* Build 67 World runtime art pack.
 * Usage: build_67_world_packs <pack_dir>
 * Run from the project root after:
 *   py -3.12 tools/project_67_world/assets/build_67_world_art.py
 */

/* clang-format off */
#include "nt_builder.h"
/* clang-format on */

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

#define HEADER_DIR "src/generated/assets"
#define RUNTIME_DIR "assets/runtime/67-world"
#define FONT_PATH "external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf"

typedef struct {
    const char *id;
    const char *path;
    const char *kind;
    uint16_t s9_left;
    uint16_t s9_top;
    uint16_t s9_right;
    uint16_t s9_bottom;
    float pivot_x;
    float pivot_y;
} atlas_asset_t;

static const atlas_asset_t ASSETS[] = {
    {"tiny_67", RUNTIME_DIR "/tiny_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"berry_67", RUNTIME_DIR "/berry_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"banana_67", RUNTIME_DIR "/banana_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"smoothie_67", RUNTIME_DIR "/smoothie_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"cool_67", RUNTIME_DIR "/cool_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"portal_67", RUNTIME_DIR "/portal_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"mystery_67", RUNTIME_DIR "/mystery_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"jelly_67", RUNTIME_DIR "/jelly_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"lemon_67", RUNTIME_DIR "/lemon_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"watermelon_67", RUNTIME_DIR "/watermelon_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"bubblegum_67", RUNTIME_DIR "/bubblegum_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"sticker_67", RUNTIME_DIR "/sticker_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"party_67", RUNTIME_DIR "/party_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"arcade_67", RUNTIME_DIR "/arcade_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"cloud_67", RUNTIME_DIR "/cloud_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"crown_67", RUNTIME_DIR "/crown_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"rocket_67", RUNTIME_DIR "/rocket_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"rainbow_67", RUNTIME_DIR "/rainbow_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"neon_67", RUNTIME_DIR "/neon_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"gummy_67", RUNTIME_DIR "/gummy_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"pixel_67", RUNTIME_DIR "/pixel_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"lava_67", RUNTIME_DIR "/lava_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"donut_67", RUNTIME_DIR "/donut_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"slime_67", RUNTIME_DIR "/slime_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"disco_67", RUNTIME_DIR "/disco_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"dragon_67", RUNTIME_DIR "/dragon_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"ninja_67", RUNTIME_DIR "/ninja_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"galaxy_67", RUNTIME_DIR "/galaxy_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"golden_67", RUNTIME_DIR "/golden_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"cosmic_67", RUNTIME_DIR "/cosmic_67-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.82F},
    {"mystery_crate", RUNTIME_DIR "/mystery_crate-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"panel_wide_blue", RUNTIME_DIR "/panel_wide_blue-v1.png", "slice9", 72, 64, 72, 64, 0.5F, 0.5F},
    {"card_unlocked", RUNTIME_DIR "/card_unlocked-v1.png", "slice9", 58, 68, 58, 68, 0.5F, 0.5F},
    {"card_locked", RUNTIME_DIR "/card_locked-v1.png", "slice9", 50, 64, 50, 64, 0.5F, 0.5F},
    {"board_tile", RUNTIME_DIR "/board_tile-v1.png", "tile", 36, 36, 36, 36, 0.5F, 0.5F},
    {"board_frame", RUNTIME_DIR "/board_frame-v1.png", "border", 58, 58, 58, 58, 0.5F, 0.5F},
    {"highlight_gold", RUNTIME_DIR "/highlight_gold-v1.png", "slice9", 42, 42, 42, 42, 0.5F, 0.5F},
    {"highlight_electric", RUNTIME_DIR "/highlight_electric-v1.png", "slice9", 36, 36, 36, 36, 0.5F, 0.5F},
    {"button_orange", RUNTIME_DIR "/button_orange-v1.png", "slice9", 48, 36, 48, 36, 0.5F, 0.5F},
    {"button_orange_pressed", RUNTIME_DIR "/button_orange_pressed-v1.png", "slice9", 48, 36, 48, 36, 0.5F, 0.5F},
    {"button_blue", RUNTIME_DIR "/button_blue-v1.png", "slice9", 48, 36, 48, 36, 0.5F, 0.5F},
    {"button_green", RUNTIME_DIR "/button_green-v1.png", "slice9", 48, 36, 48, 36, 0.5F, 0.5F},
    {"button_disabled", RUNTIME_DIR "/button_disabled-v1.png", "slice9", 48, 36, 48, 36, 0.5F, 0.5F},
    {"progress_empty", RUNTIME_DIR "/progress_empty-v1.png", "slice9", 24, 16, 24, 16, 0.5F, 0.5F},
    {"progress_blue", RUNTIME_DIR "/progress_blue-v1.png", "slice9", 24, 16, 24, 16, 0.5F, 0.5F},
    {"progress_green", RUNTIME_DIR "/progress_green-v1.png", "slice9", 24, 16, 24, 16, 0.5F, 0.5F},
    {"icon_coin_crown", RUNTIME_DIR "/icon_coin_crown-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"icon_gem", RUNTIME_DIR "/icon_gem-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"icon_plus", RUNTIME_DIR "/icon_plus-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"icon_menu", RUNTIME_DIR "/icon_menu-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"icon_star", RUNTIME_DIR "/icon_star-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"icon_lock", RUNTIME_DIR "/icon_lock-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"icon_help", RUNTIME_DIR "/icon_help-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"icon_67_badge", RUNTIME_DIR "/icon_67_badge-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"icon_mystery_crate", RUNTIME_DIR "/icon_mystery_crate-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"small_gem_corner", RUNTIME_DIR "/small_gem_corner-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"small_arrow_blue", RUNTIME_DIR "/small_arrow_blue-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"small_diamond", RUNTIME_DIR "/small_diamond-v1.png", "icon", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_grass_tile", RUNTIME_DIR "/field_grass_tile-v1.png", "tile", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_dark_grass_tile", RUNTIME_DIR "/field_dark_grass_tile-v1.png", "tile", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_light_grass_tile", RUNTIME_DIR "/field_light_grass_tile-v1.png", "tile", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_path_tile", RUNTIME_DIR "/field_path_tile-v1.png", "tile", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_ground_shadow", RUNTIME_DIR "/field_ground_shadow-v1.png", "effect", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_fence_post", RUNTIME_DIR "/field_fence_post-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.92F},
    {"field_fence_rail_h", RUNTIME_DIR "/field_fence_rail_h-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_fence_rail_v", RUNTIME_DIR "/field_fence_rail_v-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_hud_panel", RUNTIME_DIR "/field_hud_panel-v1.png", "slice9", 72, 54, 72, 54, 0.5F, 0.5F},
    {"field_catalog_panel", RUNTIME_DIR "/field_catalog_panel-v1.png", "slice9", 72, 78, 72, 78, 0.5F, 0.5F},
    {"field_catalog_drawer", RUNTIME_DIR "/field_catalog_drawer-v1.png", "slice9", 86, 58, 86, 58, 0.5F, 0.5F},
    {"field_tutorial_plaque", RUNTIME_DIR "/field_tutorial_plaque-v1.png", "slice9", 44, 38, 44, 38, 0.5F, 0.5F},
    {"field_button_green", RUNTIME_DIR "/field_button_green-v1.png", "slice9", 69, 68, 80, 68, 0.5F, 0.5F},
    {"field_button_disabled", RUNTIME_DIR "/field_button_disabled-v1.png", "slice9", 73, 68, 80, 68, 0.5F, 0.5F},
    {"field_selection_gold", RUNTIME_DIR "/field_selection_gold-v1.png", "effect", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_selection_blue", RUNTIME_DIR "/field_selection_blue-v1.png", "effect", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_reward_spark", RUNTIME_DIR "/field_reward_spark-v1.png", "effect", 0, 0, 0, 0, 0.5F, 0.5F},
    {"field_mystery_crate", RUNTIME_DIR "/field_mystery_crate-v1.png", "sprite", 0, 0, 0, 0, 0.5F, 0.74F},
};

static char s_path_buf[512];

static const char *pack_path(const char *dir, const char *name) {
    (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
    return s_path_buf;
}

static void add_asset(NtBuilderContext *ctx, const atlas_asset_t *asset) {
    nt_atlas_sprite_opts_t opts = nt_atlas_sprite_opts_defaults();
    opts.name = asset->id;
    opts.origin_x = asset->pivot_x;
    opts.origin_y = asset->pivot_y;
    opts.slice9_left = asset->s9_left;
    opts.slice9_top = asset->s9_top;
    opts.slice9_right = asset->s9_right;
    opts.slice9_bottom = asset->s9_bottom;
    nt_builder_atlas_add(ctx, asset->path, &opts);
    (void)printf("  %-22s %-8s %s\n", asset->id, asset->kind, asset->path);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        (void)fprintf(stderr, "Usage: build_67_world_packs <pack_dir>\n");
        return 1;
    }

    const char *out_dir = argv[1];
    (void)printf("=== Build 67 World Pack -> %s ===\n\n", out_dir);

    MKDIR(out_dir);
    MKDIR("src/generated");
    MKDIR(HEADER_DIR);

    char cache_dir[512];
    (void)snprintf(cache_dir, sizeof(cache_dir), "%s/_cache", out_dir);
    MKDIR(cache_dir);

    NtBuilderContext *ctx = nt_builder_start_pack(pack_path(out_dir, "world67_art.ntpack"));
    if (!ctx) {
        (void)fprintf(stderr, "Failed to start world67_art.ntpack\n");
        return 1;
    }

    nt_builder_set_header_dir(ctx, HEADER_DIR);
    nt_builder_set_cache_dir(ctx, cache_dir);
    nt_builder_set_threads_auto(ctx);
    (void)nt_builder_add_asset_root(ctx, "external/neotolis-engine");

    nt_builder_add_shader(ctx, "assets/shaders/sprite.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/sprite.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);

    nt_atlas_opts_t atlas_opts = nt_atlas_opts_defaults();
    atlas_opts.shape = NT_ATLAS_SHAPE_RECT;
    atlas_opts.allow_transform = false;
    atlas_opts.pixels_per_unit = 1.0F;
    atlas_opts.padding = 2;
    atlas_opts.margin = 2;
    atlas_opts.extrude = 1;
    atlas_opts.premultiplied = true;
    atlas_opts.compress = NULL;
    atlas_opts.filter_min = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    atlas_opts.filter_mag = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    atlas_opts.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    atlas_opts.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    atlas_opts.gen_mipmaps = false;

    nt_builder_begin_atlas(ctx, "world67_art_atlas", &atlas_opts);

    for (uint32_t i = 0; i < (uint32_t)(sizeof(ASSETS) / sizeof(ASSETS[0])); ++i) {
        add_asset(ctx, &ASSETS[i]);
    }

    static const uint8_t white_pixel[4] = {255, 255, 255, 255};
    nt_atlas_sprite_opts_t white_opts = nt_atlas_sprite_opts_defaults();
    white_opts.name = "_white";
    nt_builder_atlas_add_raw(ctx, white_pixel, 1, 1, &white_opts);

    nt_builder_end_atlas(ctx);

    nt_builder_add_font(ctx, FONT_PATH,
                        &(nt_font_opts_t){
                            .charset = NT_CHARSET_ASCII,
                            .resource_name = "world67/font",
                        });

    nt_build_result_t result = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (result != NT_BUILD_OK) {
        (void)fprintf(stderr, "world67_art.ntpack failed: %d\n", result);
        return 1;
    }

    char base_header[512];
    (void)snprintf(base_header, sizeof(base_header), "%s/world67_art.h", HEADER_DIR);
    const char *headers[] = {base_header};
    char combined[512];
    (void)snprintf(combined, sizeof(combined), "%s/world67_assets.h", HEADER_DIR);
    nt_builder_merge_headers(headers, 1, combined);
    (void)printf("Generated: %s\n", combined);

    FILE *f = fopen(pack_path(out_dir, "world67_art.ntpack"), "rb");
    if (f) {
        (void)fseek(f, 0, SEEK_END);
        long size = ftell(f);
        (void)fclose(f);
        (void)printf("Pack: world67_art.ntpack %.1f KB\n", (double)size / 1024.0);
    }

    return 0;
}
