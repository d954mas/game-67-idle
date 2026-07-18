#include "features/items/items.h"
#include "features/items/reason_tags.h"

#include "items_state.h"
#include "items_state_events.gen.h"

#include "core/nt_assert.h"
#include "game_events.h"
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
#if defined(ITEMS_RUNTIME_TESTING) && ITEMS_RUNTIME_TESTING
static bool s_fail_next_commit;

void items_test_fail_next_commit(void) { s_fail_next_commit = true; }
#endif

#define ITEMS_EPHEMERAL_REF_BIT UINT32_C(0x80000000)
#ifndef ITEMS_EPHEMERAL_MAX_CONTAINERS
#define ITEMS_EPHEMERAL_MAX_CONTAINERS 64U
#endif
#ifndef ITEMS_EPHEMERAL_MAX_ENTRIES
#define ITEMS_EPHEMERAL_MAX_ENTRIES 512U
#endif

static ItemsItemContainer s_ephemeral_containers[ITEMS_EPHEMERAL_MAX_CONTAINERS];
static ItemsItemEntry s_ephemeral_entries[ITEMS_EPHEMERAL_MAX_ENTRIES];
static uint32_t s_ephemeral_container_generation[ITEMS_EPHEMERAL_MAX_CONTAINERS];
static uint32_t s_ephemeral_entry_generation[ITEMS_EPHEMERAL_MAX_ENTRIES];

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

static bool build_indices(
    const ItemsState *state, bool publish, bool invalidate_refs,
    char *error, int error_cap) {
    items_id_index_t containers[ITEMS_STATE_MAX_CONTAINERS];
    items_id_index_t entries[ITEMS_STATE_MAX_CONTAINERS_ENTRIES];
    uint32_t container_count = 0;
    uint32_t entry_count = 0;
    uint32_t max_container_id = 0;
    uint32_t max_entry_id = 0;

    if (state->last_container_id == ITEMS_ID_RESERVED ||
        state->last_entry_id == ITEMS_ID_RESERVED) {
        set_error(error, error_cap, "reserved persisted id counter");
        return false;
    }

    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS; i++) {
        const ItemsItemContainer *container = &state->containers[i];
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
        const ItemsItemEntry *entry = &state->containers_entries[i];
        if (!entry->used) { continue; }
        if (entry->entry_id == ITEMS_ID_NONE || entry->entry_id == ITEMS_ID_RESERVED) {
            set_error(error, error_cap, "reserved entry id");
            return false;
        }
        if (entry->parent_index < 0 || entry->parent_index >= ITEMS_STATE_MAX_CONTAINERS ||
            !state->containers[entry->parent_index].used) {
            set_error(error, error_cap, "entry has no live container");
            return false;
        }
        const ItemsItemContainer *container = &state->containers[entry->parent_index];
        if (entry->slot >= container->capacity || entry->count <= 0 || entry->def_id[0] == '\0') {
            set_error(error, error_cap, "invalid entry slot, count, or definition");
            return false;
        }
        for (uint32_t j = 0; j < i; j++) {
            const ItemsItemEntry *other = &state->containers_entries[j];
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
    if (state->last_container_id < max_container_id || state->last_entry_id < max_entry_id) {
        set_error(error, error_cap, "persisted id counter is behind live ids");
        return false;
    }

    if (publish) {
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
    }
    return true;
}

bool items_runtime_validate_state(const struct ItemsState *state, char *error, int error_cap) {
    if (state == NULL) {
        set_error(error, error_cap, "Items state is required");
        return false;
    }
    return build_indices(state, false, false, error, error_cap);
}

bool items_runtime_rebuild(char *error, int error_cap) {
    if (!build_indices(&items_state, true, true, error, error_cap)) { return false; }
    memset(s_ephemeral_containers, 0, sizeof(s_ephemeral_containers));
    memset(s_ephemeral_entries, 0, sizeof(s_ephemeral_entries));
    for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_CONTAINERS; i++) {
        s_ephemeral_container_generation[i] = next_generation(s_ephemeral_container_generation[i]);
    }
    for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_ENTRIES; i++) {
        s_ephemeral_entry_generation[i] = next_generation(s_ephemeral_entry_generation[i]);
    }
    return true;
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

static bool ephemeral_ref(uint32_t index) {
    return (index & ITEMS_EPHEMERAL_REF_BIT) != 0;
}

static uint32_t ephemeral_index(uint32_t encoded) {
    return encoded & ~ITEMS_EPHEMERAL_REF_BIT;
}

static bool try_persistent_container(
    items_container_ref_t ref, ItemsItemContainer **out_container) {
    ensure_ready();
    if (out_container == NULL || ephemeral_ref(ref.index) ||
        ref.index >= ITEMS_STATE_MAX_CONTAINERS || ref.generation == 0 ||
        s_container_generation[ref.index] != ref.generation ||
        !items_state.containers[ref.index].used) {
        return false;
    }
    *out_container = &items_state.containers[ref.index];
    return true;
}

static bool try_persistent_entry(
    item_entry_ref_t ref, ItemsItemEntry **out_entry) {
    ensure_ready();
    if (out_entry == NULL || ephemeral_ref(ref.index) ||
        ref.index >= ITEMS_STATE_MAX_CONTAINERS_ENTRIES || ref.generation == 0 ||
        s_entry_generation[ref.index] != ref.generation ||
        !items_state.containers_entries[ref.index].used) {
        return false;
    }
    *out_entry = &items_state.containers_entries[ref.index];
    return true;
}

static items_container_ref_t ephemeral_container_ref(uint32_t index) {
    return (items_container_ref_t){
        index | ITEMS_EPHEMERAL_REF_BIT,
        s_ephemeral_container_generation[index],
    };
}

static item_entry_ref_t ephemeral_entry_ref(uint32_t index) {
    return (item_entry_ref_t){
        index | ITEMS_EPHEMERAL_REF_BIT,
        s_ephemeral_entry_generation[index],
    };
}

static ItemsItemContainer *require_ephemeral_container(items_container_ref_t ref) {
    uint32_t index = ephemeral_index(ref.index);
    NT_ASSERT(ephemeral_ref(ref.index) && index < ITEMS_EPHEMERAL_MAX_CONTAINERS);
    NT_ASSERT(ref.generation != 0 && s_ephemeral_container_generation[index] == ref.generation);
    NT_ASSERT(s_ephemeral_containers[index].used);
    return &s_ephemeral_containers[index];
}

static ItemsItemEntry *require_ephemeral_entry(item_entry_ref_t ref) {
    uint32_t index = ephemeral_index(ref.index);
    NT_ASSERT(ephemeral_ref(ref.index) && index < ITEMS_EPHEMERAL_MAX_ENTRIES);
    NT_ASSERT(ref.generation != 0 && s_ephemeral_entry_generation[index] == ref.generation);
    NT_ASSERT(s_ephemeral_entries[index].used);
    return &s_ephemeral_entries[index];
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

static uint32_t ephemeral_free_container(void) {
    for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_CONTAINERS; i++) {
        if (!s_ephemeral_containers[i].used) { return i; }
    }
    return UINT32_MAX;
}

static uint32_t ephemeral_free_entry(void) {
    for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_ENTRIES; i++) {
        if (!s_ephemeral_entries[i].used) { return i; }
    }
    return UINT32_MAX;
}

static ItemsItemEntry *ephemeral_entry_at_slot(uint32_t container_index, uint32_t slot) {
    for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_ENTRIES; i++) {
        ItemsItemEntry *entry = &s_ephemeral_entries[i];
        if (entry->used && entry->parent_index == (int)container_index && entry->slot == slot) { return entry; }
    }
    return NULL;
}

static uint32_t ephemeral_choose_slot(uint32_t container_index, uint32_t requested, uint32_t ignore_entry) {
    const ItemsItemContainer *container = &s_ephemeral_containers[container_index];
    if (requested != ITEMS_SLOT_AUTO) {
        ItemsItemEntry *entry = ephemeral_entry_at_slot(container_index, requested);
        bool available = entry == NULL || (uint32_t)(entry - s_ephemeral_entries) == ignore_entry;
        return requested < container->capacity && available ? requested : UINT32_MAX;
    }
    for (uint32_t slot = 0; slot < container->capacity; slot++) {
        ItemsItemEntry *entry = ephemeral_entry_at_slot(container_index, slot);
        if (entry == NULL || (uint32_t)(entry - s_ephemeral_entries) == ignore_entry) { return slot; }
    }
    return UINT32_MAX;
}

static void ephemeral_erase_entry(uint32_t index) {
    memset(&s_ephemeral_entries[index], 0, sizeof(s_ephemeral_entries[index]));
    s_ephemeral_entry_generation[index] = next_generation(s_ephemeral_entry_generation[index]);
}

static items_result_t ephemeral_container_create(
    items_container_desc_t desc, items_container_ref_t *out_container) {
    if (desc.capacity > ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX ||
        desc.policy < ITEMS_CONTAINER_POLICY_GENERIC || desc.policy > ITEMS_CONTAINER_POLICY_EQUIPMENT) {
        return ITEMS_RESULT_CAPACITY;
    }
    uint32_t index = ephemeral_free_container();
    if (index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
    ItemsItemContainer *container = &s_ephemeral_containers[index];
    memset(container, 0, sizeof(*container));
    container->used = true;
    container->capacity = desc.capacity;
    container->policy = (int)desc.policy;
    s_ephemeral_container_generation[index] = next_generation(s_ephemeral_container_generation[index]);
    *out_container = ephemeral_container_ref(index);
    return ITEMS_RESULT_OK;
}

static items_result_t ephemeral_container_destroy(items_container_ref_t ref) {
    uint32_t index = ephemeral_index(ref.index);
    (void)require_ephemeral_container(ref);
    for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_ENTRIES; i++) {
        if (s_ephemeral_entries[i].used && s_ephemeral_entries[i].parent_index == (int)index) {
            return ITEMS_RESULT_NOT_EMPTY;
        }
    }
    memset(&s_ephemeral_containers[index], 0, sizeof(s_ephemeral_containers[index]));
    s_ephemeral_container_generation[index] = next_generation(s_ephemeral_container_generation[index]);
    return ITEMS_RESULT_OK;
}

static items_result_t ephemeral_container_resize(items_container_ref_t ref, uint32_t capacity) {
    uint32_t index = ephemeral_index(ref.index);
    ItemsItemContainer *container = require_ephemeral_container(ref);
    if (capacity > ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX) { return ITEMS_RESULT_CAPACITY; }
    for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_ENTRIES; i++) {
        const ItemsItemEntry *entry = &s_ephemeral_entries[i];
        if (entry->used && entry->parent_index == (int)index && entry->slot >= capacity) {
            return ITEMS_RESULT_CAPACITY;
        }
    }
    container->capacity = capacity;
    return ITEMS_RESULT_OK;
}

static int64_t stack_limit(item_def_ref_t ref, item_core_t core);

static items_result_t ephemeral_stack_add(
    items_container_ref_t container_ref_value, const char *def_id, int64_t count,
    uint32_t requested_slot, item_entry_ref_t *out_entry, int64_t *out_applied) {
    uint32_t container_index = ephemeral_index(container_ref_value.index);
    ItemsItemContainer *container = require_ephemeral_container(container_ref_value);
    if (count <= 0 || def_id == NULL) { return ITEMS_RESULT_INSUFFICIENT; }
    item_def_ref_t def;
    item_core_t core;
    if (!lookup_item(def_id, &def, &core) || !item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    items_result_t allowed = policy_allows(container, def, core);
    if (allowed != ITEMS_RESULT_OK) { return allowed; }
    int64_t limit = stack_limit(def, core);

    ItemsItemEntry *entry = NULL;
    for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_ENTRIES; i++) {
        ItemsItemEntry *candidate = &s_ephemeral_entries[i];
        if (!candidate->used || candidate->parent_index != (int)container_index ||
            strcmp(candidate->def_id, def_id) != 0) { continue; }
        if (requested_slot != ITEMS_SLOT_AUTO && candidate->slot != requested_slot) { continue; }
        if (requested_slot != ITEMS_SLOT_AUTO || candidate->count < limit) { entry = candidate; break; }
    }
    if (entry != NULL) {
        int64_t applied = count > limit - entry->count ? limit - entry->count : count;
        if (applied <= 0) { return ITEMS_RESULT_CAPACITY; }
        entry->count += applied;
        if (out_entry != NULL) { *out_entry = ephemeral_entry_ref((uint32_t)(entry - s_ephemeral_entries)); }
        if (out_applied != NULL) { *out_applied = applied; }
        return ITEMS_RESULT_OK;
    }

    uint32_t slot = ephemeral_choose_slot(container_index, requested_slot, UINT32_MAX);
    if (slot == UINT32_MAX) {
        return requested_slot == ITEMS_SLOT_AUTO ? ITEMS_RESULT_CAPACITY : ITEMS_RESULT_SLOT_OCCUPIED;
    }
    uint32_t index = ephemeral_free_entry();
    if (index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
    int64_t applied = count > limit ? limit : count;
    entry = &s_ephemeral_entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->used = true;
    entry->parent_index = (int)container_index;
    entry->slot = slot;
    (void)snprintf(entry->def_id, sizeof(entry->def_id), "%s", def_id);
    entry->count = applied;
    entry->level = ITEMS_STATE_ITEM_ENTRY_LEVEL_DEFAULT;
    entry->durability = ITEMS_STATE_ITEM_ENTRY_DURABILITY_DEFAULT;
    s_ephemeral_entry_generation[index] = next_generation(s_ephemeral_entry_generation[index]);
    if (out_entry != NULL) { *out_entry = ephemeral_entry_ref(index); }
    if (out_applied != NULL) { *out_applied = applied; }
    return ITEMS_RESULT_OK;
}

static int64_t ephemeral_stack_count(items_container_ref_t container_ref_value, const char *def_id) {
    uint32_t container_index = ephemeral_index(container_ref_value.index);
    (void)require_ephemeral_container(container_ref_value);
    item_core_t core;
    if (!lookup_item(def_id, NULL, &core) || !item_is_stackable(core)) { return 0; }
    int64_t total = 0;
    for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_ENTRIES; i++) {
        const ItemsItemEntry *entry = &s_ephemeral_entries[i];
        if (!entry->used || entry->quarantined || entry->parent_index != (int)container_index ||
            strcmp(entry->def_id, def_id) != 0) { continue; }
        if (entry->count > INT64_MAX - total) { return INT64_MAX; }
        total += entry->count;
    }
    return total;
}

static items_result_t ephemeral_stack_remove(item_entry_ref_t ref, int64_t count) {
    uint32_t index = ephemeral_index(ref.index);
    ItemsItemEntry *entry = require_ephemeral_entry(ref);
    item_core_t core;
    if (!lookup_item(entry->def_id, NULL, &core) || !item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    if (count <= 0 || entry->count < count) { return ITEMS_RESULT_INSUFFICIENT; }
    entry->count -= count;
    if (entry->count == 0) { ephemeral_erase_entry(index); }
    return ITEMS_RESULT_OK;
}

static items_result_t ephemeral_stack_remove_from_container(
    items_container_ref_t container_ref_value, const char *def_id, int64_t count) {
    uint32_t container_index = ephemeral_index(container_ref_value.index);
    ItemsItemContainer *container = require_ephemeral_container(container_ref_value);
    item_core_t core;
    if (!lookup_item(def_id, NULL, &core) || !item_is_stackable(core)) {
        return ITEMS_RESULT_WRONG_STORAGE;
    }
    if (count <= 0 || ephemeral_stack_count(container_ref_value, def_id) < count) { return ITEMS_RESULT_INSUFFICIENT; }
    int64_t remaining = count;
    for (uint32_t slot = 0; slot < container->capacity && remaining > 0; slot++) {
        ItemsItemEntry *entry = ephemeral_entry_at_slot(container_index, slot);
        if (entry == NULL || entry->quarantined || strcmp(entry->def_id, def_id) != 0) { continue; }
        int64_t take = entry->count < remaining ? entry->count : remaining;
        entry->count -= take;
        remaining -= take;
        if (entry->count == 0) { ephemeral_erase_entry((uint32_t)(entry - s_ephemeral_entries)); }
    }
    NT_ASSERT(remaining == 0);
    return ITEMS_RESULT_OK;
}

static items_result_t ephemeral_unique_create(
    items_container_ref_t container_ref_value, const char *def_id,
    uint32_t requested_slot, item_entry_ref_t *out_entry) {
    uint32_t container_index = ephemeral_index(container_ref_value.index);
    ItemsItemContainer *container = require_ephemeral_container(container_ref_value);
    item_def_ref_t def;
    item_core_t core;
    if (!lookup_item(def_id, &def, &core) || item_is_stackable(core) || items_has_currency(def)) {
        return ITEMS_RESULT_WRONG_STORAGE;
    }
    items_result_t allowed = policy_allows(container, def, core);
    if (allowed != ITEMS_RESULT_OK) { return allowed; }
    uint32_t slot = ephemeral_choose_slot(container_index, requested_slot, UINT32_MAX);
    if (slot == UINT32_MAX) { return requested_slot == ITEMS_SLOT_AUTO ? ITEMS_RESULT_CAPACITY : ITEMS_RESULT_SLOT_OCCUPIED; }
    uint32_t index = ephemeral_free_entry();
    if (index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
    ItemsItemEntry *entry = &s_ephemeral_entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->used = true;
    entry->parent_index = (int)container_index;
    entry->slot = slot;
    (void)snprintf(entry->def_id, sizeof(entry->def_id), "%s", def_id);
    entry->count = 1;
    entry->level = ITEMS_STATE_ITEM_ENTRY_LEVEL_DEFAULT;
    entry->durability = ITEMS_STATE_ITEM_ENTRY_DURABILITY_DEFAULT;
    s_ephemeral_entry_generation[index] = next_generation(s_ephemeral_entry_generation[index]);
    *out_entry = ephemeral_entry_ref(index);
    return ITEMS_RESULT_OK;
}

static items_result_t ephemeral_entry_destroy(item_entry_ref_t ref) {
    uint32_t index = ephemeral_index(ref.index);
    ItemsItemEntry *entry = require_ephemeral_entry(ref);
    item_core_t core;
    if (!lookup_item(entry->def_id, NULL, &core) || item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    ephemeral_erase_entry(index);
    return ITEMS_RESULT_OK;
}

items_result_t items_try_container_create(items_container_desc_t desc, items_container_ref_t *out_container) {
    ensure_ready();
    NT_ASSERT(out_container != NULL);
    if (desc.lifetime == ITEMS_LIFETIME_EPHEMERAL) {
        return ephemeral_container_create(desc, out_container);
    }
    if (desc.lifetime != ITEMS_LIFETIME_PERSISTENT) { return ITEMS_RESULT_LIFETIME; }
    if (desc.capacity > ITEMS_STATE_ITEM_CONTAINER_CAPACITY_MAX ||
        desc.policy < ITEMS_CONTAINER_POLICY_GENERIC || desc.policy > ITEMS_CONTAINER_POLICY_EQUIPMENT) {
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
    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    *out_container = container_ref(index);
    return ITEMS_RESULT_OK;
}

items_result_t items_try_container_destroy_empty(items_container_ref_t ref) {
    if (ephemeral_ref(ref.index)) { return ephemeral_container_destroy(ref); }
    ItemsItemContainer *container = require_container(ref);
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        if (items_state.containers_entries[i].used &&
            items_state.containers_entries[i].parent_index == (int)ref.index) {
            return ITEMS_RESULT_NOT_EMPTY;
        }
    }
    memset(container, 0, sizeof(*container));
    s_container_generation[ref.index] = next_generation(s_container_generation[ref.index]);
    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    return ITEMS_RESULT_OK;
}

items_result_t items_try_container_resize(items_container_ref_t ref, uint32_t capacity) {
    if (ephemeral_ref(ref.index)) { return ephemeral_container_resize(ref, capacity); }
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
    NT_ASSERT(!ephemeral_ref(ref.index) && "ephemeral containers have no persistent id");
    if (ephemeral_ref(ref.index)) { return ITEMS_ID_NONE; }
    return require_container(ref)->container_id;
}

uint32_t items_container_capacity(items_container_ref_t ref) {
    if (ephemeral_ref(ref.index)) { return require_ephemeral_container(ref)->capacity; }
    return require_container(ref)->capacity;
}

items_container_policy_t items_container_policy(items_container_ref_t ref) {
    const ItemsItemContainer *container = ephemeral_ref(ref.index)
        ? require_ephemeral_container(ref) : require_container(ref);
    return (items_container_policy_t)container->policy;
}

items_lifetime_t items_container_lifetime(items_container_ref_t ref) {
    if (ephemeral_ref(ref.index)) {
        (void)require_ephemeral_container(ref);
        return ITEMS_LIFETIME_EPHEMERAL;
    }
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

static bool audit_txn_available(const char *op, const char *def_id, const char *reason) {
    const size_t size = sizeof(ItemsEvTxn) + strlen(op) + 1U + strlen(def_id) + 1U +
                        strlen(reason) + 1U;
    return size <= UINT32_MAX &&
           game_event_can_emit((uint32_t)size, _Alignof(ItemsEvTxn));
}

static bool audit_payment_available(const char *reason) {
    const size_t size = sizeof(ItemsEvPayment) + strlen(reason) + 1U;
    return size <= UINT32_MAX &&
           game_event_can_emit((uint32_t)size, _Alignof(ItemsEvPayment));
}

static bool audit_acquire_available(const char *reason) {
    const size_t size = sizeof(ItemsEvAcquire) + strlen(reason) + 1U;
    return size <= UINT32_MAX &&
           game_event_can_emit((uint32_t)size, _Alignof(ItemsEvAcquire));
}

static bool audit_upgrade_available(const char *reason) {
    const size_t size = sizeof(ItemsEvUpgrade) + strlen(reason) + 1U;
    return size <= UINT32_MAX &&
           game_event_can_emit((uint32_t)size, _Alignof(ItemsEvUpgrade));
}

static bool audit_move_available(const char *def_id, const char *reason) {
    const size_t size = sizeof(ItemsEvMove) + strlen(def_id) + 1U + strlen(reason) + 1U;
    return size <= UINT32_MAX &&
           game_event_can_emit((uint32_t)size, _Alignof(ItemsEvMove));
}

items_result_t items_try_stack_add(
    items_container_ref_t container_ref_value, const char *def_id, int64_t count,
    uint32_t requested_slot, const char *reason, item_entry_ref_t *out_entry, int64_t *out_applied) {
    if (!items_reason_check(reason)) { return ITEMS_RESULT_INVALID_REASON; }
    if (ephemeral_ref(container_ref_value.index)) {
        return ephemeral_stack_add(
            container_ref_value, def_id, count, requested_slot, out_entry, out_applied);
    }
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
        if (!audit_txn_available("add", def_id, reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }
        int64_t before = entry->count;
        entry->count += applied;
        if (entry->quarantined) { entry->quarantined = false; }
        game_save_mark_dirty();
        items_emit_txn("add", def_id, (int64_t)container->container_id,
                       (int64_t)entry->entry_id, count, applied, before, entry->count, reason);
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
    int64_t applied = count > limit ? limit : count;
    if (applied <= 0) { return ITEMS_RESULT_CAPACITY; }
    if (items_state.last_entry_id >= ITEMS_ID_RESERVED - 1U) { return ITEMS_RESULT_ID_EXHAUSTED; }
    if (!audit_txn_available("add", def_id, reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }
    uint32_t id = 0;
    bool allocated = allocate_id(&items_state.last_entry_id, &id);
    NT_ASSERT(allocated);
    (void)allocated;

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
    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    items_emit_txn("add", def_id, (int64_t)container->container_id,
                   (int64_t)id, count, applied, 0, applied, reason);
    if (out_entry != NULL) { *out_entry = entry_ref(index); }
    if (out_applied != NULL) { *out_applied = applied; }
    return ITEMS_RESULT_OK;
}

items_result_t items_try_stack_remove(item_entry_ref_t entry_ref_value, int64_t count, const char *reason) {
    if (!items_reason_check(reason)) { return ITEMS_RESULT_INVALID_REASON; }
    if (ephemeral_ref(entry_ref_value.index)) { return ephemeral_stack_remove(entry_ref_value, count); }
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
    if (!audit_txn_available("remove", def_id, reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }
    entry->count -= count;
    if (entry->count == 0) {
        memset(entry, 0, sizeof(*entry));
        s_entry_generation[entry_ref_value.index] = next_generation(s_entry_generation[entry_ref_value.index]);
        bool ok = build_indices(&items_state, true, false, NULL, 0);
        NT_ASSERT(ok);
    }
    game_save_mark_dirty();
    items_emit_txn("remove", def_id,
                   (int64_t)items_state.containers[container_index].container_id,
                   (int64_t)entry_id_value, -count, -count, before, before - count, reason);
    return ITEMS_RESULT_OK;
}

items_result_t items_try_stack_remove_from_container(
    items_container_ref_t container_ref_value, const char *def_id, int64_t count, const char *reason) {
    if (!items_reason_check(reason)) { return ITEMS_RESULT_INVALID_REASON; }
    if (ephemeral_ref(container_ref_value.index)) {
        return ephemeral_stack_remove_from_container(container_ref_value, def_id, count);
    }
    ItemsItemContainer *container = require_container(container_ref_value);
    item_core_t core;
    if (!lookup_item(def_id, NULL, &core) || !item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    int64_t before = items_stack_count(container_ref_value, def_id);
    if (count <= 0 || before < count) { return ITEMS_RESULT_INSUFFICIENT; }
    if (!audit_txn_available("remove", def_id, reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }

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
    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    items_emit_txn("remove", def_id, (int64_t)container->container_id,
                   0, -count, -count, before, before - count, reason);
    return ITEMS_RESULT_OK;
}

int64_t items_stack_count(items_container_ref_t container_ref_value, const char *def_id) {
    if (ephemeral_ref(container_ref_value.index)) {
        return ephemeral_stack_count(container_ref_value, def_id);
    }
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
    if (!items_reason_check(reason)) { return ITEMS_RESULT_INVALID_REASON; }
    if (ephemeral_ref(container_ref_value.index)) {
        NT_ASSERT(out_entry != NULL);
        return ephemeral_unique_create(container_ref_value, def_id, requested_slot, out_entry);
    }
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
    if (items_state.last_entry_id >= ITEMS_ID_RESERVED - 1U) { return ITEMS_RESULT_ID_EXHAUSTED; }
    if (!audit_txn_available("create", def_id, reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }
    uint32_t id = 0;
    bool allocated = allocate_id(&items_state.last_entry_id, &id);
    NT_ASSERT(allocated);
    (void)allocated;

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
    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    items_emit_txn("create", def_id, (int64_t)container->container_id,
                   (int64_t)id, 1, 1, 0, 1, reason);
    *out_entry = entry_ref(index);
    return ITEMS_RESULT_OK;
}

items_result_t items_try_entry_destroy(item_entry_ref_t entry_ref_value, const char *reason) {
    if (!items_reason_check(reason)) { return ITEMS_RESULT_INVALID_REASON; }
    if (ephemeral_ref(entry_ref_value.index)) { return ephemeral_entry_destroy(entry_ref_value); }
    ItemsItemEntry *entry = require_entry(entry_ref_value);
    item_core_t core;
    if (!lookup_item(entry->def_id, NULL, &core) || item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    char def_id[ITEMS_STATE_STRING_MAX];
    (void)snprintf(def_id, sizeof(def_id), "%s", entry->def_id);
    uint32_t container_id_value = items_state.containers[entry->parent_index].container_id;
    uint32_t entry_id_value = entry->entry_id;
    if (!audit_txn_available("destroy", def_id, reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }
    memset(entry, 0, sizeof(*entry));
    s_entry_generation[entry_ref_value.index] = next_generation(s_entry_generation[entry_ref_value.index]);
    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    items_emit_txn("destroy", def_id, (int64_t)container_id_value,
                   (int64_t)entry_id_value, -1, -1, 1, 0, reason);
    return ITEMS_RESULT_OK;
}

static ItemsItemEntry *entry_at_slot(uint32_t parent_index, uint32_t slot) {
    for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        ItemsItemEntry *entry = &items_state.containers_entries[i];
        if (entry->used && entry->parent_index == (int)parent_index && entry->slot == slot) { return entry; }
    }
    return NULL;
}

static items_result_t ephemeral_entry_move(
    item_entry_ref_t source_ref, items_container_ref_t destination_ref,
    int64_t count, uint32_t requested_slot, const char *reason,
    item_entry_ref_t *out_destination) {
    uint32_t source_index = ephemeral_index(source_ref.index);
    ItemsItemEntry *source = require_ephemeral_entry(source_ref);
    if (count <= 0 || count > source->count) { return ITEMS_RESULT_INSUFFICIENT; }
    item_def_ref_t def;
    item_core_t core;
    if (!lookup_item(source->def_id, &def, &core) || source->quarantined) { return ITEMS_RESULT_NOT_FOUND; }
    int64_t limit = item_is_stackable(core) ? stack_limit(def, core) : 1;

    if (!ephemeral_ref(destination_ref.index)) {
        ItemsItemContainer *destination = require_container(destination_ref);
        items_result_t allowed = policy_allows(destination, def, core);
        if (allowed != ITEMS_RESULT_OK) { return allowed; }

        item_entry_ref_t acquired = ITEM_ENTRY_REF_NONE;
        items_result_t result;
        if (!item_is_stackable(core)) {
            if (count != 1) { return ITEMS_RESULT_WRONG_STORAGE; }
            result = items_try_unique_create(
                destination_ref, source->def_id, requested_slot, reason, &acquired);
            if (result != ITEMS_RESULT_OK) { return result; }
            ItemsItemEntry *persistent = require_entry(acquired);
            persistent->level = source->level;
            persistent->durability = source->durability;
        } else {
            if (count > limit) { return ITEMS_RESULT_CAPACITY; }
            uint32_t target_slot = requested_slot;
            if (target_slot == ITEMS_SLOT_AUTO) {
                for (uint32_t slot = 0; slot < destination->capacity; slot++) {
                    ItemsItemEntry *candidate = entry_at_slot(destination_ref.index, slot);
                    if (candidate != NULL && !candidate->quarantined &&
                        strcmp(candidate->def_id, source->def_id) == 0 &&
                        candidate->count <= limit - count) {
                        target_slot = slot;
                        break;
                    }
                }
                if (target_slot == ITEMS_SLOT_AUTO) {
                    target_slot = choose_slot(destination_ref.index, ITEMS_SLOT_AUTO, UINT32_MAX);
                }
            } else {
                ItemsItemEntry *candidate = entry_at_slot(destination_ref.index, target_slot);
                if (candidate != NULL &&
                    (candidate->quarantined || strcmp(candidate->def_id, source->def_id) != 0 ||
                     candidate->count > limit - count)) {
                    return ITEMS_RESULT_SLOT_OCCUPIED;
                }
            }
            if (target_slot == UINT32_MAX || target_slot >= destination->capacity) {
                return ITEMS_RESULT_CAPACITY;
            }
            int64_t applied = 0;
            result = items_try_stack_add(
                destination_ref, source->def_id, count, target_slot,
                reason, &acquired, &applied);
            NT_ASSERT(result != ITEMS_RESULT_OK || applied == count);
            if (result != ITEMS_RESULT_OK) { return result; }
        }

        if (count == source->count) { ephemeral_erase_entry(source_index); }
        else { source->count -= count; }
        if (out_destination != NULL) { *out_destination = acquired; }
        return ITEMS_RESULT_OK;
    }

    uint32_t destination_index = ephemeral_index(destination_ref.index);
    ItemsItemContainer *destination = require_ephemeral_container(destination_ref);
    items_result_t allowed = policy_allows(destination, def, core);
    if (allowed != ITEMS_RESULT_OK) { return allowed; }

    bool whole = count == source->count;
    bool same_container = source->parent_index == (int)destination_index;
    if (!whole && same_container && requested_slot == source->slot) {
        return ITEMS_RESULT_SLOT_OCCUPIED;
    }
    uint32_t target_slot = requested_slot;
    ItemsItemEntry *merge = NULL;
    if (target_slot == ITEMS_SLOT_AUTO && item_is_stackable(core)) {
        for (uint32_t slot = 0; slot < destination->capacity; slot++) {
            ItemsItemEntry *candidate = ephemeral_entry_at_slot(destination_index, slot);
            if (candidate != NULL && candidate != source &&
                strcmp(candidate->def_id, source->def_id) == 0 &&
                candidate->count <= limit - count) {
                target_slot = slot;
                merge = candidate;
                break;
            }
        }
    }
    if (target_slot == ITEMS_SLOT_AUTO) {
        target_slot = ephemeral_choose_slot(
            destination_index, ITEMS_SLOT_AUTO,
            whole ? source_index : UINT32_MAX);
    } else {
        if (target_slot >= destination->capacity) { return ITEMS_RESULT_CAPACITY; }
        merge = ephemeral_entry_at_slot(destination_index, target_slot);
        if (merge == source) { merge = NULL; }
    }
    if (target_slot == UINT32_MAX) { return ITEMS_RESULT_CAPACITY; }
    if (merge != NULL &&
        (!item_is_stackable(core) || strcmp(merge->def_id, source->def_id) != 0 ||
         merge->count > limit - count)) {
        return ITEMS_RESULT_SLOT_OCCUPIED;
    }
    if (merge == NULL) {
        ItemsItemEntry *occupied = ephemeral_entry_at_slot(destination_index, target_slot);
        if (occupied != NULL && occupied != source) { return ITEMS_RESULT_SLOT_OCCUPIED; }
    }

    item_entry_ref_t result_ref = source_ref;
    if (merge != NULL) {
        merge->count += count;
        result_ref = ephemeral_entry_ref((uint32_t)(merge - s_ephemeral_entries));
        if (whole) { ephemeral_erase_entry(source_index); }
        else { source->count -= count; }
    } else if (whole) {
        source->parent_index = (int)destination_index;
        source->slot = target_slot;
    } else {
        if (!item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
        uint32_t split_index = ephemeral_free_entry();
        if (split_index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
        ItemsItemEntry *split = &s_ephemeral_entries[split_index];
        *split = *source;
        split->parent_index = (int)destination_index;
        split->slot = target_slot;
        split->count = count;
        source->count -= count;
        s_ephemeral_entry_generation[split_index] = next_generation(s_ephemeral_entry_generation[split_index]);
        result_ref = ephemeral_entry_ref(split_index);
    }
    if (out_destination != NULL) { *out_destination = result_ref; }
    return ITEMS_RESULT_OK;
}

typedef struct items_payment_plan_row_t {
    uint32_t entry_index;
    int64_t count;
} items_payment_plan_row_t;

typedef struct items_payment_plan_t {
    items_payment_plan_row_t rows[ITEMS_STATE_MAX_CONTAINERS_ENTRIES];
    uint32_t row_count;
    uint32_t requirement_count;
    uint32_t scope_count;
    int64_t requested_units;
    uint64_t cost_fingerprint;
    uint64_t scope_fingerprint;
    uint64_t source_fingerprint;
} items_payment_plan_t;

#define ITEMS_PAYMENT_FINGERPRINT_OFFSET UINT64_C(14695981039346656037)
#define ITEMS_PAYMENT_FINGERPRINT_PRIME UINT64_C(1099511628211)

static void payment_fingerprint_bytes(uint64_t *fingerprint, const void *bytes, uint32_t count) {
    const uint8_t *data = bytes;
    for (uint32_t i = 0; i < count; i++) {
        *fingerprint ^= data[i];
        *fingerprint *= ITEMS_PAYMENT_FINGERPRINT_PRIME;
    }
}

static void payment_fingerprint_u64(uint64_t *fingerprint, uint64_t value) {
    uint8_t little_endian[8];
    for (uint32_t i = 0; i < 8; i++) {
        little_endian[i] = (uint8_t)(value >> (i * 8U));
    }
    payment_fingerprint_bytes(fingerprint, little_endian, 8);
}

static uint64_t payment_fingerprint_begin(const char *domain) {
    uint64_t fingerprint = ITEMS_PAYMENT_FINGERPRINT_OFFSET;
    payment_fingerprint_bytes(&fingerprint, domain, (uint32_t)strlen(domain));
    return fingerprint;
}

static bool entry_matches_item(const ItemsItemEntry *entry, item_id_t item) {
    item_def_ref_t ref;
    return !entry->quarantined && items_try_get_string(entry->def_id, &ref) &&
        items_core(ref).id.value == item.value;
}

static bool transaction_commit_allowed(void) {
#if defined(ITEMS_RUNTIME_TESTING) && ITEMS_RUNTIME_TESTING
    if (s_fail_next_commit) {
        s_fail_next_commit = false;
        return false;
    }
#endif
    return true;
}

static items_result_t build_payment_plan(
    item_cost_ref_t cost, items_payment_scope_t scope, items_payment_plan_t *out_plan) {
    NT_ASSERT(out_plan != NULL);
    ensure_ready();
    memset(out_plan, 0, sizeof(*out_plan));
    if (scope.count == 0 || scope.count > ITEMS_PAYMENT_SCOPE_MAX) {
        return ITEMS_RESULT_INVALID_ARGUMENT;
    }
    out_plan->scope_count = scope.count;
    out_plan->scope_fingerprint = payment_fingerprint_begin("items.payment.scope.v1");
    payment_fingerprint_u64(&out_plan->scope_fingerprint, scope.count);
    for (uint32_t i = 0; i < scope.count; i++) {
        if (ephemeral_ref(scope.containers[i].index)) { return ITEMS_RESULT_LIFETIME; }
        ItemsItemContainer *container = NULL;
        if (!try_persistent_container(scope.containers[i], &container)) {
            return ITEMS_RESULT_NOT_FOUND;
        }
        for (uint32_t j = 0; j < i; j++) {
            if (scope.containers[i].index == scope.containers[j].index &&
                scope.containers[i].generation == scope.containers[j].generation) {
                return ITEMS_RESULT_INVALID_ARGUMENT;
            }
        }
        payment_fingerprint_u64(&out_plan->scope_fingerprint, container->container_id);
    }

    out_plan->requirement_count = items_cost_count(cost);
    out_plan->cost_fingerprint = payment_fingerprint_begin("items.payment.cost.v1");
    NT_ASSERT(out_plan->requirement_count > 0);
    payment_fingerprint_u64(&out_plan->cost_fingerprint, out_plan->requirement_count);
    for (uint32_t requirement = 0; requirement < out_plan->requirement_count; requirement++) {
        item_cost_entry_t required = items_cost_at(cost, requirement);
        NT_ASSERT(required.count > 0 && "runtime cost contains a non-positive requirement");
        NT_ASSERT(out_plan->requested_units <= INT64_MAX - required.count &&
                  "runtime payment cost total overflows i64");
        out_plan->requested_units += required.count;
        item_def_ref_t required_ref = items_get(required.item);
        NT_ASSERT(item_is_stackable(items_core(required_ref)) &&
                  "runtime payment cost must contain only stack resources");
        for (uint32_t prior = 0; prior < requirement; prior++) {
            NT_ASSERT(items_cost_at(cost, prior).item.value != required.item.value &&
                      "runtime cost contains duplicate requirements");
        }
        payment_fingerprint_u64(&out_plan->cost_fingerprint, required.item.value);
        payment_fingerprint_u64(&out_plan->cost_fingerprint, (uint64_t)required.count);
    }

    for (uint32_t requirement = 0; requirement < out_plan->requirement_count; requirement++) {
        item_cost_entry_t required = items_cost_at(cost, requirement);
        int64_t remaining = required.count;
        for (uint32_t source = 0; source < scope.count && remaining > 0; source++) {
            ItemsItemContainer *container = require_container(scope.containers[source]);
            for (uint32_t slot = 0; slot < container->capacity && remaining > 0; slot++) {
                ItemsItemEntry *entry = entry_at_slot(scope.containers[source].index, slot);
                if (entry == NULL || !entry_matches_item(entry, required.item)) { continue; }
                int64_t take = entry->count < remaining ? entry->count : remaining;
                NT_ASSERT(out_plan->row_count < ITEMS_STATE_MAX_CONTAINERS_ENTRIES);
                out_plan->rows[out_plan->row_count++] = (items_payment_plan_row_t){
                    (uint32_t)(entry - items_state.containers_entries), take};
                remaining -= take;
            }
        }
        if (remaining > 0) { return ITEMS_RESULT_INSUFFICIENT; }
    }

    out_plan->source_fingerprint = payment_fingerprint_begin("items.payment.source.v1");
    payment_fingerprint_u64(&out_plan->source_fingerprint, out_plan->row_count);
    for (uint32_t i = 0; i < out_plan->row_count; i++) {
        const ItemsItemEntry *entry = &items_state.containers_entries[out_plan->rows[i].entry_index];
        const ItemsItemContainer *container = &items_state.containers[entry->parent_index];
        item_def_ref_t def;
        bool found = items_try_get_string(entry->def_id, &def);
        NT_ASSERT(found);
        if (!found) { return ITEMS_RESULT_NOT_FOUND; }
        payment_fingerprint_u64(&out_plan->source_fingerprint, container->container_id);
        payment_fingerprint_u64(&out_plan->source_fingerprint, entry->entry_id);
        payment_fingerprint_u64(&out_plan->source_fingerprint, entry->slot);
        payment_fingerprint_u64(&out_plan->source_fingerprint, items_core(def).id.value);
        payment_fingerprint_u64(&out_plan->source_fingerprint, (uint64_t)out_plan->rows[i].count);
    }
    return ITEMS_RESULT_OK;
}

static void apply_payment_plan(const items_payment_plan_t *plan) {
    for (uint32_t i = 0; i < plan->row_count; i++) {
        ItemsItemEntry *entry = &items_state.containers_entries[plan->rows[i].entry_index];
        NT_ASSERT(entry->used && entry->count >= plan->rows[i].count);
        entry->count -= plan->rows[i].count;
        if (entry->count == 0) {
            memset(entry, 0, sizeof(*entry));
            s_entry_generation[plan->rows[i].entry_index] =
                next_generation(s_entry_generation[plan->rows[i].entry_index]);
        }
    }
}

items_result_t items_try_pay_cost(
    item_cost_ref_t cost, items_payment_scope_t scope, const char *reason) {
    if (!items_reason_check(reason)) { return ITEMS_RESULT_INVALID_REASON; }
    items_payment_plan_t plan;
    items_result_t result = build_payment_plan(cost, scope, &plan);
    if (result != ITEMS_RESULT_OK) { return result; }
    if (!transaction_commit_allowed()) { return ITEMS_RESULT_COMMIT_FAILED; }
    if (!audit_payment_available(reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }
    apply_payment_plan(&plan);
    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    items_emit_payment(
        (nt_hash64_t){plan.cost_fingerprint}, (nt_hash64_t){plan.scope_fingerprint},
        (nt_hash64_t){plan.source_fingerprint}, plan.requirement_count,
        plan.scope_count, plan.row_count, plan.requested_units, plan.requested_units, reason);
    return ITEMS_RESULT_OK;
}

static int64_t payment_planned_count(
    const items_payment_plan_t *plan, uint32_t entry_index) {
    if (plan == NULL) { return 0; }
    for (uint32_t i = 0; i < plan->row_count; i++) {
        if (plan->rows[i].entry_index == entry_index) { return plan->rows[i].count; }
    }
    return 0;
}

items_result_t items_try_acquire(
    items_container_ref_t destination_ref, item_def_ref_t item,
    items_payment_scope_t payment, const char *reason,
    item_entry_ref_t *out_entry) {
    if (!items_reason_check(reason)) { return ITEMS_RESULT_INVALID_REASON; }
    if (out_entry == NULL) { return ITEMS_RESULT_INVALID_ARGUMENT; }
    if (ephemeral_ref(destination_ref.index)) { return ITEMS_RESULT_WRONG_STORAGE; }
    ItemsItemContainer *destination = NULL;
    if (!try_persistent_container(destination_ref, &destination)) {
        return ITEMS_RESULT_NOT_FOUND;
    }
    item_core_t core = items_core(item);
    const char *def_id = items_def_id(item);
    item_transition_t transition = items_acquire_transition(item);
    if (transition.kind == ITEM_TRANSITION_UNAVAILABLE) { return ITEMS_RESULT_NOT_FOUND; }

    items_payment_plan_t payment_plan;
    items_payment_plan_t *plan = NULL;
    if (transition.kind == ITEM_TRANSITION_COST) {
        items_result_t result = build_payment_plan(transition.cost, payment, &payment_plan);
        if (result != ITEMS_RESULT_OK) { return result; }
        plan = &payment_plan;
    } else {
        NT_ASSERT(transition.kind == ITEM_TRANSITION_FREE);
    }

    items_result_t allowed = policy_allows(destination, item, core);
    if (allowed != ITEMS_RESULT_OK) { return allowed; }

    const int64_t limit = item_is_stackable(core) ? stack_limit(item, core) : 1;
    ItemsItemEntry *merge = NULL;
    if (item_is_stackable(core)) {
        for (uint32_t slot = 0; slot < destination->capacity; slot++) {
            ItemsItemEntry *candidate = entry_at_slot(destination_ref.index, slot);
            if (candidate == NULL || candidate->quarantined || strcmp(candidate->def_id, def_id) != 0) {
                continue;
            }
            uint32_t index = (uint32_t)(candidate - items_state.containers_entries);
            int64_t projected = candidate->count - payment_planned_count(plan, index);
            NT_ASSERT(projected >= 0);
            if (projected > 0 && projected < limit) {
                merge = candidate;
                break;
            }
        }
    }

    uint32_t target_slot = ITEMS_SLOT_AUTO;
    uint32_t new_index = UINT32_MAX;
    uint32_t new_id = 0;
    if (merge != NULL) {
        target_slot = merge->slot;
    } else {
        for (uint32_t slot = 0; slot < destination->capacity; slot++) {
            ItemsItemEntry *entry = entry_at_slot(destination_ref.index, slot);
            if (entry == NULL) {
                target_slot = slot;
                break;
            }
            uint32_t index = (uint32_t)(entry - items_state.containers_entries);
            if (entry->count == payment_planned_count(plan, index)) {
                target_slot = slot;
                break;
            }
        }
        if (target_slot == ITEMS_SLOT_AUTO) { return ITEMS_RESULT_CAPACITY; }
        for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
            ItemsItemEntry *entry = &items_state.containers_entries[i];
            if (!entry->used || entry->count == payment_planned_count(plan, i)) {
                new_index = i;
                break;
            }
        }
        if (new_index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
        if (items_state.last_entry_id >= ITEMS_ID_RESERVED - 1U) {
            return ITEMS_RESULT_ID_EXHAUSTED;
        }
        new_id = items_state.last_entry_id + 1U;
    }

    if (!transaction_commit_allowed()) { return ITEMS_RESULT_COMMIT_FAILED; }
    if (!audit_acquire_available(reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }
    if (plan != NULL) { apply_payment_plan(plan); }
    ItemsItemEntry *acquired;
    if (merge != NULL) {
        uint32_t merge_index = (uint32_t)(merge - items_state.containers_entries);
        acquired = &items_state.containers_entries[merge_index];
        NT_ASSERT(acquired->used && acquired->count < limit);
        acquired->count++;
    } else {
        acquired = &items_state.containers_entries[new_index];
        NT_ASSERT(!acquired->used);
        memset(acquired, 0, sizeof(*acquired));
        acquired->used = true;
        acquired->parent_index = (int)destination_ref.index;
        acquired->entry_id = new_id;
        acquired->slot = target_slot;
        (void)snprintf(acquired->def_id, sizeof(acquired->def_id), "%s", def_id);
        acquired->count = 1;
        acquired->level = ITEMS_STATE_ITEM_ENTRY_LEVEL_DEFAULT;
        acquired->durability = ITEMS_STATE_ITEM_ENTRY_DURABILITY_DEFAULT;
        items_state.last_entry_id = new_id;
        s_entry_generation[new_index] = next_generation(s_entry_generation[new_index]);
    }

    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    const bool paid = plan != NULL;
    items_emit_acquire(
        (nt_hash64_t){core.id.value}, destination->container_id, acquired->entry_id, paid,
        (nt_hash64_t){paid ? plan->cost_fingerprint : 0},
        (nt_hash64_t){paid ? plan->scope_fingerprint : 0},
        (nt_hash64_t){paid ? plan->source_fingerprint : 0},
        paid ? plan->requirement_count : 0,
        paid ? plan->requested_units : 0, reason);
    *out_entry = entry_ref((uint32_t)(acquired - items_state.containers_entries));
    return ITEMS_RESULT_OK;
}

items_result_t items_try_upgrade_instance(
    item_entry_ref_t entry_ref_value, uint32_t target_level,
    items_payment_scope_t payment, const char *reason) {
    if (!items_reason_check(reason)) { return ITEMS_RESULT_INVALID_REASON; }
    if (ephemeral_ref(entry_ref_value.index)) { return ITEMS_RESULT_WRONG_STORAGE; }
    ItemsItemEntry *entry = NULL;
    if (!try_persistent_entry(entry_ref_value, &entry)) { return ITEMS_RESULT_NOT_FOUND; }
    item_def_ref_t item;
    item_core_t core;
    if (entry->quarantined || !lookup_item(entry->def_id, &item, &core)) {
        return ITEMS_RESULT_NOT_FOUND;
    }
    if (item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
    if (target_level != (uint32_t)entry->level + 1U) { return ITEMS_RESULT_STALE_LEVEL; }
    if (target_level > ITEMS_STATE_ITEM_ENTRY_LEVEL_MAX) { return ITEMS_RESULT_NOT_FOUND; }
    if (!items_level_exists(item, target_level)) { return ITEMS_RESULT_NOT_FOUND; }
    item_transition_t transition = items_level_transition(item, target_level);
    if (transition.kind == ITEM_TRANSITION_UNAVAILABLE) { return ITEMS_RESULT_NOT_FOUND; }

    items_payment_plan_t payment_plan;
    items_payment_plan_t *plan = NULL;
    if (transition.kind == ITEM_TRANSITION_COST) {
        items_result_t result = build_payment_plan(transition.cost, payment, &payment_plan);
        if (result != ITEMS_RESULT_OK) { return result; }
        plan = &payment_plan;
    } else {
        NT_ASSERT(transition.kind == ITEM_TRANSITION_FREE);
    }

    const int from_level = entry->level;
    const uint32_t entry_id_value = entry->entry_id;
    if (!transaction_commit_allowed()) { return ITEMS_RESULT_COMMIT_FAILED; }
    if (!audit_upgrade_available(reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }
    if (plan != NULL) { apply_payment_plan(plan); }
    entry = require_entry(entry_ref_value);
    entry->level = (int)target_level;
    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    const bool paid = plan != NULL;
    items_emit_upgrade(
        (nt_hash64_t){core.id.value}, entry_id_value, from_level, (int)target_level, paid,
        (nt_hash64_t){paid ? plan->cost_fingerprint : 0},
        (nt_hash64_t){paid ? plan->scope_fingerprint : 0},
        (nt_hash64_t){paid ? plan->source_fingerprint : 0},
        paid ? plan->requirement_count : 0,
        paid ? plan->requested_units : 0, reason);
    return ITEMS_RESULT_OK;
}

items_result_t items_try_entry_move(
    item_entry_ref_t source_ref, items_container_ref_t destination_ref,
    int64_t count, uint32_t requested_slot, const char *reason,
    item_entry_ref_t *out_destination) {
    if (!items_reason_check(reason)) { return ITEMS_RESULT_INVALID_REASON; }
    if (ephemeral_ref(source_ref.index)) {
        return ephemeral_entry_move(
            source_ref, destination_ref, count, requested_slot, reason, out_destination);
    }
    if (ephemeral_ref(destination_ref.index)) { return ITEMS_RESULT_LIFETIME; }
    ItemsItemEntry *source = NULL;
    ItemsItemContainer *destination = NULL;
    if (!try_persistent_entry(source_ref, &source) ||
        !try_persistent_container(destination_ref, &destination)) {
        return ITEMS_RESULT_NOT_FOUND;
    }
    if (count <= 0 || count > source->count) { return ITEMS_RESULT_INSUFFICIENT; }
    item_def_ref_t def;
    item_core_t core;
    if (!lookup_item(source->def_id, &def, &core) || source->quarantined) { return ITEMS_RESULT_NOT_FOUND; }
    items_result_t allowed = policy_allows(destination, def, core);
    if (allowed != ITEMS_RESULT_OK) { return allowed; }
    bool whole = count == source->count;

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
        target_slot = choose_slot(
            destination_ref.index, ITEMS_SLOT_AUTO, whole ? source_ref.index : UINT32_MAX);
    } else {
        if (target_slot >= destination->capacity) { return ITEMS_RESULT_CAPACITY; }
        merge = entry_at_slot(destination_ref.index, target_slot);
        if (merge == source) {
            if (!whole) { return ITEMS_RESULT_SLOT_OCCUPIED; }
            merge = NULL;
        }
    }
    if (target_slot == UINT32_MAX) { return ITEMS_RESULT_CAPACITY; }
    if (merge != NULL) {
        if (!item_is_stackable(core) || strcmp(merge->def_id, source->def_id) != 0 || merge->count > limit - count) {
            return ITEMS_RESULT_SLOT_OCCUPIED;
        }
    } else if (slot_used(
                   destination_ref.index, target_slot, whole ? source_ref.index : UINT32_MAX)) {
        return ITEMS_RESULT_SLOT_OCCUPIED;
    }

    uint32_t split_index = UINT32_MAX;
    uint32_t split_id = ITEMS_ID_NONE;
    if (merge == NULL && !whole) {
        if (!item_is_stackable(core)) { return ITEMS_RESULT_WRONG_STORAGE; }
        split_index = free_entry_index();
        if (split_index == UINT32_MAX) { return ITEMS_RESULT_POOL_EXHAUSTED; }
        if (items_state.last_entry_id >= ITEMS_ID_RESERVED - 1U) {
            return ITEMS_RESULT_ID_EXHAUSTED;
        }
        split_id = items_state.last_entry_id + 1U;
    }
    item_entry_ref_t result_ref = source_ref;
    uint32_t source_container_id = items_state.containers[source->parent_index].container_id;
    uint32_t source_entry_id = source->entry_id;
    char def_id[ITEMS_STATE_STRING_MAX];
    (void)snprintf(def_id, sizeof(def_id), "%s", source->def_id);
    if (!audit_move_available(def_id, reason)) { return ITEMS_RESULT_AUDIT_UNAVAILABLE; }

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
        items_state.last_entry_id = split_id;
        ItemsItemEntry *split = &items_state.containers_entries[split_index];
        *split = *source;
        split->parent_index = (int)destination_ref.index;
        split->entry_id = split_id;
        split->slot = target_slot;
        split->count = count;
        source->count -= count;
        s_entry_generation[split_index] = next_generation(s_entry_generation[split_index]);
        result_ref = entry_ref(split_index);
    }

    bool ok = build_indices(&items_state, true, false, NULL, 0);
    NT_ASSERT(ok);
    game_save_mark_dirty();
    items_emit_move(def_id, (int64_t)source_entry_id,
                    (int64_t)source_container_id, (int64_t)destination->container_id,
                    count, count, reason);
    if (out_destination != NULL) { *out_destination = result_ref; }
    return ITEMS_RESULT_OK;
}

uint32_t items_entry_id(item_entry_ref_t ref) {
    NT_ASSERT(!ephemeral_ref(ref.index) && "ephemeral entries have no persistent id");
    if (ephemeral_ref(ref.index)) { return ITEMS_ID_NONE; }
    return require_entry(ref)->entry_id;
}

items_container_ref_t items_entry_container(item_entry_ref_t ref) {
    if (ephemeral_ref(ref.index)) {
        ItemsItemEntry *entry = require_ephemeral_entry(ref);
        return ephemeral_container_ref((uint32_t)entry->parent_index);
    }
    ItemsItemEntry *entry = require_entry(ref);
    return container_ref((uint32_t)entry->parent_index);
}

items_entry_view_t items_entry_view(item_entry_ref_t ref) {
    if (ephemeral_ref(ref.index)) {
        ItemsItemEntry *entry = require_ephemeral_entry(ref);
        return (items_entry_view_t){
            .entry_id = ITEMS_ID_NONE,
            .slot = entry->slot,
            .def_id = entry->def_id,
            .count = entry->count,
            .level = entry->level,
            .durability = entry->durability,
            .quarantined = false,
            .lifetime = ITEMS_LIFETIME_EPHEMERAL,
        };
    }
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

static bool inspection_budget_valid(
    items_inspection_budget_t budget, uint32_t row_capacity,
    items_inspection_result_t *out_error) {
    if (budget.max_rows == 0 || budget.max_rows > ITEMS_INSPECTION_MAX_ROWS ||
        budget.max_rows > row_capacity) {
        *out_error = ITEMS_INSPECTION_ROW_LIMIT;
        return false;
    }
    if (budget.max_bytes == 0 || budget.max_bytes > ITEMS_INSPECTION_MAX_BYTES) {
        *out_error = ITEMS_INSPECTION_BYTE_LIMIT;
        return false;
    }
    if (budget.max_context_rows == 0 ||
        budget.max_context_rows > ITEMS_INSPECTION_MAX_CONTEXT_ROWS) {
        *out_error = ITEMS_INSPECTION_CONTEXT_LIMIT;
        return false;
    }
    return true;
}

static void inspect_container_counts(
    uint32_t container_index, items_lifetime_t lifetime,
    uint32_t *out_entries, uint32_t *out_quarantined) {
    uint32_t entries = 0;
    uint32_t quarantined = 0;
    if (lifetime == ITEMS_LIFETIME_EPHEMERAL) {
        for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_ENTRIES; i++) {
            const ItemsItemEntry *entry = &s_ephemeral_entries[i];
            if (!entry->used || entry->parent_index != (int)container_index) { continue; }
            entries++;
            if (entry->quarantined) { quarantined++; }
        }
    } else {
        for (uint32_t i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
            const ItemsItemEntry *entry = &items_state.containers_entries[i];
            if (!entry->used || entry->parent_index != (int)container_index) { continue; }
            entries++;
            if (entry->quarantined) { quarantined++; }
        }
    }
    *out_entries = entries;
    *out_quarantined = quarantined;
}

static bool inspection_container_matches(
    const items_container_list_query_t *query,
    items_container_policy_t policy, items_lifetime_t lifetime,
    uint32_t entry_count) {
    return (query->policy == ITEMS_INSPECTION_FILTER_ANY || query->policy == (int)policy) &&
        (query->lifetime == ITEMS_INSPECTION_FILTER_ANY || query->lifetime == (int)lifetime) &&
        (query->include_empty || entry_count > 0);
}

static items_inspection_result_t inspect_container_candidate(
    const items_container_list_query_t *query,
    items_container_ref_t ref, const ItemsItemContainer *container,
    items_lifetime_t lifetime, uint32_t *matched,
    items_container_inspection_t *rows, items_inspection_page_t *page) {
    if (page->context_rows >= query->budget.max_context_rows) {
        return ITEMS_INSPECTION_CONTEXT_LIMIT;
    }
    page->context_rows++;
    uint32_t entries = 0;
    uint32_t quarantined = 0;
    uint32_t index = lifetime == ITEMS_LIFETIME_EPHEMERAL
        ? ephemeral_index(ref.index) : ref.index;
    inspect_container_counts(index, lifetime, &entries, &quarantined);
    if (!inspection_container_matches(
            query, (items_container_policy_t)container->policy, lifetime, entries)) {
        return ITEMS_INSPECTION_OK;
    }
    if (*matched < query->offset) {
        (*matched)++;
        return ITEMS_INSPECTION_OK;
    }
    if (page->count >= query->budget.max_rows) {
        page->has_more = true;
        return ITEMS_INSPECTION_OK;
    }
    uint32_t bytes = (uint32_t)sizeof(items_container_inspection_t);
    if (bytes > query->budget.max_bytes - page->projected_bytes) {
        if (page->count == 0) { return ITEMS_INSPECTION_BYTE_LIMIT; }
        page->has_more = true;
        return ITEMS_INSPECTION_OK;
    }
    rows[page->count++] = (items_container_inspection_t){
        .ref = ref,
        .container_id = lifetime == ITEMS_LIFETIME_PERSISTENT
            ? container->container_id : ITEMS_ID_NONE,
        .capacity = container->capacity,
        .entry_count = entries,
        .quarantined_count = quarantined,
        .policy = (items_container_policy_t)container->policy,
        .lifetime = lifetime,
    };
    page->projected_bytes += bytes;
    (*matched)++;
    return ITEMS_INSPECTION_OK;
}

items_inspection_result_t items_inspect_container_list(
    const items_container_list_query_t *query,
    items_container_inspection_t *rows,
    uint32_t row_capacity,
    items_inspection_page_t *out_page) {
    if (out_page != NULL) { memset(out_page, 0, sizeof(*out_page)); }
    if (query == NULL || rows == NULL || out_page == NULL ||
        (query->policy != ITEMS_INSPECTION_FILTER_ANY &&
         (query->policy < ITEMS_CONTAINER_POLICY_GENERIC ||
          query->policy > ITEMS_CONTAINER_POLICY_EQUIPMENT)) ||
        (query->lifetime != ITEMS_INSPECTION_FILTER_ANY &&
         query->lifetime != ITEMS_LIFETIME_PERSISTENT &&
         query->lifetime != ITEMS_LIFETIME_EPHEMERAL)) {
        return ITEMS_INSPECTION_BAD_QUERY;
    }
    items_inspection_result_t budget_error = ITEMS_INSPECTION_OK;
    if (!inspection_budget_valid(query->budget, row_capacity, &budget_error)) {
        return budget_error;
    }
    ensure_ready();

    items_inspection_page_t page = {0};
    uint32_t matched = 0;
    if (query->lifetime == ITEMS_INSPECTION_FILTER_ANY ||
        query->lifetime == ITEMS_LIFETIME_PERSISTENT) {
        for (uint32_t i = 0; i < s_container_index_count; i++) {
            uint32_t index = s_container_index[i].index;
            items_inspection_result_t result = inspect_container_candidate(
                query, container_ref(index), &items_state.containers[index],
                ITEMS_LIFETIME_PERSISTENT, &matched, rows, &page);
            if (result != ITEMS_INSPECTION_OK) { return result; }
            if (page.has_more) {
                page.next_offset = query->offset + page.count;
                *out_page = page;
                return ITEMS_INSPECTION_OK;
            }
        }
    }
    if (query->lifetime == ITEMS_INSPECTION_FILTER_ANY ||
        query->lifetime == ITEMS_LIFETIME_EPHEMERAL) {
        for (uint32_t i = 0; i < ITEMS_EPHEMERAL_MAX_CONTAINERS; i++) {
            if (!s_ephemeral_containers[i].used) { continue; }
            items_inspection_result_t result = inspect_container_candidate(
                query, ephemeral_container_ref(i), &s_ephemeral_containers[i],
                ITEMS_LIFETIME_EPHEMERAL, &matched, rows, &page);
            if (result != ITEMS_INSPECTION_OK) { return result; }
            if (page.has_more) {
                page.next_offset = query->offset + page.count;
                *out_page = page;
                return ITEMS_INSPECTION_OK;
            }
        }
    }
    page.next_offset = query->offset + page.count;
    *out_page = page;
    return ITEMS_INSPECTION_OK;
}

static bool inspect_container_ref(
    items_container_ref_t ref, const ItemsItemContainer **out_container,
    uint32_t *out_index, items_lifetime_t *out_lifetime) {
    ensure_ready();
    if (ephemeral_ref(ref.index)) {
        uint32_t index = ephemeral_index(ref.index);
        if (index >= ITEMS_EPHEMERAL_MAX_CONTAINERS || ref.generation == 0 ||
            s_ephemeral_container_generation[index] != ref.generation ||
            !s_ephemeral_containers[index].used) {
            return false;
        }
        *out_container = &s_ephemeral_containers[index];
        *out_index = index;
        *out_lifetime = ITEMS_LIFETIME_EPHEMERAL;
        return true;
    }
    if (ref.index >= ITEMS_STATE_MAX_CONTAINERS || ref.generation == 0 ||
        s_container_generation[ref.index] != ref.generation ||
        !items_state.containers[ref.index].used) {
        return false;
    }
    *out_container = &items_state.containers[ref.index];
    *out_index = ref.index;
    *out_lifetime = ITEMS_LIFETIME_PERSISTENT;
    return true;
}

static bool entry_inspection_matches(
    const items_entry_list_query_t *query, const ItemsItemEntry *entry) {
    return (query->def_id == NULL || strcmp(query->def_id, entry->def_id) == 0) &&
        (query->quarantined == ITEMS_INSPECTION_FILTER_ANY ||
         query->quarantined == (entry->quarantined ? 1 : 0));
}

items_inspection_result_t items_inspect_container_entries(
    items_container_ref_t container_ref_value,
    const items_entry_list_query_t *query,
    items_entry_inspection_t *rows,
    uint32_t row_capacity,
    items_inspection_page_t *out_page) {
    if (out_page != NULL) { memset(out_page, 0, sizeof(*out_page)); }
    if (query == NULL || rows == NULL || out_page == NULL ||
        query->slot_end <= query->slot_begin ||
        (query->quarantined != ITEMS_INSPECTION_FILTER_ANY &&
         query->quarantined != 0 && query->quarantined != 1) ||
        (query->def_id != NULL &&
         (query->def_id[0] == '\0' || strlen(query->def_id) >= ITEMS_STATE_STRING_MAX))) {
        return ITEMS_INSPECTION_BAD_QUERY;
    }
    items_inspection_result_t budget_error = ITEMS_INSPECTION_OK;
    if (!inspection_budget_valid(query->budget, row_capacity, &budget_error)) {
        return budget_error;
    }
    const ItemsItemContainer *container = NULL;
    uint32_t container_index = 0;
    items_lifetime_t lifetime = ITEMS_LIFETIME_PERSISTENT;
    if (!inspect_container_ref(
            container_ref_value, &container, &container_index, &lifetime)) {
        return ITEMS_INSPECTION_NOT_FOUND;
    }
    if (query->slot_end > container->capacity) { return ITEMS_INSPECTION_BAD_QUERY; }

    items_inspection_page_t page = {0};
    uint32_t matched = 0;
    for (uint32_t slot = query->slot_begin; slot < query->slot_end; slot++) {
        if (page.context_rows >= query->budget.max_context_rows) {
            return ITEMS_INSPECTION_CONTEXT_LIMIT;
        }
        page.context_rows++;
        ItemsItemEntry *entry = lifetime == ITEMS_LIFETIME_EPHEMERAL
            ? ephemeral_entry_at_slot(container_index, slot)
            : entry_at_slot(container_index, slot);
        if (entry == NULL || !entry_inspection_matches(query, entry)) { continue; }
        if (matched < query->offset) {
            matched++;
            continue;
        }
        if (page.count >= query->budget.max_rows) {
            page.has_more = true;
            break;
        }
        size_t byte_size = sizeof(items_entry_inspection_t) + strlen(entry->def_id) + 1U;
        if (byte_size > query->budget.max_bytes - page.projected_bytes) {
            if (page.count == 0) { return ITEMS_INSPECTION_BYTE_LIMIT; }
            page.has_more = true;
            break;
        }
        uint32_t entry_index = lifetime == ITEMS_LIFETIME_EPHEMERAL
            ? (uint32_t)(entry - s_ephemeral_entries)
            : (uint32_t)(entry - items_state.containers_entries);
        item_entry_ref_t ref = lifetime == ITEMS_LIFETIME_EPHEMERAL
            ? ephemeral_entry_ref(entry_index) : entry_ref(entry_index);
        rows[page.count++] = (items_entry_inspection_t){
            .ref = ref,
            .view = {
                .entry_id = lifetime == ITEMS_LIFETIME_PERSISTENT
                    ? entry->entry_id : ITEMS_ID_NONE,
                .slot = entry->slot,
                .def_id = entry->def_id,
                .count = entry->count,
                .level = entry->level,
                .durability = entry->durability,
                .quarantined = entry->quarantined,
                .lifetime = lifetime,
            },
        };
        page.projected_bytes += (uint32_t)byte_size;
        matched++;
    }
    page.next_offset = query->offset + page.count;
    *out_page = page;
    return ITEMS_INSPECTION_OK;
}
