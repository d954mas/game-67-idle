#include "garment/cloth.h"
#include "garment/contact.h"
#include "garment/render_binding.h"
#include "unity.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "cloth_fixtures.gen.h"

void setUp(void) {}
void tearDown(void) {}

static ClothState cloth;
static const ClothMaterial stiff = {.density = 0.2F, .drag = 2, .iterations = 6};
static const float dt = 1.0F / 120.0F;

static void test_material_restores_signed_fold(void) {
    float pose[4][3];
    memcpy(pose, fold.rest, sizeof pose);
    pose[3][1] = -0.5F;
    pose[3][2] = 0.8660254F;
    float before = atan2f(pose[3][2], -pose[3][1]);
    cloth_reset(&cloth, &fold, pose);
    TEST_ASSERT_EQUAL_PTR(&fold, cloth.mesh);
    cloth_step(&cloth, &fold, &stiff, dt, pose, NULL, NULL, 0);
    float after = atan2f(cloth.x[3][2], -cloth.x[3][1]);
    TEST_ASSERT_TRUE(after > before && fabsf(after - 1.5707963F) < 0.03F);
    ClothMetrics m = cloth_measure(&cloth, &fold, &stiff, dt, pose, NULL, 0);
    TEST_ASSERT_TRUE(m.finite && m.pin_error == 0);
}

static void test_capsule_axis_crosses_face_interior(void) {
    ClothCapsule body = {{0, 0.25F, -1}, {0, 0.25F, 1}, 0.1F};
    float bary[3], normal[3];
    float depth = cloth_face_contact(seam.rest[0], seam.rest[1], seam.rest[2], &body, 0, bary, normal);
    TEST_ASSERT_TRUE(fabsf(depth - body.radius) < 0.00001F);
    TEST_ASSERT_TRUE(bary[0] > 0 && bary[1] > 0 && bary[2] > 0);
    TEST_ASSERT_TRUE(fabsf(bary[0] + bary[1] + bary[2] - 1) < 0.00001F);
}

static void test_single_pinned_seam_contact_has_bounded_displacement(void) {
    ClothMaterial m = stiff;
    m.iterations = 1;
    ClothCapsule body = {{0, 0.001F, 0.002F}, {0, 0.001F, 0.002F}, 0.008F};
    cloth_reset(&cloth, &seam, seam.rest);
    cloth_step(&cloth, &seam, &m, dt, seam.rest, NULL, &body, 1);
    ClothMetrics residual = cloth_measure(&cloth, &seam, &m, dt, seam.rest, &body, 1);
    TEST_ASSERT_TRUE(residual.finite && residual.pin_error == 0 && residual.penetration > 0);
    TEST_ASSERT_TRUE(fabsf(cloth.x[2][2]) < 0.5F);
}

static void test_invalid_inputs_never_index_or_advance_state(void) {
    ClothMesh bad = seam;
    bad.vertices = CLOTH_VERTICES + 1;
    cloth_reset(&cloth, &bad, seam.rest);
    TEST_ASSERT_NULL(cloth.mesh);
    ClothFace face = {0, 1, UINT16_MAX};
    bad = seam;
    bad.face = &face;
    TEST_ASSERT_FALSE(cloth_mesh_valid(&bad));
    cloth_reset(&cloth, &seam, seam.rest);
    ClothMaterial bad_material = stiff;
    bad_material.drag = NAN;
    TEST_ASSERT_FALSE(cloth_frame_valid(&seam, &bad_material, dt, seam.rest, NULL, NULL, 0));
    cloth_step(&cloth, &seam, &bad_material, dt, seam.rest, NULL, NULL, 0);
    TEST_ASSERT_EQUAL_MEMORY(seam.rest, cloth.x, sizeof seam_rest);
    TEST_ASSERT_FALSE(cloth_measure(&cloth, &seam, &stiff, dt, seam.rest, NULL, 0).finite);
}

static void test_tether_limits_extension_and_preserves_slack(void) {
    ClothMaterial elastic = stiff;
    elastic.stretch = 100.0F;
    elastic.iterations = 1;
    float extended[3][3];
    memcpy(extended, seam.rest, sizeof extended);
    extended[2][1] *= 2.0F;
    ClothMesh untethered = seam;
    untethered.tether = NULL;
    cloth_reset(&cloth, &untethered, extended);
    cloth_step(&cloth, &untethered, &elastic, dt, seam.rest, NULL, NULL, 0);
    float free_strain = cloth_measure(&cloth, &untethered, &elastic, dt, seam.rest, NULL, 0).stretch;
    cloth_reset(&cloth, &seam, extended);
    cloth_step(&cloth, &seam, &elastic, dt, seam.rest, NULL, NULL, 0);
    ClothTether tether = seam.tether[2];
    const float *pin = cloth.x[tether.pin];
    float length = hypotf(cloth.x[2][0] - pin[0], cloth.x[2][1] - pin[1]);
    TEST_ASSERT_TRUE(fabsf(length - tether.max_length) < 0.00001F);
    TEST_ASSERT_TRUE(cloth_measure(&cloth, &seam, &elastic, dt, seam.rest, NULL, 0).stretch < free_strain);

    extended[2][1] = 0.4F;
    cloth_reset(&cloth, &untethered, extended);
    cloth_step(&cloth, &untethered, &elastic, dt, seam.rest, NULL, NULL, 0);
    float free_tip[3];
    memcpy(free_tip, cloth.x[2], sizeof free_tip);
    cloth_reset(&cloth, &seam, extended);
    cloth_step(&cloth, &seam, &elastic, dt, seam.rest, NULL, NULL, 0);
    TEST_ASSERT_EQUAL_MEMORY(free_tip, cloth.x[2], sizeof free_tip);
}

static void test_strain_limit_respects_pins_slack_and_material_validation(void) {
    ClothMesh mesh = seam;
    mesh.tether = NULL;
    ClothMaterial m = stiff;
    m.stretch = 100.0F;
    m.iterations = 1;
    float pose[3][3];
    memcpy(pose, seam.rest, sizeof pose);
    pose[2][1] *= 2.0F;
    cloth_reset(&cloth, &mesh, pose);
    cloth_step(&cloth, &mesh, &m, dt, seam.rest, NULL, NULL, 0);
    float initial_strain = cloth_measure(&cloth, &mesh, &m, dt, seam.rest, NULL, 0).stretch;
    m.max_edge_strain = 0.1F;
    m.strain_passes = 12;
    cloth_reset(&cloth, &mesh, pose);
    cloth_step(&cloth, &mesh, &m, dt, seam.rest, NULL, NULL, 0);
    ClothMetrics final = cloth_measure(&cloth, &mesh, &m, dt, seam.rest, NULL, 0);
    TEST_ASSERT_TRUE(final.finite && final.pin_error == 0 && final.stretch < initial_strain);
    TEST_ASSERT_TRUE(final.stretch <= m.max_edge_strain + 0.001F);

    pose[2][1] = 0.4F;
    cloth_reset(&cloth, &mesh, pose);
    cloth_step(&cloth, &mesh, &m, dt, seam.rest, NULL, NULL, 0);
    float tip[3];
    memcpy(tip, cloth.x[2], sizeof tip);
    m.strain_passes = 0;
    cloth_reset(&cloth, &mesh, pose);
    cloth_step(&cloth, &mesh, &m, dt, seam.rest, NULL, NULL, 0);
    TEST_ASSERT_EQUAL_MEMORY(tip, cloth.x[2], sizeof tip);
    m.strain_passes = CLOTH_MAX_STRAIN_PASSES + 1;
    TEST_ASSERT_FALSE(cloth_frame_valid(&mesh, &m, dt, seam.rest, NULL, NULL, 0));
    m.strain_passes = 0;
    m.max_edge_strain = NAN;
    TEST_ASSERT_FALSE(cloth_frame_valid(&mesh, &m, dt, seam.rest, NULL, NULL, 0));
}

static void test_strain_sweeps_precede_final_shape_envelope(void) {
    ClothMesh mesh = seam;
    ClothMaterial m = stiff;
    m.strain_passes = 4;
    m.max_edge_strain = 0.1F;
    float pose[3][3];
    memcpy(pose, seam.rest, sizeof pose);
    pose[2][1] *= 2.0F;
    ClothGuide guides[3] = {
        {{0,0,1},0,0}, {{0,0,1},0,0}, {{0,0,1},0,0}
    };
    ClothCapsule obstacle = {{0,0.7F,0.05F},{0,0.8F,0.05F},0.2F};
    cloth_reset(&cloth, &mesh, pose);
    cloth_step(&cloth, &mesh, &m, dt, pose, guides, &obstacle, 1);
    TEST_ASSERT_TRUE(cloth.frame_valid && cloth.x[2][1] < pose[2][1]);
    TEST_ASSERT_TRUE(cloth.x[2][2] < 0);
    float penetration = cloth_measure(&cloth, &mesh, &m, dt, pose, &obstacle, 1).penetration;
    cloth_reset(&cloth, &mesh, pose);
    cloth_step_shaped(&cloth, &mesh, &m, dt, pose, guides, &obstacle, 1);
    TEST_ASSERT_TRUE(cloth.frame_valid);
    TEST_ASSERT_EQUAL_MEMORY(pose, cloth.x, sizeof pose);
    TEST_ASSERT_TRUE(cloth_measure(&cloth, &mesh, &m, dt, pose, &obstacle, 1).penetration > penetration);
}

static void test_render_offset_follows_the_deformed_face_normal(void) {
    const float rotated[][3] = {{1,2,3},{1,2,4},{1,3,3}};
    const ClothRenderBinding binding = {.face=0, .u=0.25F, .v=0.5F, .height_m=0.03F};
    float out[3];
    cloth_render_position(&binding, seam.face, rotated, out);
    TEST_ASSERT_TRUE(fabsf(out[0]-0.97F) < 0.00001F);
    TEST_ASSERT_TRUE(fabsf(out[1]-2.5F) < 0.00001F);
    TEST_ASSERT_TRUE(fabsf(out[2]-3.25F) < 0.00001F);
}

static void test_shaped_envelope_wins_over_capsule_and_tracks_pose(void) {
    float pose[3][3];
    ClothGuide guide[3] = {
        {{0, 0, 1}, 0, 0}, {{0, 0, 1}, 0, 0}, {{0, 0, 0.9998F}, 0.15F, 0.004F}
    };
    ClothMaterial m = stiff;
    m.iterations = 1;
    memcpy(pose, seam.rest, sizeof pose);
    pose[0][0] += 0.03F;
    pose[2][2] += 0.02F;
    ClothCapsule capsule = {{0, 1, 0.03F}, {0, 1, 0.03F}, 0.1F};
    cloth_reset(&cloth, &seam, seam.rest);
    cloth_step(&cloth, &seam, &m, dt, pose, guide, &capsule, 1);
    TEST_ASSERT_TRUE(pose[2][2] - cloth.x[2][2] > guide[2].backstop);

    cloth_reset(&cloth, &seam, seam.rest);
    for (int frame = 0; frame < 3; ++frame) {
        if (frame == 1) {
            guide[2].travel = 0.007F;
            guide[2].backstop = 0.10F;
        }
        if (frame == 2) guide[2].travel = 0;
        pose[0][1] += 0.003F;
        pose[2][0] += 0.001F;
        cloth_step_shaped(&cloth, &seam, &m, dt, pose, guide, &capsule, 1);
        TEST_ASSERT_TRUE(cloth.frame_valid);
        TEST_ASSERT_EQUAL_MEMORY(pose[0], cloth.x[0], sizeof pose[0]);
        TEST_ASSERT_EQUAL_MEMORY(pose[1], cloth.x[1], sizeof pose[1]);
        double radius2 = 0;
        for (int k = 0; k < 3; ++k) {
            double d = (double)cloth.x[2][k] - (double)pose[2][k];
            radius2 += d * d;
        }
        TEST_ASSERT_TRUE(radius2 <= (double)guide[2].travel * (double)guide[2].travel);
        TEST_ASSERT_TRUE((double)pose[2][2] - (double)cloth.x[2][2] <= (double)guide[2].backstop);
        ClothMetrics residual = cloth_measure(&cloth, &seam, &m, dt, pose, &capsule, 1);
        TEST_ASSERT_TRUE(residual.finite && residual.penetration > 0 && residual.pin_error == 0);
    }
}

static void test_shaped_null_guide_matches_ordinary_and_bad_guide_rejects_frame(void) {
    static ClothState ordinary;
    ClothCapsule body = {{0, 1, 0.03F}, {0, 1, 0.03F}, 0.1F};
    cloth_reset(&ordinary, &seam, seam.rest);
    cloth_reset(&cloth, &seam, seam.rest);
    for (int i = 0; i < 3; ++i) {
        cloth_step(&ordinary, &seam, &stiff, dt, seam.rest, NULL, &body, 1);
        cloth_step_shaped(&cloth, &seam, &stiff, dt, seam.rest, NULL, &body, 1);
    }
    TEST_ASSERT_EQUAL_MEMORY(ordinary.x, cloth.x, sizeof seam_rest);
    TEST_ASSERT_EQUAL_MEMORY(ordinary.previous, cloth.previous, sizeof seam_rest);
    ClothGuide guide[3] = {
        {{0, 0, 1}, 1, 1}, {{0, 0, 1}, 1, 1}, {{0, 0, 1}, NAN, 0}
    };
    cloth_step_shaped(&cloth, &seam, &stiff, dt, seam.rest, guide, &body, 1);
    TEST_ASSERT_FALSE(cloth.frame_valid);
    TEST_ASSERT_EQUAL_MEMORY(ordinary.x, cloth.x, sizeof seam_rest);
    TEST_ASSERT_EQUAL_MEMORY(ordinary.previous, cloth.previous, sizeof seam_rest);
}

static void test_shaped_backstop_holds_after_world_coordinate_rounding(void) {
    float pose[3][3];
    memcpy(pose, seam.rest, sizeof pose);
    for (int i = 0; i < 3; ++i) {
        pose[i][0] += 257.3F;
        pose[i][2] += 130.1F;
    }
    ClothGuide guide[3] = {
        {{0, 0, 1}, 0, 0}, {{0, 0, 1}, 0, 0}, {{0.6F, 0, 0.8F}, 0.003F, 0}
    };
    ClothCapsule capsule = {{0}, {0}, 0.1F};
    memcpy(capsule.a, pose[2], sizeof capsule.a);
    capsule.a[2] += 0.01F;
    memcpy(capsule.b, capsule.a, sizeof capsule.b);
    cloth_reset(&cloth, &seam, pose);
    cloth_step_shaped(&cloth, &seam, &stiff, dt, pose, guide, &capsule, 1);
    double d[3];
    for (int k = 0; k < 3; ++k) d[k] = (double)cloth.x[2][k] - (double)pose[2][k];
    double r2 = d[0]*d[0] + d[1]*d[1] + d[2]*d[2];
    TEST_ASSERT_TRUE(cloth.frame_valid && r2 > 0 && r2 <= (double)guide[2].travel * (double)guide[2].travel);
    TEST_ASSERT_TRUE(d[0] * (double)guide[2].normal[0] + d[2] * (double)guide[2].normal[2] >= 0);
}

static void test_surface_offset_uses_posed_areas_and_preserves_history_and_pins(void) {
    ClothMesh mesh = fold;
    const float inverse[] = {1, 0, 1, 1};
    mesh.inv_area = inverse;
    float pose[4][3], output[4][3];
    memcpy(pose, fold.rest, sizeof pose);
    pose[3][2] = 2;
    const float offsets[] = {0.20F, -0.30F, -0.05F, 0};
    cloth_reset(&cloth, &mesh, pose);
    TEST_ASSERT_TRUE(cloth_offset_surface(&mesh, cloth.x, offsets, output));
    TEST_ASSERT_EQUAL_MEMORY(pose, cloth.x, sizeof pose);
    TEST_ASSERT_EQUAL_MEMORY(pose, cloth.previous, sizeof pose);
    TEST_ASSERT_TRUE(fabsf(output[0][1] - 0.40F / sqrtf(5.0F)) < 1e-6F);
    TEST_ASSERT_TRUE(fabsf(output[0][2] - 0.20F / sqrtf(5.0F)) < 1e-6F);
    TEST_ASSERT_EQUAL_MEMORY(pose[1], output[1], sizeof pose[1]);
    TEST_ASSERT_TRUE(fabsf(output[2][2] + 0.05F) < 1e-6F);
    TEST_ASSERT_EQUAL_MEMORY(pose[3], output[3], sizeof pose[3]);
}

static void test_surface_offset_falls_back_as_a_whole_when_a_normal_is_undefined(void) {
    ClothMesh mesh = fold;
    const float inverse[] = {1, 0, 1, 1};
    mesh.inv_area = inverse;
    float pose[4][3], output[4][3];
    memcpy(pose, fold.rest, sizeof pose);
    memset(pose[3], 0, sizeof pose[3]);
    float offsets[] = {0.1F, 1, 0, 0};
    TEST_ASSERT_TRUE(cloth_offset_surface(&mesh, pose, offsets, output));
    TEST_ASSERT_TRUE(fabsf(output[0][2] - 0.1F) < 1e-6F);
    offsets[3] = 0.1F;
    TEST_ASSERT_FALSE(cloth_offset_surface(&mesh, pose, offsets, output));
    TEST_ASSERT_EQUAL_MEMORY(pose, output, sizeof pose);
    memcpy(pose[3], pose[2], sizeof pose[3]);
    TEST_ASSERT_FALSE(cloth_offset_surface(&mesh, pose, offsets, output));
    TEST_ASSERT_EQUAL_MEMORY(pose, output, sizeof pose);
    memset(offsets, 0, sizeof offsets);
    pose[2][0] = -0.0F;
    TEST_ASSERT_TRUE(cloth_offset_surface(&mesh, pose, offsets, output));
    TEST_ASSERT_EQUAL_MEMORY(pose, output, sizeof pose);
    for (int i = 0; i < 4; ++i) pose[i][2] = FLT_MAX;
    offsets[2] = FLT_MAX;
    TEST_ASSERT_FALSE(cloth_offset_surface(&mesh, pose, offsets, output));
    TEST_ASSERT_EQUAL_MEMORY(pose, output, sizeof pose);
}

static void test_surface_offset_rejects_overlaps_and_nonfinite_inputs_before_writing(void) {
    float storage[5][3] = {{0}};
    float offsets[] = {0, 0, 0, 0};
    memcpy(storage, fold.rest, sizeof fold_rest);
    TEST_ASSERT_FALSE(cloth_offset_surface(&fold, storage, offsets, storage + 1));
    TEST_ASSERT_EQUAL_MEMORY(fold.rest, storage, sizeof fold_rest);
    TEST_ASSERT_FALSE(cloth_offset_surface(&fold, fold.rest, storage[0], storage));
    TEST_ASSERT_EQUAL_MEMORY(fold.rest, storage, sizeof fold_rest);
    offsets[2] = NAN;
    TEST_ASSERT_FALSE(cloth_offset_surface(&fold, fold.rest, offsets, storage));
    TEST_ASSERT_EQUAL_MEMORY(fold.rest, storage, sizeof fold_rest);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_material_restores_signed_fold);
    RUN_TEST(test_capsule_axis_crosses_face_interior);
    RUN_TEST(test_single_pinned_seam_contact_has_bounded_displacement);
    RUN_TEST(test_invalid_inputs_never_index_or_advance_state);
    RUN_TEST(test_tether_limits_extension_and_preserves_slack);
    RUN_TEST(test_strain_limit_respects_pins_slack_and_material_validation);
    RUN_TEST(test_strain_sweeps_precede_final_shape_envelope);
    RUN_TEST(test_render_offset_follows_the_deformed_face_normal);
    RUN_TEST(test_shaped_envelope_wins_over_capsule_and_tracks_pose);
    RUN_TEST(test_shaped_null_guide_matches_ordinary_and_bad_guide_rejects_frame);
    RUN_TEST(test_shaped_backstop_holds_after_world_coordinate_rounding);
    RUN_TEST(test_surface_offset_uses_posed_areas_and_preserves_history_and_pins);
    RUN_TEST(test_surface_offset_falls_back_as_a_whole_when_a_normal_is_undefined);
    RUN_TEST(test_surface_offset_rejects_overlaps_and_nonfinite_inputs_before_writing);
    return UNITY_END();
}
