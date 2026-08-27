#include "unity.h"

#include "cJSON.h"
#include "game_save_writer.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_writer_matches_cjson_order_escaping_and_numbers(void) {
    cJSON *root = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(root);
    cJSON_AddStringToObject(root, "quote", "a\\b\n\"c\x01");
    cJSON_AddNumberToObject(root, "small", 42.0);
    cJSON_AddNumberToObject(root, "large", 2147483648.0);
    cJSON_AddNumberToObject(root, "fraction", 0.10000000149011612);
    cJSON_AddBoolToObject(root, "ok", true);
    char *expected = cJSON_PrintUnformatted(root);
    TEST_ASSERT_NOT_NULL(expected);

    char text[512];
    game_save_writer_t writer;
    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
    TEST_ASSERT_TRUE(game_save_writer_key(&writer, "quote"));
    TEST_ASSERT_TRUE(game_save_writer_string(&writer, "a\\b\n\"c\x01"));
    TEST_ASSERT_TRUE(game_save_writer_key(&writer, "small"));
    TEST_ASSERT_TRUE(game_save_writer_number(&writer, 42.0));
    TEST_ASSERT_TRUE(game_save_writer_key(&writer, "large"));
    TEST_ASSERT_TRUE(game_save_writer_number(&writer, 2147483648.0));
    TEST_ASSERT_TRUE(game_save_writer_key(&writer, "fraction"));
    TEST_ASSERT_TRUE(game_save_writer_number(&writer, 0.10000000149011612));
    TEST_ASSERT_TRUE(game_save_writer_key(&writer, "ok"));
    TEST_ASSERT_TRUE(game_save_writer_bool(&writer, true));
    TEST_ASSERT_TRUE(game_save_writer_end_object(&writer));
    TEST_ASSERT_TRUE(game_save_writer_ok(&writer));
    TEST_ASSERT_EQUAL_STRING(expected, text);

    cJSON_free(expected);
    cJSON_Delete(root);
}

void test_writer_matches_cjson_edge_numbers_and_utf8(void) {
    static const char utf8[] = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 \xF0\x9F\x98\x80\n\x01";
    static const double values[] = {
        1.0000000000000002, -0.0, NAN, INFINITY, -INFINITY,
        DBL_MIN, DBL_MAX, -DBL_MAX, 2147483648.0, -2147483649.0,
    };
    cJSON *root = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(root);
    cJSON_AddStringToObject(root, "utf8", utf8);
    for (unsigned i = 0; i < sizeof values / sizeof values[0]; i++) {
        char key[8];
        (void)snprintf(key, sizeof key, "n%u", i);
        TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(root, key, values[i]));
    }
    char *expected = cJSON_PrintUnformatted(root);
    TEST_ASSERT_NOT_NULL(expected);

    char text[1024];
    game_save_writer_t writer;
    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
    TEST_ASSERT_TRUE(game_save_writer_key(&writer, "utf8"));
    TEST_ASSERT_TRUE(game_save_writer_string(&writer, utf8));
    for (unsigned i = 0; i < sizeof values / sizeof values[0]; i++) {
        char key[8];
        (void)snprintf(key, sizeof key, "n%u", i);
        TEST_ASSERT_TRUE(game_save_writer_key(&writer, key));
        TEST_ASSERT_TRUE(game_save_writer_number(&writer, values[i]));
    }
    TEST_ASSERT_TRUE(game_save_writer_end_object(&writer));
    TEST_ASSERT_EQUAL_STRING(expected, text);
    cJSON_free(expected);
    cJSON_Delete(root);
}

void test_writer_rejects_overflow_without_writing_partial_value(void) {
    char text[8];
    game_save_writer_t writer;
    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
    TEST_ASSERT_TRUE(game_save_writer_key(&writer, "x"));
    TEST_ASSERT_FALSE(game_save_writer_string(&writer, "too-long"));
    TEST_ASSERT_FALSE(game_save_writer_ok(&writer));
}

void test_writer_accepts_exact_capacity_and_fails_closed_at_depth_limit(void) {
    char exact[6];
    game_save_writer_t writer;
    game_save_writer_init(&writer, exact, sizeof exact);
    TEST_ASSERT_TRUE(game_save_writer_raw_value(&writer, "12345", 5U));
    TEST_ASSERT_EQUAL_STRING("12345", exact);

    char nested[128];
    game_save_writer_init(&writer, nested, sizeof nested);
    for (int i = 0; i < 32; i++) {
        TEST_ASSERT_TRUE(game_save_writer_begin_array(&writer));
    }
    const size_t complete_prefix = game_save_writer_size(&writer);
    TEST_ASSERT_FALSE(game_save_writer_begin_array(&writer));
    TEST_ASSERT_FALSE(game_save_writer_ok(&writer));
    TEST_ASSERT_EQUAL_UINT(complete_prefix, game_save_writer_size(&writer));
}

void test_writer_rejects_invalid_json_scope_transitions(void) {
    char text[128];
    game_save_writer_t writer;

    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_array(&writer));
    TEST_ASSERT_FALSE(game_save_writer_key(&writer, "nope"));
    TEST_ASSERT_FALSE(game_save_writer_ok(&writer));

    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
    TEST_ASSERT_FALSE(game_save_writer_number(&writer, 1.0));
    TEST_ASSERT_FALSE(game_save_writer_ok(&writer));

    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
    TEST_ASSERT_FALSE(game_save_writer_end_array(&writer));
    TEST_ASSERT_FALSE(game_save_writer_ok(&writer));
}

void test_writer_rejects_incomplete_or_invalid_raw_root_value(void) {
    char text[128];
    game_save_writer_t writer;

    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
    TEST_ASSERT_FALSE(game_save_writer_complete(&writer));
    TEST_ASSERT_TRUE(game_save_writer_end_object(&writer));
    TEST_ASSERT_TRUE(game_save_writer_complete(&writer));
    TEST_ASSERT_FALSE(game_save_writer_raw_value(&writer, "null", 4U));

    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_FALSE(game_save_writer_raw_value(&writer, NULL, 1U));
    TEST_ASSERT_FALSE(game_save_writer_ok(&writer));

    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_FALSE(game_save_writer_raw_value(&writer, "", 0U));
    TEST_ASSERT_FALSE(game_save_writer_ok(&writer));
}

void test_writer_rejects_null_object_key_or_string_value(void) {
    char text[128];
    game_save_writer_t writer;

    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
    TEST_ASSERT_FALSE(game_save_writer_key(&writer, NULL));
    TEST_ASSERT_FALSE(game_save_writer_ok(&writer));

    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_array(&writer));
    TEST_ASSERT_FALSE(game_save_writer_string(&writer, NULL));
    TEST_ASSERT_FALSE(game_save_writer_ok(&writer));
}

static unsigned s_cjson_allocations;
static void *counted_cjson_malloc(size_t size) { (void)size; s_cjson_allocations++; return NULL; }
static void counted_cjson_free(void *ptr) { (void)ptr; }

void test_writer_performs_no_cjson_allocation(void) {
    cJSON_Hooks hooks = { counted_cjson_malloc, counted_cjson_free };
    char text[128];
    game_save_writer_t writer;
    s_cjson_allocations = 0;
    cJSON_InitHooks(&hooks);
    game_save_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
    TEST_ASSERT_TRUE(game_save_writer_key(&writer, "state"));
    TEST_ASSERT_TRUE(game_save_writer_string(&writer, "ready"));
    TEST_ASSERT_TRUE(game_save_writer_end_object(&writer));
    TEST_ASSERT_EQUAL_UINT(0U, s_cjson_allocations);
    cJSON_InitHooks(NULL);
}

void test_writer_10000_snapshots_performs_no_cjson_allocation(void) {
    cJSON_Hooks hooks = { counted_cjson_malloc, counted_cjson_free };
    char text[128];
    s_cjson_allocations = 0;
    cJSON_InitHooks(&hooks);
    for (int i = 0; i < 10000; i++) {
        game_save_writer_t writer;
        game_save_writer_init(&writer, text, sizeof text);
        TEST_ASSERT_TRUE(game_save_writer_begin_object(&writer));
        TEST_ASSERT_TRUE(game_save_writer_key(&writer, "tick"));
        TEST_ASSERT_TRUE(game_save_writer_number(&writer, (double)i));
        TEST_ASSERT_TRUE(game_save_writer_key(&writer, "ready"));
        TEST_ASSERT_TRUE(game_save_writer_bool(&writer, true));
        TEST_ASSERT_TRUE(game_save_writer_end_object(&writer));
        TEST_ASSERT_TRUE(game_save_writer_ok(&writer));
    }
    TEST_ASSERT_EQUAL_UINT(0U, s_cjson_allocations);
    cJSON_InitHooks(NULL);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_writer_matches_cjson_order_escaping_and_numbers);
    RUN_TEST(test_writer_matches_cjson_edge_numbers_and_utf8);
    RUN_TEST(test_writer_rejects_overflow_without_writing_partial_value);
    RUN_TEST(test_writer_accepts_exact_capacity_and_fails_closed_at_depth_limit);
    RUN_TEST(test_writer_rejects_invalid_json_scope_transitions);
    RUN_TEST(test_writer_rejects_incomplete_or_invalid_raw_root_value);
    RUN_TEST(test_writer_rejects_null_object_key_or_string_value);
    RUN_TEST(test_writer_performs_no_cjson_allocation);
    RUN_TEST(test_writer_10000_snapshots_performs_no_cjson_allocation);
    return UNITY_END();
}
