#include "unity.h"

#include <stdint.h>
#include <string.h>

#include "items_v2_state.h"

static cJSON *make_entry(uint32_t entry_id, uint32_t slot, const char *def_id) {
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddNumberToObject(entry, "entry_id", (double)entry_id);
    cJSON_AddNumberToObject(entry, "slot", (double)slot);
    cJSON_AddStringToObject(entry, "def_id", def_id);
    cJSON_AddStringToObject(entry, "count", "1");
    cJSON_AddNumberToObject(entry, "level", 1);
    cJSON_AddNumberToObject(entry, "durability", 1.0);
    cJSON_AddBoolToObject(entry, "quarantined", false);
    return entry;
}

static cJSON *make_container(uint32_t container_id, uint32_t capacity) {
    cJSON *container = cJSON_CreateObject();
    cJSON_AddNumberToObject(container, "container_id", (double)container_id);
    cJSON_AddNumberToObject(container, "capacity", (double)capacity);
    cJSON_AddStringToObject(container, "policy", "generic");
    cJSON_AddBoolToObject(container, "persistent", true);
    cJSON_AddArrayToObject(container, "entries");
    return container;
}

void setUp(void) {}
void tearDown(void) {}

void test_empty_and_exact_u32_round_trip(void) {
    ItemsV2State state;
    items_v2_state_init_defaults(&state);
    state.last_container_id = UINT32_MAX - 1U;
    state.last_entry_id = UINT32_MAX - 1U;

    cJSON *json = items_v2_state_to_json(&state);
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(json, "last_container_id")));

    ItemsV2State loaded;
    char error[128] = {0};
    TEST_ASSERT_TRUE(items_v2_state_from_json(&loaded, json, error, (int)sizeof(error)));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 1U, loaded.last_container_id);
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 1U, loaded.last_entry_id);
    cJSON_Delete(json);
}

void test_sparse_pools_project_in_canonical_nested_order(void) {
    ItemsV2State state;
    items_v2_state_init_defaults(&state);

    state.containers[5].used = true;
    state.containers[5].container_id = 20;
    state.containers[5].capacity = 8;
    state.containers[5].persistent = true;
    state.containers[2].used = true;
    state.containers[2].container_id = 10;
    state.containers[2].capacity = 4;
    state.containers[2].persistent = true;

    state.containers_entries[7].used = true;
    state.containers_entries[7].parent_index = 5;
    state.containers_entries[7].entry_id = 4;
    state.containers_entries[7].slot = 2;
    (void)strcpy(state.containers_entries[7].def_id, "sword");
    state.containers_entries[7].count = 1;
    state.containers_entries[7].level = 1;
    state.containers_entries[7].durability = 1.0F;
    state.containers_entries[1] = state.containers_entries[7];
    state.containers_entries[1].entry_id = 5;
    state.containers_entries[1].slot = 0;

    cJSON *json = items_v2_state_to_json(&state);
    cJSON *containers = cJSON_GetObjectItemCaseSensitive(json, "containers");
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(containers));
    TEST_ASSERT_EQUAL_UINT32(10, (uint32_t)cJSON_GetArrayItem(containers, 0)->child->valuedouble);

    cJSON *second = cJSON_GetArrayItem(containers, 1);
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(second, "entries");
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(entries));
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(entries, 0), "slot")->valuedouble);
    TEST_ASSERT_NULL(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(entries, 0), "container_id"));

    ItemsV2State loaded;
    char error[128] = {0};
    TEST_ASSERT_TRUE(items_v2_state_from_json(&loaded, json, error, (int)sizeof(error)));
    cJSON *again = items_v2_state_to_json(&loaded);
    TEST_ASSERT_TRUE(cJSON_Compare(json, again, true));
    cJSON_Delete(again);
    cJSON_Delete(json);
}

void test_structural_budget_and_truncation_reject_without_publish(void) {
    ItemsV2State state;
    items_v2_state_init_defaults(&state);
    state.last_container_id = 77;
    char error[128] = {0};

    cJSON *truncated = cJSON_CreateObject();
    cJSON *containers = cJSON_AddArrayToObject(truncated, "containers");
    cJSON_AddItemToArray(containers, cJSON_CreateObject());
    TEST_ASSERT_FALSE(items_v2_state_from_json(&state, truncated, error, (int)sizeof(error)));
    TEST_ASSERT_EQUAL_UINT32(77, state.last_container_id);
    cJSON_Delete(truncated);

    cJSON *excessive = cJSON_CreateObject();
    containers = cJSON_AddArrayToObject(excessive, "containers");
    cJSON *container = make_container(1, 2048);
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(container, "entries");
    for (int i = 0; i < ITEMS_V2_STATE_MAX_CONTAINERS_ENTRIES + 1; i++) {
        cJSON_AddItemToArray(entries, cJSON_CreateObject());
    }
    cJSON_AddItemToArray(containers, container);
    TEST_ASSERT_FALSE(items_v2_state_from_json(&state, excessive, error, (int)sizeof(error)));
    TEST_ASSERT_EQUAL_UINT32(77, state.last_container_id);
    cJSON_Delete(excessive);
}

void test_max_containers_and_devapi_facing_path_round_trip(void) {
    cJSON *containers = cJSON_CreateArray();
    for (int i = 0; i < ITEMS_V2_STATE_MAX_CONTAINERS; i++) {
        cJSON_AddItemToArray(containers, make_container((uint32_t)i + 1U, 0));
    }

    ItemsV2State state;
    items_v2_state_init_defaults(&state);
    char error[128] = {0};
    TEST_ASSERT_TRUE(items_v2_state_set_path_json(&state, "containers", containers, error, (int)sizeof(error)));
    cJSON *projected = items_v2_state_get_path_json(&state, "containers", error, (int)sizeof(error));
    TEST_ASSERT_EQUAL_INT(ITEMS_V2_STATE_MAX_CONTAINERS, cJSON_GetArraySize(projected));
    TEST_ASSERT_TRUE(cJSON_Compare(containers, projected, true));

    cJSON *schema = items_v2_state_schema_json();
    cJSON *reserved = cJSON_GetObjectItemCaseSensitive(schema, "reserved");
    TEST_ASSERT_TRUE(cJSON_IsArray(reserved));
    TEST_ASSERT_EQUAL_STRING("owned", cJSON_GetArrayItem(reserved, 0)->valuestring);

    cJSON_Delete(schema);
    cJSON_Delete(projected);
    cJSON_Delete(containers);
}

void test_max_nested_pool_round_trip(void) {
    cJSON *containers = cJSON_CreateArray();
    cJSON *container = make_container(1, ITEMS_V2_STATE_MAX_CONTAINERS_ENTRIES);
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(container, "entries");
    for (int i = 0; i < ITEMS_V2_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        cJSON_AddItemToArray(entries, make_entry((uint32_t)i + 1U, (uint32_t)i, "gold"));
    }
    cJSON_AddItemToArray(containers, container);

    ItemsV2State state;
    items_v2_state_init_defaults(&state);
    char error[128] = {0};
    TEST_ASSERT_TRUE(items_v2_state_set_path_json(&state, "containers", containers, error, (int)sizeof(error)));
    cJSON *projected = items_v2_state_get_path_json(&state, "containers", error, (int)sizeof(error));
    cJSON *projected_entries = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(projected, 0), "entries");
    TEST_ASSERT_EQUAL_INT(ITEMS_V2_STATE_MAX_CONTAINERS_ENTRIES, cJSON_GetArraySize(projected_entries));
    cJSON_Delete(projected);
    cJSON_Delete(containers);
}

void test_nested_u32_string_is_rejected(void) {
    cJSON *containers = cJSON_CreateArray();
    cJSON *container = make_container(1, 1);
    cJSON *entries = cJSON_GetObjectItemCaseSensitive(container, "entries");
    cJSON *entry = make_entry(1, 0, "gold");
    cJSON_ReplaceItemInObjectCaseSensitive(entry, "entry_id", cJSON_CreateString("1"));
    cJSON_AddItemToArray(entries, entry);
    cJSON_AddItemToArray(containers, container);

    ItemsV2State state;
    items_v2_state_init_defaults(&state);
    state.last_container_id = 77;
    char error[128] = {0};
    TEST_ASSERT_FALSE(items_v2_state_set_path_json(&state, "containers", containers, error, (int)sizeof(error)));
    TEST_ASSERT_EQUAL_UINT32(77, state.last_container_id);
    TEST_ASSERT_FALSE(state.containers[0].used);
    cJSON_Delete(containers);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_and_exact_u32_round_trip);
    RUN_TEST(test_sparse_pools_project_in_canonical_nested_order);
    RUN_TEST(test_structural_budget_and_truncation_reject_without_publish);
    RUN_TEST(test_max_containers_and_devapi_facing_path_round_trip);
    RUN_TEST(test_max_nested_pool_round_trip);
    RUN_TEST(test_nested_u32_string_is_rejected);
    return UNITY_END();
}
