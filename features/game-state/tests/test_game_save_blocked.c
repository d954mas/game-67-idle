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
/* Which slot a write landed in is the whole contract now: when the primary
   holds something this build must not overwrite, the session moves beside it
   and the primary must never be touched again. A stub that ignored the
   slot could not tell those two apart. */
static int s_primary_writes;
static int s_side_writes;
static bool is_side_slot(const char *slot) {
    return slot != NULL && strstr(slot, "-side") != NULL;
}
static int s_live_value;
static int s_reset_calls;
static int s_new_game_calls;
static char *s_last_write;
static int64_t s_mono_ms;

static int64_t test_mono_ms(void) { return s_mono_ms; }
static int64_t test_wall_ms(void) { return 0; }

static bool validate_document_for_test(
    const cJSON *features, char *error, int error_cap) {
    const cJSON *test = cJSON_GetObjectItemCaseSensitive(features, "test");
    if (!cJSON_IsObject(test) || cJSON_GetObjectItemCaseSensitive(test, "reject") != NULL) {
        if (error != NULL && error_cap > 0) {
            (void)snprintf(error, (size_t)error_cap, "%s", "invalid test document");
        }
        return false;
    }
    return true;
}

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
    (void)error;
    (void)error_cap;
    free(s_last_write);
    s_last_write = copy_text(text);
    ++s_write_calls;
    if (is_side_slot(slot)) {
        ++s_side_writes;
    } else {
        ++s_primary_writes;
    }
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
    (void)error;
    (void)error_cap;
    /* The side slot is a different file: empty unless a previous session of
       this build wrote one. Scripting it from the primary's status would have
       made every test read its own corrupt bytes twice. */
    const game_storage_read_status_t st =
        is_side_slot(slot) ? GAME_STORAGE_READ_ABSENT : s_read_status;
    if (status != NULL) {
        *status = st;
    }
    if (st != GAME_STORAGE_READ_OK) {
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
    s_primary_writes = 0;
    s_side_writes = 0;
    s_live_value = 41;
    s_reset_calls = 0;
    s_new_game_calls = 0;
    s_mono_ms = 0;
    free(s_last_write);
    s_last_write = NULL;
    game_save_set_document_validator(validate_document_for_test);
    game_save__set_clocks_for_test(test_mono_ms, test_wall_ms);
    game_save_init();
}

void tearDown(void) {
    free(s_last_write);
    s_last_write = NULL;
}

/* The rule: a save this build cannot use -- corrupt, or
   written by a newer build -- goes to quarantine, and a normal new save starts
   under the normal name. One file, one name.

   These three cover the case where the move itself is REFUSED, which is the only
   remaining reason not to write: the bytes are still in the slot and writing
   would destroy them. Stopping there for good, with a note in a header
   saying the player could start a New Game. Now the tick keeps trying the move,
   and whatever is holding the file lets go on its own. */
static void test_unreadable_and_immovable_save_holds_writes_instead_of_stopping(void) {
    s_read_status = GAME_STORAGE_READ_ERROR_PRESERVED;
    game_save_load_result_t result;

    game_save_load(&result);

    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_BLOCKED, result.status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, s_write_calls, "wrote over bytes it could not move");
    TEST_ASSERT_EQUAL_INT(7, s_live_value); /* a real, playable session meanwhile */

    /* The move is retried from the tick -- nobody is asked to do anything. */
    s_quarantine_allowed = true;
    game_save_mark_dirty();
    game_save_tick();
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, s_quarantine_calls,
                                  "the move was tried once at load and never retried");
    TEST_ASSERT_TRUE_MESSAGE(game_save_flush(NULL, 0), "saving did not resume");
    TEST_ASSERT_EQUAL_INT(1, s_primary_writes);
}

static void test_corrupt_and_immovable_save_holds_writes_too(void) {
    s_read_status = GAME_STORAGE_READ_OK;
    s_read_text = "not-json";
    s_live_value = 52;
    game_save_load_result_t result;

    game_save_load(&result);

    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_BLOCKED, result.status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, s_write_calls, "wrote over bytes it could not move");
    TEST_ASSERT_TRUE_MESSAGE(s_quarantine_calls > 0, "never tried to move the file aside");
}

/* An explicit New Game does not make those bytes safe to overwrite, so it must
   not become a way around the hold. */
static void test_new_game_does_not_overwrite_a_file_that_could_not_be_moved(void) {
    s_read_status = GAME_STORAGE_READ_ERROR_PRESERVED;
    game_save_load_result_t result;
    game_save_load(&result);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_BLOCKED, result.status);

    TEST_ASSERT_FALSE(game_save_new_game(NULL, 0).persisted);
    TEST_ASSERT_EQUAL_INT(0, s_primary_writes);

    /* ...and once the file frees up, the same session persists normally. */
    s_quarantine_allowed = true;
    game_save_tick();
    TEST_ASSERT_TRUE(game_save_flush(NULL, 0));
    TEST_ASSERT_EQUAL_INT(1, s_primary_writes);
    TEST_ASSERT_EQUAL_INT(7, s_live_value);
}

static void test_playtime_accumulates_monotonic_milliseconds_and_resets_on_new_game(void) {
    game_save_update_playtime(true);
    s_mono_ms += 1;
    game_save_update_playtime(true);
    TEST_ASSERT_EQUAL_INT64(1, game_save_playtime_ms());

    TEST_ASSERT_TRUE(game_save_flush(NULL, 0));
    TEST_ASSERT_NOT_NULL(s_last_write);
    TEST_ASSERT_NOT_NULL(strstr(s_last_write, "\"playtime_ms\":\"1\""));

    TEST_ASSERT_TRUE(game_save_new_game(NULL, 0).state_changed);
    TEST_ASSERT_EQUAL_INT64(0, game_save_playtime_ms());
}

static void test_playtime_loads_legacy_and_rejects_invalid_import_without_mutation(void) {
    s_read_status = GAME_STORAGE_READ_OK;
    s_read_text =
        "{\"format\":1,\"save_version\":1,\"saved_at\":1,\"save_seq\":1,"
        "\"app\":\"blocked_save_test\",\"build\":\"0\","
        "\"features\":{\"test\":{\"v\":1,\"value\":4}}}";
    game_save_load_result_t result;
    game_save_load(&result);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_LOADED, result.status);
    TEST_ASSERT_EQUAL_INT64(0, game_save_playtime_ms());

    game_save_update_playtime(true);
    s_mono_ms += 7;
    game_save_update_playtime(true);
    TEST_ASSERT_EQUAL_INT64(7, game_save_playtime_ms());
    const char *invalid =
        "{\"format\":1,\"save_version\":1,\"saved_at\":1,\"save_seq\":1,"
        "\"playtime_ms\":-1,\"app\":\"blocked_save_test\",\"build\":\"0\","
        "\"features\":{\"test\":{\"v\":1,\"value\":4}}}";
    TEST_ASSERT_FALSE(game_save_import_string(invalid, NULL, 0));
    TEST_ASSERT_EQUAL_INT64(7, game_save_playtime_ms());
}

static void test_playtime_update_tracks_only_active_monotonic_intervals(void) {
    game_save_update_playtime(true);
    s_mono_ms = 17;
    game_save_update_playtime(true);
    s_mono_ms = 29;
    game_save_update_playtime(false);
    TEST_ASSERT_EQUAL_INT64(29, game_save_playtime_ms());

    s_mono_ms = 300;
    game_save_update_playtime(false);
    game_save_update_playtime(true);
    s_mono_ms = 305;
    game_save_update_playtime(true);
    s_mono_ms = 200;
    game_save_update_playtime(true);
    s_mono_ms = 305;
    game_save_update_playtime(true);
    s_mono_ms = 309;
    game_save_update_playtime(true);
    TEST_ASSERT_EQUAL_INT64(38, game_save_playtime_ms());

    TEST_ASSERT_TRUE(game_save_new_game(NULL, 0).state_changed);
    game_save_update_playtime(true);
    s_mono_ms = 316;
    game_save_update_playtime(true);
    TEST_ASSERT_EQUAL_INT64(7, game_save_playtime_ms());
}

static void test_ntgs_fragment_metadata_named_field_remains_a_number(void) {
    const char *text =
        "NTGS 1\nformat=1\nsave_version=1\nsaved_at=1\nsave_seq=1\n"
        "playtime_ms=1\napp=\"blocked_save_test\"\nbuild=\"0\"\n"
        "\n[test 1]\nplaytime_ms=1.5\n";
    char error[128] = {0};
    cJSON *document = game_save__parse_document_for_test(text, error, (int)sizeof error);
    TEST_ASSERT_NOT_NULL(document);
    cJSON *features = cJSON_GetObjectItemCaseSensitive(document, "features");
    cJSON *fragment = cJSON_GetObjectItemCaseSensitive(features, "test");
    cJSON *value = cJSON_GetObjectItemCaseSensitive(fragment, "playtime_ms");
    TEST_ASSERT_TRUE(cJSON_IsNumber(value));
    TEST_ASSERT_TRUE(value->valuedouble > 1.49 && value->valuedouble < 1.51);
    cJSON_Delete(document);
}

static void test_document_validation_is_pure_and_requires_complete_metadata(void) {
    s_live_value = 73;
    const char *missing_metadata =
        "{\"format\":1,\"save_version\":1,\"save_seq\":1,\"app\":\"blocked_save_test\","
        "\"build\":\"0\",\"features\":{\"test\":{\"v\":1,\"value\":4}}}";
    TEST_ASSERT_FALSE(game_save_validate_document_string(missing_metadata, NULL, 0));
    TEST_ASSERT_EQUAL_INT(73, s_live_value);

    const char *invalid_full_document =
        "{\"format\":1,\"save_version\":1,\"saved_at\":1,\"save_seq\":1,"
        "\"app\":\"blocked_save_test\",\"build\":\"0\","
        "\"features\":{\"test\":{\"v\":1,\"reject\":true}}}";
    TEST_ASSERT_FALSE(game_save_validate_document_string(invalid_full_document, NULL, 0));
    TEST_ASSERT_EQUAL_INT(73, s_live_value);
}

int main(void) {
    game_save_register_fragment(&s_fragment);
    UNITY_BEGIN();
    RUN_TEST(test_unreadable_and_immovable_save_holds_writes_instead_of_stopping);
    RUN_TEST(test_corrupt_and_immovable_save_holds_writes_too);
    RUN_TEST(test_new_game_does_not_overwrite_a_file_that_could_not_be_moved);
    RUN_TEST(test_playtime_accumulates_monotonic_milliseconds_and_resets_on_new_game);
    RUN_TEST(test_playtime_loads_legacy_and_rejects_invalid_import_without_mutation);
    RUN_TEST(test_playtime_update_tracks_only_active_monotonic_intervals);
    RUN_TEST(test_ntgs_fragment_metadata_named_field_remains_a_number);
    RUN_TEST(test_document_validation_is_pure_and_requires_complete_metadata);
    return UNITY_END();
}
