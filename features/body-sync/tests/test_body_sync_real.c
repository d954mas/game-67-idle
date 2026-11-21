#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "stub_wire.h"
#include "sync_codec.h"
#include "unity.h"

/* The real seam with Box3D on three endpoints, every state packet's fate answered at once.
   Authority is the shared part the stub's suite covers; these are what the real shell adds:
   records with rare groups, rest poses, the host's check of a guest's declared number,
   take-overs from the receiver, and untrusted bytes through apply, step and collect. */

enum { BODIES = 40, SEATS = 3, GUEST = 1, OTHER = 2 };

typedef struct side {
    b3WorldId world;
    b3BodyId bodies[BODIES];
    body_sync_t *sync;
    uint8_t seat;
    uint16_t numbers[SEATS];
} side_t;

static side_t host, guest, other;
static uint8_t packet[1500];
static uint32_t tick_no = 100;
static int box3d_asserts;
static const body_sync_id_t ID0 = 1u << 16;

static int count_assert(const char *condition, const char *file, int line) {
    (void)condition;
    (void)file;
    (void)line;
    box3d_asserts++;
    return 0;
}

static void build(side_t *s, uint8_t seat) {
    b3WorldDef def = b3DefaultWorldDef();
    def.gravity = (b3Vec3){0.0f, -12.0f, 0.0f};
    s->world = b3CreateWorld(&def);
    s->seat = seat;
    memset(s->numbers, 0, sizeof(s->numbers));
    b3BodyDef floor = b3DefaultBodyDef();
    floor.position = (b3Vec3){0.0f, -0.5f, 0.0f};
    b3BodyId ground = b3CreateBody(s->world, &floor);
    b3ShapeDef shape = b3DefaultShapeDef();
    b3BoxHull slab = b3MakeBoxHull(39.0f, 0.5f, 39.0f);
    b3CreateHullShape(ground, &shape, &slab.base);
    b3BoxHull cube = b3MakeBoxHull(0.2f, 0.2f, 0.2f);
    shape.enableContactEvents = true;
    body_sync_config_t config = {.world = s->world,
                                 .seat = seat,
                                 .seats = SEATS,
                                 .tick_hz = 60,
                                 .bounds_min = {-40.0f, -15.0f, -40.0f},
                                 .bounds_max = {40.0f, 50.0f, 40.0f},
                                 .max_speed = 100.0f,
                                 .max_spin = 200.0f};
    s->sync = body_sync_create(&config);
    TEST_ASSERT_NOT_NULL(s->sync);
    for (uint32_t i = 0; i < BODIES; i++) {
        b3BodyDef d = b3DefaultBodyDef();
        d.type = b3_dynamicBody;
        d.position = (b3Vec3){(float)(i % 8) * 1.0f, 0.2f, (float)(i / 8) * 1.0f};
        s->bodies[i] = b3CreateBody(s->world, &d);
        b3CreateHullShape(s->bodies[i], &shape, &cube.base);
        /* The last body is a character: sent every packet whatever it does. */
        body_sync_bind_t bind = {.body = s->bodies[i], .body_class = i == BODIES - 1 ? BODY_SYNC_CLASS_CHARACTER : BODY_SYNC_CLASS_LIGHT,
                                 .authority = BODY_SYNC_SEAT_HOST};
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_bind(s->sync, body_sync_id_make(i, 1), &bind));
    }
}

void setUp(void) {
    b3SetAssertFcn(count_assert);
    box3d_asserts = 0;
    build(&host, BODY_SYNC_SEAT_HOST);
    build(&guest, GUEST);
    build(&other, OTHER);
    body_sync_seat_joined(host.sync, GUEST);
    body_sync_seat_joined(host.sync, OTHER);
    body_sync_seat_joined(guest.sync, BODY_SYNC_SEAT_HOST);
    body_sync_seat_joined(other.sync, BODY_SYNC_SEAT_HOST);
}

void tearDown(void) {
    side_t *sides[] = {&host, &guest, &other};
    for (int i = 0; i < 3; i++) {
        body_sync_destroy(sides[i]->sync);
        b3DestroyWorld(sides[i]->world);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, box3d_asserts, "the seam handed Box3D something it asserts on");
}

/* The tick's one packet, delivered, its fate answered at once. */
static void state(side_t *from, side_t *to) {
    uint32_t size = 0;
    if (body_sync_write_state(from->sync, to->seat, packet, 1200, &size)) {
        TEST_ASSERT_TRUE(size <= 1000);
        TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_OK, body_sync_receive_state(to->sync, from->seat, tick_no, packet, size), body_sync_last_error(to->sync));
        uint16_t n = from->numbers[to->seat]++;
        body_sync_state_sent(from->sync, to->seat, n);
        body_sync_resolve(from->sync, to->seat, n, true);
    }
    TEST_ASSERT_FALSE(body_sync_write_state(from->sync, to->seat, packet, 1200, &size)); /* one a tick */
}

static void records(side_t *from, side_t *to) {
    uint32_t size = 0;
    while (body_sync_peek_record(from->sync, to->seat, packet, sizeof(packet), &size)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_record(to->sync, from->seat, tick_no, packet, size));
        body_sync_pop_record(from->sync, to->seat);
    }
}

static void step(side_t *s) {
    body_sync_apply(s->sync, tick_no);
    b3World_Step(s->world, 1.0f / 60.0f, 4);
    body_sync_collect(s->sync, tick_no);
}

static void tick_all(void) {
    tick_no++;
    step(&host);
    step(&guest);
    step(&other);
    state(&host, &guest);
    state(&host, &other);
    state(&guest, &host);
    state(&other, &host);
    records(&host, &guest);
    records(&host, &other);
    records(&guest, &host);
    records(&other, &host);
}

static void grant(side_t *g, body_sync_id_t id, body_sync_hold_t hold) {
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(g->sync, id, hold));
    records(g, &host);
    body_sync_claim_event_t c;
    while (body_sync_poll_claim(host.sync, &c)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, true));
    }
    records(&host, &guest);
    records(&host, &other);
}

static float speed_of(b3BodyId body) {
    b3Vec3 v = b3Body_GetLinearVelocity(body);
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static float apart(b3BodyId a, b3BodyId b) {
    b3Vec3 p = b3Body_GetPosition(a), q = b3Body_GetPosition(b);
    return sqrtf((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z));
}

/* At rest the worlds agree to the precise grid, and a quiet world sends nothing. */
static void test_worlds_at_rest_agree_and_go_quiet(void) {
    b3Body_SetLinearVelocity(host.bodies[3], (b3Vec3){2.0f, 3.0f, 0.0f});
    for (int i = 0; i < 240; i++) {
        tick_all();
    }
    for (uint32_t i = 0; i < BODIES; i++) {
        TEST_ASSERT_FALSE(b3Body_IsAwake(host.bodies[i]));
        TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[i], guest.bodies[i]) <= 0.5f * SYNC_SIM_POSITION_STEP * 1.8f, "a copy at rest is off the host's pose");
        TEST_ASSERT_TRUE(apart(host.bodies[i], other.bodies[i]) <= 0.5f * SYNC_SIM_POSITION_STEP * 1.8f);
    }
    tick_no++;
    step(&host);
    uint32_t size = 0;
    /* only the character goes out, every packet: one record */
    TEST_ASSERT_TRUE_MESSAGE(!body_sync_write_state(host.sync, GUEST, packet, 1200, &size) || size <= 17, "a world at rest still sends");
    body_sync_traffic_t t;
    TEST_ASSERT_TRUE(body_sync_traffic(host.sync, GUEST, &t));
    TEST_ASSERT_TRUE(t.sent_bytes[BODY_SYNC_TRAFFIC_COPY] > 0 && t.sent_bytes[BODY_SYNC_TRAFFIC_SIM] > 0);
}

/* A granted guest's poses count once it has declared the new number; a release hands the body
   back with the guest's velocity. */
static void test_a_guests_body_comes_back_moving(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    tick_all();
    b3Body_SetTransform(guest.bodies[0], (b3Vec3){3.0f, 2.0f, 3.0f}, b3Quat_identity);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){8.0f, 0.0f, 0.0f});
    tick_all();
    tick_all();
    TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[0], guest.bodies[0]) < 0.5f, "the host did not take the guest's poses");
    tick_all(); /* the host relays what it took a tick later */
    TEST_ASSERT_TRUE_MESSAGE(apart(other.bodies[0], guest.bodies[0]) < 1.0f, "the host did not relay the guest's body");
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    records(&guest, &host);
    tick_no++;
    step(&host);
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_TRUE_MESSAGE(speed_of(host.bodies[0]) > 4.0f, "the body lost its velocity at the handover");
}

/* A pose written under an old grant is rejected after a revoke and a re-grant. */
static void test_a_pose_of_an_old_grant_is_rejected(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    tick_all();
    b3Body_SetTransform(guest.bodies[0], (b3Vec3){5.0f, 5.0f, 5.0f}, b3Quat_identity);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){4.0f, 0.0f, 0.0f}); /* due every tick */
    tick_no++;
    step(&guest);
    uint32_t old_size = 0;
    TEST_ASSERT_TRUE(body_sync_write_state(guest.sync, BODY_SYNC_SEAT_HOST, packet, 1200, &old_size));
    uint8_t old[1200];
    memcpy(old, packet, old_size);
    body_sync_assign(host.sync, ID0, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_assign(host.sync, ID0, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_traffic_t t0, t1;
    body_sync_traffic(host.sync, GUEST, &t0);
    body_sync_receive_state(host.sync, GUEST, tick_no, old, old_size);
    body_sync_traffic(host.sync, GUEST, &t1);
    TEST_ASSERT_TRUE(t1.rejected > t0.rejected);
}

/* Becoming local continues the last owner's flags, motion type and holder. */
static void test_flags_type_and_holder_move_with_authority(void) {
    tick_all();
    b3Body_SetType(host.bodies[0], b3_kinematicBody);
    body_sync_set_flags(host.sync, ID0, 5);
    tick_all();
    tick_all();
    TEST_ASSERT_EQUAL_UINT8(5, body_sync_flags(guest.sync, ID0));
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_apply(guest.sync, tick_no);
    TEST_ASSERT_EQUAL_UINT8(5, body_sync_flags(guest.sync, ID0));
    TEST_ASSERT_EQUAL(b3_kinematicBody, b3Body_GetType(guest.bodies[0]));
    b3Body_SetType(guest.bodies[0], b3_dynamicBody);
    body_sync_set_flags(guest.sync, ID0, 3);
    body_sync_set_holder(guest.sync, ID0, body_sync_id_make(39, 1));
    tick_all();
    tick_all();
    body_sync_authority_t a;
    body_sync_authority(host.sync, ID0, &a);
    TEST_ASSERT_EQUAL_UINT32(body_sync_id_make(39, 1), a.holder);
    body_sync_set_holder(guest.sync, ID0, BODY_SYNC_ID_NONE);
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    tick_all();
    tick_all();
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_EQUAL(b3_dynamicBody, b3Body_GetType(host.bodies[0]));
    TEST_ASSERT_EQUAL_UINT8(3, body_sync_flags(host.sync, ID0));
    body_sync_authority(host.sync, ID0, &a);
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, a.holder);
}

/* Motion locks travel with the state: a grant continues the last owner's, not this side's. */
static void test_locks_follow_the_last_owner(void) {
    const b3MotionLocks locked = {.angularX = true, .angularY = true, .angularZ = true};
    b3Body_SetMotionLocks(host.bodies[0], locked);
    b3Body_SetMotionLocks(guest.bodies[0], locked);
    tick_all();
    b3MotionLocks l = b3Body_GetMotionLocks(guest.bodies[0]);
    TEST_ASSERT_FALSE(l.angularX || l.angularY || l.angularZ); /* a copy turns freely */
    b3Body_SetMotionLocks(host.bodies[0], (b3MotionLocks){0});
    tick_all();
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    l = b3Body_GetMotionLocks(guest.bodies[0]);
    TEST_ASSERT_FALSE(l.angularX || l.angularY || l.angularZ);
    b3Body_SetMotionLocks(guest.bodies[0], locked);
    tick_all();
    body_sync_seat_left(host.sync, GUEST);
    tick_no++;
    step(&host);
    l = b3Body_GetMotionLocks(host.bodies[0]);
    TEST_ASSERT_TRUE(l.angularX && l.angularY && l.angularZ);
}

/* Untrusted bytes through both receive calls, with apply, step and collect after them: FORMAT
   or OK, never a Box3D assert, never a body outside the box. */
static void test_receive_fuzz(void) {
    for (int i = 0; i < 4; i++) {
        tick_all();
    }
    b3Body_SetLinearVelocity(host.bodies[5], (b3Vec3){3.0f, 4.0f, 0.0f});
    tick_no++;
    step(&host);
    uint32_t size = 0;
    TEST_ASSERT_TRUE(body_sync_write_state(host.sync, GUEST, packet, 1200, &size));
    uint8_t valid[1200], buffer[1300];
    memcpy(valid, packet, size);
    for (uint32_t cut = 0; cut < size; cut++) {
        TEST_ASSERT_EQUAL(BODY_SYNC_FORMAT, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no, valid, cut));
    }
    uint64_t seed = 0x5EED;
    for (int i = 0; i < 20000; i++) {
        memcpy(buffer, valid, size);
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        uint32_t flips = 1 + (uint32_t)((seed >> 60) % 4u);
        for (uint32_t f = 0; f < flips; f++) {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            buffer[(seed >> 33) % size] ^= (uint8_t)(1u << ((seed >> 20) % 8));
        }
        body_sync_result_t r = body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no + (uint32_t)i, buffer, size);
        TEST_ASSERT_TRUE(r == BODY_SYNC_OK || r == BODY_SYNC_FORMAT);
        r = body_sync_receive_state(host.sync, GUEST, tick_no + (uint32_t)i, buffer, size);
        TEST_ASSERT_TRUE(r == BODY_SYNC_OK || r == BODY_SYNC_FORMAT);
        if (i % 16 == 0) {
            step(&guest);
            step(&host);
        }
    }
    for (int i = 0; i < 20000; i++) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        uint32_t n = (uint32_t)((seed >> 33) % sizeof(buffer));
        for (uint32_t k = 0; k < n; k++) {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            buffer[k] = (uint8_t)(seed >> 56);
        }
        body_sync_result_t r = body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no + 30000u + (uint32_t)i, buffer, n);
        TEST_ASSERT_TRUE(r == BODY_SYNC_OK || r == BODY_SYNC_FORMAT);
        r = body_sync_receive_record(guest.sync, BODY_SYNC_SEAT_HOST, 3, buffer, n % 16);
        TEST_ASSERT_TRUE(r == BODY_SYNC_OK || r == BODY_SYNC_FORMAT);
        r = body_sync_receive_record(host.sync, GUEST, 3, buffer, n % 16);
        TEST_ASSERT_TRUE(r == BODY_SYNC_OK || r == BODY_SYNC_FORMAT);
        if (i % 16 == 0) {
            step(&guest);
            step(&host);
        }
    }
    for (uint32_t i = 0; i < BODIES; i++) {
        b3Vec3 p = b3Body_GetPosition(guest.bodies[i]);
        TEST_ASSERT_TRUE(isfinite(p.x) && isfinite(p.y) && isfinite(p.z));
        TEST_ASSERT_TRUE(fabsf(p.x) <= 41.0f && fabsf(p.z) <= 41.0f && p.y >= -16.0f && p.y <= 51.0f);
    }
}

/* Limits the wire cannot carry refuse create. */
static void test_create_refuses_limits_too_wide_for_the_wire(void) {
    body_sync_config_t config = {.world = host.world, .seat = 0, .seats = 2, .tick_hz = 60, .bounds_min = {-5000.0f, -1.0f, -1.0f},
                                 .bounds_max = {5000.0f, 1.0f, 1.0f}, .max_speed = 10.0f, .max_spin = 10.0f};
    TEST_ASSERT_NULL(body_sync_create(&config));
    config.bounds_min[0] = -100.0f;
    config.bounds_max[0] = 100.0f;
    body_sync_t *ok = body_sync_create(&config);
    TEST_ASSERT_NOT_NULL(ok);
    body_sync_destroy(ok);
    config.state_budget = 60; /* two seats: no room for a record after the characters */
    TEST_ASSERT_NULL(body_sync_create(&config));
    config.state_budget = 1000;
    ok = body_sync_create(&config);
    TEST_ASSERT_NOT_NULL(ok);
    body_sync_destroy(ok);
    TEST_ASSERT_EQUAL_STRING("real 0.5.0", body_sync_implementation());
}

/* A carried body and a character move with velocity zero (teleported each step), yet they go
   out every tick; a guest's flags reach the other guest through the host. */
static void test_carried_bodies_characters_and_relayed_flags(void) {
    tick_all();
    tick_all();
    body_sync_set_holder(host.sync, body_sync_id_make(6, 1), body_sync_id_make(39, 1));
    for (int i = 0; i < 12; i++) {
        float x = 2.0f + 0.05f * (float)i;
        b3Body_SetTransform(host.bodies[6], (b3Vec3){x, 1.0f, 2.0f}, b3Quat_identity);
        b3Body_SetLinearVelocity(host.bodies[6], (b3Vec3){0.0f, 0.0f, 0.0f});
        b3Body_SetTransform(host.bodies[39], (b3Vec3){-x, 1.0f, -2.0f}, b3Quat_identity);
        b3Body_SetLinearVelocity(host.bodies[39], (b3Vec3){0.0f, 0.0f, 0.0f});
        tick_all();
        if (i > 2) {
            TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[6], guest.bodies[6]) < 0.09f, "a carried body lagged its rate");
            TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[39], guest.bodies[39]) < 0.09f, "a character lagged its rate");
        }
    }
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    body_sync_set_flags(guest.sync, ID0, 11);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){2.0f, 0.0f, 0.0f});
    for (int i = 0; i < 4; i++) {
        tick_all();
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(11, body_sync_flags(other.sync, ID0), "the host's relay dropped the guest's flags");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_worlds_at_rest_agree_and_go_quiet);
    RUN_TEST(test_a_guests_body_comes_back_moving);
    RUN_TEST(test_a_pose_of_an_old_grant_is_rejected);
    RUN_TEST(test_flags_type_and_holder_move_with_authority);
    RUN_TEST(test_locks_follow_the_last_owner);
    RUN_TEST(test_receive_fuzz);
    RUN_TEST(test_create_refuses_limits_too_wide_for_the_wire);
    RUN_TEST(test_carried_bodies_characters_and_relayed_flags);
    return UNITY_END();
}
