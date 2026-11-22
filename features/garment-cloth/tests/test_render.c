#include "garment/render_binding.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const ClothFace faces[] = {{0,1,2}, {1,0,3}};
static const float rest[][3] = {{0,0,0}, {1,0,0}, {0,1,0}, {0,-2,0}};
static const ClothMesh panel = {.vertices=4, .faces=2, .rest=rest, .face=faces};
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

static void test_vertex_frames_identity_rigid_smooth_and_slit_isolated(void) {
    ClothVertexBasis basis[CLOTH_VERTICES];
    float mats[4][12];
    TEST_ASSERT_TRUE(cloth_vertex_frames_prepare(&panel, basis));
    TEST_ASSERT_TRUE(cloth_vertex_frames_eval(&panel, basis, rest, mats));
    /* Rest pose evaluates to exact identity matrices (0.0 m reconstruction error). */
    for (int v = 0; v < 4; ++v) {
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 4; ++c) {
                float want = (r == c) ? 1.0F : 0.0F;
                TEST_ASSERT_TRUE(fabsf(mats[v][r * 4 + c] - want) < 1e-6F);
            }
        }
    }
    /* Rigid 90-deg rotation + translation preserves points with thickness offset exactly. */
    float moved[4][3];
    for (int i = 0; i < 4; ++i) {
        moved[i][0] = rest[i][2] + 3.0F;
        moved[i][1] = rest[i][1] - 1.5F;
        moved[i][2] = -rest[i][0] + 0.5F;
    }
    TEST_ASSERT_TRUE(cloth_vertex_frames_eval(&panel, basis, moved, mats));
    float p_rest[3] = {0.25F, 0.25F, 0.02F};
    float p_out[3] = {0};
    float w[3] = {0.5F, 0.25F, 0.25F};
    int tri[3] = {0, 1, 2};
    for (int k = 0; k < 3; ++k) {
        const float *m = mats[tri[k]];
        for (int r = 0; r < 3; ++r)
            p_out[r] += w[k] * (m[r*4+0]*p_rest[0] + m[r*4+1]*p_rest[1] + m[r*4+2]*p_rest[2] + m[r*4+3]);
    }
    TEST_ASSERT_TRUE(fabsf(p_out[0] - (p_rest[2] + 3.0F)) < 1e-5F);
    TEST_ASSERT_TRUE(fabsf(p_out[1] - (p_rest[1] - 1.5F)) < 1e-5F);
    TEST_ASSERT_TRUE(fabsf(p_out[2] - (-p_rest[0] + 0.5F)) < 1e-5F);

    /* Bent panel: shared edge (0,1) produces identical blended transform from both faces. */
    float bent[4][3];
    memcpy(bent, rest, sizeof bent);
    bent[3][1] = -1.0F;
    bent[3][2] = 1.7320508F;
    TEST_ASSERT_TRUE(cloth_vertex_frames_eval(&panel, basis, bent, mats));
    float mid[3] = {0.5F, 0.0F, 0.005F};
    float from_f0[3] = {0}, from_f1[3] = {0};
    for (int r = 0; r < 3; ++r) {
        float m0 = mats[0][r*4+0]*mid[0] + mats[0][r*4+1]*mid[1] + mats[0][r*4+2]*mid[2] + mats[0][r*4+3];
        float m1 = mats[1][r*4+0]*mid[0] + mats[1][r*4+1]*mid[1] + mats[1][r*4+2]*mid[2] + mats[1][r*4+3];
        from_f0[r] = 0.5F * m0 + 0.5F * m1;
        from_f1[r] = 0.5F * m1 + 0.5F * m0;
        TEST_ASSERT_TRUE(fabsf(from_f0[r] - from_f1[r]) < 1e-6F);
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
    RUN_TEST(test_vertex_frames_identity_rigid_smooth_and_slit_isolated);
    return UNITY_END();
}
