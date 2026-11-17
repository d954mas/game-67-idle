#include "game_input.h"

#include "input/nt_input_feed.h"
#include "unity.h"

// Events apply to the next frame; nt_input_poll publishes them, as the window pump does.
static inline void feed_key(nt_key_t key, bool down) {
    nt_input_feed((nt_input_event_t){.kind = NT_INPUT_EV_KEY, .key = {(uint8_t)key, down}});
}
static inline void feed_mouse(float x, float y, uint8_t buttons) {
    nt_input_feed((nt_input_event_t){.kind = NT_INPUT_EV_MOUSE_MOVE, .mouse = {x, y, buttons}});
}
static inline void feed_wheel(float dx, float dy) {
    nt_input_feed((nt_input_event_t){.kind = NT_INPUT_EV_WHEEL, .wheel = {dx, dy}});
}

void setUp(void) {
    nt_input_init();
}

void tearDown(void) {
    nt_input_shutdown();
}

static void test_capture_builds_one_semantic_frame_snapshot(void) {
    feed_key(NT_KEY_W, true);
    feed_mouse(320.0F, 240.0F, 3U);
    feed_wheel(0.0F, 1.0F);

    game_input_frame_t input;
    nt_input_poll();
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

    nt_input_poll();

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
