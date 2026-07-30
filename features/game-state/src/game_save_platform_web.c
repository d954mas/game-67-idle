#include "game_save_platform.h"

#include "game_save.h"
#include "time/nt_time.h"

#include <emscripten.h>

int64_t game_save_platform_mono_ms(void) {
    return (int64_t)(nt_time_now() * 1000.0);
}

/* clang-format off */
EM_JS(double, game_save_web_now_ms, (void), {
    return Date.now();
})
/* clang-format on */

int64_t game_save_platform_wall_ms(void) {
    return (int64_t)game_save_web_now_ms();
}

EMSCRIPTEN_KEEPALIVE void game_save_web_flush(void) {
    char error[128] = {0};
    (void)game_save_flush(error, (int)sizeof(error));
}

/* rAF freezes on a hidden tab, so persistence must flush synchronously from
   browser lifecycle events. DOM access belongs only in this web adapter. */
/* clang-format off */
EM_JS(void, game_save_web_install, (void), {
    var flush = function() {
        if (Module['_game_save_web_flush']) { Module['_game_save_web_flush'](); }
    };
    document.addEventListener('visibilitychange', function() {
        if (document.visibilityState === 'hidden') { flush(); }
    });
    window.addEventListener('pagehide', flush);
})
/* clang-format on */

void game_save_install_web_flush(void) {
    game_save_web_install();
}
