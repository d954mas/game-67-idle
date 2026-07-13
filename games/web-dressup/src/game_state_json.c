#include "game_state_json.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* i64 round-trip must not read a magnitude above this without a string
   (double loses integer precision above 2^53). */
#define GSJ_I64_MAX_SAFE_DOUBLE 9007199254740992.0

void gsj_set_error(char *error, int error_cap, const char *message) {
    if (error && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
}

bool gsj_copy_text(char *dst, size_t dst_cap, const char *src) {
    if (!dst || dst_cap == 0 || !src || strlen(src) >= dst_cap) {
        return false;
    }
    (void)snprintf(dst, dst_cap, "%s", src);
    return true;
}

const cJSON *gsj_object_item(const cJSON *obj, const char *name) {
    return cJSON_IsObject(obj) ? cJSON_GetObjectItemCaseSensitive(obj, name) : NULL;
}

bool gsj_read_bool(const cJSON *obj, const char *name, bool *out, char *error, int error_cap) {
    const cJSON *item = gsj_object_item(obj, name);
    if (!item) {
        return true;
    }
    if (!cJSON_IsBool(item)) {
        gsj_set_error(error, error_cap, "expected bool");
        return false;
    }
    *out = cJSON_IsTrue(item);
    return true;
}

bool gsj_read_int_range(const cJSON *obj, const char *name, int min_value, int max_value, int *out, char *error, int error_cap) {
    const cJSON *item = gsj_object_item(obj, name);
    if (!item) {
        return true;
    }
    if (!cJSON_IsNumber(item)) {
        gsj_set_error(error, error_cap, "expected number");
        return false;
    }
    double number = item->valuedouble;
    if (number < (double)min_value || number > (double)max_value || number != (double)(int)number) {
        gsj_set_error(error, error_cap, "number out of range");
        return false;
    }
    *out = (int)number;
    return true;
}

bool gsj_read_float_range(const cJSON *obj, const char *name, float min_value, float max_value, float *out, char *error, int error_cap) {
    const cJSON *item = gsj_object_item(obj, name);
    if (!item) {
        return true;
    }
    if (!cJSON_IsNumber(item)) {
        gsj_set_error(error, error_cap, "expected number");
        return false;
    }
    float value = (float)item->valuedouble;
    if (value < min_value || value > max_value) {
        gsj_set_error(error, error_cap, "number out of range");
        return false;
    }
    *out = value;
    return true;
}

bool gsj_read_string(const cJSON *obj, const char *name, char *out, size_t out_cap, char *error, int error_cap) {
    const cJSON *item = gsj_object_item(obj, name);
    if (!item) {
        return true;
    }
    if (!cJSON_IsString(item) || !gsj_copy_text(out, out_cap, item->valuestring)) {
        gsj_set_error(error, error_cap, "expected short string");
        return false;
    }
    return true;
}

int gsj_enum_index(const char *value, const char *const *names, int count) {
    if (!value) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(value, names[i]) == 0) {
            return i;
        }
    }
    return -1;
}

bool gsj_read_enum(const cJSON *obj, const char *name, const char *const *names, int count, int *out, char *error, int error_cap) {
    const cJSON *item = gsj_object_item(obj, name);
    if (!item) {
        return true;
    }
    int value = -1;
    if (cJSON_IsString(item)) {
        value = gsj_enum_index(item->valuestring, names, count);
    } else if (cJSON_IsNumber(item)) {
        double number = item->valuedouble;
        if (number != (double)(int)number) {
            gsj_set_error(error, error_cap, "enum value must be an integer");
            return false;
        }
        value = (int)number;
    } else {
        gsj_set_error(error, error_cap, "expected enum string or number");
        return false;
    }
    if (value < 0 || value >= count) {
        gsj_set_error(error, error_cap, "enum value out of range");
        return false;
    }
    *out = value;
    return true;
}

bool gsj_parse_int_value(const cJSON *item, int min_value, int max_value, int *out, char *error, int error_cap) {
    if (!cJSON_IsNumber(item)) {
        gsj_set_error(error, error_cap, "expected integer");
        return false;
    }
    double number = item->valuedouble;
    if (number < (double)min_value || number > (double)max_value || number != (double)(int)number) {
        gsj_set_error(error, error_cap, "integer value out of range");
        return false;
    }
    *out = (int)number;
    return true;
}

bool gsj_parse_enum_value(const cJSON *item, const char *const *names, int count, int *out, char *error, int error_cap) {
    int value = -1;
    if (cJSON_IsString(item)) {
        value = gsj_enum_index(item->valuestring, names, count);
    } else if (cJSON_IsNumber(item)) {
        double number = item->valuedouble;
        if (number != (double)(int)number) {
            gsj_set_error(error, error_cap, "enum value must be an integer");
            return false;
        }
        value = (int)number;
    } else {
        gsj_set_error(error, error_cap, "expected enum string or number");
        return false;
    }
    if (value < 0 || value >= count) {
        gsj_set_error(error, error_cap, "enum value out of range");
        return false;
    }
    *out = value;
    return true;
}

/* ---- i64 wire: large counters ride as a JSON string; a bare
   number is accepted only when it round-trips exactly through double (no
   silent precision loss above 2^53). ---- */

bool gsj_parse_i64_value(const cJSON *item, int64_t min_value, int64_t max_value, int64_t *out, char *error, int error_cap) {
    int64_t value = 0;
    if (cJSON_IsString(item)) {
        const char *text = item->valuestring;
        if (!text || !text[0]) {
            gsj_set_error(error, error_cap, "expected i64 string");
            return false;
        }
        char *end = NULL;
        errno = 0;
        long long parsed = strtoll(text, &end, 10);
        if (end == text || *end != '\0' || errno == ERANGE) {
            gsj_set_error(error, error_cap, "invalid i64 string");
            return false;
        }
        value = (int64_t)parsed;
    } else if (cJSON_IsNumber(item)) {
        double number = item->valuedouble;
        double magnitude = number < 0.0 ? -number : number;
        /* Bound-check BEFORE casting to int64_t: casting an out-of-range
           double to int64_t is undefined behaviour. */
        if (magnitude > GSJ_I64_MAX_SAFE_DOUBLE) {
            gsj_set_error(error, error_cap, "i64 must be sent as string");
            return false;
        }
        int64_t truncated = (int64_t)number;
        if (number != (double)truncated) {
            gsj_set_error(error, error_cap, "i64 must be sent as string");
            return false;
        }
        value = truncated;
    } else {
        gsj_set_error(error, error_cap, "expected i64 string or number");
        return false;
    }
    if (value < min_value || value > max_value) {
        gsj_set_error(error, error_cap, "i64 value out of range");
        return false;
    }
    *out = value;
    return true;
}

bool gsj_read_i64(const cJSON *obj, const char *name, int64_t min_value, int64_t max_value, int64_t *out, char *error, int error_cap) {
    const cJSON *item = gsj_object_item(obj, name);
    if (!item) {
        return true;
    }
    return gsj_parse_i64_value(item, min_value, max_value, out, error, error_cap);
}

char *gsj_i64_to_string(int64_t value, char *buf, size_t cap) {
    (void)snprintf(buf, cap, "%lld", (long long)value);
    return buf;
}

cJSON *gsj_add_i64(cJSON *obj, const char *name, int64_t value) {
    char buf[32];
    return cJSON_AddStringToObject(obj, name, gsj_i64_to_string(value, buf, sizeof buf));
}
