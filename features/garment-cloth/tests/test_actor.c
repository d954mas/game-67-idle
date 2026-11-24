#include "garment/actor.h"
#include "garment/contact.h"
#include "hash/nt_hash.h"
#include "unity.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/* A panel on a two-joint arm: the shoulder carries the pinned edge, the free corner hangs below it, and a sphere
   sits on the forearm one metre past the elbow. */
static const float rest[][3] = {{0, 0, 0}, {1, 0, 0}, {0, -1, 0}};
static const ClothFace face = {0, 2, 1};
static const ClothEdge edges[] = {{0, 1, 2, -1, 1, 0, 0}, {1, 2, 0, -1, 1.41421356F, 0, 0}, {2, 0, 1, -1, 1, 0, 0}};
static const float inv_area[] = {2, 2, 2};
static const ClothTether tethers[] = {{0, 0}, {1, 0}, {0, 1.05F}};
static const float max_distance[] = {0, 0, 0.5F};
static const ClothBackstop backstop[] = {{0, 0}, {0, 0}, {0, 0}};
static const ClothMesh mesh = {.vertices = 3, .faces = 1, .edges = 3, .rest = rest, .inv_area = inv_area, .face = &face,
                               .edge = edges, .tether = tethers, .max_distance = max_distance, .backstop = backstop};
static const GarmentBone bones[] = {{"shoulder", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}},
                                    {"elbow", {1, 0, 0, -1, 0, 1, 0, 0, 0, 0, 1, 0}}};
static const GarmentSkin skin[] = {{.weight = {1}}, {.weight = {1}}, {.weight = {1}}};
static const float direction[][3] = {{0, 0, -1}, {0, 0, -1}, {0, 0, -1}};
static const GarmentSphere hand = {.bone = 1, .center = {1, 0, 0}, .radius = 0.05F};
static const GarmentAsset piece = {.id = "panel", .mesh = &mesh, .skin = skin, .direction = direction,
                                   .material = {.density = 0.2F, .stretch = 0.03F, .iterations = 1, .friction = 0.5F}};
static const GarmentSet set = {.id = "arm", .bones = bones, .bone_count = 2, .spheres = &hand, .sphere_count = 1,
                               .pieces = &piece, .piece_count = 1};
static const uint16_t parents[] = {NT_SKELETAL_NO_PARENT, 0};
static uint32_t ids[2];
static nt_skeletal_skeleton_t rig;
static const float identity_world[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static GarmentActor actors[2];

/* The consumer builds Unity without float support. */
#define NEAR(tolerance, expected, actual) TEST_ASSERT_TRUE(fabsf((float)(expected) - (float)(actual)) <= (tolerance))

void setUp(void) {
    ids[0] = nt_hash32_str("shoulder").value;
    ids[1] = nt_hash32_str("elbow").value;
    rig = (nt_skeletal_skeleton_t){.parent = parents, .joint_id = ids, .joint_count = 2};
}
void tearDown(void) {
    garment_actor_clear(&actors[0]);
    garment_actor_clear(&actors[1]);
}

/* The arm with the shoulder turned by angle about +Z; the elbow is one metre along the upper arm. */
static void arm(float angle, nt_skeletal_mat34_t pose[2]) {
    const float c = cosf(angle), s = sinf(angle);
    pose[0] = (nt_skeletal_mat34_t){.r = {{c, -s, 0, 0}, {s, c, 0, 0}, {0, 0, 1, 0}}};
    pose[1] = (nt_skeletal_mat34_t){.r = {{c, -s, 0, c}, {s, c, 0, s}, {0, 0, 1, 0}}};
}

static bool update(GarmentActor *actor, const nt_skeletal_mat34_t *pose, const float world[16], float dt, bool reset) {
    const GarmentInput in = {.pose = pose, .world = world, .dt = dt, .reset = reset, .render = true,
                             .max_distance_scale = 1.0F};
    return garment_actor_update(actor, &set, &rig, &in);
}

static void test_a_two_joint_arm_turning_90_degrees_puts_its_tip_on_the_arc(void) {
    GarmentActor *actor = &actors[0];
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    TEST_ASSERT_TRUE(update(actor, pose, identity_world, 0.0F, true));
    NEAR(1e-5F, 2.0F, actor->spheres[0].c[0]);
    /* The frame spans 6.5 substeps: six run, and the last of them is 12/13 of the way. */
    arm(1.5707963F, pose);
    TEST_ASSERT_TRUE(update(actor, pose, identity_world, 13.0F / 360.0F, false));
    TEST_ASSERT_EQUAL_UINT(6, actor->stats.steps);
    const float *tip = actor->spheres[0].c;
    NEAR(1e-4F, 2.0F, hypotf(tip[0], tip[1]));
    NEAR(1e-3F, 1.5707963F * 12.0F / 13.0F, atan2f(tip[1], tip[0]));
}

static void test_palette_follows_the_reference_before_the_first_substep(void) {
    GarmentActor *actor = &actors[0];
    nt_skeletal_mat34_t pose[2];
    arm(0.7F, pose);
    TEST_ASSERT_TRUE(update(actor, pose, identity_world, 0.0F, true));
    TEST_ASSERT_EQUAL_INT(3, actor->palette_count);
    /* At a reset every vertex sits on its reference: the shoulder's rotation of the rest point. */
    for (int v = 0; v < 3; ++v) {
        float expected[3] = {0};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) expected[i] += pose[0].r[i][j] * rest[v][j];
        for (int i = 0; i < 3; ++i) NEAR(1e-5F, expected[i], actor->palette[v].r[0][i]);
    }
    /* Less than one substep of time: no substep runs, yet the pinned record follows the moved pose. */
    pose[0].r[0][3] += 0.02F;
    pose[1].r[0][3] += 0.02F;
    TEST_ASSERT_TRUE(update(actor, pose, identity_world, 0.001F, false));
    TEST_ASSERT_EQUAL_UINT(0, actor->stats.steps);
    NEAR(1e-5F, pose[0].r[0][3], actor->palette[0].r[0][0]);
}

static void test_reset_puts_the_cloth_on_the_reference_after_a_teleport(void) {
    GarmentActor *actor = &actors[0];
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    TEST_ASSERT_TRUE(update(actor, pose, identity_world, 0.0F, true));
    for (int i = 0; i < 20; ++i) {
        arm(0.05F * (float)i, pose);
        TEST_ASSERT_TRUE(update(actor, pose, identity_world, 1.0F / 60.0F, false));
    }
    TEST_ASSERT_TRUE(actor->stats.steps > 0);
    const float far[16] = {0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 40, 0, -3, 1};
    arm(-0.4F, pose);
    TEST_ASSERT_TRUE(update(actor, pose, far, 1.0F / 60.0F, true));
    TEST_ASSERT_EQUAL_UINT(0, actor->stats.steps);
    float expected[3] = {0};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) expected[i] += pose[0].r[i][j] * rest[2][j];
    for (int i = 0; i < 3; ++i) NEAR(1e-5F, expected[i], actor->palette[2].r[0][i]);
    TEST_ASSERT_TRUE(actor->stats.max_distance_excess_m <= 0.0F);
}

static void test_a_world_convex_holds_the_cloth_in_world_space(void) {
    GarmentActor *actor = &actors[0];
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    /* The actor stands at (3, 2, 8), turned a quarter about Y, over a floor at world y = 1.6. */
    const float world[16] = {0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 3, 2, 8, 1};
    ClothConvex floor = {.plane_count = 1, .planes = {{0, 2, 0, -3.2F}}};
    GarmentInput in = {.pose = pose, .world = world, .reset = true, .render = true, .convexes = &floor, .convex_count = 1,
                       .max_distance_scale = 1.0F};
    TEST_ASSERT_TRUE(garment_actor_update(actor, &set, &rig, &in));
    in.reset = false;
    in.dt = 1.0F / 30.0F;
    for (int i = 0; i < 30; ++i) TEST_ASSERT_TRUE(garment_actor_update(actor, &set, &rig, &in));
    /* The free corner hangs a metre below the shoulder, so it rests on the floor 0.4 m below the shoulder. */
    const float corner_world_y = 2.0F + actor->palette[2].r[0][1];
    NEAR(1e-3F, 1.6F + piece.material.thickness, corner_world_y);
}

static void test_scale_zero_makes_every_vertex_kinematic(void) {
    GarmentActor *actor = &actors[0];
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    GarmentInput in = {.pose = pose, .world = identity_world, .reset = true, .render = true, .max_distance_scale = 1.0F};
    TEST_ASSERT_TRUE(garment_actor_update(actor, &set, &rig, &in));
    in.reset = false;
    in.dt = 1.0F / 30.0F;
    for (int i = 0; i < 10; ++i) TEST_ASSERT_TRUE(garment_actor_update(actor, &set, &rig, &in));
    TEST_ASSERT_TRUE(actor->stats.max_displacement_m > 1e-3F);
    in.max_distance_scale = 0.0F;
    TEST_ASSERT_TRUE(garment_actor_update(actor, &set, &rig, &in));
    for (int v = 0; v < 3; ++v)
        for (int k = 0; k < 3; ++k) NEAR(1e-6F, rest[v][k], actor->palette[v].r[0][k]);
}

static void test_shared_asset_instances_keep_their_own_state(void) {
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    TEST_ASSERT_TRUE(update(&actors[0], pose, identity_world, 0.0F, true));
    TEST_ASSERT_TRUE(update(&actors[1], pose, identity_world, 0.0F, true));
    for (int i = 0; i < 10; ++i) {
        arm(0.08F * (float)i, pose);
        TEST_ASSERT_TRUE(update(&actors[0], pose, identity_world, 1.0F / 60.0F, false));
    }
    arm(0.0F, pose);
    TEST_ASSERT_TRUE(update(&actors[1], pose, identity_world, 1.0F / 60.0F, false));
    NEAR(1e-5F, 2.0F, actors[1].spheres[0].c[0]);
    TEST_ASSERT_TRUE(actors[0].spheres[0].c[1] > 0.5F);
}

static void test_missing_joint_or_bad_input_is_refused(void) {
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    GarmentActor *actor = &actors[0];
    GarmentInput in = {.pose = pose, .world = identity_world, .reset = true, .max_distance_scale = -1.0F};
    TEST_ASSERT_FALSE(garment_actor_update(actor, &set, &rig, &in));
    in.max_distance_scale = 1.0F;
    in.convex_count = 1;
    TEST_ASSERT_FALSE(garment_actor_update(actor, &set, &rig, &in));
    const uint32_t other[] = {nt_hash32_str("shoulder").value, nt_hash32_str("wrist").value};
    const nt_skeletal_skeleton_t partial = {.parent = parents, .joint_id = other, .joint_count = 2};
    in.convex_count = 0;
    TEST_ASSERT_FALSE(garment_actor_update(actor, &set, &partial, &in));
    TEST_ASSERT_EQUAL_INT(1, actor->stats.missing_bindings);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_a_two_joint_arm_turning_90_degrees_puts_its_tip_on_the_arc);
    RUN_TEST(test_palette_follows_the_reference_before_the_first_substep);
    RUN_TEST(test_reset_puts_the_cloth_on_the_reference_after_a_teleport);
    RUN_TEST(test_a_world_convex_holds_the_cloth_in_world_space);
    RUN_TEST(test_scale_zero_makes_every_vertex_kinematic);
    RUN_TEST(test_shared_asset_instances_keep_their_own_state);
    RUN_TEST(test_missing_joint_or_bad_input_is_refused);
    return UNITY_END();
}
