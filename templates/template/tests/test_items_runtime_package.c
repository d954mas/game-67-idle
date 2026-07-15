#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "features/items/items.h"
#include "hash/nt_hash.h"


static uint8_t *s_fixture;
static uint32_t s_fixture_size;

static uint32_t read_le32(const uint8_t *src) {
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8U) |
           ((uint32_t)src[2] << 16U) | ((uint32_t)src[3] << 24U);
}

static void write_le64(uint8_t *dst, uint64_t value) {
    for (uint32_t index = 0; index < 8U; ++index) {
        dst[index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint32_t find_bytes(const uint8_t *bytes, uint32_t size, const char *needle) {
    uint32_t count = (uint32_t)strlen(needle);
    for (uint32_t index = 0; index + count <= size; ++index) {
        if (memcmp(bytes + index, needle, count) == 0) {
            return index;
        }
    }
    return UINT32_MAX;
}

static void resign(uint8_t *bytes, uint32_t size) {
    memset(bytes + 32U, 0, 8U);
    write_le64(bytes + 32U, nt_hash64(bytes, size).value);
}

void setUp(void) { items_catalog_shutdown(); }
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

void test_bind_copies_valid_package_and_forbids_rebind(void) {
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_BAD_HEADER;
    uint8_t *input = (uint8_t *)malloc(s_fixture_size);
    TEST_ASSERT_NOT_NULL(input);
    memcpy(input, s_fixture, s_fixture_size);
    TEST_ASSERT_TRUE(items_catalog_try_bind(input, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_OK, error);
    TEST_ASSERT_TRUE(items_catalog_is_bound());
    TEST_ASSERT_EQUAL_UINT32(2, items_catalog_item_count());
    TEST_ASSERT_EQUAL_UINT64(ITEMS_CATALOG_SCHEMA_ABI, items_catalog_schema_abi());
    TEST_ASSERT_NOT_EQUAL(0, items_catalog_content_fingerprint());

    memset(input, 0, s_fixture_size);
    TEST_ASSERT_EQUAL_UINT32(2, items_catalog_item_count());
    TEST_ASSERT_FALSE(items_catalog_try_bind(input, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_ALREADY_BOUND, error);
    free(input);

    items_catalog_shutdown();
    TEST_ASSERT_TRUE(items_catalog_try_bind(s_fixture, s_fixture_size, &error));
    TEST_ASSERT_EQUAL_UINT32(2, items_catalog_item_count());
}

void test_failure_never_publishes_catalog(void) {
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;
    uint8_t *copy = (uint8_t *)malloc(s_fixture_size);
    TEST_ASSERT_NOT_NULL(copy);

    memcpy(copy, s_fixture, s_fixture_size);
    copy[s_fixture_size - 1U] ^= 1U;
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_CONTENT_MISMATCH, error);
    TEST_ASSERT_FALSE(items_catalog_is_bound());

    memcpy(copy, s_fixture, s_fixture_size);
    write_le64(copy + 24U, ITEMS_CATALOG_SCHEMA_ABI ^ UINT64_C(1));
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_ABI_MISMATCH, error);
    TEST_ASSERT_FALSE(items_catalog_is_bound());

    memcpy(copy, s_fixture, s_fixture_size);
    copy[40U] = 0U;
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_LAYOUT, error);
    TEST_ASSERT_FALSE(items_catalog_is_bound());

    TEST_ASSERT_FALSE(items_catalog_try_bind(s_fixture, s_fixture_size - 1U, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_HEADER, error);
    TEST_ASSERT_FALSE(items_catalog_is_bound());
    free(copy);
}

void test_resigned_identity_utf8_range_and_size_corruption_are_rejected(void) {
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;
    uint8_t *copy = (uint8_t *)malloc(s_fixture_size);
    TEST_ASSERT_NOT_NULL(copy);

    memcpy(copy, s_fixture, s_fixture_size);
    uint32_t id_offset = find_bytes(copy, s_fixture_size, "game.gold");
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, id_offset);
    memcpy(copy + id_offset, "evil.gold", 9U);
    uint32_t item_section = read_le32(copy + 52U);
    write_le64(copy + item_section, nt_hash64("evil.gold", 9U).value);
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_ABI_MISMATCH, error);

    memcpy(copy, s_fixture, s_fixture_size);
    uint32_t kind_offset = find_bytes(copy, s_fixture_size, "currency");
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, kind_offset);
    copy[kind_offset] = 0xFFU;
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_LAYOUT, error);

    memcpy(copy, s_fixture, s_fixture_size);
    uint32_t value_section = read_le32(copy + 88U);
    write_le64(copy + value_section + 16U, UINT64_C(1000001));
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_LAYOUT, error);

    TEST_ASSERT_FALSE(items_catalog_try_bind(
        s_fixture, UINT32_C(64) * UINT32_C(1024) * UINT32_C(1024) + 1U, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_LAYOUT, error);
    TEST_ASSERT_FALSE(items_catalog_is_bound());
    free(copy);
}

int main(void) {
    load_fixture();
    UNITY_BEGIN();
    RUN_TEST(test_bind_copies_valid_package_and_forbids_rebind);
    RUN_TEST(test_failure_never_publishes_catalog);
    RUN_TEST(test_resigned_identity_utf8_range_and_size_corruption_are_rejected);
    int result = UNITY_END();
    free(s_fixture);
    return result;
}
