#include "garment/asset.h"
#include "garment/cloth.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* After the C library: Unity defines noreturn, which the CRT headers spell as a declspec. */
#include "unity.h"

/* The host's C ABI, compiled into this test from tools/host/garment_host.c. */
typedef struct GarmentHost GarmentHost;
GarmentHost *gh_load(const void *blob, uint32_t size, int joint_count, const char *const *names, const uint16_t *parents);
void gh_free(GarmentHost *h);
int gh_piece_vertices(const GarmentHost *h, int piece);
bool gh_step(GarmentHost *h, const float *pose, const float *world, float dt, bool reset, bool render, bool measure,
             const ClothConvex *convexes, int convex_count, float max_distance_scale, float max_distance_bias);
bool gh_positions(const GarmentHost *h, int piece, float *out);
bool gh_metrics(const GarmentHost *h, float out[8]);

void setUp(void) {}
void tearDown(void) {}

static unsigned char *read_fixture(uint32_t *size) {
    FILE *f = fopen(GARMENT_HOST_FIXTURE, "rb");
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc((size_t)length);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_TRUE(fread(data, 1, (size_t)length, f) == (size_t)length);
    fclose(f);
    *size = (uint32_t)length;
    return data;
}

static ClothState direct;
static ClothReference ref[CLOTH_VERTICES];
static float host_x[CLOTH_VERTICES][3];

/* The sheet of test_host.py drapes over its cylinder and the floor for a second, at the identity pose: the host must
   give what the solver gives when stepped directly with the same reference, shapes and substeps. */
static void test_the_host_steps_the_solver_it_is_built_from(void) {
    uint32_t size;
    unsigned char *blob = read_fixture(&size);
    const char *names[] = {"root"};
    const uint16_t parents[] = {UINT16_MAX};
    GarmentHost *host = gh_load(blob, size, 1, names, parents);
    TEST_ASSERT_NOT_NULL(host);
    const GarmentSet *set = garment_asset_view(blob, size);
    TEST_ASSERT_NOT_NULL(set);
    const GarmentAsset *piece = &set->pieces[0];
    const ClothMesh *mesh = piece->mesh;
    TEST_ASSERT_EQUAL_INT(mesh->vertices, gh_piece_vertices(host, 0));

    for (int i = 0; i < mesh->vertices; ++i) {
        memcpy(ref[i].x, mesh->rest[i], sizeof ref[i].x);
        const float *d = piece->direction[i];
        float length = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        for (int k = 0; k < 3; ++k) ref[i].b[k] = d[k] / length;
    }
    ClothSphere spheres[2];
    for (int i = 0; i < 2; ++i) {
        memcpy(spheres[i].c, set->spheres[i].center, sizeof spheres[i].c);
        memcpy(spheres[i].prev, spheres[i].c, sizeof spheres[i].prev);
        spheres[i].r = set->spheres[i].radius;
    }
    const ClothCone cone = {(uint8_t)set->cones[0].a, (uint8_t)set->cones[0].b};
    const ClothConvex floor = {.plane_count = 1, .planes = {{0.0F, 1.0F, 0.0F, 0.08F}}};
    const float still[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
    ClothShapes shapes = {.spheres = spheres, .sphere_count = 2, .cones = &cone, .cone_count = 1, .convexes = &floor,
                          .convex_count = 1};
    const ClothFrame frame = {.linear_inertia = piece->linear_inertia, .angular_inertia = piece->angular_inertia};

    const float pose[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    TEST_ASSERT_TRUE(gh_step(host, pose, NULL, 0.0F, true, true, true, &floor, 1, 1.0F, 0.0F));
    TEST_ASSERT_TRUE(cloth_reset(&direct, mesh, &piece->material, ref, NULL, &shapes));
    shapes.convex_move = still;
    unsigned substeps = 0;
    float metrics[8];
    for (int frame_index = 0; frame_index < 30; ++frame_index) {
        TEST_ASSERT_TRUE(gh_step(host, pose, NULL, 1.0F / 30.0F, false, true, true, &floor, 1, 1.0F, 0.0F));
        TEST_ASSERT_TRUE(gh_metrics(host, metrics));
        for (; substeps < (unsigned)metrics[1]; ++substeps)
            cloth_step(&direct, mesh, &piece->material, 1.0F / 180.0F, &frame, ref, NULL, &shapes);
        TEST_ASSERT_TRUE(direct.frame_valid);
        TEST_ASSERT_TRUE(gh_positions(host, 0, &host_x[0][0]));
        for (int i = 0; i < mesh->vertices; ++i)
            for (int k = 0; k < 3; ++k) TEST_ASSERT_TRUE(fabsf(host_x[i][k] - direct.x[i][k]) <= 1e-5F);
    }
    TEST_ASSERT_EQUAL_UINT(180, substeps);
    /* The free edges fell around the cylinder. */
    float lowest = 1.0F;
    for (int i = 0; i < mesh->vertices; ++i) lowest = fminf(lowest, host_x[i][1]);
    TEST_ASSERT_TRUE(lowest < 0.05F);
    garment_asset_free(set);
    gh_free(host);
    free(blob);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_the_host_steps_the_solver_it_is_built_from);
    return UNITY_END();
}
