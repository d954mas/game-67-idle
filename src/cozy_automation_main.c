/*
 * Cozy Automation -- "The Little Garden" first playable slice.
 *
 * One cozy garden: a pre-planted Berry Bush auto-generates berries that
 * auto-route into a basket (rising counter + drifting-berry feedback). Spend 10
 * berries to plant a second bush (primary action); the third plot is a LOCKED
 * greenhouse that unlocks at 50 berries (the one lock) and jumps the rate.
 * Active-session automation -- no idle/away-time/reset-meta loop.
 *
 * Render path: nt_atlas + nt_sprite_renderer (direct emit) for the garden + HUD,
 * nt_text_renderer for labels. Runtime pack: assets/cozy_automation.ntpack
 * (built by src/build_packs.c). Coordinate convention: bottom-left = (0,0), Y up.
 */

#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "devapi/nt_devapi_net.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "game_devapi_ui.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "material/nt_material.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_sprite_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "window/nt_window.h"

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#include "cozy_automation_assets.h"

#ifndef NT_PLATFORM_WEB
#include <glad/gl.h>
#include "stb_image_write.h"
#endif

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COZY_DEVAPI_PORT_DEFAULT 9123

/* Design canvas the composition is authored against; scaled uniformly to the
 * framebuffer. Keep the window at this size so input/ui.click map 1:1. */
#define DESIGN_W 960.0F
#define DESIGN_H 540.0F

/* Economy (also documented in gamedesign/.../data/core_loop.json). */
#define PLANT_COST 10
#define GREENHOUSE_COST 50
#define BERRIES_MAX 999999
#define TICK_INTERVAL 1.5F /* seconds between auto-production ticks */
#define FLY_DURATION 0.9F  /* seconds for an auto-routed berry to reach basket */

/* ---- Region enum -> atlas region name hashes (codegen'd macros) ---- */

enum {
    R_BG = 0,
    R_PLOT,
    R_BUSH,
    R_GREENHOUSE,
    R_BASKET,
    R_BERRY,
    R_LOCK,
    R_PANEL,
    R_COUNT,
};

static const nt_hash64_t k_region_names[R_COUNT] = {
    ASSET_ATLAS_REGION_GARDEN_BG_PNG,    ASSET_ATLAS_REGION_GARDEN_PLOT_PNG,
    ASSET_ATLAS_REGION_GARDEN_BUSH_PNG,  ASSET_ATLAS_REGION_GARDEN_GREENHOUSE_PNG,
    ASSET_ATLAS_REGION_GARDEN_BASKET_PNG, ASSET_ATLAS_REGION_GARDEN_BERRY_PNG,
    ASSET_ATLAS_REGION_GARDEN_LOCK_PNG,  ASSET_ATLAS_REGION_GARDEN_PANEL_PNG,
};

/* ---- Layout (design units, y-up) ---- */

#define PLOT_Y 196.0F
#define PLOT1_X 268.0F
#define PLOT2_X 480.0F
#define PLOT3_X 692.0F
#define PLOT_FOOT 156.0F
#define BASKET_X 372.0F
#define BASKET_Y 486.0F

typedef struct {
    float x, y, w, h; /* bottom-left + size, y-up */
} Box;

/* ---- State ---- */

static bool s_devapi_enabled;
static uint16_t s_devapi_port = COZY_DEVAPI_PORT_DEFAULT;
static int s_window_width = (int)DESIGN_W;
static int s_window_height = (int)DESIGN_H;

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

static float s_tick_accum;
static Box s_plant_box; /* primary CTA: plant bush on plot 2 */
static Box s_gh_box;    /* the lock: unlock greenhouse on plot 3 */

/* Auto-route feedback: berries drifting from a producing plot to the basket. */
#define FLY_MAX 32
typedef struct {
    bool active;
    float t;
    float sx, sy;
} Fly;
static Fly s_flies[FLY_MAX];

/* Engine-side screenshot (robust on headless/RDP: reads our own backbuffer). */
static char s_shot_path[512];
static bool s_shot_pending;
static bool s_shot_done;
static bool s_shot_ok;

/* ---- Helpers ---- */

static uint32_t canvas_w(void) { return g_nt_window.fb_width > 0 ? g_nt_window.fb_width : (uint32_t)s_window_width; }
static uint32_t canvas_h(void) { return g_nt_window.fb_height > 0 ? g_nt_window.fb_height : (uint32_t)s_window_height; }

static uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
}

static int production_per_tick(void) {
    int p = 1; /* plot 1 bush always produces */
    if (g_game_state.cozy_plot2_planted) {
        p += 1;
    }
    if (g_game_state.cozy_greenhouse_unlocked) {
        p += 3;
    }
    return p;
}

/* Next milestone the player is working toward (drives CTA + progress bar). */
static int next_cost(void) {
    if (!g_game_state.cozy_plot2_planted) {
        return PLANT_COST;
    }
    if (!g_game_state.cozy_greenhouse_unlocked) {
        return GREENHOUSE_COST;
    }
    return 0; /* garden complete */
}

static void add_berries(int n) {
    long v = (long)g_game_state.cozy_berries + n;
    if (v > BERRIES_MAX) {
        v = BERRIES_MAX;
    }
    if (v < 0) {
        v = 0;
    }
    g_game_state.cozy_berries = (int)v;
}

static void spawn_fly(float sx, float sy) {
    for (int i = 0; i < FLY_MAX; ++i) {
        if (!s_flies[i].active) {
            s_flies[i] = (Fly){.active = true, .t = 0.0F, .sx = sx, .sy = sy};
            return;
        }
    }
}

/* One auto-production tick: berries rise and route to the basket. */
static void do_tick_once(void) {
    add_berries(production_per_tick());
    spawn_fly(PLOT1_X, PLOT_Y + 30.0F);
    if (g_game_state.cozy_plot2_planted) {
        spawn_fly(PLOT2_X, PLOT_Y + 30.0F);
    }
    if (g_game_state.cozy_greenhouse_unlocked) {
        spawn_fly(PLOT3_X, PLOT_Y + 30.0F);
    }
    game_state_mark_dirty();
}

static bool do_plant(void) {
    if (!g_game_state.cozy_plot2_planted && g_game_state.cozy_berries >= PLANT_COST) {
        add_berries(-PLANT_COST);
        g_game_state.cozy_plot2_planted = true;
        game_state_mark_dirty();
        return true;
    }
    return false;
}

static bool do_unlock(void) {
    if (g_game_state.cozy_plot2_planted && !g_game_state.cozy_greenhouse_unlocked &&
        g_game_state.cozy_berries >= GREENHOUSE_COST) {
        add_berries(-GREENHOUSE_COST);
        g_game_state.cozy_greenhouse_unlocked = true;
        game_state_mark_dirty();
        return true;
    }
    return false;
}

static void reset_playtest(void) {
    game_state_init_defaults(&g_game_state);
    s_tick_accum = 0.0F;
    memset(s_flies, 0, sizeof(s_flies));
}

static void cozy_update(float dt) {
    if (dt <= 0.0F) {
        dt = 1.0F / 60.0F;
    }
    s_tick_accum += dt;
    while (s_tick_accum >= TICK_INTERVAL) {
        s_tick_accum -= TICK_INTERVAL;
        do_tick_once();
    }
    for (int i = 0; i < FLY_MAX; ++i) {
        if (s_flies[i].active) {
            s_flies[i].t += dt / FLY_DURATION;
            if (s_flies[i].t >= 1.0F) {
                s_flies[i].active = false;
            }
        }
    }
}

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
        /* --fresh-state / --disable-autosave from launchers: accepted no-ops. */
    }
}

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

/* Column-major model matrix: scale a unit-centered quad to (w,h), translate to
 * (cx,cy). emit applies the region's source size, so divide by source pixels. */
static void emit_sprite(int region, float cx, float cy, float w, float h, uint32_t color) {
    const float sx = (s_region_w[region] > 0) ? (w / (float)s_region_w[region]) : 1.0F;
    const float sy = (s_region_h[region] > 0) ? (h / (float)s_region_h[region]) : 1.0F;
    float m[16] = {0};
    m[0] = sx;
    m[5] = sy;
    m[10] = 1.0F;
    m[12] = cx;
    m[13] = cy;
    m[15] = 1.0F;
    nt_sprite_renderer_emit_region(s_atlas, s_region_idx[region], m, 0.5F, 0.5F, color, 0);
}

/* Emit a sprite at a target height, preserving its source aspect ratio. */
static void emit_h(int region, float cx, float cy, float target_h, uint32_t color) {
    const float aspect = (s_region_h[region] > 0) ? ((float)s_region_w[region] / (float)s_region_h[region]) : 1.0F;
    emit_sprite(region, cx, cy, target_h * aspect, target_h, color);
}

static void emit_text(const char *utf8, float x, float y, float size, const float color[4]) {
    float model[16] = {0};
    model[0] = 1.0F;
    model[5] = 1.0F;
    model[10] = 1.0F;
    model[12] = x;
    model[13] = y;
    model[15] = 1.0F;
    nt_text_renderer_set_material(s_text_mat);
    nt_text_renderer_set_font(s_font);
    nt_text_renderer_draw(utf8, model, size, color, 0.0F, 0.0F);
    nt_text_renderer_flush();
}

/* Roboto average advance ~0.52em -- good enough to center HUD labels. */
static float text_width(const char *s, float size) { return (float)strlen(s) * size * 0.52F; }

static void emit_text_centered(const char *s, float cx, float y, float size, const float color[4]) {
    emit_text(s, cx - text_width(s, size) * 0.5F, y, size, color);
}

static bool box_contains(Box b, float x, float y) {
    return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h;
}

static void layout(void) {
    s_plant_box = (Box){.x = PLOT2_X - 130.0F, .y = 56.0F, .w = 260.0F, .h = 64.0F};
    s_gh_box = (Box){.x = PLOT3_X - PLOT_FOOT * 0.5F, .y = PLOT_Y - PLOT_FOOT * 0.5F, .w = PLOT_FOOT, .h = PLOT_FOOT};
}

/* ---- Input (window px, y-down) -> design (y-up) ---- */

static void handle_input(void) {
    if (!nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        return;
    }
    const float cw = (float)canvas_w();
    const float ch = (float)canvas_h();
    for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
        const nt_pointer_t p = g_nt_input.pointers[i];
        if (!p.active) {
            continue;
        }
        const float dx = p.x * (DESIGN_W / cw);
        const float dy = DESIGN_H - p.y * (DESIGN_H / ch);
        if (!g_game_state.cozy_plot2_planted && box_contains(s_plant_box, dx, dy)) {
            (void)do_plant();
            return;
        }
        if (g_game_state.cozy_plot2_planted && !g_game_state.cozy_greenhouse_unlocked &&
            box_contains(s_gh_box, dx, dy)) {
            (void)do_unlock();
            return;
        }
    }
}

#ifndef NT_PLATFORM_WEB
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
    unsigned char *flip = (unsigned char *)malloc((size_t)w * (size_t)h * 4u);
    if (!flip) {
        free(buf);
        return false;
    }
    const size_t stride = (size_t)w * 4u;
    /* glReadPixels returns bottom-up rows; convert to top-down for the PNG. This
       is a capture/asset boundary, not game logic (game stays Y-up). */
    for (int y = 0; y < h; ++y) {
        memcpy(flip + (size_t)y * stride, buf + (size_t)(h - 1 - y) * stride, stride);
    }
    free(buf);
    const int ok = stbi_write_png(path, w, h, 4, flip, (int)stride);
    free(flip);
    return ok != 0;
}
#endif

/* ---- Scene composition (design units, y-up) ---- */

static void compose_world(void) {
    const uint32_t white = 0xFFFFFFFFu;
    const uint32_t dim = rgba(150, 150, 160, 255);

    emit_sprite(R_BG, DESIGN_W * 0.5F, DESIGN_H * 0.5F, DESIGN_W, DESIGN_H, white);

    /* Plot 1: pre-planted bush (always producing). */
    emit_sprite(R_PLOT, PLOT1_X, PLOT_Y, PLOT_FOOT, PLOT_FOOT, white);
    emit_h(R_BUSH, PLOT1_X, PLOT_Y + 24.0F, PLOT_FOOT * 0.92F, white);

    /* Plot 2: empty until planted. */
    emit_sprite(R_PLOT, PLOT2_X, PLOT_Y, PLOT_FOOT, PLOT_FOOT, white);
    if (g_game_state.cozy_plot2_planted) {
        emit_h(R_BUSH, PLOT2_X, PLOT_Y + 24.0F, PLOT_FOOT * 0.92F, white);
    }

    /* Plot 3: locked greenhouse (the one lock). */
    if (g_game_state.cozy_greenhouse_unlocked) {
        emit_sprite(R_PLOT, PLOT3_X, PLOT_Y, PLOT_FOOT, PLOT_FOOT, white);
        emit_h(R_GREENHOUSE, PLOT3_X, PLOT_Y + 30.0F, PLOT_FOOT * 1.05F, white);
    } else {
        emit_sprite(R_PLOT, PLOT3_X, PLOT_Y, PLOT_FOOT, PLOT_FOOT, dim);
        emit_h(R_LOCK, PLOT3_X, PLOT_Y + 20.0F, PLOT_FOOT * 0.5F, white);
    }

    /* Auto-routed berries drifting from each producing plot to the basket. */
    for (int i = 0; i < FLY_MAX; ++i) {
        if (!s_flies[i].active) {
            continue;
        }
        const float t = s_flies[i].t;
        const float e = t * t * (3.0F - 2.0F * t); /* smoothstep */
        const float x = s_flies[i].sx + (BASKET_X - s_flies[i].sx) * e;
        const float arc = sinf(t * 3.14159265F) * 60.0F; /* gentle lift */
        const float y = s_flies[i].sy + (BASKET_Y - s_flies[i].sy) * e + arc;
        emit_h(R_BERRY, x, y, 30.0F, white);
    }

    nt_sprite_renderer_flush();
}

static void compose_hud(void) {
    const uint32_t white = 0xFFFFFFFFu;

    /* Top-center: basket + berry-count panel. */
    emit_sprite(R_PANEL, 480.0F, 488.0F, 360.0F, 86.0F, white);
    emit_h(R_BASKET, BASKET_X, BASKET_Y, 78.0F, white);
    emit_h(R_BERRY, 446.0F, 500.0F, 30.0F, white);

    /* Progress bar toward the next milestone. */
    const int cost = next_cost();
    const float bar_w = 360.0F;
    const float bar_h = 20.0F;
    const float bar_cx = 480.0F;
    const float bar_cy = 420.0F;
    const float bar_left = bar_cx - bar_w * 0.5F;
    emit_sprite(R_PANEL, bar_cx, bar_cy, bar_w, bar_h, rgba(40, 34, 30, 200));
    if (cost > 0) {
        float prog = (float)g_game_state.cozy_berries / (float)cost;
        if (prog > 1.0F) {
            prog = 1.0F;
        }
        const float fw = bar_w * prog;
        if (fw > 2.0F) {
            emit_sprite(R_PANEL, bar_left + fw * 0.5F, bar_cy, fw, bar_h, rgba(86, 200, 86, 255));
        }
    } else {
        emit_sprite(R_PANEL, bar_cx, bar_cy, bar_w, bar_h, rgba(86, 200, 86, 255));
    }

    /* Primary CTA: Plant Bush button (until plot 2 is planted). */
    if (!g_game_state.cozy_plot2_planted) {
        const bool afford = g_game_state.cozy_berries >= PLANT_COST;
        const uint32_t tint = afford ? white : rgba(160, 160, 168, 255);
        emit_sprite(R_PANEL, s_plant_box.x + s_plant_box.w * 0.5F, s_plant_box.y + s_plant_box.h * 0.5F,
                    s_plant_box.w, s_plant_box.h, tint);
        emit_h(R_BERRY, s_plant_box.x + s_plant_box.w - 56.0F, s_plant_box.y + s_plant_box.h * 0.5F, 28.0F, white);
    }

    nt_sprite_renderer_flush();
}

static void compose_text(void) {
    const float ink[4] = {0.16F, 0.12F, 0.10F, 1.0F};
    const float cream[4] = {0.996F, 0.980F, 0.937F, 1.0F};
    char buf[64];

    /* Title near the top of the screen (Y-up: high y = top). */
    emit_text("The Little Garden", 26.0F, DESIGN_H - 34.0F, 26.0F, ink);

    /* Berry count, big, on the top panel (right of the berry icon). */
    (void)snprintf(buf, sizeof(buf), "%d", g_game_state.cozy_berries);
    emit_text(buf, 466.0F, 474.0F, 40.0F, ink);

    /* Production rate, small. */
    (void)snprintf(buf, sizeof(buf), "+%d / tick", production_per_tick());
    emit_text(buf, 466.0F, 452.0F, 16.0F, ink);

    /* Progress label. */
    const int cost = next_cost();
    if (cost == PLANT_COST && !g_game_state.cozy_plot2_planted) {
        (void)snprintf(buf, sizeof(buf), "%d / %d  to plant a bush", g_game_state.cozy_berries, PLANT_COST);
    } else if (cost == GREENHOUSE_COST) {
        (void)snprintf(buf, sizeof(buf), "%d / %d  to unlock greenhouse", g_game_state.cozy_berries, GREENHOUSE_COST);
    } else {
        (void)snprintf(buf, sizeof(buf), "Garden thriving");
    }
    emit_text_centered(buf, 480.0F, 432.0F, 16.0F, ink);

    /* Plant button caption. */
    if (!g_game_state.cozy_plot2_planted) {
        emit_text("Plant Bush", s_plant_box.x + 22.0F, s_plant_box.y + s_plant_box.h * 0.5F - 8.0F, 22.0F, ink);
        (void)snprintf(buf, sizeof(buf), "%d", PLANT_COST);
        emit_text(buf, s_plant_box.x + s_plant_box.w - 38.0F, s_plant_box.y + s_plant_box.h * 0.5F - 8.0F, 22.0F, ink);
    }

    /* Locked greenhouse label. */
    if (!g_game_state.cozy_greenhouse_unlocked) {
        emit_text_centered("Unlock at 50", PLOT3_X, PLOT_Y - PLOT_FOOT * 0.5F - 8.0F, 17.0F, cream);
    }
}

/* ---- DevAPI (current engine ABI: fill result_obj, return true) ---- */

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static void emit_cozy_state(cJSON *result_obj) {
    cJSON_AddStringToObject(result_obj, "runtime", "cozy_automation");
    cJSON_AddStringToObject(result_obj, "location", "the_little_garden");
    cJSON_AddBoolToObject(result_obj, "atlas_ready", s_atlas_resolved);
    cJSON_AddNumberToObject(result_obj, "berries", (double)g_game_state.cozy_berries);
    cJSON_AddBoolToObject(result_obj, "plot2_planted", g_game_state.cozy_plot2_planted);
    cJSON_AddBoolToObject(result_obj, "greenhouse_unlocked", g_game_state.cozy_greenhouse_unlocked);
    cJSON_AddNumberToObject(result_obj, "rate", (double)production_per_tick());
    cJSON_AddNumberToObject(result_obj, "next_cost", (double)next_cost());
    cJSON_AddBoolToObject(result_obj, "can_plant",
                          !g_game_state.cozy_plot2_planted && g_game_state.cozy_berries >= PLANT_COST);
    cJSON_AddBoolToObject(result_obj, "can_unlock",
                          g_game_state.cozy_plot2_planted && !g_game_state.cozy_greenhouse_unlocked &&
                              g_game_state.cozy_berries >= GREENHOUSE_COST);
}

static bool ep_game_state(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    emit_cozy_state(result_obj);
    return true;
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    reset_playtest();
    emit_cozy_state(result_obj);
    return true;
}

static bool ep_action_plant(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    cJSON_AddBoolToObject(result_obj, "applied", do_plant());
    emit_cozy_state(result_obj);
    return true;
}

static bool ep_action_unlock(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    cJSON_AddBoolToObject(result_obj, "applied", do_unlock());
    emit_cozy_state(result_obj);
    return true;
}

/* Force N production ticks deterministically (default 1) for automation. */
static bool ep_action_tick(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)err;
    (void)user;
    int n = 1;
    const cJSON *p = params ? cJSON_GetObjectItemCaseSensitive(params, "count") : NULL;
    if (cJSON_IsNumber(p) && p->valueint > 0) {
        n = p->valueint;
    }
    for (int i = 0; i < n; ++i) {
        do_tick_once();
    }
    emit_cozy_state(result_obj);
    return true;
}

static bool ep_frame_screenshot(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)err;
    (void)user;
    const cJSON *p = params ? cJSON_GetObjectItemCaseSensitive(params, "path") : NULL;
    /* A request WITH a path starts a fresh capture (unless one is in flight).
     * A request WITHOUT a path just polls the current/last capture status. */
    if (p && cJSON_IsString(p) && !s_shot_pending) {
        (void)snprintf(s_shot_path, sizeof(s_shot_path), "%s", p->valuestring);
        s_shot_done = false;
        s_shot_ok = false;
        s_shot_pending = true;
    }
    cJSON_AddBoolToObject(result_obj, "queued", s_shot_pending);
    cJSON_AddBoolToObject(result_obj, "done", s_shot_done);
    cJSON_AddBoolToObject(result_obj, "ok", s_shot_ok);
    cJSON_AddStringToObject(result_obj, "path", s_shot_path);
    return true;
}

static void register_game_endpoints(void) {
    static const nt_devapi_command_desc descs[] = {
        {"game.state", "game", "Return cozy automation runtime state.", "", "{berries,...}", "immediate", "none"},
        {"game.reset_playtest", "game", "Reset cozy state for automation.", "", "{berries,...}", "immediate", "resets state"},
        {"game.action.plant", "game", "Plant a bush on plot 2 (costs berries).", "", "{applied,...}", "immediate", "mutates state"},
        {"game.action.unlock", "game", "Unlock the greenhouse on plot 3.", "", "{applied,...}", "immediate", "mutates state"},
        {"game.action.tick", "game", "Force N production ticks.", "count?", "{berries,...}", "immediate", "mutates state"},
        {"frame.screenshot", "game", "Capture the rendered backbuffer to a PNG.", "path?", "{queued,done,ok,path}", "next-frame", "writes file"},
    };
    game_state_register_devapi();
    game_devapi_ui_register();
    (void)nt_devapi_register(&descs[0], ep_game_state, NULL);
    (void)nt_devapi_register(&descs[1], ep_game_reset_playtest, NULL);
    (void)nt_devapi_register(&descs[2], ep_action_plant, NULL);
    (void)nt_devapi_register(&descs[3], ep_action_unlock, NULL);
    (void)nt_devapi_register(&descs[4], ep_action_tick, NULL);
    (void)nt_devapi_register(&descs[5], ep_frame_screenshot, NULL);
}

static void register_ui_devapi(void) {
    game_devapi_ui_clear();
    (void)game_devapi_ui_register_node("root", "", "screen", "Cozy Automation", "The Little Garden",
                                       0.0F, 0.0F, DESIGN_W, DESIGN_H, true, true);
    if (!g_game_state.cozy_plot2_planted) {
        (void)game_devapi_ui_register_node("plant", "root", "button", "Plant Bush", "Plant Bush",
                                           s_plant_box.x, s_plant_box.y, s_plant_box.w, s_plant_box.h, true,
                                           g_game_state.cozy_berries >= PLANT_COST);
    }
    if (!g_game_state.cozy_greenhouse_unlocked) {
        (void)game_devapi_ui_register_node("greenhouse", "root", "button", "Unlock Greenhouse", "Unlock at 50",
                                           s_gh_box.x, s_gh_box.y, s_gh_box.w, s_gh_box.h, true,
                                           g_game_state.cozy_plot2_planted &&
                                               g_game_state.cozy_berries >= GREENHOUSE_COST);
    }
}
#endif

/* ---- Frame ---- */

static void frame(void) {
    nt_window_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_update();
    }
#endif
    nt_input_poll();

    nt_resource_step();
    nt_material_step();
    resolve_regions();

    const float w = (float)canvas_w();
    const float h = (float)canvas_h();

    layout();
    handle_input();
    cozy_update(g_nt_app.dt > 0.0F ? g_nt_app.dt : (1.0F / 60.0F));

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        register_ui_devapi();
    }
#endif

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif

    /* Ortho over the design canvas (y-up, bottom-left origin). */
    float proj[16] = {0};
    proj[0] = 2.0F / DESIGN_W;
    proj[5] = 2.0F / DESIGN_H;
    proj[10] = -1.0F;
    proj[12] = -1.0F;
    proj[13] = -1.0F;
    proj[15] = 1.0F;

    nt_frame_uniforms_t u = {0};
    memcpy(u.view_proj, proj, 64);
    memcpy(u.proj, proj, 64);
    u.view[0] = u.view[5] = u.view[10] = u.view[15] = 1.0F;
    u.resolution[0] = w;
    u.resolution[1] = h;
    u.resolution[2] = (w > 0.0F) ? 1.0F / w : 0.0F;
    u.resolution[3] = (h > 0.0F) ? 1.0F / h : 0.0F;
    u.near_far[0] = -1.0F;
    u.near_far[1] = 1.0F;

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

    /* Soft sky-blue clear behind the bg sprite. */
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.541F, 0.808F, 0.961F, 1.0F}, .clear_depth = 1.0F});
    nt_font_step();

    if (can_render && !g_nt_gfx.context_restored) {
        nt_gfx_update_buffer(s_frame_ubo, &u, sizeof(u));
        nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);

        nt_sprite_renderer_set_material(s_sprite_mat);
        compose_world();
        nt_sprite_renderer_set_material(s_sprite_mat);
        compose_hud();

        if (text_info && text_info->ready) {
            compose_text();
        }
    }

    nt_gfx_end_pass();

#ifndef NT_PLATFORM_WEB
    if (s_shot_pending && can_render && !g_nt_gfx.context_restored) {
        s_shot_ok = write_backbuffer_png(s_shot_path);
        s_shot_done = true;
        s_shot_pending = false;
    }
#endif

    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

/* ---- Main ---- */

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Cozy Automation";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    parse_args(argc, argv);
    reset_playtest();

    g_nt_window.title = "Cozy Automation - The Little Garden";
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
    (void)nt_sprite_renderer_init(&sr_desc);
    nt_text_renderer_init();

    g_nt_app.target_dt = 1.0F / 60.0F;

    s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "frame_uniforms",
    });

    s_pack_id = nt_hash32_str("cozy_automation");
    nt_resource_mount(s_pack_id, 100);
    nt_resource_load_auto(s_pack_id, "assets/cozy_automation.ntpack");

    nt_resource_t vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_VERT, NT_ASSET_SHADER_CODE);
    nt_resource_t fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_FRAG, NT_ASSET_SHADER_CODE);
    s_atlas = nt_resource_request(ASSET_ATLAS_GARDEN, NT_ASSET_ATLAS);
    nt_resource_t atlas_tex = nt_resource_request(ASSET_TEXTURE_GARDEN_TEX0, NT_ASSET_TEXTURE);

    s_sprite_mat = nt_material_create(&(nt_material_create_desc_t){
        .vs = vs,
        .fs = fs,
        .textures = {{.name = "u_texture", .resource = atlas_tex}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "cozy_sprite",
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
        .label = "cozy_text",
    });
    s_font = nt_font_create(&(nt_font_create_desc_t){
        .curve_texture_width = 1024,
        .curve_texture_height = 512,
        .band_texture_height = 256,
        .band_count = 8,
    });
    nt_font_add(s_font, nt_resource_request(ASSET_FONT_COZY_FONT_UI, NT_ASSET_FONT));

    nt_resource_set_activate_time_budget(0.0F);

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
