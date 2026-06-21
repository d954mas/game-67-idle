/*
 * Build Ember Road runtime asset pack:
 *   ember_road_base.ntpack -- UI font, text shaders, sprite shaders, and
 *   Old Gate first-slice atlas art and partial town-forge v2 source-derived
 *   runtime crops.
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

#ifndef EMBER_ROAD_FONT_PATH
#define EMBER_ROAD_FONT_PATH "external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf"
#endif

#define OLD_GATE_RUNTIME_DIR "assets/runtime/ember-road-old-gate-fakeshot-v001"
#define TOWN_FORGE_RUNTIME_DIR "assets/runtime/ember-road-town-forge-v2"

typedef struct AtlasRegionInput {
    const char *name;
    uint16_t slice9_left;
    uint16_t slice9_right;
    uint16_t slice9_top;
    uint16_t slice9_bottom;
} AtlasRegionInput;

/* Cyrillic: basic Russian letters U+0410-U+044F plus Yo. */
/* clang-format off */
static const char CYRILLIC_CHARSET[] =
    "\xd0\x90\xd0\x91\xd0\x92\xd0\x93\xd0\x94\xd0\x95\xd0\x96\xd0\x97"
    "\xd0\x98\xd0\x99\xd0\x9a\xd0\x9b\xd0\x9c\xd0\x9d\xd0\x9e\xd0\x9f"
    "\xd0\xa0\xd0\xa1\xd0\xa2\xd0\xa3\xd0\xa4\xd0\xa5\xd0\xa6\xd0\xa7"
    "\xd0\xa8\xd0\xa9\xd0\xaa\xd0\xab\xd0\xac\xd0\xad\xd0\xae\xd0\xaf"
    "\xd0\xb0\xd0\xb1\xd0\xb2\xd0\xb3\xd0\xb4\xd0\xb5\xd0\xb6\xd0\xb7"
    "\xd0\xb8\xd0\xb9\xd0\xba\xd0\xbb\xd0\xbc\xd0\xbd\xd0\xbe\xd0\xbf"
    "\xd1\x80\xd1\x81\xd1\x82\xd1\x83\xd1\x84\xd1\x85\xd1\x86\xd1\x87"
    "\xd1\x88\xd1\x89\xd1\x8a\xd1\x8b\xd1\x8c\xd1\x8d\xd1\x8e\xd1\x8f"
    "\xd0\x81\xd1\x91";
/* clang-format on */

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    (void)fclose(file);
    return true;
}

static const char *join_path(char *buffer, size_t cap, const char *a, const char *b) {
    (void)snprintf(buffer, cap, "%s/%s", a, b);
    buffer[cap - 1] = '\0';
    return buffer;
}

static bool add_runtime_region(NtBuilderContext *ctx, const char *dir, const char *label, const AtlasRegionInput *region) {
    char path[512];
    (void)snprintf(path, sizeof(path), "%s/%s.png", dir, region->name);
    path[sizeof(path) - 1] = '\0';
    if (!file_exists(path)) {
        (void)fprintf(stderr, "ERROR: %s runtime sprite not found: %s\n", label, path);
        return false;
    }

    nt_atlas_sprite_opts_t opts = nt_atlas_sprite_opts_defaults();
    opts.name = region->name;
    opts.slice9_left = region->slice9_left;
    opts.slice9_right = region->slice9_right;
    opts.slice9_top = region->slice9_top;
    opts.slice9_bottom = region->slice9_bottom;
    nt_builder_atlas_add(ctx, path, &opts);
    return true;
}

static bool add_old_gate_region(NtBuilderContext *ctx, const AtlasRegionInput *region) {
    return add_runtime_region(ctx, OLD_GATE_RUNTIME_DIR, "Old Gate", region);
}

static bool add_town_forge_region(NtBuilderContext *ctx, const AtlasRegionInput *region) {
    return add_runtime_region(ctx, TOWN_FORGE_RUNTIME_DIR, "Town Forge v2", region);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        (void)fprintf(stderr, "Usage: build_ember_road_packs <pack_dir> <header_dir>\n");
        return 1;
    }

    const char *pack_dir = argv[1];
    const char *header_dir = argv[2];
    char cache_dir[512];
    char pack_path[512];

    (void)snprintf(cache_dir, sizeof(cache_dir), "%s/_cache", pack_dir);
    cache_dir[sizeof(cache_dir) - 1] = '\0';

    (void)MKDIR(pack_dir);
    (void)MKDIR(header_dir);
    (void)MKDIR(cache_dir);

    const char *font_path = EMBER_ROAD_FONT_PATH;
    if (!file_exists(font_path)) {
        (void)fprintf(stderr, "ERROR: font not found: %s\n", font_path);
        return 1;
    }

    NtBuilderContext *ctx = nt_builder_start_pack(join_path(pack_path, sizeof(pack_path), pack_dir, "ember_road_base.ntpack"));
    if (!ctx) {
        (void)fprintf(stderr, "ERROR: failed to start pack: %s\n", pack_path);
        return 1;
    }
    nt_builder_set_header_dir(ctx, header_dir);
    nt_builder_set_cache_dir(ctx, cache_dir);

    nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_shader(ctx, "assets/shaders/sprite.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/sprite.frag", NT_BUILD_SHADER_FRAGMENT);

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

    static const AtlasRegionInput old_gate_regions[] = {
        {"bottom_log_belt", 18, 18, 16, 16},
        {"claim_check", 0, 0, 0, 0},
        {"corner_cap_a", 0, 0, 0, 0},
        {"corner_cap_b", 0, 0, 0, 0},
        {"corner_cap_c", 0, 0, 0, 0},
        {"destination_plaque_a", 14, 14, 12, 12},
        {"destination_plaque_b", 14, 14, 12, 12},
        {"destination_plaque_c", 14, 14, 12, 12},
        {"destination_plaque_d", 14, 14, 12, 12},
        {"ember_particles", 0, 0, 0, 0},
        {"ember_slash", 0, 0, 0, 0},
        {"gate_warden_portrait", 0, 0, 0, 0},
        {"gate_warden_standing", 0, 0, 0, 0},
        {"gem_gold", 0, 0, 0, 0},
        {"gem_red", 0, 0, 0, 0},
        {"gem_small", 0, 0, 0, 0},
        {"gold_coin", 0, 0, 0, 0},
        {"hanging_brazier", 0, 0, 0, 0},
        {"hero_back", 0, 0, 0, 0},
        {"hero_combat", 0, 0, 0, 0},
        {"locked_mine", 0, 0, 0, 0},
        {"lock_overlay", 0, 0, 0, 0},
        {"medallion_gold", 0, 0, 0, 0},
        {"medallion_red", 0, 0, 0, 0},
        {"north_road_backdrop", 0, 0, 0, 0},
        {"old_mine_backdrop", 0, 0, 0, 0},
        {"old_gate_backdrop", 0, 0, 0, 0},
        {"portrait_round_frame", 0, 0, 0, 0},
        {"primary_button_default", 20, 20, 16, 16},
        {"primary_button_disabled", 20, 20, 16, 16},
        {"primary_button_pressed", 20, 20, 16, 16},
        {"primary_button_selected", 20, 20, 16, 16},
        {"quest_highlight_ring", 0, 0, 0, 0},
        {"quest_marker", 0, 0, 0, 0},
        {"quest_rail_panel", 20, 20, 20, 20},
        {"rail_arc_a", 0, 0, 0, 0},
        {"rail_arc_b", 0, 0, 0, 0},
        {"rail_arc_c", 0, 0, 0, 0},
        {"rail_arc_d", 0, 0, 0, 0},
        {"reward_slot_a", 12, 12, 12, 12},
        {"reward_slot_b", 12, 12, 12, 12},
        {"reward_slot_c", 12, 12, 12, 12},
        {"reward_slot_d", 12, 12, 12, 12},
        {"reward_slot_e", 12, 12, 12, 12},
        {"ring_reward", 0, 0, 0, 0},
        {"road_wolf_combat", 0, 0, 0, 0},
        {"road_wolf_side", 0, 0, 0, 0},
        {"route_arrow", 0, 0, 0, 0},
        {"route_plaque_frame", 14, 14, 14, 14},
        {"route_strip_base", 18, 18, 14, 14},
        {"small_panel", 12, 12, 12, 12},
        {"status_meter_stack", 10, 10, 8, 8},
        {"sword_auto_battle", 0, 0, 0, 0},
        {"top_status_frame", 18, 18, 16, 16},
        {"torch_wall", 0, 0, 0, 0},
        {"wide_panel", 18, 18, 16, 16},
        {"wolf_marker", 0, 0, 0, 0},
        {"xp_spark", 0, 0, 0, 0},
    };

    static const AtlasRegionInput town_forge_regions[] = {
        {"forge_action_panel_v2", 0, 0, 0, 0},
        {"forge_floor_patch_v2", 0, 0, 0, 0},
        {"forge_lantern_ready_badge_v2", 0, 0, 0, 0},
        {"forge_result_strip_slice9_v2", 48, 48, 24, 24},
        {"forge_signpost_v2", 0, 0, 0, 0},
        {"forge_workshop_v2", 0, 0, 0, 0},
        {"forge_worktable_v2", 0, 0, 0, 0},
        {"mine_lantern_standalone_v2", 0, 0, 0, 0},
    };

    nt_builder_begin_atlas(ctx, "ember_road_old_gate_atlas", &atlas_opts);
    for (size_t i = 0; i < sizeof(old_gate_regions) / sizeof(old_gate_regions[0]); ++i) {
        if (!add_old_gate_region(ctx, &old_gate_regions[i])) {
            nt_builder_free_pack(ctx);
            return 1;
        }
    }
    for (size_t i = 0; i < sizeof(town_forge_regions) / sizeof(town_forge_regions[0]); ++i) {
        if (!add_town_forge_region(ctx, &town_forge_regions[i])) {
            nt_builder_free_pack(ctx);
            return 1;
        }
    }
    nt_builder_end_atlas(ctx);

    char charset[512];
    (void)snprintf(charset, sizeof(charset), "%s%s", NT_CHARSET_ASCII, CYRILLIC_CHARSET);
    charset[sizeof(charset) - 1] = '\0';
    nt_builder_add_font(ctx, font_path, &(nt_font_opts_t){
                                          .charset = charset,
                                          .resource_name = "ember_road/font_ui",
                                      });

    const nt_build_result_t result = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (result != NT_BUILD_OK) {
        (void)fprintf(stderr, "ERROR: ember_road_base.ntpack failed: %d\n", (int)result);
        return 1;
    }

    (void)printf("built %s\n", pack_path);
    return 0;
}
