#include "game_items.h"

#include "features/progression/progression.h"
#include "game_state.h"
#include "items_state.h"

#include "core/nt_assert.h"

#include <stdio.h>

static bool ownership_error(char *error, int error_cap, const char *message) {
    if (error && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
    return false;
}

bool game_items_validate_save_document(const cJSON *features, char *error, int error_cap) {
    const cJSON *items_json = cJSON_GetObjectItemCaseSensitive(features, "items");
    const cJSON *game_json = cJSON_GetObjectItemCaseSensitive(features, "game");
    if (!cJSON_IsObject(items_json) || !cJSON_IsObject(game_json)) {
        return ownership_error(error, error_cap, "items and game fragments are required");
    }

    ItemsState staged_items;
    GameState staged_game;
    items_state_init_defaults(&staged_items);
    game_state_init_defaults(&staged_game);
    if (!items_state_from_json(&staged_items, items_json, error, error_cap) ||
        !game_state_from_json(&staged_game, game_json, error, error_cap)) {
        return false;
    }
    if (!items_runtime_validate_state(&staged_items, error, error_cap)) {
        return false;
    }
    if (staged_game.inventory_container_id == ITEMS_ID_NONE ||
        staged_game.wallet_container_id == ITEMS_ID_NONE) {
        return ownership_error(error, error_cap, "inventory and wallet owners require container ids");
    }
    if (staged_game.inventory_container_id == staged_game.wallet_container_id) {
        return ownership_error(error, error_cap, "inventory and wallet must own distinct containers");
    }

    unsigned inventory_matches = 0;
    unsigned wallet_matches = 0;
    unsigned persistent_count = 0;
    for (int i = 0; i < ITEMS_STATE_MAX_CONTAINERS; i++) {
        const ItemsItemContainer *container = &staged_items.containers[i];
        if (!container->used) { continue; }
        persistent_count++;
        if (container->container_id == staged_game.inventory_container_id) {
            inventory_matches++;
        } else if (container->container_id == staged_game.wallet_container_id) {
            wallet_matches++;
        } else {
            return ownership_error(error, error_cap, "persistent Items container is unreferenced");
        }
    }
    if (persistent_count != 2 || inventory_matches != 1 || wallet_matches != 1) {
        return ownership_error(error, error_cap, "owner references a missing Items container");
    }
    return true;
}

static items_container_ref_t require_container(uint32_t id) {
    items_container_ref_t result = ITEMS_CONTAINER_REF_NONE;
    bool found = id != ITEMS_ID_NONE && items_container_try_from_id(id, &result);
    NT_ASSERT(found && "game owner references a missing Items container");
    return result;
}

items_container_ref_t game_inventory_container(void) {
    return require_container(game_state.inventory_container_id);
}

items_container_ref_t game_wallet_container(void) {
    return require_container(game_state.wallet_container_id);
}

void game_items_create_defaults(bool grant_starting_items) {
    char error[128] = {0};
    bool rebuilt = items_runtime_rebuild(error, (int)sizeof(error));
    NT_ASSERT(rebuilt && "reset Items state must rebuild cleanly");

    items_container_ref_t inventory = ITEMS_CONTAINER_REF_NONE;
    items_container_ref_t wallet = ITEMS_CONTAINER_REF_NONE;
    items_result_t result = items_try_container_create(
        (items_container_desc_t){
            .capacity = 64,
            .policy = ITEMS_CONTAINER_POLICY_GENERIC,
            .lifetime = ITEMS_LIFETIME_PERSISTENT,
        },
        &inventory);
    NT_ASSERT(result == ITEMS_RESULT_OK);
    result = items_try_container_create(
        (items_container_desc_t){
            .capacity = 32,
            .policy = ITEMS_CONTAINER_POLICY_CURRENCY_ONLY,
            .lifetime = ITEMS_LIFETIME_PERSISTENT,
        },
        &wallet);
    NT_ASSERT(result == ITEMS_RESULT_OK);

    game_state.inventory_container_id = items_container_id(inventory);
    game_state.wallet_container_id = items_container_id(wallet);
    progression_bind_resource_container(wallet);

    if (grant_starting_items) {
        result = items_try_stack_add(
            wallet, "tmpl.gold", 50, ITEMS_SLOT_AUTO,
            "starting:new_game", NULL, NULL);
        NT_ASSERT(result == ITEMS_RESULT_OK);
        result = items_try_stack_add(
            inventory, "tmpl.potion", 1, ITEMS_SLOT_AUTO,
            "starting:new_game", NULL, NULL);
        NT_ASSERT(result == ITEMS_RESULT_OK);
    }
}

void game_on_new_game(void) {
    game_items_create_defaults(true);
}

void game_reconcile(void) {
    items_container_ref_t wallet = game_wallet_container();
    (void)game_inventory_container();
    progression_bind_resource_container(wallet);
}
