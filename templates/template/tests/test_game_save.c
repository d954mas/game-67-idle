/* System headers before Unity to avoid noreturn / __declspec conflict on MSVC
   (unity_internals.h pulls in <stdnoreturn.h>). This test drives game_save.c
   through a FAKE fragment (not the generated monolith) so it is self-contained,
   and injects both clocks via game_save__set_clocks_for_test (GAME_SAVE_TESTING). */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
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
#include "game_save.h"
#include "game_state_json.h"
#include "game_storage.h"
#include "cJSON.h"
#include "unity.h"
/* clang-format on */

#define PRIMARY_PATH "build/saves/test_slot.json"
#define PRIMARY_TMP "build/saves/test_slot.json.tmp"
#define BAK_PATH "build/saves/test_slot.bak"
#define BAK_TMP "build/saves/test_slot.bak.tmp"

/* ---- injected clocks ---- */
static int64_t g_mono_ms;
static int64_t g_wall_ms;
static int64_t test_mono(void) { return g_mono_ms; }
static int64_t test_wall(void) { return g_wall_ms; }

/* ---- fake `game` fragment {int coins; char name[32];} ---- */
static int s_frag_coins;
static char s_frag_name[32];

static cJSON *fake_to_json(void) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "coins", (double)s_frag_coins);
    cJSON_AddStringToObject(o, "name", s_frag_name);
    return o;
}
static bool fake_from_json(const cJSON *json, char *err, int cap) {
    if (!cJSON_IsObject(json)) {
        gsj_set_error(err, cap, "game not object");
        return false;
    }
    int coins = s_frag_coins;
    if (!gsj_read_int_range(json, "coins", 0, 1000000000, &coins, err, cap)) {
        return false; /* out-of-range coins => this fragment fails to load (isolation test) */
    }
    s_frag_coins = coins;
    (void)gsj_read_string(json, "name", s_frag_name, sizeof s_frag_name, err, cap);
    return true;
}
static void fake_reset(void) {
    s_frag_coins = 0;
    s_frag_name[0] = '\0';
}
static void fake_on_new_game(void) { s_frag_coins = 100; }

static const GameSaveFragment s_fake_fragment = {
    .id = "game",
    .version = 1,
    .steps = NULL,
    .reset = fake_reset,
    .on_new_game = fake_on_new_game,
    .to_json = fake_to_json,
    .from_json = fake_from_json,
    .reconcile = NULL,
    .get_path_json = NULL,
    .set_path_json = NULL,
    .schema_json = NULL,
};

/* ---- second fragment {int mark;} to prove per-fragment isolation ---- */
static int s_extra_mark;
static cJSON *extra_to_json(void) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "mark", (double)s_extra_mark);
    return o;
}
static bool extra_from_json(const cJSON *json, char *err, int cap) {
    if (!cJSON_IsObject(json)) {
        gsj_set_error(err, cap, "extra not object");
        return false;
    }
    int mark = s_extra_mark;
    if (!gsj_read_int_range(json, "mark", 0, 1000000, &mark, err, cap)) {
        return false;
    }
    s_extra_mark = mark;
    return true;
}
static void extra_reset(void) { s_extra_mark = 0; }
static const GameSaveFragment s_extra_fragment = {
    .id = "extra",
    .version = 1,
    .steps = NULL,
    .reset = extra_reset,
    .on_new_game = NULL,
    .to_json = extra_to_json,
    .from_json = extra_from_json,
    .reconcile = NULL,
    .get_path_json = NULL,
    .set_path_json = NULL,
    .schema_json = NULL,
};

/* ---- identity-ish transform (case toggle: involutive, never emits NUL) ---- */
static char *xf_toggle_case(const char *in, char *err, int cap) {
    (void)err;
    (void)cap;
    const size_t n = strlen(in);
    char *out = (char *)malloc(n + 1U);
    if (!out) {
        return NULL;
    }
    for (size_t i = 0; i < n; i++) {
        const char ch = in[i];
        if (ch >= 'a' && ch <= 'z') {
            out[i] = (char)(ch - 'a' + 'A');
        } else if (ch >= 'A' && ch <= 'Z') {
            out[i] = (char)(ch - 'A' + 'a');
        } else {
            out[i] = ch;
        }
    }
    out[n] = '\0';
    return out;
}
static const game_save_transform_t k_xf = {.id = "case", .encode = xf_toggle_case, .decode = xf_toggle_case};

/* ---- file helpers ---- */
static int sweep_corrupt(bool do_delete) {
    int count = 0;
    const char *prefix = "test_slot.corrupt-";
    const size_t prefix_len = strlen(prefix);
#ifdef _WIN32
    struct _finddata_t fd;
    intptr_t h = _findfirst("build/saves/*", &fd);
    if (h == -1) {
        return 0;
    }
    do {
        if (strncmp(fd.name, prefix, prefix_len) == 0) {
            count++;
            if (do_delete) {
                char p[512];
                (void)snprintf(p, sizeof p, "build/saves/%s", fd.name);
                (void)remove(p);
            }
        }
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
#else
    DIR *d = opendir("build/saves");
    if (!d) {
        return 0;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, prefix, prefix_len) == 0) {
            count++;
            if (do_delete) {
                char p[512];
                (void)snprintf(p, sizeof p, "build/saves/%s", e->d_name);
                (void)remove(p);
            }
        }
    }
    closedir(d);
#endif
    return count;
}

/* First build/saves/test_slot.corrupt-* bare filename; true when one exists. */
static bool first_corrupt_name(char *name, size_t cap) {
    const char *prefix = "test_slot.corrupt-";
    const size_t prefix_len = strlen(prefix);
#ifdef _WIN32
    struct _finddata_t fd;
    intptr_t h = _findfirst("build/saves/*", &fd);
    if (h == -1) {
        return false;
    }
    bool found = false;
    do {
        if (strncmp(fd.name, prefix, prefix_len) == 0) {
            (void)snprintf(name, cap, "%s", fd.name);
            found = true;
            break;
        }
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
    return found;
#else
    DIR *d = opendir("build/saves");
    if (!d) {
        return false;
    }
    struct dirent *e;
    bool found = false;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, prefix, prefix_len) == 0) {
            (void)snprintf(name, cap, "%s", e->d_name);
            found = true;
            break;
        }
    }
    closedir(d);
    return found;
#endif
}

static void cleanup_all(void) {
    (void)remove(PRIMARY_PATH);
    (void)remove(PRIMARY_TMP);
    (void)remove(BAK_PATH);
    (void)remove(BAK_TMP);
    (void)sweep_corrupt(true);
}

static void write_raw(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    TEST_ASSERT_NOT_NULL(f);
    const size_t n = strlen(content);
    TEST_ASSERT_EQUAL_UINT(n, fwrite(content, 1, n, f));
    TEST_ASSERT_EQUAL_INT(0, fclose(f));
}

static char *read_raw(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    (void)fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    (void)fseek(f, 0, SEEK_SET);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    char *buf = (char *)malloc((size_t)sz + 1U);
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

static bool file_present(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

static cJSON *parse_primary(void) {
    char *raw = read_raw(PRIMARY_PATH);
    TEST_ASSERT_NOT_NULL(raw);
    cJSON *doc = cJSON_Parse(raw);
    free(raw);
    TEST_ASSERT_NOT_NULL(doc);
    return doc;
}

static int64_t primary_save_seq(void) {
    cJSON *doc = parse_primary();
    const cJSON *sv = cJSON_GetObjectItemCaseSensitive(doc, "save_seq");
    TEST_ASSERT_TRUE(cJSON_IsNumber(sv));
    const int64_t seq = (int64_t)sv->valuedouble;
    cJSON_Delete(doc);
    return seq;
}

/* Directly make the storage layer produce a valid .bak of the current primary. */
static void make_backup_of_primary(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write_backup("test_slot", err, (int)sizeof err));
}

void setUp(void) {
    cleanup_all();
    s_frag_coins = 0;
    s_frag_name[0] = '\0';
    s_extra_mark = 0;
    g_mono_ms = 5000000;
    g_wall_ms = 1720080000000LL;
    game_save__set_clocks_for_test(test_mono, test_wall);
    game_save_set_transforms(NULL, 0);
    game_save_init();
}
void tearDown(void) { cleanup_all(); }

/* 1. Envelope round trip. */
void test_envelope_round_trip(void) {
    char err[128] = {0};
    fake_reset();
    s_frag_coins = 123;
    (void)snprintf(s_frag_name, sizeof s_frag_name, "%s", "hello");
    game_save_mark_dirty();
    TEST_ASSERT_TRUE(game_save_flush(err, (int)sizeof err));

    cJSON *doc = parse_primary();
    TEST_ASSERT_EQUAL_INT(1, (int)cJSON_GetObjectItemCaseSensitive(doc, "format")->valuedouble);
    TEST_ASSERT_EQUAL_INT(1, (int)cJSON_GetObjectItemCaseSensitive(doc, "save_version")->valuedouble);
    TEST_ASSERT_TRUE((int64_t)cJSON_GetObjectItemCaseSensitive(doc, "saved_at")->valuedouble == g_wall_ms);
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(doc, "save_seq")));
    TEST_ASSERT_EQUAL_STRING("template_test", cJSON_GetObjectItemCaseSensitive(doc, "app")->valuestring);
    const cJSON *game = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(doc, "features"), "game");
    TEST_ASSERT_EQUAL_INT(1, (int)cJSON_GetObjectItemCaseSensitive(game, "v")->valuedouble);
    TEST_ASSERT_EQUAL_INT(123, (int)cJSON_GetObjectItemCaseSensitive(game, "coins")->valuedouble);
    TEST_ASSERT_EQUAL_STRING("hello", cJSON_GetObjectItemCaseSensitive(game, "name")->valuestring);
    cJSON_Delete(doc);

    /* reload restores values */
    s_frag_coins = 0;
    s_frag_name[0] = '\0';
    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_LOADED, r.status);
    TEST_ASSERT_EQUAL_INT(123, s_frag_coins);
    TEST_ASSERT_EQUAL_STRING("hello", s_frag_name);
}

/* 2. on_new_game on FRESH. */
void test_fresh_runs_on_new_game(void) {
    TEST_ASSERT_FALSE(file_present(PRIMARY_PATH));
    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_FRESH, r.status);
    TEST_ASSERT_EQUAL_INT(100, s_frag_coins); /* on_new_game ran */
    TEST_ASSERT_TRUE(file_present(PRIMARY_PATH));
}

/* 3. on_new_game through new_game. */
void test_new_game_runs_on_new_game(void) {
    char err[128] = {0};
    s_frag_coins = 999; /* arbitrary corrupt in-memory state */
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    TEST_ASSERT_EQUAL_INT(100, s_frag_coins);
    TEST_ASSERT_TRUE(file_present(PRIMARY_PATH));
}

/* 4. CORRUPT_RESET: no double on_new_game, quarantine, paused until new_game. */
void test_corrupt_reset_no_on_new_game_then_new_game(void) {
    write_raw(PRIMARY_PATH, "THIS-IS-NOT-JSON");
    write_raw(BAK_PATH, "ALSO-NOT-JSON");

    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_CORRUPT_RESET, r.status);
    TEST_ASSERT_EQUAL_INT(1, sweep_corrupt(false));   /* quarantined */
    TEST_ASSERT_EQUAL_INT(0, s_frag_coins);           /* reset, NOT on_new_game (would be 100) */
    TEST_ASSERT_FALSE(file_present(PRIMARY_PATH));     /* primary quarantined, none rewritten */

    /* autosave paused: dirty + tick must NOT write a primary */
    game_save_mark_dirty();
    g_mono_ms += 100000;
    game_save_tick();
    TEST_ASSERT_FALSE(file_present(PRIMARY_PATH));

    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    TEST_ASSERT_EQUAL_INT(100, s_frag_coins);
    TEST_ASSERT_TRUE(file_present(PRIMARY_PATH));

    /* autosave resumed */
    s_frag_coins = 7;
    game_save_mark_dirty();
    const int64_t before = primary_save_seq();
    g_mono_ms += (int64_t)GAME_SAVE_DEBOUNCE_MS;
    game_save_tick();
    TEST_ASSERT_TRUE(primary_save_seq() > before);
}

/* 5. RECOVERED_BAK + next boot is LOADED and .bak still parses. */
void test_recovered_bak_then_next_boot_loaded(void) {
    char err[128] = {0};
    fake_reset();
    s_frag_coins = 42;
    TEST_ASSERT_TRUE(game_save_flush(err, (int)sizeof err)); /* valid primary, coins=42 */
    make_backup_of_primary();                                /* .bak = copy of good primary */

    write_raw(PRIMARY_PATH, "CORRUPT-PRIMARY"); /* primary now garbage, .bak still good */
    s_frag_coins = 0;

    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_RECOVERED_BAK, r.status);
    TEST_ASSERT_EQUAL_INT(42, s_frag_coins);

    /* primary was rewritten valid AFTER recovery; .bak must still parse */
    char *bakraw = read_raw(BAK_PATH);
    TEST_ASSERT_NOT_NULL(bakraw);
    cJSON *bakdoc = cJSON_Parse(bakraw);
    free(bakraw);
    TEST_ASSERT_NOT_NULL(bakdoc);
    cJSON_Delete(bakdoc);

    /* next boot: primary valid -> LOADED */
    s_frag_coins = 0;
    game_save_load_result_t r2;
    game_save_load(&r2);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_LOADED, r2.status);
    TEST_ASSERT_EQUAL_INT(42, s_frag_coins);
}

/* 6. NEWER (versions only): zero bytes written, export still readable. */
void test_newer_is_read_only(void) {
    const char *newer =
        "{\"format\":1,\"save_version\":1,\"saved_at\":1,\"save_seq\":5,\"app\":\"template_test\","
        "\"features\":{\"game\":{\"v\":2,\"coins\":77,\"name\":\"future\"}}}";
    write_raw(PRIMARY_PATH, newer);
    char *before = read_raw(PRIMARY_PATH);

    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_NEWER, r.status);

    char *after = read_raw(PRIMARY_PATH);
    TEST_ASSERT_EQUAL_STRING(before, after); /* not one byte changed */
    free(before);
    free(after);

    char err[128] = {0};
    char *exp = game_save_export_string(err, (int)sizeof err);
    TEST_ASSERT_NOT_NULL(exp);
    TEST_ASSERT_EQUAL_INT('{', exp[0]);
    free(exp);
}

/* 7. Orphan round trip (§14 п.16): unknown feature key retained through save. */
void test_orphan_round_trip(void) {
    const char *withghost =
        "{\"format\":1,\"save_version\":1,\"saved_at\":1,\"save_seq\":3,\"app\":\"template_test\","
        "\"features\":{\"game\":{\"v\":1,\"coins\":55,\"name\":\"hero\"},"
        "\"extra\":{\"v\":1,\"mark\":9},"
        "\"ghost\":{\"v\":1,\"secret\":\"boo\",\"n\":123}}}";
    write_raw(PRIMARY_PATH, withghost);

    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_LOADED, r.status); /* NOT NEWER, NOT refusal */
    TEST_ASSERT_EQUAL_INT(55, s_frag_coins);
    TEST_ASSERT_EQUAL_INT(9, s_extra_mark);

    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_flush(err, (int)sizeof err));

    cJSON *doc = parse_primary();
    const cJSON *ghost =
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(doc, "features"), "ghost");
    TEST_ASSERT_NOT_NULL(ghost);
    TEST_ASSERT_EQUAL_STRING("boo", cJSON_GetObjectItemCaseSensitive(ghost, "secret")->valuestring);
    TEST_ASSERT_EQUAL_INT(123, (int)cJSON_GetObjectItemCaseSensitive(ghost, "n")->valuedouble);
    cJSON_Delete(doc);
}

/* 8. save_seq monotonic across sessions. */
void test_save_seq_monotonic(void) {
    char err[128] = {0};
    s_frag_coins = 1;
    TEST_ASSERT_TRUE(game_save_flush(err, (int)sizeof err));
    const int64_t n = primary_save_seq();

    game_save_load_result_t r;
    game_save_load(&r); /* restores counter from envelope */
    TEST_ASSERT_TRUE(game_save_flush(err, (int)sizeof err));
    TEST_ASSERT_TRUE(primary_save_seq() > n);
}

/* 9. Debounce boundary on the mono clock. */
void test_debounce_boundary(void) {
    char err[128] = {0};
    const int64_t base = g_mono_ms;
    s_frag_coins = 3;
    TEST_ASSERT_TRUE(game_save_flush(err, (int)sizeof err)); /* baseline last_save_mono = base */
    const int64_t seq0 = primary_save_seq();

    s_frag_coins = 4;
    game_save_mark_dirty(); /* dirty_at = base */

    g_mono_ms = base + (int64_t)GAME_SAVE_DEBOUNCE_MS - 1;
    game_save_tick();
    TEST_ASSERT_TRUE(primary_save_seq() == seq0); /* not yet */

    g_mono_ms = base + (int64_t)GAME_SAVE_DEBOUNCE_MS;
    game_save_tick();
    TEST_ASSERT_TRUE(primary_save_seq() > seq0); /* debounce elapsed -> saved */

    /* marking every tick still saves (via debounce, never "never") */
    s_frag_coins = 5;
    const int64_t seq1 = primary_save_seq();
    const int64_t t0 = g_mono_ms;
    bool saved = false;
    for (int i = 0; i < 40 && !saved; i++) {
        game_save_mark_dirty();
        g_mono_ms += 100;
        game_save_tick();
        saved = primary_save_seq() > seq1;
        TEST_ASSERT_TRUE(g_mono_ms - t0 <= (int64_t)GAME_SAVE_MAX_INTERVAL_MS);
    }
    TEST_ASSERT_TRUE(saved);
}

/* 10. flush is a synchronous force-save (writes even when not dirty / no debounce). */
void test_flush_is_synchronous(void) {
    char err[128] = {0};
    s_frag_coins = 7;
    TEST_ASSERT_FALSE(file_present(PRIMARY_PATH));
    TEST_ASSERT_TRUE(game_save_flush(err, (int)sizeof err)); /* no mark_dirty, no wait */
    TEST_ASSERT_TRUE(file_present(PRIMARY_PATH));

    s_frag_coins = 0;
    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(7, s_frag_coins);
}

/* 11. export/import round trip; garbage import leaves state untouched. */
void test_export_import_round_trip(void) {
    char err[128] = {0};
    s_frag_coins = 321;
    (void)snprintf(s_frag_name, sizeof s_frag_name, "%s", "abc");
    char *exp = game_save_export_string(err, (int)sizeof err);
    TEST_ASSERT_NOT_NULL(exp);

    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err)); /* coins -> 100 */
    TEST_ASSERT_EQUAL_INT(100, s_frag_coins);

    TEST_ASSERT_TRUE(game_save_import_string(exp, err, (int)sizeof err));
    TEST_ASSERT_EQUAL_INT(321, s_frag_coins);
    TEST_ASSERT_EQUAL_STRING("abc", s_frag_name);
    free(exp);

    TEST_ASSERT_FALSE(game_save_import_string("not json at all", err, (int)sizeof err));
    TEST_ASSERT_EQUAL_INT(321, s_frag_coins); /* untouched */
}

/* 12. transform seam: default flat '{'; identity transform -> "NTSV1:" + reload. */
void test_transform_seam(void) {
    char err[128] = {0};
    s_frag_coins = 5;
    game_save_set_transforms(NULL, 0);
    TEST_ASSERT_TRUE(game_save_flush(err, (int)sizeof err));
    char *flat = read_raw(PRIMARY_PATH);
    TEST_ASSERT_EQUAL_INT('{', flat[0]);
    free(flat);

    game_save_set_transforms(&k_xf, 1);
    TEST_ASSERT_TRUE(game_save_flush(err, (int)sizeof err));
    char *enc = read_raw(PRIMARY_PATH);
    TEST_ASSERT_EQUAL_INT(0, strncmp(enc, "NTSV1:", 6));
    free(enc);

    s_frag_coins = 0;
    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_LOADED, r.status);
    TEST_ASSERT_EQUAL_INT(5, s_frag_coins); /* decoded back */
    game_save_set_transforms(NULL, 0);
}

/* 13. A bad fragment does not drop its neighbours (reset_fragments filled). */
void test_bad_fragment_isolation(void) {
    const char *doc =
        "{\"format\":1,\"save_version\":1,\"saved_at\":1,\"save_seq\":1,\"app\":\"template_test\","
        "\"features\":{\"game\":{\"v\":1,\"coins\":2000000000},"  /* out of range -> from_json fails */
        "\"extra\":{\"v\":1,\"mark\":7}}}";
    write_raw(PRIMARY_PATH, doc);
    s_frag_coins = 555;
    s_extra_mark = 0;

    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_LOADED, r.status); /* load did NOT abort */
    TEST_ASSERT_TRUE(r.reset_fragment_count >= 1);
    TEST_ASSERT_EQUAL_STRING("game", r.reset_fragments[0]);
    TEST_ASSERT_EQUAL_INT(0, s_frag_coins); /* bad fragment reset to default */
    TEST_ASSERT_EQUAL_INT(7, s_extra_mark); /* neighbour intact */
}

/* 14. Orphan read-access getters (Q1 follow-up): unknown feature key -> count/at expose the
   retained subtree; out-of-range -> NULL without touching the out-param; cleared by new_game. */
void test_orphan_read_access(void) {
    const char *withghost =
        "{\"format\":1,\"save_version\":1,\"saved_at\":1,\"save_seq\":3,\"app\":\"template_test\","
        "\"features\":{\"game\":{\"v\":1,\"coins\":55,\"name\":\"hero\"},"
        "\"extra\":{\"v\":1,\"mark\":9},"
        "\"ghost\":{\"v\":1,\"secret\":\"boo\",\"n\":123}}}";
    write_raw(PRIMARY_PATH, withghost);

    game_save_load_result_t r;
    game_save_load(&r);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_LOADED, r.status);

    /* exactly one orphan retained: "ghost" (game/extra are registered) */
    TEST_ASSERT_EQUAL_INT(1, game_save_orphan_count());

    const char *id = NULL;
    const cJSON *sub = game_save_orphan_at(0, &id);
    TEST_ASSERT_NOT_NULL(sub);
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_EQUAL_STRING("ghost", id);
    TEST_ASSERT_EQUAL_STRING("boo", cJSON_GetObjectItemCaseSensitive(sub, "secret")->valuestring);
    TEST_ASSERT_EQUAL_INT(123, (int)cJSON_GetObjectItemCaseSensitive(sub, "n")->valuedouble);

    /* out of range -> NULL, out-param left untouched */
    const char sentinel = 'x';
    const char *id2 = &sentinel;
    TEST_ASSERT_NULL(game_save_orphan_at(1, &id2));
    TEST_ASSERT_NULL(game_save_orphan_at(-1, &id2));
    TEST_ASSERT_EQUAL_PTR(&sentinel, id2); /* not written on out-of-range */

    /* new_game clears retained orphans */
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    TEST_ASSERT_EQUAL_INT(0, game_save_orphan_count());
    TEST_ASSERT_NULL(game_save_orphan_at(0, &id));
}

/* 15. Read ERROR (not absence): an UNREADABLE primary must NOT be silently reborn
   as FRESH. The storage layer quarantines the original bytes; the loader takes the
   SAME shape as the classic corrupt path -- reset + autosave paused, NO on_new_game/
   save, primary UNTOUCHED -- and reports CORRUPT_RESET; the shell's explicit new_game
   then overwrites the primary. An oversize primary (> GAME_STORAGE_MAX_BYTES) is a
   readable-bytes read error, so the quarantine copy is byte-identical -- the malloc-
   load-failure data-loss guard (lead 2026-07-07). */
void test_read_error_quarantines_and_corrupt_resets(void) {
    const size_t big = (size_t)(1024 * 1024) + 64u; /* > GAME_STORAGE_MAX_BYTES (1 MiB) -> read error */
    char *payload = (char *)malloc(big + 1u);
    TEST_ASSERT_NOT_NULL(payload);
    memset(payload, 'X', big);
    payload[big] = '\0';
    write_raw(PRIMARY_PATH, payload);

    s_frag_coins = 555;
    game_save_load_result_t r;
    game_save_load(&r);

    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_CORRUPT_RESET, r.status); /* NOT FRESH */
    TEST_ASSERT_EQUAL_INT(0, s_frag_coins); /* reset ONLY -- NO on_new_game (would be 100), like classic */

    /* exactly one quarantine file, byte-identical to the oversize original */
    TEST_ASSERT_EQUAL_INT(1, sweep_corrupt(false));
    char name[256] = {0};
    TEST_ASSERT_TRUE(first_corrupt_name(name, sizeof name));
    char qpath[512];
    (void)snprintf(qpath, sizeof qpath, "build/saves/%s", name);
    char *q = read_raw(qpath);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_INT((int)big, (int)strlen(q));
    TEST_ASSERT_EQUAL_INT(0, memcmp(q, payload, big));
    free(q);

    /* PRIMARY untouched: still the unreadable oversize original (loader did NOT
       overwrite it; the shell's explicit new_game will). */
    char *prim = read_raw(PRIMARY_PATH);
    TEST_ASSERT_NOT_NULL(prim);
    TEST_ASSERT_EQUAL_INT((int)big, (int)strlen(prim));
    TEST_ASSERT_EQUAL_INT(0, memcmp(prim, payload, big));
    free(prim);

    /* autosave paused: dirty + tick past the interval must NOT write -- primary
       stays the oversize original (a save would have shrunk it to a default). */
    game_save_mark_dirty();
    g_mono_ms += 100000;
    game_save_tick();
    prim = read_raw(PRIMARY_PATH);
    TEST_ASSERT_NOT_NULL(prim);
    TEST_ASSERT_EQUAL_INT((int)big, (int)strlen(prim)); /* unchanged -> tick did not save */
    free(prim);
    free(payload);

    /* explicit new_game (the single on_new_game on this path): primary overwritten
       with a fresh, parseable default, autosave resumed. */
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    TEST_ASSERT_EQUAL_INT(100, s_frag_coins); /* NOW on_new_game ran */
    cJSON *doc = parse_primary();
    const cJSON *game =
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(doc, "features"), "game");
    TEST_ASSERT_EQUAL_INT(100, (int)cJSON_GetObjectItemCaseSensitive(game, "coins")->valuedouble);
    cJSON_Delete(doc);

    /* autosave resumed: dirty + debounce -> a save lands */
    s_frag_coins = 7;
    game_save_mark_dirty();
    const int64_t before = primary_save_seq();
    g_mono_ms += (int64_t)GAME_SAVE_DEBOUNCE_MS;
    game_save_tick();
    TEST_ASSERT_TRUE(primary_save_seq() > before);
}

int main(void) {
    game_save_register_fragment(&s_extra_fragment);
    game_save_register_fragment(&s_fake_fragment); /* `game` registered last (§14 п.2) */

    UNITY_BEGIN();
    RUN_TEST(test_envelope_round_trip);
    RUN_TEST(test_fresh_runs_on_new_game);
    RUN_TEST(test_new_game_runs_on_new_game);
    RUN_TEST(test_corrupt_reset_no_on_new_game_then_new_game);
    RUN_TEST(test_recovered_bak_then_next_boot_loaded);
    RUN_TEST(test_newer_is_read_only);
    RUN_TEST(test_orphan_round_trip);
    RUN_TEST(test_save_seq_monotonic);
    RUN_TEST(test_debounce_boundary);
    RUN_TEST(test_flush_is_synchronous);
    RUN_TEST(test_export_import_round_trip);
    RUN_TEST(test_transform_seam);
    RUN_TEST(test_bad_fragment_isolation);
    RUN_TEST(test_orphan_read_access);
    RUN_TEST(test_read_error_quarantines_and_corrupt_resets);
    return UNITY_END();
}
