#include "platform_lifecycle.h"

#include "features/platform_sdk/platform_sdk.h"
#include "input/nt_input_internal.h"
#include "unity.h"

#include <string.h>

typedef struct lifecycle_backend_state_t {
    int init_calls;
    int loading_progress_calls;
    int loading_finished_calls;
    int game_ready_calls;
    int gameplay_start_calls;
    int gameplay_stop_calls;
    float last_loading_progress;
} lifecycle_backend_state_t;

static lifecycle_backend_state_t g_backend_state;

static bool backend_init(void *userdata) {
    lifecycle_backend_state_t *state = (lifecycle_backend_state_t *)userdata;
    state->init_calls++;
    return true;
}

static void backend_loading_progress(float progress01, void *userdata) {
    lifecycle_backend_state_t *state = (lifecycle_backend_state_t *)userdata;
    state->loading_progress_calls++;
    state->last_loading_progress = progress01;
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
        .game_loading_progress = backend_loading_progress,
        .game_loading_finished = backend_loading_finished,
        .game_ready = backend_game_ready,
        .gameplay_start = backend_gameplay_start,
        .gameplay_stop = backend_gameplay_stop,
    };
}

static bool float_close(float actual, float expected) {
    float diff = actual - expected;
    if (diff < 0.0f) {
        diff = -diff;
    }
    return diff < 0.0001f;
}

void setUp(void) {
    memset(&g_backend_state, 0, sizeof(g_backend_state));
    nt_input_init();
    platform_sdk_reset_for_tests();
    platform_sdk_backend_t sdk_backend = backend();
    platform_sdk_set_backend(&sdk_backend, &g_backend_state);
}

void tearDown(void) {
    platform_lifecycle_shutdown();
    platform_sdk_reset_for_tests();
    nt_input_shutdown();
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
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.loading_finished_calls);
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.game_ready_calls);

    platform_lifecycle_after_frame_present(true);
    platform_lifecycle_after_frame_present(true);
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.loading_finished_calls);
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.game_ready_calls);
    TEST_ASSERT_EQUAL_INT(2, g_backend_state.loading_progress_calls);
    TEST_ASSERT_TRUE(float_close(g_backend_state.last_loading_progress, 1.0f));
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

static void test_any_keyboard_input_starts_gameplay(void) {
    platform_lifecycle_init();

    nt_input_set_key(NT_KEY_SPACE, true);
    TEST_ASSERT_TRUE(platform_lifecycle_after_input_poll());
    platform_lifecycle_update(true, true);

    TEST_ASSERT_TRUE(platform_sdk_has_input());
    TEST_ASSERT_TRUE(platform_sdk_has_gameplay_started());
    TEST_ASSERT_TRUE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.gameplay_start_calls);
}

static void test_input_poll_reports_no_gesture_without_an_edge(void) {
    TEST_ASSERT_FALSE(platform_lifecycle_after_input_poll());
    platform_lifecycle_init();
    TEST_ASSERT_FALSE(platform_lifecycle_after_input_poll());
}

static void test_touch_input_starts_gameplay(void) {
    platform_lifecycle_init();

    nt_input_pointer_down(7u, 320.0f, 240.0f, 1.0f, NT_POINTER_TOUCH, 1u);
    TEST_ASSERT_TRUE(platform_lifecycle_after_input_poll());
    platform_lifecycle_update(true, true);

    TEST_ASSERT_TRUE(platform_sdk_has_input());
    TEST_ASSERT_TRUE(platform_sdk_has_gameplay_started());
    TEST_ASSERT_TRUE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.gameplay_start_calls);
}

static void test_input_does_not_start_gameplay_while_gameplay_is_disallowed(void) {
    platform_lifecycle_init();

    nt_input_set_key(NT_KEY_SPACE, true);
    TEST_ASSERT_TRUE(platform_lifecycle_after_input_poll());
    platform_lifecycle_update(true, false);

    TEST_ASSERT_TRUE(platform_sdk_has_input());
    TEST_ASSERT_FALSE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.gameplay_start_calls);

    platform_lifecycle_update(true, true);
    TEST_ASSERT_TRUE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.gameplay_start_calls);
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

static void test_loading_progress_mapping_handles_unknown_total(void) {
    TEST_ASSERT_TRUE(float_close(platform_lifecycle_loading_progress_from_pack(0u, 0u, false), 0.45f));
    TEST_ASSERT_TRUE(float_close(platform_lifecycle_loading_progress_from_pack(25u, 100u, false), 0.5875f));
    TEST_ASSERT_TRUE(float_close(platform_lifecycle_loading_progress_from_pack(125u, 100u, false), 1.0f));
    TEST_ASSERT_TRUE(float_close(platform_lifecycle_loading_progress_from_pack(0u, 0u, true), 1.0f));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lifecycle_init_and_playable_ready_are_one_shot);
    RUN_TEST(test_menu_input_does_not_start_gameplay);
    RUN_TEST(test_any_keyboard_input_starts_gameplay);
    RUN_TEST(test_input_poll_reports_no_gesture_without_an_edge);
    RUN_TEST(test_touch_input_starts_gameplay);
    RUN_TEST(test_input_does_not_start_gameplay_while_gameplay_is_disallowed);
    RUN_TEST(test_gameplay_intent_starts_and_menu_stops_gameplay);
    RUN_TEST(test_loading_progress_mapping_handles_unknown_total);
    return UNITY_END();
}
