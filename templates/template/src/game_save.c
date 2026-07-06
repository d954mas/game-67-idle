/* game_save.c — hand-written L0 save orchestrator (§A3).
   Fragment registry + single atomic envelope document + load state machine
   (FRESH/LOADED/RECOVERED_BAK/CORRUPT_RESET/NEWER, per-fragment, never
   all-or-nothing) + on_new_game + dirty/debounce/MAX_INTERVAL + synchronous web
   visibility-flush + export/import + empty transform seam. Single thread. */

#include "game_save.h"

#include "game_state_json.h"
#include "game_storage.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform clocks: the game target links nt_time (native) / emscripten (web);
   the ctest build sets GAME_SAVE_TESTING and injects both clocks, so it must not
   reference either symbol. Keep the platform includes out of the test build. */
#if !defined(GAME_SAVE_TESTING)
#  if defined(__EMSCRIPTEN__)
#    include <emscripten.h>
#  else
#    include <time.h>
#    include "time/nt_time.h"
#  endif
#endif

/* CMake supplies these for the game/test targets; guarded defaults keep the file
   self-contained (§A3.2). */
#ifndef GAME_SAVE_AUTOSAVE_SLOT
#define GAME_SAVE_AUTOSAVE_SLOT "autosave"
#endif
#ifndef GAME_SAVE_DEBOUNCE_MS
#define GAME_SAVE_DEBOUNCE_MS 2000
#endif
#ifndef GAME_SAVE_MAX_INTERVAL_MS
#define GAME_SAVE_MAX_INTERVAL_MS 30000
#endif
#ifndef GAME_SAVE_DOC_VERSION
#define GAME_SAVE_DOC_VERSION 1
#endif
#ifndef GAME_STORAGE_APP_ID
#define GAME_STORAGE_APP_ID "template"
#endif

/* Internal constants (§A3.2). */
#define GAME_SAVE_FORMAT 1
#ifndef GAME_SAVE_BUILD
#define GAME_SAVE_BUILD "0"
#endif

#define GAME_SAVE_TRANSFORM_PREFIX "NTSV1:"
#define GAME_SAVE_TRANSFORM_PREFIX_LEN 6

/* ---- module state (single thread) ---- */

static const GameSaveFragment *s_fragments[GAME_SAVE_MAX_FRAGMENTS];
static int s_fragment_count;

typedef struct {
    char  *id;
    cJSON *subtree;
} game_save_orphan_t;
static game_save_orphan_t s_orphans[GAME_SAVE_MAX_FRAGMENTS];
static int s_orphan_count;

static const game_save_transform_t *s_transforms;
static int s_transform_count;

static bool    s_autosave_paused; /* CORRUPT_RESET/NEWER until game_save_new_game */
static bool    s_dirty;
static bool    s_unpersisted;
static int64_t s_dirty_at;        /* mono ms of the first mark after clean (§14 п.6) */
static int64_t s_last_save_mono;  /* mono ms of the last successful save */
static int64_t s_last_saved_at;   /* wall ms stamped into the last save/load */
static int64_t s_save_seq;        /* monotonic counter, restored from the loaded envelope */

static int64_t (*s_mono_clock)(void);
static int64_t (*s_wall_clock)(void);

/* ---- platform default clocks ---- */

#if defined(GAME_SAVE_TESTING)
static int64_t default_mono_ms(void) { return 0; }
static int64_t default_wall_ms(void) { return 0; }
#elif defined(__EMSCRIPTEN__)
static int64_t default_mono_ms(void) { return (int64_t)emscripten_get_now(); }
/* clang-format off */
EM_JS(double, game_save_web_now_ms, (void), { return Date.now(); })
/* clang-format on */
static int64_t default_wall_ms(void) { return (int64_t)game_save_web_now_ms(); }
#else
static int64_t default_mono_ms(void) { return (int64_t)(nt_time_now() * 1000.0); }
static int64_t default_wall_ms(void) { return (int64_t)time(NULL) * 1000; }
#endif

static int64_t mono_now(void) { return s_mono_clock ? s_mono_clock() : 0; }
static int64_t wall_now(void) { return s_wall_clock ? s_wall_clock() : 0; }

/* ---- small helpers ---- */

/* Own allocation (strdup is POSIX, not C17: -Wpedantic + -Werror would reject it). */
static char *dup_string(const char *s) {
    if (!s) {
        return NULL;
    }
    const size_t n = strlen(s) + 1U;
    char *p = (char *)malloc(n);
    if (p) {
        memcpy(p, s, n);
    }
    return p;
}

static void set_message(game_save_load_result_t *r, const char *m) {
    (void)snprintf(r->message, sizeof r->message, "%s", m);
}

static bool is_registered(const char *id) {
    for (int i = 0; i < s_fragment_count; i++) {
        if (strcmp(s_fragments[i]->id, id) == 0) {
            return true;
        }
    }
    return false;
}

static int read_frag_version(const cJSON *frag) {
    const cJSON *vj = gsj_object_item(frag, "v");
    if (cJSON_IsNumber(vj)) {
        return (int)vj->valuedouble;
    }
    return 1; /* absent -> v1 (§A3.4 п.6) */
}

/* ---- orphan retention (§14 п.16 / §A3.3) ---- */

static void free_orphans(void) {
    for (int i = 0; i < s_orphan_count; i++) {
        free(s_orphans[i].id);
        cJSON_Delete(s_orphans[i].subtree);
        s_orphans[i].id = NULL;
        s_orphans[i].subtree = NULL;
    }
    s_orphan_count = 0;
}

static void capture_orphans(const cJSON *features) {
    free_orphans();
    if (!cJSON_IsObject(features)) {
        return;
    }
    for (const cJSON *child = features->child; child; child = child->next) {
        const char *key = child->string;
        if (!key || is_registered(key)) {
            continue;
        }
        if (s_orphan_count >= GAME_SAVE_MAX_FRAGMENTS) {
            fprintf(stderr, "game_save: too many orphan fragments, dropping '%s'\n", key);
            continue;
        }
        char *id = dup_string(key);
        cJSON *sub = cJSON_Duplicate(child, true);
        if (!id || !sub) {
            free(id);
            cJSON_Delete(sub);
            continue;
        }
        s_orphans[s_orphan_count].id = id;
        s_orphans[s_orphan_count].subtree = sub;
        s_orphan_count++;
        fprintf(stderr, "game_save: retaining orphan fragment '%s' (no registered handler)\n", key);
    }
}

/* ---- transform seam (§14 п.15 / §A3.9). Default chain empty -> flat '{' JSON. ---- */

static char *transform_encode(const char *flat, char *error, int error_cap) {
    if (s_transform_count <= 0) {
        return dup_string(flat);
    }
    char *cur = dup_string(flat);
    if (!cur) {
        return NULL;
    }
    for (int i = 0; i < s_transform_count; i++) {
        if (!s_transforms[i].encode) {
            continue;
        }
        char *next = s_transforms[i].encode(cur, error, error_cap);
        free(cur);
        if (!next) {
            return NULL;
        }
        cur = next;
    }
    size_t ids_len = 0;
    for (int i = 0; i < s_transform_count; i++) {
        ids_len += (s_transforms[i].id ? strlen(s_transforms[i].id) : 0U) + 1U; /* +comma/terminator */
    }
    const size_t total = GAME_SAVE_TRANSFORM_PREFIX_LEN + ids_len + 1U + strlen(cur) + 1U;
    char *out = (char *)malloc(total);
    if (!out) {
        free(cur);
        gsj_set_error(error, error_cap, "failed to allocate transformed save");
        return NULL;
    }
    size_t pos = 0;
    memcpy(out + pos, GAME_SAVE_TRANSFORM_PREFIX, GAME_SAVE_TRANSFORM_PREFIX_LEN);
    pos += GAME_SAVE_TRANSFORM_PREFIX_LEN;
    for (int i = 0; i < s_transform_count; i++) {
        const char *id = s_transforms[i].id ? s_transforms[i].id : "";
        const size_t n = strlen(id);
        memcpy(out + pos, id, n);
        pos += n;
        if (i + 1 < s_transform_count) {
            out[pos++] = ',';
        }
    }
    out[pos++] = ':';
    const size_t cur_len = strlen(cur);
    memcpy(out + pos, cur, cur_len);
    pos += cur_len;
    out[pos] = '\0';
    free(cur);
    return out;
}

static char *transform_decode(const char *raw, char *error, int error_cap) {
    if (!raw) {
        return NULL;
    }
    if (strncmp(raw, GAME_SAVE_TRANSFORM_PREFIX, GAME_SAVE_TRANSFORM_PREFIX_LEN) != 0) {
        return dup_string(raw); /* plain hand-edited JSON always loads (§A3.9) */
    }
    const char *colon = strchr(raw + GAME_SAVE_TRANSFORM_PREFIX_LEN, ':');
    if (!colon) {
        gsj_set_error(error, error_cap, "malformed transform header");
        return NULL;
    }
    char *cur = dup_string(colon + 1);
    if (!cur) {
        return NULL;
    }
    for (int i = s_transform_count - 1; i >= 0; i--) {
        if (!s_transforms[i].decode) {
            continue;
        }
        char *next = s_transforms[i].decode(cur, error, error_cap);
        free(cur);
        if (!next) {
            return NULL;
        }
        cur = next;
    }
    return cur;
}

/* ---- fragment fan-out ---- */

static void reset_all(void) {
    for (int i = 0; i < s_fragment_count; i++) {
        if (s_fragments[i]->reset) {
            s_fragments[i]->reset();
        }
    }
}

static void on_new_game_all(void) {
    for (int i = 0; i < s_fragment_count; i++) {
        if (s_fragments[i]->on_new_game) {
            s_fragments[i]->on_new_game();
        }
    }
}

static void reconcile_all(void) {
    for (int i = 0; i < s_fragment_count; i++) {
        if (s_fragments[i]->reconcile) {
            s_fragments[i]->reconcile();
        }
    }
}

/* ---- envelope build + save (§A3.5) ---- */

static cJSON *build_root(bool bump_seq, int64_t *out_wall) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    const int64_t wall = wall_now();
    const int64_t seq = bump_seq ? ++s_save_seq : s_save_seq;
    cJSON_AddNumberToObject(root, "format", (double)GAME_SAVE_FORMAT);
    cJSON_AddNumberToObject(root, "save_version", (double)GAME_SAVE_DOC_VERSION);
    cJSON_AddNumberToObject(root, "saved_at", (double)wall);
    cJSON_AddNumberToObject(root, "save_seq", (double)seq);
    cJSON_AddStringToObject(root, "app", GAME_STORAGE_APP_ID);
    cJSON_AddStringToObject(root, "build", GAME_SAVE_BUILD);

    cJSON *features = cJSON_CreateObject();
    if (!features) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON_AddItemToObject(root, "features", features);

    for (int i = 0; i < s_fragment_count; i++) {
        const GameSaveFragment *frag = s_fragments[i];
        cJSON *payload = frag->to_json ? frag->to_json() : NULL;
        if (!payload) {
            payload = cJSON_CreateObject();
        }
        cJSON_AddNumberToObject(payload, "v", (double)frag->version); /* shell stamps "v" */
        cJSON_AddItemToObject(features, frag->id, payload);
    }
    /* retained orphans, verbatim, AFTER registered fragments (§A3.3) */
    for (int i = 0; i < s_orphan_count; i++) {
        cJSON *dup = cJSON_Duplicate(s_orphans[i].subtree, true);
        if (dup) {
            cJSON_AddItemToObject(features, s_orphans[i].id, dup);
        }
    }
    if (out_wall) {
        *out_wall = wall;
    }
    return root;
}

static bool save_internal(char *error, int error_cap) {
    int64_t wall = 0;
    cJSON *root = build_root(true, &wall);
    if (!root) {
        gsj_set_error(error, error_cap, "failed to build save document");
        return false;
    }
    char *flat = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!flat) {
        gsj_set_error(error, error_cap, "failed to serialize save document");
        return false;
    }
    char *encoded = transform_encode(flat, error, error_cap);
    free(flat);
    if (!encoded) {
        return false;
    }
    const bool ok = game_storage_write(GAME_SAVE_AUTOSAVE_SLOT, encoded, error, error_cap);
    free(encoded);
    if (ok) {
        s_dirty = false;
        s_last_save_mono = mono_now();
        s_last_saved_at = wall;
        s_unpersisted = false;
    } else {
#if defined(__EMSCRIPTEN__)
        s_unpersisted = true; /* quota / Safari-private -> SAVE_UNPERSISTED (§14 п.3) */
#endif
        /* dirty stays set: retry on the next tick (§A3.5). */
    }
    return ok;
}

/* ---- load state machine (§A3.4) ---- */

static bool doc_is_newer(const cJSON *doc) {
    const cJSON *fj = gsj_object_item(doc, "format");
    const int format = cJSON_IsNumber(fj) ? (int)fj->valuedouble : GAME_SAVE_FORMAT;
    if (format > GAME_SAVE_FORMAT) {
        return true;
    }
    const cJSON *svj = gsj_object_item(doc, "save_version");
    const int save_version = cJSON_IsNumber(svj) ? (int)svj->valuedouble : GAME_SAVE_DOC_VERSION;
    if (save_version > GAME_SAVE_DOC_VERSION) {
        return true;
    }
    const cJSON *features = gsj_object_item(doc, "features");
    for (int i = 0; i < s_fragment_count; i++) {
        const cJSON *frag = gsj_object_item(features, s_fragments[i]->id);
        if (!frag) {
            continue;
        }
        if (read_frag_version(frag) > s_fragments[i]->version) {
            return true;
        }
    }
    return false; /* unknown feature keys are orphans, NOT NEWER (§A3.4 п.3) */
}

static bool run_migration_steps(const GameSaveFragment *frag, int from_v, cJSON *copy, char *err, int cap) {
    for (int v = from_v; v < frag->version; v++) {
        GameSaveMigrateFn fn = (frag->steps != NULL) ? frag->steps[v - 1] : NULL; /* steps[v-1]: v -> v+1 */
        if (fn == NULL) {
            gsj_set_error(err, cap, "missing migration step");
            return false;
        }
        if (!fn(copy, err, cap)) {
            return false;
        }
    }
    return true;
}

static void record_reset(game_save_load_result_t *r, const char *id) {
    if (r->reset_fragment_count < GAME_SAVE_MAX_FRAGMENTS) {
        r->reset_fragments[r->reset_fragment_count++] = id;
    }
}

/* Steps 4-7: restore counters, load each fragment independently (never
   all-or-nothing), retain orphans, reconcile. */
static void load_from_doc(const cJSON *doc, game_save_load_result_t *result) {
    int64_t seq = 0;
    (void)gsj_read_i64(doc, "save_seq", 0, INT64_MAX, &seq, NULL, 0);
    s_save_seq = seq; /* next save > loaded (monotonic across sessions, §A3.4 п.4) */

    int64_t saved_at = 0;
    (void)gsj_read_i64(doc, "saved_at", 0, INT64_MAX, &saved_at, NULL, 0);
    s_last_saved_at = saved_at;

    /* Step 5: cross-fragment doc steps — DOC_VERSION=1, none in A3 (seam present). */

    const cJSON *features = gsj_object_item(doc, "features");
    for (int i = 0; i < s_fragment_count; i++) {
        const GameSaveFragment *frag = s_fragments[i];
        const cJSON *f = gsj_object_item(features, frag->id);
        if (!f) {
            if (frag->reset) {
                frag->reset(); /* new feature != migration; on_new_game NOT called */
            }
            continue;
        }
        char ferr[128];
        ferr[0] = '\0';
        const int v = read_frag_version(f);
        bool ok;
        if (v < frag->version) {
            cJSON *copy = cJSON_Duplicate(f, true);
            ok = (copy != NULL) && run_migration_steps(frag, v, copy, ferr, (int)sizeof ferr) &&
                 (frag->from_json != NULL) && frag->from_json(copy, ferr, (int)sizeof ferr);
            cJSON_Delete(copy);
        } else {
            /* v == frag->version; v > frag->version is unreachable — NEWER preempts (§A3.4 п.6). */
            ok = (frag->from_json != NULL) && frag->from_json(f, ferr, (int)sizeof ferr);
        }
        if (!ok) {
            if (frag->reset) {
                frag->reset();
            }
            record_reset(result, frag->id);
            fprintf(stderr, "game_save: fragment '%s' failed to load, reset to defaults (%s)\n",
                    frag->id, ferr);
        }
    }

    capture_orphans(features);
    reconcile_all();
}

void game_save_load(game_save_load_result_t *result) {
    game_save_load_result_t local;
    if (!result) {
        result = &local;
    }
    memset(result, 0, sizeof *result);
    result->status = GAME_SAVE_LOAD_FRESH;

    free_orphans();

    char err[128];
    err[0] = '\0';

    char *text = NULL;
    if (!game_storage_read(GAME_SAVE_AUTOSAVE_SLOT, &text, err, (int)sizeof err)) {
        /* No save -> FRESH: reset + on_new_game + save (§A3.4 п.1). */
        reset_all();
        on_new_game_all();
        s_autosave_paused = false;
        (void)save_internal(err, (int)sizeof err);
        result->status = GAME_SAVE_LOAD_FRESH;
        set_message(result, "no save found; new game");
        s_last_save_mono = mono_now();
        return;
    }
    char *decoded = transform_decode(text, err, (int)sizeof err);
    free(text);
    cJSON *doc = decoded ? cJSON_Parse(decoded) : NULL;
    free(decoded);

    if (doc && cJSON_IsObject(doc)) {
        if (doc_is_newer(doc)) {
            /* NEWER: zero writes, autosave paused, read/export still available (§A3.4 п.3). */
            cJSON_Delete(doc);
            s_autosave_paused = true;
            result->status = GAME_SAVE_LOAD_NEWER;
            set_message(result, "save is newer than this build; read-only");
            s_last_save_mono = mono_now();
            return;
        }
        /* LOADED (§A3.4 п.6-8). */
        load_from_doc(doc, result);
        cJSON_Delete(doc);
        s_autosave_paused = false;
        result->status = GAME_SAVE_LOAD_LOADED;
        set_message(result, "loaded");
        (void)game_storage_write_backup(GAME_SAVE_AUTOSAVE_SLOT, err, (int)sizeof err); /* last-known-good */
        s_last_save_mono = mono_now();
        return;
    }
    if (doc) {
        cJSON_Delete(doc);
    }

    /* Primary unparseable -> try backup (§A3.4 п.2). */
    char *baktext = NULL;
    cJSON *bakdoc = NULL;
    if (game_storage_read_backup(GAME_SAVE_AUTOSAVE_SLOT, &baktext, err, (int)sizeof err)) {
        char *bakdec = transform_decode(baktext, err, (int)sizeof err);
        free(baktext);
        bakdoc = bakdec ? cJSON_Parse(bakdec) : NULL;
        free(bakdec);
    }
    if (bakdoc && cJSON_IsObject(bakdoc) && !doc_is_newer(bakdoc)) {
        /* RECOVERED_BAK: load from bak, then rewrite primary, THEN write_backup
           (order matters: primary is currently corrupt — backing it up first
           would clobber the live .bak with garbage, §A3.4 п.8). */
        load_from_doc(bakdoc, result);
        cJSON_Delete(bakdoc);
        s_autosave_paused = false;
        result->status = GAME_SAVE_LOAD_RECOVERED_BAK;
        set_message(result, "primary corrupt; recovered from backup");
        (void)save_internal(err, (int)sizeof err);
        (void)game_storage_write_backup(GAME_SAVE_AUTOSAVE_SLOT, err, (int)sizeof err);
        s_last_save_mono = mono_now();
        return;
    }
    if (bakdoc) {
        cJSON_Delete(bakdoc);
    }

    /* CORRUPT_RESET: quarantine + reset only. No on_new_game, no save — the shell
       waits for the player's explicit new_game (Р10). Autosave paused. */
    (void)game_storage_quarantine(GAME_SAVE_AUTOSAVE_SLOT, err, (int)sizeof err);
    reset_all();
    s_autosave_paused = true;
    result->status = GAME_SAVE_LOAD_CORRUPT_RESET;
    set_message(result, "primary and backup corrupt; quarantined, awaiting new game");
    s_last_save_mono = mono_now();
}

/* ---- public API ---- */

void game_save_register_fragment(const GameSaveFragment *fragment) {
    if (!fragment || s_fragment_count >= GAME_SAVE_MAX_FRAGMENTS) {
        return;
    }
    s_fragments[s_fragment_count++] = fragment;
}

/* ---- registry read-access for the DevAPI dispatch (§A5.3). Additive, read-only,
   behaviourally inert view of the static registry. ---- */

int game_save_fragment_count(void) { return s_fragment_count; }

const GameSaveFragment *game_save_fragment_at(int index) {
    return (index >= 0 && index < s_fragment_count) ? s_fragments[index] : NULL;
}

const GameSaveFragment *game_save_find_fragment(const char *id) {
    if (!id) {
        return NULL;
    }
    for (int i = 0; i < s_fragment_count; i++) {
        if (strcmp(s_fragments[i]->id, id) == 0) {
            return s_fragments[i];
        }
    }
    return NULL;
}

void game_save_init(void) {
    if (!s_mono_clock) {
        s_mono_clock = default_mono_ms;
    }
    if (!s_wall_clock) {
        s_wall_clock = default_wall_ms;
    }
    s_dirty = false;
    s_autosave_paused = false;
    s_unpersisted = false;
    s_dirty_at = 0;
    s_last_save_mono = 0;
    s_last_saved_at = 0;
    s_save_seq = 0;

    char err[128];
    err[0] = '\0';
    if (!game_storage_probe(err, (int)sizeof err)) {
        s_unpersisted = true; /* web quota / private mode caught at startup (§14 п.3) */
    }
}

bool game_save_new_game(char *error, int error_cap) {
    free_orphans();
    reset_all();
    on_new_game_all();
    s_autosave_paused = false; /* resume autosave (Р10) */
    s_dirty = false;
    const bool ok = save_internal(error, error_cap);
    s_last_save_mono = mono_now();
    return ok;
}

bool game_save_flush(char *error, int error_cap) {
    if (s_autosave_paused) {
        return true; /* NEWER/CORRUPT before new_game: nothing may be persisted */
    }
    return save_internal(error, error_cap);
}

void game_save_tick(void) {
    if (s_autosave_paused || !s_dirty) {
        return;
    }
    const int64_t now = mono_now();
    if ((now - s_dirty_at >= (int64_t)GAME_SAVE_DEBOUNCE_MS) ||
        (now - s_last_save_mono >= (int64_t)GAME_SAVE_MAX_INTERVAL_MS)) {
        char err[128];
        err[0] = '\0';
        (void)save_internal(err, (int)sizeof err);
    }
}

void game_save_mark_dirty(void) {
    if (!s_dirty) {
        s_dirty = true;
        s_dirty_at = mono_now(); /* first mark after clean (§14 п.6) */
    }
}

int64_t game_save_last_saved_at(void) { return s_last_saved_at; }

bool game_save_is_unpersisted(void) { return s_unpersisted; }

char *game_save_export_string(char *error, int error_cap) {
    int64_t wall = 0;
    cJSON *root = build_root(false, &wall); /* snapshot; do not bump the persistent seq */
    if (!root) {
        gsj_set_error(error, error_cap, "failed to build export document");
        return NULL;
    }
    char *flat = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!flat) {
        gsj_set_error(error, error_cap, "failed to serialize export document");
        return NULL;
    }
    char *encoded = transform_encode(flat, error, error_cap);
    free(flat);
    return encoded;
}

bool game_save_import_string(const char *text, char *error, int error_cap) {
    if (!text) {
        gsj_set_error(error, error_cap, "import text is required");
        return false;
    }
    char *decoded = transform_decode(text, error, error_cap);
    if (!decoded) {
        return false;
    }
    cJSON *doc = cJSON_Parse(decoded);
    free(decoded);
    if (!doc || !cJSON_IsObject(doc)) {
        cJSON_Delete(doc);
        gsj_set_error(error, error_cap, "import is not a valid save"); /* state untouched */
        return false;
    }
    if (!cJSON_IsObject(gsj_object_item(doc, "features"))) {
        cJSON_Delete(doc);
        gsj_set_error(error, error_cap, "import has no features"); /* state untouched */
        return false;
    }
    const int64_t old_seq = s_save_seq;
    game_save_load_result_t r;
    memset(&r, 0, sizeof r);
    load_from_doc(doc, &r);
    cJSON_Delete(doc);
    if (s_save_seq < old_seq) {
        s_save_seq = old_seq; /* never rewind the counter on import */
    }
    s_autosave_paused = false;
    game_save_mark_dirty();
    (void)error;
    (void)error_cap;
    return true;
}

void game_save_set_transforms(const game_save_transform_t *chain, int count) {
    s_transforms = chain;
    s_transform_count = (count > 0) ? count : 0;
}

/* ---- web visibility flush (§14 п.5 / §A3.8). rAF freezes on a hidden tab, so
   the flush must be a synchronous force-save fired straight from the event. ---- */
#if defined(__EMSCRIPTEN__)
EMSCRIPTEN_KEEPALIVE void game_save_web_flush(void) {
    char err[128];
    err[0] = '\0';
    (void)game_save_flush(err, (int)sizeof err);
}
/* clang-format off */
EM_JS(void, game_save_web_install, (void), {
    var flush = function() {
        if (Module['_game_save_web_flush']) { Module['_game_save_web_flush'](); }
    };
    document.addEventListener('visibilitychange', function() {
        if (document.visibilityState === 'hidden') { flush(); }
    });
    window.addEventListener('pagehide', flush);
})
/* clang-format on */
void game_save_install_web_flush(void) { game_save_web_install(); }
#else
void game_save_install_web_flush(void) {}
#endif

#ifdef GAME_SAVE_TESTING
void game_save__set_clocks_for_test(int64_t (*mono)(void), int64_t (*wall)(void)) {
    s_mono_clock = mono;
    s_wall_clock = wall;
}
#endif
