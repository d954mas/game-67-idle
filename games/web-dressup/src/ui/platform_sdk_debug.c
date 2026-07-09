#include "ui/platform_sdk_debug.h"

#include "features/platform_sdk/platform_sdk.h"

#ifndef GAME_PLATFORM_SDK_DEBUG_UI
#define GAME_PLATFORM_SDK_DEBUG_UI 0
#endif

#ifndef PLATFORM_SDK_TARGET_ID
#define PLATFORM_SDK_TARGET_ID 0
#endif

#define PLATFORM_SDK_TEMPLATE_TARGET_LOCAL 0
#define PLATFORM_SDK_DEBUG_SURFACE_ENABLED (GAME_PLATFORM_SDK_DEBUG_UI || PLATFORM_SDK_TARGET_ID == PLATFORM_SDK_TEMPLATE_TARGET_LOCAL)

#if !PLATFORM_SDK_DEBUG_SURFACE_ENABLED
void platform_sdk_debug_init(void) {
    if (platform_sdk_current() == PLATFORM_SDK_MOCK) {
        (void)platform_sdk_init();
    }
}

void platform_sdk_debug_draw_ui(nt_ui_context_t *ctx) {
    (void)ctx;
}

void platform_sdk_debug_shutdown(void) {
    platform_sdk_destroy();
}
#else

#include "clay.h"
#include "ui/nt_ui_button.h"
#include "ui/nt_ui_label.h"
#include "ui/nt_ui_modal.h"
#include "ui/nt_ui_panel.h"
#include "ui/theme.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define LAYER_BG 0
#define LAYER_IMG 1
#define LAYER_TEXT 2

typedef enum mock_ad_flow_t {
    MOCK_AD_FLOW_NONE = 0,
    MOCK_AD_FLOW_INTERSTITIAL,
    MOCK_AD_FLOW_REWARDED,
} mock_ad_flow_t;

static mock_ad_flow_t s_flow;
static char s_placement[48];
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

static void set_placement(const char *placement) {
    if (placement == NULL || placement[0] == '\0') {
        (void)snprintf(s_placement, sizeof s_placement, "default");
        return;
    }
    (void)snprintf(s_placement, sizeof s_placement, "%s", placement);
}

static void set_interstitial_result(platform_sdk_ad_result_t result) {
    (void)snprintf(s_last_result, sizeof s_last_result,
                   "interstitial: supported=%d shown=%d reason=%s",
                   result.supported ? 1 : 0, result.shown ? 1 : 0, ad_reason_name(result.reason));
}

#if GAME_PLATFORM_SDK_DEBUG_UI
static void set_request_result(const char *kind, platform_sdk_result_t result) {
    (void)snprintf(s_last_result, sizeof s_last_result, "%s request: %s", kind, sdk_result_name(result));
}
#endif

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

#if PLATFORM_SDK_TARGET_ID == PLATFORM_SDK_TEMPLATE_TARGET_LOCAL
static bool mock_backend_init(void *userdata) {
    (void)userdata;
    return true;
}

static platform_sdk_result_t mock_backend_show_interstitial(const char *placement, void *userdata) {
    (void)userdata;
    if (platform_sdk_target() != PLATFORM_TARGET_LOCAL) {
        return PLATFORM_SDK_RESULT_UNSUPPORTED;
    }
    if (s_flow != MOCK_AD_FLOW_NONE) {
        return PLATFORM_SDK_RESULT_BUSY;
    }
    set_placement(placement);
    s_flow = MOCK_AD_FLOW_INTERSTITIAL;
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_result_t mock_backend_show_rewarded(const char *placement, void *userdata) {
    (void)userdata;
    if (platform_sdk_target() != PLATFORM_TARGET_LOCAL) {
        return PLATFORM_SDK_RESULT_UNSUPPORTED;
    }
    if (s_flow != MOCK_AD_FLOW_NONE) {
        return PLATFORM_SDK_RESULT_BUSY;
    }
    set_placement(placement);
    s_flow = MOCK_AD_FLOW_REWARDED;
    return PLATFORM_SDK_RESULT_OK;
}

static void mock_backend_destroy(void *userdata) {
    (void)userdata;
    s_flow = MOCK_AD_FLOW_NONE;
}

static platform_sdk_backend_t mock_backend(void) {
    return (platform_sdk_backend_t){
        .init = mock_backend_init,
        .show_interstitial = mock_backend_show_interstitial,
        .show_rewarded = mock_backend_show_rewarded,
        .destroy = mock_backend_destroy,
    };
}
#endif

void platform_sdk_debug_init(void) {
#if PLATFORM_SDK_TARGET_ID == PLATFORM_SDK_TEMPLATE_TARGET_LOCAL
    platform_sdk_backend_t backend = mock_backend();
    platform_sdk_set_backend(&backend, NULL);
#endif
    if (platform_sdk_current() == PLATFORM_SDK_MOCK) {
        (void)platform_sdk_init();
    }
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
    platform_sdk_destroy();
}

#if PLATFORM_SDK_DEBUG_SURFACE_ENABLED
static bool button(nt_ui_context_t *ctx, const char *id, const char *label, float width, bool enabled) {
    bool clicked = false;
    CLAY({.layout = {.sizing = {CLAY_SIZING_FIXED(width), CLAY_SIZING_FIXED(46)},
                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}}) {
        nt_ui_button_begin(ctx, NT_UI_DATA_LAYER(LAYER_IMG), nt_ui_id(id), &g_theme.button,
                           &(Clay_ElementDeclaration){
                               .layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                                          .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}},
                           enabled, NULL);
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), label, &g_theme.button_label);
        clicked = nt_ui_button_end(ctx);
    }
    return clicked;
}
#endif

#if PLATFORM_SDK_TARGET_ID == PLATFORM_SDK_TEMPLATE_TARGET_LOCAL
static void complete_interstitial(platform_sdk_ad_reason_t reason, bool shown) {
    platform_sdk_ad_result_t result = {
        .supported = true,
        .shown = shown,
        .reason = reason,
    };
    set_interstitial_result(result);
    s_flow = MOCK_AD_FLOW_NONE;
    platform_sdk_backend_complete_interstitial(result);
}

static void complete_rewarded(platform_sdk_ad_reason_t reason, bool shown, bool rewarded) {
    platform_sdk_rewarded_result_t result = {
        .supported = true,
        .shown = shown,
        .rewarded = rewarded,
        .reason = reason,
    };
    set_rewarded_result(result);
    s_flow = MOCK_AD_FLOW_NONE;
    platform_sdk_backend_complete_rewarded(result);
}
#endif

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
    char line[160];

    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &g_theme.panel_region, &g_theme.panel_img,
                      &(Clay_ElementDeclaration){
                          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,
                                       .attachPoints = {.element = CLAY_ATTACH_POINT_RIGHT_BOTTOM,
                                                        .parent = CLAY_ATTACH_POINT_RIGHT_BOTTOM},
                                       .offset = {-16.0F, -16.0F}},
                          .layout = {.sizing = {CLAY_SIZING_FIXED(440), CLAY_SIZING_FIT(0)},
                                     .padding = CLAY_PADDING_ALL(16),
                                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                     .childGap = 10,
                                     .childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}}});
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "PLATFORM SDK", &g_theme.title);
    (void)snprintf(line, sizeof line, "target=%s sdk=%s status=%s",
                   platform_sdk_target_name(), platform_sdk_current_name(), sdk_result_name(platform_sdk_status() == PLATFORM_SDK_BOOT_READY ? PLATFORM_SDK_RESULT_OK : PLATFORM_SDK_RESULT_NOT_READY));
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), line, &g_theme.hint);
    (void)snprintf(line, sizeof line, "caps links=%d ads=%d rewarded=%d storage=%d",
                   caps.external_links_allowed ? 1 : 0, caps.ads_supported ? 1 : 0,
                   caps.rewarded_supported ? 1 : 0, caps.storage_supported ? 1 : 0);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), line, &g_theme.hint);
    (void)snprintf(line, sizeof line, "pause=%d resume=%d", s_pause_count, s_resume_count);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), line, &g_theme.hint);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), s_last_result, &g_theme.hint);

    CLAY({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)},
                     .layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 10}}) {
        if (button(ctx, "platform_sdk/debug/interstitial", "Show interstitial ad", 200.0F, s_flow == MOCK_AD_FLOW_NONE)) {
            platform_sdk_result_t result = platform_sdk_show_interstitial("debug_test", on_debug_interstitial_done, NULL);
            if (result != PLATFORM_SDK_RESULT_OK) {
                set_request_result("interstitial", result);
            }
        }
        if (button(ctx, "platform_sdk/debug/rewarded", "Show rewarded ad", 190.0F, s_flow == MOCK_AD_FLOW_NONE)) {
            platform_sdk_result_t result = platform_sdk_show_rewarded("debug_test", on_debug_rewarded_done, NULL);
            if (result != PLATFORM_SDK_RESULT_OK) {
                set_request_result("rewarded", result);
            }
        }
    }
    nt_ui_panel_end(ctx);
}
#endif

static void draw_mock_modal(nt_ui_context_t *ctx) {
#if PLATFORM_SDK_TARGET_ID == PLATFORM_SDK_TEMPLATE_TARGET_LOCAL
    bool open = s_flow != MOCK_AD_FLOW_NONE;
    nt_ui_modal_style_t style = nt_ui_modal_style_defaults();
    style.flags = 0U;
    style.backdrop_alpha = 0.72F;
    if (!nt_ui_modal_visible(ctx, nt_ui_id("platform_sdk/mock_ad/modal"), &style, &open)) {
        return;
    }

    nt_ui_panel_begin(ctx, NT_UI_DATA_LAYER(LAYER_BG), &g_theme.panel_region, &g_theme.panel_img,
                      &(Clay_ElementDeclaration){
                          .layout = {.sizing = {CLAY_SIZING_FIXED(520), CLAY_SIZING_FIT(0)},
                                     .padding = CLAY_PADDING_ALL(24),
                                     .layoutDirection = CLAY_TOP_TO_BOTTOM,
                                     .childGap = 16,
                                     .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}}});
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT),
                s_flow == MOCK_AD_FLOW_REWARDED ? "MOCK REWARDED AD" : "MOCK INTERSTITIAL AD",
                &g_theme.title);

    char line[96];
    (void)snprintf(line, sizeof line, "placement: %s", s_placement);
    nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), line, &g_theme.label);

    if (s_flow == MOCK_AD_FLOW_INTERSTITIAL) {
        if (button(ctx, "platform_sdk/mock_ad/close_interstitial", "Close interstitial", 220.0F, true)) {
            complete_interstitial(PLATFORM_SDK_AD_REASON_COMPLETED, true);
        }
    } else if (s_flow == MOCK_AD_FLOW_REWARDED) {
        nt_ui_label(ctx, NT_UI_DATA_LAYER(LAYER_TEXT), "Reward is granted only by explicit success.", &g_theme.hint);
        CLAY({.layout = {.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)},
                         .layoutDirection = CLAY_LEFT_TO_RIGHT,
                         .childGap = 10}}) {
            if (button(ctx, "platform_sdk/mock_ad/reward_success", "Grant reward", 140.0F, true)) {
                complete_rewarded(PLATFORM_SDK_AD_REASON_COMPLETED, true, true);
            }
            if (button(ctx, "platform_sdk/mock_ad/reward_skip", "Skip/close", 120.0F, true)) {
                complete_rewarded(PLATFORM_SDK_AD_REASON_SKIPPED, true, false);
            }
            if (button(ctx, "platform_sdk/mock_ad/reward_decline", "Decline", 110.0F, true)) {
                complete_rewarded(PLATFORM_SDK_AD_REASON_DECLINED, false, false);
            }
            if (button(ctx, "platform_sdk/mock_ad/reward_fail", "Fail ad", 100.0F, true)) {
                complete_rewarded(PLATFORM_SDK_AD_REASON_FAILED, false, false);
            }
        }
    }

    nt_ui_panel_end(ctx);
    nt_ui_modal_end(ctx);
#else
    (void)ctx;
#endif
}

void platform_sdk_debug_draw_ui(nt_ui_context_t *ctx) {
    draw_mock_modal(ctx);
#if GAME_PLATFORM_SDK_DEBUG_UI
    draw_debug_panel(ctx);
#else
    (void)ctx;
#endif
}

#endif
