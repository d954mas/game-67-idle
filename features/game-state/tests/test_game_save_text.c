#include "unity.h"

#include "game_save_text.h"

#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_writer_produces_readable_versioned_document(void) {
    char text[512];
    game_save_text_writer_t writer;
    game_save_text_writer_init(&writer, text, sizeof text);

    TEST_ASSERT_TRUE(game_save_text_write_preamble(&writer));
    TEST_ASSERT_TRUE(game_save_text_write_i64(&writer, "format", 1));
    TEST_ASSERT_TRUE(game_save_text_write_string(&writer, "app", "sample-game"));
    TEST_ASSERT_TRUE(game_save_text_begin_fragment(&writer, "settings", 2));
    TEST_ASSERT_TRUE(game_save_text_write_number(&writer, "master_volume", 0.8));
    TEST_ASSERT_TRUE(game_save_text_write_bool(&writer, "muted", false));
    TEST_ASSERT_TRUE(game_save_text_write_string(&writer, "label", "line\n\"quoted\""));

    TEST_ASSERT_TRUE(game_save_text_writer_ok(&writer));
    TEST_ASSERT_EQUAL_STRING(
        "NTGS 1\n"
        "format=1\n"
        "app=\"sample-game\"\n"
        "\n[settings 2]\n"
        "master_volume=0.80000000000000004\n"
        "muted=false\n"
        "label=\"line\\n\\\"quoted\\\"\"\n",
        game_save_text_writer_data(&writer));
}

void test_reader_parses_comments_fragments_and_typed_values(void) {
    static const char text[] =
        "NTGS 1\n"
        "# hand-edited save\n"
        "save_seq = 42\n"
        "\n"
        "[run 3]\n"
        "active=true\n"
        "coins=-17\n"
        "name=\"Slime ð\"\n";
    game_save_text_reader_t reader;
    game_save_text_record_t record;
    char error[128];
    game_save_text_reader_init(&reader, text, sizeof text - 1U);

    TEST_ASSERT_EQUAL(GAME_SAVE_TEXT_RECORD_META,
                      game_save_text_reader_next(&reader, &record, error, sizeof error));
    TEST_ASSERT_TRUE(game_save_text_record_key_is(&record, "save_seq"));
    int64_t sequence = 0;
    TEST_ASSERT_TRUE(game_save_text_record_i64(&record, 0, INT64_MAX, &sequence, error, sizeof error));
    TEST_ASSERT_EQUAL_INT64(42, sequence);

    TEST_ASSERT_EQUAL(GAME_SAVE_TEXT_RECORD_FRAGMENT,
                      game_save_text_reader_next(&reader, &record, error, sizeof error));
    TEST_ASSERT_TRUE(game_save_text_record_key_is(&record, "run"));
    TEST_ASSERT_EQUAL_INT(3, record.version);

    TEST_ASSERT_EQUAL(GAME_SAVE_TEXT_RECORD_FIELD,
                      game_save_text_reader_next(&reader, &record, error, sizeof error));
    bool active = false;
    TEST_ASSERT_TRUE(game_save_text_record_bool(&record, &active, error, sizeof error));
    TEST_ASSERT_TRUE(active);

    TEST_ASSERT_EQUAL(GAME_SAVE_TEXT_RECORD_FIELD,
                      game_save_text_reader_next(&reader, &record, error, sizeof error));
    int64_t coins = 0;
    TEST_ASSERT_TRUE(game_save_text_record_i64(&record, -100, 100, &coins, error, sizeof error));
    TEST_ASSERT_EQUAL_INT64(-17, coins);

    TEST_ASSERT_EQUAL(GAME_SAVE_TEXT_RECORD_FIELD,
                      game_save_text_reader_next(&reader, &record, error, sizeof error));
    char name[32];
    TEST_ASSERT_TRUE(game_save_text_record_string(&record, name, sizeof name, error, sizeof error));
    TEST_ASSERT_EQUAL_STRING("Slime ð", name);

    TEST_ASSERT_EQUAL(GAME_SAVE_TEXT_DONE,
                      game_save_text_reader_next(&reader, &record, error, sizeof error));
}

void test_reader_rejects_invalid_preamble_and_unterminated_string(void) {
    game_save_text_reader_t reader;
    game_save_text_record_t record;
    char error[128];

    static const char wrong[] = "JSON 1\nformat=1\n";
    game_save_text_reader_init(&reader, wrong, sizeof wrong - 1U);
    TEST_ASSERT_EQUAL(GAME_SAVE_TEXT_ERROR,
                      game_save_text_reader_next(&reader, &record, error, sizeof error));
    TEST_ASSERT_EQUAL_STRING("line 1: expected NTGS 1", error);

    static const char broken[] = "NTGS 1\n[game 1]\nname=\"unterminated\n";
    game_save_text_reader_init(&reader, broken, sizeof broken - 1U);
    TEST_ASSERT_EQUAL(GAME_SAVE_TEXT_RECORD_FRAGMENT,
                      game_save_text_reader_next(&reader, &record, error, sizeof error));
    TEST_ASSERT_EQUAL(GAME_SAVE_TEXT_RECORD_FIELD,
                      game_save_text_reader_next(&reader, &record, error, sizeof error));
    char value[32];
    TEST_ASSERT_FALSE(game_save_text_record_string(&record, value, sizeof value, error, sizeof error));
    TEST_ASSERT_EQUAL_STRING("line 3: unterminated string", error);
}

void test_writer_fails_closed_without_partial_line(void) {
    char text[18];
    game_save_text_writer_t writer;
    game_save_text_writer_init(&writer, text, sizeof text);
    TEST_ASSERT_TRUE(game_save_text_write_preamble(&writer));
    const size_t complete_size = game_save_text_writer_size(&writer);
    TEST_ASSERT_FALSE(game_save_text_write_string(&writer, "name", "too long"));
    TEST_ASSERT_FALSE(game_save_text_writer_ok(&writer));
    TEST_ASSERT_EQUAL_UINT(complete_size, game_save_text_writer_size(&writer));
    TEST_ASSERT_EQUAL_STRING("NTGS 1\n", text);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_writer_produces_readable_versioned_document);
    RUN_TEST(test_reader_parses_comments_fragments_and_typed_values);
    RUN_TEST(test_reader_rejects_invalid_preamble_and_unterminated_string);
    RUN_TEST(test_writer_fails_closed_without_partial_line);
    return UNITY_END();
}
