#include <string.h>

#include "features/platform_sdk/platform_sdk.h"
#include "features/platform_sdk/platform_sdk_events.h"
#include "game_events.h"
#include "unity.h"

typedef struct sdk_event_backend_t {
    int init_calls;
    int loading_finished_calls;
    int game_ready_calls;
    int gameplay_start_calls;
    int gameplay_stop_calls;
    platform_sdk_ad_result_t interstitial_result;
    platform_sdk_rewarded_result_t rewarded_result;
} sdk_event_backend_t;

static sdk_event_backend_t g_backend;

static bool backend_init(void *userdata) {
    sdk_event_backend_t *backend = (sdk_event_backend_t *)userdata;
    backend->init_calls++;
    return true;
}

static void backend_loading_finished(void *userdata) {
    sdk_event_backend_t *backend = (sdk_event_backend_t *)userdata;
    backend->loading_finished_calls++;
}

static void backend_game_ready(void *userdata) {
    sdk_event_backend_t *backend = (sdk_event_backend_t *)userdata;
    backend->game_ready_calls++;
}

static void backend_gameplay_start(void *userdata) {
    sdk_event_backend_t *backend = (sdk_event_backend_t *)userdata;
    backend->gameplay_start_calls++;
}

static void backend_gameplay_stop(void *userdata) {
    sdk_event_backend_t *backend = (sdk_event_backend_t *)userdata;
    backend->gameplay_stop_calls++;
}

static platform_sdk_result_t backend_interstitial(const char *placement, void *userdata) {
    sdk_event_backend_t *backend = (sdk_event_backend_t *)userdata;
    (void)placement;
    platform_sdk_backend_complete_interstitial(backend->interstitial_result);
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_result_t backend_rewarded(const char *placement, void *userdata) {
    sdk_event_backend_t *backend = (sdk_event_backend_t *)userdata;
    (void)placement;
    platform_sdk_backend_complete_rewarded(backend->rewarded_result);
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_backend_t make_backend(void) {
    return (platform_sdk_backend_t){
        .init = backend_init,
        .game_loading_finished = backend_loading_finished,
        .game_ready = backend_game_ready,
        .gameplay_start = backend_gameplay_start,
        .gameplay_stop = backend_gameplay_stop,
        .show_interstitial = backend_interstitial,
        .show_rewarded = backend_rewarded,
    };
}

static int count_type(nt_hash64_t type) {
    int n = 0;
    int count = 0;
    const game_event_t *log = game_event_log(&n);
    for (int i = 0; i < n; ++i) {
        if (log[i].type.value == type.value) {
            count++;
        }
    }
    return count;
}

static const char *placement_text(const platform_sdk_ev_placement_t *event) {
    return (const char *)event + event->placement;
}

static const char *interstitial_placement_text(const platform_sdk_ev_interstitial_result_t *event) {
    return (const char *)event + event->placement;
}

static const char *interstitial_reason_text(const platform_sdk_ev_interstitial_result_t *event) {
    return (const char *)event + event->reason;
}

static const char *rewarded_placement_text(const platform_sdk_ev_rewarded_result_t *event) {
    return (const char *)event + event->placement;
}

static const char *rewarded_reason_text(const platform_sdk_ev_rewarded_result_t *event) {
    return (const char *)event + event->reason;
}

void setUp(void) {
    memset(&g_backend, 0, sizeof(g_backend));
    g_backend.interstitial_result = (platform_sdk_ad_result_t){
        .supported = true,
        .shown = true,
        .reason = PLATFORM_SDK_AD_REASON_COMPLETED,
    };
    g_backend.rewarded_result = (platform_sdk_rewarded_result_t){
        .supported = true,
        .shown = true,
        .rewarded = true,
        .reason = PLATFORM_SDK_AD_REASON_COMPLETED,
    };
    game_events_init();
    platform_sdk_reset_for_tests();
}

void tearDown(void) {
    platform_sdk_reset_for_tests();
    game_events_shutdown();
}

static void test_lifecycle_events_are_semantic_and_one_shot(void) {
    platform_sdk_backend_t backend = make_backend();
    platform_sdk_set_backend(&backend, &g_backend);

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_loading_finished());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_loading_finished());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_ready());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_game_ready());
    platform_sdk_mark_input();
    TEST_ASSERT_TRUE(platform_sdk_gameplay_start().started);
    TEST_ASSERT_TRUE(platform_sdk_gameplay_stop().stopped);

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_UINT64(platform_sdk_ev_platform_ready_type().value, log[0].type.value);
    TEST_ASSERT_EQUAL_UINT32(sizeof(platform_sdk_ev_platform_ready_t), log[0].size);
    TEST_ASSERT_TRUE(((const platform_sdk_ev_platform_ready_t *)log[0].payload)->ready);
    TEST_ASSERT_EQUAL_UINT64(platform_sdk_ev_game_loading_finished_type().value, log[1].type.value);
    TEST_ASSERT_EQUAL_UINT32(0u, log[1].size);
    TEST_ASSERT_EQUAL_UINT64(platform_sdk_ev_gameplay_start_type().value, log[2].type.value);
    TEST_ASSERT_EQUAL_UINT32(0u, log[2].size);
    TEST_ASSERT_EQUAL_UINT64(platform_sdk_ev_gameplay_stop_type().value, log[3].type.value);
    TEST_ASSERT_EQUAL_UINT32(0u, log[3].size);
    TEST_ASSERT_EQUAL_INT(1, g_backend.game_ready_calls);
}

static void test_ad_events_capture_request_and_result_payloads(void) {
    platform_sdk_backend_t backend = make_backend();
    platform_sdk_set_backend(&backend, &g_backend);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    game_event_frame_reset();

    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_show_interstitial("level_break", NULL, NULL));
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_show_rewarded("double_reward", NULL, NULL));

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    TEST_ASSERT_EQUAL_INT(4, n);
    TEST_ASSERT_EQUAL_UINT64(platform_sdk_ev_interstitial_request_type().value, log[0].type.value);
    TEST_ASSERT_EQUAL_STRING("level_break", placement_text((const platform_sdk_ev_placement_t *)log[0].payload));

    TEST_ASSERT_EQUAL_UINT64(platform_sdk_ev_interstitial_result_type().value, log[1].type.value);
    const platform_sdk_ev_interstitial_result_t *interstitial =
        (const platform_sdk_ev_interstitial_result_t *)log[1].payload;
    TEST_ASSERT_TRUE(interstitial->supported);
    TEST_ASSERT_TRUE(interstitial->shown);
    TEST_ASSERT_EQUAL_STRING("level_break", interstitial_placement_text(interstitial));
    TEST_ASSERT_EQUAL_STRING("completed", interstitial_reason_text(interstitial));

    TEST_ASSERT_EQUAL_UINT64(platform_sdk_ev_rewarded_request_type().value, log[2].type.value);
    TEST_ASSERT_EQUAL_STRING("double_reward", placement_text((const platform_sdk_ev_placement_t *)log[2].payload));

    TEST_ASSERT_EQUAL_UINT64(platform_sdk_ev_rewarded_result_type().value, log[3].type.value);
    const platform_sdk_ev_rewarded_result_t *rewarded = (const platform_sdk_ev_rewarded_result_t *)log[3].payload;
    TEST_ASSERT_TRUE(rewarded->supported);
    TEST_ASSERT_TRUE(rewarded->shown);
    TEST_ASSERT_TRUE(rewarded->rewarded);
    TEST_ASSERT_EQUAL_STRING("double_reward", rewarded_placement_text(rewarded));
    TEST_ASSERT_EQUAL_STRING("completed", rewarded_reason_text(rewarded));
}

static void test_gameplay_start_blocked_before_input_has_no_event(void) {
    platform_sdk_backend_t backend = make_backend();
    platform_sdk_set_backend(&backend, &g_backend);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    game_event_frame_reset();

    platform_sdk_gameplay_start_result_t result = platform_sdk_gameplay_start();
    TEST_ASSERT_FALSE(result.started);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_WAITING_FOR_INPUT, result.reason);
    TEST_ASSERT_EQUAL_INT(0, count_type(platform_sdk_ev_gameplay_start_type()));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_lifecycle_events_are_semantic_and_one_shot);
    RUN_TEST(test_ad_events_capture_request_and_result_payloads);
    RUN_TEST(test_gameplay_start_blocked_before_input_has_no_event);
    return UNITY_END();
}
