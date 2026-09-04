#ifndef GAME_UI_LOC_WIDGETS_H
#define GAME_UI_LOC_WIDGETS_H

#include "ui/nt_ui.h"
#include "ui/nt_ui_label.h"

#include "features/localization/loc.h"
#include "features/ui_kit/ui_kit.h"

// LocStr-taking wrappers for the text entry points THIS TEMPLATE USES. Every UI
// surface calls these; loc_widgets.c is the only place a LocStr becomes a raw
// pointer again. That turns the localization rule into a grep on a function
// name instead of a heuristic over string contents: outside this file, a call
// to nt_ui_label or nt_text_renderer_draw is the thing to look for.
//
// Deliberately unlocalized text still compiles, through the explicit and
// greppable loc_raw() -- src/ui/platform_sdk_debug.c is the template's only
// user, because a developer diagnostic is not player text.
//
// NOT WRAPPED: the engine's other text-taking entry points (nt_ui_checkbox,
// nt_ui_slider_float's label, the combo/menu family, rich text, tooltips,
// nt_ui_fit_*). Adding a call site means adding the wrapper FIRST. One current
// exception carries its reason at the call site: settings_screen.c passes NULL
// as nt_ui_slider_float's label, so no text reaches the engine there.
//
// nt_ui_id takes a string that is an IDENTIFIER, never text -- localizing one
// would change a widget's identity between languages.

void loc_label(nt_ui_context_t *ctx, const nt_ui_element_data_t *data, LocStr text,
               const nt_ui_label_style_t *style);

// The ui-kit widgets take a raw pointer because the kit does not know how its
// consumer localizes. These are the bridge, and they are the reason the rule
// still holds: the kit's text entry points are reached only from this file.
void loc_kit_label(nt_ui_context_t *ctx, LocStr text, const nt_ui_label_style_t *style);
void loc_kit_label_shadowed(nt_ui_context_t *ctx, const char *id, int slot, LocStr text,
                            const nt_ui_label_style_t *style);

// The immediate text renderer, for surfaces that draw outside the nt_ui tree.
void loc_text_draw(LocStr text, const float model[16], float size, const float color[4],
                   float letter_tracking, float line_leading);

#endif /* GAME_UI_LOC_WIDGETS_H */
