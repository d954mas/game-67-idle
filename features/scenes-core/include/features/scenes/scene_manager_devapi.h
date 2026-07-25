#ifndef FEATURES_SCENES_SCENE_MANAGER_DEVAPI_H
#define FEATURES_SCENES_SCENE_MANAGER_DEVAPI_H

#include "features/scenes/scene_manager.h"

/*
 * Registers the complete game.scene.* method set.
 *
 * Registration is not atomic because the engine registry has no transaction
 * API. A false result means that one or more earlier methods may already be
 * registered. The host must call nt_devapi_shutdown() and must not reuse the
 * partial registry.
 */
bool scene_manager_register_devapi(scene_manager_t *manager);

#endif
