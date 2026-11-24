#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "stub_wire.h"
#ifdef SEAM_REAL
#include "sync_codec.h"
#endif
#include "unity.h"

/* The stub on three seams that pass bytes by hand: the contract, case by case, on real Box3D
   bodies. Each case names the behaviour it pins, so a stub that loses it fails here. */

enum { BODIES = 300, SEATS = 3, GUEST = 1, OTHER = 2 };
#define CAT_CHARACTER (1ull << 5) /* this endpoint's own characters, as the config names them */

typedef struct side {
    b3WorldId world;
    b3BodyId bodies[BODIES];
    body_sync_t *sync;
    uint8_t seat;
} side_t;

static side_t host, guest, other;
/* The seam suite runs on both implementations: built once with each (SEAM_REAL for the real
   seam). The real one gets a budget that holds every body, so one packet a tick carries what
   the stub's several do; its fates are answered at once. */
#ifdef SEAM_REAL
enum { CAPACITY = 16000 };
#else
enum { CAPACITY = 1200 };
#endif
static uint8_t packet[CAPACITY + 64];
static uint16_t numbers[SEATS][SEATS];
static uint32_t tick_back[SEATS]; /* a seat whose session restarted stamps fewer ticks */
static uint32_t tick_no = 100;
static int box3d_asserts;

static int count_assert(const char *condition, const char *file, int line) {
    (void)condition;
    (void)file;
    (void)line;
    box3d_asserts++;
    return 0;
}

static body_sync_config_t config_for(b3WorldId world, uint8_t seat) {
    return (body_sync_config_t){.world = world,
                                .seat = seat,
                                .seats = SEATS,
                                .tick_hz = 60,
                                .bounds_min = {-100.0f, -20.0f, -100.0f},
                                .bounds_max = {100.0f, 60.0f, 100.0f},
                                .max_speed = 100.0f,
                                .max_spin = 200.0f,
                                .state_budget = CAPACITY,
                                /* The shortest playback delay: the suite delivers every packet in its tick. */
                                .delay_min = 1,
                                .delay_max = 1,
                                .character_category = CAT_CHARACTER};
}

static void build(side_t *s, uint8_t seat) {
    b3WorldDef def = b3DefaultWorldDef();
    def.gravity = (b3Vec3){0.0f, -12.0f, 0.0f};
    s->world = b3CreateWorld(&def);
    s->seat = seat;
    b3BodyDef floor = b3DefaultBodyDef();
    floor.position = (b3Vec3){0.0f, -0.5f, 0.0f};
    b3BodyId ground = b3CreateBody(s->world, &floor);
    b3ShapeDef shape = b3DefaultShapeDef();
    b3BoxHull slab = b3MakeBoxHull(90.0f, 0.5f, 90.0f);
    b3CreateHullShape(ground, &shape, &slab.base);
    b3BoxHull cube = b3MakeBoxHull(0.2f, 0.2f, 0.2f);
    shape.enableContactEvents = true;
    body_sync_config_t config = config_for(s->world, seat);
    s->sync = body_sync_create(&config);
    TEST_ASSERT_NOT_NULL(s->sync);
    for (uint32_t i = 0; i < BODIES; i++) {
        b3BodyDef d = b3DefaultBodyDef();
        d.type = b3_dynamicBody;
        d.position = (b3Vec3){(float)(i % 20) * 1.0f, 0.2f, (float)(i / 20) * 1.0f};
        s->bodies[i] = b3CreateBody(s->world, &d);
        b3CreateHullShape(s->bodies[i], &shape, &cube.base);
        if (i == 12) { /* two shapes: the masks are kept per shape */
            b3BoxHull tall = b3MakeBoxHull(0.1f, 0.3f, 0.1f);
            b3CreateHullShape(s->bodies[i], &shape, &tall.base);
        }
        body_sync_bind_t bind = {.body = s->bodies[i], .body_class = i == 11 ? BODY_SYNC_CLASS_HEAVY : BODY_SYNC_CLASS_LIGHT,
                                 .authority = BODY_SYNC_SEAT_HOST};
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_bind(s->sync, body_sync_id_make(i, 1), &bind));
    }
}

void setUp(void) {
    memset(tick_back, 0, sizeof(tick_back));
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

static uint32_t state(side_t *from, side_t *to) {
    uint32_t size = 0, packets = 0;
    while (body_sync_write_state(from->sync, to->seat, packet, CAPACITY, &size)) {
        TEST_ASSERT_TRUE(size <= CAPACITY && size > 0);
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_state(to->sync, from->seat, tick_no - tick_back[from->seat], packet, size));
        uint16_t n = numbers[from->seat][to->seat]++;
        body_sync_state_sent(from->sync, to->seat, n);
        body_sync_resolve(from->sync, to->seat, n, true);
        packets++;
    }
    return packets;
}

static uint32_t records(side_t *from, side_t *to) {
    uint32_t size = 0, n = 0;
    while (body_sync_peek_record(from->sync, to->seat, packet, sizeof(packet), &size)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_record(to->sync, from->seat, tick_no, packet, size));
        body_sync_pop_record(from->sync, to->seat);
        n++;
    }
    return n;
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

/* A guest claims, the host answers every polled claim with `granted`. */
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

static const body_sync_id_t ID0 = 1u << 16;

/* One host state of one body naming `seat` and `number`, in the linked implementation's bytes. */
static uint32_t one_state(body_sync_id_t id, uint8_t seat, uint16_t number, float x, float y, float z) {
#ifdef SEAM_REAL
    body_sync_config_t c = config_for(host.world, 0);
    sync_limits_t limits = {{c.bounds_min[0], c.bounds_min[1], c.bounds_min[2]}, {c.bounds_max[0], c.bounds_max[1], c.bounds_max[2]}, c.max_speed, c.max_spin};
    sync_codec_t codec;
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &limits));
    sync_record_t r;
    memset(&r, 0, sizeof(r));
    r.slot = (uint16_t)body_sync_id_slot(id);
    r.generation_low = (uint8_t)(body_sync_id_generation(id) & SYNC_GEN_MASK);
    r.position[0] = x;
    r.position[1] = y;
    r.position[2] = z;
    r.rotation[3] = 1.0f;
    r.rare.groups = SYNC_RARE_AUTHORITY;
    r.rare.seat = seat;
    r.rare.number = number;
    TEST_ASSERT_TRUE(sync_codec_limit(&codec, &r));
    sync_packet_writer_t w;
    sync_packet_begin(&w, packet, sizeof(packet));
    TEST_ASSERT_TRUE(sync_packet_add(&w, &codec, &r));
    return sync_packet_finish(&w);
#else
    stub_body_state_t b = {.id = id, .seat = seat, .number = number, .position = {x, y, z}, .rotation = {0, 0, 0, 1}};
    stub_wire_put_body(packet + STUB_HEADER_BYTES, &b);
    stub_wire_put_header(packet, 1);
    return STUB_HEADER_BYTES + STUB_BODY_BYTES;
#endif
}

static void test_every_body_reaches_the_guest_within_the_packet_limit(void) {
    step(&host);
    uint32_t packets = state(&host, &guest);
#ifdef SEAM_REAL
    TEST_ASSERT_EQUAL_UINT32(1, packets); /* a budgeted sender: one packet a tick */
#else
    TEST_ASSERT_EQUAL_UINT32((BODIES + stub_wire_bodies_fit(1200) - 1) / stub_wire_bodies_fit(1200), packets);
    body_sync_traffic_t t;
    TEST_ASSERT_TRUE(body_sync_traffic(host.sync, GUEST, &t));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)BODIES * STUB_COPY_BYTES, t.sent_bytes[BODY_SYNC_TRAFFIC_COPY]);
#endif
    step(&guest);
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, body_sync_id_make(7, 1)));
    TEST_ASSERT_EQUAL(b3_kinematicBody, b3Body_GetType(guest.bodies[7]));
}

/* A copy is driven to its pose at the step's speed, not teleported and not at another rate. */
static void test_copy_is_driven_over_one_step(void) {
    tick_all();
    tick_all(); /* the copy has been placed on its first pose */
    b3Body_SetLinearVelocity(host.bodies[0], (b3Vec3){3.0f, 0.0f, 0.0f});
    tick_no++;
    step(&host);
    state(&host, &guest);
    b3Vec3 target = b3Body_GetPosition(host.bodies[0]);
#ifdef SEAM_REAL
    /* A playback delay of one tick: the copy shows this pose a tick later. */
    step(&guest);
    tick_no++;
    step(&host);
    state(&host, &guest);
#endif
    body_sync_apply(guest.sync, tick_no);
    TEST_ASSERT_TRUE_MESSAGE(speed_of(guest.bodies[0]) > 1.0f, "the copy was snapped, not driven");
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    b3Vec3 at = b3Body_GetPosition(guest.bodies[0]);
#ifdef SEAM_REAL
    const float miss = 0.06f; /* the copy's grid, and playback between two ticks: one tick of motion */
#else
    const float miss = 1e-4f;
#endif
    TEST_ASSERT_TRUE_MESSAGE(fabsf(at.x - target.x) < miss && fabsf(at.y - target.y) < miss, "the copy missed its pose in one step");
}

/* An older state from the same sender never replaces a newer one. */
static void test_an_older_state_is_stale(void) {
    tick_all();
    b3Body_SetTransform(host.bodies[0], (b3Vec3){5.0f, 0.2f, 5.0f}, b3Quat_identity);
    b3Body_SetLinearVelocity(host.bodies[0], (b3Vec3){4.0f, 0.0f, 0.0f}); /* due at once on a budgeted sender too */
    body_sync_collect(host.sync, tick_no);
    tick_no += 10;
    state(&host, &guest);
    b3Body_SetTransform(host.bodies[0], (b3Vec3){9.0f, 0.2f, 9.0f}, b3Quat_identity);
    body_sync_collect(host.sync, tick_no);
    tick_no -= 5;
    state(&host, &guest);
    body_sync_apply(guest.sync, tick_no);
    b3World_Step(guest.world, 1.0f / 60.0f, 4);
    b3Vec3 at = b3Body_GetPosition(guest.bodies[0]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(at.x - 5.0f) < 1e-3f, "an older state moved the copy");
    body_sync_traffic_t t;
    body_sync_traffic(guest.sync, BODY_SYNC_SEAT_HOST, &t);
    TEST_ASSERT_TRUE(t.stale >= 1);
}

/* Claims: the claimant simulates at once, the host takes its poses only after the grant, the
   granted hold is the claim's, a refusal turns the loser back into a copy. */
static void test_claims_answers_and_holds(void) {
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(other.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE));
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    tick_no++;
    step(&guest);
    state(&guest, &host);
    body_sync_traffic_t t;
    body_sync_traffic(host.sync, GUEST, &t);
    TEST_ASSERT_EQUAL_UINT32(1, t.rejected);
    records(&guest, &host);
    records(&other, &host);
    body_sync_claim_event_t first, second, none;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &first));
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &second));
    TEST_ASSERT_FALSE(body_sync_poll_claim(host.sync, &none));
    TEST_ASSERT_EQUAL_UINT8(GUEST, first.seat);
    TEST_ASSERT_EQUAL(BODY_SYNC_HOLD_UNTIL_SETTLED, first.hold);
    TEST_ASSERT_TRUE(first.sequence != 0);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &first, true));
    TEST_ASSERT_EQUAL(BODY_SYNC_UNKNOWN, body_sync_answer(host.sync, &first, true)); /* answered already */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &second, false));
    body_sync_authority_t a;
    body_sync_authority(host.sync, ID0, &a);
    TEST_ASSERT_EQUAL_UINT8(GUEST, a.seat);
    TEST_ASSERT_EQUAL(BODY_SYNC_HOLD_UNTIL_SETTLED, a.hold);
    records(&host, &guest);
    records(&host, &other);
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    TEST_ASSERT_FALSE(body_sync_is_local(other.sync, ID0));
    body_sync_authority(guest.sync, ID0, &a);
    TEST_ASSERT_EQUAL(BODY_SYNC_HOLD_UNTIL_SETTLED, a.hold);
    /* the host now takes the owner's poses, and the owner keeps its own hold against the host's
       state, which still says UNTIL_RELEASE for a moment */
    tick_all();
    body_sync_traffic(host.sync, GUEST, &t);
    TEST_ASSERT_EQUAL_UINT32(1, t.rejected);
    body_sync_authority(guest.sync, ID0, &a);
    TEST_ASSERT_EQUAL(BODY_SYNC_HOLD_UNTIL_SETTLED, a.hold);
}

/* A release that crosses the host's grant leaves the body with the host. */
static void test_a_release_crossing_the_grant_stays_with_the_host(void) {
    tick_all();
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&guest, &host);
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &c));
    body_sync_answer(host.sync, &c, true); /* the grant is on its way */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, ID0));
    records(&host, &guest); /* the grant arrives after the release */
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, ID0));
    records(&guest, &host); /* the release reaches the host */
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    records(&host, &guest);
    for (int i = 0; i < 3; i++) {
        tick_all();
    }
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, ID0));
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
}

/* Claim then release before the host hears either: the claim is withdrawn, never polled, and
   the body stays the host's. */
static void test_a_released_claim_is_withdrawn(void) {
    tick_all();
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_release(guest.sync, ID0);
    records(&guest, &host);
    body_sync_claim_event_t c;
    TEST_ASSERT_FALSE_MESSAGE(body_sync_poll_claim(host.sync, &c), "a withdrawn claim was polled");
    tick_all();
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, ID0));
}

/* The owner's hold is its own: a grant of a claim made with another hold does not overwrite a
   hold the claimant changed while it waited. */
static void test_the_owner_keeps_its_hold_through_the_grant(void) {
    tick_all();
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&guest, &host);
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED); /* thrown before the answer */
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_HOLD_UNTIL_RELEASE, c.hold);
    body_sync_answer(host.sync, &c, true);
    records(&host, &guest);
    body_sync_authority_t a;
    body_sync_authority(guest.sync, ID0, &a);
    TEST_ASSERT_EQUAL_UINT8(GUEST, a.seat);
    TEST_ASSERT_EQUAL(BODY_SYNC_HOLD_UNTIL_SETTLED, a.hold);
}

/* The host sends a guest every body but its own: they would only cost bytes. */
static void test_the_host_does_not_echo_a_guests_bodies(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    body_sync_traffic_t before, after;
    body_sync_traffic(host.sync, GUEST, &before);
    tick_no++;
    step(&host);
    state(&host, &guest);
    body_sync_traffic(host.sync, GUEST, &after);
#ifdef SEAM_REAL
    (void)before;
    (void)after;
    tick_no++;
    b3Body_SetLinearVelocity(host.bodies[5], (b3Vec3){3.0f, 0.0f, 0.0f}); /* something to send */
    step(&host);
    uint32_t size = 0, count = 0;
    TEST_ASSERT_TRUE(body_sync_write_state(host.sync, GUEST, packet, CAPACITY, &size));
    body_sync_config_t c = config_for(host.world, 0);
    sync_limits_t limits = {{c.bounds_min[0], c.bounds_min[1], c.bounds_min[2]}, {c.bounds_max[0], c.bounds_max[1], c.bounds_max[2]}, c.max_speed, c.max_spin};
    static sync_codec_t codec;
    static sync_record_t got[BODIES];
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &limits));
    TEST_ASSERT_TRUE(sync_packet_read(&codec, SEATS, packet, size, got, BODIES, &count, NULL, 0));
    for (uint32_t i = 0; i < count; i++) {
        TEST_ASSERT_TRUE_MESSAGE(got[i].slot != 0, "the host sent a guest its own body");
    }
#else
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(BODIES - 1) * STUB_COPY_BYTES, after.sent_bytes[BODY_SYNC_TRAFFIC_COPY] - before.sent_bytes[BODY_SYNC_TRAFFIC_COPY]);
#endif
}

/* An honest body past the box or the speeds is written at the limits, so the peer accepts it
   and no session closes over it. */
static void test_writes_stay_within_the_limits(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
    b3Body_SetTransform(guest.bodies[0], (b3Vec3){0.0f, -30.0f, 0.0f}, b3Quat_identity);
    body_sync_collect(guest.sync, tick_no);
    tick_no++;
    state(&guest, &host); /* fails inside state() on anything but OK */
    b3Body_SetTransform(host.bodies[5], (b3Vec3){0.0f, 5.0f, 0.0f}, b3Quat_identity);
    b3Body_SetLinearVelocity(host.bodies[5], (b3Vec3){0.0f, -150.0f, 0.0f});
    b3Body_SetAngularVelocity(host.bodies[5], (b3Vec3){300.0f, 0.0f, 0.0f});
    body_sync_collect(host.sync, tick_no);
    tick_no++;
    state(&host, &guest);
    body_sync_traffic_t t;
    body_sync_traffic(host.sync, GUEST, &t);
    TEST_ASSERT_EQUAL_UINT32(0, t.refused);
    body_sync_traffic(guest.sync, BODY_SYNC_SEAT_HOST, &t);
    TEST_ASSERT_EQUAL_UINT32(0, t.refused);
}

/* A grant and a forced move hand the new owner no holder, even of a body the host's own
   character carried; a claimant that set its own holder before the grant keeps it. */
static void test_new_owners_start_without_the_old_carrier(void) {
    tick_all();
    body_sync_set_holder(host.sync, ID0, body_sync_id_make(298, 1));
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_authority_t a;
    body_sync_authority(guest.sync, ID0, &a);
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, a.holder);
    body_sync_set_holder(guest.sync, ID0, body_sync_id_make(297, 1));
    tick_all();
    body_sync_assign(host.sync, ID0, OTHER, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&host, &other);
    records(&host, &guest);
    body_sync_authority(other.sync, ID0, &a);
    TEST_ASSERT_TRUE(body_sync_is_local(other.sync, ID0));
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, a.holder);
    /* the host taking a carried body by its own claim does not inherit the guest's carrier */
    body_sync_set_holder(other.sync, ID0, body_sync_id_make(296, 1));
    tick_all();
    body_sync_claim(host.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_authority(host.sync, ID0, &a);
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, a.holder);
    /* a claimant that set its holder while it waited keeps it through the grant */
    body_sync_id_t id = body_sync_id_make(4, 1);
    body_sync_claim(guest.sync, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_set_holder(guest.sync, id, body_sync_id_make(297, 1));
    records(&guest, &host);
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &c));
    body_sync_answer(host.sync, &c, true);
    records(&host, &guest);
    body_sync_authority(guest.sync, id, &a);
    TEST_ASSERT_EQUAL_UINT32(body_sync_id_make(297, 1), a.holder);
}

/* A release the guest sent before the host moved the body to it by force is stale: it must
   not take the body back. */
static void test_a_stale_release_does_not_undo_a_forced_move(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    body_sync_assign(host.sync, ID0, GUEST, BODY_SYNC_HOLD_UNTIL_SETTLED);
    records(&host, &guest);
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    records(&guest, &host); /* the old release arrives */
    records(&host, &guest);
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    TEST_ASSERT_FALSE(body_sync_is_local(host.sync, ID0));
}

/* A refusal not yet delivered, a release, then a forced move to the same guest: the forced
   move still lands, so the body is never left with nobody simulating it. */
static void test_a_forced_move_lands_after_an_answer(void) {
    tick_all();
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&guest, &host);
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &c));
    body_sync_answer(host.sync, &c, false);
    body_sync_release(guest.sync, ID0);
    records(&guest, &host);
    body_sync_assign(host.sync, ID0, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&host, &guest);
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    TEST_ASSERT_FALSE(body_sync_is_local(host.sync, ID0));
    /* the same with the refusal delivered first */
    tick_all();
    body_sync_assign(host.sync, ID0, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&host, &guest);
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&guest, &host);
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &c));
    body_sync_answer(host.sync, &c, false);
    records(&host, &guest);
    body_sync_assign(host.sync, ID0, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&host, &guest);
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
}

/* A release always withdraws its seat's claim of the body, also when it takes the body back.
   Left pending, the claim would be granted to a guest that reads the grant as stale, and the
   body would be left with nobody simulating it. */
static void test_a_release_that_returns_the_body_withdraws_the_claim_too(void) {
    tick_all();
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&guest, &host); /* pending at the host, not yet polled */
    body_sync_assign(host.sync, ID0, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&host, &guest);
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    records(&guest, &host);
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    body_sync_claim_event_t c;
    TEST_ASSERT_FALSE_MESSAGE(body_sync_poll_claim(host.sync, &c), "a claim outlived its claimant's release");
    records(&host, &guest);
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, ID0));
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
}

/* The answer names the claim it answers: one for an older claim is not the pending one's. */
static void test_an_answer_counts_only_for_its_claim(void) {
    tick_all();
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE); /* sequence 1 */
    body_sync_release(guest.sync, ID0);
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE); /* sequence 2, pending */
    stub_record_t old = {.type = STUB_REC_ASSIGN, .id = ID0, .seat = BODY_SYNC_SEAT_HOST, .number = 0, .sequence = 1};
    uint32_t size = stub_wire_put_record(&old, packet, sizeof(packet));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_record(guest.sync, BODY_SYNC_SEAT_HOST, tick_no, packet, size));
    TEST_ASSERT_TRUE_MESSAGE(body_sync_is_local(guest.sync, ID0), "an answer to claim 1 ended claim 2");
}

/* answer() refuses an event whose claim is no longer pending: superseded, its seat gone, the
   body unbound. */
static void test_answer_refuses_claims_no_longer_pending(void) {
    tick_all();
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&guest, &host);
    body_sync_claim_event_t first, second;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &first));
    body_sync_release(guest.sync, ID0);
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&guest, &host);
    TEST_ASSERT_EQUAL(BODY_SYNC_UNKNOWN, body_sync_answer(host.sync, &first, true));
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &second));
    TEST_ASSERT_TRUE(second.sequence != first.sequence);
    /* a seat that leaves takes its pending claims with it */
    body_sync_claim(other.sync, body_sync_id_make(3, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_claim(other.sync, body_sync_id_make(8, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&other, &host);
    body_sync_claim_event_t polled, none;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &polled));
    body_sync_seat_left(host.sync, OTHER);
    TEST_ASSERT_FALSE(body_sync_poll_claim(host.sync, &none));
    TEST_ASSERT_EQUAL(BODY_SYNC_UNKNOWN, body_sync_answer(host.sync, &polled, true));
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, body_sync_id_make(3, 1)));
    /* an unbound body takes its pending claims with it */
    body_sync_id_t id = body_sync_id_make(6, 1);
    body_sync_claim(guest.sync, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&guest, &host);
    body_sync_unbind(host.sync, id);
    body_sync_bind_t bind = {.body = host.bodies[6], .body_class = BODY_SYNC_CLASS_LIGHT, .authority = BODY_SYNC_SEAT_HOST};
    body_sync_bind(host.sync, body_sync_id_make(6, 2), &bind);
    TEST_ASSERT_FALSE(body_sync_poll_claim(host.sync, &none));
}

/* Claim sequences never reach 0, also across the 16-bit wrap: the host refuses a claim of 0. */
static void test_claim_sequences_skip_zero_at_the_wrap(void) {
    tick_all();
    stub_record_t r;
    char why[96];
    uint32_t size;
    for (uint32_t i = 0; i < 70000; i++) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE));
        TEST_ASSERT_TRUE(body_sync_peek_record(guest.sync, BODY_SYNC_SEAT_HOST, packet, sizeof(packet), &size));
        TEST_ASSERT_TRUE_MESSAGE(stub_wire_read_record(packet, size, SEATS, false, &r, why, sizeof(why)), why);
        TEST_ASSERT_TRUE(r.sequence != 0);
        body_sync_pop_record(guest.sync, BODY_SYNC_SEAT_HOST);
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
        body_sync_pop_record(guest.sync, BODY_SYNC_SEAT_HOST);
    }
}

/* Only an announcement with a newer number moves authority, and a host state never makes a
   guest local: crafted bytes that try either change nothing. */
static void test_authority_moves_only_by_newer_announcements(void) {
    tick_all();
    stub_record_t same = {.type = STUB_REC_ASSIGN, .id = ID0, .seat = GUEST, .number = 0, .hold = 0, .sequence = 0};
    uint32_t size = stub_wire_put_record(&same, packet, sizeof(packet));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_record(guest.sync, BODY_SYNC_SEAT_HOST, tick_no, packet, size));
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_local(guest.sync, ID0), "an equal number moved authority");
    size = one_state(ID0, GUEST, 50, 0.0f, 0.2f, 0.0f);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no + 1, packet, size));
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_local(guest.sync, ID0), "a host state made the guest local");
    same.number = 1;
    size = stub_wire_put_record(&same, packet, sizeof(packet));
    body_sync_receive_record(guest.sync, BODY_SYNC_SEAT_HOST, tick_no, packet, size);
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0)); /* a newer forced move does */
}

/* Claim, release, claim before the host hears any: only the second claim is polled and counts. */
static void test_only_the_newest_claim_counts(void) {
    tick_all();
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_release(guest.sync, ID0);
    body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
    records(&guest, &host);
    body_sync_claim_event_t c, none;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &c));
    TEST_ASSERT_FALSE_MESSAGE(body_sync_poll_claim(host.sync, &none), "a withdrawn claim was polled");
    TEST_ASSERT_EQUAL(BODY_SYNC_HOLD_UNTIL_SETTLED, c.hold);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, true));
    records(&host, &guest);
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    body_sync_authority_t a;
    body_sync_authority(guest.sync, ID0, &a);
    TEST_ASSERT_EQUAL_UINT8(GUEST, a.seat);
}

/* No claim is lost, however many arrive in one tick. */
static void test_every_claim_is_polled(void) {
    tick_all();
    for (uint32_t i = 0; i < 200; i++) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, body_sync_id_make(i, 1), BODY_SYNC_HOLD_UNTIL_RELEASE));
    }
    for (uint32_t i = 200; i < BODIES; i++) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(other.sync, body_sync_id_make(i, 1), BODY_SYNC_HOLD_UNTIL_RELEASE));
    }
    records(&guest, &host);
    records(&other, &host);
    body_sync_claim_event_t c;
    uint32_t polled = 0;
    while (body_sync_poll_claim(host.sync, &c)) {
        polled++;
        body_sync_answer(host.sync, &c, true);
    }
    TEST_ASSERT_EQUAL_UINT32(BODIES, polled);
    for (int k = 0; k < 3; k++) {
        tick_all();
    }
    uint32_t both = 0;
    for (uint32_t i = 0; i < BODIES; i++) {
        side_t *g = i < 200 ? &guest : &other;
        both += body_sync_is_local(g->sync, body_sync_id_make(i, 1)) && body_sync_is_local(host.sync, body_sync_id_make(i, 1)) ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT32(0, both);
}

/* A forced move, a host claim and a left seat all continue the body with its newest velocity. */
static void test_forced_moves_keep_the_velocity(void) {
    for (int how = 0; how < 3; how++) {
        if (how > 0) {
            tearDown();
            setUp();
        }
        tick_all();
        grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
        tick_all();
        b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){0.0f, 8.0f, 0.0f});
        body_sync_collect(guest.sync, tick_no);
        tick_no++;
        state(&guest, &host);
        if (how == 0) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_assign(host.sync, ID0, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE));
        } else if (how == 1) {
            body_sync_claim(host.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
        } else {
            body_sync_seat_left(host.sync, GUEST);
        }
        body_sync_apply(host.sync, tick_no);
        TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
        TEST_ASSERT_TRUE_MESSAGE(speed_of(host.bodies[0]) > 7.0f, "the body lost its velocity at the handover");
    }
}

/* After a revoke and a re-grant, a late pose of the old grant is not taken. */
static void test_a_pose_of_an_old_grant_is_rejected(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetTransform(guest.bodies[0], (b3Vec3){5.0f, 5.0f, 5.0f}, b3Quat_identity);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){4.0f, 0.0f, 0.0f}); /* due at once on a budgeted sender too */
    body_sync_collect(guest.sync, tick_no);
    uint32_t old_size = 0;
    body_sync_write_state(guest.sync, BODY_SYNC_SEAT_HOST, packet, 1200, &old_size);
    uint8_t old[1200];
    memcpy(old, packet, old_size);
    uint32_t old_tick = ++tick_no;
    uint32_t first = old_size;
    while (body_sync_write_state(guest.sync, BODY_SYNC_SEAT_HOST, packet, 1200, &old_size)) {
    }
#ifdef SEAM_REAL
    old_size = first;
#else
    (void)first;
    old_size = STUB_HEADER_BYTES + STUB_BODY_BYTES;
#endif
    body_sync_assign(host.sync, ID0, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_assign(host.sync, ID0, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_traffic_t t0, t1;
    body_sync_traffic(host.sync, GUEST, &t0);
    body_sync_receive_state(host.sync, GUEST, old_tick, old, old_size);
    body_sync_traffic(host.sync, GUEST, &t1);
    TEST_ASSERT_EQUAL_UINT32(t0.rejected + 1, t1.rejected);
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
    /* the owner makes it dynamic; the host gets that type back with the release */
    b3Body_SetType(guest.bodies[0], b3_dynamicBody);
    body_sync_set_flags(guest.sync, ID0, 3);
    body_sync_set_holder(guest.sync, ID0, body_sync_id_make(299, 1));
    tick_all();
    tick_all();
    body_sync_set_holder(guest.sync, ID0, BODY_SYNC_ID_NONE);
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    tick_all();
    tick_all();
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_EQUAL(b3_dynamicBody, b3Body_GetType(host.bodies[0]));
    TEST_ASSERT_EQUAL_UINT8(3, body_sync_flags(host.sync, ID0));
    body_sync_authority_t a;
    body_sync_authority(host.sync, ID0, &a);
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, a.holder);
}

/* A revoke learned from the host's state first, then a new grant: the guest starts from the
   host's holder, not the one it had before. */
static void test_a_revoke_seen_by_state_first_clears_the_holder(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_set_holder(guest.sync, ID0, body_sync_id_make(299, 1));
    tick_all();
    body_sync_assign(host.sync, ID0, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_no++;
    step(&host);
    state(&host, &guest);
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, ID0));
    records(&host, &guest);
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
    tick_all();
    body_sync_authority_t g, h;
    body_sync_authority(guest.sync, ID0, &g);
    body_sync_authority(host.sync, ID0, &h);
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, g.holder);
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, h.holder);
}

/* One judge of contacts: a thrown body goes back to the host on its first contact with a
   moving body, long before it would sleep. */
/* The character that let a body go is no contact that ends its settled hold. */
static void test_a_settled_hold_ignores_its_last_holder(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
    body_sync_set_holder(guest.sync, ID0, body_sync_id_make(1, 1));
    body_sync_set_holder(guest.sync, ID0, BODY_SYNC_ID_NONE);
    body_sync_apply(guest.sync, tick_no);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){6.0f, 0.0f, 0.0f}); /* into body 1 */
    for (int ticks = 0; ticks < 20; ticks++) {
        tick_no++;
        b3World_Step(guest.world, 1.0f / 60.0f, 4);
        body_sync_collect(guest.sync, tick_no);
    }
    TEST_ASSERT_TRUE_MESSAGE(body_sync_is_local(guest.sync, ID0), "a touch of the last holder ended the hold");
}

static void test_a_settled_hold_ends_at_the_first_contact(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
    body_sync_apply(guest.sync, tick_no);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){6.0f, 0.0f, 0.0f});
    int ticks = 0;
    for (; ticks < 30 && body_sync_is_local(guest.sync, ID0); ticks++) {
        tick_no++;
        b3World_Step(guest.world, 1.0f / 60.0f, 4);
        body_sync_collect(guest.sync, tick_no);
    }
    TEST_ASSERT_FALSE_MESSAGE(body_sync_is_local(guest.sync, ID0), "the hold did not end at the contact");
    TEST_ASSERT_TRUE(b3Body_IsAwake(guest.bodies[0]));
    records(&guest, &host);
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
}

static void test_a_settled_hold_ends_at_rest(void) {
    body_sync_id_t id = body_sync_id_make(5, 1);
    grant(&guest, id, BODY_SYNC_HOLD_UNTIL_SETTLED);
    for (int i = 0; i < 300 && body_sync_is_local(guest.sync, id); i++) {
        tick_no++;
        step(&guest);
    }
    TEST_ASSERT_FALSE(body_sync_is_local(guest.sync, id));
    records(&guest, &host);
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, id));
}

static void test_stale_generations_left_seats_and_refusals(void) {
    body_sync_id_t old = body_sync_id_make(9, 1);
    step(&host);
    uint32_t size;
    TEST_ASSERT_TRUE(body_sync_write_state(host.sync, GUEST, packet, 1200, &size));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_unbind(guest.sync, old));
    body_sync_bind_t bind = {.body = guest.bodies[9], .body_class = BODY_SYNC_CLASS_LIGHT, .authority = BODY_SYNC_SEAT_HOST, .number = 7};
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_bind(guest.sync, body_sync_id_make(9, 2), &bind));
    body_sync_authority_t a;
    body_sync_authority(guest.sync, body_sync_id_make(9, 2), &a);
    TEST_ASSERT_EQUAL_UINT16(7, a.number);
    TEST_ASSERT_EQUAL(BODY_SYNC_TAKEN, body_sync_bind(guest.sync, body_sync_id_make(9, 3), &bind));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, 5, packet, size));
    body_sync_traffic_t t;
    body_sync_traffic(guest.sync, BODY_SYNC_SEAT_HOST, &t);
    TEST_ASSERT_EQUAL_UINT32(1, t.stale);
    TEST_ASSERT_EQUAL(BODY_SYNC_UNKNOWN, body_sync_claim(guest.sync, old, BODY_SYNC_HOLD_UNTIL_RELEASE));

    body_sync_id_t id = body_sync_id_make(2, 1);
    body_sync_assign(host.sync, id, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    TEST_ASSERT_FALSE(body_sync_is_local(host.sync, id));
    body_sync_seat_left(host.sync, GUEST);
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, id));
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, body_sync_assign(host.sync, id, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE));
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, body_sync_assign(guest.sync, id, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE));
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, body_sync_set_flags(host.sync, id, 0x10));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_flags(host.sync, id, 0x05));
    TEST_ASSERT_EQUAL_UINT8(0x05, body_sync_flags(host.sync, id));

    /* a guest without a host can neither claim nor release */
    body_sync_claim(other.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_seat_left(other.sync, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, body_sync_release(other.sync, ID0));
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, body_sync_claim(other.sync, body_sync_id_make(3, 1), BODY_SYNC_HOLD_UNTIL_RELEASE));
}

/* A full queue answers FULL on the guest; the host owes and never drops an announcement. */
static void test_queues_never_drop(void) {
    tick_all();
    body_sync_result_t r = BODY_SYNC_OK;
    uint32_t taken = 0;
    for (uint32_t i = 0; i < BODIES && r == BODY_SYNC_OK; i++) {
        r = body_sync_claim(guest.sync, body_sync_id_make(i, 1), BODY_SYNC_HOLD_UNTIL_RELEASE);
        taken += r == BODY_SYNC_OK ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL(BODY_SYNC_FULL, r);
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_RECORDS_QUEUED, taken);
    for (uint32_t i = 0; i < BODIES; i++) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_assign(host.sync, body_sync_id_make(i, 1), OTHER, BODY_SYNC_HOLD_UNTIL_RELEASE));
    }
    TEST_ASSERT_EQUAL_UINT32(BODIES, records(&host, &other));
    for (uint32_t i = 0; i < BODIES; i++) {
        TEST_ASSERT_TRUE(body_sync_is_local(other.sync, body_sync_id_make(i, 1)));
    }
}

/* Truncations, bit flips and random bytes on both receive calls: OK or FORMAT, nothing else,
   and every accepted packet goes through apply, a step and collect without a Box3D assert. */
static void test_receive_fuzz(void) {
    tick_all();
    step(&host);
    uint32_t size;
    TEST_ASSERT_TRUE(body_sync_write_state(host.sync, GUEST, packet, 1200, &size));
    uint8_t valid[1200], buffer[1300];
    memcpy(valid, packet, size);
    uint64_t seed = 0x1234;
    for (uint32_t cut = 0; cut < size; cut++) {
        TEST_ASSERT_EQUAL(BODY_SYNC_FORMAT, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, 1, valid, cut));
    }
    for (int i = 0; i < 3000; i++) {
        memcpy(buffer, valid, size);
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        buffer[(seed >> 33) % size] ^= (uint8_t)(1u << ((seed >> 20) % 8));
        body_sync_result_t r = body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no + (uint32_t)i, buffer, size);
        TEST_ASSERT_TRUE(r == BODY_SYNC_OK || r == BODY_SYNC_FORMAT);
        if (r == BODY_SYNC_OK && i % 8 == 0) {
            step(&guest);
        }
    }
#ifndef SEAM_REAL /* the stub's own byte layout: the codec's suite covers the real one */
    /* rotations at the edge of the tolerance reach Box3D unit */
    for (int i = 0; i < 64; i++) {
        memcpy(buffer, valid, size);
        float w = 1.0f + 0.0099f * ((float)(i % 3) - 1.0f);
        memcpy(buffer + STUB_HEADER_BYTES + 6 + 12 + 12, &w, 4); /* id, flags, locks, position, rotation xyz */
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, tick_no + 5000u + (uint32_t)i, buffer, size));
        b3Body_SetTransform(guest.bodies[0], (b3Vec3){30.0f, 5.0f, 30.0f}, b3Quat_identity);
        step(&guest);
    }
#endif
    for (int i = 0; i < 3000; i++) {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        uint32_t n = (uint32_t)((seed >> 33) % sizeof(buffer));
        for (uint32_t k = 0; k < n; k++) {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            buffer[k] = (uint8_t)(seed >> 56);
        }
        body_sync_result_t r = body_sync_receive_state(guest.sync, BODY_SYNC_SEAT_HOST, 3, buffer, n);
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
    stub_record_t assign = {.type = STUB_REC_ASSIGN, .id = body_sync_id_make(1, 1), .seat = GUEST, .number = 9};
    uint32_t rs = stub_wire_put_record(&assign, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(BODY_SYNC_FORMAT, body_sync_receive_record(host.sync, GUEST, 4, buffer, rs));
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, body_sync_receive_state(guest.sync, OTHER, 4, valid, size));
    body_sync_traffic_t t;
    body_sync_traffic(guest.sync, BODY_SYNC_SEAT_HOST, &t);
    TEST_ASSERT_TRUE(t.refused >= size);
}

/* Motion locks travel with the pose: whoever simulates a body next continues the last owner's
   locks, never ones this endpoint held before the body was a copy. */
static const b3MotionLocks ANGULAR_LOCKED = {.angularX = true, .angularY = true, .angularZ = true};

static int angular_locks(const side_t *s) {
    b3MotionLocks l = b3Body_GetMotionLocks(s->bodies[0]);
    return l.angularX + l.angularY + l.angularZ;
}

static void lock_everywhere(void) {
    b3Body_SetMotionLocks(host.bodies[0], ANGULAR_LOCKED);
    b3Body_SetMotionLocks(guest.bodies[0], ANGULAR_LOCKED);
    b3Body_SetMotionLocks(other.bodies[0], ANGULAR_LOCKED);
    tick_all();
}

static void test_locks_follow_the_last_owner_on_a_grant(void) {
    lock_everywhere();
    TEST_ASSERT_EQUAL_INT(0, angular_locks(&guest)); /* a copy turns freely to its owner's rotation */
    b3Body_SetMotionLocks(host.bodies[0], (b3MotionLocks){0}); /* the host frees them: a knock */
    tick_all();
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    TEST_ASSERT_EQUAL_INT(0, angular_locks(&guest));
    b3Body_SetMotionLocks(guest.bodies[0], ANGULAR_LOCKED);
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    tick_all();
    tick_all();
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_EQUAL_INT(3, angular_locks(&host));
}

static void test_locks_follow_the_last_owner_when_a_settled_hold_returns(void) {
    lock_everywhere();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
    tick_all();
    b3Body_SetMotionLocks(guest.bodies[0], (b3MotionLocks){0});
    tick_all();
    for (int i = 0; i < 300 && body_sync_is_local(guest.sync, ID0); i++) {
        tick_all();
    }
    tick_all();
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_EQUAL_INT(0, angular_locks(&host));
}

static void test_locks_follow_the_last_owner_when_its_seat_leaves(void) {
    lock_everywhere();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetMotionLocks(guest.bodies[0], (b3MotionLocks){0});
    tick_all();
    body_sync_seat_left(host.sync, GUEST);
    tick_no++;
    step(&host); /* the guest is gone: nothing more from it */
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_EQUAL_INT(0, angular_locks(&host));
}

static void test_locks_follow_the_last_owner_on_a_forced_move(void) {
    lock_everywhere();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    b3Body_SetMotionLocks(guest.bodies[0], (b3MotionLocks){0});
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_assign(host.sync, ID0, OTHER, BODY_SYNC_HOLD_UNTIL_RELEASE));
    records(&host, &guest);
    records(&host, &other);
    tick_all();
    tick_all();
    TEST_ASSERT_TRUE(body_sync_is_local(other.sync, ID0));
    TEST_ASSERT_EQUAL_INT(0, angular_locks(&other));
}

/* Steps every world with no state packet delivered: only reliable records move. */
static void tick_dark(void) {
    tick_no++;
    step(&host);
    step(&guest);
    step(&other);
}

/* A handover does not wait for the state that would carry the body's flags, type and locks:
   the grant and the release carry them. */
static void test_a_handover_carries_flags_type_and_locks(void) {
    tick_all();
    tick_all();
    body_sync_set_flags(host.sync, ID0, 6);
    b3Body_SetType(host.bodies[0], b3_kinematicBody);
    b3Body_SetMotionLocks(host.bodies[0], (b3MotionLocks){.angularY = true});
    tick_dark();
    tick_dark();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_dark();
    TEST_ASSERT_TRUE(body_sync_is_local(guest.sync, ID0));
    TEST_ASSERT_EQUAL_UINT8(6, body_sync_flags(guest.sync, ID0));
    TEST_ASSERT_EQUAL(b3_kinematicBody, b3Body_GetType(guest.bodies[0]));
    b3MotionLocks l = b3Body_GetMotionLocks(guest.bodies[0]);
    TEST_ASSERT_TRUE(l.angularY && !l.angularX && !l.angularZ);
    /* and back: the guest changes them and releases before any state of it reaches the host */
    body_sync_set_flags(guest.sync, ID0, 9);
    b3Body_SetType(guest.bodies[0], b3_dynamicBody);
    b3Body_SetMotionLocks(guest.bodies[0], (b3MotionLocks){.angularX = true, .angularZ = true});
    tick_dark();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    records(&guest, &host);
    tick_dark();
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_EQUAL_UINT8(9, body_sync_flags(host.sync, ID0));
    TEST_ASSERT_EQUAL(b3_dynamicBody, b3Body_GetType(host.bodies[0]));
    l = b3Body_GetMotionLocks(host.bodies[0]);
    TEST_ASSERT_TRUE(l.angularX && l.angularZ && !l.angularY);
}

/* A late state packet arriving after a newer one changes nothing: it neither counts as a pose
   nor moves back the authority number its sender declared. */
static void test_a_late_packet_changes_nothing(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){2.0f, 0.0f, 0.0f});
    tick_no++;
    step(&host);
    step(&guest);
    uint32_t old_size = 0;
    TEST_ASSERT_TRUE(body_sync_write_state(guest.sync, BODY_SYNC_SEAT_HOST, packet, CAPACITY, &old_size));
    static uint8_t old[CAPACITY];
    memcpy(old, packet, old_size);
    uint32_t old_tick = tick_no;
    body_sync_assign(host.sync, ID0, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_assign(host.sync, ID0, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE);
    records(&host, &guest);
    tick_all(); /* the guest declares the new number */
    tick_all();
    body_sync_traffic_t t0, t1;
    body_sync_receive_state(host.sync, GUEST, old_tick, old, old_size); /* itself stale or rejected */
    body_sync_traffic(host.sync, GUEST, &t0);
    for (int i = 0; i < 6; i++) {
        b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){2.0f + (float)(i % 2), 0.0f, 0.0f});
        tick_all();
    }
    body_sync_traffic(host.sync, GUEST, &t1);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(t0.rejected, t1.rejected, "a late packet made the host reject the owner's poses");
    b3Vec3 g = b3Body_GetPosition(guest.bodies[0]), h = b3Body_GetPosition(host.bodies[0]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(g.x - h.x) < 0.2f, "the host's copy froze");
}

/* A copy holds still without news, and one placed far away does not drift on the velocity it
   was driven with before. */
static void test_a_copy_stops_without_news_and_after_a_jump(void) {
    tick_all();
    tick_all();
    for (int i = 0; i < 3; i++) {
        b3Body_SetLinearVelocity(host.bodies[0], (b3Vec3){3.0f, 0.0f, 0.0f});
        tick_all();
    }
    TEST_ASSERT_TRUE(speed_of(guest.bodies[0]) > 1.0f); /* driven */
    b3Vec3 last = b3Body_GetPosition(guest.bodies[0]);
    tick_dark();
    tick_dark();
    b3Vec3 now = b3Body_GetPosition(guest.bodies[0]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(now.x - last.x) < 0.06f, "a copy without news coasted");
    tick_dark();
    TEST_ASSERT_TRUE_MESSAGE(fabsf(b3Body_GetPosition(guest.bodies[0]).x - now.x) < 1e-5f, "a copy without news coasted");
    for (int i = 0; i < 3; i++) {
        b3Body_SetLinearVelocity(host.bodies[0], (b3Vec3){3.0f, 0.0f, 0.0f});
        tick_all();
    }
    b3Body_SetTransform(host.bodies[0], (b3Vec3){12.0f, 0.2f, 12.0f}, b3Quat_identity);
    b3Body_SetLinearVelocity(host.bodies[0], (b3Vec3){0.0f, 0.0f, 0.0f});
    tick_all();
    tick_all(); /* the copy jumps, and steps once from there */
#ifdef SEAM_REAL
    for (int i = 0; i < 6; i++) {
        tick_all(); /* playback delay, and the pace catching up the dark ticks */
    }
#endif
    TEST_ASSERT_TRUE_MESSAGE(fabsf(b3Body_GetPosition(guest.bodies[0]).x - 12.0f) < 0.02f, "a placed copy drifted on its old velocity");
}

/* A guest that leaves and comes back with its tick count started over is heard again. */
static void test_a_rejoined_guest_with_new_ticks_is_heard(void) {
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    for (int i = 0; i < 4; i++) {
        b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){1.0f, 0.0f, 0.0f});
        tick_all();
    }
    body_sync_seat_left(host.sync, GUEST);
    body_sync_seat_joined(host.sync, GUEST);
    body_sync_destroy(guest.sync);
    body_sync_config_t config = config_for(guest.world, GUEST);
    guest.sync = body_sync_create(&config);
    TEST_ASSERT_NOT_NULL(guest.sync);
    for (uint32_t i = 0; i < BODIES; i++) {
        body_sync_bind_t bind = {.body = guest.bodies[i], .body_class = BODY_SYNC_CLASS_LIGHT, .authority = BODY_SYNC_SEAT_HOST};
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_bind(guest.sync, body_sync_id_make(i, 1), &bind));
    }
    body_sync_seat_joined(guest.sync, BODY_SYNC_SEAT_HOST);
    tick_back[GUEST] = 90; /* its new session counts from far below the old one */
    numbers[GUEST][BODY_SYNC_SEAT_HOST] = 0;
    numbers[BODY_SYNC_SEAT_HOST][GUEST] = 0;
    tick_all();
    tick_all();
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE);
    body_sync_traffic_t t0, t1;
    body_sync_traffic(host.sync, GUEST, &t0);
    for (int i = 0; i < 8; i++) {
        b3Body_SetLinearVelocity(guest.bodies[0], (b3Vec3){-1.5f, 0.0f, 0.0f});
        tick_all();
    }
    body_sync_traffic(host.sync, GUEST, &t1);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(t0.stale, t1.stale, "the rejoined guest's packets read as stale");
    b3Vec3 g = b3Body_GetPosition(guest.bodies[0]), h = b3Body_GetPosition(host.bodies[0]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(g.x - h.x) < 0.1f, "the host froze the rejoined guest's body");
}

/* A claimant keeps the motion type it set itself, and takes the locks the host changed. */
static void test_a_claimant_keeps_its_own_type_and_takes_the_hosts_locks(void) {
    b3Body_SetMotionLocks(host.bodies[0], ANGULAR_LOCKED);
    tick_all();
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE));
    records(&guest, &host);
    tick_no++;
    step(&guest); /* the take resumes */
    b3Body_SetType(guest.bodies[0], b3_kinematicBody); /* it carries the body */
    body_sync_collect(guest.sync, tick_no);
    b3Body_SetMotionLocks(host.bodies[0], (b3MotionLocks){0}); /* the host knocks it */
    step(&host);
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, true));
    records(&host, &guest);
    tick_no++;
    step(&guest);
    TEST_ASSERT_EQUAL_MESSAGE(b3_kinematicBody, b3Body_GetType(guest.bodies[0]), "the grant took the claimant's own type");
    b3MotionLocks l = b3Body_GetMotionLocks(guest.bodies[0]);
    TEST_ASSERT_FALSE_MESSAGE(l.angularX || l.angularY || l.angularZ, "the claimant kept locks the host had freed");
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    records(&guest, &host);
    tick_no++;
    step(&host);
    l = b3Body_GetMotionLocks(host.bodies[0]);
    TEST_ASSERT_FALSE_MESSAGE(l.angularX || l.angularY || l.angularZ, "the host's body came back locked");
}

/* A copy's hold is the owner's, from its state; a holder is named only by a body of that
   generation. */
static void test_a_copy_reads_hold_and_holder_from_its_owner(void) {
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(host.sync, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_holder(host.sync, ID0, body_sync_id_make(5, 2))); /* slot 5 holds generation 1 */
    b3Body_SetLinearVelocity(host.bodies[0], (b3Vec3){0.0f, 3.0f, 0.0f});
    tick_all();
    tick_all();
    body_sync_authority_t a;
    TEST_ASSERT_TRUE(body_sync_authority(guest.sync, ID0, &a));
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_HOLD_UNTIL_SETTLED, a.hold, "a copy's hold did not come from its owner");
    TEST_ASSERT_TRUE_MESSAGE(a.holder == BODY_SYNC_ID_NONE || a.holder == body_sync_id_make(5, 2), "a holder of another generation was taken");
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_holder(host.sync, ID0, body_sync_id_make(5, 1)));
    tick_all();
    tick_all();
    TEST_ASSERT_TRUE(body_sync_authority(guest.sync, ID0, &a));
    TEST_ASSERT_EQUAL_UINT32(body_sync_id_make(5, 1), a.holder);
}

/* A slot bound again starts over: the new body's rest pose goes out though the old body's was
   acknowledged at a pose the new one does not have. */
static void test_a_slot_bound_again_starts_over(void) {
    for (int i = 0; i < 60; i++) {
        tick_all();
    }
    body_sync_id_t old = body_sync_id_make(7, 1), young = body_sync_id_make(7, 2);
    side_t *sides[] = {&host, &guest, &other};
    for (int k = 0; k < 3; k++) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_unbind(sides[k]->sync, old));
    }
    b3Body_SetTransform(host.bodies[7], (b3Vec3){7.0f, 0.2f, 9.0f}, b3Quat_identity);
    b3Body_SetAwake(host.bodies[7], false);
    for (int k = 0; k < 3; k++) {
        body_sync_bind_t bind = {.body = sides[k]->bodies[7], .body_class = BODY_SYNC_CLASS_LIGHT, .authority = BODY_SYNC_SEAT_HOST};
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_bind(sides[k]->sync, young, &bind));
    }
    for (int i = 0; i < 4; i++) {
        tick_all();
    }
    b3Vec3 g = b3Body_GetPosition(guest.bodies[7]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(g.x - 7.0f) < 0.01f && fabsf(g.z - 9.0f) < 0.01f, "the new body was never sent: the slot kept the old one's memory");
}

/* What the host changes in the tick it answers or moves goes with the record. */
static void test_a_grant_carries_the_hosts_change_of_that_tick(void) {
    b3Body_SetMotionLocks(host.bodies[0], ANGULAR_LOCKED);
    tick_all();
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE));
    records(&guest, &host);
    b3Body_SetMotionLocks(host.bodies[0], (b3MotionLocks){0}); /* freed, then answered, no collect between */
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, true));
    records(&host, &guest);
    tick_no++;
    step(&guest);
    b3MotionLocks l = b3Body_GetMotionLocks(guest.bodies[0]);
    TEST_ASSERT_FALSE_MESSAGE(l.angularX || l.angularY || l.angularZ, "the grant carried the locks of the last collect");
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    records(&guest, &host);
    tick_all();
    b3Body_SetType(host.bodies[0], b3_kinematicBody); /* changed, then moved by force at once */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_assign(host.sync, ID0, GUEST, BODY_SYNC_HOLD_UNTIL_RELEASE));
    records(&host, &guest);
    tick_no++;
    step(&guest);
    TEST_ASSERT_EQUAL_MESSAGE(b3_kinematicBody, b3Body_GetType(guest.bodies[0]), "the forced move carried the type of the last collect");
}

/* A claimant keeps the flag bits it set itself; the others come from the grant. */
static void test_a_claimant_keeps_its_own_flag_bits_only(void) {
    tick_all();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_claim(guest.sync, ID0, BODY_SYNC_HOLD_UNTIL_RELEASE));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_flags(guest.sync, ID0, (uint8_t)(body_sync_flags(guest.sync, ID0) | 2u)));
    records(&guest, &host);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_flags(host.sync, ID0, (uint8_t)(body_sync_flags(host.sync, ID0) | 1u))); /* a knock */
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(body_sync_poll_claim(host.sync, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_answer(host.sync, &c, true));
    records(&host, &guest);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(3, body_sync_flags(guest.sync, ID0), "the claimant's own bit or the host's knock was lost");
}

static uint64_t mask_of(b3BodyId body) {
    b3ShapeId shape;
    TEST_ASSERT_EQUAL_INT(1, b3Body_GetShapes(body, &shape, 1));
    return b3Shape_GetFilter(shape).maskBits;
}

/* Opaque data goes out from the owner and reads at the copy; a LIGHT copy stops colliding with
   this endpoint's own characters, and its mask comes back exactly when it is taken. */
static void test_data_travels_and_a_light_copy_lets_the_own_character_pass(void) {
    const body_sync_id_t id = body_sync_id_make(5, 1);
    b3ShapeId shape;
    b3Body_GetShapes(guest.bodies[5], &shape, 1);
    b3Filter own = b3Shape_GetFilter(shape);
    own.maskBits = 0x00FF00FF000000F0ull | CAT_CHARACTER;
    b3Shape_SetFilter(shape, own, false);
    tick_all();
    TEST_ASSERT_TRUE(mask_of(guest.bodies[5]) == (own.maskBits & ~CAT_CHARACTER));
    TEST_ASSERT_TRUE(mask_of(host.bodies[5]) == B3_DEFAULT_MASK_BITS); /* local: untouched */
    const uint8_t bytes[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_data(host.sync, id, bytes, 3));
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, body_sync_set_data(guest.sync, id, bytes, 3));
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, body_sync_set_data(host.sync, id, bytes, BODY_SYNC_DATA_MAX + 1));
    for (int i = 0; i < 4; i++) {
        b3Body_SetLinearVelocity(host.bodies[5], (b3Vec3){0.5f, 0.0f, 0.0f});
        tick_all();
    }
    uint8_t got[BODY_SYNC_DATA_MAX] = {0};
    TEST_ASSERT_EQUAL_UINT32(3, body_sync_data(guest.sync, id, got, sizeof(got)));
    TEST_ASSERT_EQUAL_MEMORY(bytes, got, 3);
    TEST_ASSERT_EQUAL_UINT32(2, body_sync_data(guest.sync, id, got, 2));
    /* The guest's own character walks through the light copy; the host simulates the box. */
    b3BodyDef d = b3DefaultBodyDef();
    d.type = b3_dynamicBody;
    d.gravityScale = 0.0f;
    b3Vec3 at = b3Body_GetPosition(guest.bodies[5]);
    d.position = (b3Vec3){at.x - 0.8f, 0.3f, at.z};
    b3BodyId walker = b3CreateBody(guest.world, &d);
    b3ShapeDef sd = b3DefaultShapeDef();
    sd.filter.categoryBits = CAT_CHARACTER;
    b3BoxHull cube = b3MakeBoxHull(0.2f, 0.2f, 0.2f);
    b3CreateHullShape(walker, &sd, &cube.base);
    for (int i = 0; i < 45; i++) {
        b3Body_SetLinearVelocity(walker, (b3Vec3){2.0f, 0.0f, 0.0f});
        tick_all();
    }
    TEST_ASSERT_TRUE_MESSAGE(b3Body_GetPosition(walker).x > at.x + 0.5f, "the own character was blocked by a light copy");
    b3DestroyBody(walker);
    grant(&guest, id, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    TEST_ASSERT_TRUE_MESSAGE(mask_of(guest.bodies[5]) == own.maskBits, "the mask did not come back exactly");
    TEST_ASSERT_EQUAL_UINT32(3, body_sync_data(guest.sync, id, got, sizeof(got))); /* the owner's bytes carry on */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_set_data(guest.sync, id, bytes + 4, 2));
    for (int i = 0; i < 3; i++) {
        tick_all();
    }
    TEST_ASSERT_EQUAL_UINT32(2, body_sync_data(host.sync, id, got, sizeof(got)));
    TEST_ASSERT_EQUAL_UINT8(5, got[0]);
    double owner_tick = -1.0;
    TEST_ASSERT_TRUE(body_sync_playback(host.sync, id, &owner_tick) && owner_tick >= 0.0);
    TEST_ASSERT_TRUE(body_sync_playback(guest.sync, id, &owner_tick) && owner_tick == (double)tick_no);
}

static uint64_t mask_at(b3BodyId body, int index) {
    b3ShapeId shapes[4];
    TEST_ASSERT_TRUE(b3Body_GetShapes(body, shapes, 4) > index);
    return b3Shape_GetFilter(shapes[index]).maskBits;
}

/* The seam takes the character bits off a LIGHT copy's shapes, each one, and puts back exactly
   those bits: what the caller did to the filter meanwhile stays, a shape added meanwhile is
   the caller's, a HEAVY copy keeps its masks, and unbind and a second copy entry do the same.
   Shapes are named by id: Box3D lists a new shape first. */
static uint64_t mask_of_shape(b3ShapeId shape) { return b3Shape_GetFilter(shape).maskBits; }

static void set_shape_mask(b3ShapeId shape, uint64_t mask) {
    b3Filter f = b3Shape_GetFilter(shape);
    f.maskBits = mask;
    b3Shape_SetFilter(shape, f, false);
}

static void test_mask_bits_are_taken_and_given_back_exactly(void) {
    const body_sync_id_t two = body_sync_id_make(12, 1), carried = body_sync_id_make(14, 1);
    b3ShapeId pair[2], held;
    TEST_ASSERT_EQUAL_INT(2, b3Body_GetShapes(guest.bodies[12], pair, 2));
    TEST_ASSERT_EQUAL_INT(1, b3Body_GetShapes(guest.bodies[14], &held, 1));
    set_shape_mask(held, 0); /* the caller's held filter when the copy starts */
    tick_all();
    TEST_ASSERT_TRUE(mask_of_shape(pair[0]) == (B3_DEFAULT_MASK_BITS & ~CAT_CHARACTER));
    TEST_ASSERT_TRUE(mask_of_shape(pair[1]) == (B3_DEFAULT_MASK_BITS & ~CAT_CHARACTER));
    TEST_ASSERT_TRUE_MESSAGE(mask_at(guest.bodies[11], 0) == B3_DEFAULT_MASK_BITS, "a heavy copy lost its character bit");
    /* Meanwhile the caller changes other bits, adds a shape and drops its held filter. */
    set_shape_mask(pair[0], (B3_DEFAULT_MASK_BITS & ~CAT_CHARACTER) & ~1ull);
    b3ShapeDef sd = b3DefaultShapeDef();
    sd.filter.maskBits = 0x0Full;
    b3BoxHull small = b3MakeBoxHull(0.05f, 0.05f, 0.05f);
    b3ShapeId added = b3CreateHullShape(guest.bodies[12], &sd, &small.base);
    set_shape_mask(held, B3_DEFAULT_MASK_BITS & ~CAT_CHARACTER);
    grant(&guest, two, BODY_SYNC_HOLD_UNTIL_RELEASE);
    grant(&guest, carried, BODY_SYNC_HOLD_UNTIL_RELEASE);
    tick_all();
    TEST_ASSERT_TRUE_MESSAGE(mask_of_shape(pair[0]) == (B3_DEFAULT_MASK_BITS & ~1ull), "the caller's change was overwritten");
    TEST_ASSERT_TRUE(mask_of_shape(pair[1]) == B3_DEFAULT_MASK_BITS);
    TEST_ASSERT_TRUE_MESSAGE(mask_of_shape(added) == 0x0Full, "a shape added during the copy was changed");
    /* It had no character bit when the copy began: the seam took none and gives none back. */
    TEST_ASSERT_TRUE_MESSAGE(mask_of_shape(held) == (B3_DEFAULT_MASK_BITS & ~CAT_CHARACTER), "an old snapshot of the held filter came back");
    /* Released, it is a copy again: the bits go again; unbind gives them back. */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, two));
    records(&guest, &host);
    tick_all();
    tick_all();
    TEST_ASSERT_TRUE_MESSAGE((mask_of_shape(pair[1]) & CAT_CHARACTER) == 0, "a second copy entry kept the bit");
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_unbind(guest.sync, two));
    TEST_ASSERT_TRUE_MESSAGE(mask_of_shape(pair[1]) == B3_DEFAULT_MASK_BITS, "unbind kept the bit off");
}

/* With character_category set, a body of more shapes than the seam keeps masks for is refused. */
static void test_bind_refuses_too_many_shapes(void) {
    b3BodyDef d = b3DefaultBodyDef();
    d.type = b3_dynamicBody;
    d.position = (b3Vec3){-20.0f, 0.5f, -20.0f};
    b3BodyId many = b3CreateBody(guest.world, &d);
    b3ShapeDef sd = b3DefaultShapeDef();
    b3BoxHull cube = b3MakeBoxHull(0.1f, 0.1f, 0.1f);
    for (int i = 0; i < BODY_SYNC_SHAPES_MAX + 1; i++) {
        b3CreateHullShape(many, &sd, &cube.base);
    }
    body_sync_bind_t bind = {.body = many, .body_class = BODY_SYNC_CLASS_LIGHT, .authority = BODY_SYNC_SEAT_HOST};
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, body_sync_bind(guest.sync, body_sync_id_make(BODIES + 5, 1), &bind));
}

/* Destroying the seam gives the bits back too. */
static void test_destroy_gives_the_mask_bits_back(void) {
    tick_all();
    TEST_ASSERT_TRUE((mask_at(guest.bodies[20], 0) & CAT_CHARACTER) == 0);
    body_sync_destroy(guest.sync);
    uint64_t after = mask_at(guest.bodies[20], 0);
    body_sync_config_t config = config_for(guest.world, GUEST);
    guest.sync = body_sync_create(&config); /* tearDown destroys it, whatever the checks find */
    TEST_ASSERT_NOT_NULL(guest.sync);
    TEST_ASSERT_TRUE_MESSAGE(after == B3_DEFAULT_MASK_BITS, "destroy kept the character bit off");
}

/* The contact gate end to end: the guest's release travels in the seam's records, its claim of
   the contact that ended the flight after them, so the host has the body back before it judges
   the claim, and the claim still counts. The host's world around it is measured. */
static void test_a_contact_claim_counts_after_its_release(void) {
    tick_all();
    body_sync_id_t target = body_sync_id_make(1, 1);
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
    uint16_t n = 0;
    TEST_ASSERT_TRUE(body_sync_contact_number(guest.sync, ID0, &n));
    TEST_ASSERT_FALSE(body_sync_contact_number(other.sync, ID0, &n) || body_sync_contact_number(guest.sync, target, &n)); /* copies */
    TEST_ASSERT_TRUE(body_sync_contact_number(guest.sync, ID0, &n));
    body_sync_contact_t c;
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, body_sync_check_contact(host.sync, GUEST, ID0, n, target, &c));
    /* neighbours a metre apart, 0.4 m cubes, nearly still: a finite gap under a metre */
    TEST_ASSERT_TRUE(c.gap >= 0.0f && c.gap < 1.0f);
    TEST_ASSERT_TRUE(c.reach >= 0.15f && isfinite(c.reach));
    TEST_ASSERT_TRUE(c.approach_max >= 0.0f && isfinite(c.approach_max));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    records(&guest, &host);
    TEST_ASSERT_TRUE(body_sync_is_local(host.sync, ID0));
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_OK, body_sync_check_contact(host.sync, GUEST, ID0, n, target, &c),
                              "the claim of a flight its own release ended was rejected");
    TEST_ASSERT_TRUE(c.gap >= 0.0f && c.gap < 1.0f);
    /* the host's own forced move of that flight's body, before the release arrived */
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
    TEST_ASSERT_TRUE(body_sync_contact_number(guest.sync, ID0, &n));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_assign(host.sync, ID0, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, ID0));
    records(&guest, &host);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_REVOKED, body_sync_check_contact(host.sync, GUEST, ID0, n, target, &c));
    TEST_ASSERT_TRUE(c.gap >= 0.0f && c.gap < 1.0f); /* measured all the same: the game may log it */
}

/* A guest's claim names a slot the host bound again since: the old generation and the new
   body are both STALE, as is everything the host cannot judge, and nothing is measured. */
static void test_a_contact_of_a_rebound_slot_is_stale(void) {
    tick_all();
    body_sync_id_t old = body_sync_id_make(5, 1), fresh = body_sync_id_make(5, 2), target = body_sync_id_make(6, 1);
    grant(&guest, old, BODY_SYNC_HOLD_UNTIL_SETTLED);
    uint16_t n = 0;
    TEST_ASSERT_TRUE(body_sync_contact_number(guest.sync, old, &n));
    body_sync_contact_t c;
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, body_sync_check_contact(host.sync, GUEST, old, n, target, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_unbind(host.sync, old));
    body_sync_bind_t bind = {.body = host.bodies[5], .body_class = BODY_SYNC_CLASS_LIGHT, .authority = BODY_SYNC_SEAT_HOST, .number = n};
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_bind(host.sync, fresh, &bind));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(host.sync, GUEST, old, n, target, &c));
    TEST_ASSERT_TRUE(isinf(c.gap) && c.reach == 0.0f && c.approach_max == 0.0f);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(host.sync, GUEST, fresh, n, target, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, body_sync_release(guest.sync, old));
    records(&guest, &host); /* the release names the old generation: stale, ends nothing */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(host.sync, GUEST, old, n, target, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(host.sync, GUEST, fresh, n, target, &c));
    /* what the host cannot judge either */
    grant(&guest, ID0, BODY_SYNC_HOLD_UNTIL_SETTLED);
    TEST_ASSERT_TRUE(body_sync_contact_number(guest.sync, ID0, &n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, body_sync_check_contact(host.sync, GUEST, ID0, n, target, NULL));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(host.sync, GUEST, ID0, n, ID0, &c));                    /* itself */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(host.sync, GUEST, ID0, n, old, &c));                    /* gone */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(host.sync, GUEST, ID0, n, body_sync_id_make(0xFFFF, 1), &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(host.sync, OTHER, ID0, n, target, &c));                 /* not its */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(host.sync, 200, ID0, n, target, &c));                   /* no seat */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, body_sync_check_contact(guest.sync, GUEST, ID0, n, target, &c));                /* host only */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_every_body_reaches_the_guest_within_the_packet_limit);
    RUN_TEST(test_copy_is_driven_over_one_step);
    RUN_TEST(test_an_older_state_is_stale);
    RUN_TEST(test_claims_answers_and_holds);
    RUN_TEST(test_a_release_crossing_the_grant_stays_with_the_host);
    RUN_TEST(test_a_released_claim_is_withdrawn);
    RUN_TEST(test_the_owner_keeps_its_hold_through_the_grant);
    RUN_TEST(test_the_host_does_not_echo_a_guests_bodies);
    RUN_TEST(test_writes_stay_within_the_limits);
    RUN_TEST(test_new_owners_start_without_the_old_carrier);
    RUN_TEST(test_a_stale_release_does_not_undo_a_forced_move);
    RUN_TEST(test_a_forced_move_lands_after_an_answer);
    RUN_TEST(test_a_release_that_returns_the_body_withdraws_the_claim_too);
    RUN_TEST(test_an_answer_counts_only_for_its_claim);
    RUN_TEST(test_answer_refuses_claims_no_longer_pending);
    RUN_TEST(test_claim_sequences_skip_zero_at_the_wrap);
    RUN_TEST(test_authority_moves_only_by_newer_announcements);
    RUN_TEST(test_only_the_newest_claim_counts);
    RUN_TEST(test_every_claim_is_polled);
    RUN_TEST(test_forced_moves_keep_the_velocity);
    RUN_TEST(test_a_pose_of_an_old_grant_is_rejected);
    RUN_TEST(test_flags_type_and_holder_move_with_authority);
    RUN_TEST(test_a_revoke_seen_by_state_first_clears_the_holder);
    RUN_TEST(test_a_settled_hold_ends_at_the_first_contact);
    RUN_TEST(test_a_settled_hold_ends_at_rest);
    RUN_TEST(test_stale_generations_left_seats_and_refusals);
    RUN_TEST(test_queues_never_drop);
    RUN_TEST(test_receive_fuzz);
    RUN_TEST(test_locks_follow_the_last_owner_on_a_grant);
    RUN_TEST(test_locks_follow_the_last_owner_when_a_settled_hold_returns);
    RUN_TEST(test_locks_follow_the_last_owner_when_its_seat_leaves);
    RUN_TEST(test_locks_follow_the_last_owner_on_a_forced_move);
    RUN_TEST(test_a_handover_carries_flags_type_and_locks);
    RUN_TEST(test_a_late_packet_changes_nothing);
    RUN_TEST(test_a_copy_stops_without_news_and_after_a_jump);
    RUN_TEST(test_a_settled_hold_ignores_its_last_holder);
    RUN_TEST(test_a_rejoined_guest_with_new_ticks_is_heard);
    RUN_TEST(test_a_claimant_keeps_its_own_type_and_takes_the_hosts_locks);
    RUN_TEST(test_a_copy_reads_hold_and_holder_from_its_owner);
    RUN_TEST(test_a_slot_bound_again_starts_over);
    RUN_TEST(test_a_grant_carries_the_hosts_change_of_that_tick);
    RUN_TEST(test_a_claimant_keeps_its_own_flag_bits_only);
    RUN_TEST(test_data_travels_and_a_light_copy_lets_the_own_character_pass);
    RUN_TEST(test_mask_bits_are_taken_and_given_back_exactly);
    RUN_TEST(test_destroy_gives_the_mask_bits_back);
    RUN_TEST(test_bind_refuses_too_many_shapes);
    RUN_TEST(test_a_contact_claim_counts_after_its_release);
    RUN_TEST(test_a_contact_of_a_rebound_slot_is_stale);
    return UNITY_END();
}
