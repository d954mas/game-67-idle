#include "features/software_cursor/software_cursor_samples.h"

static void set_all_styles(
    software_cursor_theme_t *theme, software_cursor_style_t style) {
    for (int i = 0; i < SOFTWARE_CURSOR_INTENT_COUNT; ++i) {
        theme->styles[i] = style;
    }
}

software_cursor_theme_t software_cursor_sample_pointer_theme(void) {
    software_cursor_theme_t theme = software_cursor_theme_defaults();
    theme.follow_response = 20.0F;
    set_all_styles(&theme, (software_cursor_style_t){
        .idle_visual = SOFTWARE_CURSOR_SAMPLE_POINTER_IDLE,
        .pressed_visual = SOFTWARE_CURSOR_SAMPLE_POINTER_PRESS,
        .width = 46.0F,
        .height = 46.0F,
        .hotspot_x = 3.0F,
        .hotspot_y = 3.0F,
        .pressed_scale = 0.84F,
    });
    return theme;
}

software_cursor_theme_t software_cursor_sample_finger_theme(void) {
    software_cursor_theme_t theme = software_cursor_theme_defaults();
    theme.follow_response = 16.0F;
    theme.click_pulse_seconds = 0.16F;
    set_all_styles(&theme, (software_cursor_style_t){
        .idle_visual = SOFTWARE_CURSOR_SAMPLE_FINGER_OPEN,
        .pressed_visual = SOFTWARE_CURSOR_SAMPLE_FINGER_PRESS,
        .width = 72.0F,
        .height = 72.0F,
        .hotspot_x = 18.0F,
        .hotspot_y = 8.0F,
        .pressed_scale = 0.88F,
    });
    return theme;
}
