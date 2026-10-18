#include "unity.h"

#include "features/items/items.h"
#include "hash/nt_hash.h"

#ifndef ITEMS_GAME_HAS_WEAPON
#error "weapon proof did not generate its game-specific capability"
#endif

void setUp(void) {}
void tearDown(void) {}

void test_all_weapon_fixture_ids_match_engine_hash(void) {
    TEST_ASSERT_EQUAL_UINT64(nt_hash64_str("game.gold").value, ITEM_GAME_GOLD.value);
    TEST_ASSERT_EQUAL_UINT64(nt_hash64_str("game.metal").value, ITEM_GAME_METAL.value);
    TEST_ASSERT_EQUAL_UINT64(nt_hash64_str("game.fixed_sword").value, ITEM_GAME_FIXED_SWORD.value);
    TEST_ASSERT_EQUAL_UINT64(nt_hash64_str("game.iron_sword").value, ITEM_GAME_IRON_SWORD.value);
    TEST_ASSERT_EQUAL_UINT64(
        nt_hash64_str("game.extraordinarily_long_balance_resource_identifier").value,
        ITEM_GAME_EXTRAORDINARILY_LONG_BALANCE_RESOURCE_IDENTIFIER.value);
}

void test_core_copy_has_only_value_fields(void) {
    item_core_t core = items_core(items_get(ITEM_GAME_GOLD));
    TEST_ASSERT_EQUAL_UINT64(ITEM_GAME_GOLD.value, core.id.value);
    TEST_ASSERT_EQUAL_INT64(0, core.stack);
}

void test_weapon_api_copies_levels_and_distinguishes_transitions(void) {
    item_def_ref_t sword = items_get(ITEM_GAME_IRON_SWORD);
    TEST_ASSERT_TRUE(items_is_weapon(sword));
    TEST_ASSERT_EQUAL_UINT32(3, items_weapon_level_count(sword));
    TEST_ASSERT_TRUE(items_weapon_level_exists(sword, 1));
    TEST_ASSERT_FALSE(items_weapon_level_exists(sword, 4));

    item_weapon_level_t level1 = items_weapon_level(sword, 1);
    item_weapon_level_t level2 = items_weapon_level(sword, 2);
    item_weapon_level_t level3 = items_weapon_level(sword, 3);
    TEST_ASSERT_EQUAL_INT64(10, level1.attack);
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_UNAVAILABLE, level1.cost_to_reach.kind);
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_COST, level2.cost_to_reach.kind);
    TEST_ASSERT_EQUAL_UINT32(2, items_cost_count(level2.cost_to_reach.cost));
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_FREE, level3.cost_to_reach.kind);
    TEST_ASSERT_EQUAL_INT64(21, level3.attack);
}

void test_acquire_cost_is_opaque_and_copy_out(void) {
    item_transition_t acquire = items_acquire_transition(items_get(ITEM_GAME_FIXED_SWORD));
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_COST, acquire.kind);
    TEST_ASSERT_EQUAL_UINT32(1, items_cost_count(acquire.cost));
    item_cost_entry_t entry = items_cost_at(acquire.cost, 0);
    TEST_ASSERT_EQUAL_UINT64(ITEM_GAME_GOLD.value, entry.item.value);
    TEST_ASSERT_EQUAL_INT64(100, entry.count);
}

void test_generic_level_transition_api_does_not_require_capability_fields(void) {
    item_def_ref_t sword = items_get(ITEM_GAME_IRON_SWORD);
    TEST_ASSERT_EQUAL_UINT32(3, items_level_count(sword));
    TEST_ASSERT_TRUE(items_level_exists(sword, 2));
    TEST_ASSERT_FALSE(items_level_exists(sword, 4));
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_UNAVAILABLE, items_level_transition(sword, 1).kind);
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_COST, items_level_transition(sword, 2).kind);
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_FREE, items_level_transition(sword, 3).kind);
}

int main(void) {
    items_register_debug_labels();
    UNITY_BEGIN();
    RUN_TEST(test_all_weapon_fixture_ids_match_engine_hash);
    RUN_TEST(test_core_copy_has_only_value_fields);
    RUN_TEST(test_weapon_api_copies_levels_and_distinguishes_transitions);
    RUN_TEST(test_acquire_cost_is_opaque_and_copy_out);
    RUN_TEST(test_generic_level_transition_api_does_not_require_capability_fields);
    return UNITY_END();
}
