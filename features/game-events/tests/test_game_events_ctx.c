/* Caller-owned event contexts: isolation from each other and from the global
   default context, and the prev window a swap opens and the next swap closes. */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* clang-format off */
#include "core/nt_assert.h"
#include "game_events.h"
#include "hash/nt_hash.h"
#include "test_helpers/nt_assert_trap.h"
#include "unity.h"
/* clang-format on */

#define ARENA_BYTES 256u
#define LOG_CAP 8u

typedef struct {
    _Alignas(max_align_t) uint8_t bytes[2u * ARENA_BYTES + 2u * LOG_CAP * sizeof(game_event_t)];
} ctx_memory_t;

static ctx_memory_t s_mem_a, s_mem_b;
static game_events_ctx_t s_a, s_b;

static nt_hash64_t T(const char *s) { return nt_hash64_str(s); }

static const int32_t *emit_i32(game_events_ctx_t *ctx, const char *type, int32_t value) {
    return (const int32_t *)game_events_ctx_emit(ctx, T(type), &value, (uint32_t)sizeof value, _Alignof(int32_t));
}

void setUp(void) {
    nt_test_assert_install();
    game_events_init();
    TEST_ASSERT_EQUAL_size_t(sizeof s_mem_a.bytes, game_events_ctx_memory_bytes(ARENA_BYTES, LOG_CAP));
    game_events_ctx_init(&s_a, s_mem_a.bytes, sizeof s_mem_a.bytes, ARENA_BYTES, LOG_CAP);
    game_events_ctx_init(&s_b, s_mem_b.bytes, sizeof s_mem_b.bytes, ARENA_BYTES, LOG_CAP);
}

void tearDown(void) { game_events_shutdown(); }

static void test_two_contexts_and_the_default_are_isolated(void) {
    (void)emit_i32(&s_a, "test.a", 1);
    (void)emit_i32(&s_a, "test.a", 2);
    (void)emit_i32(&s_b, "test.b", 10);
    const int32_t global = 99;
    (void)game_event_emit(T("test.global"), &global, (uint32_t)sizeof global, _Alignof(int32_t));

    int na = 0, nb = 0, ng = 0;
    const game_event_t *la = game_events_ctx_log(&s_a, &na);
    const game_event_t *lb = game_events_ctx_log(&s_b, &nb);
    (void)game_event_log(&ng);
    TEST_ASSERT_EQUAL_INT(2, na);
    TEST_ASSERT_EQUAL_INT(1, nb);
    TEST_ASSERT_EQUAL_INT(1, ng);
    TEST_ASSERT_EQUAL_UINT64(0u, la[0].seq);
    TEST_ASSERT_EQUAL_UINT64(1u, la[1].seq);
    TEST_ASSERT_EQUAL_UINT64(0u, lb[0].seq); /* seq is per context */
    TEST_ASSERT_EQUAL_INT32(2, *(const int32_t *)la[1].payload);
    TEST_ASSERT_EQUAL_INT32(10, *(const int32_t *)lb[0].payload);

    game_events_ctx_swap(&s_a);
    TEST_ASSERT_EQUAL_UINT32(1u, game_events_ctx_tick(&s_a));
    TEST_ASSERT_EQUAL_UINT32(0u, game_events_ctx_tick(&s_b));
    TEST_ASSERT_EQUAL_UINT32(0u, game_events_tick());
    (void)game_events_ctx_log(&s_b, &nb);
    (void)game_event_log(&ng);
    TEST_ASSERT_EQUAL_INT(1, nb);
    TEST_ASSERT_EQUAL_INT(1, ng);

    game_event_frame_reset();
    (void)game_events_ctx_prev(&s_a, &na);
    (void)game_events_ctx_log(&s_b, &nb);
    TEST_ASSERT_EQUAL_INT(2, na);
    TEST_ASSERT_EQUAL_INT(1, nb);
}

static void test_prev_is_readable_after_one_swap_and_poisoned_after_the_second(void) {
    const int32_t *first = emit_i32(&s_a, "test.tick0", 1234);
    TEST_ASSERT_NOT_NULL(first);
    int n = 0;
    (void)game_events_ctx_prev(&s_a, &n);
    TEST_ASSERT_EQUAL_INT(0, n);

    game_events_ctx_swap(&s_a);
    const game_event_t *prev = game_events_ctx_prev(&s_a, &n);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_UINT32(0u, prev[0].tick);
    TEST_ASSERT_EQUAL_UINT64(T("test.tick0").value, prev[0].type.value);
    TEST_ASSERT_EQUAL_PTR(first, prev[0].payload);
    TEST_ASSERT_EQUAL_INT32(1234, *first);
    (void)game_events_ctx_log(&s_a, &n);
    TEST_ASSERT_EQUAL_INT(0, n);

    /* Emitting into the new cur leaves prev untouched. */
    (void)emit_i32(&s_a, "test.tick1", 5678);
    TEST_ASSERT_EQUAL_INT32(1234, *first);

    game_events_ctx_swap(&s_a);
    prev = game_events_ctx_prev(&s_a, &n);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_UINT32(1u, prev[0].tick);
    TEST_ASSERT_EQUAL_INT32(5678, *(const int32_t *)prev[0].payload);
    (void)game_events_ctx_log(&s_a, &n);
    TEST_ASSERT_EQUAL_INT(0, n);
#ifdef NT_DEBUG
    const uint8_t poison[sizeof(int32_t)] = {0xDD, 0xDD, 0xDD, 0xDD};
    TEST_ASSERT_EQUAL_MEMORY(poison, first, sizeof poison);
#endif
}

/* Death tests need the handler-calling assert mode; TRAP mode would kill the run. */
#if NT_ASSERT_MODE == NT_ASSERT_FULL
static void test_a_full_context_asserts_and_the_other_keeps_emitting(void) {
    for (uint32_t i = 0; i < LOG_CAP; ++i) {
        TEST_ASSERT_NOT_NULL(emit_i32(&s_a, "test.fill", (int32_t)i));
    }
    NT_TEST_EXPECT_ASSERT(emit_i32(&s_a, "test.over", 0));
    TEST_ASSERT_NOT_NULL(emit_i32(&s_b, "test.b", 1));
    game_events_ctx_swap(&s_a);
    TEST_ASSERT_NOT_NULL(emit_i32(&s_a, "test.after", 1));
}

static void test_init_refuses_short_or_misaligned_memory(void) {
    game_events_ctx_t ctx;
    NT_TEST_EXPECT_ASSERT(game_events_ctx_init(&ctx, s_mem_a.bytes, sizeof s_mem_a.bytes - 1u, ARENA_BYTES, LOG_CAP));
    NT_TEST_EXPECT_ASSERT(game_events_ctx_init(&ctx, s_mem_a.bytes + 1, sizeof s_mem_a.bytes - 1u, 8u, 1u));
}
#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_two_contexts_and_the_default_are_isolated);
    RUN_TEST(test_prev_is_readable_after_one_swap_and_poisoned_after_the_second);
#if NT_ASSERT_MODE == NT_ASSERT_FULL
    RUN_TEST(test_a_full_context_asserts_and_the_other_keeps_emitting);
    RUN_TEST(test_init_refuses_short_or_misaligned_memory);
#endif
    return UNITY_END();
}
