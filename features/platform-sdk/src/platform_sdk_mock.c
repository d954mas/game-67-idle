#include "features/platform_sdk/platform_sdk_mock.h"

#include <stdio.h>

#ifndef PLATFORM_SDK_TARGET_ID
#error "PLATFORM_SDK_TARGET_ID must come from the build: a default would install the local mock on a portal target"
#endif

#if PLATFORM_SDK_TARGET_ID == 0 && !defined(__EMSCRIPTEN__)
#define PLATFORM_SDK_MOCK_PORTAL 1
#else
#define PLATFORM_SDK_MOCK_PORTAL 0
#endif

static platform_sdk_mock_flow_t s_flow;
static char s_placement[48] = "default";
static platform_sdk_ad_request_id_t s_interstitial_request_id;
static platform_sdk_ad_request_id_t s_rewarded_request_id;

#if PLATFORM_SDK_MOCK_PORTAL
static void set_placement(const char *placement) {
    (void)snprintf(s_placement, sizeof s_placement, "%s",
                   placement == NULL || placement[0] == '\0' ? "default" : placement);
}

static bool mock_backend_init(void *userdata) {
    (void)userdata;
    return true;
}

static platform_sdk_result_t mock_backend_show_interstitial(const char *placement, void *userdata) {
    (void)userdata;
    if (platform_sdk_target() != PLATFORM_TARGET_LOCAL) {
        return PLATFORM_SDK_RESULT_UNSUPPORTED;
    }
    if (s_flow != PLATFORM_SDK_MOCK_IDLE) {
        return PLATFORM_SDK_RESULT_BUSY;
    }
    set_placement(placement);
    s_flow = PLATFORM_SDK_MOCK_INTERSTITIAL;
    s_interstitial_request_id = platform_sdk_active_interstitial_request_id();
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_result_t mock_backend_show_rewarded(const char *placement, void *userdata) {
    (void)userdata;
    if (platform_sdk_target() != PLATFORM_TARGET_LOCAL) {
        return PLATFORM_SDK_RESULT_UNSUPPORTED;
    }
    if (s_flow != PLATFORM_SDK_MOCK_IDLE) {
        return PLATFORM_SDK_RESULT_BUSY;
    }
    set_placement(placement);
    s_flow = PLATFORM_SDK_MOCK_REWARDED;
    s_rewarded_request_id = platform_sdk_active_rewarded_request_id();
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_result_t mock_backend_login(void *userdata) {
    (void)userdata;
    if (s_flow != PLATFORM_SDK_MOCK_IDLE) {
        return PLATFORM_SDK_RESULT_BUSY;
    }
    s_flow = PLATFORM_SDK_MOCK_LOGIN;
    return PLATFORM_SDK_RESULT_OK;
}

static void mock_backend_destroy(void *userdata) {
    (void)userdata;
    s_flow = PLATFORM_SDK_MOCK_IDLE;
    s_interstitial_request_id = 0u;
    s_rewarded_request_id = 0u;
}
#endif

void platform_sdk_mock_init(void) {
#if PLATFORM_SDK_MOCK_PORTAL
    platform_sdk_backend_t backend = {
        .init = mock_backend_init,
        .show_interstitial = mock_backend_show_interstitial,
        .show_rewarded = mock_backend_show_rewarded,
        .login = mock_backend_login,
        .destroy = mock_backend_destroy,
    };
    platform_sdk_set_backend(&backend, NULL);
#endif
    if (platform_sdk_current() == PLATFORM_SDK_MOCK) {
        (void)platform_sdk_init();
    }
}

void platform_sdk_mock_shutdown(void) {
    platform_sdk_destroy();
}

platform_sdk_mock_flow_t platform_sdk_mock_flow(void) {
    return s_flow;
}

const char *platform_sdk_mock_placement(void) {
    return s_placement;
}

platform_sdk_ad_result_t platform_sdk_mock_close_interstitial(void) {
    const platform_sdk_ad_result_t result = {
        .supported = true,
        .shown = true,
        .reason = PLATFORM_SDK_AD_REASON_COMPLETED,
    };
    if (s_flow != PLATFORM_SDK_MOCK_INTERSTITIAL) {
        return result;
    }
    const platform_sdk_ad_request_id_t request_id = s_interstitial_request_id;
    s_interstitial_request_id = 0u;
    s_flow = PLATFORM_SDK_MOCK_IDLE;
    platform_sdk_backend_complete_interstitial_request(request_id, result);
    return result;
}

platform_sdk_rewarded_result_t platform_sdk_mock_answer_rewarded(platform_sdk_ad_reason_t reason) {
    const bool shown = reason == PLATFORM_SDK_AD_REASON_COMPLETED || reason == PLATFORM_SDK_AD_REASON_SKIPPED;
    const platform_sdk_rewarded_result_t result = {
        .supported = true,
        .shown = shown,
        .rewarded = reason == PLATFORM_SDK_AD_REASON_COMPLETED,
        .reason = reason,
    };
    if (s_flow != PLATFORM_SDK_MOCK_REWARDED) {
        return result;
    }
    const platform_sdk_ad_request_id_t request_id = s_rewarded_request_id;
    s_rewarded_request_id = 0u;
    s_flow = PLATFORM_SDK_MOCK_IDLE;
    platform_sdk_backend_complete_rewarded_request(request_id, result);
    return result;
}

platform_sdk_auth_result_t platform_sdk_mock_answer_login(bool accepted) {
    const platform_sdk_auth_result_t result = {
        .supported = true,
        .authorized = accepted,
        .reason = accepted ? PLATFORM_SDK_AUTH_REASON_ACCEPTED : PLATFORM_SDK_AUTH_REASON_DECLINED,
        .name = accepted ? "Local Player" : "",
        .avatar_url = "",
    };
    if (s_flow != PLATFORM_SDK_MOCK_LOGIN) {
        return result;
    }
    s_flow = PLATFORM_SDK_MOCK_IDLE;
    platform_sdk_backend_complete_login(result);
    return result;
}
