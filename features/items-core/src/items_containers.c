#include "features/items/items.h"
#include "features/items/reason_tags.h"

#include "items_state.h"
#include "items_state_events.gen.h"

#include "core/nt_assert.h"
#include "game_save.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct items_id_index_t {
    uint32_t id;
    uint32_t index;
} items_id_index_t;

static items_id_index_t s_container_index[ITEMS_STATE_MAX_CONTAINERS];
static items_id_index_t s_entry_index[ITEMS_STATE_MAX_CONTAINERS_ENTRIES];
static uint32_t s_container_index_count;
static uint32_t s_entry_index_count;
static uint32_t s_container_generation[ITEMS_STATE_MAX_CONTAINERS];
static uint32_t s_entry_generation[ITEMS_STATE_MAX_CONTAINERS_ENTRIES];
static bool s_ready;

static void set_error(char *error, int cap, const char *message) {
    if (error != NULL && cap > 0) {
        (void)snprintf(error, (size_t)cap, "%s", message);
    }
}

static uint32_t next_generation(uint32_t generation) {
    generation++;
    return generation == 0 ? 1 : generation;
}

static int compare_id_index(const void *lhs_ptr, const void *rhs_ptr) {
    const items_id_index_t *lhs = (const items_id_index_t *)lhs_ptr;
    const items_id_index_t *rhs = (const items_id_index_t *)rhs_ptr;
    if (lhs->id < rhs->id) { return -1; }
    if (lhs->id > rhs->id) { return 1; }
    return 0;
}

static bool lookup_item(const char *def_id, item_def_ref_t *out_ref, item_core_t *out_core) {
    item_def_ref_t ref;
    if (def_id == NULL || def_id[0] == '\0' || strlen(def_id) >= ITEMS_STATE_STRING_MAX ||
        !items_catalog_is_bound() || !items_try_get_string(def_id, &ref)) {
        return false;
    }
    if (out_ref != NULL) { *out_ref = ref; }
    if (out_core != NULL) { *out_core = items_core(ref); }
    return true;
}

static bool item_is_stackable(item_core_t core) { return core.stack != 1; }

static bool build_indices(bool invalidate_refs, char *error, int error_cap) {
    items_id_index_t containers[ITEMS_STATE_MAX_CONTAINERS];
    items_id_index_t entries[ITEMS_STATE_MAX_CONTAINERS_ENTRIES];
    uint32_t container_count = 0;
    uint32_t entry_count = 0;
    uint32_t max_container_id = 0;
    uint32_t max_entry_id = 0;

    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS; i++) {
        const ItemsItemContainer *container = &items_state.containers[i];
        if (!container->used) { continue; }
        if (container->container_id == ITEMS_ID_NONE || container->container_id == ITEMS_ID_RESERVED) {
            set_error(error, error_cap, "reserved container id");
            return false;
        }
        if (container->capacity > ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX ||
            container->policy < 0 || container->policy >= ITEMS_STATE_CONTAINER_POLICY_COUNT) {
            set_error(error, error_cap, "invalid container policy or capacity");
            return false;
        }
        containers[container_count++] = (items_id_index_t){container->container_id, i};
        if (container->container_id > max_container_id) { max_container_id = container->container_id; }
    }

    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        const ItemsItemEntry *entry = &items_state.containers_entries[i];
        if (!entry->used) { continue; }
        if (entry->entry_id == ITEMS_ID_NONE || entry->entry_id == ITEMS_ID_RESERVED) {
            set_error(error, error_cap, "reserved entry id");
            return false;
        }
        if (entry->parent_index < 0 || entry->parent_index >= ITEMS_STATE_MAX_CONTAINERS ||
            !items_state.containers[entry->parent_index].used) {
            set_error(error, error_cap, "entry has no live container");
            return false;
        }
        const ItemsItemContainer *container = &items_state.containers[entry->parent_index];
        if (entry->slot >= container->capacity || entry->count <= 0 || entry->def_id[0] == '\0') {
            set_error(error, error_cap, "invalid entry slot, count, or definition");
            return false;
        }
        for (uint32_t j = 0; j < i; j++) {
            const ItemsItemEntry *other = &items_state.containers_entries[j];
            if (other->used && other->parent_index == entry->parent_index && other->slot == entry->slot) {
                set_error(error, error_cap, "duplicate container slot");
                return false;
            }
        }
        entries[entry_count++] = (items_id_index_t){entry->entry_id, i};
        if (entry->entry_id > max_entry_id) { max_entry_id = entry->entry_id; }
    }

    qsort(containers, container_count, sizeof(containers[0]), compare_id_index);
    qsort(entries, entry_count, sizeof(entries[0]), compare_id_index);
    for (uint32_t i = 1; i < container_count; i++) {
        if (containers[i - 1].id == containers[i].id) {
            set_error(error, error_cap, "duplicate container id");
            return false;
        }
    }
    for (uint32_t i = 1; i < entry_count; i++) {
        if (entries[i - 1].id == entries[i].id) {
            set_error(error, error_cap, "duplicate entry id");
            return false;
        }
    }
    if (items_state.last_container_id < max_container_id || items_state.last_entry_id < max_entry_id) {
        set_error(error, error_cap, "persisted id counter is behind live ids");
        return false;
    }

    memcpy(s_container_index, containers, container_count * sizeof(containers[0]));
    memcpy(s_entry_index, entries, entry_count * sizeof(entries[0]));
    s_container_index_count = container_count;
    s_entry_index_count = entry_count;
    if (invalidate_refs) {
        for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS; i++) {
            s_container_generation[i] = next_generation(s_container_generation[i]);
        }
        for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
            s_entry_generation[i] = next_generation(s_entry_generation[i]);
        }
    }
    s_ready = true;
    return true;
}

bool items_runtime_rebuild(char *error, int error_cap) {
    return build_indices(true, error, error_cap);
}

static void ensure_ready(void) {
    if (!s_ready) {
        char error[128] = {0};
        bool ok = items_runtime_rebuild(error, (int)sizeof(error));
        NT_ASSERT(ok && "items runtime state is invalid");
        (void)ok;
    }
}

static uint32_t find_index(const items_id_index_t *index, uint32_t count, uint32_t id) {
    uint32_t lo = 0;
    uint32_t hi = count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2U;
        if (index[mid].id < id) { lo = mid + 1U; }
        else { hi = mid; }
    }
    return lo < count && index[lo].id == id ? index[lo].index : UINT32_MAX;
}

static ItemsItemContainer *require_container(items_container_ref_t ref) {
    ensure_ready();
    NT_ASSERT(ref.index < ITEMS_STATE_MAX_CONTAINERS);
    NT_ASSERT(ref.generation != 0 && s_container_generation[ref.index] == ref.generation);
    NT_ASSERT(items_state.containers[ref.index].used);
    return &items_state.containers[ref.index];
}

static ItemsItemEntry *require_entry(item_entry_ref_t ref) {
    ensure_ready();
    NT_ASSERT(ref.index < ITEMS_STATE_MAX_CONTAINERS_ENTRIES);
    NT_ASSERT(ref.generation != 0 && s_entry_generation[ref.index] == ref.generation);
    NT_ASSERT(items_state.containers_entries[ref.index].used);
    return &items_state.containers_entries[ref.index];
}

static items_container_ref_t container_ref(uint32_t index) {
    return (items_container_ref_t){index, s_container_generation[index]};
}

static item_entry_ref_t entry_ref(uint32_t index) {
    return (item_entry_ref_t){index, s_entry_generation[index]};
}

bool items_container_try_from_id(uint32_t container_id, items_container_ref_t *out_container) {
    ensure_ready();
    if (container_id == ITEMS_ID_NONE || container_id == ITEMS_ID_RESERVED || out_container == NULL) { return false; }
    uint32_t index = find_index(s_container_index, s_container_index_count, container_id);
    if (index == UINT32_MAX) { return false; }
    *out_container = container_ref(index);
    return true;
}

bool items_entry_try_from_id(uint32_t entry_id_value, item_entry_ref_t *out_entry) {
    ensure_ready();
    if (entry_id_value == ITEMS_ID_NONE || entry_id_value == ITEMS_ID_RESERVED || out_entry == NULL) { return false; }
    uint32_t index = find_index(s_entry_index, s_entry_index_count, entry_id_value);
    if (index == UINT32_MAX) { return false; }
    *out_entry = entry_ref(index);
    return true;
}

static uint32_t free_container_index(void) {
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS; i++) {
        if (!items_state.containers[i].used) { return i; }
    }
    return UINT32_MAX;
}

static uint32_t free_entry_index(void) {
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        if (!items_state.containers_entries[i].used) { return i; }
    }
    return UINT32_MAX;
}

static bool allocate_id(uint32_t *counter, uint32_t *out_id) {
    if (*counter >= UINT32_MAX - 1U) { return false; }
    *counter += 1U;
    *out_id = *counter;
    return true;
}

static bool slot_used(uint32_t parent_index, uint32_t slot, uint32_t ignore_entry) {
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        const ItemsItemEntry *entry = &items_state.containers_entries[i];
        if (i != ignore_entry && entry->used && entry->parent_index == (int)parent_index && entry->slot == slot) {
            return true;
        }
    }
    return false;
}

static uint32_t choose_slot(uint32_t parent_index, uint32_t requested, uint32_t ignore_entry) {
    const ItemsItemContainer *container = &items_state.containers[parent_index];
    if (requested != ITEMS_SLOT_AUTO) {
        return requested < container->capacity && !slot_used(parent_index, requested, ignore_entry)
            ? requested : UINT32_MAX;
    }
    for (uint32_t slot = 0; slot < container->capacity; slot++) {
        if (!slot_used(parent_index, slot, ignore_entry)) { return slot; }
    }
    return UINT32_MAX;
}

static items_result_t policy_allows(const ItemsItemContainer *container, item_def_ref_t def, item_core_t core) {
    bool currency = items_has_currency(def);
    if (currency && !item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    if (container->policy == ITEMS_STATE_CONTAINER_POLICY_CURRENCY_ONLY && !currency) { return ITEMS_RESULT_POLICY; }
    if (container->policy == ITEMS_STATE_CONTAINER_POLICY_EQUIPMENT && item_is_stackable(core)) { return ITEMS_RESULT_POLICY; }
    return ITEMS_RESULT_OK;
}

items_result_t items_try_container_create(items_container_desc_t desc, items_container_ref_t *out_container) {
    ensure_ready();
    NT_ASSERT(out_container != NULL);
    if (desc.lifetime != ITEMS_LIFETIME_PERSISTENT) { return ITEMS_RESULT_LIFETIME; }
    if (desc.capacity > ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX || desc.policy > ITEMS_CONTAINER_POLICY_EQUIPMENT) {
        return ITEMS_RESULT_CAPACITY;
    }
    uint32_t index = free_container_index();
    if (index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
    uint32_t id = 0;
    if (!allocate_id(&items_state.last_container_id, &id)) { return ITEMS_RESULT_ID_EXHAUSTED; }

    ItemsItemContainer *container = &items_state.containers[index];
    memset(container, 0, sizeof(*container));
    container->used = true;
    container->container_id = id;
    container->capacity = desc.capacity;
    container->policy = (int)desc.policy;
    s_container_generation[index] = next_generation(s_container_generation[index]);
    bool ok = build_indices(false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    *out_container = container_ref(index);
    return ITEMS_RESULT_OK;
}

items_result_t items_try_container_destroy_empty(items_container_ref_t ref) {
    ItemsItemContainer *container = require_container(ref);
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        if (items_state.containers_entries[i].used &&
            items_state.containers_entries[i].parent_index == (int)ref.index) {
            return ITEMS_RESULT_NOT_EMPTY;
        }
    }
    memset(container, 0, sizeof(*container));
    s_container_generation[ref.index] = next_generation(s_container_generation[ref.index]);
    bool ok = build_indices(false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    return ITEMS_RESULT_OK;
}

items_result_t items_try_container_resize(items_container_ref_t ref, uint32_t capacity) {
    ItemsItemContainer *container = require_container(ref);
    if (capacity > ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX) { return ITEMS_RESULT_CAPACITY; }
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        const ItemsItemEntry *entry = &items_state.containers_entries[i];
        if (entry->used && entry->parent_index == (int)ref.index && entry->slot >= capacity) {
            return ITEMS_RESULT_CAPACITY;
        }
    }
    container->capacity = capacity;
    game_save_mark_dirty();
    return ITEMS_RESULT_OK;
}

uint32_t items_container_id(items_container_ref_t ref) {
    return require_container(ref)->container_id;
}

uint32_t items_container_capacity(items_container_ref_t ref) {
    return require_container(ref)->capacity;
}

items_lifetime_t items_container_lifetime(items_container_ref_t ref) {
    (void)require_container(ref);
    return ITEMS_LIFETIME_PERSISTENT;
}

static ItemsItemEntry *find_stack(uint32_t parent_index, const char *def_id, uint32_t slot) {
    ItemsItemEntry *best = NULL;
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        ItemsItemEntry *entry = &items_state.containers_entries[i];
        if (!entry->used || entry->parent_index != (int)parent_index || strcmp(entry->def_id, def_id) != 0) { continue; }
        if (slot != ITEMS_SLOT_AUTO) { return entry->slot == slot ? entry : NULL; }
        if (best == NULL || entry->slot < best->slot) { best = entry; }
    }
    return best;
}

static ItemsItemEntry *entry_at_slot(uint32_t parent_index, uint32_t slot);

static int64_t stack_limit(item_def_ref_t ref, item_core_t core) {
    int64_t limit = ITEMS_STATE_ITEM_ENTRY_COUNT_MAX;
    if (items_has_currency(ref)) {
        int64_t cap = items_currency_cap(ref);
        if (cap > 0 && cap < limit) { limit = cap; }
    }
    if (core.stack > 1 && core.stack < limit) { limit = core.stack; }
    return limit;
}

static void format_id(char *out, size_t cap, uint32_t id) {
    (void)snprintf(out, cap, "%u", (unsigned)id);
}

items_result_t items_try_stack_add(
    items_container_ref_t container_ref_value, const char *def_id, int64_t count,
    uint32_t requested_slot, const char *reason, item_entry_ref_t *out_entry, int64_t *out_applied) {
    items_reason_check(reason);
    ItemsItemContainer *container = require_container(container_ref_value);
    if (count <= 0 || def_id == NULL) { return ITEMS_RESULT_INSUFFICIENT; }
    item_def_ref_t def;
    item_core_t core;
    if (!lookup_item(def_id, &def, &core) || !item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    items_result_t allowed = policy_allows(container, def, core);
    if (allowed != ITEMS_RESULT_OK) { return allowed; }

    ItemsItemEntry *entry = find_stack(container_ref_value.index, def_id, requested_slot);
    int64_t limit = stack_limit(def, core);
    if (entry != NULL && (requested_slot != ITEMS_SLOT_AUTO || entry->count < limit)) {
        int64_t applied = count > limit - entry->count ? limit - entry->count : count;
        if (applied <= 0) { return ITEMS_RESULT_CAPACITY; }
        int64_t before = entry->count;
        entry->count += applied;
        if (entry->quarantined) { entry->quarantined = false; }
        game_save_mark_dirty();
        char container_id_text[16];
        char entry_id_text[16];
        format_id(container_id_text, sizeof(container_id_text), container->container_id);
        format_id(entry_id_text, sizeof(entry_id_text), entry->entry_id);
        items_emit_txn("add", def_id, container_id_text, entry_id_text, count, applied, before, entry->count, reason);
        if (out_entry != NULL) { *out_entry = entry_ref((uint32_t)(entry - items_state.containers_entries)); }
        if (out_applied != NULL) { *out_applied = applied; }
        return ITEMS_RESULT_OK;
    }

    uint32_t slot = choose_slot(container_ref_value.index, requested_slot, UINT32_MAX);
    if (slot == UINT32_MAX) {
        return requested_slot == ITEMS_SLOT_AUTO ? ITEMS_RESULT_CAPACITY : ITEMS_RESULT_SLOT_OCCUPIED;
    }
    uint32_t index = free_entry_index();
    if (index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
    uint32_t id = 0;
    if (!allocate_id(&items_state.last_entry_id, &id)) { return ITEMS_RESULT_ID_EXHAUSTED; }
    int64_t applied = count > limit ? limit : count;
    if (applied <= 0) { return ITEMS_RESULT_CAPACITY; }

    entry = &items_state.containers_entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->used = true;
    entry->parent_index = (int)container_ref_value.index;
    entry->entry_id = id;
    entry->slot = slot;
    (void)snprintf(entry->def_id, sizeof(entry->def_id), "%s", def_id);
    entry->count = applied;
    entry->level = ITEMS_STATE_ITEM_ENTRY_LEVEL_DEFAULT;
    entry->durability = ITEMS_STATE_ITEM_ENTRY_DURABILITY_DEFAULT;
    s_entry_generation[index] = next_generation(s_entry_generation[index]);
    bool ok = build_indices(false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    char container_id_text[16];
    char entry_id_text[16];
    format_id(container_id_text, sizeof(container_id_text), container->container_id);
    format_id(entry_id_text, sizeof(entry_id_text), id);
    items_emit_txn("add", def_id, container_id_text, entry_id_text, count, applied, 0, applied, reason);
    if (out_entry != NULL) { *out_entry = entry_ref(index); }
    if (out_applied != NULL) { *out_applied = applied; }
    return ITEMS_RESULT_OK;
}

items_result_t items_try_stack_remove(item_entry_ref_t entry_ref_value, int64_t count, const char *reason) {
    items_reason_check(reason);
    ItemsItemEntry *entry = require_entry(entry_ref_value);
    item_core_t core;
    if (!lookup_item(entry->def_id, NULL, &core) || !item_is_stackable(core) || entry->quarantined) {
        return ITEMS_RESULT_WRONG_STORAGE;
    }
    if (count <= 0 || entry->count < count) { return ITEMS_RESULT_INSUFFICIENT; }
    int64_t before = entry->count;
    uint32_t container_index = (uint32_t)entry->parent_index;
    uint32_t entry_id_value = entry->entry_id;
    char def_id[ITEMS_STATE_STRING_MAX];
    (void)snprintf(def_id, sizeof(def_id), "%s", entry->def_id);
    entry->count -= count;
    if (entry->count == 0) {
        memset(entry, 0, sizeof(*entry));
        s_entry_generation[entry_ref_value.index] = next_generation(s_entry_generation[entry_ref_value.index]);
        bool ok = build_indices(false, NULL, 0);
        NT_ASSERT(ok);
    }
    game_save_mark_dirty();
    char container_id_text[16];
    char entry_id_text[16];
    format_id(container_id_text, sizeof(container_id_text), items_state.containers[container_index].container_id);
    format_id(entry_id_text, sizeof(entry_id_text), entry_id_value);
    items_emit_txn("remove", def_id, container_id_text, entry_id_text, -count, -count, before, before - count, reason);
    return ITEMS_RESULT_OK;
}

items_result_t items_try_stack_remove_from_container(
    items_container_ref_t container_ref_value, const char *def_id, int64_t count, const char *reason) {
    items_reason_check(reason);
    ItemsItemContainer *container = require_container(container_ref_value);
    item_core_t core;
    if (!lookup_item(def_id, NULL, &core) || !item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    if (count <= 0 || items_stack_count(container_ref_value, def_id) < count) { return ITEMS_RESULT_INSUFFICIENT; }

    int64_t remaining = count;
    for (uint32_t slot = 0; slot < container->capacity && remaining > 0; slot++) {
        ItemsItemEntry *entry = entry_at_slot(container_ref_value.index, slot);
        if (entry == NULL || entry->quarantined || strcmp(entry->def_id, def_id) != 0) { continue; }
        int64_t take = entry->count < remaining ? entry->count : remaining;
        entry->count -= take;
        remaining -= take;
        if (entry->count == 0) {
            uint32_t index = (uint32_t)(entry - items_state.containers_entries);
            memset(entry, 0, sizeof(*entry));
            s_entry_generation[index] = next_generation(s_entry_generation[index]);
        }
    }
    NT_ASSERT(remaining == 0);
    bool ok = build_indices(false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    char container_id_text[16];
    format_id(container_id_text, sizeof(container_id_text), container->container_id);
    items_emit_txn("remove", def_id, container_id_text, "scope", -count, -count, count, 0, reason);
    return ITEMS_RESULT_OK;
}

int64_t items_stack_count(items_container_ref_t container_ref_value, const char *def_id) {
    (void)require_container(container_ref_value);
    item_core_t core;
    if (!lookup_item(def_id, NULL, &core) || !item_is_stackable(core)) { return 0; }
    int64_t total = 0;
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        const ItemsItemEntry *entry = &items_state.containers_entries[i];
        if (!entry->used || entry->quarantined || entry->parent_index != (int)container_ref_value.index ||
            strcmp(entry->def_id, def_id) != 0) { continue; }
        if (entry->count > INT64_MAX - total) { return INT64_MAX; }
        total += entry->count;
    }
    return total;
}

bool items_can_afford(items_container_ref_t container, const char *def_id, int64_t count) {
    if (count < 0) { return false; }
    return items_stack_count(container, def_id) >= count;
}

items_result_t items_try_unique_create(
    items_container_ref_t container_ref_value, const char *def_id, uint32_t requested_slot,
    const char *reason, item_entry_ref_t *out_entry) {
    items_reason_check(reason);
    ItemsItemContainer *container = require_container(container_ref_value);
    NT_ASSERT(out_entry != NULL);
    item_def_ref_t def;
    item_core_t core;
    if (!lookup_item(def_id, &def, &core) || item_is_stackable(core) || items_has_currency(def)) {
        return ITEMS_RESULT_WRONG_STORAGE;
    }
    items_result_t allowed = policy_allows(container, def, core);
    if (allowed != ITEMS_RESULT_OK) { return allowed; }
    uint32_t slot = choose_slot(container_ref_value.index, requested_slot, UINT32_MAX);
    if (slot == UINT32_MAX) { return requested_slot == ITEMS_SLOT_AUTO ? ITEMS_RESULT_CAPACITY : ITEMS_RESULT_SLOT_OCCUPIED; }
    uint32_t index = free_entry_index();
    if (index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
    uint32_t id = 0;
    if (!allocate_id(&items_state.last_entry_id, &id)) { return ITEMS_RESULT_ID_EXHAUSTED; }

    ItemsItemEntry *entry = &items_state.containers_entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->used = true;
    entry->parent_index = (int)container_ref_value.index;
    entry->entry_id = id;
    entry->slot = slot;
    (void)snprintf(entry->def_id, sizeof(entry->def_id), "%s", def_id);
    entry->count = 1;
    entry->level = ITEMS_STATE_ITEM_ENTRY_LEVEL_DEFAULT;
    entry->durability = ITEMS_STATE_ITEM_ENTRY_DURABILITY_DEFAULT;
    s_entry_generation[index] = next_generation(s_entry_generation[index]);
    bool ok = build_indices(false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    char container_id_text[16];
    char entry_id_text[16];
    format_id(container_id_text, sizeof(container_id_text), container->container_id);
    format_id(entry_id_text, sizeof(entry_id_text), id);
    items_emit_txn("create", def_id, container_id_text, entry_id_text, 1, 1, 0, 1, reason);
    *out_entry = entry_ref(index);
    return ITEMS_RESULT_OK;
}

items_result_t items_try_entry_destroy(item_entry_ref_t entry_ref_value, const char *reason) {
    items_reason_check(reason);
    ItemsItemEntry *entry = require_entry(entry_ref_value);
    item_core_t core;
    if (!lookup_item(entry->def_id, NULL, &core) || item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    char def_id[ITEMS_STATE_STRING_MAX];
    (void)snprintf(def_id, sizeof(def_id), "%s", entry->def_id);
    uint32_t container_id_value = items_state.containers[entry->parent_index].container_id;
    uint32_t entry_id_value = entry->entry_id;
    memset(entry, 0, sizeof(*entry));
    s_entry_generation[entry_ref_value.index] = next_generation(s_entry_generation[entry_ref_value.index]);
    bool ok = build_indices(false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    char container_id_text[16];
    char entry_id_text[16];
    format_id(container_id_text, sizeof(container_id_text), container_id_value);
    format_id(entry_id_text, sizeof(entry_id_text), entry_id_value);
    items_emit_txn("destroy", def_id, container_id_text, entry_id_text, -1, -1, 1, 0, reason);
    return ITEMS_RESULT_OK;
}

static ItemsItemEntry *entry_at_slot(uint32_t parent_index, uint32_t slot) {
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        ItemsItemEntry *entry = &items_state.containers_entries[i];
        if (entry->used && entry->parent_index == (int)parent_index && entry->slot == slot) { return entry; }
    }
    return NULL;
}

items_result_t items_try_entry_move(
    item_entry_ref_t source_ref, items_container_ref_t destination_ref,
    int64_t count, uint32_t requested_slot, const char *reason,
    item_entry_ref_t *out_destination) {
    items_reason_check(reason);
    ItemsItemEntry *source = require_entry(source_ref);
    ItemsItemContainer *destination = require_container(destination_ref);
    if (count <= 0 || count > source->count) { return ITEMS_RESULT_INSUFFICIENT; }
    item_def_ref_t def;
    item_core_t core;
    if (!lookup_item(source->def_id, &def, &core) || source->quarantined) { return ITEMS_RESULT_NOT_FOUND; }
    items_result_t allowed = policy_allows(destination, def, core);
    if (allowed != ITEMS_RESULT_OK) { return allowed; }

    uint32_t target_slot = requested_slot;
    ItemsItemEntry *merge = NULL;
    int64_t limit = item_is_stackable(core) ? stack_limit(def, core) : 1;
    if (target_slot == ITEMS_SLOT_AUTO && item_is_stackable(core)) {
        for (uint32_t slot = 0; slot < destination->capacity; slot++) {
            ItemsItemEntry *candidate = entry_at_slot(destination_ref.index, slot);
            if (candidate != NULL && strcmp(candidate->def_id, source->def_id) == 0 && candidate != source &&
                candidate->count <= limit - count) {
                target_slot = slot;
                merge = candidate;
                break;
            }
        }
    }
    if (target_slot == ITEMS_SLOT_AUTO) {
        target_slot = choose_slot(destination_ref.index, ITEMS_SLOT_AUTO, source_ref.index);
    } else {
        if (target_slot >= destination->capacity) { return ITEMS_RESULT_CAPACITY; }
        merge = entry_at_slot(destination_ref.index, target_slot);
        if (merge == source) { merge = NULL; }
    }
    if (target_slot == UINT32_MAX) { return ITEMS_RESULT_CAPACITY; }
    if (merge != NULL) {
        if (!item_is_stackable(core) || strcmp(merge->def_id, source->def_id) != 0 || merge->count > limit - count) {
            return ITEMS_RESULT_SLOT_OCCUPIED;
        }
    } else if (slot_used(destination_ref.index, target_slot, source_ref.index)) {
        return ITEMS_RESULT_SLOT_OCCUPIED;
    }

    bool whole = count == source->count;
    item_entry_ref_t result_ref = source_ref;
    uint32_t source_container_id = items_state.containers[source->parent_index].container_id;
    uint32_t source_entry_id = source->entry_id;
    char def_id[ITEMS_STATE_STRING_MAX];
    (void)snprintf(def_id, sizeof(def_id), "%s", source->def_id);

    if (merge != NULL) {
        merge->count += count;
        result_ref = entry_ref((uint32_t)(merge - items_state.containers_entries));
        if (whole) {
            memset(source, 0, sizeof(*source));
            s_entry_generation[source_ref.index] = next_generation(s_entry_generation[source_ref.index]);
        } else {
            source->count -= count;
        }
    } else if (whole) {
        source->parent_index = (int)destination_ref.index;
        source->slot = target_slot;
    } else {
        if (!item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
        uint32_t new_index = free_entry_index();
        if (new_index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
        uint32_t new_id = 0;
        if (!allocate_id(&items_state.last_entry_id, &new_id)) { return ITEMS_RESULT_ID_EXHAUSTED; }
        ItemsItemEntry *split = &items_state.containers_entries[new_index];
        *split = *source;
        split->parent_index = (int)destination_ref.index;
        split->entry_id = new_id;
        split->slot = target_slot;
        split->count = count;
        source->count -= count;
        s_entry_generation[new_index] = next_generation(s_entry_generation[new_index]);
        result_ref = entry_ref(new_index);
    }

    bool ok = build_indices(false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    char from_text[16];
    char to_text[16];
    char entry_text[16];
    format_id(from_text, sizeof(from_text), source_container_id);
    format_id(to_text, sizeof(to_text), destination->container_id);
    format_id(entry_text, sizeof(entry_text), source_entry_id);
    items_emit_move(def_id, entry_text, from_text, to_text, count, count, reason);
    if (out_destination != NULL) { *out_destination = result_ref; }
    return ITEMS_RESULT_OK;
}

uint32_t items_entry_id(item_entry_ref_t ref) {
    return require_entry(ref)->entry_id;
}

items_container_ref_t items_entry_container(item_entry_ref_t ref) {
    ItemsItemEntry *entry = require_entry(ref);
    return container_ref((uint32_t)entry->parent_index);
}

items_entry_view_t items_entry_view(item_entry_ref_t ref) {
    ItemsItemEntry *entry = require_entry(ref);
    return (items_entry_view_t){
        .entry_id = entry->entry_id,
        .slot = entry->slot,
        .def_id = entry->def_id,
        .count = entry->count,
        .level = entry->level,
        .durability = entry->durability,
        .quarantined = entry->quarantined,
        .lifetime = ITEMS_LIFETIME_PERSISTENT,
    };
}
