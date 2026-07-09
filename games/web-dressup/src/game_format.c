#include "game_format.h"

#include <stdbool.h>
#include <stdio.h>

/* Suffix ladder K,M,B,T,Qa,Qi over 1e3..1e18 (§6.1). Pure int64/uint64 --
   no float thresholds (a float-based tier boundary can misclassify a value
   sitting exactly on a power-of-1000 edge; integer compare cannot). */
static const char *const GAME_FORMAT_SUFFIXES[] = {"", "K", "M", "B", "T", "Qa", "Qi"};
static const uint64_t GAME_FORMAT_THRESHOLDS[] = {
    1ULL, 1000ULL, 1000000ULL, 1000000000ULL, 1000000000000ULL, 1000000000000000ULL, 1000000000000000000ULL,
};
#define GAME_FORMAT_TIER_COUNT 7

char *game_format_i64_abbrev(int64_t v, char *out, size_t cap) {
    if (out == NULL || cap == 0) {
        return out;
    }

    bool negative = v < 0;
    /* #13: -INT64_MIN is UB (INT64_MIN has no positive int64 counterpart).
       Compute the magnitude via uint64_t without ever negating v directly:
       v+1 is always representable (INT64_MIN+1 .. 0), negating THAT is safe
       (result in [0, INT64_MAX]), then the +1u restores the 1 subtracted above. */
    uint64_t mag = negative ? (uint64_t)(-(v + 1)) + 1u : (uint64_t)v;

    int tier = 0;
    for (int i = GAME_FORMAT_TIER_COUNT - 1; i >= 0; --i) {
        if (mag >= GAME_FORMAT_THRESHOLDS[i]) {
            tier = i;
            break;
        }
    }

    if (tier == 0) {
        (void)snprintf(out, cap, "%s%llu", negative ? "-" : "", (unsigned long long)mag);
        return out;
    }

    uint64_t threshold = GAME_FORMAT_THRESHOLDS[tier];
    uint64_t whole = mag / threshold;
    uint64_t remainder = mag % threshold;
    uint64_t frac = (remainder * 10ULL) / threshold; /* one significant fractional digit, floor */
    (void)snprintf(out, cap, "%s%llu.%llu%s", negative ? "-" : "", (unsigned long long)whole, (unsigned long long)frac,
                   GAME_FORMAT_SUFFIXES[tier]);
    return out;
}
