#include "game_state_doc.h"

#include "cJSON.h"
#include "game_state_json.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { HEADER_SAVE_VERSION = 1, HEADER_SAVE_ID = 2, HEADER_REV = 4, HEADER_ALL = 7 };

/* Longest fragment id or dotted field path a migration stages. */
#define DOC_KEY_MAX 128

static size_t error_size(int error_cap) { return error_cap > 0 ? (size_t)error_cap : 0U; }

static bool record_slice_is(const char *start, size_t size, const char *text) {
    return strlen(text) == size && memcmp(start, text, size) == 0;
}

static int find_fragment(const game_state_doc_schema_t *schema, const char *id, size_t id_size) {
    for (int i = 0; i < schema->fragment_count; i++) {
        if (record_slice_is(id, id_size, schema->fragments[i]->id)) return i;
    }
    return -1;
}

static bool parse_save_id(const game_save_text_record_t *record, uint64_t *out, char *error, int error_cap) {
    char hex[17];
    if (!game_save_text_record_string(record, hex, sizeof hex, error, error_size(error_cap)) || strlen(hex) != 16U) {
        gsj_set_error(error, error_cap, "save_id must be 16 hex digits");
        return false;
    }
    uint64_t value = 0;
    for (int i = 0; i < 16; i++) {
        const char c = hex[i];
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (digit < 0) {
            gsj_set_error(error, error_cap, "save_id must be 16 lowercase hex digits");
            return false;
        }
        value = (value << 4) | (uint64_t)digit;
    }
    *out = value;
    return true;
}

static bool read_meta(const game_save_text_record_t *record, game_state_doc_header_t *header, unsigned *seen,
                      char *error, int error_cap) {
    unsigned bit = 0;
    bool ok = true;
    if (game_save_text_record_key_is(record, "save_version")) {
        int64_t value = 0;
        ok = game_save_text_record_i64(record, 1, INT32_MAX, &value, error, error_size(error_cap));
        header->save_version = (int)value;
        bit = HEADER_SAVE_VERSION;
    } else if (game_save_text_record_key_is(record, "save_id")) {
        ok = parse_save_id(record, &header->save_id, error, error_cap);
        bit = HEADER_SAVE_ID;
    } else if (game_save_text_record_key_is(record, "rev")) {
        ok = game_save_text_record_i64(record, 0, INT64_MAX, &header->rev, error, error_size(error_cap));
        bit = HEADER_REV;
    } else {
        gsj_set_error(error, error_cap, "unknown document header key");
        return false;
    }
    if (!ok) return false;
    if (*seen & bit) {
        gsj_set_error(error, error_cap, "duplicate document header key");
        return false;
    }
    *seen |= bit;
    return true;
}

/* One pass over the document: the header, then every fragment as the slice of
   its body. `on_fragment` sees each; a NULL callback reads the header only. */
typedef bool (*fragment_fn)(void *user, const game_save_text_record_t *header_record, const char *body,
                            size_t body_size, char *error, int error_cap);

static bool walk_document(const char *text, size_t size, game_state_doc_header_t *header, fragment_fn on_fragment,
                          void *user, char *error, int error_cap) {
    if (text == NULL || header == NULL) {
        gsj_set_error(error, error_cap, "document is null");
        return false;
    }
    *header = (game_state_doc_header_t){0};
    unsigned seen = 0;
    game_save_text_reader_t reader;
    game_save_text_record_t record;
    game_save_text_record_t open = {0};
    bool has_open = false;
    game_save_text_reader_init(&reader, text, size);
    for (;;) {
        const game_save_text_result_t result = game_save_text_reader_next(&reader, &record, error, error_size(error_cap));
        if (result == GAME_SAVE_TEXT_ERROR) return false;
        if (result == GAME_SAVE_TEXT_RECORD_META) {
            if (!read_meta(&record, header, &seen, error, error_cap)) return false;
            continue;
        }
        if (result == GAME_SAVE_TEXT_RECORD_FIELD) continue;
        if (seen != HEADER_ALL) {
            gsj_set_error(error, error_cap, "document header needs save_version, save_id and rev");
            return false;
        }
        if (on_fragment == NULL) return true;
        const size_t body_end = result == GAME_SAVE_TEXT_DONE ? size : record.source_offset;
        if (has_open && !on_fragment(user, &open, text + open.next_offset, body_end - open.next_offset, error,
                                     error_cap)) {
            return false;
        }
        if (result == GAME_SAVE_TEXT_DONE) return true;
        open = record;
        has_open = true;
    }
}

bool game_state_doc_read_header(const char *text, size_t size, game_state_doc_header_t *header, char *error,
                                int error_cap) {
    return walk_document(text, size, header, NULL, NULL, error, error_cap);
}

bool game_state_doc_write(const game_state_doc_schema_t *schema, const game_state_doc_header_t *header,
                          const void *const *states, game_save_text_writer_t *writer, char *error, int error_cap) {
    if (schema == NULL || header == NULL || states == NULL || writer == NULL || header->rev < 0) {
        gsj_set_error(error, error_cap, "invalid document write");
        return false;
    }
    for (int i = 0; i < schema->fragment_count; i++) {
        if (!schema->fragments[i]->validate(states[i], error, error_cap)) return false;
    }
    char save_id[17];
    (void)snprintf(save_id, sizeof save_id, "%016llx", (unsigned long long)header->save_id);
    bool ok = game_save_text_write_preamble(writer) &&
              game_save_text_write_i64(writer, "save_version", schema->save_version) &&
              game_save_text_write_string(writer, "save_id", save_id) &&
              game_save_text_write_i64(writer, "rev", header->rev);
    for (int i = 0; ok && i < schema->fragment_count; i++) {
        const game_state_doc_fragment_t *fragment = schema->fragments[i];
        ok = game_save_text_begin_fragment(writer, fragment->id, fragment->version) &&
             fragment->write_text(states[i], writer);
    }
    if (!ok || !game_save_text_writer_ok(writer)) {
        gsj_set_error(error, error_cap, "document does not fit the writer");
        return false;
    }
    return true;
}

typedef struct {
    const game_state_doc_schema_t *schema;
    void *const *states;
    bool seen[GAME_SAVE_MAX_FRAGMENTS];
} read_context_t;

static bool read_fragment(void *user, const game_save_text_record_t *header_record, const char *body,
                          size_t body_size, char *error, int error_cap) {
    read_context_t *context = (read_context_t *)user;
    const int index = find_fragment(context->schema, header_record->key, header_record->key_size);
    if (index < 0) {
        gsj_set_error(error, error_cap, "document has a fragment the schema lacks");
        return false;
    }
    const game_state_doc_fragment_t *fragment = context->schema->fragments[index];
    if (context->seen[index]) {
        gsj_set_error(error, error_cap, "duplicate fragment");
        return false;
    }
    context->seen[index] = true;
    if (header_record->version != fragment->version) {
        gsj_set_error(error, error_cap, "fragment version differs from the schema; migrate first");
        return false;
    }
    return fragment->from_text(context->states[index], body, body_size, error, error_cap);
}

bool game_state_doc_read(const game_state_doc_schema_t *schema, const char *text, size_t size,
                         game_state_doc_header_t *header, void *const *states, char *error, int error_cap) {
    if (schema == NULL || states == NULL || schema->fragment_count > GAME_SAVE_MAX_FRAGMENTS) {
        gsj_set_error(error, error_cap, "invalid document schema");
        return false;
    }
    for (int i = 0; i < schema->fragment_count; i++) schema->fragments[i]->reset(states[i]);
    game_state_doc_header_t parsed;
    if (!game_state_doc_read_header(text, size, &parsed, error, error_cap)) return false;
    if (parsed.save_version != schema->save_version) {
        gsj_set_error(error, error_cap,
                      parsed.save_version > schema->save_version ? "document is newer than this build"
                                                                 : "document save_version is old; migrate first");
        return false;
    }
    read_context_t context = {.schema = schema, .states = states};
    if (!walk_document(text, size, &parsed, read_fragment, &context, error, error_cap)) {
        for (int i = 0; i < schema->fragment_count; i++) schema->fragments[i]->reset(states[i]);
        return false;
    }
    if (header != NULL) *header = parsed;
    return true;
}

/* ---- Migration: through cJSON, the representation every step is written against ---- */

typedef struct {
    const game_state_doc_schema_t *schema;
    bool current;
} current_context_t;

static bool fragment_is_current(void *user, const game_save_text_record_t *header_record, const char *body,
                                size_t body_size, char *error, int error_cap) {
    (void)body;
    (void)body_size;
    (void)error;
    (void)error_cap;
    current_context_t *context = (current_context_t *)user;
    const int index = find_fragment(context->schema, header_record->key, header_record->key_size);
    if (index < 0 || header_record->version != context->schema->fragments[index]->version) context->current = false;
    return true;
}

static cJSON *record_value(const game_save_text_record_t *record, char *error, int error_cap) {
    if (record->value_size >= 2U && record->value[0] == '"') {
        char *value = (char *)malloc(record->value_size + 1U);
        if (value == NULL) return NULL;
        cJSON *json = game_save_text_record_string(record, value, record->value_size + 1U, error, error_size(error_cap))
                          ? cJSON_CreateString(value)
                          : NULL;
        free(value);
        return json;
    }
    if (game_save_text_record_is_null(record)) return cJSON_CreateNull();
    bool boolean = false;
    if (game_save_text_record_bool(record, &boolean, NULL, 0U)) return cJSON_CreateBool(boolean);
    double number = 0.0;
    if (game_save_text_record_number(record, -DBL_MAX, DBL_MAX, &number, error, error_size(error_cap))) {
        return cJSON_CreateNumber(number);
    }
    return NULL;
}

/* Dotted keys are nested objects, as game_save stages them for its migrations. */
static bool add_field(cJSON *fragment, const game_save_text_record_t *record, char *error, int error_cap) {
    const char *part = record->key;
    const char *end = record->key + record->key_size;
    cJSON *owner = fragment;
    char key[DOC_KEY_MAX];
    for (;;) {
        const char *dot = memchr(part, '.', (size_t)(end - part));
        const char *part_end = dot != NULL ? dot : end;
        const size_t length = (size_t)(part_end - part);
        if (length == 0U || length >= sizeof key) {
            gsj_set_error(error, error_cap, "invalid field path");
            return false;
        }
        memcpy(key, part, length);
        key[length] = '\0';
        cJSON *existing = cJSON_GetObjectItemCaseSensitive(owner, key);
        if (dot == NULL) {
            cJSON *value = existing == NULL ? record_value(record, error, error_cap) : NULL;
            if (value == NULL || !cJSON_AddItemToObject(owner, key, value)) {
                cJSON_Delete(value);
                gsj_set_error(error, error_cap, existing != NULL ? "duplicate field" : "invalid field value");
                return false;
            }
            return true;
        }
        if (existing == NULL) {
            existing = cJSON_AddObjectToObject(owner, key);
        } else if (!cJSON_IsObject(existing)) {
            existing = NULL;
        }
        if (existing == NULL) {
            gsj_set_error(error, error_cap, "field path conflicts with a value");
            return false;
        }
        owner = existing;
        part = dot + 1;
    }
}

/* A fragment's body as the document wrote it, so a fragment no step touches is
   copied byte for byte instead of passing through cJSON doubles. */
typedef struct {
    char id[DOC_KEY_MAX];
    const char *body;
    size_t body_size;
} source_fragment_t;

typedef struct {
    source_fragment_t items[GAME_SAVE_MAX_FRAGMENTS];
    int count;
} source_fragments_t;

static const source_fragment_t *find_source(const source_fragments_t *sources, const char *id) {
    for (int i = 0; i < sources->count; i++) {
        if (strcmp(sources->items[i].id, id) == 0) return &sources->items[i];
    }
    return NULL;
}

static cJSON *parse_features(const char *text, size_t size, source_fragments_t *sources, char *error,
                             int error_cap) {
    cJSON *features = cJSON_CreateObject();
    if (features == NULL) {
        gsj_set_error(error, error_cap, "failed to stage document");
        return NULL;
    }
    sources->count = 0;
    game_save_text_reader_t reader;
    game_save_text_record_t record;
    cJSON *fragment = NULL;
    game_save_text_reader_init(&reader, text, size);
    for (;;) {
        const game_save_text_result_t result = game_save_text_reader_next(&reader, &record, error, error_size(error_cap));
        if (result == GAME_SAVE_TEXT_ERROR) break;
        if (result == GAME_SAVE_TEXT_RECORD_META) continue;
        if (result == GAME_SAVE_TEXT_DONE || result == GAME_SAVE_TEXT_RECORD_FRAGMENT) {
            if (sources->count > 0) {
                source_fragment_t *open = &sources->items[sources->count - 1];
                const char *end = result == GAME_SAVE_TEXT_DONE ? text + size : text + record.source_offset;
                open->body_size = (size_t)(end - open->body);
            }
            if (result == GAME_SAVE_TEXT_DONE) return features;
        }
        if (result == GAME_SAVE_TEXT_RECORD_FRAGMENT) {
            if (record.key_size >= DOC_KEY_MAX) {
                gsj_set_error(error, error_cap, "fragment id too long");
                break;
            }
            if (sources->count >= GAME_SAVE_MAX_FRAGMENTS) {
                gsj_set_error(error, error_cap, "document has too many fragments");
                break;
            }
            source_fragment_t *source = &sources->items[sources->count++];
            memcpy(source->id, record.key, record.key_size);
            source->id[record.key_size] = '\0';
            source->body = text + record.next_offset;
            source->body_size = 0;
            if (cJSON_GetObjectItemCaseSensitive(features, source->id) != NULL) {
                gsj_set_error(error, error_cap, "duplicate fragment");
                break;
            }
            fragment = cJSON_AddObjectToObject(features, source->id);
            if (fragment == NULL || cJSON_AddNumberToObject(fragment, "v", (double)record.version) == NULL) {
                gsj_set_error(error, error_cap, "failed to stage fragment");
                break;
            }
            continue;
        }
        if (!add_field(fragment, &record, error, error_cap)) break;
    }
    cJSON_Delete(features);
    return NULL;
}

static bool append_body(game_save_text_writer_t *out, const char *body, size_t size) {
    while (size > 0U && (body[size - 1U] == '\n' || body[size - 1U] == '\r' || body[size - 1U] == ' ')) size--;
    if (!game_save_text_writer_ok(out) || size + 1U >= out->capacity - out->used) {
        out->failed = true;
        return false;
    }
    memcpy(out->data + out->used, body, size);
    out->used += size;
    if (size > 0U) out->data[out->used++] = '\n';
    out->data[out->used] = '\0';
    return true;
}

static bool write_json_fields(game_save_text_writer_t *out, const cJSON *object, char *path, size_t path_size,
                              size_t path_used, char *error, int error_cap) {
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, object) {
        if (path_used == 0U && strcmp(item->string, "v") == 0) continue;
        const int length = snprintf(path + path_used, path_size - path_used, "%s%s", path_used ? "." : "",
                                    item->string);
        if (length < 0 || (size_t)length >= path_size - path_used) {
            gsj_set_error(error, error_cap, "field path too long");
            return false;
        }
        bool ok = true;
        if (cJSON_IsObject(item)) {
            ok = write_json_fields(out, item, path, path_size, path_used + (size_t)length, error, error_cap);
        } else if (cJSON_IsBool(item)) {
            ok = game_save_text_write_bool(out, path, cJSON_IsTrue(item));
        } else if (cJSON_IsNull(item)) {
            ok = game_save_text_write_null(out, path);
        } else if (cJSON_IsString(item)) {
            ok = game_save_text_write_string(out, path, item->valuestring);
        } else if (cJSON_IsNumber(item)) {
            const double value = item->valuedouble;
            if (value == trunc(value) && fabs(value) >= 9007199254740992.0) {
                /* A double cannot tell such an integer from its neighbours; writing
                   it would produce a document that reads back wrong or not at all. */
                char message[DOC_KEY_MAX + 64];
                (void)snprintf(message, sizeof message, "an integer of magnitude 2^53 or more cannot pass a migration step: %s",
                               path);
                gsj_set_error(error, error_cap, message);
                return false;
            }
            ok = value == trunc(value) ? game_save_text_write_i64(out, path, (int64_t)value)
                                       : game_save_text_write_number(out, path, value);
        } else {
            gsj_set_error(error, error_cap, "a migrated fragment holds a value NTGS cannot carry");
            return false;
        }
        path[path_used] = '\0';
        if (!ok) {
            gsj_set_error(error, error_cap, "failed to write migrated field");
            return false;
        }
    }
    return true;
}

static bool run_fragment_steps(const game_state_doc_fragment_t *fragment, cJSON *object, char *error,
                               int error_cap) {
    const cJSON *stored = cJSON_GetObjectItemCaseSensitive(object, "v");
    const int from = cJSON_IsNumber(stored) ? (int)stored->valuedouble : 0;
    if (from < 1 || from > fragment->version) {
        gsj_set_error(error, error_cap, from > fragment->version ? "fragment is newer than this build"
                                                                 : "invalid fragment version");
        return false;
    }
    for (int v = from; v < fragment->version; v++) {
        const GameSaveMigrateFn step = fragment->steps != NULL ? fragment->steps[v - 1] : NULL;
        if (step == NULL) {
            gsj_set_error(error, error_cap, "missing fragment migration step");
            return false;
        }
        if (!step(object, error, error_cap)) return false;
    }
    return true;
}

bool game_state_doc_migrate(const game_state_doc_schema_t *schema, const char *text, size_t size,
                            game_save_text_writer_t *out, char *error, int error_cap) {
    if (schema == NULL || out == NULL || schema->fragment_count > GAME_SAVE_MAX_FRAGMENTS) {
        gsj_set_error(error, error_cap, "invalid document schema");
        return false;
    }
    game_state_doc_header_t header;
    if (!game_state_doc_read_header(text, size, &header, error, error_cap)) return false;
    if (header.save_version > schema->save_version) {
        gsj_set_error(error, error_cap, "document is newer than this build");
        return false;
    }
    current_context_t probe = {.schema = schema, .current = header.save_version == schema->save_version};
    if (probe.current && !walk_document(text, size, &header, fragment_is_current, &probe, error, error_cap)) {
        return false;
    }
    if (probe.current) {
        if (!game_save_text_writer_ok(out) || out->used != 0U || out->capacity <= size) {
            gsj_set_error(error, error_cap, "migration output does not fit");
            return false;
        }
        memcpy(out->data, text, size);
        out->data[size] = '\0';
        out->used = size;
        out->preamble_written = true;
        return true;
    }

    source_fragments_t sources;
    cJSON *features = parse_features(text, size, &sources, error, error_cap);
    if (features == NULL) return false;
    cJSON *before = cJSON_Duplicate(features, true);
    bool ok = before != NULL;
    if (!ok) gsj_set_error(error, error_cap, "failed to stage document");
    for (int v = header.save_version; ok && v < schema->save_version; v++) {
        const GameSaveDocumentMigrateFn step = schema->document_steps != NULL ? schema->document_steps[v - 1] : NULL;
        if (step == NULL) {
            gsj_set_error(error, error_cap, "missing document migration step");
            ok = false;
        } else {
            ok = step(features, error, error_cap);
        }
    }
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, features) {
        if (!ok) break;
        if (find_fragment(schema, item->string, strlen(item->string)) < 0) {
            gsj_set_error(error, error_cap, "document has a fragment the schema lacks");
            ok = false;
        }
    }
    char save_id[17];
    (void)snprintf(save_id, sizeof save_id, "%016llx", (unsigned long long)header.save_id);
    ok = ok && game_save_text_write_preamble(out) &&
         game_save_text_write_i64(out, "save_version", schema->save_version) &&
         game_save_text_write_string(out, "save_id", save_id) && game_save_text_write_i64(out, "rev", header.rev);
    for (int i = 0; ok && i < schema->fragment_count; i++) {
        const game_state_doc_fragment_t *fragment = schema->fragments[i];
        cJSON *object = cJSON_GetObjectItemCaseSensitive(features, fragment->id);
        if (object == NULL) continue;
        /* Untouched by every document step and already current: its source text
           is the exact document, integers beyond 2^53 included. */
        const source_fragment_t *source = find_source(&sources, fragment->id);
        const cJSON *staged = cJSON_GetObjectItemCaseSensitive(before, fragment->id);
        const cJSON *stored = cJSON_GetObjectItemCaseSensitive(object, "v");
        if (source != NULL && staged != NULL && cJSON_IsNumber(stored) &&
            (int)stored->valuedouble == fragment->version && cJSON_Compare(object, staged, true)) {
            ok = game_save_text_begin_fragment(out, fragment->id, fragment->version) &&
                 append_body(out, source->body, source->body_size);
            continue;
        }
        char path[DOC_KEY_MAX];
        path[0] = '\0';
        ok = run_fragment_steps(fragment, object, error, error_cap) &&
             game_save_text_begin_fragment(out, fragment->id, fragment->version) &&
             write_json_fields(out, object, path, sizeof path, 0U, error, error_cap);
    }
    cJSON_Delete(before);
    cJSON_Delete(features);
    if (ok && !game_save_text_writer_ok(out)) {
        gsj_set_error(error, error_cap, "migration output does not fit");
        ok = false;
    }
    return ok;
}
