#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "game_audio.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "material/nt_material.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_shape_renderer.h"
#include "renderers/nt_sprite_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "window/nt_window.h"

#include "critter_corral_assets.h"

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
#include <glad/gl.h>
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLEAN_SEED_DEVAPI_PORT_DEFAULT 9123

/* ---- Critter Corral: first playable core moment (sprite-rendered) ---- */

#define CORRAL_MAX_CRITTERS 64
#define CORRAL_MAX_COLORS 5
#define CORRAL_PEN_COUNT CORRAL_MAX_COLORS /* one pen per active color */
#define CORRAL_MAX_PARTICLES 256

/* ---- Run structure (waves + progression around the core moment) ----
 * Calm identity: escalation without harsh/unfair fail. Wave 1 is tiny + slow
 * (also serves as FTUE). Each wave adds critters; colors ramp 2 -> 5 over the
 * first waves; later waves wander a touch faster/odder. A soft WIN milestone
 * fires after clearing wave WIN_WAVE, then play continues ENDLESS. */
#define CORRAL_WIN_WAVE 10
#define CORRAL_WAVE_CLEARED_TIME 1.0F /* brief celebratory beat (~1s) */
#define CORRAL_WIN_BEAT_TIME 2.2F     /* "you did it!" beat before endless */

#define CORRAL_PACK_PATH "assets/runtime/critter-corral/critter_corral.ntpack"

/* Up to five bold, clearly-distinct critter hues (DIRECTION: bright, friendly).
 * Drawn on a NEUTRAL tintable critter sprite (corral/critter.png) so emit color
 * reproduces each hue cleanly; also used for the pen wash, flag, gate glow, and
 * particles. Order chosen so the first two (red, blue) match the original slice
 * and each added hue stays maximally distinct from its neighbours. */
static const float CORRAL_COLORS[CORRAL_MAX_COLORS][4] = {
    {0.98F, 0.34F, 0.24F, 1.0F}, /* 0 warm red */
    {0.22F, 0.56F, 1.0F, 1.0F},  /* 1 cool blue */
    {0.36F, 0.82F, 0.34F, 1.0F}, /* 2 fresh green */
    {1.0F, 0.80F, 0.18F, 1.0F},  /* 3 sunny gold */
    {0.70F, 0.40F, 0.95F, 1.0F}, /* 4 soft purple */
};

/* Plain color names, index-aligned with CORRAL_COLORS — used in the FTUE/level
 * text ("Bring red critters into the red pen", "New color: gold!"). */
static const char *CORRAL_COLOR_NAMES[CORRAL_MAX_COLORS] = {
    "red", "blue", "green", "gold", "purple",
};

typedef enum {
  CORRAL_PHASE_TITLE = 0,     /* start beat — press/click to start */
  CORRAL_PHASE_PLAYING,       /* herding a wave */
  CORRAL_PHASE_WAVE_CLEARED,  /* brief celebratory beat, then next wave */
  CORRAL_PHASE_UPGRADE_CHOICE,/* LIGHT META: pick 1-of-3 between waves */
  CORRAL_PHASE_WIN,           /* soft "you did it!" milestone, then endless */
} corral_phase_t;

/* ---- LIGHT META: between-wave upgrade pick (adds AGENCY + "one more wave") ----
 * Identity = CALM: every upgrade is a clear player-POSITIVE power-fantasy, never
 * a tradeoff or a shop. On WAVE_CLEARED we offer a pick-1-of-3 (fontless icon
 * tiles); the chosen upgrade is a PERSISTENT run modifier that visibly changes
 * play (e.g. a bigger lure ring). Levels stack up to a small cap so a maxed
 * upgrade stops being offered; the acquired build shows as a fontless HUD row.
 *   RADIUS  — lure radius +            (ring icon)
 *   PULL    — lure pull strength +     (arrow icon)
 *   SECOND  — a trailing 2nd lure point(two-dots icon)
 *   GATE    — wider pen gates (easier) (gate icon)
 *   CALM    — calmer critters (less special-behavior intensity / wander)
 *   CHAIN   — longer/stronger chain    (chain icon) */
typedef enum {
  CORRAL_UPG_RADIUS = 0,
  CORRAL_UPG_PULL,
  CORRAL_UPG_SECOND_LURE,
  CORRAL_UPG_GATE,
  CORRAL_UPG_CALM,
  CORRAL_UPG_CHAIN,
  CORRAL_UPG_COUNT,
} corral_upgrade_t;

#define CORRAL_UPG_MAX_LEVEL 3 /* small cap per upgrade (stacks, then retires) */
#define CORRAL_UPGRADE_OFFER 3 /* always a pick-1-of-3 */

/* ---- Critter behavior variety (the depth that makes herding a decision) ----
 * Each type forces a DIFFERENT herding read without precision/twitch demands:
 *   NORMAL   — herds toward the lure (the teachable default; early waves).
 *   SKITTISH — briefly FLEES when the lure gets very close, then can be herded;
 *              rewards approaching gently (DON'T crowd it). Tell: lighter,
 *              smaller, jittery micro-motion + wide darting eyes.
 *   STUBBORN — steers toward the lure only slowly (high inertia); needs
 *              SUSTAINED luring (commit longer). Tell: bigger, darker outline,
 *              slow blink, slower.
 *   FOLLOWER — weakly attracted to OTHER critters (clings to clumps, even other
 *              colors), so it drifts with the crowd and resists being separated
 *              (BREAK UP clumps). Tell: a soft link cue toward its nearest
 *              neighbour. */
typedef enum {
  CORRAL_BEHAVIOR_NORMAL = 0,
  CORRAL_BEHAVIOR_SKITTISH,
  CORRAL_BEHAVIOR_STUBBORN,
  CORRAL_BEHAVIOR_FOLLOWER,
  CORRAL_BEHAVIOR_COUNT,
} corral_behavior_t;

typedef struct Critter {
  float x, y;
  float vx, vy;
  float wander;          /* current wander heading (radians) */
  float squash;          /* capture squash timer (seconds remaining) */
  float flee;            /* SKITTISH: flee-burst timer (seconds remaining) */
  float jitter;          /* SKITTISH: free-running phase for nervous shimmer */
  float blink;           /* STUBBORN: slow blink phase (free-running) */
  float link_x, link_y;  /* FOLLOWER: smoothed nearest-neighbour point (tell) */
  bool has_link;         /* FOLLOWER: a neighbour was found this frame */
  uint8_t color;         /* index into CORRAL_COLORS */
  uint8_t behavior;      /* corral_behavior_t */
  bool alive;            /* still loose on the field */
  bool parked;           /* captured and resting in a pen */
  int8_t parked_pen;
} Critter;

typedef struct Pen {
  float x, y, w, h; /* top-left rect */
  uint8_t color;
  float gx, gy;   /* gate mouth point (open face, toward the field) */
  float gdx, gdy; /* inward gate normal (unit, points into the field) */
  float flash;    /* capture flash timer */
  float chain;    /* chain-boost timer (signature cascade) */
  int chain_steps;/* captures during the CURRENT chain (drives rising pitch +
                   * escalating burst — the signature satisfaction) */
  int parked;     /* count parked in this pen */
} Pen;

typedef struct Particle {
  float x, y;
  float vx, vy;
  float life; /* seconds remaining */
  float max_life;
  const float *color;
} Particle;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = CLEAN_SEED_DEVAPI_PORT_DEFAULT;
static int s_window_width = 960;
static int s_window_height = 540;

static Critter s_critters[CORRAL_MAX_CRITTERS];
static int s_critter_count;
static Pen s_pens[CORRAL_PEN_COUNT];
static int s_color_count = 2; /* active colors this wave (ramps 2 -> 5) */
static Particle s_particles[CORRAL_MAX_PARTICLES];
static int s_score; /* total captured across all waves */
static int s_wave = 1;
static corral_phase_t s_phase = CORRAL_PHASE_TITLE;
static float s_phase_timer;   /* counts down the celebratory / win beats */
static float s_cleared_flash; /* wave-clear celebration pulse */
static bool s_win_shown;      /* the soft WIN milestone fired once this run */
static float s_ftue_hint;     /* wave-1 lure pulse hint (seconds remaining) */
static uint32_t s_rng = 0x1234abcdU;

/* ---- FTUE / tutorial (first run only, <=3 beats, tutorial-by-doing) ----
 * Beat 0: "Move your mouse to herd the critters" (advances once the player
 *         moves the lure a meaningful distance).
 * Beat 1: "Bring [red] critters into the [red] pen" (advances on first capture).
 * Beat 2: "Pen them all to finish the level!" (advances when nearly cleared).
 * s_ftue_active is set true once on the very first run and never again (a flag),
 * so onboarding shows on the first run only — calm, brief, no modal spam. */
static bool s_ftue_active;        /* the first-run tutorial is currently showing */
static bool s_ftue_seen;          /* the tutorial has run once -> never show again */
static int s_ftue_beat;           /* current beat 0..2 */
static float s_ftue_beat_age;     /* seconds the current beat has been on screen */
static float s_ftue_move_accum;   /* lure travel accumulated for beat-0 advance */
static float s_ftue_last_lure_x;  /* previous lure pos (for movement detection) */
static float s_ftue_last_lure_y;

/* ---- NEW-COLOR callout (difficulty legibility) ----
 * When a wave first introduces a color the player hasn't seen, a brief text
 * banner ("New color: green!") fires so rising difficulty is visible, not silent.
 */
static int s_seen_colors;         /* highest color_count reached so far this run */
static float s_new_color_timer;   /* seconds remaining on the "New color!" banner */
static int s_new_color_index;     /* which color was just introduced */

/* Lure (the one action): follows the mouse. */
static float s_lure_x;
static float s_lure_y;
static float s_lure_radius = 150.0F;
static bool s_lure_active;

/* ---- LIGHT META state ---- */
static uint8_t s_upgrades[CORRAL_UPG_COUNT]; /* acquired level per upgrade */
static corral_upgrade_t s_offer[CORRAL_UPGRADE_OFFER]; /* the 3 offered cards */
static int s_offer_count;          /* valid entries in s_offer (usually 3) */
static int s_acquired_total;       /* total upgrade picks across the run */
/* trailing second lure point: smoothed lag behind the cursor (SECOND_LURE). */
static float s_lure2_x;
static float s_lure2_y;

/* ---- Sprite-render plumbing ---- */

static nt_buffer_t s_frame_ubo;
static nt_hash32_t s_pack_id;
static nt_resource_t s_atlas_handle;
static nt_resource_t s_vs_handle;
static nt_resource_t s_fs_handle;
static nt_material_t s_sprite_material;

/* ---- Text-render plumbing (slug_text material + a UI font) ----
 * Text is the keystone of this increment: it makes upgrades, the FTUE, and the
 * level/difficulty read in plain language. Shares the sprite renderer's Y-down
 * Globals VP (slot 0), so a pixel (x,y) in game space == a pixel on screen. */
static nt_resource_t s_text_vs_handle;
static nt_resource_t s_text_fs_handle;
static nt_material_t s_text_material;
static nt_font_t s_font;
static bool s_text_ready; /* font loaded + material ready this frame */

/* Text alignment for draw_text(). */
typedef enum {
  TEXT_ALIGN_LEFT = 0,
  TEXT_ALIGN_CENTER,
  TEXT_ALIGN_RIGHT,
} text_align_t;

/* Region indices resolved once the atlas is READY. */
typedef enum {
  CORRAL_RGN_CRITTER = 0, /* neutral tintable critter (one shape, N hues) */
  CORRAL_RGN_CRITTER_A,
  CORRAL_RGN_CRITTER_B,
  CORRAL_RGN_PEN,
  CORRAL_RGN_FLAG,
  CORRAL_RGN_GRASS,
  CORRAL_RGN_LURE,
  CORRAL_RGN_SPARK,
  CORRAL_RGN_PIP,
  CORRAL_RGN_CARD,        /* upgrade-choice card backdrop */
  CORRAL_RGN_ICON_RADIUS, /* upgrade icons (order matches corral_upgrade_t) */
  CORRAL_RGN_ICON_PULL,
  CORRAL_RGN_ICON_SECOND,
  CORRAL_RGN_ICON_GATE,
  CORRAL_RGN_ICON_CALM,
  CORRAL_RGN_ICON_CHAIN,
  CORRAL_RGN_COUNT,
} corral_region_t;

/* Map an upgrade id to its icon region (icons laid out in upgrade order). */
static corral_region_t upgrade_icon_region(corral_upgrade_t u) {
  return (corral_region_t)(CORRAL_RGN_ICON_RADIUS + (int)u);
}

/* Plain-language upgrade copy (what it is) — shown as the card TITLE. */
static const char *upgrade_title(corral_upgrade_t u) {
  switch (u) {
  case CORRAL_UPG_RADIUS:
    return "Lure Radius";
  case CORRAL_UPG_PULL:
    return "Lure Pull";
  case CORRAL_UPG_SECOND_LURE:
    return "Second Lure";
  case CORRAL_UPG_GATE:
    return "Wider Gates";
  case CORRAL_UPG_CALM:
    return "Calmer Critters";
  case CORRAL_UPG_CHAIN:
    return "Longer Chain";
  case CORRAL_UPG_COUNT:
    break;
  }
  return "Upgrade";
}

/* Compact one-word label for the tight acquired-build HUD row (full titles
 * collide at the icon pitch; the card uses the full upgrade_title()). */
static const char *upgrade_short(corral_upgrade_t u) {
  switch (u) {
  case CORRAL_UPG_RADIUS:
    return "Radius";
  case CORRAL_UPG_PULL:
    return "Pull";
  case CORRAL_UPG_SECOND_LURE:
    return "2nd Lure";
  case CORRAL_UPG_GATE:
    return "Gates";
  case CORRAL_UPG_CALM:
    return "Calm";
  case CORRAL_UPG_CHAIN:
    return "Chain";
  case CORRAL_UPG_COUNT:
    break;
  }
  return "";
}

/* One-line "why it helps" — shown as the card DESCRIPTION (kept short so it
 * wraps cleanly on the card). */
static const char *upgrade_desc(corral_upgrade_t u) {
  switch (u) {
  case CORRAL_UPG_RADIUS:
    return "Herd critters\nfrom farther away";
  case CORRAL_UPG_PULL:
    return "Critters come\nto you faster";
  case CORRAL_UPG_SECOND_LURE:
    return "A second\nherding point";
  case CORRAL_UPG_GATE:
    return "Pens catch\nmore easily";
  case CORRAL_UPG_CALM:
    return "Critters wander\nless wildly";
  case CORRAL_UPG_CHAIN:
    return "Friends follow\nin for longer";
  case CORRAL_UPG_COUNT:
    break;
  }
  return "";
}

static uint32_t s_region_idx[CORRAL_RGN_COUNT];
static uint16_t s_region_w[CORRAL_RGN_COUNT];
static uint16_t s_region_h[CORRAL_RGN_COUNT];
static bool s_atlas_resolved;
static bool s_sprites_ready; /* atlas + material both usable this frame */

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static bool s_capture_pending;
static char s_capture_path[512];
#endif

/* ---- Small math / rng helpers ---- */

static float frand(void) {
  /* xorshift32 -> [0,1) so reset_playtest is deterministic for screenshots. */
  s_rng ^= s_rng << 13;
  s_rng ^= s_rng >> 17;
  s_rng ^= s_rng << 5;
  return (float)(s_rng & 0x00ffffffU) / (float)0x01000000U;
}

static float frand_range(float lo, float hi) {
  return lo + (hi - lo) * frand();
}

static float clampf(float v, float lo, float hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

/* Y-down ortho (top-left origin) so all game logic coordinates — which track
 * top-down pointer pixels and lay pens out with y growing downward — map 1:1
 * onto the sprite renderer's Globals VP without touching any sim math. */
static void ortho_ydown(float w, float h, float out[16]) {
  memset(out, 0, sizeof(float) * 16);
  out[0] = 2.0F / w;
  out[5] = -2.0F / h; /* flip Y: top-left origin */
  out[10] = -1.0F;    /* near=-1,far=1 -> -2/(far-near) = -1 */
  out[12] = -1.0F;
  out[13] = 1.0F;
  out[15] = 1.0F;
}

/* ---- Sprite emit helpers ---- */

static uint32_t pack_rgba(const float c[4]) {
  /* 0xAABBGGRR; renderer/shader premultiply by alpha. */
  uint32_t r = (uint32_t)clampf(c[0] * 255.0F + 0.5F, 0.0F, 255.0F);
  uint32_t g = (uint32_t)clampf(c[1] * 255.0F + 0.5F, 0.0F, 255.0F);
  uint32_t b = (uint32_t)clampf(c[2] * 255.0F + 0.5F, 0.0F, 255.0F);
  uint32_t a = (uint32_t)clampf(c[3] * 255.0F + 0.5F, 0.0F, 255.0F);
  return (a << 24) | (b << 16) | (g << 8) | r;
}

static uint32_t pack_white_alpha(float alpha) {
  uint32_t a = (uint32_t)clampf(alpha * 255.0F + 0.5F, 0.0F, 255.0F);
  return (a << 24) | 0x00FFFFFFU;
}

/* Build a column-major mat4 with independent X/Y scale + translation. The
 * sprite renderer reads columns 0/1 (scale/rot) and m[12/13] (translate). */
static void sprite_mat(float cx, float cy, float sx, float sy, float out[16]) {
  memset(out, 0, sizeof(float) * 16);
  out[0] = sx;
  out[5] = sy;
  out[10] = 1.0F;
  out[12] = cx;
  out[13] = cy;
  out[15] = 1.0F;
}

/* Emit one centred sprite at (cx,cy) scaled so its source covers (dst_w,dst_h)
 * pixels, tinted by color_packed. Region is centre-pivot in the atlas. A
 * negative dst_w mirrors the sprite horizontally about its centre. */
static void emit_sprite(corral_region_t rgn, float cx, float cy, float dst_w,
                        float dst_h, uint32_t color_packed) {
  uint16_t sw = s_region_w[rgn];
  uint16_t sh = s_region_h[rgn];
  if (sw == 0 || sh == 0) {
    return;
  }
  float m[16];
  sprite_mat(cx, cy, dst_w / (float)sw, dst_h / (float)sh, m);
  nt_sprite_renderer_emit_region(s_atlas_handle, s_region_idx[rgn], m, 0.5F,
                                 0.5F, color_packed, 0);
}

/* ---- Text emit helpers ----
 * The text renderer's glyph space is Y-up; our screen ortho is Y-down. Negating
 * column 1 of the model matrix flips glyphs upright. `size` is the em height in
 * pixels (1 world unit == 1 framebuffer pixel here). */

/* Measured width of a single-line string in pixels (0 if the font isn't ready). */
static float text_width(const char *s, float size) {
  if (!s_text_ready || s == NULL || s[0] == '\0') {
    return 0.0F;
  }
  nt_text_size_t ts = nt_font_measure(s_font, s, size, 0.0F);
  return ts.width;
}

/* Draw a single-line UTF-8 string. (x,y) is the TOP-LEFT for LEFT align (top-
 * center / top-right for the others); the baseline is derived from font ascent
 * so callers position by the visual top edge. No-op until the font is ready. */
static void draw_text(const char *s, float x, float y, float size,
                      const float color[4], text_align_t align) {
  if (!s_text_ready || s == NULL || s[0] == '\0') {
    return;
  }
  nt_font_metrics_t fm = nt_font_get_metrics(s_font);
  if (fm.units_per_em == 0) {
    return; /* font resource not resolved yet */
  }
  float ox = x;
  if (align != TEXT_ALIGN_LEFT) {
    float wpx = text_width(s, size);
    ox = (align == TEXT_ALIGN_CENTER) ? x - wpx * 0.5F : x - wpx;
  }
  float baseline = y + (float)fm.ascent * (size / (float)fm.units_per_em);
  float m[16];
  memset(m, 0, sizeof(m));
  m[0] = 1.0F;
  m[5] = -1.0F; /* flip glyph Y-up -> screen Y-down */
  m[10] = 1.0F;
  m[12] = ox;
  m[13] = baseline;
  m[15] = 1.0F;
  nt_text_renderer_draw(s, m, size, color, 0.0F, 0.0F);
}

/* Convenience: a soft dark drop-shadow behind a string so light HUD text stays
 * legible over any field color, then the string itself. */
static void draw_text_shadow(const char *s, float x, float y, float size,
                             const float color[4], text_align_t align) {
  const float shadow[4] = {0.04F, 0.05F, 0.06F, 0.85F * color[3]};
  draw_text(s, x + 1.5F, y + 1.5F, size, shadow, align);
  draw_text(s, x, y, size, color, align);
}

/* Like draw_text_shadow but shrinks `size` (down to a floor) so the string fits
 * within `max_w` pixels — keeps long copy on-screen on a narrow/portrait
 * screen without manual per-string tuning. Returns the size actually used. */
static float draw_text_shadow_fit(const char *s, float x, float y, float size,
                                  float max_w, const float color[4],
                                  text_align_t align) {
  if (s_text_ready && s != NULL && s[0] != '\0' && max_w > 0.0F) {
    float wpx = text_width(s, size);
    if (wpx > max_w) {
      float scaled = size * (max_w / wpx);
      size = (scaled < 11.0F) ? 11.0F : scaled; /* floor for legibility */
    }
  }
  draw_text_shadow(s, x, y, size, color, align);
  return size;
}

/* ---- Progression curve (wave -> difficulty) ----
 * Calm escalation: wave 1 is tiny + slow (FTUE), then more critters, more
 * colors (2 -> 5 over the first waves), and a gentle wander speed-up later. */

static int wave_color_count(int wave) {
  /* start 2 colors, add +1 every other wave up to CORRAL_MAX_COLORS. */
  int n = 2 + (wave - 1) / 2;
  if (n > CORRAL_MAX_COLORS) {
    n = CORRAL_MAX_COLORS;
  }
  if (n < 2) {
    n = 2;
  }
  return n;
}

static int wave_critter_count(int wave) {
  /* wave 1 tiny (4) for FTUE, then grow ~3 per wave, clamped to the field cap.
   */
  int n = 4 + (wave - 1) * 3;
  if (n > CORRAL_MAX_CRITTERS) {
    n = CORRAL_MAX_CRITTERS;
  }
  return n;
}

/* Gentle late pressure only: wander gets slightly faster/odder on later waves
 * but stays calm (no fail). Returns a [1.0 .. ~1.6] multiplier. */
static float wave_wander_scale(int wave) {
  float s = 1.0F + 0.06F * (float)(wave - 1);
  return clampf(s, 1.0F, 1.6F);
}

/* ---- Progressive behavior introduction (calm escalation via VARIETY) ----
 * Identity = CALM: escalate by adding a new herding READ, never by unfair
 * difficulty. Behaviors unlock one at a time over successive waves and each
 * stays a capped MINORITY so most critters remain the teachable default. The
 * proportion is the difficulty knob — bounded so the field never turns hostile.
 *
 * Unlock schedule (when a behavior may appear at all):
 *   waves 1-2  : NORMAL only            (FTUE + first waves stay teachable)
 *   wave  3+   : + SKITTISH
 *   wave  5+   : + STUBBORN
 *   wave  7+   : + FOLLOWER
 * Caps grow slowly with the wave but each behavior is held to a fair share, so
 * a wave is, e.g., "mostly normal with a few skittish", never "all skittish". */
static bool behavior_unlocked(int wave, corral_behavior_t b) {
  switch (b) {
  case CORRAL_BEHAVIOR_SKITTISH:
    return wave >= 3;
  case CORRAL_BEHAVIOR_STUBBORN:
    return wave >= 5;
  case CORRAL_BEHAVIOR_FOLLOWER:
    return wave >= 7;
  case CORRAL_BEHAVIOR_NORMAL:
  case CORRAL_BEHAVIOR_COUNT:
    break;
  }
  return true; /* NORMAL is always available */
}

/* Fraction (0..~0.4) of the wave allowed to be a given non-normal behavior.
 * Ramps in gently from its unlock wave and is capped so it stays a minority. */
static float behavior_cap_fraction(int wave, corral_behavior_t b) {
  int unlock;
  switch (b) {
  case CORRAL_BEHAVIOR_SKITTISH:
    unlock = 3;
    break;
  case CORRAL_BEHAVIOR_STUBBORN:
    unlock = 5;
    break;
  case CORRAL_BEHAVIOR_FOLLOWER:
    unlock = 7;
    break;
  default:
    return 0.0F;
  }
  if (wave < unlock) {
    return 0.0F;
  }
  /* start ~0.12 at unlock, +0.04 per wave after, capped at 0.34 so the sum of
   * non-normal behaviors stays well under half the field (calm, not frantic). */
  float f = 0.12F + 0.04F * (float)(wave - unlock);
  return clampf(f, 0.0F, 0.34F);
}

/* Assign a behavior for critter slot `idx` of `count` this wave. The first
 * critters of each color are NORMAL (so every color is always herdable the
 * teachable way); the capped tail of the wave is sprinkled with unlocked
 * behaviors. Deterministic via frand() so screenshots/playtests reproduce. */
static corral_behavior_t pick_behavior(int wave, int idx, int count) {
  /* keep the leading chunk of the wave fully normal for readability/FTUE. */
  int normal_floor = count / 2; /* >= half the field is always plain NORMAL */
  if (idx < normal_floor) {
    return CORRAL_BEHAVIOR_NORMAL;
  }
  /* roll the unlocked behaviors against their caps, in unlock order. */
  static const corral_behavior_t order[3] = {CORRAL_BEHAVIOR_SKITTISH,
                                             CORRAL_BEHAVIOR_STUBBORN,
                                             CORRAL_BEHAVIOR_FOLLOWER};
  float r = frand();
  float acc = 0.0F;
  for (int k = 0; k < 3; ++k) {
    corral_behavior_t b = order[k];
    if (!behavior_unlocked(wave, b)) {
      continue;
    }
    acc += behavior_cap_fraction(wave, b);
    if (r < acc) {
      return b;
    }
  }
  return CORRAL_BEHAVIOR_NORMAL;
}

#if NT_DEVAPI_ENABLED
/* Only consumed by the DevAPI state report (compiled out on web/release). */
static int corral_behavior_loose_count(corral_behavior_t b);
#endif
static const nt_pointer_t *primary_pointer(void);
static void restart_marker_rect(float w, float h, float *rx, float *ry,
                                float *rw, float *rh);

/* ---- LIGHT META: derived run modifiers from acquired upgrade levels ----
 * Each is a clear, readable, player-POSITIVE buff that scales with level so the
 * build is felt in play. Kept gentle so the field stays calm, never trivial. */

/* Effective lure radius (base 150 + RADIUS level). Visible: the ring grows. */
static float corral_effective_lure_radius(void) {
  return 150.0F + 46.0F * (float)s_upgrades[CORRAL_UPG_RADIUS];
}

/* Lure pull multiplier (PULL level): stronger steering toward the lure. */
static float corral_pull_mult(void) {
  return 1.0F + 0.28F * (float)s_upgrades[CORRAL_UPG_PULL];
}

/* Wider pen gate: extra capture padding on the mouth (GATE level). */
static float corral_gate_bonus(void) {
  return 18.0F * (float)s_upgrades[CORRAL_UPG_GATE];
}

/* Calmer critters: 0..~0.66 dampening of special-behavior intensity + wander
 * (CALM level). 1.0 = full intensity, lower = calmer. */
static float corral_calm_mult(void) {
  return clampf(1.0F - 0.22F * (float)s_upgrades[CORRAL_UPG_CALM], 0.34F, 1.0F);
}

/* Chain duration (base 0.85s + CHAIN level): cascade lasts longer / pulls more. */
static float corral_chain_time(void) {
  return 0.85F + 0.55F * (float)s_upgrades[CORRAL_UPG_CHAIN];
}

static bool corral_second_lure_active(void) {
  return s_upgrades[CORRAL_UPG_SECOND_LURE] > 0;
}

/* An upgrade can still be offered while below its cap. */
static bool corral_upgrade_available(corral_upgrade_t u) {
  return s_upgrades[u] < CORRAL_UPG_MAX_LEVEL;
}

/* Build the pick-1-of-3 offer: choose CORRAL_UPGRADE_OFFER distinct, non-maxed
 * upgrades at random (deterministic via frand for reproducible screenshots).
 * If fewer than 3 remain unmaxed, offer as many as are available. */
static void corral_make_offer(void) {
  corral_upgrade_t pool[CORRAL_UPG_COUNT];
  int n = 0;
  for (int u = 0; u < CORRAL_UPG_COUNT; ++u) {
    if (corral_upgrade_available((corral_upgrade_t)u)) {
      pool[n++] = (corral_upgrade_t)u;
    }
  }
  /* Fisher-Yates partial shuffle, then take the first CORRAL_UPGRADE_OFFER. */
  for (int i = 0; i < n; ++i) {
    int j = i + (int)(frand() * (float)(n - i));
    if (j >= n) {
      j = n - 1;
    }
    corral_upgrade_t tmp = pool[i];
    pool[i] = pool[j];
    pool[j] = tmp;
  }
  s_offer_count = (n < CORRAL_UPGRADE_OFFER) ? n : CORRAL_UPGRADE_OFFER;
  for (int i = 0; i < s_offer_count; ++i) {
    s_offer[i] = pool[i];
  }
}

static void corral_next_wave(float w, float h);

/* Apply the chosen offered card (by offer index 0..2): bump that upgrade's
 * level (persistent run modifier), refresh derived values, then advance to the
 * next wave. Out-of-range / no-pending picks are ignored. Returns true if a
 * pick was applied. */
static bool corral_pick_upgrade(int offer_index, float w, float h) {
  if (s_phase != CORRAL_PHASE_UPGRADE_CHOICE || s_offer_count <= 0) {
    return false;
  }
  if (offer_index < 0 || offer_index >= s_offer_count) {
    return false;
  }
  corral_upgrade_t u = s_offer[offer_index];
  if (s_upgrades[u] < CORRAL_UPG_MAX_LEVEL) {
    s_upgrades[u] += 1;
    s_acquired_total += 1;
  }
  /* refresh anything cached from upgrade levels. */
  s_lure_radius = corral_effective_lure_radius();
  s_offer_count = 0;
  game_audio_play(GAME_AUDIO_CUE_CORRAL_CHIME); /* pleasant reward chime */
  corral_next_wave(w, h); /* chosen -> the next wave begins */
  return true;
}

/* ---- Field / wave setup ---- */

/* Lay out one pen per ACTIVE color around the field edges/corners, each with an
 * open gate facing the field centre. Readable spreads for 2..5 pens; the pen
 * shrinks a touch as the count grows so the pasture stays open. */
static void corral_layout_pens(float w, float h) {
  const int n = s_color_count;
  const bool portrait = h > w; /* TALL phone screen vs wide desktop */
  const float top = h * 0.10F; /* HUD band reserved at the very top */
  const float cx = w * 0.5F;
  const float cy = h * 0.5F;
  const float mx = w * 0.035F; /* edge margins */
  const float my = top + h * 0.02F;

  /* Pen size adapts to orientation: a tall portrait screen wants WIDE, SHORT
   * pens hugging the top/bottom edges (so the centre pasture stays open and the
   * gates face inward up/down); a wide landscape screen wants the original
   * NARROW, TALL pens on the side edges. Sizes track the screen so nothing
   * overflows at any ratio. */
  float pen_w;
  float pen_h;
  if (portrait) {
    pen_w = clampf(w * 0.40F, 120.0F, 320.0F);
    pen_h = clampf(h * 0.13F, 96.0F, 200.0F);
  } else {
    pen_w = clampf(w * 0.15F, 96.0F, 180.0F);
    pen_h = clampf(h * 0.30F, 130.0F, 240.0F);
  }

  /* Anchored slots, picked per active-color count for a balanced, readable
   * spread. Each slot is a top-left rect; the gate faces the field centre.
   * Portrait uses top/bottom edges; landscape uses left/right sides. */
  for (int i = 0; i < n && i < CORRAL_PEN_COUNT; ++i) {
    Pen *p = &s_pens[i];
    p->color = (uint8_t)i;
    p->w = pen_w;
    p->h = pen_h;
    const float bottom_y = h - pen_h - h * 0.015F;
    if (portrait) {
      /* PORTRAIT: stack pens on the top and bottom edges (gates face center). */
      switch (n) {
      case 2:
        /* top-centre & bottom-centre */
        p->x = cx - pen_w * 0.5F;
        p->y = (i == 0) ? my : bottom_y;
        break;
      case 3:
        /* top-centre + two bottom (left/right) */
        if (i == 0) {
          p->x = cx - pen_w * 0.5F;
          p->y = my;
        } else {
          p->x = (i == 1) ? mx : (w - pen_w - mx);
          p->y = bottom_y;
        }
        break;
      case 4:
        /* two top + two bottom (a 2x2 ring) */
        p->x = (i % 2 == 0) ? mx : (w - pen_w - mx);
        p->y = (i < 2) ? my : bottom_y;
        break;
      default: /* 5: two top + two bottom + one bottom-centre is tight, so use
                * two top + three bottom spread across the wide bottom band. */
        if (i < 2) {
          p->x = (i % 2 == 0) ? mx : (w - pen_w - mx);
          p->y = my;
        } else {
          int j = i - 2; /* 0,1,2 across the bottom */
          float span = w - pen_w - 2.0F * mx;
          p->x = mx + span * (float)j * 0.5F;
          p->y = bottom_y;
        }
        break;
      }
    } else {
      /* LANDSCAPE: original side-edge / corner spread. */
      switch (n) {
      case 2:
        /* left & right, vertically centred */
        p->x = (i == 0) ? mx : (w - pen_w - mx);
        p->y = cy - pen_h * 0.5F;
        break;
      case 3:
        /* two sides + bottom-centre */
        if (i == 0) {
          p->x = mx;
          p->y = cy - pen_h * 0.5F;
        } else if (i == 1) {
          p->x = w - pen_w - mx;
          p->y = cy - pen_h * 0.5F;
        } else {
          p->x = cx - pen_w * 0.5F;
          p->y = h - pen_h - my * 0.4F;
        }
        break;
      case 4:
        /* four corners */
        p->x = (i % 2 == 0) ? mx : (w - pen_w - mx);
        p->y = (i < 2) ? my : (h - pen_h - my * 0.4F);
        break;
      default: /* 5 */
        /* four corners + bottom-centre */
        if (i < 4) {
          p->x = (i % 2 == 0) ? mx : (w - pen_w - mx);
          p->y = (i < 2) ? my : (h - pen_h - my * 0.4F);
        } else {
          p->x = cx - pen_w * 0.5F;
          p->y = h - pen_h - my * 0.4F;
        }
        break;
      }
    }
    /* gate mouth on the pen face nearest the field centre; inward normal. */
    float pcx = p->x + p->w * 0.5F;
    float pcy = p->y + p->h * 0.5F;
    float dx = cx - pcx;
    float dy = cy - pcy;
    if (fabsf(dx) >= fabsf(dy)) {
      /* horizontal opening (left/right face) */
      p->gdx = (dx >= 0.0F) ? 1.0F : -1.0F;
      p->gdy = 0.0F;
      p->gx = (dx >= 0.0F) ? (p->x + p->w) : p->x;
      p->gy = pcy;
    } else {
      /* vertical opening (top/bottom face) */
      p->gdx = 0.0F;
      p->gdy = (dy >= 0.0F) ? 1.0F : -1.0F;
      p->gx = pcx;
      p->gy = (dy >= 0.0F) ? (p->y + p->h) : p->y;
    }
  }
}

static void corral_spawn_wave(float w, float h) {
  s_color_count = wave_color_count(s_wave);
  /* difficulty legibility: if this wave introduces a color the player hasn't
   * seen yet, fire a brief "New color!" banner (skip on the very first wave —
   * the FTUE introduces the first colors). */
  if (s_color_count > s_seen_colors) {
    if (s_seen_colors > 0) {
      s_new_color_index = s_color_count - 1; /* the freshly added hue */
      s_new_color_timer = 2.6F;
    }
    s_seen_colors = s_color_count;
  }
  corral_layout_pens(w, h);
  for (int i = 0; i < CORRAL_PEN_COUNT; ++i) {
    s_pens[i].parked = 0;
    s_pens[i].flash = 0.0F;
    s_pens[i].chain = 0.0F;
    s_pens[i].chain_steps = 0;
  }
  /* Critters spawn in the central pasture, away from pen gates. */
  s_critter_count = wave_critter_count(s_wave);
  if (s_critter_count > CORRAL_MAX_CRITTERS) {
    s_critter_count = CORRAL_MAX_CRITTERS;
  }
  const float mid_lo = w * 0.32F;
  const float mid_hi = w * 0.68F;
  const float wander_scale = wave_wander_scale(s_wave);
  /* wave 1 wanders slow (gentle FTUE); later waves a touch livelier. */
  const float sp_lo = (s_wave == 1 ? 12.0F : 20.0F) * wander_scale;
  const float sp_hi = (s_wave == 1 ? 26.0F : 40.0F) * wander_scale;
  for (int i = 0; i < s_critter_count; ++i) {
    Critter *c = &s_critters[i];
    c->x = frand_range(mid_lo, mid_hi);
    c->y = frand_range(h * 0.26F, h * 0.74F);
    c->wander = frand_range(0.0F, 6.2831853F);
    float sp = frand_range(sp_lo, sp_hi);
    c->vx = cosf(c->wander) * sp;
    c->vy = sinf(c->wander) * sp;
    c->squash = 0.0F;
    c->flee = 0.0F;
    c->jitter = frand_range(0.0F, 6.2831853F);
    c->blink = frand_range(0.0F, 6.2831853F);
    c->link_x = c->x;
    c->link_y = c->y;
    c->has_link = false;
    c->color = (uint8_t)(i % s_color_count);
    /* progressive behavior variety: early waves normal, then a capped mix. */
    c->behavior = (uint8_t)pick_behavior(s_wave, i, s_critter_count);
    c->alive = true;
    c->parked = false;
    c->parked_pen = -1;
  }
  for (int i = 0; i < CORRAL_MAX_PARTICLES; ++i) {
    s_particles[i].life = 0.0F;
  }
}

static void corral_reset(float w, float h) {
  s_rng = 0x1234abcdU; /* deterministic per the screenshot contract */
  s_score = 0;
  s_wave = 1;
  s_phase = CORRAL_PHASE_TITLE;
  s_phase_timer = 0.0F;
  s_cleared_flash = 0.0F;
  s_win_shown = false;
  s_ftue_hint = 0.0F;
  /* FTUE: arm the first-run tutorial only if it has never been seen this session
   * (s_ftue_seen is NOT cleared by a reset/restart — first run means first run). */
  s_ftue_active = !s_ftue_seen;
  s_ftue_beat = 0;
  s_ftue_beat_age = 0.0F;
  s_ftue_move_accum = 0.0F;
  s_ftue_last_lure_x = 0.0F;
  s_ftue_last_lure_y = 0.0F;
  s_seen_colors = 0;
  s_new_color_timer = 0.0F;
  s_new_color_index = -1;
  s_lure_active = false;
  /* LIGHT META: a fresh run starts with no upgrades (clean slate). */
  for (int u = 0; u < CORRAL_UPG_COUNT; ++u) {
    s_upgrades[u] = 0;
  }
  s_offer_count = 0;
  s_acquired_total = 0;
  s_lure_radius = corral_effective_lure_radius();
  s_lure2_x = 0.0F;
  s_lure2_y = 0.0F;
  corral_spawn_wave(w, h);
}

/* Leave TITLE -> begin the run (also (re)starts the FTUE hint on wave 1). */
static void corral_start(float w, float h) {
  if (s_phase != CORRAL_PHASE_TITLE) {
    return;
  }
  s_phase = CORRAL_PHASE_PLAYING;
  if (s_wave == 1) {
    s_ftue_hint = 6.0F; /* gentle pulsing lure hint for the first action */
  }
  (void)w;
  (void)h;
  game_audio_play(GAME_AUDIO_CUE_CORRAL_START); /* soft welcoming swell */
}

/* Advance to the next wave (clears celebratory beat state). */
static void corral_next_wave(float w, float h) {
  s_wave += 1;
  s_cleared_flash = 0.0F;
  s_phase = CORRAL_PHASE_PLAYING;
  corral_spawn_wave(w, h);
}

/* ---- Juice ---- */

static void spawn_burst(float x, float y, const float *color, int n) {
  int spawned = 0;
  for (int i = 0; i < CORRAL_MAX_PARTICLES && spawned < n; ++i) {
    if (s_particles[i].life > 0.0F) {
      continue;
    }
    Particle *p = &s_particles[i];
    float a = frand_range(0.0F, 6.2831853F);
    float sp = frand_range(60.0F, 180.0F);
    p->x = x;
    p->y = y;
    p->vx = cosf(a) * sp;
    p->vy = sinf(a) * sp;
    p->max_life = frand_range(0.28F, 0.5F);
    p->life = p->max_life;
    p->color = color;
    ++spawned;
  }
}

/* ---- Simulation ---- */

static int corral_loose_count(void) {
  int n = 0;
  for (int i = 0; i < s_critter_count; ++i) {
    if (s_critters[i].alive && !s_critters[i].parked) {
      ++n;
    }
  }
  return n;
}

static int corral_color_remaining(int color) {
  int n = 0;
  for (int i = 0; i < s_critter_count; ++i) {
    const Critter *c = &s_critters[i];
    if (c->alive && !c->parked && c->color == (uint8_t)color) {
      ++n;
    }
  }
  return n;
}

#if NT_DEVAPI_ENABLED
/* Count loose (alive, not parked) critters of a given behavior — drives the
 * DevAPI behavior-mix report so a playtest can confirm progressive intro.
 * DevAPI-only: unused (and -Werror=unused-function) in the web/release build. */
static int corral_behavior_loose_count(corral_behavior_t b) {
  int n = 0;
  for (int i = 0; i < s_critter_count; ++i) {
    const Critter *c = &s_critters[i];
    if (c->alive && !c->parked && c->behavior == (uint8_t)b) {
      ++n;
    }
  }
  return n;
}
#endif

/* Gate mouth lives on the pen struct (pen->gx/gy + gdx/gdy), precomputed in
 * corral_layout_pens so any edge/corner placement works. */

/* Capture zone = the inner ~half of the pen on its open face. The GATE upgrade
 * widens this mouth (more perpendicular tolerance + a deeper accept band) so
 * captures get easier — a clear, calm player-positive buff. */
static bool point_in_pen_mouth(const Pen *pen, float x, float y) {
  const float gate = corral_gate_bonus(); /* 0 at level 0 */
  const float pad = 14.0F + gate;
  /* a higher GATE level reaches further into the pen face (0.45 -> ~0.30). */
  const float band = clampf(0.45F - 0.05F * (float)s_upgrades[CORRAL_UPG_GATE],
                            0.25F, 0.45F);
  if (pen->gdx != 0.0F) {
    /* horizontal opening */
    if (y < pen->y - pad || y > pen->y + pen->h + pad) {
      return false;
    }
    if (pen->gdx > 0.0F) {
      return x <= pen->x + pen->w && x >= pen->x + pen->w * band;
    }
    return x >= pen->x && x <= pen->x + pen->w * (1.0F - band);
  }
  /* vertical opening */
  if (x < pen->x - pad || x > pen->x + pen->w + pad) {
    return false;
  }
  if (pen->gdy > 0.0F) {
    return y <= pen->y + pen->h && y >= pen->y + pen->h * band;
  }
  return y >= pen->y && y <= pen->y + pen->h * (1.0F - band);
}

static void capture_critter(Critter *c, Pen *pen) {
  c->parked = true;
  c->alive = false;
  c->parked_pen = (int8_t)(pen->color);
  c->squash = 0.22F; /* squash/scale on entry (~0.2s) */
  pen->parked += 1;
  pen->flash = 0.30F;
  /* Is THIS capture continuing an in-flight chain (the pen was still pulling
   * same-color friends in)? If so it's a chain step; otherwise a fresh pop that
   * STARTS a new chain. chain_steps drives both the rising pitch and the
   * escalating particle burst — the signature "satisfying chain" moment. */
  const bool chaining = (pen->chain > 0.0F);
  if (chaining) {
    pen->chain_steps += 1;
  } else {
    pen->chain_steps = 0;
  }
  /* CHAIN: briefly boost same-color attraction (CHAIN upgrade lengthens it). */
  pen->chain = corral_chain_time();

  /* AUDIO + JUICE (calm/ASMR identity): a soft "plip" on a fresh capture; for a
   * continuing chain a brighter "chain" cue that RISES in pitch as the chain
   * grows (escalating satisfaction). Even fresh pops get a small per-capture
   * pitch wobble so repeats never fatigue. */
  if (chaining) {
    /* rise ~1 semitone per step, capped so it never gets shrill (~+9). */
    float semis = 2.0F + 1.4F * (float)pen->chain_steps;
    if (semis > 9.0F) {
      semis = 9.0F;
    }
    game_audio_play_pitched(GAME_AUDIO_CUE_CORRAL_CHAIN, semis);
  } else {
    /* small symmetric wobble (~ +/-1.5 semitones) for variety on repeats. */
    float wobble = (frand() - 0.5F) * 3.0F;
    game_audio_play_pitched(GAME_AUDIO_CUE_CORRAL_POP, wobble);
  }

  /* escalating burst: more particles as the chain deepens (cap so it stays
   * tasteful, not a screen-filling explosion). */
  int burst = 8 + 3 * pen->chain_steps;
  if (burst > 22) {
    burst = 22;
  }
  spawn_burst(pen->gx, pen->gy, CORRAL_COLORS[pen->color], burst);
  s_score += 1;
}

static void critter_update(float dt, float w, float h) {
  if (dt <= 0.0F) {
    dt = 1.0F / 60.0F;
  }
  if (dt > 0.05F) {
    dt = 0.05F;
  }

  game_audio_update();

  /* Lure tracks the primary pointer — mouse cursor OR a held/dragged finger
   * (framebuffer pixels == our draw space). Using the first ACTIVE pointer
   * (not a fixed slot 0) means a touch that lands in any slot still drives the
   * lure, so the lure follows a dragged finger on a phone. */
  {
    const nt_pointer_t *pp = primary_pointer();
    if (pp != NULL) {
      s_lure_x = pp->x;
      s_lure_y = pp->y;
      s_lure_active = true;
    } else {
      /* no pointer down (touch released) — hold the last lure position so the
       * field settles calmly instead of snapping the lure to the origin. */
      s_lure_active = s_lure_x > 0.0F || s_lure_y > 0.0F;
    }
  }

  /* SECOND_LURE upgrade: a trailing secondary lure point that lags smoothly
   * behind the cursor (an obvious "two influence points" extension of the one
   * action). It tracks even when the upgrade is off so it's settled when gained. */
  {
    float k = clampf(dt * 6.0F, 0.0F, 1.0F);
    s_lure2_x += (s_lure_x - s_lure2_x) * k;
    s_lure2_y += (s_lure_y - s_lure2_y) * k;
  }

  /* timers tick in every phase so flashes/particles settle even on the beats */
  for (int i = 0; i < s_color_count; ++i) {
    if (s_pens[i].flash > 0.0F) {
      s_pens[i].flash -= dt;
    }
    if (s_pens[i].chain > 0.0F) {
      s_pens[i].chain -= dt;
      if (s_pens[i].chain <= 0.0F) {
        s_pens[i].chain_steps = 0; /* chain window lapsed -> reset escalation */
      }
    }
  }
  if (s_cleared_flash > 0.0F) {
    s_cleared_flash -= dt;
  }
  if (s_ftue_hint > 0.0F) {
    s_ftue_hint -= dt;
  }
  if (s_new_color_timer > 0.0F) {
    s_new_color_timer -= dt;
  }

  /* --- particles always settle (so beats keep their fade) --- */
  for (int i = 0; i < CORRAL_MAX_PARTICLES; ++i) {
    Particle *p = &s_particles[i];
    if (p->life <= 0.0F) {
      continue;
    }
    p->life -= dt;
    p->vx *= 0.92F;
    p->vy *= 0.92F;
    p->x += p->vx * dt;
    p->y += p->vy * dt;
  }

  /* --- phase machine: only PLAYING runs the herding sim --- */
  if (s_phase == CORRAL_PHASE_WAVE_CLEARED) {
    if (s_phase_timer > 0.0F) {
      s_phase_timer -= dt;
    }
    if (s_phase_timer <= 0.0F) {
      /* LIGHT META: after the celebratory beat, offer a pick-1-of-3 upgrade
       * (if any remain unmaxed) BEFORE the next wave; otherwise go straight on. */
      corral_make_offer();
      if (s_offer_count > 0) {
        s_phase = CORRAL_PHASE_UPGRADE_CHOICE;
      } else {
        corral_next_wave(w, h);
      }
    }
    return;
  }
  if (s_phase == CORRAL_PHASE_UPGRADE_CHOICE) {
    /* hold here until the player picks (mouse/keys handled in handle_input or
     * via DevAPI game.debug.pick_upgrade). Field stays frozen behind the cards. */
    return;
  }
  if (s_phase == CORRAL_PHASE_WIN) {
    if (s_phase_timer > 0.0F) {
      s_phase_timer -= dt;
    }
    if (s_phase_timer <= 0.0F) {
      corral_next_wave(w, h); /* soft milestone done -> continue ENDLESS */
    }
    return;
  }
  if (s_phase != CORRAL_PHASE_PLAYING) {
    return; /* TITLE: field is visible but frozen until game.start */
  }

  /* ---- FTUE: advance beats by DOING (tutorial-by-doing, first run only) ----
   * Beat 0 -> 1: the player moved the lure a meaningful distance (learned the
   *              one action). Beat 1 -> 2: first capture (learned matching).
   * Beat 2 ends: the wave is nearly cleared (learned the goal). Then the
   * tutorial retires for the rest of the session (s_ftue_seen). */
  if (s_ftue_active) {
    s_ftue_beat_age += dt;
    if (s_ftue_beat == 0) {
      float mdx = s_lure_x - s_ftue_last_lure_x;
      float mdy = s_lure_y - s_ftue_last_lure_y;
      s_ftue_move_accum += sqrtf(mdx * mdx + mdy * mdy);
      /* advance once they've swept the lure a bit (and the beat has shown). */
      if (s_ftue_move_accum > 240.0F && s_ftue_beat_age > 0.6F) {
        s_ftue_beat = 1;
        s_ftue_beat_age = 0.0F;
      }
    } else if (s_ftue_beat == 1) {
      if (s_score > 0) { /* first capture made */
        s_ftue_beat = 2;
        s_ftue_beat_age = 0.0F;
      }
    } else { /* beat 2: clearing the level */
      int loose = corral_loose_count();
      if (loose <= 1 || s_ftue_beat_age > 8.0F) {
        s_ftue_active = false;
        s_ftue_seen = true; /* never show the tutorial again this session */
      }
    }
  }
  s_ftue_last_lure_x = s_lure_x;
  s_ftue_last_lure_y = s_lure_y;

  const float crit_r = 13.0F;

  for (int i = 0; i < s_critter_count; ++i) {
    Critter *c = &s_critters[i];
    if (c->squash > 0.0F) {
      c->squash -= dt;
    }
    /* free-running tells tick for every critter (even parked ones settle). */
    c->jitter += dt * 11.0F;
    c->blink += dt * 1.7F;
    if (c->flee > 0.0F) {
      c->flee -= dt;
    }
    if (!c->alive || c->parked) {
      continue;
    }

    const corral_behavior_t bhv = (corral_behavior_t)c->behavior;
    /* CALM upgrade: dampens wander + special-behavior intensity for a calmer,
     * more controllable field (1.0 = no upgrade, lower = calmer). */
    const float calm = corral_calm_mult();

    /* --- random wander (smooth heading drift) ---
     * SKITTISH wanders a touch more nervously; STUBBORN a touch less. */
    float wander_amp = (bhv == CORRAL_BEHAVIOR_SKITTISH)  ? 3.0F
                       : (bhv == CORRAL_BEHAVIOR_STUBBORN) ? 1.4F
                                                           : 2.2F;
    wander_amp *= calm;
    c->wander += frand_range(-wander_amp, wander_amp) * dt;
    float ax = cosf(c->wander) * 22.0F;
    float ay = sinf(c->wander) * 22.0F;

    /* --- separation + (FOLLOWER) cohesion to the crowd ---
     * One neighbour pass does double duty: push off close neighbours so they
     * don't overlap, and for FOLLOWERs accumulate a clump centre + remember the
     * nearest neighbour (for the visual "looking at neighbour" link tell). */
    float clump_x = 0.0F;
    float clump_y = 0.0F;
    int clump_n = 0;
    float near_d2 = 1.0e18F;
    float near_x = c->x;
    float near_y = c->y;
    const bool is_follower = (bhv == CORRAL_BEHAVIOR_FOLLOWER);
    const float clump_range = 130.0F; /* followers sense this far for the crowd */
    for (int j = 0; j < s_critter_count; ++j) {
      if (j == i) {
        continue;
      }
      const Critter *o = &s_critters[j];
      if (!o->alive || o->parked) {
        continue;
      }
      float dx = c->x - o->x;
      float dy = c->y - o->y;
      float d2 = dx * dx + dy * dy;
      float min_d = crit_r * 2.2F;
      if (d2 > 0.0001F && d2 < min_d * min_d) {
        float d = sqrtf(d2);
        float push = (min_d - d) / min_d;
        ax += (dx / d) * push * 120.0F;
        ay += (dy / d) * push * 120.0F;
      }
      if (is_follower && d2 < clump_range * clump_range) {
        /* clings to ANY neighbour (including other colors) -> drifts w/ crowd */
        clump_x += o->x;
        clump_y += o->y;
        ++clump_n;
        if (d2 < near_d2) {
          near_d2 = d2;
          near_x = o->x;
          near_y = o->y;
        }
      }
    }

    /* --- FOLLOWER cohesion: weak pull toward the local clump centre. Weaker
     * than the lure so the player CAN still herd one, but a lone follower drifts
     * back to the crowd — so the decision is "break up the clump first". --- */
    c->has_link = false;
    if (is_follower && clump_n > 0) {
      float cxw = clump_x / (float)clump_n;
      float cyw = clump_y / (float)clump_n;
      float dx = cxw - c->x;
      float dy = cyw - c->y;
      float d = sqrtf(dx * dx + dy * dy);
      if (d > 1.0F) {
        /* gentle cohesion (well under lure pull); CALM weakens it so clumps are
         * easier to break up. */
        float coh = 60.0F * calm;
        ax += (dx / d) * coh;
        ay += (dy / d) * coh;
      }
      /* smooth the link point toward the nearest neighbour for a calm tell. */
      c->link_x += (near_x - c->link_x) * clampf(dt * 8.0F, 0.0F, 1.0F);
      c->link_y += (near_y - c->link_y) * clampf(dt * 8.0F, 0.0F, 1.0F);
      c->has_link = true;
    }

    /* --- lure attraction (the one action), with per-behavior response ---
     *   SKITTISH: if the lure crowds it (gets very close) it triggers a short
     *             FLEE burst AWAY from the lure; outside that panic ring it
     *             herds normally -> reward approaching gently / not crowding.
     *   STUBBORN: only a fraction of the lure pull lands (high inertia) -> needs
     *             sustained luring (commit longer).
     *   FOLLOWER: herds normally to the lure, but the clump cohesion above
     *             fights it -> break up the crowd first. */
    if (s_lure_active) {
      /* The lure point(s): the cursor, plus an optional trailing SECOND lure.
       * Each point can attract independently; a critter responds to whichever
       * it is near (so the second lure genuinely doubles your reach). */
      const bool second = corral_second_lure_active();
      const float pull_mult = corral_pull_mult();
      const int lure_n = second ? 2 : 1;
      const float lure_pts[2][2] = {
          {s_lure_x, s_lure_y},
          {s_lure2_x, s_lure2_y},
      };
      for (int lp = 0; lp < lure_n; ++lp) {
        float dx = lure_pts[lp][0] - c->x;
        float dy = lure_pts[lp][1] - c->y;
        float d2 = dx * dx + dy * dy;
        /* the trailing lure is a touch weaker than the primary (it's a helper). */
        float pt_scale = (lp == 0) ? 1.0F : 0.78F;
        /* skittish panic ring: crowding it inside this radius spooks it. CALM
         * shrinks the panic ring so calmed critters are easier to approach. */
        const float panic_r = 56.0F * calm;
        if (bhv == CORRAL_BEHAVIOR_SKITTISH && d2 < panic_r * panic_r &&
            d2 > 1.0F) {
          c->flee = 0.45F * calm; /* shorter, gentler burst when calmed */
        }
        if (c->flee > 0.0F && d2 > 1.0F && lp == 0) {
          /* FLEE: push away from the PRIMARY lure (skittish only sets c->flee) */
          float d = sqrtf(d2);
          float panic = 170.0F * clampf(c->flee / 0.45F, 0.0F, 1.0F);
          ax -= (dx / d) * panic;
          ay -= (dy / d) * panic;
        } else if (c->flee <= 0.0F && d2 < s_lure_radius * s_lure_radius &&
                   d2 > 1.0F) {
          float d = sqrtf(d2);
          float t = 1.0F - (d / s_lure_radius); /* stronger when closer */
          float pull = 140.0F * (0.4F + t) * pull_mult * pt_scale;
          /* stubborn resists steering: only part of the pull registers. */
          if (bhv == CORRAL_BEHAVIOR_STUBBORN) {
            pull *= 0.42F;
          }
          ax += (dx / d) * pull;
          ay += (dy / d) * pull;
        }
      }
    }

    /* --- chain boost: matching pen pulls same-color friends in --- */
    for (int p = 0; p < s_color_count; ++p) {
      Pen *pen = &s_pens[p];
      if (pen->color != c->color || pen->chain <= 0.0F) {
        continue;
      }
      float dx = pen->gx - c->x;
      float dy = pen->gy - c->y;
      float d = sqrtf(dx * dx + dy * dy);
      if (d > 1.0F) {
        /* CHAIN upgrade: a stronger cascade pull (and it lasts longer via the
         * longer pen->chain timer set on capture). Normalize so level 0 reads
         * as before; higher levels boost a touch harder. */
        float chain_str = 1.0F + 0.25F * (float)s_upgrades[CORRAL_UPG_CHAIN];
        float boost = 130.0F * clampf(pen->chain / 0.85F, 0.0F, 1.4F) * chain_str;
        ax += (dx / d) * boost;
        ay += (dy / d) * boost;
      }
    }

    /* integrate velocity, clamp speed (gentle), per-behavior feel:
     *   STUBBORN: high inertia -> accel damped + lower top speed (slow, heavy).
     *   SKITTISH: light/fast -> a slightly higher top speed for darty bursts. */
    float accel_scale = (bhv == CORRAL_BEHAVIOR_STUBBORN) ? 0.55F : 1.0F;
    c->vx += ax * dt * accel_scale;
    c->vy += ay * dt * accel_scale;
    float speed = sqrtf(c->vx * c->vx + c->vy * c->vy);
    float max_speed = (bhv == CORRAL_BEHAVIOR_STUBBORN)  ? 95.0F
                      : (bhv == CORRAL_BEHAVIOR_SKITTISH) ? 155.0F
                                                          : 130.0F;
    if (speed > max_speed) {
      c->vx = c->vx / speed * max_speed;
      c->vy = c->vy / speed * max_speed;
    }
    c->x += c->vx * dt;
    c->y += c->vy * dt;

    /* --- pens: capture matching, bounce wrong --- */
    for (int p = 0; p < s_color_count; ++p) {
      Pen *pen = &s_pens[p];
      bool overlaps =
          c->x >= pen->x - crit_r && c->x <= pen->x + pen->w + crit_r &&
          c->y >= pen->y - crit_r && c->y <= pen->y + pen->h + crit_r;
      if (!overlaps) {
        continue;
      }
      if (pen->color == c->color && point_in_pen_mouth(pen, c->x, c->y)) {
        capture_critter(c, pen);
        break;
      }
      /* wrong color: bounce out along the gate-normal axis (works for any
       * edge/corner pen since the open face is the gate-normal direction). */
      if (pen->color != c->color) {
        if (pen->gdx > 0.0F && c->x < pen->x + pen->w) {
          c->x = pen->x + pen->w + crit_r;
          c->vx = fabsf(c->vx) + 30.0F;
        } else if (pen->gdx < 0.0F && c->x > pen->x) {
          c->x = pen->x - crit_r;
          c->vx = -fabsf(c->vx) - 30.0F;
        } else if (pen->gdy > 0.0F && c->y < pen->y + pen->h) {
          c->y = pen->y + pen->h + crit_r;
          c->vy = fabsf(c->vy) + 30.0F;
        } else if (pen->gdy < 0.0F && c->y > pen->y) {
          c->y = pen->y - crit_r;
          c->vy = -fabsf(c->vy) - 30.0F;
        }
        /* Only voice the bonk on a FRESH contact (pen not already flashing from
         * a very recent bonk) so a critter wedged against a gate doesn't
         * machine-gun the sound — keeps it gentle, not punishing/annoying. A
         * tiny pitch wobble softens repeats further. */
        if (pen->flash <= 0.0F) {
          game_audio_play_pitched(GAME_AUDIO_CUE_CORRAL_BONK,
                                  (frand() - 0.5F) * 2.0F);
        }
        pen->flash = 0.12F;
      }
    }

    /* keep critters on the field */
    const float m = crit_r;
    if (c->x < m) {
      c->x = m;
      c->vx = fabsf(c->vx);
    }
    if (c->x > w - m) {
      c->x = w - m;
      c->vx = -fabsf(c->vx);
    }
    if (c->y < m + h * 0.10F) {
      c->y = m + h * 0.10F;
      c->vy = fabsf(c->vy);
    }
    if (c->y > h - m) {
      c->y = h - m;
      c->vy = -fabsf(c->vy);
    }
  }

  /* --- wave clear -> celebratory beat (WAVE_CLEARED) or soft WIN milestone.
   * No harsh fail; the beat is brief, then the next wave spawns. The WIN beat
   * fires once after clearing CORRAL_WIN_WAVE, then play continues ENDLESS. */
  if (corral_loose_count() == 0 && s_critter_count > 0) {
    s_cleared_flash = 0.85F;
    if (s_wave >= CORRAL_WIN_WAVE && !s_win_shown) {
      s_win_shown = true;
      s_phase = CORRAL_PHASE_WIN;
      s_phase_timer = CORRAL_WIN_BEAT_TIME;
      game_audio_play(GAME_AUDIO_CUE_CORRAL_WIN); /* bigger "you did it!" */
    } else {
      s_phase = CORRAL_PHASE_WAVE_CLEARED;
      s_phase_timer = CORRAL_WAVE_CLEARED_TIME;
      game_audio_play(GAME_AUDIO_CUE_CORRAL_WAVE); /* short happy flourish */
    }
  }
}

/* ---- Sprite render ---- */

/* Packed emit tint that turns the NEUTRAL critter sprite into a given hue. The
 * sprite body is near-white so the color multiplies through cleanly; the dark
 * rim/eyes stay dark. Lift the hue toward full saturation so it reads boldly.
 */
static uint32_t critter_tint(int color) {
  const float *col = CORRAL_COLORS[color];
  float c[4] = {clampf(col[0] * 1.05F, 0.0F, 1.0F),
                clampf(col[1] * 1.05F, 0.0F, 1.0F),
                clampf(col[2] * 1.05F, 0.0F, 1.0F), 1.0F};
  return pack_rgba(c);
}

static void draw_critter_sprite(const Critter *c) {
  const corral_behavior_t bhv = (corral_behavior_t)c->behavior;

  /* Per-behavior READABLE tell (size + tint + motion), all on the sprite
   * renderer (alpha) reusing existing regions + transform/color:
   *   SKITTISH — lighter + smaller + nervous jitter (and a flee-flash).
   *   STUBBORN — bigger + darker, framed by a dark outline ring (slow blink).
   *   FOLLOWER — normal size + a soft link dot toward its nearest neighbour. */
  float base = 46.0F; /* on-screen diameter of a critter (bolder) */
  float jx = 0.0F;
  float jy = 0.0F;
  float tint_mul = 1.0F; /* >1 lightens (skittish), <1 darkens (stubborn) */
  if (bhv == CORRAL_BEHAVIOR_SKITTISH) {
    base = 40.0F;          /* lighter/smaller silhouette */
    tint_mul = 1.18F;      /* paler, "nervous" */
    /* jittery micro-motion (free-running), spiking while actually fleeing. */
    float jamp = 1.4F + 6.0F * clampf(c->flee / 0.45F, 0.0F, 1.0F);
    jx = sinf(c->jitter) * jamp;
    jy = cosf(c->jitter * 1.3F) * jamp;
  } else if (bhv == CORRAL_BEHAVIOR_STUBBORN) {
    base = 54.0F;     /* bigger/heavier look */
    tint_mul = 0.82F; /* darker body */
  }

  /* squash: brief vertical squish on capture (scale the world transform) */
  float sx = 1.0F;
  float sy = 1.0F;
  if (c->squash > 0.0F) {
    float t = c->squash / 0.22F;
    float s = 0.30F * sinf(t * 3.14159F);
    sx = 1.0F + s;        /* widen */
    sy = 1.0F - s * 0.7F; /* squish */
  }

  float dx = c->x + jx;
  float dy = c->y + jy;

  /* FOLLOWER: a soft link line of dots toward the nearest neighbour — the
   * "clinging to the crowd" tell. Drawn under the body so it reads as a leash. */
  if (bhv == CORRAL_BEHAVIOR_FOLLOWER && c->has_link) {
    for (int k = 1; k <= 3; ++k) {
      float t = (float)k / 4.0F;
      float lx = c->x + (c->link_x - c->x) * t;
      float ly = c->y + (c->link_y - c->y) * t;
      float a = 0.42F * (1.0F - t); /* fade toward the neighbour */
      emit_sprite(CORRAL_RGN_SPARK, lx, ly, 12.0F, 12.0F,
                  pack_white_alpha(a));
    }
  }

  /* soft ground shadow under the critter (pop against the calmed grass) */
  emit_sprite(CORRAL_RGN_SPARK, dx, dy + base * 0.36F, base * 1.0F,
              base * 0.46F, 0x66000000U /* AABBGGRR: dark, low alpha */);

  /* STUBBORN: a dark outline ring just behind the (larger) body = heavy look. */
  if (bhv == CORRAL_BEHAVIOR_STUBBORN) {
    emit_sprite(CORRAL_RGN_CRITTER, dx, dy, base * sx + 8.0F, base * sy + 8.0F,
                0xCC101014U /* dark, framing outline */);
  }

  /* body tint = the color hue, lightened/darkened by behavior for the tell. */
  const float *col = CORRAL_COLORS[c->color];
  float body[4] = {clampf(col[0] * 1.05F * tint_mul, 0.0F, 1.0F),
                   clampf(col[1] * 1.05F * tint_mul, 0.0F, 1.0F),
                   clampf(col[2] * 1.05F * tint_mul, 0.0F, 1.0F), 1.0F};
  emit_sprite(CORRAL_RGN_CRITTER, dx, dy, base * sx, base * sy, pack_rgba(body));

  /* SKITTISH: wide, quick-darting eyes — two small bright pips that flick
   * side-to-side (and widen while fleeing) for a clear "nervous" read. */
  if (bhv == CORRAL_BEHAVIOR_SKITTISH) {
    float dart = sinf(c->jitter * 0.7F) * (2.5F + base * 0.05F);
    float eye = 7.0F + 3.0F * clampf(c->flee / 0.45F, 0.0F, 1.0F);
    float ex = base * 0.20F;
    float ey = -base * 0.10F;
    emit_sprite(CORRAL_RGN_PIP, dx - ex + dart, dy + ey, eye, eye,
                0xFFFFFFFFU);
    emit_sprite(CORRAL_RGN_PIP, dx + ex + dart, dy + ey, eye, eye,
                0xFFFFFFFFU);
  } else if (bhv == CORRAL_BEHAVIOR_STUBBORN) {
    /* STUBBORN: a slow blink — a dark lid pip that periodically covers a small
     * sleepy eye, reinforcing the heavy/low-energy read. */
    float blink = 0.5F + 0.5F * sinf(c->blink);
    float lid = (blink > 0.78F) ? 1.0F : 0.0F; /* mostly open, slow closes */
    float eye = 6.0F;
    if (lid < 0.5F) {
      emit_sprite(CORRAL_RGN_PIP, dx, dy - base * 0.06F, eye, eye,
                  0xFF202024U /* sleepy dark eye */);
    }
  }
}

static void draw_pen_sprite(const Pen *pen) {
  float flash =
      pen->flash > 0.0F ? clampf(pen->flash / 0.30F, 0.0F, 1.0F) : 0.0F;
  float chain =
      pen->chain > 0.0F ? clampf(pen->chain / 0.85F, 0.0F, 1.0F) : 0.0F;
  const float *col = CORRAL_COLORS[pen->color];

  /* Panel = the pen's EXACT critter hue. The sprite is near-white with a dark
   * fence rim, so a near-saturated tint reads as "the red pen" / "the blue pen"
   * while the dark fence/posts (already baked dark) still frame it. Lighten a
   * touch + brighten on flash/chain so the pen stays inviting, not muddy. */
  float lit = 0.30F * flash + 0.12F * chain;
  float tint[4] = {
      clampf(col[0] * 0.82F + 0.16F + lit, 0.0F, 1.0F),
      clampf(col[1] * 0.82F + 0.16F + lit, 0.0F, 1.0F),
      clampf(col[2] * 0.82F + 0.16F + lit, 0.0F, 1.0F),
      1.0F,
  };
  float cx = pen->x + pen->w * 0.5F;
  float cy = pen->y + pen->h * 0.5F;
  /* The panel art carves its gate out of the RIGHT face. For a pen that opens
   * LEFT, mirror horizontally (negative width). For top/bottom-opening pens the
   * emit transform can't rotate, so the panel reads as a framed pen and the
   * gate glow + flag (placed at the real gate point below) carry the opening.
   */
  float draw_w = (pen->gdx < 0.0F) ? -pen->w : pen->w;
  emit_sprite(CORRAL_RGN_PEN, cx, cy, draw_w, pen->h, pack_rgba(tint));

  /* color flag marker above the pen so the pen<->color mapping is unmistakable.
   * Tinted to the pen hue; points inward toward the field. */
  float flag_w = 40.0F;
  float flag_h = 50.0F;
  float fcol[4] = {col[0], col[1], col[2], 1.0F};
  float flag_draw_w = (pen->gdx < 0.0F) ? -flag_w : flag_w;
  emit_sprite(CORRAL_RGN_FLAG, cx, pen->y - flag_h * 0.42F, flag_draw_w, flag_h,
              pack_rgba(fcol));

  /* soft gate glow on the open (inner) face so the entrance reads (warm, low
   * intensity halo — not a hard marker). Sized along the open face. */
  float glow = 0.40F + 0.45F * flash;
  float gate_col[4] = {clampf(col[0] + 0.35F, 0.0F, 1.0F),
                       clampf(col[1] + 0.35F, 0.0F, 1.0F),
                       clampf(col[2] + 0.35F, 0.0F, 1.0F), glow};
  float glow_w = (pen->gdx != 0.0F) ? 60.0F : pen->w * 0.7F;
  float glow_h = (pen->gdx != 0.0F) ? pen->h * 0.7F : 60.0F;
  emit_sprite(CORRAL_RGN_LURE, pen->gx, pen->gy, glow_w, glow_h,
              pack_rgba(gate_col));

  /* parked critters stacked inside the pen (neutral sprite, tinted to hue) */
  uint32_t parked_tint = critter_tint(pen->color);
  const float pr = 24.0F; /* parked critter on-screen size */
  int cols = (int)((pen->w - 28.0F) / (pr * 0.92F));
  if (cols < 1) {
    cols = 1;
  }
  for (int i = 0; i < pen->parked; ++i) {
    int gxx = i % cols;
    int gyy = i / cols;
    float px = pen->x + 16.0F + pr * 0.5F + (float)gxx * pr * 0.92F;
    float py = pen->y + 22.0F + pr * 0.5F + (float)gyy * pr * 0.88F;
    if (py > pen->y + pen->h - pr * 0.5F) {
      break;
    }
    emit_sprite(CORRAL_RGN_CRITTER, px, py, pr, pr, parked_tint);
  }
}

/* Centered translucent panel (PIP sprite stretched) used as a beat backdrop. */
static void emit_panel(float cx, float cy, float pw, float ph, uint32_t color) {
  emit_sprite(CORRAL_RGN_PIP, cx, cy, pw, ph, color);
}

/* LIGHT META: rect of the i-th offered upgrade card (top-left + size). Shared by
 * the overlay draw and the click hit-test so they stay aligned. Cards are big,
 * evenly spaced across the centre band — readable, calm 1-of-3 layout. */
static void upgrade_card_rect(int i, float w, float h, float *cx, float *cy,
                              float *cw, float *ch) {
  int n = (s_offer_count > 0) ? s_offer_count : CORRAL_UPGRADE_OFFER;
  const bool portrait = h > w;
  if (portrait) {
    /* PORTRAIT: a vertical STACK of wide, finger-tall cards — three of them fit
     * a narrow phone screen without overflow, each a big tap target. */
    float card_w = clampf(w * 0.84F, 200.0F, 520.0F);
    float card_h = clampf(h * 0.18F, 120.0F, 220.0F);
    float gap = card_h * 0.16F;
    float total = (float)n * card_h + (float)(n - 1) * gap;
    /* keep the stack clear of the header text near the top. */
    float y0 = h * 0.16F;
    float avail = h - y0 - h * 0.04F;
    if (total > avail) {
      /* shrink to fit the available band so nothing runs off the bottom. */
      float scale = avail / total;
      card_h *= scale;
      gap *= scale;
      total = avail;
    }
    *cw = card_w;
    *ch = card_h;
    *cx = w * 0.5F - card_w * 0.5F;
    *cy = y0 + (float)i * (card_h + gap);
    return;
  }
  /* LANDSCAPE: the original 1x3 row across the centre band. */
  float card_w = clampf(w * 0.20F, 150.0F, 230.0F);
  float card_h = clampf(h * 0.46F, 200.0F, 300.0F);
  float gap = card_w * 0.18F;
  float total = (float)n * card_w + (float)(n - 1) * gap;
  float x0 = w * 0.5F - total * 0.5F;
  *cw = card_w;
  *ch = card_h;
  *cx = x0 + (float)i * (card_w + gap);
  *cy = h * 0.5F - card_h * 0.5F + h * 0.02F;
}

/* Magnitude shown on a card: how many pips to display = level AFTER picking it
 * (so a card that would take you to level 2 shows 2 filled pips of MAX). */
static int upgrade_card_pips(corral_upgrade_t u) {
  int next = s_upgrades[u] + 1;
  return (next > CORRAL_UPG_MAX_LEVEL) ? CORRAL_UPG_MAX_LEVEL : next;
}

/* Fontless phase beats: TITLE start prompt, WAVE_CLEARED beat, and the soft
 * WIN milestone — all built from panels + pips/critter/lure sprites so they
 * read without any font. The field stays visible behind every beat. */
static void corral_draw_phase_overlay(float w, float h) {
  if (s_phase == CORRAL_PHASE_TITLE) {
    const bool portrait = h > w;
    const float screen_min = (w < h) ? w : h;
    /* soft dim over the (visible, frozen) field so the start beat pops. */
    emit_panel(w * 0.5F, h * 0.5F, w, h, 0x55101418U);
    /* title plate — wider/taller share of a narrow portrait screen. */
    float plate_w = portrait ? w * 0.88F : w * 0.62F;
    float plate_h = portrait ? h * 0.30F : h * 0.34F;
    emit_panel(w * 0.5F, h * 0.42F, plate_w, plate_h, 0xDD2A2218U);
    /* a friendly row of all five hues = "this is the game" identity badge.
     * Pitch + critter size track the screen so the row never runs past the
     * plate on a narrow screen. */
    float badge = clampf(screen_min * 0.10F, 34.0F, 56.0F);
    float pitch = badge * 1.25F;
    float bx = w * 0.5F - 2.0F * pitch;
    for (int i = 0; i < CORRAL_MAX_COLORS; ++i) {
      float bob = 6.0F * sinf((float)g_nt_app.frame * 0.06F + (float)i * 0.9F);
      emit_sprite(CORRAL_RGN_CRITTER, bx + (float)i * pitch, h * 0.38F + bob,
                  badge, badge, critter_tint(i));
    }
    /* pulsing lure orb = "press / tap to start" (the one action, invited). */
    float pulse = 0.5F + 0.5F * sinf((float)g_nt_app.frame * 0.12F);
    float sz = clampf(screen_min * 0.18F, 80.0F, 130.0F) + 26.0F * pulse;
    emit_sprite(CORRAL_RGN_LURE, w * 0.5F, h * 0.62F, sz, sz,
                pack_white_alpha(0.55F + 0.35F * pulse));
    emit_sprite(CORRAL_RGN_FLAG, w * 0.5F, h * 0.62F, 34.0F, 42.0F,
                pack_white_alpha(0.9F));
    return;
  }

  if (s_phase == CORRAL_PHASE_WAVE_CLEARED) {
    /* brief celebratory plate: a bright row of pips ticking up + sparkle. */
    float t = clampf(s_phase_timer / CORRAL_WAVE_CLEARED_TIME, 0.0F, 1.0F);
    float a = 0.85F * t + 0.15F;
    uint32_t plate = ((uint32_t)(a * 200.0F) << 24) | 0x002A2218U;
    emit_panel(w * 0.5F, h * 0.5F, w * 0.40F, h * 0.16F, plate);
    /* a green "cleared" tick row */
    const float *g = CORRAL_COLORS[2];
    float gc[4] = {g[0], g[1], g[2], a};
    for (int i = 0; i < 5; ++i) {
      emit_sprite(CORRAL_RGN_PIP, w * 0.5F - 2.0F * 28.0F + (float)i * 28.0F,
                  h * 0.5F, 20.0F, 20.0F, pack_rgba(gc));
    }
    return;
  }

  if (s_phase == CORRAL_PHASE_UPGRADE_CHOICE) {
    /* LIGHT META pick-1-of-3: soft dim over the frozen field, a calm header of
     * three lure orbs (= "choose one"), then three big readable cards. Each
     * card = a hue-tinted backdrop + the big upgrade icon + magnitude pips +
     * a 1/2/3 key hint pip. Fully fontless; the icon conveys the effect. */
    emit_panel(w * 0.5F, h * 0.5F, w, h, 0x99101418U);

    /* header: three soft lure orbs arched above the cards = "pick one". */
    for (int i = 0; i < 3; ++i) {
      float bob = 5.0F * sinf((float)g_nt_app.frame * 0.07F + (float)i * 1.1F);
      emit_sprite(CORRAL_RGN_LURE, w * 0.5F + (float)(i - 1) * 70.0F,
                  h * 0.16F + bob, 46.0F, 46.0F, pack_white_alpha(0.6F));
    }

    int n = (s_offer_count > 0) ? s_offer_count : CORRAL_UPGRADE_OFFER;
    for (int i = 0; i < n; ++i) {
      corral_upgrade_t u = s_offer[i];
      float cx;
      float cy;
      float cw;
      float ch;
      upgrade_card_rect(i, w, h, &cx, &cy, &cw, &ch);
      float mcx = cx + cw * 0.5F;
      float mcy = cy + ch * 0.5F;

      /* gentle idle bob per card so the choice feels alive (calm, slow). */
      float bob = 4.0F * sinf((float)g_nt_app.frame * 0.05F + (float)i * 1.3F);
      mcy += bob;

      /* card backdrop: a near-white tile, faintly tinted toward a warm hue so
       * the three cards read as inviting buttons. */
      float wash[4] = {0.96F, 0.93F, 0.86F, 1.0F};
      emit_sprite(CORRAL_RGN_CARD, mcx, mcy, cw, ch, pack_rgba(wash));

      const bool portrait = h > w;
      int filled = upgrade_card_pips(u);
      if (portrait) {
        /* WIDE/SHORT portrait card: icon on the LEFT, magnitude pips + the
         * 1/2/3 index dots on the RIGHT; the title/desc text sit in the middle
         * (drawn in the text pass). Sizes track the card height so it scales. */
        float icon = ch * 0.62F;
        float icx = cx + ch * 0.5F; /* left gutter, one card-height in */
        emit_sprite(upgrade_icon_region(u), icx, mcy, icon, icon, 0xFF20242CU);

        const float dot = ch * 0.13F;
        const float gap = dot * 1.4F;
        float rx = cx + cw - ch * 0.55F; /* right gutter */
        float row_w = (float)CORRAL_UPG_MAX_LEVEL * gap;
        float px0 = rx - row_w * 0.5F + gap * 0.5F;
        for (int l = 0; l < CORRAL_UPG_MAX_LEVEL; ++l) {
          uint32_t pc = (l < filled) ? 0xFF40D0FFU : 0xFFB0A89CU;
          emit_sprite(CORRAL_RGN_PIP, px0 + (float)l * gap, mcy - ch * 0.14F,
                      dot, dot, pc);
        }
        const float kd = ch * 0.09F;
        const float kgap = kd * 1.6F;
        float kw = (float)(i + 1) * kgap;
        float kx0 = rx - kw * 0.5F + kgap * 0.5F;
        for (int k = 0; k <= i; ++k) {
          emit_sprite(CORRAL_RGN_PIP, kx0 + (float)k * kgap, mcy + ch * 0.22F,
                      kd, kd, 0xFF303838U);
        }
      } else {
        /* big upgrade icon (dark so it pops on the light card). */
        float icon = cw * 0.56F;
        emit_sprite(upgrade_icon_region(u), mcx, mcy - ch * 0.12F, icon, icon,
                    0xFF20242CU);

        /* magnitude: a row of MAX pips, filled up to the level this pick grants
         * (so a bigger magnitude reads as more filled pips — no text). */
        const float dot = cw * 0.10F;
        const float gap = dot * 1.4F;
        float row_w = (float)CORRAL_UPG_MAX_LEVEL * gap;
        float px0 = mcx - row_w * 0.5F + gap * 0.5F;
        for (int l = 0; l < CORRAL_UPG_MAX_LEVEL; ++l) {
          uint32_t pc = (l < filled) ? 0xFF40D0FFU /* warm gold-ish filled */
                                     : 0xFFB0A89CU /* dim empty slot */;
          emit_sprite(CORRAL_RGN_PIP, px0 + (float)l * gap, mcy + ch * 0.24F,
                      dot, dot, pc);
        }

        /* 1/2/3 key hint: that many small dots at the card foot (fontless). */
        const float kd = cw * 0.06F;
        const float kgap = kd * 1.6F;
        float kw = (float)(i + 1) * kgap;
        float kx0 = mcx - kw * 0.5F + kgap * 0.5F;
        for (int k = 0; k <= i; ++k) {
          emit_sprite(CORRAL_RGN_PIP, kx0 + (float)k * kgap, mcy + ch * 0.40F,
                      kd, kd, 0xFF303838U);
        }
      }
    }
    return;
  }

  if (s_phase == CORRAL_PHASE_WIN) {
    /* soft "you did it!" milestone: bigger plate, a star of all hues + sparkle.
     * Calm and celebratory — then play continues endless. */
    float t = clampf(s_phase_timer / CORRAL_WIN_BEAT_TIME, 0.0F, 1.0F);
    emit_panel(w * 0.5F, h * 0.5F, w, h,
               ((uint32_t)(0.30F * t * 255.0F) << 24));
    emit_panel(w * 0.5F, h * 0.5F, w * 0.5F, h * 0.30F, 0xE02A2218U);
    /* radial star of tinted critters */
    float spin = (float)g_nt_app.frame * 0.05F;
    for (int i = 0; i < CORRAL_MAX_COLORS; ++i) {
      float ang = spin + (float)i * (6.2831853F / (float)CORRAL_MAX_COLORS);
      float rad = h * 0.10F;
      emit_sprite(CORRAL_RGN_CRITTER, w * 0.5F + cosf(ang) * rad,
                  h * 0.5F + sinf(ang) * rad, 54.0F, 54.0F, critter_tint(i));
    }
    /* gold sparkle burst */
    float pulse = 0.5F + 0.5F * sinf((float)g_nt_app.frame * 0.2F);
    emit_sprite(CORRAL_RGN_LURE, w * 0.5F, h * 0.5F, 70.0F + 30.0F * pulse,
                70.0F + 30.0F * pulse, pack_white_alpha(0.5F + 0.4F * pulse));
    return;
  }
}

static void corral_draw_sprites(float w, float h) {
  nt_sprite_renderer_set_material(s_sprite_material);

  /* Pasture background: tile the soft grass across the field. */
  {
    const float tile = 160.0F;
    int nx = (int)(w / tile) + 1;
    int ny = (int)(h / tile) + 1;
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        emit_sprite(CORRAL_RGN_GRASS, (float)i * tile + tile * 0.5F,
                    (float)j * tile + tile * 0.5F, tile + 1.0F, tile + 1.0F,
                    0xFFFFFFFFU);
      }
    }
  }

  for (int i = 0; i < s_color_count; ++i) {
    draw_pen_sprite(&s_pens[i]);
  }

  /* lure ring (attract radius) at the cursor — soft glowing orb. The center is
   * intentionally soft (no harsh bright dot); the halo shows the attract zone.
   * "lure follows cursor" affordance = the gentle radius ring tracking input.
   */
  if (s_lure_active) {
    /* FTUE: on wave 1 the lure gently PULSES so the first action is
     * discoverable (tutorial-by-doing, no text). The pulse fades as the hint
     * timer runs out. */
    float hint =
        (s_ftue_hint > 0.0F) ? clampf(s_ftue_hint / 6.0F, 0.0F, 1.0F) : 0.0F;
    float pulse = 1.0F + 0.18F * hint * sinf(s_ftue_hint * 6.0F);
    uint32_t halo =
        (hint > 0.0F) ? pack_white_alpha(0.33F + 0.22F * hint) : 0x55FFFFFFU;
    emit_sprite(CORRAL_RGN_LURE, s_lure_x, s_lure_y,
                s_lure_radius * 2.0F * pulse, s_lure_radius * 2.0F * pulse,
                halo);
    emit_sprite(
        CORRAL_RGN_LURE, s_lure_x, s_lure_y, 72.0F * pulse, 72.0F * pulse,
        (hint > 0.0F) ? pack_white_alpha(0.6F + 0.4F * hint) : 0x99FFFFFFU);

    /* SECOND_LURE upgrade: draw the trailing secondary lure (smaller, softer)
     * with a faint link so "you now have two influence points" reads at a
     * glance. Its ring uses the same effective radius (it pulls the same way). */
    if (corral_second_lure_active()) {
      /* a few faint dots linking the two points */
      for (int k = 1; k <= 3; ++k) {
        float t = (float)k / 4.0F;
        float lx = s_lure_x + (s_lure2_x - s_lure_x) * t;
        float ly = s_lure_y + (s_lure2_y - s_lure_y) * t;
        emit_sprite(CORRAL_RGN_SPARK, lx, ly, 10.0F, 10.0F,
                    pack_white_alpha(0.30F * (1.0F - t)));
      }
      emit_sprite(CORRAL_RGN_LURE, s_lure2_x, s_lure2_y,
                  s_lure_radius * 1.6F, s_lure_radius * 1.6F, 0x44FFFFFFU);
      emit_sprite(CORRAL_RGN_LURE, s_lure2_x, s_lure2_y, 54.0F, 54.0F,
                  0x77FFFFFFU);
    }
  }

  for (int i = 0; i < s_critter_count; ++i) {
    const Critter *c = &s_critters[i];
    if (c->alive && !c->parked) {
      draw_critter_sprite(c);
    }
  }

  /* FTUE beat 1 cue: a soft pulsing pointer trail from the nearest loose FIRST-
   * color critter toward its matching pen gate, so "bring [red] into the [red]
   * pen" reads as a visible arrow (tutorial-by-doing, calm). */
  if (s_ftue_active && s_ftue_beat == 1 && s_color_count > 0) {
    /* nearest loose critter of color 0 (matches the FTUE text). */
    const Critter *src = NULL;
    float best = 1.0e18F;
    for (int i = 0; i < s_critter_count; ++i) {
      const Critter *c = &s_critters[i];
      if (!c->alive || c->parked || c->color != 0) {
        continue;
      }
      float dx = s_pens[0].gx - c->x;
      float dy = s_pens[0].gy - c->y;
      float d2 = dx * dx + dy * dy;
      if (d2 < best) {
        best = d2;
        src = c;
      }
    }
    if (src != NULL) {
      const float *col = CORRAL_COLORS[0];
      float pulse = 0.5F + 0.5F * sinf((float)g_nt_app.frame * 0.16F);
      for (int k = 1; k <= 5; ++k) {
        float t = (float)k / 6.0F;
        float lx = src->x + (s_pens[0].gx - src->x) * t;
        float ly = src->y + (s_pens[0].gy - src->y) * t;
        float a = (0.35F + 0.4F * pulse) * (0.4F + 0.6F * t); /* brighter toward pen */
        float ac[4] = {clampf(col[0] + 0.2F, 0.0F, 1.0F),
                       clampf(col[1] + 0.2F, 0.0F, 1.0F),
                       clampf(col[2] + 0.2F, 0.0F, 1.0F), a};
        float sz = 10.0F + 8.0F * t;
        emit_sprite(CORRAL_RGN_SPARK, lx, ly, sz, sz, pack_rgba(ac));
      }
      /* a bright flag head at the pen gate = "here". */
      float fc[4] = {col[0], col[1], col[2], 0.7F + 0.3F * pulse};
      emit_sprite(CORRAL_RGN_FLAG, s_pens[0].gx, s_pens[0].gy - 30.0F, 30.0F,
                  38.0F, pack_rgba(fc));
    }
  }

  /* particles (fade out) — tinted spark sprites */
  for (int i = 0; i < CORRAL_MAX_PARTICLES; ++i) {
    const Particle *p = &s_particles[i];
    if (p->life <= 0.0F) {
      continue;
    }
    float a = clampf(p->life / p->max_life, 0.0F, 1.0F);
    float col[4] = {p->color[0], p->color[1], p->color[2], a};
    float sz = 18.0F * a + 4.0F;
    emit_sprite(CORRAL_RGN_SPARK, p->x, p->y, sz, sz, pack_rgba(col));
  }

  /* ---- HUD: score + per-color GOAL, fontless (crisp pips + bars) ---- */
  {
    const float strip_h = h * 0.085F;
    /* dark translucent top strip so the HUD reads over any field color and
     * gives the composition a clear header band (focal hierarchy). */
    emit_sprite(CORRAL_RGN_PIP, w * 0.5F, strip_h * 0.5F, w, strip_h,
                0x99201810U /* AABBGGRR: dark, semi-transparent */);

    const float cy = strip_h * 0.5F;

    /* SCORE (left): a soft warm bar + bright pips counting total captured.
     * The bar grows with score so the at-a-glance "progress" reads even when
     * pips wrap. */
    {
      const float dot = 11.0F;
      const float gap = dot * 1.25F;
      /* leave room on the left for the "Score N" TEXT label (drawn in the text
       * pass); the pips become a subtle progress bar to the right of the word. */
      float x0 = 110.0F;
      int max_pips = 16;
      int shown = s_score < max_pips ? s_score : max_pips;
      for (int i = 0; i < shown; ++i) {
        float px = x0 + dot * 0.5F + (float)i * gap;
        emit_sprite(CORRAL_RGN_PIP, px, cy, dot, dot, 0xCCF0F0F0U /* white */);
      }
    }

    /* PER-COLOR GOAL (right): for each ACTIVE color, show "remaining loose" as
     * that color's pips. Empty = wave goal met for that color. The wave goal at
     * a glance: clear all loose critters of every active color. */
    {
      const float dot = 14.0F;
      const float gap = dot * 1.15F;
      float xr = w - 18.0F;
      for (int p = s_color_count - 1; p >= 0; --p) {
        const float *col = CORRAL_COLORS[p];
        float ccol[4] = {col[0], col[1], col[2], 1.0F};
        int rem = corral_color_remaining(p);
        /* slots = how many of this color spawned this wave (cap the row). */
        int total = 0;
        for (int i = 0; i < s_critter_count; ++i) {
          if (s_critters[i].color == (uint8_t)p) {
            ++total;
          }
        }
        if (total < 1) {
          total = 1;
        }
        if (total > 8) {
          total = 8; /* cap row width; rem still bright up to here */
        }
        for (int i = 0; i < total; ++i) {
          float px = xr - dot * 0.5F - (float)i * gap;
          bool loose = i < rem;
          uint32_t cp =
              loose ? pack_rgba(ccol)
                    : (pack_rgba((float[4]){col[0] * 0.35F, col[1] * 0.35F,
                                            col[2] * 0.35F, 0.85F}));
          emit_sprite(CORRAL_RGN_PIP, px, cy, dot, dot, cp);
        }
        xr -= (float)total * gap + dot * 0.7F;
      }
    }
  }

  /* per-pen mini counter: a small color pip + remaining count next to each pen
   * gate so "how many of THIS color are still loose" reads right where the
   * player is aiming. Placed just outside the gate, along its open face. */
  for (int p = 0; p < s_color_count; ++p) {
    const Pen *pen = &s_pens[p];
    const float *col = CORRAL_COLORS[pen->color];
    int rem = corral_color_remaining(pen->color);
    float ccol[4] = {col[0], col[1], col[2], 1.0F};
    const float dot = 11.0F;
    const float gap = dot * 1.15F;
    float row_w = (float)((rem < 12 ? rem : 12)) * gap;
    /* center the row on the gate, biased a touch inward toward the field. */
    float bx = pen->gx + pen->gdx * 26.0F;
    float by = pen->gy + pen->gdy * 26.0F;
    float sx0 = bx - row_w * 0.5F + dot * 0.5F;
    for (int i = 0; i < rem && i < 12; ++i) {
      emit_sprite(CORRAL_RGN_PIP, sx0 + (float)i * gap, by, dot, dot,
                  pack_rgba(ccol));
    }
  }

  /* WAVE NUMBER (top-center): a clear, readable fontless count. A bright pip
   * per wave up to 9; from wave 10 a gold "ten" pip leads each group of ten so
   * deep endless runs still read at a glance. */
  {
    const float cy = h * 0.085F * 0.5F;
    int tens = s_wave / 10;
    int ones = s_wave % 10;
    float x = w * 0.5F - (float)(tens + ones) * 7.0F;
    for (int i = 0; i < tens; ++i) {
      emit_sprite(CORRAL_RGN_PIP, x, cy, 13.0F, 13.0F,
                  0xFF30D0FFU /* gold-ish = 10 */);
      x += 16.0F;
    }
    for (int i = 0; i < ones; ++i) {
      emit_sprite(CORRAL_RGN_PIP, x, cy, 10.0F, 10.0F, 0xFFE0D8C8U);
      x += 13.0F;
    }
  }

  /* clickable/tappable RESTART marker (top-right): a dark chip with a looping
   * ring glyph (reuse the lure ring) — fontless "start over", calm, always
   * available. Rect matches restart_marker_rect() so the tap hit-test lines up;
   * it is finger-sized on a narrow/portrait screen. */
  {
    float rw;
    float rh;
    float rx;
    float ry;
    restart_marker_rect(w, h, &rx, &ry, &rw, &rh);
    emit_sprite(CORRAL_RGN_PIP, rx + rw * 0.5F, ry + rh * 0.5F, rw, rh,
                0xCC2A2218U);
    float gsz = rh * 0.7F;
    emit_sprite(CORRAL_RGN_LURE, rx + rw * 0.5F, ry + rh * 0.5F, gsz, gsz,
                0xFFD8E0E8U);
  }

  /* ---- LIGHT META: acquired-upgrade row (fontless build readout) ----
   * A small row of the upgrades you've picked, just under the HUD strip on the
   * left, each as its tinted icon with tiny level pips beneath. Lets the player
   * see their growing build at a glance (the "one more wave" pull). */
  {
    const float strip_h = h * 0.085F;
    float iy = strip_h + 22.0F;
    float ix = 22.0F;
    const float isz = 30.0F;          /* icon tile size */
    const float istep = isz + 28.0F;  /* matches the text label pitch below */
    for (int u = 0; u < CORRAL_UPG_COUNT; ++u) {
      if (s_upgrades[u] == 0) {
        continue;
      }
      /* dark rounded chip behind the icon for contrast over the field. */
      emit_sprite(CORRAL_RGN_PIP, ix + isz * 0.5F, iy + isz * 0.5F, isz + 8.0F,
                  isz + 8.0F, 0xAA1A1410U);
      emit_sprite(upgrade_icon_region((corral_upgrade_t)u), ix + isz * 0.5F,
                  iy + isz * 0.5F, isz, isz, 0xFFF2F2F2U);
      /* level pips beneath (1..MAX) so stacking reads without text. */
      const float dot = 6.0F;
      const float gap = dot + 3.0F;
      float px0 = ix + isz * 0.5F - (float)(s_upgrades[u] - 1) * gap * 0.5F;
      for (int l = 0; l < s_upgrades[u]; ++l) {
        emit_sprite(CORRAL_RGN_PIP, px0 + (float)l * gap, iy + isz + 8.0F, dot,
                    dot, 0xFFFFD040U /* warm gold pip */);
      }
      ix += istep;
    }
  }

  /* wave-clear / win celebration: bright pulsing gold sparks along the frame */
  if (s_cleared_flash > 0.0F) {
    float pulse = 0.5F + 0.5F * sinf(s_cleared_flash * 24.0F);
    uint32_t gold = pack_white_alpha(0.6F + 0.4F * pulse);
    float sz = 26.0F + 18.0F * pulse;
    int n = 10;
    for (int i = 0; i < n; ++i) {
      float t = (float)i / (float)(n - 1);
      emit_sprite(CORRAL_RGN_LURE, t * w, 18.0F, sz, sz, gold);
      emit_sprite(CORRAL_RGN_LURE, t * w, h - 18.0F, sz, sz, gold);
    }
  }

  /* ---- Phase beats: fontless overlays (panel + pips/sprites) ---- */
  corral_draw_phase_overlay(w, h);

  nt_sprite_renderer_flush();
}

/* ---- TEXT pass (the keystone): readable copy over the sprite field ----
 * Makes the build TEACHABLE: a clear level/difficulty readout, plain-language
 * upgrade cards + acquired-build labels, a real first-run FTUE, and a "New
 * color!" difficulty callout. Calm, casual, legible — short strings, soft
 * shadow for contrast, the actual color names for the FTUE. */
static void corral_draw_text(float w, float h) {
  nt_text_renderer_set_material(s_text_material);
  nt_text_renderer_set_font(s_font);

  const float white[4] = {0.98F, 0.98F, 0.96F, 1.0F};
  const float warm[4] = {1.0F, 0.86F, 0.46F, 1.0F};

  /* ---- HUD: "Level N" prominently (top-center), plus a small Score label ---- */
  {
    char buf[48];
    (void)snprintf(buf, sizeof(buf), "Level %d", s_wave);
    draw_text_shadow(buf, w * 0.5F, 6.0F, 26.0F, warm, TEXT_ALIGN_CENTER);

    (void)snprintf(buf, sizeof(buf), "Score %d", s_score);
    draw_text_shadow(buf, 16.0F, 8.0F, 20.0F, white, TEXT_ALIGN_LEFT);
  }

  /* ---- Acquired-upgrade HUD row: a small NAME label under each picked icon so
   * the player can read their growing build (not just guess from icons). ---- */
  {
    const float strip_h = h * 0.085F;
    float iy = strip_h + 22.0F;
    float ix = 22.0F;
    const float isz = 30.0F;
    const float istep = isz + 28.0F; /* room for the compact label under each icon */
    for (int u = 0; u < CORRAL_UPG_COUNT; ++u) {
      if (s_upgrades[u] == 0) {
        continue;
      }
      /* compact label just below the level pips, centered under the icon. */
      draw_text_shadow(upgrade_short((corral_upgrade_t)u), ix + isz * 0.5F,
                       iy + isz + 16.0F, 12.0F, white, TEXT_ALIGN_CENTER);
      ix += istep;
    }
  }

  /* ---- "New color!" difficulty callout (text banner) ---- */
  if (s_new_color_timer > 0.0F && s_new_color_index >= 0 &&
      s_new_color_index < CORRAL_MAX_COLORS) {
    char buf[48];
    (void)snprintf(buf, sizeof(buf), "New color: %s!",
                   CORRAL_COLOR_NAMES[s_new_color_index]);
    const float *cc = CORRAL_COLORS[s_new_color_index];
    float col[4] = {clampf(cc[0] + 0.18F, 0.0F, 1.0F),
                    clampf(cc[1] + 0.18F, 0.0F, 1.0F),
                    clampf(cc[2] + 0.18F, 0.0F, 1.0F), 1.0F};
    draw_text_shadow(buf, w * 0.5F, h * 0.16F, 30.0F, col, TEXT_ALIGN_CENTER);
  }

  /* ---- FTUE beats (first run only): one short line + the actual first color.
   * The motion/arrow cues are sprite-side; this is the readable instruction.
   * Only while PLAYING — it's a herding hint, irrelevant over the upgrade
   * cards / beats (and in portrait it sits where the cards are). */
  if (s_ftue_active && s_phase == CORRAL_PHASE_PLAYING) {
    const char *line = NULL;
    char buf[80];
    if (s_ftue_beat == 0) {
      /* device-neutral: a drag works with a mouse or a finger. */
      line = "Drag to herd the critters";
    } else if (s_ftue_beat == 1) {
      const char *cn = CORRAL_COLOR_NAMES[0]; /* the actual first color */
      (void)snprintf(buf, sizeof(buf), "Bring %s critters into the %s pen", cn,
                     cn);
      line = buf;
    } else {
      line = "Pen them all to finish the level!";
    }
    /* a gentle fade-in over the first ~0.4s of each beat so it feels calm. */
    float a = clampf(s_ftue_beat_age / 0.4F, 0.0F, 1.0F);
    float tip[4] = {1.0F, 0.97F, 0.88F, a};
    /* sits above the bottom pen band in portrait; fit to width so the longer
     * FTUE lines never run off a narrow screen. */
    float ftue_y = (h > w) ? h * 0.46F : h - 56.0F;
    draw_text_shadow_fit(line, w * 0.5F, ftue_y, 22.0F, w * 0.92F, tip,
                         TEXT_ALIGN_CENTER);
  }

  /* ---- Phase-beat copy (title / cleared / win / upgrade cards) ---- */
  if (s_phase == CORRAL_PHASE_TITLE) {
    float maxw = w * 0.86F; /* keep copy inside the title plate on any ratio */
    draw_text_shadow_fit("CRITTER CORRAL", w * 0.5F, h * 0.26F, 44.0F, maxw,
                         warm, TEXT_ALIGN_CENTER);
    draw_text_shadow_fit("Herd each color into its matching pen", w * 0.5F,
                         h * 0.26F + 54.0F, 20.0F, maxw, white,
                         TEXT_ALIGN_CENTER);
    /* "Tap to start" reads on phone and desktop alike (a click is a tap). */
    draw_text_shadow("Tap to start", w * 0.5F, h * 0.72F, 24.0F, white,
                     TEXT_ALIGN_CENTER);
  } else if (s_phase == CORRAL_PHASE_WAVE_CLEARED) {
    char buf[48];
    (void)snprintf(buf, sizeof(buf), "Level %d cleared!", s_wave);
    draw_text_shadow(buf, w * 0.5F, h * 0.5F - 14.0F, 30.0F, warm,
                     TEXT_ALIGN_CENTER);
  } else if (s_phase == CORRAL_PHASE_WIN) {
    draw_text_shadow("You did it!", w * 0.5F, h * 0.5F - 70.0F, 40.0F, warm,
                     TEXT_ALIGN_CENTER);
    draw_text_shadow_fit("Keep going — the pasture never ends", w * 0.5F,
                         h * 0.5F + 48.0F, 20.0F, w * 0.9F, white,
                         TEXT_ALIGN_CENTER);
  } else if (s_phase == CORRAL_PHASE_UPGRADE_CHOICE) {
    const bool portrait = h > w;
    /* header */
    draw_text_shadow("Choose an upgrade", w * 0.5F, h * 0.06F,
                     portrait ? 26.0F : 30.0F, warm, TEXT_ALIGN_CENTER);
    int n = (s_offer_count > 0) ? s_offer_count : CORRAL_UPGRADE_OFFER;
    for (int i = 0; i < n; ++i) {
      corral_upgrade_t u = s_offer[i];
      float cx;
      float cy;
      float cw;
      float ch;
      upgrade_card_rect(i, w, h, &cx, &cy, &cw, &ch);
      float bob = 4.0F * sinf((float)g_nt_app.frame * 0.05F + (float)i * 1.3F);
      float top = cy + bob;
      /* dark ink on the light card so the copy is crisp. */
      const float ink[4] = {0.12F, 0.13F, 0.16F, 1.0F};
      const float ink2[4] = {0.24F, 0.26F, 0.30F, 1.0F};
      const char *desc = upgrade_desc(u);
      const char *nl = strchr(desc, '\n');
      char lvl[40];
      (void)snprintf(lvl, sizeof(lvl), "Level %d / %d", upgrade_card_pips(u),
                     CORRAL_UPG_MAX_LEVEL);
      char key[8];
      (void)snprintf(key, sizeof(key), "[%d]", i + 1);

      if (portrait) {
        /* WIDE/SHORT card: text column lives BETWEEN the left icon and the
         * right pips. Title on top, the two-line desc under it, level + key at
         * the foot — all in the middle band, vertically centered on the card. */
        float tcx = cx + cw * 0.5F;       /* card centre x for the title row */
        float mid = top + ch * 0.5F;
        draw_text(upgrade_title(u), tcx, top + ch * 0.10F, 18.0F, ink,
                  TEXT_ALIGN_CENTER);
        if (nl != NULL) {
          char l1[40];
          size_t l1len = (size_t)(nl - desc);
          if (l1len >= sizeof(l1)) {
            l1len = sizeof(l1) - 1;
          }
          memcpy(l1, desc, l1len);
          l1[l1len] = '\0';
          draw_text(l1, tcx, mid - 6.0F, 12.0F, ink2, TEXT_ALIGN_CENTER);
          draw_text(nl + 1, tcx, mid + 10.0F, 12.0F, ink2, TEXT_ALIGN_CENTER);
        } else {
          draw_text(desc, tcx, mid, 12.0F, ink2, TEXT_ALIGN_CENTER);
        }
        draw_text(lvl, tcx, top + ch * 0.78F, 12.0F, ink2, TEXT_ALIGN_CENTER);
        draw_text(key, tcx, top + ch * 0.78F + 16.0F, 13.0F, ink,
                  TEXT_ALIGN_CENTER);
      } else {
        float mcx = cx + cw * 0.5F;
        /* TITLE near the top of the card. */
        draw_text(upgrade_title(u), mcx, top + ch * 0.06F, 20.0F, ink,
                  TEXT_ALIGN_CENTER);
        /* DESCRIPTION (two short lines) just under the icon. */
        if (nl != NULL) {
          char l1[40];
          size_t l1len = (size_t)(nl - desc);
          if (l1len >= sizeof(l1)) {
            l1len = sizeof(l1) - 1;
          }
          memcpy(l1, desc, l1len);
          l1[l1len] = '\0';
          draw_text(l1, mcx, top + ch * 0.56F, 13.0F, ink2, TEXT_ALIGN_CENTER);
          draw_text(nl + 1, mcx, top + ch * 0.56F + 17.0F, 13.0F, ink2,
                    TEXT_ALIGN_CENTER);
        } else {
          draw_text(desc, mcx, top + ch * 0.56F, 13.0F, ink2,
                    TEXT_ALIGN_CENTER);
        }
        /* current level / what this pick grants, as words under the pips. */
        draw_text(lvl, mcx, top + ch * 0.80F, 13.0F, ink2, TEXT_ALIGN_CENTER);
        /* the number key hint, as a real "[1]" so the pick is obvious. */
        draw_text(key, mcx, top + ch * 0.90F, 14.0F, ink, TEXT_ALIGN_CENTER);
      }
    }
  }

  nt_text_renderer_flush();
}

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static void corral_do_capture(void) {
  if (!s_capture_pending) {
    return;
  }
  s_capture_pending = false;
  int fbw = (int)g_nt_window.fb_width;
  int fbh = (int)g_nt_window.fb_height;
  if (fbw <= 0 || fbh <= 0) {
    return;
  }
  size_t row = (size_t)fbw * 3U;
  unsigned char *buf = (unsigned char *)malloc(row * (size_t)fbh);
  if (buf == NULL) {
    return;
  }
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, fbw, fbh, GL_RGB, GL_UNSIGNED_BYTE, buf);

  FILE *f = fopen(s_capture_path, "wb");
  if (f != NULL) {
    (void)fprintf(f, "P6\n%d %d\n255\n", fbw, fbh);
    /* GL origin is bottom-left; flip rows so the PPM reads top-down. */
    for (int y = fbh - 1; y >= 0; --y) {
      (void)fwrite(buf + (size_t)y * row, 1, row, f);
    }
    (void)fclose(f);
  }
  free(buf);
}
#endif

/* ---- DevAPI ---- */

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static const char *param_string(const cJSON *params, const char *key,
                                const char *fallback) {
  if (params == NULL) {
    return fallback;
  }
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, key);
  if (cJSON_IsString(item) && item->valuestring != NULL) {
    return item->valuestring;
  }
  return fallback;
}

static const char *phase_name(corral_phase_t phase) {
  switch (phase) {
  case CORRAL_PHASE_TITLE:
    return "title";
  case CORRAL_PHASE_PLAYING:
    return "playing";
  case CORRAL_PHASE_WAVE_CLEARED:
    return "wave_cleared";
  case CORRAL_PHASE_UPGRADE_CHOICE:
    return "upgrade_choice";
  case CORRAL_PHASE_WIN:
    return "win";
  }
  return "unknown";
}

static const char *upgrade_name(corral_upgrade_t u) {
  switch (u) {
  case CORRAL_UPG_RADIUS:
    return "lure_radius";
  case CORRAL_UPG_PULL:
    return "lure_pull";
  case CORRAL_UPG_SECOND_LURE:
    return "second_lure";
  case CORRAL_UPG_GATE:
    return "wider_gates";
  case CORRAL_UPG_CALM:
    return "calmer_critters";
  case CORRAL_UPG_CHAIN:
    return "longer_chain";
  case CORRAL_UPG_COUNT:
    break;
  }
  return "unknown";
}

static cJSON *state_json(void) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "runtime", "critter_corral");
  cJSON_AddStringToObject(root, "phase", phase_name(s_phase));
  cJSON_AddNumberToObject(root, "wave", s_wave);
  cJSON_AddNumberToObject(root, "color_count", s_color_count);
  cJSON_AddNumberToObject(root, "critter_count", s_critter_count);
  cJSON_AddNumberToObject(root, "loose", corral_loose_count());
  cJSON_AddNumberToObject(root, "score", s_score);
  cJSON_AddBoolToObject(root, "win_milestone_shown", s_win_shown);
  cJSON_AddBoolToObject(root, "sprites_ready", s_sprites_ready);
  cJSON_AddBoolToObject(root, "text_ready", s_text_ready);

  /* FTUE / tutorial state so a playtest can prove the first-run onboarding. */
  cJSON *ftue = cJSON_AddObjectToObject(root, "ftue");
  cJSON_AddBoolToObject(ftue, "active", s_ftue_active);
  cJSON_AddBoolToObject(ftue, "seen", s_ftue_seen);
  cJSON_AddNumberToObject(ftue, "beat", s_ftue_beat);
  /* difficulty callout (the "New color!" banner). */
  cJSON_AddBoolToObject(root, "new_color_callout", s_new_color_timer > 0.0F);

  cJSON *remaining = cJSON_AddArrayToObject(root, "remaining_by_color");
  cJSON *penned = cJSON_AddArrayToObject(root, "penned_by_color");
  for (int i = 0; i < s_color_count; ++i) {
    cJSON_AddItemToArray(remaining,
                         cJSON_CreateNumber(corral_color_remaining(i)));
    cJSON_AddItemToArray(penned, cJSON_CreateNumber(s_pens[i].parked));
  }

  /* Behavior mix of LOOSE critters — lets a playtest confirm the progressive
   * introduction (normal-only early -> skittish -> stubborn -> follower). */
  cJSON *behaviors = cJSON_AddObjectToObject(root, "loose_by_behavior");
  cJSON_AddNumberToObject(behaviors, "normal",
                          corral_behavior_loose_count(CORRAL_BEHAVIOR_NORMAL));
  cJSON_AddNumberToObject(behaviors, "skittish",
                          corral_behavior_loose_count(CORRAL_BEHAVIOR_SKITTISH));
  cJSON_AddNumberToObject(behaviors, "stubborn",
                          corral_behavior_loose_count(CORRAL_BEHAVIOR_STUBBORN));
  cJSON_AddNumberToObject(behaviors, "follower",
                          corral_behavior_loose_count(CORRAL_BEHAVIOR_FOLLOWER));

  cJSON_AddBoolToObject(root, "wave_cleared",
                        corral_loose_count() == 0 && s_critter_count > 0);

  /* ---- LIGHT META: acquired upgrades (build) + effective derived values ---- */
  cJSON *upgrades = cJSON_AddObjectToObject(root, "upgrades");
  for (int u = 0; u < CORRAL_UPG_COUNT; ++u) {
    cJSON_AddNumberToObject(upgrades, upgrade_name((corral_upgrade_t)u),
                            s_upgrades[u]);
  }
  cJSON_AddNumberToObject(root, "upgrades_acquired", s_acquired_total);

  cJSON *eff = cJSON_AddObjectToObject(root, "effective");
  cJSON_AddNumberToObject(eff, "lure_radius", (double)s_lure_radius);
  cJSON_AddNumberToObject(eff, "pull_mult", (double)corral_pull_mult());
  cJSON_AddNumberToObject(eff, "gate_bonus", (double)corral_gate_bonus());
  cJSON_AddNumberToObject(eff, "calm_mult", (double)corral_calm_mult());
  cJSON_AddNumberToObject(eff, "chain_time", (double)corral_chain_time());
  cJSON_AddBoolToObject(eff, "second_lure", corral_second_lure_active());

  /* ---- AUDIO PROOF: an automated playtest can't HEAR the speaker, so surface
   * the audio engine's play counters + last cue here. A rising total + the
   * per-event cue counts PROVE each event actually fired a sound. ---- */
  {
    GameAudioStatus as = game_audio_status();
    cJSON *audio = cJSON_AddObjectToObject(root, "audio");
    cJSON_AddBoolToObject(audio, "implemented", as.implemented);
    cJSON_AddBoolToObject(audio, "initialized", as.initialized);
    cJSON_AddBoolToObject(audio, "device_enabled", as.device_enabled);
    cJSON_AddStringToObject(audio, "backend",
                            as.backend ? as.backend : "unknown");
    cJSON_AddNumberToObject(audio, "total_play_count", as.total_play_count);
    cJSON_AddNumberToObject(audio, "last_cue", as.last_cue);
    cJSON_AddStringToObject(
        audio, "last_cue_name",
        (as.last_cue >= 0)
            ? game_audio_cue_name((GameAudioCue)as.last_cue)
            : "none");
    cJSON_AddNumberToObject(audio, "last_semitones", (double)as.last_semitones);
    cJSON *cues = cJSON_AddObjectToObject(audio, "cue_play_count");
    for (int ci = 0; ci < GAME_AUDIO_CUE_COUNT; ++ci) {
      cJSON_AddNumberToObject(cues, game_audio_cue_name((GameAudioCue)ci),
                              as.cue_play_count[ci]);
    }
  }

  /* the pending pick-1-of-3 offer (only meaningful in the upgrade_choice phase;
   * reported whenever an offer is staged so a playtest can read the options). */
  cJSON *pending = cJSON_AddArrayToObject(root, "pending_choice");
  if (s_phase == CORRAL_PHASE_UPGRADE_CHOICE) {
    for (int i = 0; i < s_offer_count; ++i) {
      cJSON *card = cJSON_CreateObject();
      cJSON_AddNumberToObject(card, "index", i);
      cJSON_AddStringToObject(card, "upgrade", upgrade_name(s_offer[i]));
      cJSON_AddNumberToObject(card, "current_level", s_upgrades[s_offer[i]]);
      cJSON_AddNumberToObject(card, "level_after", upgrade_card_pips(s_offer[i]));
      cJSON_AddItemToArray(pending, card);
    }
  }
  return root;
}

static bool ep_game_state(const cJSON *params, cJSON **result, char *error,
                          int error_cap, void *user) {
  (void)params;
  (void)error;
  (void)error_cap;
  (void)user;
  *result = state_json();
  return true;
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON **result,
                                   char *error, int error_cap, void *user) {
  (void)params;
  (void)error;
  (void)error_cap;
  (void)user;
  const float w =
      (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                : g_nt_window.height);
  corral_reset(w, h);
  *result = state_json();
  return true;
}

static bool ep_game_start(const cJSON *params, cJSON **result, char *error,
                          int error_cap, void *user) {
  (void)params;
  (void)error;
  (void)error_cap;
  (void)user;
  const float w =
      (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                : g_nt_window.height);
  corral_start(w, h);
  *result = state_json();
  return true;
}

/* Instantly clear/advance the current wave so an automated playtest can fast-
 * forward through waves. Captures every loose critter into its matching pen
 * (real score), then — matching real play — surfaces the LIGHT META pick-1-of-3
 * upgrade choice (if any upgrades remain unmaxed) so the playtest can
 * game.debug.pick_upgrade before the next wave. If the offer is empty (all
 * maxed) it advances straight to the next wave. The WIN milestone still fires
 * once at CORRAL_WIN_WAVE. From WAVE_CLEARED/UPGRADE_CHOICE/WIN it advances. */
static bool ep_game_debug_skip_wave(const cJSON *params, cJSON **result,
                                    char *error, int error_cap, void *user) {
  (void)params;
  (void)error;
  (void)error_cap;
  (void)user;
  const float w =
      (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                : g_nt_window.height);
  if (s_phase == CORRAL_PHASE_TITLE) {
    corral_start(w, h);
  }
  if (s_phase == CORRAL_PHASE_PLAYING) {
    for (int i = 0; i < s_critter_count; ++i) {
      Critter *c = &s_critters[i];
      if (c->alive && !c->parked && c->color < s_color_count) {
        capture_critter(c, &s_pens[c->color]);
      }
    }
    s_cleared_flash = 0.85F;
    if (s_wave >= CORRAL_WIN_WAVE && !s_win_shown) {
      s_win_shown = true; /* fire the soft milestone once, then offer/advance */
      game_audio_play(GAME_AUDIO_CUE_CORRAL_WIN);
    } else {
      game_audio_play(GAME_AUDIO_CUE_CORRAL_WAVE);
    }
    /* surface the upgrade choice (so the playtest picks), else advance. */
    corral_make_offer();
    if (s_offer_count > 0) {
      s_phase = CORRAL_PHASE_UPGRADE_CHOICE;
    } else {
      corral_next_wave(w, h);
    }
  } else if (s_phase == CORRAL_PHASE_UPGRADE_CHOICE) {
    /* a skip during the choice = take the first offered card, then advance. */
    if (s_offer_count > 0) {
      (void)corral_pick_upgrade(0, w, h);
    } else {
      corral_next_wave(w, h);
    }
  } else if (s_phase == CORRAL_PHASE_WAVE_CLEARED ||
             s_phase == CORRAL_PHASE_WIN) {
    corral_next_wave(w, h);
  }
  *result = state_json();
  return true;
}

/* LIGHT META playtest hook: pick the pending offered upgrade by index (0..2)
 * without a real click, so an automated playtest can choose between waves. Only
 * valid in the upgrade_choice phase; out-of-range / no-pending returns an error.
 */
static bool ep_game_debug_pick_upgrade(const cJSON *params, cJSON **result,
                                       char *error, int error_cap, void *user) {
  (void)user;
  if (s_phase != CORRAL_PHASE_UPGRADE_CHOICE || s_offer_count <= 0) {
    if (error != NULL && error_cap > 0) {
      (void)snprintf(error, (size_t)error_cap,
                     "no pending upgrade choice (phase=%s)", phase_name(s_phase));
    }
    return false;
  }
  int index = 0;
  const cJSON *item =
      (params != NULL) ? cJSON_GetObjectItemCaseSensitive(params, "index")
                       : NULL;
  if (cJSON_IsNumber(item)) {
    index = (int)item->valuedouble;
  }
  if (index < 0 || index >= s_offer_count) {
    if (error != NULL && error_cap > 0) {
      (void)snprintf(error, (size_t)error_cap,
                     "index %d out of range (offer_count=%d)", index,
                     s_offer_count);
    }
    return false;
  }
  const float w =
      (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                : g_nt_window.height);
  (void)corral_pick_upgrade(index, w, h);
  *result = state_json();
  return true;
}

static bool ep_game_capture_framebuffer(const cJSON *params, cJSON **result,
                                        char *error, int error_cap,
                                        void *user) {
  (void)user;
#if !defined(NT_PLATFORM_WEB)
  const char *output = param_string(params, "output", NULL);
  if (output == NULL || output[0] == '\0') {
    if (error != NULL && error_cap > 0) {
      (void)snprintf(error, (size_t)error_cap, "missing params.output");
    }
    return false;
  }
  (void)snprintf(s_capture_path, sizeof(s_capture_path), "%s", output);
  s_capture_path[sizeof(s_capture_path) - 1] = '\0';
  s_capture_pending = true;
  cJSON *root = cJSON_CreateObject();
  cJSON_AddBoolToObject(root, "ok", true);
  cJSON_AddStringToObject(root, "output", s_capture_path);
  cJSON_AddNumberToObject(root, "width", (double)g_nt_window.fb_width);
  cJSON_AddNumberToObject(root, "height", (double)g_nt_window.fb_height);
  *result = root;
  return true;
#else
  (void)params;
  (void)result;
  if (error != NULL && error_cap > 0) {
    (void)snprintf(error, (size_t)error_cap,
                   "framebuffer capture is native-only");
  }
  return false;
#endif
}

static void register_game_endpoints(void) {
  nt_devapi_register_builtins();
  game_state_register_devapi();
  nt_devapi_register("game.state", ep_game_state, NULL);
  nt_devapi_register("game.start", ep_game_start, NULL);
  nt_devapi_register("game.debug.skip_wave", ep_game_debug_skip_wave, NULL);
  nt_devapi_register("game.debug.pick_upgrade", ep_game_debug_pick_upgrade,
                     NULL);
  nt_devapi_register("game.reset_playtest", ep_game_reset_playtest, NULL);
  nt_devapi_register("game.capture.framebuffer", ep_game_capture_framebuffer,
                     NULL);
}

static void register_ui_devapi(float w, float h) {
  nt_devapi_set_frame(g_nt_app.frame);
  nt_devapi_set_view((float)g_nt_window.fb_width, (float)g_nt_window.fb_height,
                     w, h);
  nt_devapi_clear_ui_elements();
  (void)nt_devapi_register_ui_node("root", "", "screen", "Critter Corral",
                                   "Open pasture", 0.0F, 0.0F, w, h, true,
                                   true);
  for (int i = 0; i < s_color_count; ++i) {
    char id[16];
    (void)snprintf(id, sizeof(id), "pen.%d", i);
    (void)nt_devapi_register_ui_node(id, "root", "pen", "Pen", "", s_pens[i].x,
                                     s_pens[i].y, s_pens[i].w, s_pens[i].h,
                                     true, true);
  }
  /* clickable restart marker (top-right HUD corner). */
  {
    float rrx;
    float rry;
    float rrw;
    float rrh;
    restart_marker_rect(w, h, &rrx, &rry, &rrw, &rrh);
    (void)nt_devapi_register_ui_node("restart", "root", "button", "Restart", "",
                                     rrx, rry, rrw, rrh, true, true);
  }
  /* LIGHT META: expose the pending upgrade cards as clickable nodes so the
   * playtest/ui tooling sees the pick-1-of-3 (also pickable via
   * game.debug.pick_upgrade). */
  if (s_phase == CORRAL_PHASE_UPGRADE_CHOICE) {
    for (int i = 0; i < s_offer_count; ++i) {
      float cx;
      float cy;
      float cw;
      float ch;
      upgrade_card_rect(i, w, h, &cx, &cy, &cw, &ch);
      char id[24];
      (void)snprintf(id, sizeof(id), "upgrade.%d", i);
      (void)nt_devapi_register_ui_node(id, "root", "button",
                                       upgrade_name(s_offer[i]), "", cx, cy, cw,
                                       ch, true, true);
    }
  }
}
#endif

static void parse_args(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--devapi") == 0) {
      s_devapi_enabled = true;
      if (i + 1 < argc && argv[i + 1][0] != '-') {
        s_devapi_port = (uint16_t)strtoul(argv[++i], NULL, 10);
      }
    } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
      int width = 0;
      int height = 0;
      if (sscanf(argv[++i], "%dx%d", &width, &height) == 2 && width > 0 &&
          height > 0) {
        s_window_width = width;
        s_window_height = height;
      }
    }
  }
}

/* Clickable restart marker rect (top-right HUD corner). Touch-friendly: a
 * finger-sized chip that grows on a narrow (portrait/phone) screen so it is an
 * easy tap target, and stays clear of the top HUD band. */
static void restart_marker_rect(float w, float h, float *rx, float *ry,
                                float *rw, float *rh) {
  const float screen_min = (w < h) ? w : h;
  /* ~12% of the short edge, clamped to a comfortable finger size. */
  float sz = clampf(screen_min * 0.12F, 40.0F, 72.0F);
  *rw = sz;
  *rh = sz * 0.80F;
  *rx = w - *rw - 8.0F;
  *ry = 8.0F;
}

/* ---- Unified pointer (touch + mouse + pen) tap helpers ----
 * The engine routes touch, mouse, and pen through g_nt_input.pointers[]. The
 * mouse-only helpers (nt_input_mouse_is_*) match ONLY a NT_POINTER_MOUSE slot,
 * so on a phone a finger TAP would never register as a click. These helpers
 * treat a press/down on ANY active pointer as a tap, so taps work on touch. */

/* The "primary" pointer: the first active pointer slot (pointers[0] in the
 * common single-finger / mouse case), used for the lure position + tap coords. */
static const nt_pointer_t *primary_pointer(void) {
  for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
    if (g_nt_input.pointers[i].active) {
      return &g_nt_input.pointers[i];
    }
  }
  return NULL;
}

/* True on the frame ANY pointer's left/primary button goes down (mouse click,
 * finger tap, or pen press). Writes the tap position into *px,*py. */
static bool pointer_tap_pressed(float *px, float *py) {
  for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
    const nt_pointer_t *p = &g_nt_input.pointers[i];
    if (p->active && p->buttons[NT_BUTTON_LEFT].is_pressed) {
      if (px != NULL) {
        *px = p->x;
      }
      if (py != NULL) {
        *py = p->y;
      }
      return true;
    }
  }
  return false;
}

/* Input handling runs on BOTH native and web: the keyboard paths are no-ops on
 * a phone (no keys) but harmless, and the pointer-tap paths drive touch. */
static void handle_input(void) {
  const float w =
      (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                : g_nt_window.height);

  float tap_x = 0.0F;
  float tap_y = 0.0F;
  const bool tapped = pointer_tap_pressed(&tap_x, &tap_y);

  /* R = restart any time (back to a fresh run / title — calm, no penalty). */
  if (nt_input_key_is_pressed(NT_KEY_R)) {
    corral_reset(w, h);
    return;
  }
  /* clickable/tappable restart marker takes priority over a TITLE start-tap. */
  if (tapped) {
    float rx;
    float ry;
    float rw;
    float rh;
    restart_marker_rect(w, h, &rx, &ry, &rw, &rh);
    if (tap_x >= rx && tap_x <= rx + rw && tap_y >= ry && tap_y <= ry + rh) {
      corral_reset(w, h);
      return;
    }
  }
  /* TITLE: press/tap/click to start (any of tap / space / enter). */
  if (s_phase == CORRAL_PHASE_TITLE &&
      (tapped || nt_input_key_is_pressed(NT_KEY_SPACE) ||
       nt_input_key_is_pressed(NT_KEY_ENTER))) {
    corral_start(w, h);
  }

  /* LIGHT META: pick-1-of-3 upgrade choice — keyboard 1/2/3 or tap a card. */
  if (s_phase == CORRAL_PHASE_UPGRADE_CHOICE && s_offer_count > 0) {
    int pick = -1;
    if (nt_input_key_is_pressed(NT_KEY_1)) {
      pick = 0;
    } else if (nt_input_key_is_pressed(NT_KEY_2)) {
      pick = 1;
    } else if (nt_input_key_is_pressed(NT_KEY_3)) {
      pick = 2;
    } else if (tapped) {
      for (int i = 0; i < s_offer_count; ++i) {
        float cx;
        float cy;
        float cw;
        float ch;
        upgrade_card_rect(i, w, h, &cx, &cy, &cw, &ch);
        if (tap_x >= cx && tap_x <= cx + cw && tap_y >= cy &&
            tap_y <= cy + ch) {
          pick = i;
          break;
        }
      }
    }
    if (pick >= 0 && pick < s_offer_count) {
      corral_pick_upgrade(pick, w, h);
    }
  }
}

/* Resolve atlas region indices + source sizes once the atlas is READY. */
static void resolve_atlas_regions(void) {
  if (s_atlas_resolved || !nt_resource_is_ready(s_atlas_handle)) {
    return;
  }
  /* Order MUST match corral_region_t exactly (index-aligned with s_region_*). */
  static const nt_hash64_t names[CORRAL_RGN_COUNT] = {
      ASSET_ATLAS_REGION_CORRAL_CRITTER_PNG,
      ASSET_ATLAS_REGION_CORRAL_CRITTER_A_PNG,
      ASSET_ATLAS_REGION_CORRAL_CRITTER_B_PNG,
      ASSET_ATLAS_REGION_CORRAL_PEN_PNG,
      ASSET_ATLAS_REGION_CORRAL_FLAG_PNG,
      ASSET_ATLAS_REGION_CORRAL_GRASS_PNG,
      ASSET_ATLAS_REGION_CORRAL_LURE_PNG,
      ASSET_ATLAS_REGION_CORRAL_SPARK_PNG,
      ASSET_ATLAS_REGION_CORRAL_PIP_PNG,
      ASSET_ATLAS_REGION_CORRAL_CARD_PNG,
      ASSET_ATLAS_REGION_CORRAL_ICON_RADIUS_PNG,
      ASSET_ATLAS_REGION_CORRAL_ICON_PULL_PNG,
      ASSET_ATLAS_REGION_CORRAL_ICON_SECOND_LURE_PNG,
      ASSET_ATLAS_REGION_CORRAL_ICON_GATE_PNG,
      ASSET_ATLAS_REGION_CORRAL_ICON_CALM_PNG,
      ASSET_ATLAS_REGION_CORRAL_ICON_CHAIN_PNG,
  };
  for (int i = 0; i < CORRAL_RGN_COUNT; ++i) {
    uint32_t idx = nt_atlas_find_region(s_atlas_handle, names[i].value);
    if (idx == NT_ATLAS_INVALID_REGION) {
      return; /* atlas not fully merged yet — retry next frame */
    }
    const nt_texture_region_t *r = nt_atlas_get_region(s_atlas_handle, idx);
    s_region_idx[i] = idx;
    s_region_w[i] = r->source_w;
    s_region_h[i] = r->source_h;
  }
  s_atlas_resolved = true;
}

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

  /* Resource/material pumps for the sprite + text pipelines. */
  nt_resource_step();
  nt_material_step();
  nt_font_step(); /* resolve the font resource + upload glyph textures */
  resolve_atlas_regions();

  const float w =
      (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                : g_nt_window.height);

  /* Pens re-layout each frame so the field tracks the framebuffer size. */
  corral_layout_pens(w, h);
  handle_input();
  critter_update(g_nt_app.dt, w, h);

#if NT_DEVAPI_ENABLED
  if (s_devapi_enabled) {
    register_ui_devapi(w, h);
  }
#endif

#ifndef NT_PLATFORM_WEB
  if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
    nt_app_quit();
  }
#endif

  const nt_material_info_t *mat_info = nt_material_get_info(s_sprite_material);
  s_sprites_ready = s_atlas_resolved && mat_info != NULL && mat_info->ready;

  /* Text is usable once the slug_text material is ready AND the font resource
   * has resolved (units_per_em becomes non-zero). */
  const nt_material_info_t *tmat_info = nt_material_get_info(s_text_material);
  s_text_ready = tmat_info != NULL && tmat_info->ready &&
                 nt_font_get_metrics(s_font).units_per_em != 0;

  nt_gfx_begin_frame();
  if (g_nt_gfx.context_restored) {
    nt_shape_renderer_restore_gpu();
    nt_sprite_renderer_restore_gpu();
    nt_text_renderer_restore_gpu();
    nt_resource_invalidate(NT_ASSET_TEXTURE);
    nt_resource_invalidate(NT_ASSET_SHADER_CODE);
    nt_resource_invalidate(NT_ASSET_ATLAS);
    nt_resource_invalidate(NT_ASSET_FONT);
    s_sprites_ready = false;
    s_text_ready = false;
  }
  /* Pasture-green clear so the field reads even before the atlas resolves. */
  nt_gfx_begin_pass(&(nt_pass_desc_t){
      .clear_color = {0.49F, 0.77F, 0.40F, 1.0F}, .clear_depth = 1.0F});

  /* Globals UBO (Y-down ortho) for the sprite shader, bound at slot 0. */
  float vp[16];
  ortho_ydown(w, h, vp);
  nt_frame_uniforms_t uniforms = {0};
  memcpy(uniforms.view_proj, vp, sizeof(vp));
  uniforms.resolution[0] = w;
  uniforms.resolution[1] = h;
  uniforms.resolution[2] = (w > 0.0F) ? 1.0F / w : 0.0F;
  uniforms.resolution[3] = (h > 0.0F) ? 1.0F / h : 0.0F;
  uniforms.near_far[0] = -1.0F;
  uniforms.near_far[1] = 1.0F;

  if (s_sprites_ready) {
    nt_gfx_update_buffer(s_frame_ubo, &uniforms, sizeof(uniforms));
    nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);
    corral_draw_sprites(w, h);
  }

  /* TEXT pass (shares the Globals VP at slot 0 with the sprites above). Drawn
   * last so HUD/upgrade/FTUE/level copy reads on top of the field. */
  if (s_text_ready) {
    if (!s_sprites_ready) {
      /* sprites didn't bind the UBO this frame — bind it for text. */
      nt_gfx_update_buffer(s_frame_ubo, &uniforms, sizeof(uniforms));
      nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);
    }
    corral_draw_text(w, h);
  }

  nt_gfx_end_pass();

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
  corral_do_capture(); /* read the finished frame before swap */
#endif

  nt_gfx_end_frame();
  nt_window_swap_buffers();
}

int main(int argc, char **argv) {
  nt_engine_config_t config = {0};
  config.app_name = "Critter Corral";
  config.version = 1;
  if (nt_engine_init(&config) != NT_OK) {
    return 1;
  }

  parse_args(argc, argv);

  /* Pattern 7 init order: window → input → gfx + globals UBO register →
   * I/O (http/fs/hash/resource) → activators → atlas → component subsystems →
   * material/renderer modules → mount pack → run frame loop. */
  g_nt_window.title = "Critter Corral";
  g_nt_window.width = (uint32_t)s_window_width;
  g_nt_window.height = (uint32_t)s_window_height;
  nt_window_init();
  nt_input_init();

  nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
  gfx_desc.depth = true;
  nt_gfx_init(&gfx_desc);
  nt_gfx_register_global_block("Globals", 0);
  nt_shape_renderer_init(); /* kept for optional debug overlays */

  nt_http_init();
  nt_fs_init();
  nt_hash_init(&(nt_hash_desc_t){0});
  nt_resource_init(&(nt_resource_desc_t){0});
  nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture,
                            nt_gfx_deactivate_texture);
  nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader,
                            nt_gfx_deactivate_shader);
  nt_atlas_init();

  /* Immediate-mode sprite + text emit — no ECS components needed. Material slots
   * sized for the sprite material + the slug_text material. */
  nt_material_init(&(nt_material_desc_t){.max_materials = 8});
  nt_font_init(&(nt_font_desc_t){.max_fonts = 4});
  nt_sprite_renderer_desc_t sr_desc = nt_sprite_renderer_desc_defaults();
  nt_sprite_renderer_init(&sr_desc);
  nt_text_renderer_init(); /* the keystone: readable HUD/upgrade/FTUE/level copy */

  game_audio_init();

  s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
      .type = NT_BUFFER_UNIFORM,
      .usage = NT_USAGE_DYNAMIC,
      .size = sizeof(nt_frame_uniforms_t),
      .label = "corral_frame_uniforms",
  });

  /* Mount + load the sprite pack (relative to CWD = project root). */
  s_pack_id = nt_hash32_str("critter_corral");
  nt_resource_mount(s_pack_id, 100);
  nt_resource_load_auto(s_pack_id, CORRAL_PACK_PATH);

  s_vs_handle = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_VERT,
                                    NT_ASSET_SHADER_CODE);
  s_fs_handle = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_FRAG,
                                    NT_ASSET_SHADER_CODE);
  s_atlas_handle = nt_resource_request(ASSET_ATLAS_CORRAL, NT_ASSET_ATLAS);
  nt_resource_t atlas_tex =
      nt_resource_request(ASSET_TEXTURE_CORRAL_TEX0, NT_ASSET_TEXTURE);

  s_sprite_material = nt_material_create(&(nt_material_create_desc_t){
      .vs = s_vs_handle,
      .fs = s_fs_handle,
      .textures = {{.name = "u_texture", .resource = atlas_tex}},
      .texture_count = 1,
      .blend_mode = NT_BLEND_MODE_ALPHA,
      .depth_test = false,
      .depth_write = false,
      .cull_mode = NT_CULL_NONE,
      .label = "corral_sprite",
  });

  /* TEXT material (slug_text vs/fs from the same pack) + the UI font. The font
   * curve/band textures are bound internally by the text renderer; the only
   * material param is the coverage cutoff. depth off — it's a 2D HUD overlay. */
  s_text_vs_handle = nt_resource_request(
      ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_VERT, NT_ASSET_SHADER_CODE);
  s_text_fs_handle = nt_resource_request(
      ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_FRAG, NT_ASSET_SHADER_CODE);
  s_text_material = nt_material_create(&(nt_material_create_desc_t){
      .vs = s_text_vs_handle,
      .fs = s_text_fs_handle,
      .blend_mode = NT_BLEND_MODE_ALPHA,
      .depth_test = false,
      .depth_write = false,
      .cull_mode = NT_CULL_NONE,
      .params[0] = {.name = "u_alpha_cutoff",
                    .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
      .param_count = 1,
      .label = "corral_text",
  });
  s_font = nt_font_create(&(nt_font_create_desc_t){
      .curve_texture_width = 1024,
      .curve_texture_height = 512,
      .band_texture_height = 256,
      .band_count = 8,
      .measure_cache_size = 256,
  });
  nt_font_add(s_font, nt_resource_request(ASSET_FONT_CORRAL_FONT, NT_ASSET_FONT));

  nt_resource_set_activate_time_budget(0);

  corral_reset((float)s_window_width, (float)s_window_height);

#if NT_DEVAPI_ENABLED
  if (s_devapi_enabled) {
    nt_devapi_init();
    register_game_endpoints();
    if (!nt_devapi_net_start(s_devapi_port)) {
      (void)fprintf(stderr, "Failed to start DevAPI on port %u\n",
                    (unsigned)s_devapi_port);
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
  nt_text_renderer_shutdown();
  nt_font_destroy(s_font);
  nt_font_shutdown();
  nt_sprite_renderer_shutdown();
  nt_material_destroy(s_text_material);
  nt_material_destroy(s_sprite_material);
  nt_material_shutdown();
  nt_shape_renderer_shutdown();
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
