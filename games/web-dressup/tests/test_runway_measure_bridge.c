#include "features/dress_room/dress_room_events.h"
#include "features/dress_room/runway_measure_bridge.h"
#include "features/platform_sdk/platform_sdk.h"
#include "game_events.h"
#include "hash/nt_hash.h"
#include "unity.h"

#include <string.h>

typedef struct measure_trace {
    int count;
    char triples[24][3][33];
} measure_trace_t;

static measure_trace_t s_trace;

static bool backend_init(void *userdata) { (void)userdata; return true; }
static void backend_measure(const char *category, const char *what,
                            const char *action, void *userdata) {
    measure_trace_t *trace = userdata;
    TEST_ASSERT_TRUE(trace->count < 24);
    strcpy(trace->triples[trace->count][0], category);
    strcpy(trace->triples[trace->count][1], what);
    strcpy(trace->triples[trace->count][2], action);
    trace->count++;
}

static void assert_triple(int index, const char *category,
                          const char *what, const char *action) {
    TEST_ASSERT_EQUAL_STRING(category, s_trace.triples[index][0]);
    TEST_ASSERT_EQUAL_STRING(what, s_trace.triples[index][1]);
    TEST_ASSERT_EQUAL_STRING(action, s_trace.triples[index][2]);
}

void setUp(void) {
    memset(&s_trace, 0, sizeof s_trace);
    game_events_init();
    platform_sdk_reset_for_tests();
    platform_sdk_backend_t backend = {.init = backend_init, .measure = backend_measure};
    platform_sdk_set_backend(&backend, &s_trace);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    runway_measure_bridge_init();
}

void tearDown(void) {
    platform_sdk_reset_for_tests();
    game_events_shutdown();
}

static void test_discovery_forwards_exact_stable_triples(void) {
    dress_room_events_emit_awakening_start(0, 7, 0, 1, false, false);
    dress_room_events_emit_recipe_reveal(0, 7, 0, 1, DRESS_ROOM_REVEAL_DISCOVERY, 1, 1);
    dress_room_events_emit_collection_mastery(1, 1);
    dress_room_events_emit_lookbook_open(1, 1);

    runway_measure_bridge_record();

    TEST_ASSERT_EQUAL_INT(5, s_trace.count);
    assert_triple(0, "round", "1", "start");
    assert_triple(1, "round", "1", "complete");
    assert_triple(2, "recipe", "moon-moon", "discovered");
    assert_triple(3, "collection", "1", "complete");
    assert_triple(4, "lookbook", "main", "open");
}

static void test_remix_counts_round_but_replay_does_not(void) {
    dress_room_events_emit_awakening_start(3, 7, 1, 2, true, false);
    dress_room_events_emit_recipe_reveal(3, 7, 1, 2, DRESS_ROOM_REVEAL_REMIX, 1, 2);
    dress_room_events_emit_awakening_start(3, 7, 1, 2, true, true);
    dress_room_events_emit_recipe_reveal(3, 7, 1, 2, DRESS_ROOM_REVEAL_REPLAY, 1, 2);

    runway_measure_bridge_record();

    TEST_ASSERT_EQUAL_INT(2, s_trace.count);
    assert_triple(0, "round", "2", "start");
    assert_triple(1, "round", "2", "complete");
}

static void test_duplicate_discovery_and_mastery_are_once_guarded(void) {
    dress_room_events_emit_recipe_reveal(4, 7, 2, 3, DRESS_ROOM_REVEAL_DISCOVERY, 3, 3);
    dress_room_events_emit_recipe_reveal(4, 7, 2, 3, DRESS_ROOM_REVEAL_DISCOVERY, 3, 3);
    dress_room_events_emit_collection_mastery(3, 3);
    dress_room_events_emit_collection_mastery(3, 3);

    runway_measure_bridge_record();

    TEST_ASSERT_EQUAL_INT(3, s_trace.count);
    assert_triple(0, "round", "3", "complete");
    assert_triple(1, "recipe", "moon-flame", "discovered");
    assert_triple(2, "collection", "3", "complete");
}

int main(void) {
    nt_hash_init(&(nt_hash_desc_t){0});
    UNITY_BEGIN();
    RUN_TEST(test_discovery_forwards_exact_stable_triples);
    RUN_TEST(test_remix_counts_round_but_replay_does_not);
    RUN_TEST(test_duplicate_discovery_and_mastery_are_once_guarded);
    const int result = UNITY_END();
    nt_hash_shutdown();
    return result;
}
