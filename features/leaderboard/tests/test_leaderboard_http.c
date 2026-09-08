/* The CRT headers go before unity: its C11 `noreturn` macro breaks the
   __declspec form this CRT's stdlib.h still uses. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "features/leaderboard/leaderboard.h"
#include "features/leaderboard/leaderboard_http.h"

/* The http backend against a canned transport under a fake clock: what a
   request body carries, both scopes populating from one response, the local
   place re-estimate, the retry cadence and its back-off, and a malformed body
   counting as a failed request. Cadence numbers here are test inputs, not
   the game's. */

/* ---- fake host ---- */

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

/* ---- fake transport ---- */

#define URL_MAX 2048
#define BODY_MAX 8192

static struct {
    int request_calls;
    int released;
    bool refuse; /* request() cannot start */
    int live; /* requests started and not yet released */
    char last_url[URL_MAX];
    leaderboard_http_state_t answer; /* what the live request reports */
    char body[BODY_MAX];
    double mono;
    int64_t unix_now;
} g_net;

static uint32_t net_request(const char *url, void *ud) {
    (void)ud;
    if (g_net.refuse) {
        return 0;
    }
    g_net.request_calls++;
    g_net.live++;
    snprintf(g_net.last_url, sizeof g_net.last_url, "%s", url);
    return (uint32_t)g_net.request_calls;
}

static leaderboard_http_state_t net_state(uint32_t request, void *ud) {
    (void)ud;
    (void)request;
    return g_net.answer;
}

static uint8_t *net_take_body(uint32_t request, uint32_t *size, void *ud) {
    (void)ud;
    (void)request;
    const size_t n = strlen(g_net.body);
    *size = (uint32_t)n;
    if (n == 0) {
        return NULL;
    }
    uint8_t *copy = malloc(n);
    memcpy(copy, g_net.body, n);
    return copy;
}

static void net_release(uint32_t request, void *ud) {
    (void)ud;
    (void)request;
    TEST_ASSERT_GREATER_THAN_INT(0, g_net.live);
    g_net.live--;
    g_net.released++;
}

static double net_mono(void *ud) {
    (void)ud;
    return g_net.mono;
}

static int64_t net_unix(void *ud) {
    (void)ud;
    return g_net.unix_now;
}

static const leaderboard_http_transport_t k_net = {
    .request = net_request,
    .state = net_state,
    .take_body = net_take_body,
    .release = net_release,
    .monotonic_now = net_mono,
    .unix_now = net_unix,
    .userdata = NULL,
};

/* Decodes the `?d=` payload of the last request back into its JSON body. */
static void last_body(char *out, size_t cap) {
    const char *d = strstr(g_net.last_url, "?d=");
    TEST_ASSERT_NOT_NULL(d);
    d += 3;
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    uint32_t acc = 0;
    int bits = 0;
    size_t n = 0;
    for (; *d != '\0'; d++) {
        const char *at = strchr(alphabet, *d);
        TEST_ASSERT_NOT_NULL(at);
        acc = (acc << 6) | (uint32_t)(at - alphabet);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            TEST_ASSERT_LESS_THAN(cap, n + 1);
            out[n++] = (char)((acc >> bits) & 0xFF);
        }
    }
    out[n] = '\0';
    const char *key = "k3y";
    const size_t key_len = strlen(key);
    for (size_t i = 0; i < n; i++) {
        out[i] = (char)(out[i] ^ key[i % key_len]);
    }
}

static int count_of(const char *hay, const char *needle) {
    int n = 0;
    for (const char *p = strstr(hay, needle); p != NULL; p = strstr(p + 1, needle)) {
        n++;
    }
    return n;
}

/* ---- canned responses ---- */

#define ALL (1u << LEADERBOARD_SCOPE_ALL_TIME)
#define DAY (1u << LEADERBOARD_SCOPE_UTC_DAY)

/* 2026-08-31 12:00:00 UTC */
#define NOON_2026_08_31 1788177600LL
#define LOCAL_DAY "20260831"
#define NEXT_DAY "20260901"

/* Rows `top_rows` deep, values descending from `first` by `step`, the player
   on none of them, plus a bucket histogram past the top. */
static void canned_response(char *out, size_t cap, const char *root, int top_rows, unsigned first, unsigned step,
                            const char *day) {
    size_t len = 0;
    len += (size_t)snprintf(out + len, cap - len, "{\"success\":true,\"%s\":{\"all\":{\"top\":[", root);
    for (int i = 0; i < top_rows; i++) {
        len += (size_t)snprintf(out + len, cap - len, "%s{\"user_id\":\"all%d\",\"value\":%u,\"skin\":\"hat.s%d\",\"level\":%d}",
                                i > 0 ? "," : "", i, first - (unsigned)i * step, i, i % 7);
    }
    len += (size_t)snprintf(out + len, cap - len,
                            "],\"userPlace\":%d,\"interpolation\":[{\"value\":10,\"count\":900},{\"value\":50,\"count\":600},"
                            "{\"value\":100,\"count\":300},{\"value\":%u,\"count\":%d}]}",
                            top_rows + 1, first, top_rows);
    len += (size_t)snprintf(out + len, cap - len, ",\"day\":{\"top\":[");
    for (int i = 0; i < top_rows && i < 5; i++) {
        len += (size_t)snprintf(out + len, cap - len, "%s{\"user_id\":\"day%d\",\"value\":%u,\"skin\":\"hat.d%d\"}",
                                i > 0 ? "," : "", i, 40u - (unsigned)i * 5u, i);
    }
    len += (size_t)snprintf(out + len, cap - len, "],\"userPlace\":6,\"interpolation\":[],\"day\":\"%s\"}", day);
    len += (size_t)snprintf(out + len, cap - len, "}}");
    TEST_ASSERT_LESS_THAN(cap, len);
}

static void answer_ok_on(const char *root, int top_rows, const char *day) {
    g_net.answer = LEADERBOARD_HTTP_DONE;
    canned_response(g_net.body, sizeof g_net.body, root, top_rows, 2000, 100, day);
}

static void answer_ok(const char *root, int top_rows) {
    answer_ok_on(root, top_rows, LOCAL_DAY);
}

static void answer_failed(void) {
    g_net.answer = LEADERBOARD_HTTP_FAILED;
    g_net.body[0] = '\0';
}

static void answer_body(const char *body) {
    g_net.answer = LEADERBOARD_HTTP_DONE;
    snprintf(g_net.body, sizeof g_net.body, "%s", body);
}

/* ---- fixture ---- */

#define REPEAT_S 100.0
#define ERROR_S 5.0
#define FAST_TRIES 2

static const leaderboard_board_def_t k_boards[] = {
    {.id = "score", .sort = LEADERBOARD_SORT_DESC, .scopes = ALL | DAY, .portal_id = NULL},
};

static const leaderboard_board_def_t k_two_boards[] = {
    {.id = "score", .sort = LEADERBOARD_SORT_DESC, .scopes = ALL | DAY, .portal_id = NULL},
    {.id = "laps", .sort = LEADERBOARD_SORT_ASC, .scopes = ALL, .portal_id = "lap_time"},
};

static leaderboard_http_t g_http;
static leaderboard_board_t g_score;

static void boot_with(const leaderboard_board_def_t *boards, int count, const char *url) {
    g_http.config = (leaderboard_http_config_t){
        .url = url,
        .key = "k3y",
        .repeat_delay_s = REPEAT_S,
        .error_delay_s = ERROR_S,
        .error_fast_tries = FAST_TRIES,
        .transport = &k_net,
    };
    const leaderboard_config_t config = {
        .host = {.load = host_load, .store = host_store, .userdata = NULL},
        .backend = leaderboard_http_backend(),
        .backend_userdata = &g_http,
        .boards = boards,
        .board_count = count,
    };
    leaderboard_init(&config);
    g_score = leaderboard_board("score");
}

static void boot(void) {
    boot_with(k_boards, 1, "https://example.invalid/board");
}

void setUp(void) {
    leaderboard_reset_for_tests();
    leaderboard_set_now_for_tests(NOON_2026_08_31);
    memset(g_kv, 0, sizeof g_kv);
    g_kv_count = 0;
    memset(&g_net, 0, sizeof g_net);
    g_net.mono = 1000.0;
    g_net.unix_now = NOON_2026_08_31;
    g_net.answer = LEADERBOARD_HTTP_PENDING;
    memset(&g_http, 0, sizeof g_http);
    boot();
}

void tearDown(void) {
    leaderboard_shutdown();
}

/* Pumps until the wire is quiet: the first beat sends what was submitted, the
   next consumes the answer. */
static void settle(void) {
    leaderboard_update();
    if (g_net.live != 0) {
        leaderboard_update();
    }
    TEST_ASSERT_EQUAL_INT(0, g_net.live);
}

/* A submitted score leaves on the next pump, carrying every scope of its beat. */
static void send_beat(void) {
    leaderboard_update();
}

/* ---- capabilities ---- */

void test_dormant_without_an_endpoint(void) {
    leaderboard_shutdown();
    boot_with(k_boards, 1, "");
    TEST_ASSERT_FALSE(leaderboard_available(g_score));
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_launcher);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, NULL);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(0, g_net.request_calls);
}

void test_serves_both_scopes_anonymously(void) {
    const leaderboard_caps_t caps = leaderboard_caps(g_score);
    TEST_ASSERT_TRUE(caps.can_read);
    TEST_ASSERT_TRUE(caps.can_write);
    TEST_ASSERT_FALSE(caps.needs_login);
    TEST_ASSERT_FALSE(caps.native_popup);
    TEST_ASSERT_EQUAL_UINT32(ALL | DAY, caps.scopes);
    TEST_ASSERT_EQUAL_INT(2, leaderboard_ui_state(g_score).tab_count);
}

/* ---- request body ---- */

void test_first_update_polls_without_a_submit(void) {
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_net.request_calls);
    char body[1024];
    last_body(body, sizeof body);
    char expect[128];
    snprintf(expect, sizeof expect, "\"user_id\":\"%s\"", leaderboard_player_id());
    TEST_ASSERT_NOT_NULL(strstr(body, expect));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"score\":0"));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"score_day\":0"));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"day\":\"20260831\""));
    TEST_ASSERT_EQUAL_STRING_LEN("https://example.invalid/board?d=", g_net.last_url, 32);
}

void test_body_carries_every_scope_and_the_extra(void) {
    char extra[LEADERBOARD_EXTRA_MAX] = "";
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "skin", "hat.gummy"));
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "level", "5"));
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 3, extra);
    send_beat();
    TEST_ASSERT_EQUAL_INT(1, g_net.request_calls);
    answer_ok("score", 3);
    settle();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, extra);
    send_beat();
    TEST_ASSERT_EQUAL_INT(2, g_net.request_calls);
    char body[1024];
    last_body(body, sizeof body);
    TEST_ASSERT_NOT_NULL(strstr(body, "\"score\":7"));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"score_day\":3"));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"skin\":\"hat.gummy\""));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"level\":5")); /* a number, as the server demands */
    TEST_ASSERT_NULL(strstr(body, "\"level\":\"5\""));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"day\":\"" LOCAL_DAY "\""));
}


void test_relaunch_poll_keeps_extra_from_an_already_accepted_score(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, "skin=hat.old;");
    answer_ok("score", 3);
    settle();
    TEST_ASSERT_EQUAL_STRING("7", kv_get("lb.sent.score.all_time"));
    leaderboard_shutdown();
    boot();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, "skin=hat.current;");
    send_beat();
    char body[1024];
    last_body(body, sizeof body);
    TEST_ASSERT_NOT_NULL(strstr(body, "\"skin\":\"hat.current\""));
    settle();
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    bool found = false;
    for (int i = 0; i < view.top_count; i++) {
        if (view.top[i].you) {
            found = true;
            TEST_ASSERT_EQUAL_STRING("skin=hat.current;", view.top[i].extra);
        }
    }
    TEST_ASSERT_TRUE(found);
}

void test_poll_uses_latest_extra_across_scopes_even_when_score_is_unchanged(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, "skin=hat.all;");
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 3, "skin=hat.day;");
    answer_ok("score", 3);
    settle();
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_UTC_DAY, 3, "skin=hat.latest;");
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, NULL);
    g_net.mono += REPEAT_S + 1;
    send_beat();
    char body[1024];
    last_body(body, sizeof body);
    TEST_ASSERT_NOT_NULL(strstr(body, "\"skin\":\"hat.latest\""));
}

void test_ascending_board_ranks_smaller_values_first_and_preserves_ties(void) {
    leaderboard_shutdown();
    boot_with(k_two_boards + 1, 1, "https://example.invalid/board");
    const leaderboard_board_t laps = leaderboard_board("laps");
    leaderboard_submit(laps, LEADERBOARD_SCOPE_ALL_TIME, 20, NULL);
    answer_body("{\"success\":true,\"lap_time\":{\"all\":{\"top\":["
                "{\"user_id\":\"a\",\"value\":0},{\"user_id\":\"b\",\"value\":20},"
                "{\"user_id\":\"c\",\"value\":50}],\"userPlace\":4,\"interpolation\":[]},"
                "\"day\":{\"top\":[],\"userPlace\":0,\"interpolation\":[],\"day\":\"" LOCAL_DAY "\"}}}");
    settle();
    leaderboard_view_t view;
    leaderboard_view_get(laps, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
    TEST_ASSERT_EQUAL_INT(3, view.place);
    TEST_ASSERT_FALSE(view.top[1].you);
    TEST_ASSERT_TRUE(view.top[2].you);
    for (int i = 1; i < view.top_count; i++) {
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(view.top[i - 1].value, view.top[i].value);
    }
    leaderboard_submit(laps, LEADERBOARD_SCOPE_ALL_TIME, 5, NULL);
    leaderboard_view_get(laps, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_EQUAL_INT(2, view.place);
    TEST_ASSERT_TRUE(view.top[1].you);
}

void test_ascending_board_estimates_places_past_the_top_from_better_counts(void) {
    leaderboard_shutdown();
    boot_with(k_two_boards + 1, 1, "https://example.invalid/board");
    const leaderboard_board_t laps = leaderboard_board("laps");
    leaderboard_submit(laps, LEADERBOARD_SCOPE_ALL_TIME, 2000, NULL);
    size_t len = (size_t)snprintf(g_net.body, sizeof g_net.body,
                                "{\"success\":true,\"lap_time\":{\"all\":{\"top\":[");
    for (int i = 0; i < LEADERBOARD_TOP_MAX; i++) {
        len += (size_t)snprintf(g_net.body + len, sizeof g_net.body - len,
                               "%s{\"user_id\":\"r%d\",\"value\":%u}", i ? "," : "", i, 10u + (unsigned)i);
    }
    len += (size_t)snprintf(g_net.body + len, sizeof g_net.body - len,
                           "],\"userPlace\":1001,\"interpolation\":[{\"value\":100,\"count\":100},"
                           "{\"value\":1000,\"count\":1000}]},\"day\":{\"top\":[],\"userPlace\":0,"
                           "\"interpolation\":[],\"day\":\"" LOCAL_DAY "\"}}}");
    TEST_ASSERT_LESS_THAN(sizeof g_net.body, len);
    g_net.answer = LEADERBOARD_HTTP_DONE;
    settle();
    leaderboard_view_t view;
    leaderboard_view_get(laps, LEADERBOARD_SCOPE_ALL_TIME, &view);
    const int slow_place = view.place;
    TEST_ASSERT_GREATER_THAN_INT(1000, slow_place);
    leaderboard_submit(laps, LEADERBOARD_SCOPE_ALL_TIME, 500, NULL);
    leaderboard_view_get(laps, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_GREATER_THAN_INT(view.top_count, view.place);
    TEST_ASSERT_LESS_THAN_INT(slow_place, view.place);
    const int middle_place = view.place;
    send_beat();
    leaderboard_submit(laps, LEADERBOARD_SCOPE_ALL_TIME, 50, NULL);
    leaderboard_update();
    leaderboard_view_get(laps, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_GREATER_THAN_INT(view.top_count, view.place);
    TEST_ASSERT_LESS_THAN_INT(middle_place, view.place);
}

void test_server_day_anchors_the_clock_and_the_request(void) {
    leaderboard_update();
    answer_ok_on("score", 3, NEXT_DAY);
    settle();
    TEST_ASSERT_NOT_NULL(strstr(leaderboard_day_label(), "2026.09.01"));
    g_net.mono += REPEAT_S + 1;
    leaderboard_update();
    char body[1024];
    last_body(body, sizeof body);
    /* the server's day, not the local one, once a response anchored the clock */
    TEST_ASSERT_NOT_NULL(strstr(body, "\"day\":\"" NEXT_DAY "\""));
}

void test_extra_cannot_forge_the_wire_fields(void) {
    char extra[LEADERBOARD_EXTRA_MAX] = "";
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "user_id", "evil"));
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "score", "999"));
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "day", "19990101"));
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, extra);
    send_beat();
    char body[1024];
    last_body(body, sizeof body);
    TEST_ASSERT_NULL(strstr(body, "evil"));
    TEST_ASSERT_NULL(strstr(body, "999"));
    TEST_ASSERT_NULL(strstr(body, "1999"));
    TEST_ASSERT_EQUAL_INT(1, count_of(body, "\"score\":"));
    TEST_ASSERT_EQUAL_INT(1, count_of(body, "\"day\":"));
}

void test_second_board_uses_its_wire_name_and_declared_scopes(void) {
    leaderboard_shutdown();
    boot_with(k_two_boards, 2, "https://example.invalid/board");
    const leaderboard_board_t laps = leaderboard_board("laps");
    TEST_ASSERT_EQUAL_UINT32(ALL, leaderboard_caps(laps).scopes);
    leaderboard_submit(laps, LEADERBOARD_SCOPE_ALL_TIME, 61, NULL);
    send_beat();
    char body[1024];
    last_body(body, sizeof body);
    TEST_ASSERT_NOT_NULL(strstr(body, "\"lap_time\":61"));
    TEST_ASSERT_NULL(strstr(body, "lap_time_day"));
    answer_ok("lap_time", 3);
    leaderboard_update(); /* also starts the other board's first poll */
    leaderboard_view_t view;
    leaderboard_view_get(laps, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
    TEST_ASSERT_GREATER_THAN_INT(0, view.top_count);
}

/* ---- responses ---- */

void test_one_response_populates_both_scopes(void) {
    char extra[LEADERBOARD_EXTRA_MAX] = "";
    TEST_ASSERT_TRUE(leaderboard_extra_set(extra, sizeof extra, "skin", "hat.mine"));
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, extra);
    answer_ok("score", 3);
    settle();

    leaderboard_view_t all;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &all);
    TEST_ASSERT_TRUE(all.loaded);
    TEST_ASSERT_FALSE(all.error);
    TEST_ASSERT_GREATER_THAN_INT(0, all.top_count);
    TEST_ASSERT_EQUAL_UINT32(7, all.value);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, all.place);
    /* rows carry the server's other fields through the extra codec */
    char skin[32];
    char level[8];
    TEST_ASSERT_TRUE(leaderboard_extra_get(all.top[0].extra, "skin", skin, sizeof skin));
    TEST_ASSERT_EQUAL_STRING("hat.s0", skin);
    TEST_ASSERT_TRUE(leaderboard_extra_get(all.top[0].extra, "level", level, sizeof level));
    TEST_ASSERT_EQUAL_STRING("0", level);
    TEST_ASSERT_EQUAL_STRING("", all.top[0].name);
    /* the player's own row sits on the board with the payload just submitted */
    bool found_you = false;
    for (int i = 0; i < all.top_count; i++) {
        if (all.top[i].you) {
            found_you = true;
            TEST_ASSERT_EQUAL_UINT32(7, all.top[i].value);
            TEST_ASSERT_EQUAL_INT(all.place, all.top[i].place);
            TEST_ASSERT_TRUE(leaderboard_extra_get(all.top[i].extra, "skin", skin, sizeof skin));
            TEST_ASSERT_EQUAL_STRING("hat.mine", skin);
        }
    }
    TEST_ASSERT_TRUE(found_you);

    leaderboard_view_t day;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_UTC_DAY, &day);
    TEST_ASSERT_TRUE(day.loaded);
    TEST_ASSERT_GREATER_THAN_INT(0, day.top_count);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, day.place);

    /* the submit completed: the facade recorded what the server took */
    TEST_ASSERT_EQUAL_STRING("7", kv_get("lb.sent.score.all_time"));
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_retry);
}

void test_place_is_re_estimated_locally(void) {
    /* a full window: a value below it can only be estimated from the buckets */
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 50, NULL);
    answer_ok("score", LEADERBOARD_TOP_MAX);
    settle();
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
    const int below = view.place;
    TEST_ASSERT_GREATER_THAN_INT(view.top_count, below);

    /* a better value moves the place before any new response */
    const int requests = g_net.request_calls;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 1550, NULL);
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    const int inside = view.place;
    TEST_ASSERT_LESS_THAN_INT(below, inside);
    TEST_ASSERT_LESS_OR_EQUAL_INT(view.top_count, inside);
    TEST_ASSERT_TRUE(view.top[inside - 1].you);
    send_beat();
    TEST_ASSERT_GREATER_THAN_INT(requests, g_net.request_calls); /* and goes out on that beat */

    /* a value submitted while one is in flight reaches the backend when that
       request completes; the top is re-ranked on it before its own answer */
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 5000, NULL);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_net.live); /* the 5000 request just went out */
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_EQUAL_INT(1, view.place);
    TEST_ASSERT_TRUE(view.top[0].you);
    TEST_ASSERT_EQUAL_UINT32(5000, view.top[0].value);
}

void test_submit_mid_flight_goes_out_right_after(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, NULL);
    send_beat();
    TEST_ASSERT_EQUAL_INT(1, g_net.request_calls);
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 9, NULL);
    TEST_ASSERT_EQUAL_INT(1, g_net.request_calls); /* waits for the answer in flight */
    answer_ok("score", 3);
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(2, g_net.request_calls); /* not the repeat timer */
    TEST_ASSERT_EQUAL_STRING("7", kv_get("lb.sent.score.all_time"));
    char body[1024];
    last_body(body, sizeof body);
    TEST_ASSERT_NOT_NULL(strstr(body, "\"score\":9"));
    settle();
    TEST_ASSERT_EQUAL_STRING("9", kv_get("lb.sent.score.all_time"));
}

void test_healthy_client_polls_on_the_repeat_cadence(void) {
    leaderboard_update();
    answer_ok("score", 3);
    settle();
    TEST_ASSERT_EQUAL_INT(1, g_net.request_calls);
    g_net.mono += REPEAT_S - 1;
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_net.request_calls);
    g_net.mono += 2;
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(2, g_net.request_calls);
}

void test_manual_refresh_starts_one_request(void) {
    leaderboard_update();
    answer_ok("score", 3);
    settle();
    const int before = g_net.request_calls;
    leaderboard_refresh_now(g_score);
    TEST_ASSERT_EQUAL_INT(before + 1, g_net.request_calls);
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).loading);
    settle();
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).loading);
}

/* ---- failures ---- */

void test_error_cadence_backs_off_after_the_fast_tries(void) {
    answer_failed();
    leaderboard_update(); /* starts */
    settle();             /* fails: 1 */
    TEST_ASSERT_EQUAL_INT(1, g_net.request_calls);
    g_net.mono += ERROR_S - 1;
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_net.request_calls);
    g_net.mono += 2;
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(2, g_net.request_calls);
    settle(); /* fails: 2 = FAST_TRIES spent */
    g_net.mono += ERROR_S + 1;
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(2, g_net.request_calls); /* no longer fast */
    g_net.mono += REPEAT_S;
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(3, g_net.request_calls);
    /* a success resets the sequence: the next failure retries fast again */
    answer_ok("score", 3);
    settle();
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_retry);
    g_net.mono += REPEAT_S + 1;
    answer_failed();
    leaderboard_update();
    settle();
    TEST_ASSERT_EQUAL_INT(4, g_net.request_calls);
    g_net.mono += ERROR_S + 1;
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(5, g_net.request_calls);
}

void test_spent_retries_offer_a_manual_retry_that_recovers(void) {
    answer_failed();
    for (int i = 0; i < 20 && !leaderboard_ui_state(g_score).show_retry; i++) {
        leaderboard_update();
        settle();
        g_net.mono += REPEAT_S + 1;
    }
    TEST_ASSERT_TRUE(leaderboard_ui_state(g_score).show_retry);
    TEST_ASSERT_TRUE(leaderboard_available(g_score)); /* FAILED never withdraws the board */
    answer_ok("score", 3);
    leaderboard_refresh_now(g_score);
    settle();
    TEST_ASSERT_FALSE(leaderboard_ui_state(g_score).show_retry);
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
}

void test_malformed_body_is_a_failed_request_not_an_empty_board(void) {
    answer_body("{\"score\":[]}");
    leaderboard_update();
    settle();
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_FALSE(view.loaded);
    TEST_ASSERT_EQUAL_INT(0, view.top_count);
    /* retried on the error cadence, like a transport failure */
    g_net.mono += ERROR_S + 1;
    answer_body("not json at all");
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(2, g_net.request_calls);
    settle();
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_FALSE(view.loaded);
    TEST_ASSERT_TRUE(leaderboard_available(g_score));
}

void test_malformed_body_keeps_the_last_good_board(void) {
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, NULL);
    answer_ok("score", 3);
    settle();
    leaderboard_view_t before;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &before);
    TEST_ASSERT_TRUE(before.loaded);
    g_net.mono += REPEAT_S + 1;
    answer_body("");
    leaderboard_update();
    settle();
    leaderboard_view_t after;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &after);
    TEST_ASSERT_TRUE(after.loaded);
    TEST_ASSERT_EQUAL_INT(before.top_count, after.top_count);
    TEST_ASSERT_EQUAL_INT(before.place, after.place);
}

void test_request_that_cannot_start_is_failed_not_unsupported(void) {
    g_net.refuse = true;
    leaderboard_submit(g_score, LEADERBOARD_SCOPE_ALL_TIME, 7, NULL);
    TEST_ASSERT_EQUAL_INT(0, g_net.request_calls);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_write);
    TEST_ASSERT_TRUE(leaderboard_caps(g_score).can_read);
    TEST_ASSERT_NULL(kv_get("lb.sent.score.all_time"));
    g_net.refuse = false;
    g_net.mono += ERROR_S + 1;
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_net.request_calls);
    answer_ok("score", 3);
    settle();
    leaderboard_view_t view;
    leaderboard_view_get(g_score, LEADERBOARD_SCOPE_ALL_TIME, &view);
    TEST_ASSERT_TRUE(view.loaded);
}

void test_shutdown_releases_the_request_in_flight(void) {
    leaderboard_update();
    TEST_ASSERT_EQUAL_INT(1, g_net.live);
    leaderboard_shutdown();
    TEST_ASSERT_EQUAL_INT(0, g_net.live);
    TEST_ASSERT_EQUAL_INT(1, g_net.released);
    boot(); /* tearDown shuts down again; a second shutdown must be inert */
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dormant_without_an_endpoint);
    RUN_TEST(test_serves_both_scopes_anonymously);
    RUN_TEST(test_first_update_polls_without_a_submit);
    RUN_TEST(test_body_carries_every_scope_and_the_extra);
    RUN_TEST(test_relaunch_poll_keeps_extra_from_an_already_accepted_score);
    RUN_TEST(test_poll_uses_latest_extra_across_scopes_even_when_score_is_unchanged);
    RUN_TEST(test_ascending_board_ranks_smaller_values_first_and_preserves_ties);
    RUN_TEST(test_ascending_board_estimates_places_past_the_top_from_better_counts);
    RUN_TEST(test_server_day_anchors_the_clock_and_the_request);
    RUN_TEST(test_extra_cannot_forge_the_wire_fields);
    RUN_TEST(test_second_board_uses_its_wire_name_and_declared_scopes);
    RUN_TEST(test_one_response_populates_both_scopes);
    RUN_TEST(test_place_is_re_estimated_locally);
    RUN_TEST(test_submit_mid_flight_goes_out_right_after);
    RUN_TEST(test_healthy_client_polls_on_the_repeat_cadence);
    RUN_TEST(test_manual_refresh_starts_one_request);
    RUN_TEST(test_error_cadence_backs_off_after_the_fast_tries);
    RUN_TEST(test_spent_retries_offer_a_manual_retry_that_recovers);
    RUN_TEST(test_malformed_body_is_a_failed_request_not_an_empty_board);
    RUN_TEST(test_malformed_body_keeps_the_last_good_board);
    RUN_TEST(test_request_that_cannot_start_is_failed_not_unsupported);
    RUN_TEST(test_shutdown_releases_the_request_in_flight);
    return UNITY_END();
}
