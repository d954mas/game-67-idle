#include "coop_test_world.h"
#include "coop_wire.h"
#include "unity.h"

#include <string.h>

/* A peer without a session throws garbage, cut packets and well-formed packets that break the
   rules at a host session and at a guest session, on every lane. The session survives, closes
   the offender with FORMAT, and the seats that behave keep working. */

void setUp(void) {}
void tearDown(void) {}

static coop_world_t world;

enum { BUILD = 5, SETTLE_STEPS = 30 };

typedef struct packet {
    uint8_t data[128];
    uint32_t size;
    uint8_t lane;
    bool reliable;
} packet_t;

static packet_t wire(coop_wire_msg_t msg) {
    packet_t packet = {0};
    packet.size = coop_wire_write(&msg, packet.data, sizeof(packet.data));
    TEST_ASSERT_TRUE(packet.size > 0);
    TEST_ASSERT_TRUE(coop_wire_route(msg.kind, &packet.lane, &packet.reliable));
    return packet;
}

static packet_t on_lane(packet_t packet, uint8_t lane, bool reliable) {
    packet.lane = lane;
    packet.reliable = reliable;
    return packet;
}

static packet_t cut(packet_t packet, uint32_t size) {
    packet.size = size;
    return packet;
}

static packet_t garbage(uint64_t *seed) {
    packet_t packet = {0};
    packet.size = 1 + coop_test_rand(seed) % 80;
    for (uint32_t i = 0; i < packet.size; i++) {
        packet.data[i] = (uint8_t)coop_test_rand(seed);
    }
    if (coop_test_rand(seed) & 1U) {
        packet.data[0] = (uint8_t)((packet.data[0] & 0xF0U) | (1U + coop_test_rand(seed) % 8U));
    }
    packet.lane = (uint8_t)(coop_test_rand(seed) % COOP_LANE_COUNT);
    packet.reliable = (coop_test_rand(seed) & 1U) != 0;
    return packet;
}

static void send_packet(net_link_t *link, net_link_peer_t peer, const packet_t *packet) {
    net_link_send_result_t result = net_link_send(link, peer, packet->lane, packet->reliable ? NET_LINK_SEND_RELIABLE : NET_LINK_SEND_UNRELIABLE, packet->data, packet->size);
    TEST_ASSERT_EQUAL(NET_LINK_SEND_OK, result);
}

static const uint8_t PAYLOAD[6] = {1, 2, 3, 4, 5, 6};

static coop_wire_msg_t record_msg(void) { return (coop_wire_msg_t){.kind = COOP_WIRE_RECORD, .tick = 3, .bytes = PAYLOAD, .size = sizeof(PAYLOAD)}; }

static coop_wire_msg_t state_msg(void) { return (coop_wire_msg_t){.kind = COOP_WIRE_STATE, .number = 1, .tick = 3, .bytes = PAYLOAD, .size = sizeof(PAYLOAD)}; }

// #region a host under attack

typedef struct legit_run {
    uint32_t host_got[COOP_SEATS_MAX];
    uint32_t guest_got[2];
    uint32_t legit_left;
    uint32_t attacker_left_format;
} legit_run_t;

static void legit_tick(void *context, coop_world_t *w, int side) {
    uint8_t byte = 1;
    if (side == COOP_WORLD_HOST) {
        for (uint8_t seat = 1; seat <= 2; seat++) {
            TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_record(w->host, seat, &byte, 1));
            coop_session_send_state(w->host, seat, &byte, 1, NULL);
        }
    } else if (side < 2) {
        TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_record(w->guest[side], COOP_SEAT_HOST, &byte, 1));
    }
    (void)context;
}

static void legit_event(void *context, coop_world_t *w, int side, const coop_event_t *event) {
    legit_run_t *run = context;
    (void)w;
    if (side == COOP_WORLD_HOST) {
        if (event->type == COOP_EVENT_RECORD) {
            run->host_got[event->seat]++;
        }
        if (event->type == COOP_EVENT_SEAT_LEFT) {
            if (event->seat <= 2) {
                run->legit_left++;
            } else if (event->reason == COOP_CLOSE_FORMAT) {
                run->attacker_left_format++;
            }
        }
        return;
    }
    if (side < 2 && event->type == COOP_EVENT_RECORD) {
        run->guest_got[side]++;
    }
    TEST_ASSERT_TRUE_MESSAGE(event->type != COOP_EVENT_CLOSED, "a guest that behaved was closed");
}

static uint32_t attacks;
static uint32_t format_closes;

/* One attacker connects, optionally takes a seat, sends `count` packets and is watched. */
static void attack_host(const packet_t *packets, int count, bool seat_first, bool may_survive) {
    attacks++;
    raw_end_t raw = {.link = net_link_sim_guest(world.sim, 900 + attacks)};
    TEST_ASSERT_NOT_NULL(raw.link);
    for (int step = 0; step < SETTLE_STEPS && !raw.connected; step++) {
        coop_world_step(&world);
        raw_poll(&raw);
    }
    TEST_ASSERT_TRUE(raw.connected);
    if (seat_first) {
        packet_t hello = wire((coop_wire_msg_t){.kind = COOP_WIRE_HELLO, .protocol = COOP_SESSION_PROTOCOL, .build_key = BUILD, .player_key = attacks});
        send_packet(raw.link, raw.peer, &hello);
        for (int step = 0; step < SETTLE_STEPS && raw.messages == 0; step++) {
            coop_world_step(&world);
            raw_poll(&raw);
        }
        coop_wire_msg_t welcome;
        TEST_ASSERT_TRUE(coop_wire_parse(raw.first, raw.first_size, raw.first_lane, true, true, &welcome));
        TEST_ASSERT_EQUAL_INT(COOP_WIRE_WELCOME, welcome.kind);
        TEST_ASSERT_EQUAL_UINT8(3, welcome.seat);
    }
    for (int i = 0; i < count; i++) {
        send_packet(raw.link, raw.peer, &packets[i]);
    }
    for (int step = 0; step < SETTLE_STEPS && !raw.disconnected; step++) {
        coop_world_step(&world);
        raw_poll(&raw);
    }
    if (raw.disconnected) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(COOP_CLOSE_FORMAT, raw.reason, "closed for another reason");
        format_closes++;
    } else {
        TEST_ASSERT_TRUE_MESSAGE(may_survive, "a broken packet was let through");
        net_link_close(raw.link, raw.peer, COOP_CLOSE_LEFT);
    }
    net_link_destroy(raw.link);
    for (int step = 0; step < 5; step++) {
        coop_world_step(&world);
    }
}

static void test_a_host_closes_every_offender_and_keeps_the_others(void) {
    static legit_run_t run;
    memset(&run, 0, sizeof(run));
    attacks = 0;
    format_closes = 0;
    coop_world_open(&world, 81, 6, NULL);
    coop_side_config_t host = {.seats = 4, .build_key = BUILD, .baseline_max = 1000};
    coop_world_host(&world, &host);
    for (int guest = 0; guest < 2; guest++) {
        coop_side_config_t side = {.identity = 801 + (uint64_t)guest, .player_key = 1, .build_key = BUILD};
        coop_world_join(&world, guest, &side);
        coop_world_run(&world, 0.2);
    }
    world.context = &run;
    world.on_tick = legit_tick;
    world.on_event = legit_event;
    coop_world_run(&world, 0.2);

    packet_t hello = wire((coop_wire_msg_t){.kind = COOP_WIRE_HELLO, .protocol = COOP_SESSION_PROTOCOL, .build_key = BUILD});
    packet_t record = wire(record_msg());
    packet_t state = wire(state_msg());
    packet_t from_host[] = {
        wire((coop_wire_msg_t){.kind = COOP_WIRE_WELCOME, .seat = 1, .seats = 2, .tick_hz = 60}),
        wire((coop_wire_msg_t){.kind = COOP_WIRE_JOINED, .seat = 2}),
        wire((coop_wire_msg_t){.kind = COOP_WIRE_LEFT, .seat = 2}),
        wire((coop_wire_msg_t){.kind = COOP_WIRE_BASELINE_FIRST, .total = 4, .bytes = PAYLOAD, .size = 4}),
        wire((coop_wire_msg_t){.kind = COOP_WIRE_BASELINE_PIECE, .bytes = PAYLOAD, .size = 4}),
    };
    /* Before the handshake: anything but a valid hello. */
    packet_t early[] = {record, state, on_lane(hello, COOP_LANE_STATE, false), on_lane(hello, COOP_LANE_BASELINE, true), on_lane(hello, COOP_LANE_RECORDS, false)};
    for (size_t i = 0; i < sizeof(early) / sizeof(early[0]); i++) {
        attack_host(&early[i], 1, false, false);
    }
    for (size_t i = 0; i < sizeof(from_host) / sizeof(from_host[0]); i++) {
        attack_host(&from_host[i], 1, false, false);
        attack_host(&from_host[i], 1, true, false);
    }
    for (uint32_t size = 1; size < hello.size; size++) {
        attack_host((packet_t[]){cut(hello, size)}, 1, false, false);
    }
    packet_t bad_magic = hello;
    bad_magic.data[2] ^= 0x10;
    attack_host(&bad_magic, 1, false, false);
    /* Seated: rules a well-formed packet still breaks. */
    packet_t seated[] = {
        hello,
        on_lane(record, COOP_LANE_STATE, false),
        on_lane(record, COOP_LANE_BASELINE, true),
        on_lane(record, COOP_LANE_RECORDS, false),
        on_lane(state, COOP_LANE_STATE, true),
        on_lane(state, COOP_LANE_RECORDS, true),
    };
    for (size_t i = 0; i < sizeof(seated) / sizeof(seated[0]); i++) {
        attack_host(&seated[i], 1, true, false);
    }
    for (uint32_t size = 1; size <= COOP_WIRE_RECORD_BYTES; size++) {
        attack_host((packet_t[]){cut(record, size)}, 1, true, false);
    }
    for (uint32_t size = 1; size < COOP_WIRE_STATE_BYTES; size++) {
        attack_host((packet_t[]){cut(state, size)}, 1, true, false);
    }
    packet_t padded = record;
    padded.data[COOP_WIRE_RECORD_BYTES - 1] |= 0x80;
    attack_host(&padded, 1, true, false);
    /* Garbage before and after the handshake. Seated garbage may happen to be a valid record
       or state packet; nothing else may pass. */
    uint64_t seed = 0xBADC0DE;
    for (int i = 0; i < 40; i++) {
        packet_t junk = garbage(&seed);
        attack_host(&junk, 1, false, false);
    }
    for (int i = 0; i < 60; i++) {
        packet_t junk[3] = {garbage(&seed), garbage(&seed), garbage(&seed)};
        attack_host(junk, 3, true, true);
    }
    coop_world_run(&world, 0.5);
    TEST_ASSERT_TRUE(format_closes > attacks * 3 / 4);
    TEST_ASSERT_EQUAL_UINT32(0, run.legit_left);
    TEST_ASSERT_TRUE(run.attacker_left_format > 0);
    /* The seats that behaved never stopped: roughly one record a step each way. */
    uint32_t steps = (uint32_t)(net_link_sim_time(world.sim) * 60.0) - 60;
    for (int guest = 0; guest < 2; guest++) {
        TEST_ASSERT_TRUE(run.host_got[1 + guest] > steps * 9 / 10);
        TEST_ASSERT_TRUE(run.guest_got[guest] > steps * 9 / 10);
        TEST_ASSERT_TRUE(coop_session_seated(world.host, (uint8_t)(1 + guest)));
    }
    coop_world_close(&world);
}

// #endregion

// #region a guest under attack

static raw_end_t host_end;

static void settle_guest(int steps, bool until_closed) {
    for (int step = 0; step < steps; step++) {
        if (until_closed && coop_log_count(&world.guest_log[0], COOP_EVENT_CLOSED) > 0) {
            return;
        }
        coop_world_step(&world);
        raw_poll(&host_end);
    }
}

static void raw_host_welcome(void) {
    packet_t welcome = wire((coop_wire_msg_t){.kind = COOP_WIRE_WELCOME, .seat = 1, .seats = 4, .tick = 500, .tick_hz = 60});
    packet_t host_seat = wire((coop_wire_msg_t){.kind = COOP_WIRE_JOINED, .seat = 0});
    send_packet(host_end.link, host_end.peer, &welcome);
    send_packet(host_end.link, host_end.peer, &host_seat);
}

/* One fresh guest session greets the raw host, is optionally welcomed, gets `count` packets. */
static uint32_t attack_guest(const packet_t *packets, int count, bool welcome_first, bool may_survive) {
    attacks++;
    host_end.connected = false;
    host_end.disconnected = false;
    host_end.messages = 0;
    coop_side_config_t side = {.identity = 950 + attacks, .player_key = 1, .build_key = BUILD, .baseline_max = 64};
    coop_world_join(&world, 0, &side);
    for (int step = 0; step < SETTLE_STEPS && host_end.messages == 0; step++) {
        coop_world_step(&world);
        raw_poll(&host_end);
    }
    coop_wire_msg_t hello;
    TEST_ASSERT_TRUE(coop_wire_parse(host_end.first, host_end.first_size, host_end.first_lane, true, false, &hello));
    TEST_ASSERT_EQUAL_INT(COOP_WIRE_HELLO, hello.kind);
    if (welcome_first) {
        raw_host_welcome();
        settle_guest(5, false);
        TEST_ASSERT_EQUAL_UINT32(1, coop_log_count(&world.guest_log[0], COOP_EVENT_WELCOME));
    }
    for (int i = 0; i < count; i++) {
        send_packet(host_end.link, host_end.peer, &packets[i]);
    }
    settle_guest(SETTLE_STEPS, true);
    uint32_t closed = coop_log_count(&world.guest_log[0], COOP_EVENT_CLOSED);
    if (closed > 0) {
        TEST_ASSERT_EQUAL_UINT32(1, closed);
        const coop_logged_t *event = coop_log_find(&world.guest_log[0], COOP_EVENT_CLOSED, COOP_SEAT_NONE);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(COOP_CLOSE_FORMAT, event->event.reason, "closed for another reason");
        settle_guest(5, false);
        TEST_ASSERT_TRUE(host_end.disconnected);
        TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_FORMAT, host_end.reason);
        format_closes++;
    } else {
        TEST_ASSERT_TRUE_MESSAGE(may_survive, "a broken packet was let through");
    }
    coop_world_drop(&world, 0);
    settle_guest(5, false);
    return closed;
}

static void test_a_guest_closes_a_host_that_breaks_the_protocol(void) {
    attacks = 0;
    format_closes = 0;
    coop_world_open(&world, 82, 4, NULL);
    host_end = (raw_end_t){.link = net_link_sim_host(world.sim, 100, 4)};
    TEST_ASSERT_NOT_NULL(host_end.link);
    packet_t record = wire(record_msg());
    packet_t state = wire(state_msg());
    packet_t welcome = wire((coop_wire_msg_t){.kind = COOP_WIRE_WELCOME, .seat = 1, .seats = 4, .tick = 500, .tick_hz = 60});
    packet_t hello = wire((coop_wire_msg_t){.kind = COOP_WIRE_HELLO, .protocol = COOP_SESSION_PROTOCOL, .build_key = BUILD});
    /* Before the welcome: lane 0 must open with it. */
    packet_t early[] = {record, wire((coop_wire_msg_t){.kind = COOP_WIRE_JOINED, .seat = 0}), wire((coop_wire_msg_t){.kind = COOP_WIRE_LEFT, .seat = 2}), hello};
    for (size_t i = 0; i < sizeof(early) / sizeof(early[0]); i++) {
        attack_guest(&early[i], 1, false, false);
    }
    for (uint32_t size = 1; size < welcome.size; size++) {
        attack_guest((packet_t[]){cut(welcome, size)}, 1, false, false);
    }
    /* After it. */
    packet_t first_too_big = wire((coop_wire_msg_t){.kind = COOP_WIRE_BASELINE_FIRST, .total = 65, .bytes = PAYLOAD, .size = 4});
    packet_t first_of_50 = wire((coop_wire_msg_t){.kind = COOP_WIRE_BASELINE_FIRST, .total = 50, .bytes = PAYLOAD, .size = 4});
    packet_t first_of_8 = wire((coop_wire_msg_t){.kind = COOP_WIRE_BASELINE_FIRST, .total = 8, .bytes = PAYLOAD, .size = 4});
    packet_t piece_of_6 = wire((coop_wire_msg_t){.kind = COOP_WIRE_BASELINE_PIECE, .bytes = PAYLOAD, .size = 6});
    packet_t joined_padded = wire((coop_wire_msg_t){.kind = COOP_WIRE_JOINED, .seat = 2});
    joined_padded.data[COOP_WIRE_JOINED_BYTES - 1] |= 0x80;
    packet_t late[] = {
        welcome,
        hello,
        wire((coop_wire_msg_t){.kind = COOP_WIRE_JOINED, .seat = 1}), /* its own seat */
        wire((coop_wire_msg_t){.kind = COOP_WIRE_JOINED, .seat = 0}), /* announced already */
        wire((coop_wire_msg_t){.kind = COOP_WIRE_JOINED, .seat = 5}), /* past the seats */
        wire((coop_wire_msg_t){.kind = COOP_WIRE_LEFT, .seat = 2}),   /* never announced */
        joined_padded,
        on_lane(record, COOP_LANE_BASELINE, true),
        on_lane(record, COOP_LANE_RECORDS, false),
        on_lane(state, COOP_LANE_STATE, true),
        on_lane(state, COOP_LANE_RECORDS, true),
        first_too_big,
        piece_of_6,
        cut(record, COOP_WIRE_RECORD_BYTES),
        cut(state, COOP_WIRE_STATE_BYTES - 1),
    };
    for (size_t i = 0; i < sizeof(late) / sizeof(late[0]); i++) {
        attack_guest(&late[i], 1, true, false);
    }
    attack_guest((packet_t[]){first_of_50, first_of_50}, 2, true, false); /* a first piece mid-baseline */
    attack_guest((packet_t[]){first_of_8, piece_of_6}, 2, true, false);   /* past the announced size */
    uint64_t seed = 0xFEED;
    for (int i = 0; i < 60; i++) {
        packet_t junk[3] = {garbage(&seed), garbage(&seed), garbage(&seed)};
        attack_guest(junk, 3, true, true);
    }
    TEST_ASSERT_TRUE(format_closes > attacks * 3 / 4);

    /* What may overtake the welcome on other lanes is not a break: early state is dropped, an
       early baseline waits for the welcome. */
    attacks++;
    coop_side_config_t side = {.identity = 990, .player_key = 1, .build_key = BUILD, .baseline_max = 64};
    host_end.messages = 0;
    coop_world_join(&world, 0, &side);
    for (int step = 0; step < SETTLE_STEPS && host_end.messages == 0; step++) {
        coop_world_step(&world);
        raw_poll(&host_end);
    }
    packet_t whole = wire((coop_wire_msg_t){.kind = COOP_WIRE_BASELINE_FIRST, .total = 4, .tick = 77, .bytes = PAYLOAD, .size = 4});
    send_packet(host_end.link, host_end.peer, &state);
    send_packet(host_end.link, host_end.peer, &whole);
    settle_guest(5, false);
    TEST_ASSERT_EQUAL_UINT32(0, world.guest_log[0].count);
    raw_host_welcome();
    settle_guest(5, false);
    TEST_ASSERT_EQUAL_UINT32(3, world.guest_log[0].count);
    TEST_ASSERT_EQUAL_INT(COOP_EVENT_WELCOME, world.guest_log[0].items[0].event.type);
    TEST_ASSERT_EQUAL_INT(COOP_EVENT_BASELINE, world.guest_log[0].items[1].event.type);
    TEST_ASSERT_EQUAL_UINT32(4, world.guest_log[0].items[1].event.size);
    TEST_ASSERT_EQUAL_UINT32(77, world.guest_log[0].items[1].event.tick);
    TEST_ASSERT_EQUAL_MEMORY(PAYLOAD, world.guest_log[0].items[1].data, 4);
    TEST_ASSERT_EQUAL_INT(COOP_EVENT_SEAT_JOINED, world.guest_log[0].items[2].event.type);
    TEST_ASSERT_TRUE(world.guest_log[0].items[2].event.identity == 100);
    coop_world_drop(&world, 0);
    settle_guest(5, false);
    net_link_destroy(host_end.link);
    coop_world_close(&world);
}

// #endregion

// #region forged stamps

typedef struct state_count {
    uint32_t states;
} state_count_t;

static void count_states(void *context, coop_world_t *w, int side, const coop_event_t *event) {
    state_count_t *count = context;
    (void)w;
    if (side == COOP_WORLD_HOST && event->type == COOP_EVENT_STATE) {
        count->states++;
    }
}

static void step_raw(raw_end_t *raw, int steps) {
    for (int step = 0; step < steps; step++) {
        coop_world_step(&world);
        raw_poll(raw);
    }
}

static void send_state(raw_end_t *raw, uint16_t number, uint32_t tick) {
    packet_t packet = wire((coop_wire_msg_t){.kind = COOP_WIRE_STATE, .number = number, .tick = tick, .bytes = PAYLOAD, .size = 1});
    send_packet(raw->link, raw->peer, &packet);
}

/* A seated peer replays the same numbers and stamps after silences longer than a second:
   heard once, never again. A peer's forged stamps never drive its peer tick negative. */
static void test_a_peer_that_replays_or_forges_stamps_disturbs_only_its_path(void) {
    static state_count_t count;
    count = (state_count_t){0};
    coop_world_open(&world, 83, 2, NULL);
    coop_side_config_t host = {.seats = 3, .build_key = BUILD};
    coop_world_host(&world, &host);
    raw_end_t raw = {.link = net_link_sim_guest(world.sim, 77)};
    step_raw(&raw, 5);
    packet_t hello = wire((coop_wire_msg_t){.kind = COOP_WIRE_HELLO, .protocol = COOP_SESSION_PROTOCOL, .build_key = BUILD, .player_key = 1});
    send_packet(raw.link, raw.peer, &hello);
    step_raw(&raw, 5);
    TEST_ASSERT_TRUE(coop_session_seated(world.host, 1));
    world.context = &count;
    world.on_event = count_states;
    uint32_t delivered[3] = {0};
    for (int round = 0; round < 3; round++) {
        uint32_t before = count.states;
        for (uint16_t n = 100; n < 140; n++) {
            send_state(&raw, n, 1000U + n);
            step_raw(&raw, 1);
        }
        step_raw(&raw, 70);
        delivered[round] = count.states - before;
    }
    TEST_ASSERT_EQUAL_UINT32(40, delivered[0]);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, delivered[1], "a replayed number was heard again");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, delivered[2], "a replayed number was heard again");
    /* A packet the window still places (one behind the newest, never taken) with a stamp half
       the 32-bit range back, once the good stamps aged out of the clock. */
    send_state(&raw, 141, 1141);
    step_raw(&raw, 1);
    TEST_ASSERT_TRUE(coop_session_peer_tick(world.host, 1) > 0.0);
    step_raw(&raw, 90);
    uint32_t before = count.states;
    send_state(&raw, 140, 1140U + 0x80000001U);
    step_raw(&raw, 2);
    TEST_ASSERT_EQUAL_UINT32(before + 1, count.states);
    TEST_ASSERT_TRUE_MESSAGE(coop_session_peer_tick(world.host, 1) >= 0.0, "a forged stamp made the peer tick negative");
    TEST_ASSERT_TRUE(coop_session_seated(world.host, 1));
    net_link_destroy(raw.link);
    coop_world_close(&world);
}

// #endregion

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_host_closes_every_offender_and_keeps_the_others);
    RUN_TEST(test_a_guest_closes_a_host_that_breaks_the_protocol);
    RUN_TEST(test_a_peer_that_replays_or_forges_stamps_disturbs_only_its_path);
    return UNITY_END();
}
