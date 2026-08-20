#include "features/software_cursor/software_cursor.h"

#include <assert.h>
#include <string.h>

static float clamp01(float value) {
    if (value < 0.0F) return 0.0F;
    if (value > 1.0F) return 1.0F;
    return value;
}

software_cursor_theme_t software_cursor_theme_defaults(void) {
    software_cursor_theme_t theme;
    memset(&theme, 0, sizeof theme);
    theme.follow_response = 18.0F;
    theme.click_pulse_seconds = 0.12F;
    for (int i = 0; i < SOFTWARE_CURSOR_INTENT_COUNT; ++i) {
        theme.styles[i].width = 48.0F;
        theme.styles[i].height = 48.0F;
        theme.styles[i].pressed_scale = 0.9F;
    }
    return theme;
}

void software_cursor_init(
    software_cursor_t *cursor, const software_cursor_theme_t *theme,
    software_cursor_native_mode_fn native_mode, void *userdata) {
    assert(cursor != NULL);
    assert(theme != NULL);
    memset(cursor, 0, sizeof *cursor);
    cursor->theme = *theme;
    cursor->native_mode = native_mode;
    cursor->native_mode_userdata = userdata;
    cursor->enabled = true;
    cursor->intent = SOFTWARE_CURSOR_INTENT_DEFAULT;
    if (native_mode != NULL) native_mode(true, userdata);
}

void software_cursor_shutdown(software_cursor_t *cursor) {
    assert(cursor != NULL);
    software_cursor_set_enabled(cursor, false);
}

void software_cursor_set_enabled(software_cursor_t *cursor, bool enabled) {
    assert(cursor != NULL);
    if (cursor->enabled == enabled) return;
    cursor->enabled = enabled;
    cursor->presentation.visible = false;
    cursor->position_ready = false;
    if (cursor->native_mode != NULL) {
        cursor->native_mode(enabled, cursor->native_mode_userdata);
    }
}

void software_cursor_set_intent(
    software_cursor_t *cursor, software_cursor_intent_t intent) {
    assert(cursor != NULL);
    assert(intent >= SOFTWARE_CURSOR_INTENT_DEFAULT);
    assert(intent < SOFTWARE_CURSOR_INTENT_COUNT);
    cursor->intent = intent;
}

static const software_cursor_style_t *resolve_style(
    const software_cursor_t *cursor) {
    const software_cursor_style_t *style = &cursor->theme.styles[cursor->intent];
    if (style->idle_visual == 0U) {
        style = &cursor->theme.styles[SOFTWARE_CURSOR_INTENT_DEFAULT];
    }
    return style;
}

void software_cursor_update(
    software_cursor_t *cursor, const software_cursor_input_t *input, float dt) {
    assert(cursor != NULL);
    assert(input != NULL);
    assert(dt >= 0.0F);
    if (!cursor->enabled || !input->active) {
        cursor->presentation.visible = false;
        cursor->position_ready = false;
        return;
    }

    if (!cursor->position_ready) {
        cursor->presentation.x = input->x;
        cursor->presentation.y = input->y;
        cursor->position_ready = true;
    } else {
        const float alpha = clamp01(dt * cursor->theme.follow_response);
        cursor->presentation.x += (input->x - cursor->presentation.x) * alpha;
        cursor->presentation.y += (input->y - cursor->presentation.y) * alpha;
    }

    if (input->pressed) {
        cursor->click_pulse_remaining = cursor->theme.click_pulse_seconds;
    } else if (cursor->click_pulse_remaining > 0.0F) {
        cursor->click_pulse_remaining -= dt;
        if (cursor->click_pulse_remaining < 0.0F) {
            cursor->click_pulse_remaining = 0.0F;
        }
    }

    const software_cursor_style_t *style = resolve_style(cursor);
    const bool pressed = input->down || cursor->click_pulse_remaining > 0.0F;
    cursor->presentation.visible = style->idle_visual != 0U;
    cursor->presentation.pressed = pressed;
    cursor->presentation.visual =
        pressed && style->pressed_visual != 0U
            ? style->pressed_visual
            : style->idle_visual;
    cursor->presentation.width = style->width;
    cursor->presentation.height = style->height;
    cursor->presentation.hotspot_x = style->hotspot_x;
    cursor->presentation.hotspot_y = style->hotspot_y;
    cursor->presentation.scale =
        pressed && style->pressed_scale > 0.0F ? style->pressed_scale : 1.0F;
    cursor->presentation.click_pulse =
        cursor->theme.click_pulse_seconds > 0.0F
            ? clamp01(
                  cursor->click_pulse_remaining
                  / cursor->theme.click_pulse_seconds)
            : 0.0F;
}

const software_cursor_presentation_t *software_cursor_presentation(
    const software_cursor_t *cursor) {
    assert(cursor != NULL);
    return &cursor->presentation;
}
