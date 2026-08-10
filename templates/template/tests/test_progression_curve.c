#include "unity.h"

#include "progression_tracks.gen.h" /* the REAL demo catalog, not the test catalog */

#include <string.h>

/* GOLDEN. Links the generated demo tables, so a floor-vs-round regression or a
   struct-layout mismatch in generate_progression_tracks.py fails against the
   actually compiled output rather than a hand-copied expectation. Generation
   auto-triggers through the add_custom_command OUTPUT this target links. */

void setUp(void) {}
void tearDown(void) {}

void test_demo_catalog_shape(void) {
    TEST_ASSERT_EQUAL_INT(1, k_tracks_count);
    TEST_ASSERT_EQUAL_STRING("hero", k_tracks[0].id);
    TEST_ASSERT_EQUAL(PROGRESSION_MODE_AUTO, k_tracks[0].mode);
    /* Twenty reachable levels come from twenty-one authored rows: row 1 is the
       un-upgraded state and is the one level nothing has to reach. */
    TEST_ASSERT_EQUAL_INT(20, k_tracks[0].max_level);
    TEST_ASSERT_EQUAL_INT(1, k_tracks[0].steps[0].cost_count);
    TEST_ASSERT_EQUAL_STRING("tmpl.xp", k_tracks[0].steps[0].cost[0].def_id);
    TEST_ASSERT_EQUAL_INT64(0, k_tracks[0].steps[0].xp_cost); /* an item price, not an xp one */
    TEST_ASSERT_NULL(k_tracks[0].steps[0].grants);
    TEST_ASSERT_EQUAL_INT(0, k_tracks[0].value_count); /* the demo track carries no columns */
}

/* FLOOR, not ROUND: 50 * (3/2)^L -> 50, 75, 112.5->112, 168.75->168. */
void test_demo_curve_baked_values_floor(void) {
    TEST_ASSERT_EQUAL_INT64(50, k_tracks[0].steps[0].cost[0].amount);
    TEST_ASSERT_EQUAL_INT64(75, k_tracks[0].steps[1].cost[0].amount);
    TEST_ASSERT_EQUAL_INT64(112, k_tracks[0].steps[2].cost[0].amount);
    TEST_ASSERT_EQUAL_INT64(168, k_tracks[0].steps[3].cost[0].amount);
    TEST_ASSERT_EQUAL_INT64(110841, k_tracks[0].steps[19].cost[0].amount);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_demo_catalog_shape);
    RUN_TEST(test_demo_curve_baked_values_floor);
    return UNITY_END();
}
