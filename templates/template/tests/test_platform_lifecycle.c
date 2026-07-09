#include "platform_lifecycle.h"

#include "features/platform_sdk/platform_sdk.h"
#include "unity.h"

#include <string.h>

typedef struct lifecycle_backend_state_t {
    int init_calls;
    int loading_finished_calls;
    int game_ready_calls;
    int gameplay_start_calls;
    int gameplay_stop_calls;
} lifecycle_backend_state_t;

static lifecycle_backend_state_t g_backend_state;

static bool backend_init(void *userdata) {
    lifecycle_backend_state_t *state = (lifecycle_backend_state_t *)userdata;
    state->init_calls++;
    return true;
}

static void backend_loading_finished(void *userdata) {
    lifecycle_backend_state_t *state = (lifecycle_backend_state_t *)userdata;
    state->loading_finished_calls++;
}

static void backend_game_ready(void *userdata) {
    lifecycle_backend_state_t *state = (lifecycle_backend_state_t *)userdata;
    state->game_ready_calls++;
}

static void backend_gameplay_start(void *userdata) {
    lifecycle_backend_state_t *state = (lifecycle_backend_state_t *)userdata;
    state->gameplay_start_calls++;
}

static void backend_gameplay_stop(void *userdata) {
    lifecycle_backend_state_t *state = (lifecycle_backend_state_t *)userdata;
    state->gameplay_stop_calls++;
}

static platform_sdk_backend_t backend(void) {
    return (platform_sdk_backend_t){
        .init = backend_init,
        .game_loading_finished = backend_loading_finished,
        .game_ready = backend_game_ready,
        .gameplay_start = backend_gameplay_start,
        .gameplay_stop = backend_gameplay_stop,
    };
}

void setUp(void) {
    memset(&g_backend_state, 0, sizeof(g_backend_state));
    platform_sdk_reset_for_tests();
    platform_sdk_backend_t sdk_backend = backend();
    platform_sdk_set_backend(&sdk_backend, &g_backend_state);
}

void tearDown(void) {
    platform_lifecycle_shutdown();
    platform_sdk_reset_for_tests();
}

static void test_lifecycle_init_and_playable_ready_are_one_shot(void) {
    platform_lifecycle_init();
    platform_lifecycle_init();
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.init_calls);

    platform_lifecycle_update(false, true);
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.loading_finished_calls);
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.game_ready_calls);

    platform_lifecycle_update(true, true);
    platform_lifecycle_update(true, true);
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.loading_finished_calls);
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.game_ready_calls);
}

static void test_menu_input_does_not_start_gameplay(void) {
    platform_lifecycle_init();
    platform_sdk_mark_input();

    platform_lifecycle_update(true, true);
    TEST_ASSERT_TRUE(platform_sdk_has_input());
    TEST_ASSERT_FALSE(platform_sdk_has_gameplay_started());
    TEST_ASSERT_FALSE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.gameplay_start_calls);
}

static void test_gameplay_intent_starts_and_menu_stops_gameplay(void) {
    platform_lifecycle_init();
    platform_lifecycle_mark_gameplay_input();

    platform_lifecycle_update(true, true);
    TEST_ASSERT_TRUE(platform_sdk_has_input());
    TEST_ASSERT_TRUE(platform_sdk_has_gameplay_started());
    TEST_ASSERT_TRUE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.gameplay_start_calls);

    platform_lifecycle_update(true, false);
    platform_lifecycle_update(true, false);
    TEST_ASSERT_FALSE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.gameplay_stop_calls);

    platform_lifecycle_update(true, true);
    TEST_ASSERT_TRUE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(2, g_backend_state.gameplay_start_calls);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lifecycle_init_and_playable_ready_are_one_shot);
    RUN_TEST(test_menu_input_does_not_start_gameplay);
    RUN_TEST(test_gameplay_intent_starts_and_menu_stops_gameplay);
    return UNITY_END();
}
