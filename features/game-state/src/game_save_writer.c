#include "game_save_writer.h"

#include <limits.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool writer_put(game_save_writer_t *writer, const char *text, size_t size) {
    if (writer == NULL || writer->failed || writer->used >= writer->capacity ||
        size > writer->capacity - writer->used - 1U) {
        if (writer != NULL) { writer->failed = true; }
        return false;
    }
    memcpy(writer->data + writer->used, text, size);
    writer->used += size;
    writer->data[writer->used] = '\0';
    return true;
}

static bool writer_char(game_save_writer_t *writer, char value) { return writer_put(writer, &value, 1U); }

static bool writer_value_prefix(game_save_writer_t *writer) {
    if (writer == NULL || writer->failed) { return false; }
    if (writer->depth == 0U) {
        if (writer->root_written || writer->used != 0U) { writer->failed = true; return false; }
        writer->root_written = true;
        return true;
    }
    const unsigned char parent = (unsigned char)(writer->depth - 1U);
    if (writer->scopes[parent] == GAME_SAVE_WRITER_OBJECT) {
        if (!writer->awaiting_value[parent]) { writer->failed = true; return false; }
        writer->awaiting_value[parent] = false;
        writer->needs_comma[parent] = true;
        return true;
    }
    if (writer->scopes[parent] != GAME_SAVE_WRITER_ARRAY) { writer->failed = true; return false; }
    if (writer->needs_comma[parent] && !writer_char(writer, ',')) { return false; }
    writer->needs_comma[parent] = true;
    return true;
}

static bool writer_quoted(game_save_writer_t *writer, const char *text) {
    static const char hex[] = "0123456789abcdef";
    if (text == NULL) {
        if (writer != NULL) writer->failed = true;
        return false;
    }
    if (!writer_char(writer, '"')) { return false; }
    for (const unsigned char *p = (const unsigned char *)text; *p != 0U; ++p) {
        switch (*p) {
            case '"': if (!writer_put(writer, "\\\"", 2U)) return false; break;
            case '\\': if (!writer_put(writer, "\\\\", 2U)) return false; break;
            case '\b': if (!writer_put(writer, "\\b", 2U)) return false; break;
            case '\f': if (!writer_put(writer, "\\f", 2U)) return false; break;
            case '\n': if (!writer_put(writer, "\\n", 2U)) return false; break;
            case '\r': if (!writer_put(writer, "\\r", 2U)) return false; break;
            case '\t': if (!writer_put(writer, "\\t", 2U)) return false; break;
            default:
                if (*p < 32U) { const char escaped[] = { '\\', 'u', '0', '0', hex[*p >> 4U], hex[*p & 15U] }; if (!writer_put(writer, escaped, sizeof escaped)) return false; }
                else if (!writer_char(writer, (char)*p)) { return false; }
                break;
        }
    }
    return writer_char(writer, '"');
}

static void writer_force_json_decimal(char *number) {
    const struct lconv *locale = localeconv();
    if (locale != NULL && locale->decimal_point != NULL && locale->decimal_point[0] != '\0' &&
        locale->decimal_point[0] != '.') {
        char *decimal = strchr(number, locale->decimal_point[0]);
        if (decimal != NULL) { *decimal = '.'; }
    }
}

static bool writer_compare_double(double a, double b) {
    const double max_value = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return fabs(a - b) <= max_value * DBL_EPSILON;
}

void game_save_writer_init(game_save_writer_t *writer, char *data, size_t capacity) {
    if (writer == NULL) { return; }
    memset(writer, 0, sizeof *writer);
    writer->data = data; writer->capacity = capacity;
    if (data == NULL || capacity == 0U) { writer->failed = true; return; }
    data[0] = '\0';
}

bool game_save_writer_ok(const game_save_writer_t *writer) { return writer != NULL && !writer->failed; }
bool game_save_writer_complete(const game_save_writer_t *writer) {
    return game_save_writer_ok(writer) && writer->root_written && writer->depth == 0U;
}
size_t game_save_writer_size(const game_save_writer_t *writer) { return writer ? writer->used : 0U; }
const char *game_save_writer_data(const game_save_writer_t *writer) { return writer ? writer->data : NULL; }
bool game_save_writer_begin_object(game_save_writer_t *writer) {
    if (writer == NULL || writer->failed || writer->depth >= 32U) { if (writer) writer->failed = true; return false; }
    if (!writer_value_prefix(writer) || !writer_char(writer, '{')) return false;
    writer->needs_comma[writer->depth] = false;
    writer->awaiting_value[writer->depth] = false;
    writer->scopes[writer->depth++] = GAME_SAVE_WRITER_OBJECT;
    return true;
}
bool game_save_writer_end_object(game_save_writer_t *writer) {
    if (writer == NULL || writer->failed || writer->depth == 0U ||
        writer->scopes[writer->depth - 1U] != GAME_SAVE_WRITER_OBJECT ||
        writer->awaiting_value[writer->depth - 1U]) { if (writer) writer->failed = true; return false; }
    writer->depth--;
    return writer_char(writer, '}');
}
bool game_save_writer_begin_array(game_save_writer_t *writer) {
    if (writer == NULL || writer->failed || writer->depth >= 32U) { if (writer) writer->failed = true; return false; }
    if (!writer_value_prefix(writer) || !writer_char(writer, '[')) return false;
    writer->needs_comma[writer->depth] = false;
    writer->awaiting_value[writer->depth] = false;
    writer->scopes[writer->depth++] = GAME_SAVE_WRITER_ARRAY;
    return true;
}
bool game_save_writer_end_array(game_save_writer_t *writer) {
    if (writer == NULL || writer->failed || writer->depth == 0U ||
        writer->scopes[writer->depth - 1U] != GAME_SAVE_WRITER_ARRAY ||
        writer->awaiting_value[writer->depth - 1U]) { if (writer) writer->failed = true; return false; }
    writer->depth--;
    return writer_char(writer, ']');
}
bool game_save_writer_key(game_save_writer_t *writer, const char *key) {
    if (writer == NULL || writer->failed || writer->depth == 0U ||
        writer->scopes[writer->depth - 1U] != GAME_SAVE_WRITER_OBJECT ||
        writer->awaiting_value[writer->depth - 1U]) { if (writer) writer->failed = true; return false; }
    const unsigned char scope = (unsigned char)(writer->depth - 1U);
    if (writer->needs_comma[scope] && !writer_char(writer, ',')) return false;
    if (!writer_quoted(writer, key) || !writer_char(writer, ':')) return false;
    writer->awaiting_value[scope] = true;
    return true;
}
bool game_save_writer_string(game_save_writer_t *writer, const char *value) { return writer_value_prefix(writer) && writer_quoted(writer, value); }
bool game_save_writer_number(game_save_writer_t *writer, double value) {
    char number[26] = {0}; int written;
    if (!isfinite(value)) { return writer_value_prefix(writer) && writer_put(writer, "null", 4U); }
    const int value_int = value >= (double)INT_MAX ? INT_MAX :
                          value <= (double)INT_MIN ? INT_MIN : (int)value;
    if (value == (double)value_int) written = snprintf(number, sizeof number, "%d", value_int);
    else {
        double round_trip = 0.0;
        written = snprintf(number, sizeof number, "%1.15g", value);
        if (written > 0 && (sscanf(number, "%lg", &round_trip) != 1 ||
                            !writer_compare_double(round_trip, value))) {
            written = snprintf(number, sizeof number, "%1.17g", value);
        }
    }
    writer_force_json_decimal(number);
    return written > 0 && (size_t)written < sizeof number && writer_value_prefix(writer) && writer_put(writer, number, (size_t)written);
}
bool game_save_writer_bool(game_save_writer_t *writer, bool value) { return writer_value_prefix(writer) && writer_put(writer, value ? "true" : "false", value ? 4U : 5U); }
bool game_save_writer_null(game_save_writer_t *writer) { return writer_value_prefix(writer) && writer_put(writer, "null", 4U); }
bool game_save_writer_raw_value(game_save_writer_t *writer, const char *json, size_t size) {
    if (json == NULL || size == 0U || memchr(json, '\0', size) != NULL) {
        if (writer) writer->failed = true;
        return false;
    }
    return writer_value_prefix(writer) && writer_put(writer, json, size);
}
