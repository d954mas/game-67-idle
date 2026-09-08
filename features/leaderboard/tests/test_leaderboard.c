#include "unity.h"

#include "features/leaderboard/leaderboard.h"
#include "features/leaderboard/leaderboard_mock.h"

#include <stdio.h>
#include <string.h>

/* The facade against the mock backend: what a screen may show per capability
   shape, the four refusal kinds, coalescing under both sort orders, the host
   seam across a relaunch, and the extra codec. No row count, cadence or copy
   is pinned. */

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

/* ---- fixture ---- */

#define ALL (1u << LEADERBOARD_SCOPE_ALL_TIME)
#define DAY (1u << LEADERBOARD_SCOPE_UTC_DAY)

/* 2026-08-31 12:00:00 UTC */
#define NOON_2026_08_31 1788177600LL

static const leaderboard_board_def_t k_boards[] = {
    {.id = "score", .sort = LEADERBOARD_SORT_DESC, .scopes = ALL | DAY, .portal_id = "score"},
    {.id = "laps", .sort = LEADERBOARD_SORT_ASC, .scopes = ALL, .portal_id = NULL},
};

static leaderboard_mock_t g_mock;
static leaderboard_board_t g_score;
static leaderboard_board_t g_laps;

/* Boots the pack on the live mock and the live host store; the host store
   survives across boots so a test can simulate a relaunch. */
static void boot(void) {
    const leaderboard_config_t config = {
        .host = {.load = host_load, .store = host_store, .userdata = NULL},
        .backend = leaderboard_mock_backend(),
        .backend_userdata = &g_mock,
        .boards = k_boards,
        .board_count = (int)(sizeof k_boards / sizeof k_boards[0]),
    };
    leaderboard_init(&config);
    g_score = leaderboard_board("score");
    g_laps = leaderboard_board("laps");
}

void setUp(void) {
    leaderboard_reset_for_tests();
    leaderboard_set_now_for_tests(NOON_2026_08_31);
    memset(g_kv, 0, sizeof g_kv);
    g_kv_count = 0;
    leaderboard_mock_defaults(&g_mock);
    boot();
}

void tearDown(void) {
    leaderboard_shutdown();
}

/* ---- boards ---- */

void test_board_lookup(void) {
    TEST_ASSERT_EQUAL_INT(2, leaderboard_board_count());
    TEST_ASSERT_EQUAL_UINT8(0, g_score.index);
    TEST_ASSERT_EQUAL_UINT8(1, g_laps.index);
    TEST_ASSERT_EQUAL_UINT8(LEADERBOARD_INVALID_BOARD, leaderboard_board("nope").index);
    TEST_ASSERT_EQUAL_STRING("laps", leaderboard_board_def(g_laps)->id);
}

void test_invalid_board_is_inert(void) {
    const leaderboard_board_t none = leaderboard_board("nope");
    leaderboard_view_t view;
    leaderboard_submit(none, LEADERBOARD_SCOPE_ALL_TIME, 10, NULL);
    leaderboard_refresh_now(none);
    TEST_ASSERT_EQUAL_INT(0, g_mock.submit_calls);
    TEST_ASSERT_EQUAL_INT(0, g_mock.fetch_calls);
    TEST_ASSERT_FALSE(leaderboard_available(none));
    TEST_ASSERT_FALSE(leaderboard_ui_state(none).show_launcher);
    TEST_ASSERT_FALSE(leaderboard_open_native(none));
    leaderboard_view_get(none, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_EQUAL_INT(-1, view.place);
    TEST_ASSERT_FALSE(view.loaded);
}

void test_backend_that_never_readies_is_inert(void) {
    leaderboard_shutdown();
    g_mock.init_fails = true;
    boot();
    TEST_ASSERT_FALSE(leaderboard_available(g_score));
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, NULL);
    TEST_ASSERT_EQUAL_INT(0, g_mock.submit_calls);
}

/* ---- ui state per capability shape ---- */

void test_ui_state_no_capabilities(void) {
    g_mock.caps = (leaderboard_caps_t){0};
    const leaderboard_ui_state_t ui = leaderboard_ui_state(g_score);
    TEST_ASSERT_FALSE(ui.show_launcher);
    TEST_ASSERT_EQUAL_INT(0, ui.tab_count);
    TEST_ASSERT_FALSE(leaderboard_available(g_score));
}

void test_ui_state_read_only_portal(void) {
    g_mock.caps = (leaderboard_caps_t){.can_read = true, .scopes = ALL | DAY};
    const leaderboard_ui_state_t ui = leaderboard_ui_state(g_score);
    TEST_ASSERT_TRUE(ui.show_launcher);
    TEST_ASSERT_EQUAL_INT(2, ui.tab_count);
    TEST_ASSERT_TRUE(leaderboard_available(g_score));
}

void test_ui_state_write_only_portal(void) {
    g_mock.caps = (leaderboard_caps_t){.can_write = true, .scopes = ALL};
    const leaderboard_ui_state_t ui = leaderboard_ui_state(g_score);
    TEST_ASSERT_FALSE(ui.show_launcher);
    TEST_ASSERT_TRUE(leaderboard_available(g_score));
}

void test_ui_state_native_popup_only(void) {
    g_mock.caps = (leaderboard_caps_t){.native_popup = true};
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_launcher);
    TEST_ASSERT_TRUE(leaderboard_open_native(g_score));
    TEST_ASSERT_EQUAL_INT(1, g_mock.open_calls);
}

void test_scopes_are_masked_by_the_manifest(void) {
    /* the portal offers both scopes; the laps board declared only all-time */
    TEST_ASSERT_EQUAL_INT(1, leaderboard_ui_state(g_laps).tab_count);
    TEST_ASSERT_EQUAL_UINT32(ALL, leaderboard_caps(g_laps).scopes);
    leaderboard_submit(g_laps, LEADERBOARD_SCOPE_UTC_DAY, 10, NULL);
    TEST_ASSERT_EQUAL_INT(0, g_mock.submit_calls);
}

/* ---- refusal kinds ---- */

void test_write_refusal_withdraws_writing_only(void) {
    g_mock.submit_result = LEADERBOARD_RESULT_UNSUPPORTED;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    leaderboard_caps_t caps = leaderboard_caps(g_score);
    TEST_ASSERT_FALSE(caps.can_write);
    TEST_ASSERT_TRUE(caps.can_read);
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_launcher);
    /* latched: a better score no longer reaches the backend */
    g_mock.submit_result = LEADERBOARD_RESULT_OK;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 20, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    /* reading the board survives */
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_INT(2, g_mock.fetch_calls);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
    /* and the latch is per board */
    caps = leaderboard_caps(g_laps);
    TEST_ASSERT_TRUE(caps.can_write);
    leaderboard_submit(g_laps, LEADERBOARD_SCOPE_ALL_TIME, 90, NULL);
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
}

void test_read_refusal_withdraws_reading_only(void) {
    g_mock.fetch_result = LEADERBOARD_RESULT_UNSUPPORTED;
    leaderboard_refresh_now(g_score);
    const leaderboard_caps_t caps = leaderboard_caps(g_score);
    TEST_ASSERT_FALSE(caps.can_read);
    TEST_ASSERT_TRUE(caps.can_write);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_launcher);
    const int fetches = g_mock.fetch_calls;
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_INT(fetches, g_mock.fetch_calls);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
}

void test_needs_login_raises_the_flag_without_latching(void) {
    g_mock.submit_result = LEADERBOARD_RESULT_NEEDS_LOGIN;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, NULL);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.needs_login);
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_login);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_write);
    TEST_ASSERT_FALSE(view.error);
    TEST_ASSERT_EQUAL_UINT32(10, view.value);
    /* the player logs in: the refused value goes out again and the flag drops */
    g_mock.submit_result = LEADERBOARD_RESULT_OK;
    leaderboard_backend_auth_changed();
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
    TEST_ASSERT_EQUAL_UINT32(10, g_mock.last_submit_value);
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_FALSE(view.needs_login);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_login);
}

void test_needs_login_on_fetch_keeps_the_real_top(void) {
    g_mock.fetch_result = LEADERBOARD_RESULT_NEEDS_LOGIN;
    g_mock.row_count = 3;
    leaderboard_refresh_now(g_score);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
    TEST_ASSERT_EQUAL_INT(3, view.top_count);
    TEST_ASSERT_TRUE(view.needs_login);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_read);
}

void test_rate_limited_changes_nothing_and_retries_later(void) {
    g_mock.submit_result = LEADERBOARD_RESULT_RATE_LIMITED;
    g_mock.fetch_result = LEADERBOARD_RESULT_RATE_LIMITED;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, NULL);
    leaderboard_refresh_now(g_score);
    const leaderboard_caps_t caps = leaderboard_caps(g_score);
    TEST_ASSERT_TRUE(caps.can_write);
    TEST_ASSERT_TRUE(caps.can_read);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_retry);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_login);
    /* the pending value is still pending: the next trigger sends it again */
    g_mock.submit_result = LEADERBOARD_RESULT_OK;
    const int submits = g_mock.submit_calls;
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_INT(submits + 1, g_mock.submit_calls);
    TEST_ASSERT_EQUAL_UINT32(10, g_mock.last_submit_value);
}

void test_failed_recovers_and_spent_retries_offer_manual_retry(void) {
    g_mock.submit_result = LEADERBOARD_RESULT_FAILED;
    g_mock.fetch_result = LEADERBOARD_RESULT_FAILED;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, NULL);
    /* one failure is not an outage */
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_retry);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_write);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_read);
    /* the backend's own retry cadence keeps failing; the budget is a design
       knob, only that it exists and is finite is asserted */
    int rounds = 0;
    while (!leaderboard_ui_state(g_score).show_retry && rounds < 64) {
        leaderboard_backend_complete_fetch(g_score, LEADERBOARD_SCOPE_ALL_TIME, NULL,
                                           LEADERBOARD_RESULT_FAILED);
        rounds++;
    }
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_retry);
    TEST_ASSERT_TRUE(rounds > 0);
    TEST_ASSERT_TRUE(rounds < 64);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_write);
    /* the player asking again restarts the budget even while the backend still fails */
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_retry);
    /* a manual retry that succeeds clears the error and delivers the value */
    g_mock.submit_result = LEADERBOARD_RESULT_OK;
    g_mock.fetch_result = LEADERBOARD_RESULT_OK;
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_retry);
    TEST_ASSERT_EQUAL_UINT32(10, g_mock.last_submit_value);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_FALSE(view.error);
    TEST_ASSERT_TRUE(view.loaded);
}

/* ---- coalescing ---- */

void test_coalescing_desc_keeps_the_higher_value(void) {
    g_mock.defer = true;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, "tag=a;");
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    /* worse and better values arrive while the first is in flight */
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 5, NULL);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 20, "tag=b;");
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 15, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    leaderboard_update(); /* completes 10 */
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
    TEST_ASSERT_EQUAL_UINT32(20, g_mock.last_submit_value);
    TEST_ASSERT_EQUAL_STRING("tag=b;", g_mock.last_submit_extra);
    leaderboard_update(); /* completes 20 */
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
    /* equal is not better */
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 20, NULL);
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_EQUAL_UINT32(20, view.value);
}

void test_coalescing_asc_keeps_the_lower_value(void) {
    leaderboard_submit(g_laps, LEADERBOARD_SCOPE_ALL_TIME, 100, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    leaderboard_submit(g_laps, LEADERBOARD_SCOPE_ALL_TIME, 150, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    leaderboard_submit(g_laps, LEADERBOARD_SCOPE_ALL_TIME, 50, NULL);
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
    TEST_ASSERT_EQUAL_UINT32(50, g_mock.last_submit_value);
}

void test_scopes_are_separate_counters(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 100, NULL);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 3, NULL);
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
    TEST_ASSERT_EQUAL_INT(LEADERBOARD_SCOPE_UTC_DAY, g_mock.last_submit_scope);
    TEST_ASSERT_EQUAL_UINT32(3, g_mock.last_submit_value);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_EQUAL_UINT32(100, view.value);
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_UTC_DAY, &view);
    TEST_ASSERT_EQUAL_UINT32(3, view.value);
}

/* ---- host seam ---- */

void test_accepted_value_is_not_resent_after_relaunch(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    TEST_ASSERT_EQUAL_STRING("10", kv_get("lb.sent.score.all_time"));
    leaderboard_shutdown();
    boot();
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 10, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 11, NULL);
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
}

void test_unaccepted_day_value_is_resent_at_boot(void) {
    g_mock.defer = true;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 7, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    /* the tab closes before the portal answers */
    leaderboard_shutdown();
    g_mock.defer = false;
    boot();
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
    TEST_ASSERT_EQUAL_INT(LEADERBOARD_SCOPE_UTC_DAY, g_mock.last_submit_scope);
    TEST_ASSERT_EQUAL_UINT32(7, g_mock.last_submit_value);
    /* and once accepted, another relaunch stays quiet */
    leaderboard_shutdown();
    boot();
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
}

void test_player_id_is_generated_once_and_kept_by_the_host(void) {
    char first[LEADERBOARD_PLAYER_ID_MAX];
    snprintf(first, sizeof first, "%s", leaderboard_player_id());
    TEST_ASSERT_EQUAL_INT(36, (int)strlen(first));
    TEST_ASSERT_EQUAL_STRING(first, kv_get("lb.id"));
    leaderboard_shutdown();
    boot();
    TEST_ASSERT_EQUAL_STRING(first, leaderboard_player_id());
}

/* ---- day ---- */

void test_day_rollover_resets_the_day_scope(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 5, NULL);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_UTC_DAY, &view);
    TEST_ASSERT_EQUAL_UINT32(5, view.value);
    TEST_ASSERT_TRUE(strlen(leaderboard_day_label()) > 0);
    TEST_ASSERT_EQUAL_INT64(43200, leaderboard_seconds_to_day_reset());

    leaderboard_set_now_for_tests(NOON_2026_08_31 + 86400);
    leaderboard_update();
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_UTC_DAY, &view);
    TEST_ASSERT_EQUAL_UINT32(0, view.value);
    TEST_ASSERT_EQUAL_STRING("20260901", kv_get("lb.day"));
    /* yesterday's accepted value does not block today's first score */
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 1, NULL);
    TEST_ASSERT_EQUAL_UINT32(1, g_mock.last_submit_value);
    /* the all-time scope is untouched by the rollover */
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 9, NULL);
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_EQUAL_UINT32(9, view.value);
}

void test_stale_day_counters_are_dropped_at_boot(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 5, NULL);
    leaderboard_shutdown();
    leaderboard_set_now_for_tests(NOON_2026_08_31 + 86400);
    boot();
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_UTC_DAY, &view);
    TEST_ASSERT_EQUAL_UINT32(0, view.value);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 2, NULL);
    TEST_ASSERT_EQUAL_UINT32(2, g_mock.last_submit_value);
}

void test_rollover_without_a_submit_forgets_yesterdays_sent_value(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 5, NULL);
    leaderboard_set_now_for_tests(NOON_2026_08_31 + 86400);
    leaderboard_update();
    leaderboard_shutdown();
    boot();
    /* a smaller value than yesterday's is today's first score, not a downgrade */
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 3, NULL);
    TEST_ASSERT_EQUAL_INT(LEADERBOARD_SCOPE_UTC_DAY, g_mock.last_submit_scope);
    TEST_ASSERT_EQUAL_UINT32(3, g_mock.last_submit_value);
}

void test_server_day_anchors_the_clock(void) {
    /* the server is already on the next day: rollover happens at its midnight */
    snprintf(g_mock.day, sizeof g_mock.day, "%s", "20260901");
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_STRING("20260901", kv_get("lb.day"));
    TEST_ASSERT_EQUAL_INT64(43200, leaderboard_seconds_to_day_reset());
    /* a server behind the local clock does not wind the day back */
    snprintf(g_mock.day, sizeof g_mock.day, "%s", "20260830");
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_STRING("20260901", kv_get("lb.day"));
}

/* ---- pages ---- */

void test_fetch_fills_the_view(void) {
    g_mock.row_count = 3;
    for (int i = 0; i < 3; i++) {
        memset(&g_mock.rows[i], 0, sizeof g_mock.rows[i]);
        g_mock.rows[i].value = (uint32_t)(30 - 10 * i);
        g_mock.rows[i].place = i + 1;
    }
    g_mock.rows[1].you = true;
    g_mock.has_player = true;
    g_mock.player_place = 2;
    g_mock.player_value = 20;
    leaderboard_refresh_now(g_score);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
    TEST_ASSERT_EQUAL_INT(3, view.top_count);
    TEST_ASSERT_TRUE(view.top[1].you);
    TEST_ASSERT_EQUAL_INT(2, view.place);
    TEST_ASSERT_EQUAL_UINT32(20, view.value);
    TEST_ASSERT_EQUAL_INT(0, view.around_count);
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).loading);
}

void test_loading_while_a_fetch_is_in_flight(void) {
    g_mock.defer = true;
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).loading);
    leaderboard_update();
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).loading);
}

void test_a_payload_is_kept_when_the_value_repeats_and_null_leaves_it_alone(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 20, "look=old;");
    TEST_ASSERT_EQUAL_STRING("look=old;", g_mock.last_submit_extra);
    /* a relaunch submits the standing best again, wearing what the player wears now */
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 20, "look=new;");
    TEST_ASSERT_EQUAL_INT(1, g_mock.submit_calls);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 25, NULL);
    TEST_ASSERT_EQUAL_INT(2, g_mock.submit_calls);
    TEST_ASSERT_EQUAL_STRING("look=new;", g_mock.last_submit_extra);
}

/* ---- extra codec ---- */

void test_extra_round_trip_and_replace(void) {
    char extra[LEADERBOARD_EXTRA_MAX] = "";
    char out[LEADERBOARD_EXTRA_MAX];
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "badge", "gold"));
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "tier", "7"));
    TEST_ASSERT_TRUE(leaderboard_extra_get(extra, "badge", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("gold", out);
    TEST_ASSERT_TRUE(leaderboard_extra_get(extra, "tier", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("7", out);
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "badge", "toy"));
    TEST_ASSERT_TRUE(leaderboard_extra_get(extra, "badge", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("toy", out);
    TEST_ASSERT_TRUE(leaderboard_extra_get(extra, "tier", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("7", out);
    TEST_ASSERT_FALSE(leaderboard_extra_get(extra, "absent", out, sizeof out));
}

void test_extra_truncates_at_the_budget(void) {
    char extra[16] = "";
    char before[16];
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "a", "b"));
    memcpy(before, extra, sizeof before);
    TEST_ASSERT_FALSE(leaderboard_extra_set(extra, sizeof extra, "long", "xxxxxxxxxxxxxxxxx"));
    TEST_ASSERT_EQUAL_MEMORY(before, extra, sizeof before);
    /* replacing may not overflow either */
    TEST_ASSERT_FALSE(leaderboard_extra_set(extra, sizeof extra, "a", "xxxxxxxxxxxxxxxxx"));
    TEST_ASSERT_EQUAL_MEMORY(before, extra, sizeof before);
    /* a value too big for the reader's buffer is refused, not cut */
    char tiny[2];
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "c", "dd"));
    TEST_ASSERT_FALSE(leaderboard_extra_get(extra, "c", tiny, sizeof tiny));
}

void test_extra_rejects_garbage(void) {
    char out[LEADERBOARD_EXTRA_MAX];
    char extra[LEADERBOARD_EXTRA_MAX] = "";
    TEST_ASSERT_FALSE(leaderboard_extra_get("garbage", "a", out, sizeof out));
    TEST_ASSERT_FALSE(leaderboard_extra_get("", "a", out, sizeof out));
    TEST_ASSERT_FALSE(leaderboard_extra_get("=;;=;", "a", out, sizeof out));
    TEST_ASSERT_FALSE(leaderboard_extra_get("a=b;junk", "c", out, sizeof out));
    TEST_ASSERT_TRUE(leaderboard_extra_get("a=b;junk", "a", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("b", out);
    TEST_ASSERT_FALSE(leaderboard_extra_get(NULL, "a", out, sizeof out));
    TEST_ASSERT_FALSE(leaderboard_extra_set(extra, sizeof extra, "a=b", "c"));
    TEST_ASSERT_FALSE(leaderboard_extra_set(extra, sizeof extra, "a", "b;c"));
    TEST_ASSERT_FALSE(leaderboard_extra_set(extra, sizeof extra, "", "c"));
    TEST_ASSERT_EQUAL_STRING("", extra);
}

void test_daily_submit_completion_cannot_acknowledge_a_new_day(void) {
    g_mock.defer = true;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 100, "");
    leaderboard_set_now_for_tests(NOON_2026_08_31 + 86400);
    leaderboard_update();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 1, "");
    leaderboard_update();
    TEST_ASSERT_EQUAL_STRING("1", kv_get("lb.sent.score.utc_day"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_daily_submit_completion_cannot_acknowledge_a_new_day);
    RUN_TEST(test_board_lookup);
    RUN_TEST(test_invalid_board_is_inert);
    RUN_TEST(test_backend_that_never_readies_is_inert);
    RUN_TEST(test_ui_state_no_capabilities);
    RUN_TEST(test_ui_state_read_only_portal);
    RUN_TEST(test_ui_state_write_only_portal);
    RUN_TEST(test_ui_state_native_popup_only);
    RUN_TEST(test_scopes_are_masked_by_the_manifest);
    RUN_TEST(test_write_refusal_withdraws_writing_only);
    RUN_TEST(test_read_refusal_withdraws_reading_only);
    RUN_TEST(test_needs_login_raises_the_flag_without_latching);
    RUN_TEST(test_needs_login_on_fetch_keeps_the_real_top);
    RUN_TEST(test_rate_limited_changes_nothing_and_retries_later);
    RUN_TEST(test_failed_recovers_and_spent_retries_offer_manual_retry);
    RUN_TEST(test_coalescing_desc_keeps_the_higher_value);
    RUN_TEST(test_coalescing_asc_keeps_the_lower_value);
    RUN_TEST(test_scopes_are_separate_counters);
    RUN_TEST(test_accepted_value_is_not_resent_after_relaunch);
    RUN_TEST(test_unaccepted_day_value_is_resent_at_boot);
    RUN_TEST(test_player_id_is_generated_once_and_kept_by_the_host);
    RUN_TEST(test_day_rollover_resets_the_day_scope);
    RUN_TEST(test_stale_day_counters_are_dropped_at_boot);
    RUN_TEST(test_rollover_without_a_submit_forgets_yesterdays_sent_value);
    RUN_TEST(test_server_day_anchors_the_clock);
    RUN_TEST(test_fetch_fills_the_view);
    RUN_TEST(test_loading_while_a_fetch_is_in_flight);
    RUN_TEST(test_a_payload_is_kept_when_the_value_repeats_and_null_leaves_it_alone);
    RUN_TEST(test_extra_round_trip_and_replace);
    RUN_TEST(test_extra_truncates_at_the_budget);
    RUN_TEST(test_extra_rejects_garbage);
    return UNITY_END();
}
