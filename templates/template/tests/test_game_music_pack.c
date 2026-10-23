#include "unity.h"
#include "game_music_pack.h"
#include <stdio.h>
#include <string.h>

static unsigned s_mounts, s_loads, s_unmounts;
static nt_pack_state_t s_state;
static nt_result_t s_mount_result, s_load_result;
static bool s_path_available;

void setUp(void) {
    s_mounts = s_loads = s_unmounts = 0U;
    s_state = NT_PACK_STATE_NONE;
    s_mount_result = s_load_result = NT_OK;
    s_path_available = true;
    game_music_pack_init();
}

void tearDown(void) { game_music_pack_shutdown(); }

bool game_asset_paths_resolve(const char *relative, char *out, size_t cap) {
    TEST_ASSERT_EQUAL_STRING("assets/music.ntpack", relative);
    if (!s_path_available) { return false; }
    return snprintf(out, cap, "%s", relative) > 0;
}

nt_result_t nt_resource_mount(nt_hash32_t id, int16_t priority) {
    (void)id;
    (void)priority;
    ++s_mounts;
    return s_mount_result;
}

nt_result_t nt_resource_load_auto(nt_hash32_t id, const char *path) {
    (void)id;
    TEST_ASSERT_EQUAL_STRING("assets/music.ntpack", path);
    ++s_loads;
    return s_load_result;
}

nt_pack_state_t nt_resource_pack_state(nt_hash32_t id) { (void)id; return s_state; }
void nt_resource_unmount(nt_hash32_t id) { (void)id; ++s_unmounts; }

static void test_music_waits_for_a_presented_frame_and_wanted_mix(void) {
    game_music_pack_update(true);
    TEST_ASSERT_EQUAL_UINT(0, s_mounts);
    game_music_pack_after_present();
    game_music_pack_update(false);
    TEST_ASSERT_EQUAL_UINT(0, s_mounts);
    game_music_pack_update(true);
    TEST_ASSERT_EQUAL_UINT(1, s_mounts);
    TEST_ASSERT_EQUAL_UINT(1, s_loads);
    s_state = NT_PACK_STATE_READY;
    game_music_pack_update(true);
    TEST_ASSERT_EQUAL_INT(NT_PACK_STATE_READY, game_music_pack_state());
    game_music_pack_after_present();
    game_music_pack_update(true);
    TEST_ASSERT_EQUAL_UINT(1, s_loads);
}

static void test_terminal_failure_retries_only_when_requested_again(void) {
    game_music_pack_after_present();
    game_music_pack_update(true);
    s_state = NT_PACK_STATE_FAILED;
    for (unsigned i = 0; i < 20; ++i) { game_music_pack_update(true); }
    TEST_ASSERT_EQUAL_UINT(1, s_mounts);
    TEST_ASSERT_EQUAL_INT(NT_PACK_STATE_FAILED, game_music_pack_state());
    game_music_pack_update(false);
    game_music_pack_update(true);
    TEST_ASSERT_EQUAL_UINT(2, s_mounts);
    TEST_ASSERT_EQUAL_UINT(1, s_unmounts);
    TEST_ASSERT_EQUAL_UINT(2, s_loads);
}

static void test_engine_transient_retry_does_not_remount(void) {
    game_music_pack_after_present();
    game_music_pack_update(true);
    s_state = NT_PACK_STATE_NONE;
    game_music_pack_update(true);
    TEST_ASSERT_EQUAL_UINT(1, s_mounts);
    TEST_ASSERT_EQUAL_UINT(1, s_loads);
}

static void test_failed_mount_is_bounded_and_shutdown_is_idempotent(void) {
    game_music_pack_after_present();
    s_mount_result = NT_ERR_INVALID_ARG;
    for (unsigned i = 0; i < 20; ++i) { game_music_pack_update(true); }
    TEST_ASSERT_EQUAL_UINT(1, s_mounts);
    TEST_ASSERT_EQUAL_UINT(0, s_loads);
    game_music_pack_shutdown();
    game_music_pack_shutdown();
    TEST_ASSERT_EQUAL_UINT(0, s_unmounts);
}

static void test_shutdown_cancels_pending_pack(void) {
    game_music_pack_after_present();
    game_music_pack_update(true);
    game_music_pack_shutdown();
    game_music_pack_shutdown();
    TEST_ASSERT_EQUAL_UINT(1, s_unmounts);
}

static void test_path_and_load_failures_wait_for_a_new_request(void) {
    game_music_pack_after_present();
    s_path_available = false;
    game_music_pack_update(true);
    TEST_ASSERT_EQUAL_UINT(0, s_mounts);
    TEST_ASSERT_EQUAL_INT(NT_PACK_STATE_FAILED, game_music_pack_state());
    s_path_available = true;
    game_music_pack_update(false);
    s_load_result = NT_ERR_INVALID_ARG;
    game_music_pack_update(true);
    for (unsigned i = 0; i < 20; ++i) { game_music_pack_update(true); }
    TEST_ASSERT_EQUAL_UINT(1, s_mounts);
    TEST_ASSERT_EQUAL_UINT(1, s_loads);
    TEST_ASSERT_EQUAL_INT(NT_PACK_STATE_FAILED, game_music_pack_state());
    game_music_pack_shutdown();
    TEST_ASSERT_EQUAL_UINT(1, s_unmounts);
}

static void test_failure_while_muted_recovers_on_the_first_unmute(void) {
    game_music_pack_after_present();
    game_music_pack_update(true);
    s_state = NT_PACK_STATE_REQUESTED;
    game_music_pack_update(false);
    s_state = NT_PACK_STATE_FAILED;
    game_music_pack_update(true);
    TEST_ASSERT_EQUAL_UINT(2, s_mounts);
    TEST_ASSERT_EQUAL_UINT(1, s_unmounts);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_music_waits_for_a_presented_frame_and_wanted_mix);
    RUN_TEST(test_terminal_failure_retries_only_when_requested_again);
    RUN_TEST(test_engine_transient_retry_does_not_remount);
    RUN_TEST(test_failed_mount_is_bounded_and_shutdown_is_idempotent);
    RUN_TEST(test_shutdown_cancels_pending_pack);
    RUN_TEST(test_path_and_load_failures_wait_for_a_new_request);
    RUN_TEST(test_failure_while_muted_recovers_on_the_first_unmute);
    return UNITY_END();
}
