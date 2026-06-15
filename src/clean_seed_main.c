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
#define CORRAL_PEN_COUNT 2
#define CORRAL_MAX_PARTICLES 256
#define CORRAL_WAVE_CRITTERS 10 /* ~10 critters, 2 colors */

#define CORRAL_PACK_PATH "assets/runtime/critter-corral/critter_corral.ntpack"

/* Two bold, clearly-distinct critter hues (DIRECTION: bright, friendly).
 * Warm RED/orange vs cool BLUE — chosen to match the sprite body colors in
 * tools/critter_corral/generate_sprites.py so the pen tint reproduces the
 * critter's exact hue. Used for the pen color wash, flag, gate glow, and
 * particles. */
static const float CORRAL_COLORS[CORRAL_PEN_COUNT][4] = {
    {0.98F, 0.34F, 0.24F, 1.0F}, /* warm red (critter_a) */
    {0.22F, 0.56F, 1.0F, 1.0F},  /* cool blue (critter_b) */
};

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
  float flash; /* capture flash timer */
  float chain; /* chain-boost timer (signature cascade) */
  int parked;  /* count parked in this pen */
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
static Particle s_particles[CORRAL_MAX_PARTICLES];
static int s_score; /* total captured across all waves */
static int s_wave = 1;
static float s_cleared_flash; /* wave-clear celebration pulse */
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
  CORRAL_RGN_CRITTER_A = 0,
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

/* ---- Field / wave setup ---- */

static void corral_layout_pens(float w, float h) {
  /* One pen per color, fixed spots: left and right, gate facing inward. */
  const float pen_w = clampf(w * 0.16F, 110.0F, 220.0F);
  const float pen_h = clampf(h * 0.34F, 150.0F, 280.0F);
  const float cy = h * 0.5F - pen_h * 0.5F;
  s_pens[0].x = w * 0.045F;
  s_pens[0].y = cy;
  s_pens[0].w = pen_w;
  s_pens[0].h = pen_h;
  s_pens[0].color = 0;
  s_pens[1].x = w - pen_w - w * 0.045F;
  s_pens[1].y = cy;
  s_pens[1].w = pen_w;
  s_pens[1].h = pen_h;
  s_pens[1].color = 1;
}

static void corral_spawn_wave(float w, float h) {
  for (int i = 0; i < CORRAL_PEN_COUNT; ++i) {
    s_pens[i].parked = 0;
    s_pens[i].flash = 0.0F;
    s_pens[i].chain = 0.0F;
  }
  /* Critters spawn in the central pasture, away from pen gates. */
  s_critter_count = CORRAL_WAVE_CRITTERS;
  if (s_critter_count > CORRAL_MAX_CRITTERS) {
    s_critter_count = CORRAL_MAX_CRITTERS;
  }
  const float mid_lo = w * 0.30F;
  const float mid_hi = w * 0.70F;
  for (int i = 0; i < s_critter_count; ++i) {
    Critter *c = &s_critters[i];
    c->x = frand_range(mid_lo, mid_hi);
    c->y = frand_range(h * 0.22F, h * 0.78F);
    c->wander = frand_range(0.0F, 6.2831853F);
    float sp = frand_range(20.0F, 40.0F);
    c->vx = cosf(c->wander) * sp;
    c->vy = sinf(c->wander) * sp;
    c->squash = 0.0F;
    c->color = (uint8_t)(i % CORRAL_PEN_COUNT);
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
  s_cleared_flash = 0.0F;
  s_lure_active = false;
  corral_layout_pens(w, h);
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

/* Gate mouth = the inner face of the pen (the open side facing the field). */
static void pen_gate_point(const Pen *pen, float *gx, float *gy) {
  *gy = pen->y + pen->h * 0.5F;
  /* pen 0 opens to the right, pen 1 opens to the left */
  if (pen->color == 0) {
    *gx = pen->x + pen->w;
  } else {
    *gx = pen->x;
  }
}

static bool point_in_pen_mouth(const Pen *pen, float x, float y) {
  const float pad = 14.0F;
  if (y < pen->y - pad || y > pen->y + pen->h + pad) {
    return false;
  }
  if (pen->color == 0) {
    /* opens right: capture zone is the right third of the pen */
    return x <= pen->x + pen->w && x >= pen->x + pen->w * 0.45F;
  }
  return x >= pen->x && x <= pen->x + pen->w * 0.55F;
}

static void capture_critter(Critter *c, Pen *pen) {
  c->parked = true;
  c->alive = false;
  c->parked_pen = (int8_t)(pen->color);
  c->squash = 0.22F; /* squash/scale on entry (~0.2s) */
  pen->parked += 1;
  pen->flash = 0.30F;
  pen->chain = 0.85F; /* CHAIN: briefly boost same-color attraction */
  float gx;
  float gy;
  pen_gate_point(pen, &gx, &gy);
  spawn_burst(gx, gy, CORRAL_COLORS[pen->color], 8);
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

  for (int i = 0; i < CORRAL_PEN_COUNT; ++i) {
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
    for (int p = 0; p < CORRAL_PEN_COUNT; ++p) {
      Pen *pen = &s_pens[p];
      if (pen->color != c->color || pen->chain <= 0.0F) {
        continue;
      }
      float gx;
      float gy;
      pen_gate_point(pen, &gx, &gy);
      float dx = gx - c->x;
      float dy = gy - c->y;
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
    for (int p = 0; p < CORRAL_PEN_COUNT; ++p) {
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
      /* wrong color (or matching color hitting a solid wall): bounce */
      if (pen->color != c->color) {
        if (pen->color == 0 && c->x < pen->x + pen->w) {
          c->x = pen->x + pen->w + crit_r;
          c->vx = fabsf(c->vx) + 30.0F;
        } else if (pen->color == 1 && c->x > pen->x) {
          c->x = pen->x - crit_r;
          c->vx = -fabsf(c->vx) - 30.0F;
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

  /* --- particles --- */
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

  /* --- wave clear -> celebrate -> auto-respawn (continuous loop) --- */
  if (corral_loose_count() == 0 && s_critter_count > 0) {
    if (s_cleared_flash <= 0.0F) {
      s_cleared_flash = 0.85F;
      game_audio_play(GAME_AUDIO_CUE_NOTIFY);
    }
    if (s_cleared_flash <= 0.40F) {
      s_wave += 1;
      corral_spawn_wave(w, h);
      s_cleared_flash = 0.0F;
    }
  }
}

/* ---- Sprite render ---- */

static void draw_critter_sprite(const Critter *c) {
  corral_region_t rgn =
      (c->color == 0) ? CORRAL_RGN_CRITTER_A : CORRAL_RGN_CRITTER_B;
  const float base = 46.0F; /* on-screen diameter of a critter (bolder) */
  /* squash: brief vertical squish on capture (scale the world transform) */
  float sx = 1.0F;
  float sy = 1.0F;
  if (c->squash > 0.0F) {
    float t = c->squash / 0.22F;
    float s = 0.30F * sinf(t * 3.14159F);
    sx = 1.0F + s;       /* widen */
    sy = 1.0F - s * 0.7F; /* squish */
  }
  /* soft ground shadow under the critter (pop against the calmed grass) */
  emit_sprite(CORRAL_RGN_SPARK, c->x, c->y + base * 0.36F, base * 1.0F,
              base * 0.46F, 0x66000000U /* AABBGGRR: dark, low alpha */);
  emit_sprite(rgn, c->x, c->y, base * sx, base * sy, 0xFFFFFFFFU);
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
  /* The panel art carves its gate out of the RIGHT face. Pen 0 opens right
   * (toward the field) -> draw as-is. Pen 1 opens left -> mirror horizontally
   * by passing a negative width so the gate faces the field. */
  float draw_w = (pen->color == 0) ? pen->w : -pen->w;
  emit_sprite(CORRAL_RGN_PEN, cx, cy, draw_w, pen->h, pack_rgba(tint));

  /* color flag marker above the pen so the pen<->color mapping is unmistakable.
   * Tinted to the pen hue; flips to point inward toward the field. */
  float flag_w = 40.0F;
  float flag_h = 50.0F;
  float fcol[4] = {col[0], col[1], col[2], 1.0F};
  float flag_draw_w = (pen->color == 0) ? flag_w : -flag_w;
  emit_sprite(CORRAL_RGN_FLAG, cx, pen->y - flag_h * 0.42F, flag_draw_w, flag_h,
              pack_rgba(fcol));

  /* soft gate glow on the open (inner) face so the entrance reads (warm, low
   * intensity halo — not a hard marker). */
  float gx;
  float gy;
  pen_gate_point(pen, &gx, &gy);
  float glow = 0.40F + 0.45F * flash;
  float gate_col[4] = {clampf(col[0] + 0.35F, 0.0F, 1.0F),
                       clampf(col[1] + 0.35F, 0.0F, 1.0F),
                       clampf(col[2] + 0.35F, 0.0F, 1.0F), glow};
  emit_sprite(CORRAL_RGN_LURE, gx, gy, 60.0F, pen->h * 0.7F,
              pack_rgba(gate_col));

  /* parked critters stacked inside the pen */
  corral_region_t rgn =
      (pen->color == 0) ? CORRAL_RGN_CRITTER_A : CORRAL_RGN_CRITTER_B;
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
    emit_sprite(rgn, px, py, pr, pr, 0xFFFFFFFFU);
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

  for (int i = 0; i < CORRAL_PEN_COUNT; ++i) {
    draw_pen_sprite(&s_pens[i]);
  }

  /* lure ring (attract radius) at the cursor — soft glowing orb. The center is
   * intentionally soft (no harsh bright dot); the halo shows the attract zone.
   * "lure follows cursor" affordance = the gentle radius ring tracking input. */
  if (s_lure_active) {
    emit_sprite(CORRAL_RGN_LURE, s_lure_x, s_lure_y, s_lure_radius * 2.0F,
                s_lure_radius * 2.0F, 0x55FFFFFFU /* faint outer radius halo */);
    emit_sprite(CORRAL_RGN_LURE, s_lure_x, s_lure_y, 72.0F, 72.0F,
                0x99FFFFFFU /* soft inner glow, no hot dot */);
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

    /* PER-COLOR GOAL (right): for each color, show "remaining loose" as that
     * color's pips. Empty = wave goal met for that color. This is the wave
     * goal at a glance: clear all loose critters of both colors. */
    {
      const float dot = 16.0F;
      const float gap = dot * 1.2F;
      float xr = w - 18.0F;
      for (int p = CORRAL_PEN_COUNT - 1; p >= 0; --p) {
        const float *col = CORRAL_COLORS[p];
        float ccol[4] = {col[0], col[1], col[2], 1.0F};
        int rem = corral_color_remaining(p);
        int total = CORRAL_WAVE_CRITTERS / CORRAL_PEN_COUNT;
        if (total < 1) {
          total = 1;
        }
        /* draw `total` slots; loose ones filled bright, captured ones dim. */
        for (int i = 0; i < total; ++i) {
          float px = xr - dot * 0.5F - (float)i * gap;
          bool loose = i < rem;
          uint32_t cp = loose
                            ? pack_rgba(ccol)
                            : (pack_rgba((float[4]){col[0] * 0.35F,
                                                    col[1] * 0.35F,
                                                    col[2] * 0.35F, 0.85F}));
          emit_sprite(CORRAL_RGN_PIP, px, cy, dot, dot, cp);
        }
        xr -= (float)total * gap + dot * 0.8F;
      }
    }
  }

  /* per-pen mini counter: a small color pip + remaining count above each pen
   * gate so "how many of THIS color are still loose" reads right where the
   * player is aiming. */
  for (int p = 0; p < CORRAL_PEN_COUNT; ++p) {
    const Pen *pen = &s_pens[p];
    const float *col = CORRAL_COLORS[pen->color];
    int rem = corral_color_remaining(pen->color);
    float ccol[4] = {col[0], col[1], col[2], 1.0F};
    float bx = pen->x + pen->w * 0.5F;
    float by = pen->y + pen->h + 22.0F;
    const float dot = 11.0F;
    const float gap = dot * 1.15F;
    /* center the row */
    float row_w = (float)rem * gap;
    float sx0 = bx - row_w * 0.5F + dot * 0.5F;
    for (int i = 0; i < rem && i < 12; ++i) {
      emit_sprite(CORRAL_RGN_PIP, sx0 + (float)i * gap, by, dot, dot,
                  pack_rgba(ccol));
    }
  }

  /* wave indicator: small bright pips top-center */
  for (int i = 0; i < s_wave && i < 12; ++i) {
    emit_sprite(CORRAL_RGN_PIP, w * 0.5F - 6.0F + (float)i * 14.0F,
                h * 0.085F * 0.5F, 9.0F, 9.0F, 0xFFC8D8E0U);
  }

  /* wave-clear celebration: bright pulsing gold sparks along the frame */
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

static cJSON *state_json(void) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "runtime", "critter_corral");
  cJSON_AddNumberToObject(root, "critter_count", s_critter_count);
  cJSON_AddNumberToObject(root, "loose", corral_loose_count());
  cJSON_AddNumberToObject(root, "score", s_score);
  cJSON_AddNumberToObject(root, "wave", s_wave);
  cJSON_AddBoolToObject(root, "sprites_ready", s_sprites_ready);

  cJSON *remaining = cJSON_AddArrayToObject(root, "remaining_by_color");
  cJSON *penned = cJSON_AddArrayToObject(root, "penned_by_color");
  for (int i = 0; i < CORRAL_PEN_COUNT; ++i) {
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
  for (int i = 0; i < CORRAL_PEN_COUNT; ++i) {
    char id[16];
    (void)snprintf(id, sizeof(id), "pen.%d", i);
    (void)nt_devapi_register_ui_node(id, "root", "pen", "Pen", "", s_pens[i].x,
                                     s_pens[i].y, s_pens[i].w, s_pens[i].h,
                                     true, true);
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

static void handle_input(void) {
#ifndef NT_PLATFORM_WEB
  if (nt_input_key_is_pressed(NT_KEY_R)) {
    const float w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width
                                                 : g_nt_window.width);
    const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height
                                                  : g_nt_window.height);
    corral_reset(w, h);
  }
#endif
}

/* Resolve atlas region indices + source sizes once the atlas is READY. */
static void resolve_atlas_regions(void) {
  if (s_atlas_resolved || !nt_resource_is_ready(s_atlas_handle)) {
    return;
  }
  static const nt_hash64_t names[CORRAL_RGN_COUNT] = {
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
  s_sprites_ready =
      s_atlas_resolved && mat_info != NULL && mat_info->ready;

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

  s_vs_handle =
      nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_VERT,
                          NT_ASSET_SHADER_CODE);
  s_fs_handle =
      nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_FRAG,
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
