#include "garment/cloth.h"
#include "garment/contact.h"
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
    cloth_step(&cloth, &fold, &stiff, dt, NULL, pose, NULL, NULL, 0);
    float after = atan2f(cloth.x[3][2], -cloth.x[3][1]);
    TEST_ASSERT_TRUE(after > before && fabsf(after - 1.5707963F) < 0.03F);
    ClothMetrics m = cloth_measure(&cloth, &fold, &stiff, dt, pose, NULL, 0);
    TEST_ASSERT_TRUE(m.finite && m.pin_error == 0);
}

static void test_capsule_axis_crosses_face_interior(void) {
    ClothCapsule body = {.a = {0, 0.25F, -1}, .b = {0, 0.25F, 1}, .radius = 0.1F};
    float bary[3], normal[3];
    float depth = cloth_face_contact(seam.rest[0], seam.rest[1], seam.rest[2], &body, 0, bary, normal);
    TEST_ASSERT_TRUE(fabsf(depth - body.radius) < 0.00001F);
    TEST_ASSERT_TRUE(bary[0] > 0 && bary[1] > 0 && bary[2] > 0);
    TEST_ASSERT_TRUE(fabsf(bary[0] + bary[1] + bary[2] - 1) < 0.00001F);
}

static void test_single_pinned_seam_contact_has_bounded_displacement(void) {
    ClothMaterial m = stiff;
    m.iterations = 1;
    ClothCapsule body = {.a = {0, 0.001F, 0.002F}, .b = {0, 0.001F, 0.002F}, .radius = 0.008F};
    cloth_reset(&cloth, &seam, seam.rest);
    cloth_step(&cloth, &seam, &m, dt, NULL, seam.rest, NULL, &body, 1);
    ClothMetrics residual = cloth_measure(&cloth, &seam, &m, dt, seam.rest, &body, 1);
    TEST_ASSERT_TRUE(residual.finite && residual.pin_error == 0 && residual.penetration > 0);
    TEST_ASSERT_TRUE(fabsf(cloth.x[2][2]) < 0.5F);
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
    cloth_step(&cloth, &untethered, &elastic, dt, NULL, seam.rest, NULL, NULL, 0);
    float free_strain = cloth_measure(&cloth, &untethered, &elastic, dt, seam.rest, NULL, 0).stretch;
    cloth_reset(&cloth, &seam, extended);
    cloth_step(&cloth, &seam, &elastic, dt, NULL, seam.rest, NULL, NULL, 0);
    ClothTether tether = seam.tether[2];
    const float *pin = cloth.x[tether.pin];
    float length = hypotf(cloth.x[2][0] - pin[0], cloth.x[2][1] - pin[1]);
    TEST_ASSERT_TRUE(fabsf(length - tether.max_length) < 0.00001F);
    TEST_ASSERT_TRUE(cloth_measure(&cloth, &seam, &elastic, dt, seam.rest, NULL, 0).stretch < free_strain);

    extended[2][1] = 0.4F;
    cloth_reset(&cloth, &untethered, extended);
    cloth_step(&cloth, &untethered, &elastic, dt, NULL, seam.rest, NULL, NULL, 0);
    float free_tip[3];
    memcpy(free_tip, cloth.x[2], sizeof free_tip);
    cloth_reset(&cloth, &seam, extended);
    cloth_step(&cloth, &seam, &elastic, dt, NULL, seam.rest, NULL, NULL, 0);
    TEST_ASSERT_EQUAL_MEMORY(free_tip, cloth.x[2], sizeof free_tip);
}

static void test_strain_limit_respects_pins_and_slack(void) {
    ClothMesh mesh = seam;
    mesh.tether = NULL;
    ClothMaterial m = stiff;
    m.stretch = 100.0F;
    m.iterations = 1;
    float pose[3][3];
    memcpy(pose, seam.rest, sizeof pose);
    pose[2][1] *= 2.0F;
    cloth_reset(&cloth, &mesh, pose);
    cloth_step(&cloth, &mesh, &m, dt, NULL, seam.rest, NULL, NULL, 0);
    float initial_strain = cloth_measure(&cloth, &mesh, &m, dt, seam.rest, NULL, 0).stretch;
    m.max_edge_strain = 0.1F;
    m.strain_passes = 12;
    cloth_reset(&cloth, &mesh, pose);
    cloth_step(&cloth, &mesh, &m, dt, NULL, seam.rest, NULL, NULL, 0);
    ClothMetrics final = cloth_measure(&cloth, &mesh, &m, dt, seam.rest, NULL, 0);
    TEST_ASSERT_TRUE(final.finite && final.pin_error == 0 && final.stretch < initial_strain);
    TEST_ASSERT_TRUE(final.stretch <= m.max_edge_strain + 0.001F);

    pose[2][1] = 0.4F;
    cloth_reset(&cloth, &mesh, pose);
    cloth_step(&cloth, &mesh, &m, dt, NULL, seam.rest, NULL, NULL, 0);
    float tip[3];
    memcpy(tip, cloth.x[2], sizeof tip);
    m.strain_passes = 0;
    cloth_reset(&cloth, &mesh, pose);
    cloth_step(&cloth, &mesh, &m, dt, NULL, seam.rest, NULL, NULL, 0);
    TEST_ASSERT_EQUAL_MEMORY(tip, cloth.x[2], sizeof tip);
}

static void test_strain_cannot_pull_a_free_vertex_into_the_body(void) {
    ClothMaterial material = {.density = 0.2F, .stretch = 100.0F, .iterations = 1, .max_edge_strain = 0.1F};
    ClothCapsule body = {.a = {0.024F, 0.061F, -0.002F}, .b = {0.024F, 0.061F, -0.002F}, .radius = 0.05F};
    float pose[3][3], normal[3], t;
    memcpy(pose, small_seam.rest, sizeof pose);
    pose[2][0] = 0.04F;
    pose[2][1] = 0.12F;
    for (int v = 0; v < 3; ++v)
        TEST_ASSERT_TRUE(cloth_point_capsule_contact(pose[v], &body, 0, normal, &t) == 0.0F);
    for (int passes = 0; passes <= 3; ++passes) {
        material.strain_passes = passes;
        cloth_reset(&cloth, &small_seam, pose);
        cloth_step(&cloth, &small_seam, &material, dt, NULL, pose, NULL, &body, 1);
        TEST_ASSERT_TRUE(cloth.frame_valid);
        TEST_ASSERT_EQUAL_MEMORY(pose, cloth.x, 2 * sizeof pose[0]);
        TEST_ASSERT_TRUE(cloth_point_capsule_contact(cloth.x[2], &body, 0, normal, &t) <= 1e-6F);
    }
}

static void test_reset_rejects_excess_capsules_and_preserves_coordinates(void) {
    ClothCapsule body[CLOTH_CAPSULES + 1] = {0};
    TEST_ASSERT_TRUE(cloth_reset_supported(&cloth, &seam, &stiff, seam.rest, NULL, NULL, 0, NULL));
    cloth.x[2][1] += 0.25F;
    float y = cloth.x[2][1];
    TEST_ASSERT_TRUE(cloth_frame_valid(&seam, &stiff, dt, seam.rest, NULL, body, CLOTH_CAPSULES));
    TEST_ASSERT_FALSE(cloth_reset_supported(&cloth, &seam, &stiff, seam.rest, NULL, body, CLOTH_CAPSULES + 1, NULL));
    TEST_ASSERT_FALSE(cloth.frame_valid);
    TEST_ASSERT_TRUE(y == cloth.x[2][1]);
}

static void test_round_cone_tapered_capsule_geometry_and_degeneracy(void) {
    /* Tapered limb from y=0 (r=0.2) to y=1 (r=0.1). Slant normal tilts upward (+Y). */
    ClothCapsule cone = {.a = {0, 0, 0}, .b = {0, 1, 0}, .radius = 0.2F, .radius_b = 0.1F};
    float n[3], t = -1.0F;
    float p_mid[3] = {0.10F, 0.5F, 0.0F};
    float depth = cloth_point_capsule_contact(p_mid, &cone, 0.0F, n, &t);
    /* Exact signed distance at (x=0.1, y=0.5): (0.1*sqrt(0.99) + 0.5*0.1)/1 - 0.2 = -0.050501256 */
    float a = sqrtf(1.0F - 0.01F);
    float expected_depth = 0.2F - (0.1F * a + 0.5F * 0.1F);
    TEST_ASSERT_TRUE(fabsf(depth - expected_depth) < 1e-5F);
    TEST_ASSERT_TRUE(n[0] > 0.99F && n[1] > 0.09F && fabsf(n[2]) < 1e-5F);
    TEST_ASSERT_TRUE(t > 0.0F && t < 1.0F);
    /* Near narrow cap: point outside narrow radius (0.12 > 0.10) must not collide. */
    float p_narrow_out[3] = {0.12F, 1.0F, 0.0F};
    TEST_ASSERT_TRUE(cloth_point_capsule_contact(p_narrow_out, &cone, 0.0F, n, &t) == 0.0F);
    /* Near wide cap: same radial distance 0.12 < 0.20 must collide. */
    float p_wide_in[3] = {0.12F, 0.0F, 0.0F};
    TEST_ASSERT_TRUE(cloth_point_capsule_contact(p_wide_in, &cone, 0.0F, n, &t) > 0.07F);
    /* Degenerate containment (|r_a - r_b| >= L): reduces cleanly to dominating sphere without NaN. */
    ClothCapsule nested = {.a = {0, 0, 0}, .b = {0, 0.05F, 0}, .radius = 0.2F, .radius_b = 0.05F};
    float d_nested = cloth_point_capsule_contact((float[]){0.1F, 0.0F, 0.0F}, &nested, 0.0F, n, &t);
    TEST_ASSERT_TRUE(isfinite(d_nested) && fabsf(d_nested - 0.1F) < 1e-5F);
}

static void test_temporal_side_recovery_prevents_tunneling_across_axis(void) {
    /* Leg sweeps in +Z past a vertex that started on the +Z side and ended slightly past axis (-Z). */
    ClothCapsule leg = {
        .a = {0, 0, 0.05F}, .b = {0, 1, 0.05F}, .radius = 0.08F, .radius_b = 0.06F,
        .prev_a = {0, 0, -0.05F}, .prev_b = {0, 1, -0.05F},
        .flags = CLOTH_COLLIDER_INSIDE, .has_prev = 1
    };
    float prev_p[3] = {0, 0.5F, 0.04F};  /* +0.09m in front of prev axis */
    float curr_p[3] = {0, 0.5F, 0.03F};  /* -0.02m behind curr axis (crossed center!) */
    float n[3], t = 0.0F;
    float depth = cloth_point_capsule_contact_swept(curr_p, prev_p, &leg, 0.0F, NULL, n, &t);
    /* Must push back toward +Z (previous side), NOT -Z, and exit exactly onto r_local = 0.07. */
    TEST_ASSERT_TRUE(n[2] > 0.99F);
    float exited_z = curr_p[2] + depth * n[2];
    TEST_ASSERT_TRUE(exited_z > leg.a[2] + 0.065F && exited_z < leg.a[2] + 0.075F);
}

static void test_reference_frame_inertia_and_aero_drag(void) {
    /* 1. Aero drag damps normal velocity without slowing tangential vertical fall. */
    ClothMaterial low_drag = {.density = 0.2F, .stretch = 0.00001F, .drag = 0.1F, .aero_drag = 0.0F, .iterations = 2};
    ClothMaterial high_aero = low_drag;
    high_aero.aero_drag = 12.0F;

    /* Vertical hanging panel in XY plane (normal along +Z). */
    float vert_rest[3][3] = {{-0.2F, 1.0F, 0}, {0.2F, 1.0F, 0}, {0.0F, 0.6F, 0}};
    float vert_inv[3] = {0, 0, 1};
    ClothFace vert_face = {0, 1, 2};
    ClothEdge vert_edges[3] = {
        {0, 1, 2, -1, 0.4F, 0, CLOTH_EDGE_WEFT},
        {0, 2, 1, -1, hypotf(0.2F, 0.4F), 0, CLOTH_EDGE_WARP},
        {1, 2, 0, -1, hypotf(0.2F, 0.4F), 0, CLOTH_EDGE_WARP}
    };
    ClothMesh panel = {.vertices = 3, .faces = 1, .edges = 3, .rest = vert_rest, .inv_area = vert_inv,
                       .face = &vert_face, .edge = vert_edges};
    static ClothState s_no_aero, s_aero;
    cloth_reset(&s_no_aero, &panel, vert_rest);
    cloth_reset(&s_aero, &panel, vert_rest);
    /* Give purely normal (+Z) flapping velocity: aero_drag must damp it strongly. */
    s_no_aero.velocity[2][2] = 2.0F;
    s_aero.velocity[2][2] = 2.0F;
    cloth_step(&s_no_aero, &panel, &low_drag, dt, NULL, vert_rest, NULL, NULL, 0);
    cloth_step(&s_aero, &panel, &high_aero, dt, NULL, vert_rest, NULL, NULL, 0);
    TEST_ASSERT_TRUE(s_aero.velocity[2][2] < s_no_aero.velocity[2][2] * 0.95F);

    /* 2. Reference frame linear acceleration pushes cloth in opposite direction scaled by linear_inertia. */
    ClothFrame frame_zero = {.linear_accel = {10.0F, 0, 0}, .linear_inertia = 0.0F};
    ClothFrame frame_half = {.linear_accel = {10.0F, 0, 0}, .linear_inertia = 0.5F};
    cloth_reset(&s_no_aero, &panel, vert_rest);
    cloth_reset(&s_aero, &panel, vert_rest);
    cloth_step(&s_no_aero, &panel, &low_drag, dt, &frame_zero, vert_rest, NULL, NULL, 0);
    cloth_step(&s_aero, &panel, &low_drag, dt, &frame_half, vert_rest, NULL, NULL, 0);
    TEST_ASSERT_TRUE(s_aero.x[2][0] < s_no_aero.x[2][0]);
}

static void test_drive_converges_toward_the_target_independently_of_iterations(void) {
    float drive[3] = {0.0F, 0.0F, 400.0F};
    ClothMesh mesh = seam;
    mesh.drive = drive;
    ClothMaterial m2 = stiff;
    m2.iterations = 2;
    ClothMaterial m8 = m2;
    m8.iterations = 8;
    float displaced[3][3];
    memcpy(displaced, seam.rest, sizeof displaced);
    displaced[2][2] = 0.25F;
    static ClothState s2, s8;
    cloth_reset(&s2, &mesh, displaced);
    cloth_reset(&s8, &mesh, displaced);
    cloth_step(&s2, &mesh, &m2, dt, NULL, seam.rest, NULL, NULL, 0);
    cloth_step(&s8, &mesh, &m8, dt, NULL, seam.rest, NULL, NULL, 0);
    TEST_ASSERT_TRUE(s2.x[2][2] < displaced[2][2] - 0.005F);
    TEST_ASSERT_TRUE(fabsf(s2.x[2][2] - s8.x[2][2]) < 0.01F);
}

/* A free vertex whose skinned target lies inside a capsule is driven to the capsule surface, not into it. */
static void test_target_inside_a_capsule_is_pushed_to_its_surface(void) {
    float drive[3] = {0.0F, 0.0F, 4000.0F};
    ClothMesh mesh = seam;
    mesh.drive = drive;
    mesh.tether = NULL;
    ClothMaterial m = stiff;
    m.thickness = 0.005F;
    m.stretch = 1.0F;
    const ClothCapsule ball = {.a = {0, 1.0F, 0}, .b = {0, 1.0F, 0}, .radius = 0.15F, .flags = CLOTH_COLLIDER_INSIDE};
    float target[3][3];
    memcpy(target, seam.rest, sizeof target);
    target[2][0] = 0.0F; target[2][1] = 1.0F; target[2][2] = 0.05F; /* inside the ball, off its centre */
    cloth_reset(&cloth, &mesh, target);
    float pushed[3][3];
    for (int i = 0; i < 400; ++i) {
        memcpy(pushed, target, sizeof pushed);
        cloth_push_targets(&cloth, &mesh, &m, pushed, NULL, &ball, 1, NULL);
        cloth_step(&cloth, &mesh, &m, dt, NULL, pushed, NULL, &ball, 1);
    }
    const float d = sqrtf(cloth.x[2][0] * cloth.x[2][0] + (cloth.x[2][1] - 1.0F) * (cloth.x[2][1] - 1.0F) + cloth.x[2][2] * cloth.x[2][2]);
    TEST_ASSERT_TRUE(fabsf(d - (ball.radius + m.thickness)) < 0.01F);
    TEST_ASSERT_TRUE(fabsf(cloth.target[2][2] - (ball.radius + m.thickness)) < 1e-4F);
    /* The pin keeps its own anchor. */
    TEST_ASSERT_EQUAL_MEMORY(target[0], cloth.target[0], sizeof target[0]);
}

/* A target that crosses a limb's axis between pose samples keeps exiting on the side it came from. */
static void test_pushed_target_keeps_its_side_when_the_target_crosses_the_axis(void) {
    float drive[3] = {0.0F, 0.0F, 100.0F};
    ClothMesh mesh = seam;
    mesh.drive = drive;
    mesh.tether = NULL;
    ClothMaterial m = stiff;
    m.thickness = 0.005F;
    ClothCapsule leg = {.a = {0, 0, 0}, .b = {0, 2.0F, 0}, .radius = 0.1F, .flags = CLOTH_COLLIDER_INSIDE};
    float target[3][3];
    memcpy(target, seam.rest, sizeof target);
    target[2][0] = 0.0F; target[2][1] = 1.0F; target[2][2] = 0.2F;
    cloth_reset(&cloth, &mesh, target);
    const float zs[] = {0.06F, 0.01F, -0.04F, -0.08F};
    for (int i = 0; i < 4; ++i) {
        target[2][2] = zs[i];
        memcpy(leg.prev_a, leg.a, sizeof leg.a);
        memcpy(leg.prev_b, leg.b, sizeof leg.b);
        leg.has_prev = 1U;
        float pushed[3][3];
        memcpy(pushed, target, sizeof pushed);
        cloth_push_targets(&cloth, &mesh, &m, pushed, NULL, &leg, 1, NULL);
        cloth_step(&cloth, &mesh, &m, dt, NULL, pushed, NULL, &leg, 1);
        TEST_ASSERT_TRUE(cloth.target[2][2] >= leg.radius + m.thickness - 1e-4F);
    }
}

/* A free vertex may sink below its target along the target normal only by its backstop. */
static void test_backstop_limits_sinking_below_the_target(void) {
    ClothMesh mesh = seam;
    mesh.tether = NULL;
    ClothMaterial m = stiff;
    m.stretch = 1.0F;
    const ClothGuide guide[3] = {{.normal = {0, 0, 1}, .travel = 1.0F, .backstop = 1.0F},
                                 {.normal = {0, 0, 1}, .travel = 1.0F, .backstop = 1.0F},
                                 {.normal = {0, 0, 1}, .travel = 1.0F, .backstop = 0.01F}};
    float sunk[3][3];
    memcpy(sunk, seam.rest, sizeof sunk);
    sunk[2][2] = -0.2F;
    cloth_reset(&cloth, &mesh, sunk);
    cloth_step(&cloth, &mesh, &m, dt, NULL, seam.rest, guide, NULL, 0);
    TEST_ASSERT_TRUE(cloth.x[2][2] >= seam.rest[2][2] - 0.01F - 1e-5F);
}

/* A target inside a seat leaves through the face it entered by. */
static void test_pushed_target_leaves_a_support_through_its_entry_face(void) {
    float drive[3] = {0.0F, 0.0F, 100.0F};
    ClothMesh mesh = seam;
    mesh.drive = drive;
    mesh.tether = NULL;
    const ClothMaterial material = {.density = 1, .stretch = 1.0F, .thickness = 0.005F, .iterations = 2};
    const ClothSupport seat = {.center = {0, 1.0F, 0}, .half_u = {1, 0, 0}, .half_v = {0, 0, -1}, .depth = 0.12F};
    const ClothFrame frame = {.support = &seat};
    float target[3][3];
    memcpy(target, seam.rest, sizeof target);
    target[2][0] = 0.0F; target[2][1] = 1.05F; target[2][2] = 0.0F;
    cloth_reset(&cloth, &mesh, target);
    target[2][1] = 0.90F; /* sank through the top face to nearer the underside: only the memory says top */
    cloth_push_targets(&cloth, &mesh, &material, target, NULL, NULL, 0, &seat);
    cloth_step(&cloth, &mesh, &material, dt, &frame, target, NULL, NULL, 0);
    TEST_ASSERT_TRUE(fabsf(cloth.target[2][1] - (seat.center[1] + material.thickness)) < 1e-5F);
}

static float capsule_depth(const float p[3], const ClothCapsule *c, float margin) {
    const float dx = p[0] - c->a[0], dz = p[2] - c->a[2];
    return c->radius + margin - sqrtf(dx * dx + dz * dz);
}

/* A target inside two overlapping limbs ends outside both: the second pass undoes what the first capsule's exit did
   to the other one. */
static void test_pushed_target_leaves_two_overlapping_capsules(void) {
    ClothMesh mesh = seam;
    mesh.tether = NULL;
    ClothMaterial m = stiff;
    m.thickness = 0.005F;
    const ClothCapsule legs[2] = {{.a = {0, 0, 0}, .b = {0, 2, 0}, .radius = 0.1F, .flags = CLOTH_COLLIDER_INSIDE},
                                  {.a = {0.15F, 0, 0}, .b = {0.15F, 2, 0}, .radius = 0.1F, .flags = CLOTH_COLLIDER_INSIDE}};
    float target[3][3];
    memcpy(target, seam.rest, sizeof target);
    target[2][0] = 0.075F; target[2][1] = 1.0F; target[2][2] = 0.3F;
    cloth_reset(&cloth, &mesh, target);
    target[2][2] = 0.05F;
    TEST_ASSERT_TRUE(cloth_push_targets(&cloth, &mesh, &m, target, NULL, legs, 2, NULL));
    TEST_ASSERT_TRUE(capsule_depth(target[2], &legs[0], m.thickness) < 1e-3F);
    TEST_ASSERT_TRUE(capsule_depth(target[2], &legs[1], m.thickness) < 1e-3F);
    TEST_ASSERT_TRUE(target[2][2] > 0.05F);
}

/* The support is applied after the capsules, so a target the limb pushes into the seat still ends on the seat. */
static void test_support_has_the_last_word_after_a_capsule(void) {
    ClothMesh mesh = seam;
    mesh.tether = NULL;
    ClothMaterial m = stiff;
    m.thickness = 0.005F;
    const ClothCapsule thigh = {.a = {-1, 1.08F, 0}, .b = {1, 1.08F, 0}, .radius = 0.1F, .flags = CLOTH_COLLIDER_INSIDE};
    const ClothSupport seat = {.center = {0, 1.0F, 0}, .half_u = {1, 0, 0}, .half_v = {0, 0, -1}, .depth = 0.12F};
    float target[3][3];
    memcpy(target, seam.rest, sizeof target);
    target[2][0] = 0.0F; target[2][1] = 1.005F; target[2][2] = 0.0F; /* on the seat, under the thigh */
    cloth_reset(&cloth, &mesh, target);
    target[2][1] = 1.03F; /* inside the thigh from below: its exit along -Y lands inside the seat */
    TEST_ASSERT_TRUE(cloth_push_targets(&cloth, &mesh, &m, target, NULL, &thigh, 1, &seat));
    TEST_ASSERT_TRUE(target[2][1] >= seat.center[1] + m.thickness - 1e-5F);
}

/* An invalid support is refused by the push as by the step. */
static void test_push_refuses_an_invalid_support(void) {
    ClothMesh mesh = seam;
    mesh.tether = NULL;
    const ClothSupport flat = {.center = {0, 1.0F, 0}, .half_u = {1, 0, 0}, .half_v = {2, 0, 0}, .depth = 0.1F};
    float target[3][3];
    memcpy(target, seam.rest, sizeof target);
    cloth_reset(&cloth, &mesh, target);
    TEST_ASSERT_FALSE(cloth_push_targets(&cloth, &mesh, &stiff, target, NULL, NULL, 0, &flat));
    TEST_ASSERT_FALSE(cloth.frame_valid);
    TEST_ASSERT_EQUAL_MEMORY(seam.rest, target, sizeof target);
}

static void test_anisotropic_warp_weft_edges(void) {
    /* Compare warp-only vs softened-weft stretching on a stretched triangle. */
    ClothEdge aniso_edges[3] = {
        {0, 1, 2, -1, 1.0F, 0, CLOTH_EDGE_WARP},
        {0, 2, 1, -1, hypotf(0.5F, 1.0F), 0, CLOTH_EDGE_WEFT},
        {1, 2, 0, -1, hypotf(0.5F, 1.0F), 0, CLOTH_EDGE_WEFT}
    };
    ClothMesh mesh = seam;
    mesh.edge = aniso_edges;
    mesh.tether = NULL;
    float pulled[3][3];
    memcpy(pulled, seam.rest, sizeof pulled);
    pulled[2][1] = 1.4F;
    ClothMaterial iso = {.density = 0.2F, .stretch = 1e-4F, .drag = 0.5F, .iterations = 2};
    ClothMaterial soft_weft = iso;
    soft_weft.weft_stretch_scale = 50.0F;
    static ClothState s_iso, s_soft;
    cloth_reset(&s_iso, &mesh, pulled);
    cloth_reset(&s_soft, &mesh, pulled);
    cloth_step(&s_iso, &mesh, &iso, dt, NULL, seam.rest, NULL, NULL, 0);
    cloth_step(&s_soft, &mesh, &soft_weft, dt, NULL, seam.rest, NULL, NULL, 0);
    TEST_ASSERT_TRUE(s_soft.x[2][1] > s_iso.x[2][1]);
}

static void test_contact_velocity_friction_and_max_speed(void) {
    ClothMaterial m = {.density = 0.2F, .stretch = 1e-4F, .drag = 0.0F, .iterations = 2,
                       .contact_friction = 0.5F, .max_speed_mps = 3.0F};
    ClothCapsule leg = {
        .a = {0, 1.0F, 0.02F}, .b = {0, 1.0F, 0.02F}, .radius = 0.1F,
        .prev_a = {0, 1.0F, 0.0F}, .prev_b = {0, 1.0F, 0.0F},
        .flags = CLOTH_COLLIDER_INSIDE, .has_prev = 1
    };
    cloth_reset(&cloth, &seam, seam.rest);
    cloth.velocity[2][0] = 4.0F;
    cloth_step(&cloth, &seam, &m, dt, NULL, seam.rest, NULL, &leg, 1);
    TEST_ASSERT_TRUE(cloth.frame_valid);
    TEST_ASSERT_TRUE(cloth_measure(&cloth, &seam, &m, dt, seam.rest, &leg, 1).speed <= m.max_speed_mps + 1e-4F);
    /* Tangential slip in X damped by contact_friction. */
    TEST_ASSERT_TRUE(cloth.velocity[2][0] < 3.0F);
}

static void test_invalid_frame_motion_is_rejected_without_mutating_state(void) {
    cloth_reset(&cloth, &seam, seam.rest);
    ClothFrame bad_frame = {.linear_accel = {NAN, 0, 0}, .linear_inertia = 0.5F};
    cloth_step(&cloth, &seam, &stiff, dt, &bad_frame, seam.rest, NULL, NULL, 0);
    TEST_ASSERT_FALSE(cloth.frame_valid);
    TEST_ASSERT_EQUAL_MEMORY(seam.rest, cloth.x, sizeof seam_rest);
}

static void test_compression_limit_softens_shortening_without_weakening_tension(void) {
    ClothMesh mesh = seam;
    mesh.tether = NULL;
    ClothMaterial rigid = stiff;
    rigid.stretch = 0.00001F;
    rigid.iterations = 1;
    ClothMaterial soft = rigid;
    soft.compression_limit = 0.15F;

    /* Compressed edge (90% of rest length): soft compression yields less rebound than rigid compression. */
    float comp[3][3];
    memcpy(comp, seam.rest, sizeof comp);
    comp[2][1] = seam.rest[2][1] * 0.90F;
    cloth_reset(&cloth, &mesh, comp);
    cloth_step(&cloth, &mesh, &rigid, dt, NULL, seam.rest, NULL, NULL, 0);
    float rigid_comp_y = cloth.x[2][1];
    cloth_reset(&cloth, &mesh, comp);
    cloth_step(&cloth, &mesh, &soft, dt, NULL, seam.rest, NULL, NULL, 0);
    float soft_comp_y = cloth.x[2][1];
    TEST_ASSERT_TRUE(fabsf(soft_comp_y - comp[2][1]) < fabsf(rigid_comp_y - comp[2][1]));

    /* Extended edge (120% of rest length): tension stiffness is unchanged. */
    float ext[3][3];
    memcpy(ext, seam.rest, sizeof ext);
    ext[2][1] = seam.rest[2][1] * 1.20F;
    cloth_reset(&cloth, &mesh, ext);
    cloth_step(&cloth, &mesh, &rigid, dt, NULL, seam.rest, NULL, NULL, 0);
    float rigid_ext_y = cloth.x[2][1];
    cloth_reset(&cloth, &mesh, ext);
    cloth_step(&cloth, &mesh, &soft, dt, NULL, seam.rest, NULL, NULL, 0);
    TEST_ASSERT_TRUE(fabsf(cloth.x[2][1] - rigid_ext_y) < 1e-6F);
}

static void test_bend_sliver_triangle_stays_bounded_and_finite(void) {
    /* Two triangles sharing hinge (0-1) of length 0.1m, where triangle (0,1,3) is a 0.2mm sliver. */
    const float sliver_rest[4][3] = {
        {0.0F, 0.0F, 0.0F}, {0.10F, 0.0F, 0.0F}, {0.05F, 0.08F, 0.0F}, {0.05F, -0.0002F, 0.0F}
    };
    const float sliver_inv[4] = {0.0F, 0.0F, 200.0F, 80000.0F};
    const ClothFace sliver_faces[2] = {{0, 1, 2}, {1, 0, 3}};
    const ClothEdge sliver_edges[5] = {
        {0, 1, 2, 3, 0.10F, 0.0F, CLOTH_EDGE_WEFT},
        {0, 2, 1, -1, hypotf(0.05F, 0.08F), 0.0F, CLOTH_EDGE_WARP},
        {1, 2, 0, -1, hypotf(0.05F, 0.08F), 0.0F, CLOTH_EDGE_WARP},
        {0, 3, 1, -1, hypotf(0.05F, 0.0002F), 0.0F, CLOTH_EDGE_WARP},
        {1, 3, 0, -1, hypotf(0.05F, 0.0002F), 0.0F, CLOTH_EDGE_WARP}
    };
    ClothMesh mesh = {.vertices = 4, .faces = 2, .edges = 5, .rest = sliver_rest, .inv_area = sliver_inv,
                      .face = sliver_faces, .edge = sliver_edges};
    float bent[4][3];
    memcpy(bent, sliver_rest, sizeof bent);
    bent[2][2] = 0.04F;
    bent[3][2] = -0.0001F;
    ClothMaterial m = stiff;
    m.bend = 50.0F;
    cloth_reset(&cloth, &mesh, bent);
    for (int i = 0; i < 10; ++i) cloth_step(&cloth, &mesh, &m, dt, NULL, sliver_rest, NULL, NULL, 0);
    ClothMetrics met = cloth_measure(&cloth, &mesh, &m, dt, sliver_rest, NULL, 0);
    TEST_ASSERT_TRUE(met.finite);
    TEST_ASSERT_TRUE(met.stretch < 0.25F);
    TEST_ASSERT_TRUE(met.speed < 10.0F);
}

static void test_reset_over_body_places_seated_skirt_above_thigh_without_impaling(void) {
    /* 3-row vertical front skirt strip (y = 0.90 pinned -> 0.75 -> 0.60, z = 0.06) impaled by a
       horizontal seated thigh capsule along +Z at y = 0.72, radius = 0.08. */
    const float strip_rest[6][3] = {
        {-0.05F, 0.90F, 0.06F}, {0.05F, 0.90F, 0.06F},
        {-0.05F, 0.75F, 0.06F}, {0.05F, 0.75F, 0.06F},
        {-0.05F, 0.60F, 0.06F}, {0.05F, 0.60F, 0.06F}
    };
    const float strip_inv[6] = {0.0F, 0.0F, 100.0F, 100.0F, 100.0F, 100.0F};
    const ClothFace strip_faces[4] = {{0, 2, 1}, {1, 2, 3}, {2, 4, 3}, {3, 4, 5}};
    const ClothEdge strip_edges[9] = {
        {0, 1, 2, -1, 0.10F, 0, CLOTH_EDGE_WEFT},
        {2, 3, 1, 4, 0.10F, 0, CLOTH_EDGE_WEFT},
        {4, 5, 3, -1, 0.10F, 0, CLOTH_EDGE_WEFT},
        {0, 2, 1, -1, 0.15F, 0, CLOTH_EDGE_WARP},
        {1, 3, 2, -1, 0.15F, 0, CLOTH_EDGE_WARP},
        {2, 4, 3, -1, 0.15F, 0, CLOTH_EDGE_WARP},
        {3, 5, 4, -1, 0.15F, 0, CLOTH_EDGE_WARP},
        {1, 2, 0, 3, hypotf(0.10F, 0.15F), 0, CLOTH_EDGE_DIAG},
        {3, 4, 2, 5, hypotf(0.10F, 0.15F), 0, CLOTH_EDGE_DIAG}
    };
    ClothMesh mesh = {.vertices = 6, .faces = 4, .edges = 9, .rest = strip_rest, .inv_area = strip_inv,
                      .face = strip_faces, .edge = strip_edges};
    ClothGuide guide[6];
    for (int i = 0; i < 6; ++i) guide[i] = (ClothGuide){.normal = {0, 0, 1}, .travel = 0.5F, .backstop = 0.5F};
    ClothCapsule thigh = {.a = {0.0F, 0.72F, 0.0F}, .b = {0.0F, 0.72F, 0.40F}, .radius = 0.08F, .flags = CLOTH_COLLIDER_INSIDE};
    ClothMaterial m = stiff;
    m.thickness = 0.005F;
    m.compression_limit = 0.35F;

    cloth_reset_supported(&cloth, &mesh, &m, strip_rest, guide, &thigh, 1, NULL);
    /* Every free vertex must lie ON TOP of the thigh (y > 0.72), never trapped underneath! */
    for (int i = 2; i < 6; ++i) TEST_ASSERT_TRUE(cloth.x[i][1] > thigh.a[1]);
    ClothMetrics met = cloth_measure(&cloth, &mesh, &m, dt, strip_rest, &thigh, 1);
    TEST_ASSERT_TRUE(met.finite);
    TEST_ASSERT_TRUE(met.stretch < 0.25F);
    TEST_ASSERT_TRUE(met.penetration < 0.01F);

    float seated[6][3];
    memcpy(seated, cloth.x, sizeof seated);
    m.reset_carry_range_m = 0.02F;
    cloth_reset_supported(&cloth, &mesh, &m, strip_rest, guide, &thigh, 1, NULL);
    TEST_ASSERT_EQUAL_MEMORY(seated, cloth.x, sizeof seated);

    ClothCapsule crossbar = {.a = {-0.055F, 0.77F, 0.05F}, .b = {0.055F, 0.77F, 0.05F}, .radius = 0.08F};
    m.reset_carry_range_m = 0.0F;
    cloth_reset_supported(&cloth, &mesh, &m, strip_rest, guide, &crossbar, 1, NULL);
    float hem = fabsf(cloth.x[5][2] - strip_rest[5][2]);
    ClothMetrics full = cloth_measure(&cloth, &mesh, &m, dt, strip_rest, &crossbar, 1);
    m.reset_carry_range_m = 0.02F;
    cloth_reset_supported(&cloth, &mesh, &m, strip_rest, guide, &crossbar, 1, NULL);
    ClothMetrics local = cloth_measure(&cloth, &mesh, &m, dt, strip_rest, &crossbar, 1);
    TEST_ASSERT_TRUE(local.finite && local.pin_error == 0.0F && local.stretch < full.stretch);
    TEST_ASSERT_TRUE(local.penetration <= full.penetration + 1e-6F);
    TEST_ASSERT_TRUE(fabsf(cloth.x[5][2] - strip_rest[5][2]) < hem);
}

static void build_grid_strip(int nx, int ny, float width, float length,
                             float rest[][3], float inv_area[], ClothFace faces[], ClothEdge edges[],
                             ClothMesh *out) {
    int nv = (nx + 1) * (ny + 1);
    float dx = width / (float)nx, dy = length / (float)ny;
    float area[CLOTH_VERTICES] = {0};
    int nf = 0;
    for (int j = 0; j <= ny; ++j) {
        for (int i = 0; i <= nx; ++i) {
            int id = j * (nx + 1) + i;
            rest[id][0] = (float)i * dx - 0.5F * width;
            rest[id][1] = 1.0F;
            rest[id][2] = (float)j * dy; /* Starts horizontal in +Z (cantilever release). */
        }
    }
    for (int j = 0; j < ny; ++j) {
        for (int i = 0; i < nx; ++i) {
            int v00 = j * (nx + 1) + i, v10 = v00 + 1;
            int v01 = v00 + (nx + 1), v11 = v01 + 1;
            faces[nf++] = (ClothFace){(uint16_t)v00, (uint16_t)v10, (uint16_t)v11};
            faces[nf++] = (ClothFace){(uint16_t)v00, (uint16_t)v11, (uint16_t)v01};
            float tri_a = 0.5F * dx * dy;
            area[v00] += tri_a * (2.0F / 3.0F);
            area[v10] += tri_a * (1.0F / 3.0F);
            area[v11] += tri_a * (2.0F / 3.0F);
            area[v01] += tri_a * (1.0F / 3.0F);
        }
    }
    for (int v = 0; v < nv; ++v) inv_area[v] = (v <= nx) ? 0.0F : (1.0F / area[v]);
    int ne = 0;
    for (int f = 0; f < nf; ++f) {
        int ids[3] = {faces[f].a, faces[f].b, faces[f].c};
        for (int k = 0; k < 3; ++k) {
            int a = ids[k], b = ids[(k + 1) % 3], c = ids[(k + 2) % 3];
            int lo = a < b ? a : b, hi = a < b ? b : a;
            int found = -1;
            for (int e = 0; e < ne; ++e) {
                if (edges[e].a == lo && edges[e].b == hi) { found = e; break; }
            }
            if (found >= 0) {
                edges[found].d = c;
            } else {
                float d0 = rest[hi][0] - rest[lo][0], d2 = rest[hi][2] - rest[lo][2];
                uint8_t kind = (fabsf(d0) < 1e-6F) ? CLOTH_EDGE_WARP
                             : (fabsf(d2) < 1e-6F) ? CLOTH_EDGE_WEFT : CLOTH_EDGE_DIAG;
                edges[ne++] = (ClothEdge){lo, hi, c, -1, hypotf(d0, d2), 0.0F, kind};
            }
        }
    }
    *out = (ClothMesh){.vertices = nv, .faces = nf, .edges = ne, .rest = rest,
                       .inv_area = inv_area, .face = faces, .edge = edges};
}

static float run_cantilever_tip_drop(int nx, int ny, int hz, float duration_s, const ClothMaterial *m) {
    static float rest[CLOTH_VERTICES][3];
    static float inv_area[CLOTH_VERTICES];
    static ClothFace faces[2 * CLOTH_EDGES / 3];
    static ClothEdge edges[CLOTH_EDGES];
    ClothMesh mesh;
    build_grid_strip(nx, ny, 0.20F, 0.36F, rest, inv_area, faces, edges, &mesh);
    cloth_reset(&cloth, &mesh, rest);
    float step = 1.0F / (float)hz;
    int steps = (int)lroundf(duration_s * (float)hz);
    for (int s = 0; s < steps; ++s) {
        cloth_step(&cloth, &mesh, m, step, NULL, rest, NULL, NULL, 0);
    }
    float tip_y = 0.0F;
    for (int i = 0; i <= nx; ++i) tip_y += cloth.x[ny * (nx + 1) + i][1];
    return 1.0F - tip_y / (float)(nx + 1);
}

static void test_xpbd_timestep_and_resolution_invariance_at_earth_gravity(void) {
    ClothMaterial m = {
        .density = 0.22F, .stretch = 0.00001F, .bend = 400.0F, .drag = 0.35F,
        .thickness = 0.005F, .iterations = 4, .max_edge_strain = 0.12F, .strain_passes = 2,
        .aero_drag = 0.60F, .compression_limit = 0.20F
    };
    /* 1. Timestep invariance: 120 Hz vs 240 Hz agrees within 5%. */
    float drop_120 = run_cantilever_tip_drop(2, 4, 120, 0.25F, &m);
    float drop_240 = run_cantilever_tip_drop(2, 4, 240, 0.25F, &m);
    TEST_ASSERT_TRUE(drop_120 > 0.10F && drop_240 > 0.10F);
    TEST_ASSERT_TRUE(fabsf(drop_120 - drop_240) / fmaxf(drop_120, drop_240) < 0.05F);

    /* 2. Mesh-resolution invariance: 1x (2x4) vs 2x (4x8) agrees within 5%. */
    float drop_fine = run_cantilever_tip_drop(4, 8, 240, 0.25F, &m);
    TEST_ASSERT_TRUE(drop_fine > 0.10F);
    TEST_ASSERT_TRUE(fabsf(drop_240 - drop_fine) / fmaxf(drop_240, drop_fine) < 0.05F);
}

static const float level_seam[][3] = {{-0.02F,1,-0.04F}, {0.02F,1,-0.04F}, {0,1,0}};
static const ClothEdge level_edges[] = {
    {0,1,2,-1,0.04F,0,0}, {1,2,0,-1,0.04472136F,0,0}, {2,0,1,-1,0.04472136F,0,0}
};
static const float free_mass[] = {1, 1, 1};

static void test_fabric_can_leave_a_support_and_move_back_under_its_edge(void) {
    ClothMesh mesh = seam;
    mesh.rest = level_seam;
    mesh.edge = level_edges;
    mesh.inv_area = free_mass;
    mesh.tether = NULL;
    ClothMaterial material = {.density = 1, .stretch = 1.0F, .thickness = 0.005F, .iterations = 2};
    const ClothSupport face = {.center = {0,1.01F,0}, .half_u = {0.1F,0,0}, .half_v = {0,0,-0.1F}, .depth = 0.06F};
    const ClothFrame frame = {.support = &face};
    TEST_ASSERT_TRUE(cloth_reset_supported(&cloth, &mesh, &material, mesh.rest, NULL, NULL, 0, &face));
    TEST_ASSERT_TRUE(fabsf(cloth.x[2][1] - 1.015F) < 1e-5F);
    for (int i = 0; i < 3; ++i) cloth.velocity[i][0] = 2.0F;
    for (int step = 0; step < 24; ++step)
        cloth_step(&cloth, &mesh, &material, dt, &frame, mesh.rest, NULL, NULL, 0);
    TEST_ASSERT_TRUE(cloth.x[2][0] > 0.2F);
    TEST_ASSERT_TRUE(cloth.x[2][1] < 0.98F);
    for (int i = 0; i < 3; ++i) cloth.velocity[i][0] = -2.0F;
    for (int step = 0; step < 24; ++step)
        cloth_step(&cloth, &mesh, &material, dt, &frame, mesh.rest, NULL, NULL, 0);
    TEST_ASSERT_TRUE(fabsf(cloth.x[2][0]) < 0.05F);
    TEST_ASSERT_TRUE(cloth.x[2][1] < 0.98F);
}

static void test_support_preserves_hanging_material_at_reset_and_catches_inherited_drops(void) {
    ClothMesh mesh = seam;
    mesh.rest = level_seam;
    mesh.edge = level_edges;
    mesh.inv_area = free_mass;
    mesh.tether = NULL;
    const ClothMaterial material = {.density = 1, .stretch = 1.0F, .thickness = 0.005F, .iterations = 2};
    const ClothSupport support = {.center = {0,1.01F,0}, .half_u = {1,0,0}, .half_v = {0,0,-1}, .depth = 0.12F};
    const ClothFrame frame = {.support = &support, .anchor_shift_y = -0.18F};
    const float below[][3] = {{-0.02F,0,-0.04F}, {0.02F,0,-0.04F}, {0,0,0}};
    TEST_ASSERT_TRUE(cloth_reset_supported(&cloth, &mesh, &material, below, NULL, NULL, 0, &support));
    TEST_ASSERT_TRUE(fabsf(cloth.x[0][1]) < 0.001F && fabsf(cloth.x[1][1]) < 0.001F);
    TEST_ASSERT_TRUE(cloth_reset_supported(&cloth, &mesh, &material, level_seam, NULL, NULL, 0, &support));
    TEST_ASSERT_TRUE(fabsf(cloth.x[2][1] - 1.015F) < 1e-5F);
    cloth_step(&cloth, &mesh, &material, dt, &frame, level_seam, NULL, NULL, 0);
    TEST_ASSERT_TRUE(fabsf(cloth.x[2][1] - 1.015F) < 1e-5F);
}

static void test_support_catches_a_spanning_edge_with_both_endpoints_outside(void) {
    const float spanning[][3] = {{-0.5F,0.97F,0}, {0.5F,0.97F,0}, {0,0.97F,-1}};
    ClothMesh mesh = seam;
    mesh.rest = spanning;
    mesh.inv_area = free_mass;
    mesh.tether = NULL;
    const ClothSupport face = {.center = {0,1,0}, .half_u = {0.1F,0,0}, .half_v = {0,0,-0.1F}, .depth = 0.15F};
    const ClothFrame frame = {.support = &face};
    const ClothMaterial material = {.density = 1, .stretch = 1.0F, .thickness = 0.005F, .iterations = 2};
    TEST_ASSERT_TRUE(cloth_reset_supported(&cloth, &mesh, &material, spanning, NULL, NULL, 0, &face));
    for (int step = 0; step < 5; ++step) {
        TEST_ASSERT_TRUE(cloth.x[0][0] < -0.1F && cloth.x[1][0] > 0.1F);
        TEST_ASSERT_TRUE(0.5F * (cloth.x[0][1] + cloth.x[1][1]) >= face.center[1]);
        cloth_step(&cloth, &mesh, &material, dt, &frame, spanning, NULL, NULL, 0);
    }
}

static void test_convex_support_leaves_beveled_corners_free(void) {
    ClothMesh mesh = seam;
    mesh.rest = level_seam;
    mesh.edge = level_edges;
    mesh.inv_area = free_mass;
    mesh.tether = NULL;
    const ClothMaterial material = {.density = 1, .stretch = 1.0F, .thickness = 0.005F, .iterations = 2};
    const ClothSupport face = {.center = {0,1.01F,0}, .half_u = {0.2F,0,0}, .half_v = {0,0,-0.2F},
        .depth = 0.12F, .plane_count = 7, .planes = {{1,0,0,-0.2F},{-1,0,0,-0.2F},{0,1,0,0},
            {0,-1,0,-0.12F},{0,0,1,-0.2F},{0,0,-1,-0.2F},{1,0,1,-0.2F}}};
    TEST_ASSERT_TRUE(cloth_reset_supported(&cloth, &mesh, &material, mesh.rest, NULL, NULL, 0, &face));
    TEST_ASSERT_TRUE(fabsf(cloth.x[2][1] - 1.015F) < 1e-5F);
    const ClothFrame drop = {.support = &face, .anchor_shift_y = -0.10F};
    cloth_step(&cloth, &mesh, &material, dt, &drop, mesh.rest, NULL, NULL, 0);
    TEST_ASSERT_TRUE(fabsf(cloth.x[2][1] - 1.015F) < 1e-5F);
    float corner[3][3];
    for (int i = 0; i < 3; ++i) {
        corner[i][0] = level_seam[i][0] + 0.16F;
        corner[i][1] = level_seam[i][1];
        corner[i][2] = level_seam[i][2] + 0.16F;
    }
    TEST_ASSERT_TRUE(cloth_reset_supported(&cloth, &mesh, &material, corner, NULL, NULL, 0, &face));
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < 3; ++k) TEST_ASSERT_TRUE(fabsf(cloth.x[i][k] - corner[i][k]) < 1e-5F);
}

static void test_framed_step_keeps_free_cloth_above_the_ground_plane(void) {
    const float above[][3] = {{-0.02F,0.05F,-0.04F}, {0.02F,0.05F,-0.04F}, {0,0.05F,0}};
    ClothMesh mesh = seam;
    mesh.rest = above;
    mesh.edge = level_edges;
    mesh.inv_area = free_mass;
    mesh.tether = NULL;
    const ClothMaterial material = {.density = 1, .stretch = 1.0F, .thickness = 0.005F, .iterations = 2};
    const ClothFrame frame = {0};
    static ClothState grounded, falling;
    cloth_reset(&grounded, &mesh, above);
    cloth_reset(&falling, &mesh, above);
    for (int step = 0; step < 120; ++step) {
        cloth_step(&grounded, &mesh, &material, dt, &frame, above, NULL, NULL, 0);
        cloth_step(&falling, &mesh, &material, dt, NULL, above, NULL, NULL, 0);
        TEST_ASSERT_TRUE(grounded.frame_valid && falling.frame_valid);
        for (int i = 0; i < 3; ++i) TEST_ASSERT_TRUE(grounded.x[i][1] >= material.thickness);
    }
    for (int i = 0; i < 3; ++i) {
        TEST_ASSERT_TRUE(grounded.x[i][1] < above[i][1]);
        TEST_ASSERT_TRUE(falling.x[i][1] < 0.0F);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_framed_step_keeps_free_cloth_above_the_ground_plane);
    RUN_TEST(test_fabric_can_leave_a_support_and_move_back_under_its_edge);
    RUN_TEST(test_support_preserves_hanging_material_at_reset_and_catches_inherited_drops);
    RUN_TEST(test_support_catches_a_spanning_edge_with_both_endpoints_outside);
    RUN_TEST(test_convex_support_leaves_beveled_corners_free);
    RUN_TEST(test_material_restores_signed_fold);
    RUN_TEST(test_capsule_axis_crosses_face_interior);
    RUN_TEST(test_single_pinned_seam_contact_has_bounded_displacement);
    RUN_TEST(test_tether_limits_extension_and_preserves_slack);
    RUN_TEST(test_strain_limit_respects_pins_and_slack);
    RUN_TEST(test_strain_cannot_pull_a_free_vertex_into_the_body);
    RUN_TEST(test_reset_rejects_excess_capsules_and_preserves_coordinates);
    RUN_TEST(test_round_cone_tapered_capsule_geometry_and_degeneracy);
    RUN_TEST(test_temporal_side_recovery_prevents_tunneling_across_axis);
    RUN_TEST(test_reference_frame_inertia_and_aero_drag);
    RUN_TEST(test_drive_converges_toward_the_target_independently_of_iterations);
    RUN_TEST(test_target_inside_a_capsule_is_pushed_to_its_surface);
    RUN_TEST(test_pushed_target_keeps_its_side_when_the_target_crosses_the_axis);
    RUN_TEST(test_pushed_target_leaves_a_support_through_its_entry_face);
    RUN_TEST(test_backstop_limits_sinking_below_the_target);
    RUN_TEST(test_pushed_target_leaves_two_overlapping_capsules);
    RUN_TEST(test_support_has_the_last_word_after_a_capsule);
    RUN_TEST(test_push_refuses_an_invalid_support);
    RUN_TEST(test_anisotropic_warp_weft_edges);
    RUN_TEST(test_contact_velocity_friction_and_max_speed);
    RUN_TEST(test_invalid_frame_motion_is_rejected_without_mutating_state);
    RUN_TEST(test_compression_limit_softens_shortening_without_weakening_tension);
    RUN_TEST(test_bend_sliver_triangle_stays_bounded_and_finite);
    RUN_TEST(test_reset_over_body_places_seated_skirt_above_thigh_without_impaling);
    RUN_TEST(test_xpbd_timestep_and_resolution_invariance_at_earth_gravity);
    return UNITY_END();
}
