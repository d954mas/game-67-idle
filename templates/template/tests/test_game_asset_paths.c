#include "game_asset_paths.h"

#include "unity.h"

#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static bool is_absolute(const char *path) {
#ifdef _WIN32
    const bool has_drive = (path[0] >= 'A' && path[0] <= 'Z') ||
                           (path[0] >= 'a' && path[0] <= 'z');
    return has_drive && path[1] == ':' && (path[2] == '/' || path[2] == '\\');
#else
    return path[0] == '/';
#endif
}

// The whole point of the module: a pack must be found from any working
// directory, so the resolved path may not stay relative.
static void test_resolves_next_to_the_executable(void) {
    char resolved[GAME_ASSET_PATH_MAX];
    TEST_ASSERT_TRUE(game_asset_paths_resolve(
        "assets/game.ntpack", resolved, sizeof resolved));
    TEST_ASSERT_TRUE(is_absolute(resolved));
    const char *tail = strstr(resolved, "assets/game.ntpack");
    TEST_ASSERT_NOT_NULL(tail);
    TEST_ASSERT_EQUAL_STRING("assets/game.ntpack", tail);
    TEST_ASSERT_TRUE(tail > resolved && *(tail - 1) == '/');
}

static void test_rejects_a_result_that_does_not_fit(void) {
    char resolved[8];
    TEST_ASSERT_FALSE(game_asset_paths_resolve(
        "assets/game.ntpack", resolved, sizeof resolved));
}

static void test_rejects_missing_arguments(void) {
    char resolved[GAME_ASSET_PATH_MAX];
    TEST_ASSERT_FALSE(
        game_asset_paths_resolve(NULL, resolved, sizeof resolved));
    TEST_ASSERT_FALSE(
        game_asset_paths_resolve("assets/game.ntpack", NULL, sizeof resolved));
    TEST_ASSERT_FALSE(
        game_asset_paths_resolve("assets/game.ntpack", resolved, 0));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_resolves_next_to_the_executable);
    RUN_TEST(test_rejects_a_result_that_does_not_fit);
    RUN_TEST(test_rejects_missing_arguments);
    return UNITY_END();
}
