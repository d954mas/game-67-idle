#include "runtime_build_marker.h"

#include <emscripten.h>

#ifndef GAME_RUNTIME_BUILD_FINGERPRINT
#error "GAME_RUNTIME_BUILD_FINGERPRINT must be supplied by tools/build_web.mjs"
#endif

#define RUNTIME_BUILD_MARKER_PREFIX "ai_studio.runtime_build:"

static const char s_runtime_build_marker[] =
    RUNTIME_BUILD_MARKER_PREFIX GAME_RUNTIME_BUILD_FINGERPRINT;

EM_JS(void, runtime_build_marker_publish_js, (const char *marker_ptr), {
    const prefix = "ai_studio.runtime_build:";
    const marker = UTF8ToString(marker_ptr);
    globalThis.__AI_STUDIO_RUNTIME_BUILD_FINGERPRINT__ =
        marker.startsWith(prefix) ? marker.slice(prefix.length) : "";
})

void runtime_build_marker_publish(void) {
    runtime_build_marker_publish_js(s_runtime_build_marker);
}
