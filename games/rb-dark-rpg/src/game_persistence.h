#ifndef RB_DARK_RPG_GAME_PERSISTENCE_H
#define RB_DARK_RPG_GAME_PERSISTENCE_H

#include <stdbool.h>

const char *game_persistence_autosave_key(void);
bool game_persistence_load_autosave(bool fresh_state, char *error, int error_cap);
bool game_persistence_save_autosave_if_dirty(char *error, int error_cap);
bool game_persistence_save_autosave(char *error, int error_cap);
bool game_persistence_reset_autosave(char *error, int error_cap);

#endif
