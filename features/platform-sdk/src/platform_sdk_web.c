#include "features/platform_sdk/platform_sdk_web.h"

#include "features/platform_sdk/platform_sdk.h"
#include "features/platform_sdk/platform_sdk_cloud.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#if defined(__EMSCRIPTEN__)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_sdk_web_complete_interstitial(unsigned int request_id, int supported, int shown, int reason);
void platform_sdk_web_complete_rewarded(unsigned int request_id, int supported, int shown, int rewarded, int reason);
void platform_sdk_web_complete_init(int ready);
void platform_sdk_web_complete_login(int supported, int authorized, int reason, char *name, char *avatar_url);
void platform_sdk_web_set_player(int authorized, char *name, char *avatar_url);
void platform_sdk_web_read_player(void);
void platform_sdk_web_portal_pause(void);
void platform_sdk_web_ad_visible(unsigned int request_id, int visible);
void platform_sdk_web_portal_resume(void);
void platform_sdk_web_portal_audio(int enabled);
void platform_sdk_web_complete_leaderboard_submit(char *board_id, int scope, int status);
uint32_t platform_sdk_web_leaderboard_generation(void);
void platform_sdk_web_leaderboard_begin(void);
void platform_sdk_web_leaderboard_row(int around, double value, int rank, int you,
                                      char *name, char *avatar_url, char *extra);
void platform_sdk_web_complete_leaderboard_fetch(char *board_id, int scope, int status,
                                                 int has_player, int player_rank, double player_value);

/* clang-format off */
EM_JS_DEPS(platform_sdk_web_backend, "$UTF8ToString,$stringToNewUTF8")

/* The selected adapter is a plain ES module and cannot reach the wasm exports,
   so the pause path is published as two globals the moment the backend is
   installed. */
EM_JS(void, platform_sdk_web_install_portal_hooks, (void), {
    globalThis.__platformSdkPortalPause = function () {
        try { _platform_sdk_web_portal_pause(); } catch (e) {}
    };
    globalThis.__platformSdkAdVisible = function (requestId, visible) {
        try { _platform_sdk_web_ad_visible(requestId >>> 0, visible ? 1 : 0); } catch (e) {}
    };
    globalThis.__platformSdkPortalResume = function () {
        try { _platform_sdk_web_portal_resume(); } catch (e) {}
    };
    globalThis.__platformSdkPortalAudio = function (enabled) {
        try { _platform_sdk_web_portal_audio(enabled ? 1 : 0); } catch (e) {}
    };
    var lifecycle = globalThis.__platformSdkLifecycleState;
    if (lifecycle && typeof lifecycle === "object") {
        try { _platform_sdk_web_portal_audio(lifecycle.audioEnabled === false ? 0 : 1); } catch (e) {}
        if (lifecycle.paused) {
            try { _platform_sdk_web_portal_pause(); } catch (e) {}
        }
    }
})

EM_JS(char *, platform_sdk_web_backend_locale, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.getLocale !== "function") return 0;
    try {
        var tag = backend.getLocale();
        if (!tag) return 0;
        return stringToNewUTF8(String(tag));
    } catch (e) {
        return 0;
    }
})

/* A player who is already signed in to the portal is known the moment the SDK
   answers; the facade learns it right after init so no screen has to ask. */
EM_JS(void, platform_sdk_web_backend_read_player, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.getPlayer !== "function") return;
    try {
        Promise.resolve(backend.getPlayer()).then(function (player) {
            player = player || {};
            _platform_sdk_web_set_player(
                player.authorized ? 1 : 0,
                stringToNewUTF8(String(player.name || "")),
                stringToNewUTF8(String(player.avatarUrl || "")));
        }, function () {});
    } catch (e) {}
})

EM_JS(int, platform_sdk_web_backend_init, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.ready !== "function") {
        _platform_sdk_web_complete_init(0);
        return 0;
    }
    try {
        Promise.resolve(backend.ready()).then(function (ready) {
            _platform_sdk_web_complete_init(ready ? 1 : 0);
            if (ready) _platform_sdk_web_read_player();
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
        backend.gameLoadingProgress(progress);
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

EM_JS(void, platform_sdk_web_backend_show_banner, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.showBanner !== "function") return;
    try {
        Promise.resolve(backend.showBanner()).catch(function () {});
    } catch (e) {}
})

EM_JS(void, platform_sdk_web_backend_hide_banner, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.hideBanner !== "function") return;
    try {
        Promise.resolve(backend.hideBanner()).catch(function () {});
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

EM_JS(int, platform_sdk_web_backend_show_interstitial, (const char *placement_ptr, unsigned int request_id), {
    function reasonCode(reason, shown) {
        if (reason === "unsupported") return 1;
        if (reason === "not_ready") return 2;
        if (reason === "rate_limited") return 3;
        if (reason === "failed") return 4;
        if (reason === "skipped") return 5;
        if (reason === "declined") return 6;
        if (reason === "completed" || shown) return 7;
        if (reason === "timeout") return 8;
        return 4;
    }

    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.showInterstitial !== "function") return 0;
    var placement = placement_ptr ? UTF8ToString(placement_ptr) : "";
    try {
        Promise.resolve(backend.showInterstitial(placement, request_id)).then(function (result) {
            result = result || {};
            _platform_sdk_web_complete_interstitial(
                request_id, result.supported ? 1 : 0,
                result.shown ? 1 : 0,
                reasonCode(result.reason, result.shown));
        }, function () {
            _platform_sdk_web_complete_interstitial(request_id, 1, 0, 4);
        });
        return 1;
    } catch (e) {
        _platform_sdk_web_complete_interstitial(request_id, 1, 0, 4);
        return 1;
    }
})

EM_JS(int, platform_sdk_web_backend_show_rewarded, (const char *placement_ptr, unsigned int request_id), {
    function reasonCode(reason, rewarded, shown) {
        if (reason === "unsupported") return 1;
        if (reason === "not_ready") return 2;
        if (reason === "rate_limited") return 3;
        if (reason === "failed") return 4;
        if (reason === "skipped") return 5;
        if (reason === "declined") return 6;
        if (reason === "completed" || rewarded) return 7;
        if (shown) return 5;
        if (reason === "timeout") return 8;
        return 4;
    }

    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.showRewarded !== "function") return 0;
    var placement = placement_ptr ? UTF8ToString(placement_ptr) : "";
    try {
        Promise.resolve(backend.showRewarded(placement, request_id)).then(function (result) {
            result = result || {};
            _platform_sdk_web_complete_rewarded(
                request_id, result.supported ? 1 : 0,
                result.shown ? 1 : 0,
                result.rewarded ? 1 : 0,
                reasonCode(result.reason, result.rewarded, result.shown));
        }, function () {
            _platform_sdk_web_complete_rewarded(request_id, 1, 0, 0, 4);
        });
        return 1;
    } catch (e) {
        _platform_sdk_web_complete_rewarded(request_id, 1, 0, 0, 4);
        return 1;
    }
})

EM_JS(int, platform_sdk_web_backend_login, (void), {
    function reasonCode(reason) {
        if (reason === "unsupported") return 1;
        if (reason === "not_ready") return 2;
        if (reason === "failed") return 3;
        if (reason === "declined") return 4;
        if (reason === "accepted") return 5;
        return 3;
    }

    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.login !== "function") return 0;
    try {
        Promise.resolve(backend.login()).then(function (result) {
            result = result || {};
            _platform_sdk_web_complete_login(
                result.supported ? 1 : 0,
                result.authorized ? 1 : 0,
                reasonCode(result.reason),
                stringToNewUTF8(String(result.name || "")),
                stringToNewUTF8(String(result.avatarUrl || "")));
        }, function () {
            _platform_sdk_web_complete_login(1, 0, 3, 0, 0);
        });
        return 1;
    } catch (e) {
        _platform_sdk_web_complete_login(1, 0, 3, 0, 0);
        return 1;
    }
})

/* Adapter answers speak the leaderboard status vocabulary; anything else,
   including a rejected promise, is a transport failure and never a refusal. */
EM_JS(int, platform_sdk_web_backend_leaderboard_caps, (const char *board_id_ptr), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.leaderboardCaps !== "function") return 0;
    var boardId = board_id_ptr ? UTF8ToString(board_id_ptr) : "";
    try {
        var caps = backend.leaderboardCaps(boardId) || {};
        return (caps.canRead ? 1 : 0) | (caps.canWrite ? 2 : 0) |
               (caps.needsLogin ? 4 : 0) | (caps.nativePopup ? 8 : 0);
    } catch (e) {
        return 0;
    }
})

EM_JS(int, platform_sdk_web_backend_leaderboard_submit,
      (const char *board_id_ptr, int scope, double value, const char *extra_ptr), {
    function statusCode(status) {
        if (status === "ok") return 0;
        if (status === "unsupported") return 1;
        if (status === "needs_login") return 2;
        if (status === "rate_limited") return 3;
        return 4;
    }

    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.submitScore !== "function") return 0;
    var generation = _platform_sdk_web_leaderboard_generation();
    var boardId = board_id_ptr ? UTF8ToString(board_id_ptr) : "";
    var extra = extra_ptr ? UTF8ToString(extra_ptr) : "";
    function settle(status) {
        if (generation !== _platform_sdk_web_leaderboard_generation()) return;
        _platform_sdk_web_complete_leaderboard_submit(stringToNewUTF8(boardId), scope, status);
    }
    try {
        Promise.resolve(backend.submitScore(boardId, scope, value, extra)).then(function (result) {
            settle(statusCode((result || {}).status));
        }, function () {
            settle(4);
        });
        return 1;
    } catch (e) {
        settle(4);
        return 1;
    }
})

EM_JS(int, platform_sdk_web_backend_leaderboard_fetch, (const char *board_id_ptr, int scope), {
    function statusCode(status) {
        if (status === "ok") return 0;
        if (status === "unsupported") return 1;
        if (status === "needs_login") return 2;
        if (status === "rate_limited") return 3;
        return 4;
    }
    function pushRows(rows, around) {
        if (!Array.isArray(rows)) return;
        for (var i = 0; i < rows.length; i++) {
            var row = rows[i] || {};
            _platform_sdk_web_leaderboard_row(
                around ? 1 : 0,
                Number(row.value) || 0,
                Number(row.rank) | 0,
                row.you ? 1 : 0,
                stringToNewUTF8(String(row.name || "")),
                stringToNewUTF8(String(row.avatarUrl || "")),
                stringToNewUTF8(typeof row.extra === "string" ? row.extra : ""));
        }
    }

    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.fetchEntries !== "function") return 0;
    var generation = _platform_sdk_web_leaderboard_generation();
    var boardId = board_id_ptr ? UTF8ToString(board_id_ptr) : "";
    /* Rows are staged and settled inside one synchronous handler, so two
       boards answering in the same tick cannot interleave their rows. */
    function settle(status, page) {
        if (generation !== _platform_sdk_web_leaderboard_generation()) return;
        page = page || {};
        var player = page.player || null;
        _platform_sdk_web_leaderboard_begin();
        if (status === 0) {
            pushRows(page.top, false);
            pushRows(page.around, true);
        }
        _platform_sdk_web_complete_leaderboard_fetch(
            stringToNewUTF8(boardId), scope, status,
            player ? 1 : 0,
            player ? (Number(player.rank) | 0) : 0,
            player ? (Number(player.value) || 0) : 0);
    }
    try {
        Promise.resolve(backend.fetchEntries(boardId, scope)).then(function (result) {
            result = result || {};
            settle(statusCode(result.status), result);
        }, function () {
            settle(4, null);
        });
        return 1;
    } catch (e) {
        settle(4, null);
        return 1;
    }
})

EM_JS(int, platform_sdk_web_backend_leaderboard_open, (const char *board_id_ptr), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.showLeaderboard !== "function") return 0;
    var boardId = board_id_ptr ? UTF8ToString(board_id_ptr) : "";
    try {
        Promise.resolve(backend.showLeaderboard(boardId)).catch(function () {});
        return 1;
    } catch (e) {
        return 0;
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
    if (reason < PLATFORM_SDK_AD_REASON_NONE || reason > PLATFORM_SDK_AD_REASON_TIMEOUT) {
        return PLATFORM_SDK_AD_REASON_FAILED;
    }
    return (platform_sdk_ad_reason_t)reason;
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_complete_interstitial(unsigned int request_id, int supported, int shown, int reason) {
    platform_sdk_backend_complete_interstitial_request(request_id, (platform_sdk_ad_result_t){
        .supported = supported != 0,
        .shown = shown != 0,
        .reason = reason_from_int(reason),
    });
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_complete_rewarded(unsigned int request_id, int supported, int shown, int rewarded, int reason) {
    platform_sdk_backend_complete_rewarded_request(request_id, (platform_sdk_rewarded_result_t){
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

static platform_sdk_auth_reason_t auth_reason_from_int(int reason) {
    if (reason < PLATFORM_SDK_AUTH_REASON_NONE || reason > PLATFORM_SDK_AUTH_REASON_ACCEPTED) {
        return PLATFORM_SDK_AUTH_REASON_FAILED;
    }
    return (platform_sdk_auth_reason_t)reason;
}

/* The strings are minted on the JS side for this call alone. */
EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_complete_login(int supported, int authorized, int reason, char *name, char *avatar_url) {
    platform_sdk_backend_complete_login((platform_sdk_auth_result_t){
        .supported = supported != 0,
        .authorized = authorized != 0,
        .reason = auth_reason_from_int(reason),
        .name = name,
        .avatar_url = avatar_url,
    });
    free(name);
    free(avatar_url);
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_set_player(int authorized, char *name, char *avatar_url) {
    platform_sdk_backend_set_player(authorized != 0, name, avatar_url);
    free(name);
    free(avatar_url);
}

/* Exported so the init promise can reach it by its wasm export name. */
EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_read_player(void) {
    platform_sdk_web_backend_read_player();
}

static platform_sdk_leaderboard_status_t leaderboard_status_from_int(int status) {
    if (status < PLATFORM_SDK_LEADERBOARD_OK || status > PLATFORM_SDK_LEADERBOARD_FAILED) {
        return PLATFORM_SDK_LEADERBOARD_FAILED;
    }
    return (platform_sdk_leaderboard_status_t)status;
}

static uint32_t leaderboard_value_from_double(double value) {
    if (!(value > 0.0)) return 0u;
    if (value >= 4294967295.0) return 4294967295u;
    return (uint32_t)value;
}

EMSCRIPTEN_KEEPALIVE
uint32_t platform_sdk_web_leaderboard_generation(void) {
    return platform_sdk_backend_leaderboard_generation();
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_complete_leaderboard_submit(char *board_id, int scope, int status) {
    platform_sdk_backend_complete_leaderboard_submit(board_id, scope, leaderboard_status_from_int(status));
    free(board_id);
}

/* One page of rows staged by the fetch handler before it settles. Fixed
   buffers: a longer name is cut, a longer URL or payload is dropped, because
   a cut URL is not an image and a cut payload is not a record. */
#define WEB_LB_NAME_MAX 96
#define WEB_LB_URL_MAX 256
#define WEB_LB_EXTRA_MAX 128
#define WEB_LB_ROWS_MAX (PLATFORM_SDK_LEADERBOARD_TOP_MAX + PLATFORM_SDK_LEADERBOARD_AROUND_MAX)

typedef struct web_lb_row_t {
    char name[WEB_LB_NAME_MAX];
    char avatar_url[WEB_LB_URL_MAX];
    char extra[WEB_LB_EXTRA_MAX];
} web_lb_row_t;

static struct {
    platform_sdk_leaderboard_entry_t top[PLATFORM_SDK_LEADERBOARD_TOP_MAX];
    platform_sdk_leaderboard_entry_t around[PLATFORM_SDK_LEADERBOARD_AROUND_MAX];
    web_lb_row_t strings[WEB_LB_ROWS_MAX];
    int top_count;
    int around_count;
} g_web_lb_page;

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_leaderboard_begin(void) {
    g_web_lb_page.top_count = 0;
    g_web_lb_page.around_count = 0;
}

static void copy_or_drop(char *dst, size_t cap, const char *src) {
    if (src != NULL && strlen(src) < cap) {
        memcpy(dst, src, strlen(src) + 1u);
    } else {
        dst[0] = '\0';
    }
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_leaderboard_row(int around, double value, int rank, int you,
                                      char *name, char *avatar_url, char *extra) {
    platform_sdk_leaderboard_entry_t *entry = NULL;
    if (around) {
        if (g_web_lb_page.around_count < PLATFORM_SDK_LEADERBOARD_AROUND_MAX) {
            entry = &g_web_lb_page.around[g_web_lb_page.around_count++];
        }
    } else if (g_web_lb_page.top_count < PLATFORM_SDK_LEADERBOARD_TOP_MAX) {
        entry = &g_web_lb_page.top[g_web_lb_page.top_count++];
    }
    if (entry != NULL) {
        web_lb_row_t *strings = &g_web_lb_page.strings[
            around ? PLATFORM_SDK_LEADERBOARD_TOP_MAX + g_web_lb_page.around_count - 1
                   : g_web_lb_page.top_count - 1];
        (void)snprintf(strings->name, sizeof(strings->name), "%s", name != NULL ? name : "");
        copy_or_drop(strings->avatar_url, sizeof(strings->avatar_url), avatar_url);
        copy_or_drop(strings->extra, sizeof(strings->extra), extra);
        entry->value = leaderboard_value_from_double(value);
        entry->rank = rank > 0 ? rank : 0;
        entry->you = you != 0;
        entry->name = strings->name;
        entry->avatar_url = strings->avatar_url;
        entry->extra = strings->extra;
    }
    free(name);
    free(avatar_url);
    free(extra);
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_complete_leaderboard_fetch(char *board_id, int scope, int status,
                                                 int has_player, int player_rank, double player_value) {
    const platform_sdk_leaderboard_page_t page = {
        .top = g_web_lb_page.top,
        .top_count = g_web_lb_page.top_count,
        .around = g_web_lb_page.around_count > 0 ? g_web_lb_page.around : NULL,
        .around_count = g_web_lb_page.around_count,
        .has_player = has_player != 0,
        .player_rank = player_rank > 0 ? player_rank : 0,
        .player_value = leaderboard_value_from_double(player_value),
    };
    platform_sdk_backend_complete_leaderboard_fetch(board_id, scope, leaderboard_status_from_int(status), &page);
    platform_sdk_web_leaderboard_begin();
    free(board_id);
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_ad_visible(unsigned int request_id, int visible) {
    platform_sdk_backend_ad_visible(request_id, visible != 0);
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_portal_pause(void) {
    platform_sdk_backend_portal_pause();
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_portal_resume(void) {
    platform_sdk_backend_portal_resume();
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_web_portal_audio(int enabled) {
    platform_sdk_backend_portal_audio(enabled != 0);
}

static bool web_backend_locale(char *out, size_t out_size, void *userdata) {
    (void)userdata;
    if (out == NULL || out_size == 0u) return false;
    out[0] = '\0';

    char *tag = platform_sdk_web_backend_locale();
    if (tag == NULL) return false;
    (void)snprintf(out, out_size, "%s", tag);
    free(tag);
    return out[0] != '\0';
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

static void web_backend_show_banner(void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_show_banner();
}

static void web_backend_hide_banner(void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_hide_banner();
}

static void web_backend_measure(const char *category, const char *what,
                                const char *action, void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_measure(category, what, action);
}

static platform_sdk_result_t web_backend_show_interstitial(const char *placement, void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_show_interstitial(
        placement, platform_sdk_active_interstitial_request_id()) != 0
        ? PLATFORM_SDK_RESULT_OK
        : PLATFORM_SDK_RESULT_NOT_READY;
}

static platform_sdk_result_t web_backend_show_rewarded(const char *placement, void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_show_rewarded(
        placement, platform_sdk_active_rewarded_request_id()) != 0
        ? PLATFORM_SDK_RESULT_OK
        : PLATFORM_SDK_RESULT_NOT_READY;
}

static platform_sdk_result_t web_backend_login(void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_login() != 0
        ? PLATFORM_SDK_RESULT_OK
        : PLATFORM_SDK_RESULT_UNSUPPORTED;
}

static platform_sdk_leaderboard_caps_t web_backend_leaderboard_caps(const char *board_id, void *userdata) {
    (void)userdata;
    const int bits = platform_sdk_web_backend_leaderboard_caps(board_id);
    return (platform_sdk_leaderboard_caps_t){
        .can_read = (bits & 1) != 0,
        .can_write = (bits & 2) != 0,
        .needs_login = (bits & 4) != 0,
        .native_popup = (bits & 8) != 0,
    };
}

static platform_sdk_result_t web_backend_leaderboard_submit(const char *board_id, int32_t scope, uint32_t value,
                                                            const char *extra, void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_leaderboard_submit(board_id, (int)scope, (double)value, extra) != 0
        ? PLATFORM_SDK_RESULT_OK
        : PLATFORM_SDK_RESULT_UNSUPPORTED;
}

static platform_sdk_result_t web_backend_leaderboard_fetch(const char *board_id, int32_t scope, void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_leaderboard_fetch(board_id, (int)scope) != 0
        ? PLATFORM_SDK_RESULT_OK
        : PLATFORM_SDK_RESULT_UNSUPPORTED;
}

static platform_sdk_result_t web_backend_leaderboard_open(const char *board_id, void *userdata) {
    (void)userdata;
    return platform_sdk_web_backend_leaderboard_open(board_id) != 0
        ? PLATFORM_SDK_RESULT_OK
        : PLATFORM_SDK_RESULT_UNSUPPORTED;
}

static void web_backend_destroy(void *userdata) {
    (void)userdata;
    platform_sdk_web_backend_destroy();
    platform_sdk_cloud_reset();
}

void platform_sdk_install_web_backend(void) {
    platform_sdk_web_install_portal_hooks();
    platform_sdk_cloud_install_web_backend();
    platform_sdk_backend_t backend = {
        .init = web_backend_init,
        .locale = web_backend_locale,
        .game_loading_progress = web_backend_game_loading_progress,
        .game_loading_finished = web_backend_game_loading_finished,
        .game_ready = web_backend_game_ready,
        .gameplay_start = web_backend_gameplay_start,
        .gameplay_stop = web_backend_gameplay_stop,
        .measure = web_backend_measure,
        .show_banner = web_backend_show_banner,
        .hide_banner = web_backend_hide_banner,
        .show_interstitial = web_backend_show_interstitial,
        .show_rewarded = web_backend_show_rewarded,
        .login = web_backend_login,
        .leaderboard_caps = web_backend_leaderboard_caps,
        .leaderboard_submit = web_backend_leaderboard_submit,
        .leaderboard_fetch = web_backend_leaderboard_fetch,
        .leaderboard_open = web_backend_leaderboard_open,
        .destroy = web_backend_destroy,
    };
    platform_sdk_set_backend(&backend, NULL);
}

#else

void platform_sdk_install_web_backend(void) {}

#endif
