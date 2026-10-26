#ifndef FEATURES_SCENES_SCENE_MANAGER_DEVAPI_H
#define FEATURES_SCENES_SCENE_MANAGER_DEVAPI_H

#include "features/scenes/scene_manager.h"

/* Registers the complete game.scene.* method set; a method already registered asserts. */
void scene_manager_register_devapi(scene_manager_t *manager);

#endif
