#include "unity.h"

#include "features/leaderboard/leaderboard_internal.h"

#include <string.h>

/* Silent-failure logic of the pure core: UTC day math, the wire encoding,
   response parsing, and place recalculation. Design knobs (bucket ladders,
   cadence) live server-side and are not pinned here. */

void setUp(void) {}
void tearDown(void) {}

/* ---- UTC day ---- */

static void check_day(long long t, const char *expected) {
    char day[LB_DAY_STR_MAX];
    lb_utc_day((time_t)t, day);
    TEST_ASSERT_EQUAL_STRING(expected, day);
}

void test_utc_day_known_dates(void) {
    check_day(0, "19700101");
    check_day(86399, "19700101");
    check_day(86400, "19700102");
    /* 2026-08-31 12:00:00 UTC */
    check_day(1788177600LL, "20260831");
    /* leap day: 2024-02-29 00:00:00 UTC */
    check_day(1709164800LL, "20240229");
}

void test_utc_day_is_utc_not_local(void) {
    /* one second before and after a UTC midnight */
    check_day(1788134399LL, "20260830");
    check_day(1788134400LL, "20260831");
}

void test_day_number_roundtrip(void) {
    TEST_ASSERT_EQUAL_INT64(0, lb_day_number("19700101"));
    TEST_ASSERT_EQUAL_INT64(1, lb_day_number("19700102"));
    /* inverse of lb_utc_day across a leap boundary */
    char day[LB_DAY_STR_MAX];
    lb_utc_day((time_t)1709164800LL, day); /* 2024-02-29 */
    TEST_ASSERT_EQUAL_INT64(1709164800LL / 86400, lb_day_number(day));
    TEST_ASSERT_EQUAL_INT64(-1, lb_day_number("2026083"));
    TEST_ASSERT_EQUAL_INT64(-1, lb_day_number("2026x831"));
}

/* ---- Wire encoding ---- */

void test_encode_is_base64url_of_xor(void) {
    char out[64];
    /* "AB" ^ "K" -> {0x0A,0x09} -> base64 "Cgk=" -> url form without padding */
    const uint32_t n = lb_encode_payload("AB", "K", out, sizeof out);
    TEST_ASSERT_EQUAL_UINT32(3, n);
    TEST_ASSERT_EQUAL_STRING("Cgk", out);
}

void test_encode_rejects_small_buffer(void) {
    char out[4];
    TEST_ASSERT_EQUAL_UINT32(0, lb_encode_payload("payload-too-big", "K", out, sizeof out));
}

void test_seconds_until_day_end(void) {
    TEST_ASSERT_EQUAL_INT64(86400, lb_seconds_until_day_end((time_t)0));
    TEST_ASSERT_EQUAL_INT64(1, lb_seconds_until_day_end((time_t)86399));
    TEST_ASSERT_EQUAL_INT64(86400, lb_seconds_until_day_end((time_t)86400));
    /* 2026-08-31 12:00:00 UTC */
    TEST_ASSERT_EQUAL_INT64(43200, lb_seconds_until_day_end((time_t)1788177600LL));
}

/* ---- Response parsing ---- */

static const char k_response[] =
    "{\"success\":true,\"score\":{"
    "\"all\":{\"top\":[{\"user_id\":\"a\",\"value\":50,\"tag\":\"gold\",\"tier\":3},"
    "{\"user_id\":\"b\",\"value\":20}],"
    "\"userPlace\":2,\"interpolation\":[{\"value\":10,\"count\":90},{\"value\":100,\"count\":0}]},"
    "\"day\":{\"top\":[],\"userPlace\":1,\"interpolation\":[],\"day\":\"20260831\"}}}";

void test_parse_full_response(void) {
    lb_response_t r;
    char field[LEADERBOARD_EXTRA_MAX];
    TEST_ASSERT_TRUE(lb_parse_response(k_response, (uint32_t)strlen(k_response), "score", &r));
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_STRING("20260831", r.day_str);
    TEST_ASSERT_EQUAL_INT(2, r.all.top_count);
    TEST_ASSERT_EQUAL_STRING("a", r.all.top[0].user_id);
    TEST_ASSERT_EQUAL_UINT32(50, r.all.top[0].value);
    /* every other row field rides along as an opaque pair */
    TEST_ASSERT_TRUE(leaderboard_extra_get(r.all.top[0].extra, "tag", field, sizeof field));
    TEST_ASSERT_EQUAL_STRING("gold", field);
    TEST_ASSERT_TRUE(leaderboard_extra_get(r.all.top[0].extra, "tier", field, sizeof field));
    TEST_ASSERT_EQUAL_STRING("3", field);
    TEST_ASSERT_EQUAL_STRING("", r.all.top[1].extra);
    TEST_ASSERT_EQUAL_INT(2, r.all.interp_count);
    TEST_ASSERT_EQUAL_INT(0, r.day.top_count);
}

void test_parse_rejects_malformed(void) {
    lb_response_t r;
    TEST_ASSERT_FALSE(lb_parse_response("{\"success\":true}", 16, "score", &r));
    TEST_ASSERT_FALSE(lb_parse_response("not json", 8, "score", &r));
    TEST_ASSERT_FALSE(r.valid);
    /* the right shape under the wrong board name is somebody else's board */
    TEST_ASSERT_FALSE(lb_parse_response(k_response, (uint32_t)strlen(k_response), "laps", &r));
}

/* ---- Place recalculation ---- */

static lb_board_t small_board(void) {
    lb_board_t b;
    memset(&b, 0, sizeof b);
    strcpy(b.top[0].user_id, "a");
    b.top[0].value = 50;
    strcpy(b.top[1].user_id, "b");
    b.top[1].value = 20;
    b.top_count = 2;
    return b;
}

void test_recalc_inserts_player_between_rows(void) {
    lb_board_t b = small_board();
    TEST_ASSERT_EQUAL_INT(2, lb_recalc_place(&b, "me", "tag=toy;", 30));
    TEST_ASSERT_EQUAL_STRING("tag=toy;", b.top[1].extra);
    TEST_ASSERT_EQUAL_INT(3, b.top_count);
    TEST_ASSERT_EQUAL_STRING("me", b.top[1].user_id);
}

void test_recalc_updates_existing_player_row(void) {
    lb_board_t b = small_board();
    strcpy(b.top[2].user_id, "me");
    b.top[2].value = 5;
    b.top_count = 3;
    TEST_ASSERT_EQUAL_INT(1, lb_recalc_place(&b, "me", "", 60));
    TEST_ASSERT_EQUAL_INT(3, b.top_count);
    TEST_ASSERT_EQUAL_STRING("me", b.top[0].user_id);
}

void test_recalc_only_player_is_first(void) {
    lb_board_t b;
    memset(&b, 0, sizeof b);
    TEST_ASSERT_EQUAL_INT(1, lb_recalc_place(&b, "me", "", 0));
}

void test_recalc_past_full_window_uses_interpolation(void) {
    lb_board_t b;
    memset(&b, 0, sizeof b);
    for (int i = 0; i < LB_TOP_MAX - 1; i++) {
        b.top[i].user_id[0] = (char)('a' + i);
        b.top[i].value = (uint32_t)(1000 - i);
    }
    b.top_count = LB_TOP_MAX - 1;
    b.interp[0] = (lb_interp_entry_t){.value = 10, .count = 500};
    b.interp[1] = (lb_interp_entry_t){.value = 2000, .count = 0};
    b.interp_count = 2;
    const int place = lb_recalc_place(&b, "me", "", 5);
    /* below the whole window: the estimate must land past it, not at its edge */
    TEST_ASSERT_GREATER_THAN_INT(LB_TOP_MAX, place);
}

void test_recalc_server_ranked_last_keeps_exact_place(void) {
    /* full window, the player IS the server's last row: no estimate */
    lb_board_t b;
    memset(&b, 0, sizeof b);
    for (int i = 0; i < LB_TOP_MAX - 1; i++) {
        b.top[i].user_id[0] = (char)('a' + i);
        b.top[i].value = (uint32_t)(1000 - i);
    }
    strcpy(b.top[LB_TOP_MAX - 2].user_id, "me");
    b.top_count = LB_TOP_MAX - 1;
    b.interp[0] = (lb_interp_entry_t){.value = 2000, .count = 500};
    b.interp_count = 1;
    TEST_ASSERT_EQUAL_INT(LB_TOP_MAX - 1, lb_recalc_place(&b, "me", "", b.top[LB_TOP_MAX - 2].value));
}

void test_recalc_ties_keep_server_order(void) {
    lb_board_t b = small_board();
    (void)lb_recalc_place(&b, "me", "", 20);
    /* equal value: the earlier (server-ranked) row stays above the newcomer */
    TEST_ASSERT_EQUAL_STRING("b", b.top[1].user_id);
    TEST_ASSERT_EQUAL_STRING("me", b.top[2].user_id);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_utc_day_known_dates);
    RUN_TEST(test_utc_day_is_utc_not_local);
    RUN_TEST(test_day_number_roundtrip);
    RUN_TEST(test_encode_is_base64url_of_xor);
    RUN_TEST(test_encode_rejects_small_buffer);
    RUN_TEST(test_parse_full_response);
    RUN_TEST(test_parse_rejects_malformed);
    RUN_TEST(test_recalc_inserts_player_between_rows);
    RUN_TEST(test_recalc_updates_existing_player_row);
    RUN_TEST(test_recalc_only_player_is_first);
    RUN_TEST(test_recalc_past_full_window_uses_interpolation);
    RUN_TEST(test_recalc_server_ranked_last_keeps_exact_place);
    RUN_TEST(test_recalc_ties_keep_server_order);
    RUN_TEST(test_seconds_until_day_end);
    return UNITY_END();
}
