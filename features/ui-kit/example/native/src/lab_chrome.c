#include "lab_chrome.h"

#include "features/ui_kit/ui_kit.h"

static Clay_Color clay_color(uint32_t abgr) {
    return (Clay_Color){(float)(abgr & 0xFFU), (float)((abgr >> 8) & 0xFFU), (float)((abgr >> 16) & 0xFFU),
                        (float)((abgr >> 24) & 0xFFU)};
}

void lab_swatch(nt_ui_context_t *ctx, uint32_t abgr, float size) {
    ui_kit_plate_begin(ctx, &(Clay_ElementDeclaration){.layout = {.sizing = {CLAY_SIZING_FIXED(size), CLAY_SIZING_FIXED(size)}}}, abgr);
    ui_kit_plate_end(ctx);
}

void lab_section(nt_ui_context_t *ctx, const char *title) {
    const ui_metrics_t m = ui_metrics();
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}, .padding = {.top = (uint16_t)m.gap}}}) {
        ui_kit_label(ctx, title, &g_ui_theme.heading);
    }
}

void lab_ground(nt_ui_context_t *ctx, const char *id) {
    (void)ctx;
    CLAY({.id = (Clay_ElementId){.id = nt_ui_id(id)},
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .zIndex = -1,
                       .attachPoints = {.element = CLAY_ATTACH_POINT_LEFT_TOP, .parent = CLAY_ATTACH_POINT_LEFT_TOP},
                       .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH},
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}},
          .backgroundColor = clay_color(ui_theme_tokens()->inset),
          .userData = NT_UI_CLAY_DATA(UI_LAYER_SCRIM)}) {}
}
