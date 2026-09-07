#include "features/leaderboard/leaderboard.h"

#include <string.h>

/* "k=v;" pairs in a fixed buffer. Keys and values never contain the two
 * delimiters, so a scan needs no escaping and garbage cannot be misread as
 * a pair that was never written. */

static bool token_ok(const char *s) {
    return s != NULL && s[0] != '\0' && strchr(s, '=') == NULL && strchr(s, ';') == NULL;
}

/* Locates the pair for `key`: start of "k=" and the length up to and
 * including its ';' (or the terminator). False when absent or garbage. */
static bool find_pair(const char *extra, const char *key, size_t *start, size_t *len,
                      const char **value, size_t *value_len) {
    const size_t key_len = strlen(key);
    const char *p = extra;
    while (*p != '\0') {
        const char *end = strchr(p, ';');
        const size_t pair_len = end != NULL ? (size_t)(end - p) : strlen(p);
        const char *eq = memchr(p, '=', pair_len);
        if (eq == NULL) {
            return false; /* not a pair: the rest of the buffer is garbage */
        }
        if ((size_t)(eq - p) == key_len && memcmp(p, key, key_len) == 0) {
            *start = (size_t)(p - extra);
            *len = pair_len + (end != NULL ? 1u : 0u);
            *value = eq + 1;
            *value_len = pair_len - key_len - 1;
            return true;
        }
        p += pair_len + (end != NULL ? 1u : 0u);
    }
    return false;
}

bool leaderboard_extra_set(char *extra, size_t cap, const char *key, const char *value) {
    if (extra == NULL || cap == 0 || !token_ok(key) || value == NULL ||
        strchr(value, '=') != NULL || strchr(value, ';') != NULL) {
        return false;
    }
    const size_t key_len = strlen(key);
    const size_t value_len = strlen(value);
    const size_t pair_len = key_len + 1 + value_len + 1;
    size_t old_start = 0;
    size_t old_len = 0;
    const char *old_value = NULL;
    size_t old_value_len = 0;
    const bool replacing = find_pair(extra, key, &old_start, &old_len, &old_value, &old_value_len);
    const size_t len = strlen(extra);
    if (len - old_len + pair_len + 1 > cap) {
        return false;
    }
    if (replacing) {
        memmove(extra + old_start, extra + old_start + old_len, len - old_start - old_len + 1);
    }
    const size_t base = strlen(extra);
    memcpy(extra + base, key, key_len);
    extra[base + key_len] = '=';
    memcpy(extra + base + key_len + 1, value, value_len);
    extra[base + key_len + 1 + value_len] = ';';
    extra[base + pair_len] = '\0';
    return true;
}

bool leaderboard_extra_get(const char *extra, const char *key, char *out, size_t out_size) {
    if (extra == NULL || !token_ok(key) || out == NULL || out_size == 0) {
        return false;
    }
    size_t start = 0;
    size_t len = 0;
    const char *value = NULL;
    size_t value_len = 0;
    if (!find_pair(extra, key, &start, &len, &value, &value_len) || value_len + 1 > out_size) {
        out[0] = '\0';
        return false;
    }
    memcpy(out, value, value_len);
    out[value_len] = '\0';
    return true;
}
