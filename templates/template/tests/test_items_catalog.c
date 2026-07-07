#include "unity.h"

#include "features/items/items.h"

#include <string.h>

/* И2a (§6.8): catalog-lookup over the const tables generated from
   content/items.json by features/items-core/scripts/generate_items_catalog.py.
   Compiles ALWAYS (no
   #if FEATURE_GAME_STATE -- the axis is gone, И2-0). Lint note (G11/§10): this
   file never strcmp()s on display_name -- only ids/tags/kinds are compared. */

void setUp(void) {}
void tearDown(void) {}

void test_item_core_known_currency(void) {
    const game_item_def_t *gold = item_core("tmpl.gold");
    TEST_ASSERT_NOT_NULL(gold);
    TEST_ASSERT_TRUE(item_is_currency(gold));
    TEST_ASSERT_EQUAL_STRING("tmpl.gold", gold->id);
    TEST_ASSERT_TRUE(gold->stackable);
    TEST_ASSERT_TRUE(gold->unlimited);
}

void test_item_core_equip_not_stackable(void) {
    const game_item_def_t *sword = item_core("tmpl.sword");
    TEST_ASSERT_NOT_NULL(sword);
    TEST_ASSERT_NOT_NULL(sword->equip);
    TEST_ASSERT_EQUAL_STRING("weapon", sword->equip->slot);
    TEST_ASSERT_FALSE(sword->stackable);
    TEST_ASSERT_NULL(sword->currency);
}

void test_item_core_unknown_is_null(void) {
    TEST_ASSERT_NULL(item_core("bogus"));
    TEST_ASSERT_NULL(item_core(NULL));
}

void test_items_with_tag_currency(void) {
    const game_item_def_t *out[8];
    int n = items_with_tag("currency", out, 8);
    TEST_ASSERT_EQUAL_INT(3, n); /* gold + xp + energy (§6.1 demo catalog) */
    for (int i = 0; i < n; ++i) {
        TEST_ASSERT_TRUE(item_is_currency(out[i]));
    }
}

void test_item_container_def_purse(void) {
    const game_container_def_t *purse = item_container_def("purse");
    TEST_ASSERT_NOT_NULL(purse);
    TEST_ASSERT_EQUAL(ITEM_ACCEPT_CURRENCY_ONLY, purse->accept_policy);
    TEST_ASSERT_TRUE(purse->hidden);

    const game_container_def_t *backpack = item_container_def("backpack");
    TEST_ASSERT_NOT_NULL(backpack);
    TEST_ASSERT_EQUAL(ITEM_ACCEPT_ANY, backpack->accept_policy);
    TEST_ASSERT_FALSE(backpack->hidden);

    TEST_ASSERT_NULL(item_container_def("no_such_container"));
}

void test_items_def_count_and_at(void) {
    int count = items_def_count();
    TEST_ASSERT_EQUAL_INT(6, count); /* gold, xp, energy, potion, sword, wood */
    const game_item_def_t *first = item_at(0);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NULL(item_at(-1));
    TEST_ASSERT_NULL(item_at(count));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_item_core_known_currency);
    RUN_TEST(test_item_core_equip_not_stackable);
    RUN_TEST(test_item_core_unknown_is_null);
    RUN_TEST(test_items_with_tag_currency);
    RUN_TEST(test_item_container_def_purse);
    RUN_TEST(test_items_def_count_and_at);
    return UNITY_END();
}
