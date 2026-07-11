#include "unity.h"

#include "game_state.h"

#include <stdint.h>
#include <string.h>

/* A4 round-trip gate for the generated fragment state layer.
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

/* Tolerant from_json: empty object -> defaults; unknown key ignored, others read. */
void test_from_json_tolerant(void) {
    char err[128] = {0};

    cJSON *empty = cJSON_CreateObject();
    GameState a;
    TEST_ASSERT_TRUE(game_state_from_json(&a, empty, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_INT(0, a.test_ui_clicks);     /* default */
    cJSON_Delete(empty);

    cJSON *partial = cJSON_CreateObject();
    cJSON_AddStringToObject(partial, "totally_unknown_key", "ignored");
    cJSON_AddNumberToObject(partial, "test_ui_clicks", 7);
    GameState b;
    TEST_ASSERT_TRUE(game_state_from_json(&b, partial, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_INT(7, b.test_ui_clicks);     /* read despite the unknown key */
    cJSON_Delete(partial);
}

/* get/set path for a plain int + transitional-devapi semantics (L6). i64-on-the-wire
   coverage (string encoding, gsj_read_i64/gsj_add_i64) is NOT exercised through this
   fragment post-T0327-hygiene: the `game` fragment carries no i64 field once the dead
   rb-dark wallet.soft/hard fields were gutted (honest demo shape). That coverage lives
   in test_game_state_json.c (test_read_i64_string_round_trip et al., the generic gsj_
   toolkit) and test_items_fragment.c (a REAL i64 field, ItemOwned.count, through a
   live fragment) -- both keep exercising the exact same wire path. */
void test_get_set_path_int(void) {
    GameState a;
    game_state_init_defaults(&a);
    char err[128] = {0};

    cJSON *got = game_state_get_path_json(&a, "test_ui_clicks", err, (int)sizeof(err));
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_TRUE(cJSON_IsNumber(got));
    TEST_ASSERT_EQUAL_INT(0, (int)got->valuedouble);
    cJSON_Delete(got);

    cJSON *num_val = cJSON_CreateNumber(42);
    TEST_ASSERT_TRUE(game_state_set_path_json(&a, "test_ui_clicks", num_val, err, (int)sizeof(err)));
    cJSON_Delete(num_val);
    TEST_ASSERT_EQUAL_INT(42, a.test_ui_clicks);

    cJSON *got2 = game_state_get_path_json(&a, "test_ui_clicks", err, (int)sizeof(err));
    TEST_ASSERT_NOT_NULL(got2);
    TEST_ASSERT_EQUAL_INT(42, (int)got2->valuedouble);
    cJSON_Delete(got2);

    /* out of range is rejected */
    cJSON *bad_val = cJSON_CreateNumber(-1);
    TEST_ASSERT_FALSE(game_state_set_path_json(&a, "test_ui_clicks", bad_val, err, (int)sizeof(err)));
    cJSON_Delete(bad_val);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_round_trip);
    RUN_TEST(test_from_json_tolerant);
    RUN_TEST(test_get_set_path_int);
    return UNITY_END();
}
