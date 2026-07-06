/* System headers before Unity to avoid noreturn / __declspec conflict on MSVC
   (unity_internals.h pulls in <stdnoreturn.h>; engine tests/unit follow the
   same convention). io.h's _findfirst/_findnext, not <windows.h>, is enough
   for the directory sweep below. */
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

/* clang-format off */
#include "game_storage.h"
#include "unity.h"
/* clang-format on */

/* Every slot name any test in this file touches. setUp/tearDown sweep exactly
   these so a crash mid-run never leaks files into the next run.
   "replace_fail_slot" is deliberately NOT here -- its primary is a directory,
   not a plain file, and gets its own cleanup (see cleanup_replace_fail_dir). */
static const char *const kAllTestSlots[] = {
    "write_read", "missing_slot", "atomic_slot", "bak_slot", "bak_missing",
    "quarantine_slot", "quarantine_missing", "quarantine_twice_slot",
    "empty_slot", "player1_game", "clean_write_slot", "stale_tmp_no_primary",
    "bak_overwrite_slot",
};
#define GS_TEST_SLOT_COUNT (sizeof(kAllTestSlots) / sizeof(kAllTestSlots[0]))

/* Counts (and optionally deletes) build/saves/ entries starting with prefix.
   Used to assert quarantine leaves exactly one/two <slot>.corrupt-<ts>[-n]
   file(s) -- the timestamp (and now collision) suffix is not known ahead of
   time, so this has to enumerate the directory rather than probe a fixed
   path. When first_match_name is non-NULL, the FIRST match's bare filename
   (not full path) is copied into it -- callers that expect exactly one match
   use this to go read that file's content. */
static int sweep_files_with_prefix(const char *dir, const char *prefix, bool do_delete,
                                    char *first_match_name, size_t first_match_cap) {
    int count = 0;
    const size_t prefix_len = strlen(prefix);
#ifdef _WIN32
    char pattern[512];
    (void)snprintf(pattern, sizeof(pattern), "%s/*", dir);
    struct _finddata_t find_data;
    intptr_t h_find = _findfirst(pattern, &find_data);
    if (h_find == -1) {
        return 0;
    }
    do {
        if (strncmp(find_data.name, prefix, prefix_len) == 0) {
            if (count == 0 && first_match_name && first_match_cap > 0) {
                (void)snprintf(first_match_name, first_match_cap, "%s", find_data.name);
            }
            count++;
            if (do_delete) {
                char full_path[512];
                (void)snprintf(full_path, sizeof(full_path), "%s/%s", dir, find_data.name);
                (void)remove(full_path);
            }
        }
    } while (_findnext(h_find, &find_data) == 0);
    _findclose(h_find);
#else
    DIR *d = opendir(dir);
    if (!d) {
        return 0;
    }
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strncmp(entry->d_name, prefix, prefix_len) == 0) {
            if (count == 0 && first_match_name && first_match_cap > 0) {
                (void)snprintf(first_match_name, first_match_cap, "%s", entry->d_name);
            }
            count++;
            if (do_delete) {
                char full_path[512];
                (void)snprintf(full_path, sizeof(full_path), "%s/%s", dir, entry->d_name);
                (void)remove(full_path);
            }
        }
    }
    closedir(d);
#endif
    return count;
}

static void cleanup_slot(const char *slot) {
    char path[512];
    (void)snprintf(path, sizeof(path), "build/saves/%s.json", slot);
    (void)remove(path);
    (void)snprintf(path, sizeof(path), "build/saves/%s.json.tmp", slot);
    (void)remove(path);
    (void)snprintf(path, sizeof(path), "build/saves/%s.bak", slot);
    (void)remove(path);
    (void)snprintf(path, sizeof(path), "build/saves/%s.bak.tmp", slot);
    (void)remove(path);

    char corrupt_prefix[512];
    (void)snprintf(corrupt_prefix, sizeof(corrupt_prefix), "%s.corrupt-", slot);
    (void)sweep_files_with_prefix("build/saves", corrupt_prefix, true, NULL, 0);
}

static void test_make_dir(const char *path) {
#ifdef _WIN32
    if (_mkdir(path) != 0 && errno != EEXIST) {
        TEST_FAIL_MESSAGE("could not create test directory");
    }
#else
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        TEST_FAIL_MESSAGE("could not create test directory");
    }
#endif
}

/* test_replace_failure_preserves_primary occupies the primary's path with a
   DIRECTORY (not a plain file) to force replace_file() to fail; the generic
   per-slot cleanup above only ever remove()s plain files, so this needs its
   own teardown. */
static void cleanup_replace_fail_dir(void) {
    (void)remove("build/saves/replace_fail_slot.json/marker.txt");
    (void)remove("build/saves/replace_fail_slot.json.tmp");
#ifdef _WIN32
    (void)_rmdir("build/saves/replace_fail_slot.json");
#else
    (void)rmdir("build/saves/replace_fail_slot.json");
#endif
}

static void cleanup_all_test_files(void) {
    for (size_t i = 0; i < GS_TEST_SLOT_COUNT; i++) {
        cleanup_slot(kAllTestSlots[i]);
    }
    cleanup_replace_fail_dir();
}

/* Writes raw bytes directly to a path, bypassing game_storage_write -- used to
   simulate crash leftovers / corruption that the storage layer itself must
   survive (it never produces these states on its own). */
static void write_raw_file(const char *path, const char *content) {
    FILE *file = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(file);
    const size_t len = strlen(content);
    TEST_ASSERT_EQUAL_UINT(len, fwrite(content, 1, len, file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

void setUp(void) { cleanup_all_test_files(); }
void tearDown(void) { cleanup_all_test_files(); }

/* ---- write / read / exists round trip ---- */

void test_write_read_round_trip_and_exists(void) {
    char err[128] = {0};
    TEST_ASSERT_FALSE(game_storage_exists("write_read"));

    TEST_ASSERT_TRUE(game_storage_write("write_read", "{\"a\":1}", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(game_storage_exists("write_read"));

    char *out = NULL;
    TEST_ASSERT_TRUE(game_storage_read("write_read", &out, err, (int)sizeof(err)));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("{\"a\":1}", out);
    free(out);
}

void test_write_read_empty_text(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write("empty_slot", "", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(game_storage_exists("empty_slot"));

    char *out = NULL;
    TEST_ASSERT_TRUE(game_storage_read("empty_slot", &out, err, (int)sizeof(err)));
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQUAL_STRING("", out);
    free(out);
}

void test_read_missing_slot_fails(void) {
    char err[128] = {0};
    char *out = NULL;
    TEST_ASSERT_FALSE(game_storage_exists("missing_slot"));
    TEST_ASSERT_FALSE(game_storage_read("missing_slot", &out, err, (int)sizeof(err)));
    TEST_ASSERT_NULL(out);
    TEST_ASSERT_TRUE(strlen(err) > 0);
}

void test_write_rejects_unsafe_slot(void) {
    char err[128] = {0};
    TEST_ASSERT_FALSE(game_storage_write("bad slot!", "{}", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
    TEST_ASSERT_FALSE(game_storage_write(NULL, "{}", err, (int)sizeof(err)));
}

/* Deep-review item 2: game_storage.h/A2.2 document the slot charset as
   [a-z0-9_-] -- uppercase must be rejected, not silently accepted. */
void test_write_rejects_uppercase_slot(void) {
    char err[128] = {0};
    TEST_ASSERT_FALSE(game_storage_write("Player1", "{}", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
}

/* Deep-review item 4: path-syntax characters must never reach a path/key
   builder. is_safe_segment's charset check runs BEFORE any snprintf, so a
   rejected slot can, by construction, never produce a path outside
   build/saves/ -- this test pins that down against regression for both
   write() and the save_json compat wrapper. */
static const char *const kUnsafeTraversalSlots[] = {
    "..", ".", "a/b", "a\\b", "/abs", "C:\\x", "",
};
#define GS_UNSAFE_TRAVERSAL_COUNT (sizeof(kUnsafeTraversalSlots) / sizeof(kUnsafeTraversalSlots[0]))

void test_write_rejects_path_traversal(void) {
    for (size_t i = 0; i < GS_UNSAFE_TRAVERSAL_COUNT; i++) {
        char err[128] = {0};
        TEST_ASSERT_FALSE(game_storage_write(kUnsafeTraversalSlots[i], "{}", err, (int)sizeof(err)));
        TEST_ASSERT_TRUE(strlen(err) > 0);

        char err2[128] = {0};
        TEST_ASSERT_FALSE(game_storage_save_json(kUnsafeTraversalSlots[i], "game", "{}", err2, (int)sizeof(err2)));
        TEST_ASSERT_TRUE(strlen(err2) > 0);
    }
}

/* ---- crash scenario 1 (§A2.4): a stale/garbage <slot>.json.tmp left behind by
   a crash between "write tmp" and "replace" must stay invisible to read(). ---- */

void test_atomicity_stale_tmp_is_invisible_to_read(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write("atomic_slot", "GOOD-PRIMARY", err, (int)sizeof(err)));

    /* Simulate a crash: a leftover .tmp file sits next to a perfectly good primary. */
    write_raw_file("build/saves/atomic_slot.json.tmp", "GARBAGE-TMP-LEFTOVER");

    char *out = NULL;
    TEST_ASSERT_TRUE(game_storage_read("atomic_slot", &out, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("GOOD-PRIMARY", out);
    free(out);

    /* read() must not have touched the stale tmp file. */
    FILE *tmp_file = fopen("build/saves/atomic_slot.json.tmp", "rb");
    TEST_ASSERT_NOT_NULL(tmp_file);
    char buf[64] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, tmp_file);
    buf[n] = '\0';
    fclose(tmp_file);
    TEST_ASSERT_EQUAL_STRING("GARBAGE-TMP-LEFTOVER", buf);

    /* A stale tmp must not block a later legitimate write+replace either. */
    TEST_ASSERT_TRUE(game_storage_write("atomic_slot", "SECOND-GOOD", err, (int)sizeof(err)));
    out = NULL;
    TEST_ASSERT_TRUE(game_storage_read("atomic_slot", &out, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("SECOND-GOOD", out);
    free(out);
}

/* Deep-review item 8: the crash can also happen on a slot's FIRST-EVER write --
   only a stray .tmp exists, there is no primary at all yet. read() must fail
   cleanly (not resurrect the tmp as if it were primary), and a subsequent
   write must still produce a valid primary. */
void test_stale_tmp_with_absent_primary_recovers(void) {
    char err[128] = {0};
    write_raw_file("build/saves/stale_tmp_no_primary.json.tmp", "PARTIAL-WRITE-FROM-CRASHED-SESSION");

    TEST_ASSERT_FALSE(game_storage_exists("stale_tmp_no_primary"));
    char *out = NULL;
    TEST_ASSERT_FALSE(game_storage_read("stale_tmp_no_primary", &out, err, (int)sizeof(err)));
    TEST_ASSERT_NULL(out);

    TEST_ASSERT_TRUE(game_storage_write("stale_tmp_no_primary", "RECOVERED-GOOD", err, (int)sizeof(err)));
    out = NULL;
    TEST_ASSERT_TRUE(game_storage_read("stale_tmp_no_primary", &out, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("RECOVERED-GOOD", out);
    free(out);
}

/* Deep-review item 6: a plain, uneventful write must not leak its .tmp scratch
   file -- replace_file() either consumes it (rename/MoveFileEx) or write()
   removes it on the error path; either way nothing named "<slot>.json.tmp"
   should survive a successful write. */
void test_no_leftover_tmp_after_clean_write(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write("clean_write_slot", "{\"a\":1}", err, (int)sizeof(err)));
    FILE *tmp_file = fopen("build/saves/clean_write_slot.json.tmp", "rb");
    TEST_ASSERT_NULL(tmp_file);
}

/* Deep-review item 3: replace_file() itself can fail (disk full, AV lock, or
   here: the target is occupied by something that categorically cannot be
   replaced by a file). write() must fail cleanly: error set, nothing at the
   target changed, and the .tmp scratch file must not leak either. A
   non-empty directory at the primary's path is a reliable, portable way to
   make MoveFileEx(REPLACE_EXISTING)/rename() refuse the replace without
   needing to simulate disk-full or ACL errors. */
void test_replace_failure_preserves_primary(void) {
    char err[128] = {0};
    test_make_dir("build/saves");
    test_make_dir("build/saves/replace_fail_slot.json");
    write_raw_file("build/saves/replace_fail_slot.json/marker.txt", "SENTINEL-UNCHANGED");

    TEST_ASSERT_FALSE(game_storage_write("replace_fail_slot", "NEW-CONTENT-SHOULD-NOT-LAND", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);

    /* target untouched: the marker inside the directory is exactly as written */
    FILE *marker_file = fopen("build/saves/replace_fail_slot.json/marker.txt", "rb");
    TEST_ASSERT_NOT_NULL(marker_file);
    char buf[64] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, marker_file);
    buf[n] = '\0';
    fclose(marker_file);
    TEST_ASSERT_EQUAL_STRING("SENTINEL-UNCHANGED", buf);

    /* no leaked .tmp scratch file */
    FILE *tmp_file = fopen("build/saves/replace_fail_slot.json.tmp", "rb");
    TEST_ASSERT_NULL(tmp_file);
}

/* ---- crash scenario 2 (§A2.4): primary corrupted after a known-good backup;
   read() still returns the (garbage) primary bytes -- storage does not validate
   JSON, that is game_save's job -- but read_backup() returns last-known-good. ---- */

void test_backup_fallback_survives_corrupted_primary(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write("bak_slot", "GOOD-V1", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(game_storage_write_backup("bak_slot", err, (int)sizeof(err)));

    /* Simulate corruption: overwrite primary directly with non-JSON bytes. */
    write_raw_file("build/saves/bak_slot.json", "CORRUPT-NOT-JSON");

    char *primary_out = NULL;
    TEST_ASSERT_TRUE(game_storage_read("bak_slot", &primary_out, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("CORRUPT-NOT-JSON", primary_out);
    free(primary_out);

    char *bak_out = NULL;
    TEST_ASSERT_TRUE(game_storage_read_backup("bak_slot", &bak_out, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("GOOD-V1", bak_out);
    free(bak_out);
}

void test_write_backup_noop_when_primary_missing(void) {
    char err[128] = {0};
    TEST_ASSERT_FALSE(game_storage_exists("bak_missing"));
    TEST_ASSERT_TRUE(game_storage_write_backup("bak_missing", err, (int)sizeof(err)));

    char *out = NULL;
    TEST_ASSERT_FALSE(game_storage_read_backup("bak_missing", &out, err, (int)sizeof(err)));
    TEST_ASSERT_NULL(out);
}

/* Deep-review item 9: a second, later backup must overwrite the first (not
   append/ignore), and a leftover .bak.tmp (crash mid-way through a PRIOR
   write_backup) must not block a fresh one. */
void test_write_backup_overwrites_existing_bak(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write("bak_overwrite_slot", "V1", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(game_storage_write_backup("bak_overwrite_slot", err, (int)sizeof(err)));

    char *bak_out = NULL;
    TEST_ASSERT_TRUE(game_storage_read_backup("bak_overwrite_slot", &bak_out, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("V1", bak_out);
    free(bak_out);

    TEST_ASSERT_TRUE(game_storage_write("bak_overwrite_slot", "V2", err, (int)sizeof(err)));

    write_raw_file("build/saves/bak_overwrite_slot.bak.tmp", "GARBAGE-BAK-TMP-LEFTOVER");

    TEST_ASSERT_TRUE(game_storage_write_backup("bak_overwrite_slot", err, (int)sizeof(err)));

    bak_out = NULL;
    TEST_ASSERT_TRUE(game_storage_read_backup("bak_overwrite_slot", &bak_out, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("V2", bak_out);
    free(bak_out);
}

/* ---- crash scenario 3 (§A2.4): quarantine removes the corrupt primary and
   leaves exactly one <slot>.corrupt-<unix_ms> file behind, holding the
   ORIGINAL primary bytes (forensics, P10/§14 p.14). ---- */

void test_quarantine_moves_primary_and_leaves_exactly_one_corrupt_file(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write("quarantine_slot", "TO-BE-QUARANTINED", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(game_storage_exists("quarantine_slot"));

    TEST_ASSERT_TRUE(game_storage_quarantine("quarantine_slot", err, (int)sizeof(err)));

    TEST_ASSERT_FALSE(game_storage_exists("quarantine_slot"));
    char *out = NULL;
    TEST_ASSERT_FALSE(game_storage_read("quarantine_slot", &out, err, (int)sizeof(err)));
    TEST_ASSERT_NULL(out);

    char corrupt_name[256] = {0};
    TEST_ASSERT_EQUAL_INT(1, sweep_files_with_prefix("build/saves", "quarantine_slot.corrupt-", false,
                                                      corrupt_name, sizeof(corrupt_name)));

    char corrupt_path[512];
    (void)snprintf(corrupt_path, sizeof(corrupt_path), "build/saves/%s", corrupt_name);
    FILE *corrupt_file = fopen(corrupt_path, "rb");
    TEST_ASSERT_NOT_NULL(corrupt_file);
    char buf[64] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, corrupt_file);
    buf[n] = '\0';
    fclose(corrupt_file);
    TEST_ASSERT_EQUAL_STRING("TO-BE-QUARANTINED", buf);
}

void test_quarantine_without_primary_fails(void) {
    char err[128] = {0};
    TEST_ASSERT_FALSE(game_storage_exists("quarantine_missing"));
    TEST_ASSERT_FALSE(game_storage_quarantine("quarantine_missing", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(strlen(err) > 0);
    TEST_ASSERT_EQUAL_INT(0, sweep_files_with_prefix("build/saves", "quarantine_missing.corrupt-", false, NULL, 0));
}

/* Deep-review item 1 (real defect, now fixed): two quarantines of the SAME
   slot inside one clock tick used to collide on the same "<slot>.corrupt-
   <unix_ms>" name -- POSIX rename() silently clobbered the first .corrupt
   (forensics lost), Windows failed outright. Both quarantines must now keep
   their own file. */
void test_quarantine_twice_same_slot(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write("quarantine_twice_slot", "FIRST-BAD", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(game_storage_quarantine("quarantine_twice_slot", err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_INT(1, sweep_files_with_prefix("build/saves", "quarantine_twice_slot.corrupt-", false, NULL, 0));

    TEST_ASSERT_TRUE(game_storage_write("quarantine_twice_slot", "SECOND-BAD", err, (int)sizeof(err)));
    TEST_ASSERT_TRUE(game_storage_quarantine("quarantine_twice_slot", err, (int)sizeof(err)));

    TEST_ASSERT_EQUAL_INT(2, sweep_files_with_prefix("build/saves", "quarantine_twice_slot.corrupt-", false, NULL, 0));
}

/* ---- probe (native path is trivially true; the real check is web-only) ---- */

void test_probe_native_always_true(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_probe(err, (int)sizeof(err)));
}

/* ---- compat wrappers (§A2.6 risk: document->slot folding must not break the
   save/load round trip generated game_state_devapi.c depends on). ---- */

void test_compat_wrappers_round_trip(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_save_json("player1", "game", "{\"level\":3}", err, (int)sizeof(err)));

    char *out = NULL;
    TEST_ASSERT_TRUE(game_storage_load_json("player1", "game", &out, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("{\"level\":3}", out);
    free(out);

    char resolved[256] = {0};
    TEST_ASSERT_TRUE(game_storage_resolve_key("player1", "game", resolved, (int)sizeof(resolved), err, (int)sizeof(err)));

    /* resolve_key must point at the EXACT file save_json wrote, not merely
       return a non-empty string. */
    FILE *resolved_file = fopen(resolved, "rb");
    TEST_ASSERT_NOT_NULL(resolved_file);
    if (resolved_file) {
        fclose(resolved_file);
    }
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_write_read_round_trip_and_exists);
    RUN_TEST(test_write_read_empty_text);
    RUN_TEST(test_read_missing_slot_fails);
    RUN_TEST(test_write_rejects_unsafe_slot);
    RUN_TEST(test_write_rejects_uppercase_slot);
    RUN_TEST(test_write_rejects_path_traversal);

    RUN_TEST(test_atomicity_stale_tmp_is_invisible_to_read);
    RUN_TEST(test_stale_tmp_with_absent_primary_recovers);
    RUN_TEST(test_no_leftover_tmp_after_clean_write);
    RUN_TEST(test_replace_failure_preserves_primary);

    RUN_TEST(test_backup_fallback_survives_corrupted_primary);
    RUN_TEST(test_write_backup_noop_when_primary_missing);
    RUN_TEST(test_write_backup_overwrites_existing_bak);

    RUN_TEST(test_quarantine_moves_primary_and_leaves_exactly_one_corrupt_file);
    RUN_TEST(test_quarantine_without_primary_fails);
    RUN_TEST(test_quarantine_twice_same_slot);

    RUN_TEST(test_probe_native_always_true);

    RUN_TEST(test_compat_wrappers_round_trip);

    return UNITY_END();
}
