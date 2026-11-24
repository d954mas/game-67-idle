#include <math.h>
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
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_answer(&host, &c, true));
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
    float at[3] = {1.0f, 2.0f, 3.0f};
    sync_authority_received_pose(sync_authority_body(&host, A), 1, at, 4.0f);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, A)->seat);
    const sync_authority_ended_t *ended;
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, sync_authority_check_contact(&host, 1, A, n, GRACE, &ended));
    TEST_ASSERT_NOT_NULL(ended);
    TEST_ASSERT_TRUE_MESSAGE(ended->has_pose && ended->position[2] == 3.0f && ended->speed == 4.0f, "the end lost the seat's last state");
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, sync_authority_body(&host, A)->number)); /* the host's number now */
    expect_grace(1, n, "a voluntary end");
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
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_answer(&host, &c, true));
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
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_answer(&host, &c, true));
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
    RUN_TEST(test_contact_after_a_take_by_another_seat);
    RUN_TEST(test_contact_after_a_flight_returns);
    RUN_TEST(test_contact_of_a_retake_before_the_release_is_heard);
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
