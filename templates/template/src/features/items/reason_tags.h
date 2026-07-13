#ifndef FEATURES_ITEMS_REASON_TAGS_H
#define FEATURES_ITEMS_REASON_TAGS_H
/* Closed, append-only reason-verb contract. Every
   items_* mutation calls items_reason_check(reason) FIRST. Format "verb:subject"
   (verb from the closed list below, subject non-empty). Debug-only assert;
   release build = no-op (zero runtime cost in shipped builds). */

#include <assert.h>
#include <stddef.h>
#include <string.h>

static inline void items_reason_check(const char *reason) {
#ifndef NDEBUG
    static const char *const k_verbs[] = {
        "loot",   "quest_reward", "shop_buy", "craft_cost", "level_cost", "use",     "starting",
        "cheat",  "migration",    "sell",     "gift",       "refund",     "decay",   "convert",
        "init",   "admin",        "drop",     "pickup",     "split",      "merge",
    };
    assert(reason != NULL);
    const char *colon = strchr(reason, ':');
    assert(colon != NULL && colon[1] != '\0'); /* verb:subject, subject non-empty */
    size_t verb_len = (size_t)(colon - reason);
    int matched = 0;
    for (size_t i = 0; i < sizeof(k_verbs) / sizeof(k_verbs[0]); ++i) {
        if (strlen(k_verbs[i]) == verb_len && strncmp(k_verbs[i], reason, verb_len) == 0) {
            matched = 1;
            break;
        }
    }
    assert(matched);
#else
    (void)reason;
#endif
}

#endif /* FEATURES_ITEMS_REASON_TAGS_H */
