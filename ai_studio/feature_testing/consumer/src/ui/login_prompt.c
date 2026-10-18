#include "ui/login_prompt.h"

#include "features/platform_sdk/platform_sdk.h"
#include "features/ui_kit/ui_kit.h"
#include "game_scenes.h"
#include "game_state_events.gen.h"
#include "clay.h"
#include "ui/loc_widgets.h"
#include "ui/theme.h"

#include "loc_strings.gen.h"

#include <string.h>

// Which screen's button opened the modal; the tap is reported from there.
static const char *s_source = "";

// The row keeps its name in the tree, so a bot can find it like a CLAY_ID one.
static Clay_ElementId named_id(const char *s) {
    return Clay_GetElementId((Clay_String){.isStaticallyAllocated = true, .length = (int32_t)strlen(s), .chars = s});
}

bool login_prompt_wanted(void) {
    return platform_sdk_auth_supported() && !platform_sdk_authorized();
}

void login_prompt_row(nt_ui_context_t *ctx, const ui_metrics_t *m, const char *row_id, const char *button_id,
                      const char *source, bool interactive) {
    CLAY({.id = named_id(row_id),
          .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .childGap = (uint16_t)m->gap,
                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}) {
        nt_ui_label_style_t hint = g_ui_theme.hint;
        hint.wrap_mode = CLAY_TEXT_WRAP_WORDS;
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
            loc_kit_label(ctx, loc_login_hint(), &hint);
        }
        // One dialog at a time: the button waits while the portal's is open.
        const bool can_tap = interactive && !platform_sdk_login_pending();
        CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), ui_kit_hit_height(m)}}}) {
            ui_kit_button_begin(ctx, nt_ui_id(button_id), &g_ui_theme.button_confirm, can_tap, NULL);
            loc_kit_label(ctx, loc_login_button(), &g_ui_theme.button_label_action);
            if (ui_kit_button_end(ctx) && can_tap) {
                s_source = source;
                (void)game_scenes_show_login_confirm();
            }
        }
    }
}

static bool confirm_button(nt_ui_context_t *ctx, const ui_metrics_t *m, const char *id, LocStr text, bool primary,
                           bool interactive) {
    bool tapped = false;
    // Equal halves: neither answer is the one the thumb is steered to.
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), ui_kit_hit_height(m)}}}) {
        ui_kit_button_begin(ctx, nt_ui_id(id), primary ? &g_ui_theme.button_confirm : &g_ui_theme.button, interactive,
                            NULL);
        loc_kit_label(ctx, text, primary ? &g_ui_theme.button_label_action : &g_ui_theme.button_label);
        tapped = ui_kit_button_end(ctx) && interactive;
    }
    return tapped;
}

void login_prompt_draw_confirm(nt_ui_context_t *ctx, bool interactive) {
    const ui_metrics_t m = ui_metrics();
    ui_kit_scrim(ctx, true);
    ui_kit_panel_begin(ctx, &(Clay_ElementDeclaration){
                                .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                                             .attachPoints = {.element = CLAY_ATTACH_POINT_CENTER_CENTER,
                                                              .parent = CLAY_ATTACH_POINT_CENTER_CENTER},
                                             .offset = {(m.safe_l - m.safe_r) * .5F, (m.safe_t - m.safe_b) * .5F}},
                                .layout = {.sizing = {CLAY_SIZING_FIXED(m.panel_w), CLAY_SIZING_FIT(0)},
                                           .padding = CLAY_PADDING_ALL((uint16_t)m.pad),
                                           .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                           .childGap = (uint16_t)m.gap,
                                           .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}});
    loc_kit_label(ctx, loc_login_confirm_title(), &g_ui_theme.title);
    nt_ui_label_style_t body = g_ui_theme.label;
    body.wrap_mode = CLAY_TEXT_WRAP_WORDS;
    body.align = CLAY_TEXT_ALIGN_CENTER;
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}}) {
        loc_kit_label(ctx, loc_login_confirm_body(), &body);
    }
    // The way out is always live; the portal's dialog opens only once, so
    // the go button waits while it is up.
    bool go = false;
    bool later = false;
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .childGap = (uint16_t)m.gap,
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        go = confirm_button(ctx, &m, "login_confirm/go", loc_login_button(), true,
                            interactive && !platform_sdk_login_pending());
        later = confirm_button(ctx, &m, "login_confirm/later", loc_login_later(), false, interactive);
    }
    ui_kit_panel_end(ctx);
    if (go) {
        (void)game_emit_login_tap(s_source);
        (void)platform_sdk_login();
        (void)game_scenes_close_login_confirm();
    } else if (later) {
        (void)game_scenes_close_login_confirm();
    }
}
