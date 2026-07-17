#ifndef GAME_ITEMS_H
#define GAME_ITEMS_H

#include "features/items/items.h"
#include "cJSON.h"

#include <stdbool.h>

/* Game-owned composition: Items core never assigns meaning to containers. */
void game_items_create_defaults(bool grant_starting_items);
items_container_ref_t game_inventory_container(void);
items_container_ref_t game_wallet_container(void);
void game_items_register_devapi(void);
bool game_items_validate_save_document(const cJSON *features, char *error, int error_cap);

#endif /* GAME_ITEMS_H */
