#include "unity.h"

#include "progression_tracks.gen.h" /* demo catalog (content/progression.json), NOT the test catalog */

#include <string.h>

/* GOLDEN. Links the REAL demo
   progression_tracks.gen.c -- catches a FLOOR-vs-ROUND regression or a struct-
   layout mismatch in features/progression-core/scripts/generate_progression_tracks.py against the actual
   compiled output, not a hand-computed expectation. Generation auto-triggers
   via the add_custom_command OUTPUT this target links. */

void setUp(void) {}
void tearDown(void) {}

void test_demo_catalog_shape(void) {
    TEST_ASSERT_EQUAL_INT(1, k_tracks_count);
    TEST_ASSERT_EQUAL_STRING("hero", k_tracks[0].id);
    TEST_ASSERT_EQUAL(PROGRESSION_MODE_AUTO, k_tracks[0].mode);
    TEST_ASSERT_EQUAL_STRING("tmpl.xp", k_tracks[0].currency_def);
    TEST_ASSERT_EQUAL_INT(20, k_tracks[0].max_level);
    TEST_ASSERT_EQUAL_INT(20, k_tracks[0].cost_count);
    TEST_ASSERT_NULL(k_tracks[0].on_level_up); /* LEAN-порез A: codegen never bakes on_level_up */
    TEST_ASSERT_EQUAL_INT(0, k_tracks[0].on_level_up_count);
}

/* FLOOR, not ROUND: base=50, growth=3/2 -> 50, 75, 112.5->112, 168.75->168. */
void test_demo_curve_baked_values_floor(void) {
    TEST_ASSERT_EQUAL_INT64(50, k_tracks[0].cost[0]);
    TEST_ASSERT_EQUAL_INT64(75, k_tracks[0].cost[1]);
    TEST_ASSERT_EQUAL_INT64(112, k_tracks[0].cost[2]);
    TEST_ASSERT_EQUAL_INT64(168, k_tracks[0].cost[3]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_demo_catalog_shape);
    RUN_TEST(test_demo_curve_baked_values_floor);
    return UNITY_END();
}
