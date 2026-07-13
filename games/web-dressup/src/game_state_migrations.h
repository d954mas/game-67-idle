#ifndef GAME_STATE_MIGRATIONS_H
#define GAME_STATE_MIGRATIONS_H

#include <stdbool.h>

#include "cJSON.h"

/* v1 progress/outfit fields remain untouched; v2 adds an empty authored-look map. */
bool game_migrate_1_to_2(cJSON *fragment, char *error, int error_cap);

#endif /* GAME_STATE_MIGRATIONS_H */
