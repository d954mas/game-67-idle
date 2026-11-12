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

static void pause_playtime(void) {
    game_save_update_playtime(false);
}

static void (*s_hide_listener)(void);

void game_save_set_hide_listener(void (*listener)(void)) { s_hide_listener = listener; }

static void flush_on_hide(void) {
    pause_playtime();
    char error[128] = {0};
    (void)game_save_flush(error, (int)sizeof(error));
    if (s_hide_listener != NULL) s_hide_listener();
}

/* rAF freezes on a hidden tab, so persistence must flush synchronously from
   browser lifecycle events. DOM access belongs only in this web adapter. The
   handlers arrive as table entries rather than exports, so a game that never
   installs them links neither. */
typedef void (*game_save_web_handler)(void);
/* clang-format off */
EM_JS_DEPS(game_save_web_install, "$getWasmTableEntry")
EM_JS(void, game_save_web_install, (game_save_web_handler flush_fn, game_save_web_handler pause_fn), {
    var flush = getWasmTableEntry(flush_fn);
    var pause = getWasmTableEntry(pause_fn);
    document.addEventListener('visibilitychange', function() {
        if (document.visibilityState === 'hidden') { flush(); }
        else { pause(); }
    });
    window.addEventListener('pagehide', function() { flush(); });
})
/* clang-format on */

void game_save_install_web_flush(void) {
    game_save_web_install(flush_on_hide, pause_playtime);
}
