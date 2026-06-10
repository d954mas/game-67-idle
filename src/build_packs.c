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
#define BG_STARTER_ROOM_YARD "gamedesing/assets/generated/backgrounds/bg_starter_room_yard.png"
#define HERO_1_67_BODY "gamedesing/assets/generated/characters/hero_1_67_body.png"
#define BADGE_POWER_1_67 "gamedesing/assets/generated/ui/badge_power_1_67.png"
#define BUTTON_67_GESTURE "gamedesing/assets/generated/ui/button_67_gesture.png"
#define CARD_JOB_KIOSK "gamedesing/assets/generated/ui/card_job_kiosk.png"
#define CARD_UPGRADE_TAP "gamedesing/assets/generated/ui/card_upgrade_tap.png"
#define ICON_MEME_COIN_67 "gamedesing/assets/generated/ui/icon_meme_coin_67.png"
#define ICON_NEXT_GOAL_ARROW_67 "gamedesing/assets/generated/ui/icon_next_goal_arrow_67.png"
#define ICON_TAP_HAND_67 "gamedesing/assets/generated/ui/icon_tap_hand_67.png"
#define PILL_COIN_PLUS "gamedesing/assets/generated/ui/pill_coin_plus.png"
#define PILL_TAP_PLUS_1 "gamedesing/assets/generated/ui/pill_tap_plus_1.png"
#define TAB_CITY "gamedesing/assets/generated/ui/tab_city.png"
#define TAB_HOME "gamedesing/assets/generated/ui/tab_home.png"
#define UI_FIRST_ACTION_PLATE_9S "gamedesing/assets/generated/ui/ui_first_action_plate_9s.png"
#define UI_FIRST_STATUS_BADGE_SHELL_9S "gamedesing/assets/generated/ui/ui_first_status_badge_shell_9s.png"
#define UI_RUNTIME_GOAL_CARD_9S "gamedesing/assets/generated/ui/ui_runtime_goal_card_9s.png"
#define UI_RUNTIME_PROGRESS_BAR_9S "gamedesing/assets/generated/ui/ui_runtime_progress_bar_9s.png"
#define UI_RUNTIME_RESOURCE_PILL_9S "gamedesing/assets/generated/ui/ui_runtime_resource_pill_9s.png"
#define UI_RUNTIME_TAB_LOCKED_9S "gamedesing/assets/generated/ui/ui_runtime_tab_locked_9s.png"
#define UI_RUNTIME_TAB_SELECTED_9S "gamedesing/assets/generated/ui/ui_runtime_tab_selected_9s.png"

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
    (void)nt_builder_add_asset_root(ctx, "external/neotolis-engine");

    nt_builder_add_shader(ctx, "assets/shaders/sprite.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/sprite.frag", NT_BUILD_SHADER_FRAGMENT);

    nt_tex_opts_t tex_opts = nt_tex_opts_defaults();
    tex_opts.premultiplied = true;
    tex_opts.filter_min = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    tex_opts.filter_mag = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    tex_opts.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    tex_opts.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    tex_opts.gen_mipmaps = false;

    nt_builder_add_texture(ctx, BG_STARTER_ROOM_YARD, &tex_opts);
    nt_builder_rename(ctx, BG_STARTER_ROOM_YARD, "game_67_idle/textures/bg_starter_room_yard");
    nt_builder_add_texture(ctx, HERO_1_67_BODY, &tex_opts);
    nt_builder_rename(ctx, HERO_1_67_BODY, "game_67_idle/textures/hero_1_67_body");
    nt_builder_add_texture(ctx, BADGE_POWER_1_67, &tex_opts);
    nt_builder_rename(ctx, BADGE_POWER_1_67, "game_67_idle/textures/badge_power_1_67");
    nt_builder_add_texture(ctx, BUTTON_67_GESTURE, &tex_opts);
    nt_builder_rename(ctx, BUTTON_67_GESTURE, "game_67_idle/textures/button_67_gesture");
    nt_builder_add_texture(ctx, CARD_JOB_KIOSK, &tex_opts);
    nt_builder_rename(ctx, CARD_JOB_KIOSK, "game_67_idle/textures/card_job_kiosk");
    nt_builder_add_texture(ctx, CARD_UPGRADE_TAP, &tex_opts);
    nt_builder_rename(ctx, CARD_UPGRADE_TAP, "game_67_idle/textures/card_upgrade_tap");
    nt_builder_add_texture(ctx, ICON_MEME_COIN_67, &tex_opts);
    nt_builder_rename(ctx, ICON_MEME_COIN_67, "game_67_idle/textures/icon_meme_coin_67");
    nt_builder_add_texture(ctx, ICON_NEXT_GOAL_ARROW_67, &tex_opts);
    nt_builder_rename(ctx, ICON_NEXT_GOAL_ARROW_67, "game_67_idle/textures/icon_next_goal_arrow_67");
    nt_builder_add_texture(ctx, ICON_TAP_HAND_67, &tex_opts);
    nt_builder_rename(ctx, ICON_TAP_HAND_67, "game_67_idle/textures/icon_tap_hand_67");
    nt_builder_add_texture(ctx, PILL_COIN_PLUS, &tex_opts);
    nt_builder_rename(ctx, PILL_COIN_PLUS, "game_67_idle/textures/pill_coin_plus");
    nt_builder_add_texture(ctx, PILL_TAP_PLUS_1, &tex_opts);
    nt_builder_rename(ctx, PILL_TAP_PLUS_1, "game_67_idle/textures/pill_tap_plus_1");
    nt_builder_add_texture(ctx, TAB_CITY, &tex_opts);
    nt_builder_rename(ctx, TAB_CITY, "game_67_idle/textures/tab_city");
    nt_builder_add_texture(ctx, TAB_HOME, &tex_opts);
    nt_builder_rename(ctx, TAB_HOME, "game_67_idle/textures/tab_home");
    nt_builder_add_texture(ctx, UI_FIRST_ACTION_PLATE_9S, &tex_opts);
    nt_builder_rename(ctx, UI_FIRST_ACTION_PLATE_9S, "game_67_idle/textures/ui_first_action_plate_9s");
    nt_builder_add_texture(ctx, UI_FIRST_STATUS_BADGE_SHELL_9S, &tex_opts);
    nt_builder_rename(ctx, UI_FIRST_STATUS_BADGE_SHELL_9S, "game_67_idle/textures/ui_first_status_badge_shell_9s");
    nt_builder_add_texture(ctx, UI_RUNTIME_GOAL_CARD_9S, &tex_opts);
    nt_builder_rename(ctx, UI_RUNTIME_GOAL_CARD_9S, "game_67_idle/textures/ui_runtime_goal_card_9s");
    nt_builder_add_texture(ctx, UI_RUNTIME_PROGRESS_BAR_9S, &tex_opts);
    nt_builder_rename(ctx, UI_RUNTIME_PROGRESS_BAR_9S, "game_67_idle/textures/ui_runtime_progress_bar_9s");
    nt_builder_add_texture(ctx, UI_RUNTIME_RESOURCE_PILL_9S, &tex_opts);
    nt_builder_rename(ctx, UI_RUNTIME_RESOURCE_PILL_9S, "game_67_idle/textures/ui_runtime_resource_pill_9s");
    nt_builder_add_texture(ctx, UI_RUNTIME_TAB_LOCKED_9S, &tex_opts);
    nt_builder_rename(ctx, UI_RUNTIME_TAB_LOCKED_9S, "game_67_idle/textures/ui_runtime_tab_locked_9s");
    nt_builder_add_texture(ctx, UI_RUNTIME_TAB_SELECTED_9S, &tex_opts);
    nt_builder_rename(ctx, UI_RUNTIME_TAB_SELECTED_9S, "game_67_idle/textures/ui_runtime_tab_selected_9s");

    nt_atlas_opts_t atlas_opts = nt_atlas_opts_defaults();
    atlas_opts.shape = NT_ATLAS_SHAPE_RECT;
    atlas_opts.allow_transform = false;
    atlas_opts.padding = 2;
    atlas_opts.margin = 2;
    atlas_opts.extrude = 1;
    atlas_opts.premultiplied = true;
    atlas_opts.filter_min = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    atlas_opts.filter_mag = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    atlas_opts.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    atlas_opts.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    atlas_opts.gen_mipmaps = false;

    nt_builder_begin_atlas(ctx, "game_67_idle/runtime_atlas", &atlas_opts);
    nt_atlas_sprite_opts_t sprite_opts = nt_atlas_sprite_opts_defaults();
    sprite_opts.name = "bg_starter_room_yard";
    nt_builder_atlas_add(ctx, BG_STARTER_ROOM_YARD, &sprite_opts);
    sprite_opts.name = "hero_1_67_body";
    nt_builder_atlas_add(ctx, HERO_1_67_BODY, &sprite_opts);
    sprite_opts.name = "badge_power_1_67";
    nt_builder_atlas_add(ctx, BADGE_POWER_1_67, &sprite_opts);
    sprite_opts.name = "button_67_gesture";
    nt_builder_atlas_add(ctx, BUTTON_67_GESTURE, &sprite_opts);
    sprite_opts.name = "card_job_kiosk";
    nt_builder_atlas_add(ctx, CARD_JOB_KIOSK, &sprite_opts);
    sprite_opts.name = "card_upgrade_tap";
    nt_builder_atlas_add(ctx, CARD_UPGRADE_TAP, &sprite_opts);
    sprite_opts.name = "icon_meme_coin_67";
    nt_builder_atlas_add(ctx, ICON_MEME_COIN_67, &sprite_opts);
    sprite_opts.name = "icon_next_goal_arrow_67";
    nt_builder_atlas_add(ctx, ICON_NEXT_GOAL_ARROW_67, &sprite_opts);
    sprite_opts.name = "icon_tap_hand_67";
    nt_builder_atlas_add(ctx, ICON_TAP_HAND_67, &sprite_opts);
    sprite_opts.name = "pill_coin_plus";
    nt_builder_atlas_add(ctx, PILL_COIN_PLUS, &sprite_opts);
    sprite_opts.name = "pill_tap_plus_1";
    nt_builder_atlas_add(ctx, PILL_TAP_PLUS_1, &sprite_opts);
    sprite_opts.name = "tab_city";
    nt_builder_atlas_add(ctx, TAB_CITY, &sprite_opts);
    sprite_opts.name = "tab_home";
    nt_builder_atlas_add(ctx, TAB_HOME, &sprite_opts);
    sprite_opts.name = "ui_first_action_plate_9s";
    nt_builder_atlas_add(ctx, UI_FIRST_ACTION_PLATE_9S, &sprite_opts);
    sprite_opts.name = "ui_first_status_badge_shell_9s";
    nt_builder_atlas_add(ctx, UI_FIRST_STATUS_BADGE_SHELL_9S, &sprite_opts);
    sprite_opts.name = "ui_runtime_goal_card_9s";
    nt_builder_atlas_add(ctx, UI_RUNTIME_GOAL_CARD_9S, &sprite_opts);
    sprite_opts.name = "ui_runtime_progress_bar_9s";
    nt_builder_atlas_add(ctx, UI_RUNTIME_PROGRESS_BAR_9S, &sprite_opts);
    sprite_opts.name = "ui_runtime_resource_pill_9s";
    nt_builder_atlas_add(ctx, UI_RUNTIME_RESOURCE_PILL_9S, &sprite_opts);
    sprite_opts.name = "ui_runtime_tab_locked_9s";
    nt_builder_atlas_add(ctx, UI_RUNTIME_TAB_LOCKED_9S, &sprite_opts);
    sprite_opts.name = "ui_runtime_tab_selected_9s";
    nt_builder_atlas_add(ctx, UI_RUNTIME_TAB_SELECTED_9S, &sprite_opts);
    nt_builder_end_atlas(ctx);

    static const char k_manifest[] =
        "{\n"
        "  \"game\": \"game_67_idle\",\n"
        "  \"scene\": \"main_screen\",\n"
        "  \"purpose\": \"first playable runtime art slice\"\n"
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
