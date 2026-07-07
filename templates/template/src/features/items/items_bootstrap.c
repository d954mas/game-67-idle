#include "features/items/items.h"

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
