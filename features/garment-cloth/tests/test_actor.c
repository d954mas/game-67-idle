#include "garment/actor.h"
#include "garment/contact.h"
#include "hash/nt_hash.h"
#include "unity.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* The two flaps touch along an unsewn edge in the rest pose and attach to different joints. */
static const float rest[][3] = {
    {0, 1, 0}, {0, 2, 0}, {1, 1, 0},
    {0, 1, 0}, {1, 1, 0}, {0, 0, 0},
};
static const ClothFace triangles[] = {{0, 2, 1}, {3, 5, 4}};
static const ClothEdge edges[] = {
    {0, 2, 1, -1, 1, 0, 0}, {2, 1, 0, -1, 1.41421356F, 0, 0}, {1, 0, 2, -1, 1, 0, 0},
    {3, 5, 4, -1, 1, 0, 0}, {5, 4, 3, -1, 1.41421356F, 0, 0}, {4, 3, 5, -1, 1, 0, 0},
};
static const float inv_area[] = {0, 0, 6, 0, 6, 0};
static const ClothTether tethers[] = {{0, 0}, {1, 0}, {0, 1.05F}, {3, 0}, {3, 1.05F}, {5, 0}};
static const ClothMesh mesh = {
    .vertices = 6, .faces = 2, .edges = 6, .rest = rest, .inv_area = inv_area,
    .face = triangles, .edge = edges, .tether = tethers,
};
static const GarmentBone bones[] = {
    {"collar", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}},
    {"clasp", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}},
};
static const GarmentVertex vertices[] = {
    {.bone = {0}, .weight = {1}, .normal = {0, 0, 1}},
    {.bone = {0}, .weight = {1}, .normal = {0, 0, 1}},
    {.bone = {0}, .weight = {1}, .normal = {0, 0, 1}, .travel_m = 1, .backstop_m = 1},
    {.bone = {1}, .weight = {1}, .normal = {0, 0, 1}},
    {.bone = {1}, .weight = {1}, .normal = {0, 0, 1}, .travel_m = 1, .backstop_m = 1},
    {.bone = {1}, .weight = {1}, .normal = {0, 0, 1}},
};
static const GarmentCollider clasp_collider = {
    .bone_a = 1, .bone_b = 1, .point_a = {3, 1, 0}, .point_b = {3, 2, 0},
    .radius_m = 0.1F, .radius_b_m = 0.05F,
};
static const GarmentAsset garment = {
    .id = "flaps", .bones = bones, .bone_count = 2, .mesh = &mesh, .vertices = vertices,
    .material = {.density = 0.2F, .stretch = 0.01F, .iterations = 2},
    .colliders = &clasp_collider, .collider_count = 1,
};
static const GarmentSet set = {.id = "mantle", .pieces = &garment, .piece_count = 1};
static const nt_skeletal_mat34_t identity = {.r = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};
static const float world[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static GarmentActor actors[2];

void setUp(void) {}
void tearDown(void) {
    garment_actor_clear(&actors[0]);
    garment_actor_clear(&actors[1]);
}

/* One free corner on a two-joint rig: "leg" carries the pinned edge, "hem" carries a contact capsule. */
static void step_panel(GarmentActor *actor, const nt_skeletal_mat34_t pose[2], float dt) {
    static const float positions[][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    static const float panel_inv_area[] = {0, 0, 1};
    static const ClothFace face = {0, 1, 2};
    static const ClothEdge panel_edges[] = {{0, 1, 2, -1, 1, 0, 0}, {0, 2, 1, -1, 1, 0, 0}, {1, 2, 0, -1, 1.41421356F, 0, 0}};
    static const ClothMesh panel_mesh = {.vertices = 3, .faces = 1, .edges = 3, .rest = positions, .inv_area = panel_inv_area,
                                         .face = &face, .edge = panel_edges};
    static const GarmentBone panel_bones[] = {{"leg", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}},
                                              {"hem", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}}};
    static const GarmentVertex panel_vertices[] = {{.weight = {1}, .normal = {0, 0, 1}},
                                                   {.weight = {1}, .normal = {0, 0, 1}},
                                                   {.weight = {1}, .normal = {0, 0, 1}, .travel_m = 1.0F, .backstop_m = 1.0F}};
    static const GarmentCollider hem_capsule = {
        .bone_a = 1, .bone_b = 1, .point_a = {-0.13F, 0.9F, 0}, .point_b = {-0.13F, 1.1F, 0}, .radius_m = 0.12F};
    static const GarmentAsset piece = {.id = "panel", .bones = panel_bones, .bone_count = 2, .mesh = &panel_mesh,
                                       .vertices = panel_vertices,
                                       .material = {.density = 0.2F, .iterations = 1, .stretch = 0.03F, .contact_friction = 1.0F},
                                       .colliders = &hem_capsule, .collider_count = 1};
    static const GarmentSet outfit = {.id = "panel", .pieces = &piece, .piece_count = 1};
    const uint32_t ids[] = {nt_hash32_str("leg").value, nt_hash32_str("hem").value};
    const nt_skeletal_skeleton_t rig = {.joint_id = ids, .joint_count = 2};
    const float transform[] = {0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 3, 2, 8, 1};
    TEST_ASSERT_TRUE(garment_actor_update(actor, &outfit, &rig, pose, transform, dt, true, NULL));
}

static void test_body_guide_reaches_surface_palette_before_first_physics_tick(void) {
    GarmentActor *actor = &actors[0];
    nt_skeletal_mat34_t pose[] = {{.r = {{0, -1, 0, .25F}, {1, 0, 0, -.1F}, {0, 0, 1, .05F}}}, identity};
    step_panel(actor, pose, 0);
    TEST_ASSERT_EQUAL_INT(3, actor->palette_count);
    /* Each palette record carries the guide joint's rotation as a quaternion and its translation. */
    for (int v = 0; v < 3; ++v) {
        const float qx = actor->palette[v].r[0][3], qy = actor->palette[v].r[1][3];
        const float qz = actor->palette[v].r[2][0], qw = actor->palette[v].r[2][1];
        const float rot[3][3] = {
            {1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy - qw * qz), 2 * (qx * qz + qw * qy)},
            {2 * (qx * qy + qw * qz), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz - qw * qx)},
            {2 * (qx * qz - qw * qy), 2 * (qy * qz + qw * qx), 1 - 2 * (qx * qx + qy * qy)},
        };
        for (int i = 0; i < 3; ++i) {
            float t = actor->palette[v].r[0][i];
            for (int j = 0; j < 3; ++j) {
                TEST_ASSERT_TRUE(fabsf(pose[0].r[i][j] - rot[i][j]) < 1e-5F);
                t -= rot[i][j] * actor->palette[v].r[1][j];
            }
            TEST_ASSERT_TRUE(fabsf(pose[0].r[i][3] - t) < 1e-5F);
        }
    }
    /* Less than one physics tick of time: the pinned record still follows the moved guide. */
    pose[0].r[0][3] += .02F;
    step_panel(actor, pose, .001F);
    TEST_ASSERT_EQUAL_UINT(0, actor->stats.steps);
    TEST_ASSERT_TRUE(fabsf(pose[0].r[0][3] - actor->palette[0].r[0][0]) < 1e-5F);
}

/* The same motion fed at 30, 60, 120 Hz and an uneven 40/120 Hz cadence leaves the corner in one place. */
static void test_moving_capsule_contact_matches_across_pose_rates(void) {
    const int rates[] = {30, 60, 120, 60};
    float reference[3] = {0};
    GarmentActor *actor = &actors[0];
    for (int trial = 0; trial < 4; ++trial) {
        nt_skeletal_mat34_t pose[] = {identity, identity};
        garment_actor_clear(actor);
        step_panel(actor, pose, 0.0F);
        float elapsed = 0.0F;
        for (int i = 0; i < rates[trial] / 3; ++i) {
            const float dt = trial == 3 ? (i % 2 ? 1.0F / 40.0F : 1.0F / 120.0F) : 1.0F / (float)rates[trial];
            elapsed += dt;
            pose[1].r[0][3] = elapsed;
            step_panel(actor, pose, dt);
        }
        TEST_ASSERT_EQUAL_UINT(GARMENT_SIM_HZ / 3, actor->stats.steps);
        const float *corner = actor->palette[2].r[0];
        TEST_ASSERT_TRUE(corner[0] > 0.1F);
        for (int axis = 0; axis < 3; ++axis) {
            if (trial == 0) reference[axis] = corner[axis];
            else TEST_ASSERT_TRUE(fabsf(reference[axis] - corner[axis]) < 1e-4F);
        }
    }
}

typedef struct {
    float offset;
    ClothCapsule capsule;
    int tethers, capsule_lines, bones;
    bool collar_weight, clasp_weight;
} Inspection;

static void inspect(void *user, unsigned layer, const float a[3], const float b[3], const float color[3]) {
    Inspection *out = user;
    if (layer == CLOTH_DEBUG_SKELETON) {
        ++out->bones;
        TEST_ASSERT_TRUE(fabsf(a[0]) < 1e-5F);
        TEST_ASSERT_TRUE(fabsf(out->offset - b[0]) < 1e-5F);
    } else if (layer == CLOTH_DEBUG_TETHERS) {
        ++out->tethers;
        TEST_ASSERT_TRUE(fabsf(1 - b[1]) < 1e-5F);
        TEST_ASSERT_TRUE(fabsf(b[0]) < 1e-5F || fabsf(b[0] - out->offset) < 1e-5F);
    } else if (layer == CLOTH_DEBUG_CAPSULES) {
        float normal[3];
        ++out->capsule_lines;
        const float depth = cloth_point_capsule_contact(a, &out->capsule, 0.01F, normal, NULL);
        TEST_ASSERT_TRUE(fabsf(depth - 0.01F) < 1e-5F);
    } else if (layer == CLOTH_DEBUG_WEIGHTS && fabsf(a[1] - 1) < 1e-5F && fabsf(a[2]) < 1e-5F) {
        if (fabsf(a[0]) < 1e-5F && color[2] > color[0]) out->collar_weight = true;
        if (fabsf(a[0] - out->offset) < 1e-5F && color[0] > color[2]) out->clasp_weight = true;
    }
}

static void test_shared_asset_instances_keep_their_own_pins_contacts_and_inspection(void) {
    const uint32_t ids[] = {nt_hash32_str("collar").value, nt_hash32_str("clasp").value};
    const uint16_t parents[] = {UINT16_MAX, 0};
    const nt_skeletal_skeleton_t rig = {.joint_id = ids, .joint_count = 2, .parent = parents};
    nt_skeletal_mat34_t poses[2][2] = {{identity, identity}, {identity, identity}};
    const float offsets[] = {-0.25F, 0.5F};
    for (int frame = 0; frame <= 60; ++frame) {
        for (int i = 0; i < 2; ++i) {
            const int fps = i == 0 ? 30 : 60;
            if (i == 0 && frame % 2 != 0) continue;
            poses[i][1].r[0][3] = offsets[i] * (float)frame / 60.0F;
            TEST_ASSERT_TRUE(garment_actor_update(&actors[i], &set, &rig, poses[i], world, frame ? 1.0F / (float)fps : 0, true, NULL));
        }
    }
    for (int i = 0; i < 2; ++i) {
        const GarmentActor *actor = &actors[i];
        TEST_ASSERT_EQUAL_UINT(GARMENT_SIM_HZ, actor->stats.steps);
        TEST_ASSERT_EQUAL_INT(mesh.vertices, actor->palette_count);
        TEST_ASSERT_TRUE(fabsf(actor->palette[0].r[0][0]) < 1e-5F);
        TEST_ASSERT_TRUE(fabsf(offsets[i] - actor->palette[3].r[0][0]) < 1e-5F);
        Inspection result = {
            .offset = offsets[i], .capsule = {.a = {3 + offsets[i], 1, 0}, .b = {3 + offsets[i], 2, 0},
                                              .radius = 0.1F, .radius_b = 0.05F},
        };
        unsigned layers = CLOTH_DEBUG_WEIGHTS | CLOTH_DEBUG_TETHERS | CLOTH_DEBUG_CAPSULES | CLOTH_DEBUG_SKELETON;
        TEST_ASSERT_TRUE(garment_actor_debug_lines(actor, &rig, poses[i], layers, 1, inspect, &result));
        TEST_ASSERT_TRUE(result.clasp_weight && result.collar_weight);
        TEST_ASSERT_TRUE(result.capsule_lines > 0);
        TEST_ASSERT_EQUAL_INT(2, result.tethers);
        TEST_ASSERT_EQUAL_INT(1, result.bones);
    }
}

static void test_world_support_applies_only_to_the_wearer_above_its_finite_face(void) {
    const uint32_t ids[] = {nt_hash32_str("collar").value, nt_hash32_str("clasp").value};
    const uint16_t parents[] = {UINT16_MAX, 0};
    const nt_skeletal_skeleton_t rig = {.joint_id = ids, .joint_count = 2, .parent = parents};
    nt_skeletal_mat34_t pose[] = {identity, identity};
    const float transform[] = {0,0,-2,0, 0,1,0,0, 1,0,0,0, 12,2,-4,1};
    const ClothSupport face = {.center = {12,3.1F,-6}, .half_u = {0,0,-0.4F}, .half_v = {-0.18F,0.12F,0}, .depth = 0.3F};
    pose[1].r[0][3] = 3.0F;
    for (int frame = 0; frame < 10; ++frame) {
        float elapsed = frame ? 1.0F / 60.0F : 0.0F;
        TEST_ASSERT_TRUE(garment_actor_update(&actors[0], &set, &rig, pose, transform, elapsed, true, &face));
        TEST_ASSERT_TRUE(garment_actor_update(&actors[1], &set, &rig, pose, transform, elapsed, true, NULL));
        const float *p = actors[0].palette[2].r[0];
        TEST_ASSERT_TRUE(0.18F * (p[1] - 1.1F) + 0.12F * p[2] >= -1e-5F);
        for (int axis = 0; axis < 3; ++axis)
            TEST_ASSERT_TRUE(fabsf(actors[1].palette[4].r[0][axis] - actors[0].palette[4].r[0][axis]) < 1e-5F);
        TEST_ASSERT_TRUE(actors[1].palette[2].r[0][1] < 1.05F);
    }
}

static void near(float value, float expected, float tolerance) {
    char message[96];
    snprintf(message, sizeof message, "%.6f, expected %.6f", (double)value, (double)expected);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(value - expected) <= tolerance, message);
}

/* A strongly driven corner slides up inside a fixed post: its target is pushed out of the post, so the corner stays
   outside it. The pushed targets move linearly with the pose, so every pose rate, with or without samples that run
   no step, gives the same corner. Interpolating from an unpushed previous sample, or skipping the push on
   a sample without steps, would make the result depend on the rate. */
static void test_pushed_targets_are_interpolated_between_pose_samples(void) {
    static const float positions[][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    static const float slide_inv_area[] = {0, 0, 1};
    static const float drive[] = {0, 0, 4000};
    static const ClothFace face = {0, 1, 2};
    static const ClothEdge slide_edges[] = {{0, 1, 2, -1, 1, 0, 0}, {0, 2, 1, -1, 1, 0, 0}, {1, 2, 0, -1, 1.41421356F, 0, 0}};
    static const ClothMesh slide_mesh = {.vertices = 3, .faces = 1, .edges = 3, .rest = positions, .inv_area = slide_inv_area,
                                         .face = &face, .edge = slide_edges, .drive = drive};
    static const GarmentBone slide_bones[] = {{"slide", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}},
                                              {"post", {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}}};
    static const GarmentVertex slide_vertices[] = {{.weight = {1}, .normal = {0, 0, 1}},
                                                   {.weight = {1}, .normal = {0, 0, 1}},
                                                   {.weight = {1}, .normal = {1, 0, 0}, .travel_m = 1.0F, .backstop_m = 1.0F}};
    static const GarmentCollider post = {.bone_a = 1, .bone_b = 1, .point_a = {-0.05F, 0.5F, 0}, .point_b = {-0.05F, 4, 0},
                                         .radius_m = 0.1F};
    static const GarmentAsset piece = {.id = "slide", .bones = slide_bones, .bone_count = 2, .mesh = &slide_mesh,
                                       .vertices = slide_vertices,
                                       .material = {.density = 0.2F, .iterations = 2, .stretch = 1.0F, .thickness = 0.005F},
                                       .colliders = &post, .collider_count = 1};
    static const GarmentSet outfit = {.id = "slide", .pieces = &piece, .piece_count = 1};
    const uint32_t ids[] = {nt_hash32_str("slide").value, nt_hash32_str("post").value};
    const nt_skeletal_skeleton_t rig = {.joint_id = ids, .joint_count = 2};
    const float surface = -0.05F + 0.1F + 0.005F;
    GarmentActor *actor = &actors[0];
    float reference[3] = {0};
    for (int trial = 0; trial < 3; ++trial) {
        nt_skeletal_mat34_t pose[] = {identity, identity};
        garment_actor_clear(actor);
        TEST_ASSERT_TRUE(garment_actor_update(actor, &outfit, &rig, pose, world, 0.0F, true, NULL));
        TEST_ASSERT_TRUE(actor->palette[2].r[0][0] >= surface - 1e-4F);
        const int samples = trial == 0 ? 10 : 20;
        float elapsed = 0.0F;
        for (int i = 0; i < samples; ++i) {
            const float dt = 1.0F / 3.0F / (float)samples;
            if (trial == 2) {
                /* A sample too short for a step between two that step. */
                pose[0].r[1][3] = 0.6F * (elapsed + 0.0005F);
                TEST_ASSERT_TRUE(garment_actor_update(actor, &outfit, &rig, pose, world, 0.0005F, true, NULL));
                elapsed += 0.0005F;
                TEST_ASSERT_TRUE(garment_actor_update(actor, &outfit, &rig, pose, world, 0.0F, true, NULL));
                elapsed += dt - 0.0005F;
                pose[0].r[1][3] = 0.6F * elapsed;
                TEST_ASSERT_TRUE(garment_actor_update(actor, &outfit, &rig, pose, world, dt - 0.0005F, true, NULL));
            } else {
                elapsed += dt;
                pose[0].r[1][3] = 0.6F * elapsed;
                TEST_ASSERT_TRUE(garment_actor_update(actor, &outfit, &rig, pose, world, dt, true, NULL));
            }
        }
        TEST_ASSERT_EQUAL_UINT(GARMENT_SIM_HZ / 3, actor->stats.steps);
        const float *corner = actor->palette[2].r[0];
        TEST_ASSERT_TRUE(corner[0] >= surface - 1e-3F);
        for (int axis = 0; axis < 3; ++axis) {
            if (trial == 0) reference[axis] = corner[axis];
            else near(corner[axis], reference[axis], 1e-4F);
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_shared_asset_instances_keep_their_own_pins_contacts_and_inspection);
    RUN_TEST(test_world_support_applies_only_to_the_wearer_above_its_finite_face);
    RUN_TEST(test_body_guide_reaches_surface_palette_before_first_physics_tick);
    RUN_TEST(test_moving_capsule_contact_matches_across_pose_rates);
    RUN_TEST(test_pushed_targets_are_interpolated_between_pose_samples);
    return UNITY_END();
}
