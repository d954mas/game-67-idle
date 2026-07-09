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

// Fashion UI kit slice9 (soft pastel procedural chrome).
#define PANEL_BORDER 14
#define BUTTON_BORDER 20
#define BAR_BORDER 10

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
    nt_builder_set_threads_auto(ctx); // Basis encode is CPU-bound; default is single-threaded

    // text shell
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_font(ctx, "../../external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf",
                        &(nt_font_opts_t){.charset = NT_CHARSET_ASCII, .resource_name = "game/font"});

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

    /* Fashion glass UI v2 (game-local procedural chrome). */
    nt_atlas_sprite_opts_t panel_opts = nt_atlas_sprite_opts_defaults();
    panel_opts.slice9_left = panel_opts.slice9_right = panel_opts.slice9_top = panel_opts.slice9_bottom = PANEL_BORDER;
    panel_opts.name = "panel";
    nt_builder_atlas_add(ctx, "assets/ui/panel.png", &panel_opts);
    panel_opts.name = "panel_beige";
    nt_builder_atlas_add(ctx, "assets/ui/panel_beige.png", &panel_opts);
    panel_opts.name = "panel_blue";
    nt_builder_atlas_add(ctx, "assets/ui/panel_blue.png", &panel_opts);
    panel_opts.name = "panel_brown";
    nt_builder_atlas_add(ctx, "assets/ui/panel_brown.png", &panel_opts);

    nt_atlas_sprite_opts_t button_opts = nt_atlas_sprite_opts_defaults();
    button_opts.slice9_left = button_opts.slice9_right = button_opts.slice9_top = button_opts.slice9_bottom = BUTTON_BORDER;
    button_opts.name = "button";
    nt_builder_atlas_add(ctx, "assets/ui/button.png", &button_opts);
    button_opts.name = "button_primary";
    nt_builder_atlas_add(ctx, "assets/ui/button_primary.png", &button_opts);
    button_opts.name = "button_success";
    nt_builder_atlas_add(ctx, "assets/ui/button_success.png", &button_opts);
    button_opts.name = "button_danger";
    nt_builder_atlas_add(ctx, "assets/ui/button_danger.png", &button_opts);
    button_opts.name = "button_selected";
    nt_builder_atlas_add(ctx, "assets/ui/button_selected.png", &button_opts);

    nt_atlas_sprite_opts_t bar_opts = nt_atlas_sprite_opts_defaults();
    bar_opts.slice9_left = bar_opts.slice9_right = bar_opts.slice9_top = bar_opts.slice9_bottom = BAR_BORDER;
    bar_opts.name = "slider_track";
    nt_builder_atlas_add(ctx, "assets/ui/slider_track.png", &bar_opts);
    bar_opts.name = "slider_fill";
    nt_builder_atlas_add(ctx, "assets/ui/slider_fill.png", &bar_opts);

    nt_atlas_sprite_opts_t thumb_opts = nt_atlas_sprite_opts_defaults();
    thumb_opts.name = "slider_thumb"; /* circle: no slice9 */
    nt_builder_atlas_add(ctx, "assets/ui/slider_thumb.png", &thumb_opts);

    nt_atlas_sprite_opts_t mark_opts = nt_atlas_sprite_opts_defaults();
    mark_opts.name = "checkmark";
    nt_builder_atlas_add(ctx, "assets/ui/checkmark.png", &mark_opts);

    nt_builder_end_atlas(ctx);

    // Item icons: SECOND atlas (ui is first). Explicit region names = the
    // authored "icons/<name>" contract half. debug_png=true emits the page PNG
    // the studio items-viewer previews from. allow_transform=false keeps every
    // region an axis-aligned rect so a preview crop is a simple rectangle.
    // Opts mirror the ui atlas (build_packs.c:104-117) for parity.
    nt_atlas_opts_t icons_opts = nt_atlas_opts_defaults();
    icons_opts.shape = NT_ATLAS_SHAPE_RECT;
    icons_opts.allow_transform = false;   // -> region.transform == 0, simple rect
    icons_opts.pixels_per_unit = 1.0F;    // parity with ui atlas
    icons_opts.padding = 2;
    icons_opts.margin = 2;
    icons_opts.extrude = 1;               // outline lands in the extrude gutter
    icons_opts.premultiplied = true;      // affects ONLY the packed texture (ui parity);
                                          // debug-PNG is copied BEFORE premultiply -> straight alpha
    icons_opts.compress = NULL;           // parity with ui atlas (raw RGBA page)
    icons_opts.debug_png = true;          // -> <CMAKE_BINARY_DIR>/pack/icons_page0.png
    icons_opts.filter_min = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    icons_opts.filter_mag = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    icons_opts.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    icons_opts.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    icons_opts.gen_mipmaps = false;
    nt_builder_begin_atlas(ctx, "icons", &icons_opts);
    static const char *icon_names[] = {"gold","xp","energy","potion","sword","wood"};
    for (int i = 0; i < 6; ++i) {
        nt_atlas_sprite_opts_t o = nt_atlas_sprite_opts_defaults();
        o.name = icon_names[i];
        char path[128];
        (void)snprintf(path, sizeof path, "assets/icons/%s.png", icon_names[i]);
        nt_builder_atlas_add(ctx, path, &o);
    }
    nt_builder_end_atlas(ctx);

    /* Dress-up character layers (fashion playable): third atlas. */
    nt_atlas_opts_t dress_opts = nt_atlas_opts_defaults();
    dress_opts.shape = NT_ATLAS_SHAPE_RECT;
    dress_opts.allow_transform = false;
    dress_opts.pixels_per_unit = 1.0F;
    dress_opts.padding = 2;
    dress_opts.margin = 2;
    dress_opts.extrude = 1;
    dress_opts.premultiplied = true;
    dress_opts.compress = NULL;
    dress_opts.filter_min = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    dress_opts.filter_mag = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    dress_opts.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    dress_opts.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    dress_opts.gen_mipmaps = false;
    nt_builder_begin_atlas(ctx, "dress", &dress_opts);
    static const char *dress_sprites[] = {
        "body_base", "stage_bg",
        "hair_bob", "hair_long", "hair_pink", "hair_gold",
        "top_tee", "top_hoodie", "top_blazer", "top_crop",
        "bot_jeans", "bot_skirt", "bot_shorts", "bot_cargo",
        "shoe_sneak", "shoe_boot", "shoe_heel", "shoe_sandal",
        "acc_glasses", "acc_hat", "acc_bag", "acc_scarf",
        "look_street", "look_elegant", "look_glam",
        /* catalog thumbs = full cutouts */
        "hair_bob_full", "hair_long_full", "hair_pink_full", "hair_gold_full",
        "top_tee_full", "top_hoodie_full", "top_blazer_full", "top_crop_full",
        "bot_jeans_full", "bot_skirt_full", "bot_shorts_full", "bot_cargo_full",
        "shoe_sneak_full", "shoe_boot_full", "shoe_heel_full", "shoe_sandal_full",
        "acc_glasses_full", "acc_hat_full", "acc_bag_full", "acc_scarf_full",
        "look_street_full", "look_elegant_full", "look_glam_full",
    };
    for (int i = 0; i < (int)(sizeof dress_sprites / sizeof dress_sprites[0]); ++i) {
        nt_atlas_sprite_opts_t o = nt_atlas_sprite_opts_defaults();
        o.name = dress_sprites[i];
        char path[160];
        (void)snprintf(path, sizeof path, "assets/dress/%s.png", dress_sprites[i]);
        nt_builder_atlas_add(ctx, path, &o);
    }
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
