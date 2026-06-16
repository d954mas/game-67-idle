/*
 * Voxelheim -- First static screen ("Frost Keep Approach").
 *
 * A readable, bright "Roblox" casual-RPG scene assembled from real sprites in
 * the voxelheim atlas + a slug-text HUD. No gameplay yet: this is the static
 * first-screen visual slice (art direction proof).
 *
 * Render path: nt_atlas + nt_sprite_renderer (direct emit) for the world and
 * HUD, nt_text_renderer for labels. Mirrors clean_seed_main.c's
 * window/init/loop/devapi scaffold.
 *
 * Coordinate convention: bottom-left = (0,0), Y up (ortho VP matches).
 *
 * Pack build (explicit, like bunnymark) -- run from the project root:
 *   cmake --build build/_cmake/native-debug --target build_voxelheim_packs
 *   build/voxelheim_packer/build_voxelheim_packs.exe build/voxelheim
 *   copy build/voxelheim/voxelheim.ntpack -> assets/voxelheim.ntpack
 * This emits assets/voxelheim.ntpack + src/generated/voxelheim_assets.h.
 * Build game_seed afterwards; it loads assets/voxelheim.ntpack at runtime.
 */

#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "material/nt_material.h"
#include "math/nt_math.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_sprite_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "time/nt_time.h"
#include "window/nt_window.h"

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#include "voxelheim_assets.h"

#include <glad/gl.h>

#include "stb_image_write.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOXELHEIM_DEVAPI_PORT_DEFAULT 9123

/* Design canvas the composition is authored against (matches the fake shot
 * aspect). Everything is laid out in these units, then scaled uniformly to the
 * real framebuffer so the scene fills the window regardless of size. */
#define DESIGN_W 960.0F
#define DESIGN_H 540.0F

/* ---- Region name hashes, indexed by enum ---- */

enum {
    R_SNOW = 0,
    R_PATH,
    R_KEEP,
    R_PINE,
    R_ROCK,
    R_HERO,
    R_ENEMY,
    R_HP,
    R_STAMINA,
    R_BADGE,
    R_MINIMAP,
    R_SLOT,
    R_BANNER,
    R_BUTTON,
    R_COUNT,
};

static const nt_hash64_t k_region_names[R_COUNT] = {
    ASSET_ATLAS_REGION_VOXELS_SNOW_TILE_PNG,   ASSET_ATLAS_REGION_VOXELS_PATH_TILE_PNG, ASSET_ATLAS_REGION_VOXELS_KEEP_PNG,
    ASSET_ATLAS_REGION_VOXELS_PINE_PNG,        ASSET_ATLAS_REGION_VOXELS_ROCK_PNG,      ASSET_ATLAS_REGION_VOXELS_HERO_PNG,
    ASSET_ATLAS_REGION_VOXELS_ENEMY_PNG,       ASSET_ATLAS_REGION_VOXELS_HP_BAR_PNG,    ASSET_ATLAS_REGION_VOXELS_STAMINA_BAR_PNG,
    ASSET_ATLAS_REGION_VOXELS_LEVEL_BADGE_PNG, ASSET_ATLAS_REGION_VOXELS_MINIMAP_PNG,   ASSET_ATLAS_REGION_VOXELS_ITEM_SLOT_PNG,
    ASSET_ATLAS_REGION_VOXELS_BANNER_PNG,      ASSET_ATLAS_REGION_VOXELS_BUTTON_PNG,
};

/* ---- State ---- */

static bool s_devapi_enabled;
static uint16_t s_devapi_port = VOXELHEIM_DEVAPI_PORT_DEFAULT;
static int s_window_width = 960;
static int s_window_height = 540;

static nt_buffer_t s_frame_ubo;
static nt_hash32_t s_pack_id;
static nt_resource_t s_atlas;
static nt_material_t s_sprite_mat;
static nt_material_t s_text_mat;
static nt_font_t s_font;

static uint32_t s_region_idx[R_COUNT];
static uint16_t s_region_w[R_COUNT];
static uint16_t s_region_h[R_COUNT];
static bool s_atlas_resolved;

/* Pending engine-side screenshot. OS window-grab can't read this GL window on
 * headless/RDP sessions, so the game reads its own backbuffer via glReadPixels
 * and writes a PNG. A DevAPI endpoint (or --shot <path>) requests one. */
static char s_shot_path[512];
static bool s_shot_pending;
static bool s_shot_done;
static bool s_shot_ok;

/* ---- Helpers ---- */

static uint32_t canvas_w(void) { return g_nt_window.fb_width > 0 ? g_nt_window.fb_width : (uint32_t)s_window_width; }
static uint32_t canvas_h(void) { return g_nt_window.fb_height > 0 ? g_nt_window.fb_height : (uint32_t)s_window_height; }

static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--devapi") == 0) {
            s_devapi_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                s_devapi_port = (uint16_t)strtoul(argv[++i], NULL, 10);
            }
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            int w = 0;
            int h = 0;
            if (sscanf(argv[++i], "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
                s_window_width = w;
                s_window_height = h;
            }
        }
        /* --fresh-state / --disable-autosave from the launcher: no persistent
         * gameplay state in this static slice, so they are accepted no-ops. */
    }
}

/* Resolve the 14 atlas regions once the atlas is ready. Caches region index +
 * source pixel dimensions so the scene can size sprites in design units. */
static void resolve_regions(void) {
    if (s_atlas_resolved || !nt_resource_is_ready(s_atlas)) {
        return;
    }
    for (int i = 0; i < R_COUNT; ++i) {
        uint32_t idx = nt_atlas_find_region(s_atlas, k_region_names[i].value);
        if (idx == NT_ATLAS_INVALID_REGION) {
            return; /* atlas not fully merged yet -- retry next frame */
        }
        const nt_texture_region_t *reg = nt_atlas_get_region(s_atlas, idx);
        s_region_idx[i] = idx;
        s_region_w[i] = reg->source_w;
        s_region_h[i] = reg->source_h;
    }
    s_atlas_resolved = true;
}

/* Build a column-major mat4 that scales a unit-centered quad to (w,h) design
 * units and translates its center to (cx,cy). The sprite is emitted with pivot
 * (0.5,0.5); emit applies the region's source size internally, so we pre-divide
 * by the region's source pixels to get an exact target footprint. */
static void emit_sprite(int region, float cx, float cy, float w, float h, uint32_t color) {
    const float sx = (s_region_w[region] > 0) ? (w / (float)s_region_w[region]) : 1.0F;
    const float sy = (s_region_h[region] > 0) ? (h / (float)s_region_h[region]) : 1.0F;
    mat4 m;
    glm_mat4_identity(m);
    m[0][0] = sx;
    m[1][1] = sy;
    m[3][0] = cx;
    m[3][1] = cy;
    nt_sprite_renderer_emit_region(s_atlas, s_region_idx[region], (const float *)m, 0.5F, 0.5F, color, 0);
}

/* Emit a sprite at design-unit height, preserving its source aspect ratio. */
static void emit_h(int region, float cx, float cy, float design_h, uint32_t color) {
    const float aspect = (s_region_h[region] > 0) ? ((float)s_region_w[region] / (float)s_region_h[region]) : 1.0F;
    emit_sprite(region, cx, cy, design_h * aspect, design_h, color);
}

static void emit_text(const char *utf8, float x, float y, float size, const float color[4]) {
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){x, y, 0.0F});
    nt_text_renderer_set_material(s_text_mat);
    nt_text_renderer_set_font(s_font);
    nt_text_renderer_draw(utf8, (const float *)model, size, color, 0.0F, 0.0F);
    nt_text_renderer_flush();
}

/* Width of one text line in design units (so labels can be centered). */
static float text_width(const char *s, float size) {
    /* Roboto average advance ~0.52em; good enough for centering HUD labels. */
    return (float)strlen(s) * size * 0.52F;
}

/* Read the default framebuffer and write it to s_shot_path as PNG. Must be
 * called while the backbuffer is the current draw target (after end_pass,
 * before swap). glReadPixels gives bottom-up rows; we flip to top-down PNG. */
static bool write_backbuffer_png(const char *path) {
    const int w = (int)canvas_w();
    const int h = (int)canvas_h();
    if (w <= 0 || h <= 0) {
        return false;
    }
    unsigned char *buf = (unsigned char *)malloc((size_t)w * (size_t)h * 4u);
    if (!buf) {
        return false;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);

    /* Flip vertically into a second buffer for top-down PNG row order. */
    unsigned char *flip = (unsigned char *)malloc((size_t)w * (size_t)h * 4u);
    if (!flip) {
        free(buf);
        return false;
    }
    const size_t stride = (size_t)w * 4u;
    for (int y = 0; y < h; ++y) {
        memcpy(flip + (size_t)y * stride, buf + (size_t)(h - 1 - y) * stride, stride);
    }
    free(buf);

    const int ok = stbi_write_png(path, w, h, 4, flip, (int)stride);
    free(flip);
    return ok != 0;
}

/* ---- Scene composition (design units, y-up, bottom-left origin) ---- */

static void compose_scene(void) {
    const uint32_t white = 0xFFFFFFFFu;

    // #region background: tiled snow filling the whole canvas
    /* Overlap tiles generously so seams read as gentle snow variation rather
     * than a hard checker grid. */
    const float snow_tile = 150.0F;     /* on-screen tile footprint in design units */
    const float snow_step = snow_tile * 0.84F;
    for (float y = -snow_tile; y < DESIGN_H + snow_tile; y += snow_step) {
        for (float x = -snow_tile; x < DESIGN_W + snow_tile; x += snow_step) {
            emit_sprite(R_SNOW, x, y, snow_tile, snow_tile, white);
        }
    }
    // #endregion

    // #region stone path: a clear continuous road from bottom-center to the keep
    const float path_cx = DESIGN_W * 0.5F;
    const float keep_cy = DESIGN_H * 0.74F;
    const float path_bottom = -20.0F;
    const float path_top = keep_cy - DESIGN_H * 0.10F;
    const float path_tile = 96.0F;
    for (float y = path_bottom; y < path_top; y += path_tile * 0.5F) {
        /* Road narrows with distance (simple 3/4 depth cue) and stays wide
         * enough to read clearly under the hero. */
        const float t = (y - path_bottom) / (path_top - path_bottom);
        const float pw = path_tile * (2.05F - 0.95F * t);
        emit_sprite(R_PATH, path_cx, y, pw, path_tile * 0.72F, white);
    }
    // #endregion

    // #region keep: the goal beacon near top-center
    emit_h(R_KEEP, path_cx, keep_cy, DESIGN_H * 0.40F, white);
    // #endregion

    // #region scenery: pines + rocks scattered on the sides (back -> front)
    emit_h(R_PINE, DESIGN_W * 0.14F, DESIGN_H * 0.70F, DESIGN_H * 0.26F, white);
    emit_h(R_PINE, DESIGN_W * 0.86F, DESIGN_H * 0.71F, DESIGN_H * 0.25F, white);
    emit_h(R_PINE, DESIGN_W * 0.07F, DESIGN_H * 0.46F, DESIGN_H * 0.34F, white);
    emit_h(R_PINE, DESIGN_W * 0.93F, DESIGN_H * 0.45F, DESIGN_H * 0.35F, white);
    emit_h(R_ROCK, DESIGN_W * 0.22F, DESIGN_H * 0.40F, DESIGN_H * 0.13F, white);
    emit_h(R_ROCK, DESIGN_W * 0.78F, DESIGN_H * 0.38F, DESIGN_H * 0.15F, white);
    emit_h(R_PINE, DESIGN_W * 0.16F, DESIGN_H * 0.20F, DESIGN_H * 0.40F, white);
    emit_h(R_PINE, DESIGN_W * 0.85F, DESIGN_H * 0.19F, DESIGN_H * 0.42F, white);
    emit_h(R_ROCK, DESIGN_W * 0.30F, DESIGN_H * 0.12F, DESIGN_H * 0.16F, white);
    // #endregion

    // #region actors: enemy up the path (toward the keep), hero on the path
    emit_h(R_ENEMY, DESIGN_W * 0.585F, DESIGN_H * 0.55F, DESIGN_H * 0.20F, white);
    emit_h(R_HERO, path_cx, DESIGN_H * 0.30F, DESIGN_H * 0.38F, white);
    // #endregion

    nt_sprite_renderer_flush();
}

static void compose_hud(void) {
    const uint32_t white = 0xFFFFFFFFu;

    // #region top-left: hp bar, stamina bar, level badge
    const float bar_h = 30.0F;
    const float bar_left = 22.0F;
    const float bar_aspect = (s_region_h[R_HP] > 0) ? ((float)s_region_w[R_HP] / (float)s_region_h[R_HP]) : 4.0F;
    const float bar_w = bar_h * bar_aspect;
    emit_sprite(R_HP, bar_left + bar_w * 0.5F + 54.0F, DESIGN_H - 34.0F, bar_w, bar_h, white);
    emit_sprite(R_STAMINA, bar_left + bar_w * 0.5F + 54.0F, DESIGN_H - 34.0F - bar_h - 6.0F, bar_w, bar_h, white);
    emit_h(R_BADGE, bar_left + 28.0F, DESIGN_H - 48.0F, 70.0F, white);
    // #endregion

    // #region top-right: minimap
    emit_h(R_MINIMAP, DESIGN_W - 78.0F, DESIGN_H - 78.0F, 120.0F, white);
    // #endregion

    // #region bottom-center: 5-slot hotbar
    const float slot = 58.0F;
    const float gap = 10.0F;
    const int slots = 5;
    const float row_w = slots * slot + (slots - 1) * gap;
    const float row_x0 = (DESIGN_W - row_w) * 0.5F + slot * 0.5F;
    for (int i = 0; i < slots; ++i) {
        emit_sprite(R_SLOT, row_x0 + (float)i * (slot + gap), 44.0F, slot, slot, white);
    }
    // #endregion

    // #region banner under the minimap (top-right) carrying the objective
    emit_h(R_BANNER, DESIGN_W - 150.0F, DESIGN_H - 164.0F, 86.0F, white);
    // #endregion

    nt_sprite_renderer_flush();
}

static void compose_text(void) {
    const float ink[4] = {0.149F, 0.125F, 0.110F, 1.0F};  /* outline near-black brown */
    const float gold[4] = {1.0F, 0.784F, 0.239F, 1.0F};   /* #FFC83D */
    const float cream[4] = {0.996F, 0.980F, 0.937F, 1.0F};

    /* Game title: clear snow band just below the top-left HUD, drawn with a
     * dark drop shadow so the gold reads against the bright snow. */
    {
        const char *title = "VOXELHEIM";
        const float size = 34.0F;
        const float tx = 24.0F;
        const float ty = DESIGN_H - 132.0F;
        emit_text(title, tx + 2.0F, ty - 2.0F, size, ink);
        emit_text(title, tx, ty, size, gold);
    }

    /* Level label centered on the level badge (top-left corner). */
    {
        const char *lvl = "Lv 1";
        const float size = 18.0F;
        const float w = text_width(lvl, size);
        emit_text(lvl, 50.0F - w * 0.5F + 2.0F, DESIGN_H - 56.0F - 1.0F, size, ink);
        emit_text(lvl, 50.0F - w * 0.5F, DESIGN_H - 56.0F, size, cream);
    }

    /* Objective text centered on the red banner under the top-right minimap. */
    {
        const char *quest = "Reach the Frost Keep";
        const float size = 17.0F;
        const float bx = DESIGN_W - 150.0F;
        const float by = DESIGN_H - 170.0F;
        const float w = text_width(quest, size);
        emit_text(quest, bx - w * 0.5F, by, size, cream);
    }
}

/* ---- DevAPI ---- */

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static cJSON *state_json(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "runtime", "voxelheim");
    cJSON_AddStringToObject(root, "screen", "frost_keep_approach");
    cJSON_AddBoolToObject(root, "atlas_ready", s_atlas_resolved);
    return root;
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
    (void)error;
    (void)error_cap;
    (void)user;
    *result = state_json();
    return true;
}

/* Request an engine-side backbuffer screenshot. params: {"path": "..."}.
 * The handler runs on the main thread inside net_poll (before the frame
 * renders), so it cannot block waiting for the capture -- it flags the request
 * and returns immediately. The frame loop writes the file after end_pass and
 * sets s_shot_done/s_shot_ok, which the next frame.screenshot call reports.
 * Callers poll the returned "done" flag (or just wait + read the file). */
static bool ep_frame_screenshot(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    cJSON *root = cJSON_CreateObject();
    if (s_shot_pending) {
        /* A capture is already in flight -- just report status. */
        cJSON_AddBoolToObject(root, "queued", true);
        cJSON_AddBoolToObject(root, "done", s_shot_done);
        cJSON_AddBoolToObject(root, "ok", s_shot_ok);
        cJSON_AddStringToObject(root, "path", s_shot_path);
        *result = root;
        return true;
    }
    if (s_shot_done) {
        /* Report the previous completed capture. */
        cJSON_AddBoolToObject(root, "queued", false);
        cJSON_AddBoolToObject(root, "done", true);
        cJSON_AddBoolToObject(root, "ok", s_shot_ok);
        cJSON_AddStringToObject(root, "path", s_shot_path);
        *result = root;
        return true;
    }
    const cJSON *p = params ? cJSON_GetObjectItemCaseSensitive(params, "path") : NULL;
    const char *path = (p && cJSON_IsString(p)) ? p->valuestring : "build/captures/voxelheim_p1.png";
    (void)snprintf(s_shot_path, sizeof(s_shot_path), "%s", path);
    s_shot_done = false;
    s_shot_ok = false;
    s_shot_pending = true;
    cJSON_AddBoolToObject(root, "queued", true);
    cJSON_AddBoolToObject(root, "done", false);
    cJSON_AddStringToObject(root, "path", s_shot_path);
    *result = root;
    return true;
}

static void register_game_endpoints(void) {
    nt_devapi_register_builtins();
    game_state_register_devapi();
    nt_devapi_register("game.state", ep_game_state, NULL);
    nt_devapi_register("game.reset_playtest", ep_game_reset_playtest, NULL);
    nt_devapi_register("frame.screenshot", ep_frame_screenshot, NULL);
}

static void register_ui_devapi(float w, float h) {
    nt_devapi_set_frame(g_nt_app.frame);
    nt_devapi_set_view((float)canvas_w(), (float)canvas_h(), w, h);
    nt_devapi_clear_ui_elements();
    (void)nt_devapi_register_ui_node("root", "", "screen", "Voxelheim", "Frost Keep Approach", 0.0F, 0.0F, w, h, true, true);
}
#endif

/* ---- Frame ---- */

static void frame(void) {
    nt_window_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_net_poll();
    }
#endif
    nt_input_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_apply_pending();
    }
#endif

    nt_resource_step();
    nt_material_step();
    resolve_regions();

    const float w = (float)canvas_w();
    const float h = (float)canvas_h();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        register_ui_devapi(DESIGN_W, DESIGN_H);
    }
#endif

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif

    // #region frame uniforms: ortho over the DESIGN canvas (auto-fits the window)
    mat4 proj;
    mat4 view;
    mat4 vp;
    glm_mat4_identity(view);
    glm_ortho(0.0F, DESIGN_W, 0.0F, DESIGN_H, -1.0F, 1.0F, proj);
    glm_mat4_mul(proj, view, vp);

    nt_frame_uniforms_t u = {0};
    memcpy(u.view_proj, vp, 64);
    memcpy(u.view, view, 64);
    memcpy(u.proj, proj, 64);
    u.resolution[0] = w;
    u.resolution[1] = h;
    u.resolution[2] = (w > 0.0F) ? 1.0F / w : 0.0F;
    u.resolution[3] = (h > 0.0F) ? 1.0F / h : 0.0F;
    u.near_far[0] = -1.0F;
    u.near_far[1] = 1.0F;
    // #endregion

    const nt_material_info_t *sprite_info = nt_material_get_info(s_sprite_mat);
    const nt_material_info_t *text_info = nt_material_get_info(s_text_mat);
    const bool can_render = s_atlas_resolved && sprite_info && sprite_info->ready;

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        nt_resource_invalidate(NT_ASSET_SHADER_CODE);
        nt_resource_invalidate(NT_ASSET_TEXTURE);
        nt_resource_invalidate(NT_ASSET_FONT);
        nt_gfx_destroy_buffer(s_frame_ubo);
        s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
            .type = NT_BUFFER_UNIFORM,
            .usage = NT_USAGE_DYNAMIC,
            .size = sizeof(nt_frame_uniforms_t),
            .label = "frame_uniforms",
        });
        nt_sprite_renderer_restore_gpu();
        nt_text_renderer_restore_gpu();
    }

    /* Sky-blue clear (art bible #3FB7FF). */
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.247F, 0.717F, 1.0F, 1.0F}, .clear_depth = 1.0F});
    nt_font_step();

    if (can_render && !g_nt_gfx.context_restored) {
        nt_gfx_update_buffer(s_frame_ubo, &u, sizeof(u));
        nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);

        nt_sprite_renderer_set_material(s_sprite_mat);
        compose_scene();
        nt_sprite_renderer_set_material(s_sprite_mat);
        compose_hud();

        if (text_info && text_info->ready) {
            compose_text();
        }
    }

    nt_gfx_end_pass();

    /* Engine-side screenshot: read the just-rendered backbuffer before swap.
     * Only fire once the scene is actually drawn so the PNG is never blank. */
    if (s_shot_pending && can_render && !g_nt_gfx.context_restored) {
        s_shot_ok = write_backbuffer_png(s_shot_path);
        s_shot_done = true;
        s_shot_pending = false;
    }

    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

/* ---- Main ---- */

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Voxelheim";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    parse_args(argc, argv);

    g_nt_window.title = "Voxelheim - Frost Keep Approach";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = false;
    nt_gfx_init(&gfx_desc);
    nt_gfx_register_global_block("Globals", 0);

    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture);
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);
    nt_atlas_init();

    nt_material_init(&(nt_material_desc_t){.max_materials = 4});
    nt_font_init(&(nt_font_desc_t){.max_fonts = 2});

    nt_sprite_renderer_desc_t sr_desc = nt_sprite_renderer_desc_defaults();
    nt_sprite_renderer_init(&sr_desc);
    nt_text_renderer_init();

    g_nt_app.target_dt = 1.0F / 60.0F;

    s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "frame_uniforms",
    });

    s_pack_id = nt_hash32_str("voxelheim");
    nt_resource_mount(s_pack_id, 100);
    nt_resource_load_auto(s_pack_id, "assets/voxelheim.ntpack");

    nt_resource_t vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_VERT, NT_ASSET_SHADER_CODE);
    nt_resource_t fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_FRAG, NT_ASSET_SHADER_CODE);
    s_atlas = nt_resource_request(ASSET_ATLAS_VOXELS, NT_ASSET_ATLAS);
    nt_resource_t atlas_tex = nt_resource_request(ASSET_TEXTURE_VOXELS_TEX0, NT_ASSET_TEXTURE);

    s_sprite_mat = nt_material_create(&(nt_material_create_desc_t){
        .vs = vs,
        .fs = fs,
        .textures = {{.name = "u_texture", .resource = atlas_tex}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "voxelheim_sprite",
    });

    nt_resource_t slug_vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_VERT, NT_ASSET_SHADER_CODE);
    nt_resource_t slug_fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_FRAG, NT_ASSET_SHADER_CODE);
    s_text_mat = nt_material_create(&(nt_material_create_desc_t){
        .vs = slug_vs,
        .fs = slug_fs,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .params[0] = {.name = "u_alpha_cutoff", .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
        .param_count = 1,
        .label = "voxelheim_text",
    });
    s_font = nt_font_create(&(nt_font_create_desc_t){
        .curve_texture_width = 1024,
        .curve_texture_height = 512,
        .band_texture_height = 256,
        .band_count = 8,
    });
    nt_font_add(s_font, nt_resource_request(ASSET_FONT_VOXELHEIM_FONT_HUD, NT_ASSET_FONT));

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

    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_net_stop();
        nt_devapi_shutdown();
    }
#endif
    nt_text_renderer_shutdown();
    nt_font_destroy(s_font);
    nt_font_shutdown();
    nt_sprite_renderer_shutdown();
    nt_material_destroy(s_sprite_mat);
    nt_material_destroy(s_text_mat);
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
    return 0;
}
