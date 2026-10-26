#include "features/platform_sdk/platform_sdk_debug.h"

#include "features/platform_sdk/platform_sdk.h"
#include "features/platform_sdk/platform_sdk_mock.h"

#ifndef GAME_PLATFORM_SDK_DEBUG_UI
#define GAME_PLATFORM_SDK_DEBUG_UI 0
#endif

#ifndef PLATFORM_SDK_TARGET_ID
#define PLATFORM_SDK_TARGET_ID 0
#endif

#define PLATFORM_SDK_DEBUG_LOCAL (PLATFORM_SDK_TARGET_ID == 0)
#define PLATFORM_SDK_DEBUG_SURFACE_ENABLED (GAME_PLATFORM_SDK_DEBUG_UI || PLATFORM_SDK_DEBUG_LOCAL)

#if !PLATFORM_SDK_DEBUG_SURFACE_ENABLED
void platform_sdk_debug_init(void) {
    platform_sdk_mock_init();
}

void platform_sdk_debug_draw_ui(nt_ui_context_t *ctx) {
    (void)ctx;
}

void platform_sdk_debug_shutdown(void) {
    platform_sdk_mock_shutdown();
}
#else

#include "clay.h"
#include "features/ui_kit/ui_kit.h"
#include "features/ui_kit/ui_metrics.h"
#include "features/ui_kit/ui_theme.h"
#include "ui/nt_ui_modal.h"

#include <stdbool.h>
#include <stdio.h>

// Developer diagnostics, not player copy: every string here stays unlocalized.
static char s_last_result[128] = "none";
static int s_pause_count;
static int s_resume_count;
static platform_sdk_listener_id_t s_pause_listener;
static platform_sdk_listener_id_t s_resume_listener;

#if GAME_PLATFORM_SDK_DEBUG_UI
static const char *sdk_result_name(platform_sdk_result_t result) {
    switch (result) {
    case PLATFORM_SDK_RESULT_OK:
        return "ok";
    case PLATFORM_SDK_RESULT_NOT_READY:
        return "not_ready";
    case PLATFORM_SDK_RESULT_DESTROYED:
        return "destroyed";
    case PLATFORM_SDK_RESULT_WAITING_FOR_INPUT:
        return "waiting_for_input";
    case PLATFORM_SDK_RESULT_ALREADY_ACTIVE:
        return "already_active";
    case PLATFORM_SDK_RESULT_NOT_ACTIVE:
        return "not_active";
    case PLATFORM_SDK_RESULT_UNSUPPORTED:
        return "unsupported";
    case PLATFORM_SDK_RESULT_BUSY:
        return "busy";
    case PLATFORM_SDK_RESULT_FAILED:
        return "failed";
    }
    return "failed";
}

static void set_request_result(const char *kind, platform_sdk_result_t result) {
    (void)snprintf(s_last_result, sizeof s_last_result, "%s request: %s", kind, sdk_result_name(result));
}
#endif

static const char *ad_reason_name(platform_sdk_ad_reason_t reason) {
    switch (reason) {
    case PLATFORM_SDK_AD_REASON_NONE:
        return "none";
    case PLATFORM_SDK_AD_REASON_UNSUPPORTED:
        return "unsupported";
    case PLATFORM_SDK_AD_REASON_NOT_READY:
        return "not_ready";
    case PLATFORM_SDK_AD_REASON_RATE_LIMITED:
        return "rate_limited";
    case PLATFORM_SDK_AD_REASON_TIMEOUT:
        return "timeout";
    case PLATFORM_SDK_AD_REASON_FAILED:
        return "failed";
    case PLATFORM_SDK_AD_REASON_SKIPPED:
        return "skipped";
    case PLATFORM_SDK_AD_REASON_DECLINED:
        return "declined";
    case PLATFORM_SDK_AD_REASON_COMPLETED:
        return "completed";
    }
    return "failed";
}

static void set_interstitial_result(platform_sdk_ad_result_t result) {
    (void)snprintf(s_last_result, sizeof s_last_result,
                   "interstitial: supported=%d shown=%d reason=%s",
                   result.supported ? 1 : 0, result.shown ? 1 : 0, ad_reason_name(result.reason));
}

static void set_rewarded_result(platform_sdk_rewarded_result_t result) {
    (void)snprintf(s_last_result, sizeof s_last_result,
                   "rewarded: supported=%d shown=%d rewarded=%d reason=%s",
                   result.supported ? 1 : 0, result.shown ? 1 : 0,
                   result.rewarded ? 1 : 0, ad_reason_name(result.reason));
}

static void on_pause(void *userdata) {
    (void)userdata;
    s_pause_count++;
}

static void on_resume(void *userdata) {
    (void)userdata;
    s_resume_count++;
}

void platform_sdk_debug_init(void) {
    platform_sdk_mock_init();
    s_pause_listener = platform_sdk_on_pause(on_pause, NULL);
    s_resume_listener = platform_sdk_on_resume(on_resume, NULL);
}

void platform_sdk_debug_shutdown(void) {
    if (s_pause_listener != 0U) {
        platform_sdk_remove_listener(s_pause_listener);
        s_pause_listener = 0U;
    }
    if (s_resume_listener != 0U) {
        platform_sdk_remove_listener(s_resume_listener);
        s_resume_listener = 0U;
    }
    platform_sdk_mock_shutdown();
}

static bool button(nt_ui_context_t *ctx, const char *id, const char *label, bool enabled) {
    const ui_metrics_t m = ui_metrics();
    bool clicked = false;
    // Diagnostic buttons share the plate's width rather than sizing to their
    // own text: the labels are developer sentences, not player copy.
    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), ui_kit_hit_height(&m)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        ui_kit_button_begin(ctx, nt_ui_id(id), &g_ui_theme.button, enabled, NULL);
        ui_kit_label(ctx, label, &g_ui_theme.row_title);
        clicked = ui_kit_button_end(ctx);
    }
    return clicked;
}

#if GAME_PLATFORM_SDK_DEBUG_UI
static void on_debug_interstitial_done(platform_sdk_ad_result_t result, void *userdata) {
    (void)userdata;
    set_interstitial_result(result);
}

static void on_debug_rewarded_done(platform_sdk_rewarded_result_t result, void *userdata) {
    (void)userdata;
    set_rewarded_result(result);
}

static void draw_debug_panel(nt_ui_context_t *ctx) {
    platform_sdk_capabilities_t caps = platform_sdk_capabilities();
    const bool mock_idle = platform_sdk_mock_flow() == PLATFORM_SDK_MOCK_IDLE;
    char line[160];

    const ui_metrics_t m = ui_metrics();
    ui_kit_panel_begin(ctx, &(Clay_ElementDeclaration){
                                .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                                             .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_BOTTOM,
                                                              .parent = CLAY_ATTACH_POINT_RIGHT_BOTTOM},
                                             .offset = {-(m.margin + m.safe_r), -(m.margin + m.safe_b)}},
                                .layout = {.sizing = {CLAY_SIZING_FIXED(m.panel_w), CLAY_SIZING_FIT(0)},
                                           .padding = CLAY_PADDING_ALL((uint16_t)m.pad),
                                           .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                           .childGap = (uint16_t)m.gap,
                                           .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    ui_kit_label(ctx, "PLATFORM SDK", &g_ui_theme.heading);
    (void)snprintf(line, sizeof line, "target=%s sdk=%s status=%s",
                   platform_sdk_target_name(), platform_sdk_current_name(),
                   sdk_result_name(platform_sdk_status() == PLATFORM_SDK_BOOT_READY ? PLATFORM_SDK_RESULT_OK
                                                                                   : PLATFORM_SDK_RESULT_NOT_READY));
    ui_kit_label(ctx, line, &g_ui_theme.hint);
    (void)snprintf(line, sizeof line, "caps links=%d ads=%d rewarded=%d storage=%d",
                   caps.external_links_allowed ? 1 : 0, caps.ads_supported ? 1 : 0,
                   caps.rewarded_supported ? 1 : 0, caps.storage_supported ? 1 : 0);
    ui_kit_label(ctx, line, &g_ui_theme.hint);
    (void)snprintf(line, sizeof line, "pause=%d resume=%d", s_pause_count, s_resume_count);
    ui_kit_label(ctx, line, &g_ui_theme.hint);
    (void)snprintf(line, sizeof line, "player: %s%s",
                   platform_sdk_authorized() ? platform_sdk_player_name() : "anonymous",
                   platform_sdk_login_pending() ? " (dialog open)" : "");
    ui_kit_label(ctx, line, &g_ui_theme.hint);
    ui_kit_label(ctx, s_last_result, &g_ui_theme.hint);

    if (platform_sdk_auth_supported() && !platform_sdk_authorized()) {
        if (button(ctx, "platform_sdk/debug/login", "Sign in", !platform_sdk_login_pending())) {
            platform_sdk_result_t result = platform_sdk_login();
            if (result != PLATFORM_SDK_RESULT_OK) {
                set_request_result("login", result);
            }
        }
    }

    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 10}}) {
        if (button(ctx, "platform_sdk/debug/interstitial", "Show interstitial ad", mock_idle)) {
            platform_sdk_result_t result = platform_sdk_show_interstitial("debug_test", on_debug_interstitial_done, NULL);
            if (result != PLATFORM_SDK_RESULT_OK) {
                set_request_result("interstitial", result);
            }
        }
        if (button(ctx, "platform_sdk/debug/rewarded", "Show rewarded ad", mock_idle)) {
            platform_sdk_result_t result = platform_sdk_show_rewarded("debug_test", on_debug_rewarded_done, NULL);
            if (result != PLATFORM_SDK_RESULT_OK) {
                set_request_result("rewarded", result);
            }
        }
    }
    ui_kit_panel_end(ctx);
}
#endif

#if PLATFORM_SDK_DEBUG_LOCAL
static const char *auth_reason_name(platform_sdk_auth_reason_t reason) {
    switch (reason) {
    case PLATFORM_SDK_AUTH_REASON_NONE:
        return "none";
    case PLATFORM_SDK_AUTH_REASON_UNSUPPORTED:
        return "unsupported";
    case PLATFORM_SDK_AUTH_REASON_NOT_READY:
        return "not_ready";
    case PLATFORM_SDK_AUTH_REASON_FAILED:
        return "failed";
    case PLATFORM_SDK_AUTH_REASON_DECLINED:
        return "declined";
    case PLATFORM_SDK_AUTH_REASON_ACCEPTED:
        return "accepted";
    }
    return "failed";
}

static void answer_rewarded(platform_sdk_ad_reason_t reason) {
    set_rewarded_result(platform_sdk_mock_answer_rewarded(reason));
}

static void answer_login(bool accepted) {
    const platform_sdk_auth_result_t result = platform_sdk_mock_answer_login(accepted);
    (void)snprintf(s_last_result, sizeof s_last_result, "login: reason=%s", auth_reason_name(result.reason));
}

static void draw_mock_modal(nt_ui_context_t *ctx) {
    const platform_sdk_mock_flow_t flow = platform_sdk_mock_flow();
    bool open = flow != PLATFORM_SDK_MOCK_IDLE;
    nt_ui_modal_style_t style = nt_ui_modal_style_defaults();
    style.flags = 0U;
    style.backdrop_alpha = 0.72F;
    if (!nt_ui_modal_visible(ctx, nt_ui_id("platform_sdk/mock_ad/modal"), &style, &open)) {
        return;
    }

    const ui_metrics_t m = ui_metrics();
    ui_kit_panel_begin(ctx, &(Clay_ElementDeclaration){
                                .layout = {.sizing = {CLAY_SIZING_FIXED(m.panel_w), CLAY_SIZING_FIT(0)},
                                           .padding = CLAY_PADDING_ALL((uint16_t)m.pad),
                                           .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                           .childGap = (uint16_t)m.gap,
                                           .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}});
    if (flow == PLATFORM_SDK_MOCK_LOGIN) {
        ui_kit_label(ctx, "MOCK PORTAL LOGIN", &g_ui_theme.title);
        ui_kit_label(ctx, "Closing the dialog is a normal outcome, not an error.", &g_ui_theme.hint);
        CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 10}}) {
            if (button(ctx, "platform_sdk/mock_login/accept", "Sign in", true)) {
                answer_login(true);
            }
            if (button(ctx, "platform_sdk/mock_login/decline", "Close", true)) {
                answer_login(false);
            }
        }
        ui_kit_panel_end(ctx);
        nt_ui_modal_end(ctx);
        return;
    }

    ui_kit_label(ctx, flow == PLATFORM_SDK_MOCK_REWARDED ? "MOCK REWARDED AD" : "MOCK INTERSTITIAL AD",
                 &g_ui_theme.title);

    char line[96];
    (void)snprintf(line, sizeof line, "placement: %s", platform_sdk_mock_placement());
    ui_kit_label(ctx, line, &g_ui_theme.label);

    if (flow == PLATFORM_SDK_MOCK_INTERSTITIAL) {
        if (button(ctx, "platform_sdk/mock_ad/close_interstitial", "Close interstitial", true)) {
            set_interstitial_result(platform_sdk_mock_close_interstitial());
        }
    } else if (flow == PLATFORM_SDK_MOCK_REWARDED) {
        ui_kit_label(ctx, "Reward is granted only by explicit success.", &g_ui_theme.hint);
        CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 10}}) {
            if (button(ctx, "platform_sdk/mock_ad/reward_success", "Grant reward", true)) {
                answer_rewarded(PLATFORM_SDK_AD_REASON_COMPLETED);
            }
            if (button(ctx, "platform_sdk/mock_ad/reward_skip", "Skip/close", true)) {
                answer_rewarded(PLATFORM_SDK_AD_REASON_SKIPPED);
            }
            if (button(ctx, "platform_sdk/mock_ad/reward_decline", "Decline", true)) {
                answer_rewarded(PLATFORM_SDK_AD_REASON_DECLINED);
            }
            if (button(ctx, "platform_sdk/mock_ad/reward_fail", "Fail ad", true)) {
                answer_rewarded(PLATFORM_SDK_AD_REASON_FAILED);
            }
        }
    }

    ui_kit_panel_end(ctx);
    nt_ui_modal_end(ctx);
}
#endif

void platform_sdk_debug_draw_ui(nt_ui_context_t *ctx) {
#if PLATFORM_SDK_DEBUG_LOCAL
    draw_mock_modal(ctx);
#endif
#if GAME_PLATFORM_SDK_DEBUG_UI
    draw_debug_panel(ctx);
#endif
    (void)ctx;
}

#endif
