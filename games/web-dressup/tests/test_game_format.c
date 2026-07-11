#include "unity.h"

#include "game_format.h"

#include <stdint.h>
#include <string.h>

/* L0 int64-abbrev formatter. Границы: <1000
   точно, K/M/B/T/Qa/Qi tiers, INT64_MAX/INT64_MIN (без UB, #13), отрицательные. */

void setUp(void) {}
void tearDown(void) {}

static void check(int64_t v, const char *expected) {
    char buf[32];
    char *out = game_format_i64_abbrev(v, buf, sizeof buf);
    TEST_ASSERT_EQUAL_PTR(buf, out);
    TEST_ASSERT_EQUAL_STRING(expected, buf);
}

void test_small_values_exact(void) {
    check(0, "0");
    check(1, "1");
    check(999, "999");
    check(-1, "-1");
    check(-999, "-999");
}

void test_kilo_tier(void) {
    check(1000, "1.0K");
    check(1500, "1.5K");
    check(-2500, "-2.5K");
}

void test_higher_tiers(void) {
    check(1500000, "1.5M");
    check(1000000000LL, "1.0B");
    check(1000000000000LL, "1.0T");
    check(1000000000000000LL, "1.0Qa");
    check(1000000000000000000LL, "1.0Qi");
}

/* #13 (critical): INT64_MIN via -v is UB (no positive int64 counterpart).
   Must format cleanly, never UB/crash. */
void test_int64_extremes_no_ub(void) {
    check(INT64_MAX, "9.2Qi");
    check(INT64_MIN, "-9.2Qi");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_small_values_exact);
    RUN_TEST(test_kilo_tier);
    RUN_TEST(test_higher_tiers);
    RUN_TEST(test_int64_extremes_no_ub);
    return UNITY_END();
}
