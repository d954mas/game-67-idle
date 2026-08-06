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
   and the primary must never be touched again (T0058). A stub that ignored the
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

/* The primary cannot be read AND could not be copied aside, so its bytes are the
   only copy there is: never write it again. That much was already true. What was
   NOT true is what happened next -- autosave paused with nothing in the product
   able to unpause it, so the player kept playing a session that was thrown away
   on exit and nobody was ever told (T0058, лид: "Игрок вообще не должен ничего
   этого видеть и знать"). The session now continues in a slot beside it. */
static void test_unreadable_primary_is_left_alone_and_the_session_continues_beside_it(void) {
    s_read_status = GAME_STORAGE_READ_ERROR_PRESERVED;
    game_save_load_result_t result;

    game_save_load(&result);

    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_BLOCKED, result.status);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, s_primary_writes, "the unreadable primary was written");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, s_side_writes, "the session was not persisted anywhere");
    TEST_ASSERT_EQUAL_INT(0, s_quarantine_calls); /* storage already tried and failed */
    TEST_ASSERT_EQUAL_INT(7, s_live_value);       /* a real, playable new game */

    /* and autosave is LIVE, which is the entire point */
    game_save_mark_dirty();
    TEST_ASSERT_TRUE(game_save_flush(NULL, 0));
    TEST_ASSERT_EQUAL_INT(2, s_side_writes);
    TEST_ASSERT_EQUAL_INT(0, s_primary_writes);
}

/* Same rule from the other direction: the file parsed as garbage and could not
   be quarantined either, so a repair tool may still want those bytes. */
static void test_unquarantinable_corrupt_primary_is_left_alone(void) {
    s_read_status = GAME_STORAGE_READ_OK;
    s_read_text = "not-json";
    s_live_value = 52;
    game_save_load_result_t result;

    game_save_load(&result);

    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_BLOCKED, result.status);
    TEST_ASSERT_EQUAL_INT(1, s_quarantine_calls); /* tried, refused */
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, s_primary_writes, "the corrupt primary was written");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, s_side_writes, "the session was not persisted anywhere");
}

/* New Game after a blocked load still works -- and still keeps its hands off
   the primary, because nothing about it made the primary safe to overwrite. */
static void test_new_game_after_a_blocked_load_stays_on_the_side_slot(void) {
    s_read_status = GAME_STORAGE_READ_ERROR_PRESERVED;
    game_save_load_result_t result;
    game_save_load(&result);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_BLOCKED, result.status);
    const int side_after_load = s_side_writes;

    TEST_ASSERT_TRUE(game_save_new_game(NULL, 0).persisted);

    TEST_ASSERT_EQUAL_INT(7, s_live_value);
    TEST_ASSERT_EQUAL_INT(side_after_load + 1, s_side_writes);
    TEST_ASSERT_EQUAL_INT(0, s_primary_writes);
}

int main(void) {
    game_save_register_fragment(&s_fragment);
    UNITY_BEGIN();
    RUN_TEST(test_unreadable_primary_is_left_alone_and_the_session_continues_beside_it);
    RUN_TEST(test_unquarantinable_corrupt_primary_is_left_alone);
    RUN_TEST(test_new_game_after_a_blocked_load_stays_on_the_side_slot);
    return UNITY_END();
}
