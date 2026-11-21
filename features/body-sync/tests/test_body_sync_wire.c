#include <math.h>
#include <string.h>

#include "stub_wire.h"
#include "unity.h"

/* The stub's codec alone, without Box3D, so the parsers of peer bytes run under sanitizers. */

void setUp(void) {}
void tearDown(void) {}

enum { SEATS = 4, MAX = 64 };

static const stub_limits_t limits = {.bounds_min = {-40.0f, -15.0f, -40.0f}, .bounds_max = {40.0f, 50.0f, 40.0f}, .max_speed = 100.0f, .max_spin = 200.0f};

static stub_body_state_t bodies[MAX];
static uint8_t packet[STUB_HEADER_BYTES + MAX * STUB_BODY_BYTES + 16];

static uint64_t next(uint64_t *seed) {
    *seed = *seed * 6364136223846793005ull + 1442695040888963407ull;
    return *seed >> 17;
}

static uint32_t valid_packet(uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        stub_body_state_t *b = &bodies[i];
        memset(b, 0, sizeof(*b));
        b->id = (i + 1u) | (1u << 16);
        b->seat = (uint8_t)(i % SEATS);
        b->number = (uint16_t)(i * 7u);
        b->hold = (uint8_t)(i & 1u);
        b->holder = i % 3u == 0 ? 0u : (i | (2u << 16));
        b->flags = (uint8_t)(i & 0x0Fu);
        b->kinematic = (i & 4u) != 0;
        b->asleep = (i & 2u) != 0;
        b->locks = (uint8_t)((i * 11u) & STUB_LOCKS_ALL);
        b->position[0] = (float)i * 0.5f;
        b->position[1] = 1.0f;
        b->rotation[3] = 1.0f;
        b->velocity[2] = -3.0f;
        stub_wire_put_body(packet + STUB_HEADER_BYTES + i * STUB_BODY_BYTES, b);
    }
    stub_wire_put_header(packet, (uint16_t)count);
    return STUB_HEADER_BYTES + count * STUB_BODY_BYTES;
}

static bool read_one(uint32_t size, stub_body_state_t *out) {
    uint32_t count = 0;
    char why[96];
    return stub_wire_read_state(packet, size, SEATS, &limits, out, MAX, &count, why, sizeof(why));
}

/* Field offsets inside the first body of a packet. */
enum { AT_FLAGS = STUB_HEADER_BYTES + 4, AT_LOCKS = AT_FLAGS + 1, AT_ROT = AT_LOCKS + 1 + 12, AT_VEL = AT_ROT + 16, AT_SEAT = AT_VEL + 24, AT_HOLD = AT_SEAT + 3, AT_HOLDER = AT_SEAT + 4 };

static void put_f32(uint8_t *p, float f) { memcpy(p, &f, 4); }

static void test_state_round_trips(void) {
    uint32_t size = valid_packet(17);
    TEST_ASSERT_EQUAL_UINT32(17, stub_wire_bodies_fit(size));
    stub_body_state_t out[MAX];
    uint32_t count = 0;
    char why[96];
    TEST_ASSERT_TRUE_MESSAGE(stub_wire_read_state(packet, size, SEATS, &limits, out, MAX, &count, why, sizeof(why)), why);
    TEST_ASSERT_EQUAL_UINT32(17, count);
    for (uint32_t i = 0; i < count; i++) {
        TEST_ASSERT_EQUAL_UINT32(bodies[i].id, out[i].id);
        TEST_ASSERT_EQUAL_UINT8(bodies[i].seat, out[i].seat);
        TEST_ASSERT_EQUAL_UINT16(bodies[i].number, out[i].number);
        TEST_ASSERT_EQUAL_UINT8(bodies[i].hold, out[i].hold);
        TEST_ASSERT_EQUAL_UINT32(bodies[i].holder, out[i].holder);
        TEST_ASSERT_EQUAL_UINT8(bodies[i].flags, out[i].flags);
        TEST_ASSERT_EQUAL(bodies[i].kinematic, out[i].kinematic);
        TEST_ASSERT_EQUAL(bodies[i].asleep, out[i].asleep);
        TEST_ASSERT_EQUAL_UINT8(bodies[i].locks, out[i].locks);
        TEST_ASSERT_EQUAL_MEMORY(bodies[i].position, out[i].position, sizeof(out[i].position));
        TEST_ASSERT_EQUAL_MEMORY(bodies[i].velocity, out[i].velocity, sizeof(out[i].velocity));
    }
    TEST_ASSERT_FALSE(stub_wire_read_state(packet, size, SEATS, &limits, out, 16, &count, why, sizeof(why)));
}

/* What the writer is about to send always passes the reader, whatever the body did: past the
   box, too fast, or with a velocity that is not a number. A pose that is not a number is not
   sent at all. */
static void test_the_writers_limit_never_hands_the_reader_a_refusal(void) {
    const float huge = 3.0e38f;
    const float bad[] = {NAN, INFINITY, -INFINITY};
    stub_body_state_t out[MAX];
    uint32_t count = 0;
    char why[96];
    valid_packet(1);
    for (uint32_t trial = 0; trial < 64; trial++) {
        stub_body_state_t body = bodies[0];
        const uint32_t axis = trial % 3;
        const float wild = (trial & 4) ? huge : -huge;
        if (trial < 16) {
            body.position[axis] = wild;
        } else if (trial < 32) {
            body.velocity[axis] = wild;
            body.angular[(axis + 1) % 3] = -wild;
        } else if (trial < 48) {
            body.velocity[axis] = bad[trial % 3];
        } else {
            body.angular[axis] = bad[trial % 3];
            body.position[(axis + 1) % 3] = wild;
        }
        TEST_ASSERT_TRUE(stub_wire_limit(&limits, &body));
        stub_wire_put_header(packet, 1);
        stub_wire_put_body(packet + STUB_HEADER_BYTES, &body);
        TEST_ASSERT_TRUE_MESSAGE(stub_wire_read_state(packet, STUB_HEADER_BYTES + STUB_BODY_BYTES, SEATS, &limits, out, MAX, &count, why, sizeof(why)), why);
        for (int i = 0; i < 3; i++) {
            TEST_ASSERT_TRUE(out[0].position[i] >= limits.bounds_min[i] && out[0].position[i] <= limits.bounds_max[i]);
        }
    }
    for (uint32_t i = 0; i < 3; i++) {
        stub_body_state_t body = bodies[0];
        body.position[i] = bad[i];
        TEST_ASSERT_FALSE(stub_wire_limit(&limits, &body));
        body = bodies[0];
        body.rotation[i + 1] = bad[i];
        TEST_ASSERT_FALSE(stub_wire_limit(&limits, &body));
    }
    /* A body inside the limits is left as it is. */
    stub_body_state_t same = bodies[0];
    TEST_ASSERT_TRUE(stub_wire_limit(&limits, &same));
    TEST_ASSERT_EQUAL_MEMORY(&bodies[0], &same, sizeof(same));
}

/* Each field's own limit, one at a time, against an otherwise valid body. */
static void test_each_field_is_bounded(void) {
    stub_body_state_t out[MAX];
    uint32_t size = valid_packet(1);
    TEST_ASSERT_TRUE(read_one(size, out));

    for (uint8_t bit = 0x20; bit && bit <= 0x40; bit = (uint8_t)(bit << 1)) {
        valid_packet(1);
        packet[AT_FLAGS] |= bit; /* reserved */
        TEST_ASSERT_FALSE(read_one(size, out));
    }
    valid_packet(1);
    packet[AT_LOCKS] = STUB_LOCKS_ALL;
    TEST_ASSERT_TRUE(read_one(size, out));
    TEST_ASSERT_EQUAL_UINT8(STUB_LOCKS_ALL, out[0].locks);
    for (uint8_t bit = 0x40; bit; bit = (uint8_t)(bit << 1)) {
        valid_packet(1);
        packet[AT_LOCKS] |= bit; /* reserved */
        TEST_ASSERT_FALSE(read_one(size, out));
    }
    valid_packet(1);
    packet[AT_SEAT] = SEATS - 1;
    TEST_ASSERT_TRUE(read_one(size, out));
    packet[AT_SEAT] = SEATS; /* one past the last seat */
    TEST_ASSERT_FALSE(read_one(size, out));
    valid_packet(1);
    packet[AT_HOLD] = 2;
    TEST_ASSERT_FALSE(read_one(size, out));
    valid_packet(1);
    const uint8_t bad_holder[4] = {0x00, 0x04, 0x01, 0x00}; /* slot 1024 */
    memcpy(packet + AT_HOLDER, bad_holder, 4);
    TEST_ASSERT_FALSE(read_one(size, out));
    const uint8_t no_generation[4] = {0x05, 0x00, 0x00, 0x00};
    memcpy(packet + AT_HOLDER, no_generation, 4);
    TEST_ASSERT_FALSE(read_one(size, out));

    /* A rotation inside the tolerance is read back unit; one outside is refused. */
    valid_packet(1);
    put_f32(packet + AT_ROT + 12, 1.009f);
    TEST_ASSERT_TRUE(read_one(size, out));
    float n = out[0].rotation[0] * out[0].rotation[0] + out[0].rotation[1] * out[0].rotation[1] + out[0].rotation[2] * out[0].rotation[2] +
              out[0].rotation[3] * out[0].rotation[3];
    TEST_ASSERT_TRUE(fabsf(n - 1.0f) < 1e-6f);
    put_f32(packet + AT_ROT + 12, 1.0f);
    put_f32(packet + AT_ROT, 0.7f); /* squared length 1.49 */
    TEST_ASSERT_FALSE(read_one(size, out));
    put_f32(packet + AT_ROT, 0.0f);
    put_f32(packet + AT_ROT + 12, 0.985f); /* squared length 0.970 */
    TEST_ASSERT_FALSE(read_one(size, out));

    /* The config's box and speeds, not a fixed constant. */
    valid_packet(1);
    put_f32(packet + AT_LOCKS + 1, 39.9f);
    TEST_ASSERT_TRUE(read_one(size, out));
    put_f32(packet + AT_LOCKS + 1, 40.1f);
    TEST_ASSERT_FALSE(read_one(size, out));
    put_f32(packet + AT_LOCKS + 1, NAN);
    TEST_ASSERT_FALSE(read_one(size, out));
    valid_packet(1);
    put_f32(packet + AT_VEL, 100.0f); /* exactly the limit is still inside */
    put_f32(packet + AT_VEL + 8, 0.0f);
    TEST_ASSERT_TRUE(read_one(size, out));
    put_f32(packet + AT_VEL, 99.0f);
    TEST_ASSERT_TRUE(read_one(size, out));
    put_f32(packet + AT_VEL + 4, 20.0f); /* length above 100 */
    TEST_ASSERT_FALSE(read_one(size, out));
    valid_packet(1);
    put_f32(packet + AT_VEL + 12, 201.0f);
    TEST_ASSERT_FALSE(read_one(size, out));
}

static void test_records_round_trip_and_keep_their_direction(void) {
    const stub_record_t records[] = {
        {.type = STUB_REC_CLAIM, .id = 5u | (1u << 16), .hold = 1, .sequence = 9},
        {.type = STUB_REC_RELEASE, .id = 6u | (2u << 16)},
        {.type = STUB_REC_ASSIGN, .id = 7u | (3u << 16), .seat = 2, .number = 900, .hold = 1, .sequence = 4},
    };
    uint8_t buffer[16];
    stub_record_t out;
    char why[96];
    for (int i = 0; i < 3; i++) {
        uint32_t size = stub_wire_put_record(&records[i], buffer, sizeof(buffer));
        TEST_ASSERT_TRUE(size > 0 && size <= STUB_RECORD_BYTES_MAX);
        bool from_host = records[i].type == STUB_REC_ASSIGN;
        TEST_ASSERT_TRUE_MESSAGE(stub_wire_read_record(buffer, size, SEATS, from_host, &out, why, sizeof(why)), why);
        TEST_ASSERT_EQUAL(records[i].type, out.type);
        TEST_ASSERT_EQUAL_UINT32(records[i].id, out.id);
        TEST_ASSERT_EQUAL_UINT16(records[i].number, out.number);
        TEST_ASSERT_EQUAL_UINT8(records[i].hold, out.hold);
        TEST_ASSERT_EQUAL_UINT16(records[i].sequence, out.sequence);
        TEST_ASSERT_EQUAL_UINT8(records[i].seat, out.seat);
        TEST_ASSERT_FALSE(stub_wire_read_record(buffer, size, SEATS, !from_host, &out, why, sizeof(why)));
        for (uint32_t cut = 0; cut < size; cut++) {
            TEST_ASSERT_FALSE(stub_wire_read_record(buffer, cut, SEATS, from_host, &out, why, sizeof(why)));
        }
        TEST_ASSERT_EQUAL_UINT32(0, stub_wire_put_record(&records[i], buffer, size - 1));
    }
    stub_record_t assign = records[2];
    assign.seat = SEATS;
    uint32_t size = stub_wire_put_record(&assign, buffer, sizeof(buffer));
    TEST_ASSERT_FALSE(stub_wire_read_record(buffer, size, SEATS, true, &out, why, sizeof(why)));
    stub_record_t claim = records[0];
    claim.sequence = 0;
    size = stub_wire_put_record(&claim, buffer, sizeof(buffer));
    TEST_ASSERT_FALSE(stub_wire_read_record(buffer, size, SEATS, false, &out, why, sizeof(why)));
}

/* Every truncation, seeded bit flips and random bytes: refused or read, and what is read is in
   range. Run under ASan and UBSan, this also proves no read leaves the bytes. */
static void test_fuzz(void) {
    uint32_t size = valid_packet(12);
    uint8_t valid[sizeof(packet)];
    memcpy(valid, packet, size);
    stub_body_state_t out[MAX];
    uint32_t count;
    char why[96];
    for (uint32_t cut = 0; cut < size; cut++) {
        TEST_ASSERT_FALSE(stub_wire_read_state(valid, cut, SEATS, &limits, out, MAX, &count, why, sizeof(why)));
        TEST_ASSERT_EQUAL_UINT32(0, count);
    }
    uint64_t seed = 77;
    for (int i = 0; i < 20000; i++) {
        memcpy(packet, valid, size);
        int flips = 1 + (int)(next(&seed) % 4);
        for (int f = 0; f < flips; f++) {
            packet[next(&seed) % size] ^= (uint8_t)(1u << (next(&seed) % 8));
        }
        if (stub_wire_read_state(packet, size, SEATS, &limits, out, MAX, &count, why, sizeof(why))) {
            for (uint32_t k = 0; k < count; k++) {
                TEST_ASSERT_TRUE((out[k].id & 0xFFFFu) < STUB_SLOTS && (out[k].id >> 16) != 0 && out[k].seat < SEATS && out[k].hold <= 1);
                TEST_ASSERT_TRUE(out[k].position[0] >= limits.bounds_min[0] && out[k].position[0] <= limits.bounds_max[0]);
                float n = out[k].rotation[0] * out[k].rotation[0] + out[k].rotation[1] * out[k].rotation[1] +
                          out[k].rotation[2] * out[k].rotation[2] + out[k].rotation[3] * out[k].rotation[3];
                TEST_ASSERT_TRUE(fabsf(n - 1.0f) < 1e-5f);
            }
        } else {
            TEST_ASSERT_EQUAL_UINT32(0, count);
        }
    }
    for (int i = 0; i < 20000; i++) {
        uint32_t n = (uint32_t)(next(&seed) % sizeof(packet));
        for (uint32_t k = 0; k < n; k++) {
            packet[k] = (uint8_t)next(&seed);
        }
        if (i % 3 == 0 && n >= STUB_HEADER_BYTES) {
            packet[0] = STUB_WIRE_VERSION; /* get past the first gate now and then */
        }
        (void)stub_wire_read_state(packet, n, SEATS, &limits, out, MAX, &count, why, sizeof(why));
        stub_record_t record;
        (void)stub_wire_read_record(packet, n % 12, SEATS, (i & 1) != 0, &record, why, sizeof(why));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_state_round_trips);
    RUN_TEST(test_the_writers_limit_never_hands_the_reader_a_refusal);
    RUN_TEST(test_each_field_is_bounded);
    RUN_TEST(test_records_round_trip_and_keep_their_direction);
    RUN_TEST(test_fuzz);
    return UNITY_END();
}
