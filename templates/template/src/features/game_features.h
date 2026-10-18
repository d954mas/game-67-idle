#ifndef GAME_FEATURES_H
#define GAME_FEATURES_H

#include "game_input.h"
#include "world/world.h"

/* The shell calls these phases directly. Add a feature to the phase it owns;
   do not add a scheduler or let features call one another sideways. */
void game_features_init(World *world);
void game_features_update(World *world, float dt);
void game_features_update_root(World *world, float dt, const game_input_frame_t *input);
void game_features_react(World *world);
void game_features_record(World *world);
void game_features_draw_world(World *world);
void game_features_draw_ui(World *world, const game_input_frame_t *input);
void game_features_shutdown(World *world);

#endif
