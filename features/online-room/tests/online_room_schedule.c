// The tick schedule: a schedule far behind runs its catch-up ticks, each at
// its own time, gives up the rest and is on time again.
#include "unity.h"

#include "online_room/schedule.h"

void setUp(void) {}
void tearDown(void) {}

static void test_the_schedule_caps_its_catch_up(void) {
    online_tick_schedule schedule;
    online_schedule_init(&schedule, 10.0, 30U);
    uint64_t first = 0U;
    TEST_ASSERT_EQUAL_UINT32(1U, online_schedule_take(&schedule, 10.0, 5U, &first));
    TEST_ASSERT_TRUE(first == 0U);
    TEST_ASSERT_EQUAL_UINT32(5U, online_schedule_take(&schedule, 11.0, 5U, &first));
    TEST_ASSERT_TRUE(first == 1U);
    TEST_ASSERT_TRUE(online_schedule_time(&schedule, first + 4U) < 11.0);
    TEST_ASSERT_TRUE(schedule.dropped > 0U && schedule.late > 0U);
    TEST_ASSERT_TRUE(online_schedule_time(&schedule, schedule.next) > 11.0);
    TEST_ASSERT_EQUAL_UINT32(0U, online_schedule_take(&schedule, 11.0, 5U, &first));
    TEST_ASSERT_TRUE(online_schedule_delay(&schedule, 11.0) > 0.0);
}

// Ticks on time are neither late nor dropped, and the delay is to the next one.
static void test_an_on_time_schedule_counts_nothing(void) {
    online_tick_schedule schedule;
    online_schedule_init(&schedule, 0.0, 20U);
    uint64_t first = 0U;
    for (uint32_t tick = 0U; tick < 40U; ++tick) {
        TEST_ASSERT_EQUAL_UINT32(1U, online_schedule_take(&schedule, (double)tick * 0.05 + 0.001, 5U, &first));
        TEST_ASSERT_TRUE(first == tick);
    }
    TEST_ASSERT_TRUE(schedule.late == 0U && schedule.dropped == 0U);
    const double delay = online_schedule_delay(&schedule, 39.0 * 0.05 + 0.001);
    TEST_ASSERT_TRUE(delay > 0.04 && delay < 0.05);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_the_schedule_caps_its_catch_up);
    RUN_TEST(test_an_on_time_schedule_counts_nothing);
    return UNITY_END();
}
