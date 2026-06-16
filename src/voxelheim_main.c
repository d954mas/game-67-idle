/*
 * Voxelheim -- "Frost Keep Approach" casual-RPG core loop (P2-P4).
 *
 * Built on the static first-screen slice: real sprites from the voxelheim atlas
 * + slug-text HUD on a DESIGN canvas (960x540, ortho, bottom-left origin, Y-up).
 * Everything is authored in design units and scaled uniformly to the window.
 *
 * The loop, kept casual + readable + juicy:
 *   - Tap to move: hero straight-line walks toward the tapped design point.
 *   - 3 ice-goblins along the path; idle until the hero is near, then approach.
 *   - Auto-combat: hero auto-attacks in range (hit flash + floating damage),
 *     enemy attacks back, hero slowly regenerates out of combat.
 *   - Reward: kills sparkle, grant XP; XP fills -> LEVEL UP (heal, scale pop,
 *     glow, big text). Persisted progression in g_game_state.run.*.
 *   - Win: clear all 3 enemies + reach the keep -> "FROST KEEP CLEARED!".
 *   - FTUE (<=3 beats): move -> fight -> clear + enter the keep.
 *
 * Render path: nt_atlas + nt_sprite_renderer (direct emit), nt_text_renderer for
 * labels. A solid white atlas region (voxels/white.png) backs HP-bar fills,
 * particles, glows, and flash overlays.
 *
 * Pack build (explicit) -- run from the project root only if assets change:
 *   build/voxelheim_packer/build_voxelheim_packs.exe build/voxelheim
 *   copy build/voxelheim/voxelheim.ntpack -> assets/voxelheim.ntpack
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

/* Design canvas the composition is authored against. */
#define DESIGN_W 960.0F
#define DESIGN_H 540.0F

/* Flip flag (mirrors NT_SPRITE_FLAG_FLIP_X = 1U<<0; literal keeps deps local). */
#define VH_FLIP_X 1U

/* ---- Tuning (casual) ---- */
#define HERO_SPEED 220.0F      /* design-units/sec */
#define HERO_ATTACK_RANGE 72.0F
#define HERO_ATTACK_COOLDOWN 0.6F
#define HERO_ATTACK_DAMAGE 10
#define ENEMY_AGGRO_RANGE 140.0F
#define ENEMY_SPEED 60.0F
/* Goblins hold their post: they only step toward the hero when he comes within
 * this leash distance of their guard spot, and never chase past it. This makes
 * the player WALK UP the path and fight each encounter in turn. */
#define ENEMY_LEASH 70.0F
#define ENEMY_ATTACK_COOLDOWN 0.85F
#define ENEMY_ATTACK_DAMAGE 8
#define ENEMY_MAX_HP 34
#define HERO_REGEN_PER_SEC 5.0F
#define XP_PER_KILL 20
#define KEEP_REACH_RANGE 58.0F
#define DOWNED_DURATION 1.2F
#define HIT_FLASH_TIME 0.1F
#define LEVELUP_POP_TIME 1.3F

#define ENEMY_COUNT 3
#define MAX_PARTICLES 48
#define MAX_FLOATERS 16
#define MAX_SPRITE_FX 16

/* ---- Region name hashes, indexed by enum ---- */

enum {
    R_BACKGROUND = 0,
    R_SNOW,
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
    R_WHITE,
    R_HIT_SPARK,
    R_LEVELUP_BURST,
    R_COIN,
    R_BAR_FRAME,
    R_COUNT,
};

static const nt_hash64_t k_region_names[R_COUNT] = {
    ASSET_ATLAS_REGION_VOXELS_BACKGROUND_PNG,
    ASSET_ATLAS_REGION_VOXELS_SNOW_TILE_PNG,   ASSET_ATLAS_REGION_VOXELS_PATH_TILE_PNG, ASSET_ATLAS_REGION_VOXELS_KEEP_PNG,
    ASSET_ATLAS_REGION_VOXELS_PINE_PNG,        ASSET_ATLAS_REGION_VOXELS_ROCK_PNG,      ASSET_ATLAS_REGION_VOXELS_HERO_PNG,
    ASSET_ATLAS_REGION_VOXELS_ENEMY_PNG,       ASSET_ATLAS_REGION_VOXELS_HP_BAR_PNG,    ASSET_ATLAS_REGION_VOXELS_STAMINA_BAR_PNG,
    ASSET_ATLAS_REGION_VOXELS_LEVEL_BADGE_PNG, ASSET_ATLAS_REGION_VOXELS_MINIMAP_PNG,   ASSET_ATLAS_REGION_VOXELS_ITEM_SLOT_PNG,
    ASSET_ATLAS_REGION_VOXELS_BANNER_PNG,      ASSET_ATLAS_REGION_VOXELS_BUTTON_PNG,    ASSET_ATLAS_REGION_VOXELS_WHITE_PNG,
    ASSET_ATLAS_REGION_VOXELS_HIT_SPARK_PNG,   ASSET_ATLAS_REGION_VOXELS_LEVELUP_BURST_PNG, ASSET_ATLAS_REGION_VOXELS_COIN_PNG,
    ASSET_ATLAS_REGION_VOXELS_BAR_FRAME_PNG,
};

/* ---- Engine/runtime state ---- */

static bool s_devapi_enabled;
static uint16_t s_devapi_port = VOXELHEIM_DEVAPI_PORT_DEFAULT;
static int s_window_width = 960;
static int s_window_height = 540;
static bool s_fresh_state;
static bool s_autosave = true;

/* When a probe drives the sim via game.debug.tick, the per-frame real-time
 * update is suppressed so headless runs are fully deterministic (the probe owns
 * simulation time). The live game never sets this. */
static bool s_debug_driven;

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

static char s_shot_path[512];
static bool s_shot_pending;
static bool s_shot_done;
static bool s_shot_ok;

/* ---- Gameplay simulation state (transient; progression lives in g_game_state.run) ---- */

typedef struct Floater {
    bool active;
    float x, y;
    float age;       /* seconds */
    float ttl;       /* seconds */
    char text[16];
    float color[4];
    float size;
} Floater;

typedef struct Particle {
    bool active;
    float x, y;
    float vx, vy;
    float age, ttl;
    float r, g, b;
    float size;
} Particle;

typedef struct Enemy {
    bool alive;
    float x, y;       /* design units */
    float guard_x, guard_y; /* post the goblin holds; only leashes within ENEMY_LEASH */
    int hp;
    float attack_cd;  /* seconds until next attack */
    float flash;      /* hit-flash timer */
    bool facing_left;
} Enemy;

/* Short-lived sprite FX (hit spark, coin pop) drawn with a real atlas region. */
typedef struct SpriteFx {
    bool active;
    int region;       /* R_HIT_SPARK / R_COIN / ... */
    float x, y;
    float vy;         /* coins drift up */
    float age, ttl;
    float size;
    float spin;       /* current rotation (radians) */
    float spin_rate;
} SpriteFx;

static struct {
    float hero_x, hero_y;
    float target_x, target_y;
    bool moving;
    bool hero_facing_left;
    float hero_attack_cd;
    float hero_flash;
    float regen_accum;     /* fractional hp accumulator */
    float downed_timer;    /* >0 = hero down + respawning */
    float levelup_timer;   /* >0 = level-up pop playing */
    int attack_target;     /* index of enemy the hero is currently attacking, else -1 */
    bool has_moved;        /* FTUE: first move done */
    bool won;
    float win_timer;       /* time since victory (for banner anim) */
    Enemy enemies[ENEMY_COUNT];
    Particle particles[MAX_PARTICLES];
    Floater floaters[MAX_FLOATERS];
    SpriteFx sprite_fx[MAX_SPRITE_FX];
    float time;            /* total sim time, for idle bob */
    bool started;          /* sim initialised */
} g_sim;

/* Hero spawn (lower-left of the path, like the fake shot, so the goblin column
 * up the path stays visible) + keep center (top-center). */
#define HERO_SPAWN_X (DESIGN_W * 0.41F)
#define HERO_SPAWN_Y (DESIGN_H * 0.25F)
#define KEEP_CX (DESIGN_W * 0.5F)
#define KEEP_CY (DESIGN_H * 0.70F)

/* ---- Helpers ---- */

static uint32_t canvas_w(void) { return g_nt_window.fb_width > 0 ? g_nt_window.fb_width : (uint32_t)s_window_width; }
static uint32_t canvas_h(void) { return g_nt_window.fb_height > 0 ? g_nt_window.fb_height : (uint32_t)s_window_height; }

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* Pack an RGBA float color (0..1) into 0xAABBGGRR for the sprite renderer. */
static uint32_t pack_rgba(float r, float g, float b, float a) {
    uint32_t rr = (uint32_t)(clampf(r, 0.0F, 1.0F) * 255.0F + 0.5F);
    uint32_t gg = (uint32_t)(clampf(g, 0.0F, 1.0F) * 255.0F + 0.5F);
    uint32_t bb = (uint32_t)(clampf(b, 0.0F, 1.0F) * 255.0F + 0.5F);
    uint32_t aa = (uint32_t)(clampf(a, 0.0F, 1.0F) * 255.0F + 0.5F);
    return (aa << 24) | (bb << 16) | (gg << 8) | rr;
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
        } else if (strcmp(argv[i], "--fresh-state") == 0) {
            s_fresh_state = true;
        } else if (strcmp(argv[i], "--disable-autosave") == 0) {
            s_autosave = false;
        }
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

/* Scale a unit-centered quad to (w,h) design units, translate center to (cx,cy),
 * tint by color, optionally flip horizontally. */
static void emit_sprite_flip(int region, float cx, float cy, float w, float h, uint32_t color, uint8_t flip) {
    const float sx = (s_region_w[region] > 0) ? (w / (float)s_region_w[region]) : 1.0F;
    const float sy = (s_region_h[region] > 0) ? (h / (float)s_region_h[region]) : 1.0F;
    mat4 m;
    glm_mat4_identity(m);
    m[0][0] = sx;
    m[1][1] = sy;
    m[3][0] = cx;
    m[3][1] = cy;
    nt_sprite_renderer_emit_region(s_atlas, s_region_idx[region], (const float *)m, 0.5F, 0.5F, color, flip);
}

static void emit_sprite(int region, float cx, float cy, float w, float h, uint32_t color) {
    emit_sprite_flip(region, cx, cy, w, h, color, 0);
}

/* Scaled + rotated quad (rotation about the sprite center, radians). */
static void emit_sprite_rot(int region, float cx, float cy, float w, float h, float rot, uint32_t color) {
    const float sx = (s_region_w[region] > 0) ? (w / (float)s_region_w[region]) : 1.0F;
    const float sy = (s_region_h[region] > 0) ? (h / (float)s_region_h[region]) : 1.0F;
    const float c = cosf(rot);
    const float s = sinf(rot);
    mat4 m;
    glm_mat4_identity(m);
    /* columns: scaled basis rotated in the XY plane */
    m[0][0] = c * sx;
    m[0][1] = s * sx;
    m[1][0] = -s * sy;
    m[1][1] = c * sy;
    m[3][0] = cx;
    m[3][1] = cy;
    nt_sprite_renderer_emit_region(s_atlas, s_region_idx[region], (const float *)m, 0.5F, 0.5F, color, 0);
}

/* Emit a sprite at design-unit height, preserving its source aspect ratio. */
static void emit_h_flip(int region, float cx, float cy, float design_h, uint32_t color, uint8_t flip) {
    const float aspect = (s_region_h[region] > 0) ? ((float)s_region_w[region] / (float)s_region_h[region]) : 1.0F;
    emit_sprite_flip(region, cx, cy, design_h * aspect, design_h, color, flip);
}

static void emit_h(int region, float cx, float cy, float design_h, uint32_t color) {
    emit_h_flip(region, cx, cy, design_h, color, 0);
}

/* Solid color quad (uses the white atlas region, tinted). */
static void emit_quad(float cx, float cy, float w, float h, float r, float g, float b, float a) {
    emit_sprite(R_WHITE, cx, cy, w, h, pack_rgba(r, g, b, a));
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

static float text_width(const char *s, float size) {
    return (float)strlen(s) * size * 0.52F;
}

/* Centered text with a dark drop shadow for readability. */
static void emit_text_centered(const char *s, float cx, float y, float size, const float color[4]) {
    const float ink[4] = {0.10F, 0.08F, 0.07F, color[3]};
    const float w = text_width(s, size);
    emit_text(s, cx - w * 0.5F + 2.0F, y - 2.0F, size, ink);
    emit_text(s, cx - w * 0.5F, y, size, color);
}

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
    for (int y = 0; y < h; ++y) {
        memcpy(flip + (size_t)y * stride, buf + (size_t)(h - 1 - y) * stride, stride);
    }
    free(buf);

    const int ok = stbi_write_png(path, w, h, 4, flip, (int)stride);
    free(flip);
    return ok != 0;
}

/* ---- Effects spawning ---- */

static void spawn_floater(float x, float y, const char *text, const float color[4], float size) {
    for (int i = 0; i < MAX_FLOATERS; ++i) {
        if (!g_sim.floaters[i].active) {
            Floater *f = &g_sim.floaters[i];
            f->active = true;
            f->x = x;
            f->y = y;
            f->age = 0.0F;
            f->ttl = 0.9F;
            f->size = size;
            (void)snprintf(f->text, sizeof(f->text), "%s", text);
            memcpy(f->color, color, sizeof(f->color));
            return;
        }
    }
}

static void spawn_sparkle(float x, float y) {
    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < 14; ++i) {
        if (!g_sim.particles[i].active) {
            Particle *p = &g_sim.particles[i];
            float ang = (float)spawned / 14.0F * 6.2831853F;
            float spd = 90.0F + (float)((i * 37) % 60);
            p->active = true;
            p->x = x;
            p->y = y;
            p->vx = cosf(ang) * spd;
            p->vy = sinf(ang) * spd + 40.0F;
            p->age = 0.0F;
            p->ttl = 0.5F + (float)((i * 13) % 30) / 100.0F;
            /* warm gold sparkle */
            p->r = 1.0F;
            p->g = 0.85F;
            p->b = 0.35F;
            p->size = 6.0F + (float)((i * 7) % 6);
            spawned++;
        }
    }
}

/* Spawn a short-lived sprite FX (real atlas region) at (x,y). */
static void spawn_sprite_fx(int region, float x, float y, float size, float ttl, float vy, float spin_rate) {
    for (int i = 0; i < MAX_SPRITE_FX; ++i) {
        if (!g_sim.sprite_fx[i].active) {
            SpriteFx *fx = &g_sim.sprite_fx[i];
            fx->active = true;
            fx->region = region;
            fx->x = x;
            fx->y = y;
            fx->vy = vy;
            fx->age = 0.0F;
            fx->ttl = ttl;
            fx->size = size;
            fx->spin = 0.0F;
            fx->spin_rate = spin_rate;
            return;
        }
    }
}

/* A bright impact spark on each landed hit. */
static void spawn_hit_spark(float x, float y) {
    spawn_sprite_fx(R_HIT_SPARK, x, y, 64.0F, 0.22F, 30.0F, 0.0F);
}

/* A coin that pops up and spins on a kill (the loot beat). */
static void spawn_coin_pop(float x, float y) {
    spawn_sprite_fx(R_COIN, x, y + 10.0F, 40.0F, 0.7F, 90.0F, 9.0F);
    spawn_sprite_fx(R_COIN, x - 18.0F, y + 4.0F, 30.0F, 0.6F, 70.0F, -8.0F);
}

/* ---- Run state (g_game_state.run.*) ---- */

static void run_reset(void) {
    g_game_state.run_level = 1;
    g_game_state.run_hero_max_hp = 100;
    g_game_state.run_hero_hp = 100;
    g_game_state.run_xp = 0;
    g_game_state.run_xp_to_next = 60;
    g_game_state.run_enemies_defeated = 0;
    g_game_state.run_keep_reached = false;
    /* Keep ftue_step persistent across resets (don't re-tutorialise). */
    game_state_mark_dirty();
}

static void sim_reset(void) {
    memset(&g_sim, 0, sizeof(g_sim));
    g_sim.started = true;
    g_sim.attack_target = -1;
    g_sim.hero_x = HERO_SPAWN_X;
    g_sim.hero_y = HERO_SPAWN_Y;
    g_sim.target_x = HERO_SPAWN_X;
    g_sim.target_y = HERO_SPAWN_Y;

    /* 3 ice-goblins staggered ALONG the path at increasing distance from the
     * hero spawn, sitting ON the path (centered near x=480) so the player must
     * walk up and fight each in turn. Small alternating x offsets stop them
     * stacking into one blob. */
    /* Spaced > the hero attack range apart so each goblin is a DISTINCT fight
     * the player walks up to (no single engagement bleeds into the next). */
    const float xs[ENEMY_COUNT] = {DESIGN_W * 0.50F - 78.0F, DESIGN_W * 0.50F + 84.0F, DESIGN_W * 0.50F - 30.0F};
    const float ys[ENEMY_COUNT] = {DESIGN_H * 0.30F, DESIGN_H * 0.44F, DESIGN_H * 0.58F};
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        g_sim.enemies[i].alive = true;
        g_sim.enemies[i].x = xs[i];
        g_sim.enemies[i].y = ys[i];
        g_sim.enemies[i].guard_x = xs[i];
        g_sim.enemies[i].guard_y = ys[i];
        g_sim.enemies[i].hp = ENEMY_MAX_HP;
        g_sim.enemies[i].attack_cd = ENEMY_ATTACK_COOLDOWN;
        g_sim.enemies[i].flash = 0.0F;
        g_sim.enemies[i].facing_left = false;
    }
}

/* Full new run: reset progression AND the live sim. */
static void playtest_reset(void) {
    run_reset();
    sim_reset();
}

static int alive_enemies(void) {
    int n = 0;
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        if (g_sim.enemies[i].alive) {
            n++;
        }
    }
    return n;
}

/* ---- Simulation ---- */

static void hero_respawn(void) {
    g_sim.hero_x = HERO_SPAWN_X;
    g_sim.hero_y = HERO_SPAWN_Y;
    g_sim.target_x = HERO_SPAWN_X;
    g_sim.target_y = HERO_SPAWN_Y;
    g_sim.moving = false;
    g_game_state.run_hero_hp = g_game_state.run_hero_max_hp;
    game_state_mark_dirty();
}

/* Issue a move order to a design-space point (clamped to the playable area). */
static void hero_move_to(float x, float y) {
    if (g_sim.downed_timer > 0.0F || g_sim.won) {
        return;
    }
    /* Keep the hero on the lower 2/3 and inside the screen. */
    x = clampf(x, 40.0F, DESIGN_W - 40.0F);
    y = clampf(y, DESIGN_H * 0.12F, DESIGN_H * 0.74F);
    g_sim.target_x = x;
    g_sim.target_y = y;
    g_sim.moving = true;
    if (!g_sim.has_moved) {
        g_sim.has_moved = true;
        if (g_game_state.run_ftue_step < 1) {
            g_game_state.run_ftue_step = 1; /* advance: move -> fight */
            game_state_mark_dirty();
        }
    }
}

static void award_kill(Enemy *e) {
    e->alive = false;
    spawn_sparkle(e->x, e->y);
    spawn_coin_pop(e->x, e->y); /* loot beat: a coin pops on the kill */
    const float xpcol[4] = {1.0F, 0.9F, 0.4F, 1.0F};
    spawn_floater(e->x, e->y + 18.0F, "+20 XP", xpcol, 22.0F);
    g_game_state.run_enemies_defeated += 1;
    g_game_state.run_xp += XP_PER_KILL;

    /* FTUE: first kill advances move -> clear-the-path. */
    if (g_game_state.run_ftue_step < 2) {
        g_game_state.run_ftue_step = 2;
    }

    /* Level up (possibly multiple times). */
    while (g_game_state.run_xp >= g_game_state.run_xp_to_next) {
        g_game_state.run_xp -= g_game_state.run_xp_to_next;
        g_game_state.run_level += 1;
        g_game_state.run_hero_max_hp += 20;
        g_game_state.run_hero_hp = g_game_state.run_hero_max_hp; /* full heal */
        g_game_state.run_xp_to_next = (int)((float)g_game_state.run_xp_to_next * 1.4F + 0.5F);
        g_sim.levelup_timer = LEVELUP_POP_TIME;
        const float lc[4] = {1.0F, 0.95F, 0.5F, 1.0F};
        spawn_floater(g_sim.hero_x, g_sim.hero_y + 60.0F, "LEVEL UP!", lc, 30.0F);
        spawn_sparkle(g_sim.hero_x, g_sim.hero_y + 10.0F);
    }
    game_state_mark_dirty();
}

static void update_effects(float dt) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle *p = &g_sim.particles[i];
        if (!p->active) {
            continue;
        }
        p->age += dt;
        if (p->age >= p->ttl) {
            p->active = false;
            continue;
        }
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vy -= 160.0F * dt; /* gravity (y-up) */
    }
    for (int i = 0; i < MAX_FLOATERS; ++i) {
        Floater *f = &g_sim.floaters[i];
        if (!f->active) {
            continue;
        }
        f->age += dt;
        if (f->age >= f->ttl) {
            f->active = false;
            continue;
        }
        f->y += 38.0F * dt; /* float upward */
    }
    for (int i = 0; i < MAX_SPRITE_FX; ++i) {
        SpriteFx *fx = &g_sim.sprite_fx[i];
        if (!fx->active) {
            continue;
        }
        fx->age += dt;
        if (fx->age >= fx->ttl) {
            fx->active = false;
            continue;
        }
        fx->y += fx->vy * dt;
        fx->vy -= 80.0F * dt; /* coins arc back down (y-up) */
        fx->spin += fx->spin_rate * dt;
    }
}

static void update_sim(float dt) {
    if (!g_sim.started) {
        return;
    }
    if (dt <= 0.0F) {
        dt = 1.0F / 60.0F;
    }
    if (dt > 0.1F) {
        dt = 0.1F; /* clamp large steps so debug ticks stay stable */
    }
    g_sim.time += dt;

    /* timers */
    if (g_sim.hero_flash > 0.0F) {
        g_sim.hero_flash -= dt;
    }
    if (g_sim.levelup_timer > 0.0F) {
        g_sim.levelup_timer -= dt;
    }
    if (g_sim.win_timer > 0.0F || g_sim.won) {
        g_sim.win_timer += dt;
    }
    update_effects(dt);

    /* Downed: brief pause then respawn at full hp. */
    if (g_sim.downed_timer > 0.0F) {
        g_sim.downed_timer -= dt;
        if (g_sim.downed_timer <= 0.0F) {
            g_sim.downed_timer = 0.0F;
            hero_respawn();
        }
        return; /* no movement/combat while downed */
    }

    if (g_sim.won) {
        update_effects(0.0F);
        return; /* freeze gameplay on victory; banner animates via win_timer */
    }

    /* Hero movement toward target. */
    if (g_sim.moving) {
        float dx = g_sim.target_x - g_sim.hero_x;
        float dy = g_sim.target_y - g_sim.hero_y;
        float dist = sqrtf(dx * dx + dy * dy);
        float step = HERO_SPEED * dt;
        if (dist <= step || dist < 1.0F) {
            g_sim.hero_x = g_sim.target_x;
            g_sim.hero_y = g_sim.target_y;
            g_sim.moving = false;
        } else {
            g_sim.hero_x += dx / dist * step;
            g_sim.hero_y += dy / dist * step;
            if (fabsf(dx) > 1.0F) {
                g_sim.hero_facing_left = dx < 0.0F;
            }
        }
    }

    /* Pick nearest alive enemy in attack range; track if any enemy is near. */
    int nearest = -1;
    float nearest_d = 1e9F;
    bool any_in_aggro = false;
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy *e = &g_sim.enemies[i];
        if (!e->alive) {
            continue;
        }
        float dx = e->x - g_sim.hero_x;
        float dy = e->y - g_sim.hero_y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d <= ENEMY_AGGRO_RANGE) {
            any_in_aggro = true;
        }
        if (d <= HERO_ATTACK_RANGE && d < nearest_d) {
            nearest_d = d;
            nearest = i;
        }
    }

    /* Enemy AI: hold the guard post; only step toward the hero within a short
     * leash, and never wander further than ENEMY_LEASH from the post -- so each
     * goblin is a distinct encounter the player must walk up to. */
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy *e = &g_sim.enemies[i];
        if (!e->alive) {
            continue;
        }
        if (e->flash > 0.0F) {
            e->flash -= dt;
        }
        float dx = g_sim.hero_x - e->x;
        float dy = g_sim.hero_y - e->y;
        float d = sqrtf(dx * dx + dy * dy);
        /* How far the goblin currently is from its post. */
        float gx = e->x - e->guard_x;
        float gy = e->y - e->guard_y;
        float gdist = sqrtf(gx * gx + gy * gy);
        if (d <= ENEMY_AGGRO_RANGE && d > HERO_ATTACK_RANGE * 0.7F && gdist < ENEMY_LEASH) {
            /* Step toward the hero, but never beyond the leash from the post. */
            float step = ENEMY_SPEED * dt;
            if (d > 1.0F) {
                e->x += dx / d * step;
                e->y += dy / d * step;
                e->facing_left = dx < 0.0F;
            }
        } else if (d > ENEMY_AGGRO_RANGE && gdist > 2.0F) {
            /* Hero left: drift back to the post so the encounter resets cleanly. */
            float step = ENEMY_SPEED * 0.7F * dt;
            if (gdist <= step) {
                e->x = e->guard_x;
                e->y = e->guard_y;
            } else {
                e->x -= gx / gdist * step;
                e->y -= gy / gdist * step;
            }
        }
        /* Enemy attacks the hero when in range. */
        if (e->attack_cd > 0.0F) {
            e->attack_cd -= dt;
        }
        if (d <= HERO_ATTACK_RANGE && e->attack_cd <= 0.0F) {
            e->attack_cd = ENEMY_ATTACK_COOLDOWN;
            g_game_state.run_hero_hp = clampi(g_game_state.run_hero_hp - ENEMY_ATTACK_DAMAGE, 0, g_game_state.run_hero_max_hp);
            g_sim.hero_flash = HIT_FLASH_TIME;
            game_state_mark_dirty();
            if (g_game_state.run_hero_hp <= 0) {
                g_sim.downed_timer = DOWNED_DURATION;
                const float dc[4] = {1.0F, 0.4F, 0.4F, 1.0F};
                spawn_floater(g_sim.hero_x, g_sim.hero_y + 50.0F, "Downed!", dc, 30.0F);
            }
        }
    }

    /* Hero auto-attack nearest enemy in range. */
    if (g_sim.hero_attack_cd > 0.0F) {
        g_sim.hero_attack_cd -= dt;
    }
    g_sim.attack_target = nearest; /* drives the pulsing target ring under the foe */
    if (nearest >= 0 && g_game_state.run_hero_hp > 0) {
        Enemy *e = &g_sim.enemies[nearest];
        g_sim.hero_facing_left = (e->x < g_sim.hero_x);
        g_sim.moving = false; /* stop to fight */
        if (g_sim.hero_attack_cd <= 0.0F) {
            g_sim.hero_attack_cd = HERO_ATTACK_COOLDOWN;
            e->hp -= HERO_ATTACK_DAMAGE;
            e->flash = HIT_FLASH_TIME;
            spawn_hit_spark(e->x, e->y + 6.0F);
            char dmg[16];
            (void)snprintf(dmg, sizeof(dmg), "-%d", HERO_ATTACK_DAMAGE);
            const float dc[4] = {1.0F, 1.0F, 1.0F, 1.0F};
            spawn_floater(e->x, e->y + 24.0F, dmg, dc, 22.0F);
            if (e->hp <= 0) {
                g_sim.attack_target = -1;
                award_kill(e);
            }
        }
    }

    /* Out-of-combat regen. */
    if (!any_in_aggro && g_game_state.run_hero_hp < g_game_state.run_hero_max_hp && g_game_state.run_hero_hp > 0) {
        g_sim.regen_accum += HERO_REGEN_PER_SEC * dt;
        if (g_sim.regen_accum >= 1.0F) {
            int gain = (int)g_sim.regen_accum;
            g_sim.regen_accum -= (float)gain;
            g_game_state.run_hero_hp = clampi(g_game_state.run_hero_hp + gain, 0, g_game_state.run_hero_max_hp);
            game_state_mark_dirty();
        }
    } else {
        g_sim.regen_accum = 0.0F;
    }

    /* Win check: all enemies cleared AND hero reached the keep portal. */
    if (alive_enemies() == 0 && !g_sim.won) {
        float dx = KEEP_CX - g_sim.hero_x;
        float dy = KEEP_CY - g_sim.hero_y;
        float d = sqrtf(dx * dx + dy * dy);
        if (d <= KEEP_REACH_RANGE) {
            g_sim.won = true;
            g_sim.win_timer = 0.0001F;
            g_game_state.run_keep_reached = true;
            if (g_game_state.run_ftue_step < 3) {
                g_game_state.run_ftue_step = 3; /* tutorial complete */
            }
            spawn_sparkle(KEEP_CX, KEEP_CY);
            game_state_mark_dirty();
        }
    }
}

/* ---- Input ---- */

/* Convert a framebuffer pointer pixel to design-space (Y flips to bottom-up). */
static void pointer_to_design(float px, float py, float *dx, float *dy) {
    float fw = (float)canvas_w();
    float fh = (float)canvas_h();
    *dx = (fw > 0.0F) ? (px / fw * DESIGN_W) : px;
    *dy = (fh > 0.0F) ? ((fh - py) / fh * DESIGN_H) : py;
}

static void handle_input(void) {
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
            const nt_pointer_t p = g_nt_input.pointers[i];
            if (p.active) {
                float dx, dy;
                pointer_to_design(p.x, p.y, &dx, &dy);
                if (g_sim.won) {
                    playtest_reset(); /* click to replay after victory */
                } else {
                    hero_move_to(dx, dy);
                }
                break;
            }
        }
    }
}

/* ---- Scene composition (design units, y-up, bottom-left origin) ---- */

/* Soft dark ground-shadow ellipse (flattened white quad, low alpha) to anchor
 * a character to the ground. cy is the character's feet line. */
static void emit_ground_shadow(float cx, float cy, float w, float a) {
    emit_quad(cx, cy, w, w * 0.32F, 0.04F, 0.05F, 0.07F, a);
    emit_quad(cx, cy, w * 0.66F, w * 0.21F, 0.02F, 0.03F, 0.05F, a * 0.8F);
}

static void compose_scene(void) {
    const uint32_t white = 0xFFFFFFFFu;

    emit_sprite(R_BACKGROUND, DESIGN_W * 0.5F, DESIGN_H * 0.5F, DESIGN_W, DESIGN_H, white);

    /* Keep glows brighter once the path is clear (the goal beacon). */
    if (alive_enemies() == 0 && !g_sim.won) {
        emit_quad(KEEP_CX, KEEP_CY, 220.0F, 220.0F, 1.0F, 0.95F, 0.55F, 0.18F);
    }
    emit_h(R_KEEP, KEEP_CX, KEEP_CY, DESIGN_H * 0.40F, white);

    /* Foreground framing scenery. */
    emit_h(R_PINE, DESIGN_W * 0.08F, DESIGN_H * 0.18F, DESIGN_H * 0.46F, white);
    emit_h(R_PINE, DESIGN_W * 0.92F, DESIGN_H * 0.17F, DESIGN_H * 0.48F, white);
    emit_h(R_ROCK, DESIGN_W * 0.82F, DESIGN_H * 0.28F, DESIGN_H * 0.13F, white);

    /* Enemies (draw far-to-near by y so nearer ones overlap). */
    for (int pass = 0; pass < ENEMY_COUNT; ++pass) {
        /* simple painter sort: highest y first */
        int best = -1;
        float best_y = -1.0F;
        for (int i = 0; i < ENEMY_COUNT; ++i) {
            if (!g_sim.enemies[i].alive) {
                continue;
            }
            /* find the pass-th highest; cheap O(n^2) for 3 enemies */
            int rank = 0;
            for (int j = 0; j < ENEMY_COUNT; ++j) {
                if (g_sim.enemies[j].alive && g_sim.enemies[j].y > g_sim.enemies[i].y) {
                    rank++;
                }
            }
            if (rank == pass && g_sim.enemies[i].y > best_y - 1e6F) {
                best = i;
                best_y = g_sim.enemies[i].y;
                break;
            }
        }
        if (best < 0) {
            continue;
        }
        Enemy *e = &g_sim.enemies[best];
        /* Perspective scale: goblins lower on screen (nearer the hero) read
         * bigger; far ones up the path slightly smaller. Stays >= ~24% h so
         * every one reads clearly as a MONSTER, not a shrub. */
        float depth = clampf((e->y - DESIGN_H * 0.24F) / (DESIGN_H * 0.40F), 0.0F, 1.0F); /* 0 far .. 1 near */
        const float enemy_h = DESIGN_H * (0.245F + 0.055F * (1.0F - depth));
        const float feet = e->y - enemy_h * 0.42F;

        /* Ground shadow anchors the goblin to the path. */
        emit_ground_shadow(e->x, feet, enemy_h * 0.62F, 0.32F);

        /* Pulsing gold target ring under the foe the hero is currently fighting. */
        if (best == g_sim.attack_target) {
            float pulse = 0.5F + 0.5F * sinf(g_sim.time * 7.0F);
            float rs = enemy_h * (0.58F + 0.10F * pulse);
            float ra = 0.30F + 0.45F * pulse;
            emit_quad(e->x, feet, rs, rs * 0.42F, 1.0F, 0.82F, 0.20F, ra * 0.55F);
            emit_quad(e->x, feet, rs * 0.66F, rs * 0.27F, 1.0F, 0.92F, 0.45F, ra);
        }

        uint32_t tint = white;
        emit_h_flip(R_ENEMY, e->x, e->y, enemy_h, tint, e->facing_left ? VH_FLIP_X : 0);
        if (e->flash > 0.0F) {
            float a = clampf(e->flash / HIT_FLASH_TIME, 0.0F, 1.0F) * 0.7F;
            emit_quad(e->x, e->y, enemy_h * 0.7F, enemy_h, 1.0F, 1.0F, 1.0F, a);
        }
        /* Enemy hp pip above the goblin's head. */
        float frac = clampf((float)e->hp / (float)ENEMY_MAX_HP, 0.0F, 1.0F);
        float bw = 56.0F;
        float by = e->y + enemy_h * 0.60F;
        emit_quad(e->x, by, bw + 4.0F, 10.0F, 0.10F, 0.07F, 0.10F, 0.85F);
        emit_quad(e->x - bw * 0.5F + bw * frac * 0.5F, by, bw * frac, 6.0F, 0.95F, 0.30F, 0.28F, 1.0F);
    }

    /* Hero with level-up pop scale + burst. */
    float hero_h = DESIGN_H * 0.34F;
    float hero_feet = g_sim.hero_y - hero_h * 0.42F;
    float pop = 1.0F;
    /* idle bob when standing still */
    float bob = (!g_sim.moving && g_sim.downed_timer <= 0.0F) ? sinf(g_sim.time * 4.0F) * 3.0F : 0.0F;

    /* Hero ground shadow (skip while downed/faded). */
    if (g_sim.downed_timer <= 0.0F) {
        emit_ground_shadow(g_sim.hero_x, hero_feet, hero_h * 0.62F, 0.34F);
    }

    if (g_sim.levelup_timer > 0.0F) {
        float t = g_sim.levelup_timer / LEVELUP_POP_TIME; /* 1->0 */
        /* ease-out punch: big at the start, settling over ~1.3s */
        pop = 1.0F + 0.32F * t;
        /* Spinning level-up burst sprite BEHIND the hero, growing as it fades. */
        float burst = hero_h * (1.3F + 0.7F * (1.0F - t));
        float ba = clampf(t * 1.4F, 0.0F, 1.0F);
        emit_sprite_rot(R_LEVELUP_BURST, g_sim.hero_x, g_sim.hero_y + bob, burst, burst,
                        g_sim.time * 2.0F, pack_rgba(1.0F, 1.0F, 1.0F, ba));
    }

    uint32_t hero_tint = white;
    float hero_alpha = 1.0F;
    if (g_sim.downed_timer > 0.0F) {
        hero_alpha = 0.45F;
        hero_tint = pack_rgba(0.7F, 0.7F, 0.8F, hero_alpha);
    }
    emit_h_flip(R_HERO, g_sim.hero_x, g_sim.hero_y + bob, hero_h * pop, hero_tint, g_sim.hero_facing_left ? VH_FLIP_X : 0);
    if (g_sim.hero_flash > 0.0F) {
        float a = clampf(g_sim.hero_flash / HIT_FLASH_TIME, 0.0F, 1.0F) * 0.7F;
        emit_quad(g_sim.hero_x, g_sim.hero_y + bob, hero_h * 0.6F, hero_h * pop, 1.0F, 0.3F, 0.3F, a);
    }

    /* Sprite FX (hit sparks, coin pops) -- real atlas regions, drawn over actors. */
    for (int i = 0; i < MAX_SPRITE_FX; ++i) {
        SpriteFx *fx = &g_sim.sprite_fx[i];
        if (!fx->active) {
            continue;
        }
        float t = fx->age / fx->ttl;           /* 0->1 */
        float life = clampf(1.0F - t, 0.0F, 1.0F);
        float sz = fx->size;
        if (fx->region == R_HIT_SPARK) {
            sz = fx->size * (0.7F + 0.6F * t); /* spark expands as it fades */
        }
        emit_sprite_rot(fx->region, fx->x, fx->y, sz, sz, fx->spin,
                        pack_rgba(1.0F, 1.0F, 1.0F, clampf(life + 0.2F, 0.0F, 1.0F)));
    }

    /* Particles (gold sparkles). */
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle *p = &g_sim.particles[i];
        if (!p->active) {
            continue;
        }
        float life = 1.0F - p->age / p->ttl;
        float sz = p->size * (0.4F + 0.6F * life);
        emit_quad(p->x, p->y, sz, sz, p->r, p->g, p->b, clampf(life, 0.0F, 1.0F));
    }

    nt_sprite_renderer_flush();
}

/* ---- HUD layout (design units, y-up; shared by compose_hud + compose_text) ---- */
#define HUD_BADGE_CX 56.0F
#define HUD_BADGE_CY (DESIGN_H - 52.0F)
#define HUD_BADGE_H 76.0F
#define HUD_BAR_H 26.0F
#define HUD_BAR_W 232.0F
#define HUD_BAR_LEFT 98.0F                         /* bars start right of the badge */
#define HUD_BAR_CX (HUD_BAR_LEFT + HUD_BAR_W * 0.5F)
#define HUD_HP_CY (DESIGN_H - 34.0F)
#define HUD_XP_CY (DESIGN_H - 64.0F)
#define HUD_HOTBAR_CY 40.0F
#define HUD_SLOT_H 56.0F
#define HUD_SLOT_GAP 10.0F

/* A filled bar built from the EMPTY bar_frame art: draw the frame first (its
 * dark inner slot is the track), then a colored fill inset inside the slot so
 * the gold border stays visible and the bar fills left-to-right by value. */
static void emit_bar(int region, float cx, float cy, float w, float h, float frac, float r, float g, float b) {
    frac = clampf(frac, 0.0F, 1.0F);
    emit_sprite(region, cx, cy, w, h, 0xFFFFFFFFu); /* empty frame (slot + border) */
    const float inner_w = w - 22.0F;   /* inset past the frame border */
    const float inner_h = h - 12.0F;
    const float fill_w = inner_w * frac;
    const float fill_left = cx - inner_w * 0.5F;
    if (fill_w > 0.5F) {
        emit_quad(fill_left + fill_w * 0.5F, cy, fill_w, inner_h, r, g, b, 1.0F);
    }
}

static void compose_hud(void) {
    const uint32_t white = 0xFFFFFFFFu;

    /* Dark rounded pill plate behind the HP/XP numbers + bars for legibility. */
    emit_quad(HUD_BAR_CX + 6.0F, (HUD_HP_CY + HUD_XP_CY) * 0.5F, HUD_BAR_W + 64.0F, 84.0F, 0.07F, 0.06F, 0.10F, 0.70F);

    /* HP bar: red->orange->green by health fraction. */
    float hp_frac = clampf((float)g_game_state.run_hero_hp / (float)g_game_state.run_hero_max_hp, 0.0F, 1.0F);
    float hp_r = clampf(1.4F - hp_frac, 0.2F, 1.0F);
    float hp_g = clampf(0.3F + hp_frac, 0.3F, 0.95F);
    emit_bar(R_BAR_FRAME, HUD_BAR_CX, HUD_HP_CY, HUD_BAR_W, HUD_BAR_H, hp_frac, hp_r, hp_g, 0.28F);

    /* XP bar: repurpose the old "stamina" art; fill = xp/xp_to_next (gold). */
    float xp_frac = (g_game_state.run_xp_to_next > 0)
                        ? clampf((float)g_game_state.run_xp / (float)g_game_state.run_xp_to_next, 0.0F, 1.0F)
                        : 0.0F;
    emit_bar(R_BAR_FRAME, HUD_BAR_CX, HUD_XP_CY, HUD_BAR_W, HUD_BAR_H, xp_frac, 1.0F, 0.78F, 0.24F);

    /* Gold-star level badge, top-left (overlaps the plate's left edge). */
    emit_h(R_BADGE, HUD_BADGE_CX, HUD_BADGE_CY, HUD_BADGE_H, white);

    /* Minimap top-right. */
    emit_h(R_MINIMAP, DESIGN_W - 78.0F, DESIGN_H - 78.0F, 120.0F, white);

    /* Quest banner under the minimap. */
    emit_h(R_BANNER, DESIGN_W - 150.0F, DESIGN_H - 164.0F, 86.0F, white);

    /* Bottom-center 5-slot hotbar (reads as an action bar). */
    {
        const float slot_w = HUD_SLOT_H;
        const int slots = 5;
        const float total = slots * slot_w + (slots - 1) * HUD_SLOT_GAP;
        float x = DESIGN_W * 0.5F - total * 0.5F + slot_w * 0.5F;
        /* a subtle dark tray behind the slots */
        emit_quad(DESIGN_W * 0.5F, HUD_HOTBAR_CY, total + 24.0F, HUD_SLOT_H + 16.0F, 0.06F, 0.05F, 0.08F, 0.45F);
        for (int i = 0; i < slots; ++i) {
            emit_sprite(R_SLOT, x, HUD_HOTBAR_CY, slot_w, HUD_SLOT_H, white);
            if (i == 0) {
                /* primary action glyph (coin) so the bar reads as actionable */
                emit_sprite(R_COIN, x, HUD_HOTBAR_CY, slot_w * 0.62F, slot_w * 0.62F, white);
            }
            x += slot_w + HUD_SLOT_GAP;
        }
    }

    nt_sprite_renderer_flush();
}

/* Dim background panels for prompts/banners. Sprite pass (quads), so it MUST run
 * while the sprite material is bound -- text is drawn separately in compose_text. */
static const char *ftue_prompt(void) {
    if (g_sim.won) {
        return NULL;
    }
    /* Don't crowd the screen while the level-up reward pop is playing. */
    if (g_sim.levelup_timer > 0.0F) {
        return NULL;
    }
    if (g_game_state.run_ftue_step <= 0 || !g_sim.has_moved) {
        return "Tap anywhere to move";
    }
    if (g_game_state.run_ftue_step == 1) {
        return "Tap a monster to fight it";
    }
    if (g_game_state.run_ftue_step == 2) {
        return "Clear the path, then enter the keep";
    }
    return NULL;
}

static void compose_overlays(void) {
    const char *prompt = ftue_prompt();
    if (prompt) {
        emit_quad(DESIGN_W * 0.5F, 30.0F, text_width(prompt, 22.0F) + 40.0F, 38.0F, 0.06F, 0.05F, 0.08F, 0.55F);
    }
    if (g_sim.downed_timer > 0.0F) {
        emit_quad(DESIGN_W * 0.5F, DESIGN_H * 0.5F, 360.0F, 70.0F, 0.10F, 0.03F, 0.05F, 0.6F);
    }
    if (g_sim.won) {
        emit_quad(DESIGN_W * 0.5F, DESIGN_H * 0.55F - 14.0F, 600.0F, 150.0F, 0.06F, 0.05F, 0.10F, 0.66F);
    }
    nt_sprite_renderer_flush();
}

static void compose_floaters(void) {
    for (int i = 0; i < MAX_FLOATERS; ++i) {
        Floater *f = &g_sim.floaters[i];
        if (!f->active) {
            continue;
        }
        float life = 1.0F - f->age / f->ttl;
        float col[4] = {f->color[0], f->color[1], f->color[2], clampf(life + 0.2F, 0.0F, 1.0F)};
        emit_text_centered(f->text, f->x, f->y, f->size, col);
    }
}

static void compose_text(void) {
    const float ink[4] = {0.149F, 0.125F, 0.110F, 1.0F};
    const float gold[4] = {1.0F, 0.784F, 0.239F, 1.0F};
    const float cream[4] = {0.996F, 0.980F, 0.937F, 1.0F};

    /* Title (under the HUD plate, clear of the bars). */
    {
        const char *title = "VOXELHEIM";
        const float size = 34.0F;
        const float tx = 24.0F;
        const float ty = DESIGN_H - 128.0F;
        emit_text(title, tx + 2.0F, ty - 2.0F, size, ink);
        emit_text(title, tx, ty, size, gold);
    }

    /* "Lv N" on the gold-star badge (dark ink reads well on bright gold). */
    {
        char lvl[16];
        (void)snprintf(lvl, sizeof(lvl), "Lv %d", g_game_state.run_level);
        const float size = 19.0F;
        const float w = text_width(lvl, size);
        const float halo[4] = {1.0F, 0.94F, 0.65F, 0.9F}; /* light halo so dark text pops */
        emit_text(lvl, HUD_BADGE_CX - w * 0.5F - 1.0F, HUD_BADGE_CY - 7.0F + 1.0F, size, halo);
        emit_text(lvl, HUD_BADGE_CX - w * 0.5F, HUD_BADGE_CY - 7.0F, size, ink);
    }

    /* "HP  cur/max" label, centered on the HP bar. */
    {
        char hp[28];
        (void)snprintf(hp, sizeof(hp), "HP  %d/%d", g_game_state.run_hero_hp, g_game_state.run_hero_max_hp);
        const float size = 16.0F;
        const float w = text_width(hp, size);
        emit_text(hp, HUD_BAR_CX - w * 0.5F + 1.0F, HUD_HP_CY - 6.0F - 1.0F, size, ink);
        emit_text(hp, HUD_BAR_CX - w * 0.5F, HUD_HP_CY - 6.0F, size, cream);
    }

    /* "XP  cur/next" label, centered on the XP bar. */
    {
        char xp[28];
        (void)snprintf(xp, sizeof(xp), "XP  %d/%d", g_game_state.run_xp, g_game_state.run_xp_to_next);
        const float size = 16.0F;
        const float w = text_width(xp, size);
        emit_text(xp, HUD_BAR_CX - w * 0.5F + 1.0F, HUD_XP_CY - 6.0F - 1.0F, size, ink);
        emit_text(xp, HUD_BAR_CX - w * 0.5F, HUD_XP_CY - 6.0F, size, cream);
    }

    /* Objective banner text (phase-driven). */
    {
        int killed = g_game_state.run_enemies_defeated;
        int total = ENEMY_COUNT;
        char quest[48];
        if (g_sim.won) {
            (void)snprintf(quest, sizeof(quest), "Victory!");
        } else if (alive_enemies() == 0) {
            (void)snprintf(quest, sizeof(quest), "Enter the Frost Keep!");
        } else {
            (void)snprintf(quest, sizeof(quest), "Defeat monsters (%d/%d)", clampi(killed, 0, total), total);
        }
        const float size = 18.0F;
        const float bx = DESIGN_W - 150.0F;
        const float by = DESIGN_H - 170.0F;
        const float w = text_width(quest, size);
        emit_text(quest, bx - w * 0.5F + 1.0F, by - 1.0F, size, ink);
        emit_text(quest, bx - w * 0.5F, by, size, cream);
    }

    /* FTUE prompt (<=3 beats), centered near the bottom while not complete. */
    {
        const char *prompt = ftue_prompt();
        if (prompt) {
            emit_text_centered(prompt, DESIGN_W * 0.5F, 22.0F, 22.0F, cream);
        }
    }

    /* Downed banner. */
    if (g_sim.downed_timer > 0.0F) {
        const float red[4] = {1.0F, 0.45F, 0.45F, 1.0F};
        emit_text_centered("Downed!  Respawning...", DESIGN_W * 0.5F, DESIGN_H * 0.5F - 12.0F, 30.0F, red);
    }

    /* Victory banner + reward recap. */
    if (g_sim.won) {
        const float gold2[4] = {1.0F, 0.85F, 0.30F, 1.0F};
        emit_text_centered("FROST KEEP CLEARED!", DESIGN_W * 0.5F, DESIGN_H * 0.55F + 10.0F, 42.0F, gold2);
        char recap[48];
        (void)snprintf(recap, sizeof(recap), "Level %d  -  +%d XP", g_game_state.run_level,
                       g_game_state.run_enemies_defeated * XP_PER_KILL);
        emit_text_centered(recap, DESIGN_W * 0.5F, DESIGN_H * 0.55F - 26.0F, 24.0F, cream);
        emit_text_centered("Tap anywhere to play again", DESIGN_W * 0.5F, DESIGN_H * 0.55F - 58.0F, 20.0F, cream);
    }

    compose_floaters();
}

/* ---- Persistence ---- */

#define VOXELHEIM_SAVE_PATH "build/voxelheim_save.json"

static void load_persistent_state(void) {
    game_state_init_defaults(&g_game_state);
    if (s_fresh_state) {
        return;
    }
    char err[256] = {0};
    GameState loaded;
    if (game_state_load(&loaded, VOXELHEIM_SAVE_PATH, err, (int)sizeof(err))) {
        g_game_state = loaded;
    }
}

static void save_persistent_state(void) {
    if (!s_autosave) {
        return;
    }
    char err[256] = {0};
    (void)game_state_save(&g_game_state, VOXELHEIM_SAVE_PATH, err, (int)sizeof(err));
}

/* ---- DevAPI ---- */

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static cJSON *state_json(void) {
    cJSON *root = game_state_to_json(&g_game_state);
    cJSON_AddStringToObject(root, "runtime", "voxelheim");
    cJSON_AddStringToObject(root, "screen", "frost_keep_approach");
    cJSON_AddBoolToObject(root, "atlas_ready", s_atlas_resolved);

    /* Flat run mirror for easy probe assertions. */
    cJSON_AddNumberToObject(root, "hero_hp", g_game_state.run_hero_hp);
    cJSON_AddNumberToObject(root, "hero_max_hp", g_game_state.run_hero_max_hp);
    cJSON_AddNumberToObject(root, "level", g_game_state.run_level);
    cJSON_AddNumberToObject(root, "xp", g_game_state.run_xp);
    cJSON_AddNumberToObject(root, "xp_to_next", g_game_state.run_xp_to_next);
    cJSON_AddNumberToObject(root, "enemies_defeated", g_game_state.run_enemies_defeated);
    cJSON_AddNumberToObject(root, "enemies_alive", alive_enemies());
    cJSON_AddBoolToObject(root, "keep_reached", g_game_state.run_keep_reached);
    cJSON_AddNumberToObject(root, "ftue_step", g_game_state.run_ftue_step);
    cJSON_AddBoolToObject(root, "won", g_sim.won);
    cJSON_AddBoolToObject(root, "downed", g_sim.downed_timer > 0.0F);
    cJSON_AddNumberToObject(root, "hero_x", (double)g_sim.hero_x);
    cJSON_AddNumberToObject(root, "hero_y", (double)g_sim.hero_y);
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
    playtest_reset();
    *result = state_json();
    return true;
}

/* Drive a world move: params {x,y} in DESIGN units (0..960, 0..540, y-up).
 * Lets the headless probe walk the hero deterministically without fb/Y math. */
static bool ep_game_debug_click(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    double x = (double)DESIGN_W * 0.5;
    double y = (double)DESIGN_H * 0.5;
    if (params) {
        const cJSON *px = cJSON_GetObjectItemCaseSensitive(params, "x");
        const cJSON *py = cJSON_GetObjectItemCaseSensitive(params, "y");
        if (cJSON_IsNumber(px)) {
            x = px->valuedouble;
        }
        if (cJSON_IsNumber(py)) {
            y = py->valuedouble;
        }
    }
    if (g_sim.won) {
        playtest_reset();
    } else {
        hero_move_to((float)x, (float)y);
    }
    *result = state_json();
    return true;
}

/* Advance the simulation deterministically: params {seconds} (default 0.5s),
 * stepped at a fixed dt so combat/movement resolve the same headless or live. */
static bool ep_game_debug_tick(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    double seconds = 0.5;
    if (params) {
        const cJSON *s = cJSON_GetObjectItemCaseSensitive(params, "seconds");
        if (cJSON_IsNumber(s)) {
            seconds = s->valuedouble;
        }
    }
    if (seconds < 0.0) {
        seconds = 0.0;
    }
    if (seconds > 30.0) {
        seconds = 30.0; /* safety cap */
    }
    s_debug_driven = true; /* probe owns sim time from here on */
    const double fixed = 1.0 / 60.0;
    int steps = (int)(seconds / fixed + 0.5);
    for (int i = 0; i < steps; ++i) {
        update_sim((float)fixed);
    }
    *result = state_json();
    return true;
}

static bool ep_frame_screenshot(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    cJSON *root = cJSON_CreateObject();

    /* A request with an explicit "path" starts a NEW capture (resets prior
     * done/ok). A no-path call is a poll for the current/last capture status. */
    const cJSON *p = params ? cJSON_GetObjectItemCaseSensitive(params, "path") : NULL;
    const bool is_request = (p && cJSON_IsString(p));

    if (is_request && !s_shot_pending) {
        (void)snprintf(s_shot_path, sizeof(s_shot_path), "%s", p->valuestring);
        s_shot_done = false;
        s_shot_ok = false;
        s_shot_pending = true;
        cJSON_AddBoolToObject(root, "queued", true);
        cJSON_AddBoolToObject(root, "done", false);
        cJSON_AddStringToObject(root, "path", s_shot_path);
        *result = root;
        return true;
    }

    /* Poll (or request arriving while one is still in flight): report status. */
    cJSON_AddBoolToObject(root, "queued", s_shot_pending);
    cJSON_AddBoolToObject(root, "done", s_shot_done);
    cJSON_AddBoolToObject(root, "ok", s_shot_ok);
    cJSON_AddStringToObject(root, "path", s_shot_path);
    *result = root;
    return true;
}

static void register_game_endpoints(void) {
    nt_devapi_register_builtins();
    game_state_register_devapi();
    nt_devapi_register("game.state", ep_game_state, NULL);
    nt_devapi_register("game.reset_playtest", ep_game_reset_playtest, NULL);
    nt_devapi_register("game.debug.click", ep_game_debug_click, NULL);
    nt_devapi_register("game.debug.tick", ep_game_debug_tick, NULL);
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

    if (s_atlas_resolved && !g_sim.started) {
        sim_reset();
    }

    handle_input();
    if (!s_debug_driven) {
        update_sim(g_nt_app.target_dt);
    }

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

    /* Periodic autosave when progression changed. */
    if (game_state_is_dirty()) {
        save_persistent_state();
        game_state_clear_dirty();
    }

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

    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.247F, 0.717F, 1.0F, 1.0F}, .clear_depth = 1.0F});
    nt_font_step();

    if (can_render && !g_nt_gfx.context_restored) {
        nt_gfx_update_buffer(s_frame_ubo, &u, sizeof(u));
        nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);

        nt_sprite_renderer_set_material(s_sprite_mat);
        compose_scene();
        nt_sprite_renderer_set_material(s_sprite_mat);
        compose_hud();
        nt_sprite_renderer_set_material(s_sprite_mat);
        compose_overlays();

        if (text_info && text_info->ready) {
            compose_text();
        }
    }

    nt_gfx_end_pass();

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

    /* Load persistent progression before anything reads g_game_state. */
    load_persistent_state();

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
    save_persistent_state();
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
