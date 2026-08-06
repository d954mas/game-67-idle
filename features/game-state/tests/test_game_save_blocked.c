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
    (void)text;
    (void)error;
    (void)error_cap;
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
    game_save_init();
}

void tearDown(void) {}

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

int main(void) {
    game_save_register_fragment(&s_fragment);
    UNITY_BEGIN();
    RUN_TEST(test_unreadable_and_immovable_save_holds_writes_instead_of_stopping);
    RUN_TEST(test_corrupt_and_immovable_save_holds_writes_too);
    RUN_TEST(test_new_game_does_not_overwrite_a_file_that_could_not_be_moved);
    return UNITY_END();
}
