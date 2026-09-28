#include <string.h>

#include "unity.h"

#include "game_save_text_json.h"

static const char *const TEXT_DOCUMENT =
    "NTGS 1\nformat=1\nsave_version=2\nsaved_at=1790586309091\nsave_seq=49\n"
    "playtime_ms=71221\napp=\"game-ntgs-v1\"\nbuild=\"0\"\n\n"
    "[settings 1]\nmaster_volume=0.5\n\n"
    "[game 5]\ntutorial.done=true\nxp.level=3\nlb.planets_all=1\nname=\"slime\"\nhat=null\n";

void setUp(void) {}
void tearDown(void) {}

static const cJSON *path(const cJSON *root, const char *first, const char *second,
                         const char *third) {
    const cJSON *node = cJSON_GetObjectItemCaseSensitive(root, first);
    if (second != NULL) node = cJSON_GetObjectItemCaseSensitive(node, second);
    if (third != NULL) node = cJSON_GetObjectItemCaseSensitive(node, third);
    return node;
}

static void test_text_document_reads_into_the_json_shape(void) {
    char error[128] = {0};
    cJSON *root = game_save_text_json_parse(TEXT_DOCUMENT, error, (int)sizeof error);
    TEST_ASSERT_NOT_NULL_MESSAGE(root, error);
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(root, "features");
    const cJSON *game = cJSON_GetObjectItemCaseSensitive(features, "game");
    TEST_ASSERT_EQUAL_INT(5, cJSON_GetObjectItemCaseSensitive(game, "v")->valueint);
    TEST_ASSERT_TRUE(cJSON_IsTrue(path(game, "tutorial", "done", NULL)));
    TEST_ASSERT_EQUAL_INT(3, path(game, "xp", "level", NULL)->valueint);
    TEST_ASSERT_EQUAL_STRING("slime", cJSON_GetObjectItemCaseSensitive(game, "name")->valuestring);
    TEST_ASSERT_TRUE(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(game, "hat")));
    const double volume = path(features, "settings", "master_volume", NULL)->valuedouble;
    TEST_ASSERT_TRUE(volume > 0.49 && volume < 0.51);
    TEST_ASSERT_EQUAL_STRING("71221", cJSON_GetObjectItemCaseSensitive(root, "playtime_ms")->valuestring);
    TEST_ASSERT_EQUAL_STRING("game-ntgs-v1", cJSON_GetObjectItemCaseSensitive(root, "app")->valuestring);
    cJSON_Delete(root);
}

static void test_json_document_is_parsed_as_is(void) {
    cJSON *root = game_save_text_json_parse("{\"features\":{\"game\":{\"v\":2}}}", NULL, 0);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_INT(2, path(root, "features", "game", "v")->valueint);
    cJSON_Delete(root);
}

static void test_malformed_text_is_refused(void) {
    char error[128] = {0};
    TEST_ASSERT_NULL(game_save_text_json_parse("NTGS 1\n\n[game 5]\nxp.level=\n", error,
                                               (int)sizeof error));
    TEST_ASSERT_NULL(game_save_text_json_parse(
        "NTGS 1\n\n[game 5]\nxp=1\nxp.level=2\n", NULL, 0));
    TEST_ASSERT_NULL(game_save_text_json_parse("NTGS 1\n\n[game 5]\n[game 5]\n", NULL, 0));
    TEST_ASSERT_NULL(game_save_text_json_parse(NULL, NULL, 0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_text_document_reads_into_the_json_shape);
    RUN_TEST(test_json_document_is_parsed_as_is);
    RUN_TEST(test_malformed_text_is_refused);
    return UNITY_END();
}
