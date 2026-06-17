/*
 * Voxelheim -- "Frost Keep Rebuilder" IDLE / incremental RPG rescue slice.
 *
 * The hero stands on the path (lower-center) and AUTO-ATTACKS. An endless stream
 * of monsters spawns up the path and walks toward him; the front monster takes
 * damage, dies, drops GOLD, and the next advances. No player movement.
 *
 *   - Auto-combat stream: hero deals computed damage every computed interval.
 *   - Monsters drop Gold plus Frost Blocks, the visible rebuild currency.
 *   - First session: repair the Gate, then choose one of three rune cards.
 *   - Forge repair later reveals the legacy training panel.
 *   - Stages: kills_per_stage kills -> stage+1; monster HP/gold scale per stage.
 *   - Bosses: every boss.every_stages stages, one big timed monster.
 *   - Avalanche Reset (unlock when the first Keep chain is rebuilt): reset
 *     rooms+run economy for Frost Shards, then rebuild faster with shard boosts.
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
#include "game_audio.h"
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
#define VH_PACKET_KILLS 5

/* keep rebuild */
#define VH_BLOCKS_PER_KILL 1
#define VH_BLOCKS_PER_BOSS 8
#define VH_GATE_BLOCK_COST 5
#define VH_FORGE_BLOCK_COST 15
#define VH_CAMPFIRE_BLOCK_COST 30
#define VH_CAMPFIRE_DAMAGE_MULT 1.25
#define VH_BLOCK_CACHE_GATE 5
#define VH_BLOCK_CACHE_FORGE 8

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

/* avalanche reset / prestige */
#define VH_PRESTIGE_UNLOCK_STAGE 25
#define VH_AVALANCHE_KEEP_RANK_UNLOCK 3
#define VH_SHARDS_PER_KEEP_RANK 1
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
#define VH_OFFLINE_BLOCKS_PER_HOUR 12.0

/* ---- Sim tuning (presentation only; not economy) ----
 *
 * IDLE BATTLER layout (Clicker Heroes / Tap Titans 2): the fight is ONE big
 * enemy at a fixed FIGHT SLOT next to the hero in the lower third -- never a
 * vertical stack. Queued monsters (at most a couple) sit far up the path, small
 * and faded, as "coming next"; when the front dies the next walks down into the
 * slot. Each monster carries a progression coordinate `y` in [0..1] (1 = just
 * spawned far up the path, 0 = engaged at the fight slot); screen position is
 * derived from `y` along a receding path curve, so distance reads as depth
 * (smaller + higher + dimmer), not as a column. */
#define MON_WALK_RATE 0.9F              /* progress units/sec the front advances */
#define HIT_FLASH_TIME 0.1F
#define STAGE_FLASH_TIME 1.1F

/* Only a tiny queue is ever shown behind the active enemy (refs show ~1-2). */
#define MAX_MONSTERS 3                  /* 1 active + up to 2 small queued */
#define MAX_PARTICLES 80
#define MAX_FLOATERS 24
#define MAX_SPRITE_FX 40

/* Hero attack lunge (presentation): a quick forward poke + recoil on each swing. */
#define HERO_LUNGE_TIME 0.26F

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
    R_BOOTS_ICON,
    R_ARMOR_ICON,
    R_LUCK_ICON,
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
    ASSET_ATLAS_REGION_VOXELS_BOOTS_ICON_PNG,
    ASSET_ATLAS_REGION_VOXELS_ARMOR_ICON_PNG,
    ASSET_ATLAS_REGION_VOXELS_LUCK_ICON_PNG,
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
enum { ROOM_GATE = 0, ROOM_FORGE, ROOM_CAMPFIRE, ROOM_COUNT };
enum { CARD_SHARP = 0, CARD_BLOCK_CACHE, CARD_QUICK_HANDS, CARD_COUNT };

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

/* One monster in the stream. `prog` is a progression coordinate in [0..1]:
 * 1.0 = just spawned, far up the path; 0.0 = engaged at the fight slot next to
 * the hero. Screen (x,y,scale,alpha) are DERIVED from `prog` along a receding
 * path curve so distance reads as depth, never as a vertical column. */
typedef struct Monster {
    bool alive;
    float prog;       /* 1=far up path .. 0=engaged at the fight slot */
    int lane;         /* queued-monster lane index (slight L/R offset up-path) */
    double hp;        /* current hp (double: scales large) */
    double max_hp;
    float flash;      /* hit-flash timer */
    float knockback;  /* hit knockback timer (small recoil) */
    float spawn_pop;  /* spawn-in pop timer (scale ease) */
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
    float hero_lunge;      /* attack lunge/recoil timer (presentation) */
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
    int offline_blocks;    /* Frost Blocks recovered while away (display) */
    double offline_hours;  /* capped hours (display) */

    /* prestige confirm state: a press arms confirm; second press commits. */
    bool prestige_armed;
    float prestige_armed_timer;

    /* short reward pulses; they make major idle actions read at a glance. */
    float reward_keep_pulse;
    float reward_card_pulse;
    float reward_shard_pulse;
    float reward_avalanche_pulse;
    float reward_offline_pulse;
} g_sim;

/* IDLE-BATTLER composition (toward idle_fakeshot.png): hero lower-center-LEFT,
 * the active enemy at the FIGHT SLOT just to his right, BOTH prominent in the
 * lower third; the Frost Keep + mountains read as backdrop only. */
#define HERO_X (DESIGN_W * 0.355F)
#define HERO_Y (DESIGN_H * 0.300F)
/* Fight slot: where the engaged enemy stands (prog==0). Just up-path / right. */
#define FIGHT_X (DESIGN_W * 0.605F)
#define FIGHT_Y (DESIGN_H * 0.345F)
/* Far end of the queue path (prog==1): up toward the keep, smaller + dimmer. */
#define PATH_FAR_X (DESIGN_W * 0.560F)
#define PATH_FAR_Y (DESIGN_H * 0.560F)
#define KEEP_CX (DESIGN_W * 0.5F)
#define KEEP_CY (DESIGN_H * 0.74F)
#define FORGE_MARKER_X (KEEP_CX + 126.0F)
#define FORGE_MARKER_Y (KEEP_CY - 82.0F)
#define CAMPFIRE_MARKER_X (HERO_X - 126.0F)
#define CAMPFIRE_MARKER_Y (HERO_Y - 78.0F)

/* ---- Helpers ---- */

static uint32_t canvas_w(void) { return g_nt_window.fb_width > 0 ? g_nt_window.fb_width : (uint32_t)s_window_width; }
static uint32_t canvas_h(void) { return g_nt_window.fb_height > 0 ? g_nt_window.fb_height : (uint32_t)s_window_height; }

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* Aspect-correct world bounds. The DESIGN rect (16:9) is fit (CONTAIN) into the
 * framebuffer with EQUAL x/y scale and centered, so nothing stretches when the
 * window aspect != 16:9; the extra margin is covered by the full-bleed backdrop.
 * The projection AND the pointer->design click mapping share this helper so the
 * two never disagree. At the 16:9 default (what probes use) it returns exactly
 * [0,DESIGN_W] x [0,DESIGN_H], so headless behavior is unchanged. */
static void world_bounds(float *left, float *right, float *bottom, float *top) {
    const float fw = (float)canvas_w();
    const float fh = (float)canvas_h();
    if (fw <= 0.0F || fh <= 0.0F) {
        *left = 0.0F; *right = DESIGN_W; *bottom = 0.0F; *top = DESIGN_H;
        return;
    }
    const float scale = fminf(fw / DESIGN_W, fh / DESIGN_H);
    const float vis_w = fw / scale;
    const float vis_h = fh / scale;
    const float ox = (vis_w - DESIGN_W) * 0.5F;
    const float oy = (vis_h - DESIGN_H) * 0.5F;
    *left = -ox; *right = DESIGN_W + ox;
    *bottom = -oy; *top = DESIGN_H + oy;
}

/* Map a monster's progression coord (1 far .. 0 engaged) + lane to screen.
 * The path recedes up and slightly toward center; depth (=prog) drives scale &
 * fade so queued monsters read as small/faded "coming next", not a tower. */
static void monster_screen(const Monster *m, float *out_x, float *out_y, float *out_scale, float *out_alpha) {
    float t = clampf(m->prog, 0.0F, 1.0F);
    /* ease so the front sits clearly forward and the queue bunches up-path */
    float te = t * t;
    float x = FIGHT_X + (PATH_FAR_X - FIGHT_X) * te;
    float y = FIGHT_Y + (PATH_FAR_Y - FIGHT_Y) * te;
    /* queued monsters fan to alternating lanes so two queued don't overlap */
    if (t > 0.02F) {
        float lane_off = (m->lane % 2 == 0) ? -1.0F : 1.0F;
        x += lane_off * 34.0F * te;
    }
    *out_x = x;
    *out_y = y;
    *out_scale = 1.0F - 0.46F * te;   /* near=1.0 .. far=~0.54 */
    *out_alpha = 1.0F - 0.55F * te;   /* near=1.0 .. far=~0.45 (faded) */
}

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

/* Left-anchored text with a dark outline for readability: draw the dark ink at
 * four diagonal offsets (a cheap outline), then the light glyph on top, so the
 * string stays legible even where it overhangs a bright background. */
static void emit_text_shadow(const char *s, float x, float y, float size, const float color[4]) {
    const float ink[4] = {0.06F, 0.05F, 0.04F, color[3]};
    const float o = clampf(size * 0.09F, 1.4F, 3.0F);
    emit_text(s, x + o, y - o, size, ink);
    emit_text(s, x - o, y - o, size, ink);
    emit_text(s, x + o, y + o, size, ink);
    emit_text(s, x - o, y + o, size, ink);
    emit_text(s, x, y, size, color);
}

/* Centered text with a dark drop shadow for readability. */
static void emit_text_centered(const char *s, float cx, float y, float size, const float color[4]) {
    const float w = text_width(s, size);
    emit_text_shadow(s, cx - w * 0.5F, y, size, color);
}

/* Single SOFT drop-shadow for text that ALREADY sits on a solid plate. The
 * plate supplies the contrast, so the 4-way outline of emit_text_shadow is
 * redundant and its halo reads as "noise" (noise-avoidance: one legibility
 * treatment per text style; prefer the plate over stacked effects). Keep the
 * 4-way outline only for text floating over the bright scene (damage numbers). */
static void emit_text_soft(const char *s, float x, float y, float size, const float color[4]) {
    const float ink[4] = {0.04F, 0.04F, 0.05F, color[3] * 0.5F};
    const float o = clampf(size * 0.055F, 1.0F, 2.0F);
    emit_text(s, x + o, y - o, size, ink);
    emit_text(s, x, y, size, color);
}

static void emit_text_centered_soft(const char *s, float cx, float y, float size, const float color[4]) {
    const float w = text_width(s, size);
    emit_text_soft(s, cx - w * 0.5F, y, size, color);
}

static void emit_text_centered_plain(const char *s, float cx, float y, float size, const float color[4]) {
    const float w = text_width(s, size);
    emit_text(s, cx - w * 0.5F, y, size, color);
}

static void emit_text_centered_button(const char *s, float cx, float y, float size, const float color[4]) {
    const float w = text_width(s, size);
    const float x = cx - w * 0.5F;
    emit_text(s, x - 0.7F, y, size, color);
    emit_text(s, x + 0.7F, y, size, color);
    emit_text(s, x, y - 0.5F, size, color);
    emit_text(s, x, y + 0.5F, size, color);
    emit_text(s, x, y, size, color);
}

/* Frosted label plate that strings sit on. Drawn as a main slab plus two
 * narrower slabs (top/bottom) to fake rounded corners, with an icy rim on top.
 * Sized to FULLY contain the text + padding so labels never touch the bright
 * snow/sky directly. */
static void emit_plate(float cx, float cy, float w, float h, float a) {
    const float r = 0.070F, g = 0.155F, b = 0.215F;
    emit_quad(cx, cy, w + 4.0F, h + 4.0F, 0.52F, 0.78F, 0.95F, a * 0.45F);
    /* body + rounded-corner fakery */
    emit_quad(cx, cy, w, h, r, g, b, a);
    emit_quad(cx, cy, w - 8.0F, h + 6.0F, r, g, b, a);
    emit_quad(cx, cy, w + 6.0F, h - 8.0F, r, g, b, a);
}

/* Rounded filled panel in an ARBITRARY color (same corner-fakery as emit_plate
 * + a cool light rim), so a HUD/upgrade card is ONE coherent shape instead of
 * clashing rects stacked on a sprite. */
static void emit_round_panel(float cx, float cy, float w, float h, float r, float g, float b, float a) {
    emit_quad(cx, cy, w + 5.0F, h + 5.0F, 0.30F, 0.36F, 0.46F, a * 0.55F); /* cool rim */
    emit_quad(cx, cy, w, h, r, g, b, a);
    emit_quad(cx, cy, w - 9.0F, h + 7.0F, r, g, b, a);
    emit_quad(cx, cy, w + 7.0F, h - 9.0F, r, g, b, a);
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
    if (g_game_state.idle_room_campfire_repaired) {
        base *= VH_CAMPFIRE_DAMAGE_MULT;
    }
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

/* Frost shards earned from an Avalanche Reset.
 * First loop must pay out clearly: Keep Rank is the visible rebuild progress,
 * while highest-stage remains a legacy/late-run progress bonus. */
static int frost_shards_reward(void) {
    int hs = g_game_state.idle_highest_stage;
    int keep = g_game_state.idle_keep_rank * VH_SHARDS_PER_KEEP_RANK;
    double stage_bonus = floor(pow((double)hs, 0.8) / 3.0);
    if (stage_bonus < 0.0) stage_bonus = 0.0;
    return keep + (int)stage_bonus;
}

static bool prestige_unlocked(void) {
    return g_game_state.idle_keep_rank >= VH_AVALANCHE_KEEP_RANK_UNLOCK ||
           g_game_state.idle_highest_stage >= VH_PRESTIGE_UNLOCK_STAGE;
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

static void spawn_reward_burst(float x, float y, float size) {
    spawn_sprite_fx(R_LEVELUP_BURST, x, y, size, 0.62F, 18.0F, 0.0F);
    spawn_sprite_fx(R_HIT_SPARK, x - size * 0.16F, y + size * 0.05F, size * 0.34F, 0.34F, 34.0F, -2.0F);
    spawn_sprite_fx(R_HIT_SPARK, x + size * 0.16F, y + size * 0.02F, size * 0.30F, 0.34F, 30.0F, 2.0F);
    spawn_sparkle(x, y);
}

static void spawn_reward_coin_fountain(float x, float y) {
    spawn_coin_pop(x, y);
    spawn_coin_pop(x + 30.0F, y - 4.0F);
    spawn_coin_pop(x - 30.0F, y - 2.0F);
}

/* ---- Sim/monster spawning ---- */

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

/* Index of the front (lowest-prog == most engaged) alive monster, or -1. */
static int front_monster_index(void) {
    int front = -1;
    float best = 2.0F;
    for (int i = 0; i < MAX_MONSTERS; ++i) {
        if (g_sim.monsters[i].alive && g_sim.monsters[i].prog < best) {
            best = g_sim.monsters[i].prog;
            front = i;
        }
    }
    return front;
}

/* The smallest progress among other alive monsters that are AHEAD of `self`
 * (lower prog). Used to keep the queue staggered up-path behind the front. */
static float min_prog_excluding(int self) {
    float best = 2.0F;
    for (int i = 0; i < MAX_MONSTERS; ++i) {
        if (i != self && g_sim.monsters[i].alive && g_sim.monsters[i].prog < best) {
            best = g_sim.monsters[i].prog;
        }
    }
    return best;
}

/* Spawn a normal monster up the path. It starts at prog==1 (far, small, faded)
 * and walks down toward the front; only the front engages the hero. */
static void spawn_monster(void) {
    if (g_game_state.idle_boss_active) {
        return; /* boss stage: a single boss occupies the fight slot */
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
            m->knockback = 0.0F;
            m->spawn_pop = 0.22F;
            m->lane = i;
            m->prog = 1.0F; /* spawn far up the path */
            return;
        }
    }
}

/* Spawn the boss for the current (boss) stage -- a single big enemy that walks
 * straight into the fight slot with a brief telegraph (spawn pop). */
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
    m->knockback = 0.0F;
    m->spawn_pop = 0.5F; /* bigger telegraph for the boss entrance */
    m->lane = 0;
    m->prog = 0.55F; /* boss strides in from partway up the path */
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

static int keep_room_cost(int room) {
    switch (room) {
        case ROOM_GATE: return VH_GATE_BLOCK_COST;
        case ROOM_FORGE: return VH_FORGE_BLOCK_COST;
        case ROOM_CAMPFIRE: return VH_CAMPFIRE_BLOCK_COST;
        default: return 0;
    }
}

static const char *keep_room_name(int room) {
    switch (room) {
        case ROOM_GATE: return "Gate";
        case ROOM_FORGE: return "Forge";
        case ROOM_CAMPFIRE: return "Campfire";
        default: return "Room";
    }
}

static const char *keep_room_effect_label(int room) {
    switch (room) {
        case ROOM_GATE: return "opens rune cards";
        case ROOM_FORGE: return "opens training";
        case ROOM_CAMPFIRE: return "helper +25%";
        default: return "upgrades the Keep";
    }
}

static bool keep_room_repaired(int room) {
    switch (room) {
        case ROOM_GATE: return g_game_state.idle_room_gate_repaired;
        case ROOM_FORGE: return g_game_state.idle_room_forge_repaired;
        case ROOM_CAMPFIRE: return g_game_state.idle_room_campfire_repaired;
        default: return false;
    }
}

static void set_keep_room_repaired(int room, bool repaired) {
    switch (room) {
        case ROOM_GATE: g_game_state.idle_room_gate_repaired = repaired; break;
        case ROOM_FORGE: g_game_state.idle_room_forge_repaired = repaired; break;
        case ROOM_CAMPFIRE: g_game_state.idle_room_campfire_repaired = repaired; break;
        default: break;
    }
}

static void sync_keep_rank(void) {
    int rank = 0;
    for (int i = 0; i < ROOM_COUNT; ++i) {
        if (keep_room_repaired(i)) ++rank;
    }
    g_game_state.idle_keep_rank = rank;
}

static void grant_blocks(int amount, float x, float y, bool floater) {
    if (amount <= 0) return;
    long total = (long)g_game_state.idle_frost_blocks + (long)amount;
    if (total > 2147483647L) total = 2147483647L;
    g_game_state.idle_frost_blocks = (int)total;
    if (floater) {
        char t[24];
        (void)snprintf(t, sizeof(t), "+%d Blocks", amount);
        const float c[4] = {0.58F, 0.92F, 1.0F, 1.0F};
        spawn_floater(x, y + 44.0F, t, c, 21.0F);
    }
    game_state_mark_dirty();
}

static bool repair_room(int room) {
    if (room < 0 || room >= ROOM_COUNT) return false;
    if (keep_room_repaired(room)) return false;
    int cost = keep_room_cost(room);
    if (g_game_state.idle_frost_blocks < cost) return false;

    g_game_state.idle_frost_blocks -= cost;
    set_keep_room_repaired(room, true);
    sync_keep_rank();

    if (room == ROOM_GATE) {
        g_game_state.idle_pending_card_choice = true;
        if (g_game_state.run_ftue_step < 2) g_game_state.run_ftue_step = 2;
    }
    if (room == ROOM_FORGE && g_game_state.run_ftue_step < 3) {
        g_game_state.run_ftue_step = 3;
    }
    if (room == ROOM_CAMPFIRE && g_game_state.run_ftue_step < 4) {
        g_game_state.run_ftue_step = 4;
    }

    char t[24];
    (void)snprintf(t, sizeof(t), "%s repaired!", keep_room_name(room));
    const float c[4] = {0.68F, 1.0F, 0.82F, 1.0F};
    spawn_floater(DESIGN_W * 0.5F, DESIGN_H * 0.60F, t, c, 24.0F);
    float fx = KEEP_CX;
    float fy = KEEP_CY - 32.0F;
    if (room == ROOM_FORGE) {
        fx = FORGE_MARKER_X;
        fy = FORGE_MARKER_Y + 10.0F;
    } else if (room == ROOM_CAMPFIRE) {
        fx = CAMPFIRE_MARKER_X;
        fy = CAMPFIRE_MARKER_Y + 8.0F;
    }
    spawn_reward_burst(fx, fy, room == ROOM_GATE ? 230.0F : 170.0F);
    g_sim.reward_keep_pulse = 0.78F;
    game_audio_play(GAME_AUDIO_CUE_SUCCESS);
    game_state_mark_dirty();
    return true;
}

static bool choose_card(int card) {
    if (!g_game_state.idle_pending_card_choice) return false;
    if (card < 0 || card >= CARD_COUNT) return false;

    const char *label = "Rune";
    const bool forge_tier = g_game_state.idle_room_forge_repaired;
    switch (card) {
        case CARD_SHARP:
            g_game_state.idle_up_sword += forge_tier ? 2 : 1;
            label = forge_tier ? "Tempered Blade" : "Sharp Edges";
            break;
        case CARD_BLOCK_CACHE:
            grant_blocks(forge_tier ? VH_BLOCK_CACHE_FORGE : VH_BLOCK_CACHE_GATE, DESIGN_W * 0.5F, DESIGN_H * 0.50F, false);
            label = forge_tier ? "Block Crate" : "Block Cache";
            break;
        case CARD_QUICK_HANDS:
            g_game_state.idle_up_boots += 1;
            label = "Quick Hands";
            break;
        default:
            break;
    }
    g_game_state.idle_pending_card_choice = false;
    g_game_state.idle_card_choices_made += 1;
    g_game_state.run_hero_max_hp = hero_max_hp();
    if (g_game_state.run_hero_hp > g_game_state.run_hero_max_hp) {
        g_game_state.run_hero_hp = g_game_state.run_hero_max_hp;
    }
    if (g_game_state.run_ftue_step < 3) g_game_state.run_ftue_step = 3;
    const float c[4] = {0.92F, 0.82F, 1.0F, 1.0F};
    spawn_floater(DESIGN_W * 0.5F, DESIGN_H * 0.56F, label, c, 24.0F);
    spawn_reward_burst(DESIGN_W * 0.5F, 188.0F, 190.0F);
    spawn_reward_coin_fountain(DESIGN_W * 0.5F, 176.0F);
    g_sim.reward_card_pulse = 0.62F;
    game_audio_play(GAME_AUDIO_CUE_SUCCESS);
    game_state_mark_dirty();
    return true;
}

/* Kill the front monster: drop gold, advance the stage counter / boss. */
static void on_monster_killed(Monster *m) {
    /* Screen position of the dying enemy (FX/floaters spawn here). */
    float kx, ky, ksc, ka;
    monster_screen(m, &kx, &ky, &ksc, &ka);
    bool was_boss = m->is_boss;

    spawn_sparkle(kx, ky);
    spawn_coin_pop(kx, ky);
    /* gold burst: a ring of coins on every kill (bigger on a boss). */
    spawn_sprite_fx(R_LEVELUP_BURST, kx, ky, was_boss ? 220.0F : 120.0F,
                    was_boss ? 0.55F : 0.38F, 12.0F, 0.0F);

    if (was_boss) {
        double g = monster_gold_for_stage(g_game_state.idle_stage) * VH_BOSS_GOLD_MULT;
        grant_gold(g, kx, ky, true);
        grant_blocks(VH_BLOCKS_PER_BOSS, kx, ky, true);
        m->alive = false;
        g_game_state.idle_boss_active = false;
        g_sim.boss_timer = 0.0F;
        /* bigger death burst: extra coins + sparkle for the boss. */
        spawn_coin_pop(kx + 28.0F, ky + 8.0F);
        spawn_coin_pop(kx - 30.0F, ky - 4.0F);
        spawn_sparkle(kx, ky + 10.0F);
        advance_stage();
        return;
    }

    double g = monster_gold_for_stage(g_game_state.idle_stage);
    grant_gold(g, kx, ky, true);
    grant_blocks(VH_BLOCKS_PER_KILL, kx, ky, true);
    m->alive = false;
    g_game_state.idle_kills_in_stage += 1;

    /* FTUE: first kill -> repair beat. */
    if (g_game_state.run_ftue_step < 1) {
        g_game_state.run_ftue_step = 1;
    }
    if (g_game_state.idle_kills_in_stage >= VH_PACKET_KILLS &&
        g_game_state.idle_room_gate_repaired &&
        !g_game_state.idle_pending_card_choice) {
        g_game_state.idle_pending_card_choice = true;
        const float c[4] = {0.9F, 0.82F, 1.0F, 1.0F};
        spawn_floater(DESIGN_W * 0.5F, DESIGN_H * 0.58F, "Choose a rune", c, 23.0F);
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
    spawn_reward_burst(HERO_X, HERO_Y + 24.0F, 118.0F);
    game_audio_play(GAME_AUDIO_CUE_CLICK);
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

/* Avalanche Reset: bury the rebuilt rooms, grant Frost Shards, restart stronger. */
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
    g_game_state.idle_frost_blocks = 0;
    g_game_state.idle_keep_rank = 0;
    g_game_state.idle_room_gate_repaired = false;
    g_game_state.idle_room_forge_repaired = false;
    g_game_state.idle_room_campfire_repaired = false;
    g_game_state.idle_pending_card_choice = false;
    g_game_state.idle_card_choices_made = 0;
    if (!g_game_state.idle_offline_unlocked) {
        g_game_state.idle_offline_unlocked = true;
    }
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

    sim_seed_stage();
    g_sim.reward_keep_pulse = 0.0F;
    g_sim.reward_card_pulse = 0.0F;
    g_sim.reward_shard_pulse = 0.0F;
    g_sim.reward_offline_pulse = 0.0F;

    char t[24];
    (void)snprintf(t, sizeof(t), "+%d Frost Shards", reward);
    const float c[4] = {0.65F, 0.9F, 1.0F, 1.0F};
    spawn_floater(KEEP_CX - 132.0F, KEEP_CY - 84.0F, t, c, 22.0F);
    spawn_floater(KEEP_CX, KEEP_CY + 24.0F, "Avalanche Reset!", c, 20.0F);
    spawn_floater(HERO_X - 70.0F, HERO_Y + 76.0F, "Offline Camp ready", c, 18.0F);
    spawn_reward_burst(KEEP_CX, KEEP_CY - 18.0F, 220.0F);
    g_sim.reward_avalanche_pulse = 0.92F;
    game_audio_play(GAME_AUDIO_CUE_NOTIFY);

    game_state_mark_dirty();
    return true;
}

/* ---- Offline earnings ---- */

/* Compute + grant offline Gold + Frost Blocks from a last-seen timestamp. */
static bool compute_offline_grant(long now_unix, long *gold_out, int *blocks_out) {
    if (gold_out) *gold_out = 0;
    if (blocks_out) *blocks_out = 0;
    if (!g_game_state.idle_offline_unlocked) return false;
    long last = (long)g_game_state.idle_last_seen_unix;
    if (last <= 0 || now_unix <= last) return false;
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
    double block_grant = (elapsed_s / 3600.0) * VH_OFFLINE_BLOCKS_PER_HOUR * shard_offline_mult();
    if (block_grant < 0.0) block_grant = 0.0;
    /* Clamp in DOUBLE to the int32 gold range before any narrowing cast: with the
     * multiplicative economy, highest_stage climbs high enough that the raw grant
     * exceeds 2^31 and a direct (long) cast wraps to INT_MIN. */
    if (grant > 2147483647.0) grant = 2147483647.0;
    long g = (long)grant;
    int blocks = (int)floor(block_grant + 0.5);
    if (blocks > 9999) blocks = 9999;
    if (g > 0 || blocks > 0) {
        double total = (double)g_game_state.idle_gold + (double)g;
        g_game_state.idle_gold = gold_to_int(total);
        if (blocks > 0) {
            long total_blocks = (long)g_game_state.idle_frost_blocks + (long)blocks;
            g_game_state.idle_frost_blocks = total_blocks > 2147483647L ? 2147483647 : (int)total_blocks;
        }
        g_sim.offline_hours = elapsed_s / 3600.0;
        g_sim.offline_gold = g;
        g_sim.offline_blocks = blocks;
        for (int i = 0; i < MAX_FLOATERS; ++i) g_sim.floaters[i].active = false;
        for (int i = 0; i < MAX_SPRITE_FX; ++i) g_sim.sprite_fx[i].active = false;
        spawn_reward_burst(DESIGN_W * 0.5F, DESIGN_H * 0.5F + 16.0F, 210.0F);
        spawn_reward_coin_fountain(DESIGN_W * 0.5F - 112.0F, DESIGN_H * 0.5F + 4.0F);
        g_sim.reward_offline_pulse = 0.86F;
        game_audio_play(GAME_AUDIO_CUE_NOTIFY);
        game_state_mark_dirty();
    }
    if (gold_out) *gold_out = g;
    if (blocks_out) *blocks_out = blocks;
    return (g > 0 || blocks > 0);
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
    g_game_state.idle_frost_blocks = 0;
    g_game_state.idle_keep_rank = 0;
    g_game_state.idle_room_gate_repaired = false;
    g_game_state.idle_room_forge_repaired = false;
    g_game_state.idle_room_campfire_repaired = false;
    g_game_state.idle_pending_card_choice = false;
    g_game_state.idle_card_choices_made = 0;
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
    if (g_sim.reward_keep_pulse > 0.0F) g_sim.reward_keep_pulse = clampf(g_sim.reward_keep_pulse - dt, 0.0F, 10.0F);
    if (g_sim.reward_card_pulse > 0.0F) g_sim.reward_card_pulse = clampf(g_sim.reward_card_pulse - dt, 0.0F, 10.0F);
    if (g_sim.reward_shard_pulse > 0.0F) g_sim.reward_shard_pulse = clampf(g_sim.reward_shard_pulse - dt, 0.0F, 10.0F);
    if (g_sim.reward_avalanche_pulse > 0.0F) g_sim.reward_avalanche_pulse = clampf(g_sim.reward_avalanche_pulse - dt, 0.0F, 10.0F);
    if (g_sim.reward_offline_pulse > 0.0F) g_sim.reward_offline_pulse = clampf(g_sim.reward_offline_pulse - dt, 0.0F, 10.0F);
    if (g_sim.prestige_armed_timer > 0.0F) {
        g_sim.prestige_armed_timer -= dt;
        if (g_sim.prestige_armed_timer <= 0.0F) g_sim.prestige_armed = false;
    }
    update_effects(dt);

    if (g_game_state.idle_pending_card_choice) {
        return;
    }

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

    /* Monster stream advance (progression coords): the FRONT enemy walks down to
     * the fight slot (prog->0) next to the hero; queued enemies hold staggered
     * positions up the path (small + faded), never stacking on the front. */
    {
        if (g_sim.hero_lunge > 0.0F) g_sim.hero_lunge -= dt;
        int front = front_monster_index();
        for (int i = 0; i < MAX_MONSTERS; ++i) {
            Monster *m = &g_sim.monsters[i];
            if (!m->alive) continue;
            if (m->flash > 0.0F) m->flash -= dt;
            if (m->knockback > 0.0F) m->knockback -= dt;
            if (m->spawn_pop > 0.0F) m->spawn_pop -= dt;
            /* The front advances to the slot; everyone else stops a queue gap
             * behind whoever is ahead of them (monotone push, no overlap). */
            float floor_prog = (i == front) ? 0.0F : (min_prog_excluding(i) + 0.34F);
            if (floor_prog > 1.0F) floor_prog = 1.0F;
            if (m->prog > floor_prog) {
                m->prog -= MON_WALK_RATE * dt;
                if (m->prog < floor_prog) m->prog = floor_prog;
            }
        }

        /* Hero auto-attacks the front monster once it has reached the slot. */
        if (g_sim.hero_attack_cd > 0.0F) g_sim.hero_attack_cd -= dt;
        if (front >= 0) {
            Monster *m = &g_sim.monsters[front];
            bool engaged = (m->prog <= 0.05F);
            if (engaged && g_sim.hero_attack_cd <= 0.0F) {
                g_sim.hero_attack_cd = (float)hero_attack_interval();
                g_sim.hero_lunge = HERO_LUNGE_TIME; /* hero pokes forward */
                double dmg = hero_damage();
                m->hp -= dmg;
                m->flash = HIT_FLASH_TIME;
                m->knockback = 0.12F; /* small recoil */
                float mx, my, msc, ma;
                monster_screen(m, &mx, &my, &msc, &ma);
                float top = my + 70.0F * msc;
                spawn_hit_spark(mx, my + 8.0F);
                char d[16];
                char n[16];
                fmt_num(n, sizeof(n), dmg);
                (void)snprintf(d, sizeof(d), "-%s", n);
                const float dc[4] = {1.0F, 0.96F, 0.86F, 1.0F};
                spawn_floater(mx + 14.0F, top, d, dc, m->is_boss ? 30.0F : 26.0F);
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

/* Upgrade panel layout (design units, y-up). Taller buttons so the 3 text rows
 * (name+Lv / effect / cost) each get room and read at a glance. */
#define PANEL_SLOT_W 214.0F
#define PANEL_SLOT_H 88.0F
#define PANEL_GAP 14.0F
#define PANEL_CY 56.0F
#define TRAINING_SLOT_COUNT 3

static int training_upgrade_at(int slot) {
    switch (slot) {
        case 0: return UP_SWORD;
        case 1: return UP_BOOTS;
        case 2: return UP_LUCK;
        default: return UP_SWORD;
    }
}

static void panel_slot_rect(int i, float *cx, float *cy, float *w, float *h) {
    const int slots = TRAINING_SLOT_COUNT;
    const float total = slots * PANEL_SLOT_W + (slots - 1) * PANEL_GAP;
    float x0 = DESIGN_W * 0.5F - total * 0.5F + PANEL_SLOT_W * 0.5F;
    *cx = x0 + (float)i * (PANEL_SLOT_W + PANEL_GAP);
    *cy = PANEL_CY;
    *w = PANEL_SLOT_W;
    *h = PANEL_SLOT_H;
}

/* Frost Blueprint panel: permanent Frost Shard spend, kept compact and anchored
 * away from the hero/repair CTA so it reads as meta progress, not another run
 * action. Camp Supplies is hidden until offline is relevant. */
#define SHARD_PANEL_CX (DESIGN_W - 142.0F)
#define SHARD_PANEL_ROW_H 40.0F
#define SHARD_PANEL_W 268.0F
#define SHARD_PANEL_GAP 8.0F

static bool shard_upgrade_visible(int which) {
    if (which == SH_OFFLINE) {
        return g_game_state.idle_offline_unlocked || g_game_state.idle_shard_offline_rate > 0;
    }
    return true;
}

static bool show_shard_panel(void) {
    return g_game_state.idle_frost_shards > 0 ||
           g_game_state.idle_shard_global_damage > 0 ||
           g_game_state.idle_shard_global_gold > 0 ||
           g_game_state.idle_shard_start_stage > 0 ||
           g_game_state.idle_shard_offline_rate > 0;
}

static int shard_panel_visible_rows(void) {
    int rows = 0;
    for (int i = 0; i < SH_COUNT; ++i) {
        if (shard_upgrade_visible(i)) rows += 1;
    }
    return rows;
}

static int shard_upgrade_at_visible_slot(int slot) {
    int seen = 0;
    for (int i = 0; i < SH_COUNT; ++i) {
        if (!shard_upgrade_visible(i)) continue;
        if (seen == slot) return i;
        seen += 1;
    }
    return SH_DMG;
}

static float shard_panel_h(void) {
    return 52.0F + (float)shard_panel_visible_rows() * (SHARD_PANEL_ROW_H + SHARD_PANEL_GAP) + 10.0F;
}

static float shard_panel_cy(void) {
    return 14.0F + shard_panel_h() * 0.5F;
}

static void shard_row_rect(int slot, float *cx, float *cy, float *w, float *h) {
    const float ph = shard_panel_h();
    const float top = shard_panel_cy() + ph * 0.5F - 84.0F;
    *cx = SHARD_PANEL_CX;
    *cy = top - (float)slot * (SHARD_PANEL_ROW_H + SHARD_PANEL_GAP);
    *w = SHARD_PANEL_W - 24.0F;
    *h = SHARD_PANEL_ROW_H;
}

static int shard_upgrade_level(int which) {
    switch (which) {
        case SH_DMG: return g_game_state.idle_shard_global_damage;
        case SH_GOLD: return g_game_state.idle_shard_global_gold;
        case SH_START: return g_game_state.idle_shard_start_stage;
        case SH_OFFLINE: return g_game_state.idle_shard_offline_rate;
        default: return 0;
    }
}

static const char *shard_upgrade_name(int which) {
    switch (which) {
        case SH_DMG: return "Sharper Steel";
        case SH_GOLD: return "Rich Veins";
        case SH_START: return "Head Start";
        case SH_OFFLINE: return "Camp Supplies";
        default: return "Blueprint";
    }
}

static const char *shard_upgrade_effect(int which) {
    switch (which) {
        case SH_DMG: return "Permanent DMG +10%";
        case SH_GOLD: return "Permanent Gold +10%";
        case SH_START: return "Start +1 Stage";
        case SH_OFFLINE: return "Offline return +5%";
        default: return "Permanent boost";
    }
}

/* Avalanche Reset button rect (top-center banner area), shown only when unlocked. */
static void prestige_btn_rect(float *cx, float *cy, float *w, float *h) {
    *cx = DESIGN_W * 0.5F;
    *cy = DESIGN_H - 40.0F;
    *w = 282.0F;
    *h = 44.0F;
}

/* Collect-offline button rect (centered popup). */
static void collect_btn_rect(float *cx, float *cy, float *w, float *h) {
    *cx = DESIGN_W * 0.5F;
    *cy = DESIGN_H * 0.5F - 72.0F;
    *w = 200.0F;
    *h = 44.0F;
}

static bool show_training_panel(void) {
    return g_game_state.idle_room_forge_repaired;
}

static int next_repair_room(void) {
    for (int i = 0; i < ROOM_COUNT; ++i) {
        if (!keep_room_repaired(i)) return i;
    }
    return -1;
}

static int keep_rooms_repaired_count(void) {
    int n = 0;
    for (int i = 0; i < ROOM_COUNT; ++i) {
        if (keep_room_repaired(i)) n += 1;
    }
    return n;
}

static void main_repair_btn_rect(float *cx, float *cy, float *w, float *h) {
    *cx = DESIGN_W * 0.5F;
    *cy = 76.0F;
    *w = 312.0F;
    *h = 76.0F;
}

static void room_row_rect(int room, float *cx, float *cy, float *w, float *h) {
    *cx = DESIGN_W - 142.0F;
    *cy = DESIGN_H - 172.0F - (float)room * 60.0F;
    *w = 242.0F;
    *h = 52.0F;
}

static void card_rect(int card, float *cx, float *cy, float *w, float *h) {
    *cx = DESIGN_W * 0.5F + ((float)card - 1.0F) * 190.0F;
    *cy = 144.0F;
    *w = 172.0F;
    *h = 108.0F;
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
        (void)px; (void)py; (void)w; (void)h;
        const float c[4] = {0.70F, 0.94F, 1.0F, 1.0F};
        spawn_floater(cx, cy + 56.0F, "Collected!", c, 22.0F);
        spawn_reward_coin_fountain(DESIGN_W * 0.5F, DESIGN_H * 0.5F - 18.0F);
        g_sim.reward_offline_pulse = 0.72F;
        game_audio_play(GAME_AUDIO_CUE_SUCCESS);
        return true;
    }
    if (g_game_state.idle_pending_card_choice) {
        for (int i = 0; i < CARD_COUNT; ++i) {
            float cx, cy, w, h;
            card_rect(i, &cx, &cy, &w, &h);
            if (in_rect(px, py, cx, cy, w, h)) {
                choose_card(i);
                return true;
            }
        }
    }
    if (!g_game_state.idle_pending_card_choice) {
        int room = next_repair_room();
        float cx, cy, w, h;
        main_repair_btn_rect(&cx, &cy, &w, &h);
        if (room >= 0 && in_rect(px, py, cx, cy, w, h)) {
            if (!repair_room(room)) {
                const float c[4] = {1.0F, 0.72F, 0.45F, 1.0F};
                spawn_floater(cx, cy + 48.0F, "Need Blocks", c, 20.0F);
                game_audio_play(GAME_AUDIO_CUE_ERROR);
            }
            return true;
        }
    }
    /* permanent Frost Shard spend */
    if (!g_sim.offline_popup && show_shard_panel()) {
        const int rows = shard_panel_visible_rows();
        for (int slot = 0; slot < rows; ++slot) {
            float cx, cy, w, h;
            shard_row_rect(slot, &cx, &cy, &w, &h);
            if (in_rect(px, py, cx, cy, w, h)) {
                int which = shard_upgrade_at_visible_slot(slot);
                bool bought = buy_shard_upgrade(which);
                const float okc[4] = {0.70F, 0.94F, 1.0F, 1.0F};
                const float noc[4] = {1.0F, 0.72F, 0.45F, 1.0F};
                spawn_floater(cx, cy - 28.0F, bought ? "Learned!" : "Need Shards",
                              bought ? okc : noc, 18.0F);
                if (bought) {
                    spawn_reward_burst(cx - w * 0.34F, cy + 4.0F, 116.0F);
                    g_sim.reward_shard_pulse = 0.64F;
                    game_audio_play(GAME_AUDIO_CUE_SUCCESS);
                } else {
                    game_audio_play(GAME_AUDIO_CUE_ERROR);
                }
                return true;
            }
        }
    }
    for (int i = 0; i < ROOM_COUNT; ++i) {
        float cx, cy, w, h;
        room_row_rect(i, &cx, &cy, &w, &h);
        if (in_rect(px, py, cx, cy, w, h)) {
            if (!repair_room(i)) {
                const float c[4] = {1.0F, 0.72F, 0.45F, 1.0F};
                spawn_floater(cx, cy + 30.0F, "Need Blocks", c, 18.0F);
                game_audio_play(GAME_AUDIO_CUE_ERROR);
            }
            return true;
        }
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
                spawn_floater(DESIGN_W * 0.5F, DESIGN_H - 70.0F, "Tap again: Avalanche", c, 20.0F);
                game_audio_play(GAME_AUDIO_CUE_NOTIFY);
            }
            return true;
        }
    }
    /* upgrade panel */
    if (!g_sim.offline_popup && show_training_panel()) {
        for (int slot = 0; slot < TRAINING_SLOT_COUNT; ++slot) {
            int i = training_upgrade_at(slot);
            float cx, cy, w, h;
            panel_slot_rect(slot, &cx, &cy, &w, &h);
            if (in_rect(px, py, cx, cy, w, h)) {
                if (!buy_upgrade(i)) {
                    const float c[4] = {1.0F, 0.72F, 0.45F, 1.0F};
                    spawn_floater(cx, cy + 38.0F, "Need Gold", c, 18.0F);
                    game_audio_play(GAME_AUDIO_CUE_ERROR);
                }
                return true;
            }
        }
    }
    return false;
}

/* Convert a framebuffer pointer pixel to design-space (Y flips to bottom-up). */
static void pointer_to_design(float px, float py, float *dx, float *dy) {
    const float fw = (float)canvas_w();
    const float fh = (float)canvas_h();
    /* Invert the SAME contain transform the projection uses, so a click lands on
     * the element drawn under the cursor at any window aspect (no stretch skew). */
    float L, R, B, T;
    world_bounds(&L, &R, &B, &T);
    *dx = (fw > 0.0F) ? (L + px / fw * (R - L)) : px;
    *dy = (fh > 0.0F) ? (B + (fh - py) / fh * (T - B)) : py;
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

static void emit_world_forge_marker(float cx, float cy) {
    float pulse = 0.5F + 0.5F * sinf(g_sim.time * 4.2F);
    emit_ground_shadow(cx, cy - 20.0F, 74.0F, 0.26F);
    emit_quad(cx, cy - 2.0F, 80.0F, 56.0F, 0.14F, 0.11F, 0.12F, 0.80F);
    emit_quad(cx, cy + 20.0F, 68.0F, 16.0F, 0.36F, 0.39F, 0.45F, 0.95F);
    emit_quad(cx - 6.0F, cy + 4.0F, 46.0F, 22.0F, 0.20F, 0.22F, 0.28F, 0.95F);
    emit_quad(cx + 22.0F, cy + 26.0F, 16.0F, 42.0F, 0.18F, 0.16F, 0.18F, 0.95F);
    emit_quad(cx + 22.0F, cy + 52.0F, 24.0F, 8.0F, 0.28F, 0.26F, 0.28F, 0.90F);
    emit_quad(cx - 20.0F, cy - 4.0F, 32.0F, 24.0F, 1.0F, 0.46F, 0.13F, 0.42F + 0.18F * pulse);
    emit_h(R_HIT_SPARK, cx - 20.0F, cy + 2.0F, 38.0F, pack_rgba(1.0F, 0.62F, 0.18F, 0.75F));
}

static void emit_world_campfire_marker(float cx, float cy) {
    float pulse = 0.5F + 0.5F * sinf(g_sim.time * 5.4F);
    emit_ground_shadow(cx, cy - 14.0F, 66.0F, 0.28F);
    emit_quad(cx - 14.0F, cy - 12.0F, 44.0F, 10.0F, 0.30F, 0.16F, 0.08F, 0.96F);
    emit_quad(cx + 14.0F, cy - 12.0F, 44.0F, 10.0F, 0.28F, 0.14F, 0.07F, 0.96F);
    emit_quad(cx, cy + 2.0F, 54.0F, 38.0F, 1.0F, 0.68F, 0.10F, 0.26F + 0.18F * pulse);
    emit_h(R_HIT_SPARK, cx, cy + 10.0F, 48.0F, pack_rgba(1.0F, 0.72F, 0.18F, 0.88F));
    emit_h(R_COIN, cx, cy + 4.0F, 28.0F, pack_rgba(1.0F, 0.42F, 0.12F, 0.72F));
}

static void compose_reward_pulses(void) {
    if (g_sim.reward_avalanche_pulse > 0.0F) {
        float a = clampf(g_sim.reward_avalanche_pulse / 0.92F, 0.0F, 1.0F);
        emit_quad(DESIGN_W * 0.5F, DESIGN_H * 0.5F, DESIGN_W, DESIGN_H,
                  0.72F, 0.92F, 1.0F, 0.12F * a);
        emit_h(R_LEVELUP_BURST, KEEP_CX, KEEP_CY - 18.0F, 250.0F + 44.0F * (1.0F - a),
               pack_rgba(0.72F, 0.94F, 1.0F, 0.46F * a));
    }
    if (g_sim.reward_keep_pulse > 0.0F) {
        float a = clampf(g_sim.reward_keep_pulse / 0.78F, 0.0F, 1.0F);
        emit_h(R_LEVELUP_BURST, KEEP_CX, KEEP_CY - 28.0F, 250.0F + 40.0F * (1.0F - a),
               pack_rgba(0.72F, 1.0F, 0.86F, 0.52F * a));
        emit_quad(KEEP_CX, KEEP_CY - 62.0F, 245.0F, 34.0F, 0.42F, 1.0F, 0.76F, 0.18F * a);
    }
    if (g_sim.reward_card_pulse > 0.0F) {
        float a = clampf(g_sim.reward_card_pulse / 0.62F, 0.0F, 1.0F);
        emit_h(R_LEVELUP_BURST, DESIGN_W * 0.5F, 188.0F, 224.0F + 42.0F * (1.0F - a),
               pack_rgba(0.95F, 0.82F, 1.0F, 0.58F * a));
        emit_quad(DESIGN_W * 0.5F, 144.0F, 570.0F, 132.0F, 0.72F, 0.46F, 1.0F, 0.10F * a);
    }
    if (g_sim.reward_shard_pulse > 0.0F) {
        float a = clampf(g_sim.reward_shard_pulse / 0.64F, 0.0F, 1.0F);
        emit_h(R_LEVELUP_BURST, SHARD_PANEL_CX, shard_panel_cy() + 12.0F, 176.0F + 36.0F * (1.0F - a),
               pack_rgba(0.66F, 0.94F, 1.0F, 0.56F * a));
    }
    if (g_sim.reward_offline_pulse > 0.0F) {
        float a = clampf(g_sim.reward_offline_pulse / 0.86F, 0.0F, 1.0F);
        emit_h(R_LEVELUP_BURST, DESIGN_W * 0.5F, DESIGN_H * 0.5F + 16.0F,
               238.0F + 44.0F * (1.0F - a), pack_rgba(0.78F, 0.96F, 1.0F, 0.54F * a));
        emit_quad(DESIGN_W * 0.5F, DESIGN_H * 0.5F, 560.0F, 226.0F, 0.24F, 0.76F, 1.0F, 0.08F * a);
    }
}

static void compose_scene(void) {
    const uint32_t white = 0xFFFFFFFFu;

    /* Full-bleed backdrop: COVER the whole visible area (which extends past the
     * 16:9 design rect on a differently-shaped window) with the background,
     * UNDISTORTED + centered, so a non-16:9 window shows more snow/sky rather
     * than stretched art or empty bars. Gameplay + UI stay in the centered
     * design rect (see world_bounds). */
    {
        float L, R, B, T;
        world_bounds(&L, &R, &B, &T);
        const float vis_w = R - L, vis_h = T - B;
        const float bg_aspect = (s_region_h[R_BACKGROUND] > 0)
            ? ((float)s_region_w[R_BACKGROUND] / (float)s_region_h[R_BACKGROUND])
            : (DESIGN_W / DESIGN_H);
        float bg_w = vis_w, bg_h = bg_w / bg_aspect;
        if (bg_h < vis_h) { bg_h = vis_h; bg_w = bg_h * bg_aspect; }
        emit_sprite(R_BACKGROUND, (L + R) * 0.5F, (B + T) * 0.5F, bg_w, bg_h, white);
    }

    /* The Frost Keep sits at the top of the path (the climb goal -- backdrop). */
    emit_h(R_KEEP, KEEP_CX, KEEP_CY, DESIGN_H * 0.30F, white);

    /* Framing scenery (kept toward the edges so the FIGHT is the focal point). */
    emit_h(R_PINE, DESIGN_W * 0.06F, DESIGN_H * 0.22F, DESIGN_H * 0.50F, white);
    emit_h(R_PINE, DESIGN_W * 0.95F, DESIGN_H * 0.20F, DESIGN_H * 0.52F, white);
    emit_h(R_ROCK, DESIGN_W * 0.86F, DESIGN_H * 0.30F, DESIGN_H * 0.12F, white);

    if (g_game_state.idle_room_forge_repaired) {
        emit_world_forge_marker(FORGE_MARKER_X, FORGE_MARKER_Y);
    }
    if (g_game_state.idle_room_campfire_repaired) {
        emit_world_campfire_marker(CAMPFIRE_MARKER_X, CAMPFIRE_MARKER_Y);
    }

    const int front = front_monster_index();

    /* Monsters: draw far-to-near (highest prog first) so the engaged enemy is on
     * top and prominent; queued ones recede small + faded up the path. */
    for (int pass = 0; pass < MAX_MONSTERS; ++pass) {
        int best = -1;
        float best_prog = -1.0F;
        for (int i = 0; i < MAX_MONSTERS; ++i) {
            if (!g_sim.monsters[i].alive) continue;
            int rank = 0;
            for (int j = 0; j < MAX_MONSTERS; ++j) {
                if (g_sim.monsters[j].alive && g_sim.monsters[j].prog > g_sim.monsters[i].prog) rank++;
            }
            if (rank == pass && g_sim.monsters[i].prog > best_prog) { best = i; best_prog = g_sim.monsters[i].prog; }
        }
        if (best < 0) continue;
        Monster *m = &g_sim.monsters[best];
        const bool is_front = (best == front);

        float sx, sy, sscale, salpha;
        monster_screen(m, &sx, &sy, &sscale, &salpha);

        /* spawn-in pop: ease the scale up so an enemy "arrives" with a punch. */
        float pop = (m->spawn_pop > 0.0F)
                        ? (1.0F - 0.35F * clampf(m->spawn_pop / 0.5F, 0.0F, 1.0F))
                        : 1.0F;
        /* knockback recoil nudges the enemy back up-path briefly on a hit. */
        float kb = (m->knockback > 0.0F) ? (m->knockback / 0.12F) : 0.0F;
        float kbx = (PATH_FAR_X - FIGHT_X);
        float kby = (PATH_FAR_Y - FIGHT_Y);
        float kbl = sqrtf(kbx * kbx + kby * kby);
        if (kbl > 0.0F) { kbx /= kbl; kby /= kbl; }
        sx += kbx * 10.0F * kb;
        sy += kby * 10.0F * kb;

        float base_h = m->is_boss ? (DESIGN_H * 0.48F) : (DESIGN_H * 0.30F);
        float enemy_h = base_h * sscale * pop;
        float feet = sy - enemy_h * 0.42F;

        emit_ground_shadow(sx, feet, enemy_h * 0.62F, 0.30F * salpha);

        /* Target ring + glow under the engaged front enemy (focal cue). */
        if (is_front && m->prog <= 0.08F) {
            float pulse = 0.5F + 0.5F * sinf(g_sim.time * 7.0F);
            float rs = enemy_h * (0.62F + 0.10F * pulse);
            emit_quad(sx, feet, rs * 1.15F, rs * 0.5F, 1.0F, 0.86F, 0.30F, 0.20F + 0.18F * pulse);
            emit_quad(sx, feet, rs, rs * 0.40F, 1.0F, 0.78F, 0.18F, 0.30F + 0.30F * pulse);
        }

        uint32_t tint = m->is_boss ? pack_rgba(1.0F, 0.58F, 0.55F, salpha) : pack_rgba(1.0F, 1.0F, 1.0F, salpha);
        emit_h(R_ENEMY, sx, sy, enemy_h, tint);
        if (m->flash > 0.0F) {
            float a = clampf(m->flash / HIT_FLASH_TIME, 0.0F, 1.0F) * 0.85F;
            emit_h(R_ENEMY, sx, sy, enemy_h, pack_rgba(1.0F, 1.0F, 1.0F, a));
        }

        /* Boss telegraph: a pulsing red warning ring as the boss strides in. */
        if (m->is_boss && m->spawn_pop > 0.0F) {
            float warn = clampf(m->spawn_pop / 0.5F, 0.0F, 1.0F);
            float pr = 0.5F + 0.5F * sinf(g_sim.time * 18.0F);
            emit_quad(sx, sy, enemy_h * (0.85F + 0.25F * pr), enemy_h * (1.05F + 0.1F * pr),
                      1.0F, 0.25F, 0.20F, warn * 0.45F * pr);
        }

        /* HP pip above the enemy (only the front shows it big; queued = none). */
        if (is_front) {
            float frac = (m->max_hp > 0.0) ? clampf((float)(m->hp / m->max_hp), 0.0F, 1.0F) : 0.0F;
            float bw = m->is_boss ? 168.0F : 78.0F;
            float by = sy + enemy_h * 0.62F;
            emit_quad(sx, by, bw + 8.0F, 14.0F, 0.08F, 0.06F, 0.09F, 0.9F);
            emit_quad(sx - bw * 0.5F + bw * frac * 0.5F, by, bw * frac, 9.0F,
                      m->is_boss ? 1.0F : 0.95F, m->is_boss ? 0.35F : 0.32F, 0.28F, 1.0F);
        }
    }

    /* Hero (stationary, idle bob + attack lunge toward the fight slot). */
    float hero_h = DESIGN_H * 0.36F;
    float bob = sinf(g_sim.time * 4.0F) * 3.0F;
    /* lunge: quick forward poke (toward the enemy) then ease back. */
    float lunge = 0.0F;
    if (g_sim.hero_lunge > 0.0F) {
        float lt = g_sim.hero_lunge / HERO_LUNGE_TIME;       /* 1 -> 0 */
        lunge = sinf(lt * 3.14159F) * 18.0F;                 /* peak mid-lunge */
    }
    float hero_x = HERO_X + lunge;
    float hero_feet = HERO_Y - hero_h * 0.42F;
    emit_ground_shadow(HERO_X, hero_feet, hero_h * 0.62F, 0.34F);
    /* face up the path toward the enemy (default art faces right = toward slot). */
    emit_h(R_HERO, hero_x, HERO_Y + bob, hero_h, white);
    if (g_sim.hero_flash > 0.0F) {
        float a = clampf(g_sim.hero_flash / HIT_FLASH_TIME, 0.0F, 1.0F) * 0.7F;
        emit_h(R_HERO, hero_x, HERO_Y + bob, hero_h, pack_rgba(1.0F, 0.3F, 0.3F, a));
    }
    if (g_game_state.idle_room_campfire_repaired) {
        float helper_h = hero_h * 0.46F;
        float helper_x = HERO_X - 88.0F + sinf(g_sim.time * 3.2F) * 2.0F;
        float helper_y = HERO_Y - 16.0F + bob * 0.45F;
        emit_ground_shadow(helper_x, helper_y - helper_h * 0.42F, helper_h * 0.62F, 0.26F);
        emit_h(R_HERO, helper_x, helper_y, helper_h, pack_rgba(0.68F, 0.94F, 1.0F, 0.96F));
        emit_quad(helper_x, helper_y + helper_h * 0.34F, helper_h * 0.60F, helper_h * 0.22F,
                  0.35F, 0.92F, 1.0F, 0.28F + 0.12F * sinf(g_sim.time * 5.0F));
    }

    compose_reward_pulses();

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
        case UP_BOOTS: return R_BOOTS_ICON;
        case UP_ARMOR: return R_ARMOR_ICON;
        case UP_LUCK:  return R_LUCK_ICON;
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

/* Top-left HUD plate geometry (shared by sprite + text passes so the dark plate
 * always FULLY contains the gold/stage strings + the progress bar). */
#define HUD_PLATE_CX 178.0F
#define HUD_PLATE_W 332.0F
#define HUD_TOP_Y (DESIGN_H - 30.0F)   /* Gold row baseline */
#define HUD_MID_Y (DESIGN_H - 58.0F)   /* Frost Blocks row baseline */
#define HUD_RANK_Y (DESIGN_H - 86.0F)  /* Keep rank row baseline */
#define HUD_BAR_Y (DESIGN_H - 116.0F)  /* progress bar center */
#define HUD_BAR_W 288.0F
#define HUD_ICON_X 34.0F
#define HUD_TEXT_X 66.0F

/* Show the Frost-Shards (meta currency) HUD card only once it is relevant -- an
 * empty "Frost Shards: 0" early just clutters the corner (refs hide the meta
 * currency until it is earned). */
static bool show_shards_hud(void) {
    if (show_shard_panel()) return false;
    return g_game_state.idle_frost_shards > 0 || prestige_unlocked() ||
           g_game_state.idle_shard_global_damage > 0 || g_game_state.idle_shard_global_gold > 0 ||
           g_game_state.idle_shard_start_stage > 0 || g_game_state.idle_shard_offline_rate > 0;
}

#define SHARD_HUD_CX (DESIGN_W - 102.0F)
#define SHARD_HUD_CY (DESIGN_H - 28.0F)
#define KEEP_PANEL_CX (DESIGN_W - 142.0F)
#define KEEP_PANEL_FULL_CY (DESIGN_H - 214.0F)
#define KEEP_PANEL_COMPACT_CY (DESIGN_H - 154.0F)

static void compose_hud(void) {
    const uint32_t white = 0xFFFFFFFFu;

    /* Top-left status card: one frosted blue panel holding the coin + "Gold N"
     * + "Stage N" + the kills bar, so no number spills onto the bright sky. */
    emit_round_panel(HUD_PLATE_CX, DESIGN_H - 76.0F, HUD_PLATE_W, 118.0F, 0.115F, 0.205F, 0.285F, 0.92F);
    emit_h(R_COIN, HUD_ICON_X, HUD_TOP_Y + 5.0F, 32.0F, white);
    emit_h(R_ROCK, HUD_ICON_X + 1.0F, HUD_MID_Y + 5.0F, 30.0F, pack_rgba(0.78F, 0.94F, 1.0F, 1.0F));

    /* Keep rebuild panel: full room list before meta, compact objective strip
     * after Frost Blueprints are relevant so the right rail no longer stacks
     * two dense lists on top of each other. */
    if (show_shard_panel()) {
        emit_round_panel(KEEP_PANEL_CX, KEEP_PANEL_COMPACT_CY, 268.0F, 94.0F, 0.105F, 0.190F, 0.270F, 0.93F);
        float cx = KEEP_PANEL_CX, cy = KEEP_PANEL_COMPACT_CY - 22.0F;
        const int room = next_repair_room();
        const bool affordable = room >= 0 && g_game_state.idle_frost_blocks >= keep_room_cost(room);
        emit_round_panel(cx, cy, 232.0F, 34.0F,
                         affordable ? 0.135F : 0.125F,
                         affordable ? 0.315F : 0.165F,
                         affordable ? 0.410F : 0.215F,
                         0.96F);
    } else {
        emit_round_panel(KEEP_PANEL_CX, KEEP_PANEL_FULL_CY, 268.0F, 238.0F, 0.105F, 0.190F, 0.270F, 0.93F);
        for (int i = 0; i < ROOM_COUNT; ++i) {
            float cx, cy, w, h;
            room_row_rect(i, &cx, &cy, &w, &h);
            bool repaired = keep_room_repaired(i);
            bool affordable = g_game_state.idle_frost_blocks >= keep_room_cost(i);
            if (repaired) {
                emit_round_panel(cx, cy, w, h, 0.125F, 0.355F, 0.275F, 0.96F);
            } else if (affordable) {
                emit_round_panel(cx, cy, w, h, 0.135F, 0.315F, 0.410F, 0.96F);
            } else {
                emit_round_panel(cx, cy, w, h, 0.125F, 0.165F, 0.215F, 0.96F);
            }
        }
    }

    /* Frost-Shards card (top-right, under the minimap) only when meta currency
     * is relevant -- keeps the early-game corner clean. */
    if (show_shards_hud()) {
        emit_round_panel(SHARD_HUD_CX, SHARD_HUD_CY, 184.0F, 46.0F, 0.120F, 0.205F, 0.315F, 0.92F);
    }

    if (!g_sim.offline_popup && show_shard_panel()) {
        const float ph = shard_panel_h();
        const float pcx = SHARD_PANEL_CX;
        const float pcy = shard_panel_cy();
        emit_round_panel(pcx, pcy, SHARD_PANEL_W, ph, 0.100F, 0.175F, 0.275F, 0.95F);
        const int rows = shard_panel_visible_rows();
        for (int slot = 0; slot < rows; ++slot) {
            const int which = shard_upgrade_at_visible_slot(slot);
            float cx, cy, w, h;
            shard_row_rect(slot, &cx, &cy, &w, &h);
            bool affordable = (double)g_game_state.idle_frost_shards >= shard_cost(which);
            if (affordable) {
                emit_round_panel(cx, cy, w, h, 0.120F, 0.305F, 0.385F, 0.96F);
            } else {
                emit_round_panel(cx, cy, w, h, 0.125F, 0.160F, 0.215F, 0.96F);
            }
            emit_h(R_BADGE, cx - w * 0.5F + 23.0F, cy + 1.0F, 26.0F,
                   affordable ? 0xFFFFFFFFu : pack_rgba(0.58F, 0.68F, 0.76F, 1.0F));
        }
    }

    /* Stage progress bar inside the plate (kills toward next stage). */
    {
        float boss_max = (g_sim.boss_timer_max > 0.0F) ? g_sim.boss_timer_max : (float)VH_BOSS_TIMER_S;
        float frac = g_game_state.idle_boss_active
                         ? clampf(g_sim.boss_timer / boss_max, 0.0F, 1.0F)
                         : clampf((float)g_game_state.idle_kills_in_stage / (float)VH_KILLS_PER_STAGE, 0.0F, 1.0F);
        float bx = HUD_PLATE_CX, by = HUD_BAR_Y, bw = HUD_BAR_W, bh = 18.0F;
        /* dark bar groove */
        emit_quad(bx, by, bw + 6.0F, bh + 6.0F, 0.03F, 0.025F, 0.05F, 1.0F);
        float r = g_game_state.idle_boss_active ? 1.0F : 0.35F;
        float g = g_game_state.idle_boss_active ? 0.55F : 0.85F;
        float b = g_game_state.idle_boss_active ? 0.30F : 0.50F;
        float fx = bx - bw * 0.5F + bw * frac * 0.5F;
        emit_quad(fx, by, bw * frac, bh, r, g, b, 1.0F);
        /* glossy top highlight on the fill */
        if (frac > 0.001F) {
            emit_quad(fx, by + bh * 0.26F, bw * frac, bh * 0.32F,
                      clampf(r + 0.18F, 0.0F, 1.0F), clampf(g + 0.12F, 0.0F, 1.0F), clampf(b + 0.12F, 0.0F, 1.0F), 0.55F);
        }
    }

    /* ---- Bottom upgrade panel: 4 buttons (icon + name+Lv / effect / cost). ----
     * Affordable buttons read bright GREEN + a soft pulse "buy now" ring;
     * unaffordable ones are dimmed grey so the player can tell at a glance which
     * upgrades they can buy. */
    if (!g_sim.offline_popup && show_training_panel()) {
        /* SOLID full-width tray + a bright top rim, so the upgrades read as a
         * grounded UI panel (Clicker Heroes / Tap Titans 2) instead of buttons
         * floating on the snow. */
        emit_quad(DESIGN_W * 0.5F, 58.0F, DESIGN_W, 116.0F, 0.075F, 0.145F, 0.205F, 0.94F);
        emit_quad(DESIGN_W * 0.5F, 115.0F, DESIGN_W, 3.0F, 0.55F, 0.82F, 1.0F, 0.58F);

        for (int slot = 0; slot < TRAINING_SLOT_COUNT; ++slot) {
            int i = training_upgrade_at(slot);
            float cx, cy, w, h;
            panel_slot_rect(slot, &cx, &cy, &w, &h);
            bool affordable = (double)g_game_state.idle_gold >= upgrade_cost(i);

            /* Card surface: ONE neutral frosted plate for every slot (clean,
             * consistent). Affordability is shown by the cost pill + icon, not by
             * tinting the whole card, so the panel never looks muddy. */
            emit_round_panel(cx, cy, w, h, 0.125F, 0.215F, 0.295F, 0.96F);

            /* Upgrade icon, left column. */
            float ix = cx - w * 0.5F + 32.0F;
            float iy = cy + 12.0F;
            emit_h(upgrade_icon_region(i), ix, iy, 44.0F,
                   affordable ? white : pack_rgba(0.66F, 0.68F, 0.74F, 1.0F));

            /* Green COST pill across the card bottom: full gloss when buyable,
             * crushed dark when not (so it clearly reads disabled even though the
             * source sprite is green). The whole card stays clickable. */
            float pcx = cx, pcy = cy - h * 0.5F + 20.0F, pw = w - 26.0F, ph = 32.0F;
            uint32_t pill = affordable ? 0xFFFFFFFFu
                                       : pack_rgba(0.30F, 0.36F, 0.36F, 1.0F);
            emit_sprite(R_BUTTON, pcx, pcy, pw, ph, pill);
            emit_h(R_COIN, pcx - pw * 0.5F + 22.0F, pcy, 22.0F,
                   affordable ? white : pack_rgba(0.62F, 0.64F, 0.68F, 1.0F));
        }
    }

    if (!g_sim.offline_popup && g_game_state.idle_pending_card_choice) {
        for (int i = 0; i < CARD_COUNT; ++i) {
            float cx, cy, w, h;
            card_rect(i, &cx, &cy, &w, &h);
            emit_round_panel(cx, cy, w, h, 0.165F, 0.210F, 0.360F, 0.96F);
            emit_h(i == CARD_BLOCK_CACHE ? R_BADGE : upgrade_icon_region(i == CARD_QUICK_HANDS ? UP_BOOTS : UP_SWORD),
                   cx, cy + 20.0F, 48.0F, white);
        }
    } else if (!g_sim.offline_popup && next_repair_room() >= 0) {
        float cx, cy, w, h;
        main_repair_btn_rect(&cx, &cy, &w, &h);
        const int room = next_repair_room();
        const bool affordable = g_game_state.idle_frost_blocks >= keep_room_cost(room);
        emit_sprite(R_BUTTON, cx, cy, w, h,
                    affordable ? white : pack_rgba(0.42F, 0.48F, 0.55F, 1.0F));
        emit_h(R_ROCK, cx - w * 0.5F + 42.0F, cy + 1.0F, 34.0F,
               affordable ? pack_rgba(0.88F, 0.98F, 1.0F, 1.0F) : pack_rgba(0.56F, 0.63F, 0.70F, 1.0F));
    }

    /* ---- Avalanche Reset button (top-center) when unlocked. ---- */
    if (!g_sim.offline_popup && prestige_unlocked()) {
        float cx, cy, w, h;
        prestige_btn_rect(&cx, &cy, &w, &h);
        uint32_t tint = g_sim.prestige_armed ? pack_rgba(1.0F, 0.7F, 0.7F, 1.0F) : pack_rgba(0.6F, 0.85F, 1.0F, 1.0F);
        emit_sprite(R_BUTTON, cx, cy, w, h, tint);
    }

    /* ---- Offline popup panel. ---- */
    if (g_sim.offline_popup) {
        emit_round_panel(DESIGN_W * 0.5F, DESIGN_H * 0.5F, 540.0F, 218.0F, 0.090F, 0.165F, 0.255F, 0.94F);
        emit_round_panel(DESIGN_W * 0.5F - 112.0F, DESIGN_H * 0.5F - 12.0F, 196.0F, 54.0F, 0.145F, 0.205F, 0.285F, 0.96F);
        emit_round_panel(DESIGN_W * 0.5F + 112.0F, DESIGN_H * 0.5F - 12.0F, 196.0F, 54.0F, 0.110F, 0.260F, 0.335F, 0.96F);
        emit_h(R_COIN, DESIGN_W * 0.5F - 188.0F, DESIGN_H * 0.5F - 9.0F, 30.0F, white);
        emit_h(R_ROCK, DESIGN_W * 0.5F + 36.0F, DESIGN_H * 0.5F - 9.0F, 30.0F, pack_rgba(0.82F, 0.96F, 1.0F, 1.0F));
        float cx, cy, w, h;
        collect_btn_rect(&cx, &cy, &w, &h);
        emit_sprite(R_BUTTON, cx, cy, w, h, white);
    }

    nt_sprite_renderer_flush();
}

/* ---- FTUE prompt (<=3 beats from balance.json) ---- */
static const char *ftue_prompt(void) {
    if (g_sim.offline_popup) return NULL;
    if (g_game_state.idle_pending_card_choice) return "Choose one rune card to shape this run.";
    if (!g_game_state.idle_room_gate_repaired) {
        return NULL;
    }
    if (!g_game_state.idle_room_forge_repaired) return NULL;
    if (!g_game_state.idle_room_campfire_repaired) return NULL;
    if (prestige_unlocked()) return NULL;
    return NULL;
}

/* FTUE prompt baseline (shared by the plate + text passes), clear of the panel. */
#define FTUE_Y 216.0F

static void compose_overlays(void) {
    const char *prompt = ftue_prompt();
    if (prompt && !g_game_state.idle_pending_card_choice) {
        /* place the prompt ABOVE the upgrade panel, on a solid plate, clear of it */
        emit_plate(DESIGN_W * 0.5F, FTUE_Y + 6.0F, text_width(prompt, 21.0F) + 48.0F, 40.0F, 0.88F);
    }
    if (g_game_state.idle_pending_card_choice) {
        emit_plate(DESIGN_W * 0.5F, 218.0F, 430.0F, 54.0F, 0.88F);
    }
    if (g_game_state.idle_room_campfire_repaired) {
        const char *helper = "Helper +25% DMG";
        emit_plate(HERO_X - 88.0F, HERO_Y + 76.0F, text_width(helper, 17.0F) + 32.0F, 30.0F, 0.82F);
    }
    if (g_game_state.idle_room_forge_repaired) {
        const char *forge = "Forge lit";
        emit_plate(FORGE_MARKER_X, FORGE_MARKER_Y + 58.0F, text_width(forge, 15.0F) + 26.0F, 26.0F, 0.78F);
    }
    if (g_game_state.idle_room_campfire_repaired) {
        const char *camp = "Campfire";
        emit_plate(CAMPFIRE_MARKER_X, CAMPFIRE_MARKER_Y + 52.0F, text_width(camp, 15.0F) + 26.0F, 26.0F, 0.78F);
    }
    nt_sprite_renderer_flush();
}

static void compose_floaters(void) {
    for (int i = 0; i < MAX_FLOATERS; ++i) {
        Floater *f = &g_sim.floaters[i];
        if (!f->active) continue;
        float life = 1.0F - f->age / f->ttl;
        float col[4] = {f->color[0], f->color[1], f->color[2], clampf(life + 0.2F, 0.0F, 1.0F)};
        /* scale-pop: overshoot big for the first ~120ms, then settle (ref juice). */
        float pt = clampf(f->age / 0.12F, 0.0F, 1.0F);
        float pop = (pt < 1.0F) ? (1.35F - 0.35F * pt) : 1.0F;
        emit_text_centered(f->text, f->x, f->y, f->size * pop, col);
    }
}

static void compose_text(void) {
    const float gold[4] = {1.0F, 0.82F, 0.28F, 1.0F};
    const float cream[4] = {0.996F, 0.980F, 0.937F, 1.0F};
    const float ice[4] = {0.7F, 0.9F, 1.0F, 1.0F};

    compose_floaters();

    /* Gold counter (sits on the HUD plate, after the coin glyph) -- the primary
     * readout, so it is the biggest HUD number. */
    {
        char g[24];
        char n[16];
        fmt_num(n, sizeof(n), (double)g_game_state.idle_gold);
        (void)snprintf(g, sizeof(g), "Gold %s", n);
        emit_text_soft(g, HUD_TEXT_X, HUD_TOP_Y - 12.0F, 30.0F, gold);
    }
    /* Frost Blocks are the visible rebuild currency. */
    {
        char b[32];
        (void)snprintf(b, sizeof(b), "Blocks %d", g_game_state.idle_frost_blocks);
        emit_text_soft(b, HUD_TEXT_X, HUD_MID_Y - 10.0F, 23.0F, ice);
    }
    /* Keep Rank + stage, so progression and fantasy both read in one glance. */
    {
        char s[40];
        if (g_game_state.idle_boss_active) {
            (void)snprintf(s, sizeof(s), "Keep Rank %d  Stage %d BOSS", g_game_state.idle_keep_rank, g_game_state.idle_stage);
        } else {
            (void)snprintf(s, sizeof(s), "Keep Rank %d  Stage %d", g_game_state.idle_keep_rank, g_game_state.idle_stage);
        }
        emit_text_soft(s, 30.0F, HUD_RANK_Y - 10.0F, 19.0F, cream);
    }
    /* Frost shards (top-right card) -- only when relevant, centered on its card. */
    if (show_shards_hud()) {
        char fs[28];
        (void)snprintf(fs, sizeof(fs), "Frost Shards  %d", g_game_state.idle_frost_shards);
        emit_text_centered_soft(fs, SHARD_HUD_CX, SHARD_HUD_CY - 6.0F, 18.0F, ice);
    }

    /* Stage progress bar label (centered on the bar, inside the plate). */
    {
        char s[28];
        if (g_game_state.idle_boss_active) {
            (void)snprintf(s, sizeof(s), "BOSS  %0.0fs", (double)(g_sim.boss_timer < 0.0F ? 0.0F : g_sim.boss_timer));
        } else {
            (void)snprintf(s, sizeof(s), "%d / %d kills", g_game_state.idle_kills_in_stage, VH_KILLS_PER_STAGE);
        }
        emit_text_centered_soft(s, HUD_PLATE_CX, HUD_BAR_Y - 7.0F, 16.0F, cream);
    }

    /* Keep rebuild rows / compact meta objective. */
    if (show_shard_panel()) {
        emit_text_centered_soft("Frost Keep", KEEP_PANEL_CX, KEEP_PANEL_COMPACT_CY + 26.0F, 20.0F, cream);
        char progress[32];
        (void)snprintf(progress, sizeof(progress), "Rooms %d/%d", keep_rooms_repaired_count(), ROOM_COUNT);
        emit_text_centered_soft(progress, KEEP_PANEL_CX, KEEP_PANEL_COMPACT_CY + 7.0F, 14.0F, ice);
        const int room = next_repair_room();
        char next[42];
        if (room >= 0) {
            (void)snprintf(next, sizeof(next), "Next: %s  %d Blocks", keep_room_name(room), keep_room_cost(room));
        } else {
            (void)snprintf(next, sizeof(next), "Keep rebuilt");
        }
        const float rowc[4] = {0.92F, 0.98F, 1.0F, 1.0F};
        emit_text_centered_soft(next, KEEP_PANEL_CX, KEEP_PANEL_COMPACT_CY - 29.0F, 15.0F, rowc);
    } else {
        emit_text_centered_soft("Frost Keep", KEEP_PANEL_CX, DESIGN_H - 112.0F, 23.0F, cream);
        emit_text_centered_soft("Rebuild rooms", KEEP_PANEL_CX, DESIGN_H - 136.0F, 14.0F, ice);
        for (int i = 0; i < ROOM_COUNT; ++i) {
            float cx, cy, w, h;
            room_row_rect(i, &cx, &cy, &w, &h);
            char line[40];
            if (keep_room_repaired(i)) {
                (void)snprintf(line, sizeof(line), "%s  DONE", keep_room_name(i));
            } else {
                (void)snprintf(line, sizeof(line), "%s  %d Blocks", keep_room_name(i), keep_room_cost(i));
            }
            const float rowc[4] = {0.92F, 0.98F, 1.0F, 1.0F};
            const float subc[4] = {0.70F, 0.90F, 1.0F, 1.0F};
            emit_text_soft(line, cx - w * 0.5F + 16.0F, cy + 3.0F, 17.0F, rowc);
            emit_text_soft(keep_room_effect_label(i), cx - w * 0.5F + 16.0F, cy - 14.0F, 13.0F, subc);
        }
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

    /* ---- Upgrade panel text: 3 clear rows with hierarchy. ----
     *   row 1: NAME (bold, biggest) + "Lv N"
     *   row 2: current effect (smaller, muted)
     *   row 3: COST = coin glyph (drawn in compose_hud) + the number (a price)
     * Affordable -> full-strength cream/gold text; unaffordable -> dimmed. */
    const float dimmed[4] = {0.62F, 0.64F, 0.70F, 1.0F};
    const float mutedc[4] = {0.78F, 0.88F, 0.98F, 1.0F};
    if (show_training_panel()) {
        emit_text_centered_soft("Hero Training", DESIGN_W * 0.5F, 101.0F, 15.0F, ice);
        for (int slot = 0; slot < TRAINING_SLOT_COUNT; ++slot) {
            int i = training_upgrade_at(slot);
            float cx, cy, w, h;
            panel_slot_rect(slot, &cx, &cy, &w, &h);
            bool affordable = (double)g_game_state.idle_gold >= upgrade_cost(i);
            float tx = cx - w * 0.5F + 60.0F;     /* text column, right of the icon */

            /* NAME + level (top, biggest) */
            char lbl[28];
            (void)snprintf(lbl, sizeof(lbl), "%s  Lv%d", upgrade_label(i), upgrade_level(i));
            emit_text_soft(lbl, tx, cy + 12.0F, 20.0F, affordable ? cream : dimmed);

            /* effect (mid, muted) */
            char eff[24];
            upgrade_effect_str(i, eff, sizeof(eff));
            emit_text_soft(eff, tx, cy - 6.0F, 15.0F, affordable ? mutedc : dimmed);

            /* COST on the green pill: dark ink reads on the bright green, light grey
             * on the crushed-dark disabled pill. */
            char n[16];
            fmt_num(n, sizeof(n), upgrade_cost(i));
            float pcx = cx, pcy = cy - h * 0.5F + 20.0F, pw = w - 26.0F;
            const float ink[4] = {0.09F, 0.10F, 0.05F, 1.0F};
            const float offc[4] = {0.74F, 0.76F, 0.80F, 1.0F};
            emit_text(n, pcx - pw * 0.5F + 40.0F, pcy - 9.0F, 19.0F, affordable ? ink : offc);
        }
    } else if (!g_sim.offline_popup && g_game_state.idle_pending_card_choice) {
        emit_text_centered_soft("Choose 1 Rune", DESIGN_W * 0.5F, 229.0F, 22.0F, cream);
        emit_text_centered_soft("Pick a run bonus, then combat resumes", DESIGN_W * 0.5F, 207.0F, 15.0F, ice);
        const bool forge_tier = g_game_state.idle_room_forge_repaired;
        const char *names[CARD_COUNT] = {
            forge_tier ? "Tempered Blade" : "Sharp Edges",
            forge_tier ? "Block Crate" : "Block Cache",
            "Quick Hands"
        };
        char effect0[24];
        char effect1[24];
        (void)snprintf(effect0, sizeof(effect0), "Sword +%d", forge_tier ? 2 : 1);
        (void)snprintf(effect1, sizeof(effect1), "+%d Frost Blocks", forge_tier ? VH_BLOCK_CACHE_FORGE : VH_BLOCK_CACHE_GATE);
        const char *effects[CARD_COUNT] = {effect0, effect1, "+1 Boots level"};
        for (int i = 0; i < CARD_COUNT; ++i) {
            float cx, cy, w, h;
            card_rect(i, &cx, &cy, &w, &h);
            emit_text_centered_soft(names[i], cx, cy - 12.0F, 18.0F, cream);
            emit_text_centered_soft(effects[i], cx, cy - 38.0F, 15.0F, mutedc);
        }
    } else if (!g_sim.offline_popup && next_repair_room() >= 0) {
        float cx, cy, w, h;
        main_repair_btn_rect(&cx, &cy, &w, &h);
        const int room = next_repair_room();
        const bool affordable = g_game_state.idle_frost_blocks >= keep_room_cost(room);
        const float ink[4] = {0.09F, 0.10F, 0.05F, 1.0F};
        const float offc[4] = {0.94F, 0.96F, 0.98F, 1.0F};
        char title[32];
        char need[32];
        (void)snprintf(title, sizeof(title), "Repair %s", keep_room_name(room));
        (void)snprintf(need, sizeof(need), "Need %d Blocks", keep_room_cost(room));
        emit_text_centered_button(affordable ? title : need, cx + 40.0F, cy + 12.0F, 24.0F, affordable ? ink : offc);
        emit_text_centered_button(affordable ? "Tap to rebuild" : "Fight for Blocks", cx + 40.0F, cy - 16.0F,
                                  17.0F, affordable ? ink : offc);
    }

    /* ---- Avalanche Reset button text. ---- */
    if (!g_sim.offline_popup && prestige_unlocked()) {
        float cx, cy, w, h;
        prestige_btn_rect(&cx, &cy, &w, &h);
        char t[40];
        if (g_sim.prestige_armed) {
            (void)snprintf(t, sizeof(t), "CONFIRM AVALANCHE");
        } else {
            (void)snprintf(t, sizeof(t), "Avalanche Reset  +%d shards", frost_shards_reward());
        }
        const float ink[4] = {0.10F, 0.08F, 0.07F, 1.0F};
        emit_text_centered(t, cx, cy - 7.0F, 16.0F, ink);
    }

    /* ---- Frost Blueprint panel: permanent shard spend after Avalanche Reset. ---- */
    if (!g_sim.offline_popup && show_shard_panel()) {
        const float pcx = SHARD_PANEL_CX;
        const float pcy = shard_panel_cy();
        const float ph = shard_panel_h();
        emit_text_centered_soft("Frost Blueprints", pcx, pcy + ph * 0.5F - 32.0F, 19.0F, cream);
        char bank[28];
        (void)snprintf(bank, sizeof(bank), "%d Shards to spend", g_game_state.idle_frost_shards);
        emit_text_centered_soft(bank, pcx, pcy + ph * 0.5F - 52.0F, 14.0F, ice);

        const int rows = shard_panel_visible_rows();
        for (int slot = 0; slot < rows; ++slot) {
            const int which = shard_upgrade_at_visible_slot(slot);
            float cx, cy, w, h;
            shard_row_rect(slot, &cx, &cy, &w, &h);
            bool affordable = (double)g_game_state.idle_frost_shards >= shard_cost(which);
            const float row_main[4] = {0.94F, 0.99F, 1.0F, 1.0F};
            const float row_dim[4] = {0.62F, 0.70F, 0.78F, 1.0F};
            const float row_sub[4] = {0.76F, 0.91F, 1.0F, 1.0F};
            char name[42];
            char cost[18];
            (void)snprintf(name, sizeof(name), "%s  Lv%d", shard_upgrade_name(which), shard_upgrade_level(which));
            (void)snprintf(cost, sizeof(cost), "%.0f", shard_cost(which));
            emit_text_soft(name, cx - w * 0.5F + 48.0F, cy + 5.0F, 16.0F, affordable ? row_main : row_dim);
            emit_text_soft(shard_upgrade_effect(which), cx - w * 0.5F + 48.0F, cy - 12.0F, 12.0F,
                           affordable ? row_sub : row_dim);
            emit_text_centered_soft(cost, cx + w * 0.5F - 25.0F, cy - 7.0F, 16.0F, affordable ? ice : row_dim);
        }
    }

    /* ---- FTUE prompt text. ---- */
    {
        const char *prompt = ftue_prompt();
        if (prompt && !g_game_state.idle_pending_card_choice) {
            emit_text_centered_soft(prompt, DESIGN_W * 0.5F, FTUE_Y, 21.0F, cream);
        }
    }
    if (g_game_state.idle_room_campfire_repaired) {
        emit_text_centered_soft("Helper +25% DMG", HERO_X - 88.0F, HERO_Y + 68.0F, 17.0F, ice);
    }
    if (g_game_state.idle_room_forge_repaired) {
        emit_text_centered_soft("Forge lit", FORGE_MARKER_X, FORGE_MARKER_Y + 50.0F, 15.0F, ice);
    }
    if (g_game_state.idle_room_campfire_repaired) {
        emit_text_centered_soft("Campfire", CAMPFIRE_MARKER_X, CAMPFIRE_MARKER_Y + 44.0F, 15.0F, cream);
    }

    /* ---- Offline popup text. ---- */
    if (g_sim.offline_popup) {
        char t[56];
        char g[16];
        fmt_num(g, sizeof(g), (double)g_sim.offline_gold);
        emit_text_centered("Camp returned", DESIGN_W * 0.5F, DESIGN_H * 0.5F + 68.0F, 27.0F, ice);
        (void)snprintf(t, sizeof(t), "Away for %.1fh", g_sim.offline_hours);
        emit_text_centered_soft(t, DESIGN_W * 0.5F, DESIGN_H * 0.5F + 40.0F, 16.0F, cream);
        (void)snprintf(t, sizeof(t), "+%s Gold", g);
        emit_text_soft(t, DESIGN_W * 0.5F - 166.0F, DESIGN_H * 0.5F - 19.0F, 20.0F, gold);
        (void)snprintf(t, sizeof(t), "+%d Blocks", g_sim.offline_blocks);
        emit_text_soft(t, DESIGN_W * 0.5F + 58.0F, DESIGN_H * 0.5F - 19.0F, 20.0F, ice);
        (void)snprintf(t, sizeof(t), "Camp Supplies bonus x%.2f", shard_offline_mult());
        emit_text_centered_soft(t, DESIGN_W * 0.5F, DESIGN_H * 0.5F - 47.0F, 15.0F, cream);
        float cx, cy, w, h;
        collect_btn_rect(&cx, &cy, &w, &h);
        const float ink[4] = {0.10F, 0.08F, 0.07F, 1.0F};
        emit_text_centered_plain("Collect", cx, cy - 7.0F, 20.0F, ink);
    }

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
    cJSON_AddStringToObject(root, "screen", "frost_keep_rebuilder");
    cJSON_AddBoolToObject(root, "atlas_ready", s_atlas_resolved);

    /* Flat idle mirror for easy probe assertions. */
    cJSON_AddNumberToObject(root, "gold", (double)g_game_state.idle_gold);
    cJSON_AddNumberToObject(root, "stage", g_game_state.idle_stage);
    cJSON_AddNumberToObject(root, "highest_stage", g_game_state.idle_highest_stage);
    cJSON_AddNumberToObject(root, "kills_in_stage", g_game_state.idle_kills_in_stage);
    cJSON_AddNumberToObject(root, "frost_shards", g_game_state.idle_frost_shards);
    cJSON_AddNumberToObject(root, "frost_blocks", g_game_state.idle_frost_blocks);
    cJSON_AddNumberToObject(root, "keep_rank", g_game_state.idle_keep_rank);
    cJSON_AddBoolToObject(root, "room_gate_repaired", g_game_state.idle_room_gate_repaired);
    cJSON_AddBoolToObject(root, "room_forge_repaired", g_game_state.idle_room_forge_repaired);
    cJSON_AddBoolToObject(root, "room_campfire_repaired", g_game_state.idle_room_campfire_repaired);
    cJSON_AddBoolToObject(root, "companion_unlocked", g_game_state.idle_room_campfire_repaired);
    cJSON_AddBoolToObject(root, "pending_card_choice", g_game_state.idle_pending_card_choice);
    cJSON_AddNumberToObject(root, "card_choices_made", g_game_state.idle_card_choices_made);
    cJSON_AddNumberToObject(root, "up_sword", g_game_state.idle_up_sword);
    cJSON_AddNumberToObject(root, "up_boots", g_game_state.idle_up_boots);
    cJSON_AddNumberToObject(root, "up_armor", g_game_state.idle_up_armor);
    cJSON_AddNumberToObject(root, "up_luck", g_game_state.idle_up_luck);
    cJSON_AddNumberToObject(root, "shard_damage_level", g_game_state.idle_shard_global_damage);
    cJSON_AddNumberToObject(root, "shard_gold_level", g_game_state.idle_shard_global_gold);
    cJSON_AddNumberToObject(root, "shard_start_level", g_game_state.idle_shard_start_stage);
    cJSON_AddNumberToObject(root, "shard_offline_level", g_game_state.idle_shard_offline_rate);
    cJSON_AddBoolToObject(root, "boss_active", g_game_state.idle_boss_active);
    cJSON_AddBoolToObject(root, "offline_unlocked", g_game_state.idle_offline_unlocked);
    cJSON_AddBoolToObject(root, "prestige_unlocked", prestige_unlocked());
    cJSON_AddBoolToObject(root, "avalanche_unlocked", prestige_unlocked());
    cJSON_AddNumberToObject(root, "ftue_step", g_game_state.run_ftue_step);

    /* Derived combat stats (so the probe can assert upgrades change damage). */
    cJSON_AddNumberToObject(root, "hero_damage", hero_damage());
    cJSON_AddNumberToObject(root, "hero_attack_interval", hero_attack_interval());
    cJSON_AddNumberToObject(root, "hero_max_hp", hero_max_hp());
    cJSON_AddNumberToObject(root, "gold_find_mult", gold_find_mult());
    cJSON_AddNumberToObject(root, "campfire_damage_mult", VH_CAMPFIRE_DAMAGE_MULT);

    /* Costs / rewards (so the probe can drive deterministically). */
    cJSON_AddNumberToObject(root, "cost_sword", upgrade_cost(UP_SWORD));
    cJSON_AddNumberToObject(root, "cost_boots", upgrade_cost(UP_BOOTS));
    cJSON_AddNumberToObject(root, "cost_armor", upgrade_cost(UP_ARMOR));
    cJSON_AddNumberToObject(root, "cost_luck", upgrade_cost(UP_LUCK));
    cJSON_AddNumberToObject(root, "cost_gate_blocks", VH_GATE_BLOCK_COST);
    cJSON_AddNumberToObject(root, "cost_forge_blocks", VH_FORGE_BLOCK_COST);
    cJSON_AddNumberToObject(root, "cost_campfire_blocks", VH_CAMPFIRE_BLOCK_COST);
    cJSON_AddNumberToObject(root, "shards_reward_now", frost_shards_reward());
    cJSON_AddNumberToObject(root, "avalanche_shards_reward", frost_shards_reward());
    cJSON_AddNumberToObject(root, "cost_shard_damage", shard_cost(SH_DMG));
    cJSON_AddNumberToObject(root, "cost_shard_gold", shard_cost(SH_GOLD));
    cJSON_AddNumberToObject(root, "cost_shard_start", shard_cost(SH_START));
    cJSON_AddNumberToObject(root, "cost_shard_offline", shard_cost(SH_OFFLINE));

    cJSON_AddBoolToObject(root, "offline_popup", g_sim.offline_popup);
    cJSON_AddNumberToObject(root, "offline_gold", (double)g_sim.offline_gold);
    cJSON_AddNumberToObject(root, "offline_blocks", g_sim.offline_blocks);
    cJSON_AddNumberToObject(root, "offline_hours", g_sim.offline_hours);
    cJSON_AddNumberToObject(root, "monsters_alive", count_monsters());

    cJSON *reward = cJSON_CreateObject();
    cJSON_AddNumberToObject(reward, "keep", (double)g_sim.reward_keep_pulse);
    cJSON_AddNumberToObject(reward, "card", (double)g_sim.reward_card_pulse);
    cJSON_AddNumberToObject(reward, "shard", (double)g_sim.reward_shard_pulse);
    cJSON_AddNumberToObject(reward, "avalanche", (double)g_sim.reward_avalanche_pulse);
    cJSON_AddNumberToObject(reward, "offline", (double)g_sim.reward_offline_pulse);
    cJSON_AddItemToObject(root, "reward_pulses", reward);

    GameAudioStatus audio = game_audio_status();
    cJSON *audio_json = cJSON_CreateObject();
    cJSON_AddBoolToObject(audio_json, "implemented", audio.implemented);
    cJSON_AddBoolToObject(audio_json, "initialized", audio.initialized);
    cJSON_AddBoolToObject(audio_json, "device_enabled", audio.device_enabled);
    cJSON_AddStringToObject(audio_json, "backend", audio.backend ? audio.backend : "unknown");
    cJSON_AddNumberToObject(audio_json, "total_play_count", audio.total_play_count);
    cJSON_AddNumberToObject(audio_json, "click", audio.cue_play_count[GAME_AUDIO_CUE_CLICK]);
    cJSON_AddNumberToObject(audio_json, "success", audio.cue_play_count[GAME_AUDIO_CUE_SUCCESS]);
    cJSON_AddNumberToObject(audio_json, "notify", audio.cue_play_count[GAME_AUDIO_CUE_NOTIFY]);
    cJSON_AddNumberToObject(audio_json, "error", audio.cue_play_count[GAME_AUDIO_CUE_ERROR]);
    cJSON_AddItemToObject(root, "audio", audio_json);
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

/* Choose a rune card by name/index: params {card:"sharp"|"blocks"|"quick"} or {card:0}. */
static bool ep_game_debug_choose_card(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    int card = -1;
    if (params) {
        const cJSON *c = cJSON_GetObjectItemCaseSensitive(params, "card");
        if (cJSON_IsNumber(c)) {
            card = c->valueint;
        } else if (cJSON_IsString(c)) {
            if (strcmp(c->valuestring, "sharp") == 0) card = CARD_SHARP;
            else if (strcmp(c->valuestring, "blocks") == 0) card = CARD_BLOCK_CACHE;
            else if (strcmp(c->valuestring, "quick") == 0) card = CARD_QUICK_HANDS;
        }
    }
    bool picked = choose_card(card);
    if (!picked && error && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "no pending card or unknown card");
    }
    cJSON *root = state_json();
    cJSON_AddBoolToObject(root, "picked", picked);
    *result = root;
    return picked;
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
        if (error && error_cap > 0) {
            (void)snprintf(error, (size_t)error_cap, "avalanche reset locked (Keep Rank < %d)", VH_AVALANCHE_KEEP_RANK_UNLOCK);
        }
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
    long granted_gold = 0;
    int granted_blocks = 0;
    bool granted = compute_offline_grant(now, &granted_gold, &granted_blocks);
    g_sim.offline_gold = granted_gold;
    g_sim.offline_blocks = granted_blocks;
    g_sim.offline_popup = granted;
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
    nt_devapi_register("game.debug.choose_card", ep_game_debug_choose_card, NULL);
    nt_devapi_register("game.debug.buy_shard", ep_game_debug_buy_shard, NULL);
    nt_devapi_register("game.debug.prestige", ep_game_debug_prestige, NULL);
    nt_devapi_register("game.debug.offline", ep_game_debug_offline, NULL);
    nt_devapi_register("frame.screenshot", ep_frame_screenshot, NULL);
}

static void register_ui_devapi(float w, float h) {
    nt_devapi_set_frame(g_nt_app.frame);
    nt_devapi_set_view((float)canvas_w(), (float)canvas_h(), w, h);
    nt_devapi_clear_ui_elements();
    (void)nt_devapi_register_ui_node("root", "", "screen", "Voxelheim", "Frost Keep Rebuilder", 0.0F, 0.0F, w, h, true, true);
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
    game_audio_update();

    nt_resource_step();
    nt_material_step();
    resolve_regions();

    if (s_atlas_resolved && !g_sim.started) {
        sim_reset();
        /* compute the offline grant the first time the sim comes up */
        if (g_game_state.idle_offline_unlocked && !s_fresh_state) {
            long now = now_unix();
            long granted_gold = 0;
            int granted_blocks = 0;
            if (compute_offline_grant(now, &granted_gold, &granted_blocks)) {
                g_sim.offline_gold = granted_gold;
                g_sim.offline_blocks = granted_blocks;
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
    float wb_l, wb_r, wb_b, wb_t;
    world_bounds(&wb_l, &wb_r, &wb_b, &wb_t);
    glm_ortho(wb_l, wb_r, wb_b, wb_t, -1.0F, 1.0F, proj);
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

    g_nt_window.title = "Voxelheim - Frost Keep Rebuilder";
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
    game_audio_init();
    game_audio_set_volume(0.72F, 0.82F);

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
    game_audio_shutdown();
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
