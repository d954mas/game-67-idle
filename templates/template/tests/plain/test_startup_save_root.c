#include "startup_save_root.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_tool_root_beats_workspace_root(void) {
    TEST_ASSERT_EQUAL_STRING("C:/tool", startup_save_root("C:/tool", "C:/ws/storage/game"));
}

static void test_workspace_root_when_no_tool_root(void) {
    TEST_ASSERT_EQUAL_STRING("C:/ws/storage/game", startup_save_root(NULL, "C:/ws/storage/game"));
    TEST_ASSERT_EQUAL_STRING("C:/ws/storage/game", startup_save_root("", "C:/ws/storage/game"));
}

static void test_platform_default_outside_a_workspace(void) {
    TEST_ASSERT_NULL(startup_save_root(NULL, NULL));
    TEST_ASSERT_NULL(startup_save_root("", NULL));
}

/* A relative root is passed through, not dropped: the caller hands it to
   game_storage_set_root, which refuses it, and the launch fails loudly
   instead of silently falling back to the player's real saves. */
static void test_relative_tool_root_is_not_swallowed(void) {
    TEST_ASSERT_EQUAL_STRING("saves", startup_save_root("saves", "C:/ws/storage/game"));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tool_root_beats_workspace_root);
    RUN_TEST(test_workspace_root_when_no_tool_root);
    RUN_TEST(test_platform_default_outside_a_workspace);
    RUN_TEST(test_relative_tool_root_is_not_swallowed);
    return UNITY_END();
}
