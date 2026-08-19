#ifndef FEATURES_ITEMS_REASON_TAGS_H
#define FEATURES_ITEMS_REASON_TAGS_H
/* Closed, append-only reason-verb contract. Every items_* mutation calls
   items_reason_check(reason) FIRST. Format "verb:subject" (verb from the
   closed list below, bounded ASCII subject). Invalid input is release-visible. */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "features/items/items.h"

static inline bool items_reason_subject_char(unsigned char value) {
    return (value >= (unsigned char)'a' && value <= (unsigned char)'z') ||
           (value >= (unsigned char)'A' && value <= (unsigned char)'Z') ||
           (value >= (unsigned char)'0' && value <= (unsigned char)'9') ||
           value == (unsigned char)'_' || value == (unsigned char)'-' ||
           value == (unsigned char)'.' || value == (unsigned char)'/';
}

static inline bool items_reason_check(const char *reason) {
    static const char *const k_verbs[] = {
        "loot",   "quest_reward", "shop_buy", "craft_cost", "level_cost", "use",     "starting",
        "cheat",  "migration",    "sell",     "gift",       "refund",     "decay",   "convert",
        "init",   "admin",        "drop",     "pickup",     "split",      "merge",
    };
    if (reason == NULL) { return false; }

    size_t length = 0;
    while (length <= ITEMS_REASON_MAX_LENGTH && reason[length] != '\0') { length++; }
    if (length == 0 || length > ITEMS_REASON_MAX_LENGTH) { return false; }

    const char *colon = memchr(reason, ':', length);
    if (colon == NULL || colon == reason || colon == reason + length - 1) { return false; }
    size_t verb_len = (size_t)(colon - reason);
    bool matched = false;
    for (size_t i = 0; i < sizeof(k_verbs) / sizeof(k_verbs[0]); ++i) {
        if (strlen(k_verbs[i]) == verb_len && strncmp(k_verbs[i], reason, verb_len) == 0) {
            matched = true;
            break;
        }
    }
    if (!matched) { return false; }

    for (const char *cursor = colon + 1; cursor < reason + length; cursor++) {
        if (!items_reason_subject_char((unsigned char)*cursor)) { return false; }
    }
    return true;
}

#endif /* FEATURES_ITEMS_REASON_TAGS_H */
