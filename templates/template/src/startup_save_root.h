#ifndef GAME_STARTUP_SAVE_ROOT_H
#define GAME_STARTUP_SAVE_ROOT_H

#include <stddef.h>

/* The native save root chosen before arguments are read; --save-root, parsed
   later, still replaces it. GAME_STORAGE_ROOT (set by capture and test tools)
   beats a feature workspace's root, which beats the platform default (NULL).
   An empty variable counts as unset. */
static inline const char *startup_save_root(const char *env_root, const char *workspace_root) {
    if (env_root != NULL && env_root[0] != '\0') {
        return env_root;
    }
    return workspace_root;
}

#endif
