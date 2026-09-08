#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "features/platform_sdk/platform_sdk_cloud.h"
#include "log/nt_log.h"
#include "game_save.h"
#include "game_storage.h"
#include "systems/sys_cloud_save.h"
#include "unity.h"

static char *s_local;
static char *s_base;
static char *s_live;
static int64_t s_saved_at;
static double s_now;
static double s_write_interval;
static uint32_t s_read_id;
static uint32_t s_write_id;
static int s_read_calls;
static int s_write_calls;
static bool s_sync_write_ack;
static bool s_blocking_write_ok;
static game_save_choice_t s_policy_decision;
static int s_policy_calls;

static char *copy_text(const char *text) {
    if (text == NULL) return NULL;
    const size_t length = strlen(text);
    char *copy = malloc(length + 1U);
    if (copy != NULL) memcpy(copy, text, length + 1U);
    return copy;
}

static bool replace_text(char **destination, const char *text) {
    char *copy = copy_text(text);
    if (text != NULL && copy == NULL) return false;
    free(*destination);
    *destination = copy;
    return true;
}

double nt_time_now(void) { return s_now; }
void nt_log_write(nt_log_level_t level, const char *domain, const char *format, ...) {
    (void)level;
    (void)domain;
    (void)format;
}

bool game_storage_read(const char *slot, char **out, game_storage_read_status_t *status,
                       char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    const char *source = strcmp(slot, "cloud_sync_base") == 0 ? s_base : s_local;
    if (source == NULL) {
        if (status != NULL) *status = GAME_STORAGE_READ_ABSENT;
        return false;
    }
    *out = copy_text(source);
    if (status != NULL) *status = *out != NULL ? GAME_STORAGE_READ_OK : GAME_STORAGE_READ_ERROR;
    return *out != NULL;
}

bool game_storage_write(const char *slot, const char *text, char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    return replace_text(strcmp(slot, "cloud_sync_base") == 0 ? &s_base : &s_local, text);
}

bool game_storage_write_blocking(const char *slot, const char *text, char *error, int error_cap) {
    if (!s_blocking_write_ok) return false;
    return game_storage_write(slot, text, error, error_cap);
}

char *game_save_export_string(char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    return copy_text(s_live);
}

bool game_save_import_string(const char *text, char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    return text != NULL && strcmp(text, "invalid") != 0 && replace_text(&s_live, text);
}

int64_t game_save_last_saved_at(void) { return s_saved_at; }

static bool backend_supported(void *context) { (void)context; return true; }
static void backend_load(uint32_t id, const char *key, void *context) {
    (void)key;
    (void)context;
    s_read_id = id;
    s_read_calls++;
}
static void backend_store(uint32_t id, const char *key, const char *text, void *context) {
    (void)key;
    (void)text;
    (void)context;
    s_write_id = id;
    s_write_calls++;
    if (s_sync_write_ack) {
        platform_sdk_cloud_complete_store(id, PLATFORM_SDK_CLOUD_WRITE_ACKNOWLEDGED);
    }
}

static game_save_choice_t test_policy(const char *local, const char *remote) {
    (void)local;
    (void)remote;
    s_policy_calls++;
    return s_policy_decision;
}

static bool same_document(const char *first, const char *second) {
    return first != NULL && second != NULL && strcmp(first, second) == 0;
}

static bool same_features(const char *first, const char *second) {
    if (first == NULL || second == NULL) return false;
    return strcmp(first, second) == 0 ||
           (strcmp(first, "local") == 0 && strcmp(second, "local-playtime-advanced") == 0) ||
           (strcmp(first, "local-playtime-advanced") == 0 && strcmp(second, "local") == 0);
}

static void complete_read(platform_sdk_cloud_status_t status, const char *text) {
    platform_sdk_cloud_complete_load(s_read_id, status, text);
}

static void complete_read_id(uint32_t id, platform_sdk_cloud_status_t status, const char *text) {
    platform_sdk_cloud_complete_load(id, status, text);
}

void setUp(void) {
    const platform_sdk_cloud_backend_t backend = {
        .supported = backend_supported,
        .load = backend_load,
        .store = backend_store,
    };
    platform_sdk_cloud_set_backend(&backend, NULL);
    cloud_save_init(test_policy, same_document, s_write_interval);
    s_saved_at = 1;
    s_blocking_write_ok = true;
    s_policy_decision = GAME_SAVE_ASK;
    s_policy_calls = 0;
}

void tearDown(void) {
    game_save_cloud_shutdown();
    platform_sdk_cloud_reset();
    free(s_local);
    free(s_base);
    free(s_live);
    s_local = s_base = s_live = NULL;
    s_now = 0.0;
    s_read_id = s_write_id = 0;
    s_read_calls = s_write_calls = 0;
    s_sync_write_ack = false;
    s_blocking_write_ok = true;
    s_policy_decision = GAME_SAVE_ASK;
    s_policy_calls = 0;
    s_write_interval = 0.0;
}

static void test_preboot_ready_remote_adopts_fresh_local(void) {
    s_live = copy_text("fresh-local");
    (void)game_save_cloud_boot_settled();
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    TEST_ASSERT_TRUE(game_save_cloud_boot_settled());
    TEST_ASSERT_TRUE(game_save_cloud_start(true));
    TEST_ASSERT_EQUAL_STRING("account", s_live);
    TEST_ASSERT_EQUAL_STRING("account", s_local);
    TEST_ASSERT_EQUAL_STRING("account", s_base);
}

static void test_fresh_device_adopts_valid_account_document(void) {
    s_live = copy_text("fresh-local");
    (void)game_save_cloud_boot_settled();
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    TEST_ASSERT_TRUE(game_save_cloud_start(true));
    TEST_ASSERT_EQUAL_STRING("account", s_live);
    TEST_ASSERT_EQUAL_STRING("account", s_local);
    TEST_ASSERT_EQUAL_STRING("account", s_base);
}

static void test_failed_read_blocks_writes_and_retries_once_per_interval(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    (void)game_save_cloud_boot_settled();
    complete_read(PLATFORM_SDK_CLOUD_FAILED, NULL);
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(2, s_read_calls);
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(2, s_read_calls);
    TEST_ASSERT_EQUAL_INT(0, s_write_calls);
}

static void test_synchronous_store_ack_commits_the_exact_snapshot(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    s_sync_write_ack = true;
    (void)game_save_cloud_boot_settled();
    complete_read(PLATFORM_SDK_CLOUD_EMPTY, NULL);
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    game_save_cloud_tick();
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(1, s_write_calls);
    TEST_ASSERT_EQUAL_STRING("local", s_base);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_SYNCHRONIZED, game_save_cloud_state());
}

/* A metered portal store is the reason this interval exists: the local save
   changes every few seconds in an idle game, and every change would otherwise
   be one paid upload. After each acknowledged upload the coordinator re-reads
   the account document, so the second upload needs that read answered. */
static void upload_local_and_settle(const char *document, int64_t saved_at) {
    s_saved_at = saved_at;
    TEST_ASSERT_TRUE(replace_text(&s_local, document));
    TEST_ASSERT_TRUE(replace_text(&s_live, document));
    game_save_cloud_tick();
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"local\"}");
    game_save_cloud_tick();
}

static void test_write_interval_holds_back_the_next_upload(void) {
    s_write_interval = 60.0;
    s_sync_write_ack = true;
    game_save_cloud_shutdown();
    cloud_save_init(test_policy, same_document, s_write_interval);
    s_local = copy_text("local");
    s_live = copy_text("local");
    (void)game_save_cloud_boot_settled();
    complete_read(PLATFORM_SDK_CLOUD_EMPTY, NULL);
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(1, s_write_calls);

    s_now = 30.0;
    upload_local_and_settle("local-2", 2);
    TEST_ASSERT_EQUAL_INT(1, s_write_calls);

    s_now = 61.0;
    game_save_cloud_tick();
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"local\"}");
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(2, s_write_calls);
}

static void test_no_interval_uploads_every_change(void) {
    s_sync_write_ack = true;
    s_local = copy_text("local");
    s_live = copy_text("local");
    (void)game_save_cloud_boot_settled();
    complete_read(PLATFORM_SDK_CLOUD_EMPTY, NULL);
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(1, s_write_calls);

    s_now = 6.0;
    upload_local_and_settle("local-2", 2);
    TEST_ASSERT_EQUAL_INT(2, s_write_calls);
}

static void test_late_account_read_keeps_running_state_and_enters_conflict(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    (void)game_save_cloud_boot_settled();
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_STRING("local", s_live);
    TEST_ASSERT_EQUAL_STRING("local", s_local);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_cloud_state());
}

static void begin_running_conflict(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    (void)game_save_cloud_boot_settled();
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_cloud_state());
}

static void test_keep_local_rereads_the_pinned_remote_before_acknowledged_upload(void) {
    begin_running_conflict();
    TEST_ASSERT_TRUE(game_save_cloud_resolve(GAME_SAVE_KEEP_LOCAL));
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH, game_save_cloud_state());
    s_now = 5.0;
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(2, s_read_calls);
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    s_sync_write_ack = true;
    game_save_cloud_tick();
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_STRING("local", s_local);
    TEST_ASSERT_EQUAL_STRING("local", s_base);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_SYNCHRONIZED, game_save_cloud_state());
}

static void test_use_cloud_keeps_both_documents_when_local_replace_fails(void) {
    begin_running_conflict();
    s_blocking_write_ok = false;
    TEST_ASSERT_TRUE(game_save_cloud_resolve(GAME_SAVE_KEEP_REMOTE));
    TEST_ASSERT_FALSE(game_save_cloud_apply_remote_at_safe_point());
    TEST_ASSERT_EQUAL_STRING("local", s_live);
    TEST_ASSERT_EQUAL_STRING("local", s_local);
    TEST_ASSERT_EQUAL_STRING("account", game_save_cloud_conflict_remote_document());
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_cloud_state());
}

static void test_hung_read_retries_and_ignores_the_replaced_request(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    (void)game_save_cloud_boot_settled();
    const uint32_t first_read = s_read_id;
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    s_now = 5.0;
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(2, s_read_calls);
    TEST_ASSERT_NOT_EQUAL(first_read, s_read_id);
    complete_read_id(first_read, PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"stale\"}");
    TEST_ASSERT_EQUAL(PLATFORM_SDK_CLOUD_PENDING, platform_sdk_cloud_status());
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_STRING("local", s_live);
    TEST_ASSERT_EQUAL_STRING("account", game_save_cloud_conflict_remote_document());
}


static void test_auto_remote_adopts_before_game_start(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    s_policy_decision = GAME_SAVE_KEEP_REMOTE;
    (void)game_save_cloud_boot_settled();
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    TEST_ASSERT_TRUE(game_save_cloud_start(false));
    TEST_ASSERT_EQUAL_STRING("account", s_live);
    TEST_ASSERT_EQUAL_STRING("account", s_base);
}

static void test_auto_local_rereads_and_acknowledges_before_upload(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    s_policy_decision = GAME_SAVE_KEEP_LOCAL;
    (void)game_save_cloud_boot_settled();
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH, game_save_cloud_state());
    s_now = 5.0;
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(2, s_read_calls);
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    s_sync_write_ack = true;
    game_save_cloud_tick();
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_STRING("local", s_base);
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_SYNCHRONIZED, game_save_cloud_state());
}

static void test_auto_remote_applies_only_at_the_safe_point(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    s_policy_decision = GAME_SAVE_KEEP_REMOTE;
    (void)game_save_cloud_boot_settled();
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_STRING("local", s_live);
    TEST_ASSERT_TRUE(game_save_cloud_apply_remote_at_safe_point());
    TEST_ASSERT_EQUAL_STRING("account", s_live);
    TEST_ASSERT_EQUAL_STRING("account", s_base);
}

static void test_new_local_state_cancels_auto_remote_before_apply(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    s_policy_decision = GAME_SAVE_KEEP_REMOTE;
    (void)game_save_cloud_boot_settled();
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    game_save_cloud_tick();
    TEST_ASSERT_TRUE(replace_text(&s_live, "local-changed"));
    TEST_ASSERT_FALSE(game_save_cloud_apply_remote_at_safe_point());
    TEST_ASSERT_EQUAL_STRING("local-changed", s_live);
    TEST_ASSERT_EQUAL_STRING("account", game_save_cloud_conflict_remote_document());
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_cloud_state());
}

static void test_elapsed_playtime_keeps_remote_auto_choice_at_safe_point(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    s_policy_decision = GAME_SAVE_KEEP_REMOTE;
    game_save_cloud_shutdown();
    cloud_save_init(test_policy, same_features, s_write_interval);
    (void)game_save_cloud_boot_settled();
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    game_save_cloud_tick();
    TEST_ASSERT_TRUE(replace_text(&s_live, "local-playtime-advanced"));
    TEST_ASSERT_TRUE(game_save_cloud_apply_remote_at_safe_point());
    TEST_ASSERT_EQUAL_STRING("account", s_live);
}

static void test_reversed_policy_cancels_auto_remote_after_elapsed_playtime(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    s_policy_decision = GAME_SAVE_KEEP_REMOTE;
    game_save_cloud_shutdown();
    cloud_save_init(test_policy, same_features, s_write_interval);
    (void)game_save_cloud_boot_settled();
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    game_save_cloud_tick();
    TEST_ASSERT_TRUE(replace_text(&s_live, "local-playtime-advanced"));
    s_policy_decision = GAME_SAVE_KEEP_LOCAL;
    TEST_ASSERT_FALSE(game_save_cloud_apply_remote_at_safe_point());
    TEST_ASSERT_EQUAL_STRING("local-playtime-advanced", s_live);
    TEST_ASSERT_EQUAL_STRING("account", game_save_cloud_conflict_remote_document());
    TEST_ASSERT_EQUAL(GAME_SAVE_SYNC_CONFLICT, game_save_cloud_state());
}

static void test_failed_local_read_blocks_later_boot_adoption(void) {
    s_live = copy_text("live");
    (void)game_save_cloud_boot_settled();
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    s_local = copy_text("local");
    TEST_ASSERT_FALSE(game_save_cloud_start(true));
    TEST_ASSERT_FALSE(game_save_cloud_apply_remote_at_safe_point());
    TEST_ASSERT_EQUAL_STRING("live", s_live);
}

static void test_start_is_idempotent(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    (void)game_save_cloud_boot_settled();
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    TEST_ASSERT_EQUAL_INT(1, s_read_calls);
}

static void test_policy_runs_only_when_documents_change(void) {
    s_local = copy_text("local");
    s_live = copy_text("local");
    (void)game_save_cloud_boot_settled();
    TEST_ASSERT_FALSE(game_save_cloud_start(false));
    complete_read(PLATFORM_SDK_CLOUD_READY, "{\"saved_at\":1,\"doc\":\"account\"}");
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(1, s_policy_calls);
    game_save_cloud_tick();
    TEST_ASSERT_EQUAL_INT(1, s_policy_calls);
}

static void test_failed_remote_replace_restores_live_snapshot(void) {
    begin_running_conflict();
    TEST_ASSERT_TRUE(replace_text(&s_live, "live-current"));
    s_blocking_write_ok = false;
    TEST_ASSERT_TRUE(game_save_cloud_resolve(GAME_SAVE_KEEP_REMOTE));
    TEST_ASSERT_FALSE(game_save_cloud_apply_remote_at_safe_point());
    TEST_ASSERT_EQUAL_STRING("live-current", s_live);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_failed_local_read_blocks_later_boot_adoption);
    RUN_TEST(test_start_is_idempotent);
    RUN_TEST(test_policy_runs_only_when_documents_change);
    RUN_TEST(test_failed_remote_replace_restores_live_snapshot);
    RUN_TEST(test_preboot_ready_remote_adopts_fresh_local);
    RUN_TEST(test_fresh_device_adopts_valid_account_document);
    RUN_TEST(test_failed_read_blocks_writes_and_retries_once_per_interval);
    RUN_TEST(test_synchronous_store_ack_commits_the_exact_snapshot);
    RUN_TEST(test_write_interval_holds_back_the_next_upload);
    RUN_TEST(test_no_interval_uploads_every_change);
    RUN_TEST(test_late_account_read_keeps_running_state_and_enters_conflict);
    RUN_TEST(test_keep_local_rereads_the_pinned_remote_before_acknowledged_upload);
    RUN_TEST(test_use_cloud_keeps_both_documents_when_local_replace_fails);
    RUN_TEST(test_hung_read_retries_and_ignores_the_replaced_request);
    RUN_TEST(test_auto_remote_adopts_before_game_start);
    RUN_TEST(test_auto_local_rereads_and_acknowledges_before_upload);
    RUN_TEST(test_auto_remote_applies_only_at_the_safe_point);
    RUN_TEST(test_new_local_state_cancels_auto_remote_before_apply);
    RUN_TEST(test_elapsed_playtime_keeps_remote_auto_choice_at_safe_point);
    RUN_TEST(test_reversed_policy_cancels_auto_remote_after_elapsed_playtime);
    return UNITY_END();
}
