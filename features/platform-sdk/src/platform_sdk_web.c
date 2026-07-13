#include "features/platform_sdk/platform_sdk_web.h"

#include "features/platform_sdk/platform_sdk.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#if defined(__EMSCRIPTEN__)

void platform_sdk_web_complete_interstitial(int supported, int shown, int reason);
void platform_sdk_web_complete_rewarded(int supported, int shown, int rewarded, int reason);
void platform_sdk_web_complete_init(int ready);

/* clang-format off */
EM_JS_DEPS(platform_sdk_web_backend, "$UTF8ToString")

EM_JS(int, platform_sdk_web_backend_init, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.ready !== "function") {
        _platform_sdk_web_complete_init(0);
        return 0;
    }
    try {
        Promise.resolve(backend.ready()).then(function (ready) {
            _platform_sdk_web_complete_init(ready ? 1 : 0);
        }, function () {
            _platform_sdk_web_complete_init(0);
        });
        return 0;
    } catch (e) {
        _platform_sdk_web_complete_init(0);
        return 0;
    }
})

EM_JS(void, platform_sdk_web_backend_game_loading_progress, (double progress01), {
    var progress = Math.max(0, Math.min(1, Number(progress01) || 0));
    if (typeof globalThis.__platformSdkSetLoadingProgress === "function") {
        try {
            globalThis.__platformSdkSetLoadingProgress(progress);
        } catch (e) {}
    }
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.gameLoadingProgress !== "function") return;
    try {
        Promise.resolve(backend.gameLoadingProgress(progress)).catch(function () {});
    } catch (e) {}
})

EM_JS(void, platform_sdk_web_backend_game_loading_finished, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (typeof globalThis.__platformSdkHideLoadingOverlay === "function") {
        try {
            globalThis.__platformSdkHideLoadingOverlay();
        } catch (e) {}
    }
    if (!backend || typeof backend.gameLoadingFinished !== "function") return;
    try {
        Promise.resolve(backend.gameLoadingFinished()).catch(function () {});
    } catch (e) {}
})

EM_JS(void, platform_sdk_web_backend_game_ready, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.gameReady !== "function") return;
    try {
        Promise.resolve(backend.gameReady()).catch(function () {});
    } catch (e) {}
})

EM_JS(void, platform_sdk_web_backend_gameplay_start, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.gameplayStart !== "function") return;
    try {
        Promise.resolve(backend.gameplayStart()).catch(function () {});
    } catch (e) {}
})

EM_JS(void, platform_sdk_web_backend_gameplay_stop, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.gameplayStop !== "function") return;
    try {
        Promise.resolve(backend.gameplayStop()).catch(function () {});
    } catch (e) {}
})

EM_JS(void, platform_sdk_web_backend_measure,
      (const char *category_ptr, const char *what_ptr, const char *action_ptr), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.measure !== "function") return;
    var category = category_ptr ? UTF8ToString(category_ptr) : "";
    var what = what_ptr ? UTF8ToString(what_ptr) : "";
    var action = action_ptr ? UTF8ToString(action_ptr) : "";
    try {
        Promise.resolve(backend.measure(category, what, action)).catch(function () {});
    } catch (e) {}
})

EM_JS(int, platform_sdk_web_backend_show_interstitial, (const char *placement_ptr), {
    function reasonCode(reason, shown) {
        if (reason === "unsupported") return 1;
        if (reason === "not_ready") return 2;
        if (reason === "rate_limited") return 3;
        if (reason === "failed") return 4;
        if (reason === "skipped") return 5;
        if (reason === "declined") return 6;
        if (reason === "completed" || shown) return 7;
        return 4;
    }

    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.showInterstitial !== "function") return 0;
    var placement = placement_ptr ? UTF8ToString(placement_ptr) : "";
    try {
        Promise.resolve(backend.showInterstitial(placement)).then(function (result) {
            result = result || {};
            _platform_sdk_web_complete_interstitial(
                result.supported ? 1 : 0,
                result.shown ? 1 : 0,
                reasonCode(result.reason, result.shown));
        }, function () {
            _platform_sdk_web_complete_interstitial(1, 0, 4);
        });
        return 1;
    } catch (e) {
        _platform_sdk_web_complete_interstitial(1, 0, 4);
        return 1;
    }
})

EM_JS(int, platform_sdk_web_backend_show_rewarded, (const char *placement_ptr), {
    function reasonCode(reason, rewarded, shown) {
        if (reason === "unsupported") return 1;
        if (reason === "not_ready") return 2;
        if (reason === "rate_limited") return 3;
        if (reason === "failed") return 4;
        if (reason === "skipped") return 5;
        if (reason === "declined") return 6;
        if (reason === "completed" || rewarded) return 7;
        if (shown) return 5;
        return 4;
    }

    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.showRewarded !== "function") return 0;
    var placement = placement_ptr ? UTF8ToString(placement_ptr) : "";
    try {
        Promise.resolve(backend.showRewarded(placement)).then(function (result) {
            result = result || {};
            _platform_sdk_web_complete_rewarded(
                result.supported ? 1 : 0,
                result.shown ? 1 : 0,
                result.rewarded ? 1 : 0,
                reasonCode(result.reason, result.rewarded, result.shown));
        }, function () {
            _platform_sdk_web_complete_rewarded(1, 0, 0, 4);
        });
        return 1;
    } catch (e) {
        _platform_sdk_web_complete_rewarded(1, 0, 0, 4);
        return 1;
    }
})

EM_JS(void, platform_sdk_web_backend_destroy, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.destroy !== "function") return;
    try {
        backend.destroy();
    } catch (e) {}
})
/* clang-format on */

static platform_sdk_ad_reason_t reason_from_int(int reason) {
    if (reason < PLATFORM_SDK_AD_REASON_NONE || reason > PLATFORM_SDK_AD_REASON_COMPLETED) {
        return PLATFORM_SDK_AD_REASON_FAILED;
    }
    return (platform_sdk_ad_reason_t)reason;
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_complete_interstitial(int supported, int shown, int reason) {
    platform_sdk_backend_complete_interstitial((platform_sdk_ad_result_t){
        .supported = supported != 0,
        .shown = shown != 0,
        .reason = reason_from_int(reason),
    });
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_complete_rewarded(int supported, int shown, int rewarded, int reason) {
    platform_sdk_backend_complete_rewarded((platform_sdk_rewarded_result_t){
        .supported = supported != 0,
        .shown = shown != 0,
        .rewarded = rewarded != 0,
        .reason = reason_from_int(reason),
    });
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_complete_init(int ready) {
    platform_sdk_backend_complete_init(ready != 0);
}

static bool web_backend_init(void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_init() != 0;
}

static void web_backend_game_loading_progress(float progress01, void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_game_loading_progress((double)progress01);
}

static void web_backend_game_loading_finished(void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_game_loading_finished();
}

static void web_backend_game_ready(void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_game_ready();
}

static void web_backend_gameplay_start(void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_gameplay_start();
}

static void web_backend_gameplay_stop(void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_gameplay_stop();
}

static void web_backend_measure(const char *category, const char *what,
                                const char *action, void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_measure(category, what, action);
}

static platform_sdk_result_t web_backend_show_interstitial(const char *placement, void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_show_interstitial(placement) != 0
        ? PLATFORM_SDK_RESULT_OK
        : PLATFORM_SDK_RESULT_NOT_READY;
}

static platform_sdk_result_t web_backend_show_rewarded(const char *placement, void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_show_rewarded(placement) != 0
        ? PLATFORM_SDK_RESULT_OK
        : PLATFORM_SDK_RESULT_NOT_READY;
}

static void web_backend_destroy(void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_destroy();
}

void platform_sdk_install_web_backend(void) {
    platform_sdk_backend_t backend = {
        .init = web_backend_init,
        .game_loading_progress = web_backend_game_loading_progress,
        .game_loading_finished = web_backend_game_loading_finished,
        .game_ready = web_backend_game_ready,
        .gameplay_start = web_backend_gameplay_start,
        .gameplay_stop = web_backend_gameplay_stop,
        .measure = web_backend_measure,
        .show_interstitial = web_backend_show_interstitial,
        .show_rewarded = web_backend_show_rewarded,
        .destroy = web_backend_destroy,
    };
    platform_sdk_set_backend(&backend, NULL);
}

#else

void platform_sdk_install_web_backend(void) {}

#endif
