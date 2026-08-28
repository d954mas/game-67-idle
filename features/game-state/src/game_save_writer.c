#include "game_save_writer.h"

#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdint.h>
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

typedef struct {
    uint64_t significand;
    int exponent;
    bool negative;
} writer_decimal_t;

static bool writer_parse_decimal(const char *text, writer_decimal_t *out) {
    const char *cursor = text;
    writer_decimal_t value = {0};
    bool decimal = false;
    bool digit = false;
    if (*cursor == '-' || *cursor == '+') {
        value.negative = *cursor == '-';
        ++cursor;
    }
    for (; *cursor != '\0' && *cursor != 'e' && *cursor != 'E'; ++cursor) {
        if (*cursor == '.') {
            if (decimal) return false;
            decimal = true;
            continue;
        }
        if (*cursor < '0' || *cursor > '9') return false;
        digit = true;
        value.significand = value.significand * 10U + (uint64_t)(*cursor - '0');
        if (decimal) --value.exponent;
    }
    if (!digit) return false;
    if (*cursor == 'e' || *cursor == 'E') {
        bool exponent_negative = false;
        int explicit_exponent = 0;
        ++cursor;
        if (*cursor == '-' || *cursor == '+') {
            exponent_negative = *cursor == '-';
            ++cursor;
        }
        if (*cursor == '\0') return false;
        for (; *cursor != '\0'; ++cursor) {
            if (*cursor < '0' || *cursor > '9') return false;
            explicit_exponent = explicit_exponent * 10 + (*cursor - '0');
        }
        value.exponent += exponent_negative ? -explicit_exponent : explicit_exponent;
    }
    *out = value;
    return true;
}

static bool writer_decimal_within_epsilon(const char *shorter, const char *precise) {
    writer_decimal_t a;
    writer_decimal_t b;
    if (!writer_parse_decimal(shorter, &a) || !writer_parse_decimal(precise, &b) ||
        a.negative != b.negative) {
        return false;
    }
    while (a.exponent > b.exponent) {
        if (a.significand > UINT64_MAX / 10U) return false;
        a.significand *= 10U;
        --a.exponent;
    }
    while (b.exponent > a.exponent) {
        if (b.significand > UINT64_MAX / 10U) return false;
        b.significand *= 10U;
        --b.exponent;
    }
    const uint64_t difference = a.significand > b.significand
                                    ? a.significand - b.significand
                                    : b.significand - a.significand;
    const uint64_t magnitude = a.significand > b.significand ? a.significand : b.significand;
    return difference <= magnitude / UINT64_C(4503599627370496);
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
        char precise[26] = {0};
        written = snprintf(number, sizeof number, "%1.15g", value);
        const int precise_written = snprintf(precise, sizeof precise, "%1.17g", value);
        if (written > 0 && (size_t)written < sizeof number &&
            precise_written > 0 && (size_t)precise_written < sizeof precise) {
            writer_force_json_decimal(number);
            writer_force_json_decimal(precise);
            if (!writer_decimal_within_epsilon(number, precise)) {
                memcpy(number, precise, (size_t)precise_written + 1U);
                written = precise_written;
            }
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
