#include "systems/sys_platform_hooks.h"

#include "app/nt_app.h"
#include "features/platform_sdk/platform_sdk.h"
#include "game_audio.h"
#include "game_events.h"
#include "unity.h"

#include <string.h>

/* The real game_audio drags the engine mixer in; the hook only needs the edge. */
static int g_audio_paused_calls;
static bool g_audio_paused;

void game_audio_set_paused(bool paused) {
    g_audio_paused_calls++;
    g_audio_paused = paused;
}

typedef struct hooks_backend_state_t {
    int interstitial_calls;
    int rewarded_calls;
    char last_placement[64];
} hooks_backend_state_t;

static hooks_backend_state_t g_backend_state;

static bool backend_init(void *userdata) {
    (void)userdata;
    return true;
}

/* The portal answers on its own schedule: the backend only accepts, and the
   test completes the video the way the JavaScript adapter would. */
static platform_sdk_result_t backend_show_interstitial(const char *placement, void *userdata) {
    hooks_backend_state_t *state = (hooks_backend_state_t *)userdata;
    state->interstitial_calls++;
    strncpy(state->last_placement, placement ? placement : "", sizeof state->last_placement - 1);
    return PLATFORM_SDK_RESULT_OK;
}

static platform_sdk_result_t backend_show_rewarded(const char *placement, void *userdata) {
    hooks_backend_state_t *state = (hooks_backend_state_t *)userdata;
    state->rewarded_calls++;
    strncpy(state->last_placement, placement ? placement : "", sizeof state->last_placement - 1);
    return PLATFORM_SDK_RESULT_OK;
}

static int g_reward_calls;
static char g_reward_placement[64];

static void on_reward(const char *placement, void *userdata) {
    (void)userdata;
    g_reward_calls++;
    strncpy(g_reward_placement, placement, sizeof g_reward_placement - 1);
}

void setUp(void) {
    platform_sdk_backend_t sdk_backend = {
        .init = backend_init,
        .show_interstitial = backend_show_interstitial,
        .show_rewarded = backend_show_rewarded,
    };
    memset(&g_backend_state, 0, sizeof g_backend_state);
    g_audio_paused_calls = 0;
    g_audio_paused = false;
    g_reward_calls = 0;
    g_reward_placement[0] = '\0';
    g_nt_app.paused = false;
    game_events_init();
    platform_sdk_reset_for_tests();
    platform_sdk_set_backend(&sdk_backend, &g_backend_state);
    (void)platform_sdk_init();
    platform_hooks_init();
}

void tearDown(void) {
    platform_hooks_shutdown();
    platform_sdk_reset_for_tests();
    game_events_shutdown();
}

static void test_hidden_tab_freezes_the_clock_and_mutes(void) {
    platform_hooks_update(true);
    TEST_ASSERT_FALSE(g_nt_app.paused);

    platform_hooks_set_page_visible(false);
    platform_hooks_update(true);
    TEST_ASSERT_FALSE(platform_hooks_attention());
    TEST_ASSERT_FALSE(platform_hooks_gameplay_allowed());
    TEST_ASSERT_TRUE(g_nt_app.paused);
    TEST_ASSERT_TRUE(g_audio_paused);

    platform_hooks_set_page_visible(true);
    platform_hooks_update(true);
    TEST_ASSERT_FALSE(g_nt_app.paused);
    TEST_ASSERT_FALSE(g_audio_paused);
}

static void test_window_blur_keeps_a_visible_game_running(void) {
    platform_hooks_set_window_focused(false);
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(platform_hooks_attention());
    TEST_ASSERT_FALSE(g_nt_app.paused);

    /* Hidden while also blurred still freezes: the rule is visibility. */
    platform_hooks_set_page_visible(false);
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(g_nt_app.paused);

    platform_hooks_set_page_visible(true);
    platform_hooks_set_window_focused(true);
    platform_hooks_update(true);
    TEST_ASSERT_FALSE(g_nt_app.paused);
}

/* Yandex Games requires silence from a game that has lost focus (requirement
   1.3) and its own pause events to be honoured (1.20). The two answer different
   questions, so they are asserted apart: sound follows focus, the clock follows
   visibility, and a portal pause stops both. */
static void test_a_blurred_game_goes_silent_but_keeps_running(void) {
    platform_hooks_set_window_focused(false);
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(g_audio_paused);
    TEST_ASSERT_FALSE(g_nt_app.paused);

    platform_hooks_set_window_focused(true);
    platform_hooks_update(true);
    TEST_ASSERT_FALSE(g_audio_paused);
}

static void test_the_portal_pause_stops_the_clock_and_the_mixer(void) {
    platform_sdk_backend_portal_pause();
    platform_hooks_update(true);
    TEST_ASSERT_FALSE(platform_hooks_gameplay_allowed());
    TEST_ASSERT_TRUE(g_nt_app.paused);
    TEST_ASSERT_TRUE(g_audio_paused);

    platform_sdk_backend_portal_resume();
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(platform_hooks_gameplay_allowed());
    TEST_ASSERT_FALSE(g_nt_app.paused);
    TEST_ASSERT_FALSE(g_audio_paused);
}

/* The facade raises the same pause listeners for the portal's pause as for an
   ad. A portal pause is not an ad: nothing is on the ad timer, so a long pause
   never ends in a fabricated ad completion. */
static void test_a_portal_pause_is_not_an_ad(void) {
    platform_sdk_backend_portal_pause();
    TEST_ASSERT_FALSE(platform_hooks_ad_active());
    platform_sdk_backend_portal_resume();
    TEST_ASSERT_FALSE(platform_hooks_ad_active());
}

/* A portal mute switch silences the game and leaves it running. */
static void test_the_portal_mute_switch_silences_without_freezing(void) {
    platform_sdk_backend_portal_audio(false);
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(g_audio_paused);
    TEST_ASSERT_FALSE(g_nt_app.paused);
    TEST_ASSERT_TRUE(platform_hooks_gameplay_allowed());

    platform_sdk_backend_portal_audio(true);
    platform_hooks_update(true);
    TEST_ASSERT_FALSE(g_audio_paused);
}

static void test_freeze_waits_for_a_ready_runtime(void) {
    platform_hooks_set_page_visible(false);
    platform_hooks_update(false);
    TEST_ASSERT_FALSE(g_nt_app.paused); /* loading must finish even in a background tab */
}

static void test_the_freeze_edge_is_reported_once(void) {
    platform_hooks_set_page_visible(false);
    platform_hooks_update(true);
    platform_hooks_update(true);
    platform_hooks_update(true);
    TEST_ASSERT_EQUAL_INT(1, g_audio_paused_calls);
}

static void test_commercial_break_freezes_until_the_video_ends(void) {
    TEST_ASSERT_TRUE(platform_hooks_commercial_break("level_end"));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.interstitial_calls);
    TEST_ASSERT_EQUAL_STRING("level_end", g_backend_state.last_placement);
    TEST_ASSERT_TRUE(platform_hooks_ad_active());

    platform_hooks_update(true);
    TEST_ASSERT_TRUE(g_nt_app.paused);
    TEST_ASSERT_FALSE(platform_hooks_gameplay_allowed());

    platform_sdk_backend_complete_interstitial_request(
        platform_sdk_active_interstitial_request_id(), (platform_sdk_ad_result_t){
        .supported = true, .shown = true, .reason = PLATFORM_SDK_AD_REASON_COMPLETED});
    TEST_ASSERT_FALSE(platform_hooks_ad_active());

    platform_hooks_update(true);
    TEST_ASSERT_FALSE(g_nt_app.paused);
    TEST_ASSERT_FALSE(g_audio_paused);
    TEST_ASSERT_EQUAL_UINT(1u, platform_hooks_break_count());
}

/* The mixer must come back even when the video never played. */
static void test_a_refused_video_still_unmutes(void) {
    TEST_ASSERT_TRUE(platform_hooks_commercial_break("level_end"));
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(g_audio_paused);

    platform_sdk_backend_complete_interstitial_request(
        platform_sdk_active_interstitial_request_id(), (platform_sdk_ad_result_t){
        .supported = true, .shown = false, .reason = PLATFORM_SDK_AD_REASON_FAILED});
    platform_hooks_update(true);
    TEST_ASSERT_FALSE(g_audio_paused);
    TEST_ASSERT_FALSE(g_nt_app.paused);
}

static void test_an_empty_placement_is_refused(void) {
    TEST_ASSERT_FALSE(platform_hooks_commercial_break(""));
    TEST_ASSERT_FALSE(platform_hooks_commercial_break(NULL));
    TEST_ASSERT_FALSE(platform_hooks_rewarded("", on_reward, NULL));
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.interstitial_calls);
    TEST_ASSERT_EQUAL_INT(0, g_backend_state.rewarded_calls);
}

static void test_reward_pays_out_only_on_success(void) {
    TEST_ASSERT_TRUE(platform_hooks_rewarded("extra_life", on_reward, NULL));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.rewarded_calls);
    platform_sdk_backend_complete_rewarded_request(
        platform_sdk_active_rewarded_request_id(), (platform_sdk_rewarded_result_t){
        .supported = true, .shown = true, .rewarded = false,
        .reason = PLATFORM_SDK_AD_REASON_SKIPPED});
    TEST_ASSERT_EQUAL_INT(0, g_reward_calls);

    TEST_ASSERT_TRUE(platform_hooks_rewarded("double_prize", on_reward, NULL));
    platform_sdk_backend_complete_rewarded_request(
        platform_sdk_active_rewarded_request_id(), (platform_sdk_rewarded_result_t){
        .supported = true, .shown = true, .rewarded = true,
        .reason = PLATFORM_SDK_AD_REASON_COMPLETED});
    TEST_ASSERT_EQUAL_INT(1, g_reward_calls);
    TEST_ASSERT_EQUAL_STRING("double_prize", g_reward_placement);
    TEST_ASSERT_EQUAL_UINT(1u, platform_hooks_reward_grant_count());
}

/* The build promises videos; a portal that answers "unsupported" at runtime
   has withdrawn that promise for the session, and the offer stops. */
static void test_a_portal_refusal_withdraws_the_offer(void) {
    TEST_ASSERT_TRUE(platform_hooks_rewarded_supported());
    TEST_ASSERT_TRUE(platform_hooks_rewarded("double_prize", on_reward, NULL));
    platform_sdk_backend_complete_rewarded_request(
        platform_sdk_active_rewarded_request_id(), (platform_sdk_rewarded_result_t){
        .supported = false, .shown = false, .rewarded = false,
        .reason = PLATFORM_SDK_AD_REASON_UNSUPPORTED});
    TEST_ASSERT_EQUAL_INT(0, g_reward_calls);
    TEST_ASSERT_FALSE(platform_hooks_rewarded_supported());
    TEST_ASSERT_FALSE(platform_hooks_rewarded("double_prize", on_reward, NULL));
}

/* No chaining and no double payout while one video is in flight. */
static void test_a_second_reward_request_is_refused_while_one_is_open(void) {
    TEST_ASSERT_TRUE(platform_hooks_rewarded("extra_life", on_reward, NULL));
    TEST_ASSERT_FALSE(platform_hooks_rewarded("extra_life", on_reward, NULL));
    TEST_ASSERT_EQUAL_INT(1, g_backend_state.rewarded_calls);
}

static void test_portal_resume_keeps_a_visible_ad_frozen(void) {
    platform_sdk_backend_portal_pause();
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(platform_hooks_commercial_break("late_ad"));
    const platform_sdk_ad_request_id_t request_id = platform_sdk_active_interstitial_request_id();
    platform_sdk_backend_ad_visible(request_id, true);
    TEST_ASSERT_TRUE(platform_hooks_ad_active());
    platform_sdk_backend_portal_resume();
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(g_nt_app.paused);
    TEST_ASSERT_TRUE(g_audio_paused);

    platform_sdk_backend_complete_interstitial_request(request_id, (platform_sdk_ad_result_t){
        .supported = true, .shown = true, .reason = PLATFORM_SDK_AD_REASON_COMPLETED});
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(g_nt_app.paused);
    platform_sdk_backend_ad_visible(request_id, false);
    TEST_ASSERT_FALSE(platform_hooks_ad_active());
    platform_hooks_update(true);
    TEST_ASSERT_FALSE(g_nt_app.paused);
    TEST_ASSERT_FALSE(g_audio_paused);
}

static void test_shutdown_thaws_a_frozen_clock(void) {
    platform_hooks_set_page_visible(false);
    platform_hooks_update(true);
    TEST_ASSERT_TRUE(g_nt_app.paused);
    platform_hooks_shutdown();
    TEST_ASSERT_FALSE(g_nt_app.paused);
    TEST_ASSERT_FALSE(g_audio_paused);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_hidden_tab_freezes_the_clock_and_mutes);
    RUN_TEST(test_window_blur_keeps_a_visible_game_running);
    RUN_TEST(test_a_blurred_game_goes_silent_but_keeps_running);
    RUN_TEST(test_the_portal_pause_stops_the_clock_and_the_mixer);
    RUN_TEST(test_a_portal_pause_is_not_an_ad);
    RUN_TEST(test_the_portal_mute_switch_silences_without_freezing);
    RUN_TEST(test_freeze_waits_for_a_ready_runtime);
    RUN_TEST(test_the_freeze_edge_is_reported_once);
    RUN_TEST(test_commercial_break_freezes_until_the_video_ends);
    RUN_TEST(test_a_refused_video_still_unmutes);
    RUN_TEST(test_an_empty_placement_is_refused);
    RUN_TEST(test_reward_pays_out_only_on_success);
    RUN_TEST(test_a_portal_refusal_withdraws_the_offer);
    RUN_TEST(test_a_second_reward_request_is_refused_while_one_is_open);
    RUN_TEST(test_portal_resume_keeps_a_visible_ad_frozen);
    RUN_TEST(test_shutdown_thaws_a_frozen_clock);
    return UNITY_END();
}
