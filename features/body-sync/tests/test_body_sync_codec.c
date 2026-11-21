#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sync_codec.h"
#include "unity.h"

/* The real seam's wire: what a record carries, what it costs, and what a reader refuses. */

void setUp(void) {}
void tearDown(void) {}

static sync_codec_t codec;
static const sync_limits_t STAND = {{-40.0f, -15.0f, -40.0f}, {40.0f, 50.0f, 40.0f}, 100.0f, 200.0f};
enum { SEATS = 4, MAX = 256 };
static uint8_t packet[2048];
static sync_record_t out[MAX];

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint32_t next(void) {
    rng = rng * 6364136223846793005ull + 1442695040888963407ull;
    return (uint32_t)(rng >> 33);
}
static float unit(void) { return (float)next() / 2147483648.0f; }

static sync_record_t random_record(void) {
    sync_record_t r;
    memset(&r, 0, sizeof(r));
    r.slot = (uint16_t)(next() % BODY_SYNC_BODIES_MAX);
    r.generation_low = (uint8_t)(next() & SYNC_GEN_MASK);
    r.sim = next() & 1u;
    r.at_rest = (next() % 4u) == 0;
    for (int i = 0; i < 3; i++) {
        r.position[i] = STAND.bounds_min[i] + unit() * (STAND.bounds_max[i] - STAND.bounds_min[i]);
        r.velocity[i] = (unit() - 0.5f) * 80.0f;
        r.angular[i] = (unit() - 0.5f) * 150.0f;
    }
    float q[4] = {unit() - 0.5f, unit() - 0.5f, unit() - 0.5f, unit() - 0.5f};
    float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (int i = 0; i < 4; i++) {
        r.rotation[i] = q[i] / n;
    }
    r.rare.groups = (uint8_t)(next() & SYNC_RARE_ALL);
    r.rare.kinematic = next() & 1u;
    r.rare.locks = (uint8_t)(next() & 0x3Fu);
    r.rare.flags = (uint8_t)(next() & 0x0Fu);
    r.rare.seat = (uint8_t)(next() % SEATS);
    r.rare.number = (uint16_t)next();
    r.rare.hold = (uint8_t)(next() & 1u);
    r.rare.holder_slot = (next() & 1u) ? (uint16_t)(next() % BODY_SYNC_BODIES_MAX) : BODY_SYNC_BODIES_MAX;
    r.rare.holder_generation_low = r.rare.holder_slot < BODY_SYNC_BODIES_MAX ? (uint8_t)(next() & SYNC_GEN_MASK) : 0;
    return r;
}

/* The angle of conj(a) * b from its vector part, in double: acos of a dot near 1 cannot resolve
   the 15-bit grid in float. */
static double angle_between(const float a[4], const float b[4]) {
    double ax = (double)a[0], ay = (double)a[1], az = (double)a[2], aw = (double)a[3];
    double bx = (double)b[0], by = (double)b[1], bz = (double)b[2], bw = (double)b[3];
    double w = aw * bw + ax * bx + ay * by + az * bz;
    double vx = aw * bx - bw * ax - (ay * bz - az * by);
    double vy = aw * by - bw * ay - (az * bx - ax * bz);
    double vz = aw * bz - bw * az - (ax * by - ay * bx);
    return 2.0 * atan2(sqrt(vx * vx + vy * vy + vz * vz), fabs(w));
}

static uint32_t write_packet(const sync_record_t *records, uint32_t n, uint32_t capacity) {
    sync_packet_writer_t w;
    sync_packet_begin(&w, packet, capacity);
    for (uint32_t i = 0; i < n; i++) {
        TEST_ASSERT_TRUE(sync_packet_add(&w, &codec, &records[i]));
    }
    return sync_packet_finish(&w);
}

static bool read_packet(uint32_t size, uint32_t *count) {
    char why[96];
    return sync_packet_read(&codec, SEATS, packet, size, out, MAX, count, why, sizeof(why));
}

static void test_widths_follow_the_limits(void) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    /* 80 m at 1/512 m is 40961 points: 16 bits; a copy record sits near 11 bytes. */
    TEST_ASSERT_TRUE(codec.record_bits_min > 80 && codec.record_bits_min < 104);
    TEST_ASSERT_TRUE(codec.record_bits_max <= BODY_SYNC_RECORD_MAX * 8u);
    sync_limits_t small = STAND;
    small.bounds_min[1] = -1.0f;
    small.bounds_max[1] = 7.0f;
    sync_codec_t other;
    TEST_ASSERT_TRUE(sync_codec_init(&other, &small));
    TEST_ASSERT_TRUE(other.record_bits_min < codec.record_bits_min);
    TEST_ASSERT_TRUE(other.hash != codec.hash);
    /* Limits the wire cannot hold make create fail. */
    sync_limits_t wide = STAND;
    wide.bounds_max[0] = 10000.0f;
    TEST_ASSERT_FALSE(sync_codec_init(&other, &wide));
    sync_limits_t broken = STAND;
    broken.max_speed = NAN;
    TEST_ASSERT_FALSE(sync_codec_init(&other, &broken));
    broken = STAND;
    broken.bounds_min[2] = broken.bounds_max[2];
    TEST_ASSERT_FALSE(sync_codec_init(&other, &broken));
}

/* Every kind and every rare mask reads back as quantize says, within half a step of the value. */
static void test_records_round_trip_within_their_steps(void) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    for (int trial = 0; trial < 400; trial++) {
        sync_record_t in[8], q[8];
        uint32_t n = 1 + (uint32_t)(next() % 8u);
        for (uint32_t i = 0; i < n; i++) {
            in[i] = random_record();
            TEST_ASSERT_TRUE(sync_codec_limit(&codec, &in[i]));
            q[i] = in[i];
            sync_codec_quantize(&codec, &q[i]);
        }
        uint32_t size = write_packet(in, n, sizeof(packet));
        uint32_t count = 0;
        TEST_ASSERT_TRUE(read_packet(size, &count));
        TEST_ASSERT_EQUAL_UINT32(n, count);
        for (uint32_t i = 0; i < n; i++) {
            TEST_ASSERT_EQUAL_MEMORY(&q[i], &out[i], sizeof(sync_record_t));
            float step = in[i].sim ? SYNC_SIM_POSITION_STEP : SYNC_COPY_POSITION_STEP;
            for (int k = 0; k < 3; k++) {
                TEST_ASSERT_TRUE(fabsf(out[i].position[k] - in[i].position[k]) <= 0.5f * step + 1e-5f);
                if (in[i].sim && !in[i].at_rest) {
                    TEST_ASSERT_TRUE(fabsf(out[i].velocity[k] - in[i].velocity[k]) <= 0.5f * SYNC_VELOCITY_STEP + 1e-4f);
                } else {
                    TEST_ASSERT_TRUE(out[i].velocity[k] == 0.0f);
                }
            }
            double bound = (double)net_bits_quat_error(in[i].sim ? SYNC_SIM_QUAT_BITS : SYNC_COPY_QUAT_BITS);
            TEST_ASSERT_TRUE(angle_between(in[i].rotation, out[i].rotation) <= bound + 1e-6);
            TEST_ASSERT_EQUAL_UINT8(in[i].rare.groups, out[i].rare.groups);
            /* A record is only as long as what it carries. */
            TEST_ASSERT_TRUE(sync_codec_record_bits(&codec, &in[i]) <= codec.record_bits_max);
        }
    }
}

/* Positions, velocities and spins at and past the limits are brought inside before writing;
   a pose that is not a number is not written at all. */
static void test_the_limit_keeps_every_value_readable(void) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    sync_record_t r = random_record();
    r.sim = true;
    r.at_rest = false;
    r.position[0] = 1e9f;
    r.position[1] = -1e9f;
    r.velocity[0] = 1e6f;
    r.angular[2] = -1e6f;
    TEST_ASSERT_TRUE(sync_codec_limit(&codec, &r));
    uint32_t size = write_packet(&r, 1, sizeof(packet)), count = 0;
    TEST_ASSERT_TRUE(read_packet(size, &count));
    TEST_ASSERT_TRUE(out[0].position[0] <= STAND.bounds_max[0] && out[0].position[1] >= STAND.bounds_min[1]);
    r = random_record();
    r.sim = true;
    r.at_rest = false;
    r.velocity[1] = NAN;
    TEST_ASSERT_TRUE(sync_codec_limit(&codec, &r));
    TEST_ASSERT_TRUE(r.velocity[0] == 0.0f && r.velocity[1] == 0.0f);
    r.position[2] = INFINITY;
    TEST_ASSERT_FALSE(sync_codec_limit(&codec, &r));
    r = random_record();
    r.rotation[3] = NAN;
    TEST_ASSERT_FALSE(sync_codec_limit(&codec, &r));
}

/* A record never splits: one that does not fit what is left is refused whole. */
static void test_a_record_never_splits(void) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    sync_record_t big = random_record();
    big.sim = true;
    big.at_rest = false;
    big.rare.groups = SYNC_RARE_ALL;
    TEST_ASSERT_TRUE(sync_codec_limit(&codec, &big));
    TEST_ASSERT_EQUAL_UINT32(codec.record_bits_max, sync_codec_record_bits(&codec, &big));
    uint32_t exact = (codec.record_bits_max + 1u + 7u) / 8u; /* one record and the end bit */
    sync_packet_writer_t w;
    sync_packet_begin(&w, packet, exact - 1u);
    TEST_ASSERT_FALSE(sync_packet_add(&w, &codec, &big));
    TEST_ASSERT_EQUAL_UINT32(0, sync_packet_finish(&w));
    sync_packet_begin(&w, packet, exact);
    TEST_ASSERT_TRUE(sync_packet_add(&w, &codec, &big));
    TEST_ASSERT_FALSE(sync_packet_add(&w, &codec, &big));
    uint32_t size = sync_packet_finish(&w), count = 0;
    TEST_ASSERT_TRUE(size <= exact);
    TEST_ASSERT_TRUE(read_packet(size, &count));
    TEST_ASSERT_EQUAL_UINT32(1, count);
}

/* Truncations, set padding, missing end bits, out-of-range codes and seats, an empty rare
   mask and too many records are refused whole. */
static void test_the_reader_refuses_malformed_packets(void) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    sync_record_t r[3];
    for (int i = 0; i < 3; i++) {
        r[i] = random_record();
        r[i].rare.groups = SYNC_RARE_ALL;
        TEST_ASSERT_TRUE(sync_codec_limit(&codec, &r[i]));
    }
    uint32_t size = write_packet(r, 3, sizeof(packet)), count = 0;
    TEST_ASSERT_TRUE(read_packet(size, &count));
    for (uint32_t cut = 0; cut < size; cut++) {
        TEST_ASSERT_FALSE(read_packet(cut, &count));
        TEST_ASSERT_EQUAL_UINT32(0, count);
    }
    packet[size] = 0;
    TEST_ASSERT_FALSE(read_packet(size + 1, &count)); /* a spare byte */
    char why[96];
    /* Fewer records allowed than sent. */
    TEST_ASSERT_FALSE(sync_packet_read(&codec, SEATS, packet, size, out, 2, &count, why, sizeof(why)));
    /* Fewer seats than the authority group names. */
    sync_record_t s = random_record();
    s.rare.groups = SYNC_RARE_AUTHORITY;
    s.rare.seat = 3;
    TEST_ASSERT_TRUE(sync_codec_limit(&codec, &s));
    size = write_packet(&s, 1, sizeof(packet));
    TEST_ASSERT_TRUE(sync_packet_read(&codec, 4, packet, size, out, MAX, &count, why, sizeof(why)));
    TEST_ASSERT_FALSE(sync_packet_read(&codec, 3, packet, size, out, MAX, &count, why, sizeof(why)));
    /* An empty rare mask: a rare bit and four zero bits. Built by hand from the head's layout. */
    net_bits_writer_t w;
    net_bits_writer_init(&w, packet, sizeof(packet));
    net_bits_write(&w, 1, 1);
    net_bits_write(&w, 5, SYNC_SLOT_BITS);
    net_bits_write(&w, 1, SYNC_GEN_BITS);
    net_bits_write(&w, 0, 1);
    net_bits_write(&w, 0, 1);
    net_bits_write(&w, 1, 1);
    float pose[4] = {0, 0, 0, 1};
    net_bits_write_ranged(&w, codec.copy_fields[0].range, 0.0f);
    net_bits_write_ranged(&w, codec.copy_fields[1].range, 0.0f);
    net_bits_write_ranged(&w, codec.copy_fields[2].range, 0.0f);
    net_bits_write_quat(&w, pose, SYNC_COPY_QUAT_BITS);
    net_bits_write(&w, 0, SYNC_RARE_GROUPS);
    net_bits_write(&w, 0, 1);
    size = (uint32_t)net_bits_writer_finish(&w);
    TEST_ASSERT_FALSE(read_packet(size, &count));
    /* A velocity past the limit by a step or more is refused; the writer never sends one. */
    net_bits_writer_init(&w, packet, sizeof(packet));
    net_bits_write(&w, 1, 1);
    net_bits_write(&w, 5, SYNC_SLOT_BITS);
    net_bits_write(&w, 1, SYNC_GEN_BITS);
    net_bits_write(&w, 1, 1);
    net_bits_write(&w, 0, 1);
    net_bits_write(&w, 0, 1);
    for (int i = 0; i < 3; i++) {
        net_bits_write_ranged(&w, codec.sim_fields[i].range, 0.0f);
    }
    net_bits_write_quat(&w, pose, SYNC_SIM_QUAT_BITS);
    for (int i = 0; i < 6; i++) {
        net_bits_write_ranged(&w, codec.motion_fields[i].range, i < 3 ? 99.0f : 0.0f);
    }
    net_bits_write(&w, 0, 1);
    size = (uint32_t)net_bits_writer_finish(&w);
    TEST_ASSERT_FALSE(read_packet(size, &count));
    TEST_ASSERT_FALSE(read_packet(0, &count));
}

/* Bit flips and random bytes: the reader answers, never reads outside the bytes, and every
   record it accepts is inside the limits. Millions more run under the sanitizers. */
static void test_fuzz(void) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    uint8_t valid[2048];
    for (int round = 0; round < 20000; round++) {
        sync_record_t r[4];
        uint32_t n = 1 + (uint32_t)(next() % 4u);
        for (uint32_t i = 0; i < n; i++) {
            r[i] = random_record();
            TEST_ASSERT_TRUE(sync_codec_limit(&codec, &r[i]));
        }
        uint32_t size = write_packet(r, n, sizeof(packet));
        memcpy(valid, packet, size);
        uint32_t flips = 1 + next() % 4u;
        for (uint32_t f = 0; f < flips; f++) {
            packet[next() % size] ^= (uint8_t)(1u << (next() % 8u));
        }
        uint32_t cut = (next() % 3u) == 0 ? next() % (size + 1u) : size;
        uint32_t count = 0;
        if (read_packet(cut, &count)) {
            for (uint32_t i = 0; i < count; i++) {
                for (int k = 0; k < 3; k++) {
                    TEST_ASSERT_TRUE(out[i].position[k] >= STAND.bounds_min[k] && out[i].position[k] <= STAND.bounds_max[k]);
                }
                float v2 = out[i].velocity[0] * out[i].velocity[0] + out[i].velocity[1] * out[i].velocity[1] + out[i].velocity[2] * out[i].velocity[2];
                TEST_ASSERT_TRUE(v2 < (STAND.max_speed + SYNC_VELOCITY_STEP) * (STAND.max_speed + SYNC_VELOCITY_STEP));
                TEST_ASSERT_TRUE(out[i].slot < BODY_SYNC_BODIES_MAX && out[i].rare.seat < SEATS);
            }
        } else {
            TEST_ASSERT_EQUAL_UINT32(0, count);
        }
        for (uint32_t i = 0; i < 64; i++) {
            packet[i] = (uint8_t)next();
        }
        read_packet(1 + next() % 64u, &count);
    }
}

/* A decoded value never lies outside its limit. Encoded again, positions, velocities and rare
   groups give the same values; a rotation the same rotation within its step (a tie of its two
   largest components may change which one is dropped). */
static void test_values_stay_inside_and_honest_packets_reencode(void) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    for (int trial = 0; trial < 2000; trial++) {
        sync_record_t r = random_record();
        r.sim = true;
        r.at_rest = false;
        for (int k = 0; k < 3; k++) {
            r.position[k] = (trial & 1) ? STAND.bounds_max[k] : STAND.bounds_min[k];
            r.velocity[k] = (k == trial % 3) ? 1e6f : 99.99f;
            r.angular[k] = 199.99f;
        }
        TEST_ASSERT_TRUE(sync_codec_limit(&codec, &r));
        uint32_t size = write_packet(&r, 1, sizeof(packet)), count = 0;
        TEST_ASSERT_TRUE(read_packet(size, &count));
        uint8_t first[64];
        memcpy(first, packet, size);
        for (int k = 0; k < 3; k++) {
            TEST_ASSERT_TRUE(out[0].position[k] >= STAND.bounds_min[k] && out[0].position[k] <= STAND.bounds_max[k]);
        }
        const float *v = out[0].velocity, *w = out[0].angular;
        TEST_ASSERT_TRUE(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] <= STAND.max_speed * STAND.max_speed);
        TEST_ASSERT_TRUE(w[0] * w[0] + w[1] * w[1] + w[2] * w[2] <= STAND.max_spin * STAND.max_spin);
        sync_record_t again = out[0];
        TEST_ASSERT_TRUE(sync_codec_limit(&codec, &again));
        size = write_packet(&again, 1, sizeof(packet));
        TEST_ASSERT_TRUE(read_packet(size, &count));
        TEST_ASSERT_EQUAL_MEMORY(again.position, out[0].position, sizeof(again.position));
        TEST_ASSERT_EQUAL_MEMORY(again.velocity, out[0].velocity, sizeof(again.velocity));
        TEST_ASSERT_EQUAL_MEMORY(again.angular, out[0].angular, sizeof(again.angular));
        TEST_ASSERT_EQUAL_MEMORY(&again.rare, &out[0].rare, sizeof(again.rare));
        TEST_ASSERT_TRUE(angle_between(again.rotation, out[0].rotation) <= (double)net_bits_quat_error(SYNC_SIM_QUAT_BITS) + 1e-6);
        (void)first;
    }
}

/* Where the box may lie: every coordinate below 2048 m in magnitude. */
static void test_the_box_rule_holds_at_its_edges(void) {
    sync_codec_t c;
    sync_limits_t l = STAND;
    l.bounds_max[0] = 2047.99f;
    TEST_ASSERT_TRUE(sync_codec_init(&c, &l));
    l.bounds_max[0] = 2048.0f;
    TEST_ASSERT_FALSE(sync_codec_init(&c, &l));
    l = STAND;
    l.bounds_min[2] = -2047.99f;
    TEST_ASSERT_TRUE(sync_codec_init(&c, &l));
    l.bounds_min[2] = -2048.0f;
    TEST_ASSERT_FALSE(sync_codec_init(&c, &l));
    l = STAND;
    l.max_speed = 1.0f / 64.0f; /* below one step */
    TEST_ASSERT_FALSE(sync_codec_init(&c, &l));
}

/* The room a record needs includes the end bit: whatever the capacity, an added record leaves a
   packet that finishes and decodes. A holder group naming no holder carries no bits. An end bit
   alone is a legal, empty packet. */
static void test_room_holder_bits_and_the_empty_packet(void) {
    TEST_ASSERT_TRUE(sync_codec_init(&codec, &STAND));
    for (int trial = 0; trial < 3000; trial++) {
        sync_record_t r = random_record();
        TEST_ASSERT_TRUE(sync_codec_limit(&codec, &r));
        uint32_t cap = (sync_codec_record_bits(&codec, &r) + 7u) / 8u + (uint32_t)(trial % 2);
        sync_packet_writer_t w;
        sync_packet_begin(&w, packet, cap);
        if (sync_packet_add(&w, &codec, &r)) {
            uint32_t size = sync_packet_finish(&w), count = 0;
            TEST_ASSERT_TRUE_MESSAGE(size > 0 && size <= cap && read_packet(size, &count) && count == 1, "an added record left a packet that does not finish");
        }
    }
    net_bits_writer_t w;
    net_bits_writer_init(&w, packet, sizeof(packet));
    net_bits_write(&w, 1, 1);
    net_bits_write(&w, 5, SYNC_SLOT_BITS);
    net_bits_write(&w, 1, SYNC_GEN_BITS);
    net_bits_write(&w, 0, 1);
    net_bits_write(&w, 0, 1);
    net_bits_write(&w, 1, 1);
    float pose[4] = {0, 0, 0, 1};
    for (int i = 0; i < 3; i++) {
        net_bits_write_ranged(&w, codec.copy_fields[i].range, 0.0f);
    }
    net_bits_write_quat(&w, pose, SYNC_COPY_QUAT_BITS);
    net_bits_write(&w, SYNC_RARE_HOLDER, SYNC_RARE_GROUPS);
    net_bits_write(&w, 0, 1);  /* no holder */
    net_bits_write(&w, 7, SYNC_SLOT_BITS); /* yet a slot */
    net_bits_write(&w, 0, SYNC_GEN_BITS);
    net_bits_write(&w, 0, 1);
    uint32_t size = (uint32_t)net_bits_writer_finish(&w), count = 0;
    TEST_ASSERT_FALSE(read_packet(size, &count));
    packet[0] = 0;
    TEST_ASSERT_TRUE(read_packet(1, &count));
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

/* A box whose width is no whole number of steps: the grid still ends inside it. */
static void test_a_grid_ends_inside_an_odd_box(void) {
    sync_codec_t c;
    sync_limits_t l = STAND;
    l.bounds_min[0] = 0.3f;
    l.bounds_max[0] = 0.31f;
    TEST_ASSERT_TRUE(sync_codec_init(&c, &l));
    for (int sim = 0; sim < 2; sim++) {
        sync_record_t r;
        memset(&r, 0, sizeof(r));
        r.sim = sim != 0;
        r.position[0] = 0.31f;
        r.rotation[3] = 1.0f;
        TEST_ASSERT_TRUE(sync_codec_limit(&c, &r));
        sync_packet_writer_t w;
        sync_packet_begin(&w, packet, sizeof(packet));
        TEST_ASSERT_TRUE(sync_packet_add(&w, &c, &r));
        uint32_t size = sync_packet_finish(&w), count = 0;
        TEST_ASSERT_TRUE(sync_packet_read(&c, SEATS, packet, size, out, MAX, &count, NULL, 0));
        TEST_ASSERT_TRUE_MESSAGE(out[0].position[0] <= 0.31f && out[0].position[0] >= 0.3f, "the grid ends past the box");
        TEST_ASSERT_TRUE(out[0].position[0] > 0.31f - (sim ? SYNC_SIM_POSITION_STEP : SYNC_COPY_POSITION_STEP));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_widths_follow_the_limits);
    RUN_TEST(test_records_round_trip_within_their_steps);
    RUN_TEST(test_the_limit_keeps_every_value_readable);
    RUN_TEST(test_a_record_never_splits);
    RUN_TEST(test_the_reader_refuses_malformed_packets);
    RUN_TEST(test_fuzz);
    RUN_TEST(test_values_stay_inside_and_honest_packets_reencode);
    RUN_TEST(test_the_box_rule_holds_at_its_edges);
    RUN_TEST(test_room_holder_bits_and_the_empty_packet);
    RUN_TEST(test_a_grid_ends_inside_an_odd_box);
    return UNITY_END();
}
