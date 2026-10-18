#include "ui/focus_prompt_policy.h"

#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_focus_prompt_is_desktop_web_only(void) {
    TEST_ASSERT_TRUE(focus_prompt_should_show(true, false, true));
    TEST_ASSERT_FALSE(focus_prompt_should_show(true, true, true));
    TEST_ASSERT_FALSE(focus_prompt_should_show(true, false, false));
    TEST_ASSERT_FALSE(focus_prompt_should_show(false, false, true));
}

static void test_focus_click_is_consumed_until_pointer_release(void) {
    FocusPromptGate gate = {0};
    gate = focus_prompt_gate_next(gate, true, false);
    TEST_ASSERT_TRUE(gate.visible);

    gate = focus_prompt_gate_next(gate, false, true);
    TEST_ASSERT_TRUE(gate.visible);
    TEST_ASSERT_TRUE(gate.awaiting_pointer_release);

    gate = focus_prompt_gate_next(gate, false, false);
    TEST_ASSERT_FALSE(gate.visible);
    TEST_ASSERT_FALSE(gate.awaiting_pointer_release);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_focus_prompt_is_desktop_web_only);
    RUN_TEST(test_focus_click_is_consumed_until_pointer_release);
    return UNITY_END();
}
