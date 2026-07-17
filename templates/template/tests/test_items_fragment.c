#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "features/items/items.h"
#include "game_events.h"
#include "items_runtime_test_catalog.h"
#include "items_state.h"
#include "items_state_events.gen.h"

void game_save_mark_dirty(void) {}

void setUp(void) {
    items_state_fragment.reset();
    TEST_ASSERT_TRUE(items_runtime_rebuild(NULL, 0));
    game_event_frame_reset();
}

void tearDown(void) {}

static items_container_ref_t create_container(uint32_t capacity, items_container_policy_t policy) {
    items_container_ref_t ref = {0};
    items_container_desc_t desc = {
        .capacity = capacity,
        .policy = policy,
        .lifetime = ITEMS_LIFETIME_PERSISTENT,
    };
    TEST_ASSERT_EQUAL_INT(ITEMS_RESULT_OK, items_try_container_create(desc, &ref));
    return ref;
}

static items_container_ref_t create_ephemeral_container(
    uint32_t capacity, items_container_policy_t policy) {
    items_container_ref_t ref = ITEMS_CONTAINER_REF_NONE;
    items_container_desc_t desc = {
        .capacity = capacity,
        .policy = policy,
        .lifetime = ITEMS_LIFETIME_EPHEMERAL,
    };
    TEST_ASSERT_EQUAL_INT(ITEMS_RESULT_OK, items_try_container_create(desc, &ref));
    return ref;
}

static items_inspection_budget_t inspection_budget(uint32_t rows) {
    return (items_inspection_budget_t){
        .max_rows = rows,
        .max_bytes = ITEMS_INSPECTION_MAX_BYTES,
        .max_context_rows = ITEMS_INSPECTION_MAX_CONTEXT_ROWS,
    };
}

void test_empty_nested_state_round_trip(void) {
    cJSON *json = items_state_to_json(&items_state);
    cJSON *containers = cJSON_GetObjectItemCaseSensitive(json, "containers");
    TEST_ASSERT_TRUE(cJSON_IsArray(containers));
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(containers));

    ItemsState loaded;
    char error[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&loaded, json, error, (int)sizeof(error)));
    cJSON *again = items_state_to_json(&loaded);
    TEST_ASSERT_TRUE(cJSON_Compare(json, again, true));
    cJSON_Delete(again);
    cJSON_Delete(json);
}

void test_container_ids_are_monotone_and_reserved_max_refuses(void) {
    items_container_ref_t a = create_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t b = create_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    TEST_ASSERT_EQUAL_UINT32(1, items_container_id(a));
    TEST_ASSERT_EQUAL_UINT32(2, items_container_id(b));

    TEST_ASSERT_EQUAL_INT(ITEMS_RESULT_OK, items_try_container_destroy_empty(a));
    items_container_ref_t c = create_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    TEST_ASSERT_EQUAL_UINT32(3, items_container_id(c));

    items_state.last_container_id = UINT32_MAX - 1U;
    cJSON *before = items_state_to_json(&items_state);
    items_container_ref_t refused = {0};
    items_container_desc_t desc = {1, ITEMS_CONTAINER_POLICY_GENERIC, ITEMS_LIFETIME_PERSISTENT};
    TEST_ASSERT_EQUAL_INT(ITEMS_RESULT_ID_EXHAUSTED, items_try_container_create(desc, &refused));
    cJSON *after = items_state_to_json(&items_state);
    TEST_ASSERT_TRUE(cJSON_Compare(before, after, true));
    cJSON_Delete(after);
    cJSON_Delete(before);
}

void test_entry_id_reaches_maximum_then_exhausts_without_mutation(void) {
    items_container_ref_t bag = create_container(2, ITEMS_CONTAINER_POLICY_GENERIC);
    items_state.last_entry_id = UINT32_MAX - 2U;

    item_entry_ref_t maximum = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(bag, "tmpl.wood", 1, 0, "loot:test", &maximum, NULL));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 1U, items_entry_id(maximum));

    cJSON *before = items_state_to_json(&items_state);
    TEST_ASSERT_EQUAL_UINT32(
        UINT32_MAX - 1U,
        (uint32_t)cJSON_GetObjectItemCaseSensitive(before, "last_entry_id")->valuedouble);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_ID_EXHAUSTED,
        items_try_stack_add(bag, "tmpl.potion", 1, 1, "loot:test", NULL, NULL));
    cJSON *after = items_state_to_json(&items_state);
    TEST_ASSERT_TRUE(cJSON_Compare(before, after, true));
    cJSON_Delete(after);
    cJSON_Delete(before);
}

void test_stack_slots_caps_and_multiple_stacks(void) {
    items_container_ref_t bag = create_container(3, ITEMS_CONTAINER_POLICY_GENERIC);
    item_entry_ref_t first = {0};
    int64_t applied = 0;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(bag, "tmpl.potion", 150, 1, "loot:test", &first, &applied));
    TEST_ASSERT_EQUAL_INT64(99, applied);
    TEST_ASSERT_EQUAL_UINT32(1, items_entry_view(first).slot);

    item_entry_ref_t second = {0};
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(bag, "tmpl.potion", 5, 2, "loot:test", &second, &applied));
    TEST_ASSERT_EQUAL_INT64(104, items_stack_count(bag, "tmpl.potion"));
    TEST_ASSERT_TRUE(items_entry_id(second) > items_entry_id(first));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_SLOT_OCCUPIED,
        items_try_stack_add(bag, "tmpl.wood", 1, 2, "loot:test", NULL, NULL));
}

void test_zero_capacity_and_currency_policy(void) {
    items_container_ref_t zero = create_container(0, ITEMS_CONTAINER_POLICY_GENERIC);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_CAPACITY,
        items_try_stack_add(zero, "tmpl.wood", 1, ITEMS_SLOT_AUTO, "loot:test", NULL, NULL));

    items_container_ref_t wallet = create_container(2, ITEMS_CONTAINER_POLICY_CURRENCY_ONLY);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_POLICY,
        items_try_stack_add(wallet, "tmpl.potion", 1, ITEMS_SLOT_AUTO, "loot:test", NULL, NULL));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(wallet, "tmpl.gold", 25, ITEMS_SLOT_AUTO, "loot:test", NULL, NULL));
    TEST_ASSERT_EQUAL_INT64(25, items_stack_count(wallet, "tmpl.gold"));
}

void test_unique_instances_keep_independent_fields(void) {
    items_container_ref_t equipment = create_container(4, ITEMS_CONTAINER_POLICY_EQUIPMENT);
    item_entry_ref_t first = {0};
    item_entry_ref_t second = {0};
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_unique_create(equipment, "tmpl.sword", 0, "loot:test", &first));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_unique_create(equipment, "tmpl.sword", 1, "loot:test", &second));
    items_state.containers_entries[first.index].level = 2;
    items_state.containers_entries[first.index].durability = 0.5F;
    TEST_ASSERT_EQUAL_INT(2, items_entry_view(first).level);
    TEST_ASSERT_TRUE(items_entry_view(first).durability != items_entry_view(second).durability);
    TEST_ASSERT_TRUE(items_entry_id(first) != items_entry_id(second));
}

void test_whole_split_and_merge_identity(void) {
    items_container_ref_t source_container = create_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t destination = create_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    item_entry_ref_t source = {0};
    item_entry_ref_t moved = {0};
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(source_container, "tmpl.wood", 10, 0, "loot:test", &source, NULL));
    uint32_t source_id = items_entry_id(source);

    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_entry_move(source, destination, 4, 1, "loot:test", &moved));
    TEST_ASSERT_TRUE(items_entry_id(moved) != source_id);
    TEST_ASSERT_EQUAL_INT64(6, items_entry_view(source).count);
    TEST_ASSERT_EQUAL_INT64(4, items_entry_view(moved).count);

    item_entry_ref_t merged = {0};
    uint32_t destination_id = items_entry_id(moved);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_entry_move(source, destination, 6, 1, "loot:test", &merged));
    TEST_ASSERT_EQUAL_UINT32(destination_id, items_entry_id(merged));
    TEST_ASSERT_EQUAL_INT64(10, items_entry_view(merged).count);

    item_entry_ref_t sword = {0};
    items_container_ref_t equipment = create_container(2, ITEMS_CONTAINER_POLICY_EQUIPMENT);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_unique_create(equipment, "tmpl.sword", 0, "loot:test", &sword));
    uint32_t sword_id = items_entry_id(sword);
    items_container_ref_t other_equipment = create_container(2, ITEMS_CONTAINER_POLICY_EQUIPMENT);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_entry_move(sword, other_equipment, 1, ITEMS_SLOT_AUTO, "loot:test", &moved));
    TEST_ASSERT_EQUAL_UINT32(sword_id, items_entry_id(moved));
}

void test_resize_requires_all_occupied_slots_to_fit(void) {
    items_container_ref_t bag = create_container(8, ITEMS_CONTAINER_POLICY_GENERIC);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(bag, "tmpl.wood", 1, 7, "loot:test", NULL, NULL));
    TEST_ASSERT_EQUAL_INT(ITEMS_RESULT_CAPACITY, items_try_container_resize(bag, 7));
    TEST_ASSERT_EQUAL_INT(ITEMS_RESULT_OK, items_try_container_resize(bag, 8));
}

void test_rebuild_rejects_duplicates_and_counter_regression(void) {
    items_container_ref_t a = create_container(2, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t b = create_container(2, ITEMS_CONTAINER_POLICY_GENERIC);
    items_state.containers[b.index].container_id = items_state.containers[a.index].container_id;
    char error[128] = {0};
    TEST_ASSERT_FALSE(items_runtime_rebuild(error, (int)sizeof(error)));

    items_state.containers[b.index].container_id = 2;
    items_state.last_container_id = 1;
    TEST_ASSERT_FALSE(items_runtime_rebuild(error, (int)sizeof(error)));
}

void test_staged_runtime_validation_rejects_graph_without_publishing_indices(void) {
    items_container_ref_t container = ITEMS_CONTAINER_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_container_create(
            (items_container_desc_t){
                .capacity = 2,
                .policy = ITEMS_CONTAINER_POLICY_GENERIC,
                .lifetime = ITEMS_LIFETIME_PERSISTENT,
            },
            &container));
    item_entry_ref_t first = ITEM_ENTRY_REF_NONE;
    item_entry_ref_t second = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(container, "tmpl.gold", 1, 0, "loot:test", &first, NULL));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(container, "tmpl.potion", 1, 1, "loot:test", &second, NULL));
    const uint32_t second_id = items_entry_id(second);

    ItemsState staged = items_state;
    staged.containers_entries[second.index].entry_id = items_entry_id(first);
    char error[128] = {0};
    TEST_ASSERT_FALSE(items_runtime_validate_state(&staged, error, (int)sizeof(error)));
    TEST_ASSERT_NOT_NULL(strstr(error, "duplicate entry id"));

    item_entry_ref_t resolved = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_TRUE(items_entry_try_from_id(second_id, &resolved));
    TEST_ASSERT_EQUAL_UINT32(second.index, resolved.index);
    TEST_ASSERT_EQUAL_UINT32(second.generation, resolved.generation);
}

void test_atomic_payment_plans_scope_and_slots_before_one_commit(void) {
    items_container_ref_t first = ITEMS_CONTAINER_REF_NONE;
    items_container_ref_t second = ITEMS_CONTAINER_REF_NONE;
    items_container_desc_t desc = {
        .capacity = 4,
        .policy = ITEMS_CONTAINER_POLICY_GENERIC,
        .lifetime = ITEMS_LIFETIME_PERSISTENT,
    };
    TEST_ASSERT_EQUAL_INT(ITEMS_RESULT_OK, items_try_container_create(desc, &first));
    TEST_ASSERT_EQUAL_INT(ITEMS_RESULT_OK, items_try_container_create(desc, &second));
    item_entry_ref_t later_slot = ITEM_ENTRY_REF_NONE;
    item_entry_ref_t earlier_slot = ITEM_ENTRY_REF_NONE;
    item_entry_ref_t second_source = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(first, "tmpl.gold", 6, 3, "loot:test", &later_slot, NULL));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(first, "tmpl.gold", 6, 1, "loot:test", &earlier_slot, NULL));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(second, "tmpl.gold", 5, 0, "loot:test", &second_source, NULL));
    const uint32_t earlier_slot_id = items_entry_id(earlier_slot);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(second, "tmpl.wood", 1, 2, "loot:test", NULL, NULL));

    item_def_ref_t sword;
    TEST_ASSERT_TRUE(items_try_get_string("tmpl.sword", &sword));
    item_transition_t acquire = items_acquire_transition(sword);
    TEST_ASSERT_EQUAL_INT(ITEM_TRANSITION_COST, acquire.kind);
    items_payment_scope_t scope = {.count = 2, .containers = {first, second}};
    ItemsState before = items_state;
    game_event_frame_reset();
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_INSUFFICIENT,
        items_try_pay_cost(acquire.cost, scope, "shop_buy:sword"));
    TEST_ASSERT_EQUAL_MEMORY(&before, &items_state, sizeof(before));
    int event_count = 0;
    (void)game_event_log(&event_count);
    TEST_ASSERT_EQUAL_INT(0, event_count);

    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(second, "tmpl.wood", 1, 2, "loot:test", NULL, NULL));
    game_event_frame_reset();
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_pay_cost(acquire.cost, scope, "shop_buy:sword"));
    TEST_ASSERT_EQUAL_INT64(2, items_stack_count(first, "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(5, items_stack_count(second, "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(0, items_stack_count(second, "tmpl.wood"));
    item_entry_ref_t resolved = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_FALSE(items_entry_try_from_id(earlier_slot_id, &resolved));
    TEST_ASSERT_TRUE(items_entry_try_from_id(items_entry_id(later_slot), &resolved));
    TEST_ASSERT_EQUAL_INT64(2, items_entry_view(resolved).count);
    TEST_ASSERT_EQUAL_INT64(5, items_entry_view(second_source).count);
    const game_event_t *events = game_event_log(&event_count);
    TEST_ASSERT_EQUAL_INT(1, event_count);
    TEST_ASSERT_EQUAL_UINT64(items_ev_payment_type().value, events[0].type.value);
    const ItemsEvPayment *payment = (const ItemsEvPayment *)events[0].payload;
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0x5be1d477269e01d7), payment->cost_fingerprint.value);
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0x767ccc7cb862dee1), payment->scope_fingerprint.value);
    TEST_ASSERT_EQUAL_HEX64(UINT64_C(0xb1c03818518b2d77), payment->source_fingerprint.value);
    TEST_ASSERT_EQUAL_INT64(2, payment->requirement_count);
    TEST_ASSERT_EQUAL_INT64(2, payment->scope_count);
    TEST_ASSERT_EQUAL_INT64(3, payment->source_entry_count);
    TEST_ASSERT_EQUAL_INT64(12, payment->requested_units);
    TEST_ASSERT_EQUAL_INT64(12, payment->applied_units);
    TEST_ASSERT_EQUAL_STRING("shop_buy:sword", items_ev_payment_reason(payment));
}

void test_rebuild_rejects_reserved_persisted_counters(void) {
    (void)create_container(1, ITEMS_CONTAINER_POLICY_GENERIC);
    char error[128] = {0};

    items_state.last_container_id = UINT32_MAX;
    TEST_ASSERT_FALSE(items_runtime_rebuild(error, (int)sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("reserved persisted id counter", error);

    items_state.last_container_id = 1;
    items_state.last_entry_id = UINT32_MAX;
    TEST_ASSERT_FALSE(items_runtime_rebuild(error, (int)sizeof(error)));
    TEST_ASSERT_EQUAL_STRING("reserved persisted id counter", error);
}

void test_one_hundred_persistent_containers_round_trip(void) {
    for (int i = 0; i < 100; i++) {
        (void)create_container(2, ITEMS_CONTAINER_POLICY_GENERIC);
    }
    cJSON *json = items_state_to_json(&items_state);
    TEST_ASSERT_EQUAL_INT(100, cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(json, "containers")));
    ItemsState loaded;
    char error[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&loaded, json, error, (int)sizeof(error)));
    cJSON *again = items_state_to_json(&loaded);
    TEST_ASSERT_TRUE(cJSON_Compare(json, again, true));
    cJSON_Delete(again);
    cJSON_Delete(json);
}

void test_loaded_maximum_ids_and_long_definition_reseed_without_truncation(void) {
    char long_id[ITEMS_STATE_STRING_MAX];
    memset(long_id, 'x', sizeof(long_id) - 1U);
    long_id[sizeof(long_id) - 1U] = '\0';

    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "last_container_id", (double)(UINT32_MAX - 2U));
    cJSON_AddNumberToObject(json, "last_entry_id", (double)(UINT32_MAX - 2U));
    cJSON *containers = cJSON_AddArrayToObject(json, "containers");
    cJSON *container = cJSON_CreateObject();
    cJSON_AddNumberToObject(container, "container_id", (double)(UINT32_MAX - 2U));
    cJSON_AddNumberToObject(container, "capacity", 2);
    cJSON_AddStringToObject(container, "policy", "generic");
    cJSON *entries = cJSON_AddArrayToObject(container, "entries");
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddNumberToObject(entry, "entry_id", (double)(UINT32_MAX - 2U));
    cJSON_AddNumberToObject(entry, "slot", 0);
    cJSON_AddStringToObject(entry, "def_id", long_id);
    cJSON_AddNumberToObject(entry, "count", 1);
    cJSON_AddNumberToObject(entry, "level", 1);
    cJSON_AddNumberToObject(entry, "durability", 1);
    cJSON_AddBoolToObject(entry, "quarantined", false);
    cJSON_AddItemToArray(entries, entry);
    cJSON_AddItemToArray(containers, container);

    ItemsState loaded;
    char error[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&loaded, json, error, (int)sizeof(error)));
    items_state = loaded;
    TEST_ASSERT_TRUE(items_runtime_rebuild(error, (int)sizeof(error)));
    items_reconcile();

    item_entry_ref_t loaded_entry = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_TRUE(items_entry_try_from_id(UINT32_MAX - 2U, &loaded_entry));
    TEST_ASSERT_EQUAL_UINT32(sizeof(long_id) - 1U, strlen(items_entry_view(loaded_entry).def_id));
    TEST_ASSERT_TRUE(items_entry_view(loaded_entry).quarantined);

    items_container_ref_t loaded_container = ITEMS_CONTAINER_REF_NONE;
    TEST_ASSERT_TRUE(items_container_try_from_id(UINT32_MAX - 2U, &loaded_container));
    items_container_ref_t maximum_container = ITEMS_CONTAINER_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_container_create(
            (items_container_desc_t){1, ITEMS_CONTAINER_POLICY_GENERIC, ITEMS_LIFETIME_PERSISTENT},
            &maximum_container));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 1U, items_container_id(maximum_container));

    item_entry_ref_t maximum_entry = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(
            loaded_container, "tmpl.wood", 1, 1, "loot:test", &maximum_entry, NULL));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 1U, items_entry_id(maximum_entry));

    cJSON *before = items_state_to_json(&items_state);
    items_container_ref_t refused_container = ITEMS_CONTAINER_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_ID_EXHAUSTED,
        items_try_container_create(
            (items_container_desc_t){1, ITEMS_CONTAINER_POLICY_GENERIC, ITEMS_LIFETIME_PERSISTENT},
            &refused_container));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_ID_EXHAUSTED,
        items_try_stack_add(
            maximum_container, "tmpl.wood", 1, 0, "loot:test", NULL, NULL));
    cJSON *after = items_state_to_json(&items_state);
    TEST_ASSERT_TRUE(cJSON_Compare(before, after, true));
    cJSON_Delete(after);
    cJSON_Delete(before);
    cJSON_Delete(json);
}

void test_canonical_serialization_ignores_dense_pool_reuse_order(void) {
    items_container_ref_t first = create_container(6, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t reused = create_container(1, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t third = create_container(1, ITEMS_CONTAINER_POLICY_GENERIC);
    TEST_ASSERT_EQUAL_INT(ITEMS_RESULT_OK, items_try_container_destroy_empty(reused));
    items_container_ref_t fourth = create_container(1, ITEMS_CONTAINER_POLICY_GENERIC);

    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(first, "tmpl.wood", 1, 5, "loot:test", NULL, NULL));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(first, "tmpl.potion", 1, 2, "loot:test", NULL, NULL));

    cJSON *json = items_state_to_json(&items_state);
    cJSON *containers = cJSON_GetObjectItemCaseSensitive(json, "containers");
    TEST_ASSERT_EQUAL_UINT32(
        items_container_id(first),
        (uint32_t)cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(containers, 0), "container_id")->valuedouble);
    TEST_ASSERT_EQUAL_UINT32(
        items_container_id(third),
        (uint32_t)cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(containers, 1), "container_id")->valuedouble);
    TEST_ASSERT_EQUAL_UINT32(
        items_container_id(fourth),
        (uint32_t)cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(containers, 2), "container_id")->valuedouble);
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(containers, 0), "entries");
    TEST_ASSERT_EQUAL_INT(
        2, (int)cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(entries, 0), "slot")->valuedouble);
    TEST_ASSERT_EQUAL_INT(
        5, (int)cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(entries, 1), "slot")->valuedouble);

    ItemsState loaded;
    char error[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&loaded, json, error, (int)sizeof(error)));
    cJSON *again = items_state_to_json(&loaded);
    TEST_ASSERT_TRUE(cJSON_Compare(json, again, true));
    cJSON_Delete(again);
    cJSON_Delete(json);
}

void test_full_entry_pool_rebuilds_reverse_id_index_and_refuses_atomically(void) {
    items_container_ref_t large = create_container(
        ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX,
        ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t spill = create_container(1, ITEMS_CONTAINER_POLICY_GENERIC);

    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        ItemsItemEntry *entry = &items_state.containers_entries[i];
        memset(entry, 0, sizeof(*entry));
        entry->used = true;
        entry->parent_index = i + 1U == ITEMS_STATE_MAX_CONTAINERS_ENTRIES
            ? (int)spill.index : (int)large.index;
        entry->entry_id = ITEMS_STATE_MAX_CONTAINERS_ENTRIES - i;
        entry->slot = i + 1U == ITEMS_STATE_MAX_CONTAINERS_ENTRIES ? 0 : i;
        (void)snprintf(entry->def_id, sizeof(entry->def_id), "tmpl.wood");
        entry->count = 1;
        entry->level = ITEMS_STATE_ITEM_ENTRY_LEVEL_DEFAULT;
        entry->durability = ITEMS_STATE_ITEM_ENTRY_DURABILITY_DEFAULT;
    }
    items_state.last_entry_id = ITEMS_STATE_MAX_CONTAINERS_ENTRIES;

    char error[128] = {0};
    TEST_ASSERT_TRUE(items_runtime_rebuild(error, (int)sizeof(error)));
    TEST_ASSERT_TRUE(items_container_try_from_id(1U, &large));
    const uint32_t ids[] = {1U, ITEMS_STATE_MAX_CONTAINERS_ENTRIES / 2U,
                            ITEMS_STATE_MAX_CONTAINERS_ENTRIES};
    for (uint32_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        item_entry_ref_t found = ITEM_ENTRY_REF_NONE;
        TEST_ASSERT_TRUE(items_entry_try_from_id(ids[i], &found));
        TEST_ASSERT_EQUAL_UINT32(ids[i], items_entry_id(found));
    }

    cJSON *before = items_state_to_json(&items_state);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_POOL_EXHAUSTED,
        items_try_stack_add(
            large, "tmpl.potion", 1,
            ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX - 1U,
            "loot:test", NULL, NULL));
    cJSON *after = items_state_to_json(&items_state);
    TEST_ASSERT_TRUE(cJSON_Compare(before, after, true));
    cJSON_Delete(after);
    cJSON_Delete(before);
}

void test_missing_definition_quarantines_and_restores(void) {
    items_container_ref_t bag = create_container(2, ITEMS_CONTAINER_POLICY_GENERIC);
    item_entry_ref_t entry = {0};
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(bag, "tmpl.wood", 1, 0, "loot:test", &entry, NULL));
    (void)snprintf(items_state.containers_entries[entry.index].def_id,
        sizeof(items_state.containers_entries[entry.index].def_id), "ghost");
    items_reconcile();
    TEST_ASSERT_TRUE(items_state.containers_entries[entry.index].quarantined);
    (void)snprintf(items_state.containers_entries[entry.index].def_id,
        sizeof(items_state.containers_entries[entry.index].def_id), "tmpl.wood");
    items_reconcile();
    TEST_ASSERT_FALSE(items_state.containers_entries[entry.index].quarantined);
}

void test_ephemeral_entries_never_enter_persistent_state(void) {
    items_container_ref_t chest = create_ephemeral_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    item_entry_ref_t wood = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(ITEMS_LIFETIME_EPHEMERAL, items_container_lifetime(chest));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(chest, "tmpl.wood", 8, 0, "loot:test", &wood, NULL));
    TEST_ASSERT_EQUAL_INT(ITEMS_LIFETIME_EPHEMERAL, items_entry_view(wood).lifetime);
    TEST_ASSERT_EQUAL_INT64(8, items_stack_count(chest, "tmpl.wood"));

    cJSON *json = items_state_to_json(&items_state);
    TEST_ASSERT_EQUAL_INT(
        0, cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(json, "containers")));
    TEST_ASSERT_EQUAL_UINT32(0, items_state.last_container_id);
    TEST_ASSERT_EQUAL_UINT32(0, items_state.last_entry_id);
    cJSON_Delete(json);

    items_container_ref_t old = chest;
    TEST_ASSERT_TRUE(items_runtime_rebuild(NULL, 0));
    items_container_ref_t replacement = create_ephemeral_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    TEST_ASSERT_TRUE(old.index == replacement.index);
    TEST_ASSERT_TRUE(old.generation != replacement.generation);
}

void test_lifetime_boundary_rejects_persistent_to_ephemeral_and_acquires_reverse(void) {
    items_container_ref_t bag = create_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t chest = create_ephemeral_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    item_entry_ref_t persistent = ITEM_ENTRY_REF_NONE;
    item_entry_ref_t temporary = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(bag, "tmpl.wood", 10, 0, "loot:test", &persistent, NULL));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_LIFETIME,
        items_try_entry_move(persistent, chest, 4, 0, "drop:test", NULL));
    TEST_ASSERT_EQUAL_INT64(10, items_entry_view(persistent).count);

    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(chest, "tmpl.wood", 10, 0, "loot:test", &temporary, NULL));
    item_entry_ref_t acquired = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_entry_move(temporary, bag, 4, 1, "pickup:test", &acquired));
    TEST_ASSERT_EQUAL_INT(ITEMS_LIFETIME_PERSISTENT, items_entry_view(acquired).lifetime);
    TEST_ASSERT_TRUE(items_entry_id(acquired) > ITEMS_ID_NONE);
    TEST_ASSERT_EQUAL_INT64(4, items_entry_view(acquired).count);
    TEST_ASSERT_EQUAL_INT64(6, items_entry_view(temporary).count);
}

void test_ephemeral_move_preserves_runtime_identity(void) {
    items_container_ref_t source = create_ephemeral_container(2, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t destination = create_ephemeral_container(2, ITEMS_CONTAINER_POLICY_GENERIC);
    item_entry_ref_t entry = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(source, "tmpl.wood", 3, 0, "loot:test", &entry, NULL));
    item_entry_ref_t moved = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_entry_move(entry, destination, 3, 1, "drop:test", &moved));
    TEST_ASSERT_EQUAL_UINT32(entry.index, moved.index);
    TEST_ASSERT_EQUAL_UINT32(entry.generation, moved.generation);
    TEST_ASSERT_EQUAL_INT64(3, items_stack_count(destination, "tmpl.wood"));
}

void test_ephemeral_partial_self_move_never_duplicates_a_slot(void) {
    items_container_ref_t chest = create_ephemeral_container(3, ITEMS_CONTAINER_POLICY_GENERIC);
    item_entry_ref_t source = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(chest, "tmpl.wood", 10, 0, "loot:test", &source, NULL));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_SLOT_OCCUPIED,
        items_try_entry_move(source, chest, 4, 0, "split:test", NULL));
    TEST_ASSERT_EQUAL_INT64(10, items_entry_view(source).count);

    item_entry_ref_t split = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_entry_move(source, chest, 4, ITEMS_SLOT_AUTO, "split:test", &split));
    TEST_ASSERT_EQUAL_UINT32(0, items_entry_view(source).slot);
    TEST_ASSERT_EQUAL_UINT32(1, items_entry_view(split).slot);
    TEST_ASSERT_EQUAL_INT64(6, items_entry_view(source).count);
    TEST_ASSERT_EQUAL_INT64(4, items_entry_view(split).count);
}

void test_rejected_rebuild_preserves_ephemeral_state(void) {
    items_container_ref_t first = create_container(1, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t second = create_container(1, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t chest = create_ephemeral_container(1, ITEMS_CONTAINER_POLICY_GENERIC);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(chest, "tmpl.wood", 3, 0, "loot:test", NULL, NULL));

    items_state.containers[second.index].container_id = items_state.containers[first.index].container_id;
    char error[128] = {0};
    TEST_ASSERT_FALSE(items_runtime_rebuild(error, (int)sizeof(error)));
    TEST_ASSERT_EQUAL_INT64(3, items_stack_count(chest, "tmpl.wood"));
}

void test_container_inspection_is_paginated_filterable_and_bounded(void) {
    items_container_ref_t empty = create_container(4, ITEMS_CONTAINER_POLICY_GENERIC);
    items_container_ref_t wallet = create_container(3, ITEMS_CONTAINER_POLICY_CURRENCY_ONLY);
    items_container_ref_t chest = create_ephemeral_container(2, ITEMS_CONTAINER_POLICY_GENERIC);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(wallet, "tmpl.gold", 5, 1, "loot:test", NULL, NULL));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(chest, "tmpl.wood", 2, 0, "loot:test", NULL, NULL));

    items_container_inspection_t rows[2] = {0};
    items_inspection_page_t page = {0};
    items_container_list_query_t query = {
        .offset = 0,
        .policy = ITEMS_INSPECTION_FILTER_ANY,
        .lifetime = ITEMS_INSPECTION_FILTER_ANY,
        .include_empty = true,
        .budget = inspection_budget(2),
    };
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_OK,
        items_inspect_container_list(&query, rows, 2, &page));
    TEST_ASSERT_EQUAL_UINT32(2, page.count);
    TEST_ASSERT_TRUE(page.has_more);
    TEST_ASSERT_EQUAL_UINT32(2, page.next_offset);
    TEST_ASSERT_EQUAL_UINT32(items_container_id(empty), rows[0].container_id);
    TEST_ASSERT_EQUAL_UINT32(items_container_id(wallet), rows[1].container_id);
    TEST_ASSERT_EQUAL_INT(ITEMS_LIFETIME_PERSISTENT, rows[1].lifetime);
    TEST_ASSERT_EQUAL_UINT32(1, rows[1].entry_count);

    query.offset = page.next_offset;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_OK,
        items_inspect_container_list(&query, rows, 2, &page));
    TEST_ASSERT_EQUAL_UINT32(1, page.count);
    TEST_ASSERT_FALSE(page.has_more);
    TEST_ASSERT_EQUAL_INT(ITEMS_LIFETIME_EPHEMERAL, rows[0].lifetime);
    TEST_ASSERT_EQUAL_UINT32(ITEMS_ID_NONE, rows[0].container_id);
    TEST_ASSERT_EQUAL_UINT32(chest.index, rows[0].ref.index);

    query.offset = 0;
    query.include_empty = false;
    query.lifetime = ITEMS_LIFETIME_PERSISTENT;
    query.policy = ITEMS_CONTAINER_POLICY_CURRENCY_ONLY;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_OK,
        items_inspect_container_list(&query, rows, 2, &page));
    TEST_ASSERT_EQUAL_UINT32(1, page.count);
    TEST_ASSERT_EQUAL_UINT32(items_container_id(wallet), rows[0].container_id);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_CONTAINER_POLICY_CURRENCY_ONLY,
        items_container_policy(rows[0].ref));

    query.budget.max_rows = ITEMS_INSPECTION_MAX_ROWS + 1U;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_ROW_LIMIT,
        items_inspect_container_list(&query, rows, 2, &page));
    query.budget = inspection_budget(1);
    query.budget.max_bytes = (uint32_t)sizeof(items_container_inspection_t) - 1U;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_BYTE_LIMIT,
        items_inspect_container_list(&query, rows, 2, &page));
    query.budget = inspection_budget(1);
    query.budget.max_context_rows = 1;
    query.policy = ITEMS_CONTAINER_POLICY_EQUIPMENT;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_CONTEXT_LIMIT,
        items_inspect_container_list(&query, rows, 2, &page));
}

void test_container_entry_inspection_requires_range_and_filters_in_slot_order(void) {
    items_container_ref_t bag = create_container(8, ITEMS_CONTAINER_POLICY_GENERIC);
    item_entry_ref_t wood = ITEM_ENTRY_REF_NONE;
    item_entry_ref_t potion = ITEM_ENTRY_REF_NONE;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(bag, "tmpl.wood", 3, 5, "loot:test", &wood, NULL));
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(bag, "tmpl.potion", 2, 2, "loot:test", &potion, NULL));
    items_state.containers_entries[wood.index].quarantined = true;

    items_entry_inspection_t rows[2] = {0};
    items_inspection_page_t page = {0};
    items_entry_list_query_t query = {
        .offset = 0,
        .slot_begin = 0,
        .slot_end = 8,
        .def_id = NULL,
        .quarantined = ITEMS_INSPECTION_FILTER_ANY,
        .budget = inspection_budget(1),
    };
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_OK,
        items_inspect_container_entries(bag, &query, rows, 2, &page));
    TEST_ASSERT_EQUAL_UINT32(1, page.count);
    TEST_ASSERT_TRUE(page.has_more);
    TEST_ASSERT_EQUAL_UINT32(2, rows[0].view.slot);
    TEST_ASSERT_EQUAL_STRING("tmpl.potion", rows[0].view.def_id);

    query.offset = page.next_offset;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_OK,
        items_inspect_container_entries(bag, &query, rows, 2, &page));
    TEST_ASSERT_EQUAL_UINT32(1, page.count);
    TEST_ASSERT_FALSE(page.has_more);
    TEST_ASSERT_EQUAL_UINT32(5, rows[0].view.slot);
    TEST_ASSERT_TRUE(rows[0].view.quarantined);

    query.offset = 0;
    query.def_id = "tmpl.wood";
    query.quarantined = 1;
    query.budget = inspection_budget(2);
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_OK,
        items_inspect_container_entries(bag, &query, rows, 2, &page));
    TEST_ASSERT_EQUAL_UINT32(1, page.count);
    TEST_ASSERT_EQUAL_UINT32(items_entry_id(wood), rows[0].view.entry_id);

    query.slot_end = query.slot_begin;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_BAD_QUERY,
        items_inspect_container_entries(bag, &query, rows, 2, &page));
    query.slot_end = 9;
    TEST_ASSERT_EQUAL_INT(
        ITEMS_INSPECTION_BAD_QUERY,
        items_inspect_container_entries(bag, &query, rows, 2, &page));
}

void test_success_emits_one_numeric_compatibility_event(void) {
    items_container_ref_t wallet = create_container(2, ITEMS_CONTAINER_POLICY_CURRENCY_ONLY);
    game_event_frame_reset();
    item_entry_ref_t entry = {0};
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK,
        items_try_stack_add(wallet, "tmpl.gold", 42, 0, "loot:event", &entry, NULL));

    int count = 0;
    const game_event_t *events = game_event_log(&count);
    int txn_count = 0;
    for (int i = 0; i < count; i++) {
        if (events[i].type.value != items_ev_txn_type().value) { continue; }
        const ItemsEvTxn *event = (const ItemsEvTxn *)events[i].payload;
        TEST_ASSERT_EQUAL_STRING("1", items_ev_txn_container(event));
        TEST_ASSERT_EQUAL_STRING("1", items_ev_txn_entry_key(event));
        TEST_ASSERT_EQUAL_INT64(42, event->applied_delta);
        txn_count++;
    }
    TEST_ASSERT_EQUAL_INT(1, txn_count);
}

int main(void) {
    if (!items_runtime_test_catalog_bind()) { return 1; }
    game_events_init();
    UNITY_BEGIN();
    RUN_TEST(test_empty_nested_state_round_trip);
    RUN_TEST(test_container_ids_are_monotone_and_reserved_max_refuses);
    RUN_TEST(test_entry_id_reaches_maximum_then_exhausts_without_mutation);
    RUN_TEST(test_stack_slots_caps_and_multiple_stacks);
    RUN_TEST(test_zero_capacity_and_currency_policy);
    RUN_TEST(test_unique_instances_keep_independent_fields);
    RUN_TEST(test_whole_split_and_merge_identity);
    RUN_TEST(test_resize_requires_all_occupied_slots_to_fit);
    RUN_TEST(test_rebuild_rejects_duplicates_and_counter_regression);
    RUN_TEST(test_staged_runtime_validation_rejects_graph_without_publishing_indices);
    RUN_TEST(test_atomic_payment_plans_scope_and_slots_before_one_commit);
    RUN_TEST(test_rebuild_rejects_reserved_persisted_counters);
    RUN_TEST(test_one_hundred_persistent_containers_round_trip);
    RUN_TEST(test_loaded_maximum_ids_and_long_definition_reseed_without_truncation);
    RUN_TEST(test_canonical_serialization_ignores_dense_pool_reuse_order);
    RUN_TEST(test_full_entry_pool_rebuilds_reverse_id_index_and_refuses_atomically);
    RUN_TEST(test_missing_definition_quarantines_and_restores);
    RUN_TEST(test_ephemeral_entries_never_enter_persistent_state);
    RUN_TEST(test_lifetime_boundary_rejects_persistent_to_ephemeral_and_acquires_reverse);
    RUN_TEST(test_ephemeral_move_preserves_runtime_identity);
    RUN_TEST(test_ephemeral_partial_self_move_never_duplicates_a_slot);
    RUN_TEST(test_rejected_rebuild_preserves_ephemeral_state);
    RUN_TEST(test_container_inspection_is_paginated_filterable_and_bounded);
    RUN_TEST(test_container_entry_inspection_requires_range_and_filters_in_slot_order);
    RUN_TEST(test_success_emits_one_numeric_compatibility_event);
    int result = UNITY_END();
    game_events_shutdown();
    items_catalog_shutdown();
    return result;
}
