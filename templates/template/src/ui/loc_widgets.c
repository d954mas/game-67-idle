#include "ui/loc_widgets.h"

#include "renderers/nt_text_renderer.h"

// The one place a LocStr becomes a raw pointer. No generated accessor can hand
// back NULL (loc_get falls all the way to the bare key), so this is belt over
// braces -- but it keeps a hand-built LocStr from reaching the engine's strlen.
static const char *text_of(LocStr text) { return text.s ? text.s : ""; }

void loc_label(nt_ui_context_t *ctx, const nt_ui_element_data_t *data, LocStr text,
               const nt_ui_label_style_t *style) {
    nt_ui_label(ctx, data, text_of(text), style);
}

void loc_text_draw(LocStr text, const float model[16], float size, const float color[4],
                   float letter_tracking, float line_leading) {
    nt_text_renderer_draw(text_of(text), model, size, color, letter_tracking, line_leading);
}
