// Little Lives — JUICE / FX module.
//
// Drawn with the engine shape renderer (procedural primitives) — acknowledged
// debug-only art debt, consistent with the rest of Little Lives' runtime.
//
// Self-contained: fixed-size particle pool, a single decaying screen-shake
// scalar, and a pure squash-stretch helper. The engine shape renderer does NOT
// alpha-blend (alpha is ignored), so every particle is a small OPAQUE shaded
// shape; "fade" is done by shrinking and lifting, never by transparency.
//
// All visuals stay subtle — this is a cozy life sim, not a fireworks show.
#include "ll_fx.h"
#include "ll_art.h"
#include "renderers/nt_shape_renderer.h"
#include <math.h>
#include <string.h>

// #region pool + rng
#define LL_FX_MAX 256
#define LL_FX_GRAVITY 9.0F // m/s^2 baked for arcing particles (coins/dust lift then fall)

enum ll_fx_kind {
    LL_FX_KIND_DUST = 0, // soft ground puff
    LL_FX_KIND_SPARKLE,  // reward twinkle (rises, shrinks)
    LL_FX_KIND_COIN,     // gold cube arcing under gravity
    LL_FX_KIND_POP,      // expanding confirm burst
};

typedef struct {
    float pos[3];
    float vel[3];
    float life;     // seconds remaining
    float max_life; // seconds at spawn (for shrink curve)
    float size;     // base half-extent / radius at spawn
    float rgb[3];   // unshaded base colour
    int kind;       // enum ll_fx_kind
    bool alive;
} ll_fx_particle;

static ll_fx_particle s_pool[LL_FX_MAX];

// xorshift32 — deterministic, no rand()/srand(). Seeded with a fixed constant.
static unsigned int s_rng = 0x1A2B3C4Du;
static unsigned int ll_fx_rngu(void) {
    unsigned int x = s_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng = x;
    return x;
}
// uniform float in [0,1)
static float ll_fx_rnd01(void) { return (float)(ll_fx_rngu() >> 8) * (1.0F / 16777216.0F); }
// uniform float in [-1,1)
static float ll_fx_rnd11(void) { return ll_fx_rnd01() * 2.0F - 1.0F; }
// #endregion

// #region screen shake
static float s_shake = 0.0F;   // current shake amplitude (0..~1)
static float s_shake_t = 0.0F; // time accumulator drives the high-freq jitter
static float s_shake_off[3] = {0.0F, 0.0F, 0.0F};

void ll_fx_shake(float strength) {
    if (strength < 0.0F) {
        strength = 0.0F;
    }
    if (strength > 1.0F) {
        strength = 1.0F;
    }
    if (strength > s_shake) {
        s_shake = strength;
    }
}

void ll_fx_shake_world(float out_xyz[3]) {
    out_xyz[0] = s_shake_off[0];
    out_xyz[1] = s_shake_off[1];
    out_xyz[2] = s_shake_off[2];
}
// #endregion

// #region spawn
static ll_fx_particle *ll_fx_alloc(void) {
    for (int i = 0; i < LL_FX_MAX; i++) {
        if (!s_pool[i].alive) {
            ll_fx_particle *p = &s_pool[i];
            memset(p, 0, sizeof(*p));
            p->alive = true;
            return p;
        }
    }
    return NULL; // pool full — drop the spawn (juice is best-effort)
}

void ll_fx_dust(float x, float z) {
    // ~6 warm-grey puffs that drift outward low to the ground and lift slightly.
    for (int i = 0; i < 6; i++) {
        ll_fx_particle *p = ll_fx_alloc();
        if (!p) {
            return;
        }
        float ang = ll_fx_rnd01() * 6.2831853F;
        float spd = 0.6F + ll_fx_rnd01() * 0.6F;
        p->pos[0] = x + ll_fx_rnd11() * 0.08F;
        p->pos[1] = 0.04F + ll_fx_rnd01() * 0.05F;
        p->pos[2] = z + ll_fx_rnd11() * 0.08F;
        p->vel[0] = cosf(ang) * spd;
        p->vel[1] = 0.5F + ll_fx_rnd01() * 0.4F;
        p->vel[2] = sinf(ang) * spd;
        p->size = 0.13F + ll_fx_rnd01() * 0.06F;
        p->max_life = 0.5F;
        p->life = p->max_life;
        p->rgb[0] = 0.74F;
        p->rgb[1] = 0.70F;
        p->rgb[2] = 0.64F; // warm grey
        p->kind = LL_FX_KIND_DUST;
    }
}

void ll_fx_sparkle(float x, float y, float z) {
    // ~5 bright twinkles rising gently and shrinking out.
    for (int i = 0; i < 5; i++) {
        ll_fx_particle *p = ll_fx_alloc();
        if (!p) {
            return;
        }
        p->pos[0] = x + ll_fx_rnd11() * 0.18F;
        p->pos[1] = y + ll_fx_rnd11() * 0.12F;
        p->pos[2] = z + ll_fx_rnd11() * 0.18F;
        p->vel[0] = ll_fx_rnd11() * 0.25F;
        p->vel[1] = 0.7F + ll_fx_rnd01() * 0.5F;
        p->vel[2] = ll_fx_rnd11() * 0.25F;
        p->size = 0.10F + ll_fx_rnd01() * 0.05F;
        p->max_life = 0.6F;
        p->life = p->max_life;
        p->rgb[0] = 1.0F;
        p->rgb[1] = 0.96F;
        p->rgb[2] = 0.62F; // warm bright
        p->kind = LL_FX_KIND_SPARKLE;
    }
}

void ll_fx_coins(float x, float y, float z, int count) {
    if (count < 1) {
        return;
    }
    if (count > 8) {
        count = 8;
    }
    // Gold cubes pop up and arc back down under gravity.
    for (int i = 0; i < count; i++) {
        ll_fx_particle *p = ll_fx_alloc();
        if (!p) {
            return;
        }
        float ang = ll_fx_rnd01() * 6.2831853F;
        float spd = 0.7F + ll_fx_rnd01() * 0.7F;
        p->pos[0] = x + ll_fx_rnd11() * 0.1F;
        p->pos[1] = y + 0.05F;
        p->pos[2] = z + ll_fx_rnd11() * 0.1F;
        p->vel[0] = cosf(ang) * spd;
        p->vel[1] = 2.4F + ll_fx_rnd01() * 1.0F; // strong upward pop
        p->vel[2] = sinf(ang) * spd;
        p->size = 0.12F + ll_fx_rnd01() * 0.03F;
        p->max_life = 0.9F;
        p->life = p->max_life;
        p->rgb[0] = 1.0F;
        p->rgb[1] = 0.82F;
        p->rgb[2] = 0.26F; // gold
        p->kind = LL_FX_KIND_COIN;
    }
}

void ll_fx_pop(float x, float y, float z, const float rgb[3]) {
    // ~8 small cubes flung out in a flat-ish ring — a quick confirm burst.
    for (int i = 0; i < 8; i++) {
        ll_fx_particle *p = ll_fx_alloc();
        if (!p) {
            return;
        }
        float ang = (6.2831853F * (float)i) / 8.0F + ll_fx_rnd11() * 0.2F;
        float spd = 1.4F + ll_fx_rnd01() * 0.6F;
        p->pos[0] = x;
        p->pos[1] = y;
        p->pos[2] = z;
        p->vel[0] = cosf(ang) * spd;
        p->vel[1] = 0.4F + ll_fx_rnd01() * 0.4F;
        p->vel[2] = sinf(ang) * spd;
        p->size = 0.11F + ll_fx_rnd01() * 0.04F;
        p->max_life = 0.35F;
        p->life = p->max_life;
        p->rgb[0] = rgb[0];
        p->rgb[1] = rgb[1];
        p->rgb[2] = rgb[2];
        p->kind = LL_FX_KIND_POP;
    }
}
// #endregion

// #region lifecycle / update
void ll_fx_init(void) {
    memset(s_pool, 0, sizeof(s_pool));
    s_rng = 0x1A2B3C4Du;
    s_shake = 0.0F;
    s_shake_t = 0.0F;
    s_shake_off[0] = s_shake_off[1] = s_shake_off[2] = 0.0F;
}

void ll_fx_update(float dt) {
    if (dt < 0.0F) {
        dt = 0.0F;
    }
    if (dt > 0.1F) {
        dt = 0.1F; // clamp huge frame gaps so particles don't teleport
    }

    // #region particles
    for (int i = 0; i < LL_FX_MAX; i++) {
        ll_fx_particle *p = &s_pool[i];
        if (!p->alive) {
            continue;
        }
        p->life -= dt;
        if (p->life <= 0.0F) {
            p->alive = false;
            continue;
        }
        // Gravity only on the arcing kinds; sparkle/pop float on their own velocity.
        if (p->kind == LL_FX_KIND_COIN || p->kind == LL_FX_KIND_DUST) {
            p->vel[1] -= LL_FX_GRAVITY * dt;
        }
        // Dust meets light air drag so puffs ease out rather than shoot away.
        if (p->kind == LL_FX_KIND_DUST) {
            float drag = 1.0F - 2.0F * dt;
            if (drag < 0.0F) {
                drag = 0.0F;
            }
            p->vel[0] *= drag;
            p->vel[2] *= drag;
        }
        p->pos[0] += p->vel[0] * dt;
        p->pos[1] += p->vel[1] * dt;
        p->pos[2] += p->vel[2] * dt;
        // Keep particles from sinking through the floor.
        if (p->pos[1] < 0.02F) {
            p->pos[1] = 0.02F;
            if (p->vel[1] < 0.0F) {
                p->vel[1] = 0.0F;
            }
        }
    }
    // #endregion

    // #region shake
    if (s_shake > 0.0001F) {
        s_shake_t += dt;
        // Frame-rate-independent exponential decay (~0.86 per 60fps frame).
        s_shake *= expf(-9.0F * dt);
        if (s_shake < 0.0001F) {
            s_shake = 0.0F;
        }
        // High-frequency pseudo-random offset, magnitude ~0.12 * shake.
        float amp = 0.12F * s_shake;
        s_shake_off[0] = sinf(s_shake_t * 57.0F) * amp;
        s_shake_off[1] = sinf(s_shake_t * 43.0F + 1.7F) * amp * 0.6F;
        s_shake_off[2] = sinf(s_shake_t * 61.0F + 3.1F) * amp;
    } else {
        s_shake = 0.0F;
        s_shake_off[0] = s_shake_off[1] = s_shake_off[2] = 0.0F;
    }
    // #endregion
}
// #endregion

// #region draw
void ll_fx_draw_world(float daylight) {
    float col[4];
    for (int i = 0; i < LL_FX_MAX; i++) {
        const ll_fx_particle *p = &s_pool[i];
        if (!p->alive) {
            continue;
        }
        float t = p->max_life > 0.0F ? (p->life / p->max_life) : 0.0F; // 1 at spawn -> 0 at death
        if (t < 0.0F) {
            t = 0.0F;
        }
        float s = p->size * t; // shrink as life runs out (opaque "fade")
        if (s < 0.002F) {
            continue;
        }
        switch (p->kind) {
        case LL_FX_KIND_DUST: {
            // Soft little spheres, warm grey, shaded flat.
            ll_shade_flat(col, p->rgb[0], p->rgb[1], p->rgb[2], 1.0F, daylight);
            nt_shape_renderer_sphere(p->pos, s, col);
            break;
        }
        case LL_FX_KIND_SPARKLE: {
            // Bright sphere; brighten near the start of its life for a twinkle.
            float boost = 0.85F + 0.15F * t;
            ll_shade_flat(col, p->rgb[0] * boost, p->rgb[1] * boost, p->rgb[2] * boost, 1.0F, daylight);
            nt_shape_renderer_sphere(p->pos, s, col);
            break;
        }
        case LL_FX_KIND_COIN: {
            // Small gold cube — slight per-particle spin via the size pulse only
            // (rotation quats omitted to keep it cheap & axis-aligned-readable).
            float sz[3] = {s, s, s};
            ll_shade_flat(col, p->rgb[0], p->rgb[1], p->rgb[2], 1.0F, daylight);
            nt_shape_renderer_cube(p->pos, sz, col);
            break;
        }
        case LL_FX_KIND_POP: {
            // Quick coloured cube fleck in the caller's rgb.
            float sz[3] = {s, s, s};
            ll_shade_flat(col, p->rgb[0], p->rgb[1], p->rgb[2], 1.0F, daylight);
            nt_shape_renderer_cube(p->pos, sz, col);
            break;
        }
        default:
            break;
        }
    }
}
// #endregion

// #region squash-stretch
bool ll_fx_pop_scale(float elapsed, float out_scale[3]) {
    const float dur = 0.5F;
    if (elapsed < 0.0F) {
        elapsed = 0.0F;
    }
    if (elapsed >= dur) {
        out_scale[0] = out_scale[1] = out_scale[2] = 1.0F;
        return false;
    }
    // Normalized progress 0..1.
    float u = elapsed / dur;
    // Damped overshoot: a decaying sine that swings the Y axis up first (stretch),
    // then settles to 0. amount -> 0 as u -> 1.
    float decay = 1.0F - u;
    float wobble = sinf(u * 6.2831853F * 1.1F) * decay * decay; // ~+1 stretch early, overshoot, settle
    float amp = 0.40F;
    float sy = 1.0F + amp * wobble;
    // Conserve volume-ish: when Y stretches, X/Z squash, and vice versa.
    float sxz = 1.0F - amp * 0.6F * wobble;
    out_scale[0] = sxz;
    out_scale[1] = sy;
    out_scale[2] = sxz;
    return true;
}
// #endregion
