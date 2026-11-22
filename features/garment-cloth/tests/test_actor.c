#include "garment/actor.h"
#include "garment/contact.h"
#include "hash/nt_hash.h"
#include "unity.h"

#include <math.h>
#include <stdint.h>

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
static const GarmentSet set = {"mantle", &garment, 1};
static const nt_skeletal_mat34_t identity = {.r = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}}};
static const float world[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
static GarmentActor actors[2];

void setUp(void) {}
void tearDown(void) {
    garment_actor_clear(&actors[0]);
    garment_actor_clear(&actors[1]);
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
            TEST_ASSERT_TRUE(garment_actor_update(&actors[i], &set, &rig, poses[i], world, frame ? 1.0F / (float)fps : 0, true));
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
        TEST_ASSERT_TRUE(garment_actor_update_supported(&actors[0], &set, &rig, pose, transform, elapsed, true, &face));
        TEST_ASSERT_TRUE(garment_actor_update(&actors[1], &set, &rig, pose, transform, elapsed, true));
        const float *p = actors[0].palette[2].r[0];
        TEST_ASSERT_TRUE(0.18F * (p[1] - 1.1F) + 0.12F * p[2] >= -1e-5F);
        for (int axis = 0; axis < 3; ++axis)
            TEST_ASSERT_TRUE(fabsf(actors[1].palette[4].r[0][axis] - actors[0].palette[4].r[0][axis]) < 1e-5F);
        TEST_ASSERT_TRUE(actors[1].palette[2].r[0][1] < 1.05F);
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_shared_asset_instances_keep_their_own_pins_contacts_and_inspection);
    RUN_TEST(test_world_support_applies_only_to_the_wearer_above_its_finite_face);
    return UNITY_END();
}
