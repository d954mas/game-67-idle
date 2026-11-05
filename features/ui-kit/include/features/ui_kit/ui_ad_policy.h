#ifndef FEATURE_UI_KIT_AD_POLICY_H
#define FEATURE_UI_KIT_AD_POLICY_H

// The portals' rewarded-ad rule as arithmetic, with no engine types: the ad
// button is blue, and the standard button shows with it, equal or larger, beside
// it or above it.

#include <stdbool.h>
#include <stdint.h>

// Blue is the strongest channel of 0xRRGGBB: what a portal reads as "ad".
static inline bool ui_ad_rgb_is_blue(uint32_t rgb) {
    const uint32_t r = (rgb >> 16U) & 0xFFU;
    const uint32_t g = (rgb >> 8U) & 0xFFU;
    const uint32_t b = rgb & 0xFFU;
    return b > r && b > g;
}

typedef struct ui_ad_split_t {
    bool row;   // side by side, standard first; false = stacked, standard on top
    float each; // both buttons' width: equal, so the standard is never the smaller
} ui_ad_split_t;

// Two equal buttons side by side while each half stays `min_w` wide, else stacked.
static inline ui_ad_split_t ui_ad_split(float width, float gap, float min_w) {
    const float half = (width - gap) * 0.5F;
    return half >= min_w ? (ui_ad_split_t){true, half} : (ui_ad_split_t){false, width};
}

#endif /* FEATURE_UI_KIT_AD_POLICY_H */
