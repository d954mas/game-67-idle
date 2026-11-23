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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_routes_and_record_kinds);
    RUN_TEST(test_a_settled_hold_ends_where_it_should);
    RUN_TEST(test_a_holder_is_named_by_its_low_generation_bits);
    RUN_TEST(test_a_take_continues_the_record_then_the_state_then_the_bind);
    RUN_TEST(test_records_carry_the_body_and_owners_keep_their_own);
    RUN_TEST(test_a_seat_that_left_keeps_the_carrier);
    return UNITY_END();
}
