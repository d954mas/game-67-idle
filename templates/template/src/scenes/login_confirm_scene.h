#ifndef GAME_SCENE_LOGIN_CONFIRM_H
#define GAME_SCENE_LOGIN_CONFIRM_H

#include "features/scenes/scene_manager.h"

#include <stdbool.h>

/* The login offer's confirm step: a small MODAL over whichever screen held
 * the button, so the benefits and a way out come before the portal's dialog. */
typedef struct game_login_confirm_scene {
    bool visible;
} game_login_confirm_scene_t;

extern game_login_confirm_scene_t g_game_login_confirm_scene;
extern const scene_api_t g_game_login_confirm_scene_api;

#endif
