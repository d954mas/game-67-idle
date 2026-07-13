// Template pack builder: packs the starter-shell assets into game.ntpack + a
// generated asset-id header. Runs natively at build time (CMake custom command).
// A game extends this with its own meshes/textures; the shell below is what every
// game starts with: a font (text), the text shader, the instanced-mesh shader, and
// a 1x1 white texture (neutral u_texture for per-instance-coloured meshes).
#define NT_BUILD_MAX_ASSETS 4096
#include "nt_builder.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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

static char s_path_buf[512];
static const char *pack_path(const char *dir, const char *name) {
    (void)snprintf(s_path_buf, sizeof(s_path_buf), "%s/%s", dir, name);
    return s_path_buf;
}

static bool add_blob_file(NtBuilderContext *ctx, const char *path, const char *resource_id) {
    FILE *file = NULL;
#ifdef _WIN32
    (void)fopen_s(&file, path, "rb");
#else
    file = fopen(path, "rb");
#endif
    if (file == NULL) {
        (void)fprintf(stderr, "Failed to open blob source: %s\n", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        (void)fprintf(stderr, "Failed to seek blob source: %s\n", path);
        (void)fclose(file);
        return false;
    }
    const long end = ftell(file);
    if (end <= 0 || (uint64_t)end > UINT32_MAX || fseek(file, 0, SEEK_SET) != 0) {
        (void)fprintf(stderr, "Invalid blob source size: %s\n", path);
        (void)fclose(file);
        return false;
    }

    const uint32_t size = (uint32_t)end;
    uint8_t *bytes = (uint8_t *)malloc(size);
    if (bytes == NULL || fread(bytes, 1, size, file) != size) {
        (void)fprintf(stderr, "Failed to read blob source: %s\n", path);
        free(bytes);
        (void)fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        (void)fprintf(stderr, "Failed to close blob source: %s\n", path);
        free(bytes);
        return false;
    }

    nt_builder_add_blob(ctx, bytes, size, resource_id);
    free(bytes);
    return true;
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

    // Audio stays an opaque game-owned blob. The runtime chooses the decoder;
    // the resource ID intentionally contains no codec or file extension.
    if (!add_blob_file(ctx, "assets/audio/sfx/ui_click.wav", "audio/sfx/ui_click") ||
        !add_blob_file(ctx, "assets/audio/music/demo_jingle.mp3", "audio/music/demo_jingle")) {
        nt_builder_free_pack(ctx);
        return 1;
    }

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
