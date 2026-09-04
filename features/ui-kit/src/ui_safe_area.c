#include "features/ui_kit/ui_safe_area.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

/* clang-format off */
EM_JS(void, safe_area_read, (float *out), {
    var insets = typeof window.__gameSafeAreaInsets === 'function'
        ? window.__gameSafeAreaInsets()
        : null;
    for (var i = 0; i < 4; ++i) {
        var value = insets ? Number(insets[i]) : 0;
        HEAPF32[(out >> 2) + i] = isFinite(value) && value > 0 ? value : 0;
    }
})
/* clang-format on */

void ui_safe_area_insets_css(float out_lrtb[4]) { safe_area_read(out_lrtb); }

#else

#include <stdlib.h>

/* The desktop window has no notch, so the honest answer is zero. The override
   exists because a phone's cutout cannot be reproduced any other way on a
   development machine, and a layout that claims to respect the safe area has to
   be provable on the frames a reviewer looks at. Native only: it never reaches a
   web build. Format: "left,right,top,bottom" in CSS pixels. */
void ui_safe_area_insets_css(float out_lrtb[4]) {
    const char *spec = getenv("UI_KIT_SAFE_AREA_CSS");
    for (int i = 0; i < 4; ++i) {
        out_lrtb[i] = 0.0F;
    }
    if (spec == NULL) {
        return;
    }
    for (int i = 0; i < 4 && *spec != '\0'; ++i) {
        char *end = NULL;
        const float value = (float)strtod(spec, &end);
        if (end == spec) {
            break;
        }
        out_lrtb[i] = value > 0.0F ? value : 0.0F;
        spec = (*end == ',') ? end + 1 : end;
    }
}

#endif
