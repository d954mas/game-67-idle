#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sync_authority.h"
#include "unity.h"

/* The decisions the real shell leaves to authority, without Box3D: where a body's state goes,
   which record kind, when a settled hold ends, what a holder names, which motion a take
   continues with, and what a handover record carries. */

void setUp(void) {}
void tearDown(void) {}

enum { SEATS = 4 };
static sync_authority_t host, guest;
static char error[128];
static const body_sync_id_t A = 1u << 16, B = 2u | (1u << 16), C = 3u | (5u << 16);

static void start(void) {
    sync_authority_init(&host, BODY_SYNC_SEAT_HOST, SEATS, error, sizeof(error));
    sync_authority_init(&guest, 1, SEATS, error, sizeof(error));
    for (uint8_t s = 1; s < 3; s++) {
        sync_authority_seat_joined(&host, s);
    }
    sync_authority_seat_joined(&guest, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&host, A, BODY_SYNC_SEAT_HOST, 0));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&host, B, 2, 0));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&guest, A, BODY_SYNC_SEAT_HOST, 0));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&guest, B, 1, 0));
}

static void test_routes_and_record_kinds(void) {
    start();
    TEST_ASSERT_EQUAL_UINT8(0x6, sync_authority_route(&host, 0, false));            /* the host's own: every joined guest */
    TEST_ASSERT_EQUAL_UINT8(0x2, sync_authority_route(&host, 2, true));             /* seat 2's: the others, relayed */
    TEST_ASSERT_EQUAL_UINT8(0x0, sync_authority_route(&host, 2, false));            /* nothing to relay yet */
    TEST_ASSERT_EQUAL_UINT8(0x0, sync_authority_route(&host, 9, true));             /* unbound */
    TEST_ASSERT_EQUAL_UINT8(1u << BODY_SYNC_SEAT_HOST, sync_authority_route(&guest, 2, false)); /* a guest's own, to the host */
    TEST_ASSERT_EQUAL_UINT8(0x0, sync_authority_route(&guest, 0, true));            /* a guest relays nothing */
    TEST_ASSERT_FALSE(sync_authority_sends_sim(&host));
    TEST_ASSERT_TRUE(sync_authority_sends_sim(&guest));
    sync_authority_seat_left(&guest, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8(0x0, sync_authority_route(&guest, 2, false)); /* no host to send to */
}

static void test_a_settled_hold_ends_where_it_should(void) {
    start();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&host, A, BODY_SYNC_HOLD_UNTIL_SETTLED));
    TEST_ASSERT_FALSE(sync_authority_settle(&host, A, false, false, false));
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_HOLD_UNTIL_SETTLED, sync_authority_body(&host, A)->hold);
    TEST_ASSERT_TRUE(sync_authority_settle(&host, A, true, false, false));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(BODY_SYNC_HOLD_UNTIL_RELEASE, sync_authority_body(&host, A)->hold, "the host did not end its settled hold");
    TEST_ASSERT_FALSE(sync_authority_settle(&host, A, true, true, true)); /* no settled hold any more */
    TEST_ASSERT_FALSE(sync_authority_settle(&host, B, true, true, true)); /* not local */
    /* a guest's settled hold ends with a release to the host */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&guest, B, BODY_SYNC_HOLD_UNTIL_SETTLED));
    TEST_ASSERT_TRUE(sync_authority_settle(&guest, B, false, true, true));
    uint8_t record[STUB_RECORD_BYTES_MAX];
    uint32_t size = 0;
    TEST_ASSERT_TRUE(sync_authority_peek(&guest, BODY_SYNC_SEAT_HOST, record, sizeof(record), &size));
    TEST_ASSERT_EQUAL_UINT8(STUB_REC_RELEASE, record[0]);
    TEST_ASSERT_FALSE(sync_authority_local(&guest, sync_authority_body(&guest, B)));
}

static void test_a_holder_is_named_by_its_low_generation_bits(void) {
    start();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&host, C, BODY_SYNC_SEAT_HOST, 0));
    TEST_ASSERT_EQUAL_UINT32(C, sync_authority_holder_id(&host, 3, 5, 4));
    TEST_ASSERT_EQUAL_UINT32(C, sync_authority_holder_id(&host, 3, 5 + 16, 4)); /* only the low bits travel */
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, sync_authority_holder_id(&host, 3, 6, 4));
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, sync_authority_holder_id(&host, 7, 1, 4));
    TEST_ASSERT_EQUAL_UINT32(BODY_SYNC_ID_NONE, sync_authority_holder_id(&host, BODY_SYNC_BODIES_MAX, 1, 4));
}

static void test_a_take_continues_the_record_then_the_state_then_the_bind(void) {
    start();
    sync_authority_body_t *b = sync_authority_body(&guest, A);
    bool k;
    uint8_t l;
    sync_authority_resume_motion(b, false, true, 1, false, 2, &k, &l);
    TEST_ASSERT_TRUE(!k && l == 2); /* the bind */
    sync_authority_resume_motion(b, true, true, 1, false, 2, &k, &l);
    TEST_ASSERT_TRUE(k && l == 1); /* the state received */
    b->take_motion = true;
    b->take_kinematic = false;
    b->take_locks = 7;
    sync_authority_resume_motion(b, true, true, 1, false, 2, &k, &l);
    TEST_ASSERT_TRUE(!k && l == 7); /* the record */
}

/* What a forced move announces: the flags and motion the host holds for the body, and the
   guest that takes it continues them; a claimant that changed its own keeps them at the grant. */
static void test_records_carry_the_body_and_owners_keep_their_own(void) {
    start();
    sync_authority_body_t *h = sync_authority_body(&host, A);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_set_flags(&host, A, 5));
    sync_authority_note_motion(&host, h, true, 0x08);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, 1, BODY_SYNC_HOLD_UNTIL_RELEASE));
    uint8_t record[STUB_RECORD_BYTES_MAX];
    uint32_t size = 0, stale = 0;
    TEST_ASSERT_TRUE(sync_authority_peek(&host, 1, record, sizeof(record), &size));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_receive(&guest, BODY_SYNC_SEAT_HOST, 1, record, size, &stale, error, sizeof(error)));
    sync_authority_body_t *g = sync_authority_body(&guest, A);
    TEST_ASSERT_TRUE(sync_authority_local(&guest, g));
    TEST_ASSERT_EQUAL_UINT8(5, g->flags);
    TEST_ASSERT_TRUE(g->resume && g->take_motion && g->take_kinematic && g->take_locks == 0x08);
    /* a claim: the guest applies, then changes the type and flags itself before the grant */
    start();
    sync_authority_body_t *gb = sync_authority_body(&guest, A);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&guest, A, BODY_SYNC_HOLD_UNTIL_RELEASE));
    sync_authority_applied(gb, false, 0);
    sync_authority_note_motion(&guest, gb, true, 0);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_set_flags(&guest, A, 3));
    TEST_ASSERT_TRUE(sync_authority_peek(&guest, BODY_SYNC_SEAT_HOST, record, sizeof(record), &size));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_receive(&host, 1, 1, record, size, &stale, error, sizeof(error)));
    sync_authority_pop(&guest, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_set_flags(&host, A, 12));
    sync_authority_note_motion(&host, sync_authority_body(&host, A), false, 0x3F);
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(sync_authority_poll(&host, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_answer(&host, &c, true, SYNC_AUTHORITY_AS_CLAIMED));
    TEST_ASSERT_TRUE(sync_authority_peek(&host, 1, record, sizeof(record), &size));
    gb->take_motion = false;
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_receive(&guest, BODY_SYNC_SEAT_HOST, 2, record, size, &stale, error, sizeof(error)));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(15, gb->flags, "flags not kept bit by bit: its own 3, the host's 12");
    /* its own type stays; the locks it never touched come from the grant */
    TEST_ASSERT_TRUE(gb->take_motion);
    TEST_ASSERT_TRUE_MESSAGE(gb->take_kinematic, "the grant overwrote the claimant's own motion type");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0x3F, gb->take_locks, "the claimant kept locks it never changed");
}

/* A seat that left hands its bodies back still naming their carrier. */
static void test_a_seat_that_left_keeps_the_carrier(void) {
    start();
    sync_authority_body_t *b = sync_authority_body(&host, B);
    sync_authority_received(b, BODY_SYNC_HOLD_UNTIL_RELEASE, C, 4);
    sync_authority_seat_left(&host, 2);
    TEST_ASSERT_TRUE(sync_authority_local(&host, b));
    TEST_ASSERT_EQUAL_UINT32(C, b->holder);
    TEST_ASSERT_EQUAL_UINT8(4, b->flags);
}

/* ---- contact claims: which ownership a guest's claim of a contact may name ---- */

enum { GRACE = 6 }; /* the shell's grace in ticks, a round trip and a delay */
static sync_authority_t other;
static bool plant_revoked_as_arbitrated; /* --plant-revoked-as-arbitrated: the revocation cases must fail */
static bool plant_target_any;            /* --plant-target-any-ownership: the target's ownership case must fail */

/* Guests 1 and 2 of a host in the interaction mode, all with the host's light body A. */
static void contact_start(void) {
    sync_authority_t *sides[] = {&host, &guest, &other};
    for (uint8_t i = 0; i < 3; i++) {
        sync_authority_init(sides[i], i, SEATS, error, sizeof(error));
        sides[i]->mode = SYNC_POLICY_INTERACTION;
        sides[i]->now = 100;
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(sides[i], A, BODY_SYNC_SEAT_HOST, 0));
    }
    for (uint8_t i = 1; i < 3; i++) {
        sync_authority_seat_joined(&host, i);
        sync_authority_seat_joined(sides[i], BODY_SYNC_SEAT_HOST);
    }
    host.plant_revoked_as_arbitrated = plant_revoked_as_arbitrated;
    host.plant_target_any_ownership = plant_target_any;
    sync_authority_body(&host, A)->takable = true;
}

/* Every record `from` (seat `from_seat`) owes `to_seat`, received by `to` in order. */
static void deliver(sync_authority_t *from, uint8_t from_seat, sync_authority_t *to, uint8_t to_seat) {
    uint8_t record[STUB_RECORD_BYTES_MAX];
    uint32_t size = 0, stale = 0;
    char why[64];
    while (sync_authority_peek(from, to_seat, record, sizeof(record), &size)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_receive(to, from_seat, 1, record, size, &stale, why, sizeof(why)));
        sync_authority_pop(from, to_seat);
    }
}

/* A guest reads the host's authority of A from a relayed state. */
static void see_host(sync_authority_t *g) {
    const sync_authority_body_t *h = sync_authority_body(&host, A);
    sync_authority_from_state(g, sync_authority_body(g, A), h->seat, h->number, h->hold);
}

static body_sync_contact_verdict_t check_id(uint8_t seat, body_sync_id_t id, uint16_t number) {
    const sync_authority_ended_t *ended;
    body_sync_contact_verdict_t v = sync_authority_check_contact(&host, seat, id, number, GRACE, &ended);
    TEST_ASSERT_TRUE_MESSAGE(v != BODY_SYNC_CONTACT_STALE || ended == NULL, "a STALE verdict named an end");
    return v;
}
static body_sync_contact_verdict_t check(uint8_t seat, uint16_t number) { return check_id(seat, A, number); }

/* `g` (seat `seat`) takes A and the host grants it; with `hear`, the grant comes back. Returns
   the number the guest's contact claims named while the take waited. */
static uint16_t take_of(sync_authority_t *g, uint8_t seat, bool hear) {
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(g, A));
    uint16_t number = 0;
    TEST_ASSERT_TRUE_MESSAGE(sync_authority_contact_number(g, sync_authority_body(g, A), &number), "a waiting take names no number");
    deliver(g, seat, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(seat, sync_authority_body(&host, A)->seat, "the take was refused");
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_OK, check(seat, number), "the waiting take's number is not the grant's");
    if (hear) {
        deliver(&host, BODY_SYNC_SEAT_HOST, g, seat);
        uint16_t after = 0;
        TEST_ASSERT_TRUE(sync_authority_contact_number(g, sync_authority_body(g, A), &after));
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(number, after, "the granted owner names another number");
    }
    return number;
}

static uint16_t own_by_take(sync_authority_t *g, uint8_t seat) {
    see_host(g);
    return take_of(g, seat, true);
}

/* A claim of an ownership that ended without a recall: OK through the grace, STALE after. */
static void expect_grace(uint8_t seat, uint16_t n, const char *what) {
    uint32_t end = host.now;
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_OK, check(seat, n), what);
    host.now = end + GRACE;
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_OK, check(seat, n), what);
    host.now = end + GRACE + 1;
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_STALE, check(seat, n), what);
    host.now = end;
}

/* A recalled ownership's claims: REVOKED, and still REVOKED long after. */
static void expect_revoked(uint16_t n, const char *what) {
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_REVOKED, check(1, n), what);
    host.now += 1000;
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_REVOKED, check(1, n), what);
    /* a release the guest sent before it heard is stale: it ends nothing again */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_REVOKED, check(1, n), what);
}

static void test_contact_of_a_body_owned_now(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(1, n));
    host.now += 10000; /* owning has no grace */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(1, n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, (uint16_t)(n + 1))); /* a number it never had */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, (uint16_t)(n - 1)));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(2, n));                   /* another seat's claim */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(BODY_SYNC_SEAT_HOST, n)); /* the host judges its own */
    const sync_authority_ended_t *ended;
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, sync_authority_check_contact(&guest, 1, A, n, GRACE, &ended)); /* host only */
    uint16_t none = 0;
    TEST_ASSERT_FALSE(sync_authority_contact_number(&other, sync_authority_body(&other, A), &none)); /* a copy names none */
}

/* The guest's release reaches the host before the claim of the contact that ended the flight:
   the claim counts through the grace, then no more. */
static void test_contact_after_a_voluntary_release(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    const float before[3] = {1.0f, 2.0f, 0.0f}, at[3] = {1.0f, 2.0f, 3.0f};
    sync_authority_received_pose(&host, sync_authority_body(&host, A), 1, 10, 0, 10.0f, before);
    host.now += 2u;
    sync_authority_received_pose(&host, sync_authority_body(&host, A), 1, 12, 0, 10.0f, at);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, A)->seat);
    const sync_authority_ended_t *ended;
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, sync_authority_check_contact(&host, 1, A, n, GRACE, &ended));
    TEST_ASSERT_NOT_NULL(ended);
    TEST_ASSERT_TRUE_MESSAGE(ended->has_pose && ended->pose.position[2] == 3.0f && fabsf(ended->travel - 1.5f) < 1e-5f, "the end lost the seat's last state");
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, sync_authority_body(&host, A)->number)); /* the host's number now */
    expect_grace(1, n, "a voluntary end");
}

/* The travel the host credits a seat is what its taken states show: a sender stamping two states a
   tick apart that the host took twelve ticks apart travelled over twelve less the jitter granted;
   the ring keeps the fastest of its last SYNC_AUTHORITY_POSES; a state of another seat starts it
   over. */
static float ring_travel(const sync_authority_body_t *b) {
    float fastest = 0.0f;
    uint32_t first = (b->pose_next + SYNC_AUTHORITY_POSES - b->pose_count) % SYNC_AUTHORITY_POSES;
    for (uint32_t i = 1; i < b->pose_count; i++) {
        const sync_authority_pose_t *p = &b->poses[(first + i) % SYNC_AUTHORITY_POSES];
        float t = p->span > 0 ? sqrtf(p->step[0] * p->step[0] + p->step[1] * p->step[1] + p->step[2] * p->step[2]) : 0.0f;
        fastest = t > fastest ? t : fastest;
    }
    return fastest;
}

static bool plant_grows; /* --plant-evidence-grows-over-gaps: the gap cases must fail */
/* --plant-approach-from-ring, --plant-evidence-age-unbounded, --plant-teleport-keeps-ring: the
   approach, lag and teleport cases must fail. */
static bool plant_ring_approach, plant_age, plant_teleport;
static bool plant_rewind_off; /* --plant-target-rewind-off: the flying target case must fail */
static sync_authority_t *path_host;

/* The host's `now`, then a state of `b` from seat 1 (2 ticks of jitter granted, the box's speed
   50 m/s at 60 Hz). */
static void take_pose(sync_authority_body_t *b, uint32_t tick, uint32_t now, const float p[3]) {
    path_host->now = now;
    sync_authority_received_pose(path_host, b, 1, tick, 2, 50.0f / 60.0f, p);
}

static void test_pose_travel_is_what_the_host_took(void) {
    path_host = calloc(1, sizeof(*path_host));
    sync_authority_body_t *b = calloc(1, sizeof(*b));
    const float p0[3] = {0.0f, 0.0f, 0.0f}, p1[3] = {0.7f, 0.0f, 0.0f};
    TEST_ASSERT_TRUE(ring_travel(b) == 0.0f);
    take_pose(b, 100, 500, p0);
    TEST_ASSERT_TRUE(ring_travel(b) == 0.0f); /* one state shows no travel */
    take_pose(b, 101, 512, p1);
    TEST_ASSERT_TRUE(fabsf(ring_travel(b) - 0.07f) < 1e-6f); /* 12 ticks seen, 2 of jitter */
    TEST_ASSERT_EQUAL_UINT32(10, b->poses[1].span);           /* stamps 1 apart taken 12 apart: 12 - 2 */
    take_pose(b, 111, 513, p0);
    TEST_ASSERT_EQUAL_UINT32(3, b->poses[2].span); /* stamps 10 apart taken 1 apart: 1 + 2 of jitter */
    TEST_ASSERT_TRUE(fabsf(ring_travel(b) - 0.7f / 3.0f) < 1e-6f);
    take_pose(b, 111, 513, p1);
    TEST_ASSERT_TRUE(fabsf(ring_travel(b) - 0.7f) < 1e-6f); /* no span reads as one tick */
    for (uint32_t i = 0; i < SYNC_AUTHORITY_POSES; i++) {
        take_pose(b, 120 + i, 520 + i, p0);
    }
    TEST_ASSERT_TRUE(ring_travel(b) == 0.0f); /* the jump left the ring */
    path_host->now = 530;
    sync_authority_received_pose(path_host, b, 2, 5, 2, 50.0f / 60.0f, p1);
    TEST_ASSERT_EQUAL_UINT8(1, b->pose_count);
    TEST_ASSERT_EQUAL_UINT8(2, b->pose_seat);
    free(b);
    free(path_host);
}

/* ---- the evidence: where a seat's agent may have touched, from what the host took of it ---- */

static sync_authority_body_t *path_body, *path_other;
static float path_push;       /* the scene's push_ticks: a take's, 0 for a contact */
static uint32_t path_contact; /* the contact's tick on the claimant's clock */

static void path_start(void) {
    path_host = calloc(1, sizeof(*path_host));
    path_body = calloc(1, sizeof(*path_body));
    path_other = calloc(1, sizeof(*path_other));
    path_host->plant_evidence_grows_over_gaps = plant_grows;
    path_host->plant_approach_from_ring = plant_ring_approach;
    path_host->plant_evidence_age_unbounded = plant_age;
    path_host->plant_teleport_keeps_ring = plant_teleport;
    path_host->plant_target_rewind_off = plant_rewind_off;
    path_body->seat = path_other->seat = 1;
    path_body->number = path_other->number = 3;
    path_push = 0.0f;
    path_contact = 0;
}

static void path_end(void) {
    free(path_host);
    free(path_body);
    free(path_other);
}

typedef struct path_verdict {
    bool any;
    float gap, reach, approach;
} path_verdict_t;

/* The measure of a contact of the 0.24 m agent with a resting box of half size `half` centred at
   (x, y, z) at `path_contact`, a 20-tick window, `path_push` ticks of push. */
static path_verdict_t measure_box(float x, float y, float z, float half) {
    sync_contact_scene_t in = {.tick_hz = 60, .max_speed = 50.0f, .contact_tick = path_contact, .window_ticks = 20, .push_ticks = path_push};
    const float c[3] = {x, y, z};
    for (int i = 0; i < 3; i++) {
        in.agent_lo[i] = -0.12f;
        in.agent_hi[i] = 0.12f;
        in.target_lo[i] = c[i] - half;
        in.target_hi[i] = c[i] + half;
    }
    path_verdict_t v;
    v.any = sync_authority_path_measure(path_host, path_body, 1, NULL, &in, &v.gap, &v.reach, &v.approach);
    return v;
}

static path_verdict_t measure_at(float x, float y, float z) { return measure_box(x, y, z, 0.3f); }

static bool accepts(path_verdict_t v) { return v.any && v.gap <= v.reach; }

/* Three states at 9 m/s along x, a tick apart. */
static void flying_states(void) {
    for (uint32_t i = 0; i < 3; i++) {
        const float p[3] = {0.15f * (float)i, 0.0f, 0.0f};
        take_pose(path_body, 100 + i, 500 + i, p);
    }
}

/* The path the host saw before the contact: on along a state's step for the ticks to the contact,
   at most a chord's spacing, or on the chord over three states honest loss took. A contact five
   ticks after the last state, or inside a gap of six, has no evidence. Small targets, so the
   states alone do not reach them. */
static void test_evidence_follows_the_path_the_host_saw(void) {
    path_start();
    flying_states();
    const float on[3] = {0.9f, 0.0f, 0.0f}, far[3] = {1.8f, 0.0f, 0.0f};
    take_pose(path_body, 106, 506, on); /* the states of ticks 103-105 lost */
    path_host->now = 507;
    path_contact = 104;
    TEST_ASSERT_TRUE_MESSAGE(accepts(measure_box(0.6f, 0.0f, 0.27f, 0.05f)), "a contact beside a chord over lost states");
    path_contact = 110;
    TEST_ASSERT_TRUE_MESSAGE(accepts(measure_box(1.8f, 0.0f, 0.0f, 0.05f)), "a contact on along the last step four ticks on");
    TEST_ASSERT_FALSE_MESSAGE(accepts(measure_box(2.8f, 0.0f, 0.0f, 0.05f)), "a contact past the step's four ticks");
    TEST_ASSERT_TRUE(fabsf(measure_at(0.0f, 0.0f, 3.0f).approach - 9.0f) < 1e-3f); /* the fastest step */
    path_contact = 111;
    TEST_ASSERT_FALSE_MESSAGE(measure_box(1.8f, 0.0f, 0.0f, 0.05f).any, "a contact five ticks after the last state had evidence");
    take_pose(path_body, 112, 512, far);
    path_host->now = 513;
    TEST_ASSERT_FALSE_MESSAGE(accepts(measure_box(1.3f, 0.0f, 0.27f, 0.05f)), "a contact inside a gap had evidence");
    path_contact = 112;
    TEST_ASSERT_TRUE_MESSAGE(accepts(measure_box(1.8f, 0.0f, 0.27f, 0.05f)), "a state after a gap did not stand");
    path_end();
}

/* A seat that stops sending leaves its agent's last state: after 1 s, 10 s and 100 s it is no
   evidence at all; while the seat is still heard it places only a contact a chord's spacing after
   it, never one grown by the time since, and only while the claim's lag allows. */
static void test_evidence_of_a_silent_agent_does_not_grow(void) {
    path_start();
    flying_states();
    const uint32_t after[3] = {60, 600, 6000};
    const float elsewhere[3] = {5.0f, 0.0f, 5.0f};
    take_pose(path_other, 930, 532, elsewhere); /* the seat heard, past the window, within the lag */
    path_contact = 102;
    path_push = 20.0f; /* a take's: the steps before the window push nothing */
    path_verdict_t late = measure_at(0.3f, 0.0f, 0.5f);
    TEST_ASSERT_TRUE_MESSAGE(accepts(late), "a resting agent's last state did not stand within the lag");
    const float chord_s = (float)SYNC_POLICY_CHORD_TICKS_MAX / 60.0f;
    TEST_ASSERT_TRUE(late.reach <= SYNC_POLICY_TAKE_SLOP_M + 0.5f * SYNC_POLICY_CONTACT_BEND_MS2 * chord_s * chord_s + 1e-4f);
    path_push = 0.0f;
    for (int k = 0; k < 3; k++) {
        path_host->now = 502 + after[k];
        path_contact = 102 + after[k]; /* the claimant's clock went on with the host's */
        path_verdict_t v = measure_at(2.3f, 0.0f, 0.0f);
        TEST_ASSERT_FALSE_MESSAGE(accepts(v), "a silent agent took a target 2 m ahead");
        TEST_ASSERT_TRUE(v.reach <= SYNC_POLICY_TAKE_SLOP_M);
        path_contact = 102;
        TEST_ASSERT_FALSE_MESSAGE(accepts(measure_at(0.3f, 0.0f, 0.5f)), "a silent agent's last state stood");
        take_pose(path_other, 900 + after[k], path_host->now, elsewhere); /* the seat heard again */
        path_contact = 102 + after[k];
        TEST_ASSERT_FALSE_MESSAGE(accepts(measure_at(2.3f, 0.0f, 0.0f)), "a resting agent took a target 2 m ahead");
        TEST_ASSERT_FALSE_MESSAGE(accepts(measure_at(0.3f, 0.0f, 0.5f)), "a resting agent placed a contact long after its last state");
        path_contact = 102;
        TEST_ASSERT_FALSE_MESSAGE(measure_at(0.3f, 0.0f, 0.5f).any, "a resting agent's last state stood past the claim's lag");
    }
    path_end();
}

/* A body that slept (one rest state) and woke 5 s later: no chord between the two, and the contact
   a tick after the woken state lies within a tick's bend of it, nothing of the 5 s. */
static void test_evidence_of_a_body_that_slept_has_no_long_chord(void) {
    path_start();
    const float r0[3] = {0.0f, 0.0f, 0.0f}, r1[3] = {0.15f, 0.0f, 0.0f};
    take_pose(path_body, 100, 500, r0);
    take_pose(path_body, 400, 800, r1);
    path_host->now = 801;
    path_contact = 401;
    path_verdict_t v = measure_at(0.0f, 0.0f, 4.0f);
    TEST_ASSERT_FALSE_MESSAGE(accepts(v), "a woken body took a target 4 m to its side");
    TEST_ASSERT_FALSE_MESSAGE(accepts(measure_at(0.0f, 0.0f, 8.0f)), "a woken body took a target 8 m to its side");
    const float tick_s = 1.0f / 60.0f;
    TEST_ASSERT_TRUE(v.reach <= SYNC_POLICY_TAKE_SLOP_M + 0.5f * SYNC_POLICY_CONTACT_BEND_MS2 * tick_s * tick_s + 1e-4f);
    path_end();
}

/* Stamps 60 ticks on for a state the host took a tick after the one before: its span is the
   host's plus the jitter (3), and a contact named a tick after it is 61 ticks after the state
   before, none of that one's evidence. */
static void test_a_stamp_jump_buys_no_long_ray(void) {
    path_start();
    flying_states();
    const float on[3] = {0.45f, 0.0f, 0.0f};
    take_pose(path_body, 162, 503, on);
    TEST_ASSERT_EQUAL_UINT32(3, path_body->poses[3].span);
    path_host->now = 504;
    path_contact = 163;
    TEST_ASSERT_FALSE_MESSAGE(accepts(measure_at(3.3f, 0.0f, 0.0f)), "a stamp jump stretched the step 3 m");
    TEST_ASSERT_FALSE_MESSAGE(accepts(measure_at(0.3f, 0.0f, 4.0f)), "a stamp jump bent the path 4 m");
    path_end();
}

/* A guest that holds its states back and stamps them a tick apart: the host took them 12 ticks
   apart, so the span is the host's less the 2 of jitter. The step is no faster than that span
   shows, and the gap stays a gap: no chord between the two. */
static void test_compressed_stamps_buy_no_speed_and_no_chord(void) {
    path_start();
    const float a[3] = {0.0f, 0.0f, 0.0f}, b[3] = {1.2f, 0.0f, 0.0f};
    take_pose(path_body, 100, 500, a);
    take_pose(path_body, 101, 512, b);
    TEST_ASSERT_EQUAL_UINT32(10, path_body->poses[1].span);
    path_host->now = 513;
    path_contact = 101;
    path_verdict_t v = measure_box(0.6f, 0.0f, 0.27f, 0.05f);
    TEST_ASSERT_FALSE_MESSAGE(accepts(v), "compressed stamps made a gap a chord");
    TEST_ASSERT_TRUE_MESSAGE(v.approach <= 0.12f * 60.0f + 1e-3f, "compressed stamps sped the step up");
    path_end();
}

/* A step faster than the box's speed is a teleport: no evidence of a path into it and no speed.
   The state itself is where the host took it. */
static void test_a_teleport_step_is_refused(void) {
    path_start();
    const float a[3] = {0.0f, 0.0f, 0.0f}, b[3] = {5.0f, 0.0f, 0.0f};
    take_pose(path_body, 100, 500, a);
    take_pose(path_body, 101, 501, b);
    TEST_ASSERT_EQUAL_UINT8(1, path_body->pose_count);
    path_host->now = 502;
    path_contact = 101;
    TEST_ASSERT_FALSE_MESSAGE(accepts(measure_at(2.5f, 0.0f, 0.0f)), "a teleport's chord counted");
    path_verdict_t v = measure_at(5.0f, 0.0f, 0.5f);
    TEST_ASSERT_TRUE(accepts(v));
    TEST_ASSERT_TRUE_MESSAGE(v.approach == 0.0f, "a teleport's step counted as speed");
    path_end();
}

/* Three states at 9 m/s, then one 5 m off beside a target: the jump starts the evidence over. The
   new state places the agent, as a point with no speed and no push, and none of the states
   before it stands; its next step is its first speed again. */
static void test_a_teleport_starts_the_evidence_over(void) {
    path_start();
    flying_states();
    const float jumped[3] = {0.0f, 0.0f, 5.0f}, on[3] = {0.0f, 0.0f, 5.05f};
    take_pose(path_body, 103, 503, jumped);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, path_body->pose_count, "the states before a teleport stayed");
    path_host->now = 504;
    path_contact = 104;
    path_push = 20.0f;
    path_verdict_t v = measure_at(0.0f, 0.0f, 5.35f);
    TEST_ASSERT_TRUE_MESSAGE(accepts(v), "the state after a teleport places nothing");
    TEST_ASSERT_TRUE_MESSAGE(v.approach == 0.0f, "the steps before a teleport gave its contact a speed");
    const float chord_s = (float)SYNC_POLICY_CHORD_TICKS_MAX / 60.0f;
    TEST_ASSERT_TRUE_MESSAGE(v.reach <= SYNC_POLICY_TAKE_SLOP_M + 0.5f * SYNC_POLICY_CONTACT_BEND_MS2 * chord_s * chord_s + 1e-4f,
                             "the steps before a teleport pushed a take");
    path_push = 0.0f;
    path_contact = 102;
    TEST_ASSERT_FALSE_MESSAGE(measure_at(0.3f, 0.0f, 0.0f).any, "a state before the teleport stood");
    take_pose(path_body, 104, 504, on);
    path_host->now = 505;
    path_contact = 105;
    v = measure_at(0.0f, 0.0f, 5.4f);
    TEST_ASSERT_TRUE(accepts(v));
    TEST_ASSERT_TRUE_MESSAGE(v.approach > 2.9f && v.approach < 3.1f, "the step after a teleport is not its speed");
    path_end();
}

/* Three steps at 18 m/s, then eight rolling at 0.6 m/s into a box, the contact at the last: the
   approach is the roll's, never the ring's fastest step, so a gentle touch claims no hard hit. */
static void rolling_after_a_fast_start(void) {
    float x = 0.0f;
    for (uint32_t i = 0; i < 12; i++) {
        const float p[3] = {x, 0.0f, 0.0f};
        take_pose(path_body, 100 + i, 500 + i, p);
        x += i < 3 ? 0.3f : 0.01f;
    }
}

static void test_approach_is_the_step_at_the_contact(void) {
    path_start();
    rolling_after_a_fast_start();
    path_contact = 111;
    path_verdict_t v = measure_box(0.98f + 0.12f + 0.05f, 0.0f, 0.0f, 0.05f);
    TEST_ASSERT_TRUE_MESSAGE(accepts(v), "the rolling touch has no evidence");
    TEST_ASSERT_TRUE_MESSAGE(v.approach > 0.5f && v.approach < 0.7f, "the contact's approach is not the step at it");
    path_contact = 102; /* the contact named where the fast steps ran */
    v = measure_box(0.6f + 0.12f + 0.05f, 0.0f, 0.0f, 0.05f);
    TEST_ASSERT_TRUE(accepts(v));
    TEST_ASSERT_TRUE_MESSAGE(v.approach > 17.9f, "a contact on the fast steps lost their speed");
    path_end();
}

/* The same ring and a contact named at the fast steps: it stands while the host took those
   states within the claim's lag (two 20-tick windows), and never after, however the claim names
   its tick, the seat heard all the while. */
static void test_a_contact_named_long_after_has_no_evidence(void) {
    path_start();
    rolling_after_a_fast_start();
    const float elsewhere[3] = {5.0f, 0.0f, 5.0f};
    path_contact = 102;
    TEST_ASSERT_TRUE_MESSAGE(accepts(measure_box(0.6f, 0.0f, 0.0f, 0.05f)), "a fresh contact on the fast steps has no evidence");
    take_pose(path_other, 940, 542, elsewhere); /* 40 host ticks after the state of tick 102 */
    TEST_ASSERT_TRUE_MESSAGE(accepts(measure_box(0.6f, 0.0f, 0.0f, 0.05f)), "a contact within the lag has no evidence");
    const uint32_t late[3] = {41, 120, 1500};
    for (int k = 0; k < 3; k++) {
        take_pose(path_other, 900 + late[k], 502 + late[k], elsewhere);
        TEST_ASSERT_FALSE_MESSAGE(measure_box(0.6f, 0.0f, 0.0f, 0.05f).any, "a contact named long after had evidence");
    }
    path_end();
}

/* A host body flying at 10 m/s, judged against an agent resting at the origin 12 host ticks after
   the agent's state: it is put back 2 m along its own path. Flown into the agent's place since, it
   never met it; 2 m on, its path ran through the agent then. */
static void test_a_flying_target_is_judged_where_its_path_had_it(void) {
    path_start();
    const float rest[3] = {0.0f, 0.0f, 0.0f};
    for (uint32_t i = 0; i < 3; i++) {
        take_pose(path_body, 100 + i, 500 + i, rest);
    }
    path_host->now = 514;
    path_contact = 102;
    sync_contact_scene_t in = {.tick_hz = 60, .max_speed = 50.0f, .contact_tick = path_contact, .window_ticks = 20,
                               .target_speed = 10.0f, .target_velocity = {10.0f, 0.0f, 0.0f}, .rewind = true};
    float gap, reach, approach;
    for (int k = 0; k < 2; k++) {
        const float x = k == 0 ? 0.0f : 2.0f;
        for (int i = 0; i < 3; i++) {
            in.agent_lo[i] = -0.12f;
            in.agent_hi[i] = 0.12f;
            in.target_lo[i] = (i == 0 ? x : 0.0f) - 0.3f;
            in.target_hi[i] = (i == 0 ? x : 0.0f) + 0.3f;
        }
        TEST_ASSERT_TRUE(sync_authority_path_measure(path_host, path_body, 1, NULL, &in, &gap, &reach, &approach));
        if (k == 0) {
            TEST_ASSERT_TRUE_MESSAGE(gap > reach, "a target that flew into the agent's place was judged there");
        } else {
            TEST_ASSERT_TRUE_MESSAGE(gap <= reach, "a target whose path ran through the agent was not judged on it");
            TEST_ASSERT_TRUE_MESSAGE(reach < 1.0f, "the rewound target's slack is its whole travel");
        }
    }
    path_end();
}

/* After a voluntary end another seat takes the body: the past flight's claim keeps its grace,
   and the new owner's own ownership is OK. */
static void test_contact_after_a_take_by_another_seat(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    uint16_t m = own_by_take(&other, 2);
    TEST_ASSERT_TRUE(m != n);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(2, m));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(2, n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, m));
    expect_grace(1, n, "another seat's take since");
}

/* A thrown body returns to the host at its first contact: a release the shell sends itself. */
static void test_contact_after_a_flight_returns(void) {
    contact_start();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&guest, A, BODY_SYNC_HOLD_UNTIL_SETTLED));
    uint16_t n = 0;
    TEST_ASSERT_FALSE_MESSAGE(sync_authority_contact_number(&guest, sync_authority_body(&guest, A), &n),
                              "a claim waiting for its answer named a number");
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(sync_authority_poll(&host, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_answer(&host, &c, true, SYNC_AUTHORITY_AS_CLAIMED));
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1);
    TEST_ASSERT_TRUE(sync_authority_contact_number(&guest, sync_authority_body(&guest, A), &n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(1, n));
    TEST_ASSERT_TRUE(sync_authority_settle(&guest, A, false, true, true)); /* it met a body it may not take */
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, A)->seat);
    expect_grace(1, n, "the claim of the contact that ended the flight");
}

/* The guest releases at the flight's return and takes the body again before the host's answer
   to the release arrives: the waiting take names the number its grant gives. */
static void test_contact_of_a_retake_before_the_release_is_heard(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
    uint16_t again = take_of(&guest, 1, false); /* no see_host: the release is unheard */
    TEST_ASSERT_TRUE(again != n);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(1, again));
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1);
    uint16_t heard = 0;
    TEST_ASSERT_TRUE(sync_authority_contact_number(&guest, sync_authority_body(&guest, A), &heard));
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(again, heard, "the guest named another number once it heard");
    TEST_ASSERT_TRUE(sync_authority_local(&guest, sync_authority_body(&guest, A)));
}

/* The guest takes A again with no see_host first. `known`: it can know the host's number, and
   names the grant's while the take waits; else it names none (no claim on a guessed number).
   Either way it names the grant's once it hears. */
static void retake_names_the_grant(bool known, const char *what) {
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, A));
    uint16_t named = 0;
    bool names = sync_authority_contact_number(&guest, sync_authority_body(&guest, A), &named);
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    const sync_authority_body_t *h = sync_authority_body(&host, A);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, h->seat, what);
    if (known) {
        TEST_ASSERT_TRUE_MESSAGE(names, what);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(h->number, named, what);
    } else {
        TEST_ASSERT_FALSE_MESSAGE(names, what);
    }
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1);
    TEST_ASSERT_TRUE_MESSAGE(sync_authority_contact_number(&guest, sync_authority_body(&guest, A), &named), what);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(h->number, named, what);
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_OK, check(1, named), what);
}

/* A claim released before its answer: the host withdraws it and moves nothing. Its relayed states
   name no newer number, so the guest still cannot know, however late it takes again. */
static void test_contact_of_a_retake_after_a_withdrawn_claim(void) {
    contact_start();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&guest, A, BODY_SYNC_HOLD_UNTIL_SETTLED));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT16(0, sync_authority_body(&host, A)->number);
    see_host(&guest); /* the host's states, seconds later: the same number */
    host.now += 600;
    retake_names_the_grant(false, "a re-take after a withdrawn claim");
}

/* A release that crosses its take's grant: the host moves twice. Heard in any order, the guest
   ends on the host's number. */
static void test_contact_of_a_retake_after_a_release_crossed_the_grant(void) {
    static const char *orders[] = {"re-take before hearing anything", "the host's move of the release heard first",
                                   "the grant heard before the release reached the host"};
    for (int order = 0; order < 3; order++) {
        contact_start();
        see_host(&guest);
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, A));
        deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST); /* granted */
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
        if (order == 2) {
            deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1); /* a stale grant */
        }
        deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST); /* the release crosses the grant */
        TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, A)->seat);
        TEST_ASSERT_EQUAL_UINT16(2, sync_authority_body(&host, A)->number);
        if (order >= 1) {
            deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1);
        }
        retake_names_the_grant(order >= 1, orders[order]);
    }
}

/* A take refused while the guest released it: no move, a refusal mark. The refusal record
   settles the number, and the re-take names the grant's, two on. */
static void test_contact_of_a_retake_after_a_refused_released_take(void) {
    contact_start();
    sync_authority_host_active(&host, A, true);
    see_host(&guest);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, A));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT16(0, sync_authority_body(&host, A)->number);
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1); /* the refusal */
    sync_authority_host_active(&host, A, false);
    retake_names_the_grant(true, "a re-take after a refused take it released");
    TEST_ASSERT_EQUAL_UINT16(2, sync_authority_body(&host, A)->number);
}

/* The host grants a take and moves the body on by force before the answer leaves: the answer
   names another seat, which the guest reads as a refusal, and the host marks the same. */
static void test_contact_of_a_retake_after_a_grant_overtaken_by_a_forced_move(void) {
    static const char *kinds[] = {"a take-back", "a host claim", "an assign to the host", "an assign to another seat",
                                  "a grant to another seat"};
    for (int kind = 0; kind < 5; kind++) {
        contact_start();
        see_host(&guest);
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, A));
        deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST); /* granted, the answer not sent */
        TEST_ASSERT_EQUAL_UINT8(1, sync_authority_body(&host, A)->seat);
        if (kind == 0) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_reclaim(&host, A));
            sync_authority_settled(&host, A);
        } else if (kind == 1) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&host, A, BODY_SYNC_HOLD_UNTIL_RELEASE));
        } else if (kind == 2) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE));
        } else if (kind == 3) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, 2, BODY_SYNC_HOLD_UNTIL_RELEASE));
        } else {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&other, A, BODY_SYNC_HOLD_UNTIL_RELEASE));
            deliver(&other, 2, &host, BODY_SYNC_SEAT_HOST);
            body_sync_claim_event_t c;
            TEST_ASSERT_TRUE(sync_authority_poll(&host, &c));
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_answer(&host, &c, true, SYNC_AUTHORITY_AS_CLAIMED));
        }
        deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1); /* the answer, naming another seat */
        TEST_ASSERT_FALSE_MESSAGE(sync_authority_local(&guest, sync_authority_body(&guest, A)), kinds[kind]);
        if (kind >= 3) { /* seat 2 gives it back, so a take can be granted */
            deliver(&host, BODY_SYNC_SEAT_HOST, &other, 2);
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&other, A));
            deliver(&other, 2, &host, BODY_SYNC_SEAT_HOST);
            deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1);
        }
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, A)->seat, kinds[kind]);
        retake_names_the_grant(true, kinds[kind]);
    }
}

/* A release crosses the grant and a relayed host state names the new number before the record
   does: the state settles nothing (it carries no refusal mark); the record does. */
static void test_contact_of_a_retake_primed_by_a_relayed_state(void) {
    for (int heard = 0; heard < 2; heard++) {
        contact_start();
        see_host(&guest);
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, A));
        deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
        deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST); /* crosses the grant */
        see_host(&guest);                               /* the state, ahead of the record */
        if (heard) {
            deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1);
        }
        retake_names_the_grant(heard != 0, heard ? "the record heard after the state" : "only the state heard");
    }
}

/* A seat that left gets nothing of an end's grace: its session is over. */
static void test_contact_of_an_arbitrated_end_lapses_when_the_seat_leaves(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_reclaim(&host, A));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(1, n));
    sync_authority_seat_left(&host, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, n));
}

/* A target the guest owned is judged at the guest's own states of it only while the guest may
   still claim that ownership: through the grace after the host took it back, never after, and
   never once the host recalled it. */
static void test_target_states_count_only_under_a_claimable_ownership(void) {
    contact_start();
    own_by_take(&guest, 1);
    sync_authority_body_t *b = sync_authority_body(&host, A);
    const float at[3] = {1.0f, 0.0f, 0.0f};
    sync_authority_received_pose(&host, b, 1, 40, 0, 10.0f, at);
    TEST_ASSERT_NOT_NULL_MESSAGE(sync_authority_pose_at(&host, b, 1, 42, GRACE), "the owner's state of its target was lost");
    TEST_ASSERT_NULL(sync_authority_pose_at(&host, b, 2, 42, GRACE)); /* another seat's claim */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_reclaim(&host, A));
    uint32_t end = host.now;
    host.now = end + GRACE;
    TEST_ASSERT_NOT_NULL_MESSAGE(sync_authority_pose_at(&host, b, 1, 42, GRACE), "the take-back's grace lost the target's states");
    host.now = end + GRACE + 1u;
    TEST_ASSERT_NULL_MESSAGE(sync_authority_pose_at(&host, b, 1, 42, GRACE), "a target taken back was judged at the guest's old state");
    contact_start();
    own_by_take(&guest, 1);
    b = sync_authority_body(&host, A);
    sync_authority_received_pose(&host, b, 1, 40, 0, 10.0f, at);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE));
    TEST_ASSERT_NULL_MESSAGE(sync_authority_pose_at(&host, b, 1, 42, GRACE), "a recalled target was judged at the guest's old state");
}

/* Arbitration: the host settles a contest. The guest simulated on until it heard, so its claims
   of the old number count through the grace. */
static void test_contact_after_a_take_back_counts_within_the_grace(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_reclaim(&host, A));
    expect_grace(1, n, "a take-back");
}

static void test_contact_after_a_host_claim_counts_within_the_grace(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&host, A, BODY_SYNC_HOLD_UNTIL_RELEASE));
    expect_grace(1, n, "a host claim");
}

static void test_contact_after_a_grant_to_another_seat_counts_within_the_grace(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&other, A, BODY_SYNC_HOLD_UNTIL_RELEASE));
    deliver(&other, 2, &host, BODY_SYNC_SEAT_HOST);
    body_sync_claim_event_t c;
    TEST_ASSERT_TRUE(sync_authority_poll(&host, &c));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_answer(&host, &c, true, SYNC_AUTHORITY_AS_CLAIMED));
    TEST_ASSERT_EQUAL_UINT8(2, sync_authority_body(&host, A)->seat);
    expect_grace(1, n, "a grant to another seat");
}

/* Revocation: the host recalls the body; the recall drops the guest's late claims for good. */
static void test_contact_revoked_by_an_assign_to_the_host(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE));
    expect_revoked(n, "an assign to the host");
}

static void test_contact_revoked_by_an_assign_to_another_seat(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, 2, BODY_SYNC_HOLD_UNTIL_RELEASE));
    expect_revoked(n, "an assign to another seat");
}

static void test_contact_revoked_by_an_assign_to_the_same_seat(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, 1, BODY_SYNC_HOLD_UNTIL_RELEASE));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(1, sync_authority_body(&host, A)->number)); /* the new ownership */
    expect_revoked(n, "an assign to the same seat");
}

/* A seat that left: its ownerships are revoked, and joining again starts it afresh. */
static void test_contact_revoked_when_the_seat_left(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    sync_authority_seat_left(&host, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_REVOKED, check(1, n));
    host.now += 1000;
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_REVOKED, check(1, n));
    sync_authority_seat_joined(&host, 1);
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_STALE, check(1, n), "a new session inherited the old one's ownership");
}

/* A repeated join without a leave keeps what the seat's ownerships ended as. */
static void test_contact_ends_survive_a_repeated_join(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE));
    sync_authority_seat_joined(&host, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_REVOKED, check(1, n));
}

/* A refused take's states declared a number the seat never gets: its contacts never count,
   neither when another seat gets that number nor when this seat owns the body later. */
static void test_contact_of_a_refused_take_never_counts(void) {
    contact_start();
    sync_authority_host_active(&host, A, true); /* the host's own agents set it going: no take */
    see_host(&guest);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, A));
    uint16_t predicted = 0;
    TEST_ASSERT_TRUE(sync_authority_contact_number(&guest, sync_authority_body(&guest, A), &predicted));
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, A)->seat);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, predicted));
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1);
    sync_authority_host_active(&host, A, false);
    uint16_t m = own_by_take(&other, 2);
    TEST_ASSERT_EQUAL_UINT16(predicted, m); /* the number the refused take's states carried */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, predicted));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&other, A));
    deliver(&other, 2, &host, BODY_SYNC_SEAT_HOST);
    uint16_t later = own_by_take(&guest, 1);
    TEST_ASSERT_TRUE(later != predicted);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(1, later));
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_STALE, check(1, predicted), "a refused take's number became the seat's");
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, predicted));
}

/* Ids, generations, seats and numbers straight from a peer: never a crash, STALE unless they
   name the body, the seat and the number exactly. */
static void test_contact_fuzzed_ids_are_stale(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    uint32_t x = 0x9E3779B9u;
    const body_sync_id_t edges[] = {BODY_SYNC_ID_NONE, 0xFFFFFFFFu, body_sync_id_make(1, 0), body_sync_id_make(1, 2),
                                    body_sync_id_make(BODY_SYNC_BODIES_MAX, 1), body_sync_id_make(0xFFFF, 1), body_sync_id_make(0xFFFF, 0xFFFF)};
    const sync_authority_ended_t *ended;
    for (uint32_t i = 0; i < 20000; i++) {
        x = x * 1664525u + 1013904223u;
        body_sync_id_t id = i < 7 ? edges[i] : (i % 3 ? x : body_sync_id_make(x & 0x7FF, (x >> 11) & 3));
        uint8_t seat = (uint8_t)(i % 5 ? x >> 7 : 1);
        uint16_t number = (uint16_t)(i % 2 ? n : x >> 13);
        bool exact = id == A && seat == 1 && number == n;
        TEST_ASSERT_EQUAL_MESSAGE(exact ? BODY_SYNC_CONTACT_OK : BODY_SYNC_CONTACT_STALE, check_id(seat, id, number), "a fuzzed claim was judged");
        TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, sync_authority_check_contact(&guest, seat, id, number, x, &ended));
    }
    /* a rebound slot's new body is not the old one */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_unbind(&host, A));
    body_sync_id_t again = body_sync_id_make(body_sync_id_slot(A), 2);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&host, again, 1, n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check_id(1, again, n)); /* bound to seat 1 under n */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_unbind(&host, again));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&host, again, BODY_SYNC_SEAT_HOST, n));
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_STALE, check_id(1, again, n), "a binding inherited an ended ownership");
}

/* The evidence's span stops growing at its ceiling; a pose that is not a number never touches. */
static void test_evidence_is_capped_and_broken_boxes_never_touch(void) {
    TEST_ASSERT_TRUE(sync_policy_evidence_ticks(2.0f, 3.0f, 60) == 6.0f);
    TEST_ASSERT_TRUE(sync_policy_evidence_ticks(1e6f, 3.0f, 60) <= SYNC_POLICY_EVIDENCE_AGE_MAX_S * 60.0f);
    const float lo[3] = {0, 0, 0}, hi[3] = {1, 1, 1}, nan_lo[3] = {NAN, 0, 0}, inf_hi[3] = {INFINITY, 1, 1};
    TEST_ASSERT_TRUE(sync_policy_box_gap(lo, hi, lo, hi) == 0.0f);
    TEST_ASSERT_TRUE(isinf(sync_policy_box_gap(lo, hi, nan_lo, hi)));
    TEST_ASSERT_TRUE(isinf(sync_policy_box_gap(lo, inf_hi, lo, hi)));
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        plant_revoked_as_arbitrated = plant_revoked_as_arbitrated || strcmp(argv[i], "--plant-revoked-as-arbitrated") == 0;
        plant_grows = plant_grows || strcmp(argv[i], "--plant-evidence-grows-over-gaps") == 0;
        plant_ring_approach = plant_ring_approach || strcmp(argv[i], "--plant-approach-from-ring") == 0;
        plant_age = plant_age || strcmp(argv[i], "--plant-evidence-age-unbounded") == 0;
        plant_teleport = plant_teleport || strcmp(argv[i], "--plant-teleport-keeps-ring") == 0;
        plant_target_any = plant_target_any || strcmp(argv[i], "--plant-target-any-ownership") == 0;
        plant_rewind_off = plant_rewind_off || strcmp(argv[i], "--plant-target-rewind-off") == 0;
    }
    UNITY_BEGIN();
    RUN_TEST(test_routes_and_record_kinds);
    RUN_TEST(test_a_settled_hold_ends_where_it_should);
    RUN_TEST(test_a_holder_is_named_by_its_low_generation_bits);
    RUN_TEST(test_a_take_continues_the_record_then_the_state_then_the_bind);
    RUN_TEST(test_records_carry_the_body_and_owners_keep_their_own);
    RUN_TEST(test_a_seat_that_left_keeps_the_carrier);
    RUN_TEST(test_contact_of_a_body_owned_now);
    RUN_TEST(test_contact_after_a_voluntary_release);
    RUN_TEST(test_pose_travel_is_what_the_host_took);
    RUN_TEST(test_evidence_follows_the_path_the_host_saw);
    RUN_TEST(test_evidence_of_a_silent_agent_does_not_grow);
    RUN_TEST(test_evidence_of_a_body_that_slept_has_no_long_chord);
    RUN_TEST(test_a_stamp_jump_buys_no_long_ray);
    RUN_TEST(test_a_teleport_step_is_refused);
    RUN_TEST(test_a_teleport_starts_the_evidence_over);
    RUN_TEST(test_approach_is_the_step_at_the_contact);
    RUN_TEST(test_a_contact_named_long_after_has_no_evidence);
    RUN_TEST(test_a_flying_target_is_judged_where_its_path_had_it);
    RUN_TEST(test_compressed_stamps_buy_no_speed_and_no_chord);
    RUN_TEST(test_contact_after_a_take_by_another_seat);
    RUN_TEST(test_contact_after_a_flight_returns);
    RUN_TEST(test_contact_of_a_retake_before_the_release_is_heard);
    RUN_TEST(test_contact_of_a_retake_after_a_withdrawn_claim);
    RUN_TEST(test_contact_of_a_retake_after_a_release_crossed_the_grant);
    RUN_TEST(test_contact_of_a_retake_after_a_refused_released_take);
    RUN_TEST(test_contact_of_a_retake_after_a_grant_overtaken_by_a_forced_move);
    RUN_TEST(test_contact_of_a_retake_primed_by_a_relayed_state);
    RUN_TEST(test_contact_of_an_arbitrated_end_lapses_when_the_seat_leaves);
    RUN_TEST(test_target_states_count_only_under_a_claimable_ownership);
    RUN_TEST(test_contact_after_a_take_back_counts_within_the_grace);
    RUN_TEST(test_contact_after_a_host_claim_counts_within_the_grace);
    RUN_TEST(test_contact_after_a_grant_to_another_seat_counts_within_the_grace);
    RUN_TEST(test_contact_revoked_by_an_assign_to_the_host);
    RUN_TEST(test_contact_revoked_by_an_assign_to_another_seat);
    RUN_TEST(test_contact_revoked_by_an_assign_to_the_same_seat);
    RUN_TEST(test_contact_revoked_when_the_seat_left);
    RUN_TEST(test_contact_ends_survive_a_repeated_join);
    RUN_TEST(test_contact_of_a_refused_take_never_counts);
    RUN_TEST(test_contact_fuzzed_ids_are_stale);
    RUN_TEST(test_evidence_is_capped_and_broken_boxes_never_touch);
    return UNITY_END();
}
