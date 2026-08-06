/* The contract between game_storage's TWO write modes (T0055).
 *
 * `game_storage_write` is called by game_save_tick, from inside the frame: it
 * must never wait, because the game may not drop frames for a save. A refused
 * rename there is not a loss -- the state stays dirty and the next tick retries.
 *
 * `game_storage_write_blocking` is called by load, new game, recovery from
 * .bak, quarantine and an explicit flush: synchronous moments with no frame to
 * lose, which would rather wait out a transient refusal than leave the primary
 * unwritten.
 *
 * On Windows a rename is not a wait: another process holding either path
 * without FILE_SHARE_DELETE makes MoveFileEx FAIL, and the window clears by
 * itself in milliseconds. A handle opened with a restrictive share mode is that
 * same condition on purpose -- measured here as win32 error 5 when the
 * DESTINATION is held and 32 when the SOURCE (the temp file) is.
 *
 * These tests live beside the FEATURE rather than in a game, because that is
 * what they test. They were written in a game's test file first and moved here
 * (лид, 2026-08-06); the template's copy of the storage tests never had them at
 * all, which is exactly the gap this placement closes.
 *
 * System headers before Unity to avoid the noreturn / __declspec conflict on
 * MSVC (unity_internals.h pulls in <stdnoreturn.h>).
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* clang-format off */
#include "game_storage.h"
#include "unity.h"
/* clang-format on */

#ifndef GAME_STORAGE_NATIVE_ROOT
#error "GAME_STORAGE_NATIVE_ROOT must be defined via CMake: these tests need the same path the backend resolves"
#endif

#define SLOT_CLEARS "wm_clears"
#define SLOT_HELD "wm_held"
#define SLOT_TMP_HELD "wm_tmp_held"
#define SLOT_PERMANENT "wm_permanent"
#define SLOT_AGREE "wm_agree"

/* Paths are built from the SAME macro the backend resolves, not from a relative
   "build/saves" that only coincides with it when the working directory is
   right. That divergence has cost real debugging time elsewhere in this repo. */
static void slot_path(const char *slot, const char *suffix, char *out, size_t cap) {
    TEST_ASSERT_TRUE(
        (size_t)snprintf(out, cap, "%s/%s%s", GAME_STORAGE_NATIVE_ROOT, slot, suffix) < cap);
}

static void remove_slot(const char *slot) {
    char path[512];
    slot_path(slot, ".json", path, sizeof path);
    (void)remove(path);
    slot_path(slot, ".json.tmp", path, sizeof path);
    (void)remove(path);
    slot_path(slot, ".bak", path, sizeof path);
    (void)remove(path);
}

static void remove_permanent_obstacle(void) {
    char path[512];
    slot_path(SLOT_PERMANENT, ".json/marker.txt", path, sizeof path);
    (void)remove(path);
    slot_path(SLOT_PERMANENT, ".json", path, sizeof path);
    (void)remove(path); /* if a plain file ever lands there, it must not persist */
#ifdef _WIN32
    (void)RemoveDirectoryA(path);
#else
    (void)rmdir(path);
#endif
}

static void cleanup_all(void) {
    remove_slot(SLOT_CLEARS);
    remove_slot(SLOT_HELD);
    remove_slot(SLOT_TMP_HELD);
    remove_slot(SLOT_AGREE);
    remove_permanent_obstacle();
}

void setUp(void) { cleanup_all(); }
void tearDown(void) { cleanup_all(); }

static void write_raw(const char *path, const char *content) {
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    const size_t length = strlen(content);
    TEST_ASSERT_EQUAL_UINT(length, fwrite(content, 1, length, file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void assert_slot_reads(const char *slot, const char *expected) {
    char err[128] = {0};
    char *out = NULL;
    TEST_ASSERT_TRUE_MESSAGE(
        game_storage_read(slot, &out, NULL, err, (int)sizeof err), err);
    TEST_ASSERT_EQUAL_STRING(expected, out);
    free(out);
}

/* Both modes are the same write when nothing refuses -- the difference is only
   what they do about a refusal. Also the only test here with anything to say on
   POSIX, where rename() has no equivalent refusal to reproduce. */
void test_both_modes_write_and_read_back(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE_MESSAGE(
        game_storage_write(SLOT_AGREE, "NON-BLOCKING", err, (int)sizeof err), err);
    assert_slot_reads(SLOT_AGREE, "NON-BLOCKING");

    TEST_ASSERT_TRUE_MESSAGE(
        game_storage_write_blocking(SLOT_AGREE, "BLOCKING", err, (int)sizeof err), err);
    assert_slot_reads(SLOT_AGREE, "BLOCKING");
}

#ifdef _WIN32

/* A refused write must cost about what a successful one costs: the whole write
   is fopen + fwrite + _commit + one MoveFileEx, single-digit milliseconds here
   and tens under load. Any wait worth having must outlast a ~15.6ms timer tick
   and realistically ramps past 100ms, so this threshold is what notices a wait
   appearing where none belongs. Failure direction is false RED on a badly
   loaded machine, never false green. */
#define NO_WAITING_MS 60

/* No sharing at all: nobody else may even open it. Reproduces the DESTINATION
   half of the refusal -- win32 error 5, the code captured in the field. */
static HANDLE open_exclusively(const char *path) {
    return CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
}

/* Readable and writable to everyone else but NOT deletable -- how an on-access
   scanner actually opens a file it is inspecting. Our own fopen still works; the
   MOVE does not, because moving a file needs delete access on its source. This
   is the SOURCE half -- win32 error 32. */
static HANDLE open_undeletable(const char *path) {
    return CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_EXISTING, 0, NULL);
}

static HANDLE g_held = INVALID_HANDLE_VALUE;

static DWORD WINAPI release_held(LPVOID unused) {
    (void)unused;
    Sleep(40); /* inside the blocking budget, past its first three attempts */
    (void)CloseHandle(g_held);
    g_held = INVALID_HANDLE_VALUE;
    return 0;
}

/* THE point of the blocking mode. Without it this is a write that simply fails,
   1 run in 30, which is how T0055 was filed. */
void test_blocking_write_waits_out_a_lock_that_clears(void) {
    char err[128] = {0};
    char primary[512];
    slot_path(SLOT_CLEARS, ".json", primary, sizeof primary);

    TEST_ASSERT_TRUE_MESSAGE(
        game_storage_write_blocking(SLOT_CLEARS, "V1", err, (int)sizeof err), err);

    g_held = open_exclusively(primary);
    TEST_ASSERT_TRUE(g_held != INVALID_HANDLE_VALUE);
    HANDLE releaser = CreateThread(NULL, 0, release_held, NULL, 0, NULL);
    if (releaser == NULL) {
        (void)CloseHandle(g_held);
        g_held = INVALID_HANDLE_VALUE;
        TEST_FAIL_MESSAGE("could not start the releaser thread");
    }

    err[0] = '\0';
    const bool wrote = game_storage_write_blocking(SLOT_CLEARS, "V2", err, (int)sizeof err);
    (void)WaitForSingleObject(releaser, INFINITE);
    (void)CloseHandle(releaser);

    TEST_ASSERT_TRUE_MESSAGE(wrote, err);
    assert_slot_reads(SLOT_CLEARS, "V2");
}

/* THE point of the non-blocking mode: the frame gets its thread back. */
void test_non_blocking_write_is_refused_fast_and_keeps_the_primary(void) {
    char err[128] = {0};
    char primary[512];
    char scratch[512];
    slot_path(SLOT_HELD, ".json", primary, sizeof primary);
    slot_path(SLOT_HELD, ".json.tmp", scratch, sizeof scratch);

    TEST_ASSERT_TRUE_MESSAGE(
        game_storage_write_blocking(SLOT_HELD, "V1", err, (int)sizeof err), err);

    HANDLE held = open_exclusively(primary);
    TEST_ASSERT_TRUE(held != INVALID_HANDLE_VALUE);

    err[0] = '\0';
    const ULONGLONG started = GetTickCount64();
    const bool wrote = game_storage_write(SLOT_HELD, "V2", err, (int)sizeof err);
    const ULONGLONG elapsed = GetTickCount64() - started;
    /* Release before judging: the handle is EXCLUSIVE, so an assertion firing
       while it lives would longjmp out with the primary still locked and turn
       every later test red on its own precondition. */
    (void)CloseHandle(held);

    TEST_ASSERT_FALSE(wrote);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, "win32 error 5"), err);
    TEST_ASSERT_TRUE_MESSAGE(elapsed < NO_WAITING_MS,
                             "a refused non-blocking write waited");

    assert_slot_reads(SLOT_HELD, "V1"); /* the refused replace changed nothing */
    FILE *leaked = fopen(scratch, "rb");
    TEST_ASSERT_NULL(leaked);
}

/* The scanner holds the SOURCE -- the exact scenario the diagnosis names. A
   different code, so without this the other half of `refusal_can_clear` would
   have nothing executable behind it. */
void test_a_held_temp_file_is_refused_with_its_own_code(void) {
    char err[128] = {0};
    char scratch[512];
    slot_path(SLOT_TMP_HELD, ".json.tmp", scratch, sizeof scratch);

    TEST_ASSERT_TRUE_MESSAGE(
        game_storage_write_blocking(SLOT_TMP_HELD, "V1", err, (int)sizeof err), err);

    write_raw(scratch, "SCANNER-IS-READING-THIS");
    HANDLE held = open_undeletable(scratch);
    TEST_ASSERT_TRUE(held != INVALID_HANDLE_VALUE);

    err[0] = '\0';
    const ULONGLONG started = GetTickCount64();
    const bool wrote = game_storage_write(SLOT_TMP_HELD, "V2", err, (int)sizeof err);
    const ULONGLONG elapsed = GetTickCount64() - started;
    (void)CloseHandle(held);

    TEST_ASSERT_FALSE(wrote);
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(err, "win32 error 32"), err);
    TEST_ASSERT_TRUE_MESSAGE(elapsed < NO_WAITING_MS,
                             "a refused non-blocking write waited");

    assert_slot_reads(SLOT_TMP_HELD, "V1");
}

/* The blocking mode may wait for a refusal that CLEARS. It must not wait for
   one that cannot: a directory in the way produces the same win32 error 5 and
   will produce it forever.
 *
 * This assertion used to live on a NON-blocking write, where it was trivially
 * satisfied and proved nothing -- the caller-split silently emptied it. Here it
 * is on the only mode that could get it wrong. */
void test_blocking_write_does_not_wait_out_a_permanent_refusal(void) {
    char err[128] = {0};
    char primary[512];
    char marker[512];
    slot_path(SLOT_PERMANENT, ".json", primary, sizeof primary);
    slot_path(SLOT_PERMANENT, ".json/marker.txt", marker, sizeof marker);

    TEST_ASSERT_TRUE(CreateDirectoryA(primary, NULL) ||
                     GetLastError() == ERROR_ALREADY_EXISTS);
    write_raw(marker, "SENTINEL-UNCHANGED");

    err[0] = '\0';
    const ULONGLONG started = GetTickCount64();
    const bool wrote =
        game_storage_write_blocking(SLOT_PERMANENT, "SHOULD-NOT-LAND", err, (int)sizeof err);
    const ULONGLONG elapsed = GetTickCount64() - started;

    TEST_ASSERT_FALSE(wrote);
    TEST_ASSERT_TRUE(strlen(err) > 0);
    TEST_ASSERT_TRUE_MESSAGE(elapsed < NO_WAITING_MS,
                             "a permanent refusal was waited out");

    /* and the obstacle is untouched */
    FILE *file = fopen(marker, "rb");
    TEST_ASSERT_NOT_NULL(file);
    char buffer[64] = {0};
    const size_t read = fread(buffer, 1, sizeof buffer - 1, file);
    buffer[read] = '\0';
    (void)fclose(file);
    TEST_ASSERT_EQUAL_STRING("SENTINEL-UNCHANGED", buffer);
}
#endif /* _WIN32 */

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_both_modes_write_and_read_back);
#ifdef _WIN32
    RUN_TEST(test_blocking_write_waits_out_a_lock_that_clears);
    RUN_TEST(test_non_blocking_write_is_refused_fast_and_keeps_the_primary);
    RUN_TEST(test_a_held_temp_file_is_refused_with_its_own_code);
    RUN_TEST(test_blocking_write_does_not_wait_out_a_permanent_refusal);
#endif
    return UNITY_END();
}
