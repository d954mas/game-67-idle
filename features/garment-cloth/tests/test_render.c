#include "garment/render_binding.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const ClothFace faces[] = {{0,1,2}, {1,0,3}};
static const float rest[][3] = {{0,0,0}, {1,0,0}, {0,1,0}, {0,-2,0}};
static const ClothMesh panel = {.vertices=4, .faces=2, .face=faces};
static ClothRenderNormals field;

static void test_positive_and_negative_shell_clear_a_fold_with_a_clean_proxy(void) {
    ClothRenderBinding binding = {0, 0.5F, 0.0F, 0.0015F};
    TEST_ASSERT_TRUE(cloth_render_normals(&panel, rest, &field));
    float reference = cloth_render_normal_agreement(&binding, faces, &field);
    TEST_ASSERT_TRUE(reference == 1.0F);
    float pose[4][3], positive[3], negative[3];
    memcpy(pose, rest, sizeof pose);
    pose[3][1] = 0; pose[3][2] = 2;
    TEST_ASSERT_TRUE(cloth_render_normals(&panel, pose, &field));
    cloth_render_position_folded(&binding, faces, pose, &field, reference, positive);
    TEST_ASSERT_TRUE(positive[2] > 0 && positive[2] < binding.height_m);
    binding.height_m *= -1.0F;
    cloth_render_position_folded(&binding, faces, pose, &field, reference, negative);
    TEST_ASSERT_TRUE(negative[2] == -positive[2]);
    pose[3][1] = 2; pose[3][2] = 0;
    TEST_ASSERT_TRUE(cloth_render_normals(&panel, pose, &field));
    cloth_render_position_folded(&binding, faces, pose, &field, reference, negative);
    TEST_ASSERT_TRUE(negative[0] == 0.5F && negative[1] == 0 && negative[2] == 0);
}

static void test_rest_crease_and_extrapolated_bind_follow_a_rigid_pose(void) {
    const ClothRenderBinding binding = {0, -0.2F, 0.3F, 0.02F};
    float crease[4][3], pose[4][3], reference_position[3], result[3];
    memcpy(crease, rest, sizeof crease);
    crease[3][1] = 0; crease[3][2] = 2;
    TEST_ASSERT_TRUE(cloth_render_normals(&panel, crease, &field));
    float agreement = cloth_render_normal_agreement(&binding, faces, &field);
    TEST_ASSERT_TRUE(agreement > 0 && agreement < 1);
    cloth_render_position(&binding, faces, crease, reference_position);
    for (int i = 0; i < 4; ++i) {
        pose[i][0] = crease[i][2] + 2; pose[i][1] = crease[i][1] + 1; pose[i][2] = -crease[i][0];
    }
    TEST_ASSERT_TRUE(cloth_render_normals(&panel, pose, &field));
    cloth_render_position_folded(&binding, faces, pose, &field, agreement, result);
    TEST_ASSERT_TRUE(fabsf(result[0] - (reference_position[2] + 2)) < 1e-6F);
    TEST_ASSERT_TRUE(fabsf(result[1] - (reference_position[1] + 1)) < 1e-6F);
    TEST_ASSERT_TRUE(fabsf(result[2] + reference_position[0]) < 1e-6F);
}

static void test_split_seams_do_not_share_fold_attenuation(void) {
    const ClothFace split[] = {{0,1,2}, {4,3,5}};
    const ClothMesh mesh = {.vertices=6, .faces=2, .face=split};
    const float p[][3] = {{0,0,0},{1,0,0},{0,1,0},{0,0,0},{1,0,0},{0,2,0}};
    const ClothRenderBinding binding = {0, 0.5F, 0.0F, -0.002F};
    float result[3];
    TEST_ASSERT_TRUE(cloth_render_normals(&mesh, p, &field));
    cloth_render_position_folded(&binding, split, p, &field, 1, result);
    TEST_ASSERT_TRUE(result[2] == binding.height_m);
}

static void test_collapsed_support_suppresses_thickness_and_nonfinite_pose_rejects_cache(void) {
    float pose[4][3], result[3];
    const ClothRenderBinding binding = {0, 0.2F, 0.3F, 0.02F};
    memcpy(pose, rest, sizeof pose);
    pose[2][1] = 0;
    TEST_ASSERT_TRUE(cloth_render_normals(&panel, pose, &field));
    cloth_render_position_folded(&binding, faces, pose, &field, 1.0F, result);
    TEST_ASSERT_TRUE(result[2] == 0 && result[0] == binding.u);
    pose[0][0] = NAN;
    TEST_ASSERT_FALSE(cloth_render_normals(&panel, pose, &field));
}

static void test_nearly_collinear_triangle_cannot_flip_its_shell_across_the_fold(void) {
    const ClothRenderBinding binding = {0, 0.2F, 0.3F, 0.02F};
    const ClothMesh mesh = {.vertices=3, .faces=1, .face=faces};
    float p[][3] = {{0,0,0}, {1,0,0}, {0,1e-12F,0}}, output[3];
    for (int sample = 0; sample < 2; ++sample) {
        TEST_ASSERT_TRUE(cloth_render_normals(&mesh, p, &field));
        TEST_ASSERT_TRUE(cloth_render_normal_agreement(&binding, faces, &field) == 0);
        cloth_render_position_folded(&binding, faces, p, &field, 1, output);
        TEST_ASSERT_TRUE(output[0] == binding.u && output[1] == binding.v*p[2][1] && output[2] == 0);
        p[2][1] *= -1;
    }
    p[1][0] = 1e-12F;
    TEST_ASSERT_TRUE(cloth_render_normals(&mesh, p, &field));
    TEST_ASSERT_TRUE(cloth_render_normal_agreement(&binding, faces, &field) == 1);
}

static void test_opposing_faces_at_a_hairpin_cancel_continuously(void) {
    const ClothRenderBinding positive = {0, 0.5F, 0.0F, 0.02F}, negative = {0, 0.5F, 0.0F, -0.02F};
    const float heights[] = {0.999999F, 1.0F, 1.000001F};
    float pose[4][3];
    memcpy(pose, rest, sizeof pose);
    for (int i = 0; i < 3; ++i) {
        pose[3][1] = heights[i];
        float a[3], b[3];
        TEST_ASSERT_TRUE(cloth_render_normals(&panel, pose, &field));
        cloth_render_position_folded(&positive, faces, pose, &field, 1, a);
        cloth_render_position_folded(&negative, faces, pose, &field, 1, b);
        TEST_ASSERT_TRUE(a[2] >= 0 && a[2] < 1e-7F && b[2] == -a[2]);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_positive_and_negative_shell_clear_a_fold_with_a_clean_proxy);
    RUN_TEST(test_rest_crease_and_extrapolated_bind_follow_a_rigid_pose);
    RUN_TEST(test_split_seams_do_not_share_fold_attenuation);
    RUN_TEST(test_collapsed_support_suppresses_thickness_and_nonfinite_pose_rejects_cache);
    RUN_TEST(test_nearly_collinear_triangle_cannot_flip_its_shell_across_the_fold);
    RUN_TEST(test_opposing_faces_at_a_hairpin_cancel_continuously);
    return UNITY_END();
}
