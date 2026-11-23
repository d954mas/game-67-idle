#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "garment/asset.h"
#include "unity.h"

#include "garment_asset_fixture.gen.h"

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

static void same_colliders(const GarmentCollider *a, const GarmentCollider *b, int count) {
    for (int i = 0; i < count; ++i) {
        TEST_ASSERT_TRUE(a[i].bone_a == b[i].bone_a && a[i].bone_b == b[i].bone_b && a[i].flags == b[i].flags);
        TEST_ASSERT_EQUAL_MEMORY(a[i].point_a, b[i].point_a, 8 * sizeof(float));
    }
}

static void same_piece(const GarmentAsset *e, const GarmentAsset *v) {
    TEST_ASSERT_EQUAL_STRING(e->id, v->id);
    TEST_ASSERT_EQUAL_INT(e->bone_count, v->bone_count);
    for (int i = 0; i < e->bone_count; ++i) {
        TEST_ASSERT_EQUAL_STRING(e->bones[i].name, v->bones[i].name);
        TEST_ASSERT_EQUAL_MEMORY(e->bones[i].inverse_bind, v->bones[i].inverse_bind, sizeof e->bones[i].inverse_bind);
    }
    const ClothMesh *em = e->mesh, *vm = v->mesh;
    TEST_ASSERT_TRUE(em->vertices == vm->vertices && em->faces == vm->faces && em->edges == vm->edges);
    TEST_ASSERT_EQUAL_MEMORY(em->rest, vm->rest, (size_t)em->vertices * sizeof em->rest[0]);
    TEST_ASSERT_EQUAL_MEMORY(em->inv_area, vm->inv_area, (size_t)em->vertices * sizeof em->inv_area[0]);
    TEST_ASSERT_EQUAL_MEMORY(em->face, vm->face, (size_t)em->faces * sizeof em->face[0]);
    same_edges(em->edge, vm->edge, em->edges);
    TEST_ASSERT_EQUAL(em->tether == NULL, vm->tether == NULL);
    if (em->tether) TEST_ASSERT_EQUAL_MEMORY(em->tether, vm->tether, (size_t)em->vertices * sizeof em->tether[0]);
    TEST_ASSERT_EQUAL_MEMORY(em->drive, vm->drive, (size_t)em->vertices * sizeof(float));
    TEST_ASSERT_EQUAL_MEMORY(e->vertices, v->vertices, (size_t)em->vertices * sizeof e->vertices[0]);
    TEST_ASSERT_EQUAL_MEMORY(&e->material, &v->material, sizeof e->material);
    TEST_ASSERT_EQUAL_INT(e->collider_count, v->collider_count);
    TEST_ASSERT_EQUAL(e->colliders == NULL, v->colliders == NULL);
    same_colliders(e->colliders, v->colliders, e->collider_count);
    TEST_ASSERT_EQUAL_INT(e->render_vertices, v->render_vertices);
    TEST_ASSERT_EQUAL_MEMORY(&e->linear_inertia, &v->linear_inertia, sizeof(float));
    TEST_ASSERT_EQUAL_MEMORY(&e->angular_inertia, &v->angular_inertia, sizeof(float));
    TEST_ASSERT_EQUAL_MEMORY(&e->downward_follow, &v->downward_follow, sizeof(float));
}

/* shift 4 leaves the blob only 4-byte aligned, the least the format promises to work with. */
static void check_view(size_t shift) {
    uint32_t size = 0;
    uint8_t *data = read_file(GARMENT_FIXTURE_BLOB, &size, shift);
    const GarmentSet *set = garment_asset_view(data + shift, size);
    TEST_ASSERT_EQUAL_STRING(expected_set.id, set->id);
    TEST_ASSERT_EQUAL_INT(expected_set.piece_count, set->piece_count);
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_view_reads_every_field_the_writer_wrote);
    RUN_TEST(test_view_needs_only_four_byte_alignment);
    RUN_TEST(test_outfit_without_cloth_views_as_null);
    return UNITY_END();
}
