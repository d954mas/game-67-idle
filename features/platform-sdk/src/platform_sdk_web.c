#include "features/platform_sdk/platform_sdk_web.h"

#include "features/platform_sdk/platform_sdk.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#if defined(__EMSCRIPTEN__)

void platform_sdk_web_complete_interstitial(int supported, int shown, int reason);
void platform_sdk_web_complete_rewarded(int supported, int shown, int rewarded, int reason);

/* clang-format off */
EM_JS_DEPS(platform_sdk_web_backend, "$UTF8ToString")

EM_JS(int, platform_sdk_web_backend_init, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend) return 0;
    try {
        if (typeof backend.ready === "function") {
            Promise.resolve(backend.ready()).catch(function () {});
        }
        return 1;
    } catch (e) {
        return 0;
    }
})

EM_JS(void, platform_sdk_web_backend_game_loading_finished, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
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

static bool web_backend_init(void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_init() != 0;
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
        .game_loading_finished = web_backend_game_loading_finished,
        .game_ready = web_backend_game_ready,
        .gameplay_start = web_backend_gameplay_start,
        .gameplay_stop = web_backend_gameplay_stop,
        .show_interstitial = web_backend_show_interstitial,
        .show_rewarded = web_backend_show_rewarded,
        .destroy = web_backend_destroy,
    };
    platform_sdk_set_backend(&backend, NULL);
}

#else

void platform_sdk_install_web_backend(void) {}

#endif
