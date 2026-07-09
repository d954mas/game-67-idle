#include "features/platform_sdk/platform_sdk.h"

#include <string.h>

#ifndef PLATFORM_SDK_TARGET_ID
#define PLATFORM_SDK_TARGET_ID PLATFORM_TARGET_LOCAL
#endif

#ifndef PLATFORM_SDK_CURRENT_ID
#define PLATFORM_SDK_CURRENT_ID PLATFORM_SDK_MOCK
#endif

#ifndef PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED
#define PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED 1
#endif

#ifndef PLATFORM_SDK_ADS_SUPPORTED
#define PLATFORM_SDK_ADS_SUPPORTED 1
#endif

#ifndef PLATFORM_SDK_REWARDED_SUPPORTED
#define PLATFORM_SDK_REWARDED_SUPPORTED 1
#endif

#ifndef PLATFORM_SDK_STORAGE_SUPPORTED
#define PLATFORM_SDK_STORAGE_SUPPORTED 1
#endif

#define PLATFORM_SDK_MAX_LISTENERS 8u

typedef struct platform_sdk_listener_slot_t {
    platform_sdk_listener_id_t id;
    platform_sdk_lifecycle_callback_t callback;
    void *userdata;
} platform_sdk_listener_slot_t;

typedef struct platform_sdk_pending_interstitial_t {
    bool active;
    platform_sdk_ad_callback_t callback;
    void *userdata;
} platform_sdk_pending_interstitial_t;

typedef struct platform_sdk_pending_rewarded_t {
    bool active;
    platform_sdk_rewarded_callback_t callback;
    void *userdata;
} platform_sdk_pending_rewarded_t;

typedef struct platform_sdk_runtime_t {
    platform_sdk_boot_status_t status;
    platform_sdk_backend_t backend;
    void *backend_userdata;
    bool has_backend;
    bool has_input;
    bool has_gameplay_started;
    bool gameplay_active;
    bool loading_finished_sent;
    bool game_ready_sent;
    platform_sdk_listener_id_t next_listener_id;
    platform_sdk_listener_slot_t pause_listeners[PLATFORM_SDK_MAX_LISTENERS];
    platform_sdk_listener_slot_t resume_listeners[PLATFORM_SDK_MAX_LISTENERS];
    platform_sdk_pending_interstitial_t pending_interstitial;
    platform_sdk_pending_rewarded_t pending_rewarded;
} platform_sdk_runtime_t;

static platform_sdk_runtime_t g_platform_sdk;

#if defined(PLATFORM_SDK_TESTING)
static void platform_sdk_runtime_reset(void) {
    memset(&g_platform_sdk, 0, sizeof(g_platform_sdk));
    g_platform_sdk.status = PLATFORM_SDK_BOOT_NOT_STARTED;
    g_platform_sdk.next_listener_id = 1u;
}
#endif

static bool platform_sdk_is_ready(void) {
    return g_platform_sdk.status == PLATFORM_SDK_BOOT_READY;
}

static bool platform_sdk_default_backend_ready(void) {
    return platform_sdk_current() == PLATFORM_SDK_MOCK;
}

static platform_sdk_listener_id_t add_listener(
    platform_sdk_listener_slot_t *slots,
    platform_sdk_lifecycle_callback_t callback,
    void *userdata) {
    if (callback == NULL || g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return 0u;
    }

    for (size_t i = 0; i < PLATFORM_SDK_MAX_LISTENERS; ++i) {
        if (slots[i].id == 0u) {
            platform_sdk_listener_id_t id = g_platform_sdk.next_listener_id++;
            if (id == 0u) {
                id = g_platform_sdk.next_listener_id++;
            }
            slots[i] = (platform_sdk_listener_slot_t){
                .id = id,
                .callback = callback,
                .userdata = userdata,
            };
            return id;
        }
    }

    return 0u;
}

static void remove_listener_from(platform_sdk_listener_slot_t *slots, platform_sdk_listener_id_t id) {
    if (id == 0u) {
        return;
    }

    for (size_t i = 0; i < PLATFORM_SDK_MAX_LISTENERS; ++i) {
        if (slots[i].id == id) {
            slots[i] = (platform_sdk_listener_slot_t){0};
            return;
        }
    }
}

static void emit_lifecycle(platform_sdk_listener_slot_t *slots) {
    for (size_t i = 0; i < PLATFORM_SDK_MAX_LISTENERS; ++i) {
        if (slots[i].id != 0u && slots[i].callback != NULL) {
            slots[i].callback(slots[i].userdata);
        }
    }
}

static platform_sdk_ad_result_t ad_result(platform_sdk_ad_reason_t reason, bool supported, bool shown) {
    return (platform_sdk_ad_result_t){
        .supported = supported,
        .shown = shown,
        .reason = reason,
    };
}

static platform_sdk_rewarded_result_t rewarded_result(
    platform_sdk_ad_reason_t reason,
    bool supported,
    bool shown,
    bool rewarded) {
    return (platform_sdk_rewarded_result_t){
        .supported = supported,
        .shown = shown,
        .rewarded = rewarded,
        .reason = reason,
    };
}

static platform_sdk_ad_reason_t reason_from_result(platform_sdk_result_t result) {
    switch (result) {
    case PLATFORM_SDK_RESULT_NOT_READY:
        return PLATFORM_SDK_AD_REASON_NOT_READY;
    case PLATFORM_SDK_RESULT_UNSUPPORTED:
        return PLATFORM_SDK_AD_REASON_UNSUPPORTED;
    case PLATFORM_SDK_RESULT_OK:
    case PLATFORM_SDK_RESULT_DESTROYED:
    case PLATFORM_SDK_RESULT_WAITING_FOR_INPUT:
    case PLATFORM_SDK_RESULT_ALREADY_ACTIVE:
    case PLATFORM_SDK_RESULT_NOT_ACTIVE:
    case PLATFORM_SDK_RESULT_BUSY:
    case PLATFORM_SDK_RESULT_FAILED:
        break;
    }
    return PLATFORM_SDK_AD_REASON_FAILED;
}

platform_target_t platform_sdk_target(void) { return (platform_target_t)PLATFORM_SDK_TARGET_ID; }
platform_sdk_t platform_sdk_current(void) { return (platform_sdk_t)PLATFORM_SDK_CURRENT_ID; }

const char *platform_sdk_target_name(void) {
    switch (platform_sdk_target()) {
    case PLATFORM_TARGET_LOCAL:
        return "local";
    case PLATFORM_TARGET_ITCH:
        return "itch";
    case PLATFORM_TARGET_POKI:
        return "poki";
    case PLATFORM_TARGET_YANDEX:
        return "yandex";
    case PLATFORM_TARGET_PLAYGAMA:
        return "playgama";
    }
    return "local";
}

const char *platform_sdk_current_name(void) {
    switch (platform_sdk_current()) {
    case PLATFORM_SDK_MOCK:
        return "mock";
    case PLATFORM_SDK_POKI:
        return "poki";
    case PLATFORM_SDK_YANDEX:
        return "yandex";
    case PLATFORM_SDK_PLAYGAMA:
        return "playgama";
    }
    return "mock";
}

platform_sdk_capabilities_t platform_sdk_capabilities(void) {
    return (platform_sdk_capabilities_t){
        .external_links_allowed = PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED != 0,
        .ads_supported = PLATFORM_SDK_ADS_SUPPORTED != 0,
        .rewarded_supported = PLATFORM_SDK_REWARDED_SUPPORTED != 0,
        .storage_supported = PLATFORM_SDK_STORAGE_SUPPORTED != 0,
    };
}

bool platform_sdk_external_links_allowed(void) {
    return platform_sdk_capabilities().external_links_allowed;
}

bool platform_sdk_ads_supported(void) { return platform_sdk_capabilities().ads_supported; }

bool platform_sdk_rewarded_supported(void) {
    return platform_sdk_capabilities().rewarded_supported;
}

bool platform_sdk_storage_supported(void) {
    return platform_sdk_capabilities().storage_supported;
}

void platform_sdk_set_backend(const platform_sdk_backend_t *backend, void *userdata) {
    if (backend != NULL) {
        g_platform_sdk.backend = *backend;
        g_platform_sdk.backend_userdata = userdata;
        g_platform_sdk.has_backend = true;
    } else {
        g_platform_sdk.backend = (platform_sdk_backend_t){0};
        g_platform_sdk.backend_userdata = NULL;
        g_platform_sdk.has_backend = false;
    }
}

platform_sdk_boot_status_t platform_sdk_status(void) {
    return g_platform_sdk.status;
}

platform_sdk_result_t platform_sdk_init(void) {
    bool ready = false;

    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_READY) {
        return PLATFORM_SDK_RESULT_OK;
    }
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return PLATFORM_SDK_RESULT_DESTROYED;
    }
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_FAILED) {
        return PLATFORM_SDK_RESULT_FAILED;
    }

    g_platform_sdk.status = PLATFORM_SDK_BOOT_INITIALIZING;
    if (g_platform_sdk.has_backend && g_platform_sdk.backend.init != NULL) {
        ready = g_platform_sdk.backend.init(g_platform_sdk.backend_userdata);
    } else {
        ready = platform_sdk_default_backend_ready();
    }

    g_platform_sdk.status = ready ? PLATFORM_SDK_BOOT_READY : PLATFORM_SDK_BOOT_FAILED;
    return ready ? PLATFORM_SDK_RESULT_OK : PLATFORM_SDK_RESULT_FAILED;
}

platform_sdk_result_t platform_sdk_game_loading_finished(void) {
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return PLATFORM_SDK_RESULT_DESTROYED;
    }
    if (!platform_sdk_is_ready()) {
        return PLATFORM_SDK_RESULT_NOT_READY;
    }
    if (g_platform_sdk.loading_finished_sent) {
        return PLATFORM_SDK_RESULT_OK;
    }

    g_platform_sdk.loading_finished_sent = true;
    if (g_platform_sdk.has_backend && g_platform_sdk.backend.game_loading_finished != NULL) {
        g_platform_sdk.backend.game_loading_finished(g_platform_sdk.backend_userdata);
    }
    return PLATFORM_SDK_RESULT_OK;
}

platform_sdk_result_t platform_sdk_game_ready(void) {
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return PLATFORM_SDK_RESULT_DESTROYED;
    }
    if (!platform_sdk_is_ready()) {
        return PLATFORM_SDK_RESULT_NOT_READY;
    }
    if (g_platform_sdk.game_ready_sent) {
        return PLATFORM_SDK_RESULT_OK;
    }

    g_platform_sdk.game_ready_sent = true;
    if (g_platform_sdk.has_backend && g_platform_sdk.backend.game_ready != NULL) {
        g_platform_sdk.backend.game_ready(g_platform_sdk.backend_userdata);
    }
    return PLATFORM_SDK_RESULT_OK;
}

void platform_sdk_mark_input(void) {
    if (g_platform_sdk.status != PLATFORM_SDK_BOOT_DESTROYED) {
        g_platform_sdk.has_input = true;
    }
}

bool platform_sdk_has_input(void) {
    return g_platform_sdk.has_input;
}

bool platform_sdk_has_gameplay_started(void) {
    return g_platform_sdk.has_gameplay_started;
}

bool platform_sdk_gameplay_active(void) {
    return g_platform_sdk.gameplay_active;
}

platform_sdk_gameplay_start_result_t platform_sdk_gameplay_start(void) {
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return (platform_sdk_gameplay_start_result_t){
            .started = false,
            .reason = PLATFORM_SDK_RESULT_DESTROYED,
        };
    }
    if (!platform_sdk_is_ready()) {
        return (platform_sdk_gameplay_start_result_t){
            .started = false,
            .reason = PLATFORM_SDK_RESULT_NOT_READY,
        };
    }
    if (!g_platform_sdk.has_input) {
        return (platform_sdk_gameplay_start_result_t){
            .started = false,
            .reason = PLATFORM_SDK_RESULT_WAITING_FOR_INPUT,
        };
    }
    if (g_platform_sdk.gameplay_active) {
        return (platform_sdk_gameplay_start_result_t){
            .started = false,
            .reason = PLATFORM_SDK_RESULT_ALREADY_ACTIVE,
        };
    }

    g_platform_sdk.has_gameplay_started = true;
    g_platform_sdk.gameplay_active = true;
    if (g_platform_sdk.has_backend && g_platform_sdk.backend.gameplay_start != NULL) {
        g_platform_sdk.backend.gameplay_start(g_platform_sdk.backend_userdata);
    }
    return (platform_sdk_gameplay_start_result_t){
        .started = true,
        .reason = PLATFORM_SDK_RESULT_OK,
    };
}

platform_sdk_gameplay_stop_result_t platform_sdk_gameplay_stop(void) {
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return (platform_sdk_gameplay_stop_result_t){
            .stopped = false,
            .reason = PLATFORM_SDK_RESULT_DESTROYED,
        };
    }
    if (!platform_sdk_is_ready()) {
        return (platform_sdk_gameplay_stop_result_t){
            .stopped = false,
            .reason = PLATFORM_SDK_RESULT_NOT_READY,
        };
    }
    if (!g_platform_sdk.gameplay_active) {
        return (platform_sdk_gameplay_stop_result_t){
            .stopped = false,
            .reason = PLATFORM_SDK_RESULT_NOT_ACTIVE,
        };
    }

    g_platform_sdk.gameplay_active = false;
    if (g_platform_sdk.has_backend && g_platform_sdk.backend.gameplay_stop != NULL) {
        g_platform_sdk.backend.gameplay_stop(g_platform_sdk.backend_userdata);
    }
    return (platform_sdk_gameplay_stop_result_t){
        .stopped = true,
        .reason = PLATFORM_SDK_RESULT_OK,
    };
}

platform_sdk_listener_id_t platform_sdk_on_pause(platform_sdk_lifecycle_callback_t callback, void *userdata) {
    return add_listener(g_platform_sdk.pause_listeners, callback, userdata);
}

platform_sdk_listener_id_t platform_sdk_on_resume(platform_sdk_lifecycle_callback_t callback, void *userdata) {
    return add_listener(g_platform_sdk.resume_listeners, callback, userdata);
}

void platform_sdk_remove_listener(platform_sdk_listener_id_t listener_id) {
    remove_listener_from(g_platform_sdk.pause_listeners, listener_id);
    remove_listener_from(g_platform_sdk.resume_listeners, listener_id);
}

platform_sdk_result_t platform_sdk_show_interstitial(
    const char *placement,
    platform_sdk_ad_callback_t callback,
    void *userdata) {
    platform_sdk_result_t backend_result = PLATFORM_SDK_RESULT_OK;

    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return PLATFORM_SDK_RESULT_DESTROYED;
    }
    if (!platform_sdk_is_ready()) {
        if (callback != NULL) {
            callback(ad_result(PLATFORM_SDK_AD_REASON_NOT_READY, false, false), userdata);
        }
        return PLATFORM_SDK_RESULT_NOT_READY;
    }
    if (!platform_sdk_ads_supported()) {
        if (callback != NULL) {
            callback(ad_result(PLATFORM_SDK_AD_REASON_UNSUPPORTED, false, false), userdata);
        }
        return PLATFORM_SDK_RESULT_UNSUPPORTED;
    }
    if (g_platform_sdk.pending_interstitial.active || g_platform_sdk.pending_rewarded.active) {
        return PLATFORM_SDK_RESULT_BUSY;
    }

    g_platform_sdk.pending_interstitial = (platform_sdk_pending_interstitial_t){
        .active = true,
        .callback = callback,
        .userdata = userdata,
    };
    emit_lifecycle(g_platform_sdk.pause_listeners);

    if (!g_platform_sdk.has_backend || g_platform_sdk.backend.show_interstitial == NULL) {
        platform_sdk_backend_complete_interstitial(
            ad_result(PLATFORM_SDK_AD_REASON_FAILED, true, false));
        return PLATFORM_SDK_RESULT_FAILED;
    }

    backend_result = g_platform_sdk.backend.show_interstitial(placement, g_platform_sdk.backend_userdata);
    if (backend_result != PLATFORM_SDK_RESULT_OK && g_platform_sdk.pending_interstitial.active) {
        platform_sdk_backend_complete_interstitial(
            ad_result(reason_from_result(backend_result), backend_result != PLATFORM_SDK_RESULT_UNSUPPORTED, false));
    }
    return backend_result;
}

platform_sdk_result_t platform_sdk_show_rewarded(
    const char *placement,
    platform_sdk_rewarded_callback_t callback,
    void *userdata) {
    platform_sdk_result_t backend_result = PLATFORM_SDK_RESULT_OK;

    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return PLATFORM_SDK_RESULT_DESTROYED;
    }
    if (!platform_sdk_is_ready()) {
        if (callback != NULL) {
            callback(rewarded_result(PLATFORM_SDK_AD_REASON_NOT_READY, false, false, false), userdata);
        }
        return PLATFORM_SDK_RESULT_NOT_READY;
    }
    if (!platform_sdk_rewarded_supported()) {
        if (callback != NULL) {
            callback(rewarded_result(PLATFORM_SDK_AD_REASON_UNSUPPORTED, false, false, false), userdata);
        }
        return PLATFORM_SDK_RESULT_UNSUPPORTED;
    }
    if (g_platform_sdk.pending_interstitial.active || g_platform_sdk.pending_rewarded.active) {
        return PLATFORM_SDK_RESULT_BUSY;
    }

    g_platform_sdk.pending_rewarded = (platform_sdk_pending_rewarded_t){
        .active = true,
        .callback = callback,
        .userdata = userdata,
    };
    emit_lifecycle(g_platform_sdk.pause_listeners);

    if (!g_platform_sdk.has_backend || g_platform_sdk.backend.show_rewarded == NULL) {
        platform_sdk_backend_complete_rewarded(
            rewarded_result(PLATFORM_SDK_AD_REASON_FAILED, true, false, false));
        return PLATFORM_SDK_RESULT_FAILED;
    }

    backend_result = g_platform_sdk.backend.show_rewarded(placement, g_platform_sdk.backend_userdata);
    if (backend_result != PLATFORM_SDK_RESULT_OK && g_platform_sdk.pending_rewarded.active) {
        platform_sdk_backend_complete_rewarded(
            rewarded_result(reason_from_result(backend_result), backend_result != PLATFORM_SDK_RESULT_UNSUPPORTED, false, false));
    }
    return backend_result;
}

void platform_sdk_backend_complete_interstitial(platform_sdk_ad_result_t result) {
    platform_sdk_ad_callback_t callback = NULL;
    void *userdata = NULL;

    if (!g_platform_sdk.pending_interstitial.active) {
        return;
    }

    callback = g_platform_sdk.pending_interstitial.callback;
    userdata = g_platform_sdk.pending_interstitial.userdata;
    g_platform_sdk.pending_interstitial = (platform_sdk_pending_interstitial_t){0};
    emit_lifecycle(g_platform_sdk.resume_listeners);
    if (callback != NULL) {
        callback(result, userdata);
    }
}

void platform_sdk_backend_complete_rewarded(platform_sdk_rewarded_result_t result) {
    platform_sdk_rewarded_callback_t callback = NULL;
    void *userdata = NULL;

    if (!g_platform_sdk.pending_rewarded.active) {
        return;
    }

    callback = g_platform_sdk.pending_rewarded.callback;
    userdata = g_platform_sdk.pending_rewarded.userdata;
    g_platform_sdk.pending_rewarded = (platform_sdk_pending_rewarded_t){0};
    emit_lifecycle(g_platform_sdk.resume_listeners);
    if (callback != NULL) {
        callback(result, userdata);
    }
}

void platform_sdk_destroy(void) {
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return;
    }

    if (g_platform_sdk.has_backend && g_platform_sdk.backend.destroy != NULL) {
        g_platform_sdk.backend.destroy(g_platform_sdk.backend_userdata);
    }
    g_platform_sdk.status = PLATFORM_SDK_BOOT_DESTROYED;
    memset(g_platform_sdk.pause_listeners, 0, sizeof(g_platform_sdk.pause_listeners));
    memset(g_platform_sdk.resume_listeners, 0, sizeof(g_platform_sdk.resume_listeners));
    g_platform_sdk.pending_interstitial = (platform_sdk_pending_interstitial_t){0};
    g_platform_sdk.pending_rewarded = (platform_sdk_pending_rewarded_t){0};
}

#if defined(PLATFORM_SDK_TESTING)
void platform_sdk_reset_for_tests(void) {
    platform_sdk_runtime_reset();
}
#endif
