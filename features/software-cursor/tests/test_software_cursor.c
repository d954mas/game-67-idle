#include <stdbool.h>
#include <stdint.h>

#include "unity.h"

#include "features/software_cursor/software_cursor.h"
#include "features/software_cursor/software_cursor_samples.h"

static bool s_native_hidden;
static int s_native_mode_calls;

static void set_native_hidden(bool hidden, void *userdata) {
    (void)userdata;
    s_native_hidden = hidden;
    s_native_mode_calls++;
}

static software_cursor_theme_t test_theme(void) {
    software_cursor_theme_t theme = software_cursor_theme_defaults();
    theme.follow_response = 1.0F;
    theme.click_pulse_seconds = 0.15F;
    theme.styles[SOFTWARE_CURSOR_INTENT_DEFAULT] = (software_cursor_style_t){
        .idle_visual = 101U,
        .pressed_visual = 102U,
        .width = 40.0F,
        .height = 50.0F,
        .hotspot_x = 4.0F,
        .hotspot_y = 6.0F,
        .pressed_scale = 0.86F,
    };
    return theme;
}

void setUp(void) {
    s_native_hidden = false;
    s_native_mode_calls = 0;
}

void tearDown(void) {}

void test_enable_hides_native_cursor_and_first_sample_snaps(void) {
    software_cursor_t cursor;
    software_cursor_theme_t theme = test_theme();
    software_cursor_init(&cursor, &theme, set_native_hidden, NULL);

    TEST_ASSERT_TRUE(s_native_hidden);
    TEST_ASSERT_EQUAL_INT(1, s_native_mode_calls);

    software_cursor_update(&cursor, &(software_cursor_input_t){
        .active = true, .x = 120.0F, .y = 240.0F,
    }, 1.0F / 60.0F);
    const software_cursor_presentation_t *p = software_cursor_presentation(&cursor);
    TEST_ASSERT_TRUE(p->visible);
    TEST_ASSERT_EQUAL_INT(120, (int)p->x);
    TEST_ASSERT_EQUAL_INT(240, (int)p->y);
    TEST_ASSERT_EQUAL_UINT32(101U, p->visual);
}

void test_motion_follows_instead_of_teleporting(void) {
    software_cursor_t cursor;
    software_cursor_theme_t theme = test_theme();
    software_cursor_init(&cursor, &theme, NULL, NULL);
    software_cursor_update(&cursor, &(software_cursor_input_t){
        .active = true, .x = 0.0F, .y = 0.0F,
    }, 0.1F);
    software_cursor_update(&cursor, &(software_cursor_input_t){
        .active = true, .x = 100.0F, .y = 40.0F,
    }, 0.5F);

    const software_cursor_presentation_t *p = software_cursor_presentation(&cursor);
    TEST_ASSERT_EQUAL_INT(50, (int)p->x);
    TEST_ASSERT_EQUAL_INT(20, (int)p->y);
}

void test_press_uses_theme_visual_and_squash(void) {
    software_cursor_t cursor;
    software_cursor_theme_t theme = test_theme();
    software_cursor_init(&cursor, &theme, NULL, NULL);
    software_cursor_update(&cursor, &(software_cursor_input_t){
        .active = true, .pressed = true, .down = true, .x = 12.0F, .y = 20.0F,
    }, 1.0F / 60.0F);

    const software_cursor_presentation_t *p = software_cursor_presentation(&cursor);
    TEST_ASSERT_TRUE(p->pressed);
    TEST_ASSERT_EQUAL_UINT32(102U, p->visual);
    TEST_ASSERT_EQUAL_INT(86, (int)(p->scale * 100.0F));
}

void test_click_pulse_reports_normalized_decay_from_real_press(void) {
    software_cursor_t cursor;
    software_cursor_theme_t theme = test_theme();
    software_cursor_init(&cursor, &theme, NULL, NULL);
    software_cursor_update(&cursor, &(software_cursor_input_t){
        .active = true, .pressed = true, .x = 12.0F, .y = 20.0F,
    }, 0.01F);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001F, 1.0F, software_cursor_presentation(&cursor)->click_pulse);

    software_cursor_update(&cursor, &(software_cursor_input_t){
        .active = true, .x = 12.0F, .y = 20.0F,
    }, 0.075F);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001F, 0.5F, software_cursor_presentation(&cursor)->click_pulse);
}

void test_game_can_replace_every_visual(void) {
    software_cursor_t cursor;
    software_cursor_theme_t theme = test_theme();
    theme.styles[SOFTWARE_CURSOR_INTENT_GRAB] = (software_cursor_style_t){
        .idle_visual = 901U,
        .pressed_visual = 902U,
        .width = 72.0F,
        .height = 72.0F,
        .pressed_scale = 0.9F,
    };
    software_cursor_init(&cursor, &theme, NULL, NULL);
    software_cursor_set_intent(&cursor, SOFTWARE_CURSOR_INTENT_GRAB);
    software_cursor_update(&cursor, &(software_cursor_input_t){
        .active = true, .x = 8.0F, .y = 9.0F,
    }, 1.0F / 60.0F);

    TEST_ASSERT_EQUAL_UINT32(
        901U, software_cursor_presentation(&cursor)->visual);
}

void test_sample_themes_offer_cursor_and_finger(void) {
    const software_cursor_theme_t pointer = software_cursor_sample_pointer_theme();
    const software_cursor_theme_t finger = software_cursor_sample_finger_theme();
    TEST_ASSERT_NOT_EQUAL(
        pointer.styles[SOFTWARE_CURSOR_INTENT_DEFAULT].idle_visual,
        finger.styles[SOFTWARE_CURSOR_INTENT_DEFAULT].idle_visual);
    TEST_ASSERT_TRUE(
        finger.styles[SOFTWARE_CURSOR_INTENT_DEFAULT].pressed_visual != 0U);
}

void test_disable_restores_native_cursor(void) {
    software_cursor_t cursor;
    software_cursor_theme_t theme = test_theme();
    software_cursor_init(&cursor, &theme, set_native_hidden, NULL);
    software_cursor_set_enabled(&cursor, false);

    TEST_ASSERT_FALSE(s_native_hidden);
    TEST_ASSERT_EQUAL_INT(2, s_native_mode_calls);
    TEST_ASSERT_FALSE(software_cursor_presentation(&cursor)->visible);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_enable_hides_native_cursor_and_first_sample_snaps);
    RUN_TEST(test_motion_follows_instead_of_teleporting);
    RUN_TEST(test_press_uses_theme_visual_and_squash);
    RUN_TEST(test_click_pulse_reports_normalized_decay_from_real_press);
    RUN_TEST(test_game_can_replace_every_visual);
    RUN_TEST(test_sample_themes_offer_cursor_and_finger);
    RUN_TEST(test_disable_restores_native_cursor);
    return UNITY_END();
}
