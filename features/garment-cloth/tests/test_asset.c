#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "garment/asset.h"
#include "unity.h"

#include "garment_asset_fixture.gen.h"

#include <signal.h>

void setUp(void) {}
void tearDown(void) {}

static uint8_t *read_file(const char *path, uint32_t *size, size_t shift) {
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    *size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(*size + shift);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_UINT32(*size, (uint32_t)fread(data + shift, 1, *size, f));
    fclose(f);
    return data;
}

static void same_edges(const ClothEdge *a, const ClothEdge *b, int count) {
    for (int i = 0; i < count; ++i) {
        TEST_ASSERT_TRUE(a[i].a == b[i].a && a[i].b == b[i].b && a[i].c == b[i].c && a[i].d == b[i].d && a[i].kind == b[i].kind);
        TEST_ASSERT_EQUAL_MEMORY(&a[i].length, &b[i].length, 2 * sizeof(float));
    }
}

static void same_piece(const GarmentAsset *e, const GarmentAsset *v) {
    TEST_ASSERT_EQUAL_STRING(e->id, v->id);
    const ClothMesh *em = e->mesh, *vm = v->mesh;
    const size_t n = (size_t)em->vertices;
    TEST_ASSERT_TRUE(em->vertices == vm->vertices && em->faces == vm->faces && em->edges == vm->edges);
    TEST_ASSERT_EQUAL_MEMORY(em->rest, vm->rest, n * sizeof em->rest[0]);
    TEST_ASSERT_EQUAL_MEMORY(em->inv_area, vm->inv_area, n * sizeof em->inv_area[0]);
    TEST_ASSERT_EQUAL_MEMORY(em->face, vm->face, (size_t)em->faces * sizeof em->face[0]);
    same_edges(em->edge, vm->edge, em->edges);
    TEST_ASSERT_EQUAL(em->tether == NULL, vm->tether == NULL);
    if (em->tether) TEST_ASSERT_EQUAL_MEMORY(em->tether, vm->tether, n * sizeof em->tether[0]);
    TEST_ASSERT_EQUAL_MEMORY(em->max_distance, vm->max_distance, n * sizeof(float));
    TEST_ASSERT_EQUAL_MEMORY(em->backstop, vm->backstop, n * sizeof em->backstop[0]);
    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_EQUAL_MEMORY(e->skin[i].bone, v->skin[i].bone, sizeof e->skin[i].bone);
        TEST_ASSERT_EQUAL_MEMORY(e->skin[i].weight, v->skin[i].weight, sizeof e->skin[i].weight);
    }
    TEST_ASSERT_EQUAL_MEMORY(e->direction, v->direction, n * sizeof e->direction[0]);
    TEST_ASSERT_EQUAL(e->corrective == NULL, v->corrective == NULL);
    if (e->corrective)
        TEST_ASSERT_EQUAL_MEMORY(e->corrective, v->corrective, (size_t)expected_set.corrective_count * n * sizeof *e->corrective);
    TEST_ASSERT_EQUAL_MEMORY(&e->material, &v->material, sizeof e->material);
    TEST_ASSERT_EQUAL_INT(e->render_vertices, v->render_vertices);
    TEST_ASSERT_EQUAL_MEMORY(&e->linear_inertia, &v->linear_inertia, sizeof(float));
    TEST_ASSERT_EQUAL_MEMORY(&e->angular_inertia, &v->angular_inertia, sizeof(float));
}

static void same_outfit(const GarmentSet *e, const GarmentSet *v) {
    TEST_ASSERT_EQUAL_INT(e->bone_count, v->bone_count);
    for (int i = 0; i < e->bone_count; ++i) {
        TEST_ASSERT_EQUAL_STRING(e->bones[i].name, v->bones[i].name);
        TEST_ASSERT_EQUAL_MEMORY(e->bones[i].inverse_bind, v->bones[i].inverse_bind, sizeof e->bones[i].inverse_bind);
    }
    TEST_ASSERT_EQUAL_INT(e->sphere_count, v->sphere_count);
    for (int i = 0; i < e->sphere_count; ++i) {
        TEST_ASSERT_EQUAL_UINT16(e->spheres[i].bone, v->spheres[i].bone);
        TEST_ASSERT_EQUAL_MEMORY(e->spheres[i].center, v->spheres[i].center, 4 * sizeof(float));
    }
    TEST_ASSERT_EQUAL_INT(e->cone_count, v->cone_count);
    TEST_ASSERT_EQUAL_MEMORY(e->cones, v->cones, (size_t)e->cone_count * sizeof e->cones[0]);
    TEST_ASSERT_EQUAL_INT(e->corrective_count, v->corrective_count);
    for (int i = 0; i < e->corrective_count; ++i) {
        TEST_ASSERT_EQUAL_STRING(e->corrective_names[i], v->corrective_names[i]);
        TEST_ASSERT_EQUAL_INT(i, garment_set_corrective(v, e->corrective_names[i]));
    }
}

/* shift 4 leaves the blob only 4-byte aligned, the least the format promises to work with. */
static void check_view(size_t shift) {
    uint32_t size = 0;
    uint8_t *data = read_file(GARMENT_FIXTURE_BLOB, &size, shift);
    const GarmentSet *set = garment_asset_view(data + shift, size);
    TEST_ASSERT_EQUAL_STRING(expected_set.id, set->id);
    TEST_ASSERT_EQUAL_INT(expected_set.piece_count, set->piece_count);
    same_outfit(&expected_set, set);
    for (int p = 0; p < set->piece_count; ++p) {
        same_piece(&expected_set.pieces[p], &set->pieces[p]);
        /* A view, not a copy: per-vertex arrays stay in the blob. */
        const uint8_t *rest = (const uint8_t *)set->pieces[p].mesh->rest;
        TEST_ASSERT_TRUE(rest >= data + shift && rest < data + shift + size);
    }
    garment_asset_free(set);
    free(data);
}

static void test_view_reads_every_field_the_writer_wrote(void) {
    check_view(0);
}

static void test_view_needs_only_four_byte_alignment(void) {
    check_view(4);
}

static void test_outfit_without_cloth_views_as_null(void) {
    uint32_t size = 0;
    uint8_t *data = read_file(GARMENT_FIXTURE_EMPTY, &size, 0);
    const GarmentSet *set = garment_asset_view(data, size);
    TEST_ASSERT_NULL(set);
    garment_asset_free(set);
    free(data);
}

/* An assert traps; CTest counts a trap as a crash, not a failure WILL_FAIL can invert, so the trap exits 3 here. */
static void trapped(int signal_number) {
    (void)signal_number;
    _Exit(3);
}

/* `--reject <case>`: view the fixture's blob of that case, which must stop on an assert; exiting 0 means the view
   accepted it, so CTest runs these with WILL_FAIL. */
static int view_rejected(const char *name) {
    (void)signal(SIGILL, trapped);
#if defined(SIGTRAP)
    (void)signal(SIGTRAP, trapped);
#endif
    char path[1024];
    const size_t stem = strlen(GARMENT_FIXTURE_BLOB) - strlen(".garment");
    (void)snprintf(path, sizeof path, "%.*s.%s.garment", (int)stem, GARMENT_FIXTURE_BLOB, name);
    /* A missing file must not pass as a rejection: it returns 0, which fails the test. */
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    const uint32_t size = (uint32_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint32_t *data = malloc(size + 4U);
    const bool read = data && fread(data, 1, size, f) == size;
    fclose(f);
    if (read) garment_asset_free(garment_asset_view(data, size));
    free(data);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--reject") == 0) {
        for (size_t i = 0; i < sizeof rejected_cases / sizeof rejected_cases[0]; ++i)
            if (strcmp(argv[2], rejected_cases[i]) == 0) return view_rejected(argv[2]);
        return 0;
    }
    UNITY_BEGIN();
    RUN_TEST(test_view_reads_every_field_the_writer_wrote);
    RUN_TEST(test_view_needs_only_four_byte_alignment);
    RUN_TEST(test_outfit_without_cloth_views_as_null);
    return UNITY_END();
}
