#include "features/platform_sdk/platform_sdk_cloud.h"

#include <stddef.h>

#if defined(__EMSCRIPTEN__)

#include <emscripten/emscripten.h>
#include <stdlib.h>

/* clang-format off */
EM_JS_DEPS(platform_sdk_cloud, "$UTF8ToString,$lengthBytesUTF8,$stringToUTF8,malloc,free")

EM_JS(int, platform_sdk_cloud_web_supported, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    return (backend && typeof backend.loadData === "function" &&
            typeof backend.saveData === "function") ? 1 : 0;
})

EM_JS(void, platform_sdk_cloud_web_load, (const char *key_ptr), {
    var backend = globalThis.__platformSdkInternalBackend;
    var state = globalThis.__platformSdkCloud || (globalThis.__platformSdkCloud = {});
    state.status = 1;
    state.value = null;
    if (!backend || typeof backend.loadData !== "function") {
        state.status = 4;
        return;
    }
    var ticket = (state.ticket || 0) + 1;
    state.ticket = ticket;
    try {
        Promise.resolve(backend.loadData(UTF8ToString(key_ptr))).then(function (value) {
            if (state.ticket !== ticket) return;
            if (typeof value === "string" && value.length > 0) {
                state.value = value;
                state.status = 2;
            } else {
                state.status = 3;
            }
        }, function () {
            if (state.ticket !== ticket) return;
            state.status = 4;
        });
    } catch (e) {
        state.status = 4;
    }
})

EM_JS(int, platform_sdk_cloud_web_status, (void), {
    var state = globalThis.__platformSdkCloud;
    return state ? (state.status | 0) : 0;
})

EM_JS(char *, platform_sdk_cloud_web_take, (void), {
    var state = globalThis.__platformSdkCloud;
    if (!state || state.status !== 2 || typeof state.value !== "string") return 0;
    var size = lengthBytesUTF8(state.value) + 1;
    var ptr = _malloc(size);
    if (!ptr) return 0;
    stringToUTF8(state.value, ptr, size);
    state.value = null;
    state.status = 3;
    return ptr;
})

/* Portals throttle player data hard (Yandex accepts roughly one write a
   second and answers the rest with an error), while an autosave fires
   whenever the run changes. One write is in flight at a time and only the
   newest text queued behind it is ever sent: the intermediate states of a
   save nobody has loaded yet are worth nothing. */
EM_JS(void, platform_sdk_cloud_web_store, (const char *key_ptr, const char *text_ptr), {
    var backend = globalThis.__platformSdkInternalBackend;
    if (!backend || typeof backend.saveData !== "function") return;
    var state = globalThis.__platformSdkCloud || (globalThis.__platformSdkCloud = {});
    state.pendingKey = UTF8ToString(key_ptr);
    state.pendingText = UTF8ToString(text_ptr);
    if (state.writing) return;
    var flush = function () {
        if (state.pendingText === null || state.pendingText === undefined) {
            state.writing = false;
            return;
        }
        var key = state.pendingKey;
        var text = state.pendingText;
        state.pendingText = null;
        state.writing = true;
        var done = function () {
            (globalThis.setTimeout || setTimeout)(flush, 3000);
        };
        try {
            Promise.resolve(backend.saveData(key, text)).then(done, done);
        } catch (e) {
            done();
        }
    };
    flush();
})
/* clang-format on */

bool platform_sdk_cloud_supported(void) { return platform_sdk_cloud_web_supported() != 0; }

void platform_sdk_cloud_load(const char *key) {
    if (key == NULL || key[0] == '\0') return;
    platform_sdk_cloud_web_load(key);
}

platform_sdk_cloud_status_t platform_sdk_cloud_status(void) {
    switch (platform_sdk_cloud_web_status()) {
        case 1: return PLATFORM_SDK_CLOUD_PENDING;
        case 2: return PLATFORM_SDK_CLOUD_READY;
        case 3: return PLATFORM_SDK_CLOUD_EMPTY;
        case 4: return PLATFORM_SDK_CLOUD_UNAVAILABLE;
        default: return PLATFORM_SDK_CLOUD_IDLE;
    }
}

char *platform_sdk_cloud_take(void) { return platform_sdk_cloud_web_take(); }

void platform_sdk_cloud_store(const char *key, const char *text) {
    if (key == NULL || key[0] == '\0' || text == NULL) return;
    platform_sdk_cloud_web_store(key, text);
}

#else

bool platform_sdk_cloud_supported(void) { return false; }
void platform_sdk_cloud_load(const char *key) { (void)key; }
platform_sdk_cloud_status_t platform_sdk_cloud_status(void) { return PLATFORM_SDK_CLOUD_UNAVAILABLE; }
char *platform_sdk_cloud_take(void) { return NULL; }
void platform_sdk_cloud_store(const char *key, const char *text) { (void)key; (void)text; }

#endif
