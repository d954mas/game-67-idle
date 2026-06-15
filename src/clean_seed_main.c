#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
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

typedef enum {
  CORRAL_PHASE_TITLE = 0,    /* start beat — press/click to start */
  CORRAL_PHASE_PLAYING,      /* herding a wave */
  CORRAL_PHASE_WAVE_CLEARED, /* brief celebratory beat, then next wave */
  CORRAL_PHASE_WIN,          /* soft "you did it!" milestone, then endless */
} corral_phase_t;

typedef struct Critter {
  float x, y;
  float vx, vy;
  float wander;  /* current wander heading (radians) */
  float squash;  /* capture squash timer (seconds remaining) */
  uint8_t color; /* index into CORRAL_COLORS */
  bool alive;    /* still loose on the field */
  bool parked;   /* captured and resting in a pen */
  int8_t parked_pen;
} Critter;

typedef struct Pen {
  float x, y, w, h; /* top-left rect */
  uint8_t color;
  float gx, gy;   /* gate mouth point (open face, toward the field) */
  float gdx, gdy; /* inward gate normal (unit, points into the field) */
  float flash;    /* capture flash timer */
  float chain;    /* chain-boost timer (signature cascade) */
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

/* Lure (the one action): follows the mouse. */
static float s_lure_x;
static float s_lure_y;
static float s_lure_radius = 150.0F;
static bool s_lure_active;

/* ---- Sprite-render plumbing ---- */

static nt_buffer_t s_frame_ubo;
static nt_hash32_t s_pack_id;
static nt_resource_t s_atlas_handle;
static nt_resource_t s_vs_handle;
static nt_resource_t s_fs_handle;
static nt_material_t s_sprite_material;

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
  CORRAL_RGN_COUNT,
} corral_region_t;

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

/* ---- Field / wave setup ---- */

/* Lay out one pen per ACTIVE color around the field edges/corners, each with an
 * open gate facing the field centre. Readable spreads for 2..5 pens; the pen
 * shrinks a touch as the count grows so the pasture stays open. */
static void corral_layout_pens(float w, float h) {
  const int n = s_color_count;
  const float top = h * 0.10F; /* HUD band reserved at the very top */
  const float pen_w = clampf(w * 0.15F, 96.0F, 180.0F);
  const float pen_h = clampf(h * 0.30F, 130.0F, 240.0F);
  const float mx = w * 0.035F; /* edge margins */
  const float my = top + h * 0.02F;
  const float cx = w * 0.5F;
  const float cy = h * 0.5F;

  /* Anchored slots, picked per active-color count for a balanced, readable
   * spread. Each slot is a top-left rect; the gate faces the field centre. */
  for (int i = 0; i < n && i < CORRAL_PEN_COUNT; ++i) {
    Pen *p = &s_pens[i];
    p->color = (uint8_t)i;
    p->w = pen_w;
    p->h = pen_h;
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
  corral_layout_pens(w, h);
  for (int i = 0; i < CORRAL_PEN_COUNT; ++i) {
    s_pens[i].parked = 0;
    s_pens[i].flash = 0.0F;
    s_pens[i].chain = 0.0F;
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
    c->color = (uint8_t)(i % s_color_count);
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
  s_lure_active = false;
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
  game_audio_play(GAME_AUDIO_CUE_NOTIFY);
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

/* Gate mouth lives on the pen struct (pen->gx/gy + gdx/gdy), precomputed in
 * corral_layout_pens so any edge/corner placement works. */

/* Capture zone = the inner ~half of the pen on its open face. */
static bool point_in_pen_mouth(const Pen *pen, float x, float y) {
  const float pad = 14.0F;
  if (pen->gdx != 0.0F) {
    /* horizontal opening */
    if (y < pen->y - pad || y > pen->y + pen->h + pad) {
      return false;
    }
    if (pen->gdx > 0.0F) {
      return x <= pen->x + pen->w && x >= pen->x + pen->w * 0.45F;
    }
    return x >= pen->x && x <= pen->x + pen->w * 0.55F;
  }
  /* vertical opening */
  if (x < pen->x - pad || x > pen->x + pen->w + pad) {
    return false;
  }
  if (pen->gdy > 0.0F) {
    return y <= pen->y + pen->h && y >= pen->y + pen->h * 0.45F;
  }
  return y >= pen->y && y <= pen->y + pen->h * 0.55F;
}

static void capture_critter(Critter *c, Pen *pen) {
  c->parked = true;
  c->alive = false;
  c->parked_pen = (int8_t)(pen->color);
  c->squash = 0.22F; /* squash/scale on entry (~0.2s) */
  pen->parked += 1;
  pen->flash = 0.30F;
  pen->chain = 0.85F; /* CHAIN: briefly boost same-color attraction */
  spawn_burst(pen->gx, pen->gy, CORRAL_COLORS[pen->color], 8);
  s_score += 1;
  game_audio_play(GAME_AUDIO_CUE_SUCCESS); /* soft "ding" */
}

static void critter_update(float dt, float w, float h) {
  if (dt <= 0.0F) {
    dt = 1.0F / 60.0F;
  }
  if (dt > 0.05F) {
    dt = 0.05F;
  }

  game_audio_update();

  /* Lure tracks the cursor (framebuffer pixels == our draw space). */
  s_lure_x = g_nt_input.pointers[0].x;
  s_lure_y = g_nt_input.pointers[0].y;
  s_lure_active = s_lure_x > 0.0F || s_lure_y > 0.0F;

  /* timers tick in every phase so flashes/particles settle even on the beats */
  for (int i = 0; i < s_color_count; ++i) {
    if (s_pens[i].flash > 0.0F) {
      s_pens[i].flash -= dt;
    }
    if (s_pens[i].chain > 0.0F) {
      s_pens[i].chain -= dt;
    }
  }
  if (s_cleared_flash > 0.0F) {
    s_cleared_flash -= dt;
  }
  if (s_ftue_hint > 0.0F) {
    s_ftue_hint -= dt;
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
      corral_next_wave(w, h);
    }
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

  const float crit_r = 13.0F;

  for (int i = 0; i < s_critter_count; ++i) {
    Critter *c = &s_critters[i];
    if (c->squash > 0.0F) {
      c->squash -= dt;
    }
    if (!c->alive || c->parked) {
      continue;
    }

    /* --- random wander (smooth heading drift) --- */
    c->wander += frand_range(-2.2F, 2.2F) * dt;
    float ax = cosf(c->wander) * 22.0F;
    float ay = sinf(c->wander) * 22.0F;

    /* --- separation: push off close neighbors so they don't overlap --- */
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
    }

    /* --- lure attraction (the one action) --- */
    if (s_lure_active) {
      float dx = s_lure_x - c->x;
      float dy = s_lure_y - c->y;
      float d2 = dx * dx + dy * dy;
      if (d2 < s_lure_radius * s_lure_radius && d2 > 1.0F) {
        float d = sqrtf(d2);
        float t = 1.0F - (d / s_lure_radius); /* stronger when closer */
        float pull = 140.0F * (0.4F + t);
        ax += (dx / d) * pull;
        ay += (dy / d) * pull;
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
        float boost = 130.0F * (pen->chain / 0.85F);
        ax += (dx / d) * boost;
        ay += (dy / d) * boost;
      }
    }

    /* integrate velocity, clamp speed (gentle) */
    c->vx += ax * dt;
    c->vy += ay * dt;
    float speed = sqrtf(c->vx * c->vx + c->vy * c->vy);
    float max_speed = 130.0F;
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
        pen->flash = 0.12F;
        game_audio_play(GAME_AUDIO_CUE_ERROR); /* soft bonk */
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
    } else {
      s_phase = CORRAL_PHASE_WAVE_CLEARED;
      s_phase_timer = CORRAL_WAVE_CLEARED_TIME;
    }
    game_audio_play(GAME_AUDIO_CUE_NOTIFY);
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
  const float base = 46.0F; /* on-screen diameter of a critter (bolder) */
  /* squash: brief vertical squish on capture (scale the world transform) */
  float sx = 1.0F;
  float sy = 1.0F;
  if (c->squash > 0.0F) {
    float t = c->squash / 0.22F;
    float s = 0.30F * sinf(t * 3.14159F);
    sx = 1.0F + s;        /* widen */
    sy = 1.0F - s * 0.7F; /* squish */
  }
  /* soft ground shadow under the critter (pop against the calmed grass) */
  emit_sprite(CORRAL_RGN_SPARK, c->x, c->y + base * 0.36F, base * 1.0F,
              base * 0.46F, 0x66000000U /* AABBGGRR: dark, low alpha */);
  /* one neutral critter sprite, tinted per color -> up to 5 distinct hues. */
  emit_sprite(CORRAL_RGN_CRITTER, c->x, c->y, base * sx, base * sy,
              critter_tint(c->color));
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

/* Fontless phase beats: TITLE start prompt, WAVE_CLEARED beat, and the soft
 * WIN milestone — all built from panels + pips/critter/lure sprites so they
 * read without any font. The field stays visible behind every beat. */
static void corral_draw_phase_overlay(float w, float h) {
  if (s_phase == CORRAL_PHASE_TITLE) {
    /* soft dim over the (visible, frozen) field so the start beat pops. */
    emit_panel(w * 0.5F, h * 0.5F, w, h, 0x55101418U);
    /* title plate */
    emit_panel(w * 0.5F, h * 0.42F, w * 0.62F, h * 0.34F, 0xDD2A2218U);
    /* a friendly row of all five hues = "this is the game" identity badge. */
    float bx = w * 0.5F - 2.0F * 70.0F;
    for (int i = 0; i < CORRAL_MAX_COLORS; ++i) {
      float bob = 6.0F * sinf((float)g_nt_app.frame * 0.06F + (float)i * 0.9F);
      emit_sprite(CORRAL_RGN_CRITTER, bx + (float)i * 70.0F, h * 0.38F + bob,
                  56.0F, 56.0F, critter_tint(i));
    }
    /* pulsing lure orb = "press / click to start" (the one action, invited). */
    float pulse = 0.5F + 0.5F * sinf((float)g_nt_app.frame * 0.12F);
    float sz = 92.0F + 26.0F * pulse;
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
  }

  for (int i = 0; i < s_critter_count; ++i) {
    const Critter *c = &s_critters[i];
    if (c->alive && !c->parked) {
      draw_critter_sprite(c);
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
      const float dot = 14.0F;
      const float gap = dot * 1.25F;
      float x0 = 18.0F;
      int max_pips = 18;
      int shown = s_score < max_pips ? s_score : max_pips;
      /* backing label chip */
      emit_sprite(CORRAL_RGN_PIP, x0 + 6.0F, cy, 16.0F, 16.0F, 0xFF66CCFFU);
      x0 += 24.0F;
      for (int i = 0; i < shown; ++i) {
        float px = x0 + dot * 0.5F + (float)i * gap;
        emit_sprite(CORRAL_RGN_PIP, px, cy, dot, dot, 0xFFF0F0F0U /* white */);
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

  /* clickable RESTART marker (top-right): a small dark chip with a looping ring
   * glyph (reuse the lure ring) — fontless "start over", calm, always
   * available. Rect matches restart_marker_rect() so the click hit-test lines
   * up. */
  {
    float rw = 40.0F;
    float rh = 32.0F;
    float rx = w - rw - 6.0F;
    float ry = 6.0F;
    emit_sprite(CORRAL_RGN_PIP, rx + rw * 0.5F, ry + rh * 0.5F, rw, rh,
                0xCC2A2218U);
    emit_sprite(CORRAL_RGN_LURE, rx + rw * 0.5F, ry + rh * 0.5F, 24.0F, 24.0F,
                0xFFD8E0E8U);
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
  case CORRAL_PHASE_WIN:
    return "win";
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

  cJSON *remaining = cJSON_AddArrayToObject(root, "remaining_by_color");
  cJSON *penned = cJSON_AddArrayToObject(root, "penned_by_color");
  for (int i = 0; i < s_color_count; ++i) {
    cJSON_AddItemToArray(remaining,
                         cJSON_CreateNumber(corral_color_remaining(i)));
    cJSON_AddItemToArray(penned, cJSON_CreateNumber(s_pens[i].parked));
  }
  cJSON_AddBoolToObject(root, "wave_cleared",
                        corral_loose_count() == 0 && s_critter_count > 0);
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
 * forward through many waves to verify scaling. Captures every loose critter
 * into its matching pen (real score), then advances synchronously: the WIN
 * milestone fires once at CORRAL_WIN_WAVE, otherwise the next wave spawns. */
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
      s_win_shown = true; /* fire the soft milestone once, then advance */
    }
    corral_next_wave(w, h);
  } else if (s_phase == CORRAL_PHASE_WAVE_CLEARED ||
             s_phase == CORRAL_PHASE_WIN) {
    corral_next_wave(w, h);
  }
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
  (void)nt_devapi_register_ui_node("restart", "root", "button", "Restart", "",
                                   w - 46.0F, 6.0F, 40.0F, 32.0F, true, true);
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

/* Clickable restart marker rect (top-right HUD corner). */
static void restart_marker_rect(float w, float *rx, float *ry, float *rw,
                                float *rh) {
  *rw = 40.0F;
  *rh = 32.0F;
  *rx = w - *rw - 6.0F;
  *ry = 6.0F;
}

static void handle_input(void) {
#ifndef NT_PLATFORM_WEB
  const float w =
      (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
  const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                : g_nt_window.height);
  /* R = restart any time (back to a fresh run / title — calm, no penalty). */
  if (nt_input_key_is_pressed(NT_KEY_R)) {
    corral_reset(w, h);
    return;
  }
  /* clickable restart marker takes priority over a TITLE start-click. */
  if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
    float rx;
    float ry;
    float rw;
    float rh;
    restart_marker_rect(w, &rx, &ry, &rw, &rh);
    float px = g_nt_input.pointers[0].x;
    float py = g_nt_input.pointers[0].y;
    if (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh) {
      corral_reset(w, h);
      return;
    }
  }
  /* TITLE: press/click to start (any of click / space / enter). */
  if (s_phase == CORRAL_PHASE_TITLE &&
      (nt_input_mouse_is_pressed(NT_BUTTON_LEFT) ||
       nt_input_key_is_pressed(NT_KEY_SPACE) ||
       nt_input_key_is_pressed(NT_KEY_ENTER))) {
    corral_start(w, h);
  }
#endif
}

/* Resolve atlas region indices + source sizes once the atlas is READY. */
static void resolve_atlas_regions(void) {
  if (s_atlas_resolved || !nt_resource_is_ready(s_atlas_handle)) {
    return;
  }
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

  /* Resource/material pumps for the sprite pipeline. */
  nt_resource_step();
  nt_material_step();
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

  nt_gfx_begin_frame();
  if (g_nt_gfx.context_restored) {
    nt_shape_renderer_restore_gpu();
    nt_sprite_renderer_restore_gpu();
    nt_resource_invalidate(NT_ASSET_TEXTURE);
    nt_resource_invalidate(NT_ASSET_SHADER_CODE);
    nt_resource_invalidate(NT_ASSET_ATLAS);
    s_sprites_ready = false;
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

  /* Immediate-mode sprite emit only — no ECS components needed. */
  nt_material_init(&(nt_material_desc_t){.max_materials = 4});
  nt_sprite_renderer_desc_t sr_desc = nt_sprite_renderer_desc_defaults();
  nt_sprite_renderer_init(&sr_desc);

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
  nt_sprite_renderer_shutdown();
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
