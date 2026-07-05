#ifndef RB_DARK_RPG_UI_TUTORIAL_CALLOUT_H
#define RB_DARK_RPG_UI_TUTORIAL_CALLOUT_H

#include "clay.h"
#include "ui/nt_ui.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum tutorial_callout_anchor_t {
    TUTORIAL_CALLOUT_ANCHOR_LEFT_TOP,
    TUTORIAL_CALLOUT_ANCHOR_CENTER_TOP,
    TUTORIAL_CALLOUT_ANCHOR_CENTER,
    TUTORIAL_CALLOUT_ANCHOR_CENTER_BOTTOM,
    TUTORIAL_CALLOUT_ANCHOR_RIGHT_BOTTOM,
} tutorial_callout_anchor_t;

typedef struct tutorial_callout_style_t {
    float width;
    float height;
    float font_size;
    Clay_Color background;
    Clay_Color border;
    Clay_Color text;
    Clay_Color shadow;
    uint16_t padding_left;
    uint16_t padding_right;
    uint16_t padding_top;
    uint16_t padding_bottom;
    float corner_radius;
    float tail_width;
    float tail_height;
    float tail_offset_x;
    float tail_offset_y;
} tutorial_callout_style_t;

typedef struct tutorial_callout_desc_t {
    bool visible;
    uint32_t slot;
    const char *text;
    tutorial_callout_anchor_t element_anchor;
    tutorial_callout_anchor_t parent_anchor;
    float offset_x;
    float offset_y;
    tutorial_callout_style_t style;
} tutorial_callout_desc_t;

typedef struct tutorial_finger_desc_t {
    bool visible;
    uint32_t slot;
    float offset_x;
    float offset_y;
    float size;
    uint8_t flip_bits;
} tutorial_finger_desc_t;

tutorial_callout_style_t tutorial_callout_default_style(bool compact, float max_width);
void tutorial_callout_ui(nt_ui_context_t *ctx, const tutorial_callout_desc_t *desc);
void tutorial_finger_ui(nt_ui_context_t *ctx, const tutorial_finger_desc_t *desc);

#endif
