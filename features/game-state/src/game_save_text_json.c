#include "game_save_text_json.h"

#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_save_text.h"

/* Mirrors the private reader in game_save.c, which must build without this
   file; keep the two in step when the NTGS shape changes. Depends on neither
   game_state_json.c nor game_save.c, because the text-only save shell links
   neither. */

static void set_error(char *error, int error_cap, const char *message) {
    if (error != NULL && error_cap > 0) (void)snprintf(error, (size_t)error_cap, "%s", message);
}

static char *dup_slice(const char *text, size_t size) {
    char *copy = (char *)malloc(size + 1U);
    if (copy != NULL) {
        memcpy(copy, text, size);
        copy[size] = '\0';
    }
    return copy;
}

static cJSON *text_record_value(
    const game_save_text_record_t *record, char *error, int error_cap) {
    const size_t error_size = error_cap > 0 ? (size_t)error_cap : 0U;
    if (record->value_size >= 2U && record->value[0] == '"') {
        char *value = (char *)malloc(record->value_size + 1U);
        if (value == NULL) {
            set_error(error, error_cap, "failed to allocate save string");
            return NULL;
        }
        const bool valid = game_save_text_record_string(
            record, value, record->value_size + 1U, error, error_size);
        cJSON *json = valid ? cJSON_CreateString(value) : NULL;
        free(value);
        return json;
    }
    if (game_save_text_record_is_null(record)) {
        return cJSON_CreateNull();
    }
    bool boolean = false;
    if (game_save_text_record_bool(record, &boolean, NULL, 0U)) {
        return cJSON_CreateBool(boolean);
    }
    double number = 0.0;
    if (game_save_text_record_number(
            record, -DBL_MAX, DBL_MAX, &number, error, error_size)) {
        return cJSON_CreateNumber(number);
    }
    return NULL;
}

static bool text_metadata_i64(const game_save_text_record_t *record) {
    return game_save_text_record_key_is(record, "saved_at") ||
           game_save_text_record_key_is(record, "save_seq") ||
           game_save_text_record_key_is(record, "playtime_ms");
}

static cJSON *text_metadata_i64_value(
    const game_save_text_record_t *record, char *error, int error_cap) {
    int64_t value = 0;
    if (!game_save_text_record_i64(
            record, 0, INT64_MAX, &value, error,
            error_cap > 0 ? (size_t)error_cap : 0U)) {
        return NULL;
    }
    char decimal[32];
    (void)snprintf(decimal, sizeof decimal, "%lld", (long long)value);
    return cJSON_CreateString(decimal);
}

static bool add_text_record(
    cJSON *object, const game_save_text_record_t *record,
    bool nested_path, char *error, int error_cap) {
    const char *part = record->key;
    const char *end = record->key + record->key_size;
    cJSON *owner = object;
    while (part < end) {
        const char *separator = nested_path ? memchr(part, '.', (size_t)(end - part)) : NULL;
        const char *part_end = separator != NULL ? separator : end;
        if (part == part_end) {
            set_error(error, error_cap, "save field path is invalid");
            return false;
        }
        char *key = dup_slice(part, (size_t)(part_end - part));
        if (key == NULL) {
            set_error(error, error_cap, "failed to allocate save field");
            return false;
        }
        cJSON *existing = cJSON_GetObjectItemCaseSensitive(owner, key);
        if (separator != NULL) {
            if (existing == NULL) {
                existing = cJSON_CreateObject();
                if (existing == NULL || !cJSON_AddItemToObject(owner, key, existing)) {
                    cJSON_Delete(existing);
                    free(key);
                    set_error(error, error_cap, "failed to build save field path");
                    return false;
                }
            } else if (!cJSON_IsObject(existing)) {
                free(key);
                set_error(error, error_cap, "save field path conflicts with a value");
                return false;
            }
            owner = existing;
            part = separator + 1;
            free(key);
            continue;
        }
        if (existing != NULL) {
            free(key);
            set_error(error, error_cap, "duplicate save field");
            return false;
        }
        cJSON *value = !nested_path && text_metadata_i64(record)
                           ? text_metadata_i64_value(record, error, error_cap)
                           : text_record_value(record, error, error_cap);
        if (value == NULL || !cJSON_AddItemToObject(owner, key, value)) {
            cJSON_Delete(value);
            free(key);
            if (error == NULL || error_cap <= 0 || error[0] == '\0') {
                set_error(error, error_cap, "invalid save value");
            }
            return false;
        }
        free(key);
        return true;
    }
    return false;
}

cJSON *game_save_text_json_parse(const char *text, char *error, int error_cap) {
    if (text == NULL || strncmp(text, "NTGS 1", 6U) != 0) {
        return text != NULL ? cJSON_Parse(text) : NULL;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON *features = cJSON_CreateObject();
    if (root == NULL || features == NULL ||
        !cJSON_AddItemToObject(root, "features", features)) {
        cJSON_Delete(features);
        cJSON_Delete(root);
        set_error(error, error_cap, "failed to stage text save");
        return NULL;
    }

    game_save_text_reader_t reader;
    game_save_text_record_t record;
    cJSON *fragment = NULL;
    game_save_text_reader_init(&reader, text, strlen(text));
    for (;;) {
        const game_save_text_result_t result = game_save_text_reader_next(
            &reader, &record, error, error_cap > 0 ? (size_t)error_cap : 0U);
        if (result == GAME_SAVE_TEXT_DONE) {
            return root;
        }
        if (result == GAME_SAVE_TEXT_ERROR) {
            cJSON_Delete(root);
            return NULL;
        }
        if (result == GAME_SAVE_TEXT_RECORD_FRAGMENT) {
            char *id = dup_slice(record.key, record.key_size);
            fragment = cJSON_CreateObject();
            if (id == NULL || fragment == NULL ||
                cJSON_GetObjectItemCaseSensitive(features, id) != NULL ||
                !cJSON_AddNumberToObject(fragment, "v", (double)record.version) ||
                !cJSON_AddItemToObject(features, id, fragment)) {
                free(id);
                cJSON_Delete(fragment);
                cJSON_Delete(root);
                set_error(error, error_cap, "duplicate or invalid save fragment");
                return NULL;
            }
            free(id);
            continue;
        }
        if (result == GAME_SAVE_TEXT_RECORD_FIELD && fragment == NULL) {
            cJSON_Delete(root);
            set_error(error, error_cap, "save field has no fragment");
            return NULL;
        }
        cJSON *owner = result == GAME_SAVE_TEXT_RECORD_META ? root : fragment;
        if (!add_text_record(
                owner, &record, result == GAME_SAVE_TEXT_RECORD_FIELD,
                error, error_cap)) {
            cJSON_Delete(root);
            return NULL;
        }
    }
}
