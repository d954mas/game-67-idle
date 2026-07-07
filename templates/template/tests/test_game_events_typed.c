/* Unity ctest for the E2 typed event layer (§E2.10 case 5). Compiles the COMMITTED
   golden mini events (rich fixture: i64/f64/hash/bool/string/bytes + scalar-only) on
   top of the frozen E1 transport -- no build-time generation. Exercises emit -> typed
   read -> string/bytes accessors, retain out of the arena through an ALIGNED union
   (blind memcpy, positional independence), the scalar-only path, the descriptor, and
   event ordering. */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* clang-format off */
#include "game_events.h"
#include "hash/nt_hash.h"
#include "mini_state_events.gen.h"
#include "unity.h"
/* clang-format on */

void setUp(void) { game_events_init(); }
void tearDown(void) { game_events_shutdown(); }

/* 1. emit -> typed read: every field type round-trips, accessors read inline data. */
static void test_emit_typed_read(void) {
    const uint8_t blob[3] = {1, 2, 3};
    const nt_hash64_t epic = nt_hash64_str("Epic");
    const void *p = mini_emit_cell_spawned(42, 3.5, epic, true, "hello", blob, 3);
    TEST_ASSERT_NOT_NULL(p);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_UINT64(mini_ev_cell_spawned_type().value, log[0].type.value);

    const MiniEvCellSpawned *ev = (const MiniEvCellSpawned *)log[0].payload;
    TEST_ASSERT_EQUAL_INT64(42, ev->total);
    TEST_ASSERT_TRUE(fabs(ev->rate - 3.5) < 1e-9); /* Unity double macros not assumed */
    TEST_ASSERT_EQUAL_UINT64(epic.value, ev->kind.value);
    TEST_ASSERT_TRUE(ev->urgent);
    TEST_ASSERT_EQUAL_STRING("hello", mini_ev_cell_spawned_label(ev)); /* accessor, NEVER ev->label */
    TEST_ASSERT_EQUAL_UINT32(3u, mini_ev_cell_spawned_blob_len(ev));
    TEST_ASSERT_EQUAL_MEMORY(blob, mini_ev_cell_spawned_blob(ev), 3);
}

/* 2. retain / positional independence: copy the event OUT of the arena into an
   ALIGNED union (uint8_t[] would be align 1 -> casting to the struct is UB, M1); the
   payload-relative offsets keep reading correctly in the copy (blind memcpy is sound). */
static void test_retain_positional_independence(void) {
    const uint8_t blob[2] = {0xAA, 0xBB};
    (void)mini_emit_cell_spawned(7, 1.25, nt_hash64_str("Rare"), false, "world", blob, 2);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    union {
        MiniEvCellSpawned ev;
        uint8_t bytes[GAME_EVENT_EMIT_MAX];
    } keep;
    memcpy(&keep, log[0].payload, log[0].size);

    TEST_ASSERT_EQUAL_INT64(7, keep.ev.total);
    TEST_ASSERT_EQUAL_STRING("world", mini_ev_cell_spawned_label(&keep.ev));
    TEST_ASSERT_EQUAL_UINT32(2u, mini_ev_cell_spawned_blob_len(&keep.ev));
    TEST_ASSERT_EQUAL_MEMORY(blob, mini_ev_cell_spawned_blob(&keep.ev), 2);
}

/* 3. scalar-only path: no staging, payload size == sizeof(struct). */
static void test_scalar_only(void) {
    const void *p = mini_emit_ticked(7);
    TEST_ASSERT_NOT_NULL(p);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_UINT64(mini_ev_ticked_type().value, log[0].type.value);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(MiniEvTicked), log[0].size);

    const MiniEvTicked *ev = (const MiniEvTicked *)log[0].payload;
    TEST_ASSERT_EQUAL_INT32(7, ev->count);
}

/* 4. descriptor: names, count, string/bytes field type + offsets + len_offset. */
static void test_descriptor(void) {
    TEST_ASSERT_EQUAL_STRING("mini.cell_spawned", mini_ev_cell_spawned_desc.name);
    TEST_ASSERT_EQUAL_INT(6, mini_ev_cell_spawned_desc.field_count);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(MiniEvCellSpawned), mini_ev_cell_spawned_desc.payload_size);

    const game_event_field_t *f = mini_ev_cell_spawned_desc.fields;
    TEST_ASSERT_EQUAL_INT(GAME_EVENT_FT_STRING, f[4].type);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)offsetof(MiniEvCellSpawned, label), f[4].offset);
    TEST_ASSERT_EQUAL_INT(GAME_EVENT_FT_BYTES, f[5].type);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)offsetof(MiniEvCellSpawned, blob), f[5].offset);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)offsetof(MiniEvCellSpawned, blob_len), f[5].len_offset);

    TEST_ASSERT_EQUAL_INT(2, mini_ev_desc_count);
}

/* 5. empty string / zero-length bytes: accessor yields "", blob_len == 0. */
static void test_empty_string_and_bytes(void) {
    (void)mini_emit_cell_spawned(0, 0.0, nt_hash64_str("none"), false, NULL, NULL, 0);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    const MiniEvCellSpawned *ev = (const MiniEvCellSpawned *)log[0].payload;
    TEST_ASSERT_EQUAL_STRING("", mini_ev_cell_spawned_label(ev));
    TEST_ASSERT_EQUAL_UINT32(0u, mini_ev_cell_spawned_blob_len(ev));
}

/* 6. order: emit ticked then cell_spawned; walk by index; seq monotonic, types match. */
static void test_event_order(void) {
    const uint8_t blob[1] = {9};
    (void)mini_emit_ticked(3);
    (void)mini_emit_cell_spawned(5, 2.0, nt_hash64_str("Common"), true, "x", blob, 1);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_UINT64(log[0].seq + 1U, log[1].seq); /* monotonic */
    TEST_ASSERT_EQUAL_UINT64(mini_ev_ticked_type().value, log[0].type.value);
    TEST_ASSERT_EQUAL_UINT64(mini_ev_cell_spawned_type().value, log[1].type.value);

    const MiniEvTicked *t = (const MiniEvTicked *)log[0].payload;
    const MiniEvCellSpawned *c = (const MiniEvCellSpawned *)log[1].payload;
    TEST_ASSERT_EQUAL_INT32(3, t->count);
    TEST_ASSERT_EQUAL_INT64(5, c->total);
}

int main(void) {
    nt_hash_init(&(nt_hash_desc_t){0}); /* once: type-hash accessors need hash init */

    UNITY_BEGIN();
    RUN_TEST(test_emit_typed_read);
    RUN_TEST(test_retain_positional_independence);
    RUN_TEST(test_scalar_only);
    RUN_TEST(test_descriptor);
    RUN_TEST(test_empty_string_and_bytes);
    RUN_TEST(test_event_order);
    const int result = UNITY_END();

    nt_hash_shutdown();
    return result;
}
