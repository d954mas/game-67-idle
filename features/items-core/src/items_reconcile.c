#include "features/items/items.h"

#include "items_state.h"

#include "core/nt_assert.h"

void items_reconcile(void) {
    char error[128] = {0};
    bool valid = items_runtime_rebuild(error, (int)sizeof(error));
    NT_ASSERT(valid && "invalid persisted Items container graph");
    if (!valid) { return; }

    for (int i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        ItemsItemEntry *entry = &items_state.containers_entries[i];
        if (!entry->used) { continue; }
        item_def_ref_t def;
        entry->quarantined = !items_catalog_is_bound() || !items_try_get_string(entry->def_id, &def);
    }
}
