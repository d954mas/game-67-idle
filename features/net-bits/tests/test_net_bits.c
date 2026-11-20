#include "features/net_bits/net_bits.h"

/* Before unity.h: its noreturn macro breaks the CRT's __declspec(noreturn). */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "net_bits_test_support.h"
#include "unity.h"

static char note[256];
#define CHECK(cond, ...)                                                                                               \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            snprintf(note, sizeof note, __VA_ARGS__);                                                                  \
            TEST_FAIL_MESSAGE(note);                                                                                   \
        }                                                                                                              \
    } while (0)

void setUp(void) {}
void tearDown(void) {}

static const net_bits_range_t RANGES[] = {
    {0.0F, 1.0F, 0.001F},
    {-10.0F, 10.0F, 0.3F},             /* span not a multiple of the step */
    {-1000.0F, 1.0e6F, 7.77F},         /* negative min, wide span */
    {-0.001F, 0.002F, 3.0e-7F},        /* tiny step */
    {-1.0e9F, 1.0e9F, 1.0e5F},         /* huge step */
    {-32.0F, 31.998046875F, 1.0F / 512.0F},
    {100.0F, 100.5F, 0.01F},           /* offset far from zero */
    {-3.0F, -2.0F, 0.25F},             /* all negative, exact grid */
    {0.0F, 1.0e-30F, 1.0e-33F},        /* small magnitudes */
};
enum { RANGE_COUNT = sizeof RANGES / sizeof RANGES[0] };

static uint32_t read_code(const uint8_t *data, size_t size, unsigned bits) {
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, data, size);
    return net_bits_read(&reader, bits);
}

/* The last grid point and its code, from what the writer emits for +inf. */
static float grid_last(net_bits_range_t range, uint32_t *code) {
    uint8_t buffer[8];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write_ranged(&writer, range, INFINITY);
    *code = read_code(buffer, net_bits_writer_finish(&writer), net_bits_range_width(range));
    return net_bits_quantize(range, INFINITY);
}

static float ranged_round_trip(net_bits_range_t range, float value) {
    uint8_t buffer[8];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write_ranged(&writer, range, value);
    TEST_ASSERT_TRUE(writer.ok);
    TEST_ASSERT_EQUAL_UINT32(net_bits_range_width(range), (uint32_t)writer.bits);
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, buffer, net_bits_writer_finish(&writer));
    const float got = net_bits_read_ranged(&reader, range);
    TEST_ASSERT_TRUE(net_bits_reader_done(&reader));
    return got;
}

static bool same_float(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }

static double ulp_of(float value) {
    const float above = nextafterf(fabsf(value), INFINITY);
    return (double)above - (double)fabsf(value);
}

// #region bit stream

static void test_wire_bit_order_is_low_bit_first(void) {
    uint8_t buffer[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write(&writer, 1U, 1U);
    net_bits_write(&writer, 5U, 3U);
    net_bits_write(&writer, 0xABCU, 12U);
    net_bits_write(&writer, 1U, 2U);
    TEST_ASSERT_EQUAL_UINT32(3U, (uint32_t)net_bits_writer_finish(&writer));
    TEST_ASSERT_EQUAL_HEX8(0xCB, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAB, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[3]);
}

static void test_stream_round_trips_random_widths(void) {
    rng_seed(1);
    enum { WRITES = 64 };
    for (int round = 0; round < 400; round++) {
        uint32_t values[WRITES];
        unsigned widths[WRITES];
        const unsigned count = 1U + rng_below(WRITES);
        guarded_t buffer = guarded_alloc(WRITES * 4U, (uint8_t)rng_u32());
        net_bits_writer_t writer;
        net_bits_writer_init(&writer, buffer.data, buffer.size);
        size_t total = 0U;
        for (unsigned i = 0U; i < count; i++) {
            widths[i] = 1U + rng_below(32U);
            values[i] = widths[i] == 32U ? rng_u32() : rng_u32() & ((1U << widths[i]) - 1U);
            net_bits_write(&writer, values[i], widths[i]);
            total += widths[i];
        }
        TEST_ASSERT_TRUE(writer.ok);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)total, (uint32_t)writer.bits);
        const size_t size = net_bits_writer_finish(&writer);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)((total + 7U) / 8U), (uint32_t)size);
        TEST_ASSERT_TRUE(guarded_intact(&buffer));
        uint8_t *packet = exact_copy(buffer.data, size);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, packet, size);
        for (unsigned i = 0U; i < count; i++) {
            CHECK(net_bits_read(&reader, widths[i]) == values[i], "round %d write %u width %u", round, i, widths[i]);
        }
        TEST_ASSERT_TRUE(net_bits_reader_done(&reader));
        free(packet);
        guarded_free(&buffer);
    }
}

static void test_write_refuses_values_that_do_not_fit(void) {
    for (unsigned width = 0U; width <= 33U; width++) {
        guarded_t buffer = guarded_alloc(8U, 0x3C);
        net_bits_writer_t writer;
        net_bits_writer_init(&writer, buffer.data, buffer.size);
        net_bits_write(&writer, 3U, 2U);
        const uint32_t value = width >= 1U && width < 32U ? 1U << width : 1U;
        net_bits_write(&writer, value, width);
        if (width == 32U) {
            TEST_ASSERT_TRUE(writer.ok);
        } else {
            CHECK(!writer.ok, "width %u accepted %u", width, value);
            TEST_ASSERT_EQUAL_UINT32(2U, (uint32_t)writer.bits);
            TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_writer_finish(&writer));
            TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_writer_room(&writer));
            net_bits_write(&writer, 1U, 1U);
            TEST_ASSERT_EQUAL_UINT32(2U, (uint32_t)writer.bits);
        }
        TEST_ASSERT_TRUE(guarded_intact(&buffer));
        guarded_free(&buffer);
    }
}

static void test_writer_with_no_room_writes_nothing_outside(void) {
    rng_seed(2);
    for (size_t capacity = 0U; capacity <= 9U; capacity++) {
        for (int round = 0; round < 40; round++) {
            guarded_t buffer = guarded_alloc(capacity, 0x77);
            net_bits_writer_t writer;
            net_bits_writer_init(&writer, buffer.data, buffer.size);
            for (;;) {
                const size_t room = net_bits_writer_room(&writer);
                TEST_ASSERT_EQUAL_UINT32((uint32_t)(capacity * 8U - writer.bits), (uint32_t)room);
                const unsigned width = 1U + rng_below(32U);
                const size_t before = writer.bits;
                net_bits_write(&writer, rng_u32() >> (32U - width), width);
                if (width > room) {
                    TEST_ASSERT_FALSE(writer.ok);
                    TEST_ASSERT_EQUAL_UINT32((uint32_t)before, (uint32_t)writer.bits);
                    break;
                }
                TEST_ASSERT_TRUE(writer.ok);
            }
            TEST_ASSERT_TRUE(guarded_intact(&buffer));
            guarded_free(&buffer);
        }
    }
}

static void test_writer_restored_from_a_copy_overwrites(void) {
    uint8_t fresh[4] = {0};
    uint8_t reused[4] = {0};
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, fresh, sizeof fresh);
    net_bits_write(&writer, 7U, 3U);
    net_bits_write(&writer, 0U, 13U);
    const size_t fresh_size = net_bits_writer_finish(&writer);

    net_bits_writer_init(&writer, reused, sizeof reused);
    net_bits_write(&writer, 7U, 3U);
    const net_bits_writer_t mark = writer;
    net_bits_write(&writer, 0x1FFFFFFFU, 29U);
    writer = mark;
    net_bits_write(&writer, 0U, 13U);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)fresh_size, (uint32_t)net_bits_writer_finish(&writer));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(fresh, reused, fresh_size);
}

static void test_reader_done_needs_zero_padding_and_no_spare_bytes(void) {
    const uint8_t clean[2] = {0xFF, 0x03};
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, clean, sizeof clean);
    TEST_ASSERT_EQUAL_HEX32(0x3FFU, net_bits_read(&reader, 10U));
    TEST_ASSERT_TRUE(net_bits_reader_done(&reader));

    const uint8_t padded[2] = {0xFF, 0x07};
    net_bits_reader_init(&reader, padded, sizeof padded);
    (void)net_bits_read(&reader, 10U);
    TEST_ASSERT_FALSE(net_bits_reader_done(&reader));

    const uint8_t spare[3] = {0xFF, 0x03, 0x00};
    net_bits_reader_init(&reader, spare, sizeof spare);
    (void)net_bits_read(&reader, 10U);
    TEST_ASSERT_FALSE(net_bits_reader_done(&reader));

    const uint8_t aligned_spare[2] = {0xFF, 0x00};
    net_bits_reader_init(&reader, aligned_spare, sizeof aligned_spare);
    TEST_ASSERT_EQUAL_HEX32(0xFFU, net_bits_read(&reader, 8U));
    TEST_ASSERT_FALSE(net_bits_reader_done(&reader));
    TEST_ASSERT_EQUAL_HEX32(0U, net_bits_read(&reader, 8U));
    TEST_ASSERT_TRUE(net_bits_reader_done(&reader));

    net_bits_reader_init(&reader, clean, sizeof clean);
    (void)net_bits_read(&reader, 8U);
    TEST_ASSERT_FALSE(net_bits_reader_done(&reader));
    (void)net_bits_read(&reader, 9U);
    TEST_ASSERT_FALSE(reader.ok);
    TEST_ASSERT_FALSE(net_bits_reader_done(&reader));

    net_bits_reader_init(&reader, NULL, 0U);
    TEST_ASSERT_TRUE(net_bits_reader_done(&reader));
}

static void test_width_tells_count_values_apart(void) {
    TEST_ASSERT_EQUAL_UINT32(0U, net_bits_width(0U));
    TEST_ASSERT_EQUAL_UINT32(0U, net_bits_width(1U));
    rng_seed(3);
    for (int i = 0; i < 20000; i++) {
        uint32_t count;
        if (i < 96) {
            const unsigned power = 1U + (unsigned)(i / 3) % 32U;
            count = (power == 32U ? 0U : 1U << power) + (uint32_t)(i % 3) - 1U;
        } else {
            count = rng_u32() >> rng_below(32U);
        }
        if (count < 2U) {
            continue;
        }
        const unsigned width = net_bits_width(count);
        CHECK(width >= 1U && width <= 32U, "count %u width %u", count, width);
        CHECK((uint64_t)1 << (width - 1U) < count && count <= (uint64_t)1 << width, "count %u width %u", count, width);
    }
}

// #endregion

// #region whole numbers

static void test_int_round_trips_and_refuses_out_of_range(void) {
    const int32_t ranges[][2] = {
        {INT32_MIN, INT32_MAX}, {-5, 5}, {7, 7}, {0, 1}, {-1000000, 3}, {INT32_MAX - 3, INT32_MAX}, {INT32_MIN, INT32_MIN + 1000},
    };
    rng_seed(4);
    for (int r = 0; r < 64; r++) {
        int32_t min;
        int32_t max;
        if (r < (int)(sizeof ranges / sizeof ranges[0])) {
            min = ranges[r][0];
            max = ranges[r][1];
        } else {
            const int32_t a = (int32_t)rng_u32() >> rng_below(32U);
            const int32_t b = (int32_t)rng_u32() >> rng_below(32U);
            min = a < b ? a : b;
            max = a < b ? b : a;
        }
        const uint32_t span = (uint32_t)((int64_t)max - (int64_t)min);
        const unsigned width = span == UINT32_MAX ? 32U : net_bits_width(span + 1U);
        for (int i = 0; i < 200; i++) {
            int32_t value = (int32_t)((int64_t)min + (int64_t)(span == UINT32_MAX ? rng_u32() : rng_below(span + 1U)));
            if (i == 0) {
                value = min;
            } else if (i == 1) {
                value = max;
            }
            uint8_t buffer[8];
            net_bits_writer_t writer;
            net_bits_writer_init(&writer, buffer, sizeof buffer);
            net_bits_write_int(&writer, value, min, max);
            TEST_ASSERT_TRUE(writer.ok);
            TEST_ASSERT_EQUAL_UINT32(width, (uint32_t)writer.bits);
            net_bits_reader_t reader;
            net_bits_reader_init(&reader, buffer, net_bits_writer_finish(&writer));
            CHECK(net_bits_read_int(&reader, min, max) == value, "[%d, %d] value %d", min, max, value);
            TEST_ASSERT_TRUE(net_bits_reader_done(&reader));
        }
        for (int side = 0; side < 2; side++) {
            if (side == 0 ? min == INT32_MIN : max == INT32_MAX) {
                continue;
            }
            uint8_t buffer[8];
            net_bits_writer_t writer;
            net_bits_writer_init(&writer, buffer, sizeof buffer);
            net_bits_write_int(&writer, side == 0 ? min - 1 : max + 1, min, max);
            TEST_ASSERT_FALSE(writer.ok);
            TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)writer.bits);
        }
    }
    uint8_t buffer[8];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write_int(&writer, 0, 1, -1);
    TEST_ASSERT_FALSE(writer.ok);
}

static void test_int_code_past_max_is_refused(void) {
    const int32_t ranges[][2] = {{-5, 1000}, {0, 2}, {10, 14}, {INT32_MIN, 0}};
    for (size_t r = 0U; r < sizeof ranges / sizeof ranges[0]; r++) {
        const int32_t min = ranges[r][0];
        const int32_t max = ranges[r][1];
        const uint32_t span = (uint32_t)((int64_t)max - (int64_t)min);
        const unsigned width = net_bits_width(span + 1U);
        const uint64_t codes = (uint64_t)1 << width;
        for (uint64_t code = span + 1U; code < codes; code += 1U + (codes - span) / 64U) {
            uint8_t buffer[8];
            net_bits_writer_t writer;
            net_bits_writer_init(&writer, buffer, sizeof buffer);
            net_bits_write(&writer, (uint32_t)code, width);
            net_bits_reader_t reader;
            net_bits_reader_init(&reader, buffer, net_bits_writer_finish(&writer));
            const int32_t got = net_bits_read_int(&reader, min, max);
            CHECK(!reader.ok, "[%d, %d] code %u accepted", min, max, (unsigned)code);
            TEST_ASSERT_EQUAL_INT32(min, got);
        }
    }
}

// #endregion

// #region ranged reals

static void test_ranged_error_is_at_most_half_a_step(void) {
    rng_seed(5);
    for (int r = 0; r < RANGE_COUNT; r++) {
        const net_bits_range_t range = RANGES[r];
        TEST_ASSERT_TRUE(net_bits_range_width(range) > 0U);
        for (int i = 0; i < 4000; i++) {
            const double t = rng_unit();
            float value = (float)((double)range.min + t * ((double)range.max - (double)range.min));
            if (value > range.max) {
                value = range.max;
            }
            const float got = ranged_round_trip(range, value);
            TEST_ASSERT_TRUE(same_float(got, net_bits_quantize(range, value)));
            const double error = fabs((double)got - (double)value);
            const double allowed = 0.5 * (double)range.step + 0.5 * ulp_of(got);
            CHECK(error <= allowed, "range %d value %.9g got %.9g error %.3g allowed %.3g", r, (double)value, (double)got,
                  error, allowed);
        }
    }
}

static void test_ranged_edges_clamp_to_the_grid(void) {
    for (int r = 0; r < RANGE_COUNT; r++) {
        const net_bits_range_t range = RANGES[r];
        uint32_t last_code;
        const float last = grid_last(range, &last_code);
        const double step = (double)range.step;
        CHECK(last >= range.max && (double)last - (double)range.max < step * (1.0 + 1e-6) + ulp_of(last),
              "range %d last %.9g", r, (double)last);
        TEST_ASSERT_TRUE(same_float(range.min, ranged_round_trip(range, range.min)));
        const float at_max = ranged_round_trip(range, range.max);
        TEST_ASSERT_TRUE(fabs((double)at_max - (double)range.max) <= 0.5 * step + 0.5 * ulp_of(at_max));
        TEST_ASSERT_TRUE(same_float(range.min, ranged_round_trip(range, nextafterf(range.min, -INFINITY))));
        TEST_ASSERT_TRUE(same_float(range.min, ranged_round_trip(range, range.min - 1.0e30F)));
        TEST_ASSERT_TRUE(same_float(range.min, ranged_round_trip(range, -INFINITY)));
        TEST_ASSERT_TRUE(same_float(range.min, ranged_round_trip(range, NAN)));
        TEST_ASSERT_TRUE(same_float(range.min, ranged_round_trip(range, -NAN)));
        TEST_ASSERT_TRUE(same_float(last, ranged_round_trip(range, nextafterf(last, INFINITY))));
        TEST_ASSERT_TRUE(same_float(last, ranged_round_trip(range, INFINITY)));
        TEST_ASSERT_TRUE(same_float(last, ranged_round_trip(range, FLT_MAX)));
        TEST_ASSERT_TRUE(same_float(last, ranged_round_trip(range, last)));
        const float above = nextafterf(range.max, INFINITY);
        const float at_above = ranged_round_trip(range, above);
        TEST_ASSERT_TRUE(fabs((double)at_above - (double)above) <= 0.5 * step + 0.5 * ulp_of(at_above));
    }
}

static void test_ranged_grid_points_are_fixed(void) {
    rng_seed(6);
    for (int r = 0; r < RANGE_COUNT; r++) {
        const net_bits_range_t range = RANGES[r];
        const unsigned width = net_bits_range_width(range);
        uint32_t last_code;
        (void)grid_last(range, &last_code);
        for (int i = 0; i < 3000; i++) {
            const uint32_t code = i == 0 ? 0U : i == 1 ? last_code : rng_below(last_code + 1U);
            uint8_t buffer[8];
            net_bits_writer_t writer;
            net_bits_writer_init(&writer, buffer, sizeof buffer);
            net_bits_write(&writer, code, width);
            net_bits_reader_t reader;
            net_bits_reader_init(&reader, buffer, net_bits_writer_finish(&writer));
            const float point = net_bits_read_ranged(&reader, range);
            TEST_ASSERT_TRUE(reader.ok);
            CHECK(same_float(point, net_bits_quantize(range, point)), "range %d code %u drifts", r, code);
            net_bits_writer_init(&writer, buffer, sizeof buffer);
            net_bits_write_ranged(&writer, range, point);
            CHECK(read_code(buffer, net_bits_writer_finish(&writer), width) == code, "range %d code %u re-encodes", r, code);
        }
    }
}

static void test_ranged_code_past_the_grid_is_refused(void) {
    int checked = 0;
    for (int r = 0; r < RANGE_COUNT; r++) {
        const net_bits_range_t range = RANGES[r];
        const unsigned width = net_bits_range_width(range);
        uint32_t last_code;
        (void)grid_last(range, &last_code);
        const uint64_t codes = (uint64_t)1 << width;
        for (uint64_t code = (uint64_t)last_code + 1U; code < codes; code += 1U + (codes - last_code) / 32U) {
            uint8_t buffer[8];
            net_bits_writer_t writer;
            net_bits_writer_init(&writer, buffer, sizeof buffer);
            net_bits_write(&writer, (uint32_t)code, width);
            net_bits_reader_t reader;
            net_bits_reader_init(&reader, buffer, net_bits_writer_finish(&writer));
            const float got = net_bits_read_ranged(&reader, range);
            CHECK(!reader.ok, "range %d code %u accepted", r, (unsigned)code);
            TEST_ASSERT_TRUE(same_float(range.min, got));
            checked++;
        }
    }
    TEST_ASSERT_TRUE(checked > 0);
}

static void test_malformed_ranges_are_refused(void) {
    const net_bits_range_t bad[] = {
        {NAN, 1.0F, 0.1F},       {0.0F, NAN, 0.1F},        {0.0F, 1.0F, NAN},     {-INFINITY, 1.0F, 0.1F},
        {0.0F, INFINITY, 0.1F},  {0.0F, 1.0F, INFINITY},   {1.0F, 1.0F, 0.1F},    {2.0F, 1.0F, 0.1F},
        {0.0F, 1.0F, 0.0F},      {0.0F, 1.0F, -0.1F},      {0.0F, 1.0F, 1.0e-9F}, {1000.0F, 1001.0F, 1.0e-5F},
        {0.0F, FLT_MAX, FLT_MAX * 0.75F},                  {-1.0e9F, 1.0e9F, 0.5F},
    };
    for (size_t i = 0U; i < sizeof bad / sizeof bad[0]; i++) {
        CHECK(net_bits_range_width(bad[i]) == 0U, "bad range %u accepted", (unsigned)i);
        TEST_ASSERT_TRUE(same_float(0.0F, net_bits_quantize(bad[i], 0.5F)));
        uint8_t buffer[8] = {0};
        net_bits_writer_t writer;
        net_bits_writer_init(&writer, buffer, sizeof buffer);
        net_bits_write_ranged(&writer, bad[i], 0.5F);
        TEST_ASSERT_FALSE(writer.ok);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, buffer, sizeof buffer);
        const float got = net_bits_read_ranged(&reader, bad[i]);
        TEST_ASSERT_FALSE(reader.ok);
        TEST_ASSERT_TRUE(same_float(0.0F, got));
    }
}

// #endregion

// #region quaternion

static void normalise(const float in[4], double out[4]) {
    double norm = 0.0;
    for (int i = 0; i < 4; i++) {
        norm += (double)in[i] * (double)in[i];
    }
    norm = sqrt(norm);
    for (int i = 0; i < 4; i++) {
        out[i] = (double)in[i] / norm;
    }
}

static void quat_round_trip(const float in[4], float out[4], unsigned bits) {
    uint8_t buffer[16];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write_quat(&writer, in, bits);
    TEST_ASSERT_TRUE(writer.ok);
    TEST_ASSERT_EQUAL_UINT32(2U + 3U * bits, (uint32_t)writer.bits);
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, buffer, net_bits_writer_finish(&writer));
    net_bits_read_quat(&reader, out, bits);
    TEST_ASSERT_TRUE(net_bits_reader_done(&reader));
}

/* Uniform on the sphere of unit quaternions for the first draws, then the cases that sit
   on ties of the dropped component: axes, equal pairs, the four-equal corner and points a
   little off it. */
static void sample_rotation(int i, float out[4]) {
    if (i % 4 != 3) {
        const double u1 = rng_unit();
        const double u2 = 2.0 * 3.14159265358979323846 * rng_unit();
        const double u3 = 2.0 * 3.14159265358979323846 * rng_unit();
        out[0] = (float)(sqrt(1.0 - u1) * sin(u2));
        out[1] = (float)(sqrt(1.0 - u1) * cos(u2));
        out[2] = (float)(sqrt(u1) * sin(u3));
        out[3] = (float)(sqrt(u1) * cos(u3));
        return;
    }
    const float sign[2] = {1.0F, -1.0F};
    for (int c = 0; c < 4; c++) {
        out[c] = 0.0F;
    }
    switch (rng_below(4U)) {
    case 0: /* one axis */
        out[rng_below(4U)] = sign[rng_below(2U)];
        break;
    case 1: { /* two equal components */
        const uint32_t a = rng_below(4U);
        const uint32_t b = (a + 1U + rng_below(3U)) % 4U;
        out[a] = sign[rng_below(2U)] * 0.70710678F;
        out[b] = sign[rng_below(2U)] * 0.70710678F;
        break;
    }
    case 2: /* four equal */
        for (int c = 0; c < 4; c++) {
            out[c] = sign[rng_below(2U)] * 0.5F;
        }
        break;
    default: { /* near the corner, where the bound is reached */
        const double spread = ldexp(1.0, -(int)rng_below(24U));
        for (int c = 0; c < 4; c++) {
            out[c] = (float)(0.5 + spread * (rng_unit() - 0.5));
        }
        break;
    }
    }
}

static void test_quat_error_shrinks_with_bits(void) {
    float previous = INFINITY;
    for (unsigned bits = NET_BITS_QUAT_BITS_MIN; bits <= NET_BITS_QUAT_BITS_MAX; bits++) {
        const float bound = net_bits_quat_error(bits);
        CHECK(isfinite(bound) && bound > 0.0F && bound < previous, "bits %u bound %g", bits, (double)bound);
        previous = bound;
    }
    TEST_ASSERT_TRUE(net_bits_quat_error(NET_BITS_QUAT_BITS_MIN - 1U) >= 3.14159265F);
    TEST_ASSERT_TRUE(net_bits_quat_error(NET_BITS_QUAT_BITS_MAX + 1U) >= 3.14159265F);
}

static void test_quat_angle_is_within_the_bound(void) {
    rng_seed(7);
    for (unsigned bits = NET_BITS_QUAT_BITS_MIN; bits <= NET_BITS_QUAT_BITS_MAX; bits++) {
        const double bound = (double)net_bits_quat_error(bits);
        double worst = 0.0;
        for (int i = 0; i < 4000; i++) {
            float in[4];
            sample_rotation(i, in);
            double expected[4];
            normalise(in, expected);
            float out[4];
            quat_round_trip(in, out, bits);
            CHECK(quat_is_unit(out), "bits %u sample %d not unit", bits, i);
            const double angle = rotation_angle(expected, out);
            CHECK(angle <= bound, "bits %u sample %d angle %.6g bound %.6g", bits, i, angle, bound);
            worst = fmax(worst, angle);
        }
        /* A loose bound would pick needlessly wide fields. */
        CHECK(worst >= 0.5 * bound, "bits %u worst %.6g bound %.6g", bits, worst, bound);
    }
}

static void test_quat_scaled_zero_and_nan_inputs(void) {
    rng_seed(8);
    const unsigned bits = 12U;
    const double bound = (double)net_bits_quat_error(bits);
    const float scales[] = {1.0e-30F, 1.0e-3F, 3.0F, -1.0F, -7.5F, 1.0e30F, FLT_MAX};
    for (int i = 0; i < 500; i++) {
        float unit[4];
        sample_rotation(i, unit);
        double expected[4];
        normalise(unit, expected);
        for (size_t s = 0U; s < sizeof scales / sizeof scales[0]; s++) {
            float in[4];
            for (int c = 0; c < 4; c++) {
                in[c] = unit[c] * scales[s];
            }
            if (!isfinite(in[0]) || !isfinite(in[1]) || !isfinite(in[2]) || !isfinite(in[3])) {
                continue;
            }
            float out[4];
            quat_round_trip(in, out, bits);
            TEST_ASSERT_TRUE(quat_is_unit(out));
            CHECK(rotation_angle(expected, out) <= bound, "sample %d scale %g", i, (double)scales[s]);
        }
    }
    const float none[][4] = {
        {0.0F, 0.0F, 0.0F, 0.0F}, {NAN, 0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 0.0F, NAN},
        {INFINITY, 0.0F, 0.0F, 0.0F}, {0.5F, -INFINITY, 0.5F, 0.5F}, {-0.0F, -0.0F, -0.0F, -0.0F},
    };
    for (size_t i = 0U; i < sizeof none / sizeof none[0]; i++) {
        float out[4];
        quat_round_trip(none[i], out, bits);
        CHECK(out[0] == 0.0F && out[1] == 0.0F && out[2] == 0.0F && out[3] == 1.0F, "input %u not identity", (unsigned)i);
    }
}

static void test_quat_axis_rotations_read_back_exact(void) {
    const float axes[][4] = {{0.0F, 0.0F, 0.0F, 1.0F}, {0.0F, 0.0F, 0.0F, -1.0F}, {1.0F, 0.0F, 0.0F, 0.0F},
                             {0.0F, -1.0F, 0.0F, 0.0F}, {0.0F, 0.0F, 1.0F, 0.0F}};
    for (unsigned bits = NET_BITS_QUAT_BITS_MIN; bits <= NET_BITS_QUAT_BITS_MAX; bits++) {
        for (size_t i = 0U; i < sizeof axes / sizeof axes[0]; i++) {
            float out[4];
            quat_round_trip(axes[i], out, bits);
            for (int c = 0; c < 4; c++) {
                CHECK(out[c] == fabsf(axes[i][c]), "bits %u axis %u component %d = %g", bits, (unsigned)i, c, (double)out[c]);
            }
        }
    }
}

static void test_quat_spare_code_and_bad_widths_are_refused(void) {
    for (unsigned bits = NET_BITS_QUAT_BITS_MIN; bits <= NET_BITS_QUAT_BITS_MAX; bits++) {
        for (unsigned slot = 0U; slot < 3U; slot++) {
            uint8_t buffer[16];
            net_bits_writer_t writer;
            net_bits_writer_init(&writer, buffer, sizeof buffer);
            net_bits_write(&writer, 3U, 2U);
            for (unsigned c = 0U; c < 3U; c++) {
                net_bits_write(&writer, c == slot ? (1U << bits) - 1U : 1U << (bits - 1U), bits);
            }
            net_bits_reader_t reader;
            net_bits_reader_init(&reader, buffer, net_bits_writer_finish(&writer));
            float out[4] = {9.0F, 9.0F, 9.0F, 9.0F};
            net_bits_read_quat(&reader, out, bits);
            CHECK(!reader.ok, "bits %u slot %u spare code accepted", bits, slot);
            TEST_ASSERT_TRUE(out[0] == 0.0F && out[1] == 0.0F && out[2] == 0.0F && out[3] == 1.0F);
        }
    }
    const float identity[4] = {0.0F, 0.0F, 0.0F, 1.0F};
    const unsigned bad[] = {0U, 3U, 21U, 32U};
    for (size_t i = 0U; i < sizeof bad / sizeof bad[0]; i++) {
        uint8_t buffer[16] = {0};
        net_bits_writer_t writer;
        net_bits_writer_init(&writer, buffer, sizeof buffer);
        net_bits_write_quat(&writer, identity, bad[i]);
        TEST_ASSERT_FALSE(writer.ok);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, buffer, sizeof buffer);
        float out[4];
        net_bits_read_quat(&reader, out, bad[i]);
        TEST_ASSERT_FALSE(reader.ok);
    }
}

// #endregion

// #region untrusted bytes

enum { READ_BITS, READ_INT, READ_RANGED, READ_QUAT, READER_KINDS };

static const int32_t GARBAGE_INT[2] = {-5, 1000};

/* One read of the given kind; false once refused. A value read while ok must be one the
   table allows. */
static bool read_checked(net_bits_reader_t *reader, int kind, uint32_t pick) {
    switch (kind) {
    case READ_BITS: {
        const unsigned width = 1U + pick % 32U;
        (void)net_bits_read(reader, width);
        break;
    }
    case READ_INT: {
        const int32_t value = net_bits_read_int(reader, GARBAGE_INT[0], GARBAGE_INT[1]);
        TEST_ASSERT_TRUE(value >= GARBAGE_INT[0] && value <= GARBAGE_INT[1]);
        break;
    }
    case READ_RANGED: {
        const net_bits_range_t range = RANGES[pick % RANGE_COUNT];
        const float value = net_bits_read_ranged(reader, range);
        uint32_t last_code;
        const float last = grid_last(range, &last_code);
        TEST_ASSERT_TRUE(value >= range.min && value <= last);
        TEST_ASSERT_TRUE(same_float(value, net_bits_quantize(range, value)));
        break;
    }
    default: {
        float out[4];
        net_bits_read_quat(reader, out, NET_BITS_QUAT_BITS_MIN + pick % (NET_BITS_QUAT_BITS_MAX - NET_BITS_QUAT_BITS_MIN + 1U));
        TEST_ASSERT_TRUE(quat_is_unit(out));
        break;
    }
    }
    return reader->ok;
}

static void test_garbage_gives_valid_values_or_a_refusal(void) {
    rng_seed(9);
    for (int round = 0; round < 3000; round++) {
        const size_t size = rng_below(48U);
        uint8_t source[48];
        for (size_t i = 0U; i < sizeof source; i++) {
            source[i] = (uint8_t)rng_u32();
        }
        uint8_t *packet = exact_copy(source, size);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, packet, size);
        const int only = round % (READER_KINDS + 1);
        int reads = 0;
        while (read_checked(&reader, only < READER_KINDS ? only : (int)rng_below(READER_KINDS), rng_u32())) {
            TEST_ASSERT_TRUE(reader.bits <= size * 8U);
            reads++;
            TEST_ASSERT_TRUE(reads < 1000);
        }
        TEST_ASSERT_FALSE(net_bits_reader_done(&reader));
        free(packet);
    }
}

static void write_mixed(net_bits_writer_t *writer, int i) {
    const float rotation[4] = {0.1F * (float)i, -0.5F, 0.3F, 0.8F};
    net_bits_write(writer, (uint32_t)i & 0x7FU, 7U);
    net_bits_write_int(writer, i * 3 - 5, GARBAGE_INT[0], GARBAGE_INT[1]);
    net_bits_write_ranged(writer, RANGES[i % RANGE_COUNT], (float)i * 0.37F);
    net_bits_write_quat(writer, rotation, 9U);
}

static bool read_mixed(net_bits_reader_t *reader, int i) {
    float out[4];
    (void)net_bits_read(reader, 7U);
    (void)net_bits_read_int(reader, GARBAGE_INT[0], GARBAGE_INT[1]);
    (void)net_bits_read_ranged(reader, RANGES[i % RANGE_COUNT]);
    net_bits_read_quat(reader, out, 9U);
    return reader->ok;
}

static void test_every_truncation_is_refused(void) {
    enum { VALUES = 12 };
    uint8_t buffer[256];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    for (int i = 0; i < VALUES; i++) {
        write_mixed(&writer, i);
    }
    const size_t size = net_bits_writer_finish(&writer);
    TEST_ASSERT_TRUE(size > 0U);
    for (size_t length = 0U; length <= size; length++) {
        uint8_t *packet = exact_copy(buffer, length);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, packet, length);
        bool ok = true;
        for (int i = 0; i < VALUES && ok; i++) {
            ok = read_mixed(&reader, i);
        }
        if (length == size) {
            TEST_ASSERT_TRUE(ok && net_bits_reader_done(&reader));
        } else {
            CHECK(!ok && !reader.ok, "length %u of %u accepted", (unsigned)length, (unsigned)size);
        }
        free(packet);
    }
}

static void test_writers_short_of_room_write_nothing_outside(void) {
    for (int kind = 0; kind < READER_KINDS; kind++) {
        for (unsigned lead = 0U; lead < 8U; lead++) {
            for (size_t capacity = 0U; capacity <= 8U; capacity++) {
                guarded_t buffer = guarded_alloc(capacity, 0x5A);
                net_bits_writer_t writer;
                net_bits_writer_init(&writer, buffer.data, buffer.size);
                if (lead > 0U) {
                    net_bits_write(&writer, 0U, lead);
                }
                const size_t room = net_bits_writer_room(&writer);
                const size_t before = writer.bits;
                size_t need = 0U;
                const float rotation[4] = {0.2F, 0.4F, -0.4F, 0.8F};
                switch (kind) {
                case READ_BITS:
                    need = 32U;
                    net_bits_write(&writer, 0xFFFFFFFFU, 32U);
                    break;
                case READ_INT:
                    need = 10U;
                    net_bits_write_int(&writer, 1000, GARBAGE_INT[0], GARBAGE_INT[1]);
                    break;
                case READ_RANGED:
                    need = net_bits_range_width(RANGES[2]);
                    net_bits_write_ranged(&writer, RANGES[2], 5.0e5F);
                    break;
                default:
                    need = 2U + 3U * 20U;
                    net_bits_write_quat(&writer, rotation, 20U);
                    break;
                }
                if (lead > capacity * 8U || need > room) {
                    TEST_ASSERT_FALSE(writer.ok);
                    TEST_ASSERT_EQUAL_UINT32((uint32_t)before, (uint32_t)writer.bits);
                } else {
                    TEST_ASSERT_TRUE(writer.ok);
                }
                CHECK(guarded_intact(&buffer), "kind %d lead %u capacity %u", kind, lead, (unsigned)capacity);
                guarded_free(&buffer);
            }
        }
    }
}

// #endregion

// #region rules pinned by the contract

static void test_read_refuses_widths_out_of_range(void) {
    const uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    const unsigned widths[] = {0U, 33U, 64U, UINT_MAX};
    for (size_t i = 0U; i < sizeof widths / sizeof widths[0]; i++) {
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, data, sizeof data);
        CHECK(net_bits_read(&reader, widths[i]) == 0U && !reader.ok, "width %u accepted", widths[i]);
        TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)reader.bits);
    }
}

static void test_read_int_refuses_an_inverted_range(void) {
    const uint8_t data[4] = {0};
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, data, sizeof data);
    TEST_ASSERT_EQUAL_INT32(5, net_bits_read_int(&reader, 5, 4));
    TEST_ASSERT_FALSE(reader.ok);
}

/* The values a caller may rely on after a refusal, from every reader. */
static void test_reads_after_a_refusal_return_harmless_values(void) {
    const uint8_t data[16] = {0x5A, 0xA5, 0xFF, 0x13, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01, 0x02, 0x03, 0x04};
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, data, sizeof data);
    (void)net_bits_read(&reader, 33U);
    TEST_ASSERT_FALSE(reader.ok);
    for (int round = 0; round < 3; round++) {
        TEST_ASSERT_EQUAL_HEX32(0U, net_bits_read(&reader, 8U));
        TEST_ASSERT_EQUAL_INT32(-5, net_bits_read_int(&reader, -5, 1000));
        for (int r = 0; r < RANGE_COUNT; r++) {
            TEST_ASSERT_TRUE(same_float(RANGES[r].min, net_bits_read_ranged(&reader, RANGES[r])));
        }
        float out[4] = {9.0F, 9.0F, 9.0F, 9.0F};
        net_bits_read_quat(&reader, out, 9U);
        TEST_ASSERT_TRUE(out[0] == 0.0F && out[1] == 0.0F && out[2] == 0.0F && out[3] == 1.0F);
        TEST_ASSERT_FALSE(reader.ok);
        TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)reader.bits);
        TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_reader_left(&reader));
        TEST_ASSERT_FALSE(net_bits_reader_done(&reader));
    }
}

static void test_structs_set_by_hand_are_refused(void) {
    uint8_t data[4] = {0x0F, 0, 0, 0};
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, data, sizeof data);
    reader.bits = sizeof data * 8U + 1U;
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_reader_left(&reader));
    TEST_ASSERT_FALSE(net_bits_reader_done(&reader));
    TEST_ASSERT_EQUAL_HEX32(0U, net_bits_read(&reader, 1U));
    TEST_ASSERT_FALSE(reader.ok);

    net_bits_reader_init(&reader, data, sizeof data);
    reader.data = NULL;
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_reader_left(&reader));
    TEST_ASSERT_FALSE(net_bits_reader_done(&reader));
    (void)net_bits_read(&reader, 1U);
    TEST_ASSERT_FALSE(reader.ok);

    net_bits_reader_init(&reader, NULL, 5U);
    TEST_ASSERT_FALSE(net_bits_reader_done(&reader));
    (void)net_bits_read(&reader, 8U);
    TEST_ASSERT_FALSE(reader.ok);

    guarded_t buffer = guarded_alloc(4U, 0x00);
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer.data, buffer.size);
    writer.bits = 4U * 8U + 3U;
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_writer_room(&writer));
    net_bits_write(&writer, 1U, 1U);
    TEST_ASSERT_FALSE(writer.ok);
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_writer_finish(&writer));
    TEST_ASSERT_TRUE(guarded_intact(&buffer));

    net_bits_writer_init(&writer, buffer.data, buffer.size);
    net_bits_write(&writer, 1U, 3U);
    writer.bits = 4U * 8U + 9U;
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_writer_finish(&writer));
    TEST_ASSERT_FALSE(writer.ok);
    TEST_ASSERT_TRUE(guarded_intact(&buffer));
    guarded_free(&buffer);

    uint8_t room[4];
    net_bits_writer_init(&writer, room, sizeof room);
    writer.data = NULL;
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_writer_room(&writer));
    net_bits_write(&writer, 1U, 1U);
    TEST_ASSERT_FALSE(writer.ok);

    net_bits_writer_init(&writer, NULL, 5U);
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_writer_room(&writer));
    net_bits_write(&writer, 1U, 1U);
    TEST_ASSERT_FALSE(writer.ok);
    TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)net_bits_writer_finish(&writer));

    /* A size the caller states is taken in bits without overflow; nothing is read here. */
    net_bits_reader_init(&reader, data, SIZE_MAX);
    TEST_ASSERT_TRUE(net_bits_reader_left(&reader) == SIZE_MAX);
}

static void test_writer_restored_then_finished_leaves_no_stale_bits(void) {
    uint8_t buffer[8];
    memset(buffer, 0xFF, sizeof buffer);
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write(&writer, 5U, 3U);
    const net_bits_writer_t mark = writer;
    net_bits_write(&writer, 0x1FFFFFFFU, 29U);
    writer = mark;
    TEST_ASSERT_EQUAL_UINT32(1U, (uint32_t)net_bits_writer_finish(&writer));
    TEST_ASSERT_EQUAL_HEX8(0x05, buffer[0]);
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, buffer, 1U);
    TEST_ASSERT_EQUAL_HEX32(5U, net_bits_read(&reader, 3U));
    TEST_ASSERT_TRUE(net_bits_reader_done(&reader));
}

static void test_step_of_two_float_spacings_is_the_limit(void) {
    /* The grid's largest magnitude is 1.0, whose float spacing (taken from the top of its
       binade) is 2^-23. */
    const float limit = 0x1p-22F;
    const net_bits_range_t at = {0.0F, 1.0F, limit};
    const net_bits_range_t below = {0.0F, 1.0F, nextafterf(limit, 0.0F)};
    TEST_ASSERT_EQUAL_UINT32(23U, net_bits_range_width(at));
    TEST_ASSERT_EQUAL_UINT32(0U, net_bits_range_width(below));
    const net_bits_range_t wide = {-1000.0F, 1000.0F, 2.0F * 0x1p-14F};
    TEST_ASSERT_TRUE(net_bits_range_width(wide) > 0U);
    const net_bits_range_t narrow = {-1000.0F, 1000.0F, nextafterf(2.0F * 0x1p-14F, 0.0F)};
    TEST_ASSERT_EQUAL_UINT32(0U, net_bits_range_width(narrow));
}

static void test_writer_ties(void) {
    /* Ranged ties round toward max. */
    const net_bits_range_t quarters = {0.0F, 1.0F, 0.25F};
    const float ties[] = {0.125F, 0.375F, 0.625F, 0.875F};
    for (uint32_t i = 0U; i < 4U; i++) {
        uint8_t buffer[4];
        net_bits_writer_t writer;
        net_bits_writer_init(&writer, buffer, sizeof buffer);
        net_bits_write_ranged(&writer, quarters, ties[i]);
        TEST_ASSERT_EQUAL_UINT32(i + 1U, read_code(buffer, net_bits_writer_finish(&writer), 3U));
    }
    /* A tie of the largest quaternion component drops the lowest index. */
    const float corner[4] = {0.5F, -0.5F, 0.5F, 0.5F};
    const float pair[4] = {0.0F, 0.70710678F, -0.70710678F, 0.0F};
    const float back_pair[4] = {0.1F, 0.0F, 0.6F, 0.6F};
    const float *const inputs[] = {corner, pair, back_pair};
    const uint32_t dropped[] = {0U, 1U, 2U};
    for (int i = 0; i < 3; i++) {
        uint8_t buffer[8];
        net_bits_writer_t writer;
        net_bits_writer_init(&writer, buffer, sizeof buffer);
        net_bits_write_quat(&writer, inputs[i], 9U);
        TEST_ASSERT_EQUAL_UINT32(dropped[i], read_code(buffer, net_bits_writer_finish(&writer), 2U));
    }
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

/* Wire vectors written out by hand: the interop contract of README.md, "Known answers". A
   build that encodes or evaluates differently fails here. */
static void test_known_answers(void) {
    const net_bits_range_t x = {-32.0F, 32.0F - 1.0F / 512.0F, 1.0F / 512.0F};
    TEST_ASSERT_EQUAL_UINT32(15U, net_bits_range_width(x));
    uint8_t buffer[8];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write_ranged(&writer, x, 1.3F);
    TEST_ASSERT_EQUAL_UINT32(17050U, read_code(buffer, net_bits_writer_finish(&writer), 15U));
    TEST_ASSERT_EQUAL_HEX32(0x3FA68000U, float_bits(net_bits_quantize(x, 1.3F)));

    const net_bits_range_t tenths = {-10.0F, 10.0F, 0.3F};
    TEST_ASSERT_EQUAL_UINT32(7U, net_bits_range_width(tenths));
    uint32_t top;
    (void)grid_last(tenths, &top);
    TEST_ASSERT_EQUAL_UINT32(67U, top);
    const uint32_t codes[] = {0U, 1U, 33U, 34U, 66U, 67U};
    const uint32_t reads[] = {0xC1200000U, 0xC11B3333U, 0xBDCCCC98U, 0x3E4CCCE8U, 0x411CCCCEU, 0x4121999AU};
    for (int i = 0; i < 6; i++) {
        net_bits_writer_init(&writer, buffer, sizeof buffer);
        net_bits_write(&writer, codes[i], 7U);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, buffer, net_bits_writer_finish(&writer));
        CHECK(float_bits(net_bits_read_ranged(&reader, tenths)) == reads[i], "code %u", codes[i]);
    }
    const net_bits_range_t thirds = {-3.0F, -2.0F, 0.25F};
    (void)grid_last(thirds, &top);
    TEST_ASSERT_EQUAL_UINT32(4U, top);

    const float rotation[4] = {0.2F, 0.4F, -0.4F, 0.8F};
    const uint8_t rotation_bytes[4] = {0x1F, 0x7D, 0xFC, 0x06};
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write_quat(&writer, rotation, 9U);
    TEST_ASSERT_EQUAL_UINT32(4U, (uint32_t)net_bits_writer_finish(&writer));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(rotation_bytes, buffer, 4U);

    const struct {
        unsigned bits;
        uint8_t bytes[4];
        uint32_t read[4];
    } quats[] = {
        {9U, {0x1F, 0x7D, 0xFC, 0x06}, {0x3E4C7204U, 0x3ECC7204U, 0xBECC7204U, 0x3F4CFFCCU}},
        /* The kept components sum to one within rounding: a fused multiply-add reads a tiny
           dropped component here instead of zero. */
        {8U, {0xDE, 0x50, 0x08, 0x00}, {0xBECD4012U, 0xBF18832AU, 0x00000000U, 0xBF322B2CU}},
        {8U, {0xDF, 0xA8, 0x0B, 0x00}, {0xBECD4012U, 0x3F18832AU, 0xBF322B2CU, 0x00000000U}},
        /* Clamped and scaled back: dividing in float reads 0xBF27D60F for component 0. */
        {4U, {0x03, 0x0C, 0x00, 0x00}, {0xBF27D610U, 0xBF27D610U, 0xBEBFD012U, 0x00000000U}},
    };
    for (size_t q = 0U; q < sizeof quats / sizeof quats[0]; q++) {
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, quats[q].bytes, (2U + 3U * quats[q].bits + 7U) / 8U);
        float out[4];
        net_bits_read_quat(&reader, out, quats[q].bits);
        TEST_ASSERT_TRUE(net_bits_reader_done(&reader));
        for (int c = 0; c < 4; c++) {
            CHECK(float_bits(out[c]) == quats[q].read[c], "quaternion %u component %d reads 0x%08X", (unsigned)q, c,
                  (unsigned)float_bits(out[c]));
        }
    }
}

// #endregion

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wire_bit_order_is_low_bit_first);
    RUN_TEST(test_stream_round_trips_random_widths);
    RUN_TEST(test_write_refuses_values_that_do_not_fit);
    RUN_TEST(test_writer_with_no_room_writes_nothing_outside);
    RUN_TEST(test_writer_restored_from_a_copy_overwrites);
    RUN_TEST(test_reader_done_needs_zero_padding_and_no_spare_bytes);
    RUN_TEST(test_width_tells_count_values_apart);
    RUN_TEST(test_int_round_trips_and_refuses_out_of_range);
    RUN_TEST(test_int_code_past_max_is_refused);
    RUN_TEST(test_ranged_error_is_at_most_half_a_step);
    RUN_TEST(test_ranged_edges_clamp_to_the_grid);
    RUN_TEST(test_ranged_grid_points_are_fixed);
    RUN_TEST(test_ranged_code_past_the_grid_is_refused);
    RUN_TEST(test_malformed_ranges_are_refused);
    RUN_TEST(test_quat_error_shrinks_with_bits);
    RUN_TEST(test_quat_angle_is_within_the_bound);
    RUN_TEST(test_quat_scaled_zero_and_nan_inputs);
    RUN_TEST(test_quat_axis_rotations_read_back_exact);
    RUN_TEST(test_quat_spare_code_and_bad_widths_are_refused);
    RUN_TEST(test_garbage_gives_valid_values_or_a_refusal);
    RUN_TEST(test_every_truncation_is_refused);
    RUN_TEST(test_writers_short_of_room_write_nothing_outside);
    RUN_TEST(test_read_refuses_widths_out_of_range);
    RUN_TEST(test_read_int_refuses_an_inverted_range);
    RUN_TEST(test_reads_after_a_refusal_return_harmless_values);
    RUN_TEST(test_structs_set_by_hand_are_refused);
    RUN_TEST(test_writer_restored_then_finished_leaves_no_stale_bits);
    RUN_TEST(test_step_of_two_float_spacings_is_the_limit);
    RUN_TEST(test_writer_ties);
    RUN_TEST(test_known_answers);
    return UNITY_END();
}
