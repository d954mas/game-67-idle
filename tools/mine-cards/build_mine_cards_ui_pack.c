#define NT_BUILD_MAX_ASSETS 128
#include "nt_builder.h"

#include <stdint.h>
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

typedef struct Rgba {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} Rgba;

typedef struct UiPatch {
    const char *id;
    int width;
    int height;
    int border;
    Rgba fill;
    Rgba edge;
    Rgba light;
    Rgba shade;
} UiPatch;

static char s_path_buf[512];

static const char *pack_path(const char *dir, const char *name) {
    (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
    return s_path_buf;
}

static void make_dirs(const char *out_dir) {
    (void)MKDIR("assets");
    (void)MKDIR("build");
    (void)MKDIR("build/assets");
    (void)MKDIR("build/assets/mine_cards_ui");
    (void)MKDIR("build/assets/mine_cards_ui/generated");
    (void)MKDIR("build/assets/mine_cards_ui/_cache");
    (void)MKDIR("gamedesign/projects/mine-cards/visual/runtime_ui");
    (void)MKDIR(out_dir);
}

static uint8_t mix_u8(uint8_t a, uint8_t b, int t_num, int t_den) {
    return (uint8_t)(((int)a * (t_den - t_num) + (int)b * t_num) / t_den);
}

static Rgba mix_rgba(Rgba a, Rgba b, int t_num, int t_den) {
    return (Rgba){
        .r = mix_u8(a.r, b.r, t_num, t_den),
        .g = mix_u8(a.g, b.g, t_num, t_den),
        .b = mix_u8(a.b, b.b, t_num, t_den),
        .a = mix_u8(a.a, b.a, t_num, t_den),
    };
}

static void put_pixel(uint8_t *pixels, int width, int x, int y, Rgba c) {
    uint8_t *p = pixels + ((size_t)y * (size_t)width + (size_t)x) * 4U;
    p[0] = c.r;
    p[1] = c.g;
    p[2] = c.b;
    p[3] = c.a;
}

static void fill_patch(uint8_t *pixels, const UiPatch *patch) {
    for (int y = 0; y < patch->height; ++y) {
        for (int x = 0; x < patch->width; ++x) {
            Rgba c = patch->fill;
            const int dist_l = x;
            const int dist_r = patch->width - 1 - x;
            const int dist_b = y;
            const int dist_t = patch->height - 1 - y;
            const int near_edge = dist_l < patch->border || dist_r < patch->border || dist_b < patch->border || dist_t < patch->border;
            if (near_edge) {
                c = patch->edge;
            }
            if (dist_t < patch->border / 2 || dist_l < 2) {
                c = mix_rgba(c, patch->light, 1, 3);
            }
            if (dist_b < patch->border / 2 || dist_r < 2) {
                c = mix_rgba(c, patch->shade, 1, 3);
            }
            if ((x == 0 || y == 0 || x == patch->width - 1 || y == patch->height - 1)) {
                c = patch->shade;
            }
            put_pixel(pixels, patch->width, x, y, c);
        }
    }
}

static void add_patch(NtBuilderContext *ctx, const UiPatch *patch) {
    const size_t size = (size_t)patch->width * (size_t)patch->height * 4U;
    uint8_t *pixels = (uint8_t *)calloc(size, 1U);
    if (pixels == NULL) {
        (void)fprintf(stderr, "failed to allocate ui patch %s\n", patch->id);
        exit(1);
    }
    fill_patch(pixels, patch);
    nt_atlas_sprite_opts_t sprite = nt_atlas_sprite_opts_defaults();
    sprite.name = patch->id;
    sprite.origin_x = 0.0F;
    sprite.origin_y = 0.0F;
    sprite.slice9_left = (uint16_t)patch->border;
    sprite.slice9_right = (uint16_t)patch->border;
    sprite.slice9_top = (uint16_t)patch->border;
    sprite.slice9_bottom = (uint16_t)patch->border;
    sprite.shape = NT_ATLAS_SPRITE_SHAPE_RECT;
    sprite.allow_rotate = NT_ATLAS_SPRITE_ROTATE_NO;
    nt_builder_atlas_add_raw(ctx, pixels, (uint32_t)patch->width, (uint32_t)patch->height, &sprite);
    free(pixels);
}

static void add_runtime_node_sprite(NtBuilderContext *ctx, const char *name, const char *path) {
    nt_atlas_sprite_opts_t sprite = nt_atlas_sprite_opts_defaults();
    sprite.name = name;
    sprite.origin_x = 0.5F;
    sprite.origin_y = 0.5F;
    sprite.slice9_left = 1;
    sprite.slice9_right = 1;
    sprite.slice9_top = 1;
    sprite.slice9_bottom = 1;
    sprite.shape = NT_ATLAS_SPRITE_SHAPE_RECT;
    sprite.allow_rotate = NT_ATLAS_SPRITE_ROTATE_NO;
    nt_builder_atlas_add(ctx, path, &sprite);
}

static void add_runtime_slice9_sprite(NtBuilderContext *ctx, const char *name, const char *path, uint16_t left, uint16_t top, uint16_t right, uint16_t bottom) {
    nt_atlas_sprite_opts_t sprite = nt_atlas_sprite_opts_defaults();
    sprite.name = name;
    sprite.origin_x = 0.0F;
    sprite.origin_y = 0.0F;
    sprite.slice9_left = left;
    sprite.slice9_right = right;
    sprite.slice9_top = top;
    sprite.slice9_bottom = bottom;
    sprite.shape = NT_ATLAS_SPRITE_SHAPE_RECT;
    sprite.allow_rotate = NT_ATLAS_SPRITE_ROTATE_NO;
    nt_builder_atlas_add(ctx, path, &sprite);
}

typedef struct RuntimeIcon {
    const char *id;
    const char *path;
} RuntimeIcon;

typedef struct RuntimeStageAsset {
    const char *id;
    const char *path;
} RuntimeStageAsset;

typedef struct RuntimeFxAsset {
    const char *id;
    const char *path;
} RuntimeFxAsset;

static void add_runtime_icon(NtBuilderContext *ctx, const RuntimeIcon *icon) {
    char name[128];
    (void)snprintf(name, sizeof(name), "mine-cards/ui/icon_%s", icon->id);
    add_runtime_node_sprite(ctx, name, icon->path);
}

static void add_runtime_stage_asset(NtBuilderContext *ctx, const RuntimeStageAsset *asset) {
    char name[128];
    (void)snprintf(name, sizeof(name), "mine-cards/ui/stage_%s", asset->id);
    add_runtime_node_sprite(ctx, name, asset->path);
}

static void add_runtime_fx_asset(NtBuilderContext *ctx, const RuntimeFxAsset *asset) {
    char name[128];
    (void)snprintf(name, sizeof(name), "mine-cards/ui/fx_%s", asset->id);
    add_runtime_node_sprite(ctx, name, asset->path);
}

static void write_manifest(void) {
    FILE *f = fopen("gamedesign/projects/mine-cards/visual/runtime_ui/mine_cards_ui_asset_manifest.json", "w");
    if (f == NULL) {
        (void)fprintf(stderr, "failed to write mine_cards_ui_asset_manifest.json\n");
        exit(1);
    }
    (void)fprintf(f,
                  "{\n"
                  "  \"schema\": \"mine_cards.runtime_ui_assets\",\n"
                  "  \"updated\": \"2026-06-17\",\n"
                  "  \"status\": \"procedural_ui_with_generated_stage_nodes_icons_fx_and_compact_blank_ui\",\n"
                  "  \"pack\": \"assets/mine_cards_ui.ntpack\",\n"
                  "  \"atlas_resource\": \"mine-cards/ui_atlas\",\n"
                  "  \"source_note\": \"Temporary procedural shell with generated stage stone/copper target sprites, stage background pieces, reward FX, the first activity/resource/state icon set, generated large blank UI bases, and generated compact chip/card/button/nav slice9 bases.\",\n"
                  "  \"generated_sources\": [\n"
                  "    \"gamedesign/projects/mine-cards/visual/source_sheets/mine_nodes_source_sheet_v001.png\",\n"
                  "    \"gamedesign/projects/mine-cards/visual/runtime_ui/source/mine_node_stone_v001.png\",\n"
                  "    \"gamedesign/projects/mine-cards/visual/runtime_ui/source/mine_node_copper_v001.png\",\n"
                  "    \"gamedesign/projects/mine-cards/art/candidates/mine-cards-icons-v001-candidate-e-alpha.png\",\n"
                  "    \"gamedesign/projects/mine-cards/art/candidates/mine-cards-stage-bg-v001-candidate-a-alpha.png\",\n"
                  "    \"gamedesign/projects/mine-cards/art/candidates/mine-cards-fx-v004-candidate-d-alpha.png\",\n"
                  "    \"gamedesign/projects/mine-cards/art/candidates/mine-cards-blank-ui-kit-v002-candidate-b-clean.png\",\n"
                  "    \"gamedesign/projects/mine-cards/art/candidates/mine-cards-compact-ui-kit-v001-candidate-a-cyan-clean.png\"\n"
                  "  ],\n"
                  "  \"regions\": [\n"
                  "    \"mine-cards/ui/screen_bg\",\n"
                  "    \"mine-cards/ui/top_bar\",\n"
                  "    \"mine-cards/ui/bottom_bar\",\n"
                  "    \"mine-cards/ui/panel_dark\",\n"
                  "    \"mine-cards/ui/panel_stage\",\n"
                  "    \"mine-cards/ui/panel_content\",\n"
                  "    \"mine-cards/ui/board_panel_generated\",\n"
                  "    \"mine-cards/ui/stage_action_card_generated\",\n"
                  "    \"mine-cards/ui/tab_idle\",\n"
                  "    \"mine-cards/ui/tab_active\",\n"
                  "    \"mine-cards/ui/card_node\",\n"
                  "    \"mine-cards/ui/card_selected\",\n"
                  "    \"mine-cards/ui/button_dark\",\n"
                  "    \"mine-cards/ui/button_active\",\n"
                  "    \"mine-cards/ui/nav_idle\",\n"
                  "    \"mine-cards/ui/nav_active\",\n"
                  "    \"mine-cards/ui/progress_track\",\n"
                  "    \"mine-cards/ui/progress_fill\",\n"
                  "    \"mine-cards/ui/rock_stone\",\n"
                  "    \"mine-cards/ui/rock_copper\",\n"
                  "    \"mine-cards/ui/lock_badge\",\n"
                  "    \"mine-cards/ui/callout\",\n"
                  "    \"mine-cards/ui/icon_activity_mining\",\n"
                  "    \"mine-cards/ui/icon_activity_woodcutting\",\n"
                  "    \"mine-cards/ui/icon_activity_fishing\",\n"
                  "    \"mine-cards/ui/icon_activity_smithing\",\n"
                  "    \"mine-cards/ui/icon_activity_combat\",\n"
                  "    \"mine-cards/ui/icon_activity_farming\",\n"
                  "    \"mine-cards/ui/icon_activity_bank\",\n"
                  "    \"mine-cards/ui/icon_activity_shop\",\n"
                  "    \"mine-cards/ui/icon_resource_stone\",\n"
                  "    \"mine-cards/ui/icon_resource_copper_ore\",\n"
                  "    \"mine-cards/ui/icon_resource_coin\",\n"
                  "    \"mine-cards/ui/icon_upgrade_pickaxe\",\n"
                  "    \"mine-cards/ui/icon_state_locked\",\n"
                  "    \"mine-cards/ui/icon_state_equipped\",\n"
                  "    \"mine-cards/ui/icon_state_ready\",\n"
                  "    \"mine-cards/ui/stage_mine_wall_tile\",\n"
                  "    \"mine-cards/ui/stage_mine_floor_shadow\",\n"
                  "    \"mine-cards/ui/stage_stone_debris_cluster\",\n"
                  "    \"mine-cards/ui/stage_copper_ore_seam_stamp\",\n"
                  "    \"mine-cards/ui/stage_warm_lantern_light_overlay\",\n"
                  "    \"mine-cards/ui/fx_stone_hit_chip_fx\",\n"
                  "    \"mine-cards/ui/fx_copper_hit_chip_fx\",\n"
                  "    \"mine-cards/ui/fx_geode_reward_pop_fx\",\n"
                  "    \"mine-cards/ui/fx_xp_spark_fx\",\n"
                  "    \"mine-cards/ui/fx_coin_spark_fx\"\n"
                  "  ]\n"
                  "}\n");
    (void)fclose(f);

    FILE *md = fopen("gamedesign/projects/mine-cards/visual/runtime_ui/mine_cards_ui_asset_manifest.md", "w");
    if (md == NULL) {
        (void)fprintf(stderr, "failed to write mine_cards_ui_asset_manifest.md\n");
        exit(1);
    }
    (void)fprintf(md,
                  "# Mine Cards Runtime UI Asset Manifest\n\n"
                  "Status: procedural shell with generated stage nodes, generated icons/FX, generated large blank UI slice9 bases, and generated compact chip/card/button/nav slice9 bases.\n\n"
                  "Pack: `assets/mine_cards_ui.ntpack`\n\n"
                  "This pack exists to move the native Mining screen off debug-only shape panels and onto the engine atlas/sprite path while the final Mine Cards UI kit is still pending. The visual language is based on the old Mine Cards PSD previews: dark RPG shell, purple selected states, lime active accent, and chunky pixel panels.\n\n"
                  "Generated runtime sprites now included:\n\n"
                  "- `mine-cards/ui/rock_stone` from `gamedesign/projects/mine-cards/visual/runtime_ui/source/mine_node_stone_v001.png`\n"
                  "- `mine-cards/ui/rock_copper` from `gamedesign/projects/mine-cards/visual/runtime_ui/source/mine_node_copper_v001.png`\n\n"
                  "Icon runtime sprites now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-icons-v001-candidate-e-alpha.png` with crop/contact/pixel-audit evidence in `gamedesign/projects/mine-cards/reviews/`.\n\n"
                  "Stage runtime sprites now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-stage-bg-v001-candidate-a-alpha.png` with contact/pixel-audit evidence in `gamedesign/projects/mine-cards/reviews/`.\n\n"
                  "FX runtime sprites now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-fx-v004-candidate-d-alpha.png` with contact/pixel-audit evidence in `gamedesign/projects/mine-cards/reviews/`.\n\n"
                  "Blank UI generated slice9 bases now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-blank-ui-kit-v002-candidate-b-clean.png`; this runtime pass uses only the generated board panel and stage action card where the current layout is large enough for their slice9 margins.\n\n"
                  "Compact UI generated slice9 bases now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-compact-ui-kit-v001-candidate-a-cyan-clean.png`; these replace the compact activity chips, node rows, upgrade button, and bottom nav tabs that were previously procedural atlas patches.\n\n"
                  "Generation provenance: `gamedesign/projects/mine-cards/art/generation_records/mine-nodes-source-sheet-v001.json` and `gamedesign/projects/mine-cards/art/generation_records/mine-cards-icons-v001-candidate-e.json`.\n\n"
                  "Next art step: inspect the native desktop/portrait screenshots and tune compact layout usage before expanding mechanics.\n");
    (void)fclose(md);
}

int main(int argc, char **argv) {
    const char *out_dir = argc >= 2 ? argv[1] : "assets";
    make_dirs(out_dir);

    NtBuilderContext *ctx = nt_builder_start_pack(pack_path(out_dir, "mine_cards_ui.ntpack"));
    if (ctx == NULL) {
        (void)fprintf(stderr, "failed to start mine_cards_ui.ntpack\n");
        return 1;
    }
    nt_builder_set_header_dir(ctx, "build/assets/mine_cards_ui/generated");
    nt_builder_set_cache_dir(ctx, "build/assets/mine_cards_ui/_cache");

    nt_builder_add_shader(ctx, "external/neotolis-engine/assets/shaders/sprite.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "external/neotolis-engine/assets/shaders/sprite.frag", NT_BUILD_SHADER_FRAGMENT);

    nt_atlas_opts_t atlas = nt_atlas_opts_defaults();
    atlas.max_size = 2048;
    atlas.padding = 3;
    atlas.margin = 2;
    atlas.extrude = 1;
    atlas.shape = NT_ATLAS_SHAPE_RECT;
    atlas.allow_transform = false;
    atlas.power_of_two = true;
    atlas.debug_png = false;
    atlas.premultiplied = true;
    atlas.filter_min = NT_TEXTURE_DEFAULT_FILTER_NEAREST;
    atlas.filter_mag = NT_TEXTURE_DEFAULT_FILTER_NEAREST;
    atlas.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    atlas.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    atlas.gen_mipmaps = false;

    nt_builder_begin_atlas(ctx, "mine-cards/ui_atlas", &atlas);
    const UiPatch patches[] = {
        {"mine-cards/ui/screen_bg", 96, 64, 8, {12, 8, 20, 255}, {21, 14, 31, 255}, {42, 30, 58, 255}, {5, 4, 8, 255}},
        {"mine-cards/ui/top_bar", 96, 42, 8, {26, 18, 39, 255}, {45, 31, 58, 255}, {74, 56, 92, 255}, {8, 6, 12, 255}},
        {"mine-cards/ui/bottom_bar", 96, 42, 8, {24, 20, 31, 255}, {42, 35, 51, 255}, {72, 61, 88, 255}, {7, 6, 10, 255}},
        {"mine-cards/ui/panel_dark", 96, 64, 10, {22, 16, 30, 255}, {49, 35, 62, 255}, {97, 78, 122, 255}, {9, 8, 13, 255}},
        {"mine-cards/ui/panel_stage", 96, 64, 10, {25, 18, 35, 255}, {61, 43, 88, 255}, {110, 86, 148, 255}, {8, 7, 12, 255}},
        {"mine-cards/ui/panel_content", 96, 64, 10, {22, 16, 29, 255}, {50, 37, 62, 255}, {92, 72, 112, 255}, {7, 6, 10, 255}},
        {"mine-cards/ui/progress_track", 96, 18, 6, {12, 30, 32, 255}, {25, 45, 50, 255}, {54, 83, 90, 255}, {4, 8, 10, 255}},
        {"mine-cards/ui/progress_fill", 96, 18, 6, {38, 188, 100, 255}, {72, 224, 132, 255}, {130, 246, 174, 255}, {16, 86, 49, 255}},
        {"mine-cards/ui/lock_badge", 32, 24, 5, {52, 42, 39, 255}, {90, 69, 54, 255}, {135, 104, 82, 255}, {25, 20, 19, 255}},
        {"mine-cards/ui/callout", 96, 42, 9, {10, 52, 48, 230}, {26, 98, 84, 235}, {72, 162, 132, 235}, {4, 18, 20, 230}},
    };
    for (int i = 0; i < (int)(sizeof(patches) / sizeof(patches[0])); ++i) {
        add_patch(ctx, &patches[i]);
    }
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/board_panel_generated",
                              "assets/runtime/mine-cards-stage-ui-family-v001/blank_ui/board_panel_slice9.png",
                              56,
                              56,
                              56,
                              56);
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/stage_action_card_generated",
                              "assets/runtime/mine-cards-stage-ui-family-v001/blank_ui/node_card_selected_slice9.png",
                              54,
                              60,
                              54,
                              60);
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/tab_idle",
                              "assets/runtime/mine-cards-stage-ui-family-v001/compact_ui/compact_activity_chip_locked_slice9.png",
                              42,
                              10,
                              42,
                              10);
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/tab_active",
                              "assets/runtime/mine-cards-stage-ui-family-v001/compact_ui/compact_activity_chip_active_slice9.png",
                              42,
                              10,
                              42,
                              10);
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/card_node",
                              "assets/runtime/mine-cards-stage-ui-family-v001/compact_ui/compact_node_row_default_slice9.png",
                              42,
                              12,
                              42,
                              12);
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/card_selected",
                              "assets/runtime/mine-cards-stage-ui-family-v001/compact_ui/compact_node_row_selected_slice9.png",
                              42,
                              12,
                              42,
                              12);
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/button_dark",
                              "assets/runtime/mine-cards-stage-ui-family-v001/compact_ui/compact_primary_button_disabled_slice9.png",
                              42,
                              10,
                              42,
                              10);
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/button_active",
                              "assets/runtime/mine-cards-stage-ui-family-v001/compact_ui/compact_primary_button_affordable_slice9.png",
                              42,
                              10,
                              42,
                              10);
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/nav_idle",
                              "assets/runtime/mine-cards-stage-ui-family-v001/compact_ui/compact_nav_tab_idle_slice9.png",
                              36,
                              14,
                              36,
                              14);
    add_runtime_slice9_sprite(ctx,
                              "mine-cards/ui/nav_active",
                              "assets/runtime/mine-cards-stage-ui-family-v001/compact_ui/compact_nav_tab_active_slice9.png",
                              36,
                              14,
                              36,
                              14);
    add_runtime_node_sprite(ctx, "mine-cards/ui/rock_stone", "gamedesign/projects/mine-cards/visual/runtime_ui/source/mine_node_stone_v001.png");
    add_runtime_node_sprite(ctx, "mine-cards/ui/rock_copper", "gamedesign/projects/mine-cards/visual/runtime_ui/source/mine_node_copper_v001.png");
    const RuntimeStageAsset stage_assets[] = {
        {"mine_wall_tile", "assets/runtime/mine-cards-stage-ui-family-v001/stage/mine_wall_tile.png"},
        {"mine_floor_shadow", "assets/runtime/mine-cards-stage-ui-family-v001/stage/mine_floor_shadow.png"},
        {"stone_debris_cluster", "assets/runtime/mine-cards-stage-ui-family-v001/stage/stone_debris_cluster.png"},
        {"copper_ore_seam_stamp", "assets/runtime/mine-cards-stage-ui-family-v001/stage/copper_ore_seam_stamp.png"},
        {"warm_lantern_light_overlay", "assets/runtime/mine-cards-stage-ui-family-v001/stage/warm_lantern_light_overlay.png"},
    };
    for (int i = 0; i < (int)(sizeof(stage_assets) / sizeof(stage_assets[0])); ++i) {
        add_runtime_stage_asset(ctx, &stage_assets[i]);
    }
    const RuntimeFxAsset fx_assets[] = {
        {"stone_hit_chip_fx", "assets/runtime/mine-cards-stage-ui-family-v001/fx/stone_hit_chip_fx.png"},
        {"copper_hit_chip_fx", "assets/runtime/mine-cards-stage-ui-family-v001/fx/copper_hit_chip_fx.png"},
        {"geode_reward_pop_fx", "assets/runtime/mine-cards-stage-ui-family-v001/fx/geode_reward_pop_fx.png"},
        {"xp_spark_fx", "assets/runtime/mine-cards-stage-ui-family-v001/fx/xp_spark_fx.png"},
        {"coin_spark_fx", "assets/runtime/mine-cards-stage-ui-family-v001/fx/coin_spark_fx.png"},
    };
    for (int i = 0; i < (int)(sizeof(fx_assets) / sizeof(fx_assets[0])); ++i) {
        add_runtime_fx_asset(ctx, &fx_assets[i]);
    }
    const RuntimeIcon icons[] = {
        {"activity_mining", "assets/runtime/mine-cards-stage-ui-family-v001/icons/activity_mining.png"},
        {"activity_woodcutting", "assets/runtime/mine-cards-stage-ui-family-v001/icons/activity_woodcutting.png"},
        {"activity_fishing", "assets/runtime/mine-cards-stage-ui-family-v001/icons/activity_fishing.png"},
        {"activity_smithing", "assets/runtime/mine-cards-stage-ui-family-v001/icons/activity_smithing.png"},
        {"activity_combat", "assets/runtime/mine-cards-stage-ui-family-v001/icons/activity_combat.png"},
        {"activity_farming", "assets/runtime/mine-cards-stage-ui-family-v001/icons/activity_farming.png"},
        {"activity_bank", "assets/runtime/mine-cards-stage-ui-family-v001/icons/activity_bank.png"},
        {"activity_shop", "assets/runtime/mine-cards-stage-ui-family-v001/icons/activity_shop.png"},
        {"resource_stone", "assets/runtime/mine-cards-stage-ui-family-v001/icons/resource_stone.png"},
        {"resource_copper_ore", "assets/runtime/mine-cards-stage-ui-family-v001/icons/resource_copper_ore.png"},
        {"resource_coin", "assets/runtime/mine-cards-stage-ui-family-v001/icons/resource_coin.png"},
        {"upgrade_pickaxe", "assets/runtime/mine-cards-stage-ui-family-v001/icons/upgrade_pickaxe.png"},
        {"state_locked", "assets/runtime/mine-cards-stage-ui-family-v001/icons/state_locked.png"},
        {"state_equipped", "assets/runtime/mine-cards-stage-ui-family-v001/icons/state_equipped.png"},
        {"state_ready", "assets/runtime/mine-cards-stage-ui-family-v001/icons/state_ready.png"},
    };
    for (int i = 0; i < (int)(sizeof(icons) / sizeof(icons[0])); ++i) {
        add_runtime_icon(ctx, &icons[i]);
    }
    nt_builder_end_atlas(ctx);

    const nt_build_result_t result = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (result != NT_BUILD_OK) {
        (void)fprintf(stderr, "mine_cards_ui.ntpack failed: %d\n", (int)result);
        return 1;
    }

    write_manifest();
    (void)printf("Built %s\n", pack_path(out_dir, "mine_cards_ui.ntpack"));
    return 0;
}
