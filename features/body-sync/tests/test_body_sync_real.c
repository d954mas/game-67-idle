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
static float max_speed = 100.0f; /* a test may rebuild the sides with another */
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
                                 .max_speed = max_speed,
                                 .max_spin = 200.0f,
                                 .delay_min = 1, /* the harness delivers every packet in its tick */
                                 .delay_max = 1};
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
    tick_all(); /* a tick of playback delay */
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
/* Bit flips on a host packet to the other guest that relays a guest's body with its origin
   and data bytes, through apply, a step and collect. */
static void fuzz_relayed(void) {
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    const uint8_t bytes[BODY_SYNC_DATA_MAX] = {9, 8, 7, 6, 5, 4, 3, 2};
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_data(guest.sync, ID0, bytes, sizeof(bytes)));
    for (int i = 0; i < 3; i++) {
        b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){1.0f, 2.0f, 0.0f});
        tick_all();
    }
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){1.0f, 2.0f, 0.0f});
    tick_no++;
    step(&guest);
    state(&guest, &host);
    step(&host);
    uint32_t size = 0;
    TEST_ASSERT_TRUE(body_sync_write_state(host.sync, OTHER, packet, 1200, &size));
    uint8_t valid[1200], buffer[1200];
    memcpy(valid, packet, size);
    uint64_t seed = 0xFACE;
    for (int i = 0; i < 20000; i++) {
        memcpy(buffer, valid, size);
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        uint32_t flips = 1 + (uint32_t)((seed >> 60) % 4u);
        for (uint32_t f = 0; f < flips; f++) {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            buffer[(seed >> 33) % size] ^= (uint8_t)(1u << ((seed >> 20) % 8));
        }
        body_sync_result_t r = body_sync_receive_state(other.sync, BODY_SYNC_SEAT_HOST, tick_no + (uint32_t)i, buffer, size);
        TEST_ASSERT_TRUE(r == BODY_SYNC_OK || r == BODY_SYNC_FORMAT);
        if (i % 16 == 0) {
            step(&other);
        }
    }
    tick_no += 20000u;
}

static void test_receive_fuzz(void) {
    for (int i = 0; i < 4; i++) {
        tick_all();
    }
    fuzz_relayed();
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
    TEST_ASSERT_EQUAL_STRING("real 0.6.0", body_sync_implementation());
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
            TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[6], guest.bodies[6]) < 0.15f, "a carried body lagged its rate");
            TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[39], guest.bodies[39]) < 0.15f, "a character lagged its rate");
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

/* A take-over moves physics to the newest state received and leaves the drawn pose where the
   copy showed it: the render offset holds the difference, and physics never sees it. */
static void test_a_take_over_keeps_the_drawn_pose(void) {
    tick_all();
    tick_all();
    for (int i = 0; i < 6; i++) {
        b3Body_SetLinearVelocity(host.bodies[2], (b3Vec3){6.0f, 0.0f, 0.0f});
        tick_all();
    }
    const body_sync_id_t id = body_sync_id_make(2, 1);
    b3Vec3 drawn = b3Body_GetPosition(guest.bodies[2]), newest = b3Body_GetPosition(host.bodies[2]);
    float offset[3], turn[4];
    TEST_ASSERT_TRUE(body_sync_render_offset(guest.sync, id, offset, turn));
    drawn.x += offset[0];
    drawn.y += offset[1];
    drawn.z += offset[2];
    grant(&guest, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_no++;
    body_sync_apply(guest.sync, tick_no);
    b3Vec3 p = b3Body_GetPosition(guest.bodies[2]);
    TEST_ASSERT_TRUE(body_sync_render_offset(guest.sync, id, offset, turn));
    /* The host's copy records carry 1/512 m. */
    TEST_ASSERT_TRUE_MESSAGE(fabsf(p.x - newest.x) <= SYNC_COPY_POSITION_STEP && fabsf(p.y - newest.y) <= SYNC_COPY_POSITION_STEP,
                             "physics did not continue from the newest state");
    TEST_ASSERT_TRUE_MESSAGE(fabsf(p.x + offset[0] - drawn.x) < 1e-4f && fabsf(p.y + offset[1] - drawn.y) < 1e-4f, "the drawn pose jumped");
    TEST_ASSERT_TRUE(fabsf(offset[0]) > 0.05f); /* the copy played a tick behind the newest */
}

/* A copy is never driven past max_speed: a move it could not make in one step is a placement,
   and the render offset takes it. */
static void test_a_copy_is_never_driven_past_max_speed(void) {
    tick_all();
    tick_all();
    b3Vec3 at = b3Body_GetPosition(host.bodies[3]);
    b3Body_SetTransform(host.bodies[3], (b3Vec3){at.x + 1.8f, at.y, at.z}, b3Quat_identity); /* 1.8 m > 100 m/s / 60 */
    float most = 0.0f;
    for (int i = 0; i < 4; i++) {
        tick_all();
        b3Vec3 v = b3Body_GetLinearVelocity(guest.bodies[3]);
        TEST_ASSERT_TRUE_MESSAGE(sqrtf(v.x * v.x + v.y * v.y + v.z * v.z) <= 100.0f + 1e-3f, "a copy was driven past max_speed");
        float offset[3], turn[4];
        body_sync_render_offset(guest.sync, body_sync_id_make(3, 1), offset, turn);
        most = fabsf(offset[0]) > most ? fabsf(offset[0]) : most;
    }
    TEST_ASSERT_TRUE_MESSAGE(most > 1.0f, "the placement's jump did not go to the render offset");
    TEST_ASSERT_TRUE(apart(host.bodies[3], guest.bodies[3]) < 0.05f);
}

/* The host relays a guest's state with its age, the ticks since it took it, and stops once it
   is older than SYNC_RELAY_AGE_MAX; the other guest plays it at its own time, not the relay's. */
static void test_a_relayed_state_carries_its_age(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    for (int i = 0; i < 20; i++) {
        b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){1.0f, 0.0f, 0.0f});
        tick_all();
    }
    const uint32_t taken = tick_no;
    double before = 0.0;
    TEST_ASSERT_TRUE(body_sync_playback(other.sync, ID0, &before));
    TEST_ASSERT_TRUE(before <= (double)taken);
    const sync_limits_t limits = {{-40.0f, -15.0f, -40.0f}, {40.0f, 50.0f, 40.0f}, 100.0f, 200.0f};
    static sync_codec_t codec;
    static sync_record_t got[BODY_SYNC_BODIES_MAX];
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &limits));
    uint32_t relayed = 0, newest_age = 0, last_age = 0; /* last_age: the age less n */
    for (uint32_t n = 1; n <= 70; n++) {
        tick_no++;
        step(&host);
        uint32_t size = 0;
        while (body_sync_write_state(host.sync, OTHER, packet, 1200, &size)) {
            uint32_t count = 0;
            char why[96];
            TEST_ASSERT_TRUE(sync_packet_read(&codec, SEATS, packet, size, got, BODY_SYNC_BODIES_MAX, &count, why, sizeof(why)));
            for (uint32_t k = 0; k < count; k++) {
                if (got[k].slot == 0) {
                    TEST_ASSERT_TRUE(got[k].origin.relayed && got[k].origin.seat == GUEST);
                    /* The guest's last state went out a tick or two before `taken`: from there
                       the age is the host's ticks since, on every resend. */
                    uint32_t lead = got[k].origin.age - n;
                    TEST_ASSERT_TRUE_MESSAGE(got[k].origin.age >= n && lead <= 2 && (relayed == 0 || lead == last_age),
                                             "a relay's age is not the ticks since the host took the state");
                    last_age = lead;
                    relayed++;
                    newest_age = got[k].origin.age;
                }
            }
            if (n <= 20) {
                TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_state(other.sync, BODY_SYNC_SEAT_HOST, tick_no, packet, size));
            }
            uint16_t number = host.numbers[OTHER]++;
            body_sync_state_sent(host.sync, OTHER, number);
            body_sync_resolve(host.sync, OTHER, number, false); /* lost: it goes again, older */
        }
        if (n <= 20) {
            body_sync_apply(other.sync, tick_no);
            double played = 0.0;
            TEST_ASSERT_TRUE(body_sync_playback(other.sync, ID0, &played));
            TEST_ASSERT_TRUE_MESSAGE(played <= (double)taken, "the other guest played a relayed state at the relay's time");
        }
    }
    TEST_ASSERT_TRUE(relayed >= 30);
    /* Resent every tick or two up to the oldest age the wire carries, and never past it. */
    TEST_ASSERT_TRUE_MESSAGE(newest_age + 2 >= SYNC_RELAY_AGE_MAX && newest_age <= SYNC_RELAY_AGE_MAX, "a relay stopped early or ran past its age");
}

/* On the host a guest's dynamic body is held dynamic, so it pushes and is held as the guest's
   own would be; elsewhere copies stay kinematic. */
static void test_a_guests_body_is_dynamic_on_the_host(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    for (int i = 0; i < 4; i++) {
        b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){2.0f, 0.0f, 0.0f});
        tick_all();
    }
    TEST_ASSERT_EQUAL_MESSAGE(b3_dynamicBody, b3Body_GetType(host.bodies[0]), "the host held a guest's body kinematic");
    TEST_ASSERT_EQUAL(b3_kinematicBody, b3Body_GetType(other.bodies[0]));
    TEST_ASSERT_EQUAL(b3_kinematicBody, b3Body_GetType(guest.bodies[1]));
}

/* A body already local keeps the velocity it simulated when a record moves its motion: a
   throw made while a claim is optimistic survives the host's grant, which carries motion. */
static void test_a_local_body_keeps_its_velocity_on_a_motion_take(void) {
    tick_all();
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE));
    tick_no++;
    step(&guest); /* local at once: the copy is taken over */
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){8.0f, 0.0f, 0.0f});
    records(&guest, &host);
    body_sync_claim_event_t c;
    while (body_sync_poll_claim(host.sync, &c)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, true));
    }
    records(&host, &guest);
    tick_no++;
    step(&guest);
    TEST_ASSERT_TRUE_MESSAGE(b3Body_GetLinearVelocity(guest.bodies[0]).x > 7.0f, "a motion take reset a local body's own velocity");
}

/* The host's states of a body a guest claimed optimistically, sent before the host refused,
   are what the guest's copy plays once the refusal arrives: not the snapshots from before the
   claim. */
static void test_a_copy_again_plays_what_came_while_it_was_local(void) {
    tick_all();
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE));
    b3Body_SetTransform(host.bodies[0], (b3Vec3){6.0f, 0.2f, 6.0f}, b3Quat_identity);
    for (int i = 0; i < 4; i++) { /* the host moves it and says so; the guest is still local */
        tick_no++;
        step(&host);
        step(&guest);
        state(&host, &guest);
    }
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    records(&guest, &host);
    body_sync_claim_event_t c;
    while (body_sync_poll_claim(host.sync, &c)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, false));
    }
    records(&host, &guest);
    for (int i = 0; i < 4; i++) { /* nothing more from the host: the buffer is all it has */
        tick_no++;
        step(&guest);
    }
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, ID0));
    TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[0], guest.bodies[0]) < 0.05f, "a copy again did not play the host's state");
}

/* A body a guest took and moved, a copy again before the host says anything newer, stays where
   the guest left it: it never goes back to the snapshots from before its take. */
static void test_a_copy_again_does_not_replay_old_snapshots(void) {
    tick_all();
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[0], (b3Vec3){5.0f, 0.2f, 5.0f}, b3Quat_identity);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){0.0f, 0.0f, 0.0f});
    tick_no++;
    step(&guest);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    tick_no++;
    step(&guest);
    b3Vec3 at = b3Body_GetPosition(guest.bodies[0]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(at.x - 5.0f) < 0.05f && fabsf(at.z - 5.0f) < 0.05f, "a copy went back to snapshots from before its take");
}

/* A body that was local from its bind and becomes a copy for the first time keeps its drawn
   pose: the first placement goes to the render offset. */
static void test_a_body_bound_local_keeps_its_drawn_pose_as_a_copy(void) {
    tick_all();
    tick_all();
    const body_sync_id_t id = body_sync_id_make(9, 1);
    b3Vec3 at = b3Body_GetPosition(host.bodies[9]);
    grant(&guest, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    /* The guest moves it before its first state goes out: the host's first snapshot of it is
       0.5 m from where the host draws it, and the copy's first placement is there. */
    tick_no++;
    step(&host);
    body_sync_apply(guest.sync, tick_no);
    b3Body_SetTransform(guest.bodies[9], (b3Vec3){at.x + 0.5f, at.y, at.z}, b3Quat_identity);
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    body_sync_collect(guest.sync, tick_no);
    step(&other);
    state(&host, &guest);
    state(&host, &other);
    state(&guest, &host);
    state(&other, &host);
    float last = at.x;
    for (int i = 0; i < 5; i++) { /* that snapshot plays a tick or two later; the offset gives it back */
        tick_all();
        float offset[3], turn[4];
        body_sync_render_offset(host.sync, id, offset, turn);
        float drawn = b3Body_GetPosition(host.bodies[9]).x + offset[0];
        TEST_ASSERT_TRUE_MESSAGE(fabsf(drawn - last) < 0.1f, "a body bound local jumped on its first copy entry");
        last = drawn;
    }
    for (int i = 0; i < 20; i++) {
        tick_all();
    }
    TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[9], guest.bodies[9]) < 0.05f, "the copy never reached the guest's pose");
}

/* A forced move that overtakes a claim still waiting leaves the owner's snapshots good: the
   copy plays them, not the claimant's pose. */
static void test_a_forced_move_while_the_claim_waits_keeps_the_owners_snapshots(void) {
    tick_all();
    tick_all();
    const body_sync_id_t id = body_sync_id_make(10, 1);
    b3Vec3 at = b3Body_GetPosition(host.bodies[10]);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, id, BODY_SYNC_HOLD_UNTIL_RELEASE));
    tick_no++;
    step(&guest);
    b3Body_SetTransform(guest.bodies[10], (b3Vec3){at.x + 0.6f, at.y, at.z}, b3Quat_identity);
    tick_no++;
    step(&guest);
    records(&guest, &host); /* the claim waits on the host, unanswered */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_assign(host.sync, id, OTHER, BODY_SYNC_HOLD_UNTIL_RELEASE));
    body_sync_claim_event_t c;
    while (body_sync_poll_claim(host.sync, &c)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, false));
    }
    records(&host, &guest);
    for (int i = 0; i < 3; i++) { /* nothing more from the host */
        tick_no++;
        step(&guest);
    }
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, id));
    TEST_ASSERT_TRUE_MESSAGE(fabsf(b3Body_GetPosition(guest.bodies[10]).x - at.x) < 0.05f, "the owner's snapshots were dropped");
}

static const sync_limits_t LIMITS = {{-40.0f, -15.0f, -40.0f}, {40.0f, 50.0f, 40.0f}, 100.0f, 200.0f};

/* A host packet by hand: one copy record of `slot` at x, relayed from `origin` when nonzero. */
static uint8_t forge_generation = 1;

static uint32_t forge(uint16_t slot, float x, uint8_t origin, uint8_t age) {
    static sync_codec_t codec;
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &LIMITS));
    sync_record_t r;
    memset(&r, 0, sizeof(r));
    r.slot = slot;
    r.generation_low = forge_generation;
    r.position[0] = x;
    r.position[1] = 0.2f;
    r.rotation[3] = 1.0f;
    r.origin = (sync_origin_t){.relayed = origin != 0, .seat = origin, .age = age};
    TEST_ASSERT_TRUE(sync_codec_limit(&codec, &r));
    sync_packet_writer_t w;
    sync_packet_begin(&w, packet, sizeof(packet));
    TEST_ASSERT_TRUE(sync_packet_add(&w, &codec, &r));
    return sync_packet_finish(&w);
}

/* The host takes no relayed record from a guest, and a guest none that names the host or
   itself: either would let a peer drive another's timeline. */
static void test_a_forged_relay_is_refused(void) {
    tick_all();
    uint32_t size = forge(0, 1.0f, OTHER, 3);
    TEST_ASSERT_EQUAL(BODY_SYNC_FORMAT, body_sync_receive_state(host.sync, GUEST, tick_no, packet, size));
    size = forge(0, 1.0f, GUEST, 3);
    TEST_ASSERT_EQUAL(BODY_SYNC_FORMAT, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no, packet, size));
    size = forge(0, 1.0f, OTHER, 3);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no, packet, size));
}

/* A tick far ahead of the clock is dropped and counted, and moves no copy; records the seam does
   not take (here for a body of another generation) do not feed the clock. */
static void test_only_honest_taken_records_move_a_timeline(void) {
    for (int i = 0; i < 30; i++) {
        tick_all();
    }
    const body_sync_id_t watched = body_sync_id_make(6, 1);
    double before = 0.0, after = 0.0;
    TEST_ASSERT_TRUE(body_sync_playback(guest.sync, watched, &before));
    body_sync_traffic_t t0, t1;
    body_sync_traffic(guest.sync, BODY_SYNC_SEAT_HOST, &t0);
    uint32_t size = forge(5, 3.0f, 0, 0);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no + 5000u, packet, size));
    body_sync_traffic(guest.sync, BODY_SYNC_SEAT_HOST, &t1);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(t0.rejected + 1u, t1.rejected, "a forged far tick was not counted");
    tick_no++;
    body_sync_apply(guest.sync, tick_no);
    TEST_ASSERT_TRUE(body_sync_playback(guest.sync, watched, &after));
    TEST_ASSERT_TRUE_MESSAGE(after - before < 3.0, "a forged far tick moved the timeline");
    tick_all();
    const uint32_t real_newest = tick_no;
    forge_generation = 2; /* stale: dropped and counted, never taken */
    for (int i = 0; i < 20; i++) {
        tick_no++;
        body_sync_apply(guest.sync, tick_no);
        size = forge(0, 1.0f, 0, 0);
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no, packet, size));
    }
    forge_generation = 1;
    TEST_ASSERT_TRUE(body_sync_playback(guest.sync, watched, &after));
    TEST_ASSERT_TRUE_MESSAGE(after <= (double)real_newest, "records the seam dropped moved its clock");
}

/* A claimant refused is a copy again: its drawn pose goes on from where it was, the render
   offset taking the move back to the owner's pose. */
static void test_a_copy_again_keeps_its_drawn_pose(void) {
    tick_all();
    tick_all();
    const body_sync_id_t id = body_sync_id_make(4, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, id, BODY_SYNC_HOLD_UNTIL_RELEASE));
    tick_no++;
    step(&guest);
    b3Vec3 at = b3Body_GetPosition(guest.bodies[4]);
    b3Body_SetTransform(guest.bodies[4], (b3Vec3){at.x + 0.4f, at.y, at.z}, b3Quat_identity);
    tick_no++;
    step(&guest);
    float offset[3], turn[4];
    b3Vec3 p = b3Body_GetPosition(guest.bodies[4]);
    body_sync_render_offset(guest.sync, id, offset, turn);
    float drawn_before = p.x + offset[0];
    records(&guest, &host);
    body_sync_claim_event_t c;
    while (body_sync_poll_claim(host.sync, &c)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, false));
    }
    records(&host, &guest);
    tick_all();
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, id));
    p = b3Body_GetPosition(guest.bodies[4]);
    body_sync_render_offset(guest.sync, id, offset, turn);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(p.x + offset[0] - drawn_before) < 0.05f, "the drawn pose jumped back to the owner's");
    TEST_ASSERT_TRUE(fabsf(p.x - at.x) < 0.01f); /* physics is the owner's */
}

/* The handover records carry the data bytes, as they carry flags: a grant right after the host
   set them, and a release right after the guest did, need no state packet. */
static void test_the_data_rides_in_the_handover(void) {
    tick_all();
    const body_sync_id_t id = body_sync_id_make(7, 1);
    const uint8_t mine[3] = {4, 5, 6}, theirs[2] = {9, 1};
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_data(host.sync, id, mine, 3));
    grant(&guest, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_no++;
    body_sync_apply(guest.sync, tick_no);
    uint8_t got[BODY_SYNC_DATA_MAX];
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(3, body_sync_data(guest.sync, id, got, sizeof(got)), "the grant lost the data");
    TEST_ASSERT_EQUAL_MEMORY(mine, got, 3);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_data(guest.sync, id, theirs, 2));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, id));
    records(&guest, &host);
    tick_no++;
    body_sync_apply(host.sync, tick_no);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2, body_sync_data(host.sync, id, got, sizeof(got)), "the release lost the data");
    TEST_ASSERT_EQUAL_MEMORY(theirs, got, 2);
}

/* Flags and data on a copy read as of its playback time, not as of the newest packet. */
static void test_a_copy_reads_its_groups_at_playback_time(void) {
    tick_all();
    tick_all();
    const body_sync_id_t id = body_sync_id_make(8, 1);
    const uint8_t bytes[1] = {42};
    body_sync_set_flags(host.sync, id, 5);
    body_sync_set_data(host.sync, id, bytes, 1);
    tick_all(); /* received after the guest's apply: not yet played */
    uint8_t got[BODY_SYNC_DATA_MAX];
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, body_sync_flags(guest.sync, id), "flags read as of the newest packet");
    TEST_ASSERT_EQUAL_UINT32(0, body_sync_data(guest.sync, id, got, sizeof(got)));
    tick_all();
    tick_all();
    TEST_ASSERT_EQUAL_UINT8(5, body_sync_flags(guest.sync, id));
    TEST_ASSERT_EQUAL_UINT32(1, body_sync_data(guest.sync, id, got, sizeof(got)));
}

/* A dynamic hold follows its owner: kinematic when the owner made it so, its locks as the owner
   set them (a character cannot tip over on the host). */
static void test_a_dynamic_hold_follows_its_owners_motion(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetMotionLocks(guest.bodies[0], (b3MotionLocks){.angularX = true, .angularZ = true});
    for (int i = 0; i < 4; i++) {
        b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){1.0f, 0.0f, 0.0f});
        tick_all();
    }
    TEST_ASSERT_EQUAL(b3_dynamicBody, b3Body_GetType(host.bodies[0]));
    b3MotionLocks l = b3Body_GetMotionLocks(host.bodies[0]);
    TEST_ASSERT_TRUE_MESSAGE(l.angularX && l.angularZ && !l.angularY && !l.linearX, "the dynamic hold dropped its owner's locks");
    b3Body_SetType(guest.bodies[0], b3_kinematicBody);
    for (int i = 0; i < 3; i++) {
        tick_all();
    }
    TEST_ASSERT_EQUAL_MESSAGE(b3_kinematicBody, b3Body_GetType(host.bodies[0]), "a kinematic owner's body was held dynamic");
}

/* Past the offset's reach a dynamic copy is placed, keeping its owner's velocity; a body that is
   a copy again with nothing played yet stops where it is. With max_speed this high the speed
   cap would let the correction drive it, so the 2 m rule is what places it. */
static void test_a_far_dynamic_copy_is_placed_moving(void) {
    tearDown();
    max_speed = 400.0f;
    setUp();
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    for (int i = 0; i < 4; i++) {
        b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){2.0f, 0.0f, 0.0f});
        tick_all();
    }
    b3Vec3 at = b3Body_GetPosition(guest.bodies[0]);
    b3Body_SetTransform(guest.bodies[0], (b3Vec3){at.x + 3.0f, at.y, at.z}, b3Quat_identity);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){2.0f, 0.0f, 0.0f});
    bool placed = false;
    for (int i = 0; i < 6 && !placed; i++) { /* a slow body's snapshots come every other tick */
        tick_all();
        placed = apart(host.bodies[0], guest.bodies[0]) < 0.15f;
    }
    max_speed = 100.0f;
    TEST_ASSERT_TRUE_MESSAGE(placed, "a dynamic copy 3 m off was corrected, not placed");
    TEST_ASSERT_TRUE_MESSAGE(b3Body_GetLinearVelocity(host.bodies[0]).x > 1.5f, "a placed dynamic copy was stopped");
}

/* A copy with nothing ever received (the host has no such body) is held still, whatever
   velocity it was left with. */
static void test_a_copy_with_nothing_known_stays(void) {
    b3BodyDef d = b3DefaultBodyDef();
    d.type = b3_dynamicBody;
    d.gravityScale = 0.0f;
    d.position = (b3Vec3){-5.0f, 3.0f, -5.0f};
    b3BodyId extra = b3CreateBody(guest.world, &d);
    b3ShapeDef sd = b3DefaultShapeDef();
    b3BoxHull cube = b3MakeBoxHull(0.2f, 0.2f, 0.2f);
    b3CreateHullShape(extra, &sd, &cube.base);
    body_sync_bind_t bind = {.body = extra, .body_class = BODY_SYNC_CLASS_LIGHT, .authority = BODY_SYNC_SEAT_HOST};
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_bind(guest.sync, body_sync_id_make(60, 1), &bind));
    b3Body_SetLinearVelocity(extra, (b3Vec3){3.0f, 0.0f, 0.0f});
    for (int i = 0; i < 3; i++) {
        tick_all();
    }
    TEST_ASSERT_TRUE_MESSAGE(fabsf(b3Body_GetPosition(extra).x + 5.0f) < 0.06f, "a copy with nothing to play coasted");
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
    RUN_TEST(test_a_take_over_keeps_the_drawn_pose);
    RUN_TEST(test_a_copy_is_never_driven_past_max_speed);
    RUN_TEST(test_a_relayed_state_carries_its_age);
    RUN_TEST(test_a_guests_body_is_dynamic_on_the_host);
    RUN_TEST(test_a_local_body_keeps_its_velocity_on_a_motion_take);
    RUN_TEST(test_a_copy_again_plays_what_came_while_it_was_local);
    RUN_TEST(test_a_copy_again_does_not_replay_old_snapshots);
    RUN_TEST(test_a_forged_relay_is_refused);
    RUN_TEST(test_only_honest_taken_records_move_a_timeline);
    RUN_TEST(test_a_copy_again_keeps_its_drawn_pose);
    RUN_TEST(test_the_data_rides_in_the_handover);
    RUN_TEST(test_a_copy_reads_its_groups_at_playback_time);
    RUN_TEST(test_a_dynamic_hold_follows_its_owners_motion);
    RUN_TEST(test_a_far_dynamic_copy_is_placed_moving);
    RUN_TEST(test_a_copy_with_nothing_known_stays);
    RUN_TEST(test_a_body_bound_local_keeps_its_drawn_pose_as_a_copy);
    RUN_TEST(test_a_forced_move_while_the_claim_waits_keeps_the_owners_snapshots);
    return UNITY_END();
}
