#ifndef GAME_STORAGE_H
#define GAME_STORAGE_H

#include <stdbool.h>

bool game_storage_resolve_key(const char *key, const char *document, char *out, int out_cap, char *error, int error_cap);
bool game_storage_save_json(const char *key, const char *document, const char *json, char *error, int error_cap);
bool game_storage_load_json(const char *key, const char *document, char **out_json, char *error, int error_cap);

#endif
