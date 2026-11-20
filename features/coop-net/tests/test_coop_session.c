#include "coop_test_world.h"
#include "coop_pending.h"
#include "coop_wire.h"
#include "unity.h"

#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static coop_world_t world;

// #region helpers

static const char BASE_KEY[] = "base-7";
#define BASE_KEY_SIZE ((uint32_t)(sizeof(BASE_KEY) - 1))
enum { BUILD = 42 };

static coop_side_config_t host_side(uint32_t seats) {
    return (coop_side_config_t){.seats = seats, .build_key = BUILD, .world_key = BASE_KEY, .world_key_size = BASE_KEY_SIZE, .baseline_max = 4096};
}

static coop_side_config_t guest_side(uint64_t identity, uint64_t player_key) {
    return (coop_side_config_t){.identity = identity, .player_key = player_key, .build_key = BUILD, .baseline_max = 4096};
}

static void run_with_raw(raw_end_t *raw, double seconds, bool until_disconnected) {
    int steps = (int)(seconds / world.dt + 0.5);
    for (int step = 0; step < steps && !(until_disconnected && raw->disconnected); step++) {
        coop_world_step(&world);
        raw_poll(raw);
    }
}

static const coop_logged_t *last_of(const coop_log_t *log, coop_event_type_t type) {
    uint32_t kept = log->count < COOP_WORLD_LOG ? log->count : COOP_WORLD_LOG;
    for (uint32_t i = kept; i > 0; i--) {
        if (log->items[i - 1].event.type == type) {
            return &log->items[i - 1];
        }
    }
    return NULL;
}

static uint8_t welcomed_seat(int guest) {
    const coop_logged_t *welcome = coop_log_find(&world.guest_log[guest], COOP_EVENT_WELCOME, COOP_SEAT_NONE);
    TEST_ASSERT_NOT_NULL_MESSAGE(welcome, "guest was not welcomed");
    return welcome->event.seat;
}

static uint32_t closed_reason(int guest) {
    const coop_logged_t *closed = coop_log_find(&world.guest_log[guest], COOP_EVENT_CLOSED, COOP_SEAT_NONE);
    TEST_ASSERT_NOT_NULL_MESSAGE(closed, "guest was not closed");
    return closed->event.reason;
}

static void seat_three_guests(void) {
    coop_world_open(&world, 11, 6, NULL);
    coop_side_config_t host = host_side(4);
    coop_world_host(&world, &host);
    for (int guest = 0; guest < 3; guest++) {
        coop_side_config_t side = guest_side(201 + (uint64_t)guest, 1 + (uint64_t)guest);
        coop_world_join(&world, guest, &side);
        coop_world_run(&world, 0.2);
    }
}

// #endregion

// #region seats and roster

static void test_four_seats_and_the_roster(void) {
    seat_three_guests();
    TEST_ASSERT_EQUAL_UINT32(3, coop_log_count(&world.host_log, COOP_EVENT_SEAT_JOINED));
    for (int guest = 0; guest < 3; guest++) {
        uint8_t seat = welcomed_seat(guest);
        TEST_ASSERT_EQUAL_UINT8(1 + guest, seat);
        TEST_ASSERT_EQUAL_UINT8(seat, coop_session_seat(world.guest[guest]));
        const coop_logged_t *welcome = coop_log_find(&world.guest_log[guest], COOP_EVENT_WELCOME, COOP_SEAT_NONE);
        TEST_ASSERT_TRUE(welcome->event.tick >= 1000 && coop_session_tick(world.guest[guest]) >= 1000);
        const coop_logged_t *joined = coop_log_find(&world.host_log, COOP_EVENT_SEAT_JOINED, seat);
        TEST_ASSERT_NOT_NULL(joined);
        TEST_ASSERT_TRUE(joined->event.identity == 201 + (uint64_t)guest && joined->event.player_key == 1 + (uint64_t)guest);
        /* Every other seat is announced once, the host's with its link identity. */
        TEST_ASSERT_EQUAL_UINT32(3, coop_log_count(&world.guest_log[guest], COOP_EVENT_SEAT_JOINED));
        for (uint8_t other = 0; other < 4; other++) {
            TEST_ASSERT_TRUE(coop_session_seated(world.guest[guest], other));
            if (other == seat) {
                TEST_ASSERT_NULL(coop_log_find(&world.guest_log[guest], COOP_EVENT_SEAT_JOINED, other));
                continue;
            }
            const coop_logged_t *notice = coop_log_find(&world.guest_log[guest], COOP_EVENT_SEAT_JOINED, other);
            TEST_ASSERT_NOT_NULL(notice);
            uint64_t identity = other == 0 ? COOP_WORLD_HOST_IDENTITY : 200 + (uint64_t)other;
            TEST_ASSERT_TRUE(notice->event.identity == identity);
            TEST_ASSERT_TRUE(notice->event.player_key == 0); /* a guest is not told others' keys */
        }
        TEST_ASSERT_FALSE(coop_session_seated(world.guest[guest], 4));
        /* The welcome comes first. */
        TEST_ASSERT_EQUAL_INT(COOP_EVENT_WELCOME, world.guest_log[guest].items[0].event.type);
    }
    for (uint8_t seat = 0; seat < 4; seat++) {
        TEST_ASSERT_TRUE(coop_session_seated(world.host, seat));
    }
    coop_side_config_t side = guest_side(204, 4);
    coop_world_join(&world, 3, &side);
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_FULL, closed_reason(3));
    TEST_ASSERT_EQUAL_UINT32(0, coop_log_count(&world.guest_log[3], COOP_EVENT_WELCOME));
    TEST_ASSERT_EQUAL_UINT32(3, coop_log_count(&world.host_log, COOP_EVENT_SEAT_JOINED));
    TEST_ASSERT_EQUAL_UINT32(1, world.guest_log[3].count);
    coop_world_close(&world);
}

static void test_guests_that_join_together_take_distinct_seats(void) {
    coop_world_open(&world, 12, 6, NULL);
    coop_side_config_t host = host_side(8);
    coop_world_host(&world, &host);
    for (int guest = 0; guest < 5; guest++) {
        coop_side_config_t side = guest_side(301 + (uint64_t)guest, 7);
        coop_world_join(&world, guest, &side);
    }
    coop_world_run(&world, 0.3);
    uint32_t seats = 0;
    for (int guest = 0; guest < 5; guest++) {
        uint8_t seat = welcomed_seat(guest);
        TEST_ASSERT_TRUE(seat >= 1 && seat <= 5);
        seats |= 1U << seat;
        TEST_ASSERT_EQUAL_UINT32(5, coop_log_count(&world.guest_log[guest], COOP_EVENT_SEAT_JOINED));
    }
    TEST_ASSERT_EQUAL_HEX32(0x3E, seats);
    coop_world_close(&world);
}

// #endregion

// #region refusals

static void test_refusals_name_the_reason(void) {
    coop_world_open(&world, 13, 6, NULL);
    coop_side_config_t host = host_side(8);
    coop_world_host(&world, &host);
    coop_side_config_t build = guest_side(401, 1);
    build.build_key = BUILD + 1;
    coop_world_join(&world, 0, &build);
    coop_side_config_t other_world = guest_side(402, 1);
    other_world.world_key = "base-8";
    other_world.world_key_size = 6;
    coop_world_join(&world, 1, &other_world);
    coop_side_config_t shorter_world = guest_side(403, 1);
    shorter_world.world_key = BASE_KEY;
    shorter_world.world_key_size = BASE_KEY_SIZE - 1;
    coop_world_join(&world, 2, &shorter_world);
    coop_side_config_t same_world = guest_side(404, 1);
    same_world.world_key = BASE_KEY;
    same_world.world_key_size = BASE_KEY_SIZE;
    coop_world_join(&world, 3, &same_world);
    coop_side_config_t no_world = guest_side(405, 1);
    coop_world_join(&world, 4, &no_world);
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_BUILD, closed_reason(0));
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_WORLD, closed_reason(1));
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_WORLD, closed_reason(2));
    for (int guest = 3; guest <= 4; guest++) {
        const coop_logged_t *welcome = coop_log_find(&world.guest_log[guest], COOP_EVENT_WELCOME, COOP_SEAT_NONE);
        TEST_ASSERT_NOT_NULL(welcome);
        TEST_ASSERT_EQUAL_UINT32(BASE_KEY_SIZE, welcome->event.size);
        TEST_ASSERT_EQUAL_MEMORY(BASE_KEY, welcome->data, BASE_KEY_SIZE);
    }
    /* A raw link that speaks another session protocol. */
    raw_end_t raw = {.link = net_link_sim_guest(world.sim, 406)};
    TEST_ASSERT_NOT_NULL(raw.link);
    run_with_raw(&raw, 0.2, false);
    TEST_ASSERT_TRUE(raw.connected);
    uint8_t packet[64];
    coop_wire_msg_t hello = {.kind = COOP_WIRE_HELLO, .protocol = COOP_SESSION_PROTOCOL + 1, .build_key = BUILD};
    uint32_t size = coop_wire_write(&hello, packet, sizeof(packet));
    TEST_ASSERT_EQUAL(NET_LINK_SEND_OK, net_link_send(raw.link, raw.peer, COOP_LANE_RECORDS, NET_LINK_SEND_RELIABLE, packet, size));
    run_with_raw(&raw, 0.3, true);
    TEST_ASSERT_TRUE(raw.disconnected);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_VERSION, raw.reason);
    TEST_ASSERT_EQUAL_UINT32(2, coop_log_count(&world.host_log, COOP_EVENT_SEAT_JOINED));
    net_link_destroy(raw.link);
    coop_world_close(&world);
}

static void test_a_new_world_key_reaches_later_guests(void) {
    coop_world_open(&world, 14, 4, NULL);
    coop_side_config_t host = host_side(4);
    coop_world_host(&world, &host);
    TEST_ASSERT_TRUE(coop_session_set_world_key(world.host, "run-2", 5));
    TEST_ASSERT_FALSE(coop_session_set_world_key(world.host, NULL, 3));
    TEST_ASSERT_FALSE(coop_session_set_world_key(world.host, "x", COOP_WORLD_KEY_MAX + 1));
    coop_side_config_t old_world = guest_side(501, 1);
    old_world.world_key = BASE_KEY;
    old_world.world_key_size = BASE_KEY_SIZE;
    coop_world_join(&world, 0, &old_world);
    coop_side_config_t fresh = guest_side(502, 1);
    coop_world_join(&world, 1, &fresh);
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_WORLD, closed_reason(0));
    const coop_logged_t *welcome = coop_log_find(&world.guest_log[1], COOP_EVENT_WELCOME, COOP_SEAT_NONE);
    TEST_ASSERT_NOT_NULL(welcome);
    TEST_ASSERT_EQUAL_UINT32(5, welcome->event.size);
    TEST_ASSERT_EQUAL_MEMORY("run-2", welcome->data, 5);
    coop_world_close(&world);
}

// #endregion

// #region takeover, kick, shut

static void test_the_same_player_takes_its_seat_over(void) {
    seat_three_guests();
    uint8_t seat = welcomed_seat(0);
    coop_log_clear(&world.host_log);
    coop_log_clear(&world.guest_log[1]);
    /* Same identity, another player key: two windows of one account get two seats... */
    coop_world_drop(&world, 2);
    coop_world_run(&world, 0.2);
    coop_side_config_t twin = guest_side(201, 99);
    coop_world_join(&world, 2, &twin);
    coop_world_run(&world, 0.2);
    TEST_ASSERT_EQUAL_UINT8(3, welcomed_seat(2));
    TEST_ASSERT_EQUAL_UINT32(0, coop_log_count(&world.guest_log[0], COOP_EVENT_CLOSED));
    /* ...and the same identity with the same key takes the old seat over. */
    coop_log_clear(&world.host_log);
    coop_log_clear(&world.guest_log[1]);
    coop_side_config_t again = guest_side(201, 1);
    coop_world_join(&world, 3, &again);
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_REPLACED, closed_reason(0));
    TEST_ASSERT_EQUAL_UINT8(seat, welcomed_seat(3));
    TEST_ASSERT_EQUAL_UINT32(2, world.host_log.count);
    TEST_ASSERT_EQUAL_INT(COOP_EVENT_SEAT_LEFT, world.host_log.items[0].event.type);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_REPLACED, world.host_log.items[0].event.reason);
    TEST_ASSERT_EQUAL_INT(COOP_EVENT_SEAT_JOINED, world.host_log.items[1].event.type);
    TEST_ASSERT_EQUAL_UINT8(seat, world.host_log.items[1].event.seat);
    TEST_ASSERT_TRUE(world.host_log.items[1].event.identity == 201 && world.host_log.items[1].event.player_key == 1);
    /* The others are told in the same order. */
    TEST_ASSERT_EQUAL_UINT32(2, world.guest_log[1].count);
    TEST_ASSERT_EQUAL_INT(COOP_EVENT_SEAT_LEFT, world.guest_log[1].items[0].event.type);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_REPLACED, world.guest_log[1].items[0].event.reason);
    TEST_ASSERT_EQUAL_INT(COOP_EVENT_SEAT_JOINED, world.guest_log[1].items[1].event.type);
    TEST_ASSERT_EQUAL_UINT8(seat, world.guest_log[1].items[1].event.seat);
    coop_world_close(&world);
}

static void test_kick_closes_with_the_game_code(void) {
    seat_three_guests();
    coop_log_clear(&world.host_log);
    coop_session_kick(world.host, 2, COOP_CLOSE_GAME_FIRST + 7);
    TEST_ASSERT_FALSE(coop_session_seated(world.host, 2));
    TEST_ASSERT_EQUAL_UINT32(0, world.host_log.count);
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_GAME_FIRST + 7, closed_reason(1));
    const coop_logged_t *left = coop_log_find(&world.host_log, COOP_EVENT_SEAT_LEFT, 2);
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_GAME_FIRST + 7, left->event.reason);
    TEST_ASSERT_TRUE(left->event.identity == 202);
    TEST_ASSERT_EQUAL_UINT32(1, coop_log_count(&world.host_log, COOP_EVENT_SEAT_LEFT));
    for (int guest = 0; guest < 3; guest += 2) {
        const coop_logged_t *told = coop_log_find(&world.guest_log[guest], COOP_EVENT_SEAT_LEFT, 2);
        TEST_ASSERT_NOT_NULL(told);
        TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_GAME_FIRST + 7, told->event.reason);
        TEST_ASSERT_TRUE(told->event.identity == 202);
        TEST_ASSERT_FALSE(coop_session_seated(world.guest[guest], 2));
    }
    /* Kicking an empty seat, the host's seat or a seat past the session does nothing. */
    coop_session_kick(world.host, 2, COOP_CLOSE_KICKED);
    coop_session_kick(world.host, 0, COOP_CLOSE_KICKED);
    coop_session_kick(world.host, 9, COOP_CLOSE_KICKED);
    coop_world_run(&world, 0.1);
    TEST_ASSERT_EQUAL_UINT32(1, coop_log_count(&world.host_log, COOP_EVENT_SEAT_LEFT));
    coop_world_close(&world);
}

static void test_a_shut_host_still_seats_a_returning_player(void) {
    seat_three_guests();
    coop_world_drop(&world, 2);
    coop_world_run(&world, 0.2);
    coop_session_set_open(world.host, false);
    coop_side_config_t stranger = guest_side(299, 1);
    coop_world_join(&world, 2, &stranger);
    coop_side_config_t returning = guest_side(201, 1);
    coop_world_join(&world, 3, &returning);
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_SHUT, closed_reason(2));
    TEST_ASSERT_EQUAL_UINT8(1, welcomed_seat(3));
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_REPLACED, closed_reason(0));
    coop_session_set_open(world.host, true);
    coop_side_config_t later = guest_side(298, 1);
    coop_world_drop(&world, 2);
    coop_world_join(&world, 2, &later);
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL_UINT8(3, welcomed_seat(2));
    coop_world_close(&world);
}

// #endregion

// #region leaving

static void test_host_destroy_closes_every_guest_with_host_left(void) {
    seat_three_guests();
    coop_session_destroy(world.host);
    world.host = NULL;
    /* The link goes right after: the close still lingers. */
    net_link_destroy(world.host_link);
    world.host_link = NULL;
    coop_world_run(&world, 0.3);
    for (int guest = 0; guest < 3; guest++) {
        TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_HOST_LEFT, closed_reason(guest));
        TEST_ASSERT_EQUAL_INT(COOP_EVENT_CLOSED, last_of(&world.guest_log[guest], COOP_EVENT_CLOSED)->event.type);
        TEST_ASSERT_EQUAL_UINT32(1, coop_log_count(&world.guest_log[guest], COOP_EVENT_CLOSED));
        uint8_t byte = 1;
        TEST_ASSERT_EQUAL(COOP_SEND_NO_SEAT, coop_session_send_record(world.guest[guest], 0, &byte, 1));
    }
    coop_world_close(&world);
}

static void test_guest_destroy_is_a_left_seat(void) {
    seat_three_guests();
    coop_world_drop(&world, 1);
    coop_world_run(&world, 0.3);
    const coop_logged_t *left = coop_log_find(&world.host_log, COOP_EVENT_SEAT_LEFT, 2);
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_LEFT, left->event.reason);
    for (int guest = 0; guest < 3; guest += 2) {
        const coop_logged_t *told = coop_log_find(&world.guest_log[guest], COOP_EVENT_SEAT_LEFT, 2);
        TEST_ASSERT_NOT_NULL(told);
        TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_LEFT, told->event.reason);
    }
    coop_world_close(&world);
}

static void test_a_cut_path_times_out(void) {
    net_link_sim_config_t config = {.timeout_s = 1.0F};
    coop_world_open(&world, 15, 4, &config);
    coop_side_config_t host = host_side(4);
    coop_world_host(&world, &host);
    for (int guest = 0; guest < 2; guest++) {
        coop_side_config_t side = guest_side(601 + (uint64_t)guest, 1);
        coop_world_join(&world, guest, &side);
    }
    coop_world_run(&world, 0.3);
    net_link_sim_set_cut(world.sim, world.guest_link[0], true);
    coop_world_run(&world, 1.5);
    const coop_logged_t *left = coop_log_find(&world.host_log, COOP_EVENT_SEAT_LEFT, 1);
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, left->event.reason);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, closed_reason(0));
    const coop_logged_t *told = coop_log_find(&world.guest_log[1], COOP_EVENT_SEAT_LEFT, 1);
    TEST_ASSERT_NOT_NULL(told);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, told->event.reason);
    coop_world_close(&world);
}

static void test_no_hello_in_time_closes_with_handshake(void) {
    coop_world_open(&world, 16, 4, NULL);
    coop_side_config_t host = host_side(4);
    host.handshake_timeout_s = 0.5F;
    coop_world_host(&world, &host);
    raw_end_t raw = {.link = net_link_sim_guest(world.sim, 701)};
    run_with_raw(&raw, 0.4, false);
    TEST_ASSERT_TRUE(raw.connected);
    TEST_ASSERT_FALSE(raw.disconnected);
    run_with_raw(&raw, 0.5, true);
    TEST_ASSERT_TRUE(raw.disconnected);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_HANDSHAKE, raw.reason);
    net_link_destroy(raw.link);
    coop_world_close(&world);

    /* A host link with no session never welcomes: the guest gives up too. */
    coop_world_open(&world, 17, 4, NULL);
    raw_end_t host_end = {.link = net_link_sim_host(world.sim, 100, 4)};
    coop_side_config_t side = guest_side(702, 1);
    side.handshake_timeout_s = 0.5F;
    coop_world_join(&world, 0, &side);
    run_with_raw(&host_end, 1.0, false);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_HANDSHAKE, closed_reason(0));
    TEST_ASSERT_EQUAL_UINT32(1, host_end.messages);
    TEST_ASSERT_TRUE(host_end.disconnected);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_HANDSHAKE, host_end.reason);
    coop_world_drop(&world, 0);
    net_link_destroy(host_end.link);
    coop_world_close(&world);
}

// #endregion

// #region sending rules

static void test_nothing_leaves_without_end_tick(void) {
    coop_world_open(&world, 18, 2, NULL);
    coop_side_config_t host = host_side(2);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(801, 1);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.3);
    coop_log_clear(&world.host_log);
    uint8_t bytes[4] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_record(world.guest[0], 0, bytes, 4));
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_state(world.guest[0], 0, bytes, 4, NULL));
    for (int step = 0; step < 30; step++) {
        net_link_sim_advance(world.sim, world.dt);
        double now = net_link_sim_time(world.sim);
        coop_session_set_time(world.host, now);
        coop_event_t event;
        while (coop_session_poll(world.host, &event)) {
            TEST_FAIL_MESSAGE("something left before the guest ended its tick");
        }
        coop_session_end_tick(world.host);
        coop_session_set_time(world.guest[0], now);
        while (coop_session_poll(world.guest[0], &event)) {
        }
    }
    coop_session_end_tick(world.guest[0]);
    coop_world_run(&world, 0.1);
    TEST_ASSERT_EQUAL_UINT32(1, coop_log_count(&world.host_log, COOP_EVENT_RECORD));
    TEST_ASSERT_EQUAL_UINT32(1, coop_log_count(&world.host_log, COOP_EVENT_STATE));
    coop_world_close(&world);
}

static void test_size_limits(void) {
    coop_world_open(&world, 19, 2, NULL);
    coop_side_config_t host = host_side(2);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(901, 1);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.3);
    coop_log_clear(&world.guest_log[0]);
    static uint8_t big[20000];
    for (uint32_t i = 0; i < sizeof(big); i++) {
        big[i] = (uint8_t)(i * 7U);
    }
    uint32_t state_max = coop_session_state_max(world.host);
    uint32_t record_max = coop_session_record_max(world.host);
    TEST_ASSERT_EQUAL_UINT32(net_link_max_unreliable(world.host_link), state_max + COOP_WIRE_STATE_BYTES);
    TEST_ASSERT_EQUAL_UINT32(net_link_max_reliable(world.host_link), record_max + COOP_WIRE_RECORD_BYTES);
    TEST_ASSERT_EQUAL(COOP_SEND_TOO_LARGE, coop_session_send_state(world.host, 1, big, state_max + 1, NULL));
    TEST_ASSERT_EQUAL(COOP_SEND_TOO_LARGE, coop_session_send_record(world.host, 1, big, record_max + 1));
    TEST_ASSERT_EQUAL(COOP_SEND_TOO_LARGE, coop_session_send_baseline(world.host, 1, big, 4097));
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_state(world.host, 1, big, state_max, NULL));
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_record(world.host, 1, big, record_max));
    coop_world_run(&world, 0.2);
    const coop_logged_t *state = coop_log_find(&world.guest_log[0], COOP_EVENT_STATE, 0);
    const coop_logged_t *record = coop_log_find(&world.guest_log[0], COOP_EVENT_RECORD, 0);
    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(record);
    TEST_ASSERT_EQUAL_UINT32(state_max, state->event.size);
    TEST_ASSERT_EQUAL_UINT32(record_max, record->event.size);
    TEST_ASSERT_EQUAL_MEMORY(big, state->data, COOP_WORLD_DATA);
    coop_world_close(&world);
}

static void test_one_baseline_at_a_time(void) {
    coop_world_open(&world, 20, 2, NULL);
    coop_side_config_t host = host_side(2);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(1001, 1);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.3);
    uint8_t bytes[3000];
    memset(bytes, 0x42, sizeof(bytes));
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_baseline(world.host, 1, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL(COOP_SEND_FULL, coop_session_send_baseline(world.host, 1, bytes, sizeof(bytes)));
    coop_world_step(&world);
    /* It left in one end_tick: a second one is taken. */
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_baseline(world.host, 1, bytes, 10));
    coop_world_run(&world, 0.2);
    TEST_ASSERT_EQUAL_UINT32(2, coop_log_count(&world.guest_log[0], COOP_EVENT_BASELINE));
    TEST_ASSERT_EQUAL_UINT32(10, last_of(&world.guest_log[0], COOP_EVENT_BASELINE)->event.size);
    coop_world_close(&world);
}

static void test_calls_a_role_does_not_have(void) {
    coop_world_open(&world, 21, 3, NULL);
    coop_side_config_t host = host_side(3);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(1101, 1);
    coop_session_t *guest = coop_world_join(&world, 0, &side);
    uint8_t byte = 7;
    /* Not welcomed yet. */
    TEST_ASSERT_EQUAL_UINT8(COOP_SEAT_NONE, coop_session_seat(guest));
    TEST_ASSERT_TRUE(coop_session_peer_tick(guest, 0) < 0.0);
    TEST_ASSERT_EQUAL_UINT32(0, coop_session_seats(guest));
    TEST_ASSERT_EQUAL_UINT32(0, coop_session_tick_hz(guest));
    TEST_ASSERT_EQUAL(COOP_SEND_NO_SEAT, coop_session_send_record(guest, 0, &byte, 1));
    TEST_ASSERT_FALSE(coop_session_seated(guest, 1));
    coop_world_run(&world, 0.3);
    TEST_ASSERT_FALSE(coop_session_is_host(guest));
    TEST_ASSERT_TRUE(coop_session_is_host(world.host));
    TEST_ASSERT_EQUAL(COOP_SEND_INVALID, coop_session_send_baseline(guest, 0, &byte, 1));
    TEST_ASSERT_FALSE(coop_session_set_world_key(guest, "k", 1));
    TEST_ASSERT_EQUAL(COOP_SEND_INVALID, coop_session_send_record(guest, 0, NULL, 1));
    TEST_ASSERT_EQUAL(COOP_SEND_INVALID, coop_session_send_record(guest, 0, &byte, 0));
    TEST_ASSERT_EQUAL(COOP_SEND_INVALID, coop_session_send_state(world.host, 1, NULL, 4, NULL));
    TEST_ASSERT_EQUAL(COOP_SEND_INVALID, coop_session_send_state(NULL, 1, &byte, 1, NULL));
    TEST_ASSERT_EQUAL(COOP_SEND_NO_SEAT, coop_session_send_record(guest, 2, &byte, 1));
    TEST_ASSERT_EQUAL(COOP_SEND_NO_SEAT, coop_session_send_record(world.host, 0, &byte, 1));
    TEST_ASSERT_EQUAL(COOP_SEND_NO_SEAT, coop_session_send_record(world.host, 2, &byte, 1));
    TEST_ASSERT_EQUAL(COOP_SEND_NO_SEAT, coop_session_send_state(world.host, 7, &byte, 1, NULL));
    TEST_ASSERT_EQUAL(COOP_SEND_NO_SEAT, coop_session_send_baseline(world.host, 2, &byte, 1));
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_LOST, coop_session_packet_fate(world.host, 2, 0));
    coop_seat_stats_t stats;
    TEST_ASSERT_FALSE(coop_session_seat_stats(world.host, 0, &stats));
    TEST_ASSERT_FALSE(coop_session_seat_stats(world.host, 2, &stats));
    TEST_ASSERT_FALSE(coop_session_seat_stats(guest, 1, &stats));
    TEST_ASSERT_TRUE(coop_session_seat_stats(guest, 0, &stats));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_PEER_NONE, coop_session_peer(world.host, 0));
    TEST_ASSERT_TRUE(coop_session_peer(world.host, 1) != NET_LINK_PEER_NONE);
    TEST_ASSERT_TRUE(coop_session_peer(guest, 0) != NET_LINK_PEER_NONE);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_PEER_NONE, coop_session_peer(guest, 1));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_PEER_NONE, coop_session_peer(world.host, 200));
    TEST_ASSERT_FALSE(coop_session_seated(world.host, 200));
    /* Host-only calls on a guest change nothing. */
    coop_session_set_open(guest, false);
    coop_session_kick(guest, 0, COOP_CLOSE_KICKED);
    coop_world_run(&world, 0.1);
    TEST_ASSERT_EQUAL_UINT32(0, coop_log_count(&world.guest_log[0], COOP_EVENT_CLOSED));
    TEST_ASSERT_TRUE(coop_session_peer_tick(guest, 0) >= 0.0);
    TEST_ASSERT_TRUE(coop_session_peer_tick(world.host, 1) >= 0.0);
    TEST_ASSERT_TRUE(coop_session_peer_tick(world.host, 0) < 0.0 && coop_session_peer_tick(world.host, 2) < 0.0);
    TEST_ASSERT_EQUAL_UINT32(3, coop_session_seats(guest));
    TEST_ASSERT_EQUAL_UINT32(60, coop_session_tick_hz(guest));
    TEST_ASSERT_EQUAL_UINT32(3, coop_session_seats(world.host));
    TEST_ASSERT_EQUAL_UINT32(60, coop_session_tick_hz(world.host));
    /* NULL sessions answer without touching anything. */
    coop_session_destroy(NULL);
    coop_session_set_time(NULL, 1.0);
    coop_session_end_tick(NULL);
    TEST_ASSERT_FALSE(coop_session_poll(NULL, NULL));
    TEST_ASSERT_EQUAL_UINT32(0, coop_session_state_max(NULL));
    coop_world_close(&world);
}

static void test_configs_out_of_range_are_refused(void) {
    coop_world_open(&world, 22, 2, NULL);
    net_link_t *host_link = net_link_sim_host(world.sim, 100, 2);
    coop_session_config_t good = {.link = host_link, .seats = 4, .tick_hz = 60};
    coop_session_t *session = coop_session_host(&good);
    TEST_ASSERT_NOT_NULL(session);
    coop_session_destroy(session);
    coop_session_config_t bad[] = {
        {.link = NULL, .seats = 4, .tick_hz = 60},
        {.link = host_link, .seats = 0, .tick_hz = 60},
        {.link = host_link, .seats = COOP_SEATS_MAX + 1, .tick_hz = 60},
        {.link = host_link, .seats = 4, .tick_hz = 0},
        {.link = host_link, .seats = 4, .tick_hz = 70000},
        {.link = host_link, .seats = 4, .tick_hz = 60, .world_key_size = COOP_WORLD_KEY_MAX + 1, .world_key = BASE_KEY},
        {.link = host_link, .seats = 4, .tick_hz = 60, .world_key_size = 3},
        {.link = host_link, .seats = 4, .tick_hz = 60, .handshake_timeout_s = -1.0F},
        {.link = host_link, .seats = 4, .tick_hz = 60, .handshake_timeout_s = NAN},
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        TEST_ASSERT_NULL(coop_session_host(&bad[i]));
        if (i < 1 || i > 4) { /* seats and tick_hz are the host's */
            TEST_ASSERT_NULL(coop_session_join(&bad[i]));
        }
    }
    TEST_ASSERT_NULL(coop_session_host(NULL));
    TEST_ASSERT_NULL(coop_session_join(NULL));
    net_link_destroy(host_link);
    coop_world_close(&world);

    /* A link with other lanes. */
    net_link_sim_config_t two = {.seed = 1, .max_guests = 1, .lanes = {.count = 2}};
    net_link_sim_t *sim = net_link_sim_create(&two);
    TEST_ASSERT_NOT_NULL(sim);
    net_link_t *link = net_link_sim_host(sim, 100, 1);
    coop_session_config_t lanes = {.link = link, .seats = 2, .tick_hz = 60};
    TEST_ASSERT_NULL(coop_session_host(&lanes));
    net_link_destroy(link);
    net_link_sim_destroy(sim);
}

// #endregion

// #region waiting peers, stalls, the event queue

static void step_with_raws(raw_end_t *raws, int count, int steps) {
    for (int step = 0; step < steps; step++) {
        coop_world_step(&world);
        for (int i = 0; i < count; i++) {
            if (raws[i].link) {
                raw_poll(&raws[i]);
            }
        }
    }
}

static void test_waiting_peers_never_turn_an_honest_guest_away(void) {
    enum { RAWS = 14 };
    coop_world_open(&world, 30, 16, NULL);
    coop_side_config_t host = host_side(4);
    coop_world_host(&world, &host);
    static raw_end_t raws[RAWS];
    for (int i = 0; i < RAWS; i++) {
        raws[i] = (raw_end_t){.link = net_link_sim_guest(world.sim, 3000 + (uint64_t)i)};
        TEST_ASSERT_NOT_NULL(raws[i].link);
        step_with_raws(raws, RAWS, 1);
    }
    coop_side_config_t honest = guest_side(3100, 1);
    coop_world_join(&world, 0, &honest);
    step_with_raws(raws, RAWS, 12);
    TEST_ASSERT_EQUAL_UINT8(1, welcomed_seat(0));
    for (int i = 0; i < RAWS; i++) {
        TEST_ASSERT_TRUE(raws[i].connected && !raws[i].disconnected);
    }
    /* A newer connection of a waiting identity takes its place. */
    raw_end_t again = {.link = net_link_sim_guest(world.sim, 3004)};
    for (int step = 0; step < 6; step++) {
        step_with_raws(raws, RAWS, 1);
        raw_poll(&again);
    }
    TEST_ASSERT_TRUE(again.connected && !again.disconnected);
    TEST_ASSERT_TRUE(raws[4].disconnected);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_HANDSHAKE, raws[4].reason);
    TEST_ASSERT_FALSE(raws[5].disconnected);
    net_link_destroy(again.link);
    for (int i = 0; i < RAWS; i++) {
        net_link_destroy(raws[i].link);
    }
    coop_world_close(&world);
}

/* Sends records to `seat` until its queue refuses one; 105-byte messages fill a 2100-byte queue to the last byte. */
static void fill_queue(uint8_t seat) {
    static uint8_t record[100];
    coop_send_result_t result = COOP_SEND_OK;
    for (int i = 0; i < 1000 && result == COOP_SEND_OK; i++) {
        result = coop_session_send_record(world.host, seat, record, sizeof(record));
    }
    TEST_ASSERT_EQUAL(COOP_SEND_FULL, result);
}

static void test_a_guest_that_stops_reading_is_closed_with_stalled(void) {
    net_link_sim_config_t small = {.max_reliable_bytes = 2048, .reliable_queue_bytes = 2100};
    coop_world_open(&world, 31, 4, &small);
    coop_side_config_t host = host_side(4);
    coop_world_host(&world, &host);
    coop_side_config_t a = guest_side(3201, 1);
    coop_world_join(&world, 0, &a);
    coop_world_run(&world, 0.2);
    world.frozen = 1U << 0;
    fill_queue(1);
    coop_log_clear(&world.host_log);
    /* The notice of a newcomer does not fit the frozen guest. */
    coop_side_config_t b = guest_side(3202, 1);
    coop_world_join(&world, 1, &b);
    coop_world_run(&world, 0.3);
    const coop_logged_t *left = coop_log_find(&world.host_log, COOP_EVENT_SEAT_LEFT, 1);
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_STALLED, left->event.reason);
    TEST_ASSERT_EQUAL_UINT8(2, welcomed_seat(1));
    const coop_logged_t *told = coop_log_find(&world.guest_log[1], COOP_EVENT_SEAT_LEFT, 1);
    TEST_ASSERT_NOT_NULL(told);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_STALLED, told->event.reason);
    world.frozen = 0;
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_STALLED, closed_reason(0));
    coop_world_close(&world);

    /* A welcome and roster that cannot fit the newcomer's queue: one message at a time. */
    net_link_sim_config_t tiny = {.queue_messages = 1};
    coop_world_open(&world, 32, 2, &tiny);
    coop_side_config_t keyed = host_side(4);
    coop_world_host(&world, &keyed);
    coop_side_config_t c = guest_side(3203, 1);
    coop_world_join(&world, 0, &c);
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_STALLED, closed_reason(0));
    TEST_ASSERT_EQUAL_UINT32(0, coop_log_count(&world.host_log, COOP_EVENT_SEAT_JOINED));
    TEST_ASSERT_FALSE(coop_session_seated(world.host, 1));
    coop_world_close(&world);
}

static void test_the_event_queue_holds_its_worst_case(void) {
    coop_world_open(&world, 33, 8, NULL);
    coop_side_config_t host = host_side(8);
    coop_world_host(&world, &host);
    for (int guest = 0; guest < 7; guest++) {
        coop_side_config_t side = guest_side(3300 + (uint64_t)guest, 1);
        coop_world_join(&world, guest, &side);
    }
    coop_world_run(&world, 0.3);
    /* A full link batch: a hundred records from one guest in one tick. */
    uint8_t record[4];
    for (uint32_t i = 0; i < 100; i++) {
        memcpy(record, &i, sizeof(record));
        TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_record(world.guest[0], COOP_SEAT_HOST, record, sizeof(record)));
    }
    coop_log_clear(&world.host_log);
    coop_world_run(&world, 0.1);
    TEST_ASSERT_EQUAL_UINT32(100, coop_log_count(&world.host_log, COOP_EVENT_RECORD));
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t got = 0;
        memcpy(&got, world.host_log.items[i].data, sizeof(got));
        TEST_ASSERT_EQUAL_UINT32(i, got);
    }
    /* Every seat kicked between two polls. */
    coop_log_clear(&world.host_log);
    for (uint8_t seat = 1; seat < 8; seat++) {
        coop_session_kick(world.host, seat, COOP_CLOSE_GAME_FIRST + seat);
    }
    coop_world_step(&world);
    TEST_ASSERT_EQUAL_UINT32(7, world.host_log.count);
    for (uint8_t seat = 1; seat < 8; seat++) {
        TEST_ASSERT_EQUAL_INT(COOP_EVENT_SEAT_LEFT, world.host_log.items[seat - 1].event.type);
        TEST_ASSERT_EQUAL_UINT8(seat, world.host_log.items[seat - 1].event.seat);
        TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_GAME_FIRST + seat, world.host_log.items[seat - 1].event.reason);
    }
    coop_world_close(&world);

    /* A kick whose notice stalls three frozen guests: four seats end in one call. */
    net_link_sim_config_t small = {.max_reliable_bytes = 2048, .reliable_queue_bytes = 2100};
    coop_world_open(&world, 34, 4, &small);
    coop_side_config_t four = host_side(5);
    coop_world_host(&world, &four);
    for (int guest = 0; guest < 4; guest++) {
        coop_side_config_t side = guest_side(3400 + (uint64_t)guest, 1);
        coop_world_join(&world, guest, &side);
        coop_world_run(&world, 0.1);
    }
    world.frozen = 0x7;
    for (uint8_t seat = 1; seat <= 3; seat++) {
        fill_queue(seat);
    }
    coop_log_clear(&world.host_log);
    coop_session_kick(world.host, 4, COOP_CLOSE_KICKED);
    coop_world_step(&world);
    TEST_ASSERT_EQUAL_UINT32(4, coop_log_count(&world.host_log, COOP_EVENT_SEAT_LEFT));
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_KICKED, world.host_log.items[0].event.reason);
    for (uint32_t i = 1; i < 4; i++) {
        TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_STALLED, world.host_log.items[i].event.reason);
    }
    world.frozen = 0;
    coop_world_close(&world);
}

// #endregion

// #region identity, leave codes, time, sizes

static void test_identity_zero_never_takes_a_seat_over(void) {
    coop_world_open(&world, 35, 3, NULL);
    coop_side_config_t host = host_side(4);
    coop_world_host(&world, &host);
    coop_side_config_t nameless = guest_side(0, 1);
    coop_world_join(&world, 0, &nameless);
    coop_world_run(&world, 0.2);
    coop_world_join(&world, 1, &nameless);
    coop_world_run(&world, 0.2);
    TEST_ASSERT_EQUAL_UINT8(1, welcomed_seat(0));
    TEST_ASSERT_EQUAL_UINT8(2, welcomed_seat(1));
    TEST_ASSERT_EQUAL_UINT32(0, coop_log_count(&world.guest_log[0], COOP_EVENT_CLOSED));
    coop_world_close(&world);
}

/* A raw guest that takes a seat, then closes with `code`. Returns what the other guest heard. */
static uint32_t leave_with(uint32_t code, uint32_t *host_heard) {
    raw_end_t raw = {.link = net_link_sim_guest(world.sim, 3500 + code)};
    run_with_raw(&raw, 0.1, false);
    uint8_t packet[64];
    coop_wire_msg_t hello = {.kind = COOP_WIRE_HELLO, .protocol = COOP_SESSION_PROTOCOL, .build_key = BUILD};
    uint32_t size = coop_wire_write(&hello, packet, sizeof(packet));
    TEST_ASSERT_EQUAL(NET_LINK_SEND_OK, net_link_send(raw.link, raw.peer, COOP_LANE_RECORDS, NET_LINK_SEND_RELIABLE, packet, size));
    run_with_raw(&raw, 0.1, false);
    TEST_ASSERT_TRUE(coop_session_seated(world.host, 2));
    coop_log_clear(&world.host_log);
    coop_log_clear(&world.guest_log[0]);
    net_link_close(raw.link, raw.peer, code);
    net_link_destroy(raw.link);
    coop_world_run(&world, 0.1);
    const coop_logged_t *left = coop_log_find(&world.host_log, COOP_EVENT_SEAT_LEFT, 2);
    const coop_logged_t *told = coop_log_find(&world.guest_log[0], COOP_EVENT_SEAT_LEFT, 2);
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_NOT_NULL(told);
    *host_heard = left->event.reason;
    return told->event.reason;
}

static void test_a_guest_chooses_only_its_own_leave_code(void) {
    coop_world_open(&world, 36, 3, NULL);
    coop_side_config_t host = host_side(4);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(3600, 1);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.2);
    uint32_t heard = 0;
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_LEFT, leave_with(COOP_CLOSE_KICKED, &heard));
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_KICKED, heard);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_LEFT, leave_with(COOP_CLOSE_HOST_LEFT, &heard));
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_HOST_LEFT, heard);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_GAME_FIRST + 3, leave_with(COOP_CLOSE_GAME_FIRST + 3, &heard));
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_LEFT, leave_with(COOP_CLOSE_LEFT, &heard));
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_LEFT, heard);
    coop_world_close(&world);
}

static void test_a_guest_that_cannot_connect_is_closed(void) {
    coop_world_open(&world, 37, 1, NULL);
    coop_side_config_t side = guest_side(3700, 1);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.2);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_UNREACHABLE, closed_reason(0));
    coop_event_t event;
    TEST_ASSERT_FALSE(coop_session_poll(world.guest[0], &event));
    coop_world_close(&world);
}

static void test_time_never_goes_back(void) {
    coop_world_open(&world, 38, 1, NULL);
    coop_side_config_t host = host_side(2);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(3800, 1);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.5);
    double now = net_link_sim_time(world.sim);
    double before = coop_session_peer_tick(world.host, 1);
    TEST_ASSERT_TRUE(before > 0.0);
    coop_session_set_time(world.host, now - 0.5);
    TEST_ASSERT_TRUE(fabs(coop_session_peer_tick(world.host, 1) - before) < 1e-9);
    coop_session_set_time(world.host, (double)NAN);
    coop_session_set_time(world.host, (double)INFINITY);
    TEST_ASSERT_TRUE(fabs(coop_session_peer_tick(world.host, 1) - before) < 1e-9);
    coop_session_set_time(world.host, now + 0.5);
    TEST_ASSERT_TRUE(coop_session_peer_tick(world.host, 1) > before + 20.0);
    coop_world_close(&world);
}

static void test_a_baseline_respects_the_guest_limit(void) {
    coop_world_open(&world, 39, 1, NULL);
    coop_side_config_t host = host_side(2);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(3900, 1);
    side.baseline_max = 1000;
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.3);
    static uint8_t bytes[2000];
    TEST_ASSERT_EQUAL(COOP_SEND_TOO_LARGE, coop_session_send_baseline(world.host, 1, bytes, 1001));
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_baseline(world.host, 1, bytes, 1000));
    coop_world_run(&world, 0.2);
    TEST_ASSERT_EQUAL_UINT32(1, coop_log_count(&world.guest_log[0], COOP_EVENT_BASELINE));
    TEST_ASSERT_EQUAL_UINT32(0, coop_log_count(&world.guest_log[0], COOP_EVENT_CLOSED));
    coop_world_close(&world);
}

// #endregion

static void test_the_waiting_table_pushes_out_the_oldest(void) {
    static coop_pending_t table[COOP_PENDING_MAX];
    memset(table, 0, sizeof(table));
    for (uint32_t i = 0; i < COOP_PENDING_MAX; i++) {
        TEST_ASSERT_EQUAL_UINT32(NET_LINK_PEER_NONE, coop_pending_admit(table, 10 + i, 500 + i, 1.0 + i));
    }
    /* Full: the oldest gives way, whichever slot it sits in. */
    table[7].since_s = 0.5;
    TEST_ASSERT_EQUAL_UINT32(17, coop_pending_admit(table, 99, 999, 50.0));
    TEST_ASSERT_EQUAL_INT(7, coop_pending_find(table, 99));
    TEST_ASSERT_EQUAL_INT(-1, coop_pending_find(table, 17));
    /* One entry per identity: the newer connection replaces the older. */
    TEST_ASSERT_EQUAL_UINT32(12, coop_pending_admit(table, 100, 502, 51.0));
    TEST_ASSERT_EQUAL_INT(2, coop_pending_find(table, 100));
    /* Identity zero names nobody: two of them are two peers. */
    memset(table, 0, sizeof(table));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_PEER_NONE, coop_pending_admit(table, 1, 0, 1.0));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_PEER_NONE, coop_pending_admit(table, 2, 0, 1.0));
    TEST_ASSERT_TRUE(coop_pending_find(table, 1) >= 0 && coop_pending_find(table, 2) >= 0);
    TEST_ASSERT_EQUAL_INT(-1, coop_pending_find(table, NET_LINK_PEER_NONE));
}

static void test_two_nameless_guests_in_one_tick_both_take_a_seat(void) {
    coop_world_open(&world, 40, 3, NULL);
    coop_side_config_t host = host_side(4);
    coop_world_host(&world, &host);
    coop_side_config_t nameless = guest_side(0, 1);
    coop_world_join(&world, 0, &nameless);
    coop_world_join(&world, 1, &nameless);
    coop_world_run(&world, 0.3);
    uint32_t seats = (1U << welcomed_seat(0)) | (1U << welcomed_seat(1));
    TEST_ASSERT_EQUAL_HEX32(0x6, seats);
    coop_world_close(&world);
}

/* The worst the event queue holds: a takeover whose notices close six stalled guests, then a
   kick while those are being handed out. */
static void test_a_takeover_stall_cascade_and_a_kick(void) {
    net_link_sim_config_t small = {.max_reliable_bytes = 2048, .reliable_queue_bytes = 2100};
    coop_world_open(&world, 41, 8, &small);
    coop_side_config_t host = host_side(8);
    coop_world_host(&world, &host);
    for (int guest = 0; guest < 7; guest++) {
        coop_side_config_t side = guest_side(4100 + (uint64_t)guest, 1);
        coop_world_join(&world, guest, &side);
        coop_world_run(&world, 0.1);
    }
    world.frozen = 0x7E;
    for (uint8_t seat = 2; seat <= 7; seat++) {
        fill_queue(seat);
    }
    coop_log_clear(&world.host_log);
    /* The same player connects again while its first connection still sits in seat 1. */
    coop_side_config_t again = guest_side(4100, 1);
    coop_world_join(&world, 7, &again);
    /* Step until the takeover lands, then kick seat 1 after the first event is out. */
    coop_event_t event;
    bool kicked = false;
    for (int step = 0; step < 30 && !kicked; step++) {
        net_link_sim_advance(world.sim, world.dt);
        double now = net_link_sim_time(world.sim);
        coop_session_set_time(world.host, now);
        while (coop_session_poll(world.host, &event)) {
            coop_logged_t *item = &world.host_log.items[world.host_log.count++];
            item->event = event;
            if (!kicked && event.type == COOP_EVENT_SEAT_LEFT && event.reason == COOP_CLOSE_REPLACED) {
                coop_session_kick(world.host, 1, COOP_CLOSE_KICKED);
                kicked = true;
            }
        }
        coop_session_end_tick(world.host);
        coop_session_set_time(world.guest[7], now);
        while (coop_session_poll(world.guest[7], &event)) {
        }
        coop_session_end_tick(world.guest[7]);
    }
    TEST_ASSERT_TRUE(kicked);
    TEST_ASSERT_EQUAL_UINT32(9, world.host_log.count);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_REPLACED, world.host_log.items[0].event.reason);
    for (uint32_t i = 1; i <= 6; i++) {
        TEST_ASSERT_EQUAL_INT(COOP_EVENT_SEAT_LEFT, world.host_log.items[i].event.type);
        TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_STALLED, world.host_log.items[i].event.reason);
    }
    TEST_ASSERT_EQUAL_INT(COOP_EVENT_SEAT_JOINED, world.host_log.items[7].event.type);
    TEST_ASSERT_EQUAL_UINT8(1, world.host_log.items[7].event.seat);
    TEST_ASSERT_EQUAL_INT(COOP_EVENT_SEAT_LEFT, world.host_log.items[8].event.type);
    TEST_ASSERT_EQUAL_UINT32(COOP_CLOSE_KICKED, world.host_log.items[8].event.reason);
    world.frozen = 0;
    coop_world_close(&world);
}

static void test_records_refused_before_a_baseline_are_not_counted(void) {
    net_link_sim_config_t small = {.max_reliable_bytes = 2048, .reliable_queue_bytes = 2100};
    coop_world_open(&world, 42, 1, &small);
    coop_side_config_t host = host_side(2);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(4200, 1);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.3);
    world.frozen = 1;
    fill_queue(1); /* twenty records taken, then one FULL */
    static uint8_t record[100];
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(COOP_SEND_FULL, coop_session_send_record(world.host, 1, record, sizeof(record)));
    }
    uint8_t bytes[64] = {0};
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_baseline(world.host, 1, bytes, sizeof(bytes)));
    coop_log_clear(&world.guest_log[0]);
    world.frozen = 0;
    coop_world_run(&world, 0.5);
    const coop_logged_t *baseline = coop_log_find(&world.guest_log[0], COOP_EVENT_BASELINE, 0);
    TEST_ASSERT_NOT_NULL(baseline);
    TEST_ASSERT_EQUAL_UINT32(20, baseline->event.records_before);
    TEST_ASSERT_EQUAL_UINT32(20, coop_log_count(&world.guest_log[0], COOP_EVENT_RECORD));
    coop_world_close(&world);
}

static void test_close_names(void) {
    const uint32_t codes[] = {COOP_CLOSE_LEFT, COOP_CLOSE_HOST_LEFT, COOP_CLOSE_VERSION, COOP_CLOSE_BUILD, COOP_CLOSE_WORLD, COOP_CLOSE_FULL, COOP_CLOSE_SHUT, COOP_CLOSE_FORMAT, COOP_CLOSE_HANDSHAKE, COOP_CLOSE_KICKED, COOP_CLOSE_REPLACED, COOP_CLOSE_STALLED, NET_LINK_REASON_NONE, NET_LINK_REASON_REMOTE_CLOSE, NET_LINK_REASON_TIMEOUT, NET_LINK_REASON_REFUSED, NET_LINK_REASON_UNREACHABLE, NET_LINK_REASON_TRANSPORT};
    size_t count = sizeof(codes) / sizeof(codes[0]);
    for (size_t i = 0; i < count; i++) {
        const char *name = coop_close_name(codes[i]);
        TEST_ASSERT_NOT_NULL(name);
        TEST_ASSERT_TRUE(strcmp(name, "unknown") != 0 && strcmp(name, "game") != 0 && name[0] != '\0');
        for (size_t j = 0; j < i; j++) {
            TEST_ASSERT_TRUE_MESSAGE(strcmp(name, coop_close_name(codes[j])) != 0, "two codes share a name");
        }
    }
    TEST_ASSERT_EQUAL_STRING("game", coop_close_name(COOP_CLOSE_GAME_FIRST));
    TEST_ASSERT_EQUAL_STRING("game", coop_close_name(COOP_CLOSE_GAME_LAST));
    TEST_ASSERT_EQUAL_STRING("unknown", coop_close_name(COOP_CLOSE_GAME_LAST + 1));
    TEST_ASSERT_EQUAL_STRING("unknown", coop_close_name(1000));
    TEST_ASSERT_EQUAL_STRING("unknown", coop_close_name(1013));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_four_seats_and_the_roster);
    RUN_TEST(test_guests_that_join_together_take_distinct_seats);
    RUN_TEST(test_refusals_name_the_reason);
    RUN_TEST(test_a_new_world_key_reaches_later_guests);
    RUN_TEST(test_the_same_player_takes_its_seat_over);
    RUN_TEST(test_kick_closes_with_the_game_code);
    RUN_TEST(test_a_shut_host_still_seats_a_returning_player);
    RUN_TEST(test_host_destroy_closes_every_guest_with_host_left);
    RUN_TEST(test_guest_destroy_is_a_left_seat);
    RUN_TEST(test_a_cut_path_times_out);
    RUN_TEST(test_no_hello_in_time_closes_with_handshake);
    RUN_TEST(test_nothing_leaves_without_end_tick);
    RUN_TEST(test_size_limits);
    RUN_TEST(test_one_baseline_at_a_time);
    RUN_TEST(test_calls_a_role_does_not_have);
    RUN_TEST(test_configs_out_of_range_are_refused);
    RUN_TEST(test_waiting_peers_never_turn_an_honest_guest_away);
    RUN_TEST(test_a_guest_that_stops_reading_is_closed_with_stalled);
    RUN_TEST(test_the_event_queue_holds_its_worst_case);
    RUN_TEST(test_identity_zero_never_takes_a_seat_over);
    RUN_TEST(test_a_guest_chooses_only_its_own_leave_code);
    RUN_TEST(test_a_guest_that_cannot_connect_is_closed);
    RUN_TEST(test_time_never_goes_back);
    RUN_TEST(test_a_baseline_respects_the_guest_limit);
    RUN_TEST(test_the_waiting_table_pushes_out_the_oldest);
    RUN_TEST(test_two_nameless_guests_in_one_tick_both_take_a_seat);
    RUN_TEST(test_a_takeover_stall_cascade_and_a_kick);
    RUN_TEST(test_records_refused_before_a_baseline_are_not_counted);
    RUN_TEST(test_close_names);
    return UNITY_END();
}
