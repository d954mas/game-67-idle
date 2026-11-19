#include "garment/pose.h"
#include "unity.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

void setUp(void) {}
void tearDown(void) {}

static void test_distance_blends_at_the_current_pose_in_either_direction(void) {
    const float origin[] = {1, 0, 2}, far[] = {1, 4, 2}, near[] = {1, 1, 2};
    float weight = -1;
    TEST_ASSERT_TRUE(cloth_pose_distance(origin, near, 4, 0, &weight));
    TEST_ASSERT_TRUE(fabsf(weight - 0.84375F) < 1e-6F);
    TEST_ASSERT_TRUE(cloth_pose_distance(origin, far, 0, 2, &weight));
    TEST_ASSERT_TRUE(weight == 1);
    TEST_ASSERT_TRUE(cloth_pose_distance(origin, origin, 0, 2, &weight));
    TEST_ASSERT_TRUE(weight == 0);
    const float largest[] = {FLT_MAX, FLT_MAX, FLT_MAX}, smallest[] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    TEST_ASSERT_TRUE(cloth_pose_distance(largest, smallest, 1, 0, &weight));
    TEST_ASSERT_TRUE(weight == 0);
}

static void test_three_points_define_a_scale_independent_unsigned_angle(void) {
    const float first[] = {5, 4, 3}, pivot[] = {2, 4, 3}, right[] = {2, 2, 3}, opposite[] = {1, 4, 3};
    float weight = -1;
    TEST_ASSERT_TRUE(cloth_pose_angle(first, pivot, right, 180, 0, &weight));
    TEST_ASSERT_TRUE(fabsf(weight - 0.5F) < 1e-6F);
    TEST_ASSERT_TRUE(cloth_pose_angle(first, pivot, opposite, 90, 180, &weight));
    TEST_ASSERT_TRUE(weight == 1);
    TEST_ASSERT_TRUE(cloth_pose_angle(first, pivot, first, 90, 180, &weight));
    TEST_ASSERT_TRUE(weight == 0);
    weight = -1;
    TEST_ASSERT_FALSE(cloth_pose_angle(first, pivot, pivot, 90, 0, &weight));
    TEST_ASSERT_FALSE(cloth_pose_angle(first, pivot, opposite, 90, 181, &weight));
    TEST_ASSERT_FALSE(cloth_pose_angle(first, pivot, right, 90, 90, &weight));
    TEST_ASSERT_FALSE(cloth_pose_distance(first, (float[3]){NAN, 1, 1}, 2, 1, &weight));
    TEST_ASSERT_FALSE(cloth_pose_distance(first, pivot, -1, 0, &weight));
    TEST_ASSERT_TRUE(weight == -1);
}

static void test_masked_offsets_cancel_before_clamping_and_do_not_accumulate_across_poses(void) {
    const float lower[] = {0, 1, 0.5F, 0}, side[] = {0, 1, 0, 1};
    ClothPoseOffset layers[] = {{lower, 0.1F, 1}, {side, -0.15F, 0.5F}};
    float offsets[4] = {0};
    TEST_ASSERT_TRUE(cloth_pose_offsets(4, layers, 2, 0.04F, offsets));
    TEST_ASSERT_TRUE(offsets[0] == 0);
    TEST_ASSERT_TRUE(fabsf(offsets[1] - 0.025F) < 1e-6F);
    TEST_ASSERT_TRUE(fabsf(offsets[2] - 0.04F) < 1e-6F);
    TEST_ASSERT_TRUE(fabsf(offsets[3] + 0.04F) < 1e-6F);
    TEST_ASSERT_TRUE(cloth_pose_offsets(4, NULL, 0, 0.04F, offsets));
    for (int v = 0; v < 4; ++v) TEST_ASSERT_TRUE(offsets[v] == 0);
}

static void test_invalid_mask_or_activation_preserves_all_previous_offsets(void) {
    float mask[] = {0, 1, 0.5F}, offsets[] = {1, 2, 3};
    ClothPoseOffset layer = {mask, 0.02F, 0.5F};
    mask[2] = NAN;
    TEST_ASSERT_FALSE(cloth_pose_offsets(3, &layer, 1, 0.02F, offsets));
    mask[2] = 1.001F;
    TEST_ASSERT_FALSE(cloth_pose_offsets(3, &layer, 1, 0.02F, offsets));
    mask[2] = 0;
    layer.activation = 2;
    TEST_ASSERT_FALSE(cloth_pose_offsets(3, &layer, 1, 0.02F, offsets));
    TEST_ASSERT_TRUE(offsets[0] == 1 && offsets[1] == 2 && offsets[2] == 3);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_distance_blends_at_the_current_pose_in_either_direction);
    RUN_TEST(test_three_points_define_a_scale_independent_unsigned_angle);
    RUN_TEST(test_masked_offsets_cancel_before_clamping_and_do_not_accumulate_across_poses);
    RUN_TEST(test_invalid_mask_or_activation_preserves_all_previous_offsets);
    return UNITY_END();
}
