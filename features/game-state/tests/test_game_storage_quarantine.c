/* Quarantine retention of the native storage backend.
 *
 * A corrupt primary is moved aside under `<slot>.corrupt[-N]` so a human can
 * inspect it. The set is bounded; when it is full the oldest copy is rotated
 * out rather than the move refused, because a refused move leaves the slot
 * read-only for the rest of the install. An empty primary holds nothing to
 * inspect and is dropped without taking a retention name.
 *
 * System headers before Unity to avoid the noreturn / __declspec conflict on
 * MSVC (unity_internals.h pulls in <stdnoreturn.h>).
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <sys/utime.h>
#define set_file_mtime(path, when) _utime((path), &(struct _utimbuf){(when), (when)})
#else
#include <utime.h>
#define set_file_mtime(path, when) utime((path), &(struct utimbuf){(when), (when)})
#endif

/* clang-format off */
#include "game_storage.h"
#include "unity.h"
/* clang-format on */

#ifndef GAME_STORAGE_NATIVE_ROOT
#error "GAME_STORAGE_NATIVE_ROOT must be defined via CMake: these tests need the same path the backend resolves"
#endif

/* Mirrors the backend's retention constant; a mismatch fails loudly below. */
#define RETAINED 5
#define SLOT_ROTATE "q_rotate"
#define SLOT_EMPTY "q_empty"
#define SLOT_READ "q_read"

static void slot_path(const char *slot, const char *suffix, char *out, size_t cap) {
    TEST_ASSERT_TRUE(
        (size_t)snprintf(out, cap, "%s/%s%s", GAME_STORAGE_NATIVE_ROOT, slot, suffix) < cap);
}

static void quarantine_path(const char *slot, int index, char *out, size_t cap) {
    if (index == 0) {
        slot_path(slot, ".corrupt", out, cap);
    } else {
        char suffix[32];
        (void)snprintf(suffix, sizeof suffix, ".corrupt-%d", index);
        slot_path(slot, suffix, out, cap);
    }
}

static void remove_slot(const char *slot) {
    char path[512];
    static const char *const suffixes[] = {".json", ".json.tmp", ".bak", ".bak.tmp"};
    for (size_t i = 0; i < sizeof suffixes / sizeof suffixes[0]; ++i) {
        slot_path(slot, suffixes[i], path, sizeof path);
        (void)remove(path);
    }
    for (int i = 0; i < RETAINED + 2; ++i) {
        quarantine_path(slot, i, path, sizeof path);
        (void)remove(path);
    }
}

static void cleanup_all(void) {
    remove_slot(SLOT_ROTATE);
    remove_slot(SLOT_EMPTY);
    remove_slot(SLOT_READ);
}

/* The storage root is created by the backend's first write; tests that plant
   files by hand must not depend on which test ran before them. */
static void ensure_root(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE_MESSAGE(
        game_storage_write_blocking(SLOT_READ, "root", err, (int)sizeof err), err);
}

void setUp(void) { cleanup_all(); ensure_root(); cleanup_all(); }
void tearDown(void) { cleanup_all(); }

static void write_raw(const char *path, const char *content) {
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    const size_t length = strlen(content);
    const size_t written = fwrite(content, 1, length, file);
    const int closed = fclose(file);
    TEST_ASSERT_EQUAL_UINT(length, written);
    TEST_ASSERT_EQUAL_INT(0, closed);
}

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    (void)fclose(file);
    return true;
}

static char *read_raw(const char *path) {
    FILE *file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    char *buffer = calloc(1, 4096);
    TEST_ASSERT_NOT_NULL(buffer);
    (void)fread(buffer, 1, 4095, file);
    (void)fclose(file);
    return buffer;
}

static int count_quarantined(const char *slot) {
    char path[512];
    int count = 0;
    for (int i = 0; i < RETAINED + 2; ++i) {
        quarantine_path(slot, i, path, sizeof path);
        if (file_exists(path)) {
            ++count;
        }
    }
    return count;
}

static bool any_quarantined_holds(const char *slot, const char *content) {
    char path[512];
    for (int i = 0; i < RETAINED + 2; ++i) {
        quarantine_path(slot, i, path, sizeof path);
        if (!file_exists(path)) {
            continue;
        }
        char *text = read_raw(path);
        const bool match = strcmp(text, content) == 0;
        free(text);
        if (match) {
            return true;
        }
    }
    return false;
}

static void write_and_quarantine(const char *slot, const char *content) {
    char err[128] = {0};
    TEST_ASSERT_TRUE_MESSAGE(game_storage_write_blocking(slot, content, err, (int)sizeof err), err);
    TEST_ASSERT_TRUE_MESSAGE(game_storage_quarantine(slot, err, (int)sizeof err), err);
}

/* Filesystems stamp rapid writes with equal times, so the order under test is
   set by hand. The oldest is a middle name on purpose: a picker that always
   took the first name, or the last, must not pass. */
#define OLDEST_INDEX 2
static void age_quarantined_copies(const char *slot) {
    char path[512];
    const time_t now = time(NULL);
    for (int i = 0; i < RETAINED; ++i) {
        quarantine_path(slot, i, path, sizeof path);
        TEST_ASSERT_TRUE(file_exists(path));
        const time_t age_hours = i == OLDEST_INDEX ? (time_t)RETAINED + 1 : (time_t)(i + 1);
        TEST_ASSERT_EQUAL_INT(0, set_file_mtime(path, now - age_hours * 3600));
    }
}

void test_full_retention_rotates_out_the_oldest_copy(void) {
    char generation[RETAINED + 1][16];
    for (int i = 0; i < RETAINED; ++i) {
        (void)snprintf(generation[i], sizeof generation[i], "gen-%d", i);
        write_and_quarantine(SLOT_ROTATE, generation[i]);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(RETAINED, count_quarantined(SLOT_ROTATE),
        "retention size differs from the backend's constant");
    age_quarantined_copies(SLOT_ROTATE);

    (void)snprintf(generation[RETAINED], sizeof generation[RETAINED], "gen-%d", RETAINED);
    write_and_quarantine(SLOT_ROTATE, generation[RETAINED]);

    TEST_ASSERT_EQUAL_INT(RETAINED, count_quarantined(SLOT_ROTATE));
    TEST_ASSERT_TRUE_MESSAGE(any_quarantined_holds(SLOT_ROTATE, generation[RETAINED]),
        "the newest corrupt copy was not retained");
    TEST_ASSERT_FALSE_MESSAGE(any_quarantined_holds(SLOT_ROTATE, generation[OLDEST_INDEX]),
        "the oldest copy survived a full rotation");
    TEST_ASSERT_TRUE_MESSAGE(any_quarantined_holds(SLOT_ROTATE, generation[0]),
        "a younger copy was evicted instead of the oldest");
    TEST_ASSERT_FALSE_MESSAGE(game_storage_exists(SLOT_ROTATE),
        "primary must be gone after quarantine");
}

void test_empty_primary_is_dropped_without_a_retention_name(void) {
    char err[128] = {0};
    char primary[512];
    slot_path(SLOT_EMPTY, ".json", primary, sizeof primary);
    write_raw(primary, "");

    TEST_ASSERT_TRUE_MESSAGE(game_storage_quarantine(SLOT_EMPTY, err, (int)sizeof err), err);
    TEST_ASSERT_FALSE(file_exists(primary));
    TEST_ASSERT_EQUAL_INT(0, count_quarantined(SLOT_EMPTY));

    /* The slot is writable again straight away. */
    TEST_ASSERT_TRUE_MESSAGE(game_storage_write_blocking(SLOT_EMPTY, "fresh", err, (int)sizeof err), err);
    TEST_ASSERT_TRUE(game_storage_exists(SLOT_EMPTY));
}

/* Storage reports an empty primary as OK with empty text, never as ABSENT:
   the two statuses are what separates "no save yet" from "a save we failed". */
void test_read_reports_absent_and_empty_differently(void) {
    char err[128] = {0};
    char *out = NULL;
    game_storage_read_status_t status = GAME_STORAGE_READ_OK;

    TEST_ASSERT_FALSE(game_storage_read(SLOT_READ, &out, &status, err, (int)sizeof err));
    TEST_ASSERT_EQUAL_INT(GAME_STORAGE_READ_ABSENT, status);

    char primary[512];
    slot_path(SLOT_READ, ".json", primary, sizeof primary);
    write_raw(primary, "");
    TEST_ASSERT_TRUE_MESSAGE(game_storage_read(SLOT_READ, &out, &status, err, (int)sizeof err), err);
    TEST_ASSERT_EQUAL_INT(GAME_STORAGE_READ_OK, status);
    TEST_ASSERT_EQUAL_STRING("", out);
    free(out);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_full_retention_rotates_out_the_oldest_copy);
    RUN_TEST(test_empty_primary_is_dropped_without_a_retention_name);
    RUN_TEST(test_read_reports_absent_and_empty_differently);
    return UNITY_END();
}
