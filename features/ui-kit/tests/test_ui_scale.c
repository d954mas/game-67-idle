#include "features/ui_kit/ui_scale_policy.h"

#include "features/ui_kit/ui_tokens.h"

#include "unity.h"

#include <stdbool.h>

void setUp(void) {}
void tearDown(void) {}

/* Unity is built here with UNITY_EXCLUDE_FLOAT, so float comparisons are spelled
   out rather than asserted through the float macros. */
/* The rule is exercised on the studio defaults, because those are what a new
   prototype runs on; a game with its own sheet passes its own reference. */
static UiScaleFit fit(float fb_w, float fb_h, float dpr) {
    const ui_tokens_t *t = ui_tokens_studio_default();
    return ui_scale_fit(fb_w, fb_h, dpr, t->ref_short, 0.0F, 0.0F);
}

static bool near(float a, float b, float tolerance) {
    const float d = a - b;
    return (d < 0.0F ? -d : d) <= tolerance;
}

/* The whole point: the interface owns the same share of every window. Twice the
   window, twice the scale, and the canvas it lays out on does not move. */
static void test_sizes_follow_the_short_edge(void) {
    const UiScaleFit small = fit(180.0F, 320.0F, 1.0F);
    const UiScaleFit large = fit(360.0F, 640.0F, 1.0F);
    const ui_tokens_t *t = ui_tokens_studio_default();
    const float authored[] = {t->t_body, t->hit, t->gap, t->pad};
    for (unsigned i = 0; i < sizeof authored / sizeof authored[0]; ++i) {
        TEST_ASSERT_TRUE(near(authored[i] * large.scale, authored[i] * small.scale * 2.0F, .001F));
    }
    TEST_ASSERT_TRUE(near(small.logical_w, large.logical_w, .001F));
    TEST_ASSERT_TRUE(near(small.logical_h, large.logical_h, .001F));
}

/* The defect this rule replaces: fitting a 1280x720 reference inside a portrait
   phone left every widget at a fraction of its intended size. */
static void test_portrait_is_not_scaled_by_the_long_edge(void) {
    const UiScaleFit measured = fit(360.0F, 640.0F, 1.0F);
    /* What fitting a 1280x720 reference RECTANGLE into this window would give. */
    const float rectangle_fit = 360.0F / 1280.0F;
    TEST_ASSERT_TRUE(measured.scale > rectangle_fit);
    TEST_ASSERT_TRUE(near(measured.scale, 360.0F / ui_tokens_studio_default()->ref_short, .001F));
}

/* Orientation is not a different design: turning the device must not change how
   large anything is. */
static void test_orientation_does_not_change_scale(void) {
    const UiScaleFit portrait = fit(390.0F, 844.0F, 2.0F);
    const UiScaleFit landscape = fit(844.0F, 390.0F, 2.0F);
    TEST_ASSERT_TRUE(near(portrait.scale, landscape.scale, 0.001F));
}

/* A denser display is the same physical screen with more pixels in it, so an
   authored size keeps the same share of it and only the scale grows. */
static void test_density_buys_pixels_not_size(void) {
    const UiScaleFit one = fit(360.0F, 640.0F, 1.0F);
    const UiScaleFit three = fit(1080.0F, 1920.0F, 3.0F);
    TEST_ASSERT_TRUE(near(one.scale, three.scale / 3.0F, .001F));
    TEST_ASSERT_TRUE(near(one.logical_w, three.logical_w, .001F));
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

/* No floor anywhere: a tiny window gets a tiny interface, not a magnified one.
   A floor is exactly how a short window ends up handing the HUD a larger share
   of itself than a monitor does. */
static void test_there_is_no_small_window_floor(void) {
    const UiScaleFit tiny = fit(40.0F, 70.0F, 1.0F);
    const UiScaleFit twice = fit(80.0F, 140.0F, 1.0F);
    TEST_ASSERT_TRUE(tiny.scale > 0.0F);
    TEST_ASSERT_TRUE(near(twice.scale, tiny.scale * 2.0F, .001F));
    TEST_ASSERT_TRUE(near(tiny.logical_w, twice.logical_w, .001F));
}

/* Safe areas arrive in CSS pixels and everything authored is in units; the
   inset has to survive the round trip on a dense screen. */
static void test_safe_area_keeps_its_physical_size(void) {
    const UiScaleFit one = ui_scale_fit(360, 640, 1, 400, 24, 30);
    const UiScaleFit dense = ui_scale_fit(1080, 1920, 3, 400, 24, 30);
    const UiScaleFit turned = ui_scale_fit(640, 360, 1, 400, 30, 24);
    TEST_ASSERT_TRUE(near(one.scale, dense.scale / 3, .001F));
    TEST_ASSERT_TRUE(near(one.logical_w, dense.logical_w, .001F));
    TEST_ASSERT_TRUE(near(one.scale, turned.scale, .001F));
    const float inset_units = 24 * ui_scale_css_unit(dense.scale, 3);
    TEST_ASSERT_TRUE(near(inset_units * dense.scale / 3, 24, .001F));
}

/* A window the platform has not sized yet, or one a safe area covers whole,
   must not produce a zero or infinite canvas: every caller divides by these. */
static void test_unsized_or_occluded_viewport_is_finite(void) {
    const UiScaleFit empty = ui_scale_fit(0, 0, 0, 0, 0, 0);
    const UiScaleFit occluded = ui_scale_fit(80, 100, 1, 400, 80, 100);
    TEST_ASSERT_TRUE(empty.scale > 0 && empty.logical_w > 0 && empty.logical_h > 0);
    TEST_ASSERT_TRUE(occluded.scale > 0 && occluded.logical_w > 0 && occluded.logical_h > 0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_sizes_follow_the_short_edge);
    RUN_TEST(test_portrait_is_not_scaled_by_the_long_edge);
    RUN_TEST(test_orientation_does_not_change_scale);
    RUN_TEST(test_density_buys_pixels_not_size);
    RUN_TEST(test_scale_is_monotone_in_window_size);
    RUN_TEST(test_there_is_no_small_window_floor);
    RUN_TEST(test_safe_area_keeps_its_physical_size);
    RUN_TEST(test_unsized_or_occluded_viewport_is_finite);
    return UNITY_END();
}
