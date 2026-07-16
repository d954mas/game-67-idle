#include "game_items.h"

#include "features/progression/progression.h"
#include "game_state.h"

#include "core/nt_assert.h"

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
