#ifndef GAME_ITEMS_H
#define GAME_ITEMS_H

#include "features/items/items.h"
#include "cJSON.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum game_items_seed_route_t {
    GAME_ITEMS_SEED_INVENTORY = 0,
    GAME_ITEMS_SEED_WALLET,
} game_items_seed_route_t;

typedef struct game_items_seed_grant_t {
    const char *def_id;
    int64_t minimum_count;
    game_items_seed_route_t route;
} game_items_seed_grant_t;

typedef struct game_items_seed_plan_t {
    uint32_t inventory_capacity;
    uint32_t wallet_capacity;
    const game_items_seed_grant_t *grants;
    size_t grant_count;
} game_items_seed_plan_t;

/* Game-owned composition: Items core never assigns meaning to containers. */
void game_items_create_defaults(bool grant_starting_items);
bool game_items_validate_seed_plan(
    const game_items_seed_plan_t *plan, char *error, int error_cap);
bool game_items_validate_default_seed(char *error, int error_cap);
#if defined(GAME_ITEMS_TESTING) && GAME_ITEMS_TESTING
bool game_items_test_try_create_defaults_from_plan(
    const game_items_seed_plan_t *plan, char *error, int error_cap);
#endif
items_container_ref_t game_inventory_container(void);
items_container_ref_t game_wallet_container(void);

/* Game-owned save contract: frozen legacy mapping + cross-fragment validator. */
bool game_items_migrate_document_v1_to_v2(cJSON *features, char *error, int error_cap);
void game_items_configure_save(void);
void game_items_register_devapi(void);
bool game_items_validate_save_document(const cJSON *features, char *error, int error_cap);

#endif /* GAME_ITEMS_H */
