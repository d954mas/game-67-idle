#ifndef GAME_ITEMS_H
#define GAME_ITEMS_H

#include "features/items/items.h"
#include "cJSON.h"

#include <stdbool.h>

/* Game-owned composition: Items core never assigns meaning to containers. */
void game_items_create_defaults(bool grant_starting_items);
items_container_ref_t game_inventory_container(void);
items_container_ref_t game_wallet_container(void);

/* Game-owned save contract: frozen legacy mapping + cross-fragment validator. */
bool game_items_migrate_document_v1_to_v2(cJSON *features, char *error, int error_cap);
void game_items_configure_save(void);
void game_items_register_devapi(void);
bool game_items_validate_save_document(const cJSON *features, char *error, int error_cap);

#endif /* GAME_ITEMS_H */
