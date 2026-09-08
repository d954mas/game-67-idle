#include "runtime_build_marker.h"

#include <emscripten.h>

#ifndef GAME_RUNTIME_BUILD_FINGERPRINT
#error "GAME_RUNTIME_BUILD_FINGERPRINT must be supplied by tools/build_web.mjs"
#endif
#ifndef GAME_RUNTIME_BUILD_TARGET
#error "GAME_RUNTIME_BUILD_TARGET must be supplied by tools/build_web.mjs"
#endif
#ifndef GAME_RUNTIME_BUILD_ADAPTER
#error "GAME_RUNTIME_BUILD_ADAPTER must be supplied by tools/build_web.mjs"
#endif
#ifndef GAME_RUNTIME_BUILD_PROFILE
#error "GAME_RUNTIME_BUILD_PROFILE must be supplied by tools/build_web.mjs"
#endif
#ifndef GAME_RUNTIME_BUILD_DEBUG_UI
#error "GAME_RUNTIME_BUILD_DEBUG_UI must be supplied by tools/build_web.mjs"
#endif
#ifndef GAME_RUNTIME_BUILD_DEVAPI
#error "GAME_RUNTIME_BUILD_DEVAPI must be supplied by tools/build_web.mjs"
#endif
#ifndef GAME_RUNTIME_BUILD_ANALYTICS
#error "GAME_RUNTIME_BUILD_ANALYTICS must be supplied by tools/build_web.mjs"
#endif
#ifndef GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR
#error "GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR must be supplied by tools/build_web.mjs"
#endif

#define RUNTIME_BUILD_MARKER_PREFIX "ai_studio.runtime_build:"

static const char s_runtime_build_marker[] =
    RUNTIME_BUILD_MARKER_PREFIX GAME_RUNTIME_BUILD_FINGERPRINT
    ";t:" GAME_RUNTIME_BUILD_TARGET
    ";a:" GAME_RUNTIME_BUILD_ADAPTER
    ";p:" GAME_RUNTIME_BUILD_PROFILE
    ";du:" GAME_RUNTIME_BUILD_DEBUG_UI
    ";d:" GAME_RUNTIME_BUILD_DEVAPI
    ";a:" GAME_RUNTIME_BUILD_ANALYTICS
    ";l:" GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR;

EM_JS(void, runtime_build_marker_publish_js, (const char *marker_ptr), {
    const prefix = "ai_studio.runtime_build:";
    const marker = UTF8ToString(marker_ptr);
    globalThis.__AI_STUDIO_RUNTIME_BUILD_FINGERPRINT__ =
        marker.startsWith(prefix) ? marker.slice(prefix.length) : "";
})

void runtime_build_marker_publish(void) {
    runtime_build_marker_publish_js(s_runtime_build_marker);
}
