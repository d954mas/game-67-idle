#ifndef GAME_UI_PLATFORM_SDK_DEBUG_H
#define GAME_UI_PLATFORM_SDK_DEBUG_H

#include "ui/nt_ui.h"

void platform_sdk_debug_init(void);
void platform_sdk_debug_draw_ui(nt_ui_context_t *ctx);
void platform_sdk_debug_shutdown(void);

#endif /* GAME_UI_PLATFORM_SDK_DEBUG_H */
