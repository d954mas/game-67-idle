#include "game_items.h"

#include "features/progression/progression.h"
#include "game_state_json.h"
#include "game_state.h"
#include "game_events.h"
#include "items_state.h"
#include "items_state_events.gen.h"

#include "core/nt_assert.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEGACY_ITEMS_MAX_RECORDS 64
#define LEGACY_ITEMS_STRING_MAX 64

typedef enum legacy_item_storage {
    LEGACY_ITEM_STACK = 0,
    LEGACY_ITEM_UNIQUE,
} legacy_item_storage_t;

typedef struct legacy_item_def {
    const char *id;
    legacy_item_storage_t storage;
    bool currency;
} legacy_item_def_t;

/* Frozen v1 facts. Never derive migration behavior from the live catalog. */
static const legacy_item_def_t k_legacy_item_defs[] = {
    {"tmpl.energy", LEGACY_ITEM_STACK, true},
    {"tmpl.gold", LEGACY_ITEM_STACK, true},
    {"tmpl.potion", LEGACY_ITEM_STACK, false},
    {"tmpl.sword", LEGACY_ITEM_UNIQUE, false},
    {"tmpl.wood", LEGACY_ITEM_STACK, false},
    {"tmpl.xp", LEGACY_ITEM_STACK, true},
};

typedef struct legacy_container_def {
    const char *name;
    uint32_t id;
    uint32_t capacity;
    int policy;
} legacy_container_def_t;

/* purse.capacity=0 meant unlimited in v1. Its reviewed finite v2 bound is the
   complete v1 owned-map budget, so every structurally valid v1 purse fits. */
static const legacy_container_def_t k_legacy_containers[] = {
    {"backpack", 1U, 20U, ITEMS_STATE_CONTAINER_POLICY_GENERIC},
    {"purse", 2U, 64U, ITEMS_STATE_CONTAINER_POLICY_CURRENCY_ONLY},
};

static bool ownership_error(char *error, int error_cap, const char *message) {
    if (error && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
    return false;
}

static const legacy_item_def_t *legacy_item_def(const char *id) {
    for (size_t i = 0; i < sizeof(k_legacy_item_defs) / sizeof(k_legacy_item_defs[0]); i++) {
        if (strcmp(k_legacy_item_defs[i].id, id) == 0) {
            return &k_legacy_item_defs[i];
        }
    }
    return NULL;
}

static int legacy_container_index(const char *name) {
    for (size_t i = 0; i < sizeof(k_legacy_containers) / sizeof(k_legacy_containers[0]); i++) {
        if (strcmp(k_legacy_containers[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static bool legacy_required_string(
    const cJSON *object, const char *name, char *out, size_t out_cap,
    char *error, int error_cap) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsString(value) || !value->valuestring ||
        strlen(value->valuestring) >= LEGACY_ITEMS_STRING_MAX ||
        strlen(value->valuestring) >= out_cap) {
        return ownership_error(error, error_cap, "invalid legacy Items string");
    }
    (void)snprintf(out, out_cap, "%s", value->valuestring);
    return true;
}

static int compare_legacy_owned_keys(const void *left, const void *right) {
    const cJSON *const *a = (const cJSON *const *)left;
    const cJSON *const *b = (const cJSON *const *)right;
    return strcmp((*a)->string, (*b)->string);
}

static bool legacy_unique_key_matches(
    const char *key, const char *def_id, char *error, int error_cap) {
    const char *hash = strchr(key, '#');
    if (!hash || hash == key || hash[1] == '\0' || strchr(hash + 1, '#') || strchr(key, '/')) {
        return ownership_error(error, error_cap, "invalid legacy unique key");
    }
    const size_t def_len = strlen(def_id);
    if ((size_t)(hash - key) != def_len || memcmp(key, def_id, def_len) != 0 || hash[1] == '0') {
        return ownership_error(error, error_cap, "legacy unique key does not match definition");
    }
    for (const char *digit = hash + 1; *digit; digit++) {
        if (*digit < '0' || *digit > '9') {
            return ownership_error(error, error_cap, "invalid legacy unique sequence");
        }
    }
    errno = 0;
    char *end = NULL;
    unsigned long long sequence = strtoull(hash + 1, &end, 10);
    if (errno == ERANGE || !end || *end != '\0' || sequence == 0 || sequence > (unsigned long long)INT64_MAX) {
        return ownership_error(error, error_cap, "legacy unique sequence overflow");
    }
    return true;
}

static bool legacy_owner_ref_compatible(
    const cJSON *game, const char *name, uint32_t expected,
    char *error, int error_cap) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(game, name);
    if (!value) {
        return true;
    }
    if (!cJSON_IsNumber(value) || value->valuedouble < 0.0 ||
        value->valuedouble > (double)UINT32_MAX ||
        value->valuedouble != (double)(uint32_t)value->valuedouble) {
        return ownership_error(error, error_cap, "invalid legacy owner reference");
    }
    const uint32_t id = (uint32_t)value->valuedouble;
    return id == 0U || id == expected
        ? true
        : ownership_error(error, error_cap, "legacy owner reference conflicts with frozen mapping");
}

static bool set_owner_ref(
    cJSON *game, const char *name, uint32_t id, char *error, int error_cap) {
    cJSON *number = cJSON_CreateNumber((double)id);
    if (!number) {
        return ownership_error(error, error_cap, "failed to allocate migrated owner reference");
    }
    if (cJSON_GetObjectItemCaseSensitive(game, name)) {
        if (!cJSON_ReplaceItemInObjectCaseSensitive(game, name, number)) {
            cJSON_Delete(number);
            return ownership_error(error, error_cap, "failed to replace migrated owner reference");
        }
    } else if (!cJSON_AddItemToObject(game, name, number)) {
        cJSON_Delete(number);
        return ownership_error(error, error_cap, "failed to add migrated owner reference");
    }
    return true;
}

static bool parse_legacy_owned_entry(
    const cJSON *row, uint32_t entry_id, uint32_t slots[2],
    ItemsState *staged, bool *out_retained, char *error, int error_cap) {
    *out_retained = false;
    if (!row->string || strlen(row->string) >= LEGACY_ITEMS_STRING_MAX || !cJSON_IsObject(row)) {
        return ownership_error(error, error_cap, "invalid legacy owned row");
    }
    char def_id[ITEMS_STATE_STRING_MAX];
    char container_name[ITEMS_STATE_STRING_MAX];
    if (!legacy_required_string(row, "def_id", def_id, sizeof def_id, error, error_cap) ||
        !legacy_required_string(row, "container", container_name, sizeof container_name, error, error_cap)) {
        return false;
    }
    int64_t count = 0;
    int level = 0;
    float durability = 0.0F;
    bool quarantined = false;
    if (!cJSON_GetObjectItemCaseSensitive(row, "count") ||
        !gsj_read_i64(row, "count", 0, ITEMS_STATE_ITEM_ENTRY_COUNT_MAX, &count, error, error_cap) ||
        !cJSON_GetObjectItemCaseSensitive(row, "level") ||
        !gsj_read_int_range(row, "level", ITEMS_STATE_ITEM_ENTRY_LEVEL_MIN,
                            ITEMS_STATE_ITEM_ENTRY_LEVEL_MAX, &level, error, error_cap) ||
        !cJSON_GetObjectItemCaseSensitive(row, "durability") ||
        !gsj_read_float_range(row, "durability", ITEMS_STATE_ITEM_ENTRY_DURABILITY_MIN,
                              ITEMS_STATE_ITEM_ENTRY_DURABILITY_MAX, &durability, error, error_cap) ||
        !cJSON_GetObjectItemCaseSensitive(row, "quarantined") ||
        !gsj_read_bool(row, "quarantined", &quarantined, error, error_cap)) {
        return false;
    }

    const int container_index = legacy_container_index(container_name);
    if (container_index < 0) {
        return ownership_error(error, error_cap, "unsupported legacy container route");
    }
    const legacy_item_def_t *known = legacy_item_def(def_id);
    legacy_item_storage_t storage;
    const char *slash = strchr(row->string, '/');
    const char *hash = strchr(row->string, '#');
    if (slash && !hash) {
        char expected[LEGACY_ITEMS_STRING_MAX * 2];
        const int written = snprintf(expected, sizeof expected, "%s/%s", container_name, def_id);
        if (written < 0 || (size_t)written >= sizeof expected || strcmp(expected, row->string) != 0) {
            return ownership_error(error, error_cap, "legacy stack key does not match fields");
        }
        storage = LEGACY_ITEM_STACK;
    } else if (hash && !slash) {
        if (!legacy_unique_key_matches(row->string, def_id, error, error_cap)) {
            return false;
        }
        if (count != 1) {
            return ownership_error(error, error_cap, "legacy unique count must be one");
        }
        storage = LEGACY_ITEM_UNIQUE;
    } else {
        return ownership_error(error, error_cap, "unsupported legacy storage route");
    }
    if (known && known->storage != storage) {
        return ownership_error(error, error_cap, "legacy key conflicts with frozen storage");
    }
    if (container_index == 1 &&
        (storage != LEGACY_ITEM_STACK || (known && !known->currency))) {
        return ownership_error(error, error_cap, "legacy purse route violates frozen policy");
    }
    if (count == 0) {
        return true; /* schema-valid empty v1 stack: validate, then omit before ID/slot assignment */
    }
    if (slots[container_index] >= k_legacy_containers[container_index].capacity) {
        return ownership_error(error, error_cap, "legacy container exceeds frozen capacity");
    }

    ItemsItemEntry *entry = &staged->containers_entries[entry_id - 1U];
    entry->used = true;
    entry->parent_index = container_index;
    entry->entry_id = entry_id;
    entry->slot = slots[container_index]++;
    (void)snprintf(entry->def_id, sizeof entry->def_id, "%s", def_id);
    entry->count = count;
    entry->level = level;
    entry->durability = durability;
    entry->quarantined = quarantined || !known;
    *out_retained = true;
    return true;
}

bool game_items_migrate_document_v1_to_v2(cJSON *features, char *error, int error_cap) {
    cJSON *items = cJSON_GetObjectItemCaseSensitive(features, "items");
    cJSON *game = cJSON_GetObjectItemCaseSensitive(features, "game");
    if (!cJSON_IsObject(items) || !cJSON_IsObject(game)) {
        return ownership_error(error, error_cap, "legacy items and game fragments are required");
    }
    cJSON *owned = cJSON_GetObjectItemCaseSensitive(items, "owned");
    cJSON *containers = cJSON_GetObjectItemCaseSensitive(items, "containers");
    if (owned && containers) {
        return ownership_error(error, error_cap, "mixed legacy and current Items state");
    }
    if (containers || (!owned &&
        (cJSON_GetObjectItemCaseSensitive(game, "inventory_container_id") ||
         cJSON_GetObjectItemCaseSensitive(game, "wallet_container_id")))) {
        return true; /* identity upgrade for saves already written by the new runtime */
    }
    if (owned && !cJSON_IsObject(owned)) {
        return ownership_error(error, error_cap, "legacy owned must be an object");
    }
    if (!legacy_owner_ref_compatible(game, "inventory_container_id", 1U, error, error_cap) ||
        !legacy_owner_ref_compatible(game, "wallet_container_id", 2U, error, error_cap)) {
        return false;
    }

    const cJSON *rows[LEGACY_ITEMS_MAX_RECORDS];
    int row_count = 0;
    for (const cJSON *row = owned ? owned->child : NULL; row; row = row->next) {
        if (row_count >= LEGACY_ITEMS_MAX_RECORDS) {
            return ownership_error(error, error_cap, "legacy owned exceeds frozen record budget");
        }
        if (!row->string || strlen(row->string) >= LEGACY_ITEMS_STRING_MAX) {
            return ownership_error(error, error_cap, "invalid legacy owned key");
        }
        rows[row_count++] = row;
    }
    qsort(rows, (size_t)row_count, sizeof rows[0], compare_legacy_owned_keys);
    for (int i = 1; i < row_count; i++) {
        if (!rows[i - 1]->string || !rows[i]->string ||
            strcmp(rows[i - 1]->string, rows[i]->string) == 0) {
            return ownership_error(error, error_cap, "duplicate legacy owned key");
        }
    }

    ItemsState staged;
    items_state_init_defaults(&staged);
    for (int i = 0; i < 2; i++) {
        staged.containers[i].used = true;
        staged.containers[i].container_id = k_legacy_containers[i].id;
        staged.containers[i].capacity = k_legacy_containers[i].capacity;
        staged.containers[i].policy = k_legacy_containers[i].policy;
    }
    staged.last_container_id = 2U;
    uint32_t slots[2] = {0U, 0U};
    uint32_t retained_count = 0U;
    for (int i = 0; i < row_count; i++) {
        bool retained = false;
        if (!parse_legacy_owned_entry(rows[i], retained_count + 1U, slots,
                                      &staged, &retained, error, error_cap)) {
            return false;
        }
        if (retained) {
            retained_count++;
        }
    }
    staged.last_entry_id = retained_count;
    if (!items_runtime_validate_state(&staged, error, error_cap)) {
        return false;
    }
    cJSON *migrated_items = items_state_to_json(&staged);
    if (!migrated_items || !cJSON_AddNumberToObject(migrated_items, "v", ITEMS_STATE_VERSION)) {
        cJSON_Delete(migrated_items);
        return ownership_error(error, error_cap, "failed to serialize migrated Items state");
    }
    if (!cJSON_ReplaceItemInObjectCaseSensitive(features, "items", migrated_items)) {
        cJSON_Delete(migrated_items);
        return ownership_error(error, error_cap, "failed to publish staged Items migration");
    }
    return set_owner_ref(game, "inventory_container_id", 1U, error, error_cap) &&
           set_owner_ref(game, "wallet_container_id", 2U, error, error_cap);
}

void game_items_configure_save(void) {
    static const GameSaveDocumentMigrateFn migrations[] = {
        game_items_migrate_document_v1_to_v2,
    };
    game_save_set_document_migrations(migrations, 1);
    game_save_set_document_validator(game_items_validate_save_document);
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

static const game_items_seed_grant_t k_default_seed_grants[] = {
    {"tmpl.gold", 50, GAME_ITEMS_SEED_WALLET},
    {"tmpl.potion", 1, GAME_ITEMS_SEED_INVENTORY},
};

static const game_items_seed_plan_t k_default_seed_plan = {
    64,
    32,
    k_default_seed_grants,
    sizeof(k_default_seed_grants) / sizeof(k_default_seed_grants[0]),
};

static bool seed_error(char *error, int error_cap, const char *message) {
    if (error && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
    return false;
}

bool game_items_validate_seed_plan(
    const game_items_seed_plan_t *plan, char *error, int error_cap) {
    if (!plan || (plan->grant_count > 0 && !plan->grants)) {
        return seed_error(error, error_cap, "seed plan is missing");
    }
    if (plan->inventory_capacity > ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX ||
        plan->wallet_capacity > ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX ||
        plan->grant_count > ITEMS_STATE_MAX_CONTAINERS_ENTRIES) {
        return seed_error(error, error_cap, "seed container capacity is incompatible");
    }

    size_t inventory_slots = 0;
    size_t wallet_slots = 0;
    for (size_t i = 0; i < plan->grant_count; i++) {
        const game_items_seed_grant_t *grant = &plan->grants[i];
        for (size_t j = 0; j < i; j++) {
            const game_items_seed_grant_t *prior = &plan->grants[j];
            if (grant->def_id && prior->def_id && grant->route == prior->route &&
                strcmp(grant->def_id, prior->def_id) == 0) {
                return seed_error(error, error_cap, "duplicate logical seed grant");
            }
        }
        item_def_ref_t item;
        if (!grant->def_id || !items_try_get_string(grant->def_id, &item)) {
            return seed_error(error, error_cap, "seed item definition is missing");
        }
        item_core_t core = items_core(item);
        const bool currency = items_has_currency(item);
        if (core.stack == 1 ||
            (grant->route == GAME_ITEMS_SEED_WALLET && !currency) ||
            (grant->route == GAME_ITEMS_SEED_INVENTORY && currency) ||
            (grant->route != GAME_ITEMS_SEED_WALLET && grant->route != GAME_ITEMS_SEED_INVENTORY)) {
            return seed_error(error, error_cap, "seed item storage route is incompatible");
        }
        if (grant->minimum_count <= 0 ||
            grant->minimum_count > ITEMS_STATE_ITEM_ENTRY_COUNT_MAX ||
            (core.stack > 1 && grant->minimum_count > core.stack) ||
            (currency && items_currency_cap(item) > 0 &&
             grant->minimum_count > items_currency_cap(item))) {
            return seed_error(error, error_cap, "seed minimum amount is incompatible");
        }
        if (grant->route == GAME_ITEMS_SEED_WALLET) { wallet_slots++; }
        else { inventory_slots++; }
    }
    if (inventory_slots > plan->inventory_capacity || wallet_slots > plan->wallet_capacity) {
        return seed_error(error, error_cap, "seed container capacity is incompatible");
    }
    return true;
}

bool game_items_validate_default_seed(char *error, int error_cap) {
    return game_items_validate_seed_plan(&k_default_seed_plan, error, error_cap);
}

static bool seed_audit_batch_available(const game_items_seed_plan_t *plan) {
    if (plan->grant_count == 0) { return true; }
    int live_events = 0;
    (void)game_event_log(&live_events);
    if (live_events < 0 || plan->grant_count > (size_t)GAME_EVENTS_LOG_CAP ||
        (size_t)live_events > (size_t)GAME_EVENTS_LOG_CAP - plan->grant_count) {
        return false;
    }

    const size_t align = _Alignof(ItemsEvTxn);
    size_t batch_size = 0;
    for (size_t i = 0; i < plan->grant_count; i++) {
        batch_size = (batch_size + (align - 1U)) & ~(align - 1U);
        const size_t event_size = sizeof(ItemsEvTxn) + sizeof("add") +
                                  strlen(plan->grants[i].def_id) + 1U +
                                  sizeof("starting:new_game");
        if (event_size > UINT32_MAX || batch_size > UINT32_MAX - event_size) {
            return false;
        }
        batch_size += event_size;
    }
    return game_event_can_emit((uint32_t)batch_size, align);
}

static bool game_items_try_create_defaults_from_plan(
    const game_items_seed_plan_t *plan, bool grant_starting_items,
    char *error, int error_cap) {
    if (!game_items_validate_seed_plan(plan, error, error_cap)) { return false; }

    if (items_state.last_container_id != ITEMS_ID_NONE ||
        items_state.last_entry_id != ITEMS_ID_NONE ||
        game_state.inventory_container_id != ITEMS_ID_NONE ||
        game_state.wallet_container_id != ITEMS_ID_NONE) {
        return seed_error(error, error_cap, "seed initialization target is not reset");
    }
    for (size_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS; i++) {
        if (items_state.containers[i].used) {
            return seed_error(error, error_cap, "seed initialization target is not reset");
        }
    }
    for (size_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        if (items_state.containers_entries[i].used) {
            return seed_error(error, error_cap, "seed initialization target is not reset");
        }
    }
    if (grant_starting_items && !seed_audit_batch_available(plan)) {
        return seed_error(error, error_cap, "seed audit capacity is unavailable");
    }

    const ItemsState items_before = items_state;
    const GameState game_before = game_state;
    if (!items_runtime_rebuild(error, error_cap)) { return false; }

    items_container_ref_t inventory = ITEMS_CONTAINER_REF_NONE;
    items_container_ref_t wallet = ITEMS_CONTAINER_REF_NONE;
    items_result_t result = items_try_container_create(
        (items_container_desc_t){
            .capacity = plan->inventory_capacity,
            .policy = ITEMS_CONTAINER_POLICY_GENERIC,
            .lifetime = ITEMS_LIFETIME_PERSISTENT,
        },
        &inventory);
    if (result != ITEMS_RESULT_OK) { goto rollback; }
    result = items_try_container_create(
        (items_container_desc_t){
            .capacity = plan->wallet_capacity,
            .policy = ITEMS_CONTAINER_POLICY_CURRENCY_ONLY,
            .lifetime = ITEMS_LIFETIME_PERSISTENT,
        },
        &wallet);
    if (result != ITEMS_RESULT_OK) { goto rollback; }

    game_state.inventory_container_id = items_container_id(inventory);
    game_state.wallet_container_id = items_container_id(wallet);
    progression_bind_resource_container(wallet);

    if (grant_starting_items) {
        for (size_t i = 0; i < plan->grant_count; i++) {
            const game_items_seed_grant_t *grant = &plan->grants[i];
            const items_container_ref_t destination =
                grant->route == GAME_ITEMS_SEED_WALLET ? wallet : inventory;
            int64_t applied = 0;
            result = items_try_stack_add(
                destination, grant->def_id, grant->minimum_count, ITEMS_SLOT_AUTO,
                "starting:new_game", NULL, &applied);
            if (result != ITEMS_RESULT_OK || applied < grant->minimum_count) { goto rollback; }
        }
    }
    return true;

rollback:
    items_state = items_before;
    game_state = game_before;
    (void)items_runtime_rebuild(NULL, 0);
    items_container_ref_t restored_wallet = ITEMS_CONTAINER_REF_NONE;
    if (game_before.wallet_container_id != ITEMS_ID_NONE) {
        (void)items_container_try_from_id(game_before.wallet_container_id, &restored_wallet);
    }
    progression_bind_resource_container(restored_wallet);
    return seed_error(error, error_cap, "seed initialization failed atomically");
}

#if defined(GAME_ITEMS_TESTING) && GAME_ITEMS_TESTING
bool game_items_test_try_create_defaults_from_plan(
    const game_items_seed_plan_t *plan, char *error, int error_cap) {
    return game_items_try_create_defaults_from_plan(plan, true, error, error_cap);
}
#endif

void game_items_create_defaults(bool grant_starting_items) {
    char error[128] = {0};
    const bool created = game_items_try_create_defaults_from_plan(
        &k_default_seed_plan, grant_starting_items, error, (int)sizeof(error));
    NT_ASSERT(created && "default Items seed must be catalog-compatible and atomic");
    (void)created;
}

void game_on_new_game(void) {
    game_items_create_defaults(true);
}

void game_reconcile(void) {
    items_container_ref_t wallet = game_wallet_container();
    (void)game_inventory_container();
    progression_bind_resource_container(wallet);
}
