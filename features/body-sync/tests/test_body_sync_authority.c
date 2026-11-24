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

static sync_authority_t other;
static bool plant_forced_as_voluntary; /* --plant-forced-as-voluntary: the named test must fail */

/* Guests 1 and 2 of a host in the interaction mode, all with the host's light body A. */
static void contact_start(void) {
    sync_authority_t *sides[] = {&host, &guest, &other};
    for (uint8_t i = 0; i < 3; i++) {
        sync_authority_init(sides[i], i, SEATS, error, sizeof(error));
        sides[i]->mode = SYNC_POLICY_INTERACTION;
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(sides[i], A, BODY_SYNC_SEAT_HOST, 0));
    }
    for (uint8_t i = 1; i < 3; i++) {
        sync_authority_seat_joined(&host, i);
        sync_authority_seat_joined(sides[i], BODY_SYNC_SEAT_HOST);
    }
    host.plant_forced_as_voluntary = plant_forced_as_voluntary;
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

static body_sync_contact_verdict_t check(uint8_t seat, uint16_t number) { return sync_authority_check_contact(&host, seat, A, number); }

/* `g` (seat `seat`) takes A, the host grants it, and the grant comes back; returns the number
   the guest's contact claims named while the take waited. */
static uint16_t own_by_take(sync_authority_t *g, uint8_t seat) {
    see_host(g);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(g, A));
    uint16_t number = 0;
    TEST_ASSERT_TRUE_MESSAGE(sync_authority_contact_number(g, sync_authority_body(g, A), &number), "a waiting take names no number");
    deliver(g, seat, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(seat, sync_authority_body(&host, A)->seat, "the take was refused");
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_OK, check(seat, number), "the waiting take's number is not the grant's");
    deliver(&host, BODY_SYNC_SEAT_HOST, g, seat);
    uint16_t after = 0;
    TEST_ASSERT_TRUE(sync_authority_contact_number(g, sync_authority_body(g, A), &after));
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(number, after, "the granted owner names another number");
    return number;
}

static void test_contact_of_a_body_owned_now(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(1, n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, (uint16_t)(n + 1))); /* a number it never had */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, (uint16_t)(n - 1)));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(2, n));                   /* another seat's claim */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(BODY_SYNC_SEAT_HOST, n)); /* the host judges its own */
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, sync_authority_check_contact(&guest, 1, A, n)); /* host only */
    uint16_t none = 0;
    TEST_ASSERT_FALSE(sync_authority_contact_number(&other, sync_authority_body(&other, A), &none)); /* a copy names none */
}

/* The guest's release reaches the host before the claim of the contact that ended the flight:
   the claim still counts, and keeps counting while another seat owns the body since. */
static void test_contact_after_a_voluntary_release(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
    deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, A)->seat);
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_OK, check(1, n), "an honest claim after its own release was rejected");
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, sync_authority_body(&host, A)->number)); /* the host's number now */
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, 1);
    uint16_t m = own_by_take(&other, 2);
    TEST_ASSERT_TRUE(m != n);
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_OK, check(1, n), "another seat's ownership since revoked a past flight");
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(2, m));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(2, n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, m));
    /* the host taking it back from seat 2 ends seat 2's ownership only */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_reclaim(&host, A));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_REVOKED, check(2, m));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, check(1, n));
    /* seat 1 owning it again and losing it to force: its earlier flight is past the one end kept */
    sync_authority_settled(&host, A);
    uint16_t again = own_by_take(&guest, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_REVOKED, check(1, again));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, n));
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
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_OK, check(1, n), "the claim of the contact that ended the flight was rejected");
}

/* Every move the host makes against the owner's will revokes that ownership's claims. */
static void test_contact_forced_moves_revoke(void) {
    static const char *kinds[] = {"assign to the host", "assign to another seat", "assign to the same seat",
                                  "a host claim",       "a take-back",            "a grant to another seat"};
    for (int kind = 0; kind < 6; kind++) {
        contact_start();
        uint16_t n = own_by_take(&guest, 1);
        if (kind == 0) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE));
        } else if (kind == 1) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, 2, BODY_SYNC_HOLD_UNTIL_RELEASE));
        } else if (kind == 2) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_assign(&host, A, 1, BODY_SYNC_HOLD_UNTIL_RELEASE));
        } else if (kind == 3) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&host, A, BODY_SYNC_HOLD_UNTIL_RELEASE));
        } else if (kind == 4) {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_reclaim(&host, A));
        } else {
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_claim(&other, A, BODY_SYNC_HOLD_UNTIL_RELEASE));
            deliver(&other, 2, &host, BODY_SYNC_SEAT_HOST);
            body_sync_claim_event_t c;
            TEST_ASSERT_TRUE(sync_authority_poll(&host, &c));
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_answer(&host, &c, true));
            TEST_ASSERT_EQUAL_UINT8(2, sync_authority_body(&host, A)->seat);
        }
        TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_REVOKED, check(1, n), kinds[kind]);
        /* a release the guest sent before it heard of the move is stale: it ends nothing again */
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_release(&guest, A));
        deliver(&guest, 1, &host, BODY_SYNC_SEAT_HOST);
        TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_REVOKED, check(1, n), kinds[kind]);
    }
}

/* A seat that left owns nothing it could claim, and joining again starts it afresh. */
static void test_contact_of_a_seat_that_left_is_stale(void) {
    contact_start();
    uint16_t n = own_by_take(&guest, 1);
    sync_authority_seat_left(&host, 1);
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, n));
    sync_authority_seat_joined(&host, 1);
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_STALE, check(1, n), "a new session inherited the old one's ownership");
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
    for (uint32_t i = 0; i < 20000; i++) {
        x = x * 1664525u + 1013904223u;
        body_sync_id_t id = i < 7 ? edges[i] : (i % 3 ? x : body_sync_id_make(x & 0x7FF, (x >> 11) & 3));
        uint8_t seat = (uint8_t)(i % 5 ? x >> 7 : 1);
        uint16_t number = (uint16_t)(i % 2 ? n : x >> 13);
        bool exact = id == A && seat == 1 && number == n;
        TEST_ASSERT_EQUAL_MESSAGE(exact ? BODY_SYNC_CONTACT_OK : BODY_SYNC_CONTACT_STALE, sync_authority_check_contact(&host, seat, id, number),
                                  "a fuzzed claim was judged");
        TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, sync_authority_check_contact(&guest, seat, id, number));
    }
    /* a rebound slot's new body is not the old one */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_unbind(&host, A));
    body_sync_id_t again = body_sync_id_make(body_sync_id_slot(A), 2);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&host, again, 1, n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_STALE, check(1, n));
    TEST_ASSERT_EQUAL(BODY_SYNC_CONTACT_OK, sync_authority_check_contact(&host, 1, again, n)); /* bound to seat 1 under n */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_unbind(&host, again));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&host, again, BODY_SYNC_SEAT_HOST, n));
    TEST_ASSERT_EQUAL_MESSAGE(BODY_SYNC_CONTACT_STALE, sync_authority_check_contact(&host, 1, again, n), "a binding inherited an ended ownership");
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        plant_forced_as_voluntary = plant_forced_as_voluntary || strcmp(argv[i], "--plant-forced-as-voluntary") == 0;
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
    RUN_TEST(test_contact_after_a_flight_returns);
    RUN_TEST(test_contact_forced_moves_revoke);
    RUN_TEST(test_contact_of_a_seat_that_left_is_stale);
    RUN_TEST(test_contact_of_a_refused_take_never_counts);
    RUN_TEST(test_contact_fuzzed_ids_are_stale);
    return UNITY_END();
}
