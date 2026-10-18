#include "features/game_features.h"

void game_features_init(World *world) {
    world->elapsed_seconds = 0.0F;
}

void game_features_update(World *world, float dt) {
    world->elapsed_seconds += dt;
}

void game_features_update_root(World *world, float dt, const game_input_frame_t *input) {
    (void)world;
    (void)dt;
    (void)input;
}

void game_features_react(World *world) {
    (void)world;
}

void game_features_record(World *world) {
    (void)world;
}

void game_features_draw_world(World *world) {
    (void)world;
}

void game_features_draw_ui(World *world, const game_input_frame_t *input) {
    (void)world;
    (void)input;
}

void game_features_shutdown(World *world) {
    (void)world;
}
