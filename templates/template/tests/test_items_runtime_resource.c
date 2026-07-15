#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "features/items/items.h"
#include "nt_pack_format.h"
#include "resource/nt_resource.h"

typedef enum fake_resource_mode_t {
    FAKE_RESOURCE_MISSING = 0,
    FAKE_RESOURCE_NOT_READY,
    FAKE_RESOURCE_WRONG_TYPE,
    FAKE_RESOURCE_WRONG_TYPE_NOT_READY,
    FAKE_RESOURCE_READY_BLOB,
    FAKE_RESOURCE_EMPTY_BLOB,
} fake_resource_mode_t;

static fake_resource_mode_t s_mode;
static uint8_t *s_fixture;
static uint32_t s_fixture_size;
static const uint8_t *s_resource_bytes;

nt_resource_t nt_resource_find(nt_hash64_t resource_id) {
    return resource_id.value != 0 && s_mode != FAKE_RESOURCE_MISSING
        ? (nt_resource_t){1}
        : NT_RESOURCE_INVALID;
}

bool nt_resource_is_ready(nt_resource_t resource) {
    return resource.id != 0 && s_mode != FAKE_RESOURCE_NOT_READY &&
           s_mode != FAKE_RESOURCE_WRONG_TYPE_NOT_READY;
}

uint8_t nt_resource_get_asset_type(nt_resource_t resource) {
    if (resource.id == 0) return 0;
    return s_mode == FAKE_RESOURCE_WRONG_TYPE || s_mode == FAKE_RESOURCE_WRONG_TYPE_NOT_READY
        ? NT_ASSET_TEXTURE
        : NT_ASSET_BLOB;
}

const uint8_t *nt_resource_get_blob(nt_resource_t resource, uint32_t *out_size) {
    if (resource.id == 0 || s_mode == FAKE_RESOURCE_EMPTY_BLOB) {
        if (out_size != NULL) *out_size = 0;
        return NULL;
    }
    if (out_size != NULL) *out_size = s_fixture_size;
    return s_resource_bytes;
}

void setUp(void) {
    items_catalog_shutdown();
    s_mode = FAKE_RESOURCE_MISSING;
    s_resource_bytes = s_fixture;
}

void tearDown(void) { items_catalog_shutdown(); }

static void load_fixture(void) {
    FILE *stream = fopen(ITEMS_RUNTIME_PACKAGE_PATH, "rb");
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_INT(0, fseek(stream, 0, SEEK_END));
    long size = ftell(stream);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(stream, 0, SEEK_SET));
    s_fixture_size = (uint32_t)size;
    s_fixture = (uint8_t *)malloc(s_fixture_size);
    TEST_ASSERT_NOT_NULL(s_fixture);
    TEST_ASSERT_EQUAL_UINT32(s_fixture_size, (uint32_t)fread(s_fixture, 1, s_fixture_size, stream));
    TEST_ASSERT_EQUAL_INT(0, fclose(stream));
}

static uint64_t asset_id(void) { return UINT64_C(0x1234); }

void test_resource_failures_are_specific_and_publish_nothing(void) {
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;

    TEST_ASSERT_FALSE(items_catalog_try_bind_resource(0, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_RESOURCE_MISSING, error);

    TEST_ASSERT_FALSE(items_catalog_try_bind_resource(asset_id(), &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_RESOURCE_MISSING, error);

    s_mode = FAKE_RESOURCE_NOT_READY;
    TEST_ASSERT_FALSE(items_catalog_try_bind_resource(asset_id(), &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_RESOURCE_NOT_READY, error);

    s_mode = FAKE_RESOURCE_WRONG_TYPE;
    TEST_ASSERT_FALSE(items_catalog_try_bind_resource(asset_id(), &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_RESOURCE_WRONG_TYPE, error);

    s_mode = FAKE_RESOURCE_WRONG_TYPE_NOT_READY;
    TEST_ASSERT_FALSE(items_catalog_try_bind_resource(asset_id(), &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_RESOURCE_WRONG_TYPE, error);

    s_mode = FAKE_RESOURCE_EMPTY_BLOB;
    TEST_ASSERT_FALSE(items_catalog_try_bind_resource(asset_id(), &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_HEADER, error);
    TEST_ASSERT_FALSE(items_catalog_is_bound());

    uint8_t *corrupt = (uint8_t *)malloc(s_fixture_size);
    TEST_ASSERT_NOT_NULL(corrupt);
    memcpy(corrupt, s_fixture, s_fixture_size);
    corrupt[s_fixture_size - 1U] ^= 1U;
    s_resource_bytes = corrupt;
    s_mode = FAKE_RESOURCE_READY_BLOB;
    TEST_ASSERT_FALSE(items_catalog_try_bind_resource(asset_id(), &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_CONTENT_MISMATCH, error);
    TEST_ASSERT_FALSE(items_catalog_is_bound());
    free(corrupt);
}

void test_ready_blob_is_bound_from_an_owned_copy(void) {
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_BAD_HEADER;
    uint8_t *resource_bytes = (uint8_t *)malloc(s_fixture_size);
    TEST_ASSERT_NOT_NULL(resource_bytes);
    memcpy(resource_bytes, s_fixture, s_fixture_size);
    s_resource_bytes = resource_bytes;
    s_mode = FAKE_RESOURCE_READY_BLOB;

    TEST_ASSERT_TRUE(items_catalog_try_bind_resource(asset_id(), &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_OK, error);
    memset(resource_bytes, 0, s_fixture_size);
    free(resource_bytes);

    TEST_ASSERT_TRUE(items_catalog_is_bound());
    TEST_ASSERT_EQUAL_UINT32(ITEMS_CATALOG_ITEM_COUNT, items_catalog_item_count());
}

int main(void) {
    load_fixture();
    UNITY_BEGIN();
    RUN_TEST(test_resource_failures_are_specific_and_publish_nothing);
    RUN_TEST(test_ready_blob_is_bound_from_an_owned_copy);
    int result = UNITY_END();
    free(s_fixture);
    return result;
}
