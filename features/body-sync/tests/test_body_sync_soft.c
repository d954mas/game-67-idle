#include <math.h>
#include <string.h>

#include "sync_soft.h"
#include "unity.h"

/* The second layer's pure part alone: the dead zone and its hysteresis, the pause, its scale and
   its cap, the snap, the pull and its limits, the stall, when an owner rests, the rest hold and
   who marks. No Box3D. */

#define NEAR(tolerance, want, got) TEST_ASSERT_TRUE(fabs((double)(want) - (double)(got)) <= (double)(tolerance))

void setUp(void) {}
void tearDown(void) {}

static sync_soft_t soft;
static sync_soft_in_t in;
static sync_play_t play;
static sync_soft_out_t out;

static void at(float x) {
    memset(&in, 0, sizeof(in));
    in.position[0] = x;
    in.position[1] = 0.2f;
    in.rotation[3] = 1.0f;
    in.max_speed = 100.0f;
    in.max_spin = 200.0f;
}

static void owner_at(float x, bool rest) {
    memset(&play, 0, sizeof(play));
    play.known = true;
    play.position[0] = x;
    play.position[1] = 0.2f;
    play.rotation[3] = 1.0f;
    play.at_rest = rest;
}

static void decide(bool hit) { sync_soft_decide(&soft, &in, hit, &play, &out); }

/* Inside the dead zone nothing is done, the edge included; past it the pull starts, and stops
   only under half of it. */
static void test_dead_zone_and_hysteresis(void) {
    sync_soft_enter(&soft, false, 0);
    soft.correcting = false;
    at(0.0f);
    owner_at(SYNC_SOFT_DEAD_M, false); /* on the edge: still inside */
    decide(false);
    TEST_ASSERT_EQUAL_MESSAGE(SYNC_SOFT_NONE, out.action, "the dead zone's edge was pulled");
    owner_at(1.5f * SYNC_SOFT_DEAD_M, false);
    decide(false);
    TEST_ASSERT_EQUAL(SYNC_SOFT_VELOCITY, out.action);
    owner_at(0.7f * SYNC_SOFT_DEAD_M, false); /* inside the zone, not yet under half: still pulled */
    decide(false);
    TEST_ASSERT_EQUAL_MESSAGE(SYNC_SOFT_VELOCITY, out.action, "the pull stopped at the dead zone's edge");
    owner_at(0.4f * SYNC_SOFT_DEAD_M, false);
    decide(false);
    TEST_ASSERT_EQUAL(SYNC_SOFT_NONE, out.action);
}

/* A hit or a moving local touch pauses the pull for the pause the shell passes (the floor when
   it passes 0); the snap overrides a pause and ends it. */
static void test_the_pause(void) {
    sync_soft_enter(&soft, false, 0);
    at(0.0f);
    owner_at(0.3f, false);
    decide(true);
    TEST_ASSERT_EQUAL(SYNC_SOFT_NONE, out.action);
    for (uint32_t i = 1; i < SYNC_SOFT_PAUSE_TICKS; i++) {
        decide(false);
        TEST_ASSERT_EQUAL_MESSAGE(SYNC_SOFT_NONE, out.action, "the pause ended early");
    }
    decide(false);
    TEST_ASSERT_EQUAL_MESSAGE(SYNC_SOFT_VELOCITY, out.action, "the pause never ended");

    sync_soft_enter(&soft, false, 0);
    in.touches_local = true;
    decide(false);
    in.touches_local = false;
    TEST_ASSERT_EQUAL_MESSAGE(SYNC_SOFT_NONE, out.action, "a moving local touch did not pause");

    sync_soft_enter(&soft, false, 0);
    in.pause_ticks = 40; /* a long round trip */
    decide(true);
    for (uint32_t i = 1; i < 40u; i++) {
        decide(false);
        TEST_ASSERT_EQUAL_MESSAGE(SYNC_SOFT_NONE, out.action, "the measured pause was cut to the floor");
    }
    decide(false);
    TEST_ASSERT_EQUAL(SYNC_SOFT_VELOCITY, out.action);
    in.pause_ticks = 0;

    sync_soft_enter(&soft, true, 0); /* from local: starts paused */
    decide(false);
    TEST_ASSERT_EQUAL(SYNC_SOFT_NONE, out.action);
    owner_at(SYNC_SOFT_SNAP_M + 0.1f, false);
    decide(false);
    TEST_ASSERT_EQUAL_MESSAGE(SYNC_SOFT_PLACE, out.action, "the snap waited for the pause");
    at(SYNC_SOFT_SNAP_M + 0.1f - 0.3f); /* placed, then pushed 0.3 m off again */
    decide(false);
    TEST_ASSERT_EQUAL_MESSAGE(SYNC_SOFT_VELOCITY, out.action, "the snap left the pause running");
}

/* A touch held for ever pauses one chain, then the pull runs under it until a tick without it. */
static void test_a_held_touch_lets_the_pull_run_after_a_chain(void) {
    sync_soft_enter(&soft, false, 0);
    at(0.0f);
    owner_at(0.3f, false);
    in.touches_local = true;
    uint32_t paused = 0, pulled_after = 0;
    for (uint32_t i = 0; i < 3u * SYNC_SOFT_PAUSE_MAX_TICKS; i++) {
        decide(false);
        if (i < SYNC_SOFT_PAUSE_MAX_TICKS) {
            paused += out.action == SYNC_SOFT_NONE ? 1u : 0u;
        } else {
            pulled_after += out.action != SYNC_SOFT_NONE ? 1u : 0u;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(SYNC_SOFT_PAUSE_MAX_TICKS, paused);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u * SYNC_SOFT_PAUSE_MAX_TICKS, pulled_after, "a held touch kept re-pausing the pull");
    in.touches_local = false;
    decide(false); /* a tick without it: the next touch pauses again */
    in.touches_local = true;
    decide(false);
    TEST_ASSERT_EQUAL(SYNC_SOFT_NONE, out.action);
}

/* The pull: the owner's velocity plus the error over SYNC_SOFT_PULL_S, limited and blended; the
   result never past max_speed, whatever the body moved with. */
static void test_the_pull_and_its_limits(void) {
    sync_soft_enter(&soft, false, 0);
    at(0.0f);
    owner_at(0.1f, false);
    play.velocity[0] = 1.0f;
    decide(false);
    NEAR(1e-5f, SYNC_SOFT_BLEND * (1.0f + 0.1f / SYNC_SOFT_PULL_S), out.velocity[0]);
    owner_at(0.9f, false);
    decide(false);
    NEAR(1e-5f, SYNC_SOFT_BLEND * SYNC_SOFT_PULL_SPEED, out.velocity[0]); /* the error term is clamped */
    in.max_speed = 10.0f;
    in.velocity[0] = 30.0f; /* three times the seam's speed, while correcting */
    owner_at(0.5f, false);
    decide(false);
    TEST_ASSERT_TRUE_MESSAGE(out.action == SYNC_SOFT_VELOCITY && out.velocity[0] <= 10.0f + 1e-4f, "a pulled copy outran max_speed");
    owner_at(0.0f, false);
    soft.correcting = false;
    decide(false);
    TEST_ASSERT_TRUE_MESSAGE(out.action == SYNC_SOFT_VELOCITY && out.velocity[0] <= 10.0f + 1e-4f, "a free copy outran max_speed");
}

/* A pull that does not end in SYNC_SOFT_STALL_TICKS places the body. */
static void test_a_stalled_pull_is_placed(void) {
    sync_soft_enter(&soft, false, 0);
    at(0.0f);
    owner_at(0.3f, false);
    bool placed = false;
    for (uint32_t i = 0; i <= SYNC_SOFT_STALL_TICKS + 1u && !placed; i++) {
        decide(false); /* the body never moves: pinned */
        placed = out.action == SYNC_SOFT_PLACE;
    }
    TEST_ASSERT_TRUE_MESSAGE(placed, "a pinned body was pulled for ever");
}

/* An owner rests asleep at once; awake once its pose has stayed within a grid step for
   SYNC_SOFT_STILL_TICKS, wandering inside the step or not; a move past it starts again. */
static void test_when_an_owner_rests(void) {
    sync_soft_still_t still;
    memset(&still, 0, sizeof(still));
    owner_at(1.0f, true);
    TEST_ASSERT_TRUE(sync_soft_owner_rests(&still, &play));
    memset(&still, 0, sizeof(still));
    owner_at(1.0f, false);
    for (uint32_t i = 0; i < SYNC_SOFT_STILL_TICKS; i++) {
        play.position[0] = 1.0f + (i % 2 ? 0.5f : 0.0f) * SYNC_SOFT_HOLD_M; /* the grid's wander */
        TEST_ASSERT_FALSE_MESSAGE(sync_soft_owner_rests(&still, &play), "an awake owner rested before it was still long enough");
    }
    TEST_ASSERT_TRUE(sync_soft_owner_rests(&still, &play));
    play.position[0] = 1.0f + 3.0f * SYNC_SOFT_HOLD_M;
    TEST_ASSERT_FALSE_MESSAGE(sync_soft_owner_rests(&still, &play), "a move past the step did not restart the count");
    for (uint32_t i = 0; i <= SYNC_SOFT_STILL_TICKS; i++) {
        sync_soft_owner_rests(&still, &play);
    }
    const float s = sinf(1.5f * SYNC_SOFT_HOLD_RAD), c = cosf(1.5f * SYNC_SOFT_HOLD_RAD);
    play.rotation[1] = s; /* a turn of three times the step, about y */
    play.rotation[3] = c;
    TEST_ASSERT_FALSE_MESSAGE(sync_soft_owner_rests(&still, &play), "a turn did not restart the count");
    play.known = false;
    play.at_rest = true;
    TEST_ASSERT_FALSE_MESSAGE(sync_soft_owner_rests(&still, &play), "an unknown owner rested");
}

/* The rest hold: a resting owner, no pause, nothing of this side's coming. */
static void test_the_rest_hold(void) {
    sync_soft_enter(&soft, false, 0);
    TEST_ASSERT_TRUE(sync_soft_rest_hold(true, &soft, true, false));
    TEST_ASSERT_FALSE(sync_soft_rest_hold(false, &soft, true, false));
    TEST_ASSERT_FALSE_MESSAGE(sync_soft_rest_hold(true, &soft, true, true), "a copy something comes at was held kinematic");
    soft.pause = 5;
    TEST_ASSERT_FALSE_MESSAGE(sync_soft_rest_hold(true, &soft, true, false), "a paused soft copy was held kinematic");
    TEST_ASSERT_TRUE_MESSAGE(sync_soft_rest_hold(true, &soft, false, false), "a stale pause of a driven copy counted");
}

/* Who marks: a point of a body moving faster than a lean, its spin counted at its extent, out to
   its travel in a few steps plus a margin. */
static void test_who_marks_and_how_far(void) {
    float reach = 0.0f;
    const float lean[3] = {0.1f, 0.0f, 0.0f}, walk[3] = {0.0f, 0.0f, 2.4f}, none[3] = {0, 0, 0}, turn[3] = {0.0f, 3.0f, 0.0f};
    TEST_ASSERT_FALSE(sync_soft_mover(lean, none, 0.3f, 1.0f / 60.0f, &reach));
    TEST_ASSERT_TRUE(sync_soft_mover(walk, none, 0.3f, 1.0f / 60.0f, &reach));
    NEAR(1e-5f, 2.4f / 60.0f * SYNC_SOFT_REACH_STEPS + SYNC_SOFT_REACH_M, reach);
    TEST_ASSERT_TRUE_MESSAGE(sync_soft_mover(none, turn, 0.3f, 1.0f / 60.0f, &reach), "a body turning in place marked nothing");
    NEAR(1e-5f, 0.9f / 60.0f * SYNC_SOFT_REACH_STEPS + SYNC_SOFT_REACH_M, reach);
    TEST_ASSERT_FALSE(sync_soft_mover(none, turn, 0.05f, 1.0f / 60.0f, &reach)); /* a thin one turning is a lean */
}

/* A push is a contact impulse past stopping the pusher from a lean's speed. */
static void test_what_a_push_is(void) {
    TEST_ASSERT_FALSE(sync_soft_pushed(0.0f, 60.0f));
    TEST_ASSERT_FALSE(sync_soft_pushed(0.9f * 60.0f * SYNC_SOFT_LEAN_SPEED, 60.0f));
    TEST_ASSERT_TRUE_MESSAGE(sync_soft_pushed(60.0f * 0.2f, 60.0f), "a walk at 0.2 m/s stopped by a copy was no push");
}

/* The pause: the measured round trip, the delay and 2 ticks, inside the floor and the cap. */
static void test_the_pause_length(void) {
    TEST_ASSERT_EQUAL_UINT16(SYNC_SOFT_PAUSE_TICKS, sync_soft_pause(0.0f, 0));
    TEST_ASSERT_EQUAL_UINT16(SYNC_SOFT_PAUSE_TICKS, sync_soft_pause(6.0f, 5)); /* 100 ms: under the floor */
    TEST_ASSERT_EQUAL_UINT16(24u + 6u + 2u, sync_soft_pause(23.2f, 6));       /* 400 ms */
    TEST_ASSERT_EQUAL_UINT16(SYNC_SOFT_PAUSE_MAX_TICKS, sync_soft_pause(90.0f, 15));
    TEST_ASSERT_EQUAL_UINT16(SYNC_SOFT_PAUSE_TICKS, sync_soft_pause(-3.0f, 0));
}

/* A hit is the step's velocity change beyond what the apply and gravity gave the body. */
static void test_what_a_hit_is(void) {
    sync_soft_enter(&soft, false, 0);
    at(0.0f);
    owner_at(0.0f, false);
    in.velocity[0] = 0.5f;
    decide(false); /* the apply leaves it at 0.5 m/s */
    const float gravity[3] = {0.0f, -0.2f, 0.0f};
    float v[3] = {0.5f, -0.2f, 0.0f}, w[3] = {0, 0, 0};
    TEST_ASSERT_FALSE(sync_soft_hit(&soft, v, w, gravity));
    v[0] = 0.5f + SYNC_SOFT_HIT_SPEED * 1.1f;
    TEST_ASSERT_TRUE(sync_soft_hit(&soft, v, w, gravity));
    v[0] = 0.5f;
    w[1] = SYNC_SOFT_HIT_SPIN * 1.1f;
    TEST_ASSERT_TRUE(sync_soft_hit(&soft, v, w, gravity));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dead_zone_and_hysteresis);
    RUN_TEST(test_the_pause);
    RUN_TEST(test_a_held_touch_lets_the_pull_run_after_a_chain);
    RUN_TEST(test_the_pull_and_its_limits);
    RUN_TEST(test_a_stalled_pull_is_placed);
    RUN_TEST(test_when_an_owner_rests);
    RUN_TEST(test_the_rest_hold);
    RUN_TEST(test_who_marks_and_how_far);
    RUN_TEST(test_what_a_push_is);
    RUN_TEST(test_the_pause_length);
    RUN_TEST(test_what_a_hit_is);
    return UNITY_END();
}
