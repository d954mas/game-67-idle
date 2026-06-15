#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "game_audio.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "renderers/nt_shape_renderer.h"
#include "window/nt_window.h"

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

/* ---- Critter Corral: first playable core moment (primitives only) ---- */

#define CORRAL_MAX_CRITTERS 64
#define CORRAL_PEN_COUNT 2
#define CORRAL_MAX_PARTICLES 256
#define CORRAL_WAVE_CRITTERS 10 /* ~10 critters, 2 colors */

/* Two bold, high-contrast critter colors (DIRECTION: bright, friendly). */
static const float CORRAL_COLORS[CORRAL_PEN_COUNT][4] = {
    {0.96F, 0.45F, 0.18F, 1.0F}, /* warm orange */
    {0.20F, 0.55F, 0.95F, 1.0F}, /* sky blue */
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

typedef struct UiBox {
  float x;
  float y;
  float w;
  float h;
} UiBox;

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

static void ortho(float left, float right, float bottom, float top,
                  float near_z, float far_z, float out[16]) {
  memset(out, 0, sizeof(float) * 16);
  out[0] = 2.0F / (right - left);
  out[5] = 2.0F / (top - bottom);
  out[10] = -2.0F / (far_z - near_z);
  out[12] = -(right + left) / (right - left);
  out[13] = -(top + bottom) / (top - bottom);
  out[14] = -(far_z + near_z) / (far_z - near_z);
  out[15] = 1.0F;
}

/* ---- Primitive draw helpers (match the seed template's signatures) ---- */

static void rect(float x, float y, float w, float h, const float color[4]) {
  nt_shape_renderer_rect((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F},
                         (float[2]){w, h}, color);
}

/* The shape renderer's circle/circle_wire live in the XZ plane, so they project
 * to a flat line in our 2D XY ortho setup. Build discs/rings directly in XY. */
#define CORRAL_DISC_SEGS 20

static void circle(float x, float y, float radius, const float color[4]) {
  const float step = 6.2831853F / (float)CORRAL_DISC_SEGS;
  float center[3] = {x, y, 0.0F};
  for (int i = 0; i < CORRAL_DISC_SEGS; ++i) {
    float a0 = (float)i * step;
    float a1 = (float)(i + 1) * step;
    float p0[3] = {x + cosf(a0) * radius, y + sinf(a0) * radius, 0.0F};
    float p1[3] = {x + cosf(a1) * radius, y + sinf(a1) * radius, 0.0F};
    nt_shape_renderer_triangle(center, p0, p1, color);
  }
}

/* Thick ring drawn from triangle quads so it stays visible regardless of GL
 * line-width support. */
static void ring(float x, float y, float radius, float thickness,
                 const float color[4]) {
  const float step = 6.2831853F / (float)CORRAL_DISC_SEGS;
  float r0 = radius - thickness * 0.5F;
  float r1 = radius + thickness * 0.5F;
  for (int i = 0; i < CORRAL_DISC_SEGS; ++i) {
    float a0 = (float)i * step;
    float a1 = (float)(i + 1) * step;
    float c0 = cosf(a0);
    float s0 = sinf(a0);
    float c1 = cosf(a1);
    float s1 = sinf(a1);
    float inner0[3] = {x + c0 * r0, y + s0 * r0, 0.0F};
    float outer0[3] = {x + c0 * r1, y + s0 * r1, 0.0F};
    float inner1[3] = {x + c1 * r0, y + s1 * r0, 0.0F};
    float outer1[3] = {x + c1 * r1, y + s1 * r1, 0.0F};
    nt_shape_renderer_triangle(inner0, outer0, outer1, color);
    nt_shape_renderer_triangle(inner0, outer1, inner1, color);
  }
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

/* ---- Render ---- */

static void draw_critter(const Critter *c, float w_unused, float h_unused) {
  (void)w_unused;
  (void)h_unused;
  const float *col = CORRAL_COLORS[c->color];
  const float r = 13.0F;
  /* squash: brief vertical squish on capture */
  float sq = 1.0F;
  if (c->squash > 0.0F) {
    float t = c->squash / 0.22F;
    sq = 1.0F + 0.45F * sinf(t * 3.14159F);
  }
  const float shadow[4] = {0.34F, 0.52F, 0.30F, 1.0F}; /* opaque darker-grass */
  circle(c->x + 2.0F, c->y + r * 0.55F, r * 0.92F, shadow);
  /* body */
  float br = r * (2.0F - sq);
  circle(c->x, c->y, br, col);
  /* highlight (opaque white dot reads as a cartoon shine) */
  circle(c->x - r * 0.32F, c->y - r * 0.32F, r * 0.30F,
         (float[4]){1.0F, 1.0F, 1.0F, 1.0F});
  /* two tiny eyes (white + dark pupil), biased toward travel direction */
  const float white[4] = {1.0F, 1.0F, 1.0F, 1.0F};
  const float eye[4] = {0.10F, 0.10F, 0.14F, 1.0F};
  float dir = (c->vx >= 0.0F) ? 1.0F : -1.0F;
  float ex = c->x + dir * r * 0.18F;
  float ey = c->y - r * 0.18F;
  float gap = r * 0.34F;
  circle(ex - gap, ey, r * 0.20F, white);
  circle(ex + gap, ey, r * 0.20F, white);
  circle(ex - gap + dir * r * 0.05F, ey, r * 0.10F, eye);
  circle(ex + gap + dir * r * 0.05F, ey, r * 0.10F, eye);
}

static void draw_pen(const Pen *pen) {
  const float *col = CORRAL_COLORS[pen->color];
  float flash =
      pen->flash > 0.0F ? clampf(pen->flash / 0.30F, 0.0F, 1.0F) : 0.0F;
  float chain =
      pen->chain > 0.0F ? clampf(pen->chain / 0.85F, 0.0F, 1.0F) : 0.0F;

  /* floor (opaque: shape renderer does not alpha-blend). A pale wash of the
   * pen color, brightened on capture flash / chain so the pen "lights up". */
  float lit = 0.45F * flash + 0.18F * chain;
  float floor[4] = {
      clampf(0.74F + col[0] * 0.18F + lit, 0.0F, 1.0F),
      clampf(0.80F + col[1] * 0.10F + lit, 0.0F, 1.0F),
      clampf(0.62F + col[2] * 0.18F + lit, 0.0F, 1.0F),
      1.0F,
  };
  rect(pen->x, pen->y, pen->w, pen->h, floor);

  /* thick walls on 3 sides; the gate side is open */
  const float t = 10.0F;
  float wall[4] = {col[0] * 0.85F, col[1] * 0.85F, col[2] * 0.85F, 1.0F};
  if (flash > 0.0F) {
    wall[0] = clampf(wall[0] + flash * 0.4F, 0.0F, 1.0F);
    wall[1] = clampf(wall[1] + flash * 0.4F, 0.0F, 1.0F);
    wall[2] = clampf(wall[2] + flash * 0.4F, 0.0F, 1.0F);
  }
  rect(pen->x, pen->y, pen->w, t, wall);              /* top */
  rect(pen->x, pen->y + pen->h - t, pen->w, t, wall); /* bottom */
  if (pen->color == 0) {
    rect(pen->x, pen->y, t, pen->h, wall); /* left solid, opens right */
  } else {
    rect(pen->x + pen->w - t, pen->y, t, pen->h,
         wall); /* right solid, opens left */
  }

  /* bright gate posts flanking the open side so the entrance reads clearly */
  float post[4] = {clampf(col[0] + 0.2F, 0.0F, 1.0F),
                   clampf(col[1] + 0.2F, 0.0F, 1.0F),
                   clampf(col[2] + 0.2F, 0.0F, 1.0F), 1.0F};
  float gx = (pen->color == 0) ? pen->x + pen->w - t : pen->x;
  rect(gx, pen->y, t, t * 1.8F, post);
  rect(gx, pen->y + pen->h - t * 1.8F, t, t * 1.8F, post);

  /* parked critters stacked inside the pen */
  const float pr = 9.0F;
  int cols = (int)((pen->w - t * 2.0F) / (pr * 2.4F));
  if (cols < 1) {
    cols = 1;
  }
  for (int i = 0; i < pen->parked; ++i) {
    int cx = i % cols;
    int cy = i / cols;
    float px = pen->x + t + pr * 1.4F + (float)cx * pr * 2.4F;
    float py = pen->y + t + pr * 1.6F + (float)cy * pr * 2.2F;
    if (py > pen->y + pen->h - pr) {
      break;
    }
    circle(px, py, pr, col);
    circle(px - pr * 0.3F, py - pr * 0.3F, pr * 0.32F, (float[4]){1, 1, 1, 1});
  }
}

static void corral_draw(float w, float h) {
  /* Open pasture: calm green field, soft sky band, hill horizon for depth. */
  rect(0.0F, 0.0F, w, h, (float[4]){0.50F, 0.78F, 0.40F, 1.0F});
  rect(0.0F, 0.0F, w, h * 0.12F, (float[4]){0.62F, 0.86F, 0.96F, 1.0F});
  rect(0.0F, h * 0.12F, w, h * 0.05F,
       (float[4]){0.42F, 0.70F, 0.34F, 1.0F}); /* distant hill */
  rect(0.0F, h * 0.17F, w, h * 0.04F,
       (float[4]){0.56F, 0.82F, 0.45F, 1.0F}); /* grass highlight band */
  /* a few lighter grass patches for texture (opaque, close to field tone) */
  rect(w * 0.20F, h * 0.30F, w * 0.18F, h * 0.10F,
       (float[4]){0.58F, 0.82F, 0.46F, 1.0F});
  rect(w * 0.62F, h * 0.58F, w * 0.16F, h * 0.09F,
       (float[4]){0.58F, 0.82F, 0.46F, 1.0F});

  for (int i = 0; i < CORRAL_PEN_COUNT; ++i) {
    draw_pen(&s_pens[i]);
  }

  /* lure ring (attract radius) at the cursor */
  if (s_lure_active) {
    ring(s_lure_x, s_lure_y, s_lure_radius, 5.0F,
         (float[4]){1.0F, 0.85F, 0.20F, 1.0F});
    ring(s_lure_x, s_lure_y, s_lure_radius * 0.55F, 3.0F,
         (float[4]){1.0F, 0.93F, 0.55F, 1.0F});
    circle(s_lure_x, s_lure_y, 8.0F, (float[4]){1.0F, 0.97F, 0.65F, 1.0F});
    circle(s_lure_x, s_lure_y, 4.0F, (float[4]){0.95F, 0.55F, 0.10F, 1.0F});
  }

  for (int i = 0; i < s_critter_count; ++i) {
    const Critter *c = &s_critters[i];
    if (c->alive && !c->parked) {
      draw_critter(c, w, h);
    }
  }

  /* particles (fade out) */
  for (int i = 0; i < CORRAL_MAX_PARTICLES; ++i) {
    const Particle *p = &s_particles[i];
    if (p->life <= 0.0F) {
      continue;
    }
    float a = clampf(p->life / p->max_life, 0.0F, 1.0F);
    circle(p->x, p->y, 4.0F * a + 1.0F,
           (float[4]){p->color[0], p->color[1], p->color[2], a});
  }

  /* score as a row of dots (no font): one dot per capture, wrapping. */
  const float dot_r = 5.0F;
  const float sx = 16.0F;
  const float sy = 14.0F;
  int per_row = (int)((w - sx * 2.0F) / (dot_r * 2.6F));
  if (per_row < 1) {
    per_row = 1;
  }
  for (int i = 0; i < s_score; ++i) {
    int cx = i % per_row;
    int cy = i / per_row;
    const float *col = CORRAL_COLORS[i % CORRAL_PEN_COUNT];
    circle(sx + dot_r + (float)cx * dot_r * 2.6F,
           sy + dot_r + (float)cy * dot_r * 2.6F, dot_r, col);
  }

  /* wave indicator: small bars top-right */
  for (int i = 0; i < s_wave && i < 12; ++i) {
    rect(w - 18.0F - (float)i * 12.0F, 12.0F, 8.0F, 16.0F,
         (float[4]){0.20F, 0.20F, 0.26F, 0.85F});
  }

  /* wave-clear celebration: a bright pulsing frame border (opaque-safe) */
  if (s_cleared_flash > 0.0F) {
    float pulse = 0.5F + 0.5F * sinf(s_cleared_flash * 24.0F);
    float t = 10.0F + 18.0F * pulse;
    const float gold[4] = {1.0F, 0.92F, 0.35F, 1.0F};
    rect(0.0F, 0.0F, w, t, gold);
    rect(0.0F, h - t, w, t, gold);
    rect(0.0F, 0.0F, t, h, gold);
    rect(w - t, 0.0F, t, h, gold);
  }
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

  nt_gfx_begin_frame();
  if (g_nt_gfx.context_restored) {
    nt_shape_renderer_restore_gpu();
  }
  nt_gfx_begin_pass(&(nt_pass_desc_t){
      .clear_color = {0.55F, 0.80F, 0.42F, 1.0F}, .clear_depth = 1.0F});

  float vp[16];
  ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, vp);
  nt_shape_renderer_set_vp(vp);
  nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
  nt_shape_renderer_set_depth(false);
  nt_shape_renderer_set_line_width(3.0F);

  corral_draw(w, h);

  nt_shape_renderer_flush();
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

  g_nt_window.title = "Critter Corral";
  g_nt_window.width = (uint32_t)s_window_width;
  g_nt_window.height = (uint32_t)s_window_height;
  nt_window_init();
  nt_input_init();

  nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
  gfx_desc.depth = true;
  nt_gfx_init(&gfx_desc);
  nt_shape_renderer_init();
  game_audio_init();

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
  nt_shape_renderer_shutdown();
  nt_gfx_shutdown();
  nt_input_shutdown();
  nt_window_shutdown();
  nt_engine_shutdown();
#endif

  return 0;
}
