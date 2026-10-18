#include "game_state.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_state_round_trip_keeps_the_foundation_fragment_valid(void) {
    GameState original;
    game_state_init_defaults(&original);
    cJSON *json = game_state_to_json(&original);
    TEST_ASSERT_NOT_NULL(json);

    GameState restored;
    char error[128] = {0};
    TEST_ASSERT_TRUE(game_state_from_json(&restored, json, error, (int)sizeof error));
    TEST_ASSERT_TRUE(game_state_validate(&restored, error, (int)sizeof error));
    cJSON_Delete(json);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_state_round_trip_keeps_the_foundation_fragment_valid);
    return UNITY_END();
}
