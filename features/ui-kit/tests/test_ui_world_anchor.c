#include "features/ui_kit/ui_world_anchor.h"
#include "unity.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

// A camera at the origin looking down -z with a 90 degree frustum: clip w is the
// distance, and x, y run to +-w at the frustum's sides. Near 1, far 100.
static const float k_vp[16] = {1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, -1.0202F, -1.0F, 0.0F, 0.0F, -2.0202F, 0.0F};

static void test_project_rejects_outside_the_frustum_and_leaves_outputs(void) {
    float x = -7.0F, y = -7.0F, depth = -7.0F;
    TEST_ASSERT_TRUE(ui_world_project(k_vp, (const float[3]){0.0F, 0.0F, -10.0F}, 200.0F, 100.0F, &x, &y, &depth));
    TEST_ASSERT_TRUE(fabsf((100.0F) - (x)) <= 0.01F);
    TEST_ASSERT_TRUE(fabsf((50.0F) - (y)) <= 0.01F);
    float kept = -7.0F;
    TEST_ASSERT_FALSE(ui_world_project(k_vp, (const float[3]){20.0F, 0.0F, -10.0F}, 200.0F, 100.0F, &kept, &kept, &kept));
    TEST_ASSERT_FALSE(ui_world_project(k_vp, (const float[3]){0.0F, 0.0F, 10.0F}, 200.0F, 100.0F, &kept, &kept, &kept));
    TEST_ASSERT_TRUE(-7.0F == kept);
}

// The anchor's y-down result matches the plain canvas projection.
static void test_project_agrees_with_canvas_projection(void) {
    float x = 0.0F, y = 0.0F, depth = 0.0F, cx = 0.0F, cy = 0.0F;
    const float at[3] = {2.0F, 3.0F, -10.0F};
    TEST_ASSERT_TRUE(ui_world_project(k_vp, at, 300.0F, 200.0F, &x, &y, &depth));
    TEST_ASSERT_TRUE(ui_world_to_canvas(k_vp, at, 300.0F, 200.0F, &cx, &cy));
    TEST_ASSERT_TRUE(fabsf((cx) - (x)) <= 0.01F);
    TEST_ASSERT_TRUE(fabsf((cy) - (y)) <= 0.01F);
    TEST_ASSERT_TRUE(y < 100.0F);
}

// Biased depth moves the anchor toward the eye, and nearer anchors stay nearer.
static void test_biased_depth_is_nearer_and_ordered(void) {
    float x = 0.0F, y = 0.0F, near_depth = 0.0F, far_depth = 0.0F;
    TEST_ASSERT_TRUE(ui_world_project(k_vp, (const float[3]){0.0F, 0.0F, -5.0F}, 100.0F, 100.0F, &x, &y, &near_depth));
    TEST_ASSERT_TRUE(ui_world_project(k_vp, (const float[3]){0.0F, 0.0F, -50.0F}, 100.0F, 100.0F, &x, &y, &far_depth));
    const float plain = (-1.0202F * -5.0F + -2.0202F) / 5.0F;
    TEST_ASSERT_TRUE(near_depth < plain);
    TEST_ASSERT_TRUE(near_depth < far_depth);
}

static void test_depth_offset_is_guarded_at_the_near_plane(void) {
    TEST_ASSERT_TRUE(ui_world_depth_offset(0.5F) < 0.0F);
    TEST_ASSERT_TRUE(ui_world_depth_offset(0.5F) < ui_world_depth_offset(0.1F));
    TEST_ASSERT_TRUE(ui_world_depth_offset(-5.0F) == ui_world_depth_offset(-1.0F));
    TEST_ASSERT_TRUE(ui_world_depth_offset(-5.0F) < 0.0F);
}

static void test_billboard_scales_with_distance_and_clips(void) {
    ui_world_billboard_t near_b, far_b;
    const float focus = 10.0F, margin = 50.0F;
    TEST_ASSERT_TRUE(ui_world_billboard(k_vp, (const float[3]){0.0F, 0.0F, -10.0F}, 200.0F, 100.0F, 1.5F, focus, margin, &near_b));
    TEST_ASSERT_TRUE(fabsf((1.0F) - (near_b.scale)) <= 1e-4F);
    TEST_ASSERT_TRUE(ui_world_billboard(k_vp, (const float[3]){0.0F, 0.0F, -20.0F}, 200.0F, 100.0F, 1.5F, focus, margin, &far_b));
    TEST_ASSERT_TRUE(far_b.scale < near_b.scale);
    TEST_ASSERT_TRUE(far_b.distance > near_b.distance);
    // Behind the near limit, and far past the edge, it is gone; just past the edge it stays.
    TEST_ASSERT_FALSE(ui_world_billboard(k_vp, (const float[3]){0.0F, 0.0F, -1.0F}, 200.0F, 100.0F, 1.5F, focus, margin, &near_b));
    TEST_ASSERT_FALSE(ui_world_billboard(k_vp, (const float[3]){0.0F, 0.0F, 10.0F}, 200.0F, 100.0F, 1.5F, focus, margin, &near_b));
    TEST_ASSERT_FALSE(ui_world_billboard(k_vp, (const float[3]){60.0F, 0.0F, -10.0F}, 200.0F, 100.0F, 1.5F, focus, margin, &near_b));
    TEST_ASSERT_TRUE(ui_world_billboard(k_vp, (const float[3]){11.0F, 0.0F, -10.0F}, 200.0F, 100.0F, 1.5F, focus, margin, &near_b));
    TEST_ASSERT_TRUE(near_b.x > 200.0F);
}

static void test_project_any_keeps_a_point_off_the_side(void) {
    const float beside[3] = {20.0F, 0.0F, -10.0F};
    float x = 0.0F, y = 0.0F, depth = 0.0F, on_x = 0.0F, on_y = 0.0F, on_depth = 0.0F;
    TEST_ASSERT_FALSE(ui_world_project(k_vp, beside, 200.0F, 100.0F, &x, &y, &depth));
    TEST_ASSERT_TRUE(ui_world_project_any(k_vp, beside, 200.0F, 100.0F, &x, &y, &depth));
    TEST_ASSERT_TRUE(x > 200.0F);
    // Same distance, same depth: being off the side changes neither.
    TEST_ASSERT_TRUE(ui_world_project(k_vp, (const float[3]){0.0F, 0.0F, -10.0F}, 200.0F, 100.0F, &on_x, &on_y, &on_depth));
    TEST_ASSERT_TRUE(fabsf(on_depth - depth) <= 1e-5F);
    TEST_ASSERT_FALSE(ui_world_project_any(k_vp, (const float[3]){0.0F, 0.0F, 10.0F}, 200.0F, 100.0F, &x, &y, &depth));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_project_rejects_outside_the_frustum_and_leaves_outputs);
    RUN_TEST(test_project_agrees_with_canvas_projection);
    RUN_TEST(test_project_any_keeps_a_point_off_the_side);
    RUN_TEST(test_biased_depth_is_nearer_and_ordered);
    RUN_TEST(test_depth_offset_is_guarded_at_the_near_plane);
    RUN_TEST(test_billboard_scales_with_distance_and_clips);
    return UNITY_END();
}
