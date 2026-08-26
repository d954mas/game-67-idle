#include "ui/focus_prompt_ui.h"

#include "ui/focus_prompt_policy.h"
#include "ui/loc_widgets.h"
#include "ui/nt_ui_panel.h"
#include "ui/theme.h"

#include "loc_strings.gen.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

/* clang-format off */
EM_JS(int, focus_prompt_canvas_focused, (void), {
    return typeof window.__gameCanvasHasFocus === 'function' &&
                   window.__gameCanvasHasFocus()
               ? 1
               : 0;
})

EM_JS(int, focus_prompt_has_fine_pointer, (void), {
    return typeof window.__gameHasFinePointer === 'function' &&
                   window.__gameHasFinePointer()
               ? 1
               : 0;
})
/* clang-format on */
#endif

#define FOCUS_PROMPT_SCRIM_Z 32761
#define FOCUS_PROMPT_CARD_Z 32762
#define FOCUS_PROMPT_BG_LAYER 90
#define FOCUS_PROMPT_TEXT_LAYER 91

static FocusPromptGate s_gate;

static bool focus_required(void) {
#if defined(__EMSCRIPTEN__)
    return focus_prompt_should_show(true, focus_prompt_canvas_focused() != 0,
                                    focus_prompt_has_fine_pointer() != 0);
#else
    return false;
#endif
}

void focus_prompt_ui_update(bool pointer_down) {
    s_gate = focus_prompt_gate_next(s_gate, focus_required(), pointer_down);
}

bool focus_prompt_ui_visible(void) { return s_gate.visible; }

void focus_prompt_ui_build(nt_ui_context_t *ctx) {
    if (!s_gate.visible) {
        return;
    }

    CLAY({.id = CLAY_ID("focus_prompt/scrim"),
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                       .zIndex = FOCUS_PROMPT_SCRIM_Z},
          .backgroundColor = {10.0F, 13.0F, 25.0F, 174.0F},
          .userData = NT_UI_CLAY_DATA(FOCUS_PROMPT_BG_LAYER),
          .layout = {.sizing = {CLAY_SIZING_GROW(0),
                                CLAY_SIZING_GROW(0)}}}) {
        nt_ui_block_pointer(ctx, nt_ui_id("focus_prompt/scrim"), NULL);
    }

    nt_ui_panel_begin(
        ctx, NT_UI_DATA_LAYER(FOCUS_PROMPT_BG_LAYER), &g_theme.panel_region,
        &g_theme.panel_img,
        &(Clay_ElementDeclaration){
            .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                         .zIndex = FOCUS_PROMPT_CARD_Z,
                         .attachPoints = {
                             .element = CLAY_ATTACH_POINT_CENTER_CENTER,
                             .parent = CLAY_ATTACH_POINT_CENTER_CENTER}},
            .layout = {.sizing = {CLAY_SIZING_FIT(280.0F, 380.0F),
                                  CLAY_SIZING_FIXED(116.0F)},
                       .padding = CLAY_PADDING_ALL(22),
                       .childAlignment = {CLAY_ALIGN_X_CENTER,
                                          CLAY_ALIGN_Y_CENTER}}});
    nt_ui_label_style_t label = g_theme.title;
    label.font_size = 25.0F;
    label.align = CLAY_TEXT_ALIGN_CENTER;
    loc_label(ctx, NT_UI_DATA_LAYER(FOCUS_PROMPT_TEXT_LAYER),
              loc_focus_click_to_continue(), &label);
    nt_ui_panel_end(ctx);
}
