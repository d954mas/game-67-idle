#include <stdlib.h>
#include <string.h>

#include "game_save_sync.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_observed_remote_evaluates_when_local_arrives(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "account"));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "fresh-local", true));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_ADOPT_REMOTE, game_save_sync_state(&sync));
    game_save_sync_destroy(&sync);
}

static void test_observed_empty_evaluates_when_local_arrives(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    game_save_sync_remote_empty(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_UPLOAD_READY, game_save_sync_state(&sync));
    game_save_sync_destroy(&sync);
}

static void test_unknown_remote_blocks_upload(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    game_save_sync_remote_pending(&sync);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_WAITING_REMOTE, game_save_sync_state(&sync));
    TEST_ASSERT_NULL(game_save_sync_upload_document(&sync));
    game_save_sync_destroy(&sync);
}

static void test_failed_remote_read_blocks_upload(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    game_save_sync_remote_unavailable(&sync);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_REMOTE_UNAVAILABLE, game_save_sync_state(&sync));
    TEST_ASSERT_NULL(game_save_sync_upload_document(&sync));
    game_save_sync_destroy(&sync);
}

static void test_empty_remote_allows_initial_local_upload(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    game_save_sync_remote_empty(&sync);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_UPLOAD_READY, game_save_sync_state(&sync));
    TEST_ASSERT_EQUAL_STRING("local", game_save_sync_upload_document(&sync));
    game_save_sync_destroy(&sync);
}

static void test_fresh_local_adopts_cloud_without_timestamp_comparison(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "new-device", true));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "account"));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_ADOPT_REMOTE, game_save_sync_state(&sync));
    TEST_ASSERT_EQUAL_STRING("account", game_save_sync_remote_document_value(&sync));
    game_save_sync_destroy(&sync);
}

static void test_shared_base_selects_changed_side(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_base(&sync, "base"));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "base", false));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "remote"));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_ADOPT_REMOTE, game_save_sync_state(&sync));

    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "base"));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_UPLOAD_READY, game_save_sync_state(&sync));
    TEST_ASSERT_EQUAL_STRING("local", game_save_sync_upload_document(&sync));
    game_save_sync_destroy(&sync);
}

static void test_divergent_documents_require_explicit_resolution(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_base(&sync, "base"));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "remote"));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_sync_state(&sync));
    TEST_ASSERT_NULL(game_save_sync_upload_document(&sync));
    TEST_ASSERT_TRUE(game_save_sync_resolve(&sync, GAME_SAVE_KEEP_REMOTE));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_ADOPT_REMOTE, game_save_sync_state(&sync));
    game_save_sync_destroy(&sync);
}

static void test_keep_local_requires_the_same_remote_version_after_refresh(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_base(&sync, "base"));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "remote-one"));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_sync_state(&sync));
    TEST_ASSERT_TRUE(game_save_sync_resolve(&sync, GAME_SAVE_KEEP_LOCAL));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "remote-two"));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_sync_state(&sync));
    TEST_ASSERT_NULL(game_save_sync_upload_document(&sync));
    game_save_sync_destroy(&sync);
}


static void test_keep_local_against_empty_cloud_requires_empty_on_refresh(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_base(&sync, "base"));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    game_save_sync_remote_empty(&sync);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_sync_state(&sync));
    TEST_ASSERT_TRUE(game_save_sync_resolve(&sync, GAME_SAVE_KEEP_LOCAL));
    game_save_sync_remote_empty(&sync);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_UPLOAD_READY, game_save_sync_state(&sync));
    game_save_sync_destroy(&sync);
}

static void test_keep_local_against_remote_requires_nonempty_remote_on_refresh(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_base(&sync, "base"));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "remote"));
    TEST_ASSERT_TRUE(game_save_sync_resolve(&sync, GAME_SAVE_KEEP_LOCAL));
    game_save_sync_remote_empty(&sync);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_sync_state(&sync));
    TEST_ASSERT_NULL(game_save_sync_upload_document(&sync));
    game_save_sync_destroy(&sync);
}

static void test_local_change_keeps_unresolved_conflict_visible(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_base(&sync, "base"));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "remote"));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_sync_state(&sync));

    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local-changed", false));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_sync_state(&sync));
    TEST_ASSERT_EQUAL_STRING("remote", game_save_sync_remote_document_value(&sync));
    game_save_sync_destroy(&sync);
}

static void test_keep_local_still_requires_remote_refresh_after_local_change(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_base(&sync, "base"));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", false));
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "remote"));
    TEST_ASSERT_TRUE(game_save_sync_resolve(&sync, GAME_SAVE_KEEP_LOCAL));

    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local-changed", false));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH, game_save_sync_state(&sync));
    TEST_ASSERT_EQUAL_STRING("remote", game_save_sync_remote_document_value(&sync));
    game_save_sync_destroy(&sync);
}

static void test_late_remote_cannot_replace_a_running_local_save(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "local", true));
    game_save_sync_remote_pending(&sync);
    game_save_sync_disallow_remote_adoption(&sync);
    TEST_ASSERT_TRUE(game_save_sync_remote_document(&sync, "account"));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_sync_state(&sync));
    TEST_ASSERT_NULL(game_save_sync_upload_document(&sync));
    game_save_sync_destroy(&sync);
}

static void test_failed_store_retries_and_acknowledges_the_sent_snapshot_only(void) {
    game_save_sync_t sync;
    game_save_sync_init(&sync);
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "first", false));
    game_save_sync_remote_empty(&sync);
    TEST_ASSERT_TRUE(game_save_sync_store_started(&sync));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "second", false));
    game_save_sync_store_finished(&sync, false);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH, game_save_sync_state(&sync));
    TEST_ASSERT_NULL(game_save_sync_upload_document(&sync));
    game_save_sync_remote_empty(&sync);
    TEST_ASSERT_EQUAL_STRING("second", game_save_sync_upload_document(&sync));
    TEST_ASSERT_TRUE(game_save_sync_store_started(&sync));
    game_save_sync_store_finished(&sync, true);
    TEST_ASSERT_EQUAL_STRING("second", game_save_sync_base_document(&sync));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_SYNCHRONIZED, game_save_sync_state(&sync));
    TEST_ASSERT_TRUE(game_save_sync_set_local(&sync, "third", false));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH, game_save_sync_state(&sync));
    TEST_ASSERT_NULL(game_save_sync_upload_document(&sync));
    game_save_sync_destroy(&sync);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_observed_remote_evaluates_when_local_arrives);
    RUN_TEST(test_observed_empty_evaluates_when_local_arrives);
    RUN_TEST(test_unknown_remote_blocks_upload);
    RUN_TEST(test_failed_remote_read_blocks_upload);
    RUN_TEST(test_empty_remote_allows_initial_local_upload);
    RUN_TEST(test_fresh_local_adopts_cloud_without_timestamp_comparison);
    RUN_TEST(test_shared_base_selects_changed_side);
    RUN_TEST(test_divergent_documents_require_explicit_resolution);
    RUN_TEST(test_keep_local_requires_the_same_remote_version_after_refresh);
    RUN_TEST(test_keep_local_against_empty_cloud_requires_empty_on_refresh);
    RUN_TEST(test_keep_local_against_remote_requires_nonempty_remote_on_refresh);
    RUN_TEST(test_local_change_keeps_unresolved_conflict_visible);
    RUN_TEST(test_keep_local_still_requires_remote_refresh_after_local_change);
    RUN_TEST(test_late_remote_cannot_replace_a_running_local_save);
    RUN_TEST(test_failed_store_retries_and_acknowledges_the_sent_snapshot_only);
    return UNITY_END();
}
