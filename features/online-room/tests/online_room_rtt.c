// The round trip from the moves the room takes: a move's age when the room
// says it took it, less the time the room held it; smoothed; nothing until a
// sample, and nothing from a move it never saw sent, one too old, or one the
// room cannot say it held.
#include "unity.h"

#include "online_room/rtt.h"

void setUp(void) {}
void tearDown(void) {}

static bool near(float value, float want) { return value > want - 0.5F && value < want + 0.5F; }

static void test_nothing_until_the_room_takes_a_sent_move(void) {
    online_rtt rtt;
    online_rtt_reset(&rtt);
    TEST_ASSERT_TRUE(online_rtt_ms(&rtt) < 0.0F);
    online_rtt_sent(&rtt, 5U, 1.0);
    // A move this client never recorded is no sample.
    online_rtt_acked(&rtt, 4U, 1.2, 0.0F);
    TEST_ASSERT_TRUE(online_rtt_ms(&rtt) < 0.0F);
}

static void test_the_age_less_the_hold_smoothed(void) {
    online_rtt rtt;
    online_rtt_reset(&rtt);
    online_rtt_sent(&rtt, 1U, 10.0);
    online_rtt_sent(&rtt, 2U, 10.1);
    online_rtt_acked(&rtt, 1U, 10.15, 0.05F);
    TEST_ASSERT_TRUE(near(online_rtt_ms(&rtt), 100.0F));
    // The same ack again is no new sample.
    online_rtt_acked(&rtt, 1U, 11.0, 0.05F);
    TEST_ASSERT_TRUE(near(online_rtt_ms(&rtt), 100.0F));
    // A slower trip moves the reading part of the way.
    online_rtt_acked(&rtt, 2U, 10.45, 0.05F);
    const float ms = online_rtt_ms(&rtt);
    TEST_ASSERT_TRUE(ms > 100.0F && ms < 300.0F);
}

static void test_a_hold_the_room_cannot_name_is_no_sample(void) {
    online_rtt rtt;
    online_rtt_reset(&rtt);
    online_rtt_sent(&rtt, 1U, 10.0);
    online_rtt_sent(&rtt, 2U, 10.0);
    online_rtt_acked(&rtt, 1U, 10.3, -1.0F);
    TEST_ASSERT_TRUE(online_rtt_ms(&rtt) < 0.0F);
    online_rtt_acked(&rtt, 2U, 10.05, 0.045F);
    TEST_ASSERT_TRUE(near(online_rtt_ms(&rtt), 5.0F));
}

static void test_a_hold_longer_than_the_age_is_no_sample(void) {
    online_rtt rtt;
    online_rtt_reset(&rtt);
    online_rtt_sent(&rtt, 1U, 10.0);
    online_rtt_sent(&rtt, 2U, 10.0);
    online_rtt_acked(&rtt, 1U, 10.05, 0.045F);
    online_rtt_acked(&rtt, 2U, 10.1, 0.5F);
    TEST_ASSERT_TRUE(near(online_rtt_ms(&rtt), 5.0F));
}

static void test_only_a_newer_ack_is_a_sample(void) {
    online_rtt rtt;
    online_rtt_reset(&rtt);
    online_rtt_sent(&rtt, 1U, 10.0);
    online_rtt_sent(&rtt, 2U, 10.0);
    online_rtt_acked(&rtt, 2U, 10.1, 0.0F);
    TEST_ASSERT_TRUE(near(online_rtt_ms(&rtt), 100.0F));
    // An older ack arriving late would read 900 ms; it is no sample.
    online_rtt_acked(&rtt, 1U, 10.9, 0.0F);
    TEST_ASSERT_TRUE(near(online_rtt_ms(&rtt), 100.0F));
}

static void test_a_stale_move_is_no_sample(void) {
    online_rtt rtt;
    online_rtt_reset(&rtt);
    online_rtt_sent(&rtt, 1U, 0.0);
    online_rtt_acked(&rtt, 1U, 30.0, 0.0F);
    TEST_ASSERT_TRUE(online_rtt_ms(&rtt) < 0.0F);
    // Many more moves than the record holds: the oldest are forgotten.
    for (uint32_t seq = 2U; seq < 2U + (ONLINE_RTT_SENT * 2U); ++seq) { online_rtt_sent(&rtt, seq, 40.0); }
    online_rtt_acked(&rtt, 2U, 40.1, 0.0F);
    TEST_ASSERT_TRUE(online_rtt_ms(&rtt) < 0.0F);
    online_rtt_acked(&rtt, 1U + (ONLINE_RTT_SENT * 2U), 40.1, 0.0F);
    TEST_ASSERT_TRUE(near(online_rtt_ms(&rtt), 100.0F));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_nothing_until_the_room_takes_a_sent_move);
    RUN_TEST(test_the_age_less_the_hold_smoothed);
    RUN_TEST(test_a_hold_the_room_cannot_name_is_no_sample);
    RUN_TEST(test_a_hold_longer_than_the_age_is_no_sample);
    RUN_TEST(test_only_a_newer_ack_is_a_sample);
    RUN_TEST(test_a_stale_move_is_no_sample);
    return UNITY_END();
}
