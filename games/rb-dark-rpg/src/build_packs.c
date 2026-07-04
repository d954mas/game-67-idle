// Template pack builder: packs the starter-shell assets into game.ntpack + a
// generated asset-id header. Runs natively at build time (CMake custom command).
// A game extends this with its own meshes/textures; the shell below is what every
// game starts with: a font (text), the text shader, the instanced-mesh shader, and
// a 1x1 white texture (neutral u_texture for per-instance-coloured meshes).
#define NT_BUILD_MAX_ASSETS 4096
#include "nt_builder.h"

#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

#define HEADER_DIR "src/generated"

// Kenney CC0 slice9 corners (px): panel 100x100/10px, button 384x128/16px, bars 8px.
#define PANEL_BORDER 10
#define BUTTON_BORDER 16
#define BAR_BORDER 8
#define TOP_HUD_BAR_BORDER_X 42
#define TOP_HUD_BAR_BORDER_Y 20
#define TOP_HUD_PLAQUE_BORDER_X 46
#define TOP_HUD_PLAQUE_BORDER_Y 28
#define TOP_HUD_RESOURCE_BORDER_L 86
#define TOP_HUD_RESOURCE_BORDER_R 34
#define TOP_HUD_RESOURCE_BORDER_Y 32

// Russian UI text must be packed into the engine font; shape/pixel text is only
// a debug fallback. Basic Cyrillic + Yo covers the first-screen tutorial copy.
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

static char s_path_buf[512];
static const char *pack_path(const char *dir, const char *name) {
    (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
    return s_path_buf;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        (void)fprintf(stderr, "Usage: build_game_packs <pack_dir>\n");
        return 1;
    }
    const char *out_dir = argv[1];
    (void)MKDIR(out_dir);
    (void)MKDIR("src");
    (void)MKDIR(HEADER_DIR);

    NtBuilderContext *ctx = nt_builder_start_pack(pack_path(out_dir, "game.ntpack"));
    if (!ctx) {
        (void)fprintf(stderr, "Failed to start game pack\n");
        return 1;
    }
    nt_builder_set_header_dir(ctx, HEADER_DIR);
    (void)MKDIR("build");
    nt_builder_set_cache_dir(ctx, "build/_cache");

    // text shell
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);
    char font_charset[512];
    (void)snprintf(font_charset, sizeof(font_charset), "%s%s", NT_CHARSET_ASCII, CYRILLIC_CHARSET);
    nt_builder_add_font(ctx, "../../external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf",
                        &(nt_font_opts_t){.charset = font_charset, .resource_name = "game/font"});

    // instanced-mesh shell: TWO mesh paths a game learns from. The COLOURED path
    // (mesh_inst = position + per-instance world matrix + colour, no texture) and the
    // TEXTURED path (mesh_tex = position + uv0, samples u_texture). The starter cube is
    // packed with BOTH streams (it has UVs) so one mesh feeds both materials.
    nt_builder_add_shader(ctx, "assets/shaders/mesh_inst.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/mesh_inst.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_shader(ctx, "assets/shaders/mesh_tex.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/mesh_tex.frag", NT_BUILD_SHADER_FRAGMENT);
    NtStreamLayout mesh_layout[] = {
        {"position", "POSITION", NT_STREAM_FLOAT32, 3, false},
        {"uv0", "TEXCOORD_0", NT_STREAM_FLOAT32, 2, false},
    };
    nt_builder_add_mesh(ctx, "assets/meshes/cube.glb",
                        &(nt_mesh_opts_t){.layout = mesh_layout, .stream_count = 2, .tangent_mode = NT_TANGENT_NONE});

    // UV-grid texture for the textured mesh path. Source-first search did not find
    // a better CC0 surface texture for the starter template, so the template ships
    // a generated checker test-grid - the canonical textured-mesh teaching asset.
    // Swap in a sourced CC0 texture (nt_builder_add_texture) when one is available.
    enum { UV_TEX = 256, UV_CELL = 32 };
    static uint8_t uv_pixels[UV_TEX * UV_TEX * 4];
    for (int y = 0; y < UV_TEX; ++y) {
        for (int x = 0; x < UV_TEX; ++x) {
            uint8_t *px = &uv_pixels[(y * UV_TEX + x) * 4];
            const bool checker = (((x / UV_CELL) + (y / UV_CELL)) & 1) != 0;
            const bool grid = (x % UV_CELL) < 2 || (y % UV_CELL) < 2;
            uint8_t r = (uint8_t)(40 + (x * 180) / UV_TEX);  // gradient so orientation reads
            uint8_t g = (uint8_t)(40 + (y * 180) / UV_TEX);
            uint8_t b = checker ? 200 : 90;
            if (grid) { r = g = b = 20; } // dark grid lines
            px[0] = r;
            px[1] = g;
            px[2] = b;
            px[3] = 255;
        }
    }
    nt_tex_opts_t uv_opts = nt_tex_opts_defaults();
    nt_builder_add_texture_raw(ctx, uv_pixels, UV_TEX, UV_TEX, "assets/textures/uv_grid", &uv_opts);

    // UI shell: the sprite shader (UI rects/images) + a slice9 GUI atlas. The atlas
    // holds a 1x1 white pixel (UI rect fill / nt_ui white region) and the Kenney CC0
    // panel/button/slider art the settings panel renders with. nt_ui draws from this.
    nt_builder_add_shader(ctx, "assets/shaders/sprite.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/sprite.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_shader(ctx, "assets/shaders/sprite_mask_glow.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_shader(ctx, "assets/shaders/sprite_ui_fade.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/sprite_ui_fade.frag", NT_BUILD_SHADER_FRAGMENT);

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
    nt_builder_begin_atlas(ctx, "ui", &atlas_opts);

    // 1x1 white pixel: UI rect fill + nt_ui's required white region.
    static const uint8_t white_pixel[4] = {255, 255, 255, 255};
    nt_atlas_sprite_opts_t white_opts = nt_atlas_sprite_opts_defaults();
    white_opts.name = "_white";
    nt_builder_atlas_add_raw(ctx, white_pixel, 1, 1, &white_opts);

    nt_atlas_sprite_opts_t tutorial_finger_opts = nt_atlas_sprite_opts_defaults();
    tutorial_finger_opts.name = "tutorial_finger";
    tutorial_finger_opts.origin_x = 0.5F;
    tutorial_finger_opts.origin_y = 0.0F;
    tutorial_finger_opts.shape = NT_ATLAS_SPRITE_SHAPE_RECT;
    tutorial_finger_opts.allow_rotate = NT_ATLAS_SPRITE_ROTATE_NO;
    nt_builder_atlas_add(ctx, "assets/ui/tutorial_finger_01.png", &tutorial_finger_opts);

    nt_atlas_sprite_opts_t gate_guard_portrait_opts = nt_atlas_sprite_opts_defaults();
    gate_guard_portrait_opts.name = "gate_guard_portrait";
    gate_guard_portrait_opts.origin_x = 0.5F;
    gate_guard_portrait_opts.origin_y = 0.5F;
    gate_guard_portrait_opts.shape = NT_ATLAS_SPRITE_SHAPE_RECT;
    gate_guard_portrait_opts.allow_rotate = NT_ATLAS_SPRITE_ROTATE_NO;
    nt_builder_atlas_add(ctx, "assets/ui/gate_guard_portrait.png", &gate_guard_portrait_opts);

    nt_atlas_sprite_opts_t nav_opts = nt_atlas_sprite_opts_defaults();
    nav_opts.origin_x = 0.5F;
    nav_opts.origin_y = 0.5F;
    nav_opts.shape = NT_ATLAS_SPRITE_SHAPE_RECT;
    nav_opts.allow_rotate = NT_ATLAS_SPRITE_ROTATE_NO;
    nav_opts.name = "nav_v11_equipment";
    nt_builder_atlas_add(ctx, "assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_equipment.png", &nav_opts);
    nav_opts.name = "nav_v11_journal";
    nt_builder_atlas_add(ctx, "assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_journal.png", &nav_opts);
    nav_opts.name = "nav_v11_map";
    nt_builder_atlas_add(ctx, "assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_map.png", &nav_opts);
    nav_opts.name = "nav_v11_place";
    nt_builder_atlas_add(ctx, "assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_place.png", &nav_opts);
    nav_opts.name = "nav_v11_more";
    nt_builder_atlas_add(ctx, "assets/ui/generated/garrison_nav_tokens_11/slices/nav_v11_more.png", &nav_opts);

    nt_atlas_sprite_opts_t top_hud_opts = nt_atlas_sprite_opts_defaults();
    top_hud_opts.origin_x = 0.5F;
    top_hud_opts.origin_y = 0.5F;
    top_hud_opts.shape = NT_ATLAS_SPRITE_SHAPE_RECT;
    top_hud_opts.allow_rotate = NT_ATLAS_SPRITE_ROTATE_NO;

    top_hud_opts.name = "top_hud_portrait_frame";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_portrait_frame.png", &top_hud_opts);

    top_hud_opts.slice9_left = top_hud_opts.slice9_right = TOP_HUD_PLAQUE_BORDER_X;
    top_hud_opts.slice9_top = top_hud_opts.slice9_bottom = TOP_HUD_PLAQUE_BORDER_Y;
    top_hud_opts.name = "top_hud_status_plaque";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_status_plaque.png", &top_hud_opts);
    top_hud_opts.name = "top_hud_location_plaque";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_location_plaque.png", &top_hud_opts);

    top_hud_opts.slice9_left = top_hud_opts.slice9_right = TOP_HUD_BAR_BORDER_X;
    top_hud_opts.slice9_top = top_hud_opts.slice9_bottom = TOP_HUD_BAR_BORDER_Y;
    top_hud_opts.name = "top_hud_hp_frame";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_hp_frame.png", &top_hud_opts);
    top_hud_opts.name = "top_hud_xp_frame";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_xp_frame.png", &top_hud_opts);

    top_hud_opts.slice9_left = TOP_HUD_RESOURCE_BORDER_L;
    top_hud_opts.slice9_right = TOP_HUD_RESOURCE_BORDER_R;
    top_hud_opts.slice9_top = top_hud_opts.slice9_bottom = TOP_HUD_RESOURCE_BORDER_Y;
    top_hud_opts.name = "top_hud_resource_coin_chip";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_resource_coin_chip.png", &top_hud_opts);
    top_hud_opts.name = "top_hud_resource_supplies_chip";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_resource_supplies_chip.png", &top_hud_opts);

    top_hud_opts.slice9_left = top_hud_opts.slice9_right = top_hud_opts.slice9_top = top_hud_opts.slice9_bottom = 0;
    top_hud_opts.name = "top_hud_settings_button";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_settings_button.png", &top_hud_opts);
    top_hud_opts.name = "top_hud_level_badge";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_level_badge.png", &top_hud_opts);
    top_hud_opts.name = "top_hud_icon_coin";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_icon_coin.png", &top_hud_opts);
    top_hud_opts.name = "top_hud_icon_supplies";
    nt_builder_atlas_add(ctx, "assets/ui/generated/top_hud_tokens_02/slices/top_hud_icon_supplies.png", &top_hud_opts);

    nt_atlas_sprite_opts_t panel_opts = nt_atlas_sprite_opts_defaults();
    panel_opts.name = "panel";
    panel_opts.slice9_left = panel_opts.slice9_right = panel_opts.slice9_top = panel_opts.slice9_bottom = PANEL_BORDER;
    nt_builder_atlas_add(ctx, "assets/ui/panel.png", &panel_opts);

    nt_atlas_sprite_opts_t button_opts = nt_atlas_sprite_opts_defaults();
    button_opts.name = "button";
    button_opts.slice9_left = button_opts.slice9_right = button_opts.slice9_top = button_opts.slice9_bottom = BUTTON_BORDER;
    nt_builder_atlas_add(ctx, "assets/ui/button.png", &button_opts);

    nt_atlas_sprite_opts_t bar_opts = nt_atlas_sprite_opts_defaults();
    bar_opts.slice9_left = bar_opts.slice9_right = bar_opts.slice9_top = bar_opts.slice9_bottom = BAR_BORDER;
    bar_opts.name = "slider_track";
    nt_builder_atlas_add(ctx, "assets/ui/slider_track.png", &bar_opts);
    bar_opts.name = "slider_fill";
    nt_builder_atlas_add(ctx, "assets/ui/slider_fill.png", &bar_opts);

    nt_atlas_sprite_opts_t thumb_opts = nt_atlas_sprite_opts_defaults();
    thumb_opts.name = "slider_thumb"; // circle: no slice9
    nt_builder_atlas_add(ctx, "assets/ui/slider_thumb.png", &thumb_opts);

    nt_builder_end_atlas(ctx);

    // Hub scene art is a separate atlas so large location backgrounds do not
    // inflate or couple to the reusable UI atlas.
    nt_atlas_opts_t scene_atlas_opts = nt_atlas_opts_defaults();
    scene_atlas_opts.shape = NT_ATLAS_SHAPE_RECT;
    scene_atlas_opts.allow_transform = false;
    scene_atlas_opts.pixels_per_unit = 1.0F;
    scene_atlas_opts.padding = 0;
    scene_atlas_opts.margin = 0;
    scene_atlas_opts.extrude = 0;
    scene_atlas_opts.premultiplied = true;
    scene_atlas_opts.compress = NULL;
    scene_atlas_opts.filter_min = NT_TEXTURE_DEFAULT_FILTER_LINEAR_MIPMAP_LINEAR;
    scene_atlas_opts.filter_mag = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    scene_atlas_opts.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    scene_atlas_opts.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    scene_atlas_opts.gen_mipmaps = true;
    nt_builder_begin_atlas(ctx, "hub_scene", &scene_atlas_opts);

    nt_atlas_sprite_opts_t bg_opts = nt_atlas_sprite_opts_defaults();
    bg_opts.name = "last_post_background";
    bg_opts.origin_x = 0.0F;
    bg_opts.origin_y = 0.0F;
    bg_opts.shape = NT_ATLAS_SPRITE_SHAPE_RECT;
    bg_opts.allow_rotate = NT_ATLAS_SPRITE_ROTATE_NO;
    nt_builder_atlas_add(ctx, "assets/scenes/last_post_background_candidate05_1280x700.png", &bg_opts);

    nt_atlas_sprite_opts_t guard_opts = nt_atlas_sprite_opts_defaults();
    guard_opts.name = "last_post_guard";
    guard_opts.origin_x = 0.5F;
    guard_opts.origin_y = 0.0F;
    guard_opts.shape = NT_ATLAS_SPRITE_SHAPE_CONCAVE;
    guard_opts.allow_rotate = NT_ATLAS_SPRITE_ROTATE_NO;
    guard_opts.max_vertices = 12;
    nt_builder_atlas_add(ctx, "assets/characters/last_post_guard_05.png", &guard_opts);

    nt_builder_end_atlas(ctx);

    nt_build_result_t r = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (r != NT_BUILD_OK) {
        (void)fprintf(stderr, "game pack failed: %d\n", r);
        return 1;
    }
    const char *headers[] = {HEADER_DIR "/game.h"};
    nt_builder_merge_headers(headers, 1, HEADER_DIR "/game_assets.h");
    (void)printf("Built game.ntpack\n");
    return 0;
}
