#include "features/platform_sdk/platform_sdk_mock.h"

#include "features/platform_sdk/platform_sdk.h"
#include "unity.h"

#include <string.h>

/* Built as a native local mock-adapter target: the hand-answered portal is the
   backend, and each answer must reach the caller exactly as a portal would. */

typedef struct answers_t {
    int interstitial_calls;
    int rewarded_calls;
    platform_sdk_ad_result_t interstitial;
    platform_sdk_rewarded_result_t rewarded;
} answers_t;

static answers_t g_answers;

static void on_interstitial(platform_sdk_ad_result_t result, void *userdata) {
    (void)userdata;
    g_answers.interstitial_calls++;
    g_answers.interstitial = result;
}

static void on_rewarded(platform_sdk_rewarded_result_t result, void *userdata) {
    (void)userdata;
    g_answers.rewarded_calls++;
    g_answers.rewarded = result;
}

void setUp(void) {
    memset(&g_answers, 0, sizeof g_answers);
    platform_sdk_reset_for_tests();
    platform_sdk_mock_init();
}

void tearDown(void) {
    platform_sdk_mock_shutdown();
    platform_sdk_reset_for_tests();
}

static void test_init_boots_the_mock_adapter_idle(void) {
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_BOOT_READY, platform_sdk_status());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_MOCK_IDLE, platform_sdk_mock_flow());
}

static void test_interstitial_waits_for_the_developer(void) {
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_show_interstitial("town_gate", on_interstitial, NULL));
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_MOCK_INTERSTITIAL, platform_sdk_mock_flow());
    TEST_ASSERT_EQUAL_STRING("town_gate", platform_sdk_mock_placement());
    TEST_ASSERT_EQUAL_INT(0, g_answers.interstitial_calls);

    (void)platform_sdk_mock_close_interstitial();
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_MOCK_IDLE, platform_sdk_mock_flow());
    TEST_ASSERT_EQUAL_INT(1, g_answers.interstitial_calls);
    TEST_ASSERT_TRUE(g_answers.interstitial.shown);
}

static void test_only_a_completed_video_pays(void) {
    static const platform_sdk_ad_reason_t reasons[] = {
        PLATFORM_SDK_AD_REASON_SKIPPED, PLATFORM_SDK_AD_REASON_DECLINED,
        PLATFORM_SDK_AD_REASON_FAILED, PLATFORM_SDK_AD_REASON_COMPLETED,
    };
    for (size_t i = 0; i < sizeof reasons / sizeof reasons[0]; ++i) {
        TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_show_rewarded("chest", on_rewarded, NULL));
        TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_MOCK_REWARDED, platform_sdk_mock_flow());
        (void)platform_sdk_mock_answer_rewarded(reasons[i]);
        TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_MOCK_IDLE, platform_sdk_mock_flow());
        TEST_ASSERT_EQUAL_INT((int)i + 1, g_answers.rewarded_calls);
        TEST_ASSERT_EQUAL(reasons[i] == PLATFORM_SDK_AD_REASON_COMPLETED, g_answers.rewarded.rewarded);
    }
}

static void test_an_answer_without_an_open_flow_reaches_nobody(void) {
    (void)platform_sdk_mock_answer_rewarded(PLATFORM_SDK_AD_REASON_COMPLETED);
    (void)platform_sdk_mock_close_interstitial();
    TEST_ASSERT_EQUAL_INT(0, g_answers.rewarded_calls);
    TEST_ASSERT_EQUAL_INT(0, g_answers.interstitial_calls);
}

static void test_login_is_answered_by_hand(void) {
    platform_sdk_mark_input();
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_login());
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_MOCK_LOGIN, platform_sdk_mock_flow());
    TEST_ASSERT_FALSE(platform_sdk_authorized());

    (void)platform_sdk_mock_answer_login(true);
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_MOCK_IDLE, platform_sdk_mock_flow());
    TEST_ASSERT_TRUE(platform_sdk_authorized());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_boots_the_mock_adapter_idle);
    RUN_TEST(test_interstitial_waits_for_the_developer);
    RUN_TEST(test_only_a_completed_video_pays);
    RUN_TEST(test_an_answer_without_an_open_flow_reaches_nobody);
    RUN_TEST(test_login_is_answered_by_hand);
    return UNITY_END();
}
