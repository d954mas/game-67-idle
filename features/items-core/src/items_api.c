#include "features/items/items.h"

#if defined(ITEMS_GAME_API_ENABLED) && ITEMS_GAME_API_ENABLED

#include "items_game.internal.gen.h"

#include "core/nt_assert.h"
#include "hash/nt_hash.h"

#include <stddef.h>
#include <string.h>

bool items_try_get(item_id_t id, item_def_ref_t *out) {
    if (out == NULL) {
        return false;
    }
    const uint32_t count = items_game_internal_item_count();
    for (uint32_t index = 0; index < count; ++index) {
        if (items_game_internal_item_id(index).value == id.value) {
            out->_index = index;
            return true;
        }
    }
    return false;
}

bool items_exists(item_id_t id) {
    item_def_ref_t ignored;
    return items_try_get(id, &ignored);
}

item_def_ref_t items_get(item_id_t id) {
    item_def_ref_t ref = {UINT32_MAX};
    const bool found = items_try_get(id, &ref);
    NT_ASSERT(found && "items_get: unknown item id; use items_exists/items_try_get for expected absence");
    return ref;
}

bool items_try_get_string(const char *def_id, item_def_ref_t *out) {
    if (def_id == NULL || out == NULL) {
        return false;
    }
    const item_id_t id = {nt_hash64_str(def_id).value};
    item_def_ref_t ref;
    if (!items_try_get(id, &ref)) {
        return false;
    }
    if (strcmp(items_game_internal_def_id(ref._index), def_id) != 0) {
        return false;
    }
    *out = ref;
    return true;
}

item_core_t items_core(item_def_ref_t ref) {
    NT_ASSERT(ref._index < items_game_internal_item_count() && "items_core: invalid item ref");
    return items_game_internal_core(ref._index);
}

const char *items_def_id(item_def_ref_t ref) {
    NT_ASSERT(ref._index < items_game_internal_item_count() && "items_def_id: invalid item ref");
    return items_game_internal_def_id(ref._index);
}

item_transition_t items_acquire_transition(item_def_ref_t ref) {
    NT_ASSERT(ref._index < items_game_internal_item_count() &&
              "items_acquire_transition: invalid item ref");
    return items_game_internal_acquire(ref._index);
}

uint32_t items_level_count(item_def_ref_t ref) {
    NT_ASSERT(ref._index < items_game_internal_item_count() && "items_level_count: invalid item ref");
    return items_game_internal_level_count(ref._index);
}

bool items_level_exists(item_def_ref_t ref, uint32_t level) {
    return ref._index < items_game_internal_item_count() && level > 0U &&
        level <= items_game_internal_level_count(ref._index);
}

item_transition_t items_level_transition(item_def_ref_t ref, uint32_t level) {
    NT_ASSERT(items_level_exists(ref, level) && "items_level_transition: invalid item or level");
    return items_game_internal_level_transition(ref._index, level);
}

uint32_t items_cost_count(item_cost_ref_t cost) {
    NT_ASSERT(cost._opaque > 0U && cost._opaque < items_game_internal_cost_span_count() &&
              "items_cost_count: invalid cost ref");
    return items_game_internal_cost_count(cost._opaque);
}

item_cost_entry_t items_cost_at(item_cost_ref_t cost, uint32_t index) {
    NT_ASSERT(cost._opaque > 0U && cost._opaque < items_game_internal_cost_span_count() &&
              "items_cost_at: invalid cost ref");
    NT_ASSERT(index < items_game_internal_cost_count(cost._opaque) &&
              "items_cost_at: index out of range");
    return items_game_internal_cost_at(cost._opaque, index);
}

void items_register_debug_labels(void) {
    const uint32_t count = items_game_internal_item_count();
    for (uint32_t index = 0; index < count; ++index) {
        const item_id_t id = items_game_internal_item_id(index);
        nt_hash_register_label64((nt_hash64_t){id.value}, items_game_internal_def_id(index));
    }
}

#endif
