#include "unity.h"

#include "cJSON.h"
#include "game_save_writer.h"
#include "items_v2_state.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_generated_snapshot_matches_cjson_with_more_than_512_nested_entries(void) {
    ItemsV2State state;
    items_v2_state_init_defaults(&state);
    state.last_container_id = 1U;
    state.last_entry_id = 513U;
    state.containers[0].used = true;
    state.containers[0].container_id = 1U;
    state.containers[0].capacity = 513U;
    state.containers[0].policy = ITEMS_V2_STATE_CONTAINER_POLICY_GENERIC;
    state.containers[0].persistent = true;
    for (int i = 0; i < 513; i++) {
        ItemsV2ItemEntry *entry = &state.containers_entries[i];
        entry->used = true;
        entry->parent_index = 0;
        entry->entry_id = (uint32_t)(i + 1);
        entry->slot = (uint32_t)(512 - i);
        (void)snprintf(entry->def_id, sizeof entry->def_id, "item_%03d", i);
        entry->count = i + 1;
        entry->level = 1;
        entry->durability = 1.0F;
    }

    cJSON *legacy = items_v2_state_to_json(&state);
    TEST_ASSERT_NOT_NULL(legacy);
    char *expected = cJSON_PrintUnformatted(legacy);
    TEST_ASSERT_NOT_NULL(expected);

    static char snapshot[128U * 1024U];
    game_save_writer_t writer;
    game_save_writer_init(&writer, snapshot, sizeof snapshot);
    TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
    TEST_ASSERT_TRUE(items_v2_state_write_snapshot(&state, &writer));
    TEST_ASSERT_TRUE(game_save_writer_end_object(&writer));
    TEST_ASSERT_TRUE(game_save_writer_complete(&writer));
    TEST_ASSERT_EQUAL_STRING(expected, snapshot);

    cJSON_free(expected);
    cJSON_Delete(legacy);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_generated_snapshot_matches_cjson_with_more_than_512_nested_entries);
    return UNITY_END();
}
