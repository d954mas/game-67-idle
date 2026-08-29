#include "unity.h"

#include "scalar_state.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_generated_scalar_fragment_round_trips_readable_text(void) {
    ScalarState source;
    scalar_state_init_defaults(&source);
    source.active = true;
    source.count = -7;
    source.coins = 42U;
    source.total = 998;
    source.rate = 0.75F;
    source.mode_index = SCALAR_STATE_MODE_ON;
    (void)strcpy(source.name, "Green Slime");
    source.has_note = true;
    (void)strcpy(source.note, "hand editable");

    char text[1024];
    game_save_text_writer_t writer;
    game_save_text_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_text_write_preamble(&writer));
    TEST_ASSERT_TRUE(game_save_text_begin_fragment(&writer, "scalar", 1));
    const size_t body_offset = game_save_text_writer_size(&writer);
    TEST_ASSERT_TRUE(scalar_state_write_text(&source, &writer));
    TEST_ASSERT_NOT_NULL(strstr(text, "mode_index=\"on\"\n"));
    TEST_ASSERT_NOT_NULL(strstr(text, "note=\"hand editable\"\n"));

    ScalarState loaded;
    scalar_state_init_defaults(&loaded);
    char error[128];
    TEST_ASSERT_TRUE(scalar_state_from_text(
        &loaded, text + body_offset, game_save_text_writer_size(&writer) - body_offset,
        error, sizeof error));
    TEST_ASSERT_TRUE(loaded.active);
    TEST_ASSERT_EQUAL_INT(-7, loaded.count);
    TEST_ASSERT_EQUAL_UINT32(42U, loaded.coins);
    TEST_ASSERT_EQUAL_INT64(998, loaded.total);
    TEST_ASSERT_FLOAT_WITHIN(0.00001F, 0.75F, loaded.rate);
    TEST_ASSERT_EQUAL_INT(SCALAR_STATE_MODE_ON, loaded.mode_index);
    TEST_ASSERT_EQUAL_STRING("Green Slime", loaded.name);
    TEST_ASSERT_TRUE(loaded.has_note);
    TEST_ASSERT_EQUAL_STRING("hand editable", loaded.note);
}

void test_generated_reader_is_transactional_and_defaults_missing_fields(void) {
    ScalarState state;
    scalar_state_init_defaults(&state);
    state.count = 5;
    char error[128];
    static const char invalid[] = "count=99\n";
    TEST_ASSERT_FALSE(scalar_state_from_text(
        &state, invalid, sizeof invalid - 1U, error, sizeof error));
    TEST_ASSERT_EQUAL_INT(5, state.count);

    static const char partial[] = "active=true\nunknown=17\n";
    TEST_ASSERT_TRUE(scalar_state_from_text(
        &state, partial, sizeof partial - 1U, error, sizeof error));
    TEST_ASSERT_TRUE(state.active);
    TEST_ASSERT_EQUAL_INT(SCALAR_STATE_COUNT_DEFAULT, state.count);
    TEST_ASSERT_EQUAL_STRING(SCALAR_STATE_NAME_DEFAULT, state.name);

    static const char duplicate[] = "count=1\ncount=2\n";
    state.count = 6;
    TEST_ASSERT_FALSE(scalar_state_from_text(
        &state, duplicate, sizeof duplicate - 1U, error, sizeof error));
    TEST_ASSERT_EQUAL_INT(6, state.count);
    TEST_ASSERT_EQUAL_STRING("duplicate field", error);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_generated_scalar_fragment_round_trips_readable_text);
    RUN_TEST(test_generated_reader_is_transactional_and_defaults_missing_fields);
    return UNITY_END();
}
