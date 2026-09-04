#include "features/ui_kit/ui_scale_policy.h"

#include "features/ui_kit/ui_tokens.h"

#include "unity.h"

#include <stdbool.h>

void setUp(void) {}
void tearDown(void) {}

/* Unity is built here with UNITY_EXCLUDE_FLOAT, so float comparisons are spelled
   out rather than asserted through the float macros. */
/* The rule is exercised on the studio defaults, because those are what a new
   prototype runs on; a game with its own sheet passes its own two numbers. */
static UiScaleFit fit(float fb_w, float fb_h, float dpr) {
    const ui_tokens_t *t = ui_tokens_studio_default();
    return ui_scale_fit(fb_w, fb_h, dpr, t->ref_short, t->short_edge_max);
}

static bool near(float a, float b, float tolerance) {
    const float d = a - b;
    return (d < 0.0F ? -d : d) <= tolerance;
}

/* Phone-shaped windows, in device pixels with their density. The numbers are
   inputs to a rule, not a pinned layout: what is asserted below is the rule. */
static void test_phone_short_edge_is_capped(void) {
    const UiScaleFit portrait = fit(360.0F, 640.0F, 1.0F);
    const UiScaleFit landscape = fit(640.0F, 360.0F, 1.0F);
    const UiScaleFit dense = fit(1080.0F, 1920.0F, 3.0F);

    TEST_ASSERT_TRUE(near(portrait.logical_w, ui_tokens_studio_default()->short_edge_max, 0.01F));
    TEST_ASSERT_TRUE(near(landscape.logical_h, ui_tokens_studio_default()->short_edge_max, 0.01F));
    TEST_ASSERT_TRUE(near(dense.logical_w, ui_tokens_studio_default()->short_edge_max, 0.01F));
}

/* The whole defect this rule replaces: fitting a 1280x720 reference inside a
   portrait phone left every widget at a fraction of its intended size. */
static void test_portrait_is_not_scaled_by_the_long_edge(void) {
    const UiScaleFit measured = fit(360.0F, 640.0F, 1.0F);
    TEST_ASSERT_TRUE(measured.scale > (360.0F / 1280.0F) * 2.0F);
}

/* Orientation is not a different design: turning the device must not change how
   large anything is. */
static void test_orientation_does_not_change_scale(void) {
    const UiScaleFit portrait = fit(390.0F, 844.0F, 2.0F);
    const UiScaleFit landscape = fit(844.0F, 390.0F, 2.0F);
    TEST_ASSERT_TRUE(near(portrait.scale, landscape.scale, 0.001F));
}

/* A denser display shows the same physical size, so one CSS pixel keeps buying
   the same share of the canvas. */
static void test_css_unit_is_density_independent(void) {
    const UiScaleFit one = fit(360.0F, 640.0F, 1.0F);
    const UiScaleFit three = fit(1080.0F, 1920.0F, 3.0F);
    TEST_ASSERT_TRUE(near(ui_scale_css_unit(one.scale, 1.0F), ui_scale_css_unit(three.scale, 3.0F), 0.001F));
}

/* A larger window may show more, never smaller. */
static void test_scale_is_monotone_in_window_size(void) {
    float previous = 0.0F;
    for (float side = 320.0F; side <= 2160.0F; side += 40.0F) {
        const UiScaleFit measured = fit(side * 16.0F / 9.0F, side, 1.0F);
        TEST_ASSERT_TRUE(measured.scale >= previous);
        TEST_ASSERT_TRUE(measured.logical_h > 0.0F);
        previous = measured.scale;
    }
}

/* A window the platform has not sized yet must not produce a zero or infinite
   canvas: every caller divides by these. */
static void test_degenerate_window_still_yields_a_canvas(void) {
    const UiScaleFit measured = fit(0.0F, 0.0F, 0.0F);
    TEST_ASSERT_TRUE(measured.scale >= UI_SCALE_MIN);
    TEST_ASSERT_TRUE(measured.logical_w > 0.0F);
    TEST_ASSERT_TRUE(measured.logical_h > 0.0F);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_phone_short_edge_is_capped);
    RUN_TEST(test_portrait_is_not_scaled_by_the_long_edge);
    RUN_TEST(test_orientation_does_not_change_scale);
    RUN_TEST(test_css_unit_is_density_independent);
    RUN_TEST(test_scale_is_monotone_in_window_size);
    RUN_TEST(test_degenerate_window_still_yields_a_canvas);
    return UNITY_END();
}
