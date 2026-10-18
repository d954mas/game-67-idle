#include "game_input.h"

#include "input/nt_input_internal.h"
#include "unity.h"

void setUp(void) {
    nt_input_init();
}

void tearDown(void) {
    nt_input_shutdown();
}

static void test_capture_builds_one_semantic_frame_snapshot(void) {
    nt_input_set_key(NT_KEY_W, true);
    nt_input_pointer_down(
        1U, 320.0F, 240.0F, 1.0F, NT_POINTER_MOUSE, 3U);
    nt_input_wheel(0.0F, 1.0F);

    game_input_frame_t input;
    game_input_capture(&input);

    TEST_ASSERT_TRUE(input.any_gesture);
    TEST_ASSERT_TRUE(input.move_up);
    TEST_ASSERT_TRUE(input.pointers[0].active);
    TEST_ASSERT_TRUE(input.pointers[0].left_pressed);
    TEST_ASSERT_TRUE(input.pointers[0].right_down);
    TEST_ASSERT_TRUE(input.pointers[0].wheel_y > 0.9F);
}

static void test_empty_capture_has_no_gesture(void) {
    game_input_frame_t input;

    game_input_capture(&input);

    TEST_ASSERT_FALSE(input.any_gesture);
    TEST_ASSERT_FALSE(input.escape_pressed);
    TEST_ASSERT_FALSE(input.pointers[0].active);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_capture_builds_one_semantic_frame_snapshot);
    RUN_TEST(test_empty_capture_has_no_gesture);
    return UNITY_END();
}
