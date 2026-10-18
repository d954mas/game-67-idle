#include "unity.h"

#include "features/items/items.h"
#include "hash/nt_hash.h"

#ifdef ITEMS_GAME_HAS_WEAPON
#error "core-only generated API leaked weapon capability"
#endif

void setUp(void) {}
void tearDown(void) {}

void test_core_only_ids_match_engine_hash(void) {
    TEST_ASSERT_EQUAL_UINT64(nt_hash64_str("game.gold").value, ITEM_GAME_GOLD.value);
    TEST_ASSERT_EQUAL_UINT64(nt_hash64_str("game.metal").value, ITEM_GAME_METAL.value);
    TEST_ASSERT_EQUAL_UINT64(
        nt_hash64_str("game.extraordinarily_long_balance_resource_identifier").value,
        ITEM_GAME_EXTRAORDINARILY_LONG_BALANCE_RESOURCE_IDENTIFIER.value);
}

void test_core_only_lookup_uses_stable_items_api(void) {
    item_def_ref_t gold;
    TEST_ASSERT_TRUE(items_try_get_string("game.gold", &gold));
    item_core_t core = items_core(gold);
    TEST_ASSERT_EQUAL_UINT64(ITEM_GAME_GOLD.value, core.id.value);
    TEST_ASSERT_EQUAL_INT64(0, core.stack);
    TEST_ASSERT_TRUE(items_exists(ITEM_GAME_GOLD));
    TEST_ASSERT_FALSE(items_try_get_string("game.missing", &gold));
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_UNAVAILABLE, items_acquire_transition(items_get(ITEM_GAME_GOLD)).kind);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_core_only_ids_match_engine_hash);
    RUN_TEST(test_core_only_lookup_uses_stable_items_api);
    return UNITY_END();
}
