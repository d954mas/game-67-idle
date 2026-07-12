#include <string.h>

#include "features/platform_sdk/platform_sdk.h"
#include "features/platform_sdk/platform_sdk_measure.h"
#include "unity.h"

typedef struct fake_backend_state_t {
    int init_calls;
    int loading_progress_calls;
    int loading_finished_calls;
    int game_ready_calls;
    int gameplay_start_calls;
    int gameplay_stop_calls;
    int interstitial_calls;
    int rewarded_calls;
    int measure_calls;
    char measure_category[33];
    char measure_what[33];
    char measure_action[33];
    float last_loading_progress;
    platform_sdk_ad_result_t next_interstitial;
    platform_sdk_rewarded_result_t next_rewarded;
} fake_backend_state_t;

static fake_backend_state_t g_backend_state;

static bool fake_backend_init(void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    state->init_calls++;
    return true;
}

static bool pending_backend_init(void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    state->init_calls++;
    return false;
}

static void fake_backend_loading_progress(float progress01, void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    state->loading_progress_calls++;
    state->last_loading_progress = progress01;
}

static void fake_backend_loading_finished(void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    state->loading_finished_calls++;
}

static void fake_backend_game_ready(void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    state->game_ready_calls++;
}

static void fake_backend_gameplay_start(void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    state->gameplay_start_calls++;
}

static void fake_backend_gameplay_stop(void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    state->gameplay_stop_calls++;
}

static void fake_backend_measure(const char *category, const char *what,
                                 const char *action, void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    state->measure_calls++;
    strcpy(state->measure_category, category);
    strcpy(state->measure_what, what);
    strcpy(state->measure_action, action);
}

static platform_sdk_result_t fake_backend_show_interstitial(const char *placement, void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    (void)placement;
    state->interstitial_calls++;
    platform_sdk_backend_complete_interstitial(state->next_interstitial);
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_result_t fake_backend_show_rewarded(const char *placement, void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    (void)placement;
    state->rewarded_calls++;
    platform_sdk_backend_complete_rewarded(state->next_rewarded);
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_backend_t fake_backend(void) {
    platform_sdk_backend_t backend = {
        .init = fake_backend_init,
        .game_loading_progress = fake_backend_loading_progress,
        .game_loading_finished = fake_backend_loading_finished,
        .game_ready = fake_backend_game_ready,
        .gameplay_start = fake_backend_gameplay_start,
        .gameplay_stop = fake_backend_gameplay_stop,
        .measure = fake_backend_measure,
        .show_interstitial = fake_backend_show_interstitial,
        .show_rewarded = fake_backend_show_rewarded,
    };
    return backend;
}

static platform_sdk_result_t pending_backend_show_interstitial(const char *placement, void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    (void)placement;
    state->interstitial_calls++;
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_result_t pending_backend_show_rewarded(const char *placement, void *userdata) {
    fake_backend_state_t *state = (fake_backend_state_t *)userdata;
    (void)placement;
    state->rewarded_calls++;
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_backend_t pending_backend(void) {
    platform_sdk_backend_t backend = {
        .init = fake_backend_init,
        .show_interstitial = pending_backend_show_interstitial,
        .show_rewarded = pending_backend_show_rewarded,
    };
    return backend;
}

static platform_sdk_backend_t async_init_backend(void) {
    platform_sdk_backend_t backend = fake_backend();
    backend.init = pending_backend_init;
    return backend;
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
    g_backend_state.next_interstitial = (platform_sdk_ad_result_t){
        .supported = true,
        .shown = true,
        .reason = PLATFORM_SDK_AD_REASON_COMPLETED,
    };
    g_backend_state.next_rewarded = (platform_sdk_rewarded_result_t){
        .supported = true,
        .shown = true,
        .rewarded = true,
        .reason = PLATFORM_SDK_AD_REASON_COMPLETED,
    };
    platform_sdk_reset_for_tests();
}

void tearDown(void) { platform_sdk_reset_for_tests(); }

#ifndef PLATFORM_SDK_TARGET_ID
#define PLATFORM_SDK_TARGET_ID PLATFORM_TARGET_LOCAL
#endif
#ifndef PLATFORM_SDK_CURRENT_ID
#define PLATFORM_SDK_CURRENT_ID PLATFORM_SDK_MOCK
#endif
#ifndef PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED
#define PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED 1
#endif
#ifndef PLATFORM_SDK_ADS_SUPPORTED
#define PLATFORM_SDK_ADS_SUPPORTED 1
#endif
#ifndef PLATFORM_SDK_REWARDED_SUPPORTED
#define PLATFORM_SDK_REWARDED_SUPPORTED 1
#endif
#ifndef PLATFORM_SDK_STORAGE_SUPPORTED
#define PLATFORM_SDK_STORAGE_SUPPORTED 1
#endif

static const char *expected_target_name(void) {
    switch ((platform_target_t)PLATFORM_SDK_TARGET_ID) {
        case PLATFORM_TARGET_LOCAL:
            return "local";
        case PLATFORM_TARGET_ITCH:
            return "itch";
        case PLATFORM_TARGET_POKI:
            return "poki";
        case PLATFORM_TARGET_YANDEX:
            return "yandex";
        case PLATFORM_TARGET_PLAYGAMA:
            return "playgama";
    }
    return "unknown";
}

static const char *expected_sdk_name(void) {
    switch ((platform_sdk_t)PLATFORM_SDK_CURRENT_ID) {
        case PLATFORM_SDK_MOCK:
            return "mock";
        case PLATFORM_SDK_POKI:
            return "poki";
        case PLATFORM_SDK_YANDEX:
            return "yandex";
        case PLATFORM_SDK_PLAYGAMA:
            return "playgama";
    }
    return "unknown";
}

static void test_template_target_identity(void) {
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_TARGET_ID, platform_sdk_target());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_CURRENT_ID, platform_sdk_current());
    TEST_ASSERT_EQUAL_STRING(expected_target_name(), platform_sdk_target_name());
    TEST_ASSERT_EQUAL_STRING(expected_sdk_name(), platform_sdk_current_name());
}

static void test_template_capabilities(void) {
    platform_sdk_capabilities_t caps = platform_sdk_capabilities();
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED != 0, caps.external_links_allowed);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_ADS_SUPPORTED != 0, caps.ads_supported);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_REWARDED_SUPPORTED != 0, caps.rewarded_supported);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_STORAGE_SUPPORTED != 0, caps.storage_supported);
}

static void test_template_boot_and_loading_calls_are_one_shot(void) {
    platform_sdk_backend_t backend = fake_backend();
    platform_sdk_set_backend(&backend, &g_backend_state);

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_BOOT_NOT_STARTED, platform_sdk_status());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_BOOT_READY, platform_sdk_status());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.init_calls);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.init_calls);

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_loading_finished());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_loading_finished());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.loading_finished_calls);

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_ready());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_ready());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.game_ready_calls);
}

static void test_template_async_init_waits_for_backend_completion(void) {
    platform_sdk_backend_t backend = async_init_backend();
    platform_sdk_set_backend(&backend, &g_backend_state);

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_NOT_READY, platform_sdk_init());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_BOOT_INITIALIZING, platform_sdk_status());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.init_calls);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_NOT_READY, platform_sdk_init());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.init_calls);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_NOT_READY, platform_sdk_game_loading_finished());
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.loading_finished_calls);

    platform_sdk_backend_complete_init(true);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_BOOT_READY, platform_sdk_status());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_loading_finished());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.loading_finished_calls);
}

static void test_template_async_init_failure_is_terminal(void) {
    platform_sdk_backend_t backend = async_init_backend();
    platform_sdk_set_backend(&backend, &g_backend_state);

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_NOT_READY, platform_sdk_init());
    platform_sdk_backend_complete_init(false);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_BOOT_FAILED, platform_sdk_status());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_FAILED, platform_sdk_init());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.init_calls);
}

static void test_template_loading_progress_is_clamped_and_monotonic(void) {
    platform_sdk_backend_t backend = fake_backend();
    platform_sdk_set_backend(&backend, &g_backend_state);

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_loading_progress(-1.0f));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.loading_progress_calls);
    TEST_ASSERT_TRUE(float_close(g_backend_state.last_loading_progress, 0.0f));

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_loading_progress(0.25f));
    TEST_ASSERT_EQUAL_INT(2, g_backend_state.loading_progress_calls);
    TEST_ASSERT_TRUE(float_close(g_backend_state.last_loading_progress, 0.25f));

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_loading_progress(0.20f));
    TEST_ASSERT_EQUAL_INT(2, g_backend_state.loading_progress_calls);
    TEST_ASSERT_TRUE(float_close(g_backend_state.last_loading_progress, 0.25f));

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_loading_progress(2.0f));
    TEST_ASSERT_EQUAL_INT(3, g_backend_state.loading_progress_calls);
    TEST_ASSERT_TRUE(float_close(g_backend_state.last_loading_progress, 1.0f));
}

static void test_template_gameplay_requires_first_input_and_tracks_state(void) {
    platform_sdk_backend_t backend = fake_backend();
    platform_sdk_gameplay_start_result_t start_result;
    platform_sdk_gameplay_stop_result_t stop_result;
    platform_sdk_set_backend(&backend, &g_backend_state);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());

    TEST_ASSERT_FALSE(platform_sdk_has_input());
    TEST_ASSERT_FALSE(platform_sdk_has_gameplay_started());
    TEST_ASSERT_FALSE(platform_sdk_gameplay_active());

    start_result = platform_sdk_gameplay_start();
    TEST_ASSERT_FALSE(start_result.started);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_WAITING_FOR_INPUT, start_result.reason);
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.gameplay_start_calls);

    platform_sdk_mark_input();
    TEST_ASSERT_TRUE(platform_sdk_has_input());
    start_result = platform_sdk_gameplay_start();
    TEST_ASSERT_TRUE(start_result.started);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, start_result.reason);
    TEST_ASSERT_TRUE(platform_sdk_has_gameplay_started());
    TEST_ASSERT_TRUE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.gameplay_start_calls);

    start_result = platform_sdk_gameplay_start();
    TEST_ASSERT_FALSE(start_result.started);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_ALREADY_ACTIVE, start_result.reason);
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.gameplay_start_calls);

    stop_result = platform_sdk_gameplay_stop();
    TEST_ASSERT_TRUE(stop_result.stopped);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, stop_result.reason);
    TEST_ASSERT_FALSE(platform_sdk_gameplay_active());
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.gameplay_stop_calls);

    stop_result = platform_sdk_gameplay_stop();
    TEST_ASSERT_FALSE(stop_result.stopped);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_NOT_ACTIVE, stop_result.reason);
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.gameplay_stop_calls);
}

static void test_measure_is_internal_bounded_and_forwards_exact_stable_triple(void) {
    platform_sdk_backend_t backend = fake_backend();
    platform_sdk_set_backend(&backend, &g_backend_state);

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_NOT_READY,
                          platform_sdk_measure("recipe", "moon-bloom", "discovered"));
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK,
                          platform_sdk_measure("recipe", "moon-bloom", "discovered"));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.measure_calls);
    TEST_ASSERT_EQUAL_STRING("recipe", g_backend_state.measure_category);
    TEST_ASSERT_EQUAL_STRING("moon-bloom", g_backend_state.measure_what);
    TEST_ASSERT_EQUAL_STRING("discovered", g_backend_state.measure_action);

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_FAILED,
                          platform_sdk_measure("recipe", "Moon Bloom", "discovered"));
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_FAILED,
                          platform_sdk_measure("", "main", "open"));
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_FAILED,
                          platform_sdk_measure("lookbook", "main", "open/again"));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.measure_calls);
}

typedef struct ad_callback_state_t {
    int pauses;
    int resumes;
    int interstitial_callbacks;
    int rewarded_callbacks;
    bool rewarded_shown;
    bool rewarded;
    platform_sdk_ad_reason_t rewarded_reason;
    void *seen_userdata;
} ad_callback_state_t;

static void on_pause(void *userdata) {
    ad_callback_state_t *state = (ad_callback_state_t *)userdata;
    state->pauses++;
}

static void on_resume(void *userdata) {
    ad_callback_state_t *state = (ad_callback_state_t *)userdata;
    state->resumes++;
}

static void on_interstitial_done(platform_sdk_ad_result_t result, void *userdata) {
    ad_callback_state_t *state = (ad_callback_state_t *)userdata;
    TEST_ASSERT_TRUE(result.supported);
    TEST_ASSERT_TRUE(result.shown);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_AD_REASON_COMPLETED, result.reason);
    state->interstitial_callbacks++;
    state->seen_userdata = userdata;
}

static void on_rewarded_done(platform_sdk_rewarded_result_t result, void *userdata) {
    ad_callback_state_t *state = (ad_callback_state_t *)userdata;
    TEST_ASSERT_TRUE(result.supported);
    state->rewarded_shown = result.shown;
    state->rewarded = result.rewarded;
    state->rewarded_reason = result.reason;
    state->rewarded_callbacks++;
    state->seen_userdata = userdata;
}

static void test_template_ad_flow_pauses_resumes_once_and_preserves_userdata(void) {
    platform_sdk_backend_t backend = fake_backend();
    ad_callback_state_t callback_state = {0};
    platform_sdk_set_backend(&backend, &g_backend_state);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    TEST_ASSERT_NOT_EQUAL(0u, platform_sdk_on_pause(on_pause, &callback_state));
    TEST_ASSERT_NOT_EQUAL(0u, platform_sdk_on_resume(on_resume, &callback_state));

    TEST_ASSERT_EQUAL_INT(
        PLATFORM_SDK_RESULT_OK,
        platform_sdk_show_interstitial("level_break", on_interstitial_done, &callback_state));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.interstitial_calls);
    TEST_ASSERT_EQUAL_INT(1, callback_state.pauses);
    TEST_ASSERT_EQUAL_INT(1, callback_state.resumes);
    TEST_ASSERT_EQUAL_INT(1, callback_state.interstitial_callbacks);
    TEST_ASSERT_EQUAL_PTR(&callback_state, callback_state.seen_userdata);

    TEST_ASSERT_EQUAL_INT(
        PLATFORM_SDK_RESULT_OK,
        platform_sdk_show_rewarded("double_reward", on_rewarded_done, &callback_state));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.rewarded_calls);
    TEST_ASSERT_EQUAL_INT(2, callback_state.pauses);
    TEST_ASSERT_EQUAL_INT(2, callback_state.resumes);
    TEST_ASSERT_EQUAL_INT(1, callback_state.rewarded_callbacks);
    TEST_ASSERT_TRUE(callback_state.rewarded_shown);
    TEST_ASSERT_TRUE(callback_state.rewarded);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_AD_REASON_COMPLETED, callback_state.rewarded_reason);
    TEST_ASSERT_EQUAL_PTR(&callback_state, callback_state.seen_userdata);
}

static void test_template_rewarded_decline_does_not_grant_reward(void) {
    platform_sdk_backend_t backend = fake_backend();
    ad_callback_state_t callback_state = {0};
    g_backend_state.next_rewarded = (platform_sdk_rewarded_result_t){
        .supported = true,
        .shown = false,
        .rewarded = false,
        .reason = PLATFORM_SDK_AD_REASON_DECLINED,
    };
    platform_sdk_set_backend(&backend, &g_backend_state);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());

    TEST_ASSERT_EQUAL_INT(
        PLATFORM_SDK_RESULT_OK,
        platform_sdk_show_rewarded("double_reward", on_rewarded_done, &callback_state));
    TEST_ASSERT_EQUAL_INT(1, callback_state.rewarded_callbacks);
    TEST_ASSERT_FALSE(callback_state.rewarded_shown);
    TEST_ASSERT_FALSE(callback_state.rewarded);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_AD_REASON_DECLINED, callback_state.rewarded_reason);
}

static void test_template_ad_flow_rejects_second_call_while_pending(void) {
    platform_sdk_backend_t backend = pending_backend();
    ad_callback_state_t callback_state = {0};
    platform_sdk_set_backend(&backend, &g_backend_state);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    TEST_ASSERT_NOT_EQUAL(0u, platform_sdk_on_pause(on_pause, &callback_state));
    TEST_ASSERT_NOT_EQUAL(0u, platform_sdk_on_resume(on_resume, &callback_state));

    TEST_ASSERT_EQUAL_INT(
        PLATFORM_SDK_RESULT_OK,
        platform_sdk_show_interstitial("level_break", on_interstitial_done, &callback_state));
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_BUSY,
                          platform_sdk_show_rewarded("double_reward", on_rewarded_done, &callback_state));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.interstitial_calls);
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.rewarded_calls);
    TEST_ASSERT_EQUAL_INT(1, callback_state.pauses);
    TEST_ASSERT_EQUAL_INT(0, callback_state.resumes);
    TEST_ASSERT_EQUAL_INT(0, callback_state.interstitial_callbacks);

    platform_sdk_backend_complete_interstitial((platform_sdk_ad_result_t){
        .supported = true,
        .shown = true,
        .reason = PLATFORM_SDK_AD_REASON_COMPLETED,
    });
    TEST_ASSERT_EQUAL_INT(1, callback_state.resumes);
    TEST_ASSERT_EQUAL_INT(1, callback_state.interstitial_callbacks);

    TEST_ASSERT_EQUAL_INT(
        PLATFORM_SDK_RESULT_OK,
        platform_sdk_show_rewarded("double_reward", on_rewarded_done, &callback_state));
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_BUSY,
                          platform_sdk_show_interstitial("level_break", on_interstitial_done, &callback_state));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.interstitial_calls);
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.rewarded_calls);
    TEST_ASSERT_EQUAL_INT(2, callback_state.pauses);
    TEST_ASSERT_EQUAL_INT(1, callback_state.resumes);
    TEST_ASSERT_EQUAL_INT(0, callback_state.rewarded_callbacks);

    platform_sdk_backend_complete_rewarded((platform_sdk_rewarded_result_t){
        .supported = true,
        .shown = true,
        .rewarded = true,
        .reason = PLATFORM_SDK_AD_REASON_COMPLETED,
    });
    TEST_ASSERT_EQUAL_INT(2, callback_state.resumes);
    TEST_ASSERT_EQUAL_INT(1, callback_state.rewarded_callbacks);
}

static void test_template_destroy_clears_listeners_and_blocks_flows(void) {
    platform_sdk_backend_t backend = fake_backend();
    ad_callback_state_t callback_state = {0};
    platform_sdk_set_backend(&backend, &g_backend_state);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    TEST_ASSERT_NOT_EQUAL(0u, platform_sdk_on_pause(on_pause, &callback_state));
    TEST_ASSERT_NOT_EQUAL(0u, platform_sdk_on_resume(on_resume, &callback_state));

    platform_sdk_destroy();
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_BOOT_DESTROYED, platform_sdk_status());
    TEST_ASSERT_EQUAL_INT(
        PLATFORM_SDK_RESULT_DESTROYED,
        platform_sdk_show_interstitial("level_break", on_interstitial_done, &callback_state));
    TEST_ASSERT_EQUAL_INT(0, callback_state.pauses);
    TEST_ASSERT_EQUAL_INT(0, callback_state.resumes);
    TEST_ASSERT_EQUAL_INT(0, callback_state.interstitial_callbacks);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_template_target_identity);
    RUN_TEST(test_template_capabilities);
    RUN_TEST(test_template_boot_and_loading_calls_are_one_shot);
    RUN_TEST(test_template_async_init_waits_for_backend_completion);
    RUN_TEST(test_template_async_init_failure_is_terminal);
    RUN_TEST(test_template_loading_progress_is_clamped_and_monotonic);
    RUN_TEST(test_template_gameplay_requires_first_input_and_tracks_state);
    RUN_TEST(test_measure_is_internal_bounded_and_forwards_exact_stable_triple);
    RUN_TEST(test_template_ad_flow_pauses_resumes_once_and_preserves_userdata);
    RUN_TEST(test_template_rewarded_decline_does_not_grant_reward);
    RUN_TEST(test_template_ad_flow_rejects_second_call_while_pending);
    RUN_TEST(test_template_destroy_clears_listeners_and_blocks_flows);
    return UNITY_END();
}
