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
                             .max_distance_scale = 1.0F, .measure = true};
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

/* Near +-120 degrees glm_mat3_quat switches branch, so two nearly equal rotations come back as q and about -q. */
static void test_nearly_equal_rotations_of_opposite_quaternion_sign_interpolate_between_them(void) {
    GarmentActor *actor = &actors[0];
    nt_skeletal_mat34_t pose[2];
    const float before = -2.0941F, after = -2.0947F;
    arm(before, pose);
    TEST_ASSERT_TRUE(update(actor, pose, identity_world, 0.0F, true));
    arm(after, pose);
    TEST_ASSERT_TRUE(update(actor, pose, identity_world, 13.0F / 360.0F, false));
    const float *tip = actor->spheres[0].c;
    NEAR(1e-4F, 2.0F, hypotf(tip[0], tip[1]));
    NEAR(1e-3F, before + (after - before) * 12.0F / 13.0F, atan2f(tip[1], tip[0]));
}

/* Static friction against a floor holds a resting vertex in world space while the root walks and turns. */
static void test_static_friction_on_a_floor_holds_the_cloth_in_world_space(void) {
    static const GarmentAsset grippy = {.id = "panel", .mesh = &mesh, .skin = skin, .direction = direction,
                                        .material = {.density = 0.2F, .stretch = 0.03F, .iterations = 1, .friction = 20.0F}};
    static const GarmentSet outfit = {.id = "arm", .bones = bones, .bone_count = 2, .pieces = &grippy, .piece_count = 1};
    GarmentActor *actor = &actors[0];
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    /* The corner hangs 1 m below the shoulder; the floor 0.9 m below it holds it with slack in its tether. */
    float world[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 2, 0, 1};
    ClothConvex floor = {.plane_count = 1, .planes = {{0, 1, 0, -1.1F}}};
    GarmentInput in = {.pose = pose, .world = world, .reset = true, .render = true, .convexes = &floor, .convex_count = 1,
                       .max_distance_scale = 1.0F};
    TEST_ASSERT_TRUE(garment_actor_update(actor, &outfit, &rig, &in));
    in.reset = false;
    in.dt = 1.0F / 30.0F;
    for (int i = 0; i < 10; ++i) TEST_ASSERT_TRUE(garment_actor_update(actor, &outfit, &rig, &in));
    float start[3];
    memcpy(start, actor->palette[2].r[0], sizeof start);
    start[1] += 2.0F;
    for (int i = 1; i <= 10; ++i) {
        const float yaw = 0.01F * (float)i, c = cosf(yaw), s = sinf(yaw);
        const float moved[16] = {c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0.004F * (float)i, 2, 0, 1};
        memcpy(world, moved, sizeof world);
        TEST_ASSERT_TRUE(garment_actor_update(actor, &outfit, &rig, &in));
    }
    const float *x = actor->palette[2].r[0];
    float at[3];
    for (int k = 0; k < 3; ++k) at[k] = world[k] * x[0] + world[4 + k] * x[1] + world[8 + k] * x[2] + world[12 + k];
    for (int k = 0; k < 3; ++k) NEAR(2e-3F, start[k], at[k]);
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

/* The panel with one corrective that lifts the free corner off the arm and turns its body direction toward +X. */
static const GarmentCorrective lift[] = {{{0, 0, 0}, {0, 0, 0}, {0, 0}}, {{0, 0, 0}, {0, 0, 0}, {0, 0}},
                                         {{0, 0.2F, 0.3F}, {1, 0, 1}, {0, 0}}};
static const char *const lift_names[] = {"sit"};
static const GarmentAsset lifted = {.id = "panel", .mesh = &mesh, .skin = skin, .direction = direction, .corrective = lift,
                                    .material = {.density = 0.2F, .stretch = 0.03F, .iterations = 1, .friction = 0.5F}};
static const GarmentSet corrected = {.id = "arm", .bones = bones, .bone_count = 2, .spheres = &hand, .sphere_count = 1,
                                     .pieces = &lifted, .piece_count = 1, .corrective_names = lift_names, .corrective_count = 1};

static const ClothReference *reference_at(GarmentActor *actor, const nt_skeletal_mat34_t *pose, float weight, bool reset,
                                          float dt) {
    const GarmentInput in = {.pose = pose, .world = identity_world, .dt = dt, .reset = reset, .render = true,
                             .max_distance_scale = 1.0F, .corrective_weight = &weight};
    TEST_ASSERT_TRUE(garment_actor_update(actor, &corrected, &rig, &in));
    const ClothReference *ref = NULL;
    TEST_ASSERT_TRUE(garment_actor_piece(actor, 0, NULL, &ref));
    return ref;
}

static void test_a_corrective_is_found_by_name(void) {
    TEST_ASSERT_EQUAL_INT(0, garment_set_corrective(&corrected, "sit"));
    TEST_ASSERT_EQUAL_INT(-1, garment_set_corrective(&corrected, "stand"));
    TEST_ASSERT_EQUAL_INT(-1, garment_set_corrective(&set, "sit"));
}

/* The reference of a corrective at weight 1 is the skinned corrected rest, the body direction the skinned corrected one;
   at weight 0 it is the plain reference, bit for bit. */
static void test_a_corrective_at_full_weight_is_the_skinned_corrected_rest(void) {
    nt_skeletal_mat34_t pose[2];
    arm(0.7F, pose);
    const ClothReference *ref = reference_at(&actors[0], pose, 1.0F, true, 0.0F);
    const float shifted[3] = {rest[2][0] + lift[2].delta[0], rest[2][1] + lift[2].delta[1], rest[2][2] + lift[2].delta[2]};
    const float turned[3] = {direction[2][0] + lift[2].direction[0], direction[2][1] + lift[2].direction[1],
                             direction[2][2] + lift[2].direction[2]};
    const float length = sqrtf(turned[0] * turned[0] + turned[1] * turned[1] + turned[2] * turned[2]);
    for (int i = 0; i < 3; ++i) {
        float x = 0.0F, b = 0.0F;
        for (int j = 0; j < 3; ++j) {
            x += pose[0].r[i][j] * shifted[j];
            b += pose[0].r[i][j] * turned[j] / length;
        }
        NEAR(1e-5F, x, ref[2].x[i]);
        NEAR(1e-5F, b, ref[2].b[i]);
    }
    const GarmentInput plain = {.pose = pose, .world = identity_world, .reset = true, .render = true, .max_distance_scale = 1.0F};
    TEST_ASSERT_TRUE(garment_actor_update(&actors[1], &set, &rig, &plain));
    const ClothReference *base = NULL;
    TEST_ASSERT_TRUE(garment_actor_piece(&actors[1], 0, NULL, &base));
    ref = reference_at(&actors[0], pose, 0.0F, true, 0.0F);
    TEST_ASSERT_EQUAL_MEMORY(base, ref, 3 * sizeof *ref);
}

/* Skinning is linear in the rest point, so the reference at a weight lies that fraction of the way between the plain
   and the fully corrected one. */
static void test_corrective_weights_blend_linearly(void) {
    nt_skeletal_mat34_t pose[2];
    arm(-0.4F, pose);
    ClothReference ends[2], mid;
    memcpy(&ends[0], &reference_at(&actors[0], pose, 0.0F, true, 0.0F)[2], sizeof mid);
    memcpy(&ends[1], &reference_at(&actors[0], pose, 1.0F, true, 0.0F)[2], sizeof mid);
    memcpy(&mid, &reference_at(&actors[0], pose, 0.3F, true, 0.0F)[2], sizeof mid);
    for (int k = 0; k < 3; ++k) NEAR(1e-5F, ends[0].x[k] + 0.3F * (ends[1].x[k] - ends[0].x[k]), mid.x[k]);
}

/* A reset puts the cloth on the corrected reference; a weight changed between frames reaches its new reference by the
   frame's end. */
static void test_a_reset_with_a_corrective_puts_the_cloth_on_the_corrected_reference(void) {
    GarmentActor *actor = &actors[0];
    nt_skeletal_mat34_t pose[2];
    arm(0.3F, pose);
    const ClothReference *ref = reference_at(actor, pose, 1.0F, true, 0.0F);
    const ClothState *cloth = NULL;
    TEST_ASSERT_TRUE(garment_actor_piece(actor, 0, &cloth, NULL));
    for (int v = 0; v < 3; ++v)
        for (int k = 0; k < 3; ++k) NEAR(1e-6F, ref[v].x[k], cloth->x[v][k]);
    (void)reference_at(actor, pose, 0.0F, true, 0.0F);
    ClothReference full;
    memcpy(&full, &reference_at(&actors[1], pose, 1.0F, true, 0.0F)[2], sizeof full);
    ref = reference_at(actor, pose, 1.0F, false, 6.0F / 180.0F);
    TEST_ASSERT_EQUAL_UINT(6, actor->stats.steps);
    for (int k = 0; k < 3; ++k) NEAR(1e-6F, full.x[k], ref[2].x[k]);
    TEST_ASSERT_TRUE(actor->corrective_weight[0] == 1.0F);
}

static void test_a_corrective_weight_outside_zero_to_one_is_refused(void) {
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    const float weights[] = {1.5F, -0.1F, NAN};
    for (int i = 0; i < 3; ++i) {
        const GarmentInput in = {.pose = pose, .world = identity_world, .reset = true, .max_distance_scale = 1.0F,
                                 .corrective_weight = &weights[i]};
        TEST_ASSERT_FALSE(garment_actor_update(&actors[0], &corrected, &rig, &in));
    }
}

/* The panel's pinned shoulder vertex carries a corrective: a kinematic vertex sits on the reference of every
   substep, so it shows the weight the actor used at the last substep of a frame. The runtime takes this asset as
   it is; the writer would refuse a kinematic delta. */
static const GarmentCorrective pin_lift[] = {{{0, 0.18F, 0}, {0, 0, 0}, {0, 0}}, {{0, 0, 0}, {0, 0, 0}, {0, 0}},
                                             {{0, 0, 0}, {0, 0, 0}, {0, 0}}};
static const GarmentAsset pin_lifted = {.id = "panel", .mesh = &mesh, .skin = skin, .direction = direction,
                                        .corrective = pin_lift, .material = {.density = 0.2F, .stretch = 0.03F, .iterations = 1}};
static const GarmentSet pin_corrected = {.id = "arm", .bones = bones, .bone_count = 2, .pieces = &pin_lifted,
                                         .piece_count = 1, .corrective_names = lift_names, .corrective_count = 1};

static float pinned_lift(GarmentActor *actor, const nt_skeletal_mat34_t *pose, const float *weight, bool reset, float dt) {
    const GarmentInput in = {.pose = pose, .world = identity_world, .dt = dt, .reset = reset, .max_distance_scale = 1.0F,
                             .corrective_weight = weight};
    TEST_ASSERT_TRUE(garment_actor_update(actor, &pin_corrected, &rig, &in));
    const ClothState *cloth = NULL;
    TEST_ASSERT_TRUE(garment_actor_piece(actor, 0, &cloth, NULL));
    return cloth->x[0][1] / pin_lift[0].delta[1];
}

/* A weight change from 0 to 1 over a frame of 6.5 substeps: the last substep runs at 12/13 of the frame, so the
   reference it used carries 12/13 of the shape, not all of it and not none. */
static void test_a_weight_change_blends_across_the_substeps(void) {
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    const float none = 0.0F, full = 1.0F;
    NEAR(1e-6F, 0.0F, pinned_lift(&actors[0], pose, &none, true, 0.0F));
    NEAR(1e-5F, 12.0F / 13.0F, pinned_lift(&actors[0], pose, &full, false, 13.0F / 360.0F));
    TEST_ASSERT_EQUAL_UINT(6, actors[0].stats.steps);
    /* The next frame starts where this one's weights ended (1) and stays there. */
    NEAR(1e-6F, 1.0F, pinned_lift(&actors[0], pose, &full, false, 6.0F / 180.0F));
}

/* NULL weights after given ones are a ramp to 0 across the substeps, not a jump. */
static void test_null_weights_after_given_ones_ramp_to_zero(void) {
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    const float full = 1.0F;
    NEAR(1e-6F, 1.0F, pinned_lift(&actors[0], pose, &full, true, 0.0F));
    NEAR(1e-5F, 1.0F / 13.0F, pinned_lift(&actors[0], pose, NULL, false, 13.0F / 360.0F));
    TEST_ASSERT_TRUE(actors[0].corrective_weight[0] == 0.0F);
}

/* A new outfit (another set) binds the actor again: the weights the last outfit left are gone, and the actor starts
   from the reference at the weights it is given, without a blend from the old ones. */
static void test_an_outfit_switch_resets_the_weights(void) {
    static const GarmentSet other = {.id = "arm", .bones = bones, .bone_count = 2, .pieces = &pin_lifted, .piece_count = 1,
                                     .corrective_names = lift_names, .corrective_count = 1};
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    const float full = 1.0F;
    NEAR(1e-6F, 1.0F, pinned_lift(&actors[0], pose, &full, true, 0.0F));
    const GarmentInput in = {.pose = pose, .world = identity_world, .dt = 13.0F / 360.0F, .max_distance_scale = 1.0F};
    TEST_ASSERT_TRUE(garment_actor_update(&actors[0], &other, &rig, &in));
    TEST_ASSERT_EQUAL_UINT(0, actors[0].stats.steps);
    TEST_ASSERT_TRUE(actors[0].corrective_weight[0] == 0.0F);
    const ClothState *cloth = NULL;
    TEST_ASSERT_TRUE(garment_actor_piece(&actors[0], 0, &cloth, NULL));
    NEAR(1e-6F, 0.0F, cloth->x[0][1]);
    /* Back on the first outfit with a weight: it binds again and starts there at once. */
    NEAR(1e-6F, 1.0F, pinned_lift(&actors[0], pose, &full, false, 13.0F / 360.0F));
}

/* Two correctives and a free vertex with a backstop at rest (d 0.01, R 0.2): one key has R 0.1, the other the plane
   form, and their body directions turn the rest one toward +X and -X. */
static const ClothBackstop backed_backstop[] = {{0, 0}, {0, 0}, {0.01F, 0.2F}};
static const ClothMesh backed_mesh = {.vertices = 3, .faces = 1, .edges = 3, .rest = rest, .inv_area = inv_area,
                                      .face = &face, .edge = edges, .tether = tethers, .max_distance = max_distance,
                                      .backstop = backed_backstop};
static const GarmentCorrective pair[] = {
    {{0, 0, 0}, {0, 0, 0}, {0, 0}}, {{0, 0, 0}, {0, 0, 0}, {0, 0}}, {{0.1F, 0, 0}, {1, 0, 1}, {0.02F, 0.1F}},
    {{0, 0, 0}, {0, 0, 0}, {0, 0}}, {{0, 0, 0}, {0, 0, 0}, {0, 0}}, {{0, 0.1F, 0}, {-1, 0, 1}, {0.03F, INFINITY}}};
static const char *const pair_names[] = {"sit", "sit_side_l"};
static const GarmentAsset paired = {.id = "panel", .mesh = &backed_mesh, .skin = skin, .direction = direction,
                                    .corrective = pair, .material = {.density = 0.2F, .stretch = 0.03F, .iterations = 1}};
static const GarmentSet two = {.id = "arm", .bones = bones, .bone_count = 2, .pieces = &paired, .piece_count = 1,
                               .corrective_names = pair_names, .corrective_count = 2};

static const ClothReference *two_at(const float weights[2], const ClothBackstop **used) {
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    const GarmentInput in = {.pose = pose, .world = identity_world, .reset = true, .max_distance_scale = 1.0F,
                             .corrective_weight = weights};
    TEST_ASSERT_TRUE(garment_actor_update(&actors[0], &two, &rig, &in));
    const ClothReference *ref = NULL;
    TEST_ASSERT_TRUE(garment_actor_piece(&actors[0], 0, NULL, &ref));
    *used = garment_actor_backstop(&actors[0], 0);
    TEST_ASSERT_NOT_NULL(*used);
    return ref;
}

/* Both at once: the deltas add with their weights, d blends linearly and so does the curvature 1/R (the plane form is
   curvature 0); a key that carries the whole weight gives its own backstop exactly. */
static void test_two_correctives_at_once_add_and_blend_the_backstop(void) {
    const ClothBackstop *bs = NULL;
    const float both[2] = {0.5F, 0.25F};
    const ClothReference *ref = two_at(both, &bs);
    NEAR(1e-6F, rest[2][0] + 0.05F, ref[2].x[0]);
    NEAR(1e-6F, rest[2][1] + 0.025F, ref[2].x[1]);
    NEAR(1e-6F, 0.25F * 0.01F + 0.5F * 0.02F + 0.25F * 0.03F, bs[2].d);
    NEAR(1e-5F, 1.0F / (0.25F / 0.2F + 0.5F / 0.1F), bs[2].R);
    const float only_plane[2] = {0.0F, 1.0F};
    two_at(only_plane, &bs);
    TEST_ASSERT_TRUE(bs[2].d == 0.03F && isinf(bs[2].R));
    const float only_first[2] = {1.0F, 0.0F};
    two_at(only_first, &bs);
    TEST_ASSERT_TRUE(bs[2].d == 0.02F && bs[2].R == 0.1F);
    /* At weight 0 the mesh's own backstop holds. */
    const float none[2] = {0.0F, 0.0F};
    two_at(none, &bs);
    TEST_ASSERT_EQUAL_PTR(backed_backstop, bs);
}

/* A key whose body has a hole behind the vertex (R = 0) turns the backstop off wherever that key weighs. */
static void test_a_key_without_a_body_behind_turns_the_backstop_off(void) {
    static const GarmentCorrective open[] = {{{0, 0, 0}, {0, 0, 0}, {0, 0}}, {{0, 0, 0}, {0, 0, 0}, {0, 0}},
                                             {{0, 0, 0}, {0, 0, 0}, {0.02F, 0}}};
    static const GarmentAsset holed = {.id = "panel", .mesh = &backed_mesh, .skin = skin, .direction = direction,
                                       .corrective = open, .material = {.density = 0.2F, .stretch = 0.03F, .iterations = 1}};
    static const GarmentSet one = {.id = "arm", .bones = bones, .bone_count = 2, .pieces = &holed, .piece_count = 1,
                                   .corrective_names = lift_names, .corrective_count = 1};
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    const float weight = 0.2F;
    const GarmentInput in = {.pose = pose, .world = identity_world, .reset = true, .max_distance_scale = 1.0F,
                             .corrective_weight = &weight};
    TEST_ASSERT_TRUE(garment_actor_update(&actors[0], &one, &rig, &in));
    TEST_ASSERT_TRUE(garment_actor_backstop(&actors[0], 0)[2].R == 0.0F);
}

/* Directions that cancel never fail the update: two keys turning the rest direction to +X and -X at equal weights
   take the first key's direction (the first of the largest coefficients). */
static void test_cancelling_corrective_directions_take_the_heaviest_one(void) {
    const ClothBackstop *bs = NULL;
    const float half[2] = {0.5F, 0.5F};
    const ClothReference *ref = two_at(half, &bs);
    NEAR(1e-6F, 1.0F, ref[2].b[0]);
    NEAR(1e-6F, 0.0F, ref[2].b[2]);
}

/* Directions that the bones cancel never fail the update either: a vertex split evenly over two bones turned half a
   turn apart takes its direction from the heaviest bone (the first of equals). */
static void test_cancelling_bone_directions_take_the_heaviest_bone(void) {
    static const GarmentSkin split[] = {{.weight = {1}}, {.weight = {1}}, {.bone = {0, 1}, .weight = {0.5F, 0.5F}}};
    static const float sideways[][3] = {{0, 0, -1}, {0, 0, -1}, {1, 0, 0}};
    static const GarmentAsset torn = {.id = "panel", .mesh = &mesh, .skin = split, .direction = sideways,
                                      .material = {.density = 0.2F, .stretch = 0.03F, .iterations = 1}};
    static const GarmentSet outfit = {.id = "arm", .bones = bones, .bone_count = 2, .pieces = &torn, .piece_count = 1};
    nt_skeletal_mat34_t pose[2];
    arm(0.0F, pose);
    pose[1] = (nt_skeletal_mat34_t){.r = {{-1, 0, 0, 1}, {0, -1, 0, 0}, {0, 0, 1, 0}}};
    const GarmentInput in = {.pose = pose, .world = identity_world, .reset = true, .max_distance_scale = 1.0F};
    TEST_ASSERT_TRUE(garment_actor_update(&actors[0], &outfit, &rig, &in));
    const ClothReference *ref = NULL;
    TEST_ASSERT_TRUE(garment_actor_piece(&actors[0], 0, NULL, &ref));
    NEAR(1e-6F, 1.0F, ref[2].b[0]);
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
    RUN_TEST(test_nearly_equal_rotations_of_opposite_quaternion_sign_interpolate_between_them);
    RUN_TEST(test_static_friction_on_a_floor_holds_the_cloth_in_world_space);
    RUN_TEST(test_palette_follows_the_reference_before_the_first_substep);
    RUN_TEST(test_reset_puts_the_cloth_on_the_reference_after_a_teleport);
    RUN_TEST(test_a_world_convex_holds_the_cloth_in_world_space);
    RUN_TEST(test_scale_zero_makes_every_vertex_kinematic);
    RUN_TEST(test_shared_asset_instances_keep_their_own_state);
    RUN_TEST(test_missing_joint_or_bad_input_is_refused);
    RUN_TEST(test_a_corrective_is_found_by_name);
    RUN_TEST(test_a_corrective_at_full_weight_is_the_skinned_corrected_rest);
    RUN_TEST(test_corrective_weights_blend_linearly);
    RUN_TEST(test_a_reset_with_a_corrective_puts_the_cloth_on_the_corrected_reference);
    RUN_TEST(test_a_corrective_weight_outside_zero_to_one_is_refused);
    RUN_TEST(test_a_weight_change_blends_across_the_substeps);
    RUN_TEST(test_null_weights_after_given_ones_ramp_to_zero);
    RUN_TEST(test_an_outfit_switch_resets_the_weights);
    RUN_TEST(test_two_correctives_at_once_add_and_blend_the_backstop);
    RUN_TEST(test_a_key_without_a_body_behind_turns_the_backstop_off);
    RUN_TEST(test_cancelling_corrective_directions_take_the_heaviest_one);
    RUN_TEST(test_cancelling_bone_directions_take_the_heaviest_bone);
    return UNITY_END();
}
