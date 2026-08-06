#ifndef GAME_STORAGE_BACKEND_H
#define GAME_STORAGE_BACKEND_H

#include "game_storage.h"

/* may_wait: the caller is NOT on the frame thread and a refused rename may be
   waited out. See game_storage_write_blocking in game_storage.h for why the
   split is by caller rather than by slot. Backends that cannot block (web)
   ignore it. */
bool game_storage_backend_write(
    const char *slot, const char *text, char *error, int error_cap, bool may_wait);
bool game_storage_backend_read(
    const char *slot, char **out, game_storage_read_status_t *status,
    char *error, int error_cap);
bool game_storage_backend_exists(const char *slot);
bool game_storage_backend_write_backup(
    const char *slot, char *error, int error_cap);
bool game_storage_backend_read_backup(
    const char *slot, char **out, char *error, int error_cap);
bool game_storage_backend_quarantine(
    const char *slot, char *error, int error_cap);
bool game_storage_backend_probe(char *error, int error_cap);

#endif /* GAME_STORAGE_BACKEND_H */
