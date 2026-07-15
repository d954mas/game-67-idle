#include "features/items/items.h"
#include "features/items/reason_tags.h"

#include "items_state.h"            /* generated: ItemsState + items_state instance, ItemsItemOwned */
#include "items_state_events.gen.h" /* generated items.txn/items.move events */

#include "core/nt_assert.h" /* NT_ASSERT (L2: catch key truncation loudly in debug) */
#include "game_save.h"      /* game_save_mark_dirty */

#include <stdio.h>
#include <string.h>

/* items_state.c's find_owned/alloc_owned helpers are static inside
   that generated TU (render_collection_helpers) -- unreachable from here. This
   file scans items_state.owned[] directly; items_bootstrap.c's reconcile scan
   is the only other place that touches the array raw. */

static int64_t s_instance_seq; /* unique-instance id counter: "<def_id>#<seq>" */

/* H1 (deep-review): feature-internal only -- NOT in items.h. items_bootstrap.c's
   reconcile forward-declares this itself and calls it once per used record's
   parsed "#<seq>" suffix after every load, so a freshly created unique can never
   collide with a key already in the save (s_instance_seq otherwise restarts at 0
   every process, while the loaded save may already contain "<def_id>#7"). */
void items_internal_seq_bump(int64_t candidate) {
    if (candidate > s_instance_seq) {
        s_instance_seq = candidate;
    }
}

/* R3: ONE key-builder for stacks -- divergence here is silent save corruption
   (two stacks for the same def_id in one container). The key is
   "<container>/<def_id>", authoritative for stacks (M2). L2: returns false on
   truncation (container/def_id combo does not fit ITEMS_STATE_STRING_MAX) --
   callers MUST treat that as invalid input, never proceed with a truncated key. */
static bool build_stack_key(char *out, size_t out_cap, const char *container_id, const char *def_id) {
    int n = snprintf(out, out_cap, "%s/%s", container_id, def_id);
    NT_ASSERT(n >= 0 && (size_t)n < out_cap);
    return n >= 0 && (size_t)n < out_cap;
}

static ItemsItemOwned *find_owned_by_key(const char *key) {
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].key, key) == 0) {
            return &items_state.owned[i];
        }
    }
    return NULL;
}

static ItemsItemOwned *alloc_owned_slot(void) {
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (!items_state.owned[i].used) {
            return &items_state.owned[i];
        }
    }
    return NULL;
}

/* container.capacity (M1) counts DISTINCT owned records (stacks + uniques) in a
   container -- NOT summed count. Quarantined records still occupy a slot (they
   keep their .container home), so they count here too. */
static int container_record_count(const char *container_id) {
    int n = 0;
    for (int i = 0; i < ITEMS_STATE_MAX_OWNED; ++i) {
        if (items_state.owned[i].used && strcmp(items_state.owned[i].container, container_id) == 0) {
            ++n;
        }
    }
    return n;
}

/* Core add, shared by the public items_add() and items_move()'s strict
   remove+add re-key -- neither reason-checks nor emits txn here (the
   caller owns both: items_add emits txn; items_move emits move, not txn, since
   ownership of the def does not change, only its container).
   capacity (M1) REJECTs only when allocating a genuinely NEW record; growing an
   existing stack is never capacity-limited. currency.cap and stack>1 CLAMP the
   accepted addition, never reduce an already-over-limit loaded record.
   *out_delta (if non-NULL) receives the actual
   count applied (post-clamp); meaningful only when this returns true. */
static bool add_raw(const char *container_id, const char *def_id, int64_t count, int64_t *out_delta) {
    if (count <= 0) {
        return false;
    }
    const game_item_def_t *def = item_core(def_id);
    if (def == NULL || !def->stackable) {
        return false;
    }
    const game_container_def_t *cdef = item_container_def(container_id);
    if (cdef == NULL) {
        return false;
    }
    if (cdef->accept_policy == ITEM_ACCEPT_CURRENCY_ONLY && !item_is_currency(def)) {
        return false; /* L6: slot_filter/capacity_1 are inert in И2, treated as `any` */
    }

    char key[ITEMS_STATE_STRING_MAX];
    if (!build_stack_key(key, sizeof key, container_id, def_id)) {
        return false; /* L2: truncated key -- reject, never silently corrupt */
    }
    ItemsItemOwned *rec = find_owned_by_key(key);
    bool is_new = (rec == NULL);
    bool restore_quarantined = !is_new && rec->quarantined;
    if (is_new) {
        if (cdef->capacity > 0 && container_record_count(container_id) >= cdef->capacity) {
            return false; /* M1: capacity REJECTs new records, never clamps */
        }
        rec = alloc_owned_slot();
        if (rec == NULL) {
            return false; /* ITEMS_STATE_MAX_OWNED exhausted. */
        }
        rec->used = true;
        (void)snprintf(rec->key, sizeof rec->key, "%s", key);
        (void)snprintf(rec->def_id, sizeof rec->def_id, "%s", def_id);
        (void)snprintf(rec->container, sizeof rec->container, "%s", container_id);
        rec->count = 0;
        rec->level = ITEMS_STATE_ITEM_OWNED_LEVEL_DEFAULT;
        rec->durability = ITEMS_STATE_ITEM_OWNED_DURABILITY_DEFAULT;
        rec->quarantined = false;
    }

    int64_t before = rec->count;
    int64_t limit = ITEMS_STATE_ITEM_OWNED_COUNT_MAX;
    if (def->currency != NULL && def->currency->cap > 0 && def->currency->cap < limit) {
        limit = def->currency->cap;
    }
    if (def->max_stack > 1 && def->max_stack < limit) {
        limit = def->max_stack;
    }
    if (before >= limit) {
        return false;
    }
    rec->count = count > limit - before ? limit : before + count;
    if (restore_quarantined) {
        rec->quarantined = false;
    }
    if (out_delta != NULL) {
        *out_delta = rec->count - before; /* honest actual change after BOTH clamps */
    }
    game_save_mark_dirty();
    return true;
}

static bool remove_raw(const char *container_id, const char *def_id, int64_t count) {
    if (count <= 0) {
        return false;
    }
    char key[ITEMS_STATE_STRING_MAX];
    if (!build_stack_key(key, sizeof key, container_id, def_id)) {
        return false; /* L2: truncated key -- reject, never silently corrupt */
    }
    ItemsItemOwned *rec = find_owned_by_key(key);
    if (rec == NULL || rec->quarantined || rec->count < count) {
        return false;
    }
    rec->count -= count;
    if (rec->count <= 0) {
        memset(rec, 0, sizeof(*rec)); /* used=false -> slot freed, no cruft */
    }
    game_save_mark_dirty();
    return true;
}

bool items_add(const char *container_id, const char *def_id, int64_t count, const char *reason) {
    items_reason_check(reason);
    if (def_id == NULL) {
        return false;
    }
    const game_item_def_t *def = item_core(def_id);
    if (def == NULL || !def->stackable) {
        return false;
    }
    /* Default by accept policy: currency -> purse, otherwise backpack. */
    const char *target =
        (container_id != NULL && container_id[0] != '\0') ? container_id : (item_is_currency(def) ? "purse" : "backpack");

    char key[ITEMS_STATE_STRING_MAX];
    if (!build_stack_key(key, sizeof key, target, def_id)) {
        return false;
    }
    const int64_t before = items_count(target, def_id);
    int64_t delta = 0;
    if (!add_raw(target, def_id, count, &delta)) {
        return false;
    }
    items_emit_txn("add", def_id, target, key, count, delta, before, before + delta, reason);
    return true;
}

bool items_remove(const char *container_id, const char *def_id, int64_t count, const char *reason) {
    items_reason_check(reason);
    if (container_id == NULL || def_id == NULL) {
        return false;
    }
    const game_item_def_t *def = item_core(def_id);
    if (def == NULL || !def->stackable) {
        return false;
    }
    char key[ITEMS_STATE_STRING_MAX];
    if (!build_stack_key(key, sizeof key, container_id, def_id)) {
        return false;
    }
    const int64_t before = items_count(container_id, def_id);
    if (!remove_raw(container_id, def_id, count)) {
        return false;
    }
    items_emit_txn("remove", def_id, container_id, key, -count, -count, before, before - count, reason);
    return true;
}

int64_t items_count(const char *container_id, const char *def_id) {
    if (container_id == NULL || def_id == NULL) {
        return 0;
    }
    const game_item_def_t *def = item_core(def_id);
    if (def == NULL || !def->stackable) {
        return 0;
    }
    char key[ITEMS_STATE_STRING_MAX];
    if (!build_stack_key(key, sizeof key, container_id, def_id)) {
        return 0; /* L2: truncated key -- no such record can exist */
    }
    const ItemsItemOwned *rec = find_owned_by_key(key);
    if (rec == NULL || rec->quarantined) {
        return 0; /* Quarantined records are excluded from live queries. */
    }
    return rec->count;
}

bool items_can_afford(const char *container_id, const char *def_id, int64_t n) {
    const game_item_def_t *def = item_core(def_id);
    if (def == NULL || !def->stackable) {
        return false;
    }
    if (n < 0) {
        return false; /* L5: consistent with remove's count<=0 rejection */
    }
    return items_count(container_id, def_id) >= n;
}

bool items_move(const char *from, const char *to, const char *entry_key, int64_t count, const char *reason) {
    items_reason_check(reason);
    if (from == NULL || to == NULL || entry_key == NULL || count <= 0) {
        return false;
    }
    if (strcmp(from, to) == 0) {
        return true; /* M2: same-container move is a no-op; no txn (nothing changed) */
    }

    /* Stack route: entry_key is a def_id; the key is authoritative for
       stacks -> strict remove+add re-key, never just flip .container (the key
       would desync from the field). NOT emitted as txn: ownership of the def
       does not change, only its container. */
    char stack_key[ITEMS_STATE_STRING_MAX];
    if (!build_stack_key(stack_key, sizeof stack_key, from, entry_key)) {
        return false; /* L2: truncated key -- reject */
    }
    ItemsItemOwned *stack_rec = find_owned_by_key(stack_key);
    if (stack_rec != NULL) {
        if (count > stack_rec->count) {
            return false;
        }
        char def_id[ITEMS_STATE_STRING_MAX];
        (void)snprintf(def_id, sizeof def_id, "%s", stack_rec->def_id);
        int64_t before = stack_rec->count;

        int64_t accepted = 0;
        if (!add_raw(to, def_id, count, &accepted)) {
            return false;
        }

        /* M3: the destination may accept LESS than requested (currency.cap
           clamp) -- deduct from the source only what actually landed there, so
           total sum (source + destination) is conserved; a partially-capped
           move leaves the untransferable remainder in the source instead of
           destroying it. Record is freed only when the WHOLE original stack
           made it across (accepted == before), not merely when the requested
           `count` did. */
        int64_t remaining = before - accepted;
        if (accepted == before) {
            memset(stack_rec, 0, sizeof(*stack_rec));
        } else {
            stack_rec->count = remaining;
        }
        game_save_mark_dirty();
        if (accepted > 0) {
            items_emit_move(def_id, entry_key, from, to, count, accepted, reason);
        }
        return true;
    }

    /* Unique route: entry_key is the record key (instance_id); field
       .container is authoritative for uniques -> only the field changes. */
    ItemsItemOwned *unique = find_owned_by_key(entry_key);
    if (unique != NULL && strcmp(unique->container, from) == 0) {
        const game_container_def_t *cdef = item_container_def(to);
        if (cdef == NULL) {
            return false;
        }
        const game_item_def_t *def = item_core(unique->def_id);
        if (def == NULL || def->stackable) {
            return false;
        }
        if (cdef->accept_policy == ITEM_ACCEPT_CURRENCY_ONLY && !item_is_currency(def)) {
            return false;
        }
        /* M4: capacity is "max DISTINCT records in a container" regardless of how
           they arrive -- a unique moving in must obey the same destination check
           new-record allocation does (the mover is still in `from` here, so this
           does not double-count it). */
        if (cdef->capacity > 0 && container_record_count(to) >= cdef->capacity) {
            return false;
        }
        char def_id[ITEMS_STATE_STRING_MAX];
        (void)snprintf(def_id, sizeof def_id, "%s", unique->def_id);
        (void)snprintf(unique->container, sizeof unique->container, "%s", to);
        game_save_mark_dirty();
        items_emit_move(def_id, entry_key, from, to, count, 1, reason);
        return true;
    }

    return false;
}

const char *items_instance_create(const char *container_id, const char *def_id, const char *reason) {
    items_reason_check(reason);
    if (container_id == NULL || def_id == NULL) {
        return NULL;
    }
    const game_item_def_t *def = item_core(def_id);
    if (def == NULL || def->stackable) {
        return NULL;
    }
    const game_container_def_t *cdef = item_container_def(container_id);
    if (cdef == NULL) {
        return NULL;
    }
    if (cdef->accept_policy == ITEM_ACCEPT_CURRENCY_ONLY && !item_is_currency(def)) {
        return NULL;
    }
    if (cdef->capacity > 0 && container_record_count(container_id) >= cdef->capacity) {
        return NULL; /* M1: same REJECT rule as items_add applies to unique allocation */
    }

    ItemsItemOwned *rec = alloc_owned_slot();
    if (rec == NULL) {
        return NULL;
    }

    ++s_instance_seq;
    rec->used = true;
    (void)snprintf(rec->key, sizeof rec->key, "%s#%lld", def_id, (long long)s_instance_seq);
    (void)snprintf(rec->def_id, sizeof rec->def_id, "%s", def_id);
    (void)snprintf(rec->container, sizeof rec->container, "%s", container_id);
    rec->count = 1;
    rec->level = ITEMS_STATE_ITEM_OWNED_LEVEL_DEFAULT;
    rec->durability = ITEMS_STATE_ITEM_OWNED_DURABILITY_DEFAULT;
    rec->quarantined = false;

    game_save_mark_dirty();
    items_emit_txn("create", def_id, container_id, rec->key, 1, 1, 0, rec->count, reason);
    return rec->key;
}

bool items_instance_destroy(const char *instance_id, const char *reason) {
    items_reason_check(reason);
    if (instance_id == NULL) {
        return false;
    }
    ItemsItemOwned *rec = find_owned_by_key(instance_id);
    if (rec == NULL) {
        return false;
    }
    const game_item_def_t *def = item_core(rec->def_id);
    if (def == NULL || def->stackable) {
        return false;
    }
    if (strrchr(rec->key, '#') == NULL) {
        return false; /* L4: only records created by items_instance_create (key
                          format "<def_id>#<seq>") may be destroyed this way --
                          reject a stack record reached by coincidence/misuse. */
    }
    char def_id[ITEMS_STATE_STRING_MAX];
    (void)snprintf(def_id, sizeof def_id, "%s", rec->def_id);
    char container_id[ITEMS_STATE_STRING_MAX];
    (void)snprintf(container_id, sizeof container_id, "%s", rec->container);
    char entry_key[ITEMS_STATE_STRING_MAX];
    (void)snprintf(entry_key, sizeof entry_key, "%s", rec->key);
    int64_t count = rec->count;
    memset(rec, 0, sizeof(*rec));
    game_save_mark_dirty();
    items_emit_txn("destroy", def_id, container_id, entry_key, -count, -count, count, 0, reason);
    return true;
}

int64_t items_purse(const char *def_id) { return items_count("purse", def_id); }
