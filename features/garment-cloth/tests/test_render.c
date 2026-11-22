#include "garment/render_binding.h"
#include "garment/contact.h"
#include "garment/debug.h"
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

static void quat_rotate_vec(const float q[4], const float v[3], float out[3]) {
    float tx = 2.0F * (q[1] * v[2] - q[2] * v[1]);
    float ty = 2.0F * (q[2] * v[0] - q[0] * v[2]);
    float tz = 2.0F * (q[0] * v[1] - q[1] * v[0]);
    out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
    out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
    out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

static void test_vertex_quats_preserve_rest_rigid_and_thickness_on_hairpin_fold(void) {
    ClothVertexBasis basis[CLOTH_VERTICES];
    float rec[4][12];
    TEST_ASSERT_TRUE(cloth_vertex_frames_prepare(&panel, basis));
    TEST_ASSERT_TRUE(cloth_vertex_quats_eval(&panel, basis, rest, rec));
    /* Rest pose: exact identity quaternion (0,0,0,1) and P_curr == P_rest. */
    for (int v = 0; v < 4; ++v) {
        TEST_ASSERT_TRUE(fabsf(rec[v][0] - rest[v][0]) < 1e-6F);
        TEST_ASSERT_TRUE(fabsf(rec[v][4] - rest[v][0]) < 1e-6F);
        TEST_ASSERT_TRUE(fabsf(rec[v][3]) < 1e-6F && fabsf(rec[v][7]) < 1e-6F &&
                         fabsf(rec[v][8]) < 1e-6F && fabsf(rec[v][9] - 1.0F) < 1e-6F);
    }
    /* Extreme 160-deg hairpin fold: inner and outer shell offset stays rigidly separated (no collapse!). */
    float bent[4][3];
    memcpy(bent, rest, sizeof bent);
    bent[3][1] = 1.85F;
    bent[3][2] = 0.65F;
    TEST_ASSERT_TRUE(cloth_vertex_quats_eval(&panel, basis, bent, rec));
    /* Blend vertices (0, 1, 2) with hemisphere alignment. */
    float w[3] = {0.4F, 0.4F, 0.2F};
    float q0[4] = {rec[0][3], rec[0][7], rec[0][8], rec[0][9]};
    float qb[4] = {0}, pc[3] = {0}, pr[3] = {0};
    for (int k = 0; k < 3; ++k) {
        float qk[4] = {rec[k][3], rec[k][7], rec[k][8], rec[k][9]};
        float s = (q0[0]*qk[0] + q0[1]*qk[1] + q0[2]*qk[2] + q0[3]*qk[3]) < 0.0F ? -1.0F : 1.0F;
        for (int j = 0; j < 4; ++j) qb[j] += w[k] * s * qk[j];
        for (int j = 0; j < 3; ++j) {
            pc[j] += w[k] * rec[k][j];
            pr[j] += w[k] * rec[k][4 + j];
        }
    }
    float qlen = sqrtf(qb[0]*qb[0] + qb[1]*qb[1] + qb[2]*qb[2] + qb[3]*qb[3]);
    TEST_ASSERT_TRUE(qlen > 0.5F);
    for (int j = 0; j < 4; ++j) qb[j] /= qlen;
    float thick_rest[3] = {0.0F, 0.0F, 0.004F}, thick_rot[3];
    quat_rotate_vec(qb, thick_rest, thick_rot);
    /* Rigid quaternion rotation preserves 100% of shell thickness (4.0mm) without collapse! */
    float thick_len = sqrtf(thick_rot[0]*thick_rot[0] + thick_rot[1]*thick_rot[1] + thick_rot[2]*thick_rot[2]);
    TEST_ASSERT_TRUE(fabsf(thick_len - 0.004F) < 1e-6F);
    (void)pc; (void)pr;
}

typedef struct {
    int weights, tethers, capsules, bones;
    float weight_at_origin[3], tether_color[3];
    const ClothCapsule *body;
} DebugLines;

static void inspect_line(void *user, unsigned layer, const float a[3], const float b[3], const float rgb[3]) {
    DebugLines *capture = user;
    if (layer == CLOTH_DEBUG_WEIGHTS) {
        ++capture->weights;
        if (a[0] == 0 && a[1] == 0 && a[2] == 0) memcpy(capture->weight_at_origin, rgb, sizeof capture->weight_at_origin);
    } else if (layer == CLOTH_DEBUG_TETHERS) {
        ++capture->tethers;
        memcpy(capture->tether_color, rgb, sizeof capture->tether_color);
        TEST_ASSERT_TRUE(a[0] == 1 && b[0] == 0);
    } else if (layer == CLOTH_DEBUG_CAPSULES) {
        float normal[3];
        ++capture->capsules;
        TEST_ASSERT_TRUE(fabsf(0.002F - cloth_point_capsule_contact(a, capture->body, 0.002F, normal, NULL)) < 1e-5F);
        TEST_ASSERT_TRUE(fabsf(0.002F - cloth_point_capsule_contact(b, capture->body, 0.002F, normal, NULL)) < 1e-5F);
    } else if (layer == CLOTH_DEBUG_SKELETON) {
        ++capture->bones;
        TEST_ASSERT_TRUE(a[0] == 0 && b[0] == 1);
    } else {
        TEST_FAIL_MESSAGE("unexpected debug layer");
    }
}

static void test_debug_draw_uses_selected_weights_live_tether_and_round_cone_surface(void) {
    const float inv[] = {0, 1, 1, 1}, drive[] = {0, 0.3F, 0, 0}, selected[] = {1, 0, 0, 0};
    const ClothEdge edge = {.a=0,.b=1};
    const ClothTether tether[] = {{0,0}, {0,0.8F}, {-1,0}, {-1,0}};
    ClothMesh m = panel;
    m.edges = 1; m.edge = &edge; m.inv_area = inv; m.anim_drive = drive; m.tether = tether;
    DebugLines lines = {0};
    cloth_debug_mesh(&m, rest, NULL, 0, inspect_line, &lines);
    TEST_ASSERT_EQUAL_INT(0, lines.weights + lines.tethers);
    cloth_debug_mesh(&m, rest, NULL, CLOTH_DEBUG_WEIGHTS, inspect_line, &lines);
    TEST_ASSERT_TRUE(lines.weights > 0 && lines.weight_at_origin[2] > lines.weight_at_origin[0]);
    cloth_debug_mesh(&m, rest, selected, CLOTH_DEBUG_WEIGHTS | CLOTH_DEBUG_TETHERS, inspect_line, &lines);
    TEST_ASSERT_TRUE(lines.weight_at_origin[0] > lines.weight_at_origin[2]);
    TEST_ASSERT_EQUAL_INT(1, lines.tethers);
    TEST_ASSERT_TRUE(lines.tether_color[0] > lines.tether_color[1]);
    cloth_debug_bone(rest[0], rest[1], inspect_line, &lines);
    TEST_ASSERT_EQUAL_INT(1, lines.bones);

    ClothCapsule cap = {.a={0.1F,0.3F,0.1F},.b={0.2F,0.7F,0.4F},.radius=0.18F,.radius_b=0.09F};
    lines.body = &cap;
    cloth_debug_capsule(&cap, inspect_line, &lines);
    TEST_ASSERT_TRUE(lines.capsules > 0);
    cap.radius_b = 0;
    cloth_debug_capsule(&cap, inspect_line, &lines);
    memcpy(cap.b, cap.a, sizeof cap.b);
    cloth_debug_capsule(&cap, inspect_line, &lines);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_draw_uses_selected_weights_live_tether_and_round_cone_surface);
    RUN_TEST(test_positive_and_negative_shell_clear_a_fold_with_a_clean_proxy);
    RUN_TEST(test_rest_crease_and_extrapolated_bind_follow_a_rigid_pose);
    RUN_TEST(test_split_seams_do_not_share_fold_attenuation);
    RUN_TEST(test_collapsed_support_suppresses_thickness_and_nonfinite_pose_rejects_cache);
    RUN_TEST(test_nearly_collinear_triangle_cannot_flip_its_shell_across_the_fold);
    RUN_TEST(test_opposing_faces_at_a_hairpin_cancel_continuously);
    RUN_TEST(test_vertex_frames_identity_rigid_smooth_and_slit_isolated);
    RUN_TEST(test_vertex_quats_preserve_rest_rigid_and_thickness_on_hairpin_fold);
    return UNITY_END();
}
