#ifndef GAME_STORAGE_H
#define GAME_STORAGE_H

#include <stdbool.h>

typedef struct GameStorageConfig {
    const char *namespace_name;
    const char *native_root;
} GameStorageConfig;

void game_storage_init(const GameStorageConfig *config);
bool game_storage_key_is_valid(const char *value);
bool game_storage_doc_is_valid(const char *value);
bool game_storage_resolve_key(const char *key, const char *doc, char *out, int out_cap, char *error, int error_cap);
bool game_storage_save_json(const char *key, const char *doc, const char *json, char *error, int error_cap);
bool game_storage_load_json(const char *key, const char *doc, char **out_json, char *error, int error_cap);

#endif
