#include "unity.h"

#include "game_state.h"

#include <stdint.h>
#include <string.h>

/* A4 round-trip gate for the generated fragment state layer (§A4.8 (5)).
   Links the generated game_state.c + the gsj_* toolkit; game_save is NOT linked
   (the descriptor is data + static wrappers, no game_save_* calls). Unity is
   built with UNITY_EXCLUDE_FLOAT/DOUBLE, so no float/double asserts here. */

void setUp(void) {}
void tearDown(void) {}

/* init_defaults -> to_json -> from_json (other instance) -> to_json: equal. */
void test_defaults_round_trip(void) {
    GameState a;
    game_state_init_defaults(&a);
    cJSON *ja = game_state_to_json(&a);
    TEST_ASSERT_NOT_NULL(ja);
    char *sa = cJSON_PrintUnformatted(ja);
    TEST_ASSERT_NOT_NULL(sa);

    GameState b;
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_state_from_json(&b, ja, err, (int)sizeof(err)));
    cJSON *jb = game_state_to_json(&b);
    char *sb = cJSON_PrintUnformatted(jb);
    TEST_ASSERT_NOT_NULL(sb);

    TEST_ASSERT_EQUAL_STRING(sa, sb);

    cJSON_free(sa);
    cJSON_free(sb);
    cJSON_Delete(ja);
    cJSON_Delete(jb);
}

/* i64 rides the wire as a JSON STRING; a value > 2^53 round-trips exactly. */
void test_i64_string_round_trip(void) {
    GameState a;
    game_state_init_defaults(&a);
    a.wallet_soft = 9000000000000000000LL;
    a.wallet_hard = 1234567890123456789LL;
    cJSON *ja = game_state_to_json(&a);
    TEST_ASSERT_NOT_NULL(ja);

    const cJSON *wallet = cJSON_GetObjectItemCaseSensitive(ja, "wallet");
    TEST_ASSERT_NOT_NULL(wallet);
    const cJSON *soft = cJSON_GetObjectItemCaseSensitive(wallet, "soft");
    TEST_ASSERT_NOT_NULL(soft);
    TEST_ASSERT_TRUE(cJSON_IsString(soft)); /* not a number */
    TEST_ASSERT_EQUAL_STRING("9000000000000000000", soft->valuestring);

    GameState b;
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_state_from_json(&b, ja, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_INT64(9000000000000000000LL, b.wallet_soft);
    TEST_ASSERT_EQUAL_INT64(1234567890123456789LL, b.wallet_hard);

    cJSON_Delete(ja);
}

/* Tolerant from_json: empty object -> defaults; unknown key ignored, others read. */
void test_from_json_tolerant(void) {
    char err[128] = {0};

    cJSON *empty = cJSON_CreateObject();
    GameState a;
    TEST_ASSERT_TRUE(game_state_from_json(&a, empty, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_INT64(0, a.wallet_soft);      /* default */
    TEST_ASSERT_EQUAL_INT(0, a.test_ui_clicks);     /* default */
    cJSON_Delete(empty);

    cJSON *partial = cJSON_CreateObject();
    cJSON_AddStringToObject(partial, "totally_unknown_key", "ignored");
    cJSON_AddNumberToObject(partial, "test_ui_clicks", 7);
    GameState b;
    TEST_ASSERT_TRUE(game_state_from_json(&b, partial, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_INT(7, b.test_ui_clicks);     /* read despite the unknown key */
    TEST_ASSERT_EQUAL_INT64(0, b.wallet_soft);      /* absent -> default */
    cJSON_Delete(partial);
}

/* get/set path for i64 + transitional-devapi semantics (L6). */
void test_get_set_path_i64(void) {
    GameState a;
    game_state_init_defaults(&a);
    char err[128] = {0};

    cJSON *got = game_state_get_path_json(&a, "wallet.soft", err, (int)sizeof(err));
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_TRUE(cJSON_IsString(got)); /* i64 read exposes a string (§14 p.8) */
    cJSON_Delete(got);

    cJSON *str_val = cJSON_CreateString("42");
    TEST_ASSERT_TRUE(game_state_set_path_json(&a, "wallet.soft", str_val, err, (int)sizeof(err)));
    cJSON_Delete(str_val);
    TEST_ASSERT_EQUAL_INT64(42, a.wallet_soft);

    cJSON *got2 = game_state_get_path_json(&a, "wallet.soft", err, (int)sizeof(err));
    TEST_ASSERT_NOT_NULL(got2);
    TEST_ASSERT_TRUE(cJSON_IsString(got2));
    TEST_ASSERT_EQUAL_STRING("42", got2->valuestring);
    cJSON_Delete(got2);

    /* a bare number <= 2^53 is accepted */
    cJSON *num_val = cJSON_CreateNumber(1000);
    TEST_ASSERT_TRUE(game_state_set_path_json(&a, "wallet.soft", num_val, err, (int)sizeof(err)));
    cJSON_Delete(num_val);
    TEST_ASSERT_EQUAL_INT64(1000, a.wallet_soft);

    /* a non-numeric string is rejected */
    cJSON *bad_val = cJSON_CreateString("not-a-number");
    TEST_ASSERT_FALSE(game_state_set_path_json(&a, "wallet.soft", bad_val, err, (int)sizeof(err)));
    cJSON_Delete(bad_val);
}

/* Old A3-era save with a NUMERIC i64 loads (gsj_read_i64 accepts <= 2^53). */
void test_old_save_numeric_i64(void) {
    GameState a;
    char err[128] = {0};
    cJSON *old = cJSON_Parse("{\"wallet\":{\"soft\":123}}");
    TEST_ASSERT_NOT_NULL(old);
    TEST_ASSERT_TRUE(game_state_from_json(&a, old, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_INT64(123, a.wallet_soft);
    TEST_ASSERT_EQUAL_INT64(0, a.wallet_hard); /* absent -> default */
    cJSON_Delete(old);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_round_trip);
    RUN_TEST(test_i64_string_round_trip);
    RUN_TEST(test_from_json_tolerant);
    RUN_TEST(test_get_set_path_i64);
    RUN_TEST(test_old_save_numeric_i64);
    return UNITY_END();
}
