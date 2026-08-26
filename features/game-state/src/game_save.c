/* game_save.c — hand-written L0 save orchestrator.
   Fragment registry + single atomic envelope document + load state machine
   (FRESH/LOADED/RECOVERED_BAK/CORRUPT_RESET/NEWER/BLOCKED, per-fragment except staged
   versioned document migrations) + on_new_game + dirty/debounce/MAX_INTERVAL + synchronous web
   visibility-flush + export/import + empty transform seam. Single thread. */

#include "game_save.h"

#include "game_state_json.h"
#include "game_storage.h"
#if !defined(GAME_SAVE_TESTING)
#include "game_save_platform.h"
#endif

#include "log/nt_log.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CMake supplies these for the game/test targets; guarded defaults keep the file
   self-contained. */
/* One slot, one name: an unusable save goes to quarantine and a normal save
   takes the normal name, so nothing downstream has to work out WHICH file is
   the save. No load outcome may end in a state only the player could clear. */
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
#define GAME_STORAGE_APP_ID "game"
#endif

/* Internal constants. */
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
static const GameSaveDocumentMigrateFn *s_document_migrations;
static int s_document_migration_count;
static GameSaveDocumentValidateFn s_document_validator;

static bool    s_autosave_paused; /* CORRUPT_RESET until the shell's new_game;
                                     also while a file we could not move aside is
                                     still sitting in the slot (s_quarantine_owed) */
/* The slot holds bytes we could neither use nor move aside. Writing would
   destroy them, so the tick retries the move instead until whatever holds the
   file lets go. */
static bool    s_quarantine_owed;
static bool    s_new_game_pending;         /* Р11: deferred to a safe shell frame boundary */
static char    s_new_game_skip_id[32];     /* fragment id to leave untouched, or "" for none */
static bool    s_dirty;
static bool    s_unpersisted;
/* Autosave RATE GATES, not history: a failed save charges itself to both so the
   retry lands one debounce later instead of on the next frame. Every load path
   stamps them too. "When was the data last written" is s_last_saved_at. */
static int64_t s_dirty_at;        /* mono ms of the first mark after clean, or of the last failed attempt */
static int64_t s_last_save_mono;  /* mono ms of the last save ATTEMPT or load */
static int64_t s_last_saved_at;   /* wall ms stamped into the last save/load */
static int64_t s_save_seq;        /* monotonic counter, restored from the loaded envelope */

static int64_t (*s_mono_clock)(void);
static int64_t (*s_wall_clock)(void);

/* ---- platform default clocks ---- */

#if defined(GAME_SAVE_TESTING)
static int64_t default_mono_ms(void) { return 0; }
static int64_t default_wall_ms(void) { return 0; }
#else
static int64_t default_mono_ms(void) { return game_save_platform_mono_ms(); }
static int64_t default_wall_ms(void) { return game_save_platform_wall_ms(); }
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
        const double version = vj->valuedouble;
        if (isfinite(version) && version >= 1.0 && version <= (double)INT_MAX &&
            version == trunc(version)) {
            return (int)version;
        }
        return INT_MAX; /* invalid version is never safe to load as an old save */
    }
    return 1; /* absent -> v1 */
}

/* ---- orphan retention ---- */

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

/* A small JSON snapshot is enough to restore the opaque fragment registry when
   an import proves invalid during fragment parsing.  It intentionally contains
   only unknown keys so capture_orphans() can restore its own ownership. */
static cJSON *snapshot_orphans(void) {
    cJSON *snapshot = cJSON_CreateObject();
    if (!snapshot) {
        return NULL;
    }
    for (int i = 0; i < s_orphan_count; i++) {
        cJSON *copy = cJSON_Duplicate(s_orphans[i].subtree, true);
        if (!copy || !cJSON_AddItemToObject(snapshot, s_orphans[i].id, copy)) {
            cJSON_Delete(copy);
            cJSON_Delete(snapshot);
            return NULL;
        }
    }
    return snapshot;
}

/* ---- transform seam. Default chain empty -> flat '{' JSON. ---- */

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
        return dup_string(raw); /* plain hand-edited JSON always loads */
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

/* skip_id NULLABLE: id of the ONE fragment to leave untouched, so "hold to reset
   progress" does not take settings and volumes with it. */
static void reset_all_except(const char *skip_id) {
    for (int i = 0; i < s_fragment_count; i++) {
        if (skip_id && strcmp(s_fragments[i]->id, skip_id) == 0) {
            continue;
        }
        if (s_fragments[i]->reset) {
            s_fragments[i]->reset();
        }
    }
}

static void on_new_game_all_except(const char *skip_id) {
    for (int i = 0; i < s_fragment_count; i++) {
        if (skip_id && strcmp(s_fragments[i]->id, skip_id) == 0) {
            continue;
        }
        if (s_fragments[i]->on_new_game) {
            s_fragments[i]->on_new_game();
        }
    }
}

static void reset_all(void) { reset_all_except(NULL); }
static void on_new_game_all(void) { on_new_game_all_except(NULL); }

static void reconcile_all(void) {
    for (int i = 0; i < s_fragment_count; i++) {
        if (s_fragments[i]->reconcile) {
            s_fragments[i]->reconcile();
        }
    }
}

bool game_save_reconcile_from(const char *id) {
    if (!id) {
        return false;
    }
    for (int i = 0; i < s_fragment_count; ++i) {
        if (strcmp(s_fragments[i]->id, id) != 0) {
            continue;
        }
        for (int j = i; j < s_fragment_count; ++j) {
            if (s_fragments[j]->reconcile) {
                s_fragments[j]->reconcile();
            }
        }
        return true;
    }
    return false;
}

/* ---- envelope build + save ---- */

/* Builds a complete snapshot without changing live persistence metadata. A save
   publishes its next sequence only after the encoded document reaches storage. */
static cJSON *build_root(bool bump_seq, int64_t *out_wall, int64_t *out_seq) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    const int64_t wall = wall_now();
    if (bump_seq && s_save_seq == INT64_MAX) {
        cJSON_Delete(root);
        return NULL;
    }
    const int64_t seq = bump_seq ? s_save_seq + 1 : s_save_seq;
    if (!cJSON_AddNumberToObject(root, "format", (double)GAME_SAVE_FORMAT) ||
        !cJSON_AddNumberToObject(root, "save_version", (double)GAME_SAVE_DOC_VERSION) ||
        !cJSON_AddNumberToObject(root, "saved_at", (double)wall) ||
        !cJSON_AddNumberToObject(root, "save_seq", (double)seq) ||
        !cJSON_AddStringToObject(root, "app", GAME_STORAGE_APP_ID) ||
        !cJSON_AddStringToObject(root, "build", GAME_SAVE_BUILD)) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *features = cJSON_CreateObject();
    if (!features) {
        cJSON_Delete(root);
        return NULL;
    }
    if (!cJSON_AddItemToObject(root, "features", features)) {
        cJSON_Delete(features);
        cJSON_Delete(root);
        return NULL;
    }

    for (int i = 0; i < s_fragment_count; i++) {
        const GameSaveFragment *frag = s_fragments[i];
        cJSON *payload = frag->to_json ? frag->to_json() : NULL;
        if (!payload) {
            cJSON_Delete(root);
            return NULL;
        }
        if (!cJSON_AddNumberToObject(payload, "v", (double)frag->version) ||
            !cJSON_AddItemToObject(features, frag->id, payload)) {
            cJSON_Delete(payload);
            cJSON_Delete(root);
            return NULL;
        }
    }
    /* retained orphans, verbatim, AFTER registered fragments */
    for (int i = 0; i < s_orphan_count; i++) {
        cJSON *dup = cJSON_Duplicate(s_orphans[i].subtree, true);
        if (!dup || !cJSON_AddItemToObject(features, s_orphans[i].id, dup)) {
            cJSON_Delete(dup);
            cJSON_Delete(root);
            return NULL;
        }
    }
    if (out_wall) {
        *out_wall = wall;
    }
    if (out_seq) {
        *out_seq = seq;
    }
    return root;
}

static bool validate_document(const cJSON *doc, char *error, int error_cap) {
    if (!s_document_validator) {
        return true;
    }
    const cJSON *features = gsj_object_item(doc, "features");
    if (!cJSON_IsObject(features)) {
        gsj_set_error(error, error_cap, "save has no features");
        return false;
    }
    return s_document_validator(features, error, error_cap);
}

static bool validate_envelope_header(const cJSON *doc, char *error, int error_cap) {
    const cJSON *format = gsj_object_item(doc, "format");
    if (!cJSON_IsNumber(format) || format->valuedouble != (double)GAME_SAVE_FORMAT) {
        gsj_set_error(error, error_cap, "invalid save format");
        return false;
    }
    const cJSON *app = gsj_object_item(doc, "app");
    if (!cJSON_IsString(app) || strcmp(app->valuestring, GAME_STORAGE_APP_ID) != 0) {
        gsj_set_error(error, error_cap, "save belongs to another app");
        return false;
    }
    const cJSON *build = gsj_object_item(doc, "build");
    const cJSON *save_version = gsj_object_item(doc, "save_version");
    const bool legacy_without_build =
        !build && (!save_version ||
                   (cJSON_IsNumber(save_version) &&
                    save_version->valuedouble < (double)GAME_SAVE_DOC_VERSION));
    if (!legacy_without_build && !cJSON_IsString(build)) {
        gsj_set_error(error, error_cap, "invalid build metadata");
        return false;
    }
    if (!gsj_object_item(doc, "saved_at") || !gsj_object_item(doc, "save_seq")) {
        gsj_set_error(error, error_cap, "save metadata is incomplete");
        return false;
    }
    int64_t metadata = 0;
    if (!gsj_read_i64(doc, "saved_at", 0, INT64_MAX, &metadata, error, error_cap) ||
        !gsj_read_i64(doc, "save_seq", 0, INT64_MAX, &metadata, error, error_cap)) {
        return false;
    }
    return true;
}

/* Duplicate -> migrate every cross-fragment version -> validate, with no live
   publication and no mutation of caller-owned JSON until the whole document is
   known-good. */
static bool prepare_document_for_load(
    const cJSON *doc, cJSON **out_doc, bool *out_migrated, char *error, int error_cap) {
    if (out_doc) {
        *out_doc = NULL;
    }
    if (out_migrated) {
        *out_migrated = false;
    }
    if (!cJSON_IsObject(doc)) {
        gsj_set_error(error, error_cap, "save is not an object");
        return false;
    }
    if (!validate_envelope_header(doc, error, error_cap)) {
        return false;
    }
    const cJSON *version_json = gsj_object_item(doc, "save_version");
    int version = 1; /* absent is the frozen legacy v1 envelope */
    if (version_json) {
        if (!cJSON_IsNumber(version_json)) {
            gsj_set_error(error, error_cap, "invalid save_version");
            return false;
        }
        const double version_number = version_json->valuedouble;
        if (!isfinite(version_number) || version_number < 1.0 ||
            version_number > (double)GAME_SAVE_DOC_VERSION ||
            version_number != trunc(version_number)) {
            if (isfinite(version_number) && version_number > (double)GAME_SAVE_DOC_VERSION) {
                gsj_set_error(error, error_cap, "save is newer than this build");
            } else {
                gsj_set_error(error, error_cap, "invalid save_version");
            }
            return false;
        }
        if (version_number > (double)INT_MAX) {
            gsj_set_error(error, error_cap, "save is newer than this build");
            return false;
        }
        version = (int)version_number;
    }

    cJSON *copy = cJSON_Duplicate(doc, true);
    if (!copy) {
        gsj_set_error(error, error_cap, "failed to stage save document");
        return false;
    }
    cJSON *features = cJSON_GetObjectItemCaseSensitive(copy, "features");
    if (!cJSON_IsObject(features)) {
        cJSON_Delete(copy);
        gsj_set_error(error, error_cap, "save has no features");
        return false;
    }
    const int original_version = version;
    if (version < GAME_SAVE_DOC_VERSION && !s_document_validator) {
        cJSON_Delete(copy);
        gsj_set_error(error, error_cap, "document migrations require a validator");
        return false;
    }
    for (; version < GAME_SAVE_DOC_VERSION; version++) {
        const int step_index = version - 1;
        GameSaveDocumentMigrateFn step =
            (step_index >= 0 && step_index < s_document_migration_count && s_document_migrations)
                ? s_document_migrations[step_index]
                : NULL;
        if (!step) {
            cJSON_Delete(copy);
            gsj_set_error(error, error_cap, "missing document migration step");
            return false;
        }
        if (!step(features, error, error_cap)) {
            cJSON_Delete(copy);
            return false;
        }
    }
    cJSON *current_version = cJSON_CreateNumber((double)GAME_SAVE_DOC_VERSION);
    if (!current_version) {
        cJSON_Delete(copy);
        gsj_set_error(error, error_cap, "failed to stamp save_version");
        return false;
    }
    if (version_json) {
        if (!cJSON_ReplaceItemInObjectCaseSensitive(copy, "save_version", current_version)) {
            cJSON_Delete(current_version);
            cJSON_Delete(copy);
            gsj_set_error(error, error_cap, "failed to stamp save_version");
            return false;
        }
    } else {
        if (!cJSON_AddItemToObject(copy, "save_version", current_version)) {
            cJSON_Delete(current_version);
            cJSON_Delete(copy);
            gsj_set_error(error, error_cap, "failed to stamp save_version");
            return false;
        }
    }
    if (!validate_document(copy, error, error_cap)) {
        cJSON_Delete(copy);
        return false;
    }
    if (out_doc) {
        *out_doc = copy;
    } else {
        cJSON_Delete(copy);
    }
    if (out_migrated) {
        *out_migrated = original_version != GAME_SAVE_DOC_VERSION;
    }
    return true;
}

/* The reason of the LAST failure logged. Deliberately not keyed on
   `s_unpersisted`: a failed storage probe at init raises that too, which would
   silence the first real failure on the very platform where failures are
   expected. Keyed on the TEXT so a failure that changes cause is reported
   again instead of hiding behind the first one. */
#define GAME_SAVE_REASON_MAX 192
static char s_logged_failure[GAME_SAVE_REASON_MAX];
/* Separate from the text because "" is a REAL reason -- transform_encode can
   return NULL without filling one, and it must not compare equal to the
   never-logged initial state. */
static bool s_failure_logged;

static bool failure_is_new(const char *reason) {
    if (s_failure_logged && strcmp(s_logged_failure, reason) == 0) {
        return false;
    }
    (void)snprintf(s_logged_failure, sizeof s_logged_failure, "%s", reason);
    s_failure_logged = true;
    return true;
}

/* may_wait travels to the storage write, and only game_save_tick passes false:
   it is the one caller inside the frame, where a refused write is not a loss.
   Every other caller is a synchronous moment with no frame to lose and would
   rather wait ~0.5s than leave the primary unwritten. */
static bool save_internal(char *error, int error_cap, bool may_wait) {
    /* EVERY failure must exit through the one place below. A failure that skips
       the clock bookkeeping re-enters this function on the next frame, so a
       save the validator rejects rebuilds the whole document 60 times a
       second. */
    char reason[GAME_SAVE_REASON_MAX];
    reason[0] = '\0';
    cJSON *root = NULL;
    char *flat = NULL;
    char *encoded = NULL;
    int64_t wall = 0;
    int64_t seq = 0;
    bool ok = false;

    root = build_root(true, &wall, &seq);
    if (!root) {
        gsj_set_error(reason, (int)sizeof reason, "failed to build save document");
        goto done;
    }
    if (!validate_document(root, reason, (int)sizeof reason)) {
        goto done;
    }
    flat = cJSON_PrintUnformatted(root);
    if (!flat) {
        gsj_set_error(reason, (int)sizeof reason, "failed to serialize save document");
        goto done;
    }
    encoded = transform_encode(flat, reason, (int)sizeof reason);
    if (!encoded) {
        goto done;
    }
    ok = may_wait
             ? game_storage_write_blocking(GAME_SAVE_AUTOSAVE_SLOT, encoded, reason,
                                           (int)sizeof reason)
             : game_storage_write(GAME_SAVE_AUTOSAVE_SLOT, encoded, reason,
                                  (int)sizeof reason);

done:
    cJSON_Delete(root);
    free(flat);
    free(encoded);

    if (ok) {
        if (s_failure_logged) {
            nt_log_warn("game_save: save recovered after an earlier failure (%s)",
                        s_logged_failure[0] != '\0' ? s_logged_failure
                                                    : "no reason reported");
            s_logged_failure[0] = '\0';
            s_failure_logged = false;
        }
        s_save_seq = seq;
        s_dirty = false;
        s_last_save_mono = mono_now();
        s_last_saved_at = wall;
        s_unpersisted = false;
    } else {
        /* Both tick call sites discard the return value, so this log is the only
           place the reason survives. A flag without a reason is not a report. */
        if (failure_is_new(reason)) {
            nt_log_warn("game_save: save failed (%s); data kept in memory, will retry",
                        reason[0] != '\0' ? reason : "no reason reported");
        }
        s_unpersisted = true;
        /* Dirty even if it was clean: game_save_flush is legal on a clean state,
           and without this a failed flush would latch s_unpersisted with nothing
           left to clear it -- the tick returns early while !s_dirty. */
        s_dirty = true;
        /* Charge the attempt to BOTH clocks: they are the only thing standing
           between a persistently failing save and 60 retries a second. */
        const int64_t attempted_at = mono_now();
        s_dirty_at = attempted_at;
        s_last_save_mono = attempted_at;
    }
    if (error != NULL && error_cap > 0 && !ok) {
        (void)snprintf(error, (size_t)error_cap, "%s", reason);
    }
    return ok;
}

/* ---- load state machine ---- */

static bool doc_is_newer(const cJSON *doc) {
    const cJSON *fj = gsj_object_item(doc, "format");
    if (cJSON_IsNumber(fj) && isfinite(fj->valuedouble) &&
        fj->valuedouble > (double)GAME_SAVE_FORMAT) {
        return true;
    }
    const cJSON *svj = gsj_object_item(doc, "save_version");
    if (cJSON_IsNumber(svj) && svj->valuedouble > (double)GAME_SAVE_DOC_VERSION) {
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
    return false; /* unknown feature keys are orphans, NOT NEWER */
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
    s_save_seq = seq; /* next save > loaded (monotonic across sessions) */

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
            /* v == frag->version; v > frag->version is unreachable — NEWER preempts. */
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

/* Rewrite the primary from the backup, THEN refresh the backup -- never the
   other way round: the primary is bad right now, so backing it up first would
   clobber the live .bak with garbage. Returns false with `result` untouched
   when there is no usable backup, which is always the case on web. */
static bool try_recover_from_backup(game_save_load_result_t *result, const char *reason, char *err, int cap) {
    char *baktext = NULL;
    cJSON *bakdoc = NULL;
    cJSON *prepared = NULL;
    if (game_storage_read_backup(GAME_SAVE_AUTOSAVE_SLOT, &baktext, err, cap)) {
        char *bakdec = transform_decode(baktext, err, cap);
        free(baktext);
        bakdoc = bakdec ? cJSON_Parse(bakdec) : NULL;
        free(bakdec);
    }
    if (!(bakdoc && cJSON_IsObject(bakdoc) && !doc_is_newer(bakdoc) &&
          prepare_document_for_load(bakdoc, &prepared, NULL, err, cap))) {
        if (bakdoc) {
            cJSON_Delete(bakdoc);
        }
        return false;
    }
    load_from_doc(prepared, result);
    cJSON_Delete(prepared);
    cJSON_Delete(bakdoc);
    s_autosave_paused = false;
    result->status = GAME_SAVE_LOAD_RECOVERED_BAK;
    set_message(result, reason);
    game_save_mark_dirty();
    /* The existing .bak is the only known durable good copy until primary is
       rewritten successfully. Never refresh it from a still-corrupt primary. */
    if (save_internal(err, cap, true)) {
        (void)game_storage_write_backup(GAME_SAVE_AUTOSAVE_SLOT, err, cap);
    }
    s_last_save_mono = mono_now();
    return true;
}

/* Move a slot this build cannot use into quarantine, then start a normal game
   under the normal name. If the move is REFUSED the bytes stay put and writing
   is off until it succeeds -- game_save_tick keeps retrying, so neither branch
   waits on the player. */
static void set_aside_and_start_new(
    game_save_load_result_t *result, game_save_load_status_t status,
    const char *why, const char *message) {
    char err[128];
    err[0] = '\0';
    result->status = status;
    set_message(result, message);

    free_orphans();
    reset_all();

    if (!game_storage_quarantine(GAME_SAVE_AUTOSAVE_SLOT, err, (int)sizeof err)) {
        /* Not writable and not movable: keep the bytes, keep trying. */
        s_quarantine_owed = true;
        s_autosave_paused = true;
        on_new_game_all(); /* a playable session; it persists once the file frees up */
        nt_log_warn("game_save: %s but could not be moved aside (%s); retrying, writes held", why, err);
        s_last_save_mono = mono_now();
        return;
    }
    s_quarantine_owed = false;
    s_autosave_paused = false;
    on_new_game_all();
    game_save_mark_dirty();
    (void)save_internal(err, (int)sizeof err, true); /* load path: synchronous */
    nt_log_warn("game_save: %s; moved to quarantine, new save started", why);
    s_last_save_mono = mono_now();
}

/* Retried from the tick until the file lets go. Only then may anything be
   written, because until then the slot still holds the only copy. */
static void retry_owed_quarantine(void) {
    char err[128];
    err[0] = '\0';
    if (!game_storage_quarantine(GAME_SAVE_AUTOSAVE_SLOT, err, (int)sizeof err)) {
        return;
    }
    s_quarantine_owed = false;
    s_autosave_paused = false;
    game_save_mark_dirty();
    nt_log_warn("game_save: the unusable save could finally be moved aside; saving resumed");
}

void game_save_load(game_save_load_result_t *result) {
    game_save_load_result_t local;
    if (!result) {
        result = &local;
    }
    memset(result, 0, sizeof *result);
    result->status = GAME_SAVE_LOAD_FRESH;

    char err[128];
    err[0] = '\0';

    char *text = NULL;
    game_storage_read_status_t rst = GAME_STORAGE_READ_ABSENT;
    if (!game_storage_read(GAME_SAVE_AUTOSAVE_SLOT, &text, &rst, err, (int)sizeof err)) {
        if (rst == GAME_STORAGE_READ_ERROR_PRESERVED) {
            /* The primary holds the only known copy and could not even be
               copied aside, so it stays untouched until the move succeeds. */
            set_aside_and_start_new(
                result, GAME_SAVE_LOAD_BLOCKED, "the save could not be read",
                "save unreadable; set aside; new save started");
            return;
        }
        if (rst == GAME_STORAGE_READ_ERROR) {
            /* Primary EXISTS but could not be READ, which is not "no save":
               storage already copied the bytes to quarantine, so try the backup
               first and fall through to corrupt-reset only if there is none.
               Must never be reborn as FRESH -- that silently discards a save. */
            if (try_recover_from_backup(result, "primary unreadable; recovered from backup", err, (int)sizeof err)) {
                return;
            }
            free_orphans();
            reset_all();
            s_autosave_paused = true;
            result->status = GAME_SAVE_LOAD_CORRUPT_RESET;
            set_message(result, "save read failed; original quarantined; new game");
            nt_log_warn("game_save: autosave read failed; original quarantined; awaiting new game");
            s_last_save_mono = mono_now();
            return;
        }
        /* No save -> FRESH: reset + on_new_game + save. */
        free_orphans();
        reset_all();
        on_new_game_all();
        s_autosave_paused = false;
        game_save_mark_dirty();
        (void)save_internal(err, (int)sizeof err, true); /* load path: synchronous */
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
            /* A save from a build ahead of this one: loading it is unsafe and
               overwriting it would destroy real progress, since the usual cause
               is a rolled-back build that may come back tomorrow. */
            cJSON_Delete(doc);
            set_aside_and_start_new(
                result, GAME_SAVE_LOAD_NEWER, "the save is newer than this build",
                "save is newer than this build; set aside; new save started");
            return;
        }
        cJSON *prepared = NULL;
        bool migrated = false;
        if (prepare_document_for_load(doc, &prepared, &migrated, err, (int)sizeof err)) {
            /* LOADED. */
            load_from_doc(prepared, result);
            cJSON_Delete(prepared);
            cJSON_Delete(doc);
            s_autosave_paused = false;
            result->status = GAME_SAVE_LOAD_LOADED;
            set_message(result, "loaded");
            (void)game_storage_write_backup(GAME_SAVE_AUTOSAVE_SLOT, err, (int)sizeof err); /* last-known-good */
            if (migrated) {
                game_save_mark_dirty(); /* persist the current envelope version after debounce */
            }
            s_last_save_mono = mono_now();
            return;
        }
    }
    if (doc) {
        cJSON_Delete(doc);
    }

    /* Primary unparseable -> try backup. */
    if (try_recover_from_backup(result, "primary corrupt; recovered from backup", err, (int)sizeof err)) {
        return;
    }

    /* CORRUPT_RESET: quarantine + reset only. No on_new_game, no save — the shell
       waits for the player's explicit new_game (Р10). Autosave paused. */
    /* Corrupt primary, no usable backup. Same rule, and the status stays
       CORRUPT_RESET because the shell has always applied its own New Game on
       it; set_aside_and_start_new only takes over when the move is refused. */
    if (!game_storage_quarantine(
            GAME_SAVE_AUTOSAVE_SLOT, err, (int)sizeof err)) {
        set_aside_and_start_new(
            result, GAME_SAVE_LOAD_BLOCKED, "the save is corrupt",
            "corrupt save could not be moved aside; retrying, writes held");
        return;
    }
    free_orphans();
    reset_all();
    s_quarantine_owed = false;
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

void game_save_set_document_migrations(const GameSaveDocumentMigrateFn *steps, int step_count) {
    s_document_migrations = steps;
    s_document_migration_count = (steps && step_count > 0) ? step_count : 0;
}

void game_save_set_document_validator(GameSaveDocumentValidateFn validator) {
    s_document_validator = validator;
}

bool game_save_validate_current(char *error, int error_cap) {
    int64_t wall = 0;
    cJSON *root = build_root(false, &wall, NULL);
    if (!root) {
        gsj_set_error(error, error_cap, "failed to build validation document");
        return false;
    }
    bool valid = validate_document(root, error, error_cap);
    cJSON_Delete(root);
    return valid;
}

/* ---- registry read-access for the DevAPI dispatch. Additive, read-only,
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

/* ---- Orphan read-access (retained unknown feature keys). Additive,
   read-only view of s_orphans, symmetric to the fragment read-access above. Subtree stays
   OWNED by game_save (the DevAPI aggregate duplicates before handing out). ---- */

int game_save_orphan_count(void) { return s_orphan_count; }

const cJSON *game_save_orphan_at(int index, const char **id) {
    if (index < 0 || index >= s_orphan_count) {
        return NULL; /* out of range: leave *id untouched */
    }
    if (id) {
        *id = s_orphans[index].id;
    }
    return s_orphans[index].subtree;
}

void game_save_init(void) {
    if (!s_mono_clock) {
        s_mono_clock = default_mono_ms;
    }
    if (!s_wall_clock) {
        s_wall_clock = default_wall_ms;
    }
    s_dirty = false;
    s_quarantine_owed = false;
    s_autosave_paused = false;
    s_unpersisted = false;
    s_new_game_pending = false;
    s_new_game_skip_id[0] = '\0';
    s_dirty_at = 0;
    s_last_save_mono = 0;
    s_last_saved_at = 0;
    s_save_seq = 0;
    s_logged_failure[0] = '\0';
    s_failure_logged = false;

    char err[128];
    err[0] = '\0';
    if (!game_storage_probe(err, (int)sizeof err)) {
        s_unpersisted = true; /* web quota / private mode caught at startup */
    }
}

static void begin_new_game_except(const char *skip_id) {
    free_orphans();
    reset_all_except(skip_id);
    on_new_game_all_except(skip_id);
    /* A New Game is not a licence to overwrite bytes we could not move aside:
       retry the move, and stay held if it is still refused. */
    if (s_quarantine_owed) {
        retry_owed_quarantine();
    }
    if (!s_quarantine_owed) {
        s_autosave_paused = false;
    }
    game_save_mark_dirty();
}

static game_save_transition_result_t new_game_except(
    const char *skip_id, char *error, int error_cap, bool may_wait) {
    begin_new_game_except(skip_id);
    /* Held: the write would land on the file we could not move. Say so rather
       than reporting a save that did not happen. */
    const bool persisted =
        s_quarantine_owed ? (gsj_set_error(error, error_cap,
                                           "save is held: the previous file could not be moved aside"),
                             false)
                          : save_internal(error, error_cap, may_wait);
    if (persisted) {
        s_last_save_mono = mono_now();
    }
    /* Stamped only when persisted: save_internal already charged both clocks on
       failure, and stamping here anyway would reset the max-interval clock as
       if the write had landed. */
    return (game_save_transition_result_t){.state_changed = true, .persisted = persisted};
}

game_save_transition_result_t game_save_new_game(char *error, int error_cap) {
    /* Direct API: a synchronous moment with no frame to lose -- the corrupt-save
       path at startup and the dev endpoints. */
    return new_game_except(NULL, error, error_cap, true);
}

void game_save_begin_new_game_transition(void) {
    begin_new_game_except(NULL);
}

/* Records the request instead of performing it: the caller is a feature's
   draw_ui, which runs inside the live GPU render pass, and new_game_except does
   synchronous file I/O. The shell applies it at the next safe boundary, so the
   composition transition stays atomic and storage I/O stays out of UI
   construction. skip_fragment_id NULLABLE -- see reset_all_except. */
void game_save_request_new_game(const char *skip_fragment_id) {
    s_new_game_pending = true;
    if (skip_fragment_id) {
        (void)snprintf(s_new_game_skip_id, sizeof s_new_game_skip_id, "%s", skip_fragment_id);
    } else {
        s_new_game_skip_id[0] = '\0';
    }
}

/* Shell-only, at a safe pre-update or post-render boundary. state_changed tells
   the caller to reset the game-composition state game_save does not own, such
   as player world position, in the same beat. */
game_save_transition_result_t game_save_apply_pending_new_game(void) {
    if (!s_new_game_pending) {
        return (game_save_transition_result_t){0};
    }
    s_new_game_pending = false;
    char err[128];
    err[0] = '\0';
    /* may_wait false: both shell call sites are INSIDE the frame. A refused
       write here is not a loss -- the state stays dirty and the tick retries. */
    return new_game_except(
        s_new_game_skip_id[0] ? s_new_game_skip_id : NULL, err, (int)sizeof err, false);
}

bool game_save_flush(char *error, int error_cap) {
    if (s_autosave_paused) {
        gsj_set_error(
            error, error_cap,
            "save is read-only until New Game or a successful repair/import");
        return false; /* NEWER/CORRUPT/BLOCKED: reporting success would be data loss */
    }
    return save_internal(error, error_cap, true);
}

void game_save_tick(void) {
    if (s_quarantine_owed) {
        retry_owed_quarantine();
    }
    if (s_autosave_paused || !s_dirty) {
        return;
    }
    const int64_t now = mono_now();
    if ((now - s_dirty_at >= (int64_t)GAME_SAVE_DEBOUNCE_MS) ||
        (now - s_last_save_mono >= (int64_t)GAME_SAVE_MAX_INTERVAL_MS)) {
        char err[128];
        err[0] = '\0';
        /* false: this is the ONE caller inside the frame. A refused write here
           is not a loss -- the state stays dirty and the next tick retries. */
        (void)save_internal(err, (int)sizeof err, false);
    }
}

void game_save_mark_dirty(void) {
    if (!s_dirty) {
        s_dirty = true;
        s_dirty_at = mono_now(); /* first mark after clean */
    }
}

int64_t game_save_last_saved_at(void) { return s_last_saved_at; }

bool game_save_is_unpersisted(void) { return s_unpersisted; }

/* PAUSED outranks RETRYING: a paused save refuses every write outright, so
   "we will retry" would be the wrong thing to tell anyone. */
game_save_persistence_t game_save_persistence(void) {
    if (s_autosave_paused) {
        return GAME_SAVE_PERSISTENCE_PAUSED;
    }
    return s_unpersisted ? GAME_SAVE_PERSISTENCE_RETRYING : GAME_SAVE_PERSISTENCE_OK;
}

char *game_save_export_string(char *error, int error_cap) {
    int64_t wall = 0;
    cJSON *root = build_root(false, &wall, NULL); /* snapshot; do not bump the persistent seq */
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
    if (strlen(text) > GAME_STORAGE_MAX_BYTES) {
        gsj_set_error(error, error_cap, "import exceeds storage size limit");
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
    cJSON *prepared = NULL;
    if (doc_is_newer(doc)) {
        cJSON_Delete(doc);
        gsj_set_error(error, error_cap, "import is newer than this build");
        return false;
    }
    if (!prepare_document_for_load(doc, &prepared, NULL, error, error_cap)) {
        cJSON_Delete(doc);
        return false; /* staged cross-fragment state was never published */
    }
    /* Fragment descriptors intentionally hide their storage. Snapshot every
       fragment before publication so an invalid import cannot leave a mixture
       of imported neighbours and reset defaults. Normal disk load remains
       fault-isolated; this stronger transaction is specific to explicit import. */
    cJSON *snapshots[GAME_SAVE_MAX_FRAGMENTS] = {0};
    cJSON *old_orphans = snapshot_orphans();
    if (!old_orphans) {
        cJSON_Delete(prepared);
        cJSON_Delete(doc);
        gsj_set_error(error, error_cap, "failed to stage current orphan state");
        return false;
    }
    for (int i = 0; i < s_fragment_count; i++) {
        const GameSaveFragment *fragment = s_fragments[i];
        snapshots[i] = fragment->to_json ? fragment->to_json() : NULL;
        if (!snapshots[i]) {
            for (int j = 0; j < i; j++) {
                cJSON_Delete(snapshots[j]);
            }
            cJSON_Delete(old_orphans);
            cJSON_Delete(prepared);
            cJSON_Delete(doc);
            gsj_set_error(error, error_cap, "failed to stage current state");
            return false;
        }
    }
    const int64_t old_seq = s_save_seq;
    const int64_t old_saved_at = s_last_saved_at;
    const bool old_autosave_paused = s_autosave_paused;
    const bool old_dirty = s_dirty;
    const bool old_unpersisted = s_unpersisted;
    const int64_t old_dirty_at = s_dirty_at;
    const int64_t old_last_save_mono = s_last_save_mono;
    game_save_load_result_t r;
    memset(&r, 0, sizeof r);
    load_from_doc(prepared, &r);
    cJSON_Delete(prepared);
    cJSON_Delete(doc);
    if (r.reset_fragment_count > 0) {
        bool restored = true;
        for (int i = 0; i < s_fragment_count; i++) {
            const GameSaveFragment *fragment = s_fragments[i];
            char restore_error[128] = {0};
            if (!fragment->from_json ||
                !fragment->from_json(snapshots[i], restore_error, (int)sizeof restore_error)) {
                restored = false;
            }
        }
        capture_orphans(old_orphans);
        cJSON_Delete(old_orphans);
        for (int i = 0; i < s_fragment_count; i++) {
            cJSON_Delete(snapshots[i]);
        }
        s_save_seq = old_seq;
        s_last_saved_at = old_saved_at;
        s_autosave_paused = old_autosave_paused;
        s_dirty = old_dirty;
        s_unpersisted = old_unpersisted;
        s_dirty_at = old_dirty_at;
        s_last_save_mono = old_last_save_mono;
        if (restored) {
            reconcile_all();
            gsj_set_error(error, error_cap, "import has an invalid fragment");
        } else {
            gsj_set_error(error, error_cap, "failed to restore state after rejected import");
        }
        return false;
    }
    cJSON_Delete(old_orphans);
    for (int i = 0; i < s_fragment_count; i++) {
        cJSON_Delete(snapshots[i]);
    }
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

#ifdef GAME_SAVE_TESTING
void game_save__set_clocks_for_test(int64_t (*mono)(void), int64_t (*wall)(void)) {
    s_mono_clock = mono;
    s_wall_clock = wall;
}
#endif
