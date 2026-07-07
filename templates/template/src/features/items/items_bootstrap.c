#include "features/items/items.h"

/* И2a: СТАБЫ. The generated items_state_fragment descriptor (state/items.schema.json
   hooks: on_new_game/reconcile) takes the extern addresses of these two functions to
   populate its vtable (game_save.h GameSaveFragment) -- without them the THIRD
   fragment block fails to link (H1). Real bodies (starting-currency seed / orphan
   quarantine, Р9 §7.3) land in И2b; these are intentionally empty no-ops. */

void items_on_new_game(void) {
}

void items_reconcile(void) {
}
