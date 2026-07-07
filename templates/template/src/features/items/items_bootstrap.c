#include "features/items/items.h"

#include "items_state.h" /* generated: items_state instance, ItemsItemOwned, ITEMS_STATE_MAX_OWNED */

#include <stdlib.h> /* strtoll */
#include <string.h> /* strrchr */

/* H1 (deep-review): feature-internal only, NOT declared in items.h -- owned by
   items_containers.c (s_instance_seq stays static there). Forward-declared here
   because reconcile is the one call site that needs it. */
void items_internal_seq_bump(int64_t candidate);

/* И2b (§7.3, Р9 exercise). reset() (generated frag_reset -> items_state_init_defaults)
   stays a NEUTRAL empty default -- it is called on EVERY load, fresh or not.
   Starting content lives ONLY here, and only fires on a genuinely fresh save
   (game_save_new_game / fresh-load, §0 п.2, AFTER reset). A returning player goes
   through from_json, never through on_new_game -- starting content is NOT
   reapplied on every load (R4 guards reset != on_new_game). */
void items_on_new_game(void) {
    items_add("purse", "tmpl.gold", 50, "starting:new_game");
    items_add("backpack", "tmpl.potion", 1, "starting:new_game");
}

/* Post-load fixup (§0 п.2, order: version-steps -> reconcile -> quarantine). Scans
   owned[] directly (items_state.c's find/alloc helpers are static, §7.1/M3): a
   def_id no longer present in the catalog gets quarantined (NEVER deleted -- R5,
   save corruption otherwise); a def_id that reappears (catalog restored) is
   un-quarantined. The record keeps its .container home across the whole cycle.

   H1: also reseeds the unique-instance sequence counter above every "<def_id>
   #<seq>" key already present in the loaded save. s_instance_seq lives in
   items_containers.c and restarts at 0 every process; without this, a freshly
   created unique after a restart could reuse a key already in the save (two
   owned records, same key -- silent corruption). */
void items_reconcile(void) {
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        ItemsItemOwned *rec = &items_state.owned[i];
        if (!rec->used) {
            continue;
        }
        rec->quarantined = (item_core(rec->def_id) == NULL);

        const char *hash = strrchr(rec->key, '#');
        if (hash != NULL && hash[1] != '\0') {
            char *end = NULL;
            long long parsed = strtoll(hash + 1, &end, 10);
            if (end != hash + 1 && *end == '\0' && parsed > 0) {
                items_internal_seq_bump((int64_t)parsed);
            }
        }
    }
}
