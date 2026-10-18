/* The portal funnel is a contract with a dashboard nobody in the build can see:
   an event that stops arriving, or arrives under the wrong name, fails silently
   for five hundred players. The drain is pinned here on a mock backend: every
   frame is read once, the mapper sees every event, and nothing is measured
   before the SDK exists. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "features/platform_sdk/platform_sdk.h"
#include "game_events.h"
#include "hash/nt_hash.h"
#include "systems/sys_portal_metrics.h"
#include "unity.h"

#define CAP 32

static struct {
    char category[CAP][40];
    char what[CAP][40];
    char action[CAP][40];
    int count;
} g_seen;

static void mock_measure(const char *category, const char *what, const char *action, void *userdata) {
    (void)userdata;
    if (g_seen.count >= CAP) return;
    strncpy(g_seen.category[g_seen.count], category, 39);
    strncpy(g_seen.what[g_seen.count], what, 39);
    strncpy(g_seen.action[g_seen.count], action, 39);
    g_seen.count += 1;
}

static bool mock_init(void *userdata) {
    (void)userdata;
    return true;
}

static int find(const char *category, const char *what, const char *action) {
    int hits = 0;
    for (int i = 0; i < g_seen.count; ++i) {
        if (strcmp(g_seen.category[i], category) == 0 && strcmp(g_seen.what[i], what) == 0 &&
            strcmp(g_seen.action[i], action) == 0)
            ++hits;
    }
    return hits;
}

/* A stand-in for a game's own mapper: one event type, its payload is the level. */
typedef struct level_ev_t {
    int level;
} level_ev_t;

static nt_hash64_t s_level_start;
static int s_mapped;

/* The log also carries the SDK's own events, so only the test's type is counted. */
static void map(const game_event_t *ev, void *userdata) {
    (void)userdata;
    if (ev->payload == NULL || ev->type.value != s_level_start.value) return;
    s_mapped += 1;
    char what[16];
    (void)snprintf(what, sizeof what, "%d", ((const level_ev_t *)ev->payload)->level);
    sys_portal_metrics_measure("level", what, "start");
}

static void emit_level_start(int level) {
    const level_ev_t ev = {.level = level};
    (void)game_event_emit(s_level_start, &ev, (uint32_t)sizeof ev, _Alignof(level_ev_t));
}

void setUp(void) {
    memset(&g_seen, 0, sizeof g_seen);
    s_mapped = 0;
    s_level_start = nt_hash64_str("test.level_start");
    game_events_init();
    platform_sdk_reset_for_tests();
    platform_sdk_backend_t backend = {.init = mock_init, .measure = mock_measure};
    platform_sdk_set_backend(&backend, NULL);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    sys_portal_metrics_init(map, NULL);
}

void tearDown(void) {
    platform_sdk_reset_for_tests();
    game_events_shutdown();
}

static void test_every_event_reaches_the_mapper_once(void) {
    emit_level_start(1);
    emit_level_start(2);
    sys_portal_metrics_record();
    /* The same frame is not read twice. */
    sys_portal_metrics_record();
    TEST_ASSERT_EQUAL_INT(2, s_mapped);
    TEST_ASSERT_EQUAL_INT(1, find("level", "1", "start"));
    TEST_ASSERT_EQUAL_INT(1, find("level", "2", "start"));
    game_event_frame_reset();

    emit_level_start(3);
    sys_portal_metrics_record();
    TEST_ASSERT_EQUAL_INT(3, s_mapped);
    TEST_ASSERT_EQUAL_INT(1, find("level", "3", "start"));
    game_event_frame_reset();
}

static void test_direct_measures_need_no_mapper(void) {
    sys_portal_metrics_init(NULL, NULL);
    emit_level_start(1);
    sys_portal_metrics_record();
    TEST_ASSERT_EQUAL_INT(0, s_mapped);
    sys_portal_metrics_measure("rewarded", "double", "visible");
    TEST_ASSERT_EQUAL_INT(1, find("rewarded", "double", "visible"));
    game_event_frame_reset();
}

static void test_events_before_the_sdk_is_ready_do_not_crash(void) {
    platform_sdk_reset_for_tests();
    emit_level_start(2);
    sys_portal_metrics_record();
    TEST_ASSERT_EQUAL_INT(0, g_seen.count);
    game_event_frame_reset();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_every_event_reaches_the_mapper_once);
    RUN_TEST(test_direct_measures_need_no_mapper);
    RUN_TEST(test_events_before_the_sdk_is_ready_do_not_crash);
    return UNITY_END();
}
