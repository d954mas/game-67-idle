#include "features/platform_sdk/platform_sdk.h"
#include "features/platform_sdk/platform_sdk_measure.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef FEATURE_GAME_EVENTS
#define FEATURE_GAME_EVENTS 0
#endif

#if FEATURE_GAME_EVENTS
#include "features/platform_sdk/platform_sdk_events.h"
#include "game_events.h"
#include "log/nt_log.h"
#endif

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
#define PLATFORM_SDK_PLACEMENT_MAX 64u

typedef struct platform_sdk_listener_slot_t {
    platform_sdk_listener_id_t id;
    platform_sdk_lifecycle_callback_t callback;
    void *userdata;
} platform_sdk_listener_slot_t;

typedef struct platform_sdk_pending_interstitial_t {
    bool active;
    platform_sdk_ad_callback_t callback;
    void *userdata;
    char placement[PLATFORM_SDK_PLACEMENT_MAX];
} platform_sdk_pending_interstitial_t;

typedef struct platform_sdk_pending_rewarded_t {
    bool active;
    platform_sdk_rewarded_callback_t callback;
    void *userdata;
    char placement[PLATFORM_SDK_PLACEMENT_MAX];
} platform_sdk_pending_rewarded_t;

typedef struct platform_sdk_runtime_t {
    platform_sdk_boot_status_t status;
    platform_sdk_backend_t backend;
    void *backend_userdata;
    bool has_backend;
    bool has_input;
    bool has_gameplay_started;
    bool gameplay_active;
    bool loading_progress_sent;
    bool loading_finished_sent;
    bool game_ready_sent;
    float last_loading_progress;
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

#if FEATURE_GAME_EVENTS
#define PLATFORM_SDK_EVENT_FIELD_COUNT(fields) ((int)(sizeof(fields) / sizeof((fields)[0])))

static void copy_placement(char dst[PLATFORM_SDK_PLACEMENT_MAX], const char *placement) {
    const char *src = placement != NULL ? placement : "";
    (void)snprintf(dst, PLATFORM_SDK_PLACEMENT_MAX, "%s", src);
}

static const char *platform_sdk_ad_reason_name(platform_sdk_ad_reason_t reason) {
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

nt_hash64_t platform_sdk_ev_platform_ready_type(void) {
    static nt_hash64_t h;
    if (!h.value) {
        h = nt_hash64_str("platform.ready");
    }
    return h;
}

nt_hash64_t platform_sdk_ev_game_loading_finished_type(void) {
    static nt_hash64_t h;
    if (!h.value) {
        h = nt_hash64_str("game.loading_finished");
    }
    return h;
}

nt_hash64_t platform_sdk_ev_gameplay_start_type(void) {
    static nt_hash64_t h;
    if (!h.value) {
        h = nt_hash64_str("gameplay.start");
    }
    return h;
}

nt_hash64_t platform_sdk_ev_gameplay_stop_type(void) {
    static nt_hash64_t h;
    if (!h.value) {
        h = nt_hash64_str("gameplay.stop");
    }
    return h;
}

nt_hash64_t platform_sdk_ev_interstitial_request_type(void) {
    static nt_hash64_t h;
    if (!h.value) {
        h = nt_hash64_str("ad.interstitial.request");
    }
    return h;
}

nt_hash64_t platform_sdk_ev_interstitial_result_type(void) {
    static nt_hash64_t h;
    if (!h.value) {
        h = nt_hash64_str("ad.interstitial.result");
    }
    return h;
}

nt_hash64_t platform_sdk_ev_rewarded_request_type(void) {
    static nt_hash64_t h;
    if (!h.value) {
        h = nt_hash64_str("ad.rewarded.request");
    }
    return h;
}

nt_hash64_t platform_sdk_ev_rewarded_result_type(void) {
    static nt_hash64_t h;
    if (!h.value) {
        h = nt_hash64_str("ad.rewarded.result");
    }
    return h;
}

static const game_event_field_t platform_sdk_platform_ready_fields[] = {
    {"ready", GAME_EVENT_FT_BOOL, (uint32_t)offsetof(platform_sdk_ev_platform_ready_t, ready), 0u},
};

static const game_event_field_t platform_sdk_placement_fields[] = {
    {"placement", GAME_EVENT_FT_STRING, (uint32_t)offsetof(platform_sdk_ev_placement_t, placement), 0u},
};

static const game_event_field_t platform_sdk_interstitial_result_fields[] = {
    {"supported", GAME_EVENT_FT_BOOL, (uint32_t)offsetof(platform_sdk_ev_interstitial_result_t, supported), 0u},
    {"shown", GAME_EVENT_FT_BOOL, (uint32_t)offsetof(platform_sdk_ev_interstitial_result_t, shown), 0u},
    {"placement", GAME_EVENT_FT_STRING, (uint32_t)offsetof(platform_sdk_ev_interstitial_result_t, placement), 0u},
    {"reason", GAME_EVENT_FT_STRING, (uint32_t)offsetof(platform_sdk_ev_interstitial_result_t, reason), 0u},
};

static const game_event_field_t platform_sdk_rewarded_result_fields[] = {
    {"supported", GAME_EVENT_FT_BOOL, (uint32_t)offsetof(platform_sdk_ev_rewarded_result_t, supported), 0u},
    {"shown", GAME_EVENT_FT_BOOL, (uint32_t)offsetof(platform_sdk_ev_rewarded_result_t, shown), 0u},
    {"rewarded", GAME_EVENT_FT_BOOL, (uint32_t)offsetof(platform_sdk_ev_rewarded_result_t, rewarded), 0u},
    {"placement", GAME_EVENT_FT_STRING, (uint32_t)offsetof(platform_sdk_ev_rewarded_result_t, placement), 0u},
    {"reason", GAME_EVENT_FT_STRING, (uint32_t)offsetof(platform_sdk_ev_rewarded_result_t, reason), 0u},
};

const game_event_desc_t platform_sdk_ev_platform_ready_desc = {
    "platform.ready",
    (uint32_t)sizeof(platform_sdk_ev_platform_ready_t),
    platform_sdk_platform_ready_fields,
    PLATFORM_SDK_EVENT_FIELD_COUNT(platform_sdk_platform_ready_fields),
};

const game_event_desc_t platform_sdk_ev_game_loading_finished_desc = {
    "game.loading_finished",
    0u,
    NULL,
    0,
};

const game_event_desc_t platform_sdk_ev_gameplay_start_desc = {
    "gameplay.start",
    0u,
    NULL,
    0,
};

const game_event_desc_t platform_sdk_ev_gameplay_stop_desc = {
    "gameplay.stop",
    0u,
    NULL,
    0,
};

const game_event_desc_t platform_sdk_ev_interstitial_request_desc = {
    "ad.interstitial.request",
    (uint32_t)sizeof(platform_sdk_ev_placement_t),
    platform_sdk_placement_fields,
    PLATFORM_SDK_EVENT_FIELD_COUNT(platform_sdk_placement_fields),
};

const game_event_desc_t platform_sdk_ev_interstitial_result_desc = {
    "ad.interstitial.result",
    (uint32_t)sizeof(platform_sdk_ev_interstitial_result_t),
    platform_sdk_interstitial_result_fields,
    PLATFORM_SDK_EVENT_FIELD_COUNT(platform_sdk_interstitial_result_fields),
};

const game_event_desc_t platform_sdk_ev_rewarded_request_desc = {
    "ad.rewarded.request",
    (uint32_t)sizeof(platform_sdk_ev_placement_t),
    platform_sdk_placement_fields,
    PLATFORM_SDK_EVENT_FIELD_COUNT(platform_sdk_placement_fields),
};

const game_event_desc_t platform_sdk_ev_rewarded_result_desc = {
    "ad.rewarded.result",
    (uint32_t)sizeof(platform_sdk_ev_rewarded_result_t),
    platform_sdk_rewarded_result_fields,
    PLATFORM_SDK_EVENT_FIELD_COUNT(platform_sdk_rewarded_result_fields),
};

const game_event_desc_t *const platform_sdk_ev_descs[] = {
    &platform_sdk_ev_platform_ready_desc,
    &platform_sdk_ev_game_loading_finished_desc,
    &platform_sdk_ev_gameplay_start_desc,
    &platform_sdk_ev_gameplay_stop_desc,
    &platform_sdk_ev_interstitial_request_desc,
    &platform_sdk_ev_interstitial_result_desc,
    &platform_sdk_ev_rewarded_request_desc,
    &platform_sdk_ev_rewarded_result_desc,
};

const int platform_sdk_ev_desc_count = (int)(sizeof(platform_sdk_ev_descs) / sizeof(platform_sdk_ev_descs[0]));

void platform_sdk_events_register(void) {
    game_event_register_type_name(platform_sdk_ev_platform_ready_type(), "platform.ready");
    game_event_register_type_name(platform_sdk_ev_game_loading_finished_type(), "game.loading_finished");
    game_event_register_type_name(platform_sdk_ev_gameplay_start_type(), "gameplay.start");
    game_event_register_type_name(platform_sdk_ev_gameplay_stop_type(), "gameplay.stop");
    game_event_register_type_name(platform_sdk_ev_interstitial_request_type(), "ad.interstitial.request");
    game_event_register_type_name(platform_sdk_ev_interstitial_result_type(), "ad.interstitial.result");
    game_event_register_type_name(platform_sdk_ev_rewarded_request_type(), "ad.rewarded.request");
    game_event_register_type_name(platform_sdk_ev_rewarded_result_type(), "ad.rewarded.result");
}

static bool platform_sdk_event_append_string(uint8_t *bytes, uint32_t *used, const char *value, uint32_t *out_offset) {
    const char *src = value != NULL ? value : "";
    const size_t len = strlen(src) + 1u;
    if ((size_t)*used + len > (size_t)GAME_EVENT_EMIT_MAX) {
        nt_log_warn("platform_sdk: event payload string exceeds GAME_EVENT_EMIT_MAX -> dropped");
        return false;
    }
    *out_offset = *used;
    memcpy(bytes + *used, src, len);
    *used += (uint32_t)len;
    return true;
}

static void platform_sdk_emit_empty(nt_hash64_t type) {
    char noop = 0;
    (void)game_event_emit(type, &noop, 0u, 1u);
}

static void platform_sdk_emit_ready(bool ready) {
    platform_sdk_ev_platform_ready_t ev = {.ready = ready ? 1u : 0u};
    (void)game_event_emit(
        platform_sdk_ev_platform_ready_type(),
        &ev,
        (uint32_t)sizeof ev,
        _Alignof(platform_sdk_ev_platform_ready_t));
}

static void platform_sdk_emit_placement(nt_hash64_t type, const char *placement) {
    union {
        platform_sdk_ev_placement_t ev;
        uint8_t bytes[GAME_EVENT_EMIT_MAX];
    } u;
    uint32_t used = (uint32_t)sizeof(u.ev);
    memset(&u.ev, 0, sizeof(u.ev));
    if (!platform_sdk_event_append_string(u.bytes, &used, placement, &u.ev.placement)) {
        return;
    }
    (void)game_event_emit(type, &u, used, _Alignof(platform_sdk_ev_placement_t));
}

static void platform_sdk_emit_interstitial_result(const char *placement, platform_sdk_ad_result_t result) {
    union {
        platform_sdk_ev_interstitial_result_t ev;
        uint8_t bytes[GAME_EVENT_EMIT_MAX];
    } u;
    uint32_t used = (uint32_t)sizeof(u.ev);
    memset(&u.ev, 0, sizeof(u.ev));
    u.ev.supported = result.supported ? 1u : 0u;
    u.ev.shown = result.shown ? 1u : 0u;
    if (!platform_sdk_event_append_string(u.bytes, &used, placement, &u.ev.placement)) {
        return;
    }
    if (!platform_sdk_event_append_string(u.bytes, &used, platform_sdk_ad_reason_name(result.reason), &u.ev.reason)) {
        return;
    }
    (void)game_event_emit(platform_sdk_ev_interstitial_result_type(), &u, used, _Alignof(platform_sdk_ev_interstitial_result_t));
}

static void platform_sdk_emit_rewarded_result(const char *placement, platform_sdk_rewarded_result_t result) {
    union {
        platform_sdk_ev_rewarded_result_t ev;
        uint8_t bytes[GAME_EVENT_EMIT_MAX];
    } u;
    uint32_t used = (uint32_t)sizeof(u.ev);
    memset(&u.ev, 0, sizeof(u.ev));
    u.ev.supported = result.supported ? 1u : 0u;
    u.ev.shown = result.shown ? 1u : 0u;
    u.ev.rewarded = result.rewarded ? 1u : 0u;
    if (!platform_sdk_event_append_string(u.bytes, &used, placement, &u.ev.placement)) {
        return;
    }
    if (!platform_sdk_event_append_string(u.bytes, &used, platform_sdk_ad_reason_name(result.reason), &u.ev.reason)) {
        return;
    }
    (void)game_event_emit(platform_sdk_ev_rewarded_result_type(), &u, used, _Alignof(platform_sdk_ev_rewarded_result_t));
}

#else
static void copy_placement(char dst[PLATFORM_SDK_PLACEMENT_MAX], const char *placement) {
    const char *src = placement != NULL ? placement : "";
    (void)snprintf(dst, PLATFORM_SDK_PLACEMENT_MAX, "%s", src);
}
#define platform_sdk_emit_empty(type) ((void)0)
#define platform_sdk_emit_ready(ready) ((void)0)
#define platform_sdk_emit_placement(type, placement) ((void)0)
#define platform_sdk_emit_interstitial_result(placement, result) ((void)0)
#define platform_sdk_emit_rewarded_result(placement, result) ((void)0)
#endif

static bool platform_sdk_is_ready(void) {
    return g_platform_sdk.status == PLATFORM_SDK_BOOT_READY;
}

static bool platform_sdk_default_backend_ready(void) {
    return platform_sdk_current() == PLATFORM_SDK_MOCK;
}

static float clamp_progress(float progress01) {
    if (progress01 < 0.0f) {
        return 0.0f;
    }
    if (progress01 > 1.0f) {
        return 1.0f;
    }
    return progress01;
}

static void complete_init_once(bool ready) {
    if (g_platform_sdk.status != PLATFORM_SDK_BOOT_INITIALIZING) {
        return;
    }
    g_platform_sdk.status = ready ? PLATFORM_SDK_BOOT_READY : PLATFORM_SDK_BOOT_FAILED;
    platform_sdk_emit_ready(ready);
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
    case PLATFORM_SDK_RESULT_BUSY:
        return PLATFORM_SDK_AD_REASON_RATE_LIMITED;
    case PLATFORM_SDK_RESULT_OK:
    case PLATFORM_SDK_RESULT_DESTROYED:
    case PLATFORM_SDK_RESULT_WAITING_FOR_INPUT:
    case PLATFORM_SDK_RESULT_ALREADY_ACTIVE:
    case PLATFORM_SDK_RESULT_NOT_ACTIVE:
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
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_READY) {
        return PLATFORM_SDK_RESULT_OK;
    }
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_INITIALIZING) {
        return PLATFORM_SDK_RESULT_NOT_READY;
    }
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return PLATFORM_SDK_RESULT_DESTROYED;
    }
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_FAILED) {
        return PLATFORM_SDK_RESULT_FAILED;
    }

    g_platform_sdk.status = PLATFORM_SDK_BOOT_INITIALIZING;
    if (g_platform_sdk.has_backend && g_platform_sdk.backend.init != NULL) {
        const bool ready = g_platform_sdk.backend.init(g_platform_sdk.backend_userdata);
        if (g_platform_sdk.status == PLATFORM_SDK_BOOT_READY) {
            return PLATFORM_SDK_RESULT_OK;
        }
        if (g_platform_sdk.status == PLATFORM_SDK_BOOT_FAILED) {
            return PLATFORM_SDK_RESULT_FAILED;
        }
        if (ready) {
            complete_init_once(true);
            return PLATFORM_SDK_RESULT_OK;
        }
        return PLATFORM_SDK_RESULT_NOT_READY;
    } else {
        complete_init_once(platform_sdk_default_backend_ready());
    }

    return g_platform_sdk.status == PLATFORM_SDK_BOOT_READY ? PLATFORM_SDK_RESULT_OK : PLATFORM_SDK_RESULT_FAILED;
}

platform_sdk_result_t platform_sdk_game_loading_progress(float progress01) {
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return PLATFORM_SDK_RESULT_DESTROYED;
    }

    float clamped = clamp_progress(progress01);
    if (g_platform_sdk.loading_progress_sent && clamped <= g_platform_sdk.last_loading_progress) {
        return PLATFORM_SDK_RESULT_OK;
    }

    g_platform_sdk.loading_progress_sent = true;
    g_platform_sdk.last_loading_progress = clamped;
    if (g_platform_sdk.has_backend && g_platform_sdk.backend.game_loading_progress != NULL) {
        g_platform_sdk.backend.game_loading_progress(clamped, g_platform_sdk.backend_userdata);
    }
    return PLATFORM_SDK_RESULT_OK;
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
    platform_sdk_emit_empty(platform_sdk_ev_game_loading_finished_type());
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

static bool platform_sdk_measure_token_valid(const char *token) {
    if (token == NULL || token[0] == '\0') return false;
    size_t length = 0u;
    for (; token[length] != '\0'; ++length) {
        const char c = token[length];
        const bool allowed = (c >= 'a' && c <= 'z') ||
                             (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!allowed || length >= 32u) return false;
    }
    return length <= 32u;
}

platform_sdk_result_t platform_sdk_measure(const char *category,
                                           const char *what,
                                           const char *action) {
    if (g_platform_sdk.status == PLATFORM_SDK_BOOT_DESTROYED) {
        return PLATFORM_SDK_RESULT_DESTROYED;
    }
    if (!platform_sdk_measure_token_valid(category) ||
        !platform_sdk_measure_token_valid(what) ||
        !platform_sdk_measure_token_valid(action)) {
        return PLATFORM_SDK_RESULT_FAILED;
    }
    if (!platform_sdk_is_ready()) return PLATFORM_SDK_RESULT_NOT_READY;
    if (!g_platform_sdk.has_backend || g_platform_sdk.backend.measure == NULL) {
        return PLATFORM_SDK_RESULT_UNSUPPORTED;
    }
    g_platform_sdk.backend.measure(category, what, action, g_platform_sdk.backend_userdata);
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
#if FEATURE_GAME_EVENTS
        nt_log_warn("platform_sdk: gameplay_start ignored before first input");
#endif
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
    platform_sdk_emit_empty(platform_sdk_ev_gameplay_start_type());
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
    platform_sdk_emit_empty(platform_sdk_ev_gameplay_stop_type());
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
    platform_sdk_emit_placement(platform_sdk_ev_interstitial_request_type(), placement);
    if (!platform_sdk_is_ready()) {
        platform_sdk_ad_result_t result = ad_result(PLATFORM_SDK_AD_REASON_NOT_READY, false, false);
        platform_sdk_emit_interstitial_result(placement, result);
        if (callback != NULL) {
            callback(result, userdata);
        }
        return PLATFORM_SDK_RESULT_NOT_READY;
    }
    if (!platform_sdk_ads_supported()) {
        platform_sdk_ad_result_t result = ad_result(PLATFORM_SDK_AD_REASON_UNSUPPORTED, false, false);
        platform_sdk_emit_interstitial_result(placement, result);
        if (callback != NULL) {
            callback(result, userdata);
        }
        return PLATFORM_SDK_RESULT_UNSUPPORTED;
    }
    if (g_platform_sdk.pending_interstitial.active || g_platform_sdk.pending_rewarded.active) {
        platform_sdk_emit_interstitial_result(
            placement,
            ad_result(PLATFORM_SDK_AD_REASON_RATE_LIMITED, true, false));
        return PLATFORM_SDK_RESULT_BUSY;
    }

    g_platform_sdk.pending_interstitial = (platform_sdk_pending_interstitial_t){
        .active = true,
        .callback = callback,
        .userdata = userdata,
    };
    copy_placement(g_platform_sdk.pending_interstitial.placement, placement);
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
    platform_sdk_emit_placement(platform_sdk_ev_rewarded_request_type(), placement);
    if (!platform_sdk_is_ready()) {
        platform_sdk_rewarded_result_t result = rewarded_result(PLATFORM_SDK_AD_REASON_NOT_READY, false, false, false);
        platform_sdk_emit_rewarded_result(placement, result);
        if (callback != NULL) {
            callback(result, userdata);
        }
        return PLATFORM_SDK_RESULT_NOT_READY;
    }
    if (!platform_sdk_rewarded_supported()) {
        platform_sdk_rewarded_result_t result = rewarded_result(PLATFORM_SDK_AD_REASON_UNSUPPORTED, false, false, false);
        platform_sdk_emit_rewarded_result(placement, result);
        if (callback != NULL) {
            callback(result, userdata);
        }
        return PLATFORM_SDK_RESULT_UNSUPPORTED;
    }
    if (g_platform_sdk.pending_interstitial.active || g_platform_sdk.pending_rewarded.active) {
        platform_sdk_emit_rewarded_result(
            placement,
            rewarded_result(PLATFORM_SDK_AD_REASON_RATE_LIMITED, true, false, false));
        return PLATFORM_SDK_RESULT_BUSY;
    }

    g_platform_sdk.pending_rewarded = (platform_sdk_pending_rewarded_t){
        .active = true,
        .callback = callback,
        .userdata = userdata,
    };
    copy_placement(g_platform_sdk.pending_rewarded.placement, placement);
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
    char placement[PLATFORM_SDK_PLACEMENT_MAX];

    if (!g_platform_sdk.pending_interstitial.active) {
        return;
    }

    callback = g_platform_sdk.pending_interstitial.callback;
    userdata = g_platform_sdk.pending_interstitial.userdata;
    copy_placement(placement, g_platform_sdk.pending_interstitial.placement);
    g_platform_sdk.pending_interstitial = (platform_sdk_pending_interstitial_t){0};
    emit_lifecycle(g_platform_sdk.resume_listeners);
    platform_sdk_emit_interstitial_result(placement, result);
    if (callback != NULL) {
        callback(result, userdata);
    }
}

void platform_sdk_backend_complete_rewarded(platform_sdk_rewarded_result_t result) {
    platform_sdk_rewarded_callback_t callback = NULL;
    void *userdata = NULL;
    char placement[PLATFORM_SDK_PLACEMENT_MAX];

    if (!g_platform_sdk.pending_rewarded.active) {
        return;
    }

    callback = g_platform_sdk.pending_rewarded.callback;
    userdata = g_platform_sdk.pending_rewarded.userdata;
    copy_placement(placement, g_platform_sdk.pending_rewarded.placement);
    g_platform_sdk.pending_rewarded = (platform_sdk_pending_rewarded_t){0};
    emit_lifecycle(g_platform_sdk.resume_listeners);
    platform_sdk_emit_rewarded_result(placement, result);
    if (callback != NULL) {
        callback(result, userdata);
    }
}

void platform_sdk_backend_complete_init(bool ready) {
    complete_init_once(ready);
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
