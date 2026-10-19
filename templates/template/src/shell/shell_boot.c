#include "shell/shell.h"

#include "game_save.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "window/nt_window.h"

/* Runs after nt_engine_init and the window title, both of which main.c owns. */
void shell_boot_init(void) {
    g_nt_window.width = 1280U;
    g_nt_window.height = 720U;
    nt_window_init();
    nt_input_init();
    const nt_gfx_desc_t graphics = nt_gfx_desc_defaults();
    nt_gfx_init(&graphics);

    // Every fragment registers before init; the session loads them in shell_tick_init.
    game_save_register_fragment(&game_state_fragment);
    game_save_init();
}

void shell_boot_shutdown(void) {
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
}
