#ifndef FEATURE_SOFTWARE_CURSOR_H
#define FEATURE_SOFTWARE_CURSOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum software_cursor_intent_t {
    SOFTWARE_CURSOR_INTENT_DEFAULT = 0,
    SOFTWARE_CURSOR_INTENT_POINT,
    SOFTWARE_CURSOR_INTENT_GRAB,
    SOFTWARE_CURSOR_INTENT_FORBIDDEN,
    SOFTWARE_CURSOR_INTENT_COUNT,
} software_cursor_intent_t;

typedef struct software_cursor_style_t {
    uint32_t idle_visual;
    uint32_t pressed_visual;
    float width;
    float height;
    float hotspot_x;
    float hotspot_y;
    float pressed_scale;
} software_cursor_style_t;

typedef struct software_cursor_theme_t {
    software_cursor_style_t styles[SOFTWARE_CURSOR_INTENT_COUNT];
    float follow_response;
    float click_pulse_seconds;
} software_cursor_theme_t;

typedef struct software_cursor_input_t {
    bool active;
    bool down;
    bool pressed;
    bool released;
    float x;
    float y;
} software_cursor_input_t;

typedef struct software_cursor_presentation_t {
    bool visible;
    bool pressed;
    uint32_t visual;
    float x;
    float y;
    float width;
    float height;
    float hotspot_x;
    float hotspot_y;
    float scale;
    float click_pulse;
} software_cursor_presentation_t;

typedef void (*software_cursor_native_mode_fn)(bool hidden, void *userdata);

typedef struct software_cursor_t {
    software_cursor_theme_t theme;
    software_cursor_presentation_t presentation;
    software_cursor_native_mode_fn native_mode;
    void *native_mode_userdata;
    software_cursor_intent_t intent;
    float click_pulse_remaining;
    bool enabled;
    bool position_ready;
} software_cursor_t;

software_cursor_theme_t software_cursor_theme_defaults(void);
void software_cursor_init(
    software_cursor_t *cursor, const software_cursor_theme_t *theme,
    software_cursor_native_mode_fn native_mode, void *userdata);
void software_cursor_shutdown(software_cursor_t *cursor);
void software_cursor_set_enabled(software_cursor_t *cursor, bool enabled);
void software_cursor_set_intent(
    software_cursor_t *cursor, software_cursor_intent_t intent);
void software_cursor_update(
    software_cursor_t *cursor, const software_cursor_input_t *input, float dt);
const software_cursor_presentation_t *software_cursor_presentation(
    const software_cursor_t *cursor);

#endif
