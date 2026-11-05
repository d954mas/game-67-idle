#include "features/ui_kit/ui_ad_policy.h"
#include "features/ui_kit/ui_rgb.h"
#include "features/ui_kit/ui_scale_policy.h"
#include "unity.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

static void test_rgb_tint_swaps_channels_and_clamps_alpha(void) {
    TEST_ASSERT_EQUAL_HEX32(0xFF332211U, ui_rgb_tint(0x112233U, 1.0F));
    TEST_ASSERT_EQUAL_HEX32(0x00332211U, ui_rgb_tint(0x112233U, -2.0F));
    TEST_ASSERT_EQUAL_HEX32(0xFF332211U, ui_rgb_tint(0x112233U, 2.0F));
}

static void test_rgb_arithmetic_stays_in_a_byte_per_channel(void) {
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFU, ui_rgb_lighter(0x102030U, 1.0F));
    TEST_ASSERT_EQUAL_HEX32(0x102030U, ui_rgb_lighter(0x102030U, 0.0F));
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFU, ui_rgb_shaded(0xF0F0F0U, 4.0F));
    TEST_ASSERT_EQUAL_HEX32(0x102030U, ui_rgb_multiplied(0x102030U, 0xFFFFFFU));
    TEST_ASSERT_EQUAL_HEX32(0x000000U, ui_rgb_multiplied(0x102030U, 0x000000U));
}

// The rule's point: the two buttons are the same size whichever way they stack.
static void test_ad_split_keeps_the_standard_button_no_smaller(void) {
    for (float width = 100.0F; width < 800.0F; width += 7.0F) {
        const ui_ad_split_t split = ui_ad_split(width, 10.0F, 150.0F);
        if (split.row) {
            TEST_ASSERT_TRUE(split.each >= 150.0F);
            TEST_ASSERT_TRUE(fabsf((width) - (split.each * 2.0F + 10.0F)) <= 0.01F);
        } else {
            TEST_ASSERT_TRUE(width == split.each);
        }
    }
}

static void test_ad_blue_reads_the_strongest_channel(void) {
    TEST_ASSERT_TRUE(ui_ad_rgb_is_blue(0x1F7AF2U));
    TEST_ASSERT_FALSE(ui_ad_rgb_is_blue(0x43BF1EU));
    TEST_ASSERT_FALSE(ui_ad_rgb_is_blue(0xACB0ACU));
}

// A finger target stays a fixed share of the reference: 44 of a 390 short edge.
static void test_tap_units_scale_with_the_reference(void) {
    TEST_ASSERT_TRUE(fabsf((44.0F) - (ui_scale_tap_units(390.0F))) <= 0.01F);
    TEST_ASSERT_TRUE(ui_scale_tap_units(520.0F) > ui_scale_tap_units(390.0F));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rgb_tint_swaps_channels_and_clamps_alpha);
    RUN_TEST(test_rgb_arithmetic_stays_in_a_byte_per_channel);
    RUN_TEST(test_ad_split_keeps_the_standard_button_no_smaller);
    RUN_TEST(test_ad_blue_reads_the_strongest_channel);
    RUN_TEST(test_tap_units_scale_with_the_reference);
    return UNITY_END();
}
