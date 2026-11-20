#include "steam_platform_common.h"
#include "steam_platform_internal.h"
#include "unity.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const steam_platform_config_t DEV_CONFIG = {.app_id = 480, .restart_through_steam = false};

/* A start that fails without touching Steam in either build: the stub refuses by itself, the
   real module is handed a simulated "no client". */
static steam_platform_status_t failing_start(const steam_platform_config_t *config, char *message, size_t size) {
#ifdef STEAM_PLATFORM_HAS_SDK
    steam_platform_internal_simulate_failure(false, STEAM_PLATFORM_INTERNAL_INIT_NO_STEAM_CLIENT,
                                             "simulated: Steam is not running");
#endif
    return steam_platform_start(config, message, size);
}

static steam_platform_status_t expected_failure(void) {
#ifdef STEAM_PLATFORM_HAS_SDK
    return STEAM_PLATFORM_NO_CLIENT;
#else
    return STEAM_PLATFORM_NO_SDK;
#endif
}

static void assert_inert(void) {
    steam_platform_internal_counters_t before;
    steam_platform_internal_counters(&before);

    TEST_ASSERT_FALSE(steam_platform_ready());
    TEST_ASSERT_EQUAL_UINT64(0, steam_platform_user());
    TEST_ASSERT_FALSE(steam_platform_overlay_ready());
    steam_platform_run_frame();
    steam_platform_open_overlay(NULL);
    steam_platform_open_overlay("friends");
    steam_platform_open_overlay("");
    steam_platform_event_t event = {0};
    TEST_ASSERT_FALSE(steam_platform_next_event(&event));
    TEST_ASSERT_FALSE(steam_platform_next_event(NULL));
    steam_platform_stop();
    steam_platform_stop();
    TEST_ASSERT_FALSE(steam_platform_ready());

    TEST_ASSERT_FALSE(steam_platform_internal_listen(1281, NULL, NULL));
    TEST_ASSERT_FALSE(steam_platform_internal_await_call(1, 703, NULL, NULL));
    TEST_ASSERT_FALSE(steam_platform_internal_cancel_call(1));
    steam_platform_internal_unlisten(1281, NULL, NULL);
    steam_platform_internal_counters(NULL);

    steam_platform_internal_counters_t after;
    steam_platform_internal_counters(&after);
    TEST_ASSERT_EQUAL_UINT64(before.callbacks, after.callbacks);
    TEST_ASSERT_EQUAL_UINT64(before.unhandled, after.unhandled);
}

static void test_every_call_is_inert_before_start(void) { assert_inert(); }

static void test_null_config_is_failed_with_a_reason(void) {
    char message[64] = "untouched";
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_FAILED, steam_platform_start(NULL, message, sizeof message));
    TEST_ASSERT_TRUE(strlen(message) > 0);
    TEST_ASSERT_NOT_EQUAL(0, strcmp(message, "untouched"));
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_FAILED, steam_platform_start(NULL, NULL, 0));
    assert_inert();
}

static void test_restart_without_app_id_is_failed_in_every_build(void) {
    char message[128] = "";
    const steam_platform_config_t no_app = {.app_id = 0, .restart_through_steam = true};
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_FAILED, steam_platform_start(&no_app, message, sizeof message));
    TEST_ASSERT_NOT_NULL(strstr(message, "app_id"));
    assert_inert();
}

static void test_config_check(void) {
    char message[64];
    const steam_platform_config_t release = {.app_id = 480, .restart_through_steam = true};
    const steam_platform_config_t no_app = {.app_id = 0, .restart_through_steam = true};
    const steam_platform_config_t any_app = {.app_id = 0, .restart_through_steam = false};
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_FAILED, steam_platform_check_config(NULL, message, sizeof message));
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_FAILED, steam_platform_check_config(&no_app, NULL, 0));
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_READY, steam_platform_check_config(&release, message, sizeof message));
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_READY, steam_platform_check_config(&any_app, message, sizeof message));
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_READY, steam_platform_check_config(&DEV_CONFIG, message, sizeof message));
}

static void test_failed_start_leaves_the_module_stopped_and_inert(void) {
    char message[128];
    TEST_ASSERT_EQUAL_INT(expected_failure(), failing_start(&DEV_CONFIG, message, sizeof message));
    TEST_ASSERT_TRUE(strlen(message) > 0);
    assert_inert();
    /* A second failed start behaves the same: nothing stays half started. */
    TEST_ASSERT_EQUAL_INT(expected_failure(), failing_start(&DEV_CONFIG, message, sizeof message));
    assert_inert();
}

static void test_message_is_cut_to_the_buffer_and_terminated(void) {
    char buffer[16];
    memset(buffer, 'x', sizeof buffer);
    TEST_ASSERT_EQUAL_INT(expected_failure(), failing_start(&DEV_CONFIG, buffer, 4));
    TEST_ASSERT_EQUAL_CHAR('\0', buffer[3]);
    TEST_ASSERT_EQUAL_size_t(3, strlen(buffer));
    for (size_t i = 4; i < sizeof buffer; i++) {
        TEST_ASSERT_EQUAL_CHAR('x', buffer[i]);
    }

    memset(buffer, 'x', sizeof buffer);
    TEST_ASSERT_EQUAL_INT(expected_failure(), failing_start(&DEV_CONFIG, buffer, 1));
    TEST_ASSERT_EQUAL_CHAR('\0', buffer[0]);
    TEST_ASSERT_EQUAL_CHAR('x', buffer[1]);

    memset(buffer, 'x', sizeof buffer);
    TEST_ASSERT_EQUAL_INT(expected_failure(), failing_start(&DEV_CONFIG, buffer, 0));
    TEST_ASSERT_EQUAL_CHAR('x', buffer[0]);

    TEST_ASSERT_EQUAL_INT(expected_failure(), failing_start(&DEV_CONFIG, NULL, 64));
    assert_inert();
}

static void test_overlay_dialog_defaults_to_friends_only_for_null(void) {
    TEST_ASSERT_EQUAL_STRING("friends", steam_platform_overlay_dialog(NULL));
    const char *named = "community";
    TEST_ASSERT_EQUAL_PTR(named, steam_platform_overlay_dialog(named));
    const char *unknown = "no-such-dialog";
    TEST_ASSERT_EQUAL_PTR(unknown, steam_platform_overlay_dialog(unknown));
    TEST_ASSERT_EQUAL_STRING("", steam_platform_overlay_dialog(""));
}

static void test_queue_keeps_overlay_changes_only(void) {
    steam_platform_event_queue_t queue = {0};
    steam_platform_queue_overlay(&queue, false); /* the overlay starts closed: no change */
    steam_platform_queue_overlay(&queue, true);
    steam_platform_queue_overlay(&queue, true);
    steam_platform_queue_overlay(&queue, false);
    steam_platform_queue_overlay(&queue, false);
    steam_platform_queue_overlay(&queue, true);
    const bool expected[] = {true, false, true};
    steam_platform_event_t event;
    for (size_t i = 0; i < 3; i++) {
        TEST_ASSERT_TRUE(steam_platform_queue_pop(&queue, &event));
        TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_EVENT_OVERLAY, event.type);
        TEST_ASSERT_EQUAL(expected[i], event.active);
    }
    TEST_ASSERT_FALSE(steam_platform_queue_pop(&queue, &event));
}

static void test_queue_drops_the_oldest_when_full(void) {
    steam_platform_event_queue_t queue = {0};
    const unsigned pushed = STEAM_PLATFORM_EVENT_CAPACITY + 5U;
    for (unsigned i = 0; i < pushed; i++) {
        steam_platform_queue_overlay(&queue, i % 2U == 0U); /* every call is a change */
    }
    TEST_ASSERT_EQUAL_UINT64(5, queue.dropped);
    steam_platform_event_t event;
    for (unsigned i = 5; i < pushed; i++) {
        TEST_ASSERT_TRUE(steam_platform_queue_pop(&queue, &event));
        TEST_ASSERT_EQUAL(i % 2U == 0U, event.active);
    }
    TEST_ASSERT_FALSE(steam_platform_queue_pop(&queue, &event));
}

static void test_queue_null_out_takes_nothing_and_reset_drops(void) {
    steam_platform_event_queue_t queue = {0};
    steam_platform_queue_overlay(&queue, true);
    TEST_ASSERT_FALSE(steam_platform_queue_pop(&queue, NULL));
    steam_platform_event_t event;
    TEST_ASSERT_TRUE(steam_platform_queue_pop(&queue, &event));
    TEST_ASSERT_TRUE(event.active);

    steam_platform_queue_overlay(&queue, false);
    steam_platform_queue_overlay(&queue, true);
    steam_platform_queue_reset(&queue);
    TEST_ASSERT_FALSE(steam_platform_queue_pop(&queue, &event));
    /* Reset also forgets the state: a shown overlay counts as closed again, so "closed" is no
       change and "shown" is one. */
    steam_platform_queue_overlay(&queue, false);
    TEST_ASSERT_FALSE(steam_platform_queue_pop(&queue, &event));
    steam_platform_queue_overlay(&queue, true);
    TEST_ASSERT_TRUE(steam_platform_queue_pop(&queue, &event));
}

#ifdef STEAM_PLATFORM_HAS_SDK

static void test_init_results_map_to_statuses(void) {
    char message[128];
    steam_platform_internal_simulate_failure(false, STEAM_PLATFORM_INTERNAL_INIT_FAILED_GENERIC, "generic");
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_FAILED, steam_platform_start(&DEV_CONFIG, message, sizeof message));
    TEST_ASSERT_EQUAL_STRING("generic", message);
    assert_inert();

    steam_platform_internal_simulate_failure(false, STEAM_PLATFORM_INTERNAL_INIT_VERSION_MISMATCH, "old client");
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_FAILED, steam_platform_start(&DEV_CONFIG, message, sizeof message));
    TEST_ASSERT_EQUAL_STRING("old client", message);
    assert_inert();

    /* Steam's message may be empty; the caller still gets a reason. */
    steam_platform_internal_simulate_failure(false, STEAM_PLATFORM_INTERNAL_INIT_NO_STEAM_CLIENT, "");
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_NO_CLIENT, steam_platform_start(&DEV_CONFIG, message, sizeof message));
    TEST_ASSERT_TRUE(strlen(message) > 0);
    assert_inert();

    /* The hook cannot fake a working Steam. */
    steam_platform_internal_simulate_failure(false, 0, "");
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_FAILED, steam_platform_start(&DEV_CONFIG, message, sizeof message));
    assert_inert();
}

static void test_restart_through_steam(void) {
    char message[128];
    const steam_platform_config_t release = {.app_id = 480, .restart_through_steam = true};
    steam_platform_internal_simulate_failure(true, STEAM_PLATFORM_INTERNAL_INIT_NO_STEAM_CLIENT, "");
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_RESTARTING, steam_platform_start(&release, message, sizeof message));
    TEST_ASSERT_TRUE(strlen(message) > 0);
    assert_inert();

    /* Restart not needed: init runs and its own answer counts. */
    steam_platform_internal_simulate_failure(false, STEAM_PLATFORM_INTERNAL_INIT_NO_STEAM_CLIENT, "");
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_NO_CLIENT, steam_platform_start(&release, message, sizeof message));
    assert_inert();
}

#else

static void test_build_without_sdk_answers_no_sdk(void) {
    char message[128];
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_NO_SDK, steam_platform_start(&DEV_CONFIG, message, sizeof message));
    TEST_ASSERT_TRUE(strlen(message) > 0);
    const steam_platform_config_t release = {.app_id = 480, .restart_through_steam = true};
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_NO_SDK, steam_platform_start(&release, NULL, 0));
    TEST_ASSERT_FALSE(steam_platform_internal_feed(331, "x", 1));
    assert_inert();
}

#endif

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_every_call_is_inert_before_start);
    RUN_TEST(test_null_config_is_failed_with_a_reason);
    RUN_TEST(test_restart_without_app_id_is_failed_in_every_build);
    RUN_TEST(test_config_check);
    RUN_TEST(test_failed_start_leaves_the_module_stopped_and_inert);
    RUN_TEST(test_message_is_cut_to_the_buffer_and_terminated);
    RUN_TEST(test_overlay_dialog_defaults_to_friends_only_for_null);
    RUN_TEST(test_queue_keeps_overlay_changes_only);
    RUN_TEST(test_queue_drops_the_oldest_when_full);
    RUN_TEST(test_queue_null_out_takes_nothing_and_reset_drops);
#ifdef STEAM_PLATFORM_HAS_SDK
    RUN_TEST(test_init_results_map_to_statuses);
    RUN_TEST(test_restart_through_steam);
#else
    RUN_TEST(test_build_without_sdk_answers_no_sdk);
#endif
    return UNITY_END();
}
