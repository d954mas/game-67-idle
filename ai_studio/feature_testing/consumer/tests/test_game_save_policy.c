#include <string.h>

#include "game_save_policy.h"
#include "game_save.h"

#include "unity.h"

static bool s_document_is_valid = true;

bool game_save_validate_document_string(const char *text, char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    return s_document_is_valid && strstr(text, "\"invalid_full\":true") == NULL;
}

static game_save_choice_t decide(const char *local, const char *remote) {
    return game_save_policy_decide(local, remote);
}

void setUp(void) { s_document_is_valid = true; }
void tearDown(void) {}

static void test_higher_hero_level_with_same_tutorial_keeps_local(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_KEEP_LOCAL,
        decide("{\"saved_at\":1,\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":3}}}}}",
               "{\"saved_at\":2,\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_higher_hero_level_with_same_tutorial_keeps_remote(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_KEEP_REMOTE,
        decide("{\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":1}}}}}",
               "{\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_crossed_tutorial_and_hero_progress_requires_choice(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_ASK,
        decide("{\"features\":{\"game\":{\"tutorial\":{\"done\":false}},\"progression\":{\"tracks\":{\"hero\":{\"level\":3}}}}}",
               "{\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_equal_progress_with_different_features_requires_choice(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_ASK,
        decide("{\"features\":{\"game\":{\"tutorial\":{\"done\":true},\"test_ui_clicks\":1},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}",
               "{\"features\":{\"game\":{\"tutorial\":{\"done\":true},\"test_ui_clicks\":2},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_equal_progress_uses_positive_playtime_as_tie_breaker(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_KEEP_REMOTE,
        decide("{\"playtime_ms\":\"10\",\"features\":{\"game\":{\"tutorial\":{\"done\":true},\"test_ui_clicks\":1},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}",
               "{\"playtime_ms\":\"20\",\"features\":{\"game\":{\"tutorial\":{\"done\":true},\"test_ui_clicks\":2},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_playtime_never_resolves_crossed_progress(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_ASK,
        decide("{\"playtime_ms\":\"999999\",\"features\":{\"game\":{\"tutorial\":{\"done\":false}},\"progression\":{\"tracks\":{\"hero\":{\"level\":3}}}}}",
               "{\"playtime_ms\":\"1\",\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_missing_playtime_does_not_break_progress_tie(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_ASK,
        decide("{\"features\":{\"game\":{\"tutorial\":{\"done\":true},\"test_ui_clicks\":1},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}",
               "{\"playtime_ms\":\"20\",\"features\":{\"game\":{\"tutorial\":{\"done\":true},\"test_ui_clicks\":2},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_identical_features_with_larger_remote_playtime_keep_remote(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_KEEP_REMOTE,
        decide("{\"playtime_ms\":\"10\",\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}",
               "{\"playtime_ms\":\"20\",\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_identical_features_keep_local_despite_envelope_metadata(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_KEEP_LOCAL,
        decide("{\"saved_at\":1,\"save_seq\":4,\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}",
               "{\"saved_at\":9,\"save_seq\":99,\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_invalid_document_requires_choice(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_ASK,
        decide("{\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":\"three\"}}}}}",
               "{\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

static void test_full_document_validation_failure_requires_choice(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_ASK,
        decide("{\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":3}}}}}",
               "{\"invalid_full\":true,\"features\":{\"game\":{\"tutorial\":{\"done\":true}},\"progression\":{\"tracks\":{\"hero\":{\"level\":2}}}}}"));
}

/* Storage and the cloud carry NTGS text; this is the document the policy
   sees in a running game. */
#define TEXT_SAVE(saved_at, playtime, done, level) \
    "NTGS 1\nformat=1\nsave_version=1\nsaved_at=" saved_at "\nsave_seq=3\nplaytime_ms=" playtime "\n\n" \
    "[game 1]\ntutorial.done=" done "\n\n[progression 1]\ntracks.hero.level=" level "\n"

static void test_text_account_ahead_on_hero_level_keeps_remote(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_KEEP_REMOTE,
        decide(TEXT_SAVE("1", "10", "true", "1"), TEXT_SAVE("2", "5", "true", "2")));
}

static void test_text_device_ahead_on_hero_level_keeps_local(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_KEEP_LOCAL,
        decide(TEXT_SAVE("1", "10", "true", "3"), TEXT_SAVE("2", "50", "true", "2")));
}

static void test_text_identical_features_ignore_the_header(void) {
    TEST_ASSERT_TRUE(game_save_policy_same_features(
        TEXT_SAVE("1", "10", "true", "2"), TEXT_SAVE("9", "90", "true", "2")));
    TEST_ASSERT_FALSE(game_save_policy_same_features(
        TEXT_SAVE("1", "10", "true", "2"), TEXT_SAVE("1", "10", "true", "3")));
}

static void test_text_new_player_without_tracks_takes_the_account_ahead(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_KEEP_REMOTE,
        decide("NTGS 1\nformat=1\nsave_version=1\nsaved_at=1\nsave_seq=1\nplaytime_ms=5\n\n"
               "[game 1]\ntutorial.done=false\n\n[progression 1]\n",
               TEXT_SAVE("2", "50", "true", "2")));
}

static void test_malformed_text_requires_choice(void) {
    TEST_ASSERT_EQUAL(GAME_SAVE_ASK,
        decide(TEXT_SAVE("1", "10", "true", "1"), "NTGS 1\n\n[game 1]\ntutorial.done=\n"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_higher_hero_level_with_same_tutorial_keeps_local);
    RUN_TEST(test_higher_hero_level_with_same_tutorial_keeps_remote);
    RUN_TEST(test_crossed_tutorial_and_hero_progress_requires_choice);
    RUN_TEST(test_equal_progress_with_different_features_requires_choice);
    RUN_TEST(test_equal_progress_uses_positive_playtime_as_tie_breaker);
    RUN_TEST(test_playtime_never_resolves_crossed_progress);
    RUN_TEST(test_missing_playtime_does_not_break_progress_tie);
    RUN_TEST(test_identical_features_with_larger_remote_playtime_keep_remote);
    RUN_TEST(test_identical_features_keep_local_despite_envelope_metadata);
    RUN_TEST(test_invalid_document_requires_choice);
    RUN_TEST(test_text_account_ahead_on_hero_level_keeps_remote);
    RUN_TEST(test_text_device_ahead_on_hero_level_keeps_local);
    RUN_TEST(test_text_identical_features_ignore_the_header);
    RUN_TEST(test_text_new_player_without_tracks_takes_the_account_ahead);
    RUN_TEST(test_malformed_text_requires_choice);
    RUN_TEST(test_full_document_validation_failure_requires_choice);
    return UNITY_END();
}
