#include "shell/shell.h"

#include "app/nt_app.h"
#include "features/game_features.h"
#include "game_save.h"

void shell_tick_init(World *world) {
    game_save_load_result_t load_result;
    game_save_load(&load_result);
    game_features_init(world);
}

void shell_tick(World *world, const game_input_frame_t *input) {
    game_features_update(world, g_nt_app.dt);
    game_features_update_root(world, g_nt_app.dt, input);
    game_features_react(world);
    game_features_record(world);
    game_save_update_playtime(!g_nt_app.paused);
    game_save_tick();
}

void shell_tick_shutdown(World *world) {
    game_save_update_playtime(false);
    (void)game_save_flush(NULL, 0);
    game_features_shutdown(world);
}
