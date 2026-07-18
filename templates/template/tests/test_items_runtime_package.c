#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "features/items/items.h"
#include "hash/nt_hash.h"

#ifndef ITEMS_GAME_HAS_WEAPON
#error "runtime package did not generate its weapon capability API"
#endif

static uint8_t *s_fixture;
static uint32_t s_fixture_size;

static uint32_t read_le32(const uint8_t *src) {
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8U) |
           ((uint32_t)src[2] << 16U) | ((uint32_t)src[3] << 24U);
}

static void write_le32(uint8_t *dst, uint32_t value) {
    for (uint32_t index = 0; index < 4U; ++index) {
        dst[index] = (uint8_t)(value >> (index * 8U));
    }
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
    copy[item_section + 40U] = 4U;
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_LAYOUT, error);

    memcpy(copy, s_fixture, s_fixture_size);
    write_le64(copy + item_section + 48U, UINT64_MAX);
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_LAYOUT, error);

    memcpy(copy, s_fixture, s_fixture_size);
    uint32_t kind_offset = find_bytes(copy, s_fixture_size, "currency");
    TEST_ASSERT_NOT_EQUAL(UINT32_MAX, kind_offset);
    copy[kind_offset] = 0xFFU;
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_LAYOUT, error);

    memcpy(copy, s_fixture, s_fixture_size);
    memcpy(copy + kind_offset, "material", 8U);
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

void test_cost_spans_reject_unique_resources_and_i64_overflow(void) {
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_OK;
    uint8_t *copy = (uint8_t *)malloc(s_fixture_size);
    TEST_ASSERT_NOT_NULL(copy);
    const uint32_t item_section = read_le32(s_fixture + 52U);
    const uint32_t level_section = read_le32(s_fixture + 76U);
    const uint32_t cost_section = read_le32(s_fixture + 100U);

    memcpy(copy, s_fixture, s_fixture_size);
    write_le32(copy + cost_section, 1U);
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_LAYOUT, error);
    TEST_ASSERT_FALSE(items_catalog_is_bound());

    memcpy(copy, s_fixture, s_fixture_size);
    const uint32_t sword_item = item_section + 56U;
    const uint32_t sword_level_2 = level_section + 32U;
    write_le32(copy + sword_item + 36U, 2U);
    write_le32(copy + sword_level_2 + 16U, 2U);
    write_le32(copy + sword_level_2 + 20U, 0U);
    write_le64(copy + cost_section + 8U, INT64_MAX);
    write_le64(copy + cost_section + 16U + 8U, 1U);
    resign(copy, s_fixture_size);
    TEST_ASSERT_FALSE(items_catalog_try_bind(copy, s_fixture_size, &error));
    TEST_ASSERT_EQUAL(ITEMS_CATALOG_BIND_BAD_LAYOUT, error);
    TEST_ASSERT_FALSE(items_catalog_is_bound());
    free(copy);
}

void test_bound_catalog_exposes_typed_base_api(void) {
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_BAD_HEADER;
    TEST_ASSERT_TRUE(items_catalog_try_bind(s_fixture, s_fixture_size, &error));

    TEST_ASSERT_EQUAL_UINT64(nt_hash64_str("game.gold").value, ITEM_GAME_GOLD.value);
    TEST_ASSERT_EQUAL_UINT64(nt_hash64_str("game.sword").value, ITEM_GAME_SWORD.value);
    TEST_ASSERT_TRUE(items_exists(ITEM_GAME_GOLD));
    TEST_ASSERT_FALSE(items_exists((item_id_t){UINT64_C(0)}));
    TEST_ASSERT_FALSE(items_try_get(ITEM_GAME_GOLD, NULL));

    item_def_ref_t sword = items_get(ITEM_GAME_SWORD);
    item_def_ref_t gold = items_get(ITEM_GAME_GOLD);
    item_def_ref_t string_ref = {UINT32_MAX};
    TEST_ASSERT_TRUE(items_try_get_string("game.sword", &string_ref));
    TEST_ASSERT_EQUAL_UINT32(sword._index, string_ref._index);
    TEST_ASSERT_FALSE(items_try_get_string("game.missing", &string_ref));
    TEST_ASSERT_FALSE(items_try_get_string(NULL, &string_ref));

    item_core_t core = items_core(sword);
    TEST_ASSERT_EQUAL_UINT64(ITEM_GAME_SWORD.value, core.id.value);
    TEST_ASSERT_EQUAL_INT64(1, core.stack);
    TEST_ASSERT_TRUE(items_has_currency(gold));
    TEST_ASSERT_FALSE(items_has_currency(sword));
    TEST_ASSERT_EQUAL_INT64(100, items_currency_cap(gold));

    item_transition_t acquire = items_acquire_transition(sword);
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_COST, acquire.kind);
    TEST_ASSERT_EQUAL_UINT32(1, items_cost_count(acquire.cost));
    item_cost_entry_t cost = items_cost_at(acquire.cost, 0);
    TEST_ASSERT_EQUAL_UINT64(ITEM_GAME_GOLD.value, cost.item.value);
    TEST_ASSERT_EQUAL_INT64(100, cost.count);
    TEST_ASSERT_EQUAL(
        ITEM_TRANSITION_FREE,
        items_acquire_transition(items_get(ITEM_GAME_GOLD)).kind);
}

void test_bound_catalog_exposes_typed_weapon_levels(void) {
    items_catalog_bind_error_t error = ITEMS_CATALOG_BIND_BAD_HEADER;
    TEST_ASSERT_TRUE(items_catalog_try_bind(s_fixture, s_fixture_size, &error));
    item_def_ref_t sword = items_get(ITEM_GAME_SWORD);
    item_def_ref_t invalid = {UINT32_MAX};

    TEST_ASSERT_TRUE(items_is_weapon(sword));
    TEST_ASSERT_FALSE(items_is_weapon(items_get(ITEM_GAME_GOLD)));
    TEST_ASSERT_FALSE(items_is_weapon(invalid));
    TEST_ASSERT_EQUAL_UINT32(0, items_weapon_level_count(invalid));
    TEST_ASSERT_FALSE(items_weapon_level_exists(invalid, 1));
    TEST_ASSERT_EQUAL_UINT32(3, items_weapon_level_count(sword));
    TEST_ASSERT_TRUE(items_weapon_level_exists(sword, 1));
    TEST_ASSERT_FALSE(items_weapon_level_exists(sword, 4));

    item_weapon_level_t level1 = items_weapon_level(sword, 1);
    item_weapon_level_t level2 = items_weapon_level(sword, 2);
    item_weapon_level_t level3 = items_weapon_level(sword, 3);
    TEST_ASSERT_EQUAL_INT64(10, level1.attack);
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_UNAVAILABLE, level1.cost_to_reach.kind);
    TEST_ASSERT_EQUAL_INT64(15, level2.attack);
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_COST, level2.cost_to_reach.kind);
    TEST_ASSERT_EQUAL_UINT32(1, items_cost_count(level2.cost_to_reach.cost));
    item_cost_entry_t level_cost = items_cost_at(level2.cost_to_reach.cost, 0);
    TEST_ASSERT_EQUAL_UINT64(ITEM_GAME_GOLD.value, level_cost.item.value);
    TEST_ASSERT_EQUAL_INT64(100, level_cost.count);
    TEST_ASSERT_EQUAL_INT64(21, level3.attack);
    TEST_ASSERT_EQUAL(ITEM_TRANSITION_FREE, level3.cost_to_reach.kind);
}

int main(void) {
    load_fixture();
    UNITY_BEGIN();
    RUN_TEST(test_bind_copies_valid_package_and_forbids_rebind);
    RUN_TEST(test_failure_never_publishes_catalog);
    RUN_TEST(test_resigned_identity_utf8_range_and_size_corruption_are_rejected);
    RUN_TEST(test_cost_spans_reject_unique_resources_and_i64_overflow);
    RUN_TEST(test_bound_catalog_exposes_typed_base_api);
    RUN_TEST(test_bound_catalog_exposes_typed_weapon_levels);
    int result = UNITY_END();
    free(s_fixture);
    return result;
}
