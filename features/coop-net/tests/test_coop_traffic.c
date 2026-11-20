#include <stdlib.h>

#include "coop_acks.h"
#include "coop_clock.h"
#include "coop_test_world.h"
#include "coop_wire.h"
#include "unity.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static coop_world_t world;

// #region helpers

enum { BUILD = 7 };

static coop_side_config_t host_side(uint32_t seats, uint32_t baseline_max) { return (coop_side_config_t){.seats = seats, .build_key = BUILD, .baseline_max = baseline_max}; }

static coop_side_config_t guest_side(uint64_t identity, uint32_t baseline_max) { return (coop_side_config_t){.identity = identity, .player_key = identity, .build_key = BUILD, .baseline_max = baseline_max}; }

static void put_u32(uint8_t *out, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        out[i] = (uint8_t)(value >> (8 * i));
    }
}

static uint32_t get_u32(const uint8_t *in) { return (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24); }

static bool in_band(double value, double low, double high) { return isfinite(value) && value >= low && value <= high; }

static const net_link_conditions_t ROUGH = {.delay_ms = 30, .jitter_ms = 20, .loss = 0.05F, .duplicate = 0.05F, .reorder = 0.05F, .reorder_ms = 40};

// #endregion

// #region acknowledgement units

static void test_receive_window_places_duplicates_reorder_and_wrap(void) {
    coop_recv_window_t window;
    coop_recv_window_reset(&window);
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 65530, 100, 0.0));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_AGAIN, coop_recv_window_accept(&window, 65530, 100, 0.0));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 3, 100, 0.25)); /* across the wrap */
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 65535, 100, 0.3));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_AGAIN, coop_recv_window_accept(&window, 65535, 100, 0.3));
    bool has_ack = false;
    uint16_t ack = 0;
    uint32_t mask = 0;
    uint16_t wait = 0;
    coop_recv_window_ack(&window, 0.5, &has_ack, &ack, &mask, &wait);
    TEST_ASSERT_TRUE(has_ack);
    TEST_ASSERT_EQUAL_UINT16(3, ack);
    /* 65535 is 4 behind 3 (bit 3), 65530 is 9 behind (bit 8). */
    TEST_ASSERT_EQUAL_HEX32((1U << 3) | (1U << 8), mask);
    TEST_ASSERT_EQUAL_UINT16(1000, wait); /* 0.25 s in quarter milliseconds */
    coop_recv_window_ack(&window, 0.25 + 0.00076, &has_ack, &ack, &mask, &wait);
    TEST_ASSERT_EQUAL_UINT16(3, wait); /* below a millisecond the quarters still count */
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 35, 100, 0.5));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_AGAIN, coop_recv_window_accept(&window, 3, 100, 0.5)); /* 32 behind: the mask's last bit */
    TEST_ASSERT_EQUAL_INT(COOP_RECV_OLD, coop_recv_window_accept(&window, 2, 100, 0.5));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 4, 100, 0.5));
    coop_recv_window_ack(&window, 0.5, &has_ack, &ack, &mask, &wait);
    TEST_ASSERT_EQUAL_UINT16(35, ack);
    TEST_ASSERT_EQUAL_HEX32((1U << 31) | (1U << 30), mask);
    TEST_ASSERT_EQUAL_UINT16(0, wait);
    /* A jump past the whole mask forgets it. */
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 35 + 40, 100, 0.5));
    coop_recv_window_ack(&window, 100.0, &has_ack, &ack, &mask, &wait);
    TEST_ASSERT_EQUAL_HEX32(0, mask);
    TEST_ASSERT_EQUAL_UINT16(0xFFFF, wait);
    coop_recv_window_reset(&window);
    coop_recv_window_ack(&window, 1.0, &has_ack, &ack, &mask, &wait);
    TEST_ASSERT_FALSE(has_ack);
    TEST_ASSERT_TRUE(ack == 0 && mask == 0 && wait == 0);
}

static void test_send_window_fates_and_samples(void) {
    coop_send_window_t *window = malloc(sizeof(*window));
    TEST_ASSERT_NOT_NULL(window);
    coop_send_window_reset(window);
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_LOST, coop_send_window_fate(window, 0));
    /* Start just before the wrap. */
    window->next = 65520;
    for (int i = 0; i < 40; i++) {
        coop_send_window_push(window, 1.0 + 0.01 * i);
    }
    uint16_t newest = (uint16_t)(65520 + 39);
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_PENDING, coop_send_window_fate(window, newest));
    /* An ack for a number never sent is ignored. */
    TEST_ASSERT_TRUE(coop_send_window_ack(window, (uint16_t)(newest + 1), 0xFFFFFFFFU, 0, 2.0) < 0.0);
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_PENDING, coop_send_window_fate(window, newest));
    /* newest named, the mask names all of the 32 before but 5 and 20 back. */
    uint32_t mask = 0xFFFFFFFFU & ~(1U << 4) & ~(1U << 19);
    double sample = coop_send_window_ack(window, newest, mask, 400, 1.6);
    TEST_ASSERT_TRUE(in_band(sample, 0.11 - 1e-9, 0.11 + 1e-9));
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_ARRIVED, coop_send_window_fate(window, newest));
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_PENDING, coop_send_window_fate(window, (uint16_t)(newest - 5)));
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_PENDING, coop_send_window_fate(window, (uint16_t)(newest - 20)));
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_ARRIVED, coop_send_window_fate(window, (uint16_t)(newest - 32)));
    /* More than 32 behind the newest acknowledged: never to be named again. */
    for (int back = 33; back < 40; back++) {
        TEST_ASSERT_EQUAL_INT(COOP_PACKET_LOST, coop_send_window_fate(window, (uint16_t)(newest - back)));
    }
    /* A late, older acknowledgement may still name a pending packet; a sample comes once. */
    TEST_ASSERT_TRUE(coop_send_window_ack(window, (uint16_t)(newest - 4), 1U << 0, 0, 1.7) < 0.0);
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_ARRIVED, coop_send_window_fate(window, (uint16_t)(newest - 5)));
    TEST_ASSERT_TRUE(coop_send_window_ack(window, newest, mask, 0, 1.8) < 0.0);
    /* ...but never one already lost. */
    coop_send_window_ack(window, (uint16_t)(newest - 4), 0xFFFFFFFFU, 0, 1.9);
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_LOST, coop_send_window_fate(window, (uint16_t)(newest - 36)));
    float loss = coop_send_window_loss(window);
    TEST_ASSERT_TRUE(loss > 0.0F && loss < 0.3F);
    /* Past the window everything answers LOST. */
    for (int i = 0; i < COOP_FATE_WINDOW; i++) {
        coop_send_window_push(window, 3.0);
    }
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_LOST, coop_send_window_fate(window, newest));
    TEST_ASSERT_TRUE(coop_send_window_loss(window) < 0.0F);
    TEST_ASSERT_TRUE(coop_send_window_ack(window, newest, 0, 0, 3.0) < 0.0);
    free(window);
}

static void test_rtt_smoothing(void) {
    coop_rtt_t rtt;
    coop_rtt_reset(&rtt);
    coop_rtt_sample(&rtt, -1.0);
    coop_rtt_sample(&rtt, (double)NAN);
    TEST_ASSERT_EQUAL_UINT32(0, rtt.samples);
    coop_rtt_sample(&rtt, 0.1);
    TEST_ASSERT_TRUE(in_band(rtt.smoothed, 0.1, 0.1) && in_band(rtt.deviation, 0.05, 0.05));
    coop_rtt_sample(&rtt, 0.2);
    TEST_ASSERT_TRUE(in_band(rtt.smoothed, 0.1125 - 1e-12, 0.1125 + 1e-12));
    TEST_ASSERT_TRUE(in_band(rtt.deviation, 0.0625 - 1e-12, 0.0625 + 1e-12));
}

static void test_loss_counts_resolved_packets_only(void) {
    coop_send_window_t *window = malloc(sizeof(*window));
    TEST_ASSERT_NOT_NULL(window);
    coop_send_window_reset(window);
    for (int i = 0; i < 50; i++) {
        coop_send_window_push(window, 0.0);
    }
    /* 49 arrived; 0..16 are more than 32 behind it and lost; 17..48 still pending. */
    coop_send_window_ack(window, 49, 0, 0, 0.1);
    TEST_ASSERT_TRUE(in_band((double)coop_send_window_loss(window), 17.0 / 18.0 - 1e-6, 17.0 / 18.0 + 1e-6));
    free(window);
}

static void test_send_window_after_a_long_silence(void) {
    coop_send_window_t *window = malloc(sizeof(*window));
    TEST_ASSERT_NOT_NULL(window);
    coop_send_window_reset(window);
    for (int i = 0; i < 10; i++) {
        coop_send_window_push(window, 0.0);
    }
    coop_send_window_ack(window, 9, 0, 0, 0.1);
    /* More than 32767 packets with no word back: the old newest reads as newer than the fresh one. */
    for (int i = 0; i < 33000; i++) {
        coop_send_window_push(window, 1.0);
    }
    uint16_t newest = (uint16_t)(window->next - 1U);
    coop_send_window_ack(window, (uint16_t)(newest - 1U), 0, 0, 1.1);
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_ARRIVED, coop_send_window_fate(window, (uint16_t)(newest - 1U)));
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_PENDING, coop_send_window_fate(window, newest));
    for (uint16_t back = 2; back <= 33; back++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(COOP_PACKET_PENDING, coop_send_window_fate(window, (uint16_t)(newest - back)), "a packet the peer may still name read LOST");
    }
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_LOST, coop_send_window_fate(window, (uint16_t)(newest - 40U)));
    free(window);
}

static void test_receive_window_restarts_only_for_a_newer_stamp(void) {
    coop_recv_window_t window;
    coop_recv_window_reset(&window);
    /* Three packets a tick: one stamp covers several numbers. */
    for (uint16_t n = 0; n <= 29; n++) {
        TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, n, 1000U + n / 3U, 0.01 * n));
    }
    for (uint16_t n = 30; n <= 50; n++) {
        TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, n, 1000U + n / 3U, 0.01 * n));
    }
    /* A late copy of an early packet carries its old stamp: dropped, however late it comes. */
    TEST_ASSERT_EQUAL_INT(COOP_RECV_OLD, coop_recv_window_accept(&window, 2, 1000, 0.6));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_OLD, coop_recv_window_accept(&window, 2, 1000, 600.0));
    /* A copy stamped in the newest tick taken is not newer either. */
    TEST_ASSERT_EQUAL_INT(COOP_RECV_OLD, coop_recv_window_accept(&window, 10, 1016, 600.0));
    /* The sender went on 40000 packets while nothing arrived: its numbers read as behind, its
       stamp as newer, and the window starts again from it at once. */
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 40050, 14350, 1.0));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 40051, 14350, 1.0));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_AGAIN, coop_recv_window_accept(&window, 40050, 14350, 1.0));
    /* Copies from before the restart stay out. */
    TEST_ASSERT_EQUAL_INT(COOP_RECV_OLD, coop_recv_window_accept(&window, 49, 1016, 1.0));
    bool has_ack = false;
    uint16_t ack = 0;
    uint32_t mask = 0;
    uint16_t wait = 0;
    coop_recv_window_ack(&window, 1.0, &has_ack, &ack, &mask, &wait);
    TEST_ASSERT_EQUAL_UINT16(40051, ack);
    TEST_ASSERT_EQUAL_HEX32(1, mask);
    /* Stamps compare across the 32-bit wrap. */
    coop_recv_window_reset(&window);
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 0, 0xFFFFFFF0U, 0.0));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_NEW, coop_recv_window_accept(&window, 40000, 0x10U, 0.0));
    TEST_ASSERT_EQUAL_INT(COOP_RECV_OLD, coop_recv_window_accept(&window, 1, 0xFFFFFFF0U, 0.0));
}

static void test_clock_follows_the_least_delayed_packet(void) {
    coop_clock_t clock;
    coop_clock_reset(&clock, 0);
    coop_clock_sample(&clock, 5, 1.0);
    TEST_ASSERT_TRUE(coop_clock_estimate(&clock, 1.0) < 0.0); /* no rate */
    coop_clock_reset(&clock, 60);
    TEST_ASSERT_TRUE(coop_clock_estimate(&clock, 1.0) < 0.0); /* no sample */
    /* In one bucket: tick 600 polled at 10.0, then tick 599 polled late at 10.01. */
    coop_clock_sample(&clock, 600, 10.0);
    coop_clock_sample(&clock, 599, 10.01);
    TEST_ASSERT_TRUE(in_band(coop_clock_estimate(&clock, 10.02), 601.2 - 1e-6, 601.2 + 1e-6));
    /* The path got slower: the faster bucket still leads within its window, then ages out. */
    coop_clock_sample(&clock, 620, 10.5);
    TEST_ASSERT_TRUE(in_band(coop_clock_estimate(&clock, 10.5), 630.0 - 1e-6, 630.0 + 1e-6));
    TEST_ASSERT_TRUE(in_band(coop_clock_estimate(&clock, 11.2), 662.0 - 1e-6, 662.0 + 1e-6));
}

static void test_clock_runs_on_through_a_pause_then_steps_back(void) {
    coop_clock_t clock;
    coop_clock_reset(&clock, 60);
    for (int i = 0; i < 120; i++) {
        coop_clock_sample(&clock, 1000U + (uint32_t)i, i / 60.0);
    }
    double last = 119.0 / 60.0;
    /* The sender stops for 3 s: the value runs on at the rate. */
    TEST_ASSERT_TRUE(in_band(coop_clock_estimate(&clock, last + 3.0), 1119.0 + 180.0 - 1e-6, 1119.0 + 180.0 + 1e-6));
    /* It resumes from where it stopped: once its packets lead, the value steps back by the pause. */
    for (int i = 1; i <= 60; i++) {
        coop_clock_sample(&clock, 1119U + (uint32_t)i, last + 3.0 + i / 60.0);
    }
    TEST_ASSERT_TRUE(in_band(coop_clock_estimate(&clock, last + 4.0), 1179.0 - 1e-6, 1179.0 + 1e-6));
}

static void test_clock_reads_ticks_across_the_wrap(void) {
    coop_clock_t clock;
    coop_clock_reset(&clock, 60);
    coop_clock_sample(&clock, 0xFFFFFFF0U, 0.0);
    coop_clock_sample(&clock, 0x0000000FU, 31.0 / 60.0);
    TEST_ASSERT_TRUE(in_band(coop_clock_estimate(&clock, 31.0 / 60.0), 4294967311.0 - 1e-6, 4294967311.0 + 1e-6));
    coop_clock_sample(&clock, 0x0000004BU, 91.0 / 60.0);
    TEST_ASSERT_TRUE(in_band(coop_clock_estimate(&clock, 91.0 / 60.0), 4294967371.0 - 1e-6, 4294967371.0 + 1e-6));
}

// #endregion

// #region records

typedef struct record_run {
    bool sending;
    uint32_t sent_to_guest[COOP_SEATS_MAX];
    uint32_t got_from_host[COOP_WORLD_GUESTS];
    uint32_t sent_to_host[COOP_WORLD_GUESTS];
    uint32_t got_from_guest[COOP_SEATS_MAX];
    uint32_t full;
} record_run_t;

static void record_tick(void *context, coop_world_t *w, int side) {
    record_run_t *run = context;
    if (!run->sending) {
        return;
    }
    uint8_t payload[8];
    if (side == COOP_WORLD_HOST) {
        for (uint8_t seat = 1; seat < 4; seat++) {
            for (int burst = 0; burst < 2; burst++) {
                put_u32(payload, seat);
                put_u32(payload + 4, run->sent_to_guest[seat]);
                coop_send_result_t result = coop_session_send_record(w->host, seat, payload, sizeof(payload));
                run->sent_to_guest[seat] += result == COOP_SEND_OK ? 1U : 0U;
                run->full += result == COOP_SEND_FULL ? 1U : 0U;
            }
        }
        return;
    }
    put_u32(payload, (uint32_t)side);
    put_u32(payload + 4, run->sent_to_host[side]);
    coop_send_result_t result = coop_session_send_record(w->guest[side], COOP_SEAT_HOST, payload, sizeof(payload));
    run->sent_to_host[side] += result == COOP_SEND_OK ? 1U : 0U;
}

static void record_event(void *context, coop_world_t *w, int side, const coop_event_t *event) {
    record_run_t *run = context;
    if (event->type != COOP_EVENT_RECORD) {
        return;
    }
    TEST_ASSERT_EQUAL_UINT32(8, event->size);
    if (side == COOP_WORLD_HOST) {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run->got_from_guest[event->seat], get_u32(event->data + 4), "record out of order or twice");
        run->got_from_guest[event->seat]++;
    } else {
        TEST_ASSERT_EQUAL_UINT8(COOP_SEAT_HOST, event->seat);
        TEST_ASSERT_EQUAL_UINT32(coop_session_seat(w->guest[side]), get_u32(event->data));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(run->got_from_host[side], get_u32(event->data + 4), "record out of order or twice");
        run->got_from_host[side]++;
    }
}

static void test_records_arrive_once_and_in_order_under_loss(void) {
    static record_run_t run;
    memset(&run, 0, sizeof(run));
    coop_world_open(&world, 31, 3, NULL);
    coop_side_config_t host = host_side(4, 0);
    coop_world_host(&world, &host);
    for (int guest = 0; guest < 3; guest++) {
        coop_side_config_t side = guest_side(201 + (uint64_t)guest, 0);
        coop_world_join(&world, guest, &side);
        coop_world_conditions(&world, guest, &ROUGH, &ROUGH);
    }
    coop_world_run(&world, 1.0);
    uint32_t seats = 0;
    for (int guest = 0; guest < 3; guest++) {
        seats |= 1U << coop_session_seat(world.guest[guest]);
    }
    TEST_ASSERT_EQUAL_HEX32(0xE, seats);
    world.context = &run;
    world.on_tick = record_tick;
    world.on_event = record_event;
    run.sending = true;
    coop_world_run(&world, 25.0);
    run.sending = false;
    coop_world_run(&world, 3.0);
    for (int guest = 0; guest < 3; guest++) {
        uint8_t seat = coop_session_seat(world.guest[guest]);
        TEST_ASSERT_TRUE(run.sent_to_guest[seat] >= 2900 && run.sent_to_host[guest] >= 1400);
        TEST_ASSERT_EQUAL_UINT32(run.sent_to_guest[seat], run.got_from_host[guest]);
        TEST_ASSERT_EQUAL_UINT32(run.sent_to_host[guest], run.got_from_guest[seat]);
        TEST_ASSERT_EQUAL_UINT32(0, coop_log_count(&world.guest_log[guest], COOP_EVENT_CLOSED));
    }
    TEST_ASSERT_EQUAL_UINT32(0, run.full);
    coop_world_close(&world);
}

// #endregion

// #region fates

enum { FATE_PACKETS = 70500 };

typedef struct fate_run {
    uint32_t sent;
    uint16_t number[FATE_PACKETS];
    uint8_t delivered[FATE_PACKETS];
    uint8_t seen[FATE_PACKETS]; /* coop_packet_fate_t once it left PENDING, else 0 */
    uint32_t delivered_count;
    uint32_t out_of_order;
    uint32_t newest_delivered;
} fate_run_t;

static void fate_tick(void *context, coop_world_t *w, int side) {
    fate_run_t *run = context;
    uint8_t payload[4];
    if (side != COOP_WORLD_HOST) {
        put_u32(payload, 0);
        coop_session_send_state(w->guest[side], COOP_SEAT_HOST, payload, sizeof(payload), NULL);
        return;
    }
    /* Once the game stops, the session's empty packets move the window on: fates are read
       only while the game's packets are the newest. */
    if (run->sent >= FATE_PACKETS) {
        return;
    }
    {
        put_u32(payload, run->sent);
        uint16_t number = 0;
        coop_send_result_t result = coop_session_send_state(w->host, 1, payload, sizeof(payload), &number);
        TEST_ASSERT_TRUE(result == COOP_SEND_OK || result == COOP_SEND_DROPPED);
        if (run->sent > 0) {
            TEST_ASSERT_EQUAL_UINT16((uint16_t)(run->number[0] + run->sent), number); /* empty packets before took numbers too */
        }
        run->number[run->sent++] = number;
    }
    uint32_t window = run->sent < COOP_FATE_WINDOW ? run->sent : COOP_FATE_WINDOW;
    for (uint32_t age = 0; age < window; age++) {
        uint32_t id = run->sent - 1 - age;
        coop_packet_fate_t fate = coop_session_packet_fate(w->host, 1, run->number[id]);
        if (fate == COOP_PACKET_PENDING) {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, run->seen[id], "a resolved packet went back to pending");
            continue;
        }
        if (fate == COOP_PACKET_ARRIVED) {
            TEST_ASSERT_TRUE_MESSAGE(run->delivered[id], "ARRIVED but never delivered");
        }
        TEST_ASSERT_TRUE_MESSAGE(run->seen[id] == 0 || run->seen[id] == (uint8_t)fate, "a fate changed between ARRIVED and LOST");
        run->seen[id] = (uint8_t)fate;
    }
}

static void fate_event(void *context, coop_world_t *w, int side, const coop_event_t *event) {
    fate_run_t *run = context;
    if (side == COOP_WORLD_HOST || event->type != COOP_EVENT_STATE) {
        return;
    }
    TEST_ASSERT_EQUAL_UINT32(4, event->size);
    uint32_t id = get_u32(event->data);
    TEST_ASSERT_TRUE(id < run->sent);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, run->delivered[id], "a state packet came twice");
    run->delivered[id] = 1;
    run->delivered_count++;
    if (run->delivered_count > 1 && id < run->newest_delivered) {
        run->out_of_order++;
    }
    if (id > run->newest_delivered) {
        run->newest_delivered = id;
    }
}

static void test_fates_hold_under_loss_reorder_and_wrap(void) {
    static fate_run_t run;
    memset(&run, 0, sizeof(run));
    coop_world_open(&world, 41, 1, NULL);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(301, 0);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t path = {.delay_ms = 20, .jitter_ms = 10, .loss = 0.05F, .duplicate = 0.05F, .reorder = 0.05F, .reorder_ms = 30};
    coop_world_conditions(&world, 0, &path, &path);
    coop_world_run(&world, 0.5);
    world.context = &run;
    world.on_tick = fate_tick;
    world.on_event = fate_event;
    while (run.sent < FATE_PACKETS) {
        coop_world_step(&world);
    }
    coop_world_run(&world, 1.0);
    uint32_t arrived = 0;
    uint32_t lost = 0;
    for (uint32_t id = 0; id + COOP_FATE_WINDOW < run.sent; id++) {
        if (run.delivered[id]) {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(COOP_PACKET_ARRIVED, run.seen[id], "delivered, yet never ARRIVED");
        } else {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(COOP_PACKET_LOST, run.seen[id], "never delivered, yet not LOST");
        }
        arrived += run.seen[id] == COOP_PACKET_ARRIVED ? 1U : 0U;
        lost += run.seen[id] == COOP_PACKET_LOST ? 1U : 0U;
    }
    double share = (double)lost / (double)(arrived + lost);
    TEST_ASSERT_TRUE_MESSAGE(in_band(share, 0.02, 0.12), "lost share far from the path's loss");
    TEST_ASSERT_TRUE(run.out_of_order > 100);
    coop_seat_stats_t stats;
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.guest[0], COOP_SEAT_HOST, &stats));
    TEST_ASSERT_EQUAL_UINT32(run.delivered_count, stats.state_received);
    TEST_ASSERT_TRUE(stats.state_stale > 1000); /* the duplicates */
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.host, 1, &stats));
    TEST_ASSERT_EQUAL_UINT32(FATE_PACKETS, stats.state_sent);
    TEST_ASSERT_TRUE(in_band((double)stats.loss, 0.0, 0.2));
    coop_world_close(&world);
}

// #endregion

// #region round trip and peer tick

/* A side's ticks over time, so a test can ask what tick it was at any moment. */
typedef struct tick_history {
    double at[4096];
    uint32_t tick[4096];
    uint32_t count;
    double hz;
} tick_history_t;

static void history_add(tick_history_t *history, double at, uint32_t tick) {
    if (history->count < 4096) {
        history->at[history->count] = at;
        history->tick[history->count] = tick;
        history->count++;
    }
}

/* The side's fractional tick at `at`: its last frame at or before, carried forward at its rate. */
static double history_at(const tick_history_t *history, double at) {
    for (uint32_t i = history->count; i > 0; i--) {
        if (history->at[i - 1] <= at + 1e-9) {
            return (double)history->tick[i - 1] + (at - history->at[i - 1]) * history->hz;
        }
    }
    return -1.0;
}

typedef struct clock_band {
    double low;
    double high;
    uint32_t samples;
} clock_band_t;

static void band_add(clock_band_t *band, double error) {
    if (band->samples == 0 || error < band->low) {
        band->low = error;
    }
    if (band->samples == 0 || error > band->high) {
        band->high = error;
    }
    band->samples++;
}

/* Host and guest step at their own rates in one virtual time; `host_hz` is how often the host
   really steps (it always declares 60). Each side's peer tick is compared with the other
   side's tick one path delay ago. */
static void run_clocks(double host_hz, double guest_hz, const net_link_conditions_t *path, uint64_t seed, double seconds, clock_band_t *guest_view, clock_band_t *host_view) {
    static tick_history_t host_ticks;
    static tick_history_t guest_ticks;
    memset(&host_ticks, 0, sizeof(host_ticks));
    memset(&guest_ticks, 0, sizeof(guest_ticks));
    host_ticks.hz = host_hz;
    guest_ticks.hz = guest_hz;
    *guest_view = (clock_band_t){0};
    *host_view = (clock_band_t){0};
    coop_world_open(&world, seed, 1, NULL);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(501, 0);
    coop_world_join(&world, 0, &side);
    coop_world_conditions(&world, 0, path, path);
    double delay_s = (double)path->delay_ms / 1000.0;
    double host_next = 0.0;
    double guest_next = 0.0;
    uint8_t payload[16] = {0};
    coop_event_t event;
    while (host_next < seconds) {
        double next = host_next < guest_next ? host_next : guest_next;
        double now = net_link_sim_time(world.sim);
        if (next > now) {
            net_link_sim_advance(world.sim, next - now);
        }
        now = net_link_sim_time(world.sim);
        bool host_turn = host_next <= guest_next;
        coop_session_t *session = host_turn ? world.host : world.guest[0];
        coop_session_set_time(session, now);
        while (coop_session_poll(session, &event)) {
        }
        uint8_t peer = host_turn ? 1 : COOP_SEAT_HOST;
        if (now > 3.0) {
            double expected = history_at(host_turn ? &guest_ticks : &host_ticks, now - delay_s);
            double seen = coop_session_peer_tick(session, peer);
            TEST_ASSERT_TRUE(seen >= 0.0 && expected >= 0.0);
            band_add(host_turn ? host_view : guest_view, seen - expected);
        }
        history_add(host_turn ? &host_ticks : &guest_ticks, now, coop_session_tick(session));
        coop_session_send_state(session, peer, payload, sizeof(payload), NULL);
        coop_session_end_tick(session);
        if (host_turn) {
            host_next += 1.0 / host_hz;
        } else {
            guest_next += 1.0 / guest_hz;
        }
    }
    coop_world_close(&world);
}

static void check_stats(const coop_seat_stats_t *stats, double truth_ms, double jitter_ms) {
    double frame_ms = 1000.0 / 60.0;
    /* Each one-way delay rounds up to the next poll: up to a frame each way above the path. */
    TEST_ASSERT_TRUE_MESSAGE(in_band((double)stats->rtt_ms, truth_ms - 1.0, truth_ms + jitter_ms + 2.0 * frame_ms + 4.0), "round trip out of band");
    TEST_ASSERT_TRUE(in_band((double)stats->rtt_jitter_ms, 0.0, jitter_ms + 2.0 * frame_ms));
    TEST_ASSERT_TRUE(in_band((double)stats->loss, 0.0, 0.0));
    TEST_ASSERT_TRUE(stats->state_sent > 0 && stats->state_received > 0);
}

static void clock_tick(void *context, coop_world_t *w, int side) {
    uint8_t payload[16] = {0};
    (void)context;
    if (side == COOP_WORLD_HOST) {
        coop_session_send_state(w->host, 1, payload, sizeof(payload), NULL);
    } else {
        coop_session_send_state(w->guest[side], COOP_SEAT_HOST, payload, sizeof(payload), NULL);
    }
}

static void test_rtt_estimates(void) {
    const float settings[][2] = {{25, 0}, {50, 0}, {100, 0}, {50, 20}, {150, 30}, {50, 60}};
    for (size_t i = 0; i < sizeof(settings) / sizeof(settings[0]); i++) {
        coop_world_open(&world, 51 + i, 1, NULL);
        coop_side_config_t host = host_side(2, 0);
        coop_world_host(&world, &host);
        coop_side_config_t side = guest_side(401, 0);
        coop_world_join(&world, 0, &side);
        net_link_conditions_t path = {.delay_ms = settings[i][0], .jitter_ms = settings[i][1]};
        coop_world_conditions(&world, 0, &path, &path);
        coop_world_run(&world, 1.0);
        world.on_tick = clock_tick;
        coop_world_run(&world, 5.0);
        double truth_ms = 2.0 * (double)settings[i][0];
        double jitter_ms = (double)settings[i][1];
        coop_seat_stats_t stats;
        TEST_ASSERT_TRUE(coop_session_seat_stats(world.host, 1, &stats));
        check_stats(&stats, truth_ms, jitter_ms);
        TEST_ASSERT_TRUE(coop_session_seat_stats(world.guest[0], COOP_SEAT_HOST, &stats));
        check_stats(&stats, truth_ms, jitter_ms);
        coop_world_close(&world);
    }
}

/* The guest polls every frame but steps (and sends) only every sixth: what it acknowledges
   waited on its side, and the wait must come off the host's sample. */
static void test_rtt_with_a_side_that_steps_ten_times_a_second(void) {
    coop_world_open(&world, 58, 1, NULL);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(402, 0);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t path = {.delay_ms = 40};
    coop_world_conditions(&world, 0, &path, &path);
    coop_world_run(&world, 1.0);
    uint8_t payload[16] = {0};
    coop_event_t event;
    for (int frame = 0; frame < 6 * 60; frame++) {
        net_link_sim_advance(world.sim, world.dt);
        double now = net_link_sim_time(world.sim);
        coop_session_set_time(world.host, now);
        while (coop_session_poll(world.host, &event)) {
        }
        coop_session_send_state(world.host, 1, payload, sizeof(payload), NULL);
        coop_session_end_tick(world.host);
        coop_session_set_time(world.guest[0], now);
        while (coop_session_poll(world.guest[0], &event)) {
        }
        if (frame % 6 == 0) {
            coop_session_send_state(world.guest[0], COOP_SEAT_HOST, payload, sizeof(payload), NULL);
            coop_session_end_tick(world.guest[0]);
        }
    }
    coop_seat_stats_t stats;
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.host, 1, &stats));
    check_stats(&stats, 80.0, 0.0);
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.guest[0], COOP_SEAT_HOST, &stats));
    check_stats(&stats, 80.0, 0.0);
    coop_world_close(&world);
}

static void test_peer_tick_on_both_sides(void) {
    static const struct {
        double host_hz, guest_hz;
        net_link_conditions_t path;
        double low, high; /* ticks, against the other side's tick one delay ago */
    } cases[] = {
        {60.0, 60.0, {.delay_ms = 40}, -1.25, 0.25},
        {60.0, 60.0, {.delay_ms = 50, .jitter_ms = 20}, -1.75, 0.25},
        {60.0, 60.0, {.delay_ms = 50, .jitter_ms = 60}, -2.0, 0.25},
        {60.0, 59.5, {.delay_ms = 40, .jitter_ms = 10}, -1.75, 0.5},
        {59.5, 60.0, {.delay_ms = 40, .jitter_ms = 10}, -1.75, 1.0},
        {60.5, 60.0, {.delay_ms = 40, .jitter_ms = 10}, -1.75, 1.0},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        clock_band_t guest_view;
        clock_band_t host_view;
        run_clocks(cases[i].host_hz, cases[i].guest_hz, &cases[i].path, 61 + i, 12.0, &guest_view, &host_view);
        TEST_ASSERT_TRUE(guest_view.samples > 400 && host_view.samples > 400);
        TEST_ASSERT_TRUE_MESSAGE(guest_view.low >= cases[i].low && guest_view.high <= cases[i].high, "guest's view of the host's tick out of band");
        TEST_ASSERT_TRUE_MESSAGE(host_view.low >= cases[i].low && host_view.high <= cases[i].high, "host's view of the guest's tick out of band");
    }
}

// #endregion

// #region acknowledgements that do not depend on the game

typedef struct quiet_run {
    uint32_t sent;
    uint16_t number[6000];
    uint8_t delivered[6000];
    uint8_t seen[6000];
} quiet_run_t;

static void quiet_tick(void *context, coop_world_t *w, int side) {
    quiet_run_t *run = context;
    if (side != COOP_WORLD_HOST || run->sent >= 6000) {
        return; /* the guest's game sends no state at all */
    }
    uint8_t payload[4];
    put_u32(payload, run->sent);
    uint16_t number = 0;
    coop_send_result_t result = coop_session_send_state(w->host, 1, payload, sizeof(payload), &number);
    TEST_ASSERT_TRUE(result == COOP_SEND_OK || result == COOP_SEND_DROPPED);
    run->number[run->sent++] = number;
    uint32_t window = run->sent < COOP_FATE_WINDOW ? run->sent : COOP_FATE_WINDOW;
    for (uint32_t age = 0; age < window; age++) {
        uint32_t id = run->sent - 1 - age;
        coop_packet_fate_t fate = coop_session_packet_fate(w->host, 1, run->number[id]);
        if (fate != COOP_PACKET_PENDING) {
            run->seen[id] = (uint8_t)fate;
        }
    }
}

static void quiet_event(void *context, coop_world_t *w, int side, const coop_event_t *event) {
    quiet_run_t *run = context;
    (void)w;
    if (side != COOP_WORLD_HOST && event->type == COOP_EVENT_STATE) {
        run->delivered[get_u32(event->data)] = 1;
    }
    TEST_ASSERT_TRUE_MESSAGE(!(side == COOP_WORLD_HOST && event->type == COOP_EVENT_STATE), "an empty packet reached the game");
}

/* The guest's game sends no state; its session acknowledges every `idle` ticks. At 16 ticks
   a packet is named by two acknowledgements only, so losing both at 5% happens. */
static void run_quiet(uint32_t idle, uint32_t false_lost_max) {
    static quiet_run_t run;
    memset(&run, 0, sizeof(run));
    coop_world_open(&world, 81, 1, NULL);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(801, 0);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t path = {.delay_ms = 50, .jitter_ms = 20, .loss = 0.05F};
    coop_world_conditions(&world, 0, &path, &path);
    coop_world_run(&world, 0.5);
    coop_session_set_idle_ticks(world.guest[0], idle);
    world.context = &run;
    world.on_tick = quiet_tick;
    world.on_event = quiet_event;
    while (run.sent < 6000) {
        coop_world_step(&world);
    }
    uint32_t false_lost = 0;
    uint32_t false_arrived = 0;
    uint32_t unresolved = 0;
    uint32_t lost = 0;
    for (uint32_t id = 0; id + COOP_FATE_WINDOW < run.sent; id++) {
        false_lost += run.delivered[id] && run.seen[id] == COOP_PACKET_LOST ? 1U : 0U;
        false_arrived += !run.delivered[id] && run.seen[id] == COOP_PACKET_ARRIVED ? 1U : 0U;
        unresolved += run.seen[id] == 0 ? 1U : 0U;
        lost += run.seen[id] == COOP_PACKET_LOST ? 1U : 0U;
    }
    TEST_ASSERT_TRUE_MESSAGE(false_lost <= false_lost_max, "delivered packets read LOST");
    TEST_ASSERT_EQUAL_UINT32(0, false_arrived);
    TEST_ASSERT_EQUAL_UINT32(0, unresolved);
    TEST_ASSERT_TRUE(lost > 100 && lost < 700); /* the path's own 5% */
    coop_seat_stats_t stats;
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.guest[0], COOP_SEAT_HOST, &stats));
    TEST_ASSERT_EQUAL_UINT32(0, stats.state_sent);
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.host, 1, &stats));
    TEST_ASSERT_EQUAL_UINT32(0, stats.state_received);
    TEST_ASSERT_TRUE(stats.rtt_ms > 99.0F && stats.rtt_ms < 160.0F);
    coop_world_close(&world);
}

static void test_fates_resolve_when_the_other_game_sends_no_state(void) {
    run_quiet(1, 0);
    run_quiet(4, 0);
    run_quiet(1000, 120); /* clamped to COOP_IDLE_TICKS_MAX; unclamped, nothing would resolve */
    coop_session_set_idle_ticks(NULL, 3);
}

static void test_a_dropped_state_packet_still_takes_its_number(void) {
    net_link_sim_config_t small = {.unreliable_queue_bytes = 4096};
    coop_world_open(&world, 82, 1, &small);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(802, 0);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.3);
    world.frozen = 1;
    static uint8_t payload[200];
    uint16_t numbers[60];
    uint16_t dropped = 0;
    uint32_t dropped_count = 0;
    for (int i = 0; i < 60; i++) {
        coop_send_result_t result = coop_session_send_state(world.host, 1, payload, sizeof(payload), &numbers[i]);
        TEST_ASSERT_TRUE(result == COOP_SEND_OK || result == COOP_SEND_DROPPED);
        if (i > 0) {
            TEST_ASSERT_EQUAL_UINT16((uint16_t)(numbers[i - 1] + 1U), numbers[i]);
        }
        if (result == COOP_SEND_DROPPED) {
            dropped = numbers[i];
            dropped_count++;
        }
        coop_world_step(&world);
    }
    TEST_ASSERT_TRUE(dropped_count > 10);
    coop_seat_stats_t stats;
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.host, 1, &stats));
    TEST_ASSERT_EQUAL_UINT32(60, stats.state_sent);
    world.frozen = 0;
    coop_world_run(&world, 1.0);
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_LOST, coop_session_packet_fate(world.host, 1, dropped));
    TEST_ASSERT_EQUAL_INT(COOP_PACKET_ARRIVED, coop_session_packet_fate(world.host, 1, numbers[0]));
    coop_world_close(&world);
}

// #endregion

// #region round-trip seeds and stalls

typedef struct seed_run {
    bool welcomed;
    float guest_rtt_at_welcome;
    float host_rtt_at_welcome;
} seed_run_t;

static void seed_event(void *context, coop_world_t *w, int side, const coop_event_t *event) {
    seed_run_t *run = context;
    if (side == 0 && event->type == COOP_EVENT_WELCOME) {
        coop_seat_stats_t stats;
        TEST_ASSERT_TRUE(coop_session_seat_stats(w->guest[0], COOP_SEAT_HOST, &stats));
        run->guest_rtt_at_welcome = stats.rtt_ms;
        TEST_ASSERT_TRUE(stats.rtt_jitter_ms < 0.0F);
        TEST_ASSERT_TRUE(coop_session_seat_stats(w->host, 1, &stats));
        run->host_rtt_at_welcome = stats.rtt_ms;
        run->welcomed = true;
    }
}

static void test_the_handshake_seeds_the_round_trip_until_state_flows(void) {
    static seed_run_t run;
    memset(&run, 0, sizeof(run));
    coop_world_open(&world, 83, 1, NULL);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(803, 0);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t fast = {.delay_ms = 20};
    coop_world_conditions(&world, 0, &fast, &fast);
    world.context = &run;
    world.on_event = seed_event;
    coop_world_run(&world, 0.5);
    TEST_ASSERT_TRUE(run.welcomed);
    TEST_ASSERT_TRUE(in_band((double)run.guest_rtt_at_welcome, 39.0, 80.0));
    TEST_ASSERT_TRUE(run.host_rtt_at_welcome < 0.0F);
    /* State samples take over and follow a slower path. */
    net_link_conditions_t slow = {.delay_ms = 100};
    coop_world_conditions(&world, 0, &slow, &slow);
    coop_world_run(&world, 4.0);
    coop_seat_stats_t stats;
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.guest[0], COOP_SEAT_HOST, &stats));
    TEST_ASSERT_TRUE(in_band((double)stats.rtt_ms, 199.0, 240.0));
    coop_world_close(&world);
}

static void test_a_stalled_side_does_not_measure_its_stall(void) {
    coop_world_open(&world, 84, 1, NULL);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(804, 0);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t path = {.delay_ms = 30};
    coop_world_conditions(&world, 0, &path, &path);
    world.on_tick = clock_tick;
    coop_world_run(&world, 2.0);
    world.frozen = 1;
    coop_world_run(&world, 3.0);
    world.frozen = 0;
    for (int step = 0; step < 3; step++) {
        coop_world_step(&world);
        coop_seat_stats_t stats;
        TEST_ASSERT_TRUE(coop_session_seat_stats(world.guest[0], COOP_SEAT_HOST, &stats));
        TEST_ASSERT_TRUE_MESSAGE(in_band((double)stats.rtt_ms, 59.0, 100.0), "the guest measured its own stall");
        TEST_ASSERT_TRUE(coop_session_seat_stats(world.host, 1, &stats));
        TEST_ASSERT_TRUE_MESSAGE(in_band((double)stats.rtt_ms, 59.0, 100.0), "the host measured the guest's stall");
    }
    coop_world_close(&world);
}

// #endregion

// #region baselines on any link

static void test_a_baseline_on_a_link_with_steam_limits(void) {
    static uint8_t baseline[500000];
    net_link_sim_config_t steam = {.max_reliable_bytes = 524288, .reliable_queue_bytes = 524288};
    coop_world_open(&world, 85, 1, &steam);
    coop_side_config_t host = host_side(2, 600000);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(805, 600000);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t down = {.delay_ms = 40, .rate_bytes_per_s = 150000};
    net_link_conditions_t up = {.delay_ms = 40};
    coop_world_conditions(&world, 0, &up, &down);
    coop_world_run(&world, 0.5);
    TEST_ASSERT_EQUAL_UINT32(COOP_RELIABLE_CAP - COOP_WIRE_RECORD_BYTES, coop_session_record_max(world.host));
    coop_log_clear(&world.guest_log[0]);
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_baseline(world.host, 1, baseline, sizeof(baseline)));
    static uint8_t record[200];
    uint32_t sent = 0;
    for (int step = 0; step < 60 * 8; step++) {
        TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_record(world.host, 1, record, sizeof(record)));
        sent++;
        coop_world_step(&world);
    }
    coop_world_run(&world, 1.0);
    const coop_logged_t *whole = coop_log_find(&world.guest_log[0], COOP_EVENT_BASELINE, 0);
    TEST_ASSERT_NOT_NULL_MESSAGE(whole, "the baseline never arrived");
    TEST_ASSERT_EQUAL_UINT32(sizeof(baseline), whole->event.size);
    /* Only records and the one baseline came after the clear: every record arrived. */
    TEST_ASSERT_EQUAL_UINT32(sent + 1, world.guest_log[0].count);
    coop_world_close(&world);
}

typedef struct cut_run {
    uint32_t records;            /* RECORD events from the host since WELCOME */
    uint32_t records_at_baseline;
    uint32_t records_before;
    bool baseline;
    uint32_t guest_progress_seen;
} cut_run_t;

static void cut_event(void *context, coop_world_t *w, int side, const coop_event_t *event) {
    cut_run_t *run = context;
    (void)w;
    if (side != 0) {
        return;
    }
    if (event->type == COOP_EVENT_RECORD) {
        run->records++;
    } else if (event->type == COOP_EVENT_BASELINE) {
        run->baseline = true;
        run->records_at_baseline = run->records;
        run->records_before = event->records_before;
    }
}

static void test_records_before_a_baseline_and_its_progress(void) {
    static uint8_t baseline[60000];
    static cut_run_t run;
    memset(&run, 0, sizeof(run));
    coop_world_open(&world, 86, 1, NULL);
    coop_side_config_t host = host_side(2, 100000);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(806, 100000);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t down = {.delay_ms = 20, .rate_bytes_per_s = 100000};
    coop_world_conditions(&world, 0, &down, &down);
    world.context = &run;
    world.on_event = cut_event;
    coop_world_run(&world, 0.3);
    uint32_t done = 0;
    uint32_t total = 0;
    TEST_ASSERT_FALSE(coop_session_baseline_progress(world.host, 1, &done, &total));
    TEST_ASSERT_FALSE(coop_session_baseline_progress(world.guest[0], COOP_SEAT_HOST, &done, &total));
    uint8_t record = 1;
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_record(world.host, 1, &record, 1));
    }
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_baseline(world.host, 1, baseline, sizeof(baseline)));
    TEST_ASSERT_TRUE(coop_session_baseline_progress(world.host, 1, &done, &total));
    TEST_ASSERT_TRUE(done == 0 && total == sizeof(baseline));
    uint32_t host_done = 0;
    uint32_t sent = 5;
    for (int step = 0; step < 120 && !run.baseline; step++) {
        TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_record(world.host, 1, &record, 1));
        sent++;
        coop_world_step(&world);
        if (coop_session_baseline_progress(world.host, 1, &done, &total)) {
            TEST_ASSERT_TRUE(done >= host_done && done < total);
            host_done = done;
        }
        if (coop_session_baseline_progress(world.guest[0], COOP_SEAT_HOST, &done, &total)) {
            TEST_ASSERT_TRUE(done < total && total == sizeof(baseline));
            run.guest_progress_seen++;
        }
    }
    TEST_ASSERT_TRUE(run.baseline);
    TEST_ASSERT_TRUE(run.guest_progress_seen > 10);
    TEST_ASSERT_EQUAL_UINT32(5, run.records_before);
    /* Records sent while it travelled overtook it. */
    TEST_ASSERT_TRUE(run.records_at_baseline > run.records_before + 10);
    TEST_ASSERT_FALSE(coop_session_baseline_progress(world.host, 1, &done, &total));
    TEST_ASSERT_FALSE(coop_session_baseline_progress(world.guest[0], COOP_SEAT_HOST, &done, &total));
    for (int step = 0; step < 30; step++) {
        TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_record(world.host, 1, &record, 1));
        sent++;
        coop_world_step(&world);
    }
    coop_world_run(&world, 0.5);
    TEST_ASSERT_EQUAL_UINT32(sent, run.records);
    coop_world_close(&world);
}

// #endregion

// #region a network that repeats itself, a receiver that sleeps, a game that sends less

enum { HOSTILE_PACKETS = 6000 };

/* Host to guest state under heavy duplication and long reorder, optionally with a host that
   stops stepping for `pause_s` every `period_s`. Returns how many packets came twice. */
static uint32_t run_repeating_network(float reorder_ms, double period_s, double pause_s, uint64_t seed) {
    static uint8_t delivered[HOSTILE_PACKETS];
    memset(delivered, 0, sizeof(delivered));
    coop_world_open(&world, seed, 1, NULL);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(901, 0);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.5);
    net_link_conditions_t path = {.delay_ms = 30, .duplicate = 0.3F, .reorder = 0.3F, .reorder_ms = reorder_ms};
    coop_world_conditions(&world, 0, &path, &path);
    uint32_t sent = 0;
    uint32_t doubles = 0;
    uint32_t got = 0;
    uint8_t payload[4];
    coop_event_t event;
    for (uint32_t frame = 0; sent < HOSTILE_PACKETS || frame < sent + 400; frame++) {
        net_link_sim_advance(world.sim, world.dt);
        double now = net_link_sim_time(world.sim);
        bool paused = pause_s > 0.0 && now > 5.0 && fmod(now, period_s) < pause_s;
        if (!paused) {
            coop_session_set_time(world.host, now);
            while (coop_session_poll(world.host, &event)) {
            }
            if (sent < HOSTILE_PACKETS) {
                put_u32(payload, sent++);
                coop_session_send_state(world.host, 1, payload, sizeof(payload), NULL);
            }
            coop_session_end_tick(world.host);
        }
        coop_session_set_time(world.guest[0], now);
        while (coop_session_poll(world.guest[0], &event)) {
            if (event.type == COOP_EVENT_STATE) {
                uint32_t id = get_u32(event.data);
                TEST_ASSERT_TRUE(id < HOSTILE_PACKETS);
                doubles += delivered[id];
                delivered[id] = 1;
                got++;
            }
        }
        coop_session_send_state(world.guest[0], COOP_SEAT_HOST, payload, sizeof(payload), NULL);
        coop_session_end_tick(world.guest[0]);
    }
    TEST_ASSERT_TRUE(got > HOSTILE_PACKETS / 2);
    coop_world_close(&world);
    return doubles;
}

static void test_no_state_packet_comes_twice_on_a_repeating_network(void) {
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, run_repeating_network(2500.0F, 6.0, 3.0, 91), "a copy came twice with host pauses");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, run_repeating_network(1500.0F, 6.0, 0.0, 92), "a copy came twice");
}

typedef struct frozen_run {
    bool sending;
    uint32_t sent;
    uint32_t fresh_from;
    uint32_t fresh_got;
} frozen_run_t;

static void frozen_tick(void *context, coop_world_t *w, int side) {
    frozen_run_t *run = context;
    if (side != COOP_WORLD_HOST || !run->sending) {
        return;
    }
    uint8_t payload[4];
    put_u32(payload, run->sent++);
    coop_session_send_state(w->host, 1, payload, sizeof(payload), NULL);
}

static void frozen_event(void *context, coop_world_t *w, int side, const coop_event_t *event) {
    frozen_run_t *run = context;
    (void)w;
    if (side == 0 && event->type == COOP_EVENT_STATE && get_u32(event->data) >= run->fresh_from) {
        run->fresh_got++;
    }
}

/* The guest stops reading while the host sends more than 2^15 packets; its link keeps only the
   first few. Back, its window must take the fresh packets at once. */
static void test_a_receiver_frozen_past_the_wrap_takes_fresh_packets(void) {
    static frozen_run_t run;
    memset(&run, 0, sizeof(run));
    net_link_sim_config_t small = {.unreliable_queue_bytes = 4096};
    coop_world_open(&world, 93, 1, &small);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(902, 0);
    coop_world_join(&world, 0, &side);
    coop_world_run(&world, 0.5);
    world.context = &run;
    world.on_tick = frozen_tick;
    world.on_event = frozen_event;
    run.sending = true;
    world.frozen = 1;
    while (run.sent < 33000) {
        coop_world_step(&world);
    }
    run.fresh_from = run.sent + 2;
    world.frozen = 0;
    coop_world_run(&world, 0.5);
    TEST_ASSERT_TRUE_MESSAGE(run.fresh_got >= 25, "fresh packets read as old after the freeze");
    coop_world_close(&world);
}

/* The numbers of the game's packets show whether the session slipped empty ones between. */
static void test_idle_ticks_leave_the_packet_rate_to_the_game(void) {
    for (uint32_t idle = 1; idle <= 2; idle++) {
        coop_world_open(&world, 94, 1, NULL);
        coop_side_config_t host = host_side(2, 0);
        coop_world_host(&world, &host);
        coop_side_config_t side = guest_side(903, 0);
        coop_world_join(&world, 0, &side);
        coop_world_run(&world, 0.5);
        coop_session_set_idle_ticks(world.host, idle);
        uint8_t payload[4] = {0};
        uint16_t last = 0;
        uint32_t gaps = 0;
        for (int step = 0; step < 120; step++) {
            if (step % 2 == 0) {
                uint16_t number = 0;
                TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_state(world.host, 1, payload, sizeof(payload), &number));
                if (step > 0) {
                    gaps += (uint16_t)(number - last) - 1U;
                }
                last = number;
            }
            coop_world_step(&world);
        }
        /* Every other tick with idle 1: one empty packet in each gap; with idle 2, none. */
        TEST_ASSERT_EQUAL_UINT32(idle == 1 ? 59 : 0, gaps);
        coop_world_close(&world);
    }
}

// #endregion

// #region round trip on slow loops and loops that stop reading

static void test_a_slow_tick_rate_still_measures_the_round_trip(void) {
    coop_world_open(&world, 95, 1, NULL);
    world.dt = 0.1;
    coop_side_config_t host = host_side(2, 0);
    host.tick_hz = 10;
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(904, 0);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t path = {.delay_ms = 30};
    coop_world_conditions(&world, 0, &path, &path);
    coop_world_run(&world, 5.0);
    coop_seat_stats_t stats;
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.host, 1, &stats));
    TEST_ASSERT_TRUE_MESSAGE(in_band((double)stats.rtt_ms, 59.0, 60.0 + 200.0 + 4.0), "a 10 Hz host took no round-trip sample");
    TEST_ASSERT_TRUE(coop_session_seat_stats(world.guest[0], COOP_SEAT_HOST, &stats));
    TEST_ASSERT_TRUE(in_band((double)stats.rtt_ms, 59.0, 60.0 + 200.0 + 4.0) && stats.rtt_jitter_ms >= 0.0F);
    coop_world_close(&world);
}

/* The guest keeps its clock and its sends going but does not poll for half a second. */
static void test_a_loop_that_stops_reading_does_not_measure_its_wait(void) {
    coop_world_open(&world, 96, 1, NULL);
    coop_side_config_t host = host_side(2, 0);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(905, 0);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t path = {.delay_ms = 30};
    coop_world_conditions(&world, 0, &path, &path);
    world.on_tick = clock_tick;
    coop_world_run(&world, 2.0);
    world.on_tick = NULL;
    uint8_t payload[16] = {0};
    coop_event_t event;
    for (int step = 0; step < 36; step++) {
        net_link_sim_advance(world.sim, world.dt);
        double now = net_link_sim_time(world.sim);
        coop_session_set_time(world.host, now);
        while (coop_session_poll(world.host, &event)) {
        }
        coop_session_send_state(world.host, 1, payload, sizeof(payload), NULL);
        coop_session_end_tick(world.host);
        coop_session_set_time(world.guest[0], now);
        bool reading = step < 3 || step >= 33;
        while (reading && coop_session_poll(world.guest[0], &event)) {
        }
        coop_session_send_state(world.guest[0], COOP_SEAT_HOST, payload, sizeof(payload), NULL);
        coop_session_end_tick(world.guest[0]);
        if (step >= 33) {
            coop_seat_stats_t stats;
            TEST_ASSERT_TRUE(coop_session_seat_stats(world.guest[0], COOP_SEAT_HOST, &stats));
            TEST_ASSERT_TRUE_MESSAGE(in_band((double)stats.rtt_ms, 59.0, 100.0), "the guest measured its own wait");
        }
    }
    coop_world_close(&world);
}

// #endregion

// #region baselines

enum { BASELINE_BYTES = 150000, BASELINE_MAX = 200000 };

typedef struct baseline_run {
    uint32_t step;
    bool records;
    uint32_t sent;
    uint32_t got;
    uint32_t worst_latency; /* steps, of records sent while the baseline was on its way */
    uint32_t baseline_sent_step;
    uint32_t baseline_got_step;
    uint32_t baselines;
    uint32_t baseline_size;
    uint32_t baseline_tick;
    bool baseline_equal;
    uint8_t fill;
} baseline_run_t;

static uint8_t baseline_byte(uint32_t i, uint8_t fill) { return (uint8_t)((i * 31U + (i >> 8) + fill) & 0xFFU); }

static void baseline_tick(void *context, coop_world_t *w, int side) {
    baseline_run_t *run = context;
    if (side != COOP_WORLD_HOST || !run->records) {
        return;
    }
    uint8_t payload[8];
    put_u32(payload, run->sent);
    put_u32(payload + 4, run->step);
    if (coop_session_send_record(w->host, 1, payload, sizeof(payload)) == COOP_SEND_OK) {
        run->sent++;
    }
}

static void baseline_event(void *context, coop_world_t *w, int side, const coop_event_t *event) {
    baseline_run_t *run = context;
    if (side == COOP_WORLD_HOST) {
        return;
    }
    if (event->type == COOP_EVENT_RECORD) {
        TEST_ASSERT_EQUAL_UINT32(run->got, get_u32(event->data));
        run->got++;
        uint32_t sent_step = get_u32(event->data + 4);
        if (run->baseline_sent_step > 0 && sent_step > run->baseline_sent_step && run->baselines == 0) {
            uint32_t latency = run->step - sent_step;
            run->worst_latency = latency > run->worst_latency ? latency : run->worst_latency;
        }
    } else if (event->type == COOP_EVENT_BASELINE) {
        run->baselines++;
        run->baseline_got_step = run->step;
        run->baseline_size = event->size;
        run->baseline_tick = event->tick;
        run->baseline_equal = true;
        for (uint32_t i = 0; i < event->size; i++) {
            if (event->data[i] != baseline_byte(i, run->fill)) {
                run->baseline_equal = false;
                break;
            }
        }
    }
}

static void baseline_steps(baseline_run_t *run, double seconds) {
    int steps = (int)(seconds * 60.0 + 0.5);
    for (int i = 0; i < steps; i++) {
        run->step++;
        coop_world_step(&world);
    }
}

static void test_a_large_baseline_arrives_whole_behind_records(void) {
    static uint8_t baseline[BASELINE_BYTES];
    static baseline_run_t run;
    memset(&run, 0, sizeof(run));
    run.fill = 3;
    for (uint32_t i = 0; i < BASELINE_BYTES; i++) {
        baseline[i] = baseline_byte(i, run.fill);
    }
    coop_world_open(&world, 71, 1, NULL);
    coop_side_config_t host = host_side(2, BASELINE_MAX);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(601, BASELINE_MAX);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t down = {.delay_ms = 20, .loss = 0.05F, .rate_bytes_per_s = 100000};
    net_link_conditions_t up = {.delay_ms = 20, .loss = 0.05F};
    coop_world_conditions(&world, 0, &up, &down);
    world.context = &run;
    world.on_tick = baseline_tick;
    world.on_event = baseline_event;
    baseline_steps(&run, 0.5);
    run.records = true;
    baseline_steps(&run, 0.5);
    TEST_ASSERT_TRUE(BASELINE_BYTES > net_link_max_reliable(world.host_link));
    run.baseline_sent_step = run.step + 1;
    uint32_t host_tick = coop_session_tick(world.host);
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_baseline(world.host, 1, baseline, BASELINE_BYTES));
    baseline_steps(&run, 4.0);
    run.records = false;
    baseline_steps(&run, 1.0);
    TEST_ASSERT_EQUAL_UINT32(1, run.baselines);
    TEST_ASSERT_EQUAL_UINT32(BASELINE_BYTES, run.baseline_size);
    TEST_ASSERT_TRUE(run.baseline_equal);
    TEST_ASSERT_EQUAL_UINT32(host_tick, run.baseline_tick);
    /* The baseline took a while at this rate, and records never waited for it. */
    TEST_ASSERT_TRUE(run.baseline_got_step - run.baseline_sent_step > 60);
    TEST_ASSERT_TRUE_MESSAGE(run.worst_latency < 30, "records waited behind the baseline");
    TEST_ASSERT_EQUAL_UINT32(run.sent, run.got);
    coop_world_close(&world);
}

static void test_a_baseline_for_a_seat_that_leaves_is_dropped(void) {
    static uint8_t baseline[BASELINE_BYTES];
    static baseline_run_t run;
    memset(&run, 0, sizeof(run));
    for (uint32_t i = 0; i < BASELINE_BYTES; i++) {
        baseline[i] = baseline_byte(i, 0);
    }
    coop_world_open(&world, 72, 2, NULL);
    coop_side_config_t host = host_side(2, BASELINE_MAX);
    coop_world_host(&world, &host);
    coop_side_config_t side = guest_side(701, BASELINE_MAX);
    coop_world_join(&world, 0, &side);
    net_link_conditions_t down = {.delay_ms = 20, .rate_bytes_per_s = 100000};
    coop_world_conditions(&world, 0, &down, &down);
    coop_world_run(&world, 0.5);
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_baseline(world.host, 1, baseline, BASELINE_BYTES));
    coop_world_run(&world, 0.3);
    TEST_ASSERT_EQUAL(COOP_SEND_FULL, coop_session_send_baseline(world.host, 1, baseline, 10));
    coop_session_kick(world.host, 1, COOP_CLOSE_KICKED);
    coop_world_run(&world, 3.0);
    TEST_ASSERT_EQUAL_UINT32(0, coop_log_count(&world.guest_log[0], COOP_EVENT_BASELINE));
    coop_side_config_t next = guest_side(702, BASELINE_MAX);
    coop_world_join(&world, 1, &next);
    coop_world_run(&world, 0.5);
    TEST_ASSERT_EQUAL_UINT8(1, coop_session_seat(world.guest[1]));
    run.fill = 9;
    for (uint32_t i = 0; i < 5000; i++) {
        baseline[i] = baseline_byte(i, run.fill);
    }
    world.context = &run;
    world.on_event = baseline_event;
    TEST_ASSERT_EQUAL(COOP_SEND_OK, coop_session_send_baseline(world.host, 1, baseline, 5000));
    coop_world_run(&world, 1.0);
    TEST_ASSERT_EQUAL_UINT32(1, run.baselines);
    TEST_ASSERT_EQUAL_UINT32(5000, run.baseline_size);
    TEST_ASSERT_TRUE(run.baseline_equal);
    coop_world_close(&world);
}

// #endregion

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_receive_window_places_duplicates_reorder_and_wrap);
    RUN_TEST(test_send_window_fates_and_samples);
    RUN_TEST(test_rtt_smoothing);
    RUN_TEST(test_loss_counts_resolved_packets_only);
    RUN_TEST(test_send_window_after_a_long_silence);
    RUN_TEST(test_receive_window_restarts_only_for_a_newer_stamp);
    RUN_TEST(test_clock_follows_the_least_delayed_packet);
    RUN_TEST(test_clock_runs_on_through_a_pause_then_steps_back);
    RUN_TEST(test_clock_reads_ticks_across_the_wrap);
    RUN_TEST(test_records_arrive_once_and_in_order_under_loss);
    RUN_TEST(test_fates_hold_under_loss_reorder_and_wrap);
    RUN_TEST(test_rtt_estimates);
    RUN_TEST(test_rtt_with_a_side_that_steps_ten_times_a_second);
    RUN_TEST(test_peer_tick_on_both_sides);
    RUN_TEST(test_fates_resolve_when_the_other_game_sends_no_state);
    RUN_TEST(test_a_dropped_state_packet_still_takes_its_number);
    RUN_TEST(test_the_handshake_seeds_the_round_trip_until_state_flows);
    RUN_TEST(test_a_stalled_side_does_not_measure_its_stall);
    RUN_TEST(test_a_baseline_on_a_link_with_steam_limits);
    RUN_TEST(test_records_before_a_baseline_and_its_progress);
    RUN_TEST(test_no_state_packet_comes_twice_on_a_repeating_network);
    RUN_TEST(test_a_receiver_frozen_past_the_wrap_takes_fresh_packets);
    RUN_TEST(test_idle_ticks_leave_the_packet_rate_to_the_game);
    RUN_TEST(test_a_slow_tick_rate_still_measures_the_round_trip);
    RUN_TEST(test_a_loop_that_stops_reading_does_not_measure_its_wait);
    RUN_TEST(test_a_large_baseline_arrives_whole_behind_records);
    RUN_TEST(test_a_baseline_for_a_seat_that_leaves_is_dropped);
    return UNITY_END();
}
