#include "features/net_bits/net_bits_record.h"

/* Before unity.h: its noreturn macro breaks the CRT's __declspec(noreturn). */
#include <float.h>
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

typedef struct every_kind {
    float rot[4];
    uint32_t id;
    int32_t score;
    float x;
    bool asleep;
    float height;
} every_kind_t;

static const net_bits_field_t EVERY_KIND[] = {
    NET_BITS_UINT(every_kind_t, id, 10),
    NET_BITS_RANGED(every_kind_t, x, -32.0, 32.0 - 1.0 / 512.0, 1.0 / 512.0),
    NET_BITS_RANGED(every_kind_t, height, 0.0, 8.0, 0.01),
    NET_BITS_INT(every_kind_t, score, -5, 1000),
    NET_BITS_QUAT(every_kind_t, rot, 9),
    NET_BITS_BOOL(every_kind_t, asleep),
};
enum { FIELD_COUNT = sizeof EVERY_KIND / sizeof EVERY_KIND[0], SCORE_FIELD = 3 };

/* The same wire table over a struct with other members, order and offsets. */
typedef struct reordered {
    double unrelated;
    bool asleep;
    int32_t score;
    float height;
    uint32_t id;
    float x;
    float rot[4];
} reordered_t;

static const net_bits_field_t REORDERED[] = {
    NET_BITS_UINT(reordered_t, id, 10),
    NET_BITS_RANGED(reordered_t, x, -32.0, 32.0 - 1.0 / 512.0, 1.0 / 512.0),
    NET_BITS_RANGED(reordered_t, height, 0.0, 8.0, 0.01),
    NET_BITS_INT(reordered_t, score, -5, 1000),
    NET_BITS_QUAT(reordered_t, rot, 9),
    NET_BITS_BOOL(reordered_t, asleep),
};

static net_bits_layout_t every_kind_layout(void) {
    net_bits_layout_t layout;
    TEST_ASSERT_TRUE(net_bits_layout_init(&layout, EVERY_KIND, FIELD_COUNT, sizeof(every_kind_t)));
    return layout;
}

static every_kind_t random_record(void) {
    every_kind_t record;
    memset(&record, 0, sizeof record);
    record.id = rng_below(1024U);
    record.x = (float)(-32.0 + 64.0 * rng_unit());
    record.height = (float)(8.0 * rng_unit());
    record.score = -5 + (int32_t)rng_below(1006U);
    record.asleep = rng_below(2U) != 0U;
    double norm = 0.0;
    double q[4];
    for (int i = 0; i < 4; i++) {
        q[i] = rng_unit() - 0.5;
        norm += q[i] * q[i];
    }
    for (int i = 0; i < 4; i++) {
        record.rot[i] = (float)(q[i] / sqrt(norm));
    }
    return record;
}

static bool same_float(float a, float b) { return memcmp(&a, &b, sizeof a) == 0; }

/* A member read from an accepted record is one its line allows. */
static void check_valid(const every_kind_t *record) {
    TEST_ASSERT_TRUE(record->id < 1024U);
    TEST_ASSERT_TRUE(record->score >= -5 && record->score <= 1000);
    const net_bits_range_t x = EVERY_KIND[1].range;
    const net_bits_range_t height = EVERY_KIND[2].range;
    TEST_ASSERT_TRUE(same_float(record->x, net_bits_quantize(x, record->x)) && record->x >= x.min &&
                     record->x <= net_bits_quantize(x, INFINITY));
    TEST_ASSERT_TRUE(same_float(record->height, net_bits_quantize(height, record->height)) && record->height >= height.min &&
                     record->height <= net_bits_quantize(height, INFINITY));
    TEST_ASSERT_TRUE(quat_is_unit(record->rot));
    uint8_t flag;
    memcpy(&flag, &record->asleep, 1U);
    TEST_ASSERT_TRUE(flag <= 1U);
}

static void test_record_round_trip_with_every_kind(void) {
    rng_seed(11);
    const net_bits_layout_t layout = every_kind_layout();
    const uint32_t expected_bits = 10U + net_bits_range_width(EVERY_KIND[1].range) +
                                   net_bits_range_width(EVERY_KIND[2].range) + net_bits_width(1006U) + 2U + 3U * 9U + 1U;
    TEST_ASSERT_EQUAL_UINT32(expected_bits, layout.bits);
    const double rotation_bound = (double)net_bits_quat_error(9U);
    for (int round = 0; round < 300; round++) {
        enum { RECORDS = 8 };
        every_kind_t sent[RECORDS];
        const unsigned count = 1U + rng_below(RECORDS);
        uint8_t buffer[RECORDS * 16];
        net_bits_writer_t writer;
        net_bits_writer_init(&writer, buffer, sizeof buffer);
        for (unsigned i = 0U; i < count; i++) {
            sent[i] = random_record();
            const size_t before = writer.bits;
            net_bits_write_record(&writer, &layout, &sent[i]);
            TEST_ASSERT_TRUE(writer.ok);
            TEST_ASSERT_EQUAL_UINT32(layout.bits, (uint32_t)(writer.bits - before));
        }
        const size_t size = net_bits_writer_finish(&writer);
        uint8_t *packet = exact_copy(buffer, size);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, packet, size);
        for (unsigned i = 0U; i < count; i++) {
            every_kind_t got;
            memset(&got, 0, sizeof got);
            TEST_ASSERT_TRUE(net_bits_read_record(&reader, &layout, &got));
            TEST_ASSERT_EQUAL_UINT32(sent[i].id, got.id);
            TEST_ASSERT_EQUAL_INT32(sent[i].score, got.score);
            TEST_ASSERT_TRUE(sent[i].asleep == got.asleep);
            TEST_ASSERT_TRUE(same_float(net_bits_quantize(EVERY_KIND[1].range, sent[i].x), got.x));
            TEST_ASSERT_TRUE(same_float(net_bits_quantize(EVERY_KIND[2].range, sent[i].height), got.height));
            double expected[4];
            for (int c = 0; c < 4; c++) {
                expected[c] = (double)sent[i].rot[c];
            }
            TEST_ASSERT_TRUE(quat_is_unit(got.rot));
            CHECK(rotation_angle(expected, got.rot) <= rotation_bound, "round %d record %u rotation", round, i);
            check_valid(&got);
        }
        TEST_ASSERT_TRUE(net_bits_reader_done(&reader));
        free(packet);
    }
}

static void test_record_writer_refusals(void) {
    const net_bits_layout_t layout = every_kind_layout();
    every_kind_t record;
    memset(&record, 0, sizeof record);
    record.rot[3] = 1.0F;
    const size_t need = (layout.bits + 7U) / 8U;
    for (size_t capacity = 0U; capacity < need; capacity++) {
        guarded_t buffer = guarded_alloc(capacity, 0x33);
        net_bits_writer_t writer;
        net_bits_writer_init(&writer, buffer.data, buffer.size);
        net_bits_write_record(&writer, &layout, &record);
        TEST_ASSERT_FALSE(writer.ok);
        TEST_ASSERT_EQUAL_UINT32(0U, (uint32_t)writer.bits);
        TEST_ASSERT_TRUE(guarded_intact(&buffer));
        guarded_free(&buffer);
    }
    uint8_t buffer[32];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    record.id = 1024U;
    net_bits_write_record(&writer, &layout, &record);
    TEST_ASSERT_FALSE(writer.ok);
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    record.id = 0U;
    record.score = 1001;
    net_bits_write_record(&writer, &layout, &record);
    TEST_ASSERT_FALSE(writer.ok);
}

/* Writes the record by hand up to `bad_field`, then a code outside that field's table. */
static size_t packet_with_bad_field(const every_kind_t *record, int bad_field, uint8_t *buffer, size_t capacity) {
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, capacity);
    net_bits_write(&writer, record->id, 10U);
    net_bits_write_ranged(&writer, EVERY_KIND[1].range, record->x);
    net_bits_write_ranged(&writer, EVERY_KIND[2].range, record->height);
    if (bad_field == SCORE_FIELD) {
        net_bits_write(&writer, 1006U, net_bits_width(1006U));
    } else {
        net_bits_write_int(&writer, record->score, -5, 1000);
        net_bits_write(&writer, 0U, 2U);
        net_bits_write(&writer, 511U, 9U); /* the spare code */
        net_bits_write(&writer, 255U, 9U);
        net_bits_write(&writer, 255U, 9U);
    }
    for (int i = 0; i < 64; i++) {
        net_bits_write(&writer, 0U, 1U);
    }
    TEST_ASSERT_TRUE(writer.ok);
    return net_bits_writer_finish(&writer);
}

static void test_failed_record_keeps_the_failing_member_and_those_after(void) {
    rng_seed(12);
    const net_bits_layout_t layout = every_kind_layout();
    const int bad_fields[] = {SCORE_FIELD, SCORE_FIELD + 1};
    for (size_t b = 0U; b < sizeof bad_fields / sizeof bad_fields[0]; b++) {
        const every_kind_t sent = random_record();
        uint8_t buffer[64];
        const size_t size = packet_with_bad_field(&sent, bad_fields[b], buffer, sizeof buffer);
        guarded_t holder = guarded_alloc(sizeof(every_kind_t), 0xEE);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, buffer, size);
        TEST_ASSERT_FALSE(net_bits_read_record(&reader, &layout, holder.data));
        TEST_ASSERT_FALSE(reader.ok);
        TEST_ASSERT_TRUE(guarded_intact(&holder));
        every_kind_t got;
        memcpy(&got, holder.data, sizeof got);
        TEST_ASSERT_EQUAL_UINT32(sent.id, got.id);
        TEST_ASSERT_TRUE(same_float(net_bits_quantize(EVERY_KIND[1].range, sent.x), got.x));
        TEST_ASSERT_TRUE(same_float(net_bits_quantize(EVERY_KIND[2].range, sent.height), got.height));
        const uint8_t *bytes = holder.data;
        const size_t untouched[][2] = {
            {offsetof(every_kind_t, rot), sizeof got.rot},
            {offsetof(every_kind_t, asleep), sizeof got.asleep},
            {offsetof(every_kind_t, score), bad_fields[b] == SCORE_FIELD ? sizeof got.score : 0U},
        };
        for (size_t u = 0U; u < sizeof untouched / sizeof untouched[0]; u++) {
            for (size_t i = 0U; i < untouched[u][1]; i++) {
                CHECK(bytes[untouched[u][0] + i] == 0xEE, "bad field %d: byte %u written", bad_fields[b],
                      (unsigned)(untouched[u][0] + i));
            }
        }
        if (bad_fields[b] != SCORE_FIELD) {
            TEST_ASSERT_EQUAL_INT32(sent.score, got.score);
        }
        guarded_free(&holder);
    }
}

static void test_every_truncation_of_records_is_refused(void) {
    rng_seed(13);
    const net_bits_layout_t layout = every_kind_layout();
    enum { RECORDS = 3 };
    uint8_t buffer[64];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    for (int i = 0; i < RECORDS; i++) {
        const every_kind_t record = random_record();
        net_bits_write_record(&writer, &layout, &record);
    }
    const size_t size = net_bits_writer_finish(&writer);
    TEST_ASSERT_TRUE(size > 0U);
    for (size_t length = 0U; length <= size; length++) {
        uint8_t *packet = exact_copy(buffer, length);
        guarded_t holder = guarded_alloc(sizeof(every_kind_t), 0x00);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, packet, length);
        bool ok = true;
        for (int i = 0; i < RECORDS && ok; i++) {
            ok = net_bits_read_record(&reader, &layout, holder.data);
        }
        if (length == size) {
            TEST_ASSERT_TRUE(ok && net_bits_reader_done(&reader));
        } else {
            CHECK(!ok && !reader.ok, "length %u of %u accepted", (unsigned)length, (unsigned)size);
        }
        TEST_ASSERT_TRUE(guarded_intact(&holder));
        guarded_free(&holder);
        free(packet);
    }
}

static void test_garbage_records_are_valid_or_refused(void) {
    rng_seed(14);
    const net_bits_layout_t layout = every_kind_layout();
    int accepted = 0;
    for (int round = 0; round < 3000; round++) {
        const size_t size = rng_below(40U);
        uint8_t source[40];
        for (size_t i = 0U; i < sizeof source; i++) {
            source[i] = (uint8_t)rng_u32();
        }
        uint8_t *packet = exact_copy(source, size);
        guarded_t holder = guarded_alloc(sizeof(every_kind_t), 0x00);
        net_bits_reader_t reader;
        net_bits_reader_init(&reader, packet, size);
        while (net_bits_read_record(&reader, &layout, holder.data)) {
            every_kind_t got;
            memcpy(&got, holder.data, sizeof got);
            check_valid(&got);
            accepted++;
        }
        TEST_ASSERT_FALSE(reader.ok);
        TEST_ASSERT_TRUE(guarded_intact(&holder));
        guarded_free(&holder);
        free(packet);
    }
    TEST_ASSERT_TRUE(accepted > 0);
}

// #region table hash

static uint64_t hash_of(const net_bits_field_t *fields, uint32_t count) {
    net_bits_layout_t layout;
    TEST_ASSERT_TRUE(net_bits_layout_init(&layout, fields, count, sizeof(every_kind_t)));
    return layout.hash;
}

static void test_hash_ignores_offsets_and_member_order(void) {
    net_bits_layout_t a;
    net_bits_layout_t b;
    TEST_ASSERT_TRUE(net_bits_layout_init(&a, EVERY_KIND, FIELD_COUNT, sizeof(every_kind_t)));
    TEST_ASSERT_TRUE(net_bits_layout_init(&b, REORDERED, FIELD_COUNT, sizeof(reordered_t)));
    TEST_ASSERT_TRUE(EVERY_KIND[0].offset != REORDERED[0].offset);
    TEST_ASSERT_EQUAL_HEX64(a.hash, b.hash);
    TEST_ASSERT_EQUAL_UINT32(a.bits, b.bits);

    /* Members a kind does not use are not on the wire. */
    net_bits_field_t noisy[FIELD_COUNT];
    memcpy(noisy, EVERY_KIND, sizeof noisy);
    noisy[5].int_min = 77;
    noisy[5].range.step = 3.0F;
    noisy[5].bits = 9U;
    noisy[0].int_max = -4;
    noisy[4].range.min = 1.0F;
    TEST_ASSERT_EQUAL_HEX64(a.hash, hash_of(noisy, FIELD_COUNT));
}

static void test_hash_changes_with_anything_on_the_wire(void) {
    const uint64_t base = hash_of(EVERY_KIND, FIELD_COUNT);
    enum { CHANGES = 11 };
    uint64_t seen[CHANGES + 1];
    seen[0] = base;
    for (int change = 0; change < CHANGES; change++) {
        net_bits_field_t fields[FIELD_COUNT];
        memcpy(fields, EVERY_KIND, sizeof fields);
        switch (change) {
        case 0: /* another kind with the same single bit */
            fields[5].kind = NET_BITS_FIELD_UINT;
            fields[5].bits = 1U;
            break;
        case 1:
            fields[0].bits = 11U;
            break;
        case 2:
            fields[3].int_min = -6;
            break;
        case 3:
            fields[3].int_max = 1001;
            break;
        case 4:
            fields[1].range.min = nextafterf(fields[1].range.min, 0.0F);
            break;
        case 5:
            fields[1].range.max = nextafterf(fields[1].range.max, INFINITY);
            break;
        case 6:
            fields[2].range.step = nextafterf(fields[2].range.step, INFINITY);
            break;
        case 7:
            fields[4].bits = 10U;
            break;
        case 8: { /* order */
            const net_bits_field_t swap = fields[1];
            fields[1] = fields[2];
            fields[2] = swap;
            break;
        }
        case 9: { /* two lines of one kind swapped */
            const net_bits_field_t swap = fields[0];
            fields[0] = fields[5];
            fields[5] = swap;
            break;
        }
        default:
            break;
        }
        const uint64_t hash = change == 10 ? hash_of(fields, FIELD_COUNT - 1U) : hash_of(fields, FIELD_COUNT);
        for (int i = 0; i <= change; i++) {
            CHECK(hash != seen[i], "change %d collides with %d", change, i - 1);
        }
        seen[change + 1] = hash;
    }
}

/* The canonical form a second implementation must hash: README.md, "Table hash". */
static void test_hash_is_fnv1a_over_the_canonical_form(void) {
    const net_bits_field_t fields[] = {
        {.kind = NET_BITS_FIELD_UINT, .bits = 10U},
        {.kind = NET_BITS_FIELD_INT, .int_min = -5, .int_max = 1000},
        {.kind = NET_BITS_FIELD_RANGED, .range = {-1.0F, 1.0F, 0.5F}},
    };
    uint8_t bytes[1 + 3 * 22] = {0};
    bytes[0] = 1U; /* wire rules */
    bytes[1] = NET_BITS_FIELD_UINT;
    bytes[2] = 10U;
    bytes[23] = NET_BITS_FIELD_INT;
    bytes[24] = 10U;
    const uint8_t minus_five[4] = {0xFB, 0xFF, 0xFF, 0xFF};
    const uint8_t thousand[4] = {0xE8, 0x03, 0x00, 0x00};
    memcpy(&bytes[25], minus_five, 4U);
    memcpy(&bytes[29], thousand, 4U);
    bytes[45] = NET_BITS_FIELD_RANGED;
    bytes[46] = 3U;
    const uint8_t range[12] = {0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x3F};
    memcpy(&bytes[55], range, sizeof range);
    uint64_t expected = 0xcbf29ce484222325ULL;
    for (size_t i = 0U; i < sizeof bytes; i++) {
        expected = (expected ^ bytes[i]) * 0x100000001b3ULL;
    }
    TEST_ASSERT_EQUAL_HEX64(expected, hash_of(fields, 3U));
}

static void test_hash_mix_is_order_sensitive(void) {
    rng_seed(15);
    const uint64_t base = hash_of(EVERY_KIND, FIELD_COUNT);
    for (int i = 0; i < 1000; i++) {
        const uint64_t a = rng_next();
        const uint64_t b = rng_next();
        TEST_ASSERT_TRUE(net_bits_hash_mix(base, a) != base);
        TEST_ASSERT_EQUAL_HEX64(net_bits_hash_mix(base, a), net_bits_hash_mix(base, a));
        TEST_ASSERT_TRUE(net_bits_hash_mix(net_bits_hash_mix(base, a), b) != net_bits_hash_mix(net_bits_hash_mix(base, b), a));
    }
    TEST_ASSERT_TRUE(net_bits_hash_mix(net_bits_hash_mix(base, 1U), 2U) != net_bits_hash_mix(net_bits_hash_mix(base, 2U), 1U));
}

// #endregion

static void test_malformed_tables_are_refused(void) {
    const net_bits_field_t bad[] = {
        {.kind = 0},
        {.kind = 6},
        {.kind = NET_BITS_FIELD_UINT, .bits = 0U},
        {.kind = NET_BITS_FIELD_UINT, .bits = 33U},
        {.kind = NET_BITS_FIELD_QUAT, .bits = NET_BITS_QUAT_BITS_MIN - 1U},
        {.kind = NET_BITS_FIELD_QUAT, .bits = NET_BITS_QUAT_BITS_MAX + 1U},
        {.kind = NET_BITS_FIELD_INT, .int_min = 1, .int_max = 0},
        {.kind = NET_BITS_FIELD_RANGED, .range = {0.0F, 1.0F, 0.0F}},
        {.kind = NET_BITS_FIELD_RANGED, .range = {1.0F, 1.0F, 0.1F}},
        {.kind = NET_BITS_FIELD_RANGED, .range = {NAN, 1.0F, 0.1F}},
        {.kind = NET_BITS_FIELD_RANGED, .range = {0.0F, 1.0F, 1.0e-9F}},
    };
    for (size_t i = 0U; i < sizeof bad / sizeof bad[0]; i++) {
        net_bits_field_t fields[FIELD_COUNT + 1];
        memcpy(fields, EVERY_KIND, sizeof EVERY_KIND);
        fields[FIELD_COUNT] = bad[i];
        net_bits_layout_t layout;
        CHECK(!net_bits_layout_init(&layout, fields, FIELD_COUNT + 1U, sizeof(every_kind_t)), "bad line %u accepted",
              (unsigned)i);
        TEST_ASSERT_EQUAL_UINT32(0U, layout.count);
        TEST_ASSERT_TRUE(layout.fields == NULL);
    }
    net_bits_layout_t layout;
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, NULL, 1U, sizeof(every_kind_t)));
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, NULL, 0U, sizeof(every_kind_t)));
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, EVERY_KIND, 0U, sizeof(every_kind_t)));

    const net_bits_field_t edges[] = {
        {.kind = NET_BITS_FIELD_INT, .int_min = 7, .int_max = 7},
        {.kind = NET_BITS_FIELD_INT, .int_min = INT32_MIN, .int_max = INT32_MAX},
        {.kind = NET_BITS_FIELD_UINT, .bits = 32U},
    };
    TEST_ASSERT_TRUE(net_bits_layout_init(&layout, edges, 3U, sizeof(int32_t)));
    TEST_ASSERT_EQUAL_UINT32(64U, layout.bits);
    /* A table that puts nothing on the wire would make every record call a silent no-op. */
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, edges, 1U, sizeof(int32_t)));
    TEST_ASSERT_EQUAL_UINT32(0U, layout.bits);

    /* Each member must lie inside the record. */
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, edges, 3U, sizeof(int32_t) - 1U));
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, EVERY_KIND, FIELD_COUNT, offsetof(every_kind_t, height) + 3U));
    TEST_ASSERT_TRUE(net_bits_layout_init(&layout, EVERY_KIND, FIELD_COUNT, offsetof(every_kind_t, height) + 4U));
    const net_bits_field_t quat_line[] = {{.kind = NET_BITS_FIELD_QUAT, .bits = 9U, .offset = 4U}};
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, quat_line, 1U, 19U));
    TEST_ASSERT_TRUE(net_bits_layout_init(&layout, quat_line, 1U, 20U));
    const net_bits_field_t far_line[] = {{.kind = NET_BITS_FIELD_BOOL, .offset = UINT16_MAX}};
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, far_line, 1U, UINT16_MAX));
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, far_line, 1U, 0U));
}

static void test_a_failed_layout_is_refused_by_the_record_calls(void) {
    const net_bits_field_t bad[] = {{.kind = 9}};
    net_bits_layout_t layout;
    TEST_ASSERT_FALSE(net_bits_layout_init(&layout, bad, 1U, sizeof(every_kind_t)));
    every_kind_t record;
    memset(&record, 0, sizeof record);
    uint8_t buffer[32] = {0};
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write_record(&writer, &layout, &record);
    TEST_ASSERT_FALSE(writer.ok);
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, buffer, sizeof buffer);
    TEST_ASSERT_FALSE(net_bits_read_record(&reader, &layout, &record));
    TEST_ASSERT_FALSE(reader.ok);
}

static void test_a_stray_bool_byte_is_written_as_true(void) {
    const net_bits_layout_t layout = every_kind_layout();
    every_kind_t record;
    memset(&record, 0, sizeof record);
    record.rot[3] = 1.0F;
    const uint8_t stray = 2U;
    memcpy(&record.asleep, &stray, 1U);
    uint8_t buffer[32];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write_record(&writer, &layout, &record);
    TEST_ASSERT_TRUE(writer.ok);
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, buffer, net_bits_writer_finish(&writer));
    reader.bits = layout.bits - 1U;
    TEST_ASSERT_EQUAL_UINT32(1U, net_bits_read(&reader, 1U));
}

typedef struct body_copy {
    uint32_t id;
    float x, y, z;
    float rotation[4];
    bool asleep;
} body_copy_t;

/* README.md, "Worked example" and "Known answers": bytes a second implementation must match. */
static void test_known_answers_of_the_worked_example(void) {
    static const net_bits_field_t BODY_COPY[] = {
        NET_BITS_UINT(body_copy_t, id, 10),
        NET_BITS_RANGED(body_copy_t, x, -32.0, 32.0 - 1.0 / 512, 1.0 / 512),
        NET_BITS_RANGED(body_copy_t, y, -32.0, 32.0 - 1.0 / 512, 1.0 / 512),
        NET_BITS_RANGED(body_copy_t, z, 0.0, 8.0 - 1.0 / 512, 1.0 / 512),
        NET_BITS_QUAT(body_copy_t, rotation, 9),
        NET_BITS_BOOL(body_copy_t, asleep),
    };
    net_bits_layout_t layout;
    TEST_ASSERT_TRUE(net_bits_layout_init(&layout, BODY_COPY, 6U, sizeof(body_copy_t)));
    TEST_ASSERT_EQUAL_UINT32(82U, layout.bits);
    TEST_ASSERT_EQUAL_HEX64(0x7B6EACCC409DC632ULL, layout.hash);
    const body_copy_t sent = {517U, 1.3F, -20.25F, 1.5F, {0.2F, 0.4F, -0.4F, 0.8F}, true};
    const uint8_t expected[11] = {0x05, 0x6A, 0x0A, 0x01, 0x2F, 0x00, 0xF3, 0xD1, 0xC7, 0x6F, 0x02};
    uint8_t buffer[32];
    net_bits_writer_t writer;
    net_bits_writer_init(&writer, buffer, sizeof buffer);
    net_bits_write_record(&writer, &layout, &sent);
    TEST_ASSERT_EQUAL_UINT32(sizeof expected, (uint32_t)net_bits_writer_finish(&writer));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buffer, sizeof expected);
    net_bits_reader_t reader;
    net_bits_reader_init(&reader, expected, sizeof expected);
    body_copy_t got;
    TEST_ASSERT_TRUE(net_bits_read_record(&reader, &layout, &got));
    TEST_ASSERT_TRUE(net_bits_reader_done(&reader));
    const uint32_t reads[7] = {0x3FA68000U, 0xC1A20000U, 0x3FC00000U, 0x3E4C7204U, 0x3ECC7204U, 0xBECC7204U, 0x3F4CFFCCU};
    const float values[7] = {got.x, got.y, got.z, got.rotation[0], got.rotation[1], got.rotation[2], got.rotation[3]};
    TEST_ASSERT_EQUAL_UINT32(517U, got.id);
    TEST_ASSERT_TRUE(got.asleep);
    for (int i = 0; i < 7; i++) {
        uint32_t bits;
        memcpy(&bits, &values[i], sizeof bits);
        CHECK(bits == reads[i], "value %d reads 0x%08X", i, (unsigned)bits);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_record_round_trip_with_every_kind);
    RUN_TEST(test_record_writer_refusals);
    RUN_TEST(test_failed_record_keeps_the_failing_member_and_those_after);
    RUN_TEST(test_every_truncation_of_records_is_refused);
    RUN_TEST(test_garbage_records_are_valid_or_refused);
    RUN_TEST(test_hash_ignores_offsets_and_member_order);
    RUN_TEST(test_hash_changes_with_anything_on_the_wire);
    RUN_TEST(test_hash_is_fnv1a_over_the_canonical_form);
    RUN_TEST(test_hash_mix_is_order_sensitive);
    RUN_TEST(test_malformed_tables_are_refused);
    RUN_TEST(test_a_failed_layout_is_refused_by_the_record_calls);
    RUN_TEST(test_a_stray_bool_byte_is_written_as_true);
    RUN_TEST(test_known_answers_of_the_worked_example);
    return UNITY_END();
}
