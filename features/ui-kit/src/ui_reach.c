#include "features/ui_kit/ui_reach.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

/* clang-format off */
/* `pointer: coarse` is the primary input being a finger, which is what a phone
   and a tablet report in both orientations; a touchscreen laptop still reports
   its mouse as the primary pointer and reads as a desk. */
EM_JS(int, reach_is_hand, (void), {
    return typeof window.matchMedia === 'function' &&
                   window.matchMedia('(pointer: coarse)').matches
               ? 1
               : 0;
})
/* clang-format on */

bool ui_screen_in_hand(void) { return reach_is_hand() != 0; }

#else

#include <stdlib.h>

/* A development machine has a mouse. The override exists because a phone cannot
   be reproduced any other way on a desktop, and a layout that claims to be
   readable in the hand has to be provable on the frames a reviewer looks at.
   Native only: it never reaches a web build. */
bool ui_screen_in_hand(void) {
    const char *spec = getenv("UI_KIT_IN_HAND");
    return spec != NULL && spec[0] != '\0' && spec[0] != '0';
}

#endif
