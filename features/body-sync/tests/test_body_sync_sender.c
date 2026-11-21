#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sync_receiver.h"
#include "sync_sender.h"
#include "unity.h"

/* The budgeted sender against a pure-C peer: a lossy path with delayed fates and a receiver
   that keeps the newest record per body. No Box3D. */

void setUp(void) {}
void tearDown(void) {}

enum { GUEST = 1, SEATS = 2, FATE_DELAY = 6, FATES_MAX = 4096 };
static const sync_limits_t STAND = {{-40.0f, -15.0f, -40.0f}, {40.0f, 50.0f, 40.0f}, 100.0f, 200.0f};

static sync_codec_t codec;
static sync_sender_t sender;
static sync_body_input_t input[BODY_SYNC_BODIES_MAX];
static uint8_t send_to[BODY_SYNC_BODIES_MAX];
static uint8_t packet[4096];
static sync_record_t decoded[BODY_SYNC_BODIES_MAX];
static sync_receiver_t peer;
static uint32_t last_seen[BODY_SYNC_BODIES_MAX]; /* tick of the newest packet that carried the body */
static uint32_t tick_now;
static uint16_t number;
static float loss;
static uint64_t rng;
static uint32_t capacity;
static uint32_t packets_with_every_character;
static uint32_t packets;

typedef struct fate {
    uint16_t number;
    uint32_t at;
    bool arrived;
} fate_t;
static fate_t fates[FATES_MAX];
static uint32_t fate_head, fate_count;

static float uniform(void) {
    rng = rng * 6364136223846793005ull + 1442695040888963407ull;
    return (float)(rng >> 40) / 16777216.0f;
}

static void start(uint32_t budget, uint32_t cap, float lose, uint64_t seed, uint16_t first_number) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    sync_sender_init(&sender, &codec, SEATS, budget);
    sync_sender_seat(&sender, GUEST, true);
    sync_receiver_init(&peer, 60, STAND.max_speed);
    memset(input, 0, sizeof(input));
    memset(send_to, 0, sizeof(send_to));
    memset(last_seen, 0, sizeof(last_seen));
    tick_now = 0;
    number = first_number;
    loss = lose;
    rng = seed;
    capacity = cap;
    fate_head = fate_count = 0;
    packets = packets_with_every_character = 0;
}

static void body(uint16_t slot, float x, float speed, bool asleep) {
    sync_body_input_t *in = &input[slot];
    memset(in, 0, sizeof(*in));
    in->present = true;
    in->asleep = asleep;
    in->state.slot = slot;
    in->state.generation_low = 1;
    in->state.position[0] = x;
    in->state.position[1] = 0.5f;
    in->state.position[2] = (float)(slot % 50) * 0.5f - 12.0f;
    in->state.rotation[3] = 1.0f;
    in->state.velocity[0] = asleep ? 0.0f : speed;
    in->state.rare.holder_slot = BODY_SYNC_BODIES_MAX;
    send_to[slot] = 1u << GUEST;
}

static void resolve_due(void) {
    while (fate_count && fates[fate_head].at <= tick_now) {
        sync_sender_resolve(&sender, GUEST, fates[fate_head].number, fates[fate_head].arrived);
        fate_head = (fate_head + 1u) % FATES_MAX;
        fate_count--;
    }
}

/* One tick: the sender's packet, its size checked, its loss drawn, its fate due later. */
static uint32_t step(void) {
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    uint32_t size = sync_sender_write(&sender, GUEST, false, packet, capacity);
    TEST_ASSERT_TRUE(size <= capacity && size <= (sender.budget < capacity ? sender.budget : capacity));
    TEST_ASSERT_EQUAL_UINT32(0, sync_sender_write(&sender, GUEST, false, packet, capacity)); /* one a tick */
    if (size) {
        uint32_t count = 0;
        char why[96];
        TEST_ASSERT_TRUE_MESSAGE(sync_packet_read(&codec, SEATS, packet, size, decoded, BODY_SYNC_BODIES_MAX, &count, why, sizeof(why)), why);
        bool arrived = uniform() >= loss;
        uint32_t characters = 0, wanted = 0;
        for (uint32_t s = 0; s < BODY_SYNC_BODIES_MAX; s++) {
            wanted += input[s].present && input[s].character ? 1u : 0u;
        }
        for (uint32_t i = 0; i < count; i++) {
            last_seen[decoded[i].slot] = tick_now;
            characters += input[decoded[i].slot].character ? 1u : 0u;
            if (arrived) {
                sync_receiver_store(&peer, 0, tick_now, &decoded[i]);
            }
        }
        packets++;
        packets_with_every_character += characters == wanted ? 1u : 0u;
        sync_sender_sent(&sender, GUEST, number);
        TEST_ASSERT_TRUE(fate_count < FATES_MAX);
        fates[(fate_head + fate_count) % FATES_MAX] = (fate_t){number, tick_now + FATE_DELAY, arrived};
        fate_count++;
        number++;
    }
    resolve_due();
    return size;
}

/* Records of one packet at the stand's limits, the widest kind with every rare group: what the
   budget surely holds. */
static uint32_t room_in_records(uint32_t budget) {
    return (budget * 8u - 1u) / (codec.record_bits_min + codec.rare_mask.bits + codec.rare_motion.bits + codec.rare_flags.bits +
                                 codec.rare_authority.bits + codec.rare_holder.bits);
}

static uint32_t bound(uint32_t awake, uint32_t room) {
    uint32_t overflow = SYNC_AGE_MAX + (awake + room - 1u) / room;
    uint32_t floor_rate = (uint32_t)ceilf(1.0f / SYNC_RATE_MIN);
    return overflow > floor_rate ? overflow : floor_rate;
}

/* Card core 1: with far more moving bodies than a packet holds, every one is still carried
   within the bound, and the characters ride every packet. */
static void test_no_moving_body_starves_when_the_budget_overflows(void) {
    start(0, 1200, 0.0f, 11, 0);
    const uint32_t M = 300;
    for (uint16_t s = 0; s < M; s++) {
        body(s, -30.0f + 0.2f * (float)s, 0.05f * (float)(s % 60), false);
    }
    input[0].character = input[1].character = true;
    uint32_t limit = bound(M, room_in_records(SYNC_DEFAULT_BUDGET) - 2u);
    for (uint32_t t = 0; t < 600; t++) {
        step();
        if (t > limit) {
            for (uint16_t s = 0; s < M; s++) {
                TEST_ASSERT_TRUE_MESSAGE(tick_now - last_seen[s] <= limit, "a moving body starved");
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(packets, packets_with_every_character);
    TEST_ASSERT_TRUE(sender.stats[GUEST].stale_max_ticks <= limit);
    TEST_ASSERT_TRUE(sender.stats[GUEST].deferred > 0);
}

/* The age guard: a slow body among fast ones that keep winning on priority still goes out by
   the age limit, not only when its rate catches up. */
static void test_a_slow_body_goes_out_by_the_age_limit(void) {
    start(120, 1200, 0.0f, 3, 0);
    for (uint16_t s = 0; s < 80; s++) {
        body(s, -30.0f + 0.3f * (float)s, 5.0f, false);
    }
    body(200, 3.0f, 0.0f, false);
    for (uint32_t t = 0; t < 400; t++) {
        step();
        if (t > 60) {
            TEST_ASSERT_TRUE_MESSAGE(tick_now - last_seen[200] <= SYNC_AGE_MAX + 1u, "the slow body waited past the age limit");
        }
    }
}

/* Card core 2: bodies that fall asleep reach the peer at their exact rest pose through 10 %
   loss, and stop being sent once a packet carrying the rest pose is acknowledged. */
static void test_a_rest_pose_arrives_at_ten_percent_loss(void) {
    for (uint64_t seed = 1; seed <= 20; seed++) {
        start(0, 1200, 0.10f, seed, (uint16_t)(seed * 977u));
        for (uint16_t s = 0; s < 40; s++) {
            body(s, -10.0f + 0.5f * (float)s, 1.0f, false);
        }
        for (uint32_t t = 0; t < 30; t++) {
            step();
        }
        for (uint16_t s = 0; s < 40; s++) {
            input[s].state.position[0] += 0.123f;
            body(s, input[s].state.position[0], 0.0f, true);
        }
        for (uint32_t t = 0; t < 300; t++) {
            step();
        }
        for (uint16_t s = 0; s < 40; s++) {
            sync_record_t expect = input[s].state;
            expect.sim = true;
            expect.at_rest = true;
            expect.rare.groups = 0;
            TEST_ASSERT_TRUE(sync_codec_limit(&codec, &expect));
            sync_codec_quantize(&codec, &expect);
            const sync_record_t *have = &peer.bodies[s].newest;
            TEST_ASSERT_TRUE_MESSAGE(peer.bodies[s].has && have->at_rest && have->sim, "the rest pose never arrived");
            TEST_ASSERT_EQUAL_MEMORY(expect.position, have->position, sizeof(expect.position));
            TEST_ASSERT_EQUAL_MEMORY(expect.rotation, have->rotation, sizeof(expect.rotation));
            TEST_ASSERT_TRUE_MESSAGE(sender.bodies[GUEST][s].rest_acked, "an arrived rest pose was not acknowledged");
        }
        uint32_t quiet = 0;
        for (uint32_t t = 0; t < 20; t++) {
            quiet += step() == 0 ? 1u : 0u;
        }
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(20, quiet, "bodies at rest and acknowledged were sent again");
    }
}

/* Card core 3: a packet never exceeds the capacity it is given, at any capacity from the
   smallest a sender writes to the link's limit for unreliable messages, and decodes alone. */
static void test_a_packet_never_exceeds_max_unreliable(void) {
    static const uint32_t capacities[] = {64, 65, 100, 257, 600, 999, 1000, 1001, 1200, 1400};
    for (uint32_t c = 0; c < sizeof(capacities) / sizeof(capacities[0]); c++) {
        start(4000, capacities[c], 0.0f, 5 + c, 0);
        for (uint16_t s = 0; s < 500; s++) {
            body(s, -35.0f + 0.14f * (float)s, (float)(s % 7), (s % 9) == 0);
            input[s].state.rare.flags = (uint8_t)(s & 15u);
        }
        for (uint32_t t = 0; t < 120; t++) {
            uint32_t size = step(); /* step checks the size and decodes the packet alone */
            TEST_ASSERT_TRUE(size <= capacities[c]);
            if (t % 10 == 0) {
                input[t % 500].state.rare.flags ^= 1u;
            }
        }
    }
    start(0, 63, 0.0f, 1, 0);
    body(0, 0.0f, 1.0f, false);
    TEST_ASSERT_EQUAL_UINT32(0, step()); /* below the smallest capacity nothing is written */
}

/* Card edge 1: the budget overflows for ten seconds on end; the bound holds throughout, and
   rest poses and rare changes still get through. */
static void test_the_budget_overflows_for_ten_seconds(void) {
    start(0, 1200, 0.05f, 77, 0);
    uint32_t room = room_in_records(SYNC_DEFAULT_BUDGET);
    uint32_t M = room * 5u;
    for (uint16_t s = 0; s < M; s++) {
        body(s, -35.0f + 0.15f * (float)s, 3.0f + (float)(s % 5), false);
    }
    for (uint16_t s = (uint16_t)M; s < M + 20u; s++) {
        body(s, 5.0f, 0.0f, true);
    }
    uint32_t limit = bound(M, room);
    for (uint32_t t = 0; t < 600; t++) {
        step();
        if (t % 50 == 0) {
            input[t % M].state.rare.flags = (uint8_t)((input[t % M].state.rare.flags + 1u) & 15u);
        }
        if (t > limit) {
            for (uint16_t s = 0; s < M; s++) {
                TEST_ASSERT_TRUE_MESSAGE(tick_now - last_seen[s] <= limit, "a moving body starved during the overflow");
            }
        }
    }
    for (uint32_t t = 0; t < 2u * limit; t++) {
        step();
    }
    for (uint16_t s = 0; s < M + 20u; s++) {
        TEST_ASSERT_TRUE(peer.bodies[s].has);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(input[s].state.rare.flags, peer.bodies[s].rare.flags, "a rare change was lost in the overflow");
        if (s >= M) {
            TEST_ASSERT_TRUE_MESSAGE(peer.bodies[s].newest.at_rest, "a rest pose was lost in the overflow");
        }
    }
}

/* Card edge 2: a record larger than the room is never split: the body is left out and counted,
   and every packet still decodes alone. */
static void test_a_record_larger_than_the_room_is_left_out_whole(void) {
    start(15, 1200, 0.0f, 9, 0); /* 15 bytes: a copy fits, a copy with its rare groups does not */
    body(0, 1.0f, 1.0f, false);
    input[0].state.rare.flags = 3;
    uint64_t before = sender.stats[GUEST].deferred;
    for (uint32_t t = 0; t < 10; t++) {
        TEST_ASSERT_EQUAL_UINT32(0, step());
    }
    TEST_ASSERT_TRUE(sender.stats[GUEST].deferred > before);
    TEST_ASSERT_FALSE(peer.bodies[0].has);
    start(BODY_SYNC_RECORD_MAX, 1200, 0.0f, 9, 0);
    body(0, 1.0f, 1.0f, false);
    for (uint32_t t = 0; t < 10; t++) {
        step();
    }
    TEST_ASSERT_TRUE(peer.bodies[0].has);
}

/* Card edge 3: the 16-bit packet number wraps; acknowledgements keep working across it, and a
   number not in the window acknowledges nothing. */
static void test_the_packet_number_wraps(void) {
    start(0, 1200, 0.0f, 21, 65000);
    for (uint16_t s = 0; s < 10; s++) {
        body(s, (float)s, 2.0f, false);
    }
    body(20, 3.0f, 0.0f, true);
    for (uint32_t t = 0; t < 70000; t++) {
        step();
        uint32_t which = (t / 997u) % 10u;
        if (t % 997u == 0) {
            input[which].state.rare.flags = (uint8_t)((input[which].state.rare.flags + 1u) & 15u);
        }
        if (t % 997u == 3u * FATE_DELAY) {
            const sync_seat_body_t *b = &sender.bodies[GUEST][which];
            for (int g = 0; g < SYNC_RARE_GROUPS; g++) {
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(b->version[g], b->acked_version[g], "a rare change was not acknowledged");
            }
        }
    }
    TEST_ASSERT_TRUE(number < 65000u); /* it wrapped */
    TEST_ASSERT_TRUE(sender.bodies[GUEST][20].rest_acked);
    /* A rest pose acknowledged only by a number never sent stays unacknowledged. */
    body(20, 4.0f, 0.0f, false);
    step();
    body(20, 4.5f, 0.0f, true);
    for (uint16_t k = 0; k < 2; k++) { /* a rest pose goes out every other tick */
        tick_now++;
        sync_sender_tick(&sender, tick_now, input, send_to);
        TEST_ASSERT_TRUE(sync_sender_write(&sender, GUEST, false, packet, capacity) > 0);
        sync_sender_sent(&sender, GUEST, (uint16_t)(number + 1000u + k));
    }
    for (uint16_t k = 0; k < 2; k++) {
        sync_sender_resolve(&sender, GUEST, (uint16_t)(number + 1000u + k + 64u), true);
        sync_sender_resolve(&sender, GUEST, (uint16_t)(number + 1000u + k + 65536u - 64u), true);
    }
    TEST_ASSERT_FALSE_MESSAGE(sender.bodies[GUEST][20].rest_acked, "a number not in the window acknowledged a rest pose");
    for (uint16_t k = 0; k < 2; k++) {
        sync_sender_resolve(&sender, GUEST, (uint16_t)(number + 1000u + k), true);
    }
    TEST_ASSERT_TRUE(sender.bodies[GUEST][20].rest_acked);
}

/* A packet the caller never handed to the session acknowledges nothing, whatever arrives later. */
static void test_an_unsent_packet_acknowledges_nothing(void) {
    start(0, 1200, 0.0f, 4, 100);
    body(5, 1.0f, 0.0f, true);
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    TEST_ASSERT_TRUE(sync_sender_write(&sender, GUEST, false, packet, capacity) > 0); /* never sent */
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    sync_sender_write(&sender, GUEST, false, packet, capacity);
    sync_sender_sent(&sender, GUEST, 101);
    sync_sender_resolve(&sender, GUEST, 100, true);
    sync_sender_resolve(&sender, GUEST, 0, true);
    TEST_ASSERT_FALSE(sender.bodies[GUEST][5].rest_acked);
    uint16_t pending[SYNC_SENT_WINDOW];
    TEST_ASSERT_EQUAL_UINT32(1, sync_sender_unresolved(&sender, GUEST, pending, SYNC_SENT_WINDOW));
    TEST_ASSERT_EQUAL_UINT16(101, pending[0]);
}

/* A rare group is acknowledged only by a packet that carried its current version: an older
   carrier arriving after the value changed again does not end the resend. */
static void test_a_rare_change_is_sent_until_its_own_version_arrives(void) {
    start(0, 1200, 0.0f, 8, 0);
    body(7, 1.0f, 0.0f, false);
    for (uint32_t t = 0; t < 20; t++) {
        step();
    }
    input[7].state.rare.flags = 9;
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    TEST_ASSERT_TRUE(sync_sender_write(&sender, GUEST, false, packet, capacity) > 0);
    sync_sender_sent(&sender, GUEST, 500); /* carries flags 9 */
    input[7].state.rare.flags = 10;
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    TEST_ASSERT_TRUE(sync_sender_write(&sender, GUEST, false, packet, capacity) > 0);
    sync_sender_sent(&sender, GUEST, 501); /* carries flags 10 */
    sync_sender_resolve(&sender, GUEST, 500, true);
    sync_sender_resolve(&sender, GUEST, 501, false);
    const sync_seat_body_t *b = &sender.bodies[GUEST][7];
    TEST_ASSERT_TRUE_MESSAGE(b->version[1] != b->acked_version[1], "an older version acknowledged the newer flags");
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    uint32_t size = sync_sender_write(&sender, GUEST, false, packet, capacity), count = 0;
    TEST_ASSERT_TRUE(size > 0);
    TEST_ASSERT_TRUE(sync_packet_read(&codec, SEATS, packet, size, decoded, BODY_SYNC_BODIES_MAX, &count, NULL, 0));
    TEST_ASSERT_TRUE(count == 1 && (decoded[0].rare.groups & SYNC_RARE_FLAGS) && decoded[0].rare.flags == 10);
}

/* A lost packet acknowledges no rest pose: the rest goes out again. */
static void test_a_lost_packet_acknowledges_no_rest_pose(void) {
    start(0, 1200, 0.0f, 6, 0);
    body(3, 1.0f, 0.0f, true);
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    TEST_ASSERT_TRUE(sync_sender_write(&sender, GUEST, false, packet, capacity) > 0);
    sync_sender_sent(&sender, GUEST, 40);
    sync_sender_resolve(&sender, GUEST, 40, false);
    TEST_ASSERT_FALSE(sender.bodies[GUEST][3].rest_acked);
    uint32_t sent = 0;
    for (uint32_t t = 0; t < 4; t++) {
        tick_now++;
        sync_sender_tick(&sender, tick_now, input, send_to);
        sent += sync_sender_write(&sender, GUEST, false, packet, capacity) > 0 ? 1u : 0u;
        sync_sender_sent(&sender, GUEST, (uint16_t)(41u + t));
    }
    TEST_ASSERT_TRUE(sent >= 1);
}

/* A body that sleeps again on the pose already acknowledged is not sent again: "changed" is
   on the wire, not in the float. */
static void test_a_rest_on_the_acknowledged_pose_is_not_resent(void) {
    start(0, 1200, 0.0f, 2, 0);
    body(9, 2.0f, 0.0f, true);
    for (uint32_t t = 0; t < 20; t++) {
        step();
    }
    TEST_ASSERT_TRUE(sender.bodies[GUEST][9].rest_acked);
    body(9, 2.0f, 0.5f, false);
    step();
    body(9, 2.0f + SYNC_SIM_POSITION_STEP * 0.1f, 0.0f, true);
    uint32_t before = last_seen[9];
    for (uint32_t t = 0; t < 10; t++) {
        step();
    }
    TEST_ASSERT_EQUAL_UINT32(before, last_seen[9]);
}

/* The receiver: newest wins per sender, a new sender starts over, and a copy's velocity is
   estimated from its last two poses. */
static void test_the_receiver_keeps_the_newest_and_estimates_speed(void) {
    sync_receiver_init(&peer, 60, 100.0f);
    sync_record_t r;
    memset(&r, 0, sizeof(r));
    r.slot = 4;
    r.rotation[3] = 1.0f;
    TEST_ASSERT_TRUE(sync_receiver_store(&peer, 0, 10, &r));
    r.position[0] = 0.1f;
    TEST_ASSERT_TRUE(sync_receiver_store(&peer, 0, 12, &r));
    TEST_ASSERT_TRUE(fabsf(peer.bodies[4].velocity[0] - 3.0f) < 1e-3f);
    TEST_ASSERT_FALSE(sync_receiver_store(&peer, 0, 11, &r));
    TEST_ASSERT_FALSE(sync_receiver_store(&peer, 0, 12, &r));
    TEST_ASSERT_TRUE(sync_receiver_store(&peer, 2, 5, &r)); /* another sender's clock */
    float now_p[3] = {0.1f, 0.0f, 0.0f}, now_q[4] = {0, 0, 0, 1}, tp[3], tq[4];
    TEST_ASSERT_EQUAL(SYNC_COPY_PLACE, sync_receiver_decide(&peer, 4, now_p, now_q, false, tp, tq));
    TEST_ASSERT_EQUAL(SYNC_COPY_HOLD, sync_receiver_decide(&peer, 4, now_p, now_q, true, tp, tq)); /* consumed */
    r.position[0] = 0.3f;
    TEST_ASSERT_TRUE(sync_receiver_store(&peer, 2, 6, &r));
    TEST_ASSERT_EQUAL(SYNC_COPY_DRIVE, sync_receiver_decide(&peer, 4, now_p, now_q, true, tp, tq));
    r.position[0] = 9.0f;
    TEST_ASSERT_TRUE(sync_receiver_store(&peer, 2, 7, &r));
    TEST_ASSERT_EQUAL(SYNC_COPY_PLACE, sync_receiver_decide(&peer, 4, now_p, now_q, true, tp, tq));
}

/* An acknowledgement counts for the sleep it was sent for: a packet of an old sleep arriving
   after the body slept again elsewhere acknowledges nothing. */
static void test_a_rest_ack_counts_for_its_own_sleep(void) {
    start(0, 1200, 0.0f, 12, 0);
    body(4, 1.0f, 0.0f, true);
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    TEST_ASSERT_TRUE(sync_sender_write(&sender, GUEST, false, packet, capacity) > 0);
    sync_sender_sent(&sender, GUEST, 70); /* carries the first rest */
    body(4, 1.0f, 2.0f, false);
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    body(4, 6.0f, 0.0f, true);
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    sync_sender_resolve(&sender, GUEST, 70, true);
    TEST_ASSERT_FALSE_MESSAGE(sender.bodies[GUEST][4].rest_acked, "an old sleep's packet acknowledged the new rest pose");
}

/* Aged bodies go oldest first. */
static void test_aged_bodies_go_oldest_first(void) {
    start(20, 1200, 0.0f, 13, 0); /* room for one copy record */
    body(1, 1.0f, 0.0f, false);
    body(2, 2.0f, 0.0f, false);
    sender.bodies[GUEST][1].eligible = sender.bodies[GUEST][2].eligible = true;
    for (int t = 0; t < 3; t++) {
        step();
    }
    sender.bodies[GUEST][1].last_sent_tick = tick_now - 40;
    sender.bodies[GUEST][2].last_sent_tick = tick_now - 60;
    for (int g = 0; g < SYNC_RARE_GROUPS; g++) {
        sender.bodies[GUEST][1].acked_version[g] = sender.bodies[GUEST][1].version[g];
        sender.bodies[GUEST][2].acked_version[g] = sender.bodies[GUEST][2].version[g];
    }
    uint32_t before1 = last_seen[1], before2 = last_seen[2];
    step();
    TEST_ASSERT_TRUE_MESSAGE(last_seen[2] != before2 && last_seen[1] == before1, "the newer aged body went first");
}

/* A slot bound again starts from versions no packet still in the window carried. */
static void test_a_rebound_slot_is_not_acknowledged_by_its_old_body(void) {
    start(0, 1200, 0.0f, 14, 0);
    body(5, 1.0f, 1.0f, false);
    input[5].state.rare.flags = 3;
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    TEST_ASSERT_TRUE(sync_sender_write(&sender, GUEST, false, packet, capacity) > 0);
    sync_sender_sent(&sender, GUEST, 90);
    sync_sender_forget(&sender, 5);
    tick_now++;
    sync_sender_tick(&sender, tick_now, input, send_to);
    sync_sender_resolve(&sender, GUEST, 90, true);
    const sync_seat_body_t *b = &sender.bodies[GUEST][5];
    for (int g = 0; g < SYNC_RARE_GROUPS; g++) {
        TEST_ASSERT_TRUE_MESSAGE(b->version[g] != b->acked_version[g], "the old body's packet acknowledged the new body");
    }
}

/* What slow and sleeping bodies cost: a still body goes out about once in four ticks, a
   sleeping one every other tick until acknowledged and then never; without fates sleeping
   bodies stay due. */
static void test_slow_and_sleeping_bodies_cost_what_the_rates_say(void) {
    start(0, 1200, 0.0f, 15, 0);
    body(1, 0.0f, 0.0f, false);
    uint32_t sent = 0;
    for (int t = 0; t < 200; t++) {
        uint32_t before = last_seen[1];
        step();
        sent += last_seen[1] != before ? 1u : 0u;
    }
    TEST_ASSERT_TRUE_MESSAGE(sent >= 45 && sent <= 55, "a still body is not sent once in four ticks");
    start(0, 1200, 0.0f, 16, 0);
    for (uint16_t s = 0; s < 200; s++) {
        body(s, -20.0f + 0.2f * (float)s, 0.0f, true);
    }
    uint32_t bytes = 0;
    for (int t = 0; t < 60; t++) {
        uint32_t size = step();
        bytes += t >= 40 ? size : 0u;
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, bytes, "acknowledged sleeping bodies still cost bytes");
    /* without fates */
    start(0, 1200, 0.0f, 17, 0);
    for (uint16_t s = 0; s < 200; s++) {
        body(s, -20.0f + 0.2f * (float)s, 0.0f, true);
    }
    bytes = 0;
    for (int t = 0; t < 60; t++) {
        tick_now++;
        sync_sender_tick(&sender, tick_now, input, send_to);
        uint32_t size = sync_sender_write(&sender, GUEST, false, packet, capacity);
        sync_sender_sent(&sender, GUEST, (uint16_t)t);
        bytes += t >= 40 ? size : 0u;
    }
    TEST_ASSERT_TRUE_MESSAGE(bytes > 20u * 500u, "sleeping bodies without fates went quiet");
}

/* A wake without movement: the peer last saw a moving record, so the rest pose goes out again. */
static void test_a_rest_after_a_moving_record_is_sent_again(void) {
    start(0, 1200, 0.0f, 18, 0);
    body(9, 2.0f, 0.0f, true);
    for (int t = 0; t < 20; t++) {
        step();
    }
    TEST_ASSERT_TRUE(sender.bodies[GUEST][9].rest_acked);
    body(9, 2.0f, 3.0f, false); /* a hit: it goes out moving */
    step();
    body(9, 2.0f, 0.0f, true);  /* asleep where it was */
    for (int t = 0; t < 20; t++) {
        step();
    }
    TEST_ASSERT_TRUE_MESSAGE(peer.bodies[9].newest.at_rest && peer.bodies[9].newest.sim, "the peer kept the moving record as the rest");
}

/* A packet never handed to the session resets nothing: the bodies stay due and age. */
static void test_an_unsent_packet_resets_no_priority(void) {
    start(0, 1200, 0.0f, 19, 0);
    body(3, 1.0f, 0.0f, false);
    for (int t = 0; t < 40; t++) {
        tick_now++;
        sync_sender_tick(&sender, tick_now, input, send_to);
        sync_sender_write(&sender, GUEST, false, packet, capacity);
    }
    TEST_ASSERT_TRUE_MESSAGE(sender.stats[GUEST].stale_max_ticks >= 39, "an unsent packet counted as sent");
    TEST_ASSERT_TRUE(sender.bodies[GUEST][3].acc >= 1.0f);
}

/* A budget without room after the characters is refused. */
static void test_a_budget_too_small_is_refused(void) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    uint32_t need = ((uint32_t)(SEATS + 1) * codec.record_bits_max + 1u + 7u) / 8u;
    TEST_ASSERT_TRUE(sync_sender_budget_ok(&codec, SEATS, need));
    TEST_ASSERT_FALSE(sync_sender_budget_ok(&codec, SEATS, need - 1u));
    TEST_ASSERT_TRUE(sync_sender_budget_ok(&codec, 4, 0)); /* the default */
}

/* The receiver keeps each rare group until a newer value of that group arrives. */
static void test_rare_groups_are_sticky(void) {
    sync_receiver_init(&peer, 60, 100.0f);
    sync_record_t r;
    memset(&r, 0, sizeof(r));
    r.slot = 2;
    r.rotation[3] = 1.0f;
    r.rare.groups = SYNC_RARE_FLAGS | SYNC_RARE_MOTION;
    r.rare.flags = 7;
    r.rare.locks = 5;
    TEST_ASSERT_TRUE(sync_receiver_store(&peer, 0, 1, &r));
    r.rare.groups = 0;
    r.rare.flags = 0;
    r.rare.locks = 0;
    TEST_ASSERT_TRUE(sync_receiver_store(&peer, 0, 2, &r));
    TEST_ASSERT_EQUAL_UINT8(7, peer.bodies[2].rare.flags);
    TEST_ASSERT_EQUAL_UINT8(5, peer.bodies[2].rare.locks);
    TEST_ASSERT_TRUE(peer.bodies[2].known & SYNC_RARE_FLAGS);
}

/* The host's intake: tick order first, so a late packet neither counts nor moves the number
   back; poses count under the number declared. */
static void test_the_host_intake_orders_before_it_declares(void) {
    sync_receiver_init(&peer, 60, 100.0f);
    sync_record_t r;
    memset(&r, 0, sizeof(r));
    r.slot = 8;
    r.rotation[3] = 1.0f;
    TEST_ASSERT_EQUAL(SYNC_INTAKE_REJECTED, sync_receiver_host_intake(&peer, 1, 10, &r, 1, 3)); /* nothing declared */
    r.rare.groups = SYNC_RARE_AUTHORITY;
    r.rare.number = 3;
    TEST_ASSERT_EQUAL(SYNC_INTAKE_TAKE, sync_receiver_host_intake(&peer, 1, 11, &r, 1, 3));
    r.rare.number = 2;
    TEST_ASSERT_EQUAL(SYNC_INTAKE_STALE, sync_receiver_host_intake(&peer, 1, 9, &r, 1, 3)); /* late, an old number */
    r.rare.groups = 0;
    TEST_ASSERT_EQUAL(SYNC_INTAKE_TAKE, sync_receiver_host_intake(&peer, 1, 12, &r, 1, 3));
    TEST_ASSERT_EQUAL(SYNC_INTAKE_REJECTED, sync_receiver_host_intake(&peer, 1, 13, &r, 2, 3)); /* not that seat's */
    TEST_ASSERT_EQUAL(SYNC_INTAKE_REJECTED, sync_receiver_host_intake(&peer, 1, 14, &r, 1, 4)); /* a newer number */
}

/* A hit goes out at once: a body whose speed jumped is sent the next tick, before its rate
   would have brought it due. */
static void test_a_hit_goes_out_at_once(void) {
    start(0, 1200, 0.0f, 20, 0);
    body(6, 1.0f, 0.5f, false);
    uint32_t sent_at = 0;
    for (int t = 0; t < 40 && !sent_at; t++) {
        uint32_t before = last_seen[6];
        step();
        if (t > 10 && last_seen[6] != before) {
            sent_at = tick_now;
        }
    }
    TEST_ASSERT_TRUE(sent_at > 0);
    input[6].state.velocity[0] = 1.7f; /* +1.2 m/s in one tick */
    step();
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(tick_now, last_seen[6], "a hit waited for its rate");
}

/* Resending a rest pose is no staleness: only moving bodies count. */
static void test_rest_resends_leave_the_staleness_alone(void) {
    start(0, 1200, 0.0f, 21, 0);
    body(2, 1.0f, 0.0f, true);
    for (int t = 0; t < 40; t++) { /* no fates: the rest goes out every other tick */
        tick_now++;
        sync_sender_tick(&sender, tick_now, input, send_to);
        sync_sender_write(&sender, GUEST, false, packet, capacity);
        sync_sender_sent(&sender, GUEST, (uint16_t)t);
    }
    TEST_ASSERT_EQUAL_UINT32(0, sender.stats[GUEST].stale_max_ticks);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_moving_body_starves_when_the_budget_overflows);
    RUN_TEST(test_a_slow_body_goes_out_by_the_age_limit);
    RUN_TEST(test_a_rest_pose_arrives_at_ten_percent_loss);
    RUN_TEST(test_a_packet_never_exceeds_max_unreliable);
    RUN_TEST(test_the_budget_overflows_for_ten_seconds);
    RUN_TEST(test_a_record_larger_than_the_room_is_left_out_whole);
    RUN_TEST(test_the_packet_number_wraps);
    RUN_TEST(test_an_unsent_packet_acknowledges_nothing);
    RUN_TEST(test_a_rare_change_is_sent_until_its_own_version_arrives);
    RUN_TEST(test_a_lost_packet_acknowledges_no_rest_pose);
    RUN_TEST(test_a_rest_on_the_acknowledged_pose_is_not_resent);
    RUN_TEST(test_the_receiver_keeps_the_newest_and_estimates_speed);
    RUN_TEST(test_a_rest_ack_counts_for_its_own_sleep);
    RUN_TEST(test_aged_bodies_go_oldest_first);
    RUN_TEST(test_a_rebound_slot_is_not_acknowledged_by_its_old_body);
    RUN_TEST(test_slow_and_sleeping_bodies_cost_what_the_rates_say);
    RUN_TEST(test_a_rest_after_a_moving_record_is_sent_again);
    RUN_TEST(test_an_unsent_packet_resets_no_priority);
    RUN_TEST(test_a_budget_too_small_is_refused);
    RUN_TEST(test_rare_groups_are_sticky);
    RUN_TEST(test_the_host_intake_orders_before_it_declares);
    RUN_TEST(test_a_hit_goes_out_at_once);
    RUN_TEST(test_rest_resends_leave_the_staleness_alone);
    return UNITY_END();
}
