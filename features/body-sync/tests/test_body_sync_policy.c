#include <string.h>

#include "sync_authority.h"
#include "sync_policy.h"
#include "unity.h"

/* The authority policy alone (sync_policy.h) and the takes it drives through sync_authority: a
   flight that meets a takable body stays its thrower's, who takes what and when, the host's
   answer, the host's agents first, the return at rest and its bounds, and the claim on the wire.
   No Box3D. */

void setUp(void) {}
void tearDown(void) {}

static sync_policy_take_in_t free_body(void) {
    return (sync_policy_take_in_t){.mode = SYNC_POLICY_INTERACTION,
                                   .light = true,
                                   .owner_seat = BODY_SYNC_SEAT_HOST,
                                   .owner_hold = BODY_SYNC_HOLD_UNTIL_RELEASE,
                                   .agent_speed = 3.0f};
}

static void test_a_flight_that_meets_a_takable_body_stays_its_throwers(void) {
    sync_policy_flight_in_t in = {.mode = SYNC_POLICY_INTERACTION};
    TEST_ASSERT_EQUAL(SYNC_FLIGHT_ON, sync_policy_flight(&in));
    in.touched_body = true;
    TEST_ASSERT_EQUAL(SYNC_FLIGHT_INTERACTS, sync_policy_flight(&in));
    in.touched_untakable = true; /* a heavy body or a character: the host judges it */
    TEST_ASSERT_EQUAL(SYNC_FLIGHT_RETURNS, sync_policy_flight(&in));
    in = (sync_policy_flight_in_t){.mode = SYNC_POLICY_INTERACTION, .asleep = true};
    TEST_ASSERT_EQUAL(SYNC_FLIGHT_RETURNS, sync_policy_flight(&in));
    /* The host policy keeps 0.7.0's rule: any touch of a body ends the flight. */
    in = (sync_policy_flight_in_t){.mode = SYNC_POLICY_HOST, .touched_body = true};
    TEST_ASSERT_EQUAL(SYNC_FLIGHT_RETURNS, sync_policy_flight(&in));
}

static void test_a_guest_takes_a_free_light_body_its_moving_agent_touches(void) {
    sync_policy_take_in_t in = free_body();
    TEST_ASSERT_TRUE(sync_policy_takes(&in));
    in.agent_speed = SYNC_POLICY_AGENT_SPEED; /* a lean takes nothing */
    TEST_ASSERT_FALSE(sync_policy_takes(&in));
    in.pushed = true; /* a push from standstill does */
    TEST_ASSERT_TRUE(sync_policy_takes(&in));
    const struct {
        const char *why;
        sync_policy_take_in_t in;
    } refused[] = {
        {"heavy", {.mode = SYNC_POLICY_INTERACTION, .light = false, .owner_hold = 0, .agent_speed = 3.0f}},
        {"the host policy", {.mode = SYNC_POLICY_HOST, .light = true, .agent_speed = 3.0f}},
        {"carried", {.mode = SYNC_POLICY_INTERACTION, .light = true, .held = true, .agent_speed = 3.0f}},
        {"kinematic at its owner", {.mode = SYNC_POLICY_INTERACTION, .light = true, .owner_kinematic = true, .agent_speed = 3.0f}},
        {"set going by the host", {.mode = SYNC_POLICY_INTERACTION, .light = true, .owner_hold = BODY_SYNC_HOLD_UNTIL_REST, .agent_speed = 3.0f}},
        {"thrown by the host", {.mode = SYNC_POLICY_INTERACTION, .light = true, .owner_hold = BODY_SYNC_HOLD_UNTIL_SETTLED, .agent_speed = 3.0f}},
        {"another guest's", {.mode = SYNC_POLICY_INTERACTION, .light = true, .owner_seat = 2, .agent_speed = 3.0f}},
        {"already its own", {.mode = SYNC_POLICY_INTERACTION, .light = true, .local = true, .agent_speed = 3.0f}},
        {"owned bound", {.mode = SYNC_POLICY_INTERACTION, .light = true, .agent_speed = 3.0f, .owned = SYNC_POLICY_OWNED_MAX}},
        {"takes a tick", {.mode = SYNC_POLICY_INTERACTION, .light = true, .agent_speed = 3.0f, .taken = SYNC_POLICY_TAKES_PER_TICK}},
        {"given back under its press", {.mode = SYNC_POLICY_INTERACTION, .light = true, .agent_speed = 3.0f, .pushed = true, .returned_pressed = true}},
    };
    for (size_t i = 0; i < sizeof(refused) / sizeof(refused[0]); i++) {
        TEST_ASSERT_FALSE_MESSAGE(sync_policy_takes(&refused[i].in), refused[i].why);
    }
    /* The host marks its own free body only, with no bound: it sends nothing for it. */
    in = free_body();
    in.host = true;
    TEST_ASSERT_FALSE(sync_policy_takes(&in));
    in.local = true;
    in.owned = 1000;
    in.taken = 1000;
    TEST_ASSERT_TRUE(sync_policy_takes(&in));
}

static void test_the_host_grants_free_bodies_only(void) {
    sync_policy_grant_in_t in = {.mode = SYNC_POLICY_INTERACTION, .light = true, .claimant = 1, .seat = BODY_SYNC_SEAT_HOST, .budget = 1.0f};
    TEST_ASSERT_TRUE(sync_policy_grants(&in));
    in.seat = 1; /* a duplicate of its own */
    TEST_ASSERT_TRUE(sync_policy_grants(&in));
    in.seat = 2; /* earliest confirmed toucher wins */
    TEST_ASSERT_FALSE(sync_policy_grants(&in));
    in.seat = BODY_SYNC_SEAT_HOST;
    in.hold = BODY_SYNC_HOLD_UNTIL_REST; /* the host's agents set it going: host first */
    TEST_ASSERT_FALSE(sync_policy_grants(&in));
    in.hold = BODY_SYNC_HOLD_UNTIL_RELEASE;
    in.owned = SYNC_POLICY_OWNED_MAX;
    TEST_ASSERT_FALSE(sync_policy_grants(&in));
    in.owned = 0;
    in.held = true;
    TEST_ASSERT_FALSE(sync_policy_grants(&in));
    in.held = false;
    in.kinematic = true;
    TEST_ASSERT_FALSE(sync_policy_grants(&in));
    in.kinematic = false;
    in.light = false;
    TEST_ASSERT_FALSE(sync_policy_grants(&in));
    in.light = true;
    in.mode = SYNC_POLICY_HOST; /* a peer configured otherwise: refused, never asserted */
    TEST_ASSERT_FALSE(sync_policy_grants(&in));
}

static void test_the_hosts_agents_come_first(void) {
    sync_policy_reclaim_in_t in = {.mode = SYNC_POLICY_INTERACTION,
                                   .priority = SYNC_POLICY_HOST_FIRST,
                                   .owner_seat = 1,
                                   .owner_hold = BODY_SYNC_HOLD_UNTIL_REST,
                                   .agent_speed = 2.0f};
    TEST_ASSERT_TRUE(sync_policy_host_reclaims(&in));
    in.agent_speed = 0.1f;
    TEST_ASSERT_FALSE(sync_policy_host_reclaims(&in));
    in.pushed = true;
    TEST_ASSERT_TRUE(sync_policy_host_reclaims(&in));
    in.owner_hold = BODY_SYNC_HOLD_UNTIL_RELEASE; /* carried by the guest: the game's rule */
    TEST_ASSERT_FALSE(sync_policy_host_reclaims(&in));
    in.owner_hold = BODY_SYNC_HOLD_UNTIL_REST;
    in.priority = SYNC_POLICY_EARLIEST;
    TEST_ASSERT_FALSE(sync_policy_host_reclaims(&in));
    TEST_ASSERT_EQUAL(SYNC_POLICY_HOST_FIRST, SYNC_POLICY_PRIORITY);
}

static void test_a_body_returns_once_it_and_its_neighbours_rest_and_the_host_holds_its_pose(void) {
    sync_policy_rest_in_t in = {.quiet = true, .neighbours_quiet = true, .rest_acked = true};
    TEST_ASSERT_TRUE(sync_policy_returns_at_rest(&in));
    in.rest_acked = false;
    TEST_ASSERT_FALSE(sync_policy_returns_at_rest(&in));
    in.host = true; /* the host is the authority: no acknowledgement, only its hysteresis */
    TEST_ASSERT_FALSE(sync_policy_returns_at_rest(&in));
    in.age = sync_policy_host_hold_ticks(0.0f);
    TEST_ASSERT_TRUE(sync_policy_returns_at_rest(&in));
    in = (sync_policy_rest_in_t){.quiet = true, .rest_acked = true};
    TEST_ASSERT_FALSE(sync_policy_returns_at_rest(&in)); /* a neighbour still moves */
    in = (sync_policy_rest_in_t){.rest_acked = true};
    TEST_ASSERT_FALSE(sync_policy_returns_at_rest(&in));
    in.age = SYNC_POLICY_AGE_TICKS;
    TEST_ASSERT_TRUE(sync_policy_returns_at_rest(&in));
    in = (sync_policy_rest_in_t){.out_of_box = true};
    TEST_ASSERT_TRUE(sync_policy_returns_at_rest(&in));
    in.released = SYNC_POLICY_RELEASES_PER_TICK;
    TEST_ASSERT_FALSE(sync_policy_returns_at_rest(&in));
}

/* A quiet light body the host's agent brushes twice, `gap` ticks apart, while a guest's moving
   agent presses it from tick 1 and takes it whenever the newest state it has (one way late) shows
   it free; each take is answered on the host one way after it was sent. Counts the host's holds
   and the guest's refused takes. */
static void brush_twice(uint32_t gap, float round_trip, uint32_t *holds, uint32_t *refusals) {
    enum { TICKS = 400, WAY_MAX = 64 };
    uint32_t way = (uint32_t)(round_trip / 2.0f);
    uint8_t hold[TICKS];
    uint32_t agent_tick = 0, take_lands = UINT32_MAX;
    uint8_t now = BODY_SYNC_HOLD_UNTIL_RELEASE, seat = BODY_SYNC_SEAT_HOST;
    *holds = *refusals = 0;
    TEST_ASSERT_TRUE(way < WAY_MAX);
    for (uint32_t t = 0; t < TICKS; t++) {
        if (t == take_lands) {
            sync_policy_grant_in_t g = {.mode = SYNC_POLICY_INTERACTION, .light = true, .claimant = 1, .seat = seat, .hold = now,
                                        .budget = 8.0f, .agent_reach = 1.0f};
            if (sync_policy_grants(&g)) {
                seat = 1;
            } else {
                (*refusals)++;
            }
            take_lands = UINT32_MAX;
        }
        if ((t == 0 || t == gap) && seat == BODY_SYNC_SEAT_HOST) {
            sync_policy_take_in_t mark = free_body();
            mark.host = mark.local = true;
            mark.owner_hold = now;
            if (now == BODY_SYNC_HOLD_UNTIL_REST) {
                agent_tick = t; /* a touch of its own hold starts its age again */
            } else if (sync_policy_takes(&mark)) {
                now = BODY_SYNC_HOLD_UNTIL_REST;
                agent_tick = t;
                (*holds)++;
            }
        }
        sync_policy_rest_in_t r = {.host = true, .quiet = true, .neighbours_quiet = true, .age = t - agent_tick, .round_trip_ticks = round_trip};
        if (seat == BODY_SYNC_SEAT_HOST && now == BODY_SYNC_HOLD_UNTIL_REST && sync_policy_returns_at_rest(&r)) {
            now = BODY_SYNC_HOLD_UNTIL_RELEASE;
        }
        hold[t] = now;
        sync_policy_take_in_t take = free_body();
        take.owner_hold = t >= way ? hold[t - way] : BODY_SYNC_HOLD_UNTIL_RELEASE;
        if (t >= 1 && seat == BODY_SYNC_SEAT_HOST && take_lands == UINT32_MAX && sync_policy_takes(&take)) {
            take_lands = t + way;
        }
    }
}

/* A host agent that brushes a body twice inside its hysteresis keeps one hold, and a guest's take
   lost to the first touch is the only one refused. Without the hysteresis the hold ends between
   the touches, the guest takes on the end's first state and the second touch refuses it again. */
static void test_the_hosts_hold_outlasts_a_walks_second_touch(void) {
    for (float rtt = 6.0f; rtt <= 36.0f; rtt += 6.0f) {
        uint32_t window = sync_policy_host_hold_ticks(rtt), holds, refusals;
        TEST_ASSERT_TRUE(window >= SYNC_POLICY_HOST_CLEAR_TICKS + (uint32_t)rtt);
        for (uint32_t gap = 1; gap < window; gap++) {
            brush_twice(gap, rtt, &holds, &refusals);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, holds, "one walk, one hold");
            TEST_ASSERT_TRUE_MESSAGE(refusals <= 1u, "a take is refused once at most");
        }
    }
    sync_policy_rest_in_t r = {.host = true, .quiet = true, .neighbours_quiet = true, .round_trip_ticks = 1e9f};
    r.age = SYNC_POLICY_HOST_CLEAR_TICKS;
    TEST_ASSERT_TRUE_MESSAGE(sync_policy_returns_at_rest(&r), "an unmeasured or absurd round trip adds nothing");
}

/* Bodies of two guests owned by interaction that touch go to the host: nothing else is contested. */
static void test_two_guests_touching_bodies_go_to_the_host(void) {
    sync_policy_contest_in_t in = {.mode = SYNC_POLICY_INTERACTION, .seat = 1, .other_seat = 2, .hold = BODY_SYNC_HOLD_UNTIL_REST, .other_hold = BODY_SYNC_HOLD_UNTIL_REST};
    TEST_ASSERT_TRUE(sync_policy_host_takes_contested(&in));
    sync_policy_contest_in_t x = in;
    x.other_seat = 1;
    TEST_ASSERT_FALSE_MESSAGE(sync_policy_host_takes_contested(&x), "one guest's own heap");
    x = in;
    x.other_seat = BODY_SYNC_SEAT_HOST;
    TEST_ASSERT_FALSE_MESSAGE(sync_policy_host_takes_contested(&x), "the host's body: its agents decide");
    x = in;
    x.seat = BODY_SYNC_SEAT_HOST;
    TEST_ASSERT_FALSE_MESSAGE(sync_policy_host_takes_contested(&x), "the host's body on either side");
    x = in;
    x.other_hold = BODY_SYNC_HOLD_UNTIL_RELEASE;
    TEST_ASSERT_FALSE_MESSAGE(sync_policy_host_takes_contested(&x), "a carried or claimed body");
    x = in;
    x.hold = BODY_SYNC_HOLD_UNTIL_SETTLED;
    TEST_ASSERT_FALSE_MESSAGE(sync_policy_host_takes_contested(&x), "a flight, judged when it lands");
    x = in;
    x.mode = SYNC_POLICY_HOST;
    TEST_ASSERT_FALSE(sync_policy_host_takes_contested(&x));
}

static void test_quiet_is_asleep_or_still_for_a_quarter_second(void) {
    sync_policy_still_t still = {0};
    float p[3] = {1.0f, 2.0f, 3.0f}, q[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (uint32_t i = 0; i < SYNC_POLICY_QUIET_TICKS; i++) {
        TEST_ASSERT_FALSE(sync_policy_quiet(&still, p, q, false));
        p[0] += 0.0001f; /* a creep within the stillness */
    }
    TEST_ASSERT_TRUE(sync_policy_quiet(&still, p, q, false));
    p[0] += 0.01f;
    TEST_ASSERT_FALSE(sync_policy_quiet(&still, p, q, false));
    TEST_ASSERT_TRUE(sync_policy_quiet(&still, p, q, true));
}

/* ---- the take on the wire ---- */

enum { GUEST = 1, OTHER = 2, SEATS = 3 };
static sync_authority_t host, guest, other;
static char errors[3][128];
static const body_sync_id_t BOX = 0x00010005u, HEAVY = 0x00010006u;

static void setup_three(void) {
    sync_authority_init(&host, BODY_SYNC_SEAT_HOST, SEATS, errors[0], sizeof(errors[0]));
    sync_authority_init(&guest, GUEST, SEATS, errors[1], sizeof(errors[1]));
    sync_authority_init(&other, OTHER, SEATS, errors[2], sizeof(errors[2]));
    sync_authority_t *all[3] = {&host, &guest, &other};
    for (int i = 0; i < 3; i++) {
        all[i]->mode = SYNC_POLICY_INTERACTION;
        for (uint8_t seat = 0; seat < SEATS; seat++) {
            sync_authority_seat_joined(all[i], seat);
        }
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(all[i], BOX, BODY_SYNC_SEAT_HOST, 0));
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(all[i], HEAVY, BODY_SYNC_SEAT_HOST, 0));
        sync_authority_body(all[i], BOX)->takable = true;
    }
}

/* Every record `from` owes `seat`, received there. */
static void deliver(sync_authority_t *from, uint8_t from_seat, sync_authority_t *to, uint8_t seat) {
    uint8_t buffer[SYNC_AUTHORITY_RECORD_BYTES];
    uint32_t size, stale = 0;
    char why[96];
    while (sync_authority_peek(from, seat, buffer, sizeof(buffer), &size)) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_receive(to, from_seat, 1, buffer, size, &stale, why, sizeof(why)));
        sync_authority_pop(from, seat);
    }
}

static void test_a_take_is_optimistic_and_granted_by_the_host_itself(void) {
    setup_three();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, BOX));
    sync_authority_body_t *g = sync_authority_body(&guest, BOX);
    TEST_ASSERT_TRUE(sync_authority_local(&guest, g));
    TEST_ASSERT_EQUAL(BODY_SYNC_HOLD_UNTIL_REST, g->hold);
    /* Its states declare the number the grant will give. */
    TEST_ASSERT_EQUAL_UINT16(1, sync_authority_declared_number(&guest, g));
    deliver(&guest, GUEST, &host, BODY_SYNC_SEAT_HOST);
    body_sync_claim_event_t polled;
    TEST_ASSERT_FALSE(sync_authority_poll(&host, &polled)); /* the caller never sees a take */
    sync_authority_body_t *h = sync_authority_body(&host, BOX);
    TEST_ASSERT_EQUAL_UINT8(GUEST, h->seat);
    TEST_ASSERT_EQUAL_UINT16(1, h->number);
    TEST_ASSERT_EQUAL_UINT32(1, sync_authority_owned(&host, GUEST));
    /* Only the claimant hears it: the others read the owner from the relayed state. */
    uint8_t buffer[SYNC_AUTHORITY_RECORD_BYTES];
    uint32_t size;
    TEST_ASSERT_FALSE(sync_authority_peek(&host, OTHER, buffer, sizeof(buffer), &size));
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, GUEST);
    TEST_ASSERT_FALSE(g->claim_pending);
    TEST_ASSERT_TRUE(sync_authority_local(&guest, g));
    TEST_ASSERT_EQUAL_UINT16(1, sync_authority_declared_number(&guest, g));
}

/* A guest's bound counts its takes still waiting for the grant, the host's only what it granted. */
/* The host grants a take only with evidence and budget, and never to itself. */
static void test_a_grant_needs_a_touch_a_budget_and_a_guest(void) {
    sync_policy_grant_in_t in = {.mode = SYNC_POLICY_INTERACTION, .light = true, .claimant = GUEST, .seat = BODY_SYNC_SEAT_HOST,
                                 .hold = BODY_SYNC_HOLD_UNTIL_RELEASE, .budget = 1.0f, .agent_gap = 0.0f, .agent_reach = SYNC_POLICY_TAKE_SLOP_M};
    TEST_ASSERT_TRUE(sync_policy_grants(&in));
    sync_policy_grant_in_t x = in;
    x.claimant = BODY_SYNC_SEAT_HOST;
    TEST_ASSERT_FALSE_MESSAGE(sync_policy_grants(&x), "a take by the host itself");
    x = in;
    x.budget = 0.5f;
    TEST_ASSERT_FALSE_MESSAGE(sync_policy_grants(&x), "past the take budget");
    x = in;
    x.agent_gap = 2.0f;
    TEST_ASSERT_FALSE_MESSAGE(sync_policy_grants(&x), "no agent of the claimant near the body");
    /* The reach grows with what the agent and the body travel while the host's copy trails. */
    TEST_ASSERT_TRUE(sync_policy_take_reach(4.0f, 0.0f, 0.25f) >= SYNC_POLICY_TAKE_SLOP_M + 1.0f - 1e-4f);
    TEST_ASSERT_TRUE(sync_policy_take_reach(0.0f, 0.0f, 1.0f) >= SYNC_POLICY_TAKE_SLOP_M);
}

/* The host spends a seat's take budget per claim, refills it per tick and keeps the bound of
   owned bodies as a running count. */
static void test_the_host_bounds_a_seats_takes_per_tick(void) {
    sync_authority_t h;
    char err[128];
    sync_authority_init(&h, BODY_SYNC_SEAT_HOST, SEATS, err, sizeof(err));
    h.mode = SYNC_POLICY_INTERACTION;
    sync_authority_seat_joined(&h, GUEST);
    enum { N = 200 };
    for (uint32_t i = 0; i < N; i++) {
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_bind(&h, body_sync_id_make(i, 1), BODY_SYNC_SEAT_HOST, 0));
        sync_authority_body(&h, body_sync_id_make(i, 1))->takable = true;
    }
    uint32_t granted = 0, stale = 0;
    char why[96];
    for (uint32_t i = 0; i < N; i++) {
        stub_record_t r = {.type = STUB_REC_CLAIM, .id = body_sync_id_make(i, 1), .hold = BODY_SYNC_HOLD_UNTIL_REST, .sequence = (uint16_t)(i + 1)};
        uint8_t buffer[SYNC_AUTHORITY_RECORD_BYTES];
        uint32_t size = stub_wire_put_record(&r, buffer, sizeof(buffer));
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_receive(&h, GUEST, 100, buffer, size, &stale, why, sizeof(why)));
        if (sync_authority_body(&h, r.id)->seat == GUEST) {
            granted++;
            /* Handed back at once, so the owned bound never decides here. */
            TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_reclaim(&h, r.id));
            sync_authority_host_active(&h, r.id, false);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(SYNC_POLICY_TAKES_PER_TICK * SYNC_POLICY_TAKE_BURST_TICKS, granted);
    TEST_ASSERT_EQUAL_UINT32(N - granted, h.refused_takes[GUEST]);
    TEST_ASSERT_EQUAL_UINT32(0, sync_authority_owned(&h, GUEST));
    /* A tick later: one tick's worth more, and the running count follows the grants. */
    uint32_t more = 0;
    for (uint32_t i = 0; i < 40; i++) {
        stub_record_t r = {.type = STUB_REC_CLAIM, .id = body_sync_id_make(i, 1), .hold = BODY_SYNC_HOLD_UNTIL_REST, .sequence = (uint16_t)(N + i + 1)};
        uint8_t buffer[SYNC_AUTHORITY_RECORD_BYTES];
        uint32_t size = stub_wire_put_record(&r, buffer, sizeof(buffer));
        TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_receive(&h, GUEST, 101, buffer, size, &stale, why, sizeof(why)));
        more += sync_authority_body(&h, r.id)->seat == GUEST ? 1u : 0u;
    }
    TEST_ASSERT_EQUAL_UINT32(SYNC_POLICY_TAKES_PER_TICK, more);
    TEST_ASSERT_EQUAL_UINT32(more, sync_authority_owned(&h, GUEST));
}

static void test_a_guests_count_holds_its_waiting_takes(void) {
    setup_three();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, BOX));
    TEST_ASSERT_EQUAL_UINT32(1, sync_authority_owned(&guest, GUEST));
    TEST_ASSERT_EQUAL_UINT32(0, sync_authority_owned(&host, GUEST));
    deliver(&guest, GUEST, &host, BODY_SYNC_SEAT_HOST);
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, GUEST);
    TEST_ASSERT_EQUAL_UINT32(1, sync_authority_owned(&guest, GUEST));
    TEST_ASSERT_EQUAL_UINT32(1, sync_authority_owned(&host, GUEST));
    TEST_ASSERT_EQUAL_UINT32(0, sync_authority_owned(&other, GUEST)); /* it never heard the grant */
}

static void test_a_take_of_a_body_the_host_set_going_is_refused(void) {
    setup_three();
    sync_authority_host_active(&host, BOX, true);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, BOX));
    deliver(&guest, GUEST, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, BOX)->seat);
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, GUEST);
    TEST_ASSERT_FALSE(sync_authority_local(&guest, sync_authority_body(&guest, BOX)));
    /* A heavy body is refused alike. */
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, HEAVY));
    deliver(&guest, GUEST, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, HEAVY)->seat);
}

static void test_two_guests_the_earliest_wins(void) {
    setup_three();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, BOX));
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&other, BOX));
    deliver(&guest, GUEST, &host, BODY_SYNC_SEAT_HOST);
    deliver(&other, OTHER, &host, BODY_SYNC_SEAT_HOST);
    TEST_ASSERT_EQUAL_UINT8(GUEST, sync_authority_body(&host, BOX)->seat);
    deliver(&host, BODY_SYNC_SEAT_HOST, &other, OTHER);
    sync_authority_body_t *o = sync_authority_body(&other, BOX);
    TEST_ASSERT_FALSE(sync_authority_local(&other, o));
    TEST_ASSERT_EQUAL_UINT8(GUEST, o->seat); /* the refusal names the winner */
}

static void test_the_host_takes_back_what_its_agent_touches(void) {
    setup_three();
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_take(&guest, BOX));
    deliver(&guest, GUEST, &host, BODY_SYNC_SEAT_HOST);
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, GUEST);
    TEST_ASSERT_EQUAL(BODY_SYNC_OK, sync_authority_reclaim(&host, BOX));
    sync_authority_body_t *h = sync_authority_body(&host, BOX);
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, h->seat);
    TEST_ASSERT_EQUAL(BODY_SYNC_HOLD_UNTIL_REST, h->hold); /* the host's now, until it rests */
    deliver(&host, BODY_SYNC_SEAT_HOST, &guest, GUEST);
    TEST_ASSERT_FALSE(sync_authority_local(&guest, sync_authority_body(&guest, BOX)));
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, sync_authority_reclaim(&host, BOX)); /* already the host's */
}

static void test_a_claim_with_an_unknown_hold_is_refused_as_format(void) {
    setup_three();
    stub_record_t r = {.type = STUB_REC_CLAIM, .id = BOX, .hold = STUB_HOLD_MAX + 1, .sequence = 1};
    uint8_t buffer[SYNC_AUTHORITY_RECORD_BYTES];
    uint32_t size = stub_wire_put_record(&r, buffer, sizeof(buffer)), stale = 0;
    char why[96];
    TEST_ASSERT_EQUAL(BODY_SYNC_FORMAT, sync_authority_receive(&host, GUEST, 1, buffer, size, &stale, why, sizeof(why)));
    TEST_ASSERT_EQUAL_UINT8(BODY_SYNC_SEAT_HOST, sync_authority_body(&host, BOX)->seat);
    /* And a caller cannot claim with the seam's own hold. */
    TEST_ASSERT_EQUAL(BODY_SYNC_INVALID, sync_authority_claim(&guest, BOX, BODY_SYNC_HOLD_UNTIL_REST));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_flight_that_meets_a_takable_body_stays_its_throwers);
    RUN_TEST(test_a_guest_takes_a_free_light_body_its_moving_agent_touches);
    RUN_TEST(test_the_host_grants_free_bodies_only);
    RUN_TEST(test_the_hosts_agents_come_first);
    RUN_TEST(test_a_body_returns_once_it_and_its_neighbours_rest_and_the_host_holds_its_pose);
    RUN_TEST(test_quiet_is_asleep_or_still_for_a_quarter_second);
    RUN_TEST(test_the_hosts_hold_outlasts_a_walks_second_touch);
    RUN_TEST(test_two_guests_touching_bodies_go_to_the_host);
    RUN_TEST(test_a_take_is_optimistic_and_granted_by_the_host_itself);
    RUN_TEST(test_a_guests_count_holds_its_waiting_takes);
    RUN_TEST(test_a_grant_needs_a_touch_a_budget_and_a_guest);
    RUN_TEST(test_the_host_bounds_a_seats_takes_per_tick);
    RUN_TEST(test_a_take_of_a_body_the_host_set_going_is_refused);
    RUN_TEST(test_two_guests_the_earliest_wins);
    RUN_TEST(test_the_host_takes_back_what_its_agent_touches);
    RUN_TEST(test_a_claim_with_an_unknown_hold_is_refused_as_format);
    return UNITY_END();
}
