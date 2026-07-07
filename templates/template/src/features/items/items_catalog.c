#include "features/items/items.h"

#include "items_catalog.gen.h" /* k_items/k_items_count + k_containers/k_containers_count (codegen) */

#include <string.h>

const game_item_def_t *item_core(const char *def_id) {
    if (!def_id) {
        return NULL;
    }
    for (int i = 0; i < k_items_count; ++i) {
        if (strcmp(k_items[i].id, def_id) == 0) {
            return &k_items[i];
        }
    }
    return NULL;
}

const game_item_def_t *item_at(int index) {
    if (index < 0 || index >= k_items_count) {
        return NULL;
    }
    return &k_items[index];
}

int items_def_count(void) { return k_items_count; }

int items_with_tag(const char *tag, const game_item_def_t **out, int out_cap) {
    if (!tag || !out || out_cap <= 0) {
        return 0;
    }
    int found = 0;
    for (int i = 0; i < k_items_count && found < out_cap; ++i) {
        const game_item_def_t *def = &k_items[i];
        for (int t = 0; t < def->tag_count; ++t) {
            if (def->tags[t] && strcmp(def->tags[t], tag) == 0) {
                out[found++] = def;
                break;
            }
        }
    }
    return found;
}

bool item_is_currency(const game_item_def_t *def) { return def != NULL && def->currency != NULL; }

const game_container_def_t *item_container_def(const char *container_id) {
    if (!container_id) {
        return NULL;
    }
    for (int i = 0; i < k_containers_count; ++i) {
        if (strcmp(k_containers[i].id, container_id) == 0) {
            return &k_containers[i];
        }
    }
    return NULL;
}
