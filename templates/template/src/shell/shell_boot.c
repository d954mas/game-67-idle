#include "shell/shell.h"

#include "game_save.h"
#include "game_state.h"
#include "game_music_pack.h"
#if GAME_MUSIC_PACK_ENABLED
#include "http/nt_http.h"
#ifndef NT_PLATFORM_WEB
#include "fs/nt_fs.h"
#endif
#endif
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
#if GAME_MUSIC_PACK_ENABLED
    nt_http_init();
#ifndef NT_PLATFORM_WEB
    nt_fs_init();
#endif
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    game_music_pack_init();
#endif

    // Every fragment registers before init; the session loads them in shell_tick_init.
    game_save_register_fragment(&game_state_fragment);
    game_save_init();
}

void shell_boot_shutdown(void) {
#if GAME_MUSIC_PACK_ENABLED
    game_music_pack_shutdown();
    nt_resource_shutdown();
#ifndef NT_PLATFORM_WEB
    nt_fs_shutdown();
#endif
    nt_http_shutdown();
    nt_hash_shutdown();
#endif
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
}
