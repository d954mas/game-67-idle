#include "game_save_text.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool key_char(char value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_' || value == '-' ||
           value == '.' || value == '[' || value == ']';
}

static bool valid_key(const char *key) {
    if (key == NULL || key[0] == '\0') {
        return false;
    }
    for (const char *at = key; *at != '\0'; at++) {
        if (!key_char(*at)) {
            return false;
        }
    }
    return true;
}

static bool valid_fragment_id_slice(const char *start, const char *end) {
    if (start == end || !((*start >= 'a' && *start <= 'z') || *start == '_')) {
        return false;
    }
    for (const char *at = start + 1; at < end; at++) {
        if (!((*at >= 'a' && *at <= 'z') || (*at >= '0' && *at <= '9') || *at == '_')) {
            return false;
        }
    }
    return true;
}

static bool writer_reserve(game_save_text_writer_t *writer, size_t size) {
    if (writer == NULL || writer->failed || writer->data == NULL ||
        size > SIZE_MAX - writer->used || writer->used + size >= writer->capacity) {
        if (writer != NULL) {
            writer->failed = true;
        }
        return false;
    }
    return true;
}

static void writer_append(game_save_text_writer_t *writer, const char *data, size_t size) {
    memcpy(writer->data + writer->used, data, size);
    writer->used += size;
    writer->data[writer->used] = '\0';
}

static bool writer_line(
    game_save_text_writer_t *writer, const char *key,
    const char *value, size_t value_size) {
    if (writer == NULL || !writer->preamble_written || !valid_key(key) || value == NULL) {
        if (writer != NULL) {
            writer->failed = true;
        }
        return false;
    }
    const size_t key_size = strlen(key);
    if (!writer_reserve(writer, key_size + 1U + value_size + 1U)) {
        return false;
    }
    writer_append(writer, key, key_size);
    writer_append(writer, "=", 1U);
    writer_append(writer, value, value_size);
    writer_append(writer, "\n", 1U);
    return true;
}

void game_save_text_writer_init(game_save_text_writer_t *writer, char *data, size_t capacity) {
    if (writer == NULL) {
        return;
    }
    *writer = (game_save_text_writer_t){.data = data, .capacity = capacity};
    if (data == NULL || capacity == 0U) {
        writer->failed = true;
        return;
    }
    data[0] = '\0';
}

bool game_save_text_writer_ok(const game_save_text_writer_t *writer) {
    return writer != NULL && !writer->failed;
}

size_t game_save_text_writer_size(const game_save_text_writer_t *writer) {
    return writer != NULL ? writer->used : 0U;
}

const char *game_save_text_writer_data(const game_save_text_writer_t *writer) {
    return writer != NULL ? writer->data : NULL;
}

bool game_save_text_write_preamble(game_save_text_writer_t *writer) {
    static const char preamble[] = "NTGS 1\n";
    if (writer == NULL || writer->preamble_written || writer->used != 0U ||
        !writer_reserve(writer, sizeof preamble - 1U)) {
        if (writer != NULL) {
            writer->failed = true;
        }
        return false;
    }
    writer_append(writer, preamble, sizeof preamble - 1U);
    writer->preamble_written = true;
    return true;
}

bool game_save_text_begin_fragment(game_save_text_writer_t *writer, const char *id, int version) {
    char line[96];
    if (writer == NULL || !writer->preamble_written || id == NULL ||
        !valid_fragment_id_slice(id, id + strlen(id)) ||
        version < 1) {
        if (writer != NULL) {
            writer->failed = true;
        }
        return false;
    }
    const int length = snprintf(line, sizeof line, "\n[%s %d]\n", id, version);
    if (length < 0 || (size_t)length >= sizeof line || !writer_reserve(writer, (size_t)length)) {
        if (writer != NULL) {
            writer->failed = true;
        }
        return false;
    }
    writer_append(writer, line, (size_t)length);
    return true;
}

bool game_save_text_write_i64(game_save_text_writer_t *writer, const char *key, int64_t value) {
    char token[32];
    const int length = snprintf(token, sizeof token, "%lld", (long long)value);
    return length > 0 && (size_t)length < sizeof token &&
           writer_line(writer, key, token, (size_t)length);
}

bool game_save_text_write_number(game_save_text_writer_t *writer, const char *key, double value) {
    char token[32];
    if (!isfinite(value)) {
        if (writer != NULL) {
            writer->failed = true;
        }
        return false;
    }
    const int length = snprintf(token, sizeof token, "%.17g", value);
    return length > 0 && (size_t)length < sizeof token &&
           writer_line(writer, key, token, (size_t)length);
}

bool game_save_text_write_bool(game_save_text_writer_t *writer, const char *key, bool value) {
    const char *token = value ? "true" : "false";
    return writer_line(writer, key, token, strlen(token));
}

bool game_save_text_write_null(game_save_text_writer_t *writer, const char *key) {
    return writer_line(writer, key, "null", 4U);
}

static size_t escaped_string_size(const char *value) {
    size_t size = 2U;
    for (const unsigned char *at = (const unsigned char *)value; *at != 0U; at++) {
        if (*at == '"' || *at == '\\' || *at == '\b' || *at == '\f' ||
            *at == '\n' || *at == '\r' || *at == '\t') {
            size += 2U;
        } else if (*at < 0x20U) {
            size += 6U;
        } else {
            size++;
        }
    }
    return size;
}

bool game_save_text_write_string(game_save_text_writer_t *writer, const char *key, const char *value) {
    if (writer == NULL || !writer->preamble_written || !valid_key(key) || value == NULL) {
        if (writer != NULL) {
            writer->failed = true;
        }
        return false;
    }
    const size_t key_size = strlen(key);
    const size_t value_size = escaped_string_size(value);
    if (value_size > SIZE_MAX - key_size - 2U ||
        !writer_reserve(writer, key_size + 1U + value_size + 1U)) {
        return false;
    }
    writer_append(writer, key, key_size);
    writer_append(writer, "=\"", 2U);
    static const char hex[] = "0123456789abcdef";
    for (const unsigned char *at = (const unsigned char *)value; *at != 0U; at++) {
        const char simple = *at == '"' ? '"' : *at == '\\' ? '\\' :
                            *at == '\b' ? 'b' : *at == '\f' ? 'f' :
                            *at == '\n' ? 'n' : *at == '\r' ? 'r' :
                            *at == '\t' ? 't' : '\0';
        if (simple != '\0') {
            const char escaped[2] = {'\\', simple};
            writer_append(writer, escaped, sizeof escaped);
        } else if (*at < 0x20U) {
            const char escaped[6] = {'\\', 'u', '0', '0', hex[*at >> 4U], hex[*at & 15U]};
            writer_append(writer, escaped, sizeof escaped);
        } else {
            writer_append(writer, (const char *)at, 1U);
        }
    }
    writer_append(writer, "\"\n", 2U);
    return true;
}

static void set_error(char *error, size_t capacity, uint32_t line, const char *message) {
    if (error != NULL && capacity > 0U) {
        (void)snprintf(error, capacity, "line %u: %s", line, message);
    }
}

void game_save_text_reader_init(game_save_text_reader_t *reader, const char *data, size_t size) {
    if (reader == NULL) {
        return;
    }
    *reader = (game_save_text_reader_t){.data = data, .size = size, .line = 1U};
    if (data == NULL) {
        reader->failed = true;
    }
}

void game_save_text_fragment_reader_init(
    game_save_text_reader_t *reader, const char *data, size_t size, uint32_t first_line) {
    game_save_text_reader_init(reader, data, size);
    if (reader != NULL) {
        reader->line = first_line;
        reader->preamble_read = true;
        reader->in_fragment = true;
    }
}

static bool next_line(
    game_save_text_reader_t *reader, const char **out_start, const char **out_end,
    uint32_t *out_line) {
    if (reader->offset >= reader->size) {
        return false;
    }
    const size_t start = reader->offset;
    size_t end = start;
    while (end < reader->size && reader->data[end] != '\n') {
        end++;
    }
    reader->offset = end < reader->size ? end + 1U : end;
    *out_line = reader->line++;
    if (end > start && reader->data[end - 1U] == '\r') {
        end--;
    }
    *out_start = reader->data + start;
    *out_end = reader->data + end;
    return true;
}

static void trim(const char **start, const char **end) {
    while (*start < *end && (**start == ' ' || **start == '\t')) {
        (*start)++;
    }
    while (*end > *start && ((*end)[-1] == ' ' || (*end)[-1] == '\t')) {
        (*end)--;
    }
}

static bool slice_is(const char *start, const char *end, const char *text) {
    const size_t size = (size_t)(end - start);
    return strlen(text) == size && memcmp(start, text, size) == 0;
}

static bool valid_key_slice(const char *start, const char *end) {
    if (start == end) {
        return false;
    }
    for (const char *at = start; at < end; at++) {
        if (!key_char(*at)) {
            return false;
        }
    }
    return true;
}

static bool parse_positive_int(const char *start, const char *end, int *out) {
    int value = 0;
    if (start == end) {
        return false;
    }
    for (const char *at = start; at < end; at++) {
        if (*at < '0' || *at > '9' || value > (INT32_MAX - (*at - '0')) / 10) {
            return false;
        }
        value = value * 10 + (*at - '0');
    }
    if (value < 1) {
        return false;
    }
    *out = value;
    return true;
}

game_save_text_result_t game_save_text_reader_next(
    game_save_text_reader_t *reader, game_save_text_record_t *record,
    char *error, size_t error_capacity) {
    if (reader == NULL || record == NULL || reader->failed) {
        return GAME_SAVE_TEXT_ERROR;
    }
    *record = (game_save_text_record_t){0};
    if (!reader->preamble_read) {
        const char *start = NULL;
        const char *end = NULL;
        uint32_t line = 0;
        if (!next_line(reader, &start, &end, &line) || !slice_is(start, end, "NTGS 1")) {
            reader->failed = true;
            set_error(error, error_capacity, line != 0U ? line : 1U, "expected NTGS 1");
            return GAME_SAVE_TEXT_ERROR;
        }
        reader->preamble_read = true;
    }

    const char *start = NULL;
    const char *end = NULL;
    uint32_t line = 0;
    while (next_line(reader, &start, &end, &line)) {
        trim(&start, &end);
        if (start == end || *start == '#') {
            continue;
        }
        if (*start == '[') {
            if (end - start < 5 || end[-1] != ']') {
                reader->failed = true;
                set_error(error, error_capacity, line, "invalid fragment header");
                return GAME_SAVE_TEXT_ERROR;
            }
            const char *body_start = start + 1;
            const char *body_end = end - 1;
            const char *space = body_start;
            while (space < body_end && *space != ' ') {
                space++;
            }
            const char *version_start = space;
            while (version_start < body_end && *version_start == ' ') {
                version_start++;
            }
            int version = 0;
            if (!valid_fragment_id_slice(body_start, space) || version_start == space ||
                !parse_positive_int(version_start, body_end, &version)) {
                reader->failed = true;
                set_error(error, error_capacity, line, "invalid fragment header");
                return GAME_SAVE_TEXT_ERROR;
            }
            record->key = body_start;
            record->key_size = (size_t)(space - body_start);
            record->line = line;
            record->version = version;
            reader->in_fragment = true;
            return GAME_SAVE_TEXT_RECORD_FRAGMENT;
        }
        const char *equals = start;
        while (equals < end && *equals != '=') {
            equals++;
        }
        if (equals == end) {
            reader->failed = true;
            set_error(error, error_capacity, line, "expected key=value");
            return GAME_SAVE_TEXT_ERROR;
        }
        const char *key_start = start;
        const char *key_end = equals;
        const char *value_start = equals + 1;
        const char *value_end = end;
        trim(&key_start, &key_end);
        trim(&value_start, &value_end);
        if (!valid_key_slice(key_start, key_end) || value_start == value_end) {
            reader->failed = true;
            set_error(error, error_capacity, line, "invalid key=value");
            return GAME_SAVE_TEXT_ERROR;
        }
        record->key = key_start;
        record->key_size = (size_t)(key_end - key_start);
        record->value = value_start;
        record->value_size = (size_t)(value_end - value_start);
        record->line = line;
        return reader->in_fragment ? GAME_SAVE_TEXT_RECORD_FIELD : GAME_SAVE_TEXT_RECORD_META;
    }
    return GAME_SAVE_TEXT_DONE;
}

bool game_save_text_record_key_is(const game_save_text_record_t *record, const char *key) {
    return record != NULL && key != NULL && strlen(key) == record->key_size &&
           memcmp(record->key, key, record->key_size) == 0;
}

static bool token_is(const game_save_text_record_t *record, const char *text) {
    return record != NULL && record->value != NULL &&
           strlen(text) == record->value_size &&
           memcmp(record->value, text, record->value_size) == 0;
}

bool game_save_text_record_i64(
    const game_save_text_record_t *record, int64_t min_value, int64_t max_value,
    int64_t *out, char *error, size_t error_capacity) {
    if (record == NULL || record->value == NULL || out == NULL || min_value > max_value) {
        return false;
    }
    const char *at = record->value;
    const char *end = at + record->value_size;
    const bool negative = at < end && *at == '-';
    if (negative) {
        at++;
    }
    if (at == end) {
        set_error(error, error_capacity, record->line, "expected integer");
        return false;
    }
    uint64_t magnitude = 0U;
    const uint64_t limit = negative ? (uint64_t)INT64_MAX + 1U : (uint64_t)INT64_MAX;
    for (; at < end; at++) {
        if (*at < '0' || *at > '9') {
            set_error(error, error_capacity, record->line, "expected integer");
            return false;
        }
        const uint64_t digit = (uint64_t)(*at - '0');
        if (magnitude > (limit - digit) / 10U) {
            set_error(error, error_capacity, record->line, "integer out of range");
            return false;
        }
        magnitude = magnitude * 10U + digit;
    }
    const int64_t value = negative
        ? magnitude == (uint64_t)INT64_MAX + 1U ? INT64_MIN : -(int64_t)magnitude
        : (int64_t)magnitude;
    if (value < min_value || value > max_value) {
        set_error(error, error_capacity, record->line, "integer out of range");
        return false;
    }
    *out = value;
    return true;
}

bool game_save_text_record_number(
    const game_save_text_record_t *record, double min_value, double max_value,
    double *out, char *error, size_t error_capacity) {
    if (record == NULL || record->value == NULL || out == NULL ||
        record->value_size == 0U || record->value_size >= 64U || min_value > max_value) {
        return false;
    }
    char token[64];
    memcpy(token, record->value, record->value_size);
    token[record->value_size] = '\0';
    char *end = NULL;
    errno = 0;
    const double value = strtod(token, &end);
    if (errno == ERANGE || end != token + record->value_size || !isfinite(value) ||
        value < min_value || value > max_value) {
        set_error(error, error_capacity, record->line, "number out of range");
        return false;
    }
    *out = value;
    return true;
}

bool game_save_text_record_bool(
    const game_save_text_record_t *record, bool *out,
    char *error, size_t error_capacity) {
    if (out != NULL && token_is(record, "true")) {
        *out = true;
        return true;
    }
    if (out != NULL && token_is(record, "false")) {
        *out = false;
        return true;
    }
    if (record != NULL) {
        set_error(error, error_capacity, record->line, "expected boolean");
    }
    return false;
}

bool game_save_text_record_is_null(const game_save_text_record_t *record) {
    return token_is(record, "null");
}

static int hex_value(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

static bool append_utf8(uint32_t codepoint, char *out, size_t capacity, size_t *used) {
    unsigned char bytes[4];
    size_t count = 0U;
    if (codepoint <= 0x7FU) {
        bytes[0] = (unsigned char)codepoint;
        count = 1U;
    } else if (codepoint <= 0x7FFU) {
        bytes[0] = (unsigned char)(0xC0U | (codepoint >> 6U));
        bytes[1] = (unsigned char)(0x80U | (codepoint & 0x3FU));
        count = 2U;
    } else if (codepoint <= 0xFFFFU && (codepoint < 0xD800U || codepoint > 0xDFFFU)) {
        bytes[0] = (unsigned char)(0xE0U | (codepoint >> 12U));
        bytes[1] = (unsigned char)(0x80U | ((codepoint >> 6U) & 0x3FU));
        bytes[2] = (unsigned char)(0x80U | (codepoint & 0x3FU));
        count = 3U;
    } else {
        return false;
    }
    if (*used + count >= capacity) {
        return false;
    }
    memcpy(out + *used, bytes, count);
    *used += count;
    return true;
}

bool game_save_text_record_string(
    const game_save_text_record_t *record, char *out, size_t out_capacity,
    char *error, size_t error_capacity) {
    if (record == NULL || record->value == NULL || out == NULL || out_capacity == 0U ||
        record->value_size < 2U || record->value[0] != '"' ||
        record->value[record->value_size - 1U] != '"') {
        if (record != NULL) {
            const bool starts_quoted = record->value != NULL && record->value_size > 0U && record->value[0] == '"';
            set_error(error, error_capacity, record->line,
                      starts_quoted ? "unterminated string" : "expected string");
        }
        return false;
    }
    size_t used = 0U;
    const char *at = record->value + 1;
    const char *end = record->value + record->value_size - 1U;
    while (at < end) {
        unsigned char value = (unsigned char)*at++;
        if (value == '\\') {
            if (at == end) {
                set_error(error, error_capacity, record->line, "invalid string escape");
                return false;
            }
            const char escape = *at++;
            if (escape == 'u') {
                if (end - at < 4) {
                    set_error(error, error_capacity, record->line, "invalid unicode escape");
                    return false;
                }
                uint32_t codepoint = 0U;
                for (int i = 0; i < 4; i++) {
                    const int digit = hex_value(*at++);
                    if (digit < 0) {
                        set_error(error, error_capacity, record->line, "invalid unicode escape");
                        return false;
                    }
                    codepoint = (codepoint << 4U) | (uint32_t)digit;
                }
                if (!append_utf8(codepoint, out, out_capacity, &used)) {
                    set_error(error, error_capacity, record->line, "string out of range");
                    return false;
                }
                continue;
            }
            value = escape == '"' ? '"' : escape == '\\' ? '\\' : escape == '/' ? '/' :
                    escape == 'b' ? '\b' : escape == 'f' ? '\f' : escape == 'n' ? '\n' :
                    escape == 'r' ? '\r' : escape == 't' ? '\t' : 0U;
            if (value == 0U) {
                set_error(error, error_capacity, record->line, "invalid string escape");
                return false;
            }
        } else if (value < 0x20U || value == '"') {
            set_error(error, error_capacity, record->line, "invalid string character");
            return false;
        }
        if (used + 1U >= out_capacity) {
            set_error(error, error_capacity, record->line, "string out of range");
            return false;
        }
        out[used++] = (char)value;
    }
    out[used] = '\0';
    return true;
}
