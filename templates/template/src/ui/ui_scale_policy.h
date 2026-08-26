#ifndef GAME_UI_SCALE_POLICY_H
#define GAME_UI_SCALE_POLICY_H

typedef struct {
    float width;
    float height;
} ui_scale_reference_t;

static inline float ui_scale_policy_clampf(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static inline ui_scale_reference_t
ui_scale_reference_for_framebuffer(float framebuffer_width,
                                   float framebuffer_height,
                                   float dpr) {
    if (framebuffer_width <= 0.0F || framebuffer_height <= 0.0F) {
        return (ui_scale_reference_t){1280.0F, 720.0F};
    }
    const float pixel_ratio = dpr > 0.0F ? dpr : 1.0F;
    const float css_width = framebuffer_width / pixel_ratio;
    const float css_height = framebuffer_height / pixel_ratio;
    const float css_short = css_width < css_height ? css_width : css_height;
    const float logical_short = ui_scale_policy_clampf(css_short, 360.0F, 720.0F);
    const float css_to_logical = css_short / logical_short;
    return (ui_scale_reference_t){
        css_width / css_to_logical,
        css_height / css_to_logical,
    };
}

#endif
