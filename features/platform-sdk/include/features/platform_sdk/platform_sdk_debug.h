#ifndef FEATURES_PLATFORM_SDK_PLATFORM_SDK_DEBUG_H
#define FEATURES_PLATFORM_SDK_PLATFORM_SDK_DEBUG_H

#include "ui/nt_ui.h"

/* The developer's view of the portal, drawn with ui-kit: the hand-answered
   mock portal's modal on a local build, and the SDK probe panel when the build
   sets GAME_PLATFORM_SDK_DEBUG_UI. Off both, it only owns the facade's life.
   A game on another UI stack draws platform_sdk_mock.h itself instead. */
void platform_sdk_debug_init(void);
void platform_sdk_debug_draw_ui(nt_ui_context_t *ctx);
void platform_sdk_debug_shutdown(void);

#endif /* FEATURES_PLATFORM_SDK_PLATFORM_SDK_DEBUG_H */
