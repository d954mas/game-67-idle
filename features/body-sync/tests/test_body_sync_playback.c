#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sync_playback.h"
#include "unity.h"

/* The playback receiver alone: the clock under loss, duplicates, reordering and silence, the
   buffer's interpolation and rare groups, the drive limits and the render offset. No Box3D. */

/* Unity's float asserts are compiled out in this build: plain comparisons instead. */
#define NEAR(tolerance, want, got) TEST_ASSERT_TRUE(fabs((double)(want) - (double)(got)) <= (double)(tolerance))
#define SAME(want, got) TEST_ASSERT_TRUE((double)(want) == (double)(got))

void setUp(void) {}
void tearDown(void) {}

static sync_playback_t pb;
static uint64_t rng = 0x2545F4914F6CDD1Dull;
static uint32_t next(void) {
    rng = rng * 6364136223846793005ull + 1442695040888963407ull;
    return (uint32_t)(rng >> 33);
}

/* A body moving along x at 3 m/s: 0.05 m a tick, so a sample tick names its position. */
static sync_record_t moving(uint16_t slot, uint32_t sample) {
    sync_record_t r;
    memset(&r, 0, sizeof(r));
    r.slot = slot;
    r.sim = true;
    r.position[0] = 0.05f * (float)sample;
    r.position[1] = 1.0f;
    r.rotation[3] = 1.0f;
    r.velocity[0] = 3.0f;
    return r;
}

/* Playback time in the owner's ticks: the timeline counts it from its base. */
static double abs_playback(const sync_timeline_t *t) { return (double)t->base + t->playback; }

static void deliver(uint8_t timeline, uint32_t sample, uint32_t local, uint16_t slot) {
    sync_playback_arrival(&pb, timeline, sample, local);
    sync_record_t r = moving(slot, sample);
    sync_playback_file(&pb, timeline, sample, &r);
}

typedef struct flight {
    uint32_t at, sample;
} flight_t;

/* Over loss, duplicates and reordering, playback never goes back, never passes the newest
   sample, and moves at most rate x SYNC_PACE_FAST a tick except at a jump it reports; the copy
   it gives sits on the owner's path. */
static void test_playback_time_is_monotone_and_bounded(void) {
    sync_playback_init(&pb, 60, 0, 0);
    static flight_t queue[8192];
    uint32_t queued = 0;
    double last = -1e300;
    uint32_t jumps = 0;
    for (uint32_t local = 0; local < 3000; local++) {
        uint32_t sample = 5000u + local;
        bool silent = local >= 1500 && local < 1530; /* a gap the pace catches up after */
        if (!silent && next() % 100 >= 10) {
            uint32_t delay = 3u + next() % 7u;
            queue[queued++ % 8192] = (flight_t){local + delay, sample};
            if (next() % 100 < 5) {
                queue[queued++ % 8192] = (flight_t){local + delay + 1u + next() % 5u, sample}; /* a duplicate */
            }
        }
        for (uint32_t i = 0; i < queued && i < 8192; i++) {
            if (queue[i].at == local) {
                deliver(0, queue[i].sample, local, 7);
            }
        }
        sync_playback_advance(&pb, local);
        const sync_timeline_t *t = &pb.timelines[0];
        if (!t->started) {
            continue;
        }
        bool jumped = t->jumped_at == local + 1u;
        jumps += jumped ? 1u : 0u;
        TEST_ASSERT_TRUE_MESSAGE(abs_playback(t) >= last, "playback stepped back");
        TEST_ASSERT_TRUE_MESSAGE(abs_playback(t) <= (double)t->newest, "playback passed the newest sample");
        if (last > -1e299 && !jumped) {
            TEST_ASSERT_TRUE_MESSAGE(abs_playback(t) - last <= (double)t->rate * SYNC_PACE_FAST + 1e-6, "playback outran its pace");
        }
        last = abs_playback(t);
        sync_play_t play;
        sync_playback_sample(&pb, 7, &play);
        if (play.known && !play.frozen && abs_playback(t) >= 5000.0) {
            NEAR(2e-3f, 0.05f * (float)abs_playback(t), play.position[0]);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0, jumps); /* a 30-tick gap is caught up by pace, not jumped */
    TEST_ASSERT_TRUE(pb.timelines[0].delay >= SYNC_DELAY_MIN && pb.timelines[0].delay <= SYNC_DELAY_MAX);
}

/* A duplicate says nothing about the path's jitter: the delay stays at its floor. */
static void test_a_duplicate_is_not_jitter(void) {
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t local = 0; local < 300; local++) {
        deliver(0, 100u + local, local, 1);
        if (local >= 10) {
            sync_playback_arrival(&pb, 0, 100u + local - 8u, local); /* seen eight ticks ago */
        }
        sync_playback_advance(&pb, local);
    }
    TEST_ASSERT_EQUAL_UINT8(SYNC_DELAY_MIN, pb.timelines[0].delay);
    /* The same late samples unseen are jitter: the delay grows. */
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t local = 0; local < 300; local++) {
        if (local % 2 == 0) {
            deliver(0, 100u + local, local, 1);
        } else if (local >= 9) {
            deliver(0, 100u + local - 8u, local, 1);
        }
        sync_playback_advance(&pb, local);
    }
    TEST_ASSERT_TRUE(pb.timelines[0].delay > SYNC_DELAY_MIN + 4);
}

/* Before the clock has its samples the delay is the longest; it then settles to what the path
   needs. A seat that rejoins starts a new timeline, from its new ticks. */
static void test_the_clock_before_it_knows_and_after_a_rejoin(void) {
    sync_playback_init(&pb, 60, 0, 0);
    deliver(0, 1000, 0, 3);
    sync_playback_advance(&pb, 0);
    const sync_timeline_t *t = &pb.timelines[0];
    TEST_ASSERT_EQUAL_UINT8(SYNC_DELAY_MAX, t->delay);
    SAME(1000.0 - (double)SYNC_DELAY_MAX, abs_playback(t));
    sync_play_t play;
    sync_playback_sample(&pb, 3, &play);
    TEST_ASSERT_TRUE(play.known); /* before its first snapshot a body holds that pose */
    SAME(50.0f, play.position[0]);
    for (uint32_t local = 1; local < SYNC_CLOCK_SETTLE - 1; local++) {
        deliver(0, 1000u + local, local, 3);
        sync_playback_advance(&pb, local);
        TEST_ASSERT_EQUAL_UINT8(SYNC_DELAY_MAX, t->delay);
    }
    for (uint32_t local = SYNC_CLOCK_SETTLE - 1; local < 200; local++) {
        deliver(0, 1000u + local, local, 3);
        sync_playback_advance(&pb, local);
    }
    TEST_ASSERT_EQUAL_UINT8(SYNC_DELAY_MIN, t->delay);
    NEAR(0.5, 1199.0 - (double)SYNC_DELAY_MIN, abs_playback(t));
    /* The seat leaves and joins again: its ticks start over, far below the old ones. */
    sync_playback_forget_seat(&pb, 0, false);
    TEST_ASSERT_FALSE(pb.bodies[3].has);
    for (uint32_t local = 200; local < 400; local++) {
        deliver(0, 10u + (local - 200u), local, 3);
        sync_playback_advance(&pb, local);
    }
    NEAR(0.5, 209.0 - (double)SYNC_DELAY_MIN, abs_playback(t));
    sync_playback_sample(&pb, 3, &play);
    TEST_ASSERT_TRUE(play.known && !play.frozen);
    NEAR(2e-3f, 0.05f * (float)abs_playback(t), play.position[0]);
}

/* A body its owner stops sending freezes at its newest snapshot while its timeline plays on;
   a silent timeline waits at its newest sample, and when packets come again far ahead it
   jumps once and says so. */
static void test_freeze_and_jump(void) {
    sync_playback_init(&pb, 60, 0, 0);
    uint32_t local = 0;
    for (; local < 100; local++) {
        deliver(0, local, local, 2);
        deliver(0, local, local, 9);
        sync_playback_advance(&pb, local);
    }
    uint32_t last = local - 1u;
    sync_play_t play;
    for (; local < 130; local++) {
        deliver(0, local, local, 9);
        sync_playback_advance(&pb, local);
        sync_playback_sample(&pb, 2, &play);
        TEST_ASSERT_EQUAL(abs_playback(&pb.timelines[0]) > (double)last + (double)SYNC_FREEZE_TICKS, play.frozen);
    }
    TEST_ASSERT_TRUE(play.frozen);
    SAME(0.05f * (float)last, play.position[0]);
    uint32_t newest = local - 1u;
    for (; local < 200; local++) {
        sync_playback_advance(&pb, local);
        TEST_ASSERT_TRUE(abs_playback(&pb.timelines[0]) <= (double)newest);
    }
    TEST_ASSERT_EQUAL_UINT32(0, pb.timelines[0].jumps); /* waiting is no jump */
    for (; local < 220; local++) {
        deliver(0, local, local, 2);
        sync_playback_advance(&pb, local);
        if (local == 200) {
            sync_playback_sample(&pb, 2, &play);
            TEST_ASSERT_TRUE(play.jumped);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1, pb.timelines[0].jumps);
    sync_playback_sample(&pb, 2, &play);
    TEST_ASSERT_FALSE(play.frozen || play.jumped);
}

static void set_playback(double t) {
    pb.timelines[0].started = true; /* not active: advance plays bodies without moving the clock */
    pb.timelines[0].playback = t;
}

static void file_at(uint16_t slot, uint32_t tick, const sync_record_t *r) {
    TEST_ASSERT_EQUAL_UINT16(slot, r->slot);
    sync_playback_file(&pb, 0, tick, r);
}

/* Sim snapshots by cubic Hermite (a ballistic arc is a cubic: met exactly), copy snapshots by
   a straight line. */
static void test_hermite_for_sim_linear_for_copy(void) {
    sync_playback_init(&pb, 60, 0, 0);
    const float g = -12.0f, v0 = 6.0f, dt = 1.0f / 60.0f;
    for (uint32_t k = 10; k <= 14; k += 4) {
        float tau = (float)k * dt;
        sync_record_t r = moving(1, 0);
        r.position[0] = 0.0f;
        r.position[1] = v0 * tau + 0.5f * g * tau * tau;
        r.velocity[0] = 0.0f;
        r.velocity[1] = v0 + g * tau;
        file_at(1, k, &r);
        r.slot = 2;
        r.sim = false;
        r.velocity[1] = 0.0f; /* a copy record carries none */
        file_at(2, k, &r);
    }
    set_playback(12.0);
    sync_play_t sim, copy;
    sync_playback_sample(&pb, 1, &sim);
    sync_playback_sample(&pb, 2, &copy);
    float tau = 12.0f * dt, arc = v0 * tau + 0.5f * g * tau * tau;
    NEAR(1e-5f, arc, sim.position[1]);
    float y10 = v0 * 10.0f * dt + 0.5f * g * 100.0f * dt * dt, y14 = v0 * 14.0f * dt + 0.5f * g * 196.0f * dt * dt;
    NEAR(1e-6f, 0.5f * (y10 + y14), copy.position[1]);
    TEST_ASSERT_TRUE(fabsf(copy.position[1] - arc) > 1e-4f);
    /* Copy records carry no velocity: the line between the two gives it. */
    NEAR(1e-3f, (y14 - y10) / (4.0f * dt), copy.velocity[1]);
}

/* A dynamic hold of a still owner leaves a hair of error alone (any velocity set wakes the
   island around it) and corrects past it. */
static void test_a_still_owner_is_not_corrected_for_a_hair(void) {
    sync_play_t play;
    memset(&play, 0, sizeof(play));
    play.known = true;
    play.rotation[3] = 1.0f;
    const float q[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float near_p[3] = {0.5f * SYNC_CORRECT_REST_M, 0.0f, 0.0f}, far_p[3] = {5.0f * SYNC_CORRECT_REST_M, 0.0f, 0.0f}, v[3], w[3];
    sync_drive_dynamic(near_p, q, &play, 1.0f / 60.0f, v, w);
    TEST_ASSERT_TRUE_MESSAGE(v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f, "a hair of error was corrected");
    sync_drive_dynamic(far_p, q, &play, 1.0f / 60.0f, v, w);
    TEST_ASSERT_TRUE(v[0] < 0.0f);
    play.velocity[0] = 0.5f; /* a moving owner is always followed */
    sync_drive_dynamic(near_p, q, &play, 1.0f / 60.0f, v, w);
    TEST_ASSERT_TRUE(v[0] > 0.0f);
}

/* Rotation turns the shorter way, even when the owner's quaternion changed sign. */
static void test_rotation_takes_the_shorter_arc(void) {
    sync_playback_init(&pb, 60, 0, 0);
    sync_record_t r = moving(4, 0);
    file_at(4, 10, &r);
    float half = 0.5f * 20.0f * 3.14159265f / 180.0f;
    r.rotation[1] = -sinf(half);
    r.rotation[3] = -cosf(half); /* 20 degrees about y, written negated */
    file_at(4, 12, &r);
    set_playback(11.0);
    sync_play_t play;
    sync_playback_sample(&pb, 4, &play);
    float angle = 2.0f * acosf(fminf(1.0f, fabsf(play.rotation[3]))) * 180.0f / 3.14159265f;
    NEAR(0.5f, 10.0f, angle);
}

/* A rest snapshot gives its pose to the bit and stops the copy; past the newest snapshot the
   copy freezes after SYNC_FREEZE_TICKS. */
static void test_a_rest_pose_is_exact(void) {
    sync_playback_init(&pb, 60, 0, 0);
    sync_record_t r = moving(5, 0);
    r.position[0] = 1.2345679f;
    r.position[2] = -0.1f;
    r.at_rest = true;
    r.sim = true;
    memset(r.velocity, 0, sizeof(r.velocity));
    file_at(5, 10, &r);
    sync_record_t later = moving(5, 40);
    file_at(5, 12, &later);
    set_playback(11.5);
    sync_play_t play;
    sync_playback_sample(&pb, 5, &play);
    TEST_ASSERT_TRUE(play.at_rest);
    TEST_ASSERT_EQUAL_MEMORY(r.position, play.position, sizeof(r.position));
    TEST_ASSERT_EQUAL_MEMORY(r.rotation, play.rotation, sizeof(r.rotation));
    set_playback(12.0 + (double)SYNC_FREEZE_TICKS);
    sync_playback_sample(&pb, 5, &play);
    TEST_ASSERT_FALSE(play.frozen);
    set_playback(12.5 + (double)SYNC_FREEZE_TICKS);
    sync_playback_sample(&pb, 5, &play);
    TEST_ASSERT_TRUE(play.frozen);
}

/* Rare groups play at their snapshot's time and stay; a late record keeps only groups newer
   than those played. */
static void test_rare_groups_play_on_time_and_stay(void) {
    sync_playback_init(&pb, 60, 0, 0);
    sync_record_t r = moving(6, 0);
    r.rare.groups = SYNC_RARE_FLAGS | SYNC_RARE_DATA;
    r.rare.flags = 3;
    r.rare.data_size = 2;
    r.rare.data[0] = 'a';
    r.rare.data[1] = 'b';
    file_at(6, 10, &r);
    sync_record_t plain = moving(6, 12);
    file_at(6, 12, &plain);
    uint8_t known = 0;
    set_playback(9.0);
    sync_playback_advance(&pb, 0);
    sync_playback_rare(&pb, 6, &known);
    TEST_ASSERT_EQUAL_UINT8(0, known);
    set_playback(12.0);
    sync_playback_advance(&pb, 1);
    const sync_rare_t *rare = sync_playback_rare(&pb, 6, &known);
    TEST_ASSERT_EQUAL_UINT8(SYNC_RARE_FLAGS | SYNC_RARE_DATA, known);
    TEST_ASSERT_EQUAL_UINT8(3, rare->flags);
    TEST_ASSERT_EQUAL_UINT8(2, rare->data_size);
    TEST_ASSERT_EQUAL_MEMORY("ab", rare->data, 2);
    sync_record_t late = moving(6, 11);
    late.rare.groups = SYNC_RARE_FLAGS;
    late.rare.flags = 5;
    TEST_ASSERT_EQUAL(SYNC_FILE_LATE, sync_playback_file(&pb, 0, 11, &late));
    TEST_ASSERT_EQUAL_UINT8(5, sync_playback_rare(&pb, 6, &known)->flags);
    late.rare.flags = 7;
    TEST_ASSERT_EQUAL(SYNC_FILE_LATE, sync_playback_file(&pb, 0, 8, &late));
    TEST_ASSERT_EQUAL_UINT8(5, sync_playback_rare(&pb, 6, &known)->flags);
    TEST_ASSERT_EQUAL(SYNC_FILE_DUPLICATE, sync_playback_file(&pb, 0, 12, &plain));
}

/* A full buffer keeps its span when everything is still to play, and a dropped snapshot's
   rare groups are not lost. */
static void test_a_full_buffer_thins_and_keeps_its_span(void) {
    sync_playback_init(&pb, 60, 0, 0);
    set_playback(0.5);
    for (uint32_t k = 1; k <= 30; k++) {
        sync_record_t r = moving(8, k);
        if (k == 2) { /* the densest stretch's first inner snapshot: the first one thinned */
            r.rare.groups = SYNC_RARE_FLAGS;
            r.rare.flags = 9;
        }
        file_at(8, k, &r);
    }
    const sync_buffered_t *b = &pb.bodies[8];
    TEST_ASSERT_EQUAL_UINT8(SYNC_PLAYBACK_SNAPSHOTS, b->count);
    TEST_ASSERT_EQUAL_UINT32(1, b->snapshots[0].tick);
    TEST_ASSERT_EQUAL_UINT32(30, b->snapshots[b->count - 1].tick);
    set_playback(30.0);
    sync_playback_advance(&pb, 0);
    uint8_t known = 0;
    TEST_ASSERT_EQUAL_UINT8(9, sync_playback_rare(&pb, 8, &known)->flags);
    TEST_ASSERT_TRUE(known & SYNC_RARE_FLAGS);
    sync_play_t play;
    set_playback(17.5);
    sync_playback_sample(&pb, 8, &play);
    NEAR(1e-4f, 0.875f, play.position[0]); /* straight motion survives thinning */
}

/* A kinematic copy may not be driven past the seam's speeds; a dynamic one's correction is
   bounded on top of its owner's velocity. */
static void test_drive_limits(void) {
    const float dt = 1.0f / 60.0f, at[3] = {0, 0, 0}, still[4] = {0, 0, 0, 1};
    sync_play_t play;
    memset(&play, 0, sizeof(play));
    play.rotation[3] = 1.0f;
    play.position[0] = 10.0f * dt * 0.99f;
    TEST_ASSERT_TRUE(sync_drive_reachable(at, still, &play, dt, 10.0f, 10.0f));
    play.position[0] = 10.0f * dt * 1.01f;
    TEST_ASSERT_FALSE(sync_drive_reachable(at, still, &play, dt, 10.0f, 10.0f));
    play.position[0] = 0.0f;
    float half = 0.5f * 10.0f * dt * 1.05f;
    play.rotation[1] = sinf(half);
    play.rotation[3] = cosf(half);
    TEST_ASSERT_FALSE(sync_drive_reachable(at, still, &play, dt, 10.0f, 10.0f));
    memset(&play, 0, sizeof(play));
    play.rotation[3] = 1.0f;
    play.position[0] = 1.0f;
    play.velocity[0] = 2.0f;
    float v[3], w[3];
    sync_drive_dynamic(at, still, &play, dt, v, w);
    NEAR(1e-4f, 2.0f + SYNC_CORRECT_SPEED, v[0]);
    play.position[0] = 0.01f;
    sync_drive_dynamic(at, still, &play, dt, v, w);
    NEAR(1e-3f, 2.0f + 0.01f / (SYNC_CORRECT_TICKS * dt), v[0]);
}

/* The offset keeps the drawn pose where it was and gives it back at the reviewed rates; past
   its reach it is cleared and the copy is seen to jump. */
static void test_render_offset(void) {
    sync_offset_t o;
    sync_offset_clear(&o);
    const float a[3] = {0, 0, 0}, b[3] = {1.5f, 0, 0}, q[4] = {0, 0, 0, 1};
    sync_offset_absorb(&o, a, q, b, q, SYNC_OFFSET_SNAP_M);
    SAME(-1.5f, o.position[0]);
    sync_offset_decay(&o);
    NEAR(1e-6f, -1.5f * SYNC_OFFSET_FAST, o.position[0]);
    o.position[0] = 0.2f;
    sync_offset_decay(&o);
    NEAR(1e-6f, 0.2f * SYNC_OFFSET_SLOW, o.position[0]);
    o.position[0] = 0.6f;
    sync_offset_decay(&o);
    TEST_ASSERT_TRUE(o.position[0] < 0.6f * SYNC_OFFSET_SLOW && o.position[0] > 0.6f * SYNC_OFFSET_FAST);
    const float far[3] = {2.5f, 0, 0};
    sync_offset_clear(&o);
    sync_offset_absorb(&o, a, q, far, q, SYNC_OFFSET_SNAP_M);
    SAME(0.0f, o.position[0]);
    /* A lost take keeps it, and its drawn pose never moves more than the release at the snap
       reach in a tick, 0.15 of 2 m; past the reach it closes by 0.15 m. */
    sync_offset_absorb(&o, a, q, far, q, SYNC_OFFSET_LOST_M);
    SAME(-2.5f, o.position[0]);
    sync_offset_decay(&o);
    NEAR(1e-5f, -2.5f + (1.0f - SYNC_OFFSET_FAST) * SYNC_OFFSET_FAR_M, o.position[0]);
    for (int k = 0; k < 120; k++) {
        float before = o.position[0];
        sync_offset_decay(&o);
        TEST_ASSERT_TRUE(o.position[0] - before <= (1.0f - SYNC_OFFSET_FAST) * SYNC_OFFSET_SNAP_M + 1e-6f && o.position[0] - before >= 0.0f);
    }
    TEST_ASSERT_TRUE(fabsf(o.position[0]) < 0.01f);
}

/* The rate of a sender at 60 Hz heard by a 59.5 Hz endpoint, and the line a relay ages by. */
static void test_rate_and_line(void) {
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t local = 0; local < 600; local++) {
        uint32_t sample = (uint32_t)(((uint64_t)local * 60000u) / 59500u);
        deliver(0, 2000u + sample, local, 1);
        sync_playback_advance(&pb, local);
    }
    NEAR(0.002f, 60.0f / 59.5f, pb.timelines[0].rate);
    TEST_ASSERT_TRUE(pb.timelines[0].delay <= SYNC_DELAY_MIN + 1);
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t local = 0; local < 200; local++) {
        uint32_t late = local % 3 == 0 ? 2u : 0u; /* a third of the packets two ticks late */
        sync_playback_arrival(&pb, 1, 500u + local, local + late);
    }
    uint32_t at = 0;
    TEST_ASSERT_TRUE(sync_playback_line_tick(&pb, 1, 650, &at));
    TEST_ASSERT_EQUAL_UINT32(150, at); /* the earliest arrival, not the late one */
    TEST_ASSERT_FALSE(sync_playback_line_tick(&pb, 2, 650, &at));
}

/* A stream that starts 600 ticks before the owner's 32-bit tick wraps plays on through the wrap:
   one arithmetic for time, relative to the timeline. */
static void test_time_runs_through_the_wrap(void) {
    sync_playback_init(&pb, 60, 0, 0);
    const uint32_t start = 0xFFFFFFFFu - 600u;
    double last = -1.0, total = 0.0;
    for (uint32_t local = 0; local < 1200; local++) {
        uint32_t sample = start + local;
        sync_playback_arrival(&pb, 0, sample, local);
        sync_record_t r = moving(4, local);
        sync_playback_file(&pb, 0, sample, &r);
        sync_playback_advance(&pb, local);
        double tick = 0.0;
        TEST_ASSERT_TRUE(sync_playback_tick(&pb, 4, &tick));
        TEST_ASSERT_TRUE_MESSAGE(tick >= 0.0 && tick < 4294967296.0, "the owner tick left the owner's 32-bit count");
        if (last >= 0.0) {
            double step = tick - last;
            step += step < -2147483648.0 ? 4294967296.0 : 0.0; /* the owner's count wrapped */
            TEST_ASSERT_TRUE_MESSAGE(step >= 0.0 && step <= 1.25 * 1.0 + 1e-6, "playback went back at the wrap");
            total += step;
        }
        last = tick;
        sync_play_t play;
        sync_playback_sample(&pb, 4, &play);
        TEST_ASSERT_TRUE(play.known && !play.frozen);
    }
    TEST_ASSERT_TRUE(total > 1150.0);
}

/* The jitter is the 95th percentile, not the median: a tenth of the packets six ticks late sets
   the delay, plus its margin of a tick. 30 ticks keep the rate at 1, so the lag is exact. */
static void test_the_delay_follows_the_late_tail(void) {
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t local = 0; local < 30; local++) {
        uint32_t late = local % 10 == 0 ? 6u : 0u;
        sync_playback_arrival(&pb, 0, 100u + local, local + late);
        sync_playback_advance(&pb, local);
    }
    TEST_ASSERT_EQUAL_UINT8(6 + SYNC_DELAY_MARGIN, pb.timelines[0].delay);
}

/* A sender three times faster than this endpoint reads at the rate's bound. */
static void test_the_rate_is_bounded(void) {
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t local = 0; local < 200; local++) {
        sync_playback_arrival(&pb, 0, 100u + 3u * local, local);
        sync_playback_advance(&pb, local);
    }
    SAME((float)SYNC_RATE_HIGH, pb.timelines[0].rate);
}

/* A seat that leaves keeps its bodies' snapshots (the host takes them back from there); one that
   joins forgets them. A snapshot exactly at playback time is still to play, not late. */
static void test_leave_keeps_and_a_sample_at_playback_is_taken(void) {
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t local = 0; local < 50; local++) {
        deliver(1, local, local, 6);
        sync_playback_advance(&pb, local);
    }
    sync_playback_forget_seat(&pb, 1, true);
    TEST_ASSERT_TRUE(pb.bodies[6].has && pb.bodies[6].count > 0);
    sync_playback_forget_seat(&pb, 1, false);
    TEST_ASSERT_FALSE(pb.bodies[6].has);
    sync_playback_init(&pb, 60, 0, 0);
    set_playback(20.0);
    sync_record_t r = moving(6, 20);
    TEST_ASSERT_EQUAL(SYNC_FILE_TAKEN, sync_playback_file(&pb, 0, 20, &r));
}

/* A fast body that stopped dead between two snapshots is never drawn past where it stopped. */
static void test_hermite_stays_between_its_snapshots(void) {
    sync_playback_init(&pb, 60, 0, 0);
    sync_record_t a0 = moving(9, 0), a1 = moving(9, 0);
    a0.position[0] = 0.0f;
    a0.velocity[0] = 50.0f;
    a1.position[0] = 0.5f; /* the wall */
    a1.velocity[0] = 0.0f;
    file_at(9, 10, &a0);
    file_at(9, 12, &a1);
    for (double t = 10.0; t <= 12.0; t += 0.125) {
        set_playback(t);
        sync_play_t play;
        sync_playback_sample(&pb, 9, &play);
        TEST_ASSERT_TRUE_MESSAGE(play.position[0] >= 0.0f && play.position[0] <= 0.5f, "drawn past where the body stopped");
    }
}

/* Once the clock knows the path, a sample far ahead of its line moves nothing: not the newest,
   not playback. */
static void test_the_clock_ignores_a_far_sample(void) {
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t local = 0; local < 100; local++) {
        deliver(0, 500u + local, local, 1);
        sync_playback_advance(&pb, local);
    }
    TEST_ASSERT_FALSE(sync_playback_plausible(&pb, 0, 599u + SYNC_DELAY_MAX + SYNC_JUMP_TICKS + 5u, 99));
    TEST_ASSERT_TRUE(sync_playback_plausible(&pb, 0, 600u, 99));
    sync_playback_arrival(&pb, 0, 600u + 5000u, 99);
    TEST_ASSERT_EQUAL_UINT32(599, pb.timelines[0].newest);
}

static double owner_tick(uint16_t slot) {
    double tick = 0.0;
    TEST_ASSERT_TRUE(sync_playback_tick(&pb, slot, &tick));
    return tick;
}

/* Honest changes of an owner's time come back within SYNC_FAR_RUN ticks: a latency drop, a
   skipped tick, the end of a silence; a lone far tick, even among the first samples, is dropped
   and moves nothing. */
typedef uint32_t (*arrive_fn)(uint32_t local);
static uint32_t latency_drop(uint32_t l) { return 5000u + l - (l < 400 ? 120u : 10u); }
static uint32_t tick_skip(uint32_t l) { return 5000u + l + (l >= 500 ? 200u : 0u); }
static uint32_t silence_then_ahead(uint32_t l) { return (l >= 300 && l < 700) ? UINT32_MAX : 5000u + l + (l >= 700 ? 300u : 0u); }
static uint32_t early_far(uint32_t l) { return l == 5 ? 5000u + l + 100000u : 5000u + l; }

static void stream(arrive_fn f, uint32_t ticks) {
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t l = 1; l <= ticks; l++) {
        uint32_t sample = f(l);
        if (sample != UINT32_MAX) {
            deliver(1, sample, l, 7);
        }
        sync_playback_advance(&pb, l);
    }
}

static void test_the_far_rule_drops_a_lone_tick_and_follows_a_run(void) {
    const arrive_fn honest[] = {latency_drop, tick_skip, silence_then_ahead};
    for (int i = 0; i < 3; i++) {
        stream(honest[i], 1000);
        const sync_timeline_t *t = &pb.timelines[1];
        TEST_ASSERT_TRUE_MESSAGE(t->far_dropped <= SYNC_FAR_RUN - 1u && t->restarts == 1, "an honest change of time was not followed");
        TEST_ASSERT_TRUE_MESSAGE((double)t->newest - owner_tick(7) < (double)SYNC_DELAY_MAX + 2.0, "the copy did not catch up");
    }
    stream(early_far, 1000);
    const sync_timeline_t *t = &pb.timelines[1];
    TEST_ASSERT_EQUAL_UINT32(1, t->far_dropped);
    TEST_ASSERT_EQUAL_UINT32(0, t->restarts);
    TEST_ASSERT_EQUAL_UINT32(0, t->jumps);
    TEST_ASSERT_TRUE((double)t->newest - owner_tick(7) < (double)SYNC_DELAY_MAX);
    /* Past the longest delay but short of the far distance is no outlier. */
    TEST_ASSERT_TRUE(sync_playback_plausible(&pb, 1, t->newest + SYNC_DELAY_MAX + 20u, 1000));
    TEST_ASSERT_FALSE(sync_playback_plausible(&pb, 1, t->newest + SYNC_FAR_TICKS + 10u, 1000));
}

/* A rebase moves the base and playback together: the owner tick goes on without a step. */
static void test_a_rebase_keeps_the_owner_tick(void) {
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t l = 0; l < 60; l++) {
        deliver(0, 7000u + l, l, 2);
        sync_playback_advance(&pb, l);
    }
    sync_timeline_t *t = &pb.timelines[0];
    const double shift = (double)SYNC_REBASE_TICKS + 5.0;
    t->base -= (uint32_t)shift; /* the same times, counted from further back */
    t->playback += shift;
    double before = owner_tick(2);
    deliver(0, 7060u, 60, 2);
    sync_playback_advance(&pb, 60);
    TEST_ASSERT_EQUAL_UINT32(t->newest, t->base); /* it rebased */
    double step = owner_tick(2) - before;
    TEST_ASSERT_TRUE_MESSAGE(step >= 0.0 && step <= 1.25 * (double)t->rate + 1e-6, "a rebase moved the owner tick");
}

/* forget_until drops the snapshot at its tick too; a copy waiting at its newest snapshot keeps
   that snapshot's motion for a dynamic hold. With every snapshot dropped it plays nothing, and
   reports no owner tick: its timeline's would name a picture it does not show. */
static void test_forget_until_and_a_waiting_copys_motion(void) {
    sync_playback_init(&pb, 60, 0, 0);
    for (uint32_t k = 10; k <= 14; k++) {
        sync_record_t r = moving(3, k);
        file_at(3, k, &r);
    }
    sync_playback_forget_until(&pb, 3, 12);
    TEST_ASSERT_EQUAL_UINT8(2, pb.bodies[3].count);
    TEST_ASSERT_EQUAL_UINT32(13, pb.bodies[3].snapshots[0].tick);
    set_playback(14.5);
    sync_play_t play;
    sync_playback_sample(&pb, 3, &play);
    TEST_ASSERT_FALSE(play.frozen);
    SAME(3.0f, play.velocity[0]);
    double tick;
    TEST_ASSERT_TRUE(sync_playback_tick(&pb, 3, &tick));
    sync_playback_forget_until(&pb, 3, 14);
    sync_playback_sample(&pb, 3, &play);
    TEST_ASSERT_FALSE(play.known);
    TEST_ASSERT_FALSE_MESSAGE(sync_playback_tick(&pb, 3, &tick), "a body with nothing to play reported its timeline's tick");
}

/* The locked axes' part of a turn, and only that. */
static void test_the_locked_turn(void) {
    const float now[4] = {0, 0, 0, 1};
    float half = 0.5f * 20.0f * 3.14159265f / 180.0f;
    const float about_z[4] = {0, 0, sinf(half), cosf(half)};
    float out[4];
    TEST_ASSERT_FALSE_MESSAGE(sync_drive_locked_turn(now, about_z, 1u, out), "a turn on a free axis was set");
    const float about_x[4] = {sinf(half), 0, 0, cosf(half)};
    TEST_ASSERT_TRUE(sync_drive_locked_turn(now, about_x, 1u, out));
    NEAR(1e-4f, about_x[0], out[0]);
    NEAR(1e-4f, 0.0f, out[2]);
    TEST_ASSERT_FALSE(sync_drive_locked_turn(now, about_x, 0u, out));
    float tiny = 0.5f * 0.3f * 3.14159265f / 180.0f;
    const float hair[4] = {sinf(tiny), 0, 0, cosf(tiny)};
    TEST_ASSERT_FALSE(sync_drive_locked_turn(now, hair, 1u, out));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_playback_time_is_monotone_and_bounded);
    RUN_TEST(test_a_duplicate_is_not_jitter);
    RUN_TEST(test_the_clock_before_it_knows_and_after_a_rejoin);
    RUN_TEST(test_freeze_and_jump);
    RUN_TEST(test_hermite_for_sim_linear_for_copy);
    RUN_TEST(test_a_still_owner_is_not_corrected_for_a_hair);
    RUN_TEST(test_rotation_takes_the_shorter_arc);
    RUN_TEST(test_a_rest_pose_is_exact);
    RUN_TEST(test_rare_groups_play_on_time_and_stay);
    RUN_TEST(test_a_full_buffer_thins_and_keeps_its_span);
    RUN_TEST(test_drive_limits);
    RUN_TEST(test_render_offset);
    RUN_TEST(test_rate_and_line);
    RUN_TEST(test_time_runs_through_the_wrap);
    RUN_TEST(test_the_delay_follows_the_late_tail);
    RUN_TEST(test_the_rate_is_bounded);
    RUN_TEST(test_leave_keeps_and_a_sample_at_playback_is_taken);
    RUN_TEST(test_hermite_stays_between_its_snapshots);
    RUN_TEST(test_the_clock_ignores_a_far_sample);
    RUN_TEST(test_the_far_rule_drops_a_lone_tick_and_follows_a_run);
    RUN_TEST(test_a_rebase_keeps_the_owner_tick);
    RUN_TEST(test_forget_until_and_a_waiting_copys_motion);
    RUN_TEST(test_the_locked_turn);
    return UNITY_END();
}
