#include <math.h>
#include <string.h>

#include "features/body_sync/body_sync_types.h"
#include "sync_policy.h"
#include "sync_replay.h"
#include "sync_soft.h"
#include "unity.h"

/* The pure parts of a replayed throw: the host's replay (its start, its shift, the check of
   later flight records, landing), rounding to the wire at a release, the policy that
   hands a flight back, and the guest's event-time comparison (its history and the ledger of its
   own corrections). No Box3D. */

void setUp(void) {}
void tearDown(void) {}

static sync_replay_t replay;

static void flown(uint32_t from, uint32_t to, float x0) {
    for (uint32_t t = from; t <= to; t++) {
        sync_replay_stepped(&replay, t, (const float[3]){x0 + 0.2f * (float)(t - from), 1.0f, 0.0f}, false);
    }
}

/* The first flight state starts it once; the shift maps the owner's steps on this side's. */
static void test_the_first_flight_state_starts_the_replay(void) {
    memset(&replay, 0, sizeof(replay));
    float p[3] = {0.0f, 1.0f, 0.0f};
    TEST_ASSERT_EQUAL(SYNC_REPLAY_OFF, sync_replay_decide(&replay, 100, false, 40, p));
    TEST_ASSERT_EQUAL(SYNC_REPLAY_START, sync_replay_decide(&replay, 103, true, 40, p));
    TEST_ASSERT_EQUAL_INT32(62, replay.shift); /* this side's 103 replays the owner's 41 */
    TEST_ASSERT_EQUAL(SYNC_REPLAY_FREE, sync_replay_decide(&replay, 104, true, 40, p)); /* a duplicate */
    TEST_ASSERT_EQUAL(SYNC_REPLAY_FREE, sync_replay_decide(&replay, 104, true, 39, p)); /* reordered */
    TEST_ASSERT_EQUAL(SYNC_REPLAY_FREE, sync_replay_decide(&replay, 104, false, 41, p)); /* not a flight state */
}

/* A later record on the replayed path leaves it alone; one far off restarts it from that record;
   after a landing here nothing is checked. */
static void test_later_records_only_check_the_replay(void) {
    memset(&replay, 0, sizeof(replay));
    float p[3] = {0.0f, 1.0f, 0.0f};
    sync_replay_decide(&replay, 103, true, 40, p);
    flown(103, 110, 0.2f); /* after step 103 the body is where the owner was after step 41 */
    float on_path[3] = {0.2f + 0.2f * 3.0f + 0.01f, 1.0f, 0.0f}; /* the owner after step 44 */
    TEST_ASSERT_EQUAL(SYNC_REPLAY_FREE, sync_replay_decide(&replay, 111, true, 44, on_path));
    float faster[3] = {9.0f, 1.0f, 0.0f}; /* a record not stepped here yet is not judged */
    TEST_ASSERT_EQUAL(SYNC_REPLAY_FREE, sync_replay_decide(&replay, 111, true, 49, faster));
    memset(&replay, 0, sizeof(replay));
    sync_replay_decide(&replay, 103, true, 40, p);
    flown(103, 108, 0.2f);
    float off2[3] = {0.2f + 0.2f * 2.0f + SYNC_REPLAY_MISMATCH_M + 0.05f, 1.0f, 0.0f}; /* the owner after step 43 */
    TEST_ASSERT_EQUAL_MESSAGE(SYNC_REPLAY_START, sync_replay_decide(&replay, 109, true, 43, off2), "a gross mismatch was not restarted");
    TEST_ASSERT_EQUAL_UINT32(1, replay.restarts);
    TEST_ASSERT_EQUAL_INT32(109 - 43 - 1, replay.shift);
    sync_replay_stepped(&replay, 109, off2, true);
    TEST_ASSERT_EQUAL_MESSAGE(SYNC_REPLAY_FREE, sync_replay_decide(&replay, 110, true, 60, p), "a landed replay was still checked");
    sync_replay_stop(&replay);
    TEST_ASSERT_FALSE(replay.active);
}

/* A release rounds once; nothing in the flight after it, nothing at a sleep. */
static void test_rounding_happens_at_a_release_only(void) {
    sync_round_t r;
    memset(&r, 0, sizeof(r));
    TEST_ASSERT_FALSE(sync_round_due(&r, false));
    TEST_ASSERT_TRUE_MESSAGE(sync_round_due(&r, true), "the release was not rounded");
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_FALSE_MESSAGE(sync_round_due(&r, true), "a flight was rounded every tick");
    }
    TEST_ASSERT_FALSE(sync_round_due(&r, false));
    TEST_ASSERT_TRUE_MESSAGE(sync_round_due(&r, true), "a second throw was not rounded");
}

static void test_the_policy_returns_a_flight_at_its_first_hit(void) {
    sync_policy_in_t in = {.hold = BODY_SYNC_HOLD_UNTIL_SETTLED};
    TEST_ASSERT_FALSE(sync_policy_returns(&in));
    in.touched_body = true;
    TEST_ASSERT_TRUE(sync_policy_returns(&in));
    in.touched_body = false;
    in.asleep = true;
    TEST_ASSERT_TRUE(sync_policy_returns(&in));
    in.hold = BODY_SYNC_HOLD_UNTIL_RELEASE;
    in.touched_body = true;
    TEST_ASSERT_FALSE_MESSAGE(sync_policy_returns(&in), "a carried body was handed back");
}

static sync_history_t history;

static void moving(uint32_t from, uint32_t to, float speed) {
    memset(&history, 0, sizeof(history));
    for (uint32_t t = from; t <= to; t++) {
        float x = speed * (float)(t - from) / 60.0f;
        sync_history_put(&history, t, (const float[3]){x, 0.5f, 0.0f}, (const float[4]){0, 0, 0, 1}, (const float[3]){speed, 0, 0}, (const float[3]){0, 0, 0});
    }
}

/* Between kept ticks the state is interpolated; a tick not kept is unknown. */
static void test_the_history_between_ticks(void) {
    moving(1000, 1040, 6.0f);
    float p[3], q[4], v[3], w[3];
    TEST_ASSERT_TRUE(sync_history_at(&history, 1010.5, p, q, v, w));
    TEST_ASSERT_TRUE(fabsf(p[0] - 6.0f * 10.5f / 60.0f) < 1e-4f);
    TEST_ASSERT_FALSE(sync_history_at(&history, 1040.5, p, q, v, w));
    TEST_ASSERT_FALSE(sync_history_at(&history, 900.0, p, q, v, w));
}

/* A correction made after the compared state is taken off the error, once: the same error found
   again before the correction reached the past is not applied twice. */
static void test_a_correction_is_not_applied_twice(void) {
    moving(1000, 1040, 0.0f);
    float err[3] = {0.3f, 0.0f, 0.0f}, turn[3] = {0}, dv[3] = {1.2f, 0.0f, 0.0f}, dw[3] = {0};
    sync_history_correct(&history, 1036, (const float[3]){0.1f, 0, 0}, (const float[3]){1.2f, 0, 0}, (const float[3]){0, 0, 0});
    sync_history_since(&history, 1030.0, 1040, 1.0f / 60.0f, err, turn, dv, dw);
    TEST_ASSERT_TRUE(fabsf(dv[0]) < 1e-5f);
    TEST_ASSERT_TRUE(fabsf(err[0] - (0.3f - 0.1f - 1.2f * 4.0f / 60.0f)) < 1e-5f);
    float err2[3] = {0.3f, 0, 0}, dv2[3] = {1.2f, 0, 0};
    sync_history_since(&history, 1037.0, 1040, 1.0f / 60.0f, err2, turn, dv2, dw); /* the past already shows it */
    TEST_ASSERT_TRUE(fabsf(err2[0] - 0.3f) < 1e-6f && fabsf(dv2[0] - 1.2f) < 1e-6f);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_the_first_flight_state_starts_the_replay);
    RUN_TEST(test_later_records_only_check_the_replay);
    RUN_TEST(test_rounding_happens_at_a_release_only);
    RUN_TEST(test_the_policy_returns_a_flight_at_its_first_hit);
    RUN_TEST(test_the_history_between_ticks);
    RUN_TEST(test_a_correction_is_not_applied_twice);
    return UNITY_END();
}
