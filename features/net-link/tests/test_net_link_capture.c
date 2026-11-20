#if defined(_WIN32) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "features/net_link/net_link_capture.h"
#include "features/net_link/net_link_sim.h"

/* Before unity.h: its noreturn macro breaks the CRT's __declspec(noreturn). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

enum { HOST_ID = 500, GUEST_ID = 9, EXPECT_MAX = 1024, BATCH = 64, HEADER = 8, RECORD = 24 };

typedef struct expected {
    uint64_t time_us;
    uint64_t identity;
    uint32_t size;
    uint8_t tag;
    uint8_t direction;
    uint8_t lane;
    bool reliable;
    uint8_t first; /* first payload byte */
    uint8_t last;
} expected_t;

static expected_t expected[EXPECT_MAX];
static uint32_t expected_count;
static uint64_t clock_us;

static void expect(uint8_t tag, uint8_t direction, uint64_t identity, uint8_t lane, bool reliable, const uint8_t *data, uint32_t size) {
    TEST_ASSERT_TRUE(expected_count < EXPECT_MAX);
    expected[expected_count++] = (expected_t){clock_us, identity, size, tag, direction, lane, reliable, data[0], data[size - 1]};
}

static void fill(uint8_t *buffer, uint32_t size, uint32_t seed) {
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)(seed * 17U + i);
    }
}

static void send_and_expect(net_link_t *link, uint8_t tag, net_link_peer_t peer, uint64_t identity, uint8_t lane, uint32_t flags, uint32_t size, uint32_t seed) {
    uint8_t buffer[2048];
    fill(buffer, size, seed);
    const net_link_send_result_t result = net_link_send(link, peer, lane, flags, buffer, size);
    if (result == NET_LINK_SEND_OK) {
        expect(tag, NET_LINK_CAPTURE_OUT, identity, lane, (flags & NET_LINK_SEND_RELIABLE) != 0, buffer, size);
    }
}

static void poll_and_expect(net_link_t *link, uint8_t tag, net_link_peer_t *peer_out) {
    net_link_event_t events[BATCH];
    uint32_t n;
    while ((n = net_link_poll(link, events, BATCH)) > 0) {
        for (uint32_t e = 0; e < n; e++) {
            if (events[e].type == NET_LINK_EVENT_CONNECTED) {
                *peer_out = events[e].peer;
            } else if (events[e].type == NET_LINK_EVENT_MESSAGE) {
                expect(tag, NET_LINK_CAPTURE_IN, events[e].identity, events[e].lane, events[e].reliable, events[e].data, events[e].size);
            }
        }
    }
}

/* Runs a short session with both links captured into `path` and fills `expected`. */
static void record_session(const char *path, uint32_t flags) {
    expected_count = 0;
    clock_us = 0;
    net_link_sim_config_t config = {.seed = 3, .max_guests = 1};
    config.lanes.count = 2;
    net_link_sim_t *sim = net_link_sim_create(&config);
    net_link_t *host = net_link_sim_host(sim, HOST_ID, 1);
    net_link_t *guest = net_link_sim_guest(sim, GUEST_ID);
    net_link_sim_set_conditions(sim, guest, NET_LINK_SIM_TO_GUEST, &(net_link_conditions_t){.delay_ms = 15, .jitter_ms = 10, .loss = 0.2F, .duplicate = 0.2F});
    net_link_sim_set_conditions(sim, guest, NET_LINK_SIM_TO_HOST, &(net_link_conditions_t){.delay_ms = 25});
    net_link_capture_t *capture = net_link_capture_open(path, flags);
    TEST_ASSERT_NOT_NULL(capture);
    net_link_capture_attach(host, capture, 1);
    net_link_capture_attach(guest, capture, 2);
    net_link_peer_t to_guest = 0, to_host = 0;
    for (uint32_t tick = 0; tick < 120; tick++) {
        net_link_capture_set_time(capture, net_link_sim_time(sim));
        clock_us = (uint64_t)(net_link_sim_time(sim) * 1e6);
        if (tick == 60) {
            /* A clock that steps back must not make the file go back in time. */
            net_link_capture_set_time(capture, 0.001);
        }
        poll_and_expect(host, 1, &to_guest);
        poll_and_expect(guest, 2, &to_host);
        if (to_guest) {
            send_and_expect(host, 1, to_guest, GUEST_ID, 0, NET_LINK_SEND_RELIABLE, 8 + tick * 11 % 900, tick);
            send_and_expect(host, 1, to_guest, GUEST_ID, 1, NET_LINK_SEND_UNRELIABLE, 1 + tick % 1200, tick + 1000);
            uint8_t bad[4] = {0};
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_INVALID, net_link_send(host, to_guest, 5, 0, bad, 4));
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_TOO_LARGE, net_link_send(host, to_guest, 0, 0, bad, 1201));
        }
        if (to_host) {
            send_and_expect(guest, 2, to_host, HOST_ID, 1, NET_LINK_SEND_RELIABLE, 40, tick + 2000);
        }
        net_link_flush(host);
        net_link_flush(guest);
        net_link_sim_advance(sim, 0.01);
    }
    TEST_ASSERT_TRUE(net_link_capture_ok(capture));
    net_link_capture_attach(host, NULL, 0);
    net_link_capture_attach(guest, NULL, 0);
    const uint8_t after[8] = {0};
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(host, to_guest, 0, NET_LINK_SEND_RELIABLE, after, sizeof(after))); /* detached: not in the file */
    net_link_capture_close(capture);
    net_link_sim_destroy(sim);
    TEST_ASSERT_TRUE(expected_count > 200);
}

static void check_file(const char *path, bool payload) {
    net_link_capture_reader_t *reader = net_link_capture_reader_open(path);
    TEST_ASSERT_NOT_NULL(reader);
    net_link_capture_record_t record;
    uint32_t read = 0, ins = 0, outs = 0;
    double last_time = 0.0;
    while (net_link_capture_reader_next(reader, &record)) {
        TEST_ASSERT_TRUE(read < expected_count);
        const expected_t *want = &expected[read++];
        TEST_ASSERT_TRUE(record.time_s >= last_time);
        last_time = record.time_s;
        TEST_ASSERT_EQUAL_UINT64(want->time_us, (uint64_t)(record.time_s * 1e6 + 0.5));
        TEST_ASSERT_EQUAL_UINT64(want->identity, record.identity);
        TEST_ASSERT_EQUAL_UINT32(want->size, record.size);
        TEST_ASSERT_EQUAL_UINT8(want->tag, record.tag);
        TEST_ASSERT_EQUAL_UINT8(want->direction, record.direction);
        TEST_ASSERT_EQUAL_UINT8(want->lane, record.lane);
        TEST_ASSERT_EQUAL(want->reliable, record.reliable);
        if (payload) {
            TEST_ASSERT_NOT_NULL(record.payload);
            TEST_ASSERT_EQUAL_UINT8(want->first, record.payload[0]);
            TEST_ASSERT_EQUAL_UINT8(want->last, record.payload[record.size - 1]);
        } else {
            TEST_ASSERT_NULL(record.payload);
        }
        if (record.direction == NET_LINK_CAPTURE_IN) {
            ins++;
        } else {
            outs++;
        }
    }
    TEST_ASSERT_FALSE(net_link_capture_reader_failed(reader));
    TEST_ASSERT_FALSE(net_link_capture_reader_next(reader, &record));
    TEST_ASSERT_EQUAL_UINT32(expected_count, read);
    TEST_ASSERT_TRUE(ins > 0 && outs > 0);
    net_link_capture_reader_close(reader);
}

static void test_capture_keeps_time_lane_size_and_bytes_of_every_message(void) {
    record_session("net_link_capture_payload.nlcp", NET_LINK_CAPTURE_PAYLOAD);
    check_file("net_link_capture_payload.nlcp", true);
}

static void test_capture_without_payload_keeps_the_sizes(void) {
    record_session("net_link_capture_sizes.nlcp", 0);
    check_file("net_link_capture_sizes.nlcp", false);
}

static uint8_t *load(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    static uint8_t data[4 << 20];
    *size = fread(data, 1, sizeof(data), file);
    fclose(file);
    TEST_ASSERT_TRUE(*size < sizeof(data));
    return data;
}

static void store(const char *path, const uint8_t *data, size_t size) {
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)size, (uint32_t)fwrite(data, 1, size, file));
    fclose(file);
}

/* Reads to the end and returns how many records came out; `failed` tells why it stopped. */
static uint32_t read_all(const char *path, bool *failed) {
    net_link_capture_reader_t *reader = net_link_capture_reader_open(path);
    TEST_ASSERT_NOT_NULL(reader);
    net_link_capture_record_t record;
    uint32_t count = 0;
    while (net_link_capture_reader_next(reader, &record)) {
        count++;
    }
    *failed = net_link_capture_reader_failed(reader);
    net_link_capture_reader_close(reader);
    return count;
}

static void test_cut_and_garbage_files_are_refused_cleanly(void) {
    record_session("net_link_capture_source.nlcp", NET_LINK_CAPTURE_PAYLOAD);
    size_t size = 0;
    const uint8_t *whole = load("net_link_capture_source.nlcp", &size);
    static uint8_t copy[4 << 20];
    bool failed = false;
    const char *path = "net_link_capture_damaged.nlcp";

    store(path, whole, HEADER);
    TEST_ASSERT_EQUAL_UINT32(0, read_all(path, &failed));
    TEST_ASSERT_FALSE(failed);
    const size_t cuts[] = {HEADER + 1, HEADER + RECORD - 1, HEADER + RECORD + 1, size / 2, size - 1};
    for (size_t c = 0; c < sizeof(cuts) / sizeof(cuts[0]); c++) {
        store(path, whole, cuts[c]);
        const uint32_t count = read_all(path, &failed);
        TEST_ASSERT_TRUE(failed);
        TEST_ASSERT_TRUE(count < expected_count);
    }

    memcpy(copy, whole, size);
    copy[HEADER + 22] = 200; /* lane out of range */
    store(path, copy, size);
    TEST_ASSERT_EQUAL_UINT32(0, read_all(path, &failed));
    TEST_ASSERT_TRUE(failed);
    memcpy(copy, whole, size);
    memset(copy + HEADER + 16, 0xFF, 4); /* an absurd size must not be allocated or read */
    store(path, copy, size);
    TEST_ASSERT_EQUAL_UINT32(0, read_all(path, &failed));
    TEST_ASSERT_TRUE(failed);
    memcpy(copy, whole, size);
    memset(copy + HEADER, 0xEE, 8); /* the first record lies far ahead: the second steps back */
    store(path, copy, size);
    TEST_ASSERT_EQUAL_UINT32(1, read_all(path, &failed));
    TEST_ASSERT_TRUE(failed);

    uint32_t noise = 12345;
    for (size_t i = 0; i < 4096; i++) {
        noise = noise * 1103515245U + 12345U;
        copy[i] = (uint8_t)(noise >> 16);
    }
    store(path, copy, 4096);
    TEST_ASSERT_NULL(net_link_capture_reader_open(path));
    memcpy(copy, "NLCP", 4);
    copy[4] = NET_LINK_CAPTURE_VERSION;
    copy[5] = 0;
    copy[6] = NET_LINK_CAPTURE_PAYLOAD;
    copy[7] = 0;
    store(path, copy, 4096);
    (void)read_all(path, &failed);
    TEST_ASSERT_TRUE(failed);
    copy[4] = NET_LINK_CAPTURE_VERSION + 1;
    store(path, copy, 4096);
    TEST_ASSERT_NULL(net_link_capture_reader_open(path));
    copy[4] = NET_LINK_CAPTURE_VERSION;
    copy[6] = 0x80;
    store(path, copy, 4096);
    TEST_ASSERT_NULL(net_link_capture_reader_open(path));
    store(path, whole, 3);
    TEST_ASSERT_NULL(net_link_capture_reader_open(path));
    TEST_ASSERT_NULL(net_link_capture_reader_open("net_link_capture_missing.nlcp"));
    TEST_ASSERT_NULL(net_link_capture_reader_open(NULL));
    remove(path);
    remove("net_link_capture_source.nlcp");
}

static void test_capture_calls_tolerate_missing_pieces(void) {
    TEST_ASSERT_NULL(net_link_capture_open(".", 0));
    TEST_ASSERT_NULL(net_link_capture_open(NULL, 0));
    TEST_ASSERT_NULL(net_link_capture_open("net_link_capture_flags.nlcp", 0x80));
    TEST_ASSERT_FALSE(net_link_capture_ok(NULL));
    net_link_capture_set_time(NULL, 1.0);
    net_link_capture_close(NULL);
    net_link_capture_attach(NULL, NULL, 0);
    net_link_capture_reader_close(NULL);
    TEST_ASSERT_FALSE(net_link_capture_reader_next(NULL, NULL));
    TEST_ASSERT_FALSE(net_link_capture_reader_failed(NULL));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_capture_keeps_time_lane_size_and_bytes_of_every_message);
    RUN_TEST(test_capture_without_payload_keeps_the_sizes);
    RUN_TEST(test_cut_and_garbage_files_are_refused_cleanly);
    RUN_TEST(test_capture_calls_tolerate_missing_pieces);
    remove("net_link_capture_payload.nlcp");
    remove("net_link_capture_sizes.nlcp");
    return UNITY_END();
}
