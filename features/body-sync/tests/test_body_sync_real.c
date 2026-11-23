#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "stub_wire.h"
#include "sync_codec.h"
#include "box3d/constants.h"
#include "sync_soft.h"
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
static float light_mass = 0.0f;  /* the second layer, for the tests that rebuild with it on */
static uint64_t character_category = 0; /* for the tests that rebuild with one */
static body_sync_authority_mode_t authority_mode = BODY_SYNC_AUTHORITY_HOST;
/* Velocities a test holds on bodies of a side, set after its apply as a game would. */
typedef struct drive {
    struct side *side;
    uint32_t body;
    b3Vec3 v;
} drive_t;
static drive_t drives[4];
static uint32_t drive_count;
static bool lose_guest_fates; /* the guest's packets arrive, but it hears each lost */
static uint32_t playback_delay = 1; /* ticks; the harness delivers every packet in its tick */
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
                                 .light_mass_max = light_mass,
                                 .character_category = character_category,
                                 .authority = authority_mode,
                                 .max_spin = 200.0f,
                                 .delay_min = playback_delay,
                                 .delay_max = playback_delay};
    s->sync = body_sync_create(&config);
    TEST_ASSERT_NOT_NULL(s->sync);
    for (uint32_t i = 0; i < BODIES; i++) {
        b3BodyDef d = b3DefaultBodyDef();
        d.type = b3_dynamicBody;
        d.position = (b3Vec3){(float)(i % 8) * 1.0f, 0.2f, (float)(i / 8) * 1.0f};
        s->bodies[i] = b3CreateBody(s->world, &d);
        b3CreateHullShape(s->bodies[i], &shape, &cube.base);
        /* The last body is a character: sent every packet whatever it does. */
        body_sync_bind_t bind = {.body = s->bodies[i], .body_class = i == BODIES - 1 ? BODY_SYNC_CLASS_CHARACTER : i == BODIES - 2 ? BODY_SYNC_CLASS_HEAVY : BODY_SYNC_CLASS_LIGHT,
                                 .authority = BODY_SYNC_SEAT_HOST};
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_bind(s->sync, body_sync_id_make(i, 1), &bind));
    }
}

void setUp(void) {
    b3SetAssertFcn(count_assert);
    box3d_asserts = 0;
    drive_count = 0;
    lose_guest_fates = false;
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
/* Packet fates come back this many ticks after the send (a link's round trip); 0 at once. */
static uint32_t fate_delay = 0;
static struct {
    side_t *from;
    uint8_t seat;
    uint16_t number;
    uint32_t due;
} fates[256];
static uint32_t fate_count;

static void state(side_t *from, side_t *to) {
    uint32_t size = 0;
    for (uint32_t i = 0; i < fate_count;) {
        if (fates[i].from == from && fates[i].seat == to->seat && fates[i].due <= tick_no) {
            body_sync_resolve(from->sync, fates[i].seat, fates[i].number, true);
            fates[i] = fates[--fate_count];
        } else {
            i++;
        }
    }
    if (body_sync_write_state(from->sync, to->seat, packet, 1200, &size)) {
        TEST_ASSERT_TRUE(size <= 1000);
        TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_OK, body_sync_receive_state(to->sync, from->seat, tick_no, packet, size), body_sync_last_error(to->sync));
        uint16_t n = from->numbers[to->seat]++;
        body_sync_state_sent(from->sync, to->seat, n);
        if (fate_delay == 0 || fate_count == 256) {
            body_sync_resolve(from->sync, to->seat, n, !(lose_guest_fates && from == &guest));
        } else {
            fates[fate_count].from = from;
            fates[fate_count].seat = to->seat;
            fates[fate_count].number = n;
            fates[fate_count].due = tick_no + fate_delay;
            fate_count++;
        }
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
    for (uint32_t k = 0; k < drive_count; k++) {
        if (drives[k].side == s) {
            b3Body_SetLinearVelocity(s->bodies[drives[k].body], drives[k].v);
        }
    }
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

static float apart_from(b3BodyId body, b3Vec3 q) {
    b3Vec3 p = b3Body_GetPosition(body);
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
    TEST_ASSERT_EQUAL_STRING("real 0.8.0", body_sync_implementation());
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
   copy showed it: the render offset holds the difference, and physics never sees it. It happens
   at the claim, so the caller's own moves from then on are never undone. */
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
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_assign(host.sync, id, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE));
    records(&host, &guest);
    body_sync_apply(guest.sync, ++tick_no);
    b3Vec3 p = b3Body_GetPosition(guest.bodies[2]);
    TEST_ASSERT_TRUE(body_sync_render_offset(guest.sync, id, offset, turn));
    /* The host's copy records carry 1/512 m. */
    TEST_ASSERT_TRUE_MESSAGE(fabsf(p.x - newest.x) <= SYNC_COPY_POSITION_STEP && fabsf(p.y - newest.y) <= SYNC_COPY_POSITION_STEP,
                             "physics did not continue from the newest state");
    TEST_ASSERT_TRUE_MESSAGE(fabsf(p.x + offset[0] - drawn.x) < 1e-4f && fabsf(p.y + offset[1] - drawn.y) < 1e-4f, "the drawn pose jumped");
    TEST_ASSERT_TRUE(fabsf(offset[0]) > 0.05f); /* the copy played a tick behind the newest */
}

/* A claim takes over from the pose the claimant shows, as a take does: a moving copy is not put
   ahead to the newest state, and its render offset is not grown. */
static void test_a_claim_goes_on_from_the_pose_the_claimant_shows(void) {
    tick_all();
    tick_all();
    for (int i = 0; i < 6; i++) {
        b3Body_SetLinearVelocity(host.bodies[2], (b3Vec3){6.0f, 0.0f, 0.0f});
        tick_all();
    }
    const body_sync_id_t id = body_sync_id_make(2, 1);
    b3Vec3 shown = b3Body_GetPosition(guest.bodies[2]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(b3Body_GetPosition(host.bodies[2]).x - shown.x) > 0.05f, "the copy played its owner's present");
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, id, BODY_SYNC_HOLD_UNTIL_RELEASE));
    TEST_ASSERT_TRUE_MESSAGE(apart_from(guest.bodies[2], shown) < 1e-4f, "the claim moved the body");
}

/* What the caller does with a body it just claimed stays done: a grab carries it to the hand and
   makes it kinematic before the next apply, which must neither put it back at the newest state
   nor leave that jump in the render offset (the drawn body would overshoot the hand). */
static void test_a_claimed_body_keeps_what_the_caller_did_to_it(void) {
    tick_all();
    tick_all();
    const body_sync_id_t id = body_sync_id_make(2, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, id, BODY_SYNC_HOLD_UNTIL_RELEASE));
    const b3Vec3 hand = {2.0f, 1.0f, 1.5f};
    b3Body_SetType(guest.bodies[2], b3_kinematicBody);
    b3Body_SetTransform(guest.bodies[2], hand, b3Quat_identity);
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    body_sync_collect(guest.sync, tick_no);
    records(&guest, &host);
    body_sync_claim_event_t c;
    while (body_sync_poll_claim(host.sync, &c)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, true));
    }
    records(&host, &guest);
    body_sync_apply(guest.sync, ++tick_no);
    TEST_ASSERT_EQUAL_MESSAGE(b3_kinematicBody, b3Body_GetType(guest.bodies[2]), "the apply undid the caller's motion type");
    TEST_ASSERT_TRUE_MESSAGE(apart_from(guest.bodies[2], hand) < 1e-4f, "the apply put the body back");
    float offset[3], turn[4];
    bool has = body_sync_render_offset(guest.sync, id, offset, turn);
    TEST_ASSERT_TRUE_MESSAGE(!has || sqrtf(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]) < 1e-3f, "the drawn body overshoots the hand");
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
   the guest left it: it never goes back to the snapshots from before its take, and reports no
   playback, before the apply that drives it as a copy nor after it: those snapshots are no
   picture of what it shows. */
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
    double played;
    TEST_ASSERT_FALSE_MESSAGE(body_sync_playback(guest.sync, ID0, &played), "a copy not yet driven reported a playback");
    tick_no++;
    step(&guest);
    b3Vec3 at = b3Body_GetPosition(guest.bodies[0]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(at.x - 5.0f) < 0.05f && fabsf(at.z - 5.0f) < 0.05f, "a copy went back to snapshots from before its take");
    TEST_ASSERT_FALSE_MESSAGE(body_sync_playback(guest.sync, ID0, &played), "a copy with only snapshots from before its take reported a playback");
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

static void layer_with(float kg) {
    tearDown();
    light_mass = kg;
    setUp();
    light_mass = 0.0f;
}

static void layer_on(void) { layer_with(100.0f); /* the suite's cubes weigh 64 kg */ }

/* One guest tick with the guest's own body 1 driven along a velocity before its step. */
static void tick_driving(b3Vec3 v, int *asleep_after_apply) {
    tick_no++;
    step(&host);
    body_sync_apply(guest.sync, tick_no);
    *asleep_after_apply += b3Body_IsAwake(guest.bodies[1]) ? 0 : 1;
    b3Body_SetLinearVelocity(guest.bodies[1], v);
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    body_sync_collect(guest.sync, tick_no);
    step(&other);
    state(&host, &guest);
    state(&guest, &host);
    records(&guest, &host);
    records(&host, &guest);
}

/* Which copies the layer holds soft: on a guest, light ones of the host's with no holder, not
   heavy, not characters; never on the host; none with the knob at 0. */
static void test_the_layer_holds_only_free_light_copies_on_a_guest(void) {
    tick_all();
    tick_all();
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_predicted(guest.sync, body_sync_id_make(5, 1)), "the layer is on with the knob at 0");
    layer_on();
    tick_all();
    tick_all();
    TEST_ASSERT_TRUE(body_sync_is_predicted(guest.sync, body_sync_id_make(5, 1)));
    TEST_ASSERT_EQUAL(b3_dynamicBody, b3Body_GetType(guest.bodies[5]));
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_predicted(guest.sync, body_sync_id_make(BODIES - 2, 1)), "a heavy copy was held soft");
    TEST_ASSERT_FALSE(body_sync_is_predicted(guest.sync, body_sync_id_make(BODIES - 1, 1)));
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    tick_all();
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_predicted(host.sync, ID0), "the host held a copy soft");
    TEST_ASSERT_TRUE(body_sync_is_predicted(other.sync, body_sync_id_make(5, 1)));
    body_sync_set_holder(host.sync, body_sync_id_make(6, 1), body_sync_id_make(BODIES - 1, 1));
    for (int i = 0; i < 3; i++) {
        tick_all();
    }
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_predicted(guest.sync, body_sync_id_make(6, 1)), "a held copy was held soft");
}

/* A soft copy answers this guest's own body in the same step; the host has not moved it yet. */
static void test_a_soft_copy_answers_the_guests_own_body_at_once(void) {
    layer_on();
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    tick_all();
    b3Vec3 target = b3Body_GetPosition(guest.bodies[1]); /* one metre along x */
    b3Body_SetTransform(guest.bodies[0], (b3Vec3){target.x - 0.45f, 0.2f, target.z}, b3Quat_identity);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){6.0f, 0.0f, 0.0f});
    float moved = 0.0f;
    for (int i = 0; i < 6 && moved < 0.01f; i++) {
        tick_no++;
        step(&guest);
        moved = b3Body_GetPosition(guest.bodies[1]).x - target.x;
    }
    TEST_ASSERT_TRUE_MESSAGE(moved >= 0.01f, "a soft copy stood like a wall");
    TEST_ASSERT_TRUE(fabsf(b3Body_GetPosition(host.bodies[1]).x - target.x) < 1e-4f);
}

/* At rest the layer agrees exactly and never jitters: 600 ticks of a sleeping world, every guest
   copy asleep and still, on the host's pose. */
static void test_the_layer_at_rest_is_exact_and_still(void) {
    layer_on();
    for (int i = 0; i < 300; i++) {
        tick_all();
    }
    b3Vec3 before[BODIES];
    for (uint32_t i = 0; i < BODIES - 1; i++) {
        before[i] = b3Body_GetPosition(guest.bodies[i]);
    }
    for (int i = 0; i < 600; i++) {
        tick_all();
    }
    for (uint32_t i = 0; i < BODIES - 1; i++) {
        b3Vec3 now = b3Body_GetPosition(guest.bodies[i]);
        TEST_ASSERT_FALSE_MESSAGE(b3Body_IsAwake(guest.bodies[i]), "a copy at rest was woken");
        TEST_ASSERT_TRUE_MESSAGE(now.x == before[i].x && now.y == before[i].y && now.z == before[i].z, "a copy at rest moved");
        TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[i], guest.bodies[i]) <= 0.5f * SYNC_SIM_POSITION_STEP * 1.8f, "a copy at rest is off the host's pose");
    }
}

/* The host moving a body wakes its soft copy and pulls it there. */
static void test_a_host_move_pulls_the_soft_copy(void) {
    layer_on();
    for (int i = 0; i < 120; i++) {
        tick_all();
    }
    b3Vec3 at = b3Body_GetPosition(host.bodies[5]);
    b3Body_SetTransform(host.bodies[5], (b3Vec3){at.x + 0.3f, at.y, at.z}, b3Quat_identity);
    b3Body_SetAwake(host.bodies[5], true); /* a set transform alone leaves it asleep, unsent */
    for (int i = 0; i < 120; i++) {
        tick_all();
    }
    TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[5], guest.bodies[5]) < SYNC_SOFT_DEAD_M, "a soft copy did not follow its owner");
}

/* A grab and a release of a soft copy keep its pose: it was dynamic and simulated here already. */
static void test_a_soft_copy_taken_and_released_does_not_jump(void) {
    layer_on();
    for (int i = 0; i < 60; i++) {
        tick_all();
    }
    const body_sync_id_t id = body_sync_id_make(7, 1);
    b3Body_SetLinearVelocity(guest.bodies[7], (b3Vec3){0.5f, 0.0f, 0.0f}); /* this side's own prediction moved it */
    tick_no++;
    step(&guest);
    b3Vec3 before = b3Body_GetPosition(guest.bodies[7]);
    grant(&guest, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_no++;
    body_sync_apply(guest.sync, tick_no);
    b3Vec3 now = b3Body_GetPosition(guest.bodies[7]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(now.x - before.x) < 1e-5f, "the take placed a soft copy");
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    body_sync_collect(guest.sync, tick_no);
    before = b3Body_GetPosition(guest.bodies[7]);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, id));
    tick_no++;
    body_sync_apply(guest.sync, tick_no);
    now = b3Body_GetPosition(guest.bodies[7]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(now.x - before.x) < 1e-5f, "the release placed it");
    TEST_ASSERT_EQUAL(b3_dynamicBody, b3Body_GetType(guest.bodies[7]));
}

/* A soft copy whose owner takes flight turns driven by a placement: a kinematic sweep from where
   this side's prediction left it would bat the soft copies on its way. */
static void test_a_copy_taking_flight_is_placed_not_swept(void) {
    layer_on();
    for (int i = 0; i < 120; i++) {
        tick_all();
    }
    b3Vec3 at = b3Body_GetPosition(guest.bodies[5]);
    b3Body_SetTransform(guest.bodies[5], (b3Vec3){at.x, at.y, at.z + 0.8f}, b3Quat_identity); /* this side's prediction */
    b3Body_SetAwake(host.bodies[5], true);
    b3Body_SetLinearVelocity(host.bodies[5], (b3Vec3){0.0f, 0.0f, -8.0f}); /* away from its neighbours along x */
    bool driven = false;
    for (int i = 0; i < 60; i++) {
        tick_all();
        if (b3Body_GetType(guest.bodies[5]) == b3_kinematicBody) {
            driven = true;
            b3Vec3 v = b3Body_GetLinearVelocity(guest.bodies[5]);
            TEST_ASSERT_TRUE_MESSAGE(sqrtf(v.x * v.x + v.y * v.y + v.z * v.z) < 12.0f, "the copy swept to its owner's path");
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(driven, "a flying owner's copy stayed soft");
}

/* The layer never puts a body to sleep: this guest's own body sliding along a copy at rest is
   awake after every apply (forcing a copy asleep sleeps its whole island). */
static void test_a_local_body_sliding_on_a_resting_copy_stays_awake(void) {
    layer_on();
    for (int i = 0; i < 90; i++) {
        tick_all();
    }
    grant(&guest, body_sync_id_make(1, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){0.401f, 0.2f, -0.3f}, b3Quat_identity);
    b3Body_SetLinearVelocity(guest.bodies[1], (b3Vec3){0.0f, 0.0f, 0.15f}); /* sliding from the first tick */
    int asleep = 0;
    for (int i = 0; i < 60; i++) {
        tick_driving((b3Vec3){0.0f, 0.0f, 0.15f}, &asleep);
    }
    /* The first apply after the take finds it as the take left it, layer or not. */
    TEST_ASSERT_TRUE_MESSAGE(asleep <= 1, "the layer put this side's own sliding body to sleep");
}

/* A push from standstill: this side's body, stopped against a copy in the rest hold, pushes it
   from the next step on, at a walk and slower, as far as it pushes a body of its own; the host's
   body follows. */
static float push_for_two_seconds(float speed, bool own_both) {
    layer_on();
    for (int i = 0; i < 90; i++) {
        tick_all();
    }
    grant(&guest, body_sync_id_make(1, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    if (own_both) {
        grant(&guest, body_sync_id_make(0, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    }
    tick_all();
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){0.401f, 0.2f, 0.0f}, b3Quat_identity);
    int asleep = 0;
    float jump = 0.0f, prev = 0.0f;
    for (int i = 0; i < 120; i++) {
        tick_driving((b3Vec3){-speed, 0.0f, 0.0f}, &asleep);
        float offset[3], turn[4];
        float now = body_sync_render_offset(guest.sync, body_sync_id_make(0, 1), offset, turn) ? sqrtf(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]) : 0.0f;
        jump = now - prev > jump ? now - prev : jump;
        prev = now;
    }
    /* Pressed for two seconds, past the pause's chain: the stall waits, nothing places it. */
    TEST_ASSERT_TRUE_MESSAGE(jump < 0.002f, "a pressed copy was placed");
    return -b3Body_GetPosition(guest.bodies[0]).x;
}

static void push_from_standstill(float speed) {
    float own = push_for_two_seconds(speed, true);
    TEST_ASSERT_TRUE(own > 0.01f);
    float copy = push_for_two_seconds(speed, false);
    TEST_ASSERT_TRUE_MESSAGE(copy > 0.7f * own, "a copy in the rest hold stood against a push from standstill");
    TEST_ASSERT_TRUE_MESSAGE(-b3Body_GetPosition(host.bodies[0]).x > 0.5f * own, "the host's body never followed");
}

static void test_a_push_from_standstill_at_a_walk(void) { push_from_standstill(0.5f); }
static void test_a_push_from_standstill_slower_than_a_lean_mark(void) { push_from_standstill(0.2f); }

/* A copy its owner holds until settled (a throw in flight) or moves kinematically is never soft. */
static void test_no_soft_hold_for_a_settling_or_kinematic_owner(void) {
    layer_on();
    tick_all();
    grant(&other, body_sync_id_make(4, 1), BODY_SYNC_HOLD_UNTIL_SETTLED);
    b3Body_SetLinearVelocity(other.bodies[4], (b3Vec3){0.5f, 3.0f, 0.0f});
    for (int i = 0; i < 4; i++) {
        tick_all();
    }
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_predicted(guest.sync, body_sync_id_make(4, 1)), "a body held until settled was soft");
    b3Body_SetType(host.bodies[6], b3_kinematicBody);
    b3Body_SetLinearVelocity(host.bodies[6], (b3Vec3){0.3f, 0.0f, 0.0f});
    for (int i = 0; i < 4; i++) {
        tick_all();
    }
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_predicted(guest.sync, body_sync_id_make(6, 1)), "a kinematic owner's copy was soft");
}

/* A character standing still against a copy does not keep it from its owner: moved 3 cm on the
   host, the copy is within 1 cm 40 ticks later. */
static void test_a_leaning_character_does_not_hold_a_copy_off(void) {
    layer_on();
    for (int i = 0; i < 60; i++) {
        tick_all();
    }
    grant(&guest, body_sync_id_make(1, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){0.401f, 0.2f, 0.0f}, b3Quat_identity); /* standing at body 0 */
    b3Vec3 at = b3Body_GetPosition(host.bodies[0]);
    b3Body_SetTransform(host.bodies[0], (b3Vec3){at.x - 0.03f, at.y, at.z}, b3Quat_identity);
    b3Body_SetAwake(host.bodies[0], true);
    int asleep = 0;
    for (int i = 0; i < 40; i++) {
        tick_driving((b3Vec3){0.0f, 0.0f, 0.0f}, &asleep);
    }
    TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[0], guest.bodies[0]) < 0.01f, "a lean held the copy off its owner");
}

/* A soft copy this side pushed, then took, keeps its own pose: the take never places it on the
   owner's older one. */
static void test_a_pushed_soft_copy_taken_keeps_its_pose(void) {
    layer_on();
    for (int i = 0; i < 4; i++) {
        tick_all();
    }
    const body_sync_id_t id = body_sync_id_make(7, 1);
    TEST_ASSERT_TRUE(body_sync_is_predicted(guest.sync, id));
    b3Vec3 at = b3Body_GetPosition(guest.bodies[7]);
    b3Body_SetTransform(guest.bodies[7], (b3Vec3){at.x + 0.05f, at.y, at.z}, b3Quat_identity);
    b3Vec3 before = b3Body_GetPosition(guest.bodies[7]);
    grant(&guest, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_no++;
    body_sync_apply(guest.sync, tick_no);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(b3Body_GetPosition(guest.bodies[7]).x - before.x) < 1e-5f, "the take placed the soft copy on its owner");
}

/* A body this side releases goes on from its own simulation: no placement on the owner's older
   pose, though that is 10 cm away. */
static void test_a_released_body_goes_on_from_its_own_pose(void) {
    layer_on();
    tick_all();
    const body_sync_id_t id = body_sync_id_make(9, 1);
    grant(&guest, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Vec3 at = b3Body_GetPosition(guest.bodies[9]);
    b3Body_SetTransform(guest.bodies[9], (b3Vec3){at.x, at.y, at.z + 0.10f}, b3Quat_identity);
    tick_no++;
    step(&guest);
    b3Vec3 before = b3Body_GetPosition(guest.bodies[9]);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, id));
    tick_no++;
    body_sync_apply(guest.sync, tick_no);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(b3Body_GetPosition(guest.bodies[9]).z - before.z) < 1e-5f, "the release placed the body");
    TEST_ASSERT_TRUE(body_sync_is_predicted(guest.sync, id));
}

/* A hit by this side's body pauses the pull: the copy stays where the hit sent it for the pause,
   then comes back to its owner. */
static void test_a_hit_pauses_the_pull(void) {
    layer_on();
    for (int i = 0; i < 60; i++) {
        tick_all();
    }
    grant(&guest, body_sync_id_make(1, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){0.8f, 0.2f, 0.0f}, b3Quat_identity);
    int asleep = 0;
    for (int i = 0; i < 7; i++) {
        tick_driving((b3Vec3){-6.0f, 0.0f, 0.0f}, &asleep); /* into body 0 and on, already moving as it comes */
    }
    for (int i = 0; i < 3; i++) {
        tick_driving((b3Vec3){0.0f, 0.0f, 0.0f}, &asleep);
    }
    float away = apart(host.bodies[0], guest.bodies[0]);
    TEST_ASSERT_TRUE_MESSAGE(away > 0.05f, "the hit did not move the copy");
    for (int i = 0; i < 15; i++) {
        tick_driving((b3Vec3){0.0f, 0.0f, 0.0f}, &asleep);
        TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[0], guest.bodies[0]) > 0.5f * away, "the pull ran inside the pause");
    }
    for (int i = 0; i < 240; i++) {
        tick_all();
    }
    TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[0], guest.bodies[0]) < 0.01f, "the copy never came back");
}

/* A layer copy keeps this side's character bits, so its own character meets it; with the layer
   off a LIGHT copy loses them. */
static void test_a_layer_copy_keeps_the_character_bits(void) {
    character_category = 0x2;
    layer_on();
    tick_all();
    tick_all();
    b3ShapeId shape;
    b3Body_GetShapes(guest.bodies[5], &shape, 1);
    TEST_ASSERT_TRUE_MESSAGE((b3Shape_GetFilter(shape).maskBits & 0x2) != 0, "a layer copy lost the character bits");
    layer_with(0.0f);
    tick_all();
    tick_all();
    b3Body_GetShapes(guest.bodies[5], &shape, 1);
    character_category = 0;
    TEST_ASSERT_TRUE_MESSAGE((b3Shape_GetFilter(shape).maskBits & 0x2) == 0, "a driven LIGHT copy kept the character bits");
}

/* The mass threshold: the suite's 64 kg cubes are under 100 kg, over 50 kg. */
static void test_the_layer_mass_threshold(void) {
    layer_with(50.0f);
    tick_all();
    tick_all();
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_predicted(guest.sync, body_sync_id_make(5, 1)), "a copy over the threshold was soft");
    layer_with(64.0f);
    tick_all();
    tick_all();
    TEST_ASSERT_TRUE_MESSAGE(body_sync_is_predicted(guest.sync, body_sync_id_make(5, 1)), "a copy at the threshold was not soft");
}

/* light_mass_max out of range refuses the seam. */
static void test_a_bad_light_mass_is_refused(void) {
    const float bad[] = {-1.0f, NAN, INFINITY, -INFINITY, 1.1e6f};
    b3WorldDef wd = b3DefaultWorldDef();
    b3WorldId world = b3CreateWorld(&wd);
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        body_sync_config_t config = {.world = world, .seat = 0, .seats = SEATS, .tick_hz = 60, .bounds_min = {-40.0f, -15.0f, -40.0f},
                                     .bounds_max = {40.0f, 50.0f, 40.0f}, .max_speed = 100.0f, .max_spin = 200.0f, .light_mass_max = bad[i]};
        body_sync_t *sync = body_sync_create(&config);
        TEST_ASSERT_NULL_MESSAGE(sync, "a bad light_mass_max was taken");
        if (sync) {
            body_sync_destroy(sync);
        }
    }
    b3DestroyWorld(world);
}

/* One guest tick with its own body 5 driven along a velocity before its step. */
static void tick_driving5(b3Vec3 v, int *asleep_after_apply) {
    tick_no++;
    step(&host);
    body_sync_apply(guest.sync, tick_no);
    *asleep_after_apply += b3Body_IsAwake(guest.bodies[5]) ? 0 : 1;
    b3Body_SetLinearVelocity(guest.bodies[5], v);
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    body_sync_collect(guest.sync, tick_no);
    step(&other);
    state(&host, &guest);
    state(&guest, &host);
    records(&guest, &host);
    records(&host, &guest);
}

/* One guest tick with its own body 9 spinning in place, before the step. */
static void tick_spinning(void) {
    tick_no++;
    step(&host);
    body_sync_apply(guest.sync, tick_no);
    b3Body_SetAngularVelocity(guest.bodies[9], (b3Vec3){0.0f, 8.0f, 0.0f});
    b3Body_SetLinearVelocity(guest.bodies[9], (b3Vec3){0.0f, 0.0f, 0.0f});
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    body_sync_collect(guest.sync, tick_no);
    step(&other);
    state(&host, &guest);
    state(&guest, &host);
    records(&guest, &host);
    records(&host, &guest);
}

/* A soft copy struck hard by another copy (no body of this side's touches it, so only the hit
   can pause it) coasts through the pause unsteered, and the pause is the measured one. */
static void struck_by_a_copy(uint32_t delay) {
    fate_delay = delay;
    fate_count = 0;
    layer_on();
    grant(&guest, body_sync_id_make(9, 1), BODY_SYNC_HOLD_UNTIL_RELEASE); /* the guest sends while it holds it */
    for (int i = 0; i < 120; i++) {
        tick_all();
    }
    /* Spinning between copies 0 and 1, body 9 marks both soft without touching them. */
    b3Body_SetTransform(guest.bodies[9], (b3Vec3){0.5f, 0.2f, 0.55f}, b3Quat_identity);
    for (int i = 0; i < 3; i++) {
        tick_spinning();
    }
    TEST_ASSERT_TRUE(body_sync_is_predicted(guest.sync, body_sync_id_make(0, 1)));
    TEST_ASSERT_TRUE(body_sync_is_predicted(guest.sync, body_sync_id_make(1, 1)));
    tick_no++;
    step(&host);
    body_sync_apply(guest.sync, tick_no);
    b3Body_SetAngularVelocity(guest.bodies[9], (b3Vec3){0.0f, 8.0f, 0.0f});
    b3Body_SetLinearVelocity(guest.bodies[1], (b3Vec3){-8.0f, 0.0f, 0.0f}); /* copy 1 flung at copy 0 */
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    body_sync_collect(guest.sync, tick_no);
    for (int i = 0; i < 8; i++) {
        tick_spinning();
    }
    float hit = -b3Body_GetLinearVelocity(guest.bodies[0]).x;
    TEST_ASSERT_TRUE_MESSAGE(hit > 1.0f, "copy 0 was not struck");
    for (int i = 0; i < 3; i++) {
        tick_spinning();
    }
    /* Friction alone takes 0.1 m/s a tick; a pull would steer it back at once. */
    TEST_ASSERT_TRUE_MESSAGE(-b3Body_GetLinearVelocity(guest.bodies[0]).x > hit - 0.5f, "a struck copy was steered inside the pause");
    uint32_t pause = sync_soft_pause(body_sync_round_trip(guest.sync, BODY_SYNC_SEAT_HOST), 1);
    float away = apart(host.bodies[0], guest.bodies[0]);
    for (uint32_t i = 12; i + 2u < pause; i++) {
        tick_spinning();
        float now = apart(host.bodies[0], guest.bodies[0]);
        TEST_ASSERT_TRUE_MESSAGE(now > away - 0.005f, "a struck copy was pulled before the pause ran out");
        away = now > away ? now : away;
    }
    fate_delay = 0;
    fate_count = 0;
}

static void test_a_struck_copy_is_not_pulled_in_the_pause(void) { struck_by_a_copy(0); }

static void test_a_struck_copy_waits_the_measured_round_trip(void) {
    struck_by_a_copy(40);
    TEST_ASSERT_TRUE(fabsf(body_sync_round_trip(guest.sync, BODY_SYNC_SEAT_HOST) - 40.0f) <= 2.0f);
}

/* With the character's category set, a soft copy keeps it: this side's body running into one is
   slowed by it, never passes. */
static void test_this_sides_body_is_slowed_by_a_soft_copy(void) {
    character_category = 0x1;
    layer_on();
    character_category = 0;
    tick_all();
    grant(&guest, body_sync_id_make(1, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){0.6f, 0.2f, 0.0f}, b3Quat_identity);
    int asleep = 0;
    for (int i = 0; i < 10; i++) {
        tick_driving((b3Vec3){-2.0f, 0.0f, 0.0f}, &asleep);
    }
    TEST_ASSERT_TRUE_MESSAGE(b3Body_GetPosition(guest.bodies[1]).x - b3Body_GetPosition(guest.bodies[0]).x > 0.39f, "this side's body passed through a soft copy");
}

/* With the character's category set, a copy in the rest hold keeps it: a slow walk into a resting
   copy meets it, it does not walk into it. */
static void test_a_slow_walk_meets_a_resting_copy(void) {
    character_category = 0x1;
    layer_on();
    character_category = 0;
    for (int i = 0; i < 90; i++) {
        tick_all();
    }
    grant(&guest, body_sync_id_make(1, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){0.45f, 0.2f, 0.0f}, b3Quat_identity);
    int asleep = 0;
    for (int i = 0; i < 30; i++) {
        tick_driving((b3Vec3){-0.1f, 0.0f, 0.0f}, &asleep);
        TEST_ASSERT_TRUE_MESSAGE(b3Body_GetPosition(guest.bodies[1]).x - b3Body_GetPosition(guest.bodies[0]).x > 0.39f, "a slow walk went into a resting copy");
    }
}

/* Marks spread through contacts: pushing the near box of a touching row turns the far box soft
   at once, beyond the pusher's own reach. */
static void test_marks_spread_through_a_touching_row(void) {
    layer_on();
    b3Body_SetTransform(host.bodies[2], (b3Vec3){-0.4f, 0.2f, 0.0f}, b3Quat_identity);
    b3Body_SetTransform(host.bodies[3], (b3Vec3){-0.8f, 0.2f, 0.0f}, b3Quat_identity);
    b3Body_SetAwake(host.bodies[2], true);
    b3Body_SetAwake(host.bodies[3], true);
    for (int i = 0; i < 150; i++) {
        tick_all();
    }
    TEST_ASSERT_FALSE(body_sync_is_predicted(guest.sync, body_sync_id_make(3, 1)));
    grant(&guest, body_sync_id_make(1, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){0.5f, 0.2f, 0.0f}, b3Quat_identity);
    b3Body_SetLinearVelocity(guest.bodies[1], (b3Vec3){-1.0f, 0.0f, 0.0f});
    int asleep = 0;
    bool far_soft = false;
    for (int i = 0; i < 4 && !far_soft; i++) {
        tick_driving((b3Vec3){-1.0f, 0.0f, 0.0f}, &asleep);
        far_soft = body_sync_is_predicted(guest.sync, body_sync_id_make(3, 1));
    }
    TEST_ASSERT_TRUE_MESSAGE(far_soft, "the far box of a touching row stayed a wall");
}

/* A soft copy back inside the dead zone joins the rest hold exactly on its owner's pose, with
   nothing for the offset to carry. */
static void test_the_rest_hold_is_joined_exactly(void) {
    layer_on();
    tick_all();
    tick_all();
    b3Vec3 at = b3Body_GetPosition(guest.bodies[5]);
    b3Body_SetTransform(guest.bodies[5], (b3Vec3){at.x + 0.005f, at.y, at.z}, b3Quat_identity);
    for (int i = 0; i < 120 && body_sync_is_predicted(guest.sync, body_sync_id_make(5, 1)); i++) {
        tick_all(); /* stops on the tick it joins the hold, before any offset could decay */
    }
    TEST_ASSERT_FALSE(body_sync_is_predicted(guest.sync, body_sync_id_make(5, 1)));
    TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[5], guest.bodies[5]) <= SYNC_SOFT_HOLD_M, "the rest hold is off its owner's pose"); /* a copy record's grid */
    float offset[3], turn[4];
    TEST_ASSERT_TRUE(body_sync_render_offset(guest.sync, body_sync_id_make(5, 1), offset, turn));
    TEST_ASSERT_TRUE_MESSAGE(fabsf(offset[0]) + fabsf(offset[1]) + fabsf(offset[2]) < 1e-4f, "joining the rest hold left an offset");
}

/* On a 670 ms link the seam measures the round trip from the fates, and the layer's pause covers
   it and the playback delay: a body this side releases is not pulled for that long. */
static void test_the_pause_scales_with_the_measured_round_trip(void) {
    fate_delay = 40; /* well past the pause's floor */
    fate_count = 0;
    layer_on();
    const body_sync_id_t id = body_sync_id_make(9, 1);
    grant(&guest, id, BODY_SYNC_HOLD_UNTIL_RELEASE); /* the guest sends while it holds a body */
    for (int i = 0; i < 120; i++) {
        tick_all();
    }
    float rtt = body_sync_round_trip(guest.sync, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(rtt - 40.0f) <= 2.0f, "the measured round trip is off the link's");
    b3Vec3 at = b3Body_GetPosition(guest.bodies[9]);
    b3Body_SetTransform(guest.bodies[9], (b3Vec3){at.x, at.y, at.z + 0.3f}, b3Quat_identity);
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, id));
    float off = apart(host.bodies[9], guest.bodies[9]);
    uint32_t pause = sync_soft_pause(rtt, 1); /* this harness's playback delay is 1 tick */
    TEST_ASSERT_TRUE(pause > SYNC_SOFT_PAUSE_TICKS);
    for (uint32_t i = 0; i + 2u < pause; i++) {
        tick_all();
        TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[9], guest.bodies[9]) > off - 0.01f, "a released body was pulled before the measured pause ran out");
    }
    for (int i = 0; i < 120; i++) {
        tick_all();
    }
    TEST_ASSERT_TRUE_MESSAGE(apart(host.bodies[9], guest.bodies[9]) < 0.01f, "the released body never came back");
    fate_delay = 0;
    fate_count = 0;
}

/* A press reaches through a touching row: pushed slower than a mover marks, the far box turns soft
   too, never a wall behind the near one. */
static void test_a_press_reaches_through_a_touching_row(void) {
    layer_on();
    b3Body_SetTransform(host.bodies[2], (b3Vec3){-0.4f, 0.2f, 0.0f}, b3Quat_identity);
    b3Body_SetTransform(host.bodies[3], (b3Vec3){-0.8f, 0.2f, 0.0f}, b3Quat_identity);
    b3Body_SetAwake(host.bodies[2], true);
    b3Body_SetAwake(host.bodies[3], true);
    for (int i = 0; i < 150; i++) {
        tick_all();
    }
    grant(&guest, body_sync_id_make(1, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){0.401f, 0.2f, 0.0f}, b3Quat_identity);
    int asleep = 0;
    bool far_soft = false;
    for (int i = 0; i < 6 && !far_soft; i++) {
        tick_driving((b3Vec3){-0.2f, 0.0f, 0.0f}, &asleep);
        far_soft = body_sync_is_predicted(guest.sync, body_sync_id_make(3, 1));
    }
    TEST_ASSERT_TRUE_MESSAGE(far_soft, "a press did not reach the far box of a touching row");
}

/* Category 0x2 for every body, 0x1 (the character category) for the guest's walker, body 5. */
static void give_categories(void) {
    side_t *sides[3] = {&host, &guest, &other};
    for (int k = 0; k < 3; k++) {
        for (uint32_t b = 0; b < BODIES; b++) {
            b3ShapeId shape;
            b3Body_GetShapes(sides[k]->bodies[b], &shape, 1);
            b3Filter f = b3Shape_GetFilter(shape);
            f.categoryBits = sides[k] == &guest && b == 5 ? 0x1u : 0x2u;
            b3Shape_SetFilter(shape, f, true);
        }
    }
}

static uint64_t mask_of(b3BodyId body) {
    b3ShapeId shape;
    b3Body_GetShapes(body, &shape, 1);
    return b3Shape_GetFilter(shape).maskBits;
}

/* A light body handed back to the host after a carry (another guest carried and dropped it, or
   threw it and it landed, or the host's own character threw it) is a layer copy here again: the
   character bits the carry took come back, and this side's walker is stopped at it. The walker is
   body 5, next to body 4: a body teleported across the row would have its host copy sweep the
   bodies on its way. */
static void walk_into_handed_back(int how) {
    character_category = 0x1;
    layer_on();
    character_category = 0;
    give_categories();
    const body_sync_id_t id = body_sync_id_make(4, 1), carrier = body_sync_id_make(BODIES - 1, 1);
    tick_all();
    side_t *owner = how == 2 ? &host : &other;
    if (how != 2) {
        grant(&other, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    }
    body_sync_set_holder(owner->sync, id, carrier);
    for (int i = 0; i < 10; i++) {
        tick_all();
    }
    TEST_ASSERT_TRUE_MESSAGE((mask_of(guest.bodies[4]) & 0x1u) == 0, "a carried copy kept the character bits");
    body_sync_set_holder(owner->sync, id, BODY_SYNC_ID_NONE);
    if (how != 0) {
        b3Body_SetLinearVelocity(owner->bodies[4], (b3Vec3){0.0f, 3.0f, 1.5f}); /* thrown: handed back in flight */
    }
    if (how != 2) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(other.sync, id));
    }
    for (int i = 0; i < 150; i++) {
        tick_all();
    }
    TEST_ASSERT_TRUE_MESSAGE((mask_of(guest.bodies[4]) & 0x1u) != 0, "the character bits did not come back after the carry");
    grant(&guest, body_sync_id_make(5, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Vec3 at = b3Body_GetPosition(guest.bodies[4]);
    b3Body_SetTransform(guest.bodies[5], (b3Vec3){at.x + 0.45f, 0.2f, at.z}, b3Quat_identity);
    int asleep = 0;
    for (int i = 0; i < 60; i++) { /* 0.2 m of walk: through the copy it would end 0.25 m from its centre */
        tick_driving5((b3Vec3){-0.2f, 0.0f, 0.0f}, &asleep);
        TEST_ASSERT_TRUE_MESSAGE(b3Body_GetPosition(guest.bodies[5]).x - b3Body_GetPosition(guest.bodies[4]).x > 0.37f, "this side's body passed into a handed-back copy");
    }
}

static void test_a_copy_another_guest_dropped_stops_this_sides_body(void) { walk_into_handed_back(0); }
static void test_a_copy_another_guest_threw_stops_this_sides_body_where_it_landed(void) { walk_into_handed_back(1); }
static void test_a_body_the_host_threw_stops_this_sides_body_where_it_landed(void) { walk_into_handed_back(2); }

/* ---- authority follows interaction ---- */

static void interaction_on(void) {
    tearDown();
    light_mass = 100.0f;
    authority_mode = BODY_SYNC_AUTHORITY_INTERACTION;
    setUp();
    light_mass = 0.0f;
    authority_mode = BODY_SYNC_AUTHORITY_HOST;
}

static body_sync_id_t id_of(uint32_t i) { return body_sync_id_make(i, 1); }

/* Body `i` of `s` carried by `s` (a holder named: one of its agents, the game moving it) at x, z. */
static void hand(side_t *s, uint32_t i, float x, float z) {
    grant(s, id_of(i), BODY_SYNC_HOLD_UNTIL_RELEASE);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_holder(s->sync, id_of(i), id_of(BODIES - 1)));
    tick_all(); /* its take-over's placement first */
    b3Body_SetTransform(s->bodies[i], (b3Vec3){x, 0.2f, z}, b3Quat_identity);
}

static void drive(side_t *s, uint32_t i, b3Vec3 v) {
    TEST_ASSERT_TRUE(drive_count < 4);
    drives[drive_count++] = (drive_t){s, i, v};
}

static void stop_driving(side_t *s, uint32_t i) {
    for (uint32_t k = 0; k < drive_count; k++) {
        if (drives[k].side == s && drives[k].body == i) {
            drives[k] = drives[--drive_count];
            return;
        }
    }
}

/* The hand put away: still carried, out of every contact. */
static void put_away(side_t *s, uint32_t i) {
    stop_driving(s, i);
    b3Body_SetTransform(s->bodies[i], (b3Vec3){-30.0f, 0.2f, -30.0f}, b3Quat_identity);
    b3Body_SetLinearVelocity(s->bodies[i], (b3Vec3){0.0f, 0.0f, 0.0f});
    b3Body_SetAngularVelocity(s->bodies[i], (b3Vec3){0.0f, 0.0f, 0.0f});
}

/* A scene change made on every side alike, before anyone moves it. */
static void place_everywhere(uint32_t i, float x, float y, float z) {
    side_t *sides[] = {&host, &guest, &other};
    for (int k = 0; k < 3; k++) {
        b3Body_SetTransform(sides[k]->bodies[i], (b3Vec3){x, y, z}, b3Quat_identity);
        b3Body_SetAwake(sides[k]->bodies[i], true); /* the host sends the new pose */
    }
}

static uint8_t seat_on_host(uint32_t i) {
    body_sync_authority_t a;
    TEST_ASSERT_TRUE(body_sync_authority(host.sync, id_of(i), &a));
    return a.seat;
}

static void settle_interaction(void) {
    interaction_on();
    for (int i = 0; i < 90; i++) {
        tick_all();
    }
}

/* Ticks until `i` is local on `s`; false after `max`. */
static bool tick_until_local(side_t *s, uint32_t i, int max) {
    for (int k = 0; k < max; k++) {
        tick_all();
        if (body_sync_is_local(s->sync, id_of(i))) {
            return true;
        }
    }
    return false;
}

/* A take goes on from the pose the copy shows. A copy in flight plays the playback delay behind
   the newest state, which a placement at the take would jump to. */
static void test_a_take_keeps_the_copys_pose(void) {
    playback_delay = 6;
    settle_interaction();
    playback_delay = 1;
    hand(&guest, 1, 3.0f, -1.2f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, 0.5f});
    b3Body_SetLinearVelocity(host.bodies[3], (b3Vec3){0.0f, 1.5f, -6.0f});
    bool taken = false;
    for (int k = 0; k < 60 && !taken; k++) {
        tick_all();
        taken = body_sync_is_local(guest.sync, id_of(3));
    }
    TEST_ASSERT_TRUE_MESSAGE(taken, "the hand never took the thrown box");
    b3Vec3 before = b3Body_GetPosition(guest.bodies[3]);
    body_sync_apply(guest.sync, ++tick_no);
    TEST_ASSERT_TRUE_MESSAGE(apart_from(guest.bodies[3], before) < 1e-3f, "the take moved the body");
}

/* The take's body goes out dynamic: the reads come before the takes, and a copy in flight is
   driven kinematic until its resume. The claim here reaches the host before the take tick's
   state, which the host then takes at the number that state declares. */
static void test_a_taken_body_never_goes_out_kinematic(void) {
    settle_interaction();
    hand(&guest, 1, 3.0f, -1.2f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, 0.5f});
    b3Body_SetLinearVelocity(host.bodies[3], (b3Vec3){0.0f, 1.5f, -6.0f});
    bool owned = false;
    for (int k = 0; k < 60; k++) {
        tick_no++;
        step(&host);
        step(&guest);
        step(&other);
        records(&guest, &host);
        records(&other, &host);
        state(&host, &guest);
        state(&host, &other);
        state(&guest, &host);
        state(&other, &host);
        records(&host, &guest);
        records(&host, &other);
        owned = owned || seat_on_host(3) == GUEST;
        TEST_ASSERT_EQUAL_MESSAGE(b3_dynamicBody, b3Body_GetType(host.bodies[3]), "the host heard the take kinematic");
    }
    TEST_ASSERT_TRUE_MESSAGE(owned, "the hand never took the thrown box");
}

/* A pile leaning on a character never sleeps: it creeps, its owner's pose crossing a step of the
   copy grid now and then. Box 2 creeps at `speed` away from box 3, resting 3 cm off it; the
   guest's copies are watched while it does. */
static void creep_box(float speed, bool *left_hold, bool *neighbour_left, float *gap) {
    layer_on();
    place_everywhere(3, 2.43f, 0.2f, 0.0f);
    for (int i = 0; i < 150; i++) {
        tick_all();
    }
    *left_hold = *neighbour_left = false;
    *gap = 0.0f;
    const float d = speed / 60.0f * 0.70710678f; /* diagonal: the grid steps in two axes */
    for (int i = 0; i < 240; i++) {
        b3Vec3 p = b3Body_GetPosition(host.bodies[2]);
        b3Body_SetTransform(host.bodies[2], (b3Vec3){p.x - d, p.y, p.z + d}, b3Body_GetRotation(host.bodies[2]));
        b3Body_SetAwake(host.bodies[2], true);
        tick_all();
        *left_hold = *left_hold || body_sync_is_predicted(guest.sync, id_of(2));
        *neighbour_left = *neighbour_left || body_sync_is_predicted(guest.sync, id_of(3));
        float g = apart(host.bodies[2], guest.bodies[2]);
        *gap = g > *gap ? g : *gap;
    }
}

/* Slower than the creep bound the owner rests: its copy stays in the rest hold, following on the
   grid within a step and the grid's rounding, and its catch-up steps (0.2 m/s for a tick) turn
   no resting neighbour soft. Faster than the bound it moves, and the copy leaves the hold. */
static void test_a_creeping_pile_stays_in_the_rest_hold(void) {
    bool left, neighbour;
    float gap;
    creep_box(0.015f, &left, &neighbour, &gap); /* the random sweep's creep */
    TEST_ASSERT_FALSE_MESSAGE(left, "a creeping owner's copy left the rest hold");
    TEST_ASSERT_FALSE_MESSAGE(neighbour, "a held copy's catch-up step turned its resting neighbour soft");
    TEST_ASSERT_TRUE_MESSAGE(gap <= 2.0f * SYNC_SOFT_HOLD_M, "the held copy did not follow its creeping owner");
    creep_box(0.06f, &left, &neighbour, &gap); /* twice the creep bound */
    TEST_ASSERT_TRUE_MESSAGE(left, "an owner moving past the creep bound kept its copy in the rest hold");
}

/* A guest that never hears the host hold its rest pose keeps the body; it returns once it does. */
static void test_a_return_waits_for_the_hosts_acknowledgement(void) {
    settle_interaction();
    hand(&guest, 1, 2.0f, 0.6f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    TEST_ASSERT_TRUE(tick_until_local(&guest, 2, 60));
    for (int k = 0; k < 10; k++) {
        tick_all();
    }
    put_away(&guest, 1);
    lose_guest_fates = true; /* every state reaches the host; the guest hears each lost */
    for (int k = 0; k < 150; k++) {
        tick_all();
        TEST_ASSERT_TRUE_MESSAGE(body_sync_is_local(guest.sync, id_of(2)), "returned before the host acknowledged the rest pose");
    }
    TEST_ASSERT_TRUE_MESSAGE(speed_of(guest.bodies[2]) < 1e-3f, "the box never came to rest");
    lose_guest_fates = false;
    for (int k = 0; k < 60 && seat_on_host(2) != BODY_SYNC_SEAT_HOST; k++) {
        tick_all();
    }
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, seat_on_host(2));
}

/* The host's character meets a guest's body owned by interaction: the copy keeps the character's
   bits, so the touch takes it back. Without them the guest's own simulation would still push the
   box ahead of the character's copy there, and the host's agent would never come first. */
static void test_the_hosts_character_is_stopped_by_a_guests_body(void) {
    character_category = 0x2;
    settle_interaction();
    character_category = 0;
    b3ShapeId shape;
    b3Body_GetShapes(host.bodies[BODIES - 1], &shape, 1);
    b3Filter f = b3Shape_GetFilter(shape);
    f.categoryBits = 0x2;
    b3Shape_SetFilter(shape, f, true);
    hand(&guest, 1, 2.0f, 0.6f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    TEST_ASSERT_TRUE(tick_until_local(&guest, 2, 60));
    put_away(&guest, 1);
    lose_guest_fates = true; /* the guest keeps the box */
    for (int k = 0; k < 30; k++) {
        tick_all();
    }
    TEST_ASSERT_EQUAL_UINT8(GUEST, seat_on_host(2));
    b3Vec3 box = b3Body_GetPosition(host.bodies[2]);
    b3Body_SetTransform(host.bodies[BODIES - 1], (b3Vec3){box.x, 0.2f, box.z - 1.0f}, b3Quat_identity);
    drive(&host, BODIES - 1, (b3Vec3){0.0f, 0.0f, 1.0f});
    for (int k = 0; k < 90 && seat_on_host(2) != BODY_SYNC_SEAT_HOST; k++) {
        tick_all();
        float gap = b3Body_GetPosition(host.bodies[2]).z - b3Body_GetPosition(host.bodies[BODIES - 1]).z;
        TEST_ASSERT_TRUE_MESSAGE(gap > 0.3f, "the host's character walked through the guest's box");
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(BODY_SYNC_SEAT_HOST, seat_on_host(2), "the host's character met the box and never took it back");
    body_sync_traffic_t t;
    body_sync_traffic(host.sync, GUEST, &t);
    TEST_ASSERT_EQUAL_UINT32(1, t.reclaims);
}

/* The host's state of a body sent before it granted the take, arriving after the grant, is
   dropped: played once the body is a copy again, it would pull it back to where it was then. */
static void test_a_state_from_before_the_grant_is_dropped(void) {
    settle_interaction();
    hand(&guest, 1, 2.0f, 0.6f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    drive(&host, 2, (b3Vec3){0.0f, 0.0f, -0.3f}); /* the host moves it: its states go out */
    bool taken = false;
    uint8_t late[1500];
    uint32_t late_size = 0;
    for (int k = 0; k < 60 && !taken; k++) {
        tick_no++;
        step(&host);
        step(&guest);
        step(&other);
        taken = body_sync_is_local(guest.sync, id_of(2));
        if (taken) {
            stop_driving(&host, 2);
            TEST_ASSERT_TRUE(body_sync_write_state(host.sync, GUEST, late, 1200, &late_size));
            body_sync_state_sent(host.sync, GUEST, host.numbers[GUEST]);
            body_sync_resolve(host.sync, GUEST, host.numbers[GUEST]++, true);
        } else {
            state(&host, &guest);
        }
        state(&host, &other);
        state(&guest, &host);
        state(&other, &host);
        records(&guest, &host);
        records(&host, &guest);
        records(&host, &other);
        records(&other, &host);
    }
    TEST_ASSERT_TRUE_MESSAGE(taken, "the hand never took the box");
    TEST_ASSERT_EQUAL_UINT8(GUEST, seat_on_host(2));
    body_sync_traffic_t t0, t1;
    body_sync_traffic(guest.sync, BODY_SYNC_SEAT_HOST, &t0);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no, late, late_size));
    body_sync_traffic(guest.sync, BODY_SYNC_SEAT_HOST, &t1);
    TEST_ASSERT_TRUE_MESSAGE(t1.stale > t0.stale, "the state from before the grant was kept");
    for (int k = 0; k < 10; k++) {
        tick_all();
    }
    put_away(&guest, 1);
    for (int k = 0; k < 300 && body_sync_is_local(guest.sync, id_of(2)); k++) {
        tick_all();
    }
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_local(guest.sync, id_of(2)), "the box never returned");
    b3Vec3 rest = b3Body_GetPosition(guest.bodies[2]);
    for (int k = 0; k < 30; k++) {
        tick_all();
        TEST_ASSERT_TRUE_MESSAGE(apart_from(guest.bodies[2], rest) < 0.01f, "the copy went back to a pose from before the grant");
    }
}

/* A body owned by interaction returns only once every owned body it touches is quiet too. */
static void test_a_quiet_body_waits_for_its_moving_neighbour(void) {
    settle_interaction();
    place_everywhere(3, 2.0f, 0.6f, 0.0f); /* on box 2 */
    for (int k = 0; k < 30; k++) {
        tick_all();
    }
    hand(&guest, 1, 2.0f, 0.5f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    for (int k = 0; k < 60 && !(body_sync_is_local(guest.sync, id_of(2)) && body_sync_is_local(guest.sync, id_of(3))); k++) {
        tick_all();
    }
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, id_of(2)) && body_sync_is_local(guest.sync, id_of(3)));
    for (int k = 0; k < 3; k++) {
        tick_all();
    }
    put_away(&guest, 1);
    drive(&guest, 3, (b3Vec3){0.1f, 0.0f, 0.0f}); /* sliding on box 2, slower than sleep */
    b3Body_EnableSleep(guest.bodies[3], false);
    b3Vec3 still = b3Body_GetPosition(guest.bodies[2]);
    for (int k = 0; k < 120; k++) {
        tick_all();
        TEST_ASSERT_TRUE_MESSAGE(body_sync_is_local(guest.sync, id_of(2)), "returned while a body it touches still moved");
        still = k == 60 ? b3Body_GetPosition(guest.bodies[2]) : still;
    }
    /* Quiet itself (under SYNC_POLICY_QUIET_M over the last second): only its neighbour holds it. */
    TEST_ASSERT_TRUE_MESSAGE(apart_from(guest.bodies[2], still) < 0.002f, "box 2 never came to rest");
    stop_driving(&guest, 3);
    b3Body_EnableSleep(guest.bodies[3], true);
    for (int k = 0; k < 120 && seat_on_host(2) != BODY_SYNC_SEAT_HOST; k++) {
        tick_all();
    }
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, seat_on_host(2));
}

/* A body the host's agent sets going is the host's until it rests: a guest's touch never takes it. */
static void test_what_the_hosts_agent_sets_going_stays_the_hosts(void) {
    settle_interaction();
    b3Body_SetTransform(host.bodies[BODIES - 1], (b3Vec3){2.0f, 0.2f, 0.6f}, b3Quat_identity);
    drive(&host, BODIES - 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    hand(&guest, 1, 1.4f, -0.3f);
    for (int k = 0; k < 10; k++) {
        tick_all();
    }
    drive(&guest, 1, (b3Vec3){1.0f, 0.0f, 0.0f});
    for (int k = 0; k < 60; k++) {
        tick_all();
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(BODY_SYNC_SEAT_HOST, seat_on_host(2), "a guest took a body the host's agent set going");
    }
    TEST_ASSERT_TRUE_MESSAGE(apart_from(guest.bodies[1], b3Body_GetPosition(guest.bodies[2])) < 0.5f, "the hand never reached the box");
}

/* A body owned by interaction takes only a body at rest at its owner: one its owner moves was set
   going by someone else. */
static void test_an_owned_body_never_takes_a_moving_one(void) {
    settle_interaction();
    place_everywhere(3, 2.0f, 0.2f, -0.7f);
    for (int k = 0; k < 30; k++) {
        tick_all();
    }
    drive(&host, 3, (b3Vec3){0.0f, 0.0f, -0.3f});
    hand(&guest, 1, 2.0f, 0.6f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.2f});
    bool met = false;
    for (int k = 0; k < 90; k++) {
        tick_all();
        met = met || apart(guest.bodies[2], guest.bodies[3]) < 0.45f;
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(BODY_SYNC_SEAT_HOST, seat_on_host(3), "an owned body took one its owner moves");
    }
    TEST_ASSERT_TRUE_MESSAGE(met, "the owned box never met the moving one");
    TEST_ASSERT_EQUAL_UINT8(GUEST, seat_on_host(2));
}

/* A take names a body: the host grants it only where its own world shows one of the claimant's
   agents touching it. Claims of far bodies no agent of the guest's is near are refused; the
   hand's real take is granted. */
static void test_a_take_of_a_body_no_agent_touches_is_refused(void) {
    settle_interaction();
    hand(&guest, 1, 2.0f, 0.6f);
    for (int k = 0; k < 10; k++) {
        tick_all();
    }
    uint32_t stale = 0;
    for (uint32_t i = 16; i < 32; i++) {
        stub_record_t r = {.type = STUB_REC_CLAIM, .id = id_of(i), .hold = BODY_SYNC_HOLD_UNTIL_REST, .sequence = (uint16_t)(900 + i)};
        uint32_t size = stub_wire_put_record(&r, packet, sizeof(packet));
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_record(host.sync, GUEST, tick_no, packet, size));
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(BODY_SYNC_SEAT_HOST, seat_on_host(i), "a forged take of a far body was granted");
    }
    (void)stale;
    body_sync_traffic_t t;
    body_sync_traffic(host.sync, GUEST, &t);
    TEST_ASSERT_EQUAL_UINT32(16, t.take_refusals);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    for (int k = 0; k < 60 && seat_on_host(2) != GUEST; k++) {
        tick_all();
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(GUEST, seat_on_host(2), "the hand's own take was refused");
}

/* One tick with the guest's records to the host held back while `hold`: its take waits as long
   as the test says, the host's states reaching it, and acknowledged, meanwhile. */
static void tick_holding(bool hold) {
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
    if (!hold) {
        records(&guest, &host);
    }
    records(&other, &host);
}

static b3Vec3 drawn_at(const side_t *s, uint32_t i) {
    float offset[3], turn[4];
    TEST_ASSERT_TRUE(body_sync_render_offset(s->sync, id_of(i), offset, turn));
    b3Vec3 p = b3Body_GetPosition(s->bodies[i]);
    return (b3Vec3){p.x + offset[0], p.y + offset[1], p.z + offset[2]};
}

static float distance_of(b3Vec3 a, b3Vec3 b) { return sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }

/* The hand takes a box; while its claim waits up to `wait` ticks, the host's character sets the
   box going from behind, so the host refuses the take, and the hand presses on. The refusals of
   the hand's takes while the host's agent holds the box. */
static uint32_t refused_take(uint32_t wait) {
    settle_interaction();
    hand(&guest, 1, 2.0f, 0.6f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    bool taken = false;
    for (int k = 0; k < 60 && !taken; k++) {
        tick_holding(true);
        taken = body_sync_is_local(guest.sync, id_of(2));
    }
    TEST_ASSERT_TRUE_MESSAGE(taken, "the hand never took the box");
    /* The host's character pushes the box along x from behind, the hand presses its side and
       keeps up with it. */
    b3Vec3 box = b3Body_GetPosition(host.bodies[2]);
    b3Body_SetTransform(host.bodies[BODIES - 1], (b3Vec3){box.x - 0.45f, 0.2f, box.z}, b3Quat_identity);
    drive(&host, BODIES - 1, (b3Vec3){1.5f, 0.0f, 0.0f});
    stop_driving(&guest, 1);
    drive(&guest, 1, (b3Vec3){1.5f, 0.0f, -0.3f});
    /* The guest's records reach the host once every `wait` ticks: each take waits up to that. */
    body_sync_traffic_t t;
    body_sync_authority_t a;
    bool active = false, ended = false;
    uint32_t before = 0, refusals = 0;
    for (uint32_t k = 0; k < 150; k++) {
        tick_holding(k % wait != wait - 1u);
        TEST_ASSERT_TRUE(body_sync_authority(host.sync, id_of(2), &a));
        bool now = a.seat == BODY_SYNC_SEAT_HOST && a.hold == BODY_SYNC_HOLD_UNTIL_REST;
        body_sync_traffic(host.sync, GUEST, &t);
        if (now && !active) {
            before = t.take_refusals;
        }
        ended = ended || (active && !now);
        active = active || now;
        if (active && !ended) {
            refusals = t.take_refusals - before;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(active, "the host's character never set the box going");
    return refusals;
}

/* The host counts each rare group as known once the packet that carried it arrived. A take that
   waits drops the host's states of its body (it simulates the body itself), so a guest that also
   dropped their groups would hold the hold from before its take once refused, and take the box
   the host's agent sets going again at every touch. */
static void test_a_refused_take_is_not_repeated(void) {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, refused_take(20), "a take the host refused for its own agent's push was sent again");
}

/* A refused take hands the box to the host's picture without a visible jump, however far the
   take carried it. Bound: the render offset's release at its snap reach, 0.15 of 2 m (README
   "Metrics", jump.max), beyond the owner's own move. */
static void test_a_refused_take_does_not_jump(void) {
    /* The host's copy of the hand plays 20 ticks behind: while the take waits, the hand carries the
       box away on the guest and the host's world still holds it where its character touched it. */
    playback_delay = 20;
    settle_interaction();
    playback_delay = 1;
    hand(&guest, 1, 1.4f, 0.0f);
    drive(&guest, 1, (b3Vec3){4.0f, 0.0f, 0.0f});
    bool taken = false;
    for (int k = 0; k < 60 && !taken; k++) {
        tick_holding(true);
        taken = body_sync_is_local(guest.sync, id_of(2));
    }
    TEST_ASSERT_TRUE_MESSAGE(taken, "the hand never took the box");
    b3Vec3 box = b3Body_GetPosition(host.bodies[2]);
    b3Body_SetTransform(host.bodies[BODIES - 1], (b3Vec3){box.x, 0.2f, box.z - 0.45f}, b3Quat_identity);
    drive(&host, BODIES - 1, (b3Vec3){0.0f, 0.0f, 0.5f});
    b3Vec3 drawn = drawn_at(&guest, 2), owner = b3Body_GetPosition(host.bodies[2]), body = b3Body_GetPosition(guest.bodies[2]);
    float owner_moved = 0.0f, jump = 0.0f, placed = 0.0f;
    bool refused = false;
    for (uint32_t k = 0; k < 150; k++) {
        tick_holding(k < 40u);
        b3Vec3 d = drawn_at(&guest, 2), o = b3Body_GetPosition(host.bodies[2]), p = b3Body_GetPosition(guest.bodies[2]);
        /* The copy plays a tick behind: its owner's move is this tick's or the one before. */
        float moved = distance_of(o, owner), allowed = moved > owner_moved ? moved : owner_moved;
        float j = distance_of(d, drawn) - allowed;
        jump = j > jump ? j : jump;
        placed = distance_of(p, body) > placed ? distance_of(p, body) : placed;
        drawn = d;
        owner = o;
        body = p;
        owner_moved = moved;
        refused = refused || (k >= 40u && !body_sync_is_local(guest.sync, id_of(2)));
    }
    TEST_ASSERT_TRUE_MESSAGE(refused, "the host never refused the take");
    TEST_ASSERT_TRUE_MESSAGE(placed > SYNC_OFFSET_SNAP_M, "the refusal never placed the box past the offset's snap reach");
    TEST_ASSERT_TRUE_MESSAGE(jump <= (1.0f - SYNC_OFFSET_FAST) * SYNC_OFFSET_SNAP_M, "the refused take's drawn pose jumped");
}

/* Only a walk this side's own body starts wakes a resting heap whole: a soft copy flying at a row
   of resting copies 1 cm apart frees the one it comes at, not the row behind it, which a walk
   through the touch distance would free in the same apply. */
static void test_a_soft_copy_does_not_wake_a_heap_whole(void) {
    layer_on();
    const float row[3] = {-0.95f, -1.36f, -1.77f};
    for (uint32_t k = 0; k < 3; k++) {
        place_everywhere(2u + k, row[k], 0.2f, 0.0f);
    }
    for (int i = 0; i < 60; i++) {
        tick_all();
    }
    for (uint32_t k = 0; k < 3; k++) {
        TEST_ASSERT_FALSE_MESSAGE(body_sync_is_predicted(guest.sync, body_sync_id_make(2u + k, 1)), "the row was not resting in the hold");
    }
    grant(&guest, body_sync_id_make(1, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){0.6f, 0.2f, 0.0f}, b3Quat_identity);
    int asleep = 0;
    for (int i = 0; i < 7; i++) {
        tick_driving((b3Vec3){-3.0f, 0.0f, 0.0f}, &asleep); /* into body 0 and on: soft, slower than a flight */
    }
    bool reached = false;
    for (int i = 0; i < 40 && !reached; i++) {
        tick_driving((b3Vec3){0.0f, 0.0f, 0.0f}, &asleep);
        reached = body_sync_is_predicted(guest.sync, body_sync_id_make(2, 1));
        if (reached) {
            TEST_ASSERT_FALSE_MESSAGE(body_sync_is_predicted(guest.sync, body_sync_id_make(4, 1)), "a soft copy woke the resting row whole");
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(reached, "the flying copy never reached the row");
}

/* The host's take-back goes on from its own simulation of the body: its pose does not move at
   the reclaim, though the guest's newest state of the moving body is elsewhere. */
static void test_a_take_back_goes_on_from_the_hosts_own_pose(void) {
    character_category = 0x2;
    playback_delay = 6; /* the host's copy plays well behind the guest's newest state */
    settle_interaction();
    character_category = 0;
    playback_delay = 1;
    b3ShapeId shape;
    b3Body_GetShapes(host.bodies[BODIES - 1], &shape, 1);
    b3Filter f = b3Shape_GetFilter(shape);
    f.categoryBits = 0x2;
    b3Shape_SetFilter(shape, f, true);
    hand(&guest, 1, 2.0f, 0.6f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    TEST_ASSERT_TRUE(tick_until_local(&guest, 2, 60));
    for (int k = 0; k < 10; k++) {
        tick_all();
    }
    b3Vec3 box = b3Body_GetPosition(host.bodies[2]);
    b3Body_SetTransform(host.bodies[BODIES - 1], (b3Vec3){box.x, 0.2f, box.z - 0.7f}, b3Quat_identity);
    drive(&host, BODIES - 1, (b3Vec3){0.0f, 0.0f, 1.5f});
    bool back = false;
    for (int k = 0; k < 60 && !back; k++) {
        tick_all();
        back = seat_on_host(2) == BODY_SYNC_SEAT_HOST;
    }
    TEST_ASSERT_TRUE_MESSAGE(back, "the host's character never took the box back");
    b3Vec3 before = b3Body_GetPosition(host.bodies[2]);
    body_sync_apply(host.sync, ++tick_no);
    TEST_ASSERT_TRUE_MESSAGE(apart_from(host.bodies[2], before) < 1e-3f, "the take-back moved the host's body");
}

/* The take-back of a body the guest drives fast: the host's copy plays the playback delay behind
   the guest's newest state, and a take-back placed there would jump the host's body that far. */
static void test_a_take_back_of_a_fast_body_keeps_the_hosts_pose(void) {
    character_category = 0x2;
    playback_delay = 6;
    settle_interaction();
    character_category = 0;
    playback_delay = 1;
    b3ShapeId shape;
    b3Body_GetShapes(host.bodies[BODIES - 1], &shape, 1);
    b3Filter f = b3Shape_GetFilter(shape);
    f.categoryBits = 0x2;
    b3Shape_SetFilter(shape, f, true);
    hand(&guest, 1, 2.0f, 0.6f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    TEST_ASSERT_TRUE(tick_until_local(&guest, 2, 60));
    stop_driving(&guest, 1);
    drive(&guest, 2, (b3Vec3){0.0f, 0.0f, -2.5f});
    for (int k = 0; k < 6; k++) {
        tick_all();
    }
    b3Vec3 box = b3Body_GetPosition(host.bodies[2]);
    b3Body_SetTransform(host.bodies[BODIES - 1], (b3Vec3){box.x, 0.2f, box.z - 0.6f}, b3Quat_identity);
    drive(&host, BODIES - 1, (b3Vec3){0.0f, 0.0f, 1.5f});
    bool back = false;
    for (int k = 0; k < 60 && !back; k++) {
        tick_all();
        back = seat_on_host(2) == BODY_SYNC_SEAT_HOST;
    }
    TEST_ASSERT_TRUE_MESSAGE(back, "the host's character never took the box back");
    TEST_ASSERT_TRUE_MESSAGE(apart_from(host.bodies[2], b3Body_GetPosition(guest.bodies[2])) > 0.1f, "the host's copy was not behind the guest's newest state");
    b3Vec3 before = b3Body_GetPosition(host.bodies[2]);
    body_sync_apply(host.sync, ++tick_no);
    TEST_ASSERT_TRUE_MESSAGE(apart_from(host.bodies[2], before) < 1e-3f, "the take-back of a fast body moved the host's body");
}

/* Where bodies two guests own by interaction touch, the host takes both back. */
static void test_the_host_takes_back_two_guests_touching_bodies(void) {
    settle_interaction();
    hand(&guest, 0, 1.55f, 1.0f);
    hand(&other, 5, 3.45f, 1.0f);
    drive(&guest, 0, (b3Vec3){1.0f, 0.0f, 0.0f});
    drive(&other, 5, (b3Vec3){-1.0f, 0.0f, 0.0f});
    body_sync_traffic_t g, o;
    bool both = false;
    for (int k = 0; k < 90 && !both; k++) {
        tick_all();
        body_sync_traffic(host.sync, GUEST, &g);
        body_sync_traffic(host.sync, OTHER, &o);
        both = g.reclaims > 0 && o.reclaims > 0;
    }
    TEST_ASSERT_TRUE_MESSAGE(g.takes > 0 && o.takes > 0, "a guest never took its box");
    TEST_ASSERT_TRUE_MESSAGE(both, "the host left two guests' touching bodies with them");
}

/* How deep a kinematic body lies inside `body` as its last step's contacts saw it, in metres. */
static float kinematic_depth_in(b3BodyId body) {
    float deepest = 0.0f;
    b3ContactData contacts[16];
    int n = b3Body_GetContactData(body, contacts, 16);
    for (int k = 0; k < n; k++) {
        b3BodyId a = b3Shape_GetBody(contacts[k].shapeIdA), b = b3Shape_GetBody(contacts[k].shapeIdB);
        b3BodyId them = B3_ID_EQUALS(a, body) ? b : a;
        for (int m = 0; m < contacts[k].manifoldCount && b3Body_GetType(them) == b3_kinematicBody; m++) {
            for (int p = 0; p < contacts[k].manifolds[m].pointCount; p++) {
                deepest = fmaxf(deepest, -contacts[k].manifolds[m].points[p].separation);
            }
        }
    }
    return deepest;
}

/* One tick with the guest's body `c` run along x at `run` before its step, as a controller does
   (the rest of its velocity left to the world); none when `run` is NULL. */
static void tick_running(uint32_t c, const float *run) {
    tick_no++;
    step(&host);
    body_sync_apply(guest.sync, tick_no);
    if (run) {
        b3Vec3 v = b3Body_GetLinearVelocity(guest.bodies[c]);
        b3Body_SetLinearVelocity(guest.bodies[c], (b3Vec3){*run, v.y, v.z});
    }
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    body_sync_collect(guest.sync, tick_no);
    step(&other);
    state(&host, &guest);
    state(&guest, &host);
    records(&guest, &host);
    records(&host, &guest);
}

/* This guest's character runs into a row of resting light copies, and her own copy on the host
   pushes them past the flight speed. Here her push already moved them, soft; driven kinematic
   along the owner's older path while still against her, they would be placed inside her and
   fling her. Derived bounds: nothing she meets moves faster than her run, and Box3D pushes a
   body out of an overlap at no more than its contact speed (3 m/s), so she never passes their
   sum; no driven copy lies in her deeper than Box3D's linear slop, the depth of a resting
   contact. */
static void test_a_run_into_copies_its_own_push_sets_flying_does_not_fling_her(void) {
    const uint32_t c = BODIES - 1, row[4] = {1, 2, 3, 4};
    const float run = 6.0f, contact_speed = 3.0f;
    playback_delay = 6; /* the host's picture plays well behind her own pushes */
    layer_on();
    playback_delay = 1;
    for (int k = 0; k < 4; k++) {
        place_everywhere(row[k], 2.0f + 0.41f * (float)k, 0.2f, -3.0f);
    }
    place_everywhere(c, 1.4f, 0.2f, -3.0f);
    for (int i = 0; i < 150; i++) {
        tick_all();
    }
    grant(&guest, id_of(c), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    bool flew = false;
    float fastest = 0.0f, deepest = 0.0f;
    for (int i = 0; i < 45; i++) {
        tick_running(c, &run);
        fastest = fmaxf(fastest, speed_of(guest.bodies[c]));
        deepest = fmaxf(deepest, kinematic_depth_in(guest.bodies[c]));
        for (int k = 0; k < 4; k++) {
            flew = flew || speed_of(host.bodies[row[k]]) >= SYNC_SOFT_FLIGHT_SPEED;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(flew, "her copy on the host never set the row flying: the case is not met");
    char what[96];
    snprintf(what, sizeof(what), "her character was flung to %.2f m/s", (double)fastest);
    TEST_ASSERT_TRUE_MESSAGE(fastest <= run + contact_speed, what);
    snprintf(what, sizeof(what), "a kinematic copy lay %.4f m inside her", (double)deepest);
    TEST_ASSERT_TRUE_MESSAGE(deepest <= B3_LINEAR_SLOP, what);
}

/* A copy far from this guest's standing character, out of every reach and mark, whose owner
   flies straight through where she stands: on the host her copy is filtered out of its way, so
   only this side's older owner path crosses her. Driven along that path it would be placed and
   swept into her; it is never inside her deeper than Box3D's linear slop. */
static void test_a_flying_copys_path_through_her_never_places_it_inside_her(void) {
    const uint32_t c = BODIES - 1, flyer = 1;
    playback_delay = 6;
    layer_on();
    playback_delay = 1;
    place_everywhere(flyer, 1.0f, 0.2f, -3.0f);
    place_everywhere(c, 4.0f, 0.2f, -3.0f);
    b3ShapeId shape;
    b3Body_GetShapes(host.bodies[c], &shape, 1);
    b3Filter f = b3Shape_GetFilter(shape);
    f.categoryBits = 0x4u;
    b3Shape_SetFilter(shape, f, true);
    b3Body_GetShapes(host.bodies[flyer], &shape, 1);
    f = b3Shape_GetFilter(shape);
    f.maskBits = ~0x4ull;
    b3Shape_SetFilter(shape, f, true);
    for (int i = 0; i < 150; i++) {
        tick_all();
    }
    grant(&guest, id_of(c), BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    drive(&host, flyer, (b3Vec3){8.0f, 0.0f, 0.0f});
    float deepest = 0.0f;
    for (int i = 0; i < 60; i++) {
        tick_running(c, NULL);
        deepest = fmaxf(deepest, kinematic_depth_in(guest.bodies[c]));
    }
    TEST_ASSERT_TRUE_MESSAGE(b3Body_GetPosition(host.bodies[flyer]).x > 5.0f, "the owner never flew past where she stood: the case is not met");
    char what[96];
    snprintf(what, sizeof(what), "a kinematic copy lay %.4f m inside her", (double)deepest);
    TEST_ASSERT_TRUE_MESSAGE(deepest <= B3_LINEAR_SLOP, what);
}

/* Another guest's throw, held until it settles, flies through where this guest's character
   stands; on the thrower her copy is filtered out of its way, so the owner's path crosses her.
   It strikes her here, never passes through her. Derived bound: the two cubes' inscribed spheres
   (0.2 m each) overlap no deeper than the cubes do, and the flight reaches at most one step of
   travel (v dt) plus Box3D's linear slop into her before the contact acts, so their centres stay
   0.4 m less that apart. The host holds the throw dynamic with the character bits on. */
static void test_another_seats_throw_never_passes_through_this_sides_character(void) {
    const uint32_t c = BODIES - 1, flyer = 1;
    const float speed = 8.0f, inner = 0.2f, dt = 1.0f / 60.0f;
    const uint64_t her_bits = 0x1u;
    character_category = her_bits;
    layer_on();
    character_category = 0;
    side_t *sides[3] = {&host, &guest, &other};
    for (int k = 0; k < 3; k++) {
        for (uint32_t b = 0; b < BODIES; b++) {
            b3ShapeId shape;
            b3Body_GetShapes(sides[k]->bodies[b], &shape, 1);
            b3Filter f = b3Shape_GetFilter(shape);
            f.categoryBits = b == c ? her_bits : 0x2u;
            if (sides[k] == &other && b == flyer) {
                f.maskBits = ~her_bits;
            }
            b3Shape_SetFilter(shape, f, true);
        }
    }
    place_everywhere(flyer, 1.0f, 0.2f, -3.0f);
    place_everywhere(c, 4.0f, 0.2f, -3.0f);
    for (int i = 0; i < 150; i++) {
        tick_all();
    }
    grant(&guest, id_of(c), BODY_SYNC_HOLD_UNTIL_RELEASE);
    grant(&other, id_of(flyer), BODY_SYNC_HOLD_UNTIL_SETTLED);
    tick_all();
    drive(&other, flyer, (b3Vec3){speed, 0.0f, 0.0f});
    float nearest = INFINITY;
    int host_ticks = 0, host_bits = 0;
    for (int i = 0; i < 60; i++) {
        tick_all();
        nearest = fminf(nearest, apart(guest.bodies[flyer], guest.bodies[c]));
        body_sync_authority_t a;
        if (body_sync_authority(host.sync, id_of(flyer), &a) && a.seat == OTHER && a.hold == BODY_SYNC_HOLD_UNTIL_SETTLED) {
            host_ticks++;
            host_bits += (mask_of(host.bodies[flyer]) & her_bits) != 0;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(b3Body_GetPosition(other.bodies[flyer]).x > 5.0f, "the owner never flew past where she stood: the case is not met");
    TEST_ASSERT_TRUE_MESSAGE(host_ticks > 0, "the host never held the throw: the case is not met");
    TEST_ASSERT_EQUAL_INT_MESSAGE(host_ticks, host_bits, "the host's copy of the throw let its character through");
    char what[96];
    snprintf(what, sizeof(what), "the throw's centre came %.3f m from hers", (double)nearest);
    TEST_ASSERT_TRUE_MESSAGE(nearest >= 2.0f * inner - (speed * dt + B3_LINEAR_SLOP), what);
}

/* A carried box pressed at a lean's speed into a body this side owns, pinned against a wall: the
   body is quiet but leaned on. Returned there, its copy would be a wall the box pushes into and the
   push would take it again, take and return for as long as the press lasts. */
static void test_a_body_leaned_on_by_its_owner_never_ping_pongs(void) {
    settle_interaction();
    hand(&guest, 1, 2.0f, 0.6f);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -1.0f});
    TEST_ASSERT_TRUE(tick_until_local(&guest, 2, 60));
    stop_driving(&guest, 1);
    for (int k = 0; k < 30; k++) {
        tick_all(); /* box 2 slides to a stop */
    }
    b3Vec3 at = b3Body_GetPosition(guest.bodies[2]);
    side_t *sides[] = {&host, &guest, &other};
    for (int k = 0; k < 3; k++) {
        b3BodyDef def = b3DefaultBodyDef();
        def.position = (b3Vec3){at.x, 0.5f, at.z - 0.3f};
        b3BodyId wall = b3CreateBody(sides[k]->world, &def);
        b3ShapeDef shape = b3DefaultShapeDef();
        b3BoxHull slab = b3MakeBoxHull(1.0f, 0.5f, 0.1f);
        b3CreateHullShape(wall, &shape, &slab.base);
    }
    b3Body_SetTransform(guest.bodies[1], (b3Vec3){at.x, 0.2f, at.z + 0.401f}, b3Quat_identity);
    drive(&guest, 1, (b3Vec3){0.0f, 0.0f, -0.04f});
    int retakes = 0;
    bool was_local = true;
    for (int k = 0; k < 900; k++) {
        tick_all();
        bool local = body_sync_is_local(guest.sync, id_of(2));
        retakes += local && !was_local ? 1 : 0;
        was_local = local;
    }
    /* One take may start the press (the box is set against it); every further one is a cycle. */
    TEST_ASSERT_TRUE_MESSAGE(retakes <= 1, "a body leaned on by its owner went back and forth");
}

/* A throw leaves the hand inside its thrower's reach, and on another side her copy and its copy
   play from different states. Every side keeps them apart until the throw has travelled her
   half-width, its own half-diagonal and the speculative distance at its release speed, and no
   longer: a later touch is a real one. */
static void test_a_throw_is_kept_off_its_thrower_until_it_clears_her_on_every_side(void) {
    layer_on();
    const uint32_t box = 1, her = BODIES - 1;
    const float speed = 6.0f, dt = 1.0f / 60.0f, half = 0.2f;
    place_everywhere(her, 4.0f, 0.2f, -3.0f);
    place_everywhere(box, 4.45f, 0.2f, -3.0f);
    for (int i = 0; i < 30; i++) {
        tick_all();
    }
    grant(&guest, id_of(box), BODY_SYNC_HOLD_UNTIL_RELEASE);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_holder(guest.sync, id_of(box), id_of(her)));
    for (int i = 0; i < 10; i++) {
        tick_all();
    }
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_holder(guest.sync, id_of(box), BODY_SYNC_ID_NONE));
    grant(&guest, id_of(box), BODY_SYNC_HOLD_UNTIL_SETTLED);
    b3Body_SetLinearVelocity(guest.bodies[box], (b3Vec3){speed, 0.0f, 0.0f});
    side_t *sides[3] = {&guest, &host, &other};
    int kept[3] = {0, 0, 0};
    for (int i = 0; i < 60; i++) {
        tick_all();
        for (int k = 0; k < 3; k++) {
            kept[k] += b3Body_GetJointCount(sides[k]->bodies[her]) > 0 ? 1 : 0;
        }
    }
    const float s = B3_SPECULATIVE_DISTANCE;
    const int bound = (int)ceilf((half + s + (half + s) * sqrtf(3.0f)) / (speed * dt));
    const char *names[3] = {"the thrower", "the host", "the other guest"};
    for (int k = 0; k < 3; k++) {
        char what[96];
        snprintf(what, sizeof(what), "%s kept the throw off her for %d ticks (1..%d)", names[k], kept[k], bound);
        TEST_ASSERT_TRUE_MESSAGE(kept[k] >= 1 && kept[k] <= bound, what);
    }
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
    RUN_TEST(test_a_claimed_body_keeps_what_the_caller_did_to_it);
    RUN_TEST(test_a_claim_goes_on_from_the_pose_the_claimant_shows);
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
    RUN_TEST(test_the_layer_holds_only_free_light_copies_on_a_guest);
    RUN_TEST(test_a_soft_copy_answers_the_guests_own_body_at_once);
    RUN_TEST(test_the_layer_at_rest_is_exact_and_still);
    RUN_TEST(test_a_host_move_pulls_the_soft_copy);
    RUN_TEST(test_a_soft_copy_taken_and_released_does_not_jump);
    RUN_TEST(test_a_copy_taking_flight_is_placed_not_swept);
    RUN_TEST(test_a_local_body_sliding_on_a_resting_copy_stays_awake);
    RUN_TEST(test_no_soft_hold_for_a_settling_or_kinematic_owner);
    RUN_TEST(test_a_leaning_character_does_not_hold_a_copy_off);
    RUN_TEST(test_a_pushed_soft_copy_taken_keeps_its_pose);
    RUN_TEST(test_a_released_body_goes_on_from_its_own_pose);
    RUN_TEST(test_a_hit_pauses_the_pull);
    RUN_TEST(test_a_soft_copy_does_not_wake_a_heap_whole);
    RUN_TEST(test_a_layer_copy_keeps_the_character_bits);
    RUN_TEST(test_the_layer_mass_threshold);
    RUN_TEST(test_a_bad_light_mass_is_refused);
    RUN_TEST(test_a_push_from_standstill_at_a_walk);
    RUN_TEST(test_a_push_from_standstill_slower_than_a_lean_mark);
    RUN_TEST(test_a_struck_copy_is_not_pulled_in_the_pause);
    RUN_TEST(test_a_struck_copy_waits_the_measured_round_trip);
    RUN_TEST(test_a_press_reaches_through_a_touching_row);
    RUN_TEST(test_a_copy_another_guest_dropped_stops_this_sides_body);
    RUN_TEST(test_a_copy_another_guest_threw_stops_this_sides_body_where_it_landed);
    RUN_TEST(test_a_body_the_host_threw_stops_this_sides_body_where_it_landed);

    RUN_TEST(test_this_sides_body_is_slowed_by_a_soft_copy);
    RUN_TEST(test_a_slow_walk_meets_a_resting_copy);
    RUN_TEST(test_marks_spread_through_a_touching_row);
    RUN_TEST(test_the_rest_hold_is_joined_exactly);
    RUN_TEST(test_the_pause_scales_with_the_measured_round_trip);
    RUN_TEST(test_a_take_keeps_the_copys_pose);
    RUN_TEST(test_a_taken_body_never_goes_out_kinematic);
    RUN_TEST(test_a_creeping_pile_stays_in_the_rest_hold);
    RUN_TEST(test_a_return_waits_for_the_hosts_acknowledgement);
    RUN_TEST(test_the_hosts_character_is_stopped_by_a_guests_body);
    RUN_TEST(test_a_state_from_before_the_grant_is_dropped);
    RUN_TEST(test_a_quiet_body_waits_for_its_moving_neighbour);
    RUN_TEST(test_what_the_hosts_agent_sets_going_stays_the_hosts);
    RUN_TEST(test_an_owned_body_never_takes_a_moving_one);
    RUN_TEST(test_the_host_takes_back_two_guests_touching_bodies);
    RUN_TEST(test_a_take_of_a_body_no_agent_touches_is_refused);
    RUN_TEST(test_a_refused_take_is_not_repeated);
    RUN_TEST(test_a_refused_take_does_not_jump);
    RUN_TEST(test_a_take_back_goes_on_from_the_hosts_own_pose);
    RUN_TEST(test_a_take_back_of_a_fast_body_keeps_the_hosts_pose);
    RUN_TEST(test_a_run_into_copies_its_own_push_sets_flying_does_not_fling_her);
    RUN_TEST(test_a_flying_copys_path_through_her_never_places_it_inside_her);
    RUN_TEST(test_another_seats_throw_never_passes_through_this_sides_character);
    RUN_TEST(test_a_body_leaned_on_by_its_owner_never_ping_pongs);
    RUN_TEST(test_a_throw_is_kept_off_its_thrower_until_it_clears_her_on_every_side);
    return UNITY_END();
}
