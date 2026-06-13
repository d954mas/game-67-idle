#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "fs/nt_fs.h"
#include "game_audio.h"
#include "game_state_actions.h"
#include "game_storage.h"
#include "generated/game_state.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "log/nt_log.h"
#include "material/nt_material.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_shape_renderer.h"
#include "renderers/nt_sprite_renderer.h"
#include "resource/nt_resource.h"
#include "window/nt_window.h"

#include "world67_assets.h"

#include "cJSON.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#else
#include <glad/gl.h>
#endif

#define DEFAULT_SAVE_KEY "autosave"
#define COLLECTION_VISIBLE_SLOTS 7

typedef struct UiBox {
    float x;
    float y;
    float w;
    float h;
} UiBox;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = 9123;
static bool s_fresh_state_requested;
static bool s_autosave_enabled = true;
static bool s_capture_once_requested;
static bool s_capture_once_written;
static char s_capture_once_path[512];
static int s_window_width = 960;
static int s_window_height = 540;
static UiBox s_spawn_box;
static UiBox s_merge_box;
static UiBox s_upgrade_box;
static UiBox s_board_box;
static UiBox s_collection_box;
static UiBox s_goal_box;
static UiBox s_hud_coin_box;
static UiBox s_hud_collection_box;
static UiBox s_hud_title_box;
static int s_selected_slot = -1;
static int s_feedback_frames;
static char s_feedback_text[32] = "SPAWN TWO 67";
static int s_reward_frames;
static int s_reward_variant = -1;
static nt_buffer_t s_frame_ubo;
static nt_hash32_t s_pack_id;
static nt_resource_t s_atlas_handle;
static nt_resource_t s_atlas_tex_handle;
static nt_resource_t s_sprite_vs_handle;
static nt_resource_t s_sprite_fs_handle;
static nt_material_t s_sprite_material;
static bool s_assets_bound;
static uint32_t s_region_white;
static uint32_t s_region_board_frame;
static uint32_t s_region_board_tile;
static uint32_t s_region_card_unlocked;
static uint32_t s_region_card_locked;
static uint32_t s_region_button_orange;
static uint32_t s_region_button_blue;
static uint32_t s_region_button_green;
static uint32_t s_region_button_disabled;
static uint32_t s_region_progress_empty;
static uint32_t s_region_progress_blue;
static uint32_t s_region_progress_green;
static uint32_t s_region_icon_coin;
static uint32_t s_region_icon_gem;
static uint32_t s_region_icon_plus;
static uint32_t s_region_icon_67_badge;
static uint32_t s_region_icon_mystery_crate;
static uint32_t s_region_icon_star;
static uint32_t s_region_icon_lock;
static uint32_t s_region_mystery_crate;
static uint32_t s_region_panel_wide_blue;
static uint32_t s_region_highlight_gold;
static uint32_t s_region_highlight_electric;
static uint32_t s_region_small_arrow_blue;
static uint32_t s_region_field_grass_tile;
static uint32_t s_region_field_dark_grass_tile;
static uint32_t s_region_field_light_grass_tile;
static uint32_t s_region_field_path_tile;
static uint32_t s_region_field_fence_post;
static uint32_t s_region_field_fence_rail_h;
static uint32_t s_region_field_fence_rail_v;
static uint32_t s_region_field_hud_panel;
static uint32_t s_region_field_catalog_panel;
static uint32_t s_region_field_catalog_drawer;
static uint32_t s_region_field_tutorial_plaque;
static uint32_t s_region_field_button_green;
static uint32_t s_region_field_button_disabled;
static uint32_t s_region_field_ground_shadow;
static uint32_t s_region_field_selection_gold;
static uint32_t s_region_field_selection_blue;
static uint32_t s_region_field_reward_spark;
static uint32_t s_region_field_mystery_crate;
static uint32_t s_region_characters[GAME_67_VARIANT_COUNT];

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static char s_pending_capture_path[512];
#endif

static void ortho(float left, float right, float bottom, float top, float near_z, float far_z, float out[16]) {
    memset(out, 0, sizeof(float) * 16);
    out[0] = 2.0F / (right - left);
    out[5] = 2.0F / (top - bottom);
    out[10] = -2.0F / (far_z - near_z);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(far_z + near_z) / (far_z - near_z);
    out[15] = 1.0F;
}

static void mat_identity(float out[16]) {
    memset(out, 0, sizeof(float) * 16);
    out[0] = 1.0F;
    out[5] = 1.0F;
    out[10] = 1.0F;
    out[15] = 1.0F;
}

static void mat_translate_scale(float x, float y, float sx, float sy, float out[16]) {
    mat_identity(out);
    out[0] = sx;
    out[5] = sy;
    out[12] = x;
    out[13] = y;
}

static uint32_t find_region(nt_hash64_t hash, const char *label) {
    const uint32_t region = nt_atlas_find_region(s_atlas_handle, hash.value);
    if (region == NT_ATLAS_INVALID_REGION) {
        nt_log_error("67 World art region missing: %s", label);
    }
    return region;
}

static bool valid_region(uint32_t region) {
    return region != NT_ATLAS_INVALID_REGION;
}

static void try_bind_art_assets(void) {
    if (s_assets_bound || !nt_resource_is_ready(s_atlas_handle)) {
        return;
    }

    s_region_white = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS__WHITE, "_white");
    s_region_board_frame = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_BOARD_FRAME, "board_frame");
    s_region_board_tile = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_BOARD_TILE, "board_tile");
    s_region_card_unlocked = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_CARD_UNLOCKED, "card_unlocked");
    s_region_card_locked = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_CARD_LOCKED, "card_locked");
    s_region_button_orange = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_BUTTON_ORANGE, "button_orange");
    s_region_button_blue = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_BUTTON_BLUE, "button_blue");
    s_region_button_green = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_BUTTON_GREEN, "button_green");
    s_region_button_disabled = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_BUTTON_DISABLED, "button_disabled");
    s_region_progress_empty = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_PROGRESS_EMPTY, "progress_empty");
    s_region_progress_blue = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_PROGRESS_BLUE, "progress_blue");
    s_region_progress_green = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_PROGRESS_GREEN, "progress_green");
    s_region_icon_coin = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_ICON_COIN_CROWN, "icon_coin_crown");
    s_region_icon_gem = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_ICON_GEM, "icon_gem");
    s_region_icon_plus = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_ICON_PLUS, "icon_plus");
    s_region_icon_67_badge = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_ICON_67_BADGE, "icon_67_badge");
    s_region_icon_mystery_crate = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_ICON_MYSTERY_CRATE, "icon_mystery_crate");
    s_region_icon_star = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_ICON_STAR, "icon_star");
    s_region_icon_lock = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_ICON_LOCK, "icon_lock");
    s_region_mystery_crate = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_MYSTERY_CRATE, "mystery_crate");
    s_region_panel_wide_blue = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_PANEL_WIDE_BLUE, "panel_wide_blue");
    s_region_highlight_gold = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_HIGHLIGHT_GOLD, "highlight_gold");
    s_region_highlight_electric = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_HIGHLIGHT_ELECTRIC, "highlight_electric");
    s_region_small_arrow_blue = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_SMALL_ARROW_BLUE, "small_arrow_blue");
    s_region_field_grass_tile = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_GRASS_TILE, "field_grass_tile");
    s_region_field_dark_grass_tile = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_DARK_GRASS_TILE, "field_dark_grass_tile");
    s_region_field_light_grass_tile = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_LIGHT_GRASS_TILE, "field_light_grass_tile");
    s_region_field_path_tile = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_PATH_TILE, "field_path_tile");
    s_region_field_fence_post = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_FENCE_POST, "field_fence_post");
    s_region_field_fence_rail_h = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_FENCE_RAIL_H, "field_fence_rail_h");
    s_region_field_fence_rail_v = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_FENCE_RAIL_V, "field_fence_rail_v");
    s_region_field_hud_panel = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_HUD_PANEL, "field_hud_panel");
    s_region_field_catalog_panel = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_CATALOG_PANEL, "field_catalog_panel");
    s_region_field_catalog_drawer = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_CATALOG_DRAWER, "field_catalog_drawer");
    s_region_field_tutorial_plaque = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_TUTORIAL_PLAQUE, "field_tutorial_plaque");
    s_region_field_button_green = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_BUTTON_GREEN, "field_button_green");
    s_region_field_button_disabled = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_BUTTON_DISABLED, "field_button_disabled");
    s_region_field_ground_shadow = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_GROUND_SHADOW, "field_ground_shadow");
    s_region_field_selection_gold = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_SELECTION_GOLD, "field_selection_gold");
    s_region_field_selection_blue = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_SELECTION_BLUE, "field_selection_blue");
    s_region_field_reward_spark = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_REWARD_SPARK, "field_reward_spark");
    s_region_field_mystery_crate = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_FIELD_MYSTERY_CRATE, "field_mystery_crate");
    s_region_characters[0] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_TINY_67, "tiny_67");
    s_region_characters[1] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_BERRY_67, "berry_67");
    s_region_characters[2] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_BANANA_67, "banana_67");
    s_region_characters[3] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_SMOOTHIE_67, "smoothie_67");
    s_region_characters[4] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_COOL_67, "cool_67");
    s_region_characters[5] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_PORTAL_67, "portal_67");
    s_region_characters[6] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_MYSTERY_67, "mystery_67");
    s_region_characters[7] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_JELLY_67, "jelly_67");
    s_region_characters[8] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_LEMON_67, "lemon_67");
    s_region_characters[9] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_WATERMELON_67, "watermelon_67");
    s_region_characters[10] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_BUBBLEGUM_67, "bubblegum_67");
    s_region_characters[11] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_STICKER_67, "sticker_67");
    s_region_characters[12] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_PARTY_67, "party_67");
    s_region_characters[13] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_ARCADE_67, "arcade_67");
    s_region_characters[14] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_CLOUD_67, "cloud_67");
    s_region_characters[15] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_CROWN_67, "crown_67");
    s_region_characters[16] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_ROCKET_67, "rocket_67");
    s_region_characters[17] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_RAINBOW_67, "rainbow_67");
    s_region_characters[18] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_NEON_67, "neon_67");
    s_region_characters[19] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_GUMMY_67, "gummy_67");
    s_region_characters[20] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_PIXEL_67, "pixel_67");
    s_region_characters[21] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_LAVA_67, "lava_67");
    s_region_characters[22] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_DONUT_67, "donut_67");
    s_region_characters[23] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_SLIME_67, "slime_67");
    s_region_characters[24] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_DISCO_67, "disco_67");
    s_region_characters[25] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_DRAGON_67, "dragon_67");
    s_region_characters[26] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_NINJA_67, "ninja_67");
    s_region_characters[27] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_GALAXY_67, "galaxy_67");
    s_region_characters[28] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_GOLDEN_67, "golden_67");
    s_region_characters[29] = find_region(ASSET_ATLAS_REGION_WORLD67_ART_ATLAS_COSMIC_67, "cosmic_67");

    bool character_regions_bound = true;
    for (int i = 0; i < GAME_67_VARIANT_COUNT; ++i) {
        character_regions_bound = character_regions_bound && valid_region(s_region_characters[i]);
    }
    s_assets_bound = valid_region(s_region_white) && valid_region(s_region_field_grass_tile) && valid_region(s_region_field_hud_panel) && valid_region(s_region_icon_mystery_crate) && character_regions_bound;
    if (s_assets_bound) {
        nt_log_info("67 World art atlas bound");
    }
}

#if !defined(NT_PLATFORM_WEB)
static bool write_framebuffer_ppm(const char *path, int width, int height) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        (void)fprintf(stderr, "Failed to open framebuffer capture: %s\n", path);
        return false;
    }

    const size_t bytes = (size_t)width * (size_t)height * 3U;
    unsigned char *pixels = (unsigned char *)malloc(bytes);
    if (!pixels) {
        (void)fprintf(stderr, "Failed to allocate framebuffer capture buffer\n");
        (void)fclose(file);
        return false;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    (void)fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; y--) {
        (void)fwrite(pixels + ((size_t)y * (size_t)width * 3U), 1, (size_t)width * 3U, file);
    }

    free(pixels);
    (void)fclose(file);
    return true;
}
#endif

static void rect(float x, float y, float w, float h, const float color[4]) {
    const float pos[3] = {x, y, 0.0F};
    const float size[2] = {w, h};
    nt_shape_renderer_rect(pos, size, color);
}

static void line(float x0, float y0, float x1, float y1, const float color[4]) {
    const float a[3] = {x0, y0, 0.0F};
    const float b[3] = {x1, y1, 0.0F};
    nt_shape_renderer_line(a, b, color);
}

static void outline(float x, float y, float w, float h, const float color[4]) {
    line(x, y, x + w, y, color);
    line(x + w, y, x + w, y + h, color);
    line(x + w, y + h, x, y + h, color);
    line(x, y + h, x, y, color);
}

static void circle(float x, float y, float radius, const float color[4]) {
    const float center[3] = {x, y, 0.0F};
    nt_shape_renderer_circle(center, radius, color);
}

static uint8_t glyph_row(char ch, int row) {
    static const uint8_t digits[10][7] = {
        {14, 17, 19, 21, 25, 17, 14},
        {4, 12, 4, 4, 4, 4, 14},
        {14, 17, 1, 2, 4, 8, 31},
        {30, 1, 1, 14, 1, 1, 30},
        {2, 6, 10, 18, 31, 2, 2},
        {31, 16, 30, 1, 1, 17, 14},
        {6, 8, 16, 30, 17, 17, 14},
        {31, 1, 2, 4, 8, 8, 8},
        {14, 17, 17, 14, 17, 17, 14},
        {14, 17, 17, 15, 1, 2, 12},
    };
    static const uint8_t letters[26][7] = {
        {14, 17, 17, 31, 17, 17, 17}, /* A */
        {30, 17, 17, 30, 17, 17, 30},
        {14, 17, 16, 16, 16, 17, 14},
        {30, 17, 17, 17, 17, 17, 30},
        {31, 16, 16, 30, 16, 16, 31},
        {31, 16, 16, 30, 16, 16, 16},
        {14, 17, 16, 23, 17, 17, 14},
        {17, 17, 17, 31, 17, 17, 17},
        {14, 4, 4, 4, 4, 4, 14},
        {7, 2, 2, 2, 18, 18, 12},
        {17, 18, 20, 24, 20, 18, 17},
        {16, 16, 16, 16, 16, 16, 31},
        {17, 27, 21, 21, 17, 17, 17},
        {17, 25, 21, 19, 17, 17, 17},
        {14, 17, 17, 17, 17, 17, 14},
        {30, 17, 17, 30, 16, 16, 16},
        {14, 17, 17, 17, 21, 18, 13},
        {30, 17, 17, 30, 20, 18, 17},
        {15, 16, 16, 14, 1, 1, 30},
        {31, 4, 4, 4, 4, 4, 4},
        {17, 17, 17, 17, 17, 17, 14},
        {17, 17, 17, 17, 17, 10, 4},
        {17, 17, 17, 21, 21, 27, 17},
        {17, 17, 10, 4, 10, 17, 17},
        {17, 17, 10, 4, 4, 4, 4},
        {31, 1, 2, 4, 8, 16, 31},
    };
    if (ch >= '0' && ch <= '9') {
        return digits[ch - '0'][row];
    }
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    }
    if (ch >= 'A' && ch <= 'Z') {
        return letters[ch - 'A'][row];
    }
    if (ch == '/') {
        static const uint8_t slash[7] = {1, 1, 2, 4, 8, 16, 16};
        return slash[row];
    }
    if (ch == '+') {
        static const uint8_t plus[7] = {0, 4, 4, 31, 4, 4, 0};
        return plus[row];
    }
    if (ch == '!') {
        static const uint8_t bang[7] = {4, 4, 4, 4, 4, 0, 4};
        return bang[row];
    }
    if (ch == '?') {
        static const uint8_t q[7] = {14, 17, 1, 2, 4, 0, 4};
        return q[row];
    }
    if (ch == ':') {
        static const uint8_t colon[7] = {0, 4, 4, 0, 4, 4, 0};
        return colon[row];
    }
    return 0;
}

static float text_width(const char *text, float scale) {
    return (float)strlen(text) * scale * 6.0F;
}

static void draw_text(float x, float y, const char *text, float scale, const float color[4]) {
    for (const char *p = text; *p; p++) {
        if (*p != ' ') {
            for (int row = 0; row < 7; row++) {
                const uint8_t bits = glyph_row(*p, row);
                for (int col = 0; col < 5; col++) {
                    if (bits & (uint8_t)(1U << (4 - col))) {
                        rect(x + (float)col * scale, y + (float)row * scale, scale * 0.86F, scale * 0.86F, color);
                    }
                }
            }
        }
        x += scale * 6.0F;
    }
}

static void draw_text_center(float cx, float y, const char *text, float scale, const float color[4]) {
    draw_text(cx - text_width(text, scale) * 0.5F, y, text, scale, color);
}

static void draw_text_center_fit(float cx, float y, const char *text, float max_scale, float min_scale, float max_width, const float color[4]) {
    float scale = max_scale;
    const float width = text_width(text, scale);
    if (width > max_width && width > 0.0F) {
        scale = fmaxf(min_scale, scale * max_width / width);
    }
    draw_text_center(cx, y, text, scale, color);
}

static bool contains(UiBox box, float x, float y) {
    return x >= box.x && x <= box.x + box.w && y >= box.y && y <= box.y + box.h;
}

static bool portrait_layout(float w, float h) {
    return h > w * 1.25F && w < 620.0F;
}

static UiBox board_slot_box(int slot) {
    static const float positions[GAME_67_BOARD_SLOTS][2] = {
        {0.16F, 0.27F},
        {0.34F, 0.25F},
        {0.52F, 0.27F},
        {0.73F, 0.28F},
        {0.88F, 0.38F},
        {0.18F, 0.51F},
        {0.39F, 0.49F},
        {0.56F, 0.48F},
        {0.83F, 0.58F},
        {0.22F, 0.73F},
        {0.48F, 0.73F},
        {0.76F, 0.77F},
    };
    const int index = slot < 0 ? 0 : slot % GAME_67_BOARD_SLOTS;
    const float slot_w = fminf(96.0F, fmaxf(64.0F, s_board_box.w * 0.098F));
    const float slot_h = slot_w * 0.82F;
    const float cx = s_board_box.x + s_board_box.w * positions[index][0];
    const float cy = s_board_box.y + s_board_box.h * positions[index][1];
    return (UiBox){cx - slot_w * 0.50F, cy - slot_h * 0.50F, slot_w, slot_h};
}

static int variant_for_slot(int slot) {
    int remaining = slot;
    for (int v = 0; v < GAME_67_VARIANT_COUNT; v++) {
        const int count = game_67_variant_count(&g_game_state, v);
        if (remaining < count) {
            return v;
        }
        remaining -= count;
    }
    return -1;
}

static int collection_visible_start(void) {
    int start = g_game_state.collection_discovered_count - 3;
    if (start < 0) {
        start = 0;
    }
    const int max_start = GAME_67_VARIANT_COUNT - COLLECTION_VISIBLE_SLOTS;
    if (start > max_start) {
        start = max_start;
    }
    return start;
}

static void feedback(const char *text) {
    (void)snprintf(s_feedback_text, sizeof(s_feedback_text), "%s", text);
    s_feedback_frames = 120;
}

static void feedback_spawned_variant(void) {
    const int spawn_index = game_67_spawn_variant_index(&g_game_state);
    if (spawn_index <= 0) {
        feedback("NEW TINY 67!");
        return;
    }
    const Game67VariantDef *defs = game_67_variants();
    char text[32];
    (void)snprintf(text, sizeof(text), "+%s", defs[spawn_index].name);
    feedback(text);
}

static void reward_variant(int variant) {
    if (variant < 0 || variant >= GAME_67_VARIANT_COUNT) {
        return;
    }
    s_reward_variant = variant;
    s_reward_frames = 150;
}

static void format_compact_number(int value, char *out, size_t out_cap) {
    if (out_cap == 0U) {
        return;
    }
    if (value < 0) {
        value = 0;
    }
    if (value < 10000) {
        (void)snprintf(out, out_cap, "%d", value);
    } else if (value < 1000000) {
        (void)snprintf(out, out_cap, "%dK", (value + 999) / 1000);
    } else {
        (void)snprintf(out, out_cap, "%dM", (value + 999999) / 1000000);
    }
}

static void format_action_amount(const char *prefix, int amount, char *out, size_t out_cap) {
    char compact[8];
    format_compact_number(amount, compact, sizeof(compact));
    (void)snprintf(out, out_cap, "%s%s", prefix, compact);
}

static void format_upgrade_display_value(const char *value, char *out, size_t out_cap) {
    if (out_cap == 0U) {
        return;
    }
    if (strncmp(value, "NEED", 4U) == 0 && value[4] != '\0') {
        (void)snprintf(out, out_cap, "NEED %s", value + 4);
        return;
    }
    if (strncmp(value, "BUY", 3U) == 0 && value[3] != '\0') {
        (void)snprintf(out, out_cap, "BUY %s", value + 3);
        return;
    }
    (void)snprintf(out, out_cap, "%s", value);
}

static void progress_upgrade_labels(const GameState *state, char *title, size_t title_cap, char *value, size_t value_cap) {
    if (!state->faster_spawn_bought) {
        (void)snprintf(title, title_cap, "%s", "SPEED");
        if (state->collection_discovered_count < 2) {
            (void)snprintf(value, value_cap, "%s", "LOCK");
        } else if (game_67_can_buy_faster_spawn(state)) {
            format_action_amount("BUY", GAME_67_FASTER_SPAWN_COST, value, value_cap);
        } else {
            format_action_amount("NEED", game_67_faster_spawn_cost_remaining(state), value, value_cap);
        }
        return;
    }

    const char *crate_state = game_67_better_crate_state(state);
    if (crate_state[0] == 'm') {
        (void)snprintf(title, title_cap, "%s", "BOX");
        (void)snprintf(value, value_cap, "%s", "MAX");
        return;
    }
    if (crate_state[0] == 'l') {
        (void)snprintf(title, title_cap, "%s", "BOX");
        (void)snprintf(value, value_cap, "%s", "NEXT");
        return;
    }

    const int next_level = state->better_crate_level + 1;
    (void)snprintf(title, title_cap, "BOX L%d", next_level);
    if (game_67_can_buy_better_crate(state)) {
        format_action_amount("BUY", game_67_better_crate_next_cost(state), value, value_cap);
    } else {
        format_action_amount("NEED", game_67_better_crate_cost_remaining(state), value, value_cap);
    }
}

static const char *spawn_action_label(const GameState *state) {
    if (game_67_can_spawn(state)) {
        return "TAP BOX";
    }
    if (game_67_can_recycle_lowest(state)) {
        return "FREE SLOT";
    }
    return "MERGE PAIR";
}

static void play_sfx(GameAudioCue cue) {
    game_audio_set_volume(g_game_state.settings_master_volume, g_game_state.settings_sfx_volume);
    game_audio_play(cue);
}

static UiBox spawn_cta_box(float w, float h) {
    const bool portrait = portrait_layout(w, h);
    const float label_w = portrait ? fminf(142.0F, s_board_box.w * 0.46F) : 112.0F;
    const float label_h = portrait ? 50.0F : 42.0F;
    const float min_x = s_board_box.x + 10.0F;
    const float max_x = s_board_box.x + s_board_box.w - label_w - 10.0F;
    float x = s_spawn_box.x + s_spawn_box.w * 0.50F - label_w * 0.50F;
    x = fmaxf(min_x, fminf(x, max_x));
    const float y = s_spawn_box.y - label_h - (portrait ? 12.0F : 10.0F);
    return (UiBox){x, y, label_w, label_h};
}

static UiBox reward_toast_box(float w, float h, float text_y) {
    const bool portrait = portrait_layout(w, h);
    const float toast_w = portrait ? fminf(178.0F, s_board_box.w * 0.58F) : fminf(238.0F, s_board_box.w * 0.32F);
    const float toast_h = portrait ? 38.0F : 40.0F;
    const float min_x = s_board_box.x + 12.0F;
    const float max_x = s_board_box.x + s_board_box.w - toast_w - 12.0F;
    float x = s_board_box.x + s_board_box.w * 0.50F - toast_w * 0.50F;
    x = fmaxf(min_x, fminf(x, max_x));
    return (UiBox){x, text_y - 10.0F, toast_w, toast_h};
}

static float reward_feedback_y(float w, float h) {
    if (portrait_layout(w, h)) {
        const UiBox spawn_label_box = spawn_cta_box(w, h);
        const float goal_y = s_board_box.y + 93.0F;
        const float feedback_min_y = goal_y + 54.0F;
        const float feedback_max_y = spawn_label_box.y - 34.0F;
        return fmaxf(feedback_min_y, fminf(s_board_box.y + s_board_box.h * 0.45F, feedback_max_y));
    }
    return s_board_box.y + s_board_box.h * 0.58F;
}

static void layout(float w, float h) {
    const bool portrait = portrait_layout(w, h);
    const float pad = fmaxf(16.0F, fminf(w, h) * 0.035F);
    const float top_h = portrait ? fmaxf(132.0F, h * 0.16F) : fmaxf(70.0F, h * 0.13F);
    const float drawer_h = portrait ? fmaxf(122.0F, h * 0.145F) : fmaxf(82.0F, h * 0.155F);
    const float hud_y = 10.0F;
    const float hud_h = portrait ? 48.0F : fminf(52.0F, top_h - 18.0F);
    const float coin_w = portrait ? fminf(110.0F, w * 0.28F) : fminf(210.0F, w * 0.22F);
    const float collection_w = portrait ? fminf(90.0F, w * 0.23F) : fminf(136.0F, w * 0.15F);
    const float title_w = portrait ? fminf(w - pad * 2.0F - 20.0F, 230.0F) : fminf(205.0F, w * 0.22F);
    const float speed_w = portrait ? fminf(122.0F, w * 0.31F) : fminf(190.0F, w * 0.20F);
    const float side_margin = portrait ? fmaxf(12.0F, pad * 0.75F) : pad + 8.0F;
    s_hud_coin_box = (UiBox){side_margin, hud_y, coin_w, hud_h};
    s_hud_collection_box = portrait ? (UiBox){(w - collection_w) * 0.50F, hud_y, collection_w, hud_h}
                                    : (UiBox){s_hud_coin_box.x + s_hud_coin_box.w + 12.0F, hud_y, collection_w, hud_h};
    s_hud_title_box = (UiBox){(w - title_w) * 0.50F, portrait ? hud_y + hud_h + 8.0F : hud_y, title_w, hud_h};
    s_upgrade_box = (UiBox){w - side_margin - speed_w, hud_y, speed_w, hud_h};
    s_goal_box = s_hud_title_box;
    const float board_bottom_pad = portrait ? pad * 2.85F : pad * 1.20F;
    s_board_box = (UiBox){pad, top_h + pad * 0.35F, w - pad * 2.0F, h - top_h - drawer_h - board_bottom_pad};
    if (s_board_box.h < 230.0F) {
        s_board_box.h = fmaxf(185.0F, h * 0.48F);
    }
    const float drawer_bottom_margin = portrait ? pad * 0.92F : pad * 0.30F;
    s_collection_box = (UiBox){pad, h - drawer_h - drawer_bottom_margin, w - pad * 2.0F, drawer_h};
    const float crate = fminf(78.0F, fmaxf(58.0F, fminf(s_board_box.w, s_board_box.h) * 0.20F));
    s_spawn_box = (UiBox){s_board_box.x + s_board_box.w * 0.67F - crate * 0.60F,
                          s_board_box.y + s_board_box.h * 0.70F - crate * 0.50F,
                          crate * 1.20F,
                          crate};
    s_merge_box = (UiBox){0};
}

static const float *variant_color(int index) {
    static const float colors[GAME_67_VARIANT_COUNT][4] = {
        {0.12F, 0.48F, 1.00F, 1.0F},
        {1.00F, 0.22F, 0.30F, 1.0F},
        {1.00F, 0.78F, 0.10F, 1.0F},
        {0.58F, 0.20F, 0.90F, 1.0F},
        {0.06F, 0.18F, 0.32F, 1.0F},
        {0.05F, 0.82F, 0.94F, 1.0F},
        {0.18F, 0.18F, 0.22F, 1.0F},
        {0.86F, 0.28F, 0.78F, 1.0F},
        {1.00F, 0.92F, 0.20F, 1.0F},
        {0.12F, 0.74F, 0.34F, 1.0F},
        {1.00F, 0.42F, 0.72F, 1.0F},
        {0.94F, 0.46F, 0.18F, 1.0F},
        {0.92F, 0.18F, 0.48F, 1.0F},
        {0.12F, 0.18F, 0.86F, 1.0F},
        {0.62F, 0.86F, 1.00F, 1.0F},
        {1.00F, 0.76F, 0.12F, 1.0F},
        {0.92F, 0.22F, 0.12F, 1.0F},
        {0.46F, 0.24F, 1.00F, 1.0F},
        {0.05F, 0.42F, 1.00F, 1.0F},
        {1.00F, 0.28F, 0.62F, 1.0F},
        {0.16F, 0.70F, 0.86F, 1.0F},
        {0.95F, 0.18F, 0.06F, 1.0F},
        {1.00F, 0.50F, 0.72F, 1.0F},
        {0.20F, 0.86F, 0.18F, 1.0F},
        {0.86F, 0.88F, 1.00F, 1.0F},
        {0.16F, 0.62F, 0.24F, 1.0F},
        {0.04F, 0.04F, 0.08F, 1.0F},
        {0.26F, 0.12F, 0.88F, 1.0F},
        {1.00F, 0.74F, 0.05F, 1.0F},
        {0.60F, 0.20F, 1.00F, 1.0F},
    };
    return colors[index < 0 || index >= GAME_67_VARIANT_COUNT ? 0 : index];
}

static uint32_t variant_tint(int index) {
    (void)index;
    return 0xFFFFFFFFU;
}

static void draw_67_mascot(float cx, float cy, float s, int variant, bool locked) {
    const float white[4] = {1.0F, 0.96F, 0.86F, 1.0F};
    const float black[4] = {0.04F, 0.04F, 0.08F, 1.0F};
    const float shadow[4] = {0.03F, 0.07F, 0.11F, 1.0F};
    const float locked_col[4] = {0.34F, 0.38F, 0.46F, 1.0F};
    const float *body = locked ? locked_col : variant_color(variant);
    const float bw = s * 0.98F;
    const float bh = s * 0.78F;
    const float x = cx - bw * 0.50F;
    const float y = cy - bh * 0.50F;
    rect(x + bw * 0.08F, y + bh * 0.12F, bw, bh, shadow);
    rect(x, y, bw, bh, body);
    rect(x + bw * 0.06F, y + bh * 0.08F, bw * 0.88F, bh * 0.16F, (float[4]){1.0F, 1.0F, 1.0F, 0.35F});
    outline(x, y, bw, bh, black);
    const float eye_w = fmaxf(2.0F, bw * 0.08F);
    rect(cx - bw * 0.24F, y + bh * 0.22F, eye_w, eye_w, white);
    rect(cx + bw * 0.16F, y + bh * 0.22F, eye_w, eye_w, white);
    rect(cx - bw * 0.22F, y + bh * 0.24F, eye_w * 0.55F, eye_w * 0.55F, black);
    rect(cx + bw * 0.18F, y + bh * 0.24F, eye_w * 0.55F, eye_w * 0.55F, black);
    const float digit_scale = fmaxf(2.2F, fminf(bw / 13.0F, bh / 8.3F));
    draw_text_center(cx, cy - digit_scale * 1.55F, locked ? "?" : "67", digit_scale, locked ? white : black);
    line(x - bw * 0.06F, y + bh * 0.56F, x - bw * 0.22F, y + bh * 0.72F, black);
    line(x + bw * 1.06F, y + bh * 0.56F, x + bw * 1.22F, y + bh * 0.72F, black);
    if (variant == 1) {
        const float leaf[4] = {0.18F, 0.72F, 0.18F, 1.0F};
        rect(cx - bw * 0.18F, y - bh * 0.18F, bw * 0.16F, bh * 0.20F, leaf);
        rect(cx + bw * 0.02F, y - bh * 0.16F, bw * 0.16F, bh * 0.18F, leaf);
    } else if (variant == 2) {
        const float peel[4] = {1.0F, 0.94F, 0.28F, 1.0F};
        rect(x + bw * 0.08F, y - bh * 0.16F, bw * 0.84F, bh * 0.10F, peel);
    } else if (variant == 3) {
        const float straw[4] = {1.0F, 0.95F, 0.95F, 1.0F};
        rect(x + bw * 0.72F, y - bh * 0.34F, bw * 0.08F, bh * 0.42F, straw);
    } else if (variant == 4) {
        const float gold[4] = {1.0F, 0.82F, 0.12F, 1.0F};
        rect(x + bw * 0.12F, y + bh * 0.24F, bw * 0.76F, bh * 0.15F, gold);
    } else if (variant == 5) {
        const float glow[4] = {0.5F, 1.0F, 1.0F, 0.9F};
        outline(x - bw * 0.10F, y - bh * 0.10F, bw * 1.20F, bh * 1.20F, glow);
    } else if (variant == 6) {
        const float pink[4] = {1.0F, 0.30F, 0.80F, 1.0F};
        rect(x + bw * 0.76F, y - bh * 0.18F, bw * 0.18F, bh * 0.18F, pink);
    }
}

static int lowest_pair_variant(void) {
    for (int i = 0; i < GAME_67_VARIANT_COUNT - 1; i++) {
        if (game_67_variant_count(&g_game_state, i) >= 2) {
            return i;
        }
    }
    return -1;
}

static bool first_pair_slots(int *slot_a, int *slot_b) {
    const int pair_variant = lowest_pair_variant();
    if (pair_variant < 0) {
        return false;
    }
    int first = -1;
    for (int i = 0; i < GAME_67_BOARD_SLOTS; i++) {
        if (variant_for_slot(i) != pair_variant) {
            continue;
        }
        if (first < 0) {
            first = i;
        } else {
            *slot_a = first;
            *slot_b = i;
            return true;
        }
    }
    return false;
}

static const char *ftue_prompt(void) {
    return game_67_ftue_prompt(&g_game_state);
}

static void draw_button(UiBox box, const float color[4], bool enabled, int icon) {
    const float off[4] = {0.02F, 0.08F, 0.14F, 1.0F};
    const float disabled[4] = {0.38F, 0.42F, 0.48F, 1.0F};
    rect(box.x, box.y + 5.0F, box.w, box.h, off);
    rect(box.x, box.y, box.w, box.h, enabled ? color : disabled);
    if (icon == 0) {
        draw_67_mascot(box.x + box.w * 0.5F, box.y + box.h * 0.52F, fminf(box.w, box.h) * 0.42F, 0, !enabled);
    } else if (icon == 1) {
        const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        circle(box.x + box.w * 0.38F, box.y + box.h * 0.50F, box.h * 0.18F, white);
        circle(box.x + box.w * 0.62F, box.y + box.h * 0.50F, box.h * 0.18F, white);
        line(box.x + box.w * 0.46F, box.y + box.h * 0.50F, box.x + box.w * 0.54F, box.y + box.h * 0.50F, white);
    } else {
        const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
        rect(box.x + box.w * 0.30F, box.y + box.h * 0.43F, box.w * 0.40F, box.h * 0.14F, white);
        rect(box.x + box.w * 0.43F, box.y + box.h * 0.30F, box.w * 0.14F, box.h * 0.40F, white);
    }
}

static void draw_crate(UiBox box, bool enabled) {
    const float shadow[4] = {0.04F, 0.08F, 0.12F, 1.0F};
    const float wood[4] = {0.86F, 0.52F, 0.18F, 1.0F};
    const float wood2[4] = {1.0F, 0.68F, 0.26F, 1.0F};
    const float tape[4] = {1.0F, 0.90F, 0.36F, 1.0F};
    const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    const float disabled[4] = {0.42F, 0.42F, 0.46F, 1.0F};
    rect(box.x, box.y + 8.0F, box.w, box.h, shadow);
    rect(box.x, box.y, box.w, box.h, enabled ? wood : disabled);
    outline(box.x, box.y, box.w, box.h, (float[4]){0.38F, 0.20F, 0.08F, 1.0F});
    rect(box.x + box.w * 0.08F, box.y + box.h * 0.16F, box.w * 0.84F, box.h * 0.20F, enabled ? wood2 : disabled);
    rect(box.x + box.w * 0.45F, box.y, box.w * 0.10F, box.h, tape);
    draw_67_mascot(box.x + box.w * 0.22F, box.y + box.h * 0.55F, box.h * 0.42F, 0, !enabled);
    draw_text_center(box.x + box.w * 0.66F, box.y + box.h * 0.30F, "TAP", fmaxf(3.2F, box.h * 0.085F), white);
    draw_text_center(box.x + box.w * 0.66F, box.y + box.h * 0.58F, "BOX", fmaxf(3.2F, box.h * 0.085F), white);
}

static void draw_field_crate(UiBox box, bool enabled) {
    const float shadow[4] = {0.03F, 0.08F, 0.07F, 0.65F};
    const float wood[4] = {0.86F, 0.48F, 0.18F, 1.0F};
    const float wood_dark[4] = {0.54F, 0.27F, 0.10F, 1.0F};
    const float wood_light[4] = {1.0F, 0.67F, 0.26F, 1.0F};
    const float tape[4] = {1.0F, 0.88F, 0.30F, 1.0F};
    const float disabled[4] = {0.45F, 0.45F, 0.49F, 1.0F};
    const float base_y = box.y + box.h * 0.14F;
    const float body_h = box.h * 0.72F;

    rect(box.x + box.w * 0.10F, box.y + box.h * 0.88F, box.w * 0.80F, box.h * 0.12F, shadow);
    rect(box.x + box.w * 0.06F, base_y, box.w * 0.88F, body_h, enabled ? wood : disabled);
    rect(box.x + box.w * 0.06F, base_y, box.w * 0.88F, body_h * 0.22F, enabled ? wood_light : disabled);
    rect(box.x + box.w * 0.44F, base_y, box.w * 0.12F, body_h, tape);
    line(box.x + box.w * 0.10F, base_y + body_h * 0.84F, box.x + box.w * 0.90F, base_y + body_h * 0.10F, wood_dark);
    line(box.x + box.w * 0.10F, base_y + body_h * 0.10F, box.x + box.w * 0.90F, base_y + body_h * 0.84F, wood_dark);
    outline(box.x + box.w * 0.06F, base_y, box.w * 0.88F, body_h, wood_dark);
    draw_67_mascot(box.x + box.w * 0.50F, box.y + box.h * 0.28F, box.h * 0.36F, 0, !enabled);
}

static void draw_art_region_fit_tint(uint32_t region, float cx, float cy, float max_w, float max_h, float scale_mul, uint32_t color) {
    if (!valid_region(region)) {
        return;
    }
    const nt_texture_region_t *info = nt_atlas_get_region(s_atlas_handle, region);
    if (!info || info->source_w == 0 || info->source_h == 0) {
        return;
    }
    const float sx = max_w / (float)info->source_w;
    const float sy = max_h / (float)info->source_h;
    const float scale = fminf(sx, sy) * scale_mul;
    float model[16];
    mat_translate_scale(cx, cy, scale, scale, model);
    nt_sprite_renderer_emit_region(s_atlas_handle, region, model, info->origin_x, info->origin_y, color, 0U);
}

static void draw_art_region_fit(uint32_t region, float cx, float cy, float max_w, float max_h, float scale_mul) {
    draw_art_region_fit_tint(region, cx, cy, max_w, max_h, scale_mul, 0xFFFFFFFFU);
}

static void draw_art_region_box(uint32_t region, UiBox box) {
    if (!valid_region(region)) {
        return;
    }
    const nt_texture_region_t *info = nt_atlas_get_region(s_atlas_handle, region);
    if (!info || info->source_w == 0 || info->source_h == 0) {
        return;
    }
    float model[16];
    mat_translate_scale(box.x + box.w * 0.50F, box.y + box.h * 0.50F, box.w / (float)info->source_w, box.h / (float)info->source_h, model);
    nt_sprite_renderer_emit_region(s_atlas_handle, region, model, info->origin_x, info->origin_y, 0xFFFFFFFFU, 0U);
}

static void draw_art_slice9(uint32_t region, UiBox box) {
    if (!valid_region(region)) {
        return;
    }
    float identity[16];
    mat_identity(identity);
    nt_sprite_renderer_emit_slice9(s_atlas_handle, region, box.x, box.y, box.w, box.h, NULL, 1.0F, 0xFFFFFFFFU, 0U, identity);
}

static void draw_art_field_first_layer(float w, float h) {
    const bool portrait = portrait_layout(w, h);
    const float field_top_h = s_board_box.h * 0.76F;
    const int cols = 5;
    const int rows = 3;
    const float tile_w = s_board_box.w / (float)cols;
    const float tile_h = field_top_h / (float)rows;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            const uint32_t region = ((x + y) % 5 == 0) ? s_region_field_light_grass_tile : s_region_field_grass_tile;
            draw_art_region_box(region, (UiBox){s_board_box.x + tile_w * (float)x, s_board_box.y + tile_h * (float)y, tile_w + 1.0F, tile_h + 1.0F});
        }
    }
    const float dark_h = s_board_box.h - field_top_h;
    const float dark_tile_w = s_board_box.w / 5.0F;
    for (int x = 0; x < 5; x++) {
        draw_art_region_box(s_region_field_dark_grass_tile, (UiBox){s_board_box.x + dark_tile_w * (float)x, s_board_box.y + field_top_h, dark_tile_w + 1.0F, dark_h + 1.0F});
    }
    draw_art_region_box(s_region_field_path_tile,
                        (UiBox){s_board_box.x + s_board_box.w * 0.47F,
                                s_board_box.y + s_board_box.h * 0.62F,
                                s_board_box.w * 0.25F,
                                s_board_box.h * 0.17F});

    const float post_w = fminf(42.0F, s_board_box.w * 0.045F);
    const float post_h = fminf(88.0F, s_board_box.h * 0.23F);
    const float rail_h = fminf(34.0F, s_board_box.h * 0.09F);
    const float rail_w = (s_board_box.w - post_w * 2.0F) / 4.0F;
    for (int i = 0; i <= 4; i++) {
        const float x = s_board_box.x + (s_board_box.w - post_w) * (float)i / 4.0F;
        draw_art_region_box(s_region_field_fence_post, (UiBox){x, s_board_box.y - post_h * 0.18F, post_w, post_h});
        draw_art_region_box(s_region_field_fence_post, (UiBox){x, s_board_box.y + s_board_box.h - post_h * 0.78F, post_w, post_h});
    }
    for (int i = 0; i < 4; i++) {
        const float x = s_board_box.x + post_w * 0.65F + rail_w * (float)i;
        draw_art_region_box(s_region_field_fence_rail_h, (UiBox){x, s_board_box.y + 8.0F, rail_w + 8.0F, rail_h});
        draw_art_region_box(s_region_field_fence_rail_h, (UiBox){x, s_board_box.y + s_board_box.h - rail_h - 10.0F, rail_w + 8.0F, rail_h});
    }

    draw_art_slice9(s_region_field_hud_panel, s_hud_coin_box);
    draw_art_slice9(s_region_field_hud_panel, s_hud_collection_box);
    draw_art_slice9(s_region_field_hud_panel, s_hud_title_box);
    draw_art_slice9(s_region_field_hud_panel, s_upgrade_box);
    const float plaque_w = portrait ? s_board_box.w * 0.42F : s_board_box.w * 0.32F;
    const float plaque_y = s_board_box.y + (portrait ? 58.0F : 38.0F);
    draw_art_slice9(s_region_field_tutorial_plaque,
                    (UiBox){s_board_box.x + (s_board_box.w - plaque_w) * 0.50F,
                            plaque_y,
                            plaque_w,
                            54.0F});
    draw_art_slice9(s_region_field_catalog_drawer, s_collection_box);
}

static bool art_ready(void) {
    const nt_material_info_t *sprite_info = nt_material_get_info(s_sprite_material);
    return s_assets_bound && sprite_info && sprite_info->ready;
}

static void draw_art_overlay(const float vp[16], float w, float h) {
    if (!art_ready()) {
        return;
    }

    nt_frame_uniforms_t uniforms = {0};
    memcpy(uniforms.view_proj, vp, 64);
    memcpy(uniforms.view, (float[16]){1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F}, 64);
    memcpy(uniforms.proj, vp, 64);
    uniforms.resolution[0] = w;
    uniforms.resolution[1] = h;
    uniforms.resolution[2] = (uniforms.resolution[0] > 0.0F) ? (1.0F / uniforms.resolution[0]) : 0.0F;
    uniforms.resolution[3] = (uniforms.resolution[1] > 0.0F) ? (1.0F / uniforms.resolution[1]) : 0.0F;
    uniforms.near_far[0] = -1.0F;
    uniforms.near_far[1] = 1.0F;

    nt_gfx_update_buffer(s_frame_ubo, &uniforms, sizeof(uniforms));
    nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);
    nt_sprite_renderer_set_material(s_sprite_material);

    draw_art_field_first_layer(w, h);

    for (int i = 0; i < GAME_67_BOARD_SLOTS; i++) {
        const UiBox slot = board_slot_box(i);
        const int variant = variant_for_slot(i);
        if (variant >= 0 && variant < GAME_67_VARIANT_COUNT) {
            draw_art_region_fit(s_region_field_ground_shadow,
                                slot.x + slot.w * 0.50F,
                                slot.y + slot.h * 0.78F,
                                slot.w * 0.74F,
                                slot.h * 0.24F,
                                1.0F);
            if (variant == lowest_pair_variant()) {
                draw_art_region_fit(valid_region(s_region_field_selection_gold) ? s_region_field_selection_gold : s_region_highlight_gold,
                                    slot.x + slot.w * 0.50F,
                                    slot.y + slot.h * 0.52F,
                                    slot.w * 0.98F,
                                    slot.h * 0.98F,
                                    1.0F);
            }
            if (s_selected_slot == i) {
                draw_art_region_fit(valid_region(s_region_field_selection_blue) ? s_region_field_selection_blue : s_region_highlight_electric,
                                    slot.x + slot.w * 0.50F,
                                    slot.y + slot.h * 0.52F,
                                    slot.w * 1.03F,
                                    slot.h * 1.03F,
                                    1.0F);
            }
            draw_art_region_fit_tint(s_region_characters[variant],
                                     slot.x + slot.w * 0.50F,
                                     slot.y + slot.h * 0.56F,
                                     slot.w * 0.84F,
                                     slot.h * 0.94F,
                                     1.0F,
                                     variant_tint(variant));
        }
    }

    draw_art_region_fit(valid_region(s_region_field_mystery_crate) ? s_region_field_mystery_crate : (valid_region(s_region_mystery_crate) ? s_region_mystery_crate : s_region_icon_mystery_crate),
                        s_spawn_box.x + s_spawn_box.w * 0.50F,
                        s_spawn_box.y + s_spawn_box.h * 0.50F,
                        s_spawn_box.w * 0.86F,
                        s_spawn_box.h * 1.04F,
                        1.0F);
    const bool can_spawn_or_recycle = game_67_can_spawn(&g_game_state) || game_67_can_recycle_lowest(&g_game_state);
    const uint32_t cta_region = can_spawn_or_recycle ? s_region_field_button_green : s_region_field_button_disabled;
    draw_art_slice9(valid_region(cta_region) ? cta_region : s_region_button_green, spawn_cta_box(w, h));
    if (s_feedback_frames > 0) {
        const uint32_t toast_region = valid_region(s_region_field_tutorial_plaque) ? s_region_field_tutorial_plaque : s_region_field_hud_panel;
        draw_art_slice9(toast_region, reward_toast_box(w, h, reward_feedback_y(w, h)));
    }

    int pair_a = -1;
    int pair_b = -1;
    if (first_pair_slots(&pair_a, &pair_b)) {
        const UiBox a = board_slot_box(pair_a);
        const UiBox b = board_slot_box(pair_b);
        const float ax = a.x + a.w * 0.50F;
        const float ay = a.y + a.h * 0.24F;
        const float bx = b.x + b.w * 0.50F;
        const float by = b.y + b.h * 0.24F;
        const float mid_x = (ax + bx) * 0.50F;
        const float mid_y = (ay + by) * 0.50F;
        const float gap_w = fmaxf(72.0F, fabsf(bx - ax) * 0.66F);
        const float guide_h = fminf(a.h, b.h) * 0.28F;
        draw_art_region_fit_tint(s_region_small_arrow_blue, mid_x, mid_y, gap_w, guide_h, 1.0F, 0xFFFFFFFFU);
        draw_art_region_fit_tint(s_region_icon_star, ax, ay, a.w * 0.20F, a.h * 0.22F, 1.0F, 0xFFFFFFFFU);
        draw_art_region_fit_tint(s_region_icon_star, bx, by, b.w * 0.20F, b.h * 0.22F, 1.0F, 0xFFFFFFFFU);
    }

    const float card_gap = 8.0F;
    const int visible_start = collection_visible_start();
    const float card_w = (s_collection_box.w - card_gap * (float)(COLLECTION_VISIBLE_SLOTS + 1)) / (float)COLLECTION_VISIBLE_SLOTS;
    for (int visible_i = 0; visible_i < COLLECTION_VISIBLE_SLOTS; visible_i++) {
        const int i = visible_start + visible_i;
        const float x = s_collection_box.x + card_gap + (float)visible_i * (card_w + card_gap);
        const bool unlocked = g_game_state.collection_discovered_count >= i + 1;
        const UiBox card_box = {x, s_collection_box.y + 28.0F, card_w, s_collection_box.h - 36.0F};
        if (unlocked) {
            draw_art_region_fit_tint(s_region_characters[i],
                                     x + card_w * 0.50F,
                                     s_collection_box.y + s_collection_box.h * 0.58F,
                                     card_w * 0.68F,
                                     s_collection_box.h * 0.50F,
                                     1.0F,
                                     variant_tint(i));
        } else if (i == g_game_state.collection_discovered_count) {
            draw_art_region_fit_tint(s_region_characters[i],
                                     x + card_w * 0.50F,
                                     s_collection_box.y + s_collection_box.h * 0.58F,
                                     card_w * 0.68F,
                                     s_collection_box.h * 0.50F,
                                     1.0F,
                                     0x88787878U);
            draw_art_region_fit(s_region_icon_lock, x + card_w * 0.72F, card_box.y + card_box.h * 0.33F, card_w * 0.26F, card_box.h * 0.30F, 1.0F);
        } else {
            draw_art_region_fit(s_region_icon_lock, x + card_w * 0.50F, card_box.y + card_box.h * 0.50F, card_w * 0.34F, card_box.h * 0.38F, 1.0F);
        }
        if (s_reward_frames > 0 && s_reward_variant == i) {
            const float t = (float)s_reward_frames / 150.0F;
            draw_art_region_fit_tint(valid_region(s_region_field_reward_spark) ? s_region_field_reward_spark : s_region_icon_star,
                                     x + card_w * (0.78F + 0.04F * sinf((float)s_reward_frames * 0.22F)),
                                     card_box.y + card_box.h * (0.23F + 0.04F * cosf((float)s_reward_frames * 0.18F)),
                                     card_w * 0.44F,
                                     card_box.h * 0.52F,
                                     0.85F + t * 0.25F,
                                     0xFFFFFFFFU);
        }
    }

    draw_art_region_fit(s_region_icon_plus,
                        s_upgrade_box.x + s_upgrade_box.w * 0.18F,
                        s_upgrade_box.y + s_upgrade_box.h * 0.50F,
                        fminf(36.0F, s_upgrade_box.w * 0.24F),
                        fminf(36.0F, s_upgrade_box.h * 0.58F),
                        1.0F);
    if (game_67_can_buy_faster_spawn(&g_game_state) || game_67_can_buy_better_crate(&g_game_state) || g_game_state.faster_spawn_bought) {
        draw_art_region_fit_tint(s_region_icon_star,
                                 s_upgrade_box.x + s_upgrade_box.w * 0.91F,
                                 s_upgrade_box.y + s_upgrade_box.h * 0.26F,
                                 fminf(24.0F, s_upgrade_box.w * 0.16F),
                                 fminf(24.0F, s_upgrade_box.h * 0.38F),
                                 1.0F,
                                 0xFFFFFFFFU);
    }

    draw_art_region_fit(s_region_icon_coin,
                        s_hud_coin_box.x + fminf(34.0F, s_hud_coin_box.w * 0.18F),
                        s_hud_coin_box.y + s_hud_coin_box.h * 0.50F,
                        fminf(36.0F, s_hud_coin_box.h * 0.70F),
                        fminf(36.0F, s_hud_coin_box.h * 0.70F),
                        1.0F);

    nt_sprite_renderer_flush();
}

static void draw_game_screen(float w, float h) {
    const bool portrait = portrait_layout(w, h);
    float vp[16];
    ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, vp);

    const float bg[4] = {0.08F, 0.58F, 0.92F, 1.0F};
    const float grass[4] = {0.22F, 0.78F, 0.38F, 1.0F};
    const float board[4] = {1.0F, 0.86F, 0.58F, 1.0F};
    const float board_edge[4] = {1.0F, 0.32F, 0.34F, 1.0F};
    const float card[4] = {0.18F, 0.56F, 0.92F, 1.0F};
    const float slot[4] = {1.0F, 0.96F, 0.82F, 1.0F};
    const float top[4] = {0.08F, 0.25F, 0.52F, 1.0F};
    const float coin[4] = {1.0F, 0.82F, 0.10F, 1.0F};
    const float gem[4] = {0.76F, 0.24F, 0.96F, 1.0F};
    const float yellow[4] = {1.0F, 0.74F, 0.12F, 1.0F};
    const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
    const float ink[4] = {0.04F, 0.08F, 0.16F, 1.0F};

    nt_gfx_begin_frame();
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {bg[0], bg[1], bg[2], bg[3]}, .clear_depth = 1.0F});

    nt_shape_renderer_set_vp(vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(5.0F);

    if (art_ready()) {
        const float clean_top[4] = {0.05F, 0.31F, 0.62F, 1.0F};
        const float clean_sky[4] = {0.11F, 0.61F, 0.93F, 1.0F};
        const float hill_a[4] = {0.28F, 0.81F, 0.42F, 1.0F};
        const float hill_b[4] = {0.15F, 0.66F, 0.34F, 1.0F};
        const float text_shadow[4] = {0.03F, 0.06F, 0.13F, 1.0F};
        const float text_light[4] = {1.0F, 0.98F, 0.84F, 1.0F};
        const float text_blue[4] = {0.05F, 0.14F, 0.33F, 1.0F};

        const float field_grass[4] = {0.34F, 0.83F, 0.38F, 1.0F};
        const float field_dark[4] = {0.15F, 0.56F, 0.25F, 1.0F};
        const float field_light[4] = {0.68F, 0.94F, 0.44F, 1.0F};
        const float fence[4] = {0.93F, 0.70F, 0.36F, 1.0F};
        const float fence_dark[4] = {0.50F, 0.28F, 0.12F, 1.0F};
        const float path[4] = {0.93F, 0.80F, 0.48F, 1.0F};
        const float panel_blue[4] = {0.05F, 0.36F, 0.69F, 1.0F};
        const float panel_deep[4] = {0.03F, 0.18F, 0.42F, 1.0F};
        const float card_light[4] = {0.28F, 0.68F, 0.92F, 1.0F};
        const float card_lock[4] = {0.17F, 0.36F, 0.54F, 1.0F};
        const float card_next[4] = {0.46F, 0.56F, 0.65F, 1.0F};
        const float gold_line[4] = {1.0F, 0.83F, 0.16F, 1.0F};
        const float selected_line[4] = {0.26F, 0.93F, 1.0F, 1.0F};

        rect(0.0F, 0.0F, w, h, clean_sky);
        rect(0.0F, 0.0F, w, fmaxf(58.0F, h * 0.11F), clean_top);
        rect(0.0F, h * 0.62F, w, h * 0.38F, hill_a);
        rect(0.0F, h * 0.81F, w, h * 0.19F, hill_b);
        circle(w * 0.12F, h * 0.20F, 36.0F, (float[4]){1.0F, 0.96F, 0.84F, 1.0F});
        circle(w * 0.78F, h * 0.19F, 48.0F, (float[4]){1.0F, 0.96F, 0.84F, 1.0F});

        rect(s_board_box.x - 6.0F, s_board_box.y + 5.0F, s_board_box.w + 12.0F, s_board_box.h + 12.0F, (float[4]){0.03F, 0.12F, 0.08F, 0.45F});
        const float field_tile_w = s_board_box.w / 12.0F;
        const float field_tile_h = s_board_box.h / 5.0F;
        for (int row_i = 0; row_i < 5; row_i++) {
            const float y = s_board_box.y + field_tile_h * (float)row_i;
            for (int col_i = 0; col_i < 12; col_i++) {
                const float x = s_board_box.x + field_tile_w * (float)col_i;
                rect(x, y, field_tile_w + 1.5F, field_tile_h + 1.5F, row_i >= 4 ? field_dark : field_grass);
            }
        }
        circle(s_board_box.x + s_board_box.w * 0.16F, s_board_box.y + s_board_box.h * 0.18F, s_board_box.h * 0.08F, field_light);
        circle(s_board_box.x + s_board_box.w * 0.86F, s_board_box.y + s_board_box.h * 0.24F, s_board_box.h * 0.07F, field_light);
        circle(s_board_box.x + s_board_box.w * 0.36F, s_board_box.y + s_board_box.h * 0.87F, s_board_box.h * 0.10F, field_light);
        rect(s_board_box.x + s_board_box.w * 0.59F, s_board_box.y + s_board_box.h * 0.66F, s_board_box.w * 0.24F, s_board_box.h * 0.10F, path);
        line(s_board_box.x + 12.0F, s_board_box.y + 14.0F, s_board_box.x + s_board_box.w - 12.0F, s_board_box.y + 14.0F, fence);
        line(s_board_box.x + 12.0F, s_board_box.y + 34.0F, s_board_box.x + s_board_box.w - 12.0F, s_board_box.y + 34.0F, fence);
        line(s_board_box.x + 12.0F, s_board_box.y + s_board_box.h - 18.0F, s_board_box.x + s_board_box.w - 12.0F, s_board_box.y + s_board_box.h - 18.0F, fence);
        line(s_board_box.x + 12.0F, s_board_box.y + s_board_box.h - 38.0F, s_board_box.x + s_board_box.w - 12.0F, s_board_box.y + s_board_box.h - 38.0F, fence);
        for (int i = 0; i < 9; i++) {
            const float post_x = s_board_box.x + 18.0F + (s_board_box.w - 36.0F) * (float)i / 8.0F;
            rect(post_x - 4.0F, s_board_box.y + 4.0F, 8.0F, 42.0F, fence_dark);
            rect(post_x - 4.0F, s_board_box.y + s_board_box.h - 48.0F, 8.0F, 42.0F, fence_dark);
        }
        outline(s_board_box.x, s_board_box.y, s_board_box.w, s_board_box.h, fence_dark);

        const int pair_variant = lowest_pair_variant();
        for (int i = 0; i < GAME_67_BOARD_SLOTS; i++) {
            const UiBox slot_box = board_slot_box(i);
            const int variant = variant_for_slot(i);
            const float cx = slot_box.x + slot_box.w * 0.50F;
            const float cy = slot_box.y + slot_box.h * 0.78F;
            rect(cx - slot_box.w * 0.35F, cy - slot_box.h * 0.08F, slot_box.w * 0.70F, slot_box.h * 0.12F, (float[4]){0.10F, 0.34F, 0.16F, 1.0F});
            if (variant == pair_variant) {
                circle(slot_box.x + slot_box.w * 0.50F, slot_box.y + slot_box.h * 0.50F, slot_box.w * 0.48F, (float[4]){1.0F, 0.83F, 0.12F, 0.32F});
                outline(slot_box.x + 4.0F, slot_box.y + 4.0F, slot_box.w - 8.0F, slot_box.h - 8.0F, gold_line);
            }
            if (s_selected_slot == i) {
                outline(slot_box.x, slot_box.y, slot_box.w, slot_box.h, selected_line);
            }
        }

        draw_field_crate(s_spawn_box, game_67_can_spawn(&g_game_state));

        rect(s_hud_coin_box.x - 3.0F, s_hud_coin_box.y + 4.0F, s_hud_coin_box.w + 6.0F, s_hud_coin_box.h, panel_deep);
        rect(s_hud_collection_box.x - 3.0F, s_hud_collection_box.y + 4.0F, s_hud_collection_box.w + 6.0F, s_hud_collection_box.h, panel_deep);
        rect(s_hud_title_box.x - 3.0F, s_hud_title_box.y + 4.0F, s_hud_title_box.w + 6.0F, s_hud_title_box.h, panel_deep);
        rect(s_upgrade_box.x - 3.0F, s_upgrade_box.y + 4.0F, s_upgrade_box.w + 6.0F, s_upgrade_box.h, panel_deep);
        rect(s_hud_coin_box.x + 14.0F, s_hud_coin_box.y + 10.0F, s_hud_coin_box.w - 28.0F, s_hud_coin_box.h - 20.0F, panel_blue);
        rect(s_hud_collection_box.x + 10.0F, s_hud_collection_box.y + s_hud_collection_box.h - 14.0F,
             (s_hud_collection_box.w - 20.0F) * fminf(1.0F, (float)g_game_state.collection_discovered_count / 67.0F),
             6.0F,
             gold_line);
        rect(s_upgrade_box.x + 8.0F,
             s_upgrade_box.y + 8.0F,
             s_upgrade_box.w - 16.0F,
             s_upgrade_box.h - 16.0F,
             game_67_can_buy_faster_spawn(&g_game_state) || g_game_state.faster_spawn_bought ? (float[4]){0.14F, 0.62F, 0.38F, 0.88F} : panel_blue);

        rect(s_collection_box.x, s_collection_box.y, s_collection_box.w, s_collection_box.h, panel_deep);
        rect(s_collection_box.x + 8.0F, s_collection_box.y + 8.0F, s_collection_box.w - 16.0F, s_collection_box.h - 16.0F, panel_blue);
        const float card_gap = 8.0F;
        const int visible_start = collection_visible_start();
        const float card_w = (s_collection_box.w - card_gap * (float)(COLLECTION_VISIBLE_SLOTS + 1)) / (float)COLLECTION_VISIBLE_SLOTS;
        for (int visible_i = 0; visible_i < COLLECTION_VISIBLE_SLOTS; visible_i++) {
            const int i = visible_start + visible_i;
            const float x = s_collection_box.x + card_gap + (float)visible_i * (card_w + card_gap);
            const bool unlocked = g_game_state.collection_discovered_count >= i + 1;
            const bool next = i == g_game_state.collection_discovered_count;
            const UiBox card_box = {x, s_collection_box.y + 28.0F, card_w, s_collection_box.h - 36.0F};
            rect(card_box.x, card_box.y + 3.0F, card_box.w, card_box.h, (float[4]){0.02F, 0.10F, 0.20F, 0.75F});
            rect(card_box.x, card_box.y, card_box.w, card_box.h, unlocked ? card_light : (next ? card_next : card_lock));
            if (s_reward_frames > 0 && s_reward_variant == i) {
                outline(card_box.x + 2.0F, card_box.y + 2.0F, card_box.w - 4.0F, card_box.h - 4.0F, gold_line);
            }
        }

        nt_shape_renderer_flush();

        draw_art_overlay(vp, w, h);

        char hud[32];
        (void)snprintf(hud, sizeof(hud), "%d", g_game_state.wallet_soft);
        const float coin_text_x = s_hud_coin_box.x + fminf(70.0F, s_hud_coin_box.w * 0.34F);
        draw_text_center_fit(coin_text_x + s_hud_coin_box.w * 0.31F + 1.0F,
                             s_hud_coin_box.y + 17.0F,
                             hud,
                             3.8F,
                             2.2F,
                             s_hud_coin_box.w * 0.48F,
                             text_shadow);
        draw_text_center_fit(coin_text_x + s_hud_coin_box.w * 0.31F,
                             s_hud_coin_box.y + 15.0F,
                             hud,
                             3.8F,
                             2.2F,
                             s_hud_coin_box.w * 0.48F,
                             text_light);
        (void)snprintf(hud, sizeof(hud), "%d/67", g_game_state.collection_discovered_count);
        draw_text_center_fit(s_hud_collection_box.x + s_hud_collection_box.w * 0.50F + 1.0F,
                             s_hud_collection_box.y + 14.0F,
                             hud,
                             3.1F,
                             2.0F,
                             s_hud_collection_box.w - 22.0F,
                             text_shadow);
        draw_text_center_fit(s_hud_collection_box.x + s_hud_collection_box.w * 0.50F,
                             s_hud_collection_box.y + 12.0F,
                             hud,
                             3.1F,
                             2.0F,
                             s_hud_collection_box.w - 22.0F,
                             text_light);
        draw_text_center_fit(s_hud_title_box.x + s_hud_title_box.w * 0.50F + 1.0F,
                             s_hud_title_box.y + 15.0F,
                             "67 WORLD",
                             2.9F,
                             2.0F,
                             s_hud_title_box.w - 26.0F,
                             text_shadow);
        draw_text_center_fit(s_hud_title_box.x + s_hud_title_box.w * 0.50F,
                             s_hud_title_box.y + 13.0F,
                             "67 WORLD",
                             2.9F,
                             2.0F,
                             s_hud_title_box.w - 26.0F,
                             text_light);
        const char *prompt = ftue_prompt();
        const float prompt_scale = portrait ? 2.25F : 2.8F;
        const float prompt_min = portrait ? 1.55F : 2.0F;
        const float goal_scale = portrait ? 1.70F : 2.0F;
        const float prompt_y = s_board_box.y + (portrait ? 72.0F : 50.0F);
        const float goal_y = s_board_box.y + (portrait ? 93.0F : 75.0F);
        draw_text_center_fit(s_board_box.x + s_board_box.w * 0.50F + 1.0F,
                             prompt_y + 2.0F,
                             prompt,
                             prompt_scale,
                             prompt_min,
                             s_board_box.w - 72.0F,
                             text_shadow);
        draw_text_center_fit(s_board_box.x + s_board_box.w * 0.50F,
                             prompt_y,
                             prompt,
                             prompt_scale,
                             prompt_min,
                             s_board_box.w - 72.0F,
                             text_light);
        draw_text_center_fit(s_board_box.x + s_board_box.w * 0.50F,
                             goal_y,
                             game_67_next_goal(&g_game_state),
                             goal_scale,
                             1.35F,
                             s_board_box.w - 96.0F,
                             text_blue);
        const float collection_label_scale = portrait ? 1.65F : 2.4F;
        const float collection_label_min = portrait ? 1.30F : 1.80F;
        const float collection_label_y = s_collection_box.y + (portrait ? 14.0F : 8.0F);
        draw_text_center_fit(s_collection_box.x + s_collection_box.w * 0.50F,
                             collection_label_y,
                             "COLLECTION",
                             collection_label_scale,
                             collection_label_min,
                             s_collection_box.w - 42.0F,
                             text_light);
        char upgrade_title[16];
        char upgrade_value[16];
        progress_upgrade_labels(&g_game_state, upgrade_title, sizeof(upgrade_title), upgrade_value, sizeof(upgrade_value));
        char upgrade_display_value[16];
        format_upgrade_display_value(upgrade_value, upgrade_display_value, sizeof(upgrade_display_value));
        const float upgrade_text_x = s_upgrade_box.x + s_upgrade_box.w * (portrait ? 0.64F : 0.63F);
        const float upgrade_text_w = s_upgrade_box.w * (portrait ? 0.62F : 0.58F);
        const float upgrade_title_y = s_upgrade_box.y + (portrait ? 11.0F : 10.0F);
        const float upgrade_value_y = s_upgrade_box.y + (portrait ? 30.0F : 29.0F);
        draw_text_center_fit(upgrade_text_x + 1.0F, upgrade_title_y + 2.0F, upgrade_title, 2.0F, 1.20F, upgrade_text_w, text_shadow);
        draw_text_center_fit(upgrade_text_x, upgrade_title_y, upgrade_title, 2.0F, 1.20F, upgrade_text_w, text_light);
        draw_text_center_fit(upgrade_text_x + 1.0F, upgrade_value_y + 2.0F, upgrade_display_value, 2.25F, 1.20F, upgrade_text_w, text_shadow);
        draw_text_center_fit(upgrade_text_x, upgrade_value_y, upgrade_display_value, 2.25F, 1.20F, upgrade_text_w, text_light);
        const char *spawn_label = spawn_action_label(&g_game_state);
        const bool free_slot_label = spawn_label[0] == 'F';
        const char *spawn_label_a = free_slot_label ? "FREE" : "TAP";
        const char *spawn_label_b = free_slot_label ? "SLOT" : "BOX";
        const UiBox spawn_label_box = spawn_cta_box(w, h);
        if (portrait) {
            draw_text_center_fit(spawn_label_box.x + spawn_label_box.w * 0.50F + 1.0F,
                                 spawn_label_box.y + 11.0F,
                                 spawn_label_a,
                                 2.15F,
                                 1.55F,
                                 spawn_label_box.w - 22.0F,
                                 text_shadow);
            draw_text_center_fit(spawn_label_box.x + spawn_label_box.w * 0.50F,
                                 spawn_label_box.y + 9.0F,
                                 spawn_label_a,
                                 2.15F,
                                 1.55F,
                                 spawn_label_box.w - 22.0F,
                                 text_blue);
            draw_text_center_fit(spawn_label_box.x + spawn_label_box.w * 0.50F + 1.0F,
                                 spawn_label_box.y + 30.0F,
                                 spawn_label_b,
                                 2.15F,
                                 1.55F,
                                 spawn_label_box.w - 22.0F,
                                 text_shadow);
            draw_text_center_fit(spawn_label_box.x + spawn_label_box.w * 0.50F,
                                 spawn_label_box.y + 28.0F,
                                 spawn_label_b,
                                 2.15F,
                                 1.55F,
                                 spawn_label_box.w - 22.0F,
                                 text_blue);
        } else {
            draw_text_center_fit(spawn_label_box.x + spawn_label_box.w * 0.5F + 1.0F,
                                 spawn_label_box.y + 10.0F,
                                 spawn_label_a,
                                 2.30F,
                                 1.70F,
                                 spawn_label_box.w - 14.0F,
                                 text_shadow);
            draw_text_center_fit(spawn_label_box.x + spawn_label_box.w * 0.5F,
                                 spawn_label_box.y + 8.0F,
                                 spawn_label_a,
                                 2.30F,
                                 1.70F,
                                 spawn_label_box.w - 14.0F,
                                 text_blue);
            draw_text_center_fit(spawn_label_box.x + spawn_label_box.w * 0.5F + 1.0F,
                                 spawn_label_box.y + 27.0F,
                                 spawn_label_b,
                                 2.30F,
                                 1.70F,
                                 spawn_label_box.w - 14.0F,
                                 text_shadow);
            draw_text_center_fit(spawn_label_box.x + spawn_label_box.w * 0.5F,
                                 spawn_label_box.y + 25.0F,
                                 spawn_label_b,
                                 2.30F,
                                 1.70F,
                                 spawn_label_box.w - 14.0F,
                                 text_blue);
        }
        if (s_feedback_frames > 0) {
            if (portrait) {
                const float feedback_y = reward_feedback_y(w, h);
                draw_text_center_fit(s_board_box.x + s_board_box.w * 0.50F + 1.0F,
                                     feedback_y + 2.0F,
                                     s_feedback_text,
                                     2.15F,
                                     1.45F,
                                     s_board_box.w - 92.0F,
                                     text_shadow);
                draw_text_center_fit(s_board_box.x + s_board_box.w * 0.50F,
                                     feedback_y,
                                     s_feedback_text,
                                     2.15F,
                                     1.45F,
                                     s_board_box.w - 92.0F,
                                     text_blue);
            } else {
                const float feedback_y = reward_feedback_y(w, h);
                draw_text_center_fit(s_board_box.x + s_board_box.w * 0.50F + 1.0F,
                                     feedback_y + 2.0F,
                                     s_feedback_text,
                                     2.45F,
                                     1.60F,
                                     s_board_box.w - 120.0F,
                                     text_shadow);
                draw_text_center_fit(s_board_box.x + s_board_box.w * 0.50F,
                                     feedback_y,
                                     s_feedback_text,
                                     2.45F,
                                     1.60F,
                                     s_board_box.w - 120.0F,
                                     text_blue);
            }
        }
        nt_shape_renderer_flush();
    } else {
    rect(0.0F, h * 0.66F, w, h * 0.34F, grass);
    rect(0.0F, h * 0.83F, w, h * 0.17F, (float[4]){0.18F, 0.68F, 0.28F, 1.0F});
    circle(w * 0.12F, h * 0.20F, 36.0F, (float[4]){1.0F, 0.96F, 0.84F, 1.0F});
    circle(w * 0.78F, h * 0.19F, 48.0F, (float[4]){1.0F, 0.96F, 0.84F, 1.0F});

    const float top_h = fmaxf(72.0F, h * 0.13F);
    rect(0.0F, 0.0F, w, top_h, top);
    circle(46.0F, 34.0F, 20.0F, coin);
    rect(76.0F, 18.0F, fminf(180.0F, w * 0.22F), 30.0F, (float[4]){0.16F, 0.36F, 0.70F, 1.0F});
    char hud[32];
    (void)snprintf(hud, sizeof(hud), "%d", g_game_state.wallet_soft);
    draw_text(82.0F, 24.0F, hud, 4.0F, white);
    circle(w * 0.39F - 42.0F, 34.0F, 18.0F, gem);
    rect(w * 0.39F - 10.0F, 18.0F, fminf(150.0F, w * 0.18F), 30.0F, (float[4]){0.16F, 0.36F, 0.70F, 1.0F});
    (void)snprintf(hud, sizeof(hud), "%d/67", g_game_state.collection_discovered_count);
    draw_text(w * 0.39F + 4.0F, 24.0F, hud, 4.0F, white);
    draw_text_center(w * 0.56F, 20.0F, "67 WORLD", 3.8F, white);
    const float progress_w = fminf(220.0F, w * 0.28F);
    rect(w - progress_w - 28.0F, 20.0F, progress_w, 24.0F, (float[4]){0.08F, 0.14F, 0.26F, 1.0F});
    const float progress = fminf(1.0F, (float)g_game_state.collection_discovered_count / 67.0F);
    rect(w - progress_w - 28.0F, 20.0F, progress_w * progress, 24.0F, yellow);
    draw_text(w - progress_w - 20.0F, 26.0F, "WORLD", 2.4F, white);

    rect(s_goal_box.x, s_goal_box.y, s_goal_box.w, s_goal_box.h, (float[4]){1.0F, 0.92F, 0.36F, 1.0F});
    outline(s_goal_box.x, s_goal_box.y, s_goal_box.w, s_goal_box.h, ink);
    draw_text(s_goal_box.x + 14.0F, s_goal_box.y + 10.0F, ftue_prompt(), 3.2F, ink);
    draw_text(s_goal_box.x + 14.0F, s_goal_box.y + 34.0F, game_67_next_goal(&g_game_state), 2.5F, ink);

    rect(s_board_box.x - 8.0F, s_board_box.y - 8.0F, s_board_box.w + 16.0F, s_board_box.h + 16.0F, board_edge);
    rect(s_board_box.x, s_board_box.y, s_board_box.w, s_board_box.h, board);
    draw_text(s_board_box.x + 16.0F, s_board_box.y + 10.0F, "MERGE FIELD", 2.4F, ink);

    const int cols = 4;
    const int rows = 3;
    const float gap = fminf(s_board_box.w, s_board_box.h) * 0.035F;
    const float cell_w = (s_board_box.w - gap * (float)(cols + 1)) / (float)cols;
    const float cell_h = (s_board_box.h - gap * (float)(rows + 1)) / (float)rows;
    const int pair_variant = lowest_pair_variant();
    int slot_i = 0;
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            const UiBox cell = board_slot_box(slot_i);
            const float sx = cell.x;
            const float sy = cell.y;
            rect(sx, sy, cell_w, cell_h, slot);
            if (s_selected_slot == slot_i) {
                outline(sx + 6.0F, sy + 6.0F, cell_w - 12.0F, cell_h - 12.0F, (float[4]){1.0F, 0.42F, 0.05F, 1.0F});
            }
            int variant = variant_for_slot(slot_i);
            if (variant >= 0) {
                if (variant == pair_variant) {
                    outline(sx + 3.0F, sy + 3.0F, cell_w - 6.0F, cell_h - 6.0F, (float[4]){1.0F, 0.88F, 0.10F, 1.0F});
                }
                draw_67_mascot(sx + cell_w * 0.50F, sy + cell_h * 0.52F, fminf(cell_w, cell_h) * 0.72F, variant, false);
            } else {
                circle(sx + cell_w * 0.5F, sy + cell_h * 0.5F, fminf(cell_w, cell_h) * 0.08F, (float[4]){0.86F, 0.72F, 0.46F, 1.0F});
            }
            slot_i++;
        }
    }

    rect(s_collection_box.x, s_collection_box.y, s_collection_box.w, s_collection_box.h, (float[4]){0.05F, 0.30F, 0.60F, 1.0F});
    draw_text(s_collection_box.x + 14.0F, s_collection_box.y + 8.0F, "COLLECTION", 2.4F, white);
    const float card_gap = 8.0F;
    const int visible_start = collection_visible_start();
    const float card_w = (s_collection_box.w - card_gap * (float)(COLLECTION_VISIBLE_SLOTS + 1)) / (float)COLLECTION_VISIBLE_SLOTS;
    for (int visible_i = 0; visible_i < COLLECTION_VISIBLE_SLOTS; visible_i++) {
        const int i = visible_start + visible_i;
        const float x = s_collection_box.x + card_gap + (float)visible_i * (card_w + card_gap);
        const bool unlocked = g_game_state.collection_discovered_count >= i + 1;
        rect(x, s_collection_box.y + 28.0F, card_w, s_collection_box.h - 36.0F, unlocked ? card : (float[4]){0.14F, 0.36F, 0.62F, 1.0F});
        draw_67_mascot(x + card_w * 0.50F, s_collection_box.y + s_collection_box.h * 0.50F, fminf(card_w, s_collection_box.h) * 0.56F, i, !unlocked);
    }

    draw_crate(s_spawn_box, game_67_can_spawn(&g_game_state));
    draw_button(s_upgrade_box, yellow, game_67_can_buy_faster_spawn(&g_game_state) || g_game_state.faster_spawn_bought, 2);
    draw_text_center(s_upgrade_box.x + s_upgrade_box.w * 0.5F, s_upgrade_box.y + s_upgrade_box.h + 5.0F, "SPEED", 2.3F, white);
    if (s_feedback_frames > 0) {
        const float pulse = 1.0F + 0.08F * sinf((float)s_feedback_frames * 0.22F);
        const float bw = fminf(w - 60.0F, 360.0F * pulse);
        const UiBox toast = {(w - bw) * 0.5F, s_spawn_box.y - 44.0F, bw, 38.0F};
        rect(toast.x, toast.y + 5.0F, toast.w, toast.h, (float[4]){0.04F, 0.08F, 0.14F, 0.8F});
        rect(toast.x, toast.y, toast.w, toast.h, (float[4]){1.0F, 0.32F, 0.58F, 1.0F});
        draw_text_center(toast.x + toast.w * 0.5F, toast.y + 11.0F, s_feedback_text, 2.8F, white);
    }
    }

    nt_shape_renderer_flush();
    nt_gfx_end_pass();
    nt_gfx_end_frame();

#if !defined(NT_PLATFORM_WEB)
    if (s_capture_once_requested && !s_capture_once_written && s_assets_bound && g_nt_app.frame >= 4) {
        s_capture_once_written = write_framebuffer_ppm(s_capture_once_path, (int)w, (int)h);
        if (s_capture_once_written) {
            nt_log_info("67 World framebuffer capture written: %s", s_capture_once_path);
        }
        nt_app_quit();
    }

#if NT_DEVAPI_ENABLED
    if (s_pending_capture_path[0] != '\0') {
        char path[sizeof(s_pending_capture_path)];
        (void)snprintf(path, sizeof(path), "%s", s_pending_capture_path);
        s_pending_capture_path[0] = '\0';
        (void)write_framebuffer_ppm(path, (int)w, (int)h);
    }
#endif
#endif

    nt_window_swap_buffers();
}

static void register_ui_devapi(float w, float h) {
#if NT_DEVAPI_ENABLED
    nt_devapi_set_view(w, h, w, h);
    nt_devapi_clear_ui_elements();
    (void)nt_devapi_register_ui_node("root", "", "screen", "67 World", "Playable prototype", 0.0F, 0.0F, w, h, true, true);
    (void)nt_devapi_register_ui_node("world.top", "root", "panel", "Top Bar", "Giggle coins, collection progress, title, and speed state", 0.0F, 0.0F, w, s_board_box.y, true, true);
    (void)nt_devapi_register_ui_node("world.board", "root", "panel", "67 Field", "Tap in-field crates and two matching 67 variants", s_board_box.x, s_board_box.y, s_board_box.w, s_board_box.h, true, true);
    (void)nt_devapi_register_ui_node("world.collection", "root", "panel", "Catalog Drawer", "First 7 of 67 variants", s_collection_box.x, s_collection_box.y, s_collection_box.w, s_collection_box.h, true, true);
    for (int i = 0; i < GAME_67_BOARD_SLOTS; i++) {
        char id[32];
        char label[32];
        (void)snprintf(id, sizeof(id), "world.slot.%02d", i);
        (void)snprintf(label, sizeof(label), "Field 67 %d", i + 1);
        const UiBox slot = board_slot_box(i);
        (void)nt_devapi_register_ui_node(id, "world.board", "slot", label, "Tap occupied matching slots to merge", slot.x, slot.y, slot.w, slot.h, true, variant_for_slot(i) >= 0);
    }
    (void)nt_devapi_register_ui_node("world.spawn", "world.board", "button", "Field Crate", "Tap in-field crate to create the best unlocked 67 from the upgraded box, or clear a full stuck board", s_spawn_box.x, s_spawn_box.y, s_spawn_box.w, s_spawn_box.h, true, game_67_can_spawn(&g_game_state) || game_67_can_recycle_lowest(&g_game_state));
    (void)nt_devapi_register_ui_node("world.merge", "root", "button", "Auto Merge", "Automation shortcut for lowest matching 67 pair", 0.0F, 0.0F, 1.0F, 1.0F, false, game_67_can_merge(&g_game_state));
    (void)nt_devapi_register_ui_node("world.upgrade", "root", "button", "Progress Upgrade", "Buy speed first, then better crates", s_upgrade_box.x, s_upgrade_box.y, s_upgrade_box.w, s_upgrade_box.h, true,
                                     game_67_can_buy_faster_spawn(&g_game_state) || game_67_can_buy_better_crate(&g_game_state) || g_game_state.faster_spawn_bought);
#else
    (void)w;
    (void)h;
#endif
}

static void handle_clicks(void) {
    const nt_pointer_t pointer = g_nt_input.pointers[0];
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        if (contains(s_spawn_box, pointer.x, pointer.y)) {
            if (game_67_spawn(&g_game_state)) {
                s_selected_slot = -1;
                feedback_spawned_variant();
                play_sfx(GAME_AUDIO_CUE_SPAWN);
            } else if (game_67_recycle_lowest(&g_game_state)) {
                s_selected_slot = -1;
                feedback("SLOT CLEARED!");
                play_sfx(GAME_AUDIO_CUE_RECYCLE);
            } else {
                feedback("BOARD FULL!");
                play_sfx(GAME_AUDIO_CUE_BLOCKED);
            }
        } else if (contains(s_board_box, pointer.x, pointer.y)) {
            for (int i = 0; i < GAME_67_BOARD_SLOTS; i++) {
                const UiBox slot = board_slot_box(i);
                if (!contains(slot, pointer.x, pointer.y)) {
                    continue;
                }
                const int variant = variant_for_slot(i);
                if (variant < 0) {
                    s_selected_slot = -1;
                    feedback("TAP BOX!");
                    break;
                }
                if (s_selected_slot >= 0 && s_selected_slot != i && variant_for_slot(s_selected_slot) == variant) {
                    if (game_67_merge_variant(&g_game_state, variant)) {
                        s_selected_slot = -1;
                        feedback("NEW 67!");
                        reward_variant(variant + 1);
                        play_sfx(GAME_AUDIO_CUE_MERGE);
                    }
                } else {
                    s_selected_slot = i;
                    feedback("FIND MATCH!");
                    play_sfx(GAME_AUDIO_CUE_BLOCKED);
                }
                break;
            }
        } else if (contains(s_upgrade_box, pointer.x, pointer.y)) {
            if (game_67_buy_faster_spawn(&g_game_state)) {
                feedback("SPEED UP!");
                play_sfx(GAME_AUDIO_CUE_UPGRADE);
            } else if (game_67_buy_better_crate(&g_game_state)) {
                feedback("BOX LEVEL UP!");
                play_sfx(GAME_AUDIO_CUE_UPGRADE);
            } else {
                feedback("NEED COINS!");
                play_sfx(GAME_AUDIO_CUE_BLOCKED);
            }
        }
    }
    if (nt_input_key_is_pressed(NT_KEY_SPACE)) {
        if (game_67_spawn(&g_game_state)) {
            feedback_spawned_variant();
            play_sfx(GAME_AUDIO_CUE_SPAWN);
        }
    }
    if (nt_input_key_is_pressed(NT_KEY_M)) {
        const int merged_variant = lowest_pair_variant();
        if (game_67_merge_lowest(&g_game_state)) {
            feedback("NEW 67!");
            reward_variant(merged_variant + 1);
            play_sfx(GAME_AUDIO_CUE_MERGE);
        }
    }
    if (nt_input_key_is_pressed(NT_KEY_U)) {
        if (game_67_buy_faster_spawn(&g_game_state)) {
            feedback("SPEED UP!");
            play_sfx(GAME_AUDIO_CUE_UPGRADE);
        } else if (game_67_buy_better_crate(&g_game_state)) {
            feedback("BOX LEVEL UP!");
            play_sfx(GAME_AUDIO_CUE_UPGRADE);
        }
    }
}

#if NT_DEVAPI_ENABLED
static cJSON *state_json(void) {
    cJSON *state = game_state_to_json(&g_game_state);
    cJSON_AddNumberToObject(state, "frame", (double)g_nt_app.frame);
    cJSON_AddBoolToObject(state, "state_dirty", game_state_is_dirty());
    cJSON *world = cJSON_AddObjectToObject(state, "world_67");
    cJSON_AddNumberToObject(world, "board_slots", GAME_67_BOARD_SLOTS);
    cJSON_AddNumberToObject(world, "board_used", game_67_total_on_board(&g_game_state));
    cJSON_AddNumberToObject(world, "passive_income_per_tick", game_67_passive_income_per_tick(&g_game_state));
    cJSON_AddBoolToObject(world, "can_spawn", game_67_can_spawn(&g_game_state));
    cJSON_AddBoolToObject(world, "can_merge", game_67_can_merge(&g_game_state));
    cJSON_AddBoolToObject(world, "can_recycle_lowest", game_67_can_recycle_lowest(&g_game_state));
    cJSON_AddBoolToObject(world, "can_buy_faster_spawn", game_67_can_buy_faster_spawn(&g_game_state));
    cJSON_AddBoolToObject(world, "can_buy_better_crate", game_67_can_buy_better_crate(&g_game_state));
    cJSON_AddStringToObject(world, "faster_spawn_state", game_67_faster_spawn_state(&g_game_state));
    cJSON_AddStringToObject(world, "better_crate_state", game_67_better_crate_state(&g_game_state));
    cJSON_AddNumberToObject(world, "faster_spawn_cost", GAME_67_FASTER_SPAWN_COST);
    cJSON_AddNumberToObject(world, "faster_spawn_cost_remaining", game_67_faster_spawn_cost_remaining(&g_game_state));
    cJSON_AddNumberToObject(world, "better_crate_level", g_game_state.better_crate_level);
    cJSON_AddNumberToObject(world, "better_crate_next_cost", game_67_better_crate_next_cost(&g_game_state));
    cJSON_AddNumberToObject(world, "better_crate_cost_remaining", game_67_better_crate_cost_remaining(&g_game_state));
    cJSON_AddNumberToObject(world, "spawn_variant_index", game_67_spawn_variant_index(&g_game_state));
    char upgrade_title[12];
    char upgrade_value[12];
    progress_upgrade_labels(&g_game_state, upgrade_title, sizeof(upgrade_title), upgrade_value, sizeof(upgrade_value));
    cJSON_AddStringToObject(world, "progress_upgrade_title", upgrade_title);
    cJSON_AddStringToObject(world, "progress_upgrade_value", upgrade_value);
    cJSON_AddStringToObject(world, "spawn_action_label", spawn_action_label(&g_game_state));
    cJSON_AddStringToObject(world, "ftue_step", game_67_ftue_step(&g_game_state));
    cJSON_AddStringToObject(world, "ftue_prompt", game_67_ftue_prompt(&g_game_state));
    cJSON_AddStringToObject(world, "next_goal", game_67_next_goal(&g_game_state));
    int pair_a = -1;
    int pair_b = -1;
    if (first_pair_slots(&pair_a, &pair_b)) {
        cJSON_AddNumberToObject(world, "merge_hint_slot_a", pair_a);
        cJSON_AddNumberToObject(world, "merge_hint_slot_b", pair_b);
    } else {
        cJSON_AddNullToObject(world, "merge_hint_slot_a");
        cJSON_AddNullToObject(world, "merge_hint_slot_b");
    }
    cJSON *variants = cJSON_AddArrayToObject(world, "variants");
    const Game67VariantDef *defs = game_67_variants();
    for (int i = 0; i < GAME_67_VARIANT_COUNT; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", defs[i].id);
        cJSON_AddStringToObject(item, "name", defs[i].name);
        cJSON_AddNumberToObject(item, "order", defs[i].order);
        cJSON_AddNumberToObject(item, "count", game_67_variant_count(&g_game_state, i));
        cJSON_AddBoolToObject(item, "discovered", g_game_state.collection_discovered_count >= defs[i].order);
        cJSON_AddItemToArray(variants, item);
    }
    return state;
}

static bool ep_game_state(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    *result = state_json();
    return true;
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)user;
    (void)error;
    (void)error_cap;
    game_67_reset_playtest(&g_game_state);
    s_reward_frames = 0;
    s_reward_variant = -1;
    s_feedback_frames = 0;
    s_selected_slot = -1;
    *result = state_json();
    return true;
}

static cJSON *audio_status_json(void) {
    const GameAudioStatus status = game_audio_status();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "implemented", status.implemented);
    cJSON_AddBoolToObject(root, "initialized", status.initialized);
    cJSON_AddBoolToObject(root, "device_enabled", status.device_enabled);
    cJSON_AddStringToObject(root, "backend", status.backend ? status.backend : "unknown");
    cJSON_AddNumberToObject(root, "total_play_count", status.total_play_count);
    cJSON *cues = cJSON_AddObjectToObject(root, "cue_play_counts");
    for (int i = 0; i < GAME_AUDIO_CUE_COUNT; ++i) {
        cJSON_AddNumberToObject(cues, game_audio_cue_name((GameAudioCue)i), status.cue_play_count[i]);
    }
    return root;
}

static bool ep_game_audio_status(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    *result = audio_status_json();
    return true;
}

static bool ep_game_action_spawn_67(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)user;
    if (!game_67_spawn(&g_game_state)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "board is full");
        play_sfx(GAME_AUDIO_CUE_BLOCKED);
        return false;
    }
    play_sfx(GAME_AUDIO_CUE_SPAWN);
    *result = state_json();
    return true;
}

static bool ep_game_action_merge_matching_67(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)user;
    const int merged_variant = lowest_pair_variant();
    if (!game_67_merge_lowest(&g_game_state)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "no matching 67 pair");
        play_sfx(GAME_AUDIO_CUE_BLOCKED);
        return false;
    }
    reward_variant(merged_variant + 1);
    feedback("NEW 67!");
    play_sfx(GAME_AUDIO_CUE_MERGE);
    *result = state_json();
    return true;
}

static bool ep_game_action_recycle_lowest(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)user;
    if (!game_67_recycle_lowest(&g_game_state)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "board is not stuck");
        play_sfx(GAME_AUDIO_CUE_BLOCKED);
        return false;
    }
    feedback("SLOT CLEARED!");
    play_sfx(GAME_AUDIO_CUE_RECYCLE);
    *result = state_json();
    return true;
}

static bool ep_game_action_buy_faster_spawn(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)user;
    if (!game_67_buy_faster_spawn(&g_game_state)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "upgrade is locked or too expensive");
        play_sfx(GAME_AUDIO_CUE_BLOCKED);
        return false;
    }
    play_sfx(GAME_AUDIO_CUE_UPGRADE);
    *result = state_json();
    return true;
}

static bool ep_game_action_buy_better_crate(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)user;
    if (!game_67_buy_better_crate(&g_game_state)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "better crate is locked or too expensive");
        play_sfx(GAME_AUDIO_CUE_BLOCKED);
        return false;
    }
    play_sfx(GAME_AUDIO_CUE_UPGRADE);
    *result = state_json();
    return true;
}

static bool ep_game_action_tick_passive(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    const cJSON *frames_json = cJSON_GetObjectItemCaseSensitive(params, "frames");
    if (!cJSON_IsNumber(frames_json)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "frames number is required");
        return false;
    }
    const double raw_frames = frames_json->valuedouble;
    if (raw_frames < 0.0 || raw_frames > 360000.0 || floor(raw_frames) != raw_frames) {
        (void)snprintf(error, (size_t)error_cap, "%s", "frames must be an integer between 0 and 360000");
        return false;
    }
    (void)game_67_tick_passive(&g_game_state, (int)raw_frames);
    *result = state_json();
    return true;
}

static bool ep_game_action_run_one_hour_progression(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    enum {
        target_fps = 60,
        passive_interval_frames = GAME_67_PASSIVE_INTERVAL_FRAMES,
        base_spawn_frames = 90,
        upgraded_spawn_frames = 60,
        max_sim_frames = 60 * 60 * target_fps,
        max_iterations = 200000
    };
    const Game67VariantDef *defs = game_67_variants();
    int frame = 0;
    int next_spawn_frame = 0;
    int next_passive_frame = passive_interval_frames;
    int spawn_frames = base_spawn_frames;
    int actions_spawn = 0;
    int actions_merge = 0;
    int actions_buy_speed = 0;
    int actions_buy_crate = 0;
    int actions_recycle = 0;
    int actions_tick = 0;
    int max_board_used = 0;
    int unlock_frames[GAME_67_VARIANT_COUNT] = {0};
    for (int i = 1; i < GAME_67_VARIANT_COUNT; ++i) {
        unlock_frames[i] = -1;
    }

    int iterations = 0;
    while (frame <= max_sim_frames && g_game_state.collection_discovered_count < GAME_67_VARIANT_COUNT && iterations++ < max_iterations) {
        bool progressed = true;
        while (progressed && iterations++ < max_iterations) {
            progressed = false;
            const int board_used = game_67_total_on_board(&g_game_state);
            if (board_used > max_board_used) {
                max_board_used = board_used;
            }
            if (game_67_can_buy_faster_spawn(&g_game_state)) {
                if (game_67_buy_faster_spawn(&g_game_state)) {
                    spawn_frames = upgraded_spawn_frames;
                    actions_buy_speed++;
                    progressed = true;
                }
            } else if (game_67_can_buy_better_crate(&g_game_state)) {
                if (game_67_buy_better_crate(&g_game_state)) {
                    actions_buy_crate++;
                    progressed = true;
                }
            } else if (game_67_can_merge(&g_game_state)) {
                const int before = g_game_state.collection_discovered_count;
                if (game_67_merge_lowest(&g_game_state)) {
                    actions_merge++;
                    const int after = g_game_state.collection_discovered_count;
                    if (after > before && after >= 1 && after <= GAME_67_VARIANT_COUNT) {
                        unlock_frames[after - 1] = frame;
                    }
                    progressed = true;
                }
            } else if (game_67_can_recycle_lowest(&g_game_state)) {
                if (game_67_recycle_lowest(&g_game_state)) {
                    actions_recycle++;
                    progressed = true;
                }
            }
        }

        if (game_67_can_spawn(&g_game_state) && frame >= next_spawn_frame) {
            if (game_67_spawn(&g_game_state)) {
                actions_spawn++;
                next_spawn_frame = frame + spawn_frames;
                continue;
            }
        }

        int next_frame = next_spawn_frame < next_passive_frame ? next_spawn_frame : next_passive_frame;
        if (next_frame <= frame) {
            next_frame = frame + (spawn_frames < passive_interval_frames ? spawn_frames : passive_interval_frames);
        }
        const int delta = next_frame - frame;
        frame = next_frame;
        if (delta > 0) {
            (void)game_67_tick_passive(&g_game_state, delta);
            actions_tick++;
        }
        while (next_passive_frame <= frame) {
            next_passive_frame += passive_interval_frames;
        }
    }

    const double final_minutes = (double)frame / (double)(target_fps * 60);
    const bool reached_cosmic = g_game_state.collection_discovered_count >= GAME_67_VARIANT_COUNT && g_game_state.count_cosmic_67 > 0;
    const bool in_window = final_minutes >= 55.0 && final_minutes <= 60.0;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "passed", reached_cosmic && in_window && iterations < max_iterations);
    cJSON_AddStringToObject(root, "method", "native C runtime one-hour progression bot using game_67 action functions");
    cJSON_AddNumberToObject(root, "final_frame", frame);
    cJSON_AddNumberToObject(root, "final_minutes", final_minutes);
    cJSON_AddNumberToObject(root, "iterations", iterations);
    cJSON_AddNumberToObject(root, "max_iterations", max_iterations);
    cJSON_AddNumberToObject(root, "collection_discovered_count", g_game_state.collection_discovered_count);
    cJSON_AddNumberToObject(root, "better_crate_level", g_game_state.better_crate_level);
    cJSON_AddNumberToObject(root, "count_cosmic_67", g_game_state.count_cosmic_67);
    cJSON_AddNumberToObject(root, "max_board_used", max_board_used);
    cJSON *actions = cJSON_AddObjectToObject(root, "actions");
    cJSON_AddNumberToObject(actions, "spawn", actions_spawn);
    cJSON_AddNumberToObject(actions, "merge", actions_merge);
    cJSON_AddNumberToObject(actions, "buy_faster_spawn", actions_buy_speed);
    cJSON_AddNumberToObject(actions, "buy_better_crate", actions_buy_crate);
    cJSON_AddNumberToObject(actions, "recycle", actions_recycle);
    cJSON_AddNumberToObject(actions, "tick_passive", actions_tick);
    cJSON *unlocks = cJSON_AddObjectToObject(root, "unlock_times_minutes");
    for (int i = 0; i < GAME_67_VARIANT_COUNT; ++i) {
        if (unlock_frames[i] >= 0) {
            cJSON_AddNumberToObject(unlocks, defs[i].id, (double)unlock_frames[i] / (double)(target_fps * 60));
        }
    }
    cJSON_AddItemToObject(root, "state", state_json());
    *result = root;
    return true;
}

static bool ep_game_capture_framebuffer(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
#ifdef NT_PLATFORM_WEB
    (void)params;
    (void)result;
    (void)error;
    (void)error_cap;
    return false;
#else
    const cJSON *output = cJSON_GetObjectItemCaseSensitive(params, "output");
    if (!cJSON_IsString(output) || output->valuestring[0] == '\0' || strlen(output->valuestring) >= sizeof(s_pending_capture_path)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "valid output path is required");
        return false;
    }
    (void)snprintf(s_pending_capture_path, sizeof(s_pending_capture_path), "%s", output->valuestring);
    *result = cJSON_CreateObject();
    cJSON_AddStringToObject(*result, "output", s_pending_capture_path);
    cJSON_AddBoolToObject(*result, "pending", true);
    return true;
#endif
}

static void register_game_endpoints(void) {
    nt_devapi_register_builtins();
    game_state_register_devapi();
    nt_devapi_register("game.state", ep_game_state, NULL);
    nt_devapi_register("game.audio.status", ep_game_audio_status, NULL);
    nt_devapi_register("game.reset_playtest", ep_game_reset_playtest, NULL);
    nt_devapi_register("game.action.spawn_67", ep_game_action_spawn_67, NULL);
    nt_devapi_register("game.action.merge_matching_67", ep_game_action_merge_matching_67, NULL);
    nt_devapi_register("game.action.recycle_lowest", ep_game_action_recycle_lowest, NULL);
    nt_devapi_register("game.action.buy_faster_spawn", ep_game_action_buy_faster_spawn, NULL);
    nt_devapi_register("game.action.buy_better_crate", ep_game_action_buy_better_crate, NULL);
    nt_devapi_register("game.action.tick_passive", ep_game_action_tick_passive, NULL);
    nt_devapi_register("game.action.run_one_hour_progression", ep_game_action_run_one_hour_progression, NULL);
    nt_devapi_register("game.capture.framebuffer", ep_game_capture_framebuffer, NULL);
}
#endif

static void load_default_save_if_available(void) {
    if (!s_autosave_enabled || s_fresh_state_requested) {
        return;
    }
    char error[128];
    char *data = NULL;
    if (!game_storage_load_json(DEFAULT_SAVE_KEY, GAME_STATE_DOCUMENT, &data, error, (int)sizeof(error))) {
        return;
    }
    if (!game_state_load_json_string(&g_game_state, data, error, (int)sizeof(error))) {
        (void)fprintf(stderr, "Autosave load failed: %s\n", error);
    }
    free(data);
}

static void autosave_if_dirty(void) {
    if (!s_autosave_enabled || !game_state_is_dirty()) {
        return;
    }
    char error[128];
    char *data = game_state_save_json_string(&g_game_state, error, (int)sizeof(error));
    if (!data) {
        (void)fprintf(stderr, "Autosave failed: %s\n", error);
        return;
    }
    if (game_storage_save_json(DEFAULT_SAVE_KEY, GAME_STATE_DOCUMENT, data, error, (int)sizeof(error))) {
        game_state_clear_dirty();
    } else {
        (void)fprintf(stderr, "Autosave failed: %s\n", error);
    }
    cJSON_free(data);
}

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    fclose(file);
    return true;
}

static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--devapi") == 0 && i + 1 < argc) {
            s_devapi_enabled = true;
            s_devapi_port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            const char *size_arg = argv[++i];
            char *end = NULL;
            long parsed_w = strtol(size_arg, &end, 10);
            if (end && (*end == 'x' || *end == 'X')) {
                char *height_end = NULL;
                long parsed_h = strtol(end + 1, &height_end, 10);
                if (height_end && *height_end == '\0' && parsed_w >= 320 && parsed_h >= 320 && parsed_w <= 4096 && parsed_h <= 4096) {
                    s_window_width = (int)parsed_w;
                    s_window_height = (int)parsed_h;
                }
            }
        } else if (strcmp(argv[i], "--fresh-state") == 0) {
            s_fresh_state_requested = true;
        } else if (strcmp(argv[i], "--disable-autosave") == 0) {
            s_autosave_enabled = false;
        } else if (strcmp(argv[i], "--capture-framebuffer-once") == 0 && i + 1 < argc) {
            const char *path = argv[++i];
            if (path[0] != '\0' && strlen(path) < sizeof(s_capture_once_path)) {
                s_capture_once_requested = true;
                (void)snprintf(s_capture_once_path, sizeof(s_capture_once_path), "%s", path);
            }
        }
    }
}

static void frame(void) {
    nt_window_poll();
    nt_input_poll();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_set_frame((uint64_t)g_nt_app.frame);
        nt_devapi_net_poll();
        nt_devapi_apply_pending();
    }
#endif

    nt_resource_step();
    nt_material_step();
    try_bind_art_assets();

    const float w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    layout(w, h);
    handle_clicks();
    game_audio_update();
    (void)game_67_tick_passive(&g_game_state, 1);
    if (s_feedback_frames > 0) {
        s_feedback_frames--;
    }
    if (s_reward_frames > 0) {
        s_reward_frames--;
    }
    register_ui_devapi(w, h);
    draw_game_screen(w, h);
    autosave_if_dirty();

#ifndef NT_PLATFORM_WEB
    if (s_capture_once_requested && !s_capture_once_written && !s_assets_bound && g_nt_app.frame > 240) {
        (void)fprintf(stderr, "Timed out waiting for art assets before framebuffer capture: %s\n", s_capture_once_path);
        nt_app_quit();
    }
#endif

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "67 World";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    game_state_init();
    parse_args(argc, argv);
    load_default_save_if_available();
    game_audio_init();
    game_audio_set_device_enabled(!s_devapi_enabled);
    game_audio_set_volume(g_game_state.settings_master_volume, g_game_state.settings_sfx_volume);

    g_nt_window.title = "67 World";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();
    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    nt_gfx_init(&gfx_desc);
    nt_gfx_register_global_block("Globals", 0);
    nt_shape_renderer_init();
    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture);
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);
    nt_atlas_init();
    nt_material_init(&(nt_material_desc_t){.max_materials = 4});
    nt_sprite_renderer_init(&(nt_sprite_renderer_desc_t){.max_pipelines = 8});

    s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "frame_uniforms",
    });

    s_pack_id = nt_hash32_str("world67_art");
    nt_resource_mount(s_pack_id, 100);
#ifdef NT_PLATFORM_WEB
    nt_resource_load_auto(s_pack_id, "assets/world67_art.ntpack");
#else
    const char *art_pack_path = file_exists("assets/world67_art.ntpack")
                                    ? "assets/world67_art.ntpack"
                                    : "build/game_seed/67-world-packs/world67_art.ntpack";
    nt_resource_load_auto(s_pack_id, art_pack_path);
#endif

    s_sprite_vs_handle = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_VERT, NT_ASSET_SHADER_CODE);
    s_sprite_fs_handle = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_FRAG, NT_ASSET_SHADER_CODE);
    s_atlas_handle = nt_resource_request(ASSET_ATLAS_WORLD67_ART_ATLAS, NT_ASSET_ATLAS);
    s_atlas_tex_handle = nt_resource_request(ASSET_TEXTURE_WORLD67_ART_ATLAS_TEX0, NT_ASSET_TEXTURE);
    s_sprite_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_sprite_vs_handle,
        .fs = s_sprite_fs_handle,
        .textures = {{.name = "u_texture", .resource = s_atlas_tex_handle}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "world67_sprite",
    });
    nt_resource_set_activate_time_budget(0);

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_init();
        register_game_endpoints();
        if (!nt_devapi_net_start(s_devapi_port)) {
            (void)fprintf(stderr, "Failed to start DevAPI on port %u\n", (unsigned)s_devapi_port);
        }
    }
#endif

#ifdef NT_PLATFORM_WEB
    nt_platform_web_loading_complete();
#endif

    g_nt_app.target_dt = 1.0F / 60.0F;
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_net_stop();
        nt_devapi_shutdown();
    }
#endif
    game_audio_shutdown();
    nt_shape_renderer_shutdown();
    nt_sprite_renderer_shutdown();
    nt_material_destroy(s_sprite_material);
    nt_material_shutdown();
    nt_resource_shutdown();
    nt_fs_shutdown();
    nt_http_shutdown();
    nt_hash_shutdown();
    nt_gfx_destroy_buffer(s_frame_ubo);
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif

    return (s_capture_once_requested && !s_capture_once_written) ? 2 : 0;
}
