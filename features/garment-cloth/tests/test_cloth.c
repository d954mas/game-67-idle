#include "garment/cloth.h"
#include "garment/contact.h"
#include "unity.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#include "cloth_fixtures.gen.h"

/* The consumer builds Unity without float support. */
#define NEAR(tolerance, expected, actual) TEST_ASSERT_TRUE(fabsf((float)(expected) - (float)(actual)) <= (tolerance))

void setUp(void) {}
void tearDown(void) {}

static ClothState cloth;
static ClothReference ref[CLOTH_VERTICES];
static float r[CLOTH_VERTICES];
static const ClothMaterial stiff = {.density = 0.2F, .drag = 2, .iterations = 6};
/* Fabric so compliant that only the constraint under test moves a vertex. */
static const ClothMaterial inert = {.density = 0.2F, .stretch = 1e12F, .bend = 1e12F, .iterations = 1};
static const float dt = 1.0F / 180.0F;

static void reference(const ClothMesh *mesh, const float (*x)[3], const float b[3]) {
    for (int i = 0; i < mesh->vertices; ++i) {
        memcpy(ref[i].x, x[i], sizeof ref[i].x);
        memcpy(ref[i].b, b, sizeof ref[i].b);
        r[i] = mesh->max_distance[i];
    }
}

static float distance(const float a[3], const float b[3]) {
    return sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

static const float toward_back[3] = {0, 0, -1};

static void test_max_distance_ends_on_its_sphere(void) {
    reference(&fold, fold.rest, toward_back);
    r[3] = 0.1F;
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &inert, ref, r, NULL));
    cloth.x[3][0] += 0.3F;
    cloth.x[3][2] += 0.4F;
    cloth_step(&cloth, &fold, &inert, dt, NULL, ref, r, NULL);
    TEST_ASSERT_TRUE(cloth.frame_valid);
    NEAR(1e-5F, r[3], distance(cloth.x[3], ref[3].x));
    ClothMetrics m = cloth_measure(&cloth, &fold, &inert, ref, r, NULL);
    TEST_ASSERT_TRUE(m.finite && m.max_distance_excess_m < 1e-5F);
}

static void test_zero_max_distance_equals_the_reference(void) {
    reference(&fold, fold.rest, toward_back);
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &stiff, ref, r, NULL));
    float moved[4][3];
    memcpy(moved, fold.rest, sizeof moved);
    for (int i = 0; i < 4; ++i) moved[i][1] += 0.02F * (float)(i + 1);
    reference(&fold, (const float (*)[3])moved, toward_back);
    memset(r, 0, sizeof r);
    cloth_step(&cloth, &fold, &stiff, dt, NULL, ref, r, NULL);
    TEST_ASSERT_TRUE(cloth.frame_valid);
    for (int i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_MEMORY(ref[i].x, cloth.x[i], sizeof cloth.x[i]);
        NEAR(1e-3F, 0.02F * (float)(i + 1) / dt, cloth.velocity[i][1]);
    }
}

static void test_backstop_ends_on_its_sphere_along_b(void) {
    const ClothBackstop off = {0, 0}, behind = {0.01F, 0.1F};
    ClothBackstop backstop[4] = {off, off, off, behind};
    ClothMesh mesh = fold;
    mesh.backstop = backstop;
    const float b[3] = {0.0F, -0.6F, -0.8F};
    reference(&mesh, mesh.rest, b);
    r[3] = 0.5F;
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &mesh, &inert, ref, r, NULL));
    /* Pushed in along b, the vertex comes back to the sphere's nearest point to the reference, d behind it. */
    for (int k = 0; k < 3; ++k) cloth.x[3][k] = ref[3].x[k] + b[k] * 0.06F;
    cloth_step(&cloth, &mesh, &inert, dt, NULL, ref, r, NULL);
    float center[3], surface[3];
    for (int k = 0; k < 3; ++k) {
        center[k] = ref[3].x[k] + b[k] * (behind.d + behind.R);
        surface[k] = ref[3].x[k] + b[k] * behind.d;
    }
    NEAR(1e-5F, behind.R, distance(cloth.x[3], center));
    TEST_ASSERT_TRUE(distance(cloth.x[3], surface) < 1e-3F);
    TEST_ASSERT_TRUE(cloth_measure(&cloth, &mesh, &inert, ref, r, NULL).backstop_depth_m < 1e-5F);
}

static void test_a_shape_holding_the_reference_deeper_than_r_is_reported(void) {
    reference(&fold, fold.rest, toward_back);
    ClothSphere sphere = {.r = 0.2F};
    memcpy(sphere.c, fold.rest[3], sizeof sphere.c);
    memcpy(sphere.prev, sphere.c, sizeof sphere.prev);
    ClothShapes shapes = {.spheres = &sphere, .sphere_count = 1};
    r[3] = 0.05F;
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &stiff, ref, r, &shapes));
    ClothMetrics held = cloth_measure(&cloth, &fold, &stiff, ref, r, &shapes);
    TEST_ASSERT_EQUAL_INT(1, held.held_vertices);
    NEAR(1e-5F, sphere.r + stiff.thickness - r[3], held.held_depth_m);
    r[3] = 0.5F;
    TEST_ASSERT_EQUAL_INT(0, cloth_measure(&cloth, &fold, &stiff, ref, r, &shapes).held_vertices);
}

static void test_tethers_hold(void) {
    ClothMaterial elastic = stiff;
    elastic.stretch = 100.0F;
    elastic.iterations = 1;
    reference(&seam, seam.rest, toward_back);
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &seam, &elastic, ref, r, NULL));
    cloth.x[2][1] *= 2.0F;
    cloth_step(&cloth, &seam, &elastic, dt, NULL, ref, r, NULL);
    ClothTether tether = seam.tether[2];
    NEAR(1e-5F, tether.max_length, distance(cloth.x[2], cloth.x[tether.pin]));

    /* Inside its length a tether does not act: the step equals one without it. */
    ClothMesh untethered = seam;
    untethered.tether = NULL;
    float slack[3];
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &untethered, &elastic, ref, r, NULL));
    cloth.x[2][1] = 0.4F;
    cloth_step(&cloth, &untethered, &elastic, dt, NULL, ref, r, NULL);
    memcpy(slack, cloth.x[2], sizeof slack);
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &seam, &elastic, ref, r, NULL));
    cloth.x[2][1] = 0.4F;
    cloth_step(&cloth, &seam, &elastic, dt, NULL, ref, r, NULL);
    TEST_ASSERT_EQUAL_MEMORY(slack, cloth.x[2], sizeof slack);
}

static void test_a_cone_sweeping_a_pinned_panel_at_15_mps_leaves_no_vertex_inside(void) {
    const ClothMaterial fabric = {.density = 0.2F, .stretch = 1e-5F, .bend = 200.0F, .thickness = 0.004F, .iterations = 2,
                                  .max_edge_strain = 0.12F, .strain_passes = 4};
    const float b[3] = {0, 0, 1};
    reference(&panel, panel.rest, b);
    /* A horizontal limb wider than the panel crosses it at mid-height. */
    ClothSphere limb[2] = {{.c = {-0.3F, 0.2F, -0.3F}, .r = 0.05F}, {.c = {0.3F, 0.2F, -0.3F}, .r = 0.04F}};
    const ClothCone cone = {0, 1};
    for (int i = 0; i < 2; ++i) memcpy(limb[i].prev, limb[i].c, sizeof limb[i].prev);
    ClothShapes shapes = {.spheres = limb, .sphere_count = 2, .cones = &cone, .cone_count = 1};
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &panel, &fabric, ref, r, &shapes));
    int contacts = 0;
    for (int step = 0; step < 12; ++step) {
        for (int i = 0; i < 2; ++i) {
            memcpy(limb[i].prev, limb[i].c, sizeof limb[i].prev);
            limb[i].c[2] += 15.0F * dt;
        }
        cloth_step(&cloth, &panel, &fabric, dt, NULL, ref, r, &shapes);
        TEST_ASSERT_TRUE(cloth.frame_valid);
        for (int v = 0; v < panel.vertices; ++v) {
            float n[3], t;
            float depth = cloth_point_cone_contact(cloth.x[v], &limb[0], &limb[1], 0.0F, n, &t);
            TEST_ASSERT_TRUE_MESSAGE(depth <= 1e-4F, "a vertex is inside the cone after the substep");
            contacts += cloth_point_cone_contact(cloth.x[v], &limb[0], &limb[1], fabric.thickness + 1e-4F, n, &t) > 0.0F;
        }
    }
    TEST_ASSERT_TRUE(contacts > 0);
}

/* A thin limb (r + thickness = 14 mm) crosses a pinned panel at 15 m/s, 83 mm per substep, and one substep ends with its
   axis 4 mm in front of the panel. Contact is discrete: the vertices it lands on leave by the nearest exit, backward,
   and the reference constraints keep every vertex within its max distance of where it belongs. */
static void test_a_thin_cone_at_15_mps_leaves_the_panel_outside_and_within_max_distance(void) {
    const ClothMaterial fabric = {.density = 0.2F, .stretch = 1e-5F, .bend = 200.0F, .thickness = 0.004F, .iterations = 2,
                                  .max_edge_strain = 0.12F, .strain_passes = 4};
    const float b[3] = {0, 0, 1};
    reference(&panel, panel.rest, b);
    for (int i = 0; i < panel.vertices; ++i) r[i] = panel.max_distance[i] > 0.0F ? 0.1F : 0.0F;
    ClothSphere limb[2] = {{.c = {-0.3F, 0.2F, -0.246F}, .r = 0.01F}, {.c = {0.3F, 0.2F, -0.246F}, .r = 0.01F}};
    const ClothCone cone = {0, 1};
    for (int i = 0; i < 2; ++i) memcpy(limb[i].prev, limb[i].c, sizeof limb[i].prev);
    ClothShapes shapes = {.spheres = limb, .sphere_count = 2, .cones = &cone, .cone_count = 1};
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &panel, &fabric, ref, r, &shapes));
    int contacts = 0;
    float pushed = 0.0F;
    for (int step = 0; step < 6; ++step) {
        for (int i = 0; i < 2; ++i) {
            memcpy(limb[i].prev, limb[i].c, sizeof limb[i].prev);
            limb[i].c[2] += 15.0F * dt;
        }
        cloth_step(&cloth, &panel, &fabric, dt, NULL, ref, r, &shapes);
        TEST_ASSERT_TRUE(cloth.frame_valid);
        for (int v = 0; v < panel.vertices; ++v) {
            float n[3], t;
            TEST_ASSERT_TRUE(cloth_point_cone_contact(cloth.x[v], &limb[0], &limb[1], 0.0F, n, &t) <= 1e-4F);
            /* Left on the surface, then drawn a little off it by the fabric after the contact pass. */
            contacts += cloth_point_cone_contact(cloth.x[v], &limb[0], &limb[1], 2.0F * fabric.thickness, n, &t) > 0.0F;
            TEST_ASSERT_TRUE(distance(cloth.x[v], ref[v].x) <= r[v] + 1e-4F);
            pushed = fmaxf(pushed, ref[v].x[2] - cloth.x[v][2]);
        }
    }
    TEST_ASSERT_TRUE(contacts > 0);
    /* Gravity and the fabric move the panel in its plane only, so a step back from the reference is the exit. */
    TEST_ASSERT_TRUE(pushed >= 0.5F * (limb[0].r + fabric.thickness));
}

static void test_a_plane_backstop_puts_a_vertex_pushed_past_it_back_on_the_plane(void) {
    const ClothBackstop off = {0, 0}, plane = {0.01F, INFINITY};
    ClothBackstop backstop[4] = {off, off, off, plane};
    ClothMesh mesh = fold;
    mesh.backstop = backstop;
    const float b[3] = {0.0F, -0.6F, -0.8F};
    reference(&mesh, mesh.rest, b);
    r[3] = 0.5F;
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &mesh, &inert, ref, r, NULL));
    for (int k = 0; k < 3; ++k) cloth.x[3][k] = ref[3].x[k] + b[k] * 0.06F + (k == 0 ? 0.03F : 0.0F);
    cloth_step(&cloth, &mesh, &inert, dt, NULL, ref, r, NULL);
    float along = 0.0F;
    for (int k = 0; k < 3; ++k) along += (cloth.x[3][k] - ref[3].x[k]) * b[k];
    NEAR(1e-5F, plane.d, along);
    /* The plane pushes along b only: the step across it stays. */
    NEAR(1e-5F, ref[3].x[0] + 0.03F, cloth.x[3][0]);
    TEST_ASSERT_TRUE(cloth_measure(&cloth, &mesh, &inert, ref, r, NULL).backstop_depth_m < 1e-5F);
}

static void test_reset_leaves_a_seat_through_its_nearest_face_not_along_b(void) {
    /* The seat top is at y = 0.05; the reference lies 3 mm inside it and its body direction points up at the thigh. */
    ClothConvex seat = {.plane_count = 6, .planes = {{0, 1, 0, -0.05F}, {0, -1, 0, -0.05F}, {1, 0, 0, -0.2F},
                                                      {-1, 0, 0, -0.2F}, {0, 0, 1, -0.2F}, {0, 0, -1, -0.2F}}};
    ClothShapes shapes = {.convexes = &seat, .convex_count = 1};
    ClothMaterial m = stiff;
    m.thickness = 0.004F;
    float placed[4][3];
    memcpy(placed, fold.rest, sizeof placed);
    for (int i = 0; i < 3; ++i) placed[i][1] += 1.0F;
    placed[3][0] = 0.0F;
    placed[3][1] = 0.047F;
    placed[3][2] = 0.0F;
    const float up[3] = {0, 1, 0};
    reference(&fold, (const float (*)[3])placed, up);
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &m, ref, r, &shapes));
    NEAR(1e-5F, 0.05F + m.thickness, cloth.x[3][1]);
}

static void test_reset_gives_the_reference_and_exits_along_minus_b(void) {
    const float b[3] = {0.0F, 0.6F, -0.8F};
    reference(&fold, fold.rest, b);
    ClothSphere sphere = {.c = {0.02F, 0.01F, 0.97F}, .r = 0.1F};
    memcpy(sphere.prev, sphere.c, sizeof sphere.prev);
    ClothShapes shapes = {.spheres = &sphere, .sphere_count = 1};
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &stiff, ref, r, &shapes));
    for (int i = 0; i < 3; ++i) TEST_ASSERT_EQUAL_MEMORY(ref[i].x, cloth.x[i], sizeof cloth.x[i]);
    float offset[3], length = 0.0F, along = 0.0F;
    for (int k = 0; k < 3; ++k) {
        offset[k] = cloth.x[3][k] - ref[3].x[k];
        length += offset[k] * offset[k];
        along -= offset[k] * b[k];
    }
    length = sqrtf(length);
    TEST_ASSERT_TRUE(length > 0.01F);
    NEAR(1e-5F, length, along);
    NEAR(1e-4F, sphere.r + stiff.thickness, distance(cloth.x[3], sphere.c));
    for (int i = 0; i < 4; ++i) TEST_ASSERT_TRUE(cloth.velocity[i][0] == 0 && cloth.velocity[i][1] == 0 && cloth.velocity[i][2] == 0);
}

/* A vertex resting on a plane tilted by angle under gravity for one second. */
static float slide_on_slope(float angle, float mu) {
    ClothMaterial m = inert;
    m.friction = mu;
    m.thickness = 0.002F;
    reference(&fold, fold.rest, toward_back);
    r[3] = 10.0F;
    const float n[3] = {-sinf(angle), cosf(angle), 0.0F};
    ClothConvex slope = {.plane_count = 1};
    float start[3] = {fold.rest[3][0], fold.rest[3][1], fold.rest[3][2]};
    memcpy(slope.planes[0], n, sizeof n);
    slope.planes[0][3] = -(n[0] * start[0] + n[1] * start[1] + n[2] * start[2]) + m.thickness;
    ClothShapes shapes = {.convexes = &slope, .convex_count = 1};
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &m, ref, r, &shapes));
    for (int s = 0; s < 180; ++s) cloth_step(&cloth, &fold, &m, dt, NULL, ref, r, &shapes);
    TEST_ASSERT_TRUE(cloth.frame_valid);
    float height = n[0] * cloth.x[3][0] + n[1] * cloth.x[3][1] + n[2] * cloth.x[3][2] + slope.planes[0][3];
    NEAR(1e-4F, m.thickness, height);
    return distance(cloth.x[3], start);
}

static void test_friction_holds_below_mu_and_slides_above(void) {
    const float mu = 0.5F;
    TEST_ASSERT_TRUE(slide_on_slope(atanf(0.8F * mu), mu) < 1e-3F);
    TEST_ASSERT_TRUE(slide_on_slope(atanf(1.4F * mu), mu) > 0.1F);
    TEST_ASSERT_TRUE(slide_on_slope(atanf(1.4F * mu), 0.0F) > slide_on_slope(atanf(1.4F * mu), mu));
}

static void test_a_vertex_entering_a_convex_through_its_top_leaves_through_the_top(void) {
    /* A 0.4 x 0.1 x 0.4 m seat with its top at y = 0.05. */
    ClothConvex seat = {.plane_count = 6, .planes = {{0, 1, 0, -0.05F}, {0, -1, 0, -0.05F}, {1, 0, 0, -0.2F},
                                                      {-1, 0, 0, -0.2F}, {0, 0, 1, -0.2F}, {0, 0, -1, -0.2F}}};
    ClothShapes shapes = {.convexes = &seat, .convex_count = 1};
    ClothMaterial m = inert;
    m.thickness = 0.003F;
    float placed[4][3];
    memcpy(placed, fold.rest, sizeof placed);
    for (int i = 0; i < 4; ++i) placed[i][1] += 1.0F;
    placed[3][0] = 0.05F;
    placed[3][1] = 0.06F;
    placed[3][2] = 0.0F;
    reference(&fold, (const float (*)[3])placed, toward_back);
    r[3] = 2.0F;
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &m, ref, r, &shapes));
    /* In one substep it falls 0.083 m and ends nearer the underside than the top. */
    cloth.velocity[3][1] = -15.0F;
    cloth_step(&cloth, &fold, &m, dt, NULL, ref, r, &shapes);
    TEST_ASSERT_TRUE(cloth.frame_valid);
    NEAR(1e-5F, 0.05F + m.thickness, cloth.x[3][1]);
    NEAR(1e-5F, 0.05F, cloth.x[3][0]);
}

static void test_material_restores_signed_fold(void) {
    reference(&fold, fold.rest, toward_back);
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &stiff, ref, r, NULL));
    cloth.x[3][1] = -0.5F;
    cloth.x[3][2] = 0.8660254F;
    float before = atan2f(cloth.x[3][2], -cloth.x[3][1]);
    cloth_step(&cloth, &fold, &stiff, dt, NULL, ref, r, NULL);
    float after = atan2f(cloth.x[3][2], -cloth.x[3][1]);
    TEST_ASSERT_TRUE(after > before && fabsf(after - 1.5707963F) < 0.05F);
    TEST_ASSERT_TRUE(cloth_measure(&cloth, &fold, &stiff, ref, r, NULL).finite);
}

static void test_cone_axis_crosses_face_interior(void) {
    ClothSphere a = {.c = {0, 0.25F, -1}, .r = 0.1F}, b = {.c = {0, 0.25F, 1}, .r = 0.1F};
    float bary[3], normal[3];
    float depth = cloth_face_contact(seam.rest[0], seam.rest[1], seam.rest[2], &a, &b, 0, bary, normal);
    TEST_ASSERT_TRUE(fabsf(depth - a.r) < 0.00001F);
    TEST_ASSERT_TRUE(bary[0] > 0 && bary[1] > 0 && bary[2] > 0);
    TEST_ASSERT_TRUE(fabsf(bary[0] + bary[1] + bary[2] - 1) < 0.00001F);
}

static void test_round_cone_geometry_and_degeneracy(void) {
    /* Tapered limb from y=0 (r=0.2) to y=1 (r=0.1). Slant normal tilts upward (+Y). */
    ClothSphere a = {.c = {0, 0, 0}, .r = 0.2F}, b = {.c = {0, 1, 0}, .r = 0.1F};
    float n[3], t = -1.0F;
    float depth = cloth_point_cone_contact((float[]){0.10F, 0.5F, 0.0F}, &a, &b, 0.0F, n, &t);
    float expected_depth = 0.2F - (0.1F * sqrtf(1.0F - 0.01F) + 0.5F * 0.1F);
    TEST_ASSERT_TRUE(fabsf(depth - expected_depth) < 1e-5F);
    TEST_ASSERT_TRUE(n[0] > 0.99F && n[1] > 0.09F && fabsf(n[2]) < 1e-5F);
    TEST_ASSERT_TRUE(t > 0.0F && t < 1.0F);
    TEST_ASSERT_TRUE(cloth_point_cone_contact((float[]){0.12F, 1.0F, 0.0F}, &a, &b, 0.0F, n, &t) == 0.0F);
    TEST_ASSERT_TRUE(cloth_point_cone_contact((float[]){0.12F, 0.0F, 0.0F}, &a, &b, 0.0F, n, &t) > 0.07F);
    /* One sphere containing the other reduces to the larger sphere without NaN; a == b is the sphere alone. */
    ClothSphere nested = {.c = {0, 0.05F, 0}, .r = 0.05F};
    float d_nested = cloth_point_cone_contact((float[]){0.1F, 0.0F, 0.0F}, &a, &nested, 0.0F, n, &t);
    TEST_ASSERT_TRUE(isfinite(d_nested) && fabsf(d_nested - 0.1F) < 1e-5F);
    TEST_ASSERT_TRUE(fabsf(cloth_point_cone_contact((float[]){0.1F, 0.0F, 0.0F}, &a, &a, 0.0F, n, &t) - 0.1F) < 1e-5F);
}

static void test_strain_limit_respects_pins_and_slack(void) {
    ClothMesh mesh = seam;
    mesh.tether = NULL;
    ClothMaterial m = stiff;
    m.stretch = 100.0F;
    m.iterations = 1;
    reference(&mesh, mesh.rest, toward_back);
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &mesh, &m, ref, r, NULL));
    cloth.x[2][1] *= 2.0F;
    cloth_step(&cloth, &mesh, &m, dt, NULL, ref, r, NULL);
    float initial_strain = cloth_measure(&cloth, &mesh, &m, ref, r, NULL).stretch;
    m.max_edge_strain = 0.1F;
    m.strain_passes = 12;
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &mesh, &m, ref, r, NULL));
    cloth.x[2][1] *= 2.0F;
    cloth_step(&cloth, &mesh, &m, dt, NULL, ref, r, NULL);
    ClothMetrics final = cloth_measure(&cloth, &mesh, &m, ref, r, NULL);
    TEST_ASSERT_TRUE(final.finite && final.stretch < initial_strain && final.stretch <= m.max_edge_strain + 0.001F);
    for (int i = 0; i < 2; ++i) TEST_ASSERT_EQUAL_MEMORY(ref[i].x, cloth.x[i], sizeof cloth.x[i]);
}

static void test_invalid_inputs_are_rejected(void) {
    reference(&fold, fold.rest, toward_back);
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &stiff, ref, r, NULL));
    ref[2].b[2] = 0.5F;
    cloth_step(&cloth, &fold, &stiff, dt, NULL, ref, r, NULL);
    TEST_ASSERT_FALSE(cloth.frame_valid);
    reference(&fold, fold.rest, toward_back);
    r[3] = -0.1F;
    TEST_ASSERT_FALSE(cloth_reset(&cloth, &fold, &stiff, ref, r, NULL));
    r[3] = 0.1F;
    ClothSphere sphere = {.c = {0, 0, 0}, .r = 0.1F};
    const ClothCone loop = {0, 1};
    ClothShapes shapes = {.spheres = &sphere, .sphere_count = 1, .cones = &loop, .cone_count = 1};
    TEST_ASSERT_FALSE(cloth_reset(&cloth, &fold, &stiff, ref, r, &shapes));
    const ClothFrame spin = {.angular_vel_y = NAN};
    TEST_ASSERT_TRUE(cloth_reset(&cloth, &fold, &stiff, ref, r, NULL));
    cloth_step(&cloth, &fold, &stiff, dt, &spin, ref, r, NULL);
    TEST_ASSERT_FALSE(cloth.frame_valid);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_max_distance_ends_on_its_sphere);
    RUN_TEST(test_zero_max_distance_equals_the_reference);
    RUN_TEST(test_backstop_ends_on_its_sphere_along_b);
    RUN_TEST(test_a_shape_holding_the_reference_deeper_than_r_is_reported);
    RUN_TEST(test_tethers_hold);
    RUN_TEST(test_a_cone_sweeping_a_pinned_panel_at_15_mps_leaves_no_vertex_inside);
    RUN_TEST(test_a_thin_cone_at_15_mps_leaves_the_panel_outside_and_within_max_distance);
    RUN_TEST(test_a_plane_backstop_puts_a_vertex_pushed_past_it_back_on_the_plane);
    RUN_TEST(test_reset_leaves_a_seat_through_its_nearest_face_not_along_b);
    RUN_TEST(test_reset_gives_the_reference_and_exits_along_minus_b);
    RUN_TEST(test_friction_holds_below_mu_and_slides_above);
    RUN_TEST(test_a_vertex_entering_a_convex_through_its_top_leaves_through_the_top);
    RUN_TEST(test_material_restores_signed_fold);
    RUN_TEST(test_cone_axis_crosses_face_interior);
    RUN_TEST(test_round_cone_geometry_and_degeneracy);
    RUN_TEST(test_strain_limit_respects_pins_and_slack);
    RUN_TEST(test_invalid_inputs_are_rejected);
    return UNITY_END();
}
