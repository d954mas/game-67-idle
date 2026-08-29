#include "game_save.h"

#include "game_storage.h"
#if !defined(GAME_SAVE_TESTING)
#include "game_save_platform.h"
#endif
#include "log/nt_log.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#ifndef GAME_SAVE_BUILD
#define GAME_SAVE_BUILD "0"
#endif
#ifndef GAME_STORAGE_APP_ID
#define GAME_STORAGE_APP_ID "game"
#endif

#define GAME_SAVE_FORMAT 1
#define GAME_SAVE_ERROR_CAPACITY 192

void gsj_set_error(char *error, int error_cap, const char *message);

typedef struct {
    const GameSaveFragment *fragment;
    size_t body_start;
    size_t body_end;
    uint32_t first_line;
    int version;
    bool present;
} text_fragment_t;

typedef struct {
    text_fragment_t fragments[GAME_SAVE_MAX_FRAGMENTS];
    int64_t saved_at;
    int64_t save_seq;
} text_document_t;

typedef enum {
    TEXT_DOCUMENT_INVALID = 0,
    TEXT_DOCUMENT_CURRENT,
    TEXT_DOCUMENT_NEWER,
} text_document_status_t;

static const GameSaveFragment *s_fragments[GAME_SAVE_MAX_FRAGMENTS];
static int s_fragment_count;
static GameSaveLiveValidateFn s_live_validator;
static char *s_snapshot;
static size_t s_snapshot_capacity;
static bool s_autosave_paused;
static bool s_quarantine_owed;
static bool s_new_game_pending;
static char s_new_game_skip_id[32];
static bool s_dirty;
static bool s_unpersisted;
static int64_t s_dirty_at;
static int64_t s_last_save_mono;
static int64_t s_last_saved_at;
static int64_t s_save_seq;
static int64_t (*s_mono_clock)(void);
static int64_t (*s_wall_clock)(void);

#if defined(GAME_SAVE_TESTING)
static int64_t default_mono_ms(void) { return 0; }
static int64_t default_wall_ms(void) { return 0; }
#else
static int64_t default_mono_ms(void) { return game_save_platform_mono_ms(); }
static int64_t default_wall_ms(void) { return game_save_platform_wall_ms(); }
#endif

static int64_t mono_now(void) { return s_mono_clock != NULL ? s_mono_clock() : 0; }
static int64_t wall_now(void) { return s_wall_clock != NULL ? s_wall_clock() : 0; }

static void set_error(char *error, int error_cap, const char *message) {
    if (error != NULL && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
}

void gsj_set_error(char *error, int error_cap, const char *message) {
    set_error(error, error_cap, message);
}

bool gsj_copy_text(char *dst, size_t dst_cap, const char *src) {
    if (dst == NULL || dst_cap == 0U || src == NULL) {
        return false;
    }
    const size_t size = strlen(src);
    if (size >= dst_cap) {
        dst[0] = '\0';
        return false;
    }
    memcpy(dst, src, size + 1U);
    return true;
}

static void set_result_message(game_save_load_result_t *result, const char *message) {
    (void)snprintf(result->message, sizeof result->message, "%s", message);
}

static int fragment_index_slice(const char *id, size_t size) {
    for (int i = 0; i < s_fragment_count; i++) {
        if (strlen(s_fragments[i]->id) == size && memcmp(s_fragments[i]->id, id, size) == 0) {
            return i;
        }
    }
    return -1;
}

static void reset_all_except(const char *skip_id) {
    for (int i = 0; i < s_fragment_count; i++) {
        const GameSaveFragment *fragment = s_fragments[i];
        if (skip_id != NULL && strcmp(fragment->id, skip_id) == 0) {
            continue;
        }
        if (fragment->reset != NULL) {
            fragment->reset();
        }
    }
}

static void seed_all_except(const char *skip_id) {
    for (int i = 0; i < s_fragment_count; i++) {
        const GameSaveFragment *fragment = s_fragments[i];
        if (skip_id != NULL && strcmp(fragment->id, skip_id) == 0) {
            continue;
        }
        if (fragment->on_new_game != NULL) {
            fragment->on_new_game();
        }
    }
}

static void reconcile_all(void) {
    for (int i = 0; i < s_fragment_count; i++) {
        if (s_fragments[i]->reconcile != NULL) {
            s_fragments[i]->reconcile();
        }
    }
}

static bool validate_live(char *error, int error_cap) {
    return s_live_validator == NULL || s_live_validator(error, error_cap);
}

static bool write_document(
    game_save_text_writer_t *writer, int64_t saved_at, int64_t save_seq) {
    if (!game_save_text_write_preamble(writer) ||
        !game_save_text_write_i64(writer, "format", GAME_SAVE_FORMAT) ||
        !game_save_text_write_i64(writer, "save_version", GAME_SAVE_DOC_VERSION) ||
        !game_save_text_write_i64(writer, "saved_at", saved_at) ||
        !game_save_text_write_i64(writer, "save_seq", save_seq) ||
        !game_save_text_write_string(writer, "app", GAME_STORAGE_APP_ID) ||
        !game_save_text_write_string(writer, "build", GAME_SAVE_BUILD)) {
        return false;
    }
    for (int i = 0; i < s_fragment_count; i++) {
        const GameSaveFragment *fragment = s_fragments[i];
        if (fragment->write_text == NULL ||
            !game_save_text_begin_fragment(writer, fragment->id, fragment->version) ||
            !fragment->write_text(writer)) {
            return false;
        }
    }
    return game_save_text_writer_ok(writer);
}

static bool save_current(char *error, int error_cap, bool may_wait) {
    char reason[GAME_SAVE_ERROR_CAPACITY] = {0};
    if (s_save_seq == INT64_MAX) {
        set_error(reason, (int)sizeof reason, "save sequence exhausted");
    } else if (!validate_live(reason, (int)sizeof reason)) {
    } else if (s_snapshot == NULL || s_snapshot_capacity == 0U) {
        set_error(reason, (int)sizeof reason, "save snapshot buffer is not configured");
    } else {
        const int64_t saved_at = wall_now();
        const int64_t save_seq = s_save_seq + 1;
        game_save_text_writer_t writer;
        game_save_text_writer_init(&writer, s_snapshot, s_snapshot_capacity);
        if (!write_document(&writer, saved_at, save_seq)) {
            set_error(reason, (int)sizeof reason, "save snapshot exceeds configured capacity");
        } else {
            const bool written = may_wait
                ? game_storage_write_blocking(
                      GAME_SAVE_AUTOSAVE_SLOT, game_save_text_writer_data(&writer),
                      reason, (int)sizeof reason)
                : game_storage_write(
                      GAME_SAVE_AUTOSAVE_SLOT, game_save_text_writer_data(&writer),
                      reason, (int)sizeof reason);
            if (written) {
                s_save_seq = save_seq;
                s_last_saved_at = saved_at;
                s_last_save_mono = mono_now();
                s_dirty = false;
                s_unpersisted = false;
                return true;
            }
        }
    }

    s_unpersisted = true;
    s_dirty = true;
    s_dirty_at = mono_now();
    s_last_save_mono = s_dirty_at;
    set_error(error, error_cap, reason[0] != '\0' ? reason : "save failed");
    nt_log_warn("game_save: save failed (%s); data kept in memory", reason);
    return false;
}

static bool read_meta_i64(
    const game_save_text_record_t *record, int64_t min, int64_t max,
    int64_t *out, char *error, int error_cap) {
    return game_save_text_record_i64(
        record, min, max, out, error, error_cap > 0 ? (size_t)error_cap : 0U);
}

static text_document_status_t scan_document(
    const char *text, text_document_t *document, char *error, int error_cap) {
    if (text == NULL || strncmp(text, "NTGS 1", 6U) != 0) {
        set_error(error, error_cap, "expected NTGS save; legacy JSON is unsupported");
        return TEXT_DOCUMENT_INVALID;
    }
    memset(document, 0, sizeof *document);
    for (int i = 0; i < s_fragment_count; i++) {
        document->fragments[i].fragment = s_fragments[i];
    }

    enum {
        META_FORMAT = 1U << 0,
        META_VERSION = 1U << 1,
        META_SAVED_AT = 1U << 2,
        META_SAVE_SEQ = 1U << 3,
        META_APP = 1U << 4,
        META_BUILD = 1U << 5,
        META_REQUIRED = (1U << 6) - 1U,
    };
    unsigned metadata = 0U;
    int64_t format = 0;
    int64_t save_version = 0;
    char app[96] = {0};
    char build[96] = {0};
    int open_fragment = -1;
    bool unknown_fragment = false;

    game_save_text_reader_t reader;
    game_save_text_record_t record;
    game_save_text_reader_init(&reader, text, strlen(text));
    for (;;) {
        const game_save_text_result_t result = game_save_text_reader_next(
            &reader, &record, error, error_cap > 0 ? (size_t)error_cap : 0U);
        if (result == GAME_SAVE_TEXT_DONE) {
            break;
        }
        if (result == GAME_SAVE_TEXT_ERROR) {
            return TEXT_DOCUMENT_INVALID;
        }
        if (result == GAME_SAVE_TEXT_RECORD_FRAGMENT) {
            if (open_fragment >= 0) {
                document->fragments[open_fragment].body_end = record.source_offset;
            }
            open_fragment = fragment_index_slice(record.key, record.key_size);
            unknown_fragment = unknown_fragment || open_fragment < 0;
            if (open_fragment >= 0) {
                text_fragment_t *fragment = &document->fragments[open_fragment];
                if (fragment->present) {
                    set_error(error, error_cap, "duplicate save fragment");
                    return TEXT_DOCUMENT_INVALID;
                }
                fragment->present = true;
                fragment->version = record.version;
                fragment->body_start = record.next_offset;
                fragment->body_end = strlen(text);
                fragment->first_line = record.line + 1U;
            }
            continue;
        }
        if (result == GAME_SAVE_TEXT_RECORD_FIELD) {
            continue;
        }

        unsigned bit = 0U;
        bool valid = true;
        if (game_save_text_record_key_is(&record, "format")) {
            bit = META_FORMAT;
            valid = read_meta_i64(&record, 1, INT_MAX, &format, error, error_cap);
        } else if (game_save_text_record_key_is(&record, "save_version")) {
            bit = META_VERSION;
            valid = read_meta_i64(&record, 1, INT_MAX, &save_version, error, error_cap);
        } else if (game_save_text_record_key_is(&record, "saved_at")) {
            bit = META_SAVED_AT;
            valid = read_meta_i64(&record, 0, INT64_MAX, &document->saved_at, error, error_cap);
        } else if (game_save_text_record_key_is(&record, "save_seq")) {
            bit = META_SAVE_SEQ;
            valid = read_meta_i64(&record, 0, INT64_MAX, &document->save_seq, error, error_cap);
        } else if (game_save_text_record_key_is(&record, "app")) {
            bit = META_APP;
            valid = game_save_text_record_string(
                &record, app, sizeof app, error, error_cap > 0 ? (size_t)error_cap : 0U);
        } else if (game_save_text_record_key_is(&record, "build")) {
            bit = META_BUILD;
            valid = game_save_text_record_string(
                &record, build, sizeof build, error, error_cap > 0 ? (size_t)error_cap : 0U);
        }
        if (!valid) {
            return TEXT_DOCUMENT_INVALID;
        }
        if (bit != 0U) {
            if ((metadata & bit) != 0U) {
                set_error(error, error_cap, "duplicate save metadata");
                return TEXT_DOCUMENT_INVALID;
            }
            metadata |= bit;
        }
    }
    if (open_fragment >= 0) {
        document->fragments[open_fragment].body_end = strlen(text);
    }
    if ((metadata & META_REQUIRED) != META_REQUIRED) {
        set_error(error, error_cap, "save metadata is incomplete");
        return TEXT_DOCUMENT_INVALID;
    }
    if (format > GAME_SAVE_FORMAT || save_version > GAME_SAVE_DOC_VERSION) {
        return TEXT_DOCUMENT_NEWER;
    }
    if (format != GAME_SAVE_FORMAT || save_version != GAME_SAVE_DOC_VERSION ||
        strcmp(app, GAME_STORAGE_APP_ID) != 0) {
        set_error(error, error_cap, "save format, version, or app is unsupported");
        return TEXT_DOCUMENT_INVALID;
    }
    if (unknown_fragment) {
        return TEXT_DOCUMENT_NEWER;
    }
    for (int i = 0; i < s_fragment_count; i++) {
        const text_fragment_t *fragment = &document->fragments[i];
        if (fragment->present && fragment->version > fragment->fragment->version) {
            return TEXT_DOCUMENT_NEWER;
        }
        if (fragment->present && fragment->version != fragment->fragment->version) {
            set_error(error, error_cap, "save fragment version is unsupported");
            return TEXT_DOCUMENT_INVALID;
        }
    }
    (void)build;
    return TEXT_DOCUMENT_CURRENT;
}

static bool publish_document(
    const char *text, const text_document_t *document,
    game_save_load_result_t *result, bool strict, char *error, int error_cap) {
    bool valid = true;
    for (int i = 0; i < s_fragment_count; i++) {
        const text_fragment_t *section = &document->fragments[i];
        const GameSaveFragment *fragment = section->fragment;
        if (!section->present) {
            if (fragment->reset != NULL) {
                fragment->reset();
            }
            continue;
        }
        char fragment_error[128] = {0};
        const bool loaded = fragment->from_text != NULL && fragment->from_text(
            text + section->body_start, section->body_end - section->body_start,
            fragment_error, (int)sizeof fragment_error);
        if (!loaded) {
            valid = false;
            if (strict) {
                set_error(error, error_cap,
                          fragment_error[0] != '\0' ? fragment_error : "invalid save fragment");
                break;
            }
            if (fragment->reset != NULL) {
                fragment->reset();
            }
            if (result != NULL && result->reset_fragment_count < GAME_SAVE_MAX_FRAGMENTS) {
                result->reset_fragments[result->reset_fragment_count++] = fragment->id;
            }
            nt_log_warn("game_save: fragment '%s' failed to load, reset (%s)",
                        fragment->id, fragment_error);
        }
    }
    if (!valid && strict) {
        return false;
    }
    s_save_seq = document->save_seq;
    s_last_saved_at = document->saved_at;
    reconcile_all();
    return true;
}

static bool load_text(
    const char *text, game_save_load_result_t *result,
    bool strict, bool *newer, char *error, int error_cap) {
    text_document_t document;
    const text_document_status_t status = scan_document(text, &document, error, error_cap);
    if (newer != NULL) {
        *newer = status == TEXT_DOCUMENT_NEWER;
    }
    return status == TEXT_DOCUMENT_CURRENT &&
           publish_document(text, &document, result, strict, error, error_cap);
}

static void start_fresh(game_save_load_result_t *result, const char *message) {
    reset_all_except(NULL);
    seed_all_except(NULL);
    s_autosave_paused = false;
    s_quarantine_owed = false;
    game_save_mark_dirty();
    char error[128] = {0};
    (void)save_current(error, (int)sizeof error, true);
    result->status = GAME_SAVE_LOAD_FRESH;
    set_result_message(result, message);
    s_last_save_mono = mono_now();
}

static bool recover_backup(game_save_load_result_t *result, char *error, int error_cap) {
    char *backup = NULL;
    if (!game_storage_read_backup(GAME_SAVE_AUTOSAVE_SLOT, &backup, error, error_cap)) {
        return false;
    }
    bool newer = false;
    const bool loaded = load_text(backup, result, false, &newer, error, error_cap);
    free(backup);
    if (!loaded) {
        return false;
    }
    s_autosave_paused = false;
    result->status = GAME_SAVE_LOAD_RECOVERED_BAK;
    set_result_message(result, "primary invalid; recovered from backup");
    game_save_mark_dirty();
    (void)save_current(error, error_cap, true);
    return true;
}

static void quarantine_and_start(
    game_save_load_result_t *result, game_save_load_status_t status,
    const char *message, char *error, int error_cap) {
    if (!game_storage_quarantine(GAME_SAVE_AUTOSAVE_SLOT, error, error_cap)) {
        reset_all_except(NULL);
        seed_all_except(NULL);
        s_quarantine_owed = true;
        s_autosave_paused = true;
        result->status = GAME_SAVE_LOAD_BLOCKED;
        set_result_message(result, "save could not be set aside; writes paused");
        return;
    }
    reset_all_except(NULL);
    seed_all_except(NULL);
    s_quarantine_owed = false;
    s_autosave_paused = false;
    game_save_mark_dirty();
    (void)save_current(error, error_cap, true);
    result->status = status;
    set_result_message(result, message);
}

void game_save_register_fragment(const GameSaveFragment *fragment) {
    if (fragment != NULL && s_fragment_count < GAME_SAVE_MAX_FRAGMENTS) {
        s_fragments[s_fragment_count++] = fragment;
    }
}

void game_save_set_document_migrations(
    const GameSaveDocumentMigrateFn *steps, int step_count) {
    (void)steps;
    (void)step_count;
}

void game_save_set_document_validator(GameSaveDocumentValidateFn validator) {
    (void)validator;
}

void game_save_set_live_validator(GameSaveLiveValidateFn validator) {
    s_live_validator = validator;
}

void game_save_set_hot_snapshot_buffer(char *buffer, size_t capacity) {
    s_snapshot = buffer;
    s_snapshot_capacity = buffer != NULL ? capacity : 0U;
}

bool game_save_validate_current(char *error, int error_cap) {
    return validate_live(error, error_cap);
}

int game_save_fragment_count(void) { return s_fragment_count; }

const GameSaveFragment *game_save_fragment_at(int index) {
    return index >= 0 && index < s_fragment_count ? s_fragments[index] : NULL;
}

const GameSaveFragment *game_save_find_fragment(const char *id) {
    if (id == NULL) {
        return NULL;
    }
    for (int i = 0; i < s_fragment_count; i++) {
        if (strcmp(s_fragments[i]->id, id) == 0) {
            return s_fragments[i];
        }
    }
    return NULL;
}

bool game_save_reconcile_from(const char *id) {
    const GameSaveFragment *fragment = game_save_find_fragment(id);
    if (fragment == NULL) {
        return false;
    }
    bool reached = false;
    for (int i = 0; i < s_fragment_count; i++) {
        reached = reached || s_fragments[i] == fragment;
        if (reached && s_fragments[i]->reconcile != NULL) {
            s_fragments[i]->reconcile();
        }
    }
    return true;
}

int game_save_orphan_count(void) { return 0; }

const cJSON *game_save_orphan_at(int index, const char **id) {
    (void)index;
    (void)id;
    return NULL;
}

void game_save_init(void) {
    if (s_mono_clock == NULL) {
        s_mono_clock = default_mono_ms;
    }
    if (s_wall_clock == NULL) {
        s_wall_clock = default_wall_ms;
    }
    s_autosave_paused = false;
    s_quarantine_owed = false;
    s_new_game_pending = false;
    s_new_game_skip_id[0] = '\0';
    s_dirty = false;
    s_unpersisted = false;
    s_dirty_at = 0;
    s_last_save_mono = 0;
    s_last_saved_at = 0;
    s_save_seq = 0;
    char error[128] = {0};
    if (!game_storage_probe(error, (int)sizeof error)) {
        s_unpersisted = true;
    }
}

void game_save_load(game_save_load_result_t *result) {
    game_save_load_result_t local;
    if (result == NULL) {
        result = &local;
    }
    memset(result, 0, sizeof *result);
    char error[128] = {0};
    char *text = NULL;
    game_storage_read_status_t read_status = GAME_STORAGE_READ_ABSENT;
    if (!game_storage_read(
            GAME_SAVE_AUTOSAVE_SLOT, &text, &read_status, error, (int)sizeof error)) {
        if (read_status == GAME_STORAGE_READ_ABSENT) {
            start_fresh(result, "no save found; new game");
        } else if (read_status == GAME_STORAGE_READ_ERROR_PRESERVED) {
            reset_all_except(NULL);
            seed_all_except(NULL);
            s_quarantine_owed = true;
            s_autosave_paused = true;
            result->status = GAME_SAVE_LOAD_BLOCKED;
            set_result_message(result, "save unreadable; writes paused");
        } else if (!recover_backup(result, error, (int)sizeof error)) {
            start_fresh(result, "unreadable save reset");
        }
        return;
    }

    bool newer = false;
    if (load_text(text, result, false, &newer, error, (int)sizeof error)) {
        free(text);
        s_autosave_paused = false;
        result->status = GAME_SAVE_LOAD_LOADED;
        set_result_message(result, "loaded");
        (void)game_storage_write_backup(
            GAME_SAVE_AUTOSAVE_SLOT, error, (int)sizeof error);
        s_last_save_mono = mono_now();
        return;
    }
    free(text);
    if (recover_backup(result, error, (int)sizeof error)) {
        return;
    }
    quarantine_and_start(
        result, newer ? GAME_SAVE_LOAD_NEWER : GAME_SAVE_LOAD_CORRUPT_RESET,
        newer ? "newer save reset" : "invalid save reset",
        error, (int)sizeof error);
}

static void begin_new_game(const char *skip_id) {
    reset_all_except(skip_id);
    seed_all_except(skip_id);
    if (s_quarantine_owed) {
        char error[128] = {0};
        if (game_storage_quarantine(
                GAME_SAVE_AUTOSAVE_SLOT, error, (int)sizeof error)) {
            s_quarantine_owed = false;
            s_autosave_paused = false;
        }
    } else {
        s_autosave_paused = false;
    }
    game_save_mark_dirty();
}

static game_save_transition_result_t new_game(
    const char *skip_id, char *error, int error_cap, bool may_wait) {
    begin_new_game(skip_id);
    if (s_quarantine_owed) {
        set_error(error, error_cap, "save is held until the previous file can be set aside");
        return (game_save_transition_result_t){.state_changed = true, .persisted = false};
    }
    return (game_save_transition_result_t){
        .state_changed = true,
        .persisted = save_current(error, error_cap, may_wait),
    };
}

game_save_transition_result_t game_save_new_game(char *error, int error_cap) {
    return new_game(NULL, error, error_cap, true);
}

void game_save_begin_new_game_transition(void) { begin_new_game(NULL); }

void game_save_request_new_game(const char *skip_fragment_id) {
    s_new_game_pending = true;
    (void)snprintf(
        s_new_game_skip_id, sizeof s_new_game_skip_id, "%s",
        skip_fragment_id != NULL ? skip_fragment_id : "");
}

game_save_transition_result_t game_save_apply_pending_new_game(void) {
    if (!s_new_game_pending) {
        return (game_save_transition_result_t){0};
    }
    s_new_game_pending = false;
    char error[128] = {0};
    return new_game(
        s_new_game_skip_id[0] != '\0' ? s_new_game_skip_id : NULL,
        error, (int)sizeof error, false);
}

bool game_save_flush(char *error, int error_cap) {
    if (s_autosave_paused) {
        set_error(error, error_cap, "save writes are paused");
        return false;
    }
    return save_current(error, error_cap, true);
}

void game_save_tick(void) {
    if (s_quarantine_owed) {
        char error[128] = {0};
        if (game_storage_quarantine(
                GAME_SAVE_AUTOSAVE_SLOT, error, (int)sizeof error)) {
            s_quarantine_owed = false;
            s_autosave_paused = false;
            game_save_mark_dirty();
        }
    }
    if (s_autosave_paused || !s_dirty) {
        return;
    }
    const int64_t now = mono_now();
    if (now - s_dirty_at >= GAME_SAVE_DEBOUNCE_MS ||
        now - s_last_save_mono >= GAME_SAVE_MAX_INTERVAL_MS) {
        char error[128] = {0};
        (void)save_current(error, (int)sizeof error, false);
    }
}

void game_save_mark_dirty(void) {
    if (!s_dirty) {
        s_dirty = true;
        s_dirty_at = mono_now();
    }
}

int64_t game_save_last_saved_at(void) { return s_last_saved_at; }

bool game_save_is_unpersisted(void) { return s_unpersisted; }

game_save_persistence_t game_save_persistence(void) {
    if (s_autosave_paused) {
        return GAME_SAVE_PERSISTENCE_PAUSED;
    }
    return s_unpersisted ? GAME_SAVE_PERSISTENCE_RETRYING : GAME_SAVE_PERSISTENCE_OK;
}

char *game_save_export_string(char *error, int error_cap) {
    if (!validate_live(error, error_cap)) {
        return NULL;
    }
    char *text = (char *)malloc((size_t)GAME_STORAGE_MAX_BYTES + 1U);
    if (text == NULL) {
        set_error(error, error_cap, "failed to allocate export document");
        return NULL;
    }
    game_save_text_writer_t writer;
    game_save_text_writer_init(&writer, text, (size_t)GAME_STORAGE_MAX_BYTES + 1U);
    if (!write_document(&writer, wall_now(), s_save_seq)) {
        free(text);
        set_error(error, error_cap, "export exceeds storage size limit");
        return NULL;
    }
    return text;
}

bool game_save_import_string(const char *text, char *error, int error_cap) {
    if (text == NULL || strlen(text) > GAME_STORAGE_MAX_BYTES) {
        set_error(error, error_cap, text == NULL ? "import text is required" :
                                             "import exceeds storage size limit");
        return false;
    }
    char *snapshot = game_save_export_string(error, error_cap);
    if (snapshot == NULL) {
        return false;
    }
    const int64_t old_seq = s_save_seq;
    const int64_t old_saved_at = s_last_saved_at;
    game_save_load_result_t result = {0};
    bool newer = false;
    if (!load_text(text, &result, true, &newer, error, error_cap)) {
        text_document_t old_document;
        if (scan_document(snapshot, &old_document, NULL, 0) == TEXT_DOCUMENT_CURRENT) {
            (void)publish_document(snapshot, &old_document, NULL, false, NULL, 0);
        }
        s_save_seq = old_seq;
        s_last_saved_at = old_saved_at;
        free(snapshot);
        if (newer) {
            set_error(error, error_cap, "import is newer than this build");
        }
        return false;
    }
    free(snapshot);
    if (s_save_seq < old_seq) {
        s_save_seq = old_seq;
    }
    s_autosave_paused = false;
    game_save_mark_dirty();
    return true;
}

void game_save_set_transforms(const game_save_transform_t *chain, int count) {
    (void)chain;
    (void)count;
}

#if defined(GAME_SAVE_TESTING)
void game_save__set_clocks_for_test(
    int64_t (*mono)(void), int64_t (*wall)(void)) {
    s_mono_clock = mono;
    s_wall_clock = wall;
}

cJSON *game_save__parse_document_for_test(
    const char *text, char *error, int error_cap) {
    (void)text;
    set_error(error, error_cap, "cJSON parser is unavailable in text-only saves");
    return NULL;
}
#endif
