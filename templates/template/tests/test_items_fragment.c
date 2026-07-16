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
    RUN_TEST(test_stack_slots_caps_and_multiple_stacks);
    RUN_TEST(test_zero_capacity_and_currency_policy);
    RUN_TEST(test_unique_instances_keep_independent_fields);
    RUN_TEST(test_whole_split_and_merge_identity);
    RUN_TEST(test_resize_requires_all_occupied_slots_to_fit);
    RUN_TEST(test_rebuild_rejects_duplicates_and_counter_regression);
    RUN_TEST(test_one_hundred_persistent_containers_round_trip);
    RUN_TEST(test_missing_definition_quarantines_and_restores);
    RUN_TEST(test_ephemeral_entries_never_enter_persistent_state);
    RUN_TEST(test_lifetime_boundary_rejects_persistent_to_ephemeral_and_acquires_reverse);
    RUN_TEST(test_ephemeral_move_preserves_runtime_identity);
    RUN_TEST(test_ephemeral_partial_self_move_never_duplicates_a_slot);
    RUN_TEST(test_rejected_rebuild_preserves_ephemeral_state);
    RUN_TEST(test_success_emits_one_numeric_compatibility_event);
    int result = UNITY_END();
    game_events_shutdown();
    items_catalog_shutdown();
    return result;
}
