#include "unity.h"

#include "features/leaderboard/leaderboard.h"
#include "features/leaderboard/leaderboard_portal.h"
#include "features/platform_sdk/platform_sdk.h"

#include <stdio.h>
#include <string.h>

/* The portal backend driven through a stub platform bridge: the platform
   facade is real, its backend hooks are scripted here. What is pinned is the
   refusal mapping and the row round trip, never a row count or a value. */

/* ---- fake host: the game's save, in memory ---- */

#define KV_MAX 32

typedef struct {
    char key[64];
    char value[64];
} kv_t;

static kv_t g_kv[KV_MAX];
static int g_kv_count;

static bool host_load(const char *key, char *out, size_t out_size, void *userdata) {
    (void)userdata;
    for (int i = 0; i < g_kv_count; i++) {
        if (strcmp(g_kv[i].key, key) == 0) {
            snprintf(out, out_size, "%s", g_kv[i].value);
            return true;
        }
    }
    return false;
}

static void host_store(const char *key, const char *value, void *userdata) {
    (void)userdata;
    for (int i = 0; i < g_kv_count; i++) {
        if (strcmp(g_kv[i].key, key) == 0) {
            snprintf(g_kv[i].value, sizeof g_kv[i].value, "%s", value);
            return;
        }
    }
    TEST_ASSERT_LESS_THAN_INT(KV_MAX, g_kv_count);
    snprintf(g_kv[g_kv_count].key, sizeof g_kv[g_kv_count].key, "%s", key);
    snprintf(g_kv[g_kv_count].value, sizeof g_kv[g_kv_count].value, "%s", value);
    g_kv_count++;
}

static const char *kv_get(const char *key) {
    for (int i = 0; i < g_kv_count; i++) {
        if (strcmp(g_kv[i].key, key) == 0) {
            return g_kv[i].value;
        }
    }
    return NULL;
}

/* ---- stub bridge: the platform backend a portal adapter would be ---- */

static struct {
    bool init_ready;
    bool has_hooks;
    platform_sdk_leaderboard_caps_t caps;
    int submit_calls;
    int fetch_calls;
    int open_calls;
    char last_board[32];
    int32_t last_scope;
    uint32_t last_value;
    char last_extra[LEADERBOARD_EXTRA_MAX];
    platform_sdk_result_t submit_answer;
    platform_sdk_result_t fetch_answer;
} g_bridge;

static bool bridge_init(void *ud) {
    (void)ud;
    return g_bridge.init_ready;
}

static platform_sdk_leaderboard_caps_t bridge_caps(const char *board_id, void *ud) {
    (void)board_id;
    (void)ud;
    return g_bridge.caps;
}

static platform_sdk_result_t bridge_submit(const char *board_id, int32_t scope, uint32_t value,
                                           const char *extra, void *ud) {
    (void)ud;
    g_bridge.submit_calls++;
    snprintf(g_bridge.last_board, sizeof g_bridge.last_board, "%s", board_id);
    g_bridge.last_scope = scope;
    g_bridge.last_value = value;
    snprintf(g_bridge.last_extra, sizeof g_bridge.last_extra, "%s", extra != NULL ? extra : "");
    return g_bridge.submit_answer;
}

static platform_sdk_result_t bridge_fetch(const char *board_id, int32_t scope, void *ud) {
    (void)ud;
    g_bridge.fetch_calls++;
    snprintf(g_bridge.last_board, sizeof g_bridge.last_board, "%s", board_id);
    g_bridge.last_scope = scope;
    return g_bridge.fetch_answer;
}

static platform_sdk_result_t bridge_open(const char *board_id, void *ud) {
    (void)ud;
    g_bridge.open_calls++;
    snprintf(g_bridge.last_board, sizeof g_bridge.last_board, "%s", board_id);
    return PLATFORM_SDK_RESULT_OK;
}

static void install_bridge(void) {
    platform_sdk_backend_t backend = {
        .init = bridge_init,
    };
    if (g_bridge.has_hooks) {
        backend.leaderboard_caps = bridge_caps;
        backend.leaderboard_submit = bridge_submit;
        backend.leaderboard_fetch = bridge_fetch;
        backend.leaderboard_open = bridge_open;
    }
    platform_sdk_set_backend(&backend, NULL);
}

/* ---- fixture ---- */

#define ALL (1u << LEADERBOARD_SCOPE_ALL_TIME)
#define DAY (1u << LEADERBOARD_SCOPE_UTC_DAY)

static const leaderboard_board_def_t k_boards[] = {
    {.id = "score", .sort = LEADERBOARD_SORT_DESC, .scopes = ALL | DAY, .portal_id = "planets"},
    {.id = "laps", .sort = LEADERBOARD_SORT_ASC, .scopes = ALL, .portal_id = NULL},
};

static leaderboard_board_t g_score;
static leaderboard_board_t g_laps;

static void boot_leaderboard(void) {
    const leaderboard_config_t config = {
        .host = {.load = host_load, .store = host_store, .userdata = NULL},
        .backend = leaderboard_portal_backend(),
        .backend_userdata = NULL,
        .boards = k_boards,
        .board_count = (int)(sizeof k_boards / sizeof k_boards[0]),
    };
    leaderboard_init(&config);
    g_score = leaderboard_board("score");
    g_laps = leaderboard_board("laps");
}

/* Yandex-shaped stub: reads anonymous, writes behind a login. */
static void boot(void) {
    install_bridge();
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_OK, platform_sdk_init());
    boot_leaderboard();
}

void setUp(void) {
    platform_sdk_reset_for_tests();
    leaderboard_reset_for_tests();
    memset(g_kv, 0, sizeof g_kv);
    g_kv_count = 0;
    memset(&g_bridge, 0, sizeof g_bridge);
    g_bridge.init_ready = true;
    g_bridge.has_hooks = true;
    g_bridge.caps = (platform_sdk_leaderboard_caps_t){.can_read = true, .can_write = true, .needs_login = true};
    g_bridge.submit_answer = PLATFORM_SDK_RESULT_OK;
    g_bridge.fetch_answer = PLATFORM_SDK_RESULT_OK;
}

void tearDown(void) {
    leaderboard_shutdown();
    platform_sdk_destroy();
}

/* ---- capabilities ---- */

void test_caps_come_from_the_portal_and_only_the_all_time_scope_is_offered(void) {
    boot();
    const leaderboard_caps_t caps = leaderboard_caps(g_score);
    TEST_ASSERT_TRUE(caps.can_read);
    TEST_ASSERT_TRUE(caps.can_write);
    TEST_ASSERT_TRUE(caps.needs_login);
    TEST_ASSERT_FALSE(caps.native_popup);
    TEST_ASSERT_EQUAL_UINT32(ALL, caps.scopes);
    TEST_ASSERT_EQUAL_INT(1, leaderboard_ui_state(g_score).tab_count);
}

void test_a_board_without_a_portal_id_has_no_capability(void) {
    boot();
    TEST_ASSERT_FALSE(leaderboard_available(g_laps));
    leaderboard_submit(g_laps, LEADERBOARD_SCOPE_ALL_TIME, 90, "");
    TEST_ASSERT_EQUAL_INT(0, g_bridge.submit_calls);
}

void test_a_platform_without_a_board_api_withdraws_everything(void) {
    g_bridge.has_hooks = false;
    boot();
    TEST_ASSERT_FALSE(leaderboard_available(g_score));
    TEST_ASSERT_FALSE(leaderboard_open_native(g_score));
}

void test_playgama_style_caps_are_read_live(void) {
    boot();
    g_bridge.caps = (platform_sdk_leaderboard_caps_t){.can_write = true, .native_popup = true};
    const leaderboard_caps_t caps = leaderboard_caps(g_score);
    TEST_ASSERT_FALSE(caps.can_read);
    TEST_ASSERT_TRUE(caps.native_popup);
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_launcher);
    TEST_ASSERT_TRUE(leaderboard_open_native(g_score));
    TEST_ASSERT_EQUAL_STRING("planets", g_bridge.last_board);

    g_bridge.caps = (platform_sdk_leaderboard_caps_t){0};
    TEST_ASSERT_FALSE(leaderboard_available(g_score));
}

/* ---- submit ---- */

void test_submit_reaches_the_portal_by_its_id_and_an_accepted_score_is_remembered(void) {
    boot();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 120, "skin=7;");
    TEST_ASSERT_EQUAL_INT(1, g_bridge.submit_calls);
    TEST_ASSERT_EQUAL_STRING("planets", g_bridge.last_board);
    TEST_ASSERT_EQUAL_INT32(LEADERBOARD_SCOPE_ALL_TIME, g_bridge.last_scope);
    TEST_ASSERT_EQUAL_UINT32(120, g_bridge.last_value);
    TEST_ASSERT_EQUAL_STRING("skin=7;", g_bridge.last_extra);

    platform_sdk_backend_complete_leaderboard_submit("planets", LEADERBOARD_SCOPE_ALL_TIME, PLATFORM_SDK_LEADERBOARD_OK);
    TEST_ASSERT_EQUAL_STRING("120", kv_get("lb.sent.score.all_time"));
}

void test_the_day_scope_never_reaches_a_portal(void) {
    boot();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 5, "");
    TEST_ASSERT_EQUAL_INT(0, g_bridge.submit_calls);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_write);
}

void test_unsupported_withdraws_only_the_capability_that_refused(void) {
    boot();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, "");
    platform_sdk_backend_complete_leaderboard_submit("planets", LEADERBOARD_SCOPE_ALL_TIME,
                                                     PLATFORM_SDK_LEADERBOARD_UNSUPPORTED);
    leaderboard_caps_t caps = leaderboard_caps(g_score);
    TEST_ASSERT_FALSE(caps.can_write);
    TEST_ASSERT_TRUE(caps.can_read);
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_launcher);

    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_INT(1, g_bridge.fetch_calls);
    platform_sdk_backend_complete_leaderboard_fetch("planets", LEADERBOARD_SCOPE_ALL_TIME,
                                                    PLATFORM_SDK_LEADERBOARD_UNSUPPORTED, NULL);
    caps = leaderboard_caps(g_score);
    TEST_ASSERT_FALSE(caps.can_read);
    TEST_ASSERT_FALSE(leaderboard_available(g_score));
}

void test_needs_login_keeps_reading_alive_and_raises_the_login_flag(void) {
    boot();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, "");
    platform_sdk_backend_complete_leaderboard_submit("planets", LEADERBOARD_SCOPE_ALL_TIME,
                                                     PLATFORM_SDK_LEADERBOARD_NEEDS_LOGIN);
    const leaderboard_caps_t caps = leaderboard_caps(g_score);
    TEST_ASSERT_TRUE(caps.can_read);
    TEST_ASSERT_TRUE(caps.can_write);
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_login);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.needs_login);
    TEST_ASSERT_NULL(kv_get("lb.sent.score.all_time"));
}

void test_a_login_re_sends_the_score_the_portal_never_took(void) {
    boot();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, "");
    platform_sdk_backend_complete_leaderboard_submit("planets", LEADERBOARD_SCOPE_ALL_TIME,
                                                     PLATFORM_SDK_LEADERBOARD_NEEDS_LOGIN);
    TEST_ASSERT_EQUAL_INT(1, g_bridge.submit_calls);

    platform_sdk_backend_set_player(true, "Ada", "");
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(2, g_bridge.submit_calls);
    platform_sdk_backend_complete_leaderboard_submit("planets", LEADERBOARD_SCOPE_ALL_TIME, PLATFORM_SDK_LEADERBOARD_OK);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_login);
    TEST_ASSERT_EQUAL_STRING("10", kv_get("lb.sent.score.all_time"));
}

void test_rate_limited_and_failed_never_latch(void) {
    boot();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, "");
    platform_sdk_backend_complete_leaderboard_submit("planets", LEADERBOARD_SCOPE_ALL_TIME,
                                                     PLATFORM_SDK_LEADERBOARD_RATE_LIMITED);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_write);
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_INT(2, g_bridge.submit_calls);
    platform_sdk_backend_complete_leaderboard_submit("planets", LEADERBOARD_SCOPE_ALL_TIME,
                                                     PLATFORM_SDK_LEADERBOARD_FAILED);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_write);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_login);
}

void test_a_request_the_platform_never_started_does_not_latch(void) {
    boot();
    g_bridge.submit_answer = PLATFORM_SDK_RESULT_FAILED;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, "");
    TEST_ASSERT_EQUAL_INT(1, g_bridge.submit_calls);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_write);

    g_bridge.submit_answer = PLATFORM_SDK_RESULT_OK;
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_INT(2, g_bridge.submit_calls);
}

void test_a_pending_score_waits_for_the_portal_to_come_up(void) {
    g_bridge.init_ready = false;
    install_bridge();
    TEST_ASSERT_EQUAL_INT(PLATFORM_SDK_RESULT_NOT_READY, platform_sdk_init());
    boot_leaderboard();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 33, "");
    TEST_ASSERT_EQUAL_INT(0, g_bridge.submit_calls);
    TEST_ASSERT_FALSE(leaderboard_available(g_score));

    platform_sdk_backend_complete_init(true);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_bridge.submit_calls);
    TEST_ASSERT_EQUAL_UINT32(33, g_bridge.last_value);
}

/* ---- fetch ---- */

void test_rows_carry_name_avatar_and_the_extra_payload(void) {
    boot();
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_INT(1, g_bridge.fetch_calls);
    TEST_ASSERT_EQUAL_STRING("planets", g_bridge.last_board);

    const platform_sdk_leaderboard_entry_t top[] = {
        {.value = 900, .rank = 1, .name = "Ada", .avatar_url = "https://avatars.example/a/small", .extra = "skin=1;"},
        {.value = 800, .rank = 2, .name = "", .avatar_url = "", .extra = ""},
    };
    const platform_sdk_leaderboard_entry_t around[] = {
        {.value = 100, .rank = 42, .you = true, .name = "Me", .avatar_url = "", .extra = "skin=7;"},
    };
    const platform_sdk_leaderboard_page_t page = {
        .top = top,
        .top_count = 2,
        .around = around,
        .around_count = 1,
        .has_player = true,
        .player_rank = 42,
        .player_value = 100,
    };
    platform_sdk_backend_complete_leaderboard_fetch("planets", LEADERBOARD_SCOPE_ALL_TIME, PLATFORM_SDK_LEADERBOARD_OK, &page);

    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
    TEST_ASSERT_EQUAL_INT(2, view.top_count);
    TEST_ASSERT_EQUAL_UINT32(900, view.top[0].value);
    TEST_ASSERT_EQUAL_INT(1, view.top[0].place);
    TEST_ASSERT_EQUAL_STRING("Ada", view.top[0].name);
    TEST_ASSERT_EQUAL_STRING("https://avatars.example/a/small", view.top[0].avatar);
    char skin[8];
    TEST_ASSERT_TRUE(leaderboard_extra_get(view.top[0].extra, "skin", skin, sizeof skin));
    TEST_ASSERT_EQUAL_STRING("1", skin);
    TEST_ASSERT_EQUAL_STRING("", view.top[1].name);
    TEST_ASSERT_EQUAL_INT(1, view.around_count);
    TEST_ASSERT_TRUE(view.around[0].you);
    TEST_ASSERT_TRUE(leaderboard_extra_get(view.around[0].extra, "skin", skin, sizeof skin));
    TEST_ASSERT_EQUAL_STRING("7", skin);
    TEST_ASSERT_EQUAL_INT(42, view.place);
    TEST_ASSERT_EQUAL_UINT32(100, view.value);
}

void test_an_oversized_payload_or_url_is_dropped_not_cut(void) {
    boot();
    leaderboard_refresh_now(g_score);
    char long_extra[LEADERBOARD_EXTRA_MAX + 8];
    memset(long_extra, 'x', sizeof long_extra - 1);
    long_extra[sizeof long_extra - 1] = '\0';
    char long_url[LEADERBOARD_URL_MAX + 8];
    memset(long_url, 'u', sizeof long_url - 1);
    long_url[sizeof long_url - 1] = '\0';
    const platform_sdk_leaderboard_entry_t top[] = {
        {.value = 5, .rank = 1, .name = "N", .avatar_url = long_url, .extra = long_extra},
    };
    const platform_sdk_leaderboard_page_t page = {.top = top, .top_count = 1};
    platform_sdk_backend_complete_leaderboard_fetch("planets", LEADERBOARD_SCOPE_ALL_TIME, PLATFORM_SDK_LEADERBOARD_OK, &page);

    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_EQUAL_INT(1, view.top_count);
    TEST_ASSERT_EQUAL_STRING("", view.top[0].extra);
    TEST_ASSERT_EQUAL_STRING("", view.top[0].avatar);
    TEST_ASSERT_EQUAL_UINT32(5, view.top[0].value);
}

void test_a_completion_for_an_unknown_portal_board_is_ignored(void) {
    boot();
    leaderboard_refresh_now(g_score);
    platform_sdk_backend_complete_leaderboard_fetch("someone-elses-board", LEADERBOARD_SCOPE_ALL_TIME,
                                                    PLATFORM_SDK_LEADERBOARD_UNSUPPORTED, NULL);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_read);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_FALSE(view.loaded);
}

void test_refresh_waits_for_portal_readiness(void) {
    g_bridge.init_ready = false;
    install_bridge();
    (void)platform_sdk_init();
    boot_leaderboard();
    leaderboard_refresh_now(g_score);
    platform_sdk_backend_complete_init(true);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_bridge.fetch_calls);
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).loading);
}

void test_fetch_retries_stop_at_error_and_manual_refresh_recovers(void) {
    boot();
    int64_t now = 1788177600;
    leaderboard_set_now_for_tests(now);
    leaderboard_refresh_now(g_score);
    int attempts = 0;
    while (!leaderboard_ui_state(g_score).show_retry && attempts++ < 10) {
        platform_sdk_backend_complete_leaderboard_fetch("planets", 0, PLATFORM_SDK_LEADERBOARD_FAILED, NULL);
        now += 60;
        leaderboard_set_now_for_tests(now);
        leaderboard_update();
    }
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_retry);
    TEST_ASSERT_GREATER_THAN_INT(1, g_bridge.fetch_calls);
    const int calls = g_bridge.fetch_calls;
    leaderboard_set_now_for_tests(now + 3600);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(calls, g_bridge.fetch_calls);
    leaderboard_refresh_now(g_score);
    const platform_sdk_leaderboard_page_t page = {0};
    platform_sdk_backend_complete_leaderboard_fetch("planets", 0, PLATFORM_SDK_LEADERBOARD_OK, &page);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
    TEST_ASSERT_FALSE(view.error);
}

void test_failed_submit_retries_the_pending_best_without_new_score(void) {
    boot();
    leaderboard_set_now_for_tests(1788177600);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 100, "");
    platform_sdk_backend_complete_leaderboard_submit("planets", 0, PLATFORM_SDK_LEADERBOARD_FAILED);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_bridge.submit_calls);
    leaderboard_set_now_for_tests(1788177660);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(2, g_bridge.submit_calls);
    platform_sdk_backend_complete_leaderboard_submit("planets", 0, PLATFORM_SDK_LEADERBOARD_OK);
    TEST_ASSERT_EQUAL_STRING("100", kv_get("lb.sent.score.all_time"));
}

void test_readless_portal_does_not_leave_a_refresh_loading(void) {
    g_bridge.caps.can_read = false;
    g_bridge.caps.native_popup = true;
    boot();
    leaderboard_refresh_now(g_score);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(0, g_bridge.fetch_calls);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).loading);
}

void test_submit_success_does_not_hide_an_exhausted_read_error(void) {
    boot();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 100, "");
    leaderboard_refresh_now(g_score);
    for (int attempt = 0; attempt < 10 && !leaderboard_ui_state(g_score).show_retry; attempt++) {
        platform_sdk_backend_complete_leaderboard_fetch("planets", 0, PLATFORM_SDK_LEADERBOARD_FAILED, NULL);
        leaderboard_set_now_for_tests(1788177600 + (attempt + 1) * 60);
        leaderboard_update();
    }
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_retry);
    platform_sdk_backend_complete_leaderboard_submit("planets", 0, PLATFORM_SDK_LEADERBOARD_OK);
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_retry);
}

void test_login_refused_read_resumes_on_auth_change(void) {
    boot();
    leaderboard_refresh_now(g_score);
    platform_sdk_backend_complete_leaderboard_fetch("planets", 0, PLATFORM_SDK_LEADERBOARD_NEEDS_LOGIN, NULL);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_bridge.fetch_calls);
    leaderboard_backend_auth_changed();
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(2, g_bridge.fetch_calls);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_refresh_waits_for_portal_readiness);
    RUN_TEST(test_readless_portal_does_not_leave_a_refresh_loading);
    RUN_TEST(test_login_refused_read_resumes_on_auth_change);
    RUN_TEST(test_submit_success_does_not_hide_an_exhausted_read_error);
    RUN_TEST(test_fetch_retries_stop_at_error_and_manual_refresh_recovers);
    RUN_TEST(test_failed_submit_retries_the_pending_best_without_new_score);
    RUN_TEST(test_caps_come_from_the_portal_and_only_the_all_time_scope_is_offered);
    RUN_TEST(test_a_board_without_a_portal_id_has_no_capability);
    RUN_TEST(test_a_platform_without_a_board_api_withdraws_everything);
    RUN_TEST(test_playgama_style_caps_are_read_live);
    RUN_TEST(test_submit_reaches_the_portal_by_its_id_and_an_accepted_score_is_remembered);
    RUN_TEST(test_the_day_scope_never_reaches_a_portal);
    RUN_TEST(test_unsupported_withdraws_only_the_capability_that_refused);
    RUN_TEST(test_needs_login_keeps_reading_alive_and_raises_the_login_flag);
    RUN_TEST(test_a_login_re_sends_the_score_the_portal_never_took);
    RUN_TEST(test_rate_limited_and_failed_never_latch);
    RUN_TEST(test_a_request_the_platform_never_started_does_not_latch);
    RUN_TEST(test_a_pending_score_waits_for_the_portal_to_come_up);
    RUN_TEST(test_rows_carry_name_avatar_and_the_extra_payload);
    RUN_TEST(test_an_oversized_payload_or_url_is_dropped_not_cut);
    RUN_TEST(test_a_completion_for_an_unknown_portal_board_is_ignored);
    return UNITY_END();
}
