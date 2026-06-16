/*
 * Voxelheim -- "Frost Keep Climb" IDLE / incremental auto-battle RPG.
 *
 * The hero stands on the path (lower-center) and AUTO-ATTACKS. An endless stream
 * of monsters spawns up the path and walks toward him; the front monster takes
 * damage, dies, drops GOLD, and the next advances. No player movement.
 *
 *   - Auto-combat stream: hero deals computed damage every computed interval.
 *   - Stages: kills_per_stage kills -> stage+1; monster HP/gold scale per stage.
 *   - Bosses: every boss.every_stages stages, one big timed monster.
 *   - Gold -> 4 upgrades (Sword/Boots/Armor/Luck) in a bottom UPGRADE PANEL.
 *   - Prestige (unlock @ stage 25): reset stage+gold+upgrades for Frost Shards,
 *     spend on permanent shard upgrades.
 *   - Offline (unlock after first boss): earn gold while away (capped).
 *   - FTUE (<=3 beats from balance.json).
 *
 * All economy numbers come from gamedesign/projects/voxelheim/data/balance.json;
 * they are mirrored as the VH_* constants below (single source of truth: keep in
 * sync with that file).
 *
 * Render path: nt_atlas + nt_sprite_renderer (direct emit), nt_text_renderer for
 * labels. A solid white atlas region (voxels/white.png) backs bar fills,
 * particles, glows, panels, and flash overlays.
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
#include <time.h>

#define VOXELHEIM_DEVAPI_PORT_DEFAULT 9123

/* Design canvas the composition is authored against. */
#define DESIGN_W 960.0F
#define DESIGN_H 540.0F

/* Flip flag (mirrors NT_SPRITE_FLAG_FLIP_X = 1U<<0; literal keeps deps local). */
#define VH_FLIP_X 1U

/* ============================================================================
 * Economy constants -- MIRROR gamedesign/projects/voxelheim/data/balance.json.
 * ==========================================================================*/

/* combat */
#define VH_HERO_BASE_DAMAGE 5.0
#define VH_HERO_ATTACK_INTERVAL_S 1.0
#define VH_HERO_BASE_MAX_HP 100.0
#define VH_KILLS_PER_STAGE 10

/* monster */
#define VH_MON_HP_BASE 10.0
#define VH_MON_HP_GROWTH 1.45
#define VH_MON_GOLD_BASE 5.0
#define VH_MON_GOLD_GROWTH 1.42

/* boss */
#define VH_BOSS_EVERY_STAGES 10
#define VH_BOSS_HP_MULT 8.0
#define VH_BOSS_GOLD_MULT 15.0
/* FIXED boss timer (v3). With MULTIPLICATIVE damage (Sword x1.12/level + prestige
 * x-damage) the hero's DPS compounds alongside monster HP, so the stage-10 boss
 * is beaten in ~6s, stage-20/30 in <1s -- all inside 30s. The old relative-timer
 * band-aid is removed. See boss.timer_note in data/balance.json. */
#define VH_BOSS_TIMER_S 30.0

/* upgrades */
#define VH_UP_COST_GROWTH 1.09
#define VH_SWORD_BASE_COST 10.0
/* MULTIPLICATIVE damage (v3): hero damage = base * VH_SWORD_DMG_MULT^sword_level.
 * x1.12/level => 1.12^3.28 = x1.45, so ~3-4 sword levels recover one stage's HP
 * x1.45 step, keeping flat-feeling purchases tracking the exponential HP climb. */
#define VH_SWORD_DMG_MULT 1.12
#define VH_BOOTS_BASE_COST 25.0
#define VH_BOOTS_INTERVAL_MULT 0.97
#define VH_BOOTS_MIN_INTERVAL 0.2
#define VH_ARMOR_BASE_COST 20.0
#define VH_ARMOR_HP_PER_LEVEL 12.0
#define VH_ARMOR_REGEN_PER_LEVEL 1.0
#define VH_LUCK_BASE_COST 30.0
#define VH_LUCK_GOLD_FIND_PCT 4.0

/* prestige */
#define VH_PRESTIGE_UNLOCK_STAGE 25
/* frost_shards = floor(highest_stage ^ 0.8 / 3).
 * Shard damage/gold apply MULTIPLICATIVELY (v3): mult = (1 + pct/100)^level, i.e.
 * +10%/level COMPOUNDING (Clicker Heroes Hero Souls), not a flat sum. */
#define VH_SHARD_DMG_PCT 10.0
#define VH_SHARD_DMG_BASE_COST 1.0
#define VH_SHARD_DMG_GROWTH 1.5
#define VH_SHARD_GOLD_PCT 10.0
#define VH_SHARD_GOLD_BASE_COST 1.0
#define VH_SHARD_GOLD_GROWTH 1.5
#define VH_SHARD_START_PER_LEVEL 1
#define VH_SHARD_START_BASE_COST 2.0
#define VH_SHARD_START_GROWTH 2.0
#define VH_SHARD_OFFLINE_PCT 5.0
#define VH_SHARD_OFFLINE_BASE_COST 2.0
#define VH_SHARD_OFFLINE_GROWTH 1.6

/* offline */
#define VH_OFFLINE_RATE_PCT 50.0
#define VH_OFFLINE_CAP_HOURS 8.0

/* ---- Sim tuning (presentation only; not economy) ---- */
#define MON_SPAWN_Y (DESIGN_H * 0.72F)  /* monsters appear up the path */
#define MON_FRONT_Y (DESIGN_H * 0.40F)  /* front monster engagement line */
#define MON_WALK_SPEED 70.0F            /* design units/sec down the path */
#define MON_SLOT_GAP 96.0F              /* spacing between queued monsters */
#define HIT_FLASH_TIME 0.1F
#define STAGE_FLASH_TIME 1.1F

#define MAX_MONSTERS 6                  /* visible queue depth */
#define MAX_PARTICLES 64
#define MAX_FLOATERS 24
#define MAX_SPRITE_FX 32

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
    R_SWORD_ICON,
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
    ASSET_ATLAS_REGION_VOXELS_SWORD_ICON_PNG,
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

/* ---- Upgrade indices ---- */
enum { UP_SWORD = 0, UP_BOOTS, UP_ARMOR, UP_LUCK, UP_COUNT };
enum { SH_DMG = 0, SH_GOLD, SH_START, SH_OFFLINE, SH_COUNT };

/* ---- Gameplay simulation state (transient; progression lives in g_game_state.idle) ---- */

typedef struct Floater {
    bool active;
    float x, y;
    float age;
    float ttl;
    char text[24];
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

/* One monster in the descending stream. */
typedef struct Monster {
    bool alive;
    float x, y;       /* design units */
    double hp;        /* current hp (double: scales large) */
    double max_hp;
    float flash;      /* hit-flash timer */
    bool is_boss;
} Monster;

/* Short-lived sprite FX (hit spark, coin pop). */
typedef struct SpriteFx {
    bool active;
    int region;
    float x, y;
    float vy;
    float age, ttl;
    float size;
    float spin;
    float spin_rate;
} SpriteFx;

static struct {
    Monster monsters[MAX_MONSTERS];
    Particle particles[MAX_PARTICLES];
    Floater floaters[MAX_FLOATERS];
    SpriteFx sprite_fx[MAX_SPRITE_FX];

    float hero_attack_cd;  /* seconds until next swing */
    float hero_flash;
    double regen_accum;    /* fractional hp regen accumulator */
    double gold_accum;     /* fractional gold accumulator (boss/regen rounding) */

    float boss_timer;      /* seconds remaining for the active boss */
    float boss_timer_max;  /* full duration of the active boss timer */
    float stage_flash;     /* stage-advance banner flash */

    float spawn_cooldown;  /* time until next monster enqueued */
    float time;            /* total sim time */
    bool started;

    /* offline grant pending presentation (popup). */
    bool offline_popup;
    long offline_gold;     /* gold granted while away (display) */
    double offline_hours;  /* capped hours (display) */

    /* prestige confirm state: a press arms confirm; second press commits. */
    bool prestige_armed;
    float prestige_armed_timer;
} g_sim;

/* Hero stands lower-center on the path. */
#define HERO_X (DESIGN_W * 0.50F)
#define HERO_Y (DESIGN_H * 0.27F)
#define KEEP_CX (DESIGN_W * 0.5F)
#define KEEP_CY (DESIGN_H * 0.72F)

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

/* Clamp a double down to the int32 storage range used by the state schema. */
static int gold_to_int(double v) {
    if (v < 0.0) return 0;
    if (v > 2147483647.0) return 2147483647;
    return (int)(v + 0.5);
}

/* Pretty number for the HUD (1.2k, 3.4M, ...). */
static void fmt_num(char *out, size_t cap, double v) {
    if (v < 1000.0) {
        (void)snprintf(out, cap, "%d", (int)(v + 0.5));
    } else if (v < 1.0e6) {
        (void)snprintf(out, cap, "%.1fk", v / 1.0e3);
    } else if (v < 1.0e9) {
        (void)snprintf(out, cap, "%.2fM", v / 1.0e6);
    } else if (v < 1.0e12) {
        (void)snprintf(out, cap, "%.2fB", v / 1.0e9);
    } else {
        (void)snprintf(out, cap, "%.2eT", v / 1.0e12);
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

/* Left-anchored text with a dark drop shadow for readability. */
static void emit_text_shadow(const char *s, float x, float y, float size, const float color[4]) {
    const float ink[4] = {0.10F, 0.08F, 0.07F, color[3]};
    emit_text(s, x + 2.0F, y - 2.0F, size, ink);
    emit_text(s, x, y, size, color);
}

/* Centered text with a dark drop shadow for readability. */
static void emit_text_centered(const char *s, float cx, float y, float size, const float color[4]) {
    const float w = text_width(s, size);
    emit_text_shadow(s, cx - w * 0.5F, y, size, color);
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

/* ============================================================================
 * Economy math (derived from balance.json constants + upgrade/shard levels).
 * ==========================================================================*/

static double ipow(double base, int exp) {
    double r = 1.0;
    for (int i = 0; i < exp; ++i) {
        r *= base;
    }
    return r;
}

/* Permanent shard multipliers (>=1.0), applied MULTIPLICATIVELY (v3):
 * mult = (1 + pct/100)^level, i.e. +10%/level COMPOUNDING (Clicker Heroes Hero
 * Souls), so the prestige bonus stacks on top of the multiplicative Sword and
 * the whole power curve shifts up -> the next run blasts the early stages. */
static double shard_damage_mult(void) {
    return pow(1.0 + VH_SHARD_DMG_PCT / 100.0, (double)g_game_state.idle_shard_global_damage);
}
static double shard_gold_mult(void) {
    return pow(1.0 + VH_SHARD_GOLD_PCT / 100.0, (double)g_game_state.idle_shard_global_gold);
}
static int shard_start_stage(void) {
    return 1 + g_game_state.idle_shard_start_stage * VH_SHARD_START_PER_LEVEL;
}
static double shard_offline_mult(void) {
    return 1.0 + (double)g_game_state.idle_shard_offline_rate * VH_SHARD_OFFLINE_PCT / 100.0;
}

/* Hero combat stats from upgrade levels + permanent shard multipliers.
 * MULTIPLICATIVE damage (v3): base * Sword_mult^sword_level * prestige damage
 * mult, so power COMPOUNDS and tracks the exponential HP climb. */
static double hero_damage(void) {
    double base = VH_HERO_BASE_DAMAGE * ipow(VH_SWORD_DMG_MULT, g_game_state.idle_up_sword);
    return base * shard_damage_mult();
}
static double hero_attack_interval(void) {
    double iv = VH_HERO_ATTACK_INTERVAL_S * ipow(VH_BOOTS_INTERVAL_MULT, g_game_state.idle_up_boots);
    if (iv < VH_BOOTS_MIN_INTERVAL) iv = VH_BOOTS_MIN_INTERVAL;
    return iv;
}
static int hero_max_hp(void) {
    return (int)(VH_HERO_BASE_MAX_HP + (double)g_game_state.idle_up_armor * VH_ARMOR_HP_PER_LEVEL);
}
static double hero_regen_per_sec(void) {
    return (double)g_game_state.idle_up_armor * VH_ARMOR_REGEN_PER_LEVEL;
}
static double gold_find_mult(void) {
    /* Luck % per level + permanent shard gold multiplier. */
    double luck = 1.0 + (double)g_game_state.idle_up_luck * VH_LUCK_GOLD_FIND_PCT / 100.0;
    return luck * shard_gold_mult();
}

/* Monster stats for a stage (per the per-stage step in balance.json). */
static double monster_hp_for_stage(int stage) {
    return VH_MON_HP_BASE * ipow(VH_MON_HP_GROWTH, stage - 1);
}
static double monster_gold_for_stage(int stage) {
    double g = VH_MON_GOLD_BASE * ipow(VH_MON_GOLD_GROWTH, stage - 1);
    return g * gold_find_mult();
}
static bool stage_is_boss(int stage) {
    return (stage % VH_BOSS_EVERY_STAGES) == 0;
}

/* Upgrade cost = base_cost * cost_growth^level. */
static double upgrade_cost(int which) {
    int lvl = 0;
    double base = 0.0;
    switch (which) {
        case UP_SWORD: lvl = g_game_state.idle_up_sword; base = VH_SWORD_BASE_COST; break;
        case UP_BOOTS: lvl = g_game_state.idle_up_boots; base = VH_BOOTS_BASE_COST; break;
        case UP_ARMOR: lvl = g_game_state.idle_up_armor; base = VH_ARMOR_BASE_COST; break;
        case UP_LUCK:  lvl = g_game_state.idle_up_luck;  base = VH_LUCK_BASE_COST;  break;
        default: return 0.0;
    }
    return base * ipow(VH_UP_COST_GROWTH, lvl);
}

/* Shard upgrade cost = base_cost_shards * cost_growth^level. */
static double shard_cost(int which) {
    int lvl = 0;
    double base = 0.0, growth = 1.5;
    switch (which) {
        case SH_DMG:     lvl = g_game_state.idle_shard_global_damage; base = VH_SHARD_DMG_BASE_COST;     growth = VH_SHARD_DMG_GROWTH;     break;
        case SH_GOLD:    lvl = g_game_state.idle_shard_global_gold;   base = VH_SHARD_GOLD_BASE_COST;    growth = VH_SHARD_GOLD_GROWTH;    break;
        case SH_START:   lvl = g_game_state.idle_shard_start_stage;   base = VH_SHARD_START_BASE_COST;   growth = VH_SHARD_START_GROWTH;   break;
        case SH_OFFLINE: lvl = g_game_state.idle_shard_offline_rate;  base = VH_SHARD_OFFLINE_BASE_COST; growth = VH_SHARD_OFFLINE_GROWTH; break;
        default: return 0.0;
    }
    return base * ipow(growth, lvl);
}

/* Frost shards earned from a prestige at the current highest stage. */
static int frost_shards_reward(void) {
    int hs = g_game_state.idle_highest_stage;
    double v = floor(pow((double)hs, 0.8) / 3.0);
    if (v < 0.0) v = 0.0;
    return (int)v;
}

static bool prestige_unlocked(void) {
    return g_game_state.idle_highest_stage >= VH_PRESTIGE_UNLOCK_STAGE;
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
            p->r = 1.0F;
            p->g = 0.85F;
            p->b = 0.35F;
            p->size = 6.0F + (float)((i * 7) % 6);
            spawned++;
        }
    }
}

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

static void spawn_hit_spark(float x, float y) {
    spawn_sprite_fx(R_HIT_SPARK, x, y, 64.0F, 0.22F, 30.0F, 0.0F);
}

static void spawn_coin_pop(float x, float y) {
    spawn_sprite_fx(R_COIN, x, y + 10.0F, 40.0F, 0.7F, 90.0F, 9.0F);
    spawn_sprite_fx(R_COIN, x - 18.0F, y + 4.0F, 30.0F, 0.6F, 70.0F, -8.0F);
}

/* ---- Sim/monster spawning ---- */

/* Highest queued y so a new monster spawns above the column. */
static float column_top_y(void) {
    float top = MON_FRONT_Y;
    for (int i = 0; i < MAX_MONSTERS; ++i) {
        if (g_sim.monsters[i].alive && g_sim.monsters[i].y > top) {
            top = g_sim.monsters[i].y;
        }
    }
    return top;
}

static int count_monsters(void) {
    int n = 0;
    for (int i = 0; i < MAX_MONSTERS; ++i) {
        if (g_sim.monsters[i].alive) n++;
    }
    return n;
}

static bool boss_present(void) {
    for (int i = 0; i < MAX_MONSTERS; ++i) {
        if (g_sim.monsters[i].alive && g_sim.monsters[i].is_boss) return true;
    }
    return false;
}

/* Spawn a normal monster for the current stage at the top of the column. */
static void spawn_monster(void) {
    if (g_game_state.idle_boss_active) {
        return; /* boss stage: a single boss occupies the path */
    }
    if (count_monsters() >= MAX_MONSTERS) {
        return;
    }
    for (int i = 0; i < MAX_MONSTERS; ++i) {
        if (!g_sim.monsters[i].alive) {
            Monster *m = &g_sim.monsters[i];
            m->alive = true;
            m->is_boss = false;
            m->max_hp = monster_hp_for_stage(g_game_state.idle_stage);
            m->hp = m->max_hp;
            m->flash = 0.0F;
            float top = column_top_y();
            float y = top + MON_SLOT_GAP;
            if (y < MON_SPAWN_Y) y = MON_SPAWN_Y;
            m->y = y;
            /* slight horizontal weave so the column does not read as a single bar */
            m->x = KEEP_CX + (((i % 2) == 0) ? -22.0F : 24.0F);
            return;
        }
    }
}

/* Spawn the boss for the current (boss) stage. */
static void spawn_boss(void) {
    for (int i = 0; i < MAX_MONSTERS; ++i) {
        g_sim.monsters[i].alive = false; /* clear the path for the boss */
    }
    Monster *m = &g_sim.monsters[0];
    m->alive = true;
    m->is_boss = true;
    m->max_hp = monster_hp_for_stage(g_game_state.idle_stage) * VH_BOSS_HP_MULT;
    m->hp = m->max_hp;
    m->flash = 0.0F;
    m->x = KEEP_CX;
    m->y = MON_FRONT_Y + 30.0F;
    /* FIXED timer (v3): with multiplicative damage the hero's DPS compounds with
     * monster HP, so bosses are beaten well inside 30s; the relative-timer
     * band-aid is removed. See boss.timer_note in data/balance.json. */
    g_sim.boss_timer = (float)VH_BOSS_TIMER_S;
    g_sim.boss_timer_max = (float)VH_BOSS_TIMER_S;
}

/* Enter the current stage: if it is a boss stage, start the boss; else seed the
 * stream. Call after stage changes. */
static void enter_stage(void) {
    g_game_state.idle_kills_in_stage = 0;
    if (stage_is_boss(g_game_state.idle_stage)) {
        g_game_state.idle_boss_active = true;
        spawn_boss();
    } else {
        g_game_state.idle_boss_active = false;
        g_sim.boss_timer = 0.0F;
        /* seed a couple of monsters so the screen reads immediately */
        for (int i = 0; i < MAX_MONSTERS; ++i) g_sim.monsters[i].alive = false;
        spawn_monster();
        spawn_monster();
        spawn_monster();
    }
    g_sim.stage_flash = STAGE_FLASH_TIME;
    game_state_mark_dirty();
}

static void advance_stage(void) {
    g_game_state.idle_stage += 1;
    if (g_game_state.idle_stage > g_game_state.idle_highest_stage) {
        g_game_state.idle_highest_stage = g_game_state.idle_stage;
    }
    enter_stage();
}

/* Grant gold (with rounding accumulator) and a coin pop. */
static void grant_gold(double amount, float x, float y, bool floater) {
    g_sim.gold_accum += amount;
    long whole = (long)g_sim.gold_accum;
    if (whole > 0) {
        g_sim.gold_accum -= (double)whole;
        double g = (double)g_game_state.idle_gold + (double)whole;
        g_game_state.idle_gold = gold_to_int(g);
    }
    if (floater) {
        char t[24];
        char n[16];
        fmt_num(n, sizeof(n), amount);
        (void)snprintf(t, sizeof(t), "+%s", n);
        const float gc[4] = {1.0F, 0.88F, 0.32F, 1.0F};
        spawn_floater(x, y + 18.0F, t, gc, 22.0F);
    }
    game_state_mark_dirty();
}

/* Kill the front monster: drop gold, advance the stage counter / boss. */
static void on_monster_killed(Monster *m) {
    spawn_sparkle(m->x, m->y);
    spawn_coin_pop(m->x, m->y);

    if (m->is_boss) {
        double g = monster_gold_for_stage(g_game_state.idle_stage) * VH_BOSS_GOLD_MULT;
        grant_gold(g, m->x, m->y, true);
        m->alive = false;
        g_game_state.idle_boss_active = false;
        g_sim.boss_timer = 0.0F;
        /* first boss cleared unlocks offline earnings */
        if (!g_game_state.idle_offline_unlocked) {
            g_game_state.idle_offline_unlocked = true;
            const float c[4] = {0.7F, 0.9F, 1.0F, 1.0F};
            spawn_floater(HERO_X, HERO_Y + 80.0F, "Offline unlocked!", c, 24.0F);
        }
        advance_stage();
        return;
    }

    double g = monster_gold_for_stage(g_game_state.idle_stage);
    grant_gold(g, m->x, m->y, true);
    m->alive = false;
    g_game_state.idle_kills_in_stage += 1;

    /* FTUE: first kill -> beat 2 (buy an upgrade). */
    if (g_game_state.run_ftue_step < 1) {
        g_game_state.run_ftue_step = 1;
    }

    if (g_game_state.idle_kills_in_stage >= VH_KILLS_PER_STAGE) {
        advance_stage();
    } else {
        spawn_monster(); /* keep the stream full */
    }
    game_state_mark_dirty();
}

/* ---- Upgrade / prestige actions ---- */

static bool buy_upgrade(int which) {
    if (which < 0 || which >= UP_COUNT) return false;
    double cost = upgrade_cost(which);
    if ((double)g_game_state.idle_gold < cost) return false;
    g_game_state.idle_gold = gold_to_int((double)g_game_state.idle_gold - cost);
    switch (which) {
        case UP_SWORD: g_game_state.idle_up_sword += 1; break;
        case UP_BOOTS: g_game_state.idle_up_boots += 1; break;
        case UP_ARMOR: g_game_state.idle_up_armor += 1; break;
        case UP_LUCK:  g_game_state.idle_up_luck += 1; break;
        default: break;
    }
    /* keep hero hp consistent with new max */
    g_game_state.run_hero_max_hp = hero_max_hp();
    if (g_game_state.run_hero_hp > g_game_state.run_hero_max_hp) {
        g_game_state.run_hero_hp = g_game_state.run_hero_max_hp;
    }
    /* FTUE: first purchase -> beat 3 once prestige is reachable. */
    if (g_game_state.run_ftue_step < 2) {
        g_game_state.run_ftue_step = 2;
    }
    const float c[4] = {0.7F, 1.0F, 0.8F, 1.0F};
    spawn_floater(HERO_X, HERO_Y + 70.0F, "Upgrade!", c, 22.0F);
    game_state_mark_dirty();
    return true;
}

static bool buy_shard_upgrade(int which) {
    if (which < 0 || which >= SH_COUNT) return false;
    double cost = shard_cost(which);
    if ((double)g_game_state.idle_frost_shards < cost) return false;
    g_game_state.idle_frost_shards -= (int)(cost + 0.5);
    switch (which) {
        case SH_DMG:     g_game_state.idle_shard_global_damage += 1; break;
        case SH_GOLD:    g_game_state.idle_shard_global_gold += 1; break;
        case SH_START:   g_game_state.idle_shard_start_stage += 1; break;
        case SH_OFFLINE: g_game_state.idle_shard_offline_rate += 1; break;
        default: break;
    }
    game_state_mark_dirty();
    return true;
}

static void sim_seed_stage(void); /* fwd */

/* Prestige: reset stage+gold+the 4 upgrades, grant Frost Shards. */
static bool do_prestige(void) {
    if (!prestige_unlocked()) return false;
    int reward = frost_shards_reward();
    g_game_state.idle_frost_shards += reward;

    /* reset run economy */
    g_game_state.idle_gold = 0;
    g_game_state.idle_up_sword = 0;
    g_game_state.idle_up_boots = 0;
    g_game_state.idle_up_armor = 0;
    g_game_state.idle_up_luck = 0;
    g_game_state.idle_stage = shard_start_stage();
    if (g_game_state.idle_stage > g_game_state.idle_highest_stage) {
        g_game_state.idle_highest_stage = g_game_state.idle_stage;
    }
    g_game_state.idle_kills_in_stage = 0;
    g_game_state.idle_boss_active = false;

    g_game_state.run_hero_max_hp = hero_max_hp();
    g_game_state.run_hero_hp = g_game_state.run_hero_max_hp;

    g_sim.gold_accum = 0.0;
    g_sim.regen_accum = 0.0;
    g_sim.prestige_armed = false;
    g_sim.prestige_armed_timer = 0.0F;

    char t[24];
    (void)snprintf(t, sizeof(t), "+%d Frost Shards", reward);
    const float c[4] = {0.65F, 0.9F, 1.0F, 1.0F};
    spawn_floater(HERO_X, HERO_Y + 90.0F, t, c, 26.0F);
    spawn_sparkle(HERO_X, HERO_Y + 30.0F);

    sim_seed_stage();
    game_state_mark_dirty();
    return true;
}

/* ---- Offline earnings ---- */

/* Compute + grant offline gold from a last-seen timestamp. Returns granted. */
static long compute_offline_grant(long now_unix) {
    if (!g_game_state.idle_offline_unlocked) return 0;
    long last = (long)g_game_state.idle_last_seen_unix;
    if (last <= 0 || now_unix <= last) return 0;
    double elapsed_s = (double)(now_unix - last);
    double cap_s = VH_OFFLINE_CAP_HOURS * 3600.0;
    if (elapsed_s > cap_s) elapsed_s = cap_s;

    /* Rate = gold/sec at the highest cleared stage's clear rate.
     * Per kill at that stage * kills/sec (one kill per attack interval),
     * times the offline rate % and the permanent offline shard mult. */
    int clear_stage = g_game_state.idle_highest_stage;
    double gold_per_kill = monster_gold_for_stage(clear_stage);
    double kills_per_sec = 1.0 / hero_attack_interval();
    double rate = gold_per_kill * kills_per_sec * (VH_OFFLINE_RATE_PCT / 100.0) * shard_offline_mult();
    double grant = rate * elapsed_s;
    if (grant < 0.0) grant = 0.0;
    /* Clamp in DOUBLE to the int32 gold range before any narrowing cast: with the
     * multiplicative economy, highest_stage climbs high enough that the raw grant
     * exceeds 2^31 and a direct (long) cast wraps to INT_MIN. */
    if (grant > 2147483647.0) grant = 2147483647.0;
    long g = (long)grant;
    if (g > 0) {
        double total = (double)g_game_state.idle_gold + (double)g;
        g_game_state.idle_gold = gold_to_int(total);
        g_sim.offline_hours = elapsed_s / 3600.0;
        game_state_mark_dirty();
    }
    return g;
}

/* ---- Sim lifecycle ---- */

/* Reset the visible sim for the current persistent stage. */
static void sim_seed_stage(void) {
    for (int i = 0; i < MAX_MONSTERS; ++i) g_sim.monsters[i].alive = false;
    for (int i = 0; i < MAX_PARTICLES; ++i) g_sim.particles[i].active = false;
    for (int i = 0; i < MAX_FLOATERS; ++i) g_sim.floaters[i].active = false;
    for (int i = 0; i < MAX_SPRITE_FX; ++i) g_sim.sprite_fx[i].active = false;
    g_sim.hero_attack_cd = (float)hero_attack_interval();
    g_sim.hero_flash = 0.0F;
    g_sim.spawn_cooldown = 0.0F;
    g_sim.boss_timer = 0.0F;
    g_sim.stage_flash = 0.0F;

    g_game_state.run_hero_max_hp = hero_max_hp();
    if (g_game_state.run_hero_hp <= 0 || g_game_state.run_hero_hp > g_game_state.run_hero_max_hp) {
        g_game_state.run_hero_hp = g_game_state.run_hero_max_hp;
    }

    if (stage_is_boss(g_game_state.idle_stage)) {
        g_game_state.idle_boss_active = true;
        spawn_boss();
    } else {
        g_game_state.idle_boss_active = false;
        spawn_monster();
        spawn_monster();
        spawn_monster();
    }
}

static void sim_reset(void) {
    memset(&g_sim, 0, sizeof(g_sim));
    g_sim.started = true;
    g_sim.time = 0.0F;
    sim_seed_stage();
}

/* Full new idle profile (devapi reset for tests). */
static void playtest_reset(void) {
    /* reset persistent idle economy to defaults */
    g_game_state.idle_gold = 0;
    g_game_state.idle_stage = 1;
    g_game_state.idle_highest_stage = 1;
    g_game_state.idle_kills_in_stage = 0;
    g_game_state.idle_up_sword = 0;
    g_game_state.idle_up_boots = 0;
    g_game_state.idle_up_armor = 0;
    g_game_state.idle_up_luck = 0;
    g_game_state.idle_frost_shards = 0;
    g_game_state.idle_shard_global_damage = 0;
    g_game_state.idle_shard_global_gold = 0;
    g_game_state.idle_shard_start_stage = 0;
    g_game_state.idle_shard_offline_rate = 0;
    g_game_state.idle_last_seen_unix = 0;
    g_game_state.idle_offline_unlocked = false;
    g_game_state.idle_boss_active = false;
    g_game_state.run_ftue_step = 0;
    g_game_state.run_hero_max_hp = hero_max_hp();
    g_game_state.run_hero_hp = g_game_state.run_hero_max_hp;
    game_state_mark_dirty();
    sim_reset();
}

/* ---- Effects update ---- */

static void update_effects(float dt) {
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle *p = &g_sim.particles[i];
        if (!p->active) continue;
        p->age += dt;
        if (p->age >= p->ttl) { p->active = false; continue; }
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vy -= 160.0F * dt;
    }
    for (int i = 0; i < MAX_FLOATERS; ++i) {
        Floater *f = &g_sim.floaters[i];
        if (!f->active) continue;
        f->age += dt;
        if (f->age >= f->ttl) { f->active = false; continue; }
        f->y += 38.0F * dt;
    }
    for (int i = 0; i < MAX_SPRITE_FX; ++i) {
        SpriteFx *fx = &g_sim.sprite_fx[i];
        if (!fx->active) continue;
        fx->age += dt;
        if (fx->age >= fx->ttl) { fx->active = false; continue; }
        fx->y += fx->vy * dt;
        fx->vy -= 80.0F * dt;
        fx->spin += fx->spin_rate * dt;
    }
}

/* ---- Core idle simulation ---- */

static void update_sim(float dt) {
    if (!g_sim.started) return;
    if (dt <= 0.0F) dt = 1.0F / 60.0F;
    if (dt > 0.1F) dt = 0.1F; /* clamp big steps so debug ticks stay stable */
    g_sim.time += dt;

    if (g_sim.hero_flash > 0.0F) g_sim.hero_flash -= dt;
    if (g_sim.stage_flash > 0.0F) g_sim.stage_flash -= dt;
    if (g_sim.prestige_armed_timer > 0.0F) {
        g_sim.prestige_armed_timer -= dt;
        if (g_sim.prestige_armed_timer <= 0.0F) g_sim.prestige_armed = false;
    }
    update_effects(dt);

    const bool boss = g_game_state.idle_boss_active;

    /* Boss countdown. Timeout -> retry boss (no progress loss). */
    if (boss && boss_present()) {
        g_sim.boss_timer -= dt;
        if (g_sim.boss_timer <= 0.0F) {
            spawn_boss(); /* retry: fresh full-hp boss + fresh timer */
            const float c[4] = {1.0F, 0.5F, 0.5F, 1.0F};
            spawn_floater(HERO_X, HERO_Y + 70.0F, "Boss retry!", c, 24.0F);
        }
    }

    /* Monster stream advance: front monster walks to the engagement line, the
     * rest follow at slot spacing. */
    {
        /* find the front (lowest-y) alive monster */
        int front = -1;
        float front_y = 1e9F;
        for (int i = 0; i < MAX_MONSTERS; ++i) {
            if (g_sim.monsters[i].alive && g_sim.monsters[i].y < front_y) {
                front_y = g_sim.monsters[i].y;
                front = i;
            }
        }
        (void)front_y;
        for (int i = 0; i < MAX_MONSTERS; ++i) {
            Monster *m = &g_sim.monsters[i];
            if (!m->alive) continue;
            if (m->flash > 0.0F) m->flash -= dt;
            /* Each monster marches down toward the front line but stops a
             * slot-gap behind whoever is ahead of it (cheap monotone push). */
            int ahead = 0;
            for (int j = 0; j < MAX_MONSTERS; ++j) {
                if (j != i && g_sim.monsters[j].alive && g_sim.monsters[j].y < m->y) ahead++;
            }
            float want = MON_FRONT_Y + MON_SLOT_GAP * (float)ahead;
            if (m->y > want) {
                m->y -= MON_WALK_SPEED * dt;
                if (m->y < want) m->y = want;
            }
        }

        /* Hero auto-attacks the front monster once it is at/near the line. */
        if (g_sim.hero_attack_cd > 0.0F) g_sim.hero_attack_cd -= dt;
        if (front >= 0) {
            Monster *m = &g_sim.monsters[front];
            bool engaged = (m->y <= MON_FRONT_Y + 4.0F);
            if (engaged && g_sim.hero_attack_cd <= 0.0F) {
                g_sim.hero_attack_cd = (float)hero_attack_interval();
                double dmg = hero_damage();
                m->hp -= dmg;
                m->flash = HIT_FLASH_TIME;
                spawn_hit_spark(m->x, m->y + 6.0F);
                char d[16];
                char n[16];
                fmt_num(n, sizeof(n), dmg);
                (void)snprintf(d, sizeof(d), "-%s", n);
                const float dc[4] = {1.0F, 1.0F, 1.0F, 1.0F};
                spawn_floater(m->x + 24.0F, m->y + 18.0F, d, dc, 20.0F);
                if (m->hp <= 0.0) {
                    on_monster_killed(m);
                }
            }
        }
    }

    /* Keep the (non-boss) stream topped up. */
    if (!g_game_state.idle_boss_active) {
        if (g_sim.spawn_cooldown > 0.0F) g_sim.spawn_cooldown -= dt;
        if (count_monsters() < MAX_MONSTERS && g_sim.spawn_cooldown <= 0.0F) {
            spawn_monster();
            g_sim.spawn_cooldown = 0.6F;
        }
    }

    /* Hero passive regen (armor). */
    double regen = hero_regen_per_sec();
    if (regen > 0.0 && g_game_state.run_hero_hp < g_game_state.run_hero_max_hp) {
        g_sim.regen_accum += regen * (double)dt;
        if (g_sim.regen_accum >= 1.0) {
            int gain = (int)g_sim.regen_accum;
            g_sim.regen_accum -= (double)gain;
            g_game_state.run_hero_hp = clampi(g_game_state.run_hero_hp + gain, 0, g_game_state.run_hero_max_hp);
            game_state_mark_dirty();
        }
    }
}

/* ---- Input: click the upgrade panel / prestige / collect popup ---- */

/* Upgrade panel layout (design units, y-up). */
#define PANEL_SLOT_W 210.0F
#define PANEL_SLOT_H 64.0F
#define PANEL_GAP 12.0F
#define PANEL_CY 46.0F

static void panel_slot_rect(int i, float *cx, float *cy, float *w, float *h) {
    const int slots = UP_COUNT;
    const float total = slots * PANEL_SLOT_W + (slots - 1) * PANEL_GAP;
    float x0 = DESIGN_W * 0.5F - total * 0.5F + PANEL_SLOT_W * 0.5F;
    *cx = x0 + (float)i * (PANEL_SLOT_W + PANEL_GAP);
    *cy = PANEL_CY;
    *w = PANEL_SLOT_W;
    *h = PANEL_SLOT_H;
}

/* Prestige button rect (top-center banner area), shown only when unlocked. */
static void prestige_btn_rect(float *cx, float *cy, float *w, float *h) {
    *cx = DESIGN_W * 0.5F;
    *cy = DESIGN_H - 40.0F;
    *w = 240.0F;
    *h = 44.0F;
}

/* Collect-offline button rect (centered popup). */
static void collect_btn_rect(float *cx, float *cy, float *w, float *h) {
    *cx = DESIGN_W * 0.5F;
    *cy = DESIGN_H * 0.5F - 36.0F;
    *w = 200.0F;
    *h = 44.0F;
}

static bool in_rect(float px, float py, float cx, float cy, float w, float h) {
    return px >= cx - w * 0.5F && px <= cx + w * 0.5F && py >= cy - h * 0.5F && py <= cy + h * 0.5F;
}

/* Route a design-space click to a UI action. Returns true if consumed. */
static bool handle_world_click(float px, float py) {
    /* offline popup eats clicks first */
    if (g_sim.offline_popup) {
        float cx, cy, w, h;
        collect_btn_rect(&cx, &cy, &w, &h);
        g_sim.offline_popup = false; /* any click dismisses/collects (gold already added) */
        (void)px; (void)py; (void)cx; (void)cy; (void)w; (void)h;
        return true;
    }
    /* prestige button */
    if (prestige_unlocked()) {
        float cx, cy, w, h;
        prestige_btn_rect(&cx, &cy, &w, &h);
        if (in_rect(px, py, cx, cy, w, h)) {
            if (g_sim.prestige_armed) {
                do_prestige();
            } else {
                g_sim.prestige_armed = true;
                g_sim.prestige_armed_timer = 3.0F;
                const float c[4] = {1.0F, 0.85F, 0.4F, 1.0F};
                spawn_floater(DESIGN_W * 0.5F, DESIGN_H - 70.0F, "Tap again to confirm", c, 20.0F);
            }
            return true;
        }
    }
    /* upgrade panel */
    for (int i = 0; i < UP_COUNT; ++i) {
        float cx, cy, w, h;
        panel_slot_rect(i, &cx, &cy, &w, &h);
        if (in_rect(px, py, cx, cy, w, h)) {
            buy_upgrade(i);
            return true;
        }
    }
    return false;
}

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
                handle_world_click(dx, dy);
                break;
            }
        }
    }
}

/* ============================================================================
 * Scene composition (design units, y-up, bottom-left origin).
 * ==========================================================================*/

static void emit_ground_shadow(float cx, float cy, float w, float a) {
    emit_quad(cx, cy, w, w * 0.32F, 0.04F, 0.05F, 0.07F, a);
    emit_quad(cx, cy, w * 0.66F, w * 0.21F, 0.02F, 0.03F, 0.05F, a * 0.8F);
}

static void compose_scene(void) {
    const uint32_t white = 0xFFFFFFFFu;

    emit_sprite(R_BACKGROUND, DESIGN_W * 0.5F, DESIGN_H * 0.5F, DESIGN_W, DESIGN_H, white);

    /* The Frost Keep sits at the top of the path (the climb goal). */
    emit_h(R_KEEP, KEEP_CX, KEEP_CY, DESIGN_H * 0.36F, white);

    /* Framing scenery. */
    emit_h(R_PINE, DESIGN_W * 0.07F, DESIGN_H * 0.20F, DESIGN_H * 0.46F, white);
    emit_h(R_PINE, DESIGN_W * 0.93F, DESIGN_H * 0.18F, DESIGN_H * 0.48F, white);
    emit_h(R_ROCK, DESIGN_W * 0.83F, DESIGN_H * 0.30F, DESIGN_H * 0.13F, white);

    /* Monsters: draw far-to-near (highest y first) so nearer overlap. */
    for (int pass = 0; pass < MAX_MONSTERS; ++pass) {
        int best = -1;
        for (int i = 0; i < MAX_MONSTERS; ++i) {
            if (!g_sim.monsters[i].alive) continue;
            int rank = 0;
            for (int j = 0; j < MAX_MONSTERS; ++j) {
                if (g_sim.monsters[j].alive && g_sim.monsters[j].y > g_sim.monsters[i].y) rank++;
            }
            if (rank == pass) { best = i; break; }
        }
        if (best < 0) continue;
        Monster *m = &g_sim.monsters[best];

        float depth = clampf((m->y - MON_FRONT_Y) / (MON_SPAWN_Y - MON_FRONT_Y), 0.0F, 1.0F); /* 0 near .. 1 far */
        float base_h = m->is_boss ? (DESIGN_H * 0.42F) : (DESIGN_H * 0.245F);
        float enemy_h = base_h * (1.0F - 0.30F * depth);
        float feet = m->y - enemy_h * 0.42F;

        emit_ground_shadow(m->x, feet, enemy_h * 0.62F, 0.32F);

        /* Target ring under the engaged front monster. */
        if (best == 0 || m->y <= MON_FRONT_Y + 6.0F) {
            float pulse = 0.5F + 0.5F * sinf(g_sim.time * 7.0F);
            float rs = enemy_h * (0.58F + 0.10F * pulse);
            float ra = 0.30F + 0.40F * pulse;
            emit_quad(m->x, feet, rs, rs * 0.42F, 1.0F, 0.82F, 0.20F, ra * 0.5F);
        }

        uint32_t tint = m->is_boss ? pack_rgba(1.0F, 0.55F, 0.55F, 1.0F) : white;
        emit_h(R_ENEMY, m->x, m->y, enemy_h, tint);
        if (m->flash > 0.0F) {
            float a = clampf(m->flash / HIT_FLASH_TIME, 0.0F, 1.0F) * 0.7F;
            emit_quad(m->x, m->y, enemy_h * 0.7F, enemy_h, 1.0F, 1.0F, 1.0F, a);
        }
        /* HP pip above the monster. */
        float frac = (m->max_hp > 0.0) ? clampf((float)(m->hp / m->max_hp), 0.0F, 1.0F) : 0.0F;
        float bw = m->is_boss ? 120.0F : 56.0F;
        float by = m->y + enemy_h * 0.60F;
        emit_quad(m->x, by, bw + 4.0F, 10.0F, 0.10F, 0.07F, 0.10F, 0.85F);
        emit_quad(m->x - bw * 0.5F + bw * frac * 0.5F, by, bw * frac, 6.0F, 0.95F, 0.30F, 0.28F, 1.0F);
    }

    /* Hero (stationary, idle bob). */
    float hero_h = DESIGN_H * 0.34F;
    float hero_feet = HERO_Y - hero_h * 0.42F;
    float bob = sinf(g_sim.time * 4.0F) * 3.0F;
    emit_ground_shadow(HERO_X, hero_feet, hero_h * 0.62F, 0.34F);
    /* face up the path toward the monsters (flip so the sword faces them). */
    emit_h(R_HERO, HERO_X, HERO_Y + bob, hero_h, white);
    if (g_sim.hero_flash > 0.0F) {
        float a = clampf(g_sim.hero_flash / HIT_FLASH_TIME, 0.0F, 1.0F) * 0.7F;
        emit_quad(HERO_X, HERO_Y + bob, hero_h * 0.6F, hero_h, 1.0F, 0.3F, 0.3F, a);
    }

    /* Sprite FX (hit sparks, coin pops). */
    for (int i = 0; i < MAX_SPRITE_FX; ++i) {
        SpriteFx *fx = &g_sim.sprite_fx[i];
        if (!fx->active) continue;
        float t = fx->age / fx->ttl;
        float life = clampf(1.0F - t, 0.0F, 1.0F);
        float sz = fx->size;
        if (fx->region == R_HIT_SPARK) sz = fx->size * (0.7F + 0.6F * t);
        emit_sprite_rot(fx->region, fx->x, fx->y, sz, sz, fx->spin,
                        pack_rgba(1.0F, 1.0F, 1.0F, clampf(life + 0.2F, 0.0F, 1.0F)));
    }

    /* Particles (gold sparkles). */
    for (int i = 0; i < MAX_PARTICLES; ++i) {
        Particle *p = &g_sim.particles[i];
        if (!p->active) continue;
        float life = 1.0F - p->age / p->ttl;
        float sz = p->size * (0.4F + 0.6F * life);
        emit_quad(p->x, p->y, sz, sz, p->r, p->g, p->b, clampf(life, 0.0F, 1.0F));
    }

    nt_sprite_renderer_flush();
}

/* ---- HUD: top counters, upgrade panel, prestige, popups ---- */

static int upgrade_icon_region(int i) {
    switch (i) {
        case UP_SWORD: return R_SWORD_ICON;
        case UP_BOOTS: return R_ROCK;   /* boots stand-in (free-asset reuse) */
        case UP_ARMOR: return R_KEEP;   /* armor stand-in */
        case UP_LUCK:  return R_COIN;   /* luck = gold find */
        default: return R_SLOT;
    }
}

static const char *upgrade_label(int i) {
    switch (i) {
        case UP_SWORD: return "Sword";
        case UP_BOOTS: return "Boots";
        case UP_ARMOR: return "Armor";
        case UP_LUCK:  return "Luck";
        default: return "?";
    }
}

/* Current-effect short string for an upgrade (drives the panel readout). */
static void upgrade_effect_str(int i, char *out, size_t cap) {
    switch (i) {
        case UP_SWORD:
            (void)snprintf(out, cap, "DMG %.0f", hero_damage());
            break;
        case UP_BOOTS:
            (void)snprintf(out, cap, "%.2fs/hit", hero_attack_interval());
            break;
        case UP_ARMOR:
            (void)snprintf(out, cap, "HP %d", hero_max_hp());
            break;
        case UP_LUCK:
            (void)snprintf(out, cap, "+%.0f%% gold", (gold_find_mult() - 1.0) * 100.0);
            break;
        default: out[0] = 0; break;
    }
}

static int upgrade_level(int i) {
    switch (i) {
        case UP_SWORD: return g_game_state.idle_up_sword;
        case UP_BOOTS: return g_game_state.idle_up_boots;
        case UP_ARMOR: return g_game_state.idle_up_armor;
        case UP_LUCK:  return g_game_state.idle_up_luck;
        default: return 0;
    }
}

static void compose_hud(void) {
    const uint32_t white = 0xFFFFFFFFu;

    /* Top-left status plate: Gold + Stage + Shards counters. */
    emit_quad(176.0F, DESIGN_H - 40.0F, 320.0F, 64.0F, 0.07F, 0.06F, 0.10F, 0.72F);
    emit_h(R_COIN, 36.0F, DESIGN_H - 30.0F, 30.0F, white);
    emit_h(R_BADGE, 36.0F, DESIGN_H - 62.0F, 40.0F, white);

    /* Minimap top-right (progress flavor). */
    emit_h(R_MINIMAP, DESIGN_W - 70.0F, DESIGN_H - 70.0F, 110.0F, white);

    /* Stage progress bar under the top plate (kills toward next stage). */
    {
        float boss_max = (g_sim.boss_timer_max > 0.0F) ? g_sim.boss_timer_max : (float)VH_BOSS_TIMER_S;
        float frac = g_game_state.idle_boss_active
                         ? clampf(g_sim.boss_timer / boss_max, 0.0F, 1.0F)
                         : clampf((float)g_game_state.idle_kills_in_stage / (float)VH_KILLS_PER_STAGE, 0.0F, 1.0F);
        float bx = 176.0F, by = DESIGN_H - 80.0F, bw = 300.0F, bh = 16.0F;
        emit_quad(bx, by, bw + 6.0F, bh + 6.0F, 0.05F, 0.04F, 0.07F, 0.8F);
        float r = g_game_state.idle_boss_active ? 1.0F : 0.35F;
        float g = g_game_state.idle_boss_active ? 0.55F : 0.85F;
        float b = g_game_state.idle_boss_active ? 0.30F : 0.45F;
        emit_quad(bx - bw * 0.5F + bw * frac * 0.5F, by, bw * frac, bh, r, g, b, 1.0F);
    }

    /* ---- Bottom upgrade panel: 4 buttons (icon + label + effect + cost). ---- */
    {
        const int slots = UP_COUNT;
        const float total = slots * PANEL_SLOT_W + (slots - 1) * PANEL_GAP;
        /* tray */
        emit_quad(DESIGN_W * 0.5F, PANEL_CY, total + 28.0F, PANEL_SLOT_H + 22.0F, 0.06F, 0.05F, 0.08F, 0.55F);
        for (int i = 0; i < slots; ++i) {
            float cx, cy, w, h;
            panel_slot_rect(i, &cx, &cy, &w, &h);
            bool affordable = (double)g_game_state.idle_gold >= upgrade_cost(i);
            /* button background: bright if affordable, dim if not. */
            emit_sprite(R_BUTTON, cx, cy, w, h, affordable ? white : pack_rgba(0.55F, 0.55F, 0.6F, 1.0F));
            /* affordable pulse ring */
            if (affordable) {
                float pulse = 0.5F + 0.5F * sinf(g_sim.time * 5.0F + (float)i);
                emit_quad(cx, cy, w + 6.0F, h + 6.0F, 1.0F, 0.9F, 0.4F, 0.10F + 0.18F * pulse);
            }
            /* icon on the left */
            emit_h(upgrade_icon_region(i), cx - w * 0.5F + 26.0F, cy + 6.0F, 36.0F, white);
        }
    }

    /* ---- Prestige button (top-center) when unlocked. ---- */
    if (prestige_unlocked()) {
        float cx, cy, w, h;
        prestige_btn_rect(&cx, &cy, &w, &h);
        uint32_t tint = g_sim.prestige_armed ? pack_rgba(1.0F, 0.7F, 0.7F, 1.0F) : pack_rgba(0.6F, 0.85F, 1.0F, 1.0F);
        emit_sprite(R_BUTTON, cx, cy, w, h, tint);
    }

    /* ---- Offline popup panel. ---- */
    if (g_sim.offline_popup) {
        emit_quad(DESIGN_W * 0.5F, DESIGN_H * 0.5F, 520.0F, 160.0F, 0.06F, 0.06F, 0.12F, 0.86F);
        float cx, cy, w, h;
        collect_btn_rect(&cx, &cy, &w, &h);
        emit_sprite(R_BUTTON, cx, cy, w, h, white);
    }

    nt_sprite_renderer_flush();
}

/* ---- FTUE prompt (<=3 beats from balance.json) ---- */
static const char *ftue_prompt(void) {
    if (g_sim.offline_popup) return NULL;
    int step = g_game_state.run_ftue_step;
    if (step <= 0) return "Your hero fights on its own - watch the gold pile up.";
    if (step == 1) return "Tap Sword to upgrade - hit harder, kill faster.";
    if (step == 2 && prestige_unlocked())
        return "Push as far as you can. When it slows, Prestige for a permanent boost.";
    return NULL;
}

static void compose_overlays(void) {
    const char *prompt = ftue_prompt();
    if (prompt) {
        /* place the prompt ABOVE the upgrade panel, clear of it */
        emit_quad(DESIGN_W * 0.5F, PANEL_CY + 78.0F, text_width(prompt, 20.0F) + 40.0F, 34.0F, 0.06F, 0.05F, 0.08F, 0.6F);
    }
    nt_sprite_renderer_flush();
}

static void compose_floaters(void) {
    for (int i = 0; i < MAX_FLOATERS; ++i) {
        Floater *f = &g_sim.floaters[i];
        if (!f->active) continue;
        float life = 1.0F - f->age / f->ttl;
        float col[4] = {f->color[0], f->color[1], f->color[2], clampf(life + 0.2F, 0.0F, 1.0F)};
        emit_text_centered(f->text, f->x, f->y, f->size, col);
    }
}

static void compose_text(void) {
    const float gold[4] = {1.0F, 0.82F, 0.28F, 1.0F};
    const float cream[4] = {0.996F, 0.980F, 0.937F, 1.0F};
    const float ice[4] = {0.7F, 0.9F, 1.0F, 1.0F};
    const float dim[4] = {0.78F, 0.78F, 0.82F, 1.0F};

    /* Title. */
    {
        const float ink[4] = {0.149F, 0.125F, 0.110F, 1.0F};
        const char *title = "VOXELHEIM";
        emit_text(title, DESIGN_W - 150.0F, DESIGN_H - 132.0F + 2.0F - 2.0F, 18.0F, ink);
        emit_text(title, DESIGN_W - 150.0F, DESIGN_H - 132.0F, 18.0F, gold);
    }

    /* Gold counter. */
    {
        char g[24];
        char n[16];
        fmt_num(n, sizeof(n), (double)g_game_state.idle_gold);
        (void)snprintf(g, sizeof(g), "Gold  %s", n);
        emit_text_shadow(g, 60.0F, DESIGN_H - 38.0F, 22.0F, gold);
    }
    /* Stage counter. */
    {
        char s[28];
        if (g_game_state.idle_boss_active) {
            (void)snprintf(s, sizeof(s), "Stage %d  BOSS", g_game_state.idle_stage);
        } else {
            (void)snprintf(s, sizeof(s), "Stage %d", g_game_state.idle_stage);
        }
        emit_text_shadow(s, 60.0F, DESIGN_H - 70.0F, 20.0F, cream);
    }
    /* Frost shards (top-right under minimap), always shown once any exist. */
    {
        char fs[28];
        (void)snprintf(fs, sizeof(fs), "Frost Shards: %d", g_game_state.idle_frost_shards);
        emit_text_shadow(fs, DESIGN_W - 240.0F, DESIGN_H - 160.0F, 18.0F, ice);
    }

    /* Stage progress bar label. */
    {
        char s[28];
        if (g_game_state.idle_boss_active) {
            (void)snprintf(s, sizeof(s), "BOSS  %0.0fs", (double)(g_sim.boss_timer < 0.0F ? 0.0F : g_sim.boss_timer));
        } else {
            (void)snprintf(s, sizeof(s), "%d / %d kills", g_game_state.idle_kills_in_stage, VH_KILLS_PER_STAGE);
        }
        emit_text_centered(s, 176.0F, DESIGN_H - 86.0F, 13.0F, cream);
    }

    /* Stage-advance flash banner. */
    if (g_sim.stage_flash > 0.0F) {
        float a = clampf(g_sim.stage_flash / STAGE_FLASH_TIME, 0.0F, 1.0F);
        float col[4] = {1.0F, 0.9F, 0.45F, a};
        char s[28];
        if (stage_is_boss(g_game_state.idle_stage)) {
            (void)snprintf(s, sizeof(s), "BOSS! Stage %d", g_game_state.idle_stage);
        } else {
            (void)snprintf(s, sizeof(s), "Stage %d", g_game_state.idle_stage);
        }
        emit_text_centered(s, DESIGN_W * 0.5F, DESIGN_H * 0.62F, 40.0F, col);
    }

    /* ---- Upgrade panel text (label + effect + cost). ---- */
    for (int i = 0; i < UP_COUNT; ++i) {
        float cx, cy, w, h;
        panel_slot_rect(i, &cx, &cy, &w, &h);
        bool affordable = (double)g_game_state.idle_gold >= upgrade_cost(i);
        const float *labelc = affordable ? cream : dim;

        char lbl[24];
        (void)snprintf(lbl, sizeof(lbl), "%s  Lv%d", upgrade_label(i), upgrade_level(i));
        emit_text_shadow(lbl, cx - w * 0.5F + 48.0F, cy + 12.0F, 16.0F, labelc);

        char eff[24];
        upgrade_effect_str(i, eff, sizeof(eff));
        emit_text_shadow(eff, cx - w * 0.5F + 48.0F, cy - 8.0F, 13.0F, ice);

        char cost[24];
        char n[16];
        fmt_num(n, sizeof(n), upgrade_cost(i));
        (void)snprintf(cost, sizeof(cost), "%s", n);
        const float *cc = affordable ? gold : dim;
        emit_text_shadow(cost, cx - w * 0.5F + 48.0F, cy - 26.0F, 14.0F, cc);
    }

    /* ---- Prestige button text. ---- */
    if (prestige_unlocked()) {
        float cx, cy, w, h;
        prestige_btn_rect(&cx, &cy, &w, &h);
        char t[40];
        if (g_sim.prestige_armed) {
            (void)snprintf(t, sizeof(t), "CONFIRM PRESTIGE");
        } else {
            (void)snprintf(t, sizeof(t), "Prestige  +%d shards", frost_shards_reward());
        }
        const float ink[4] = {0.10F, 0.08F, 0.07F, 1.0F};
        emit_text_centered(t, cx, cy - 7.0F, 16.0F, ink);
    }

    /* ---- Shard upgrades mini-panel (left column) when shards exist. ---- */
    if (g_game_state.idle_frost_shards > 0 || g_game_state.idle_shard_global_damage > 0 ||
        g_game_state.idle_shard_global_gold > 0 || g_game_state.idle_shard_start_stage > 0 ||
        g_game_state.idle_shard_offline_rate > 0) {
        const char *names[SH_COUNT] = {"Sharper Steel", "Rich Veins", "Head Start", "Camp Supplies"};
        int lvls[SH_COUNT] = {g_game_state.idle_shard_global_damage, g_game_state.idle_shard_global_gold,
                              g_game_state.idle_shard_start_stage, g_game_state.idle_shard_offline_rate};
        for (int i = 0; i < SH_COUNT; ++i) {
            char t[48];
            (void)snprintf(t, sizeof(t), "%s Lv%d (%.0f)", names[i], lvls[i], shard_cost(i));
            emit_text_shadow(t, 18.0F, DESIGN_H - 200.0F - (float)i * 22.0F, 13.0F, ice);
        }
    }

    /* ---- FTUE prompt text. ---- */
    {
        const char *prompt = ftue_prompt();
        if (prompt) {
            emit_text_centered(prompt, DESIGN_W * 0.5F, PANEL_CY + 72.0F, 20.0F, cream);
        }
    }

    /* ---- Offline popup text. ---- */
    if (g_sim.offline_popup) {
        char t[48];
        char n[16];
        fmt_num(n, sizeof(n), (double)g_sim.offline_gold);
        emit_text_centered("While you were away", DESIGN_W * 0.5F, DESIGN_H * 0.5F + 44.0F, 24.0F, ice);
        (void)snprintf(t, sizeof(t), "+%s gold  (%.1fh)", n, g_sim.offline_hours);
        emit_text_centered(t, DESIGN_W * 0.5F, DESIGN_H * 0.5F + 8.0F, 22.0F, gold);
        float cx, cy, w, h;
        collect_btn_rect(&cx, &cy, &w, &h);
        const float ink[4] = {0.10F, 0.08F, 0.07F, 1.0F};
        emit_text_centered("Collect", cx, cy - 7.0F, 18.0F, ink);
    }

    compose_floaters();
}

/* ---- Persistence ---- */

#define VOXELHEIM_SAVE_PATH "build/voxelheim_save.json"

static long now_unix(void) {
    return (long)time(NULL);
}

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
    g_game_state.idle_last_seen_unix = (int)now_unix();
    char err[256] = {0};
    (void)game_state_save(&g_game_state, VOXELHEIM_SAVE_PATH, err, (int)sizeof(err));
}

/* ---- DevAPI ---- */

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static cJSON *state_json(void) {
    cJSON *root = game_state_to_json(&g_game_state);
    cJSON_AddStringToObject(root, "runtime", "voxelheim");
    cJSON_AddStringToObject(root, "screen", "frost_keep_climb");
    cJSON_AddBoolToObject(root, "atlas_ready", s_atlas_resolved);

    /* Flat idle mirror for easy probe assertions. */
    cJSON_AddNumberToObject(root, "gold", (double)g_game_state.idle_gold);
    cJSON_AddNumberToObject(root, "stage", g_game_state.idle_stage);
    cJSON_AddNumberToObject(root, "highest_stage", g_game_state.idle_highest_stage);
    cJSON_AddNumberToObject(root, "kills_in_stage", g_game_state.idle_kills_in_stage);
    cJSON_AddNumberToObject(root, "frost_shards", g_game_state.idle_frost_shards);
    cJSON_AddNumberToObject(root, "up_sword", g_game_state.idle_up_sword);
    cJSON_AddNumberToObject(root, "up_boots", g_game_state.idle_up_boots);
    cJSON_AddNumberToObject(root, "up_armor", g_game_state.idle_up_armor);
    cJSON_AddNumberToObject(root, "up_luck", g_game_state.idle_up_luck);
    cJSON_AddBoolToObject(root, "boss_active", g_game_state.idle_boss_active);
    cJSON_AddBoolToObject(root, "offline_unlocked", g_game_state.idle_offline_unlocked);
    cJSON_AddBoolToObject(root, "prestige_unlocked", prestige_unlocked());
    cJSON_AddNumberToObject(root, "ftue_step", g_game_state.run_ftue_step);

    /* Derived combat stats (so the probe can assert upgrades change damage). */
    cJSON_AddNumberToObject(root, "hero_damage", hero_damage());
    cJSON_AddNumberToObject(root, "hero_attack_interval", hero_attack_interval());
    cJSON_AddNumberToObject(root, "hero_max_hp", hero_max_hp());
    cJSON_AddNumberToObject(root, "gold_find_mult", gold_find_mult());

    /* Costs / rewards (so the probe can drive deterministically). */
    cJSON_AddNumberToObject(root, "cost_sword", upgrade_cost(UP_SWORD));
    cJSON_AddNumberToObject(root, "cost_boots", upgrade_cost(UP_BOOTS));
    cJSON_AddNumberToObject(root, "cost_armor", upgrade_cost(UP_ARMOR));
    cJSON_AddNumberToObject(root, "cost_luck", upgrade_cost(UP_LUCK));
    cJSON_AddNumberToObject(root, "shards_reward_now", frost_shards_reward());

    cJSON_AddBoolToObject(root, "offline_popup", g_sim.offline_popup);
    cJSON_AddNumberToObject(root, "offline_gold", (double)g_sim.offline_gold);
    cJSON_AddNumberToObject(root, "monsters_alive", count_monsters());
    return root;
}

static bool ep_game_state(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params; (void)error; (void)error_cap; (void)user;
    *result = state_json();
    return true;
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params; (void)error; (void)error_cap; (void)user;
    playtest_reset();
    *result = state_json();
    return true;
}

/* Drive a world click: params {x,y} in DESIGN units (0..960, 0..540, y-up). */
static bool ep_game_debug_click(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error; (void)error_cap; (void)user;
    double x = (double)DESIGN_W * 0.5;
    double y = (double)DESIGN_H * 0.5;
    if (params) {
        const cJSON *px = cJSON_GetObjectItemCaseSensitive(params, "x");
        const cJSON *py = cJSON_GetObjectItemCaseSensitive(params, "y");
        if (cJSON_IsNumber(px)) x = px->valuedouble;
        if (cJSON_IsNumber(py)) y = py->valuedouble;
    }
    handle_world_click((float)x, (float)y);
    *result = state_json();
    return true;
}

/* Advance the simulation deterministically: params {seconds} (default 0.5s). */
static bool ep_game_debug_tick(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error; (void)error_cap; (void)user;
    double seconds = 0.5;
    if (params) {
        const cJSON *s = cJSON_GetObjectItemCaseSensitive(params, "seconds");
        if (cJSON_IsNumber(s)) seconds = s->valuedouble;
    }
    if (seconds < 0.0) seconds = 0.0;
    if (seconds > 600.0) seconds = 600.0;
    s_debug_driven = true; /* probe owns sim time from here on */
    const double fixed = 1.0 / 60.0;
    int steps = (int)(seconds / fixed + 0.5);
    for (int i = 0; i < steps; ++i) {
        update_sim((float)fixed);
    }
    *result = state_json();
    return true;
}

/* Buy an upgrade by name: params {upgrade:"sword"|"boots"|"armor"|"luck"}. */
static bool ep_game_debug_buy(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    int which = -1;
    if (params) {
        const cJSON *u = cJSON_GetObjectItemCaseSensitive(params, "upgrade");
        if (cJSON_IsString(u)) {
            if (strcmp(u->valuestring, "sword") == 0) which = UP_SWORD;
            else if (strcmp(u->valuestring, "boots") == 0) which = UP_BOOTS;
            else if (strcmp(u->valuestring, "armor") == 0) which = UP_ARMOR;
            else if (strcmp(u->valuestring, "luck") == 0) which = UP_LUCK;
        }
    }
    if (which < 0) {
        if (error && error_cap > 0) (void)snprintf(error, (size_t)error_cap, "unknown upgrade");
        return false;
    }
    bool bought = buy_upgrade(which);
    cJSON *root = state_json();
    cJSON_AddBoolToObject(root, "bought", bought);
    *result = root;
    return true;
}

/* Buy a shard upgrade: params {shard:"damage"|"gold"|"start"|"offline"}. */
static bool ep_game_debug_buy_shard(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    int which = -1;
    if (params) {
        const cJSON *u = cJSON_GetObjectItemCaseSensitive(params, "shard");
        if (cJSON_IsString(u)) {
            if (strcmp(u->valuestring, "damage") == 0) which = SH_DMG;
            else if (strcmp(u->valuestring, "gold") == 0) which = SH_GOLD;
            else if (strcmp(u->valuestring, "start") == 0) which = SH_START;
            else if (strcmp(u->valuestring, "offline") == 0) which = SH_OFFLINE;
        }
    }
    if (which < 0) {
        if (error && error_cap > 0) (void)snprintf(error, (size_t)error_cap, "unknown shard upgrade");
        return false;
    }
    bool bought = buy_shard_upgrade(which);
    cJSON *root = state_json();
    cJSON_AddBoolToObject(root, "bought", bought);
    *result = root;
    return true;
}

/* Commit a prestige (no confirm gate). */
static bool ep_game_debug_prestige(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params; (void)user;
    bool ok = do_prestige();
    if (!ok) {
        if (error && error_cap > 0) (void)snprintf(error, (size_t)error_cap, "prestige locked (stage < %d)", VH_PRESTIGE_UNLOCK_STAGE);
        return false;
    }
    *result = state_json();
    return true;
}

/* Simulate an offline absence: params {seconds} -> set last_seen back, grant. */
static bool ep_game_debug_offline(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error; (void)error_cap; (void)user;
    double seconds = 3600.0;
    if (params) {
        const cJSON *s = cJSON_GetObjectItemCaseSensitive(params, "seconds");
        if (cJSON_IsNumber(s)) seconds = s->valuedouble;
    }
    long now = now_unix();
    g_game_state.idle_last_seen_unix = (int)(now - (long)seconds);
    long granted = compute_offline_grant(now);
    g_sim.offline_gold = granted;
    g_sim.offline_popup = (granted > 0);
    g_game_state.idle_last_seen_unix = (int)now;
    *result = state_json();
    return true;
}

static bool ep_frame_screenshot(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error; (void)error_cap; (void)user;
    cJSON *root = cJSON_CreateObject();
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
    nt_devapi_register("game.debug.buy", ep_game_debug_buy, NULL);
    nt_devapi_register("game.debug.buy_shard", ep_game_debug_buy_shard, NULL);
    nt_devapi_register("game.debug.prestige", ep_game_debug_prestige, NULL);
    nt_devapi_register("game.debug.offline", ep_game_debug_offline, NULL);
    nt_devapi_register("frame.screenshot", ep_frame_screenshot, NULL);
}

static void register_ui_devapi(float w, float h) {
    nt_devapi_set_frame(g_nt_app.frame);
    nt_devapi_set_view((float)canvas_w(), (float)canvas_h(), w, h);
    nt_devapi_clear_ui_elements();
    (void)nt_devapi_register_ui_node("root", "", "screen", "Voxelheim", "Frost Keep Climb", 0.0F, 0.0F, w, h, true, true);
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
        /* compute the offline grant the first time the sim comes up */
        if (g_game_state.idle_offline_unlocked && !s_fresh_state) {
            long now = now_unix();
            long granted = compute_offline_grant(now);
            if (granted > 0) {
                g_sim.offline_gold = granted;
                g_sim.offline_popup = true;
            }
            g_game_state.idle_last_seen_unix = (int)now;
        }
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

    g_nt_window.title = "Voxelheim - Frost Keep Climb";
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
