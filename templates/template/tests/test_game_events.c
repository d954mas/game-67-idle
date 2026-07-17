/* Unity ctest for game_events.c. Two ctest binaries compile THIS file:
   - test_game_events           (default): positive cases 1-7 + death-tests 8/9/11
     (overflow/phase = NT_ASSERT, via NT_TEST_EXPECT_ASSERT).
   - test_game_events_overflow  (GAME_EVENTS_SOFT_OVERFLOW=1): only case #10, the
     release drop-path (overflow's NT_ASSERT compiled out by the test seam).
   Both link against small GAME_EVENTS_ARENA_BYTES/LOG_CAP overrides (CMakeLists)
   so overflow is cheap to force. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* clang-format off */
#include "game_events.h"
#include "hash/nt_hash.h"
#ifndef GAME_EVENTS_SOFT_OVERFLOW
#include "test_helpers/nt_assert_trap.h" /* NT_TEST_EXPECT_ASSERT: death-tests 8/9/11 */
#endif
#include "unity.h"
/* clang-format on */

static nt_hash64_t T(const char *s) { return nt_hash64_str(s); }

void setUp(void) {
    game_events_init();
#ifndef GAME_EVENTS_SOFT_OVERFLOW
    nt_test_assert_install();
#endif
}
void tearDown(void) { game_events_shutdown(); }

#ifndef GAME_EVENTS_SOFT_OVERFLOW

/* 1. emit -> walk: 3 payloads, seq/tick/type/size/content all check out. */
static void test_emit_then_walk(void) {
    const int32_t a = 11, b = 22, c = 33;
    const nt_hash64_t ta = T("test.a"), tb = T("test.b"), tc = T("test.c");
    (void)game_event_emit(ta, &a, (uint32_t)sizeof a, _Alignof(int32_t));
    (void)game_event_emit(tb, &b, (uint32_t)sizeof b, _Alignof(int32_t));
    (void)game_event_emit(tc, &c, (uint32_t)sizeof c, _Alignof(int32_t));

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_UINT64(0U, log[0].seq);
    TEST_ASSERT_EQUAL_UINT64(1U, log[1].seq);
    TEST_ASSERT_EQUAL_UINT64(2U, log[2].seq);
    TEST_ASSERT_EQUAL_UINT32(log[0].tick, log[1].tick);
    TEST_ASSERT_EQUAL_UINT32(log[1].tick, log[2].tick);
    TEST_ASSERT_EQUAL_UINT64(ta.value, log[0].type.value);
    TEST_ASSERT_EQUAL_UINT64(tb.value, log[1].type.value);
    TEST_ASSERT_EQUAL_UINT64(tc.value, log[2].type.value);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof a, log[0].size);
    TEST_ASSERT_EQUAL_MEMORY(&a, log[0].payload, sizeof a);
    TEST_ASSERT_EQUAL_MEMORY(&b, log[1].payload, sizeof b);
    TEST_ASSERT_EQUAL_MEMORY(&c, log[2].payload, sizeof c);
}

/* 2. seq is globally monotonic across frame_reset; tick bumps. */
static void test_seq_monotonic_across_frames(void) {
    const int32_t x = 1, y = 2, z = 3;
    (void)game_event_emit(T("test.a"), &x, (uint32_t)sizeof x, _Alignof(int32_t));
    (void)game_event_emit(T("test.a"), &y, (uint32_t)sizeof y, _Alignof(int32_t));
    TEST_ASSERT_EQUAL_UINT32(0U, game_events_tick());

    game_event_frame_reset();
    TEST_ASSERT_EQUAL_UINT32(1U, game_events_tick());

    (void)game_event_emit(T("test.a"), &z, (uint32_t)sizeof z, _Alignof(int32_t));
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n); /* frame_reset cleared the log */
    TEST_ASSERT_EQUAL_UINT64(2U, log[0].seq); /* seq keeps counting up */
    TEST_ASSERT_EQUAL_UINT32(1U, log[0].tick);
}

/* 3. frame_reset clears the log. */
static void test_frame_reset_clears_log(void) {
    const int32_t v = 7;
    (void)game_event_emit(T("test.a"), &v, (uint32_t)sizeof v, _Alignof(int32_t));
    int n = -1;
    (void)game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(1, n);

    game_event_frame_reset();
    n = -1;
    (void)game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(0, n);
}

/* 4. cascade in the same frame: fixed arena never moves -> holding a pointer
   across a later emit (simulating a reactor cascade) stays legal. */
static void test_cascade_same_frame_pointer_stability(void) {
    const int32_t va = 42;
    const void *pa = game_event_emit(T("test.a"), &va, (uint32_t)sizeof va, _Alignof(int32_t));
    TEST_ASSERT_NOT_NULL(pa);
    int n1 = 0;
    const game_event_t *log1 = game_event_log(&n1);
    TEST_ASSERT_EQUAL_INT(1, n1);
    const uint64_t seq_a = log1[0].seq;

    const int32_t vb = 99; /* simulated reactor cascade emit */
    (void)game_event_emit(T("test.b"), &vb, (uint32_t)sizeof vb, _Alignof(int32_t));

    int n2 = 0;
    const game_event_t *log2 = game_event_log(&n2);
    TEST_ASSERT_EQUAL_INT(2, n2);
    TEST_ASSERT_EQUAL_UINT64(seq_a + 1U, log2[1].seq);

    /* pa (held across the second emit) and log2[0] must still read correctly:
       fixed arena -> no rebase, holding a pointer through emit is legal. */
    TEST_ASSERT_TRUE(log2[0].payload == pa);
    TEST_ASSERT_EQUAL_INT(42, *(const int32_t *)pa);
    TEST_ASSERT_EQUAL_INT(42, *(const int32_t *)log2[0].payload);
}

/* 5. alignment contract: several powers of 2 up to the ceiling all yield a
   correctly aligned pointer (base is malloc-aligned to max_align_t -> offset-
   by-align stays aligned). The original cases use {1,8,16} assuming a >=16-byte
   max_align_t (common on glibc/gcc); THIS toolchain (clang targeting the MSVC
   ABI on Windows) has _Alignof(max_align_t)==8 -- 16 would exceed the very
   ceiling the contract defines and correctly NT_ASSERT. Deriving the top value
   from _Alignof(max_align_t) keeps the same intent (smallest/mid/ceiling)
   portable across ABIs instead of baking in a platform-specific magic number. */
static void test_alignment_powers_of_two_up_to_ceiling(void) {
    const uint64_t payload = 0x1122334455667788ULL;
    const size_t max_align = (size_t)_Alignof(max_align_t);
    const size_t aligns[3] = {1U, max_align > 1U ? max_align / 2U : 1U, max_align};
    for (size_t i = 0; i < 3U; ++i) {
        const void *p = game_event_emit(T("test.align"), &payload, (uint32_t)sizeof payload, aligns[i]);
        TEST_ASSERT_NOT_NULL(p);
        TEST_ASSERT_EQUAL_UINT64(0U, (uint64_t)((uintptr_t)p % aligns[i]));
    }
    /* Rounding must actually fire: a 1-byte payload leaves the arena cursor at
       an odd offset; the next aligned emit must round it up. Without the
       round-up in the allocator, p == base+odd and the modulo check fails. */
    const uint8_t one = 0xA5U;
    (void)game_event_emit(T("test.align.odd"), &one, 1U, 1U);
    const void *p = game_event_emit(T("test.align.rounded"), &payload, (uint32_t)sizeof payload, max_align);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_UINT64(0U, (uint64_t)((uintptr_t)p % max_align));
}

/* 6. react fixpoint: (a) continuous cascading stops within the generation cap;
   (b) zero cascades -> zero idle passes (LOW-8). */
static void test_react_generation_cap(void) {
    game_events_react_begin();
    int iterations = 0;
    do {
        const int32_t v = iterations;
        (void)game_event_emit(T("test.cascade"), &v, (uint32_t)sizeof v, _Alignof(int32_t));
        iterations++;
    } while (game_events_react_progressed());
    TEST_ASSERT_TRUE(iterations >= 1);
    TEST_ASSERT_TRUE(iterations <= GAME_EVENTS_MAX_GENERATIONS);

    /* fresh frame, no cascades emitted in the body -> immediate fixpoint. */
    game_event_frame_reset();
    game_events_react_begin();
    TEST_ASSERT_FALSE(game_events_react_progressed());
}

/* 7. consumer cursor resets on tick CHANGE, not on event count (HIGH-2): frame A
   drains 5, frame B emits 8 -- the canon idiom must see all 8, not silently drop
   B[0..5). */
static void test_cursor_reset_on_tick_change(void) {
    static int s_pos;
    static uint32_t s_last_tick;
    static bool s_init;
    if (!s_init) {
        s_pos = 0;
        s_last_tick = 0;
        s_init = true;
    }

    for (int i = 0; i < 5; ++i) {
        const int32_t v = i;
        (void)game_event_emit(T("test.drain"), &v, (uint32_t)sizeof v, _Alignof(int32_t));
    }
    {
        uint32_t tick = game_events_tick();
        if (tick != s_last_tick) {
            s_last_tick = tick;
            s_pos = 0;
        }
        int n = 0;
        const game_event_t *log = game_event_log(&n);
        for (; s_pos < n; ++s_pos) {
            (void)log[s_pos];
        }
    }
    TEST_ASSERT_EQUAL_INT(5, s_pos);

    game_event_frame_reset(); /* tick bumps -> next drain must reset s_pos */

    for (int i = 0; i < 8; ++i) {
        const int32_t v = i;
        (void)game_event_emit(T("test.drain"), &v, (uint32_t)sizeof v, _Alignof(int32_t));
    }
    int seen = 0;
    {
        uint32_t tick = game_events_tick();
        if (tick != s_last_tick) {
            s_last_tick = tick;
            s_pos = 0;
        }
        int n = 0;
        const game_event_t *log = game_event_log(&n);
        for (; s_pos < n; ++s_pos) {
            (void)log[s_pos];
            seen++;
        }
    }
    TEST_ASSERT_EQUAL_INT(8, seen); /* all of frame B seen, none silently lost */
    TEST_ASSERT_EQUAL_INT(8, s_pos);
}

static void test_capacity_probe_is_exact_and_side_effect_free(void) {
    const uint8_t one = 0x5A;
    TEST_ASSERT_TRUE(game_event_can_emit(1U, 1U));
    for (int i = 0; i < GAME_EVENTS_LOG_CAP; i++) {
        TEST_ASSERT_NOT_NULL(game_event_emit(T("test.probe"), &one, 1U, 1U));
    }
    TEST_ASSERT_FALSE(game_event_can_emit(1U, 1U));
    TEST_ASSERT_EQUAL_UINT32(0U, game_events_dropped());
    int count = -1;
    (void)game_event_log(&count);
    TEST_ASSERT_EQUAL_INT(GAME_EVENTS_LOG_CAP, count);
}

static void test_capacity_probe_matches_aligned_arena_boundary(void) {
    const uint8_t one = 0x5A;
    TEST_ASSERT_NOT_NULL(game_event_emit(T("test.odd"), &one, 1U, 1U));
    const size_t align = _Alignof(max_align_t);
    const size_t aligned_offset = align;
    const uint32_t fit = (uint32_t)((size_t)GAME_EVENTS_ARENA_BYTES - aligned_offset);
    TEST_ASSERT_TRUE(game_event_can_emit(fit, align));
    TEST_ASSERT_FALSE(game_event_can_emit(fit + 1U, align));
    TEST_ASSERT_EQUAL_UINT32(0U, game_events_dropped());

    uint8_t payload[GAME_EVENTS_ARENA_BYTES];
    memset(payload, 0xA5, sizeof payload);
    TEST_ASSERT_NOT_NULL(game_event_emit(T("test.fit"), payload, fit, align));
    TEST_ASSERT_FALSE(game_event_can_emit(1U, 1U));
    TEST_ASSERT_EQUAL_UINT32(0U, game_events_dropped());
}

/* Death-tests are only meaningful when asserts longjmp (FULL): in TRAP/OFF
   builds the overflow assert is compiled out or traps instead of returning. */
#if NT_ASSERT_MODE == NT_ASSERT_FULL
/* 8. arena overflow -> NT_ASSERT (death-test; GAME_EVENTS_ARENA_BYTES=1024
   override from CMakeLists makes this cheap). */
static void test_arena_overflow_asserts(void) {
    uint8_t big[1000];
    memset(big, 0xAB, sizeof big);
    TEST_ASSERT_NOT_NULL(game_event_emit(T("test.big"), big, (uint32_t)sizeof big, 1U));
    /* off=1000, need=1000+100=1100 > 1024 -> overflow. */
    uint8_t more[100];
    memset(more, 0xCD, sizeof more);
    NT_TEST_EXPECT_ASSERT(game_event_emit(T("test.big"), more, (uint32_t)sizeof more, 1U));
}

/* 9. log cap overflow -> NT_ASSERT (GAME_EVENTS_LOG_CAP=64 override). */
static void test_log_cap_overflow_asserts(void) {
    const uint8_t one = 0x5A;
    for (int i = 0; i < GAME_EVENTS_LOG_CAP; ++i) {
        TEST_ASSERT_NOT_NULL(game_event_emit(T("test.tiny"), &one, 1U, 1U));
    }
    NT_TEST_EXPECT_ASSERT(game_event_emit(T("test.tiny"), &one, 1U, 1U));
}

/* 11. emit during RECORD phase -> NT_ASSERT (symmetry guard). */
static void test_phase_assert_on_record(void) {
    const int32_t v = 1;
    game_events_set_phase(GAME_EVENT_PHASE_RECORD);
    NT_TEST_EXPECT_ASSERT(game_event_emit(T("test.a"), &v, (uint32_t)sizeof v, _Alignof(int32_t)));
}
#endif /* NT_ASSERT_MODE == NT_ASSERT_FULL */

#else /* GAME_EVENTS_SOFT_OVERFLOW: release drop-path test seam */

/* 10. release-semantics drop: with the debug-assert compiled out (test seam),
   overflow drops the event (NULL + dropped++) and leaves earlier entries intact. */
static void test_overflow_drop_semantics(void) {
    TEST_ASSERT_EQUAL_UINT32(0U, game_events_dropped());
    const uint8_t one = 0x5A;
    for (int i = 0; i < GAME_EVENTS_LOG_CAP; ++i) {
        TEST_ASSERT_NOT_NULL(game_event_emit(T("test.tiny"), &one, 1U, 1U));
    }
    /* 65th event: log cap full -> dropped, NOT asserted (SOFT_OVERFLOW seam). */
    const void *dropped = game_event_emit(T("test.tiny"), &one, 1U, 1U);
    TEST_ASSERT_NULL(dropped);
    TEST_ASSERT_EQUAL_UINT32(1U, game_events_dropped());

    /* earlier events are untouched: seq/content still readable. */
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(GAME_EVENTS_LOG_CAP, n);
    TEST_ASSERT_EQUAL_UINT64(0U, log[0].seq);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(GAME_EVENTS_LOG_CAP - 1), log[n - 1].seq);
    TEST_ASSERT_EQUAL_UINT32(1U, log[0].size);
    TEST_ASSERT_EQUAL_MEMORY(&one, log[0].payload, 1U);
}

#endif /* GAME_EVENTS_SOFT_OVERFLOW */

int main(void) {
    nt_hash_init(&(nt_hash_desc_t){0}); /* idempotent-errors on repeat -> call exactly once */

    UNITY_BEGIN();
#ifndef GAME_EVENTS_SOFT_OVERFLOW
    RUN_TEST(test_emit_then_walk);
    RUN_TEST(test_seq_monotonic_across_frames);
    RUN_TEST(test_frame_reset_clears_log);
    RUN_TEST(test_cascade_same_frame_pointer_stability);
    RUN_TEST(test_alignment_powers_of_two_up_to_ceiling);
    RUN_TEST(test_react_generation_cap);
    RUN_TEST(test_cursor_reset_on_tick_change);
    RUN_TEST(test_capacity_probe_is_exact_and_side_effect_free);
    RUN_TEST(test_capacity_probe_matches_aligned_arena_boundary);
#if NT_ASSERT_MODE == NT_ASSERT_FULL
    RUN_TEST(test_arena_overflow_asserts);
    RUN_TEST(test_log_cap_overflow_asserts);
    RUN_TEST(test_phase_assert_on_record);
#endif
#else
    RUN_TEST(test_overflow_drop_semantics);
#endif
    const int result = UNITY_END();

    nt_hash_shutdown();
    return result;
}
