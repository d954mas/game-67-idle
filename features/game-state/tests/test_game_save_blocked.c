#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "game_save.h"
#include "game_storage.h"
#include "unity.h"

static game_storage_read_status_t s_read_status;
static const char *s_read_text;
static bool s_quarantine_allowed;
static int s_quarantine_calls;
static int s_write_calls;
static int s_live_value;
static int s_reset_calls;
static int s_new_game_calls;

static char *copy_text(const char *text) {
    const size_t size = strlen(text) + 1U;
    char *copy = (char *)malloc(size);
    if (copy != NULL) {
        memcpy(copy, text, size);
    }
    return copy;
}

bool game_storage_write(
    const char *slot, const char *text, char *error, int error_cap) {
    (void)slot;
    (void)text;
    (void)error;
    (void)error_cap;
    ++s_write_calls;
    return true;
}

/* The blocking variant differs only in whether a refused rename may be waited
   out, which a stub has nothing to refuse -- so it is the same write. Both
   exist because the real storage has two callers with different rights to
   block: see game_storage_write_blocking in game_storage.h. */
bool game_storage_write_blocking(
    const char *slot, const char *text, char *error, int error_cap) {
    return game_storage_write(slot, text, error, error_cap);
}

bool game_storage_read(
    const char *slot, char **out, game_storage_read_status_t *status,
    char *error, int error_cap) {
    (void)slot;
    (void)error;
    (void)error_cap;
    if (status != NULL) {
        *status = s_read_status;
    }
    if (s_read_status != GAME_STORAGE_READ_OK) {
        return false;
    }
    *out = copy_text(s_read_text);
    return *out != NULL;
}

bool game_storage_exists(const char *slot) {
    (void)slot;
    return s_read_status != GAME_STORAGE_READ_ABSENT;
}

bool game_storage_write_backup(
    const char *slot, char *error, int error_cap) {
    (void)slot;
    (void)error;
    (void)error_cap;
    return true;
}

bool game_storage_read_backup(
    const char *slot, char **out, char *error, int error_cap) {
    (void)slot;
    (void)out;
    (void)error;
    (void)error_cap;
    return false;
}

bool game_storage_quarantine(
    const char *slot, char *error, int error_cap) {
    (void)slot;
    (void)error;
    (void)error_cap;
    ++s_quarantine_calls;
    return s_quarantine_allowed;
}

bool game_storage_probe(char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    return true;
}

static void fragment_reset(void) {
    ++s_reset_calls;
    s_live_value = 0;
}

static void fragment_new_game(void) {
    ++s_new_game_calls;
    s_live_value = 7;
}

static cJSON *fragment_to_json(void) {
    cJSON *object = cJSON_CreateObject();
    if (object != NULL) {
        cJSON_AddNumberToObject(object, "value", s_live_value);
    }
    return object;
}

static bool fragment_from_json(
    const cJSON *fragment, char *error, int error_cap) {
    (void)fragment;
    (void)error;
    (void)error_cap;
    return true;
}

static const GameSaveFragment s_fragment = {
    .id = "test",
    .version = 1,
    .reset = fragment_reset,
    .on_new_game = fragment_new_game,
    .to_json = fragment_to_json,
    .from_json = fragment_from_json,
};

void setUp(void) {
    s_read_status = GAME_STORAGE_READ_ABSENT;
    s_read_text = NULL;
    s_quarantine_allowed = false;
    s_quarantine_calls = 0;
    s_write_calls = 0;
    s_live_value = 41;
    s_reset_calls = 0;
    s_new_game_calls = 0;
    game_save_init();
}

void tearDown(void) {}

static void test_unreadable_unquarantined_primary_blocks_without_mutation(void) {
    s_read_status = GAME_STORAGE_READ_ERROR_PRESERVED;
    game_save_load_result_t result;

    game_save_load(&result);

    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_BLOCKED, result.status);
    TEST_ASSERT_EQUAL_INT(41, s_live_value);
    TEST_ASSERT_EQUAL_INT(0, s_reset_calls);
    TEST_ASSERT_EQUAL_INT(0, s_new_game_calls);
    TEST_ASSERT_EQUAL_INT(0, s_quarantine_calls);
    TEST_ASSERT_EQUAL_INT(0, s_write_calls);

    game_save_mark_dirty();
    TEST_ASSERT_FALSE(game_save_flush(NULL, 0));
    TEST_ASSERT_EQUAL_INT(0, s_write_calls);
}

static void test_parse_failure_blocks_when_quarantine_cannot_be_verified(void) {
    s_read_status = GAME_STORAGE_READ_OK;
    s_read_text = "not-json";
    s_live_value = 52;
    game_save_load_result_t result;

    game_save_load(&result);

    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_BLOCKED, result.status);
    TEST_ASSERT_EQUAL_INT(52, s_live_value);
    TEST_ASSERT_EQUAL_INT(0, s_reset_calls);
    TEST_ASSERT_EQUAL_INT(1, s_quarantine_calls);
    TEST_ASSERT_EQUAL_INT(0, s_write_calls);
}

static void test_explicit_new_game_unblocks_and_persists(void) {
    s_read_status = GAME_STORAGE_READ_ERROR_PRESERVED;
    game_save_load_result_t result;
    game_save_load(&result);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_BLOCKED, result.status);

    TEST_ASSERT_TRUE(game_save_new_game(NULL, 0).persisted);

    TEST_ASSERT_EQUAL_INT(1, s_reset_calls);
    TEST_ASSERT_EQUAL_INT(1, s_new_game_calls);
    TEST_ASSERT_EQUAL_INT(7, s_live_value);
    TEST_ASSERT_EQUAL_INT(1, s_write_calls);
}

int main(void) {
    game_save_register_fragment(&s_fragment);
    UNITY_BEGIN();
    RUN_TEST(test_unreadable_unquarantined_primary_blocks_without_mutation);
    RUN_TEST(test_parse_failure_blocks_when_quarantine_cannot_be_verified);
    RUN_TEST(test_explicit_new_game_unblocks_and_persists);
    return UNITY_END();
}
