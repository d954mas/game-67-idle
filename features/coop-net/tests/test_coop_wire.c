#include <stdlib.h>

#include "coop_test_world.h"
#include "coop_wire.h"
#include "features/net_bits/net_bits.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// #region helpers

static const uint8_t KEY[5] = {9, 8, 7, 6, 5};
static const uint8_t PAYLOAD[7] = {1, 2, 3, 4, 5, 6, 7};

enum { KINDS = 8, GUARD = 16 };

/* One valid message of every kind. */
static coop_wire_msg_t sample(coop_wire_kind_t kind) {
    coop_wire_msg_t msg = {.kind = kind};
    switch (kind) {
    case COOP_WIRE_HELLO:
        msg.protocol = COOP_SESSION_PROTOCOL;
        msg.build_key = 0x0123456789ABCDEFULL;
        msg.player_key = 0xFEDCBA9876543210ULL;
        msg.baseline_max = 123456789;
        msg.bytes = KEY;
        msg.size = sizeof(KEY);
        break;
    case COOP_WIRE_WELCOME:
        msg.seat = 3;
        msg.seats = 4;
        msg.tick = 0xDEADBEEF;
        msg.tick_hz = 60;
        msg.bytes = KEY;
        msg.size = sizeof(KEY);
        break;
    case COOP_WIRE_JOINED:
        msg.seat = 7;
        msg.identity = 0x1111222233334444ULL;
        break;
    case COOP_WIRE_LEFT:
        msg.seat = 2;
        msg.reason = COOP_CLOSE_KICKED;
        break;
    case COOP_WIRE_RECORD:
        msg.tick = 77;
        msg.bytes = PAYLOAD;
        msg.size = sizeof(PAYLOAD);
        break;
    case COOP_WIRE_STATE:
        msg.number = 65535;
        msg.tick = 4000000000U;
        msg.has_ack = true;
        msg.ack = 12345;
        msg.ack_mask = 0x80000001U;
        msg.ack_wait = 321;
        msg.bytes = PAYLOAD;
        msg.size = sizeof(PAYLOAD);
        break;
    case COOP_WIRE_BASELINE_FIRST:
        msg.total = 100;
        msg.tick = 5;
        msg.records_before = 0xABCDEF01U;
        msg.bytes = PAYLOAD;
        msg.size = sizeof(PAYLOAD);
        break;
    case COOP_WIRE_BASELINE_PIECE:
        msg.bytes = PAYLOAD;
        msg.size = sizeof(PAYLOAD);
        break;
    }
    return msg;
}

static bool is_from_host(coop_wire_kind_t kind) { return kind == COOP_WIRE_WELCOME || kind == COOP_WIRE_JOINED || kind == COOP_WIRE_LEFT || kind == COOP_WIRE_BASELINE_FIRST || kind == COOP_WIRE_BASELINE_PIECE; }

/* Parses an exact-size heap copy, so a read past the end is a sanitizer error. */
static bool parse_copy(const uint8_t *data, uint32_t size, uint8_t lane, bool reliable, bool from_host, coop_wire_msg_t *out) {
    uint8_t *copy = malloc(size > 0 ? size : 1);
    TEST_ASSERT_NOT_NULL(copy);
    if (size > 0) {
        memcpy(copy, data, size);
    }
    bool ok = coop_wire_parse(size > 0 ? copy : NULL, size, lane, reliable, from_host, out);
    if (ok && out->size > 0) {
        TEST_ASSERT_TRUE(out->bytes >= copy && out->bytes + out->size <= copy + size);
    }
    free(copy);
    out->bytes = NULL; /* pointed into the freed copy */
    return ok;
}

static bool parse_routed(const uint8_t *data, uint32_t size, coop_wire_kind_t kind, coop_wire_msg_t *out) {
    uint8_t lane = 0;
    bool reliable = false;
    TEST_ASSERT_TRUE(coop_wire_route(kind, &lane, &reliable));
    return parse_copy(data, size, lane, reliable, is_from_host(kind), out);
}

static uint32_t write_sample(coop_wire_kind_t kind, uint8_t *out, uint32_t capacity) {
    coop_wire_msg_t msg = sample(kind);
    return coop_wire_write(&msg, out, capacity);
}

// #endregion

// #region round trips and sizes

static void test_every_kind_round_trips(void) {
    for (int k = 1; k <= KINDS; k++) {
        coop_wire_kind_t kind = (coop_wire_kind_t)k;
        coop_wire_msg_t msg = sample(kind);
        uint8_t buffer[512];
        uint32_t size = coop_wire_write(&msg, buffer, sizeof(buffer));
        uint32_t bytes = (kind == COOP_WIRE_JOINED || kind == COOP_WIRE_LEFT) ? 0 : msg.size;
        TEST_ASSERT_EQUAL_UINT32(coop_wire_header_bytes(kind) + bytes, size);
        coop_wire_msg_t back;
        uint8_t lane = 0;
        bool reliable = false;
        TEST_ASSERT_TRUE(coop_wire_route(kind, &lane, &reliable));
        TEST_ASSERT_TRUE(coop_wire_parse(buffer, size, lane, reliable, is_from_host(kind), &back));
        TEST_ASSERT_EQUAL_INT(kind, back.kind);
        TEST_ASSERT_EQUAL_UINT32(msg.protocol, back.protocol);
        TEST_ASSERT_TRUE(msg.build_key == back.build_key && msg.player_key == back.player_key && msg.identity == back.identity);
        TEST_ASSERT_EQUAL_UINT8(msg.seat, back.seat);
        TEST_ASSERT_EQUAL_UINT8(msg.seats, back.seats);
        TEST_ASSERT_EQUAL_UINT32(msg.tick_hz, back.tick_hz);
        TEST_ASSERT_EQUAL_UINT32(msg.tick, back.tick);
        TEST_ASSERT_EQUAL_UINT32(msg.reason, back.reason);
        TEST_ASSERT_EQUAL_UINT32(msg.total, back.total);
        TEST_ASSERT_EQUAL_UINT32(msg.baseline_max, back.baseline_max);
        TEST_ASSERT_EQUAL_UINT32(msg.records_before, back.records_before);
        TEST_ASSERT_EQUAL_UINT16(msg.number, back.number);
        TEST_ASSERT_EQUAL(msg.has_ack, back.has_ack);
        TEST_ASSERT_EQUAL_UINT16(msg.ack, back.ack);
        TEST_ASSERT_EQUAL_HEX32(msg.ack_mask, back.ack_mask);
        TEST_ASSERT_EQUAL_UINT16(msg.ack_wait, back.ack_wait);
        TEST_ASSERT_EQUAL_UINT32(bytes, back.size);
        if (bytes > 0) {
            TEST_ASSERT_EQUAL_MEMORY(msg.bytes, back.bytes, bytes);
        }
    }
}

static void test_header_sizes_match_the_bits(void) {
    /* Kind 4 bits plus each kind's fields, rounded up to bytes. */
    const uint32_t bits[KINDS + 1] = {0, 4 + 32 + 16 + 64 + 64 + 32 + 9, 4 + 3 + 3 + 32 + 16 + 9, 4 + 3 + 64, 4 + 3 + 32, 4 + 32, 4 + 16 + 32 + 1 + 16 + 32 + 16, 4 + 32 + 32 + 32, 4};
    for (int k = 1; k <= KINDS; k++) {
        TEST_ASSERT_EQUAL_UINT32((bits[k] + 7) / 8, coop_wire_header_bytes((coop_wire_kind_t)k));
    }
    TEST_ASSERT_EQUAL_UINT32(0, coop_wire_header_bytes((coop_wire_kind_t)0));
    TEST_ASSERT_EQUAL_UINT32(0, coop_wire_header_bytes((coop_wire_kind_t)9));
}

static void test_hello_starts_with_kind_magic_protocol(void) {
    uint8_t buffer[64];
    uint32_t size = write_sample(COOP_WIRE_HELLO, buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(size > 0);
    /* Kind 1 in the low nibble of byte 0, then "COOP" shifted by four bits. */
    TEST_ASSERT_EQUAL_HEX8(0x31, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0xF4, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0xF4, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x04, buffer[3]);
    TEST_ASSERT_EQUAL_HEX8(0x15, buffer[4]);
}

static void test_writer_refusals(void) {
    uint8_t buffer[512];
    for (int k = 1; k <= KINDS; k++) {
        coop_wire_kind_t kind = (coop_wire_kind_t)k;
        uint32_t size = write_sample(kind, buffer, sizeof(buffer));
        TEST_ASSERT_EQUAL_UINT32(0, write_sample(kind, buffer, size - 1));
    }
    coop_wire_msg_t msg = sample(COOP_WIRE_RECORD);
    msg.size = 0;
    TEST_ASSERT_EQUAL_UINT32(0, coop_wire_write(&msg, buffer, sizeof(buffer)));
    /* The session's empty state packet is the one packet with no payload. */
    msg = sample(COOP_WIRE_STATE);
    msg.size = 0;
    TEST_ASSERT_EQUAL_UINT32(COOP_WIRE_STATE_BYTES, coop_wire_write(&msg, buffer, sizeof(buffer)));
    msg = sample(COOP_WIRE_HELLO);
    msg.size = COOP_WORLD_KEY_MAX + 1;
    TEST_ASSERT_EQUAL_UINT32(0, coop_wire_write(&msg, buffer, sizeof(buffer)));
    msg = sample(COOP_WIRE_WELCOME);
    msg.seat = 0;
    TEST_ASSERT_EQUAL_UINT32(0, coop_wire_write(&msg, buffer, sizeof(buffer)));
    msg = sample(COOP_WIRE_STATE);
    msg.has_ack = false;
    TEST_ASSERT_EQUAL_UINT32(0, coop_wire_write(&msg, buffer, sizeof(buffer)));
    msg = sample(COOP_WIRE_BASELINE_FIRST);
    msg.total = 3;
    TEST_ASSERT_EQUAL_UINT32(0, coop_wire_write(&msg, buffer, sizeof(buffer)));
    msg = (coop_wire_msg_t){.kind = (coop_wire_kind_t)9};
    TEST_ASSERT_EQUAL_UINT32(0, coop_wire_write(&msg, buffer, sizeof(buffer)));
}

// #endregion

// #region refusals

static void test_every_truncation_is_refused_or_shorter_payload(void) {
    for (int k = 1; k <= KINDS; k++) {
        coop_wire_kind_t kind = (coop_wire_kind_t)k;
        uint8_t buffer[512];
        uint32_t size = write_sample(kind, buffer, sizeof(buffer));
        uint32_t header = coop_wire_header_bytes(kind);
        bool to_the_end = kind == COOP_WIRE_RECORD || kind == COOP_WIRE_STATE || kind == COOP_WIRE_BASELINE_FIRST || kind == COOP_WIRE_BASELINE_PIECE;
        for (uint32_t cut = 0; cut < size; cut++) {
            coop_wire_msg_t back;
            bool ok = parse_routed(buffer, cut, kind, &back);
            if (to_the_end && (cut > header || (kind == COOP_WIRE_STATE && cut == header))) {
                TEST_ASSERT_TRUE(ok);
                TEST_ASSERT_EQUAL_UINT32(cut - header, back.size);
            } else {
                TEST_ASSERT_FALSE_MESSAGE(ok, "a cut packet parsed");
            }
        }
    }
}

static void test_wrong_lane_reliability_or_direction_is_refused(void) {
    for (int k = 1; k <= KINDS; k++) {
        coop_wire_kind_t kind = (coop_wire_kind_t)k;
        uint8_t buffer[512];
        uint32_t size = write_sample(kind, buffer, sizeof(buffer));
        uint8_t lane = 0;
        bool reliable = false;
        TEST_ASSERT_TRUE(coop_wire_route(kind, &lane, &reliable));
        for (uint8_t try_lane = 0; try_lane < 4; try_lane++) {
            for (int try_reliable = 0; try_reliable < 2; try_reliable++) {
                for (int try_from_host = 0; try_from_host < 2; try_from_host++) {
                    bool both_ways = kind == COOP_WIRE_RECORD || kind == COOP_WIRE_STATE;
                    bool direction = both_ways || (try_from_host != 0) == is_from_host(kind);
                    bool right = try_lane == lane && (try_reliable != 0) == reliable && direction;
                    coop_wire_msg_t back;
                    TEST_ASSERT_EQUAL(right, parse_copy(buffer, size, try_lane, try_reliable != 0, try_from_host != 0, &back));
                }
            }
        }
    }
}

static void test_trailing_bytes_and_set_padding_are_refused(void) {
    for (int k = 1; k <= KINDS; k++) {
        coop_wire_kind_t kind = (coop_wire_kind_t)k;
        uint8_t buffer[512];
        uint32_t size = write_sample(kind, buffer, sizeof(buffer));
        uint32_t header = coop_wire_header_bytes(kind);
        coop_wire_msg_t back;
        /* A byte past the end: payload kinds take it as payload, the others refuse it. */
        buffer[size] = 0;
        bool sized = kind == COOP_WIRE_JOINED || kind == COOP_WIRE_LEFT || kind == COOP_WIRE_HELLO || kind == COOP_WIRE_WELCOME;
        if (sized) {
            TEST_ASSERT_FALSE(parse_routed(buffer, size + 1, kind, &back));
        }
        /* Every header has padding in its last byte (none is a whole number of bytes). */
        uint8_t padded[512];
        memcpy(padded, buffer, size);
        padded[header - 1] |= 0x80;
        TEST_ASSERT_FALSE_MESSAGE(parse_routed(padded, size, kind, &back), "a set padding bit parsed");
    }
}

static void test_unknown_kinds_are_refused(void) {
    for (uint8_t kind = 0; kind < 16; kind++) {
        if (kind >= 1 && kind <= KINDS) {
            continue;
        }
        uint8_t buffer[64];
        memset(buffer, 0, sizeof(buffer));
        buffer[0] = kind;
        for (uint8_t lane = 0; lane < COOP_LANE_COUNT; lane++) {
            coop_wire_msg_t back;
            TEST_ASSERT_FALSE(parse_copy(buffer, sizeof(buffer), lane, true, true, &back));
            TEST_ASSERT_FALSE(parse_copy(buffer, sizeof(buffer), lane, false, false, &back));
        }
    }
}

/* Writes a header field by field for packets the writer refuses to build. */
typedef struct crafted {
    uint8_t data[64];
    net_bits_writer_t writer;
} crafted_t;

static void craft_begin(crafted_t *c, coop_wire_kind_t kind) {
    memset(c->data, 0, sizeof(c->data));
    net_bits_writer_init(&c->writer, c->data, sizeof(c->data));
    net_bits_write(&c->writer, (uint32_t)kind, COOP_WIRE_KIND_BITS);
}

static uint32_t craft_end(crafted_t *c, uint32_t extra_bytes) {
    uint32_t size = (uint32_t)net_bits_writer_finish(&c->writer);
    TEST_ASSERT_TRUE(size > 0 && size + extra_bytes <= sizeof(c->data));
    memset(c->data + size, 0xAB, extra_bytes);
    return size + extra_bytes;
}

static void test_field_rules_are_refused(void) {
    crafted_t c;
    coop_wire_msg_t back;
    /* HELLO with another magic. */
    craft_begin(&c, COOP_WIRE_HELLO);
    net_bits_write(&c.writer, 0x12345678, 32);
    net_bits_write(&c.writer, COOP_SESSION_PROTOCOL, 16);
    for (int i = 0; i < 5; i++) {
        net_bits_write(&c.writer, 0, 32);
    }
    net_bits_write(&c.writer, 0, 9);
    TEST_ASSERT_FALSE(parse_routed(c.data, craft_end(&c, 0), COOP_WIRE_HELLO, &back));
    /* HELLO whose world key size is past COOP_WORLD_KEY_MAX. */
    craft_begin(&c, COOP_WIRE_HELLO);
    net_bits_write(&c.writer, COOP_WIRE_MAGIC, 32);
    net_bits_write(&c.writer, COOP_SESSION_PROTOCOL, 16);
    for (int i = 0; i < 5; i++) {
        net_bits_write(&c.writer, 0, 32);
    }
    net_bits_write(&c.writer, 300, 9);
    TEST_ASSERT_FALSE(parse_routed(c.data, craft_end(&c, 0), COOP_WIRE_HELLO, &back));
    /* WELCOME: guest seat 0, a seat past the seats, a session of one seat, no tick rate. */
    const uint32_t welcome[][3] = {{0, 3, 60}, {4, 3, 60}, {1, 0, 60}, {1, 3, 0}}; /* seat, seats - 1, hz */
    for (size_t i = 0; i < sizeof(welcome) / sizeof(welcome[0]); i++) {
        craft_begin(&c, COOP_WIRE_WELCOME);
        net_bits_write(&c.writer, welcome[i][0], 3);
        net_bits_write(&c.writer, welcome[i][1], 3);
        net_bits_write(&c.writer, 0, 32);
        net_bits_write(&c.writer, welcome[i][2], 16);
        net_bits_write(&c.writer, 0, 9);
        TEST_ASSERT_FALSE(parse_routed(c.data, craft_end(&c, 0), COOP_WIRE_WELCOME, &back));
    }
    /* LEFT for the host's seat. */
    craft_begin(&c, COOP_WIRE_LEFT);
    net_bits_write(&c.writer, 0, 3);
    net_bits_write(&c.writer, COOP_CLOSE_LEFT, 32);
    TEST_ASSERT_FALSE(parse_routed(c.data, craft_end(&c, 0), COOP_WIRE_LEFT, &back));
    /* STATE that says it has no ack but carries one. */
    craft_begin(&c, COOP_WIRE_STATE);
    net_bits_write(&c.writer, 1, 16);
    net_bits_write(&c.writer, 1, 32);
    net_bits_write(&c.writer, 0, 1);
    net_bits_write(&c.writer, 5, 16);
    net_bits_write(&c.writer, 0, 32);
    net_bits_write(&c.writer, 0, 16);
    TEST_ASSERT_FALSE(parse_routed(c.data, craft_end(&c, 4), COOP_WIRE_STATE, &back));
    /* BASELINE_FIRST of zero bytes, and one whose piece is larger than its total. */
    craft_begin(&c, COOP_WIRE_BASELINE_FIRST);
    net_bits_write(&c.writer, 0, 32);
    net_bits_write(&c.writer, 0, 32);
    net_bits_write(&c.writer, 0, 32);
    TEST_ASSERT_FALSE(parse_routed(c.data, craft_end(&c, 4), COOP_WIRE_BASELINE_FIRST, &back));
    craft_begin(&c, COOP_WIRE_BASELINE_FIRST);
    net_bits_write(&c.writer, 3, 32);
    net_bits_write(&c.writer, 0, 32);
    net_bits_write(&c.writer, 0, 32);
    TEST_ASSERT_FALSE(parse_routed(c.data, craft_end(&c, 4), COOP_WIRE_BASELINE_FIRST, &back));
    /* Payload kinds with no payload. */
    craft_begin(&c, COOP_WIRE_BASELINE_PIECE);
    TEST_ASSERT_FALSE(parse_routed(c.data, craft_end(&c, 0), COOP_WIRE_BASELINE_PIECE, &back));
    /* A world key one byte short or long of its size. */
    uint8_t buffer[512];
    uint32_t size = write_sample(COOP_WIRE_WELCOME, buffer, sizeof(buffer));
    TEST_ASSERT_FALSE(parse_routed(buffer, size - 1, COOP_WIRE_WELCOME, &back));
    buffer[size] = 1;
    TEST_ASSERT_FALSE(parse_routed(buffer, size + 1, COOP_WIRE_WELCOME, &back));
}

static void test_hello_of_another_protocol_reads_only_its_prefix(void) {
    crafted_t c;
    craft_begin(&c, COOP_WIRE_HELLO);
    net_bits_write(&c.writer, COOP_WIRE_MAGIC, 32);
    net_bits_write(&c.writer, COOP_SESSION_PROTOCOL + 1, 16);
    coop_wire_msg_t back;
    /* Whatever another protocol puts after its number, this one reads the number. */
    TEST_ASSERT_TRUE(parse_routed(c.data, craft_end(&c, 11), COOP_WIRE_HELLO, &back));
    TEST_ASSERT_EQUAL_UINT32(COOP_SESSION_PROTOCOL + 1, back.protocol);
    TEST_ASSERT_TRUE(back.build_key == 0 && back.size == 0);
}

// #endregion

// #region garbage

static void test_seeded_garbage_never_reads_or_writes_outside(void) {
    uint64_t seed = 0xC0FFEE;
    uint8_t arena[GUARD + 600 + GUARD];
    uint32_t accepted = 0;
    for (int round = 0; round < 40000; round++) {
        uint32_t size = coop_test_rand(&seed) % 64;
        if (round % 16 == 0) {
            size = coop_test_rand(&seed) % 600;
        }
        memset(arena, 0x5A, sizeof(arena));
        uint8_t *data = arena + GUARD;
        for (uint32_t i = 0; i < size; i++) {
            data[i] = (uint8_t)coop_test_rand(&seed);
        }
        /* Most garbage dies at the kind; give a valid kind half the time. */
        if (size > 0 && (round & 1)) {
            data[0] = (uint8_t)((data[0] & 0xF0U) | (1U + coop_test_rand(&seed) % KINDS));
        }
        uint8_t lane = (uint8_t)(coop_test_rand(&seed) % 4);
        bool reliable = (coop_test_rand(&seed) & 1U) != 0;
        bool from_host = (coop_test_rand(&seed) & 1U) != 0;
        coop_wire_msg_t back;
        bool ok = coop_wire_parse(size > 0 ? data : NULL, size, lane, reliable, from_host, &back);
        if (ok) {
            accepted++;
            TEST_ASSERT_TRUE(back.kind >= COOP_WIRE_HELLO && back.kind <= COOP_WIRE_BASELINE_PIECE);
            TEST_ASSERT_TRUE(back.size <= size);
            if (back.size > 0) {
                TEST_ASSERT_TRUE(back.bytes >= data && back.bytes + back.size <= data + size);
            }
        }
        for (int i = 0; i < GUARD; i++) {
            TEST_ASSERT_EQUAL_HEX8(0x5A, arena[i]);
            TEST_ASSERT_EQUAL_HEX8(0x5A, arena[GUARD + 600 + i]);
        }
        TEST_ASSERT_EQUAL_HEX8(0x5A, size < 600 ? data[size] : 0x5A);
        coop_wire_msg_t copy_back;
        TEST_ASSERT_EQUAL(ok, parse_copy(data, size, lane, reliable, from_host, &copy_back));
    }
    /* Payload kinds take most bytes, so some garbage must parse; the rules above held for it. */
    TEST_ASSERT_TRUE(accepted > 0);
}

static void test_null_and_empty_are_refused(void) {
    coop_wire_msg_t back;
    uint8_t byte = COOP_WIRE_BASELINE_PIECE;
    TEST_ASSERT_FALSE(coop_wire_parse(NULL, 10, COOP_LANE_BASELINE, true, true, &back));
    TEST_ASSERT_FALSE(coop_wire_parse(&byte, 0, COOP_LANE_BASELINE, true, true, &back));
    TEST_ASSERT_FALSE(coop_wire_parse(&byte, 1, COOP_LANE_BASELINE, true, true, NULL));
    TEST_ASSERT_EQUAL_UINT32(0, coop_wire_write(NULL, &byte, 1));
}

// #endregion

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_every_kind_round_trips);
    RUN_TEST(test_header_sizes_match_the_bits);
    RUN_TEST(test_hello_starts_with_kind_magic_protocol);
    RUN_TEST(test_writer_refusals);
    RUN_TEST(test_every_truncation_is_refused_or_shorter_payload);
    RUN_TEST(test_wrong_lane_reliability_or_direction_is_refused);
    RUN_TEST(test_trailing_bytes_and_set_padding_are_refused);
    RUN_TEST(test_unknown_kinds_are_refused);
    RUN_TEST(test_field_rules_are_refused);
    RUN_TEST(test_hello_of_another_protocol_reads_only_its_prefix);
    RUN_TEST(test_seeded_garbage_never_reads_or_writes_outside);
    RUN_TEST(test_null_and_empty_are_refused);
    return UNITY_END();
}
