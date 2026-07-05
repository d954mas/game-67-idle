#include "ui/title_card.h"

#include "clay.h"
#include "ui/nt_ui_label.h"

#define LAYER_TITLE_BG 0
#define LAYER_TITLE_TEXT_SHADOW 1
#define LAYER_TITLE_TEXT 2
#define TITLE_CARD_Z_INDEX 900

/* Total ~2.6s: fade in, hold, fade out, then never draws again this session. */
#define TITLE_CARD_FADE_IN_SECONDS 0.6F
#define TITLE_CARD_HOLD_SECONDS 1.3F
#define TITLE_CARD_FADE_OUT_SECONDS 0.7F
#define TITLE_CARD_TOTAL_SECONDS \
    (TITLE_CARD_FADE_IN_SECONDS + TITLE_CARD_HOLD_SECONDS + TITLE_CARD_FADE_OUT_SECONDS)

static bool s_started;
static bool s_done;
static float s_start_time;

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float title_card_alpha(float elapsed) {
    if (elapsed < TITLE_CARD_FADE_IN_SECONDS) {
        return clampf(elapsed / TITLE_CARD_FADE_IN_SECONDS, 0.0F, 1.0F);
    }
    const float hold_end = TITLE_CARD_FADE_IN_SECONDS + TITLE_CARD_HOLD_SECONDS;
    if (elapsed < hold_end) {
        return 1.0F;
    }
    const float fade_out_t = (elapsed - hold_end) / TITLE_CARD_FADE_OUT_SECONDS;
    return 1.0F - clampf(fade_out_t, 0.0F, 1.0F);
}

static nt_ui_label_style_t label_style(float font_size, float r, float g, float b, float a) {
    return (nt_ui_label_style_t){.font_id = 0, .font_size = font_size, .color = {r, g, b, a}};
}

static void title_card_label(nt_ui_context_t *ctx, int slot, const char *text, const nt_ui_label_style_t *style,
                             float shadow_alpha) {
    nt_ui_label_style_t shadow = *style;
    shadow.color = (Clay_Color){6.0F, 4.0F, 3.0F, shadow_alpha};

    CLAY({.id = CLAY_IDI("title_card/label", slot),
          .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
        CLAY({.id = CLAY_IDI("title_card/label_shadow", slot),
              .floating = {.attachTo = CLAY_ATTACH_TO_PARENT,
                           .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                           .offset = {1.0F, 1.0F}},
              .layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}}}) {
            nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TITLE_TEXT_SHADOW), text, &shadow);
        }
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TITLE_TEXT), text, style);
    }
}

void title_card_ui(nt_ui_context_t *ctx, World *w) {
    if (!ctx || !w || s_done) {
        return;
    }
    if (!s_started) {
        s_started = true;
        s_start_time = w->time_seconds;
    }

    const float elapsed = w->time_seconds - s_start_time;
    if (elapsed >= TITLE_CARD_TOTAL_SECONDS) {
        s_done = true;
        return;
    }

    const float t = title_card_alpha(elapsed);
    if (t <= 0.0F) {
        return;
    }

    float layout_w = 0.0F;
    float layout_h = 0.0F;
    nt_ui_context_layout_size(ctx, &layout_w, &layout_h);
    const bool portrait = layout_h > layout_w;

    const float panel_w = portrait ? clampf(layout_w - 40.0F, 240.0F, 380.0F) : 460.0F;
    const float panel_h = portrait ? 112.0F : 104.0F;
    const nt_ui_label_style_t title_style =
        label_style(portrait ? 24.0F : 29.0F, 255.0F, 238.0F, 204.0F, 255.0F * t);
    const nt_ui_label_style_t subtitle_style =
        label_style(portrait ? 13.0F : 14.0F, 214.0F, 189.0F, 150.0F, 235.0F * t);

    /* Floating, unregistered (no widget/hit target): visually on top but never
     * intercepts pointer input, so it cannot block the guard tap or nav taps. */
    CLAY({.id = CLAY_ID("title_card/root"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER, .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                       .zIndex = TITLE_CARD_Z_INDEX},
          .layout = {.sizing = {CLAY_SIZING_FIXED(panel_w), CLAY_SIZING_FIXED(panel_h)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = 8,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
          .backgroundColor = {9.0F, 6.0F, 5.0F, 168.0F * t},
          .cornerRadius = CLAY_CORNER_RADIUS(6),
          .border = {.color = {150.0F, 104.0F, 54.0F, 190.0F * t}, .width = {1, 1, 1, 1, 0}},
          .userData = NT_UI_CLAY_DATA(LAYER_TITLE_BG)}) {
        title_card_label(ctx, 0, "Дракон не вернулся", &title_style, 190.0F * t);
        title_card_label(ctx, 1, "Мир без тебя - VibeJam #1", &subtitle_style, 150.0F * t);
    }
}
