#ifndef GAME_ASSET_PATHS_H
#define GAME_ASSET_PATHS_H

#include <stdbool.h>
#include <stddef.h>

#define GAME_ASSET_PATH_MAX 1024

/* Resolves a shipped data path against the executable's own directory.
   A native build must never depend on the working directory: a store launcher,
   a desktop shortcut or a shell start the process from an arbitrary one, and a
   path baked at build time points at the build machine. On web the argument is
   an URL relative to the page and is returned unchanged.
   Returns false only when the result does not fit `cap`. */
bool game_asset_paths_resolve(const char *relative, char *out, size_t cap);

#endif /* GAME_ASSET_PATHS_H */
