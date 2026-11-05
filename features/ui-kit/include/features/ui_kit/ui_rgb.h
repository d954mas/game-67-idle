#ifndef FEATURE_UI_KIT_RGB_H
#define FEATURE_UI_KIT_RGB_H

// Arithmetic on 0xRRGGBB, the notation the shape styles take. No engine types.

#include <stdint.h>

static inline float ui_rgb_clamp01(float v) { return v < 0.0F ? 0.0F : v > 1.0F ? 1.0F : v; }

// 0xRRGGBB at an opacity as the engine's packed tint, 0xAABBGGRR.
static inline uint32_t ui_rgb_tint(uint32_t rgb, float alpha) {
    const uint32_t a = (uint32_t)(ui_rgb_clamp01(alpha) * 255.0F + 0.5F);
    return (a << 24U) | ((rgb & 0xFFU) << 16U) | (rgb & 0xFF00U) | ((rgb >> 16U) & 0xFFU);
}

// A colour at `shade` of its brightness, saturating.
static inline uint32_t ui_rgb_shaded(uint32_t rgb, float shade) {
    uint32_t out = 0U;
    for (int shift = 0; shift < 24; shift += 8) {
        const float c = (float)((rgb >> shift) & 0xFFU) * shade;
        out |= (uint32_t)(c > 255.0F ? 255.0F : c) << shift;
    }
    return out;
}

// A colour `share` of the way to white.
static inline uint32_t ui_rgb_lighter(uint32_t rgb, float share) {
    uint32_t out = 0U;
    for (int shift = 0; shift < 24; shift += 8) {
        const float c = (float)((rgb >> shift) & 0xFFU);
        out |= (uint32_t)(c + (255.0F - c) * ui_rgb_clamp01(share) + 0.5F) << shift;
    }
    return out;
}

// Two colours multiplied channel by channel, as a tint multiplies art.
static inline uint32_t ui_rgb_multiplied(uint32_t a, uint32_t b) {
    uint32_t out = 0U;
    for (int shift = 0; shift < 24; shift += 8) {
        const float c = (float)((a >> shift) & 0xFFU) * (float)((b >> shift) & 0xFFU) / 255.0F;
        out |= (uint32_t)(c + 0.5F) << shift;
    }
    return out;
}

#endif /* FEATURE_UI_KIT_RGB_H */
