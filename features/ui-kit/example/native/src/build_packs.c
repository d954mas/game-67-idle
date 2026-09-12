/* UI Lab pack builder: ui_lab.ntpack = sprite + text shaders, one UI atlas
 * carrying the kit's slice9 art for every theme, the demo icons and the scene
 * backdrop, and the font.
 * Usage: build_ui_lab_packs <pack_dir> <header_dir> <lab_dir>
 * Run from the engine root so the shader resource ids match other consumers. */
#include "nt_builder.h"

#include "features/ui_kit/ui_icons.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

/* The kit ships at this multiple of its design units (tokens `art.export_scale`);
 * slice9 borders are source pixels, so each design border is scaled up here. */
#define UI_KIT_EXPORT_SCALE 4
#define PANEL_BORDER (14 * UI_KIT_EXPORT_SCALE)
#define BUTTON_BORDER_X (16 * UI_KIT_EXPORT_SCALE)
#define BUTTON_BORDER_TOP (16 * UI_KIT_EXPORT_SCALE)
#define BUTTON_BORDER_BOTTOM (22 * UI_KIT_EXPORT_SCALE)
#define TILE_BORDER (14 * UI_KIT_EXPORT_SCALE)
#define HEADER_BORDER (12 * UI_KIT_EXPORT_SCALE)
#define HEADER_BORDER_BOTTOM (2 * UI_KIT_EXPORT_SCALE)
#define BAR_BORDER (11 * UI_KIT_EXPORT_SCALE)
#define BAR_BORDER_SM 11 /* design-size copies the engine slider draws 1:1 */

#define FONT_PATH "assets/fonts/LilitaOne-RussianChineseKo.ttf"
/* Both cases of the whole Russian alphabet. `ё` is packed even though the lab
 * writes `е`: a player name from a portal may carry it. */
#define CHARSET_CYRILLIC "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя"
#define CHARSET_SYMBOLS "→×•–"

/* One folder of generated art per theme; the runtime addresses regions as
 * "ui/<theme>/<region>" by hash, so a new theme needs no new symbol. */
static const char *const THEMES[] = {"b", "forest", "ember", "night"};
static const char *const ICONS[] = {"coin", "xp", "energy", "sword", "potion"};
/* The template's CC0 Kenney files, read in place: the studio tracks one copy. */
static const char *const ICON_FILES[] = {"gold", "xp", "energy", "sword", "potion"};
#define STUDIO_FROM_LAB "../../../.."

static char s_path[1024];
static const char *path_join(const char *dir, const char *rel) {
    (void)snprintf(s_path, sizeof s_path, "%s/%s", dir, rel);
    return s_path;
}

/* The builder copies the region name at add time, so one buffer per call is enough. */
static void add_slice9(NtAtlasBuild *atlas, const char *lab_dir, const char *theme, const char *region, int l, int r, int t, int b) {
    char name[128];
    char rel[256];
    (void)snprintf(name, sizeof name, "%s/%s", theme, region);
    (void)snprintf(rel, sizeof rel, "assets/ui/%s/%s.png", theme, region);
    nt_atlas_sprite_opts_t opts = nt_atlas_sprite_opts_defaults();
    opts.name = name;
    opts.slice9_left = (uint16_t)l;
    opts.slice9_right = (uint16_t)r;
    opts.slice9_top = (uint16_t)t;
    opts.slice9_bottom = (uint16_t)b;
    nt_atlas_add(atlas, path_join(lab_dir, rel), &opts);
}

static void add_plain(NtAtlasBuild *atlas, const char *lab_dir, const char *name_prefix, const char *theme, const char *region, const char *subdir) {
    char name[128];
    char rel[256];
    (void)snprintf(name, sizeof name, "%s/%s", theme != NULL ? theme : name_prefix, region);
    if (theme != NULL) {
        (void)snprintf(rel, sizeof rel, "assets/%s/%s/%s.png", subdir, theme, region);
    } else {
        (void)snprintf(rel, sizeof rel, "assets/%s/%s.png", subdir, region);
    }
    nt_atlas_sprite_opts_t opts = nt_atlas_sprite_opts_defaults();
    opts.name = name;
    nt_atlas_add(atlas, path_join(lab_dir, rel), &opts);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        (void)fprintf(stderr, "Usage: build_ui_lab_packs <pack_dir> <header_dir> <lab_dir>\n");
        return 1;
    }
    const char *out_dir = argv[1];
    const char *header_dir = argv[2];
    const char *lab_dir = argv[3];
    (void)MKDIR(out_dir);
    (void)MKDIR(header_dir);

    NtBuilderContext *ctx = nt_builder_start_pack(path_join(out_dir, "ui_lab.ntpack"));
    if (ctx == NULL) {
        (void)fprintf(stderr, "Failed to start ui_lab.ntpack\n");
        return 1;
    }
    nt_builder_set_header_dir(ctx, header_dir);
    static char cache_dir[1024];
    (void)snprintf(cache_dir, sizeof cache_dir, "%s/_cache", out_dir);
    (void)MKDIR(cache_dir);
    nt_builder_set_cache_dir(ctx, cache_dir);
    nt_builder_set_threads_auto(ctx);

    nt_builder_add_shader(ctx, "assets/shaders/sprite.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/sprite.frag", NT_BUILD_SHADER_FRAGMENT);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/slug_text.frag", NT_BUILD_SHADER_FRAGMENT);
    /* The SDF disc behind the kit's round close: crisp at any size, no art. */
    nt_builder_add_shader(ctx, "assets/shaders/sprite_radial.vert", NT_BUILD_SHADER_VERTEX);
    nt_builder_add_shader(ctx, "assets/shaders/radial.frag", NT_BUILD_SHADER_FRAGMENT);

    nt_builder_add_font(ctx, FONT_PATH,
                        &(nt_font_opts_t){.charset = NT_CHARSET_ASCII CHARSET_CYRILLIC CHARSET_SYMBOLS,
                                          .resource_name = "ui_lab/font"});

    /* UI atlas: mip-safe gutters, because the kit draws below its export size. */
    nt_atlas_opts_t ui_opts = nt_atlas_opts_defaults();
    ui_opts.shape = NT_ATLAS_SHAPE_RECT;
    ui_opts.allowed_transforms = NT_ATLAS_TRANSFORMS_IDENTITY;
    ui_opts.pixels_per_unit = 1.0F;
    ui_opts.padding = 8;
    ui_opts.margin = 8;
    ui_opts.extrude = 2;
    ui_opts.premultiplied = true;
    ui_opts.compress = (nt_basisu_encode_opts_t){0}; /* raw RGBA page */
    ui_opts.filter_min = NT_TEXTURE_DEFAULT_FILTER_LINEAR_MIPMAP_LINEAR;
    ui_opts.filter_mag = NT_TEXTURE_DEFAULT_FILTER_LINEAR;
    ui_opts.wrap_u = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    ui_opts.wrap_v = NT_TEXTURE_DEFAULT_WRAP_CLAMP_TO_EDGE;
    ui_opts.gen_mipmaps = true;
    NtAtlasBuild *ui = nt_atlas_begin(ctx, "ui", &ui_opts);

    static const uint8_t white_pixel[4] = {255, 255, 255, 255};
    nt_atlas_sprite_opts_t white_opts = nt_atlas_sprite_opts_defaults();
    white_opts.name = "_white";
    nt_atlas_add_raw(ui, white_pixel, 1, 1, &white_opts);

    for (size_t i = 0; i < sizeof THEMES / sizeof THEMES[0]; ++i) {
        const char *theme = THEMES[i];
        add_slice9(ui, lab_dir, theme, "panel", PANEL_BORDER, PANEL_BORDER, PANEL_BORDER, PANEL_BORDER);
        add_slice9(ui, lab_dir, theme, "button", BUTTON_BORDER_X, BUTTON_BORDER_X, BUTTON_BORDER_TOP, BUTTON_BORDER_BOTTOM);
        add_slice9(ui, lab_dir, theme, "tile", TILE_BORDER, TILE_BORDER, TILE_BORDER, TILE_BORDER);
        add_slice9(ui, lab_dir, theme, "slider_track", BAR_BORDER, BAR_BORDER, BAR_BORDER, BAR_BORDER);
        add_slice9(ui, lab_dir, theme, "slider_fill", BAR_BORDER, BAR_BORDER, BAR_BORDER, BAR_BORDER);
        add_slice9(ui, lab_dir, theme, "slider_track_sm", BAR_BORDER_SM, BAR_BORDER_SM, BAR_BORDER_SM, BAR_BORDER_SM);
        add_slice9(ui, lab_dir, theme, "slider_fill_sm", BAR_BORDER_SM, BAR_BORDER_SM, BAR_BORDER_SM, BAR_BORDER_SM);
        add_plain(ui, lab_dir, NULL, theme, "slider_thumb", "ui");
        add_plain(ui, lab_dir, NULL, theme, "icon_play", "ui");
        add_slice9(ui, lab_dir, theme, "header", HEADER_BORDER, HEADER_BORDER, HEADER_BORDER, HEADER_BORDER_BOTTOM);
    }
    for (size_t i = 0; i < sizeof ICONS / sizeof ICONS[0]; ++i) {
        char name[64];
        char rel[256];
        (void)snprintf(name, sizeof name, "icon/%s", ICONS[i]);
        (void)snprintf(rel, sizeof rel, STUDIO_FROM_LAB "/templates/template/assets/icons/%s.png", ICON_FILES[i]);
        nt_atlas_sprite_opts_t opts = nt_atlas_sprite_opts_defaults();
        opts.name = name;
        nt_atlas_add(ui, path_join(lab_dir, rel), &opts);
    }
    /* The kit's own glyphs, under `kit/` as the theme binds them. */
    for (int i = 0; i < UI_ICON_COUNT; ++i) {
        char name[64];
        char rel[256];
        (void)snprintf(name, sizeof name, "kit/%s", ui_icon_name((ui_icon_t)i));
        (void)snprintf(rel, sizeof rel, STUDIO_FROM_LAB "/features/ui-kit/assets/icons/%s.png", ui_icon_name((ui_icon_t)i));
        nt_atlas_sprite_opts_t opts = nt_atlas_sprite_opts_defaults();
        opts.name = name;
        nt_atlas_add(ui, path_join(lab_dir, rel), &opts);
    }
    /* The backdrop shares the UI atlas: nt_ui draws every image through one
     * sprite material, so a second atlas would need a second material. */
    nt_atlas_sprite_opts_t world_opts = nt_atlas_sprite_opts_defaults();
    world_opts.name = "scene/world";
    nt_atlas_add(ui, path_join(lab_dir, "assets/scene/world.png"), &world_opts);
    (void)nt_atlas_commit(ui);

    const nt_build_result_t r = nt_builder_finish_pack(ctx);
    nt_builder_free_pack(ctx);
    if (r != NT_BUILD_OK) {
        (void)fprintf(stderr, "ui_lab.ntpack failed: %d\n", r);
        return 1;
    }
    static char base_hdr[1024];
    static char merged_hdr[1024];
    (void)snprintf(base_hdr, sizeof base_hdr, "%s/ui_lab.h", header_dir);
    (void)snprintf(merged_hdr, sizeof merged_hdr, "%s/ui_lab_assets.h", header_dir);
    const char *headers[] = {base_hdr};
    nt_builder_merge_headers(headers, 1, merged_hdr);
    (void)printf("Built ui_lab.ntpack\n");
    return 0;
}
