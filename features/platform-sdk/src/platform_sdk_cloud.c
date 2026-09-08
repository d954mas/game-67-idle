#include "features/platform_sdk/platform_sdk_cloud.h"

#if defined(__EMSCRIPTEN__)
#include "features/platform_sdk/platform_sdk.h"
#include <emscripten/emscripten.h>
#include <stdlib.h>

EMSCRIPTEN_KEEPALIVE
void platform_sdk_cloud_web_complete_load(uint32_t id, int status, char *text) {
    platform_sdk_cloud_complete_load(id, (platform_sdk_cloud_status_t)status, text);
    free(text);
}

EMSCRIPTEN_KEEPALIVE
void platform_sdk_cloud_web_complete_store(uint32_t id, int status) {
    platform_sdk_cloud_complete_store(id, (platform_sdk_cloud_write_status_t)status);
}

/* clang-format off */
EM_JS_DEPS(platform_sdk_cloud, "$UTF8ToString,$stringToNewUTF8,malloc,free")

EM_JS(int, web_supported, (void), {
    var backend = globalThis.__platformSdkInternalBackend;
    return backend && typeof backend.loadData === "function" &&
        typeof backend.saveData === "function" ? 1 : 0;
})

EM_JS(void, web_load, (uint32_t id, const char *key_ptr), {
    var backend = globalThis.__platformSdkInternalBackend;
    var key = UTF8ToString(key_ptr);
    Promise.resolve().then(function () {
        return backend.loadData(key);
    }).then(function (result) {
        if (!result) throw new Error("Missing storage result");
        if (result.status === "missing") {
            _platform_sdk_cloud_web_complete_load(id, 3, 0);
        } else if (result.status === "unavailable") {
            _platform_sdk_cloud_web_complete_load(id, 4, 0);
        } else if (result.status === "found") {
            var text = typeof result.value === "string" ? result.value : JSON.stringify(result.value);
            if (typeof text !== "string") throw new Error("Invalid storage value");
            var pointer = stringToNewUTF8(text);
            if (!pointer) throw new Error("Storage allocation failed");
            _platform_sdk_cloud_web_complete_load(id, 2, pointer);
        } else {
            _platform_sdk_cloud_web_complete_load(id, 5, 0);
        }
    }).catch(function () { _platform_sdk_cloud_web_complete_load(id, 5, 0); });
})

EM_JS(void, web_store, (uint32_t id, const char *key_ptr, const char *text_ptr), {
    var backend = globalThis.__platformSdkInternalBackend;
    var key = UTF8ToString(key_ptr);
    var text = UTF8ToString(text_ptr);
    Promise.resolve().then(function () { return backend.saveData(key, text); }).then(function (result) {
        var status = result && result.status === "acknowledged" ? 2 :
            result && result.status === "unavailable" ? 3 : 4;
        _platform_sdk_cloud_web_complete_store(id, status);
    }).catch(function () { _platform_sdk_cloud_web_complete_store(id, 4); });
})
/* clang-format on */

static bool supported(void *context) {
    (void)context;
    return platform_sdk_storage_supported() && web_supported() != 0;
}
static void load(uint32_t id, const char *key, void *context) {
    (void)context;
    web_load(id, key);
}
static void store(uint32_t id, const char *key, const char *text, void *context) {
    (void)context;
    web_store(id, key, text);
}
void platform_sdk_cloud_install_web_backend(void) {
    const platform_sdk_cloud_backend_t backend = { .supported = supported, .load = load, .store = store };
    platform_sdk_cloud_set_backend(&backend, NULL);
}

#else
void platform_sdk_cloud_install_web_backend(void) {}
#endif
