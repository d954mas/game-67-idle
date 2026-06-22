// Little Lives — ART DIRECTION CONTRACT (authored once, frozen).
//
// One coherent stylized-flat-low-poly "miniature diorama" look. Every draw path
// (ground, walls, furniture facets, sims, HUD) reads its palette + lighting from
// here so the scene reads as ONE authored place, not a pile of ad-hoc colours.
//
// The engine shape renderer flat-shades each primitive with a single colour, so
// all lighting is BAKED on the CPU here: directional sun key + cool sky fill +
// warm/cool temperature grade + distance fog, evaluated per face/per surface.
//
// Pure header: no game globals. Callers pass world normal + camera distance +
// daylight (0.58 night .. 1.0 midday). Read-only to every module.
#ifndef LL_ART_H
#define LL_ART_H

#include <math.h>

// #region camera framing (authored fixed 3/4 diorama angle)
#define LL_CAM_PITCH 0.70F
#define LL_CAM_YAW 0.72F
#define LL_CAM_DIST 19.5F
// #endregion

// #region lighting tokens
// Direction TOWARD the sun (already unit length). High, angled to +x/+z so
// vertical facets pick up clear light/shadow variation under the 3/4 camera.
static const float LL_SUN_DIR[3] = {0.40F, 0.78F, 0.48F};

#define LL_AMBIENT 0.46F      // shadow floor (0.35..0.5)
#define LL_SUN_STRENGTH 0.60F // direct key contribution at full facing
#define LL_SKYFILL 0.13F      // cool bounce on up-facing surfaces

// Warm highlight / cool shadow temperature split (the "golden hour" grade).
static const float LL_TINT_WARM[3] = {1.08F, 1.00F, 0.88F};
static const float LL_TINT_COOL[3] = {0.88F, 0.94F, 1.12F};
// #endregion

// #region sky + fog tokens
// Day zenith->horizon (vertical gradient drawn as stacked bands). Night shifts
// toward navy. Fog == horizon colour so distant lots melt into the sky.
static const float LL_SKY_ZENITH_DAY[3] = {0.24F, 0.49F, 0.85F};
static const float LL_SKY_HORIZON_DAY[3] = {0.80F, 0.87F, 0.91F};
static const float LL_SKY_ZENITH_NIGHT[3] = {0.05F, 0.07F, 0.16F};
static const float LL_SKY_HORIZON_NIGHT[3] = {0.16F, 0.18F, 0.30F};

#define LL_FOG_START 14.0F  // world units from camera target before haze begins
#define LL_FOG_FULL 78.0F   // fully hazed beyond this
#define LL_FOG_MAX 0.82F    // never fully erase distant geometry

// Soft contact-shadow (fake AO) quad colour under objects/sims.
static const float LL_AO_RGBA[4] = {0.06F, 0.08F, 0.12F, 0.34F};
// #endregion

static inline float ll_clamp01(float v) { return v < 0.0F ? 0.0F : (v > 1.0F ? 1.0F : v); }

// daylight (0.58..1.0) -> 0..1 day strength.
static inline float ll_daynorm(float daylight) { return ll_clamp01((daylight - 0.58F) / 0.42F); }

// #region core surface lighting
// Bake directional sun + sky fill + warm/cool grade into a base colour given the
// surface world normal. This is the one function every opaque draw path uses.
static inline void ll_shade_n(float out[4], float r, float g, float b, float a,
                              const float n[3], float daylight) {
    float nl = n[0] * LL_SUN_DIR[0] + n[1] * LL_SUN_DIR[1] + n[2] * LL_SUN_DIR[2];
    float key = nl < 0.0F ? 0.0F : nl; // direct sun term
    float skyfill = (0.5F + 0.5F * n[1]) * LL_SKYFILL;
    float dn = ll_daynorm(daylight);

    // Sun weakens + warmth fades at night; ambient stays so nothing goes black.
    float sun = LL_SUN_STRENGTH * (0.35F + 0.65F * dn);
    float light = LL_AMBIENT + sun * key + skyfill;

    // Temperature: lerp cool(shadow) -> warm(lit) by the key term, fading to
    // neutral-cool at night.
    float tw = key * (0.55F + 0.45F * dn);
    float tr = LL_TINT_COOL[0] + (LL_TINT_WARM[0] - LL_TINT_COOL[0]) * tw;
    float tg = LL_TINT_COOL[1] + (LL_TINT_WARM[1] - LL_TINT_COOL[1]) * tw;
    float tb = LL_TINT_COOL[2] + (LL_TINT_WARM[2] - LL_TINT_COOL[2]) * tw;

    float dim = 0.60F + 0.40F * dn; // global night dim
    out[0] = ll_clamp01(r * light * tr * dim);
    out[1] = ll_clamp01(g * light * tg * dim);
    out[2] = ll_clamp01(b * light * tb * dim);
    out[3] = a;
}

// Flat-lit fallback for shapes whose normal isn't worth computing (small props,
// billboards): a gentle top-down key so they don't read as unlit.
static inline void ll_shade_flat(float out[4], float r, float g, float b, float a, float daylight) {
    const float up[3] = {0.0F, 1.0F, 0.0F};
    ll_shade_n(out, r, g, b, a, up, daylight);
}

// Face normal from three world-space triangle verts (CCW).
static inline void ll_face_normal(const float a[3], const float b[3], const float c[3], float out[3]) {
    float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    float vx = c[0] - a[0], vy = c[1] - a[1], vz = c[2] - a[2];
    float nx = uy * vz - uz * vy;
    float ny = uz * vx - ux * vz;
    float nz = ux * vy - uy * vx;
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len < 1e-6F) {
        out[0] = 0.0F; out[1] = 1.0F; out[2] = 0.0F;
        return;
    }
    out[0] = nx / len; out[1] = ny / len; out[2] = nz / len;
}
// #endregion

// #region fog (aerial perspective)
static inline void ll_sky_color(float t01, float daylight, float out[3]) {
    float dn = ll_daynorm(daylight);
    float t = ll_clamp01(t01); // 0 = horizon, 1 = zenith
    for (int i = 0; i < 3; i++) {
        float day = LL_SKY_HORIZON_DAY[i] + (LL_SKY_ZENITH_DAY[i] - LL_SKY_HORIZON_DAY[i]) * t;
        float night = LL_SKY_HORIZON_NIGHT[i] + (LL_SKY_ZENITH_NIGHT[i] - LL_SKY_HORIZON_NIGHT[i]) * t;
        out[i] = night + (day - night) * dn;
    }
}

// Mix an already-shaded colour toward the horizon/fog colour by camera distance.
static inline void ll_fog_mix(float io[4], float dist, float daylight) {
    float t = (dist - LL_FOG_START) / (LL_FOG_FULL - LL_FOG_START);
    t = ll_clamp01(t) * LL_FOG_MAX;
    if (t <= 0.0F) {
        return;
    }
    float fog[3];
    ll_sky_color(0.18F, daylight, fog); // near-horizon haze colour
    io[0] = io[0] + (fog[0] - io[0]) * t;
    io[1] = io[1] + (fog[1] - io[1]) * t;
    io[2] = io[2] + (fog[2] - io[2]) * t;
}
// #endregion

#endif // LL_ART_H
