#include "unity.h"

#include "game_state_json.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/* Unity is built with UNITY_EXCLUDE_FLOAT/UNITY_EXCLUDE_DOUBLE (engine deps/unity
   CMakeLists) -- no TEST_ASSERT_EQUAL_FLOAT here; compare with fabsf + epsilon. */
#define GSJ_TEST_EPS 0.0001f

static const char *const kEnumNames[] = {"idle", "walk", "run"};
static const int kEnumCount = 3;

void setUp(void) {}
void tearDown(void) {}

/* ---- gsj_set_error / gsj_copy_text ---- */

void test_set_error_truncates_and_is_safe(void) {
    char buf[8];
    gsj_set_error(buf, (int)sizeof(buf), "this message is far too long for buf");
    TEST_ASSERT_TRUE(strlen(buf) < sizeof(buf));
    gsj_set_error(NULL, 10, "no crash with NULL error");
    gsj_set_error(buf, 0, "no-op with cap<=0");
}

void test_copy_text_bounds(void) {
    char dst[8];
    TEST_ASSERT_TRUE(gsj_copy_text(dst, sizeof(dst), "abc"));
    TEST_ASSERT_EQUAL_STRING("abc", dst);
    TEST_ASSERT_FALSE(gsj_copy_text(dst, sizeof(dst), "way-too-long"));
    TEST_ASSERT_FALSE(gsj_copy_text(NULL, sizeof(dst), "abc"));
    TEST_ASSERT_FALSE(gsj_copy_text(dst, 0, "abc"));
    TEST_ASSERT_FALSE(gsj_copy_text(dst, sizeof(dst), NULL));
}

/* ---- gsj_object_item ---- */

void test_object_item_lookup(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "a", 1);
    TEST_ASSERT_NOT_NULL(gsj_object_item(root, "a"));
    TEST_ASSERT_NULL(gsj_object_item(root, "missing"));

    cJSON *arr = cJSON_CreateArray();
    TEST_ASSERT_NULL(gsj_object_item(arr, "a"));

    cJSON_Delete(root);
    cJSON_Delete(arr);
}

/* ---- gsj_read_bool ---- */

void test_read_bool_absent_key(void) {
    cJSON *root = cJSON_CreateObject();
    bool out = true;
    TEST_ASSERT_TRUE(gsj_read_bool(root, "flag", &out, NULL, 0));
    TEST_ASSERT_TRUE(out); /* untouched */
    cJSON_Delete(root);
}

void test_read_bool_wrong_type(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "flag", "yes");
    bool out = false;
    char err[64] = {0};
    TEST_ASSERT_FALSE(gsj_read_bool(root, "flag", &out, err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
    cJSON_Delete(root);
}

void test_read_bool_valid(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "flag", true);
    bool out = false;
    TEST_ASSERT_TRUE(gsj_read_bool(root, "flag", &out, NULL, 0));
    TEST_ASSERT_TRUE(out);
    cJSON_Delete(root);
}

/* ---- gsj_read_int_range ---- */

void test_read_int_range_absent_key(void) {
    cJSON *root = cJSON_CreateObject();
    int out = -12345;
    TEST_ASSERT_TRUE(gsj_read_int_range(root, "n", 0, 100, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT(-12345, out);
    cJSON_Delete(root);
}

void test_read_int_range_wrong_type(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "n", "5");
    int out = 0;
    char err[64] = {0};
    TEST_ASSERT_FALSE(gsj_read_int_range(root, "n", 0, 10, &out, err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
    cJSON_Delete(root);
}

void test_read_int_range_out_of_range(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "n", 500);
    int out = 0;
    TEST_ASSERT_FALSE(gsj_read_int_range(root, "n", 0, 100, &out, NULL, 0));

    cJSON *root2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(root2, "n", 5.5);
    TEST_ASSERT_FALSE(gsj_read_int_range(root2, "n", 0, 100, &out, NULL, 0));

    cJSON_Delete(root);
    cJSON_Delete(root2);
}

void test_read_int_range_valid(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "n", 42);
    int out = 0;
    TEST_ASSERT_TRUE(gsj_read_int_range(root, "n", 0, 100, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT(42, out);
    cJSON_Delete(root);
}

/* ---- gsj_read_float_range ---- */

void test_read_float_range_absent_key(void) {
    cJSON *root = cJSON_CreateObject();
    float out = -999.0f;
    TEST_ASSERT_TRUE(gsj_read_float_range(root, "f", 0.0f, 10.0f, &out, NULL, 0));
    TEST_ASSERT_TRUE(fabsf(out - (-999.0f)) < GSJ_TEST_EPS);
    cJSON_Delete(root);
}

void test_read_float_range_wrong_type(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "f", "x");
    float out = 0.0f;
    char err[64] = {0};
    TEST_ASSERT_FALSE(gsj_read_float_range(root, "f", 0.0f, 10.0f, &out, err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
    cJSON_Delete(root);
}

void test_read_float_range_out_of_range(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "f", 500.0);
    float out = 0.0f;
    TEST_ASSERT_FALSE(gsj_read_float_range(root, "f", 0.0f, 100.0f, &out, NULL, 0));
    cJSON_Delete(root);
}

void test_read_float_range_valid(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "f", 3.5);
    float out = 0.0f;
    TEST_ASSERT_TRUE(gsj_read_float_range(root, "f", 0.0f, 10.0f, &out, NULL, 0));
    TEST_ASSERT_TRUE(fabsf(out - 3.5f) < GSJ_TEST_EPS);
    cJSON_Delete(root);
}

/* ---- gsj_read_string ---- */

void test_read_string_absent_key(void) {
    cJSON *root = cJSON_CreateObject();
    char out[16] = "SENTINEL";
    TEST_ASSERT_TRUE(gsj_read_string(root, "s", out, sizeof(out), NULL, 0));
    TEST_ASSERT_EQUAL_STRING("SENTINEL", out);
    cJSON_Delete(root);
}

void test_read_string_wrong_type(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "s", 5);
    char out[16] = {0};
    char err[64] = {0};
    TEST_ASSERT_FALSE(gsj_read_string(root, "s", out, sizeof(out), err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
    cJSON_Delete(root);
}

void test_read_string_out_of_range(void) {
    /* "out of range" for a bounded string buffer = does not fit dst_cap. */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "s", "abcdefghij");
    char out[4] = {0};
    TEST_ASSERT_FALSE(gsj_read_string(root, "s", out, sizeof(out), NULL, 0));
    cJSON_Delete(root);
}

void test_read_string_valid(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "s", "hi");
    char out[16] = {0};
    TEST_ASSERT_TRUE(gsj_read_string(root, "s", out, sizeof(out), NULL, 0));
    TEST_ASSERT_EQUAL_STRING("hi", out);
    cJSON_Delete(root);
}

/* ---- gsj_enum_index / gsj_read_enum ---- */

void test_enum_index_lookup(void) {
    TEST_ASSERT_EQUAL_INT(0, gsj_enum_index("idle", kEnumNames, kEnumCount));
    TEST_ASSERT_EQUAL_INT(1, gsj_enum_index("walk", kEnumNames, kEnumCount));
    TEST_ASSERT_EQUAL_INT(-1, gsj_enum_index("nope", kEnumNames, kEnumCount));
    TEST_ASSERT_EQUAL_INT(-1, gsj_enum_index(NULL, kEnumNames, kEnumCount));
}

void test_read_enum_absent_key(void) {
    cJSON *root = cJSON_CreateObject();
    int out = -77;
    TEST_ASSERT_TRUE(gsj_read_enum(root, "e", kEnumNames, kEnumCount, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT(-77, out);
    cJSON_Delete(root);
}

void test_read_enum_wrong_type(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "e", true);
    int out = 0;
    char err[64] = {0};
    TEST_ASSERT_FALSE(gsj_read_enum(root, "e", kEnumNames, kEnumCount, &out, err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
    cJSON_Delete(root);
}

void test_read_enum_out_of_range(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "e", "fly"); /* unknown name -> enum_index -1 */
    int out = 0;
    TEST_ASSERT_FALSE(gsj_read_enum(root, "e", kEnumNames, kEnumCount, &out, NULL, 0));

    cJSON *root2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(root2, "e", 5); /* legacy index out of bounds */
    TEST_ASSERT_FALSE(gsj_read_enum(root2, "e", kEnumNames, kEnumCount, &out, NULL, 0));

    cJSON_Delete(root);
    cJSON_Delete(root2);
}

void test_read_enum_valid_by_name(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "e", "walk");
    int out = 0;
    TEST_ASSERT_TRUE(gsj_read_enum(root, "e", kEnumNames, kEnumCount, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT(1, out);
    cJSON_Delete(root);
}

void test_read_enum_valid_by_legacy_number(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "e", 2);
    int out = 0;
    TEST_ASSERT_TRUE(gsj_read_enum(root, "e", kEnumNames, kEnumCount, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT(2, out);
    cJSON_Delete(root);
}

/* ---- gsj_parse_int_value / gsj_parse_enum_value (single-node parsers) ---- */

void test_parse_int_value(void) {
    cJSON *ok = cJSON_CreateNumber(7);
    int out = 0;
    TEST_ASSERT_TRUE(gsj_parse_int_value(ok, 0, 10, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT(7, out);

    cJSON *wrong_type = cJSON_CreateString("x");
    TEST_ASSERT_FALSE(gsj_parse_int_value(wrong_type, 0, 10, &out, NULL, 0));

    cJSON *out_of_range = cJSON_CreateNumber(999);
    TEST_ASSERT_FALSE(gsj_parse_int_value(out_of_range, 0, 10, &out, NULL, 0));

    cJSON_Delete(ok);
    cJSON_Delete(wrong_type);
    cJSON_Delete(out_of_range);
}

void test_parse_enum_value(void) {
    cJSON *by_name = cJSON_CreateString("run");
    int out = 0;
    TEST_ASSERT_TRUE(gsj_parse_enum_value(by_name, kEnumNames, kEnumCount, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT(2, out);

    cJSON *by_number = cJSON_CreateNumber(0);
    TEST_ASSERT_TRUE(gsj_parse_enum_value(by_number, kEnumNames, kEnumCount, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT(0, out);

    cJSON *wrong_type = cJSON_CreateBool(true);
    TEST_ASSERT_FALSE(gsj_parse_enum_value(wrong_type, kEnumNames, kEnumCount, &out, NULL, 0));

    cJSON *out_of_range = cJSON_CreateString("nope");
    TEST_ASSERT_FALSE(gsj_parse_enum_value(out_of_range, kEnumNames, kEnumCount, &out, NULL, 0));

    cJSON_Delete(by_name);
    cJSON_Delete(by_number);
    cJSON_Delete(wrong_type);
    cJSON_Delete(out_of_range);
}

/* ---- exact u32 numeric wire (all uint32 values fit exactly in JSON double) ---- */

void test_read_u32_exact_bounds(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "id", 4294967295.0);
    uint32_t out = 0;
    TEST_ASSERT_TRUE(gsj_read_u32(root, "id", 0, UINT32_MAX, &out, NULL, 0));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, out);
    cJSON_Delete(root);
}

void test_parse_u32_rejects_ambiguous_values(void) {
    uint32_t out = 7;
    cJSON *fractional = cJSON_CreateNumber(1.5);
    cJSON *negative = cJSON_CreateNumber(-1);
    cJSON *overflow = cJSON_CreateNumber(4294967296.0);
    cJSON *string = cJSON_CreateString("42");

    TEST_ASSERT_FALSE(gsj_parse_u32_value(fractional, 0, UINT32_MAX, &out, NULL, 0));
    TEST_ASSERT_FALSE(gsj_parse_u32_value(negative, 0, UINT32_MAX, &out, NULL, 0));
    TEST_ASSERT_FALSE(gsj_parse_u32_value(overflow, 0, UINT32_MAX, &out, NULL, 0));
    TEST_ASSERT_FALSE(gsj_parse_u32_value(string, 0, UINT32_MAX, &out, NULL, 0));
    TEST_ASSERT_EQUAL_UINT32(7, out);

    cJSON_Delete(fractional);
    cJSON_Delete(negative);
    cJSON_Delete(overflow);
    cJSON_Delete(string);
}

/* ---- gsj_read_i64 / gsj_parse_i64_value: the risky corner ---- */

void test_read_i64_absent_key(void) {
    cJSON *root = cJSON_CreateObject();
    int64_t out = -999;
    TEST_ASSERT_TRUE(gsj_read_i64(root, "c", INT64_MIN, INT64_MAX, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT64(-999, out);
    cJSON_Delete(root);
}

void test_read_i64_wrong_type(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "c", true);
    int64_t out = 0;
    char err[64] = {0};
    TEST_ASSERT_FALSE(gsj_read_i64(root, "c", INT64_MIN, INT64_MAX, &out, err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
    cJSON_Delete(root);
}

void test_read_i64_string_round_trip(void) {
    /* 9e18 > 2^53: exercises the exact case the i64 wire exists for. */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "c", "9000000000000000000");
    int64_t out = 0;
    TEST_ASSERT_TRUE(gsj_read_i64(root, "c", INT64_MIN, INT64_MAX, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT64(9000000000000000000LL, out);
    cJSON_Delete(root);
}

void test_read_i64_negative_string_round_trip(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "c", "-123456789012345");
    int64_t out = 0;
    TEST_ASSERT_TRUE(gsj_read_i64(root, "c", INT64_MIN, INT64_MAX, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT64(-123456789012345LL, out);
    cJSON_Delete(root);
}

void test_read_i64_number_above_2_53_rejected(void) {
    /* Same magnitude as the string test, but sent as a raw JSON number:
       must be rejected -- doubles cannot carry this integer exactly. */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "c", 9000000000000000000.0);
    int64_t out = 0;
    char err[64] = {0};
    TEST_ASSERT_FALSE(gsj_read_i64(root, "c", INT64_MIN, INT64_MAX, &out, err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
    cJSON_Delete(root);
}

void test_read_i64_small_number_accepted(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "c", 42);
    int64_t out = 0;
    TEST_ASSERT_TRUE(gsj_read_i64(root, "c", INT64_MIN, INT64_MAX, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT64(42, out);
    cJSON_Delete(root);
}

void test_read_i64_out_of_custom_range(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "c", "5");
    int64_t out = 0;
    TEST_ASSERT_FALSE(gsj_read_i64(root, "c", 10, 20, &out, NULL, 0));
    cJSON_Delete(root);
}

void test_read_i64_invalid_string_trailing_garbage(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "c", "123abc");
    int64_t out = 0;
    TEST_ASSERT_FALSE(gsj_read_i64(root, "c", INT64_MIN, INT64_MAX, &out, NULL, 0));
    cJSON_Delete(root);
}

void test_read_i64_empty_string_rejected(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "c", "");
    int64_t out = 0;
    TEST_ASSERT_FALSE(gsj_read_i64(root, "c", INT64_MIN, INT64_MAX, &out, NULL, 0));
    cJSON_Delete(root);
}

void test_read_i64_string_overflow_beyond_int64_rejected(void) {
    /* Bigger than INT64_MAX: strtoll must overflow (ERANGE), not wrap. */
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "c", "99999999999999999999999999");
    int64_t out = 0;
    TEST_ASSERT_FALSE(gsj_read_i64(root, "c", INT64_MIN, INT64_MAX, &out, NULL, 0));
    cJSON_Delete(root);
}

void test_parse_i64_value_direct(void) {
    cJSON *ok = cJSON_CreateString("123");
    int64_t out = 0;
    TEST_ASSERT_TRUE(gsj_parse_i64_value(ok, 0, 1000, &out, NULL, 0));
    TEST_ASSERT_EQUAL_INT64(123, out);

    cJSON *wrong_type = cJSON_CreateBool(false);
    TEST_ASSERT_FALSE(gsj_parse_i64_value(wrong_type, 0, 1000, &out, NULL, 0));

    cJSON_Delete(ok);
    cJSON_Delete(wrong_type);
}

/* ---- gsj_add_i64 / gsj_i64_to_string ---- */

void test_i64_to_string(void) {
    char buf[32];
    char *ret = gsj_i64_to_string(1234567890123456789LL, buf, sizeof(buf));
    TEST_ASSERT_TRUE(ret == buf);
    TEST_ASSERT_EQUAL_STRING("1234567890123456789", buf);

    (void)gsj_i64_to_string(INT64_MIN, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("-9223372036854775808", buf);

    (void)gsj_i64_to_string(0, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0", buf);
}

void test_add_i64_stores_as_json_string(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON *added = gsj_add_i64(root, "c", 9000000000000000000LL);
    TEST_ASSERT_NOT_NULL(added);

    const cJSON *stored = cJSON_GetObjectItemCaseSensitive(root, "c");
    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_TRUE(cJSON_IsString(stored));
    TEST_ASSERT_EQUAL_STRING("9000000000000000000", stored->valuestring);

    cJSON_Delete(root);
}

void test_add_i64_then_read_i64_round_trip(void) {
    const int64_t values[] = {
        0, 1, -1, 42, -42,
        9000000000000000000LL, -9000000000000000000LL,
        INT64_MAX, INT64_MIN,
    };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        cJSON *root = cJSON_CreateObject();
        TEST_ASSERT_NOT_NULL(gsj_add_i64(root, "v", values[i]));

        int64_t out = 0;
        char err[64] = {0};
        TEST_ASSERT_TRUE(gsj_read_i64(root, "v", INT64_MIN, INT64_MAX, &out, err, (int)sizeof(err)));
        TEST_ASSERT_EQUAL_INT64(values[i], out);

        cJSON_Delete(root);
    }
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_set_error_truncates_and_is_safe);
    RUN_TEST(test_copy_text_bounds);
    RUN_TEST(test_object_item_lookup);

    RUN_TEST(test_read_bool_absent_key);
    RUN_TEST(test_read_bool_wrong_type);
    RUN_TEST(test_read_bool_valid);

    RUN_TEST(test_read_int_range_absent_key);
    RUN_TEST(test_read_int_range_wrong_type);
    RUN_TEST(test_read_int_range_out_of_range);
    RUN_TEST(test_read_int_range_valid);

    RUN_TEST(test_read_float_range_absent_key);
    RUN_TEST(test_read_float_range_wrong_type);
    RUN_TEST(test_read_float_range_out_of_range);
    RUN_TEST(test_read_float_range_valid);

    RUN_TEST(test_read_string_absent_key);
    RUN_TEST(test_read_string_wrong_type);
    RUN_TEST(test_read_string_out_of_range);
    RUN_TEST(test_read_string_valid);

    RUN_TEST(test_enum_index_lookup);
    RUN_TEST(test_read_enum_absent_key);
    RUN_TEST(test_read_enum_wrong_type);
    RUN_TEST(test_read_enum_out_of_range);
    RUN_TEST(test_read_enum_valid_by_name);
    RUN_TEST(test_read_enum_valid_by_legacy_number);

    RUN_TEST(test_parse_int_value);
    RUN_TEST(test_parse_enum_value);

    RUN_TEST(test_read_u32_exact_bounds);
    RUN_TEST(test_parse_u32_rejects_ambiguous_values);

    RUN_TEST(test_read_i64_absent_key);
    RUN_TEST(test_read_i64_wrong_type);
    RUN_TEST(test_read_i64_string_round_trip);
    RUN_TEST(test_read_i64_negative_string_round_trip);
    RUN_TEST(test_read_i64_number_above_2_53_rejected);
    RUN_TEST(test_read_i64_small_number_accepted);
    RUN_TEST(test_read_i64_out_of_custom_range);
    RUN_TEST(test_read_i64_invalid_string_trailing_garbage);
    RUN_TEST(test_read_i64_empty_string_rejected);
    RUN_TEST(test_read_i64_string_overflow_beyond_int64_rejected);
    RUN_TEST(test_parse_i64_value_direct);

    RUN_TEST(test_i64_to_string);
    RUN_TEST(test_add_i64_stores_as_json_string);
    RUN_TEST(test_add_i64_then_read_i64_round_trip);

    return UNITY_END();
}
