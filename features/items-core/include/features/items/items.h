#ifndef FEATURES_ITEMS_H
#define FEATURES_ITEMS_H
// feature-layer: L1

#include <stdbool.h>
#include <stdint.h>

/* ---- Typed catalog API (generated-C proof or runtime package) ---- */
#if (defined(ITEMS_GAME_API_ENABLED) && ITEMS_GAME_API_ENABLED) || \
    (defined(ITEMS_RUNTIME_PACKAGE_ENABLED) && ITEMS_RUNTIME_PACKAGE_ENABLED)
typedef struct item_id_t { uint64_t value; } item_id_t;
typedef struct item_def_ref_t { uint32_t _index; } item_def_ref_t;
typedef struct item_cost_ref_t { uint32_t _opaque; } item_cost_ref_t;

typedef enum item_transition_kind_t {
    ITEM_TRANSITION_UNAVAILABLE = 0,
    ITEM_TRANSITION_FREE,
    ITEM_TRANSITION_COST,
} item_transition_kind_t;

typedef struct item_transition_t {
    item_transition_kind_t kind;
    item_cost_ref_t cost;
} item_transition_t;

typedef struct item_cost_entry_t {
    item_id_t item;
    int64_t count;
} item_cost_entry_t;

typedef struct item_core_t {
    item_id_t id;
    int64_t stack;
} item_core_t;

item_def_ref_t items_get(item_id_t id);
bool items_exists(item_id_t id);
bool items_try_get(item_id_t id, item_def_ref_t *out);
bool items_try_get_string(const char *def_id, item_def_ref_t *out);
item_core_t items_core(item_def_ref_t ref);
const char *items_def_id(item_def_ref_t ref);
item_transition_t items_acquire_transition(item_def_ref_t ref);
uint32_t items_level_count(item_def_ref_t ref);
bool items_level_exists(item_def_ref_t ref, uint32_t level);
item_transition_t items_level_transition(item_def_ref_t ref, uint32_t level);
uint32_t items_cost_count(item_cost_ref_t cost);
item_cost_entry_t items_cost_at(item_cost_ref_t cost, uint32_t index);
void items_register_debug_labels(void);
#endif

#if defined(ITEMS_RUNTIME_PACKAGE_ENABLED) && ITEMS_RUNTIME_PACKAGE_ENABLED
#include "items_catalog_abi.gen.h"

#ifndef ITEMS_RUNTIME_PACKAGE_MAX_BYTES
#define ITEMS_RUNTIME_PACKAGE_MAX_BYTES (UINT32_C(64) * UINT32_C(1024) * UINT32_C(1024))
#endif

typedef enum items_catalog_bind_error_t {
    ITEMS_CATALOG_BIND_OK = 0,
    ITEMS_CATALOG_BIND_BAD_HEADER,
    ITEMS_CATALOG_BIND_BAD_MAGIC,
    ITEMS_CATALOG_BIND_BAD_VERSION,
    ITEMS_CATALOG_BIND_ABI_MISMATCH,
    ITEMS_CATALOG_BIND_CONTENT_MISMATCH,
    ITEMS_CATALOG_BIND_BAD_LAYOUT,
    ITEMS_CATALOG_BIND_NO_MEMORY,
    ITEMS_CATALOG_BIND_ALREADY_BOUND,
    ITEMS_CATALOG_BIND_RESOURCE_MISSING,
    ITEMS_CATALOG_BIND_RESOURCE_NOT_READY,
    ITEMS_CATALOG_BIND_RESOURCE_WRONG_TYPE,
} items_catalog_bind_error_t;

bool items_catalog_try_bind(
    const uint8_t *bytes, uint32_t byte_count,
    items_catalog_bind_error_t *out_error);
bool items_catalog_try_bind_resource(
    uint64_t asset_id, items_catalog_bind_error_t *out_error);
void items_catalog_shutdown(void);
bool items_catalog_is_bound(void);
uint32_t items_catalog_item_count(void);
uint64_t items_catalog_schema_abi(void);
uint64_t items_catalog_content_fingerprint(void);
bool items_has_currency(item_def_ref_t ref);
int64_t items_currency_cap(item_def_ref_t ref);
#endif

#if defined(ITEMS_GAME_API_ENABLED) && ITEMS_GAME_API_ENABLED
#include "items_game.gen.h"
#endif

/* ---- Runtime containers and owned entries ---- */

#define ITEMS_ID_NONE UINT32_C(0)
#define ITEMS_ID_RESERVED UINT32_MAX
#define ITEMS_SLOT_AUTO UINT32_MAX
#define ITEMS_PAYMENT_SCOPE_MAX UINT32_C(8)

typedef enum items_container_policy_t {
    ITEMS_CONTAINER_POLICY_GENERIC = 0,
    ITEMS_CONTAINER_POLICY_CURRENCY_ONLY,
    ITEMS_CONTAINER_POLICY_EQUIPMENT,
} items_container_policy_t;

typedef enum items_lifetime_t {
    ITEMS_LIFETIME_PERSISTENT = 0,
    ITEMS_LIFETIME_EPHEMERAL,
} items_lifetime_t;

typedef enum items_result_t {
    ITEMS_RESULT_OK = 0,
    ITEMS_RESULT_NOT_FOUND,
    ITEMS_RESULT_CAPACITY,
    ITEMS_RESULT_SLOT_OCCUPIED,
    ITEMS_RESULT_POLICY,
    ITEMS_RESULT_WRONG_STORAGE,
    ITEMS_RESULT_INSUFFICIENT,
    ITEMS_RESULT_POOL_EXHAUSTED,
    ITEMS_RESULT_ID_EXHAUSTED,
    ITEMS_RESULT_NOT_EMPTY,
    ITEMS_RESULT_LIFETIME,
    ITEMS_RESULT_STALE_LEVEL,
    ITEMS_RESULT_COMMIT_FAILED,
} items_result_t;

typedef struct items_container_ref_t { uint32_t index; uint32_t generation; } items_container_ref_t;
typedef struct item_entry_ref_t { uint32_t index; uint32_t generation; } item_entry_ref_t;

#define ITEMS_CONTAINER_REF_NONE ((items_container_ref_t){0, 0})
#define ITEM_ENTRY_REF_NONE ((item_entry_ref_t){0, 0})

typedef struct items_container_desc_t {
    uint32_t capacity;
    items_container_policy_t policy;
    items_lifetime_t lifetime;
} items_container_desc_t;

typedef struct items_payment_scope_t {
    uint32_t count;
    items_container_ref_t containers[ITEMS_PAYMENT_SCOPE_MAX];
} items_payment_scope_t;

typedef struct items_entry_view_t {
    uint32_t entry_id;
    uint32_t slot;
    const char *def_id;
    int64_t count;
    int level;
    float durability;
    bool quarantined;
    items_lifetime_t lifetime;
} items_entry_view_t;

#define ITEMS_INSPECTION_FILTER_ANY (-1)
#define ITEMS_INSPECTION_MAX_ROWS UINT32_C(64)
#define ITEMS_INSPECTION_MAX_BYTES (UINT32_C(32) * UINT32_C(1024))
#define ITEMS_INSPECTION_MAX_CONTEXT_ROWS UINT32_C(2048)

typedef enum items_inspection_result_t {
    ITEMS_INSPECTION_OK = 0,
    ITEMS_INSPECTION_BAD_QUERY,
    ITEMS_INSPECTION_NOT_FOUND,
    ITEMS_INSPECTION_ROW_LIMIT,
    ITEMS_INSPECTION_BYTE_LIMIT,
    ITEMS_INSPECTION_CONTEXT_LIMIT,
} items_inspection_result_t;

typedef struct items_inspection_budget_t {
    uint32_t max_rows;
    uint32_t max_bytes;
    uint32_t max_context_rows;
} items_inspection_budget_t;

typedef struct items_inspection_page_t {
    uint32_t count;
    uint32_t next_offset;
    uint32_t projected_bytes;
    uint32_t context_rows;
    bool has_more;
} items_inspection_page_t;

typedef struct items_container_list_query_t {
    uint32_t offset;
    int policy;
    int lifetime;
    bool include_empty;
    items_inspection_budget_t budget;
} items_container_list_query_t;

typedef struct items_container_inspection_t {
    items_container_ref_t ref;
    uint32_t container_id;
    uint32_t capacity;
    uint32_t entry_count;
    uint32_t quarantined_count;
    items_container_policy_t policy;
    items_lifetime_t lifetime;
} items_container_inspection_t;

typedef struct items_entry_list_query_t {
    uint32_t offset;
    uint32_t slot_begin;
    uint32_t slot_end;
    const char *def_id;
    int quarantined;
    items_inspection_budget_t budget;
} items_entry_list_query_t;

typedef struct items_entry_inspection_t {
    item_entry_ref_t ref;
    items_entry_view_t view;
} items_entry_inspection_t;

bool items_runtime_rebuild(char *error, int error_cap);
struct ItemsState;
bool items_runtime_validate_state(const struct ItemsState *state, char *error, int error_cap);
void items_reconcile(void);

items_result_t items_try_container_create(items_container_desc_t desc, items_container_ref_t *out_container);
items_result_t items_try_container_destroy_empty(items_container_ref_t container);
items_result_t items_try_container_resize(items_container_ref_t container, uint32_t capacity);
bool items_container_try_from_id(uint32_t container_id, items_container_ref_t *out_container);
uint32_t items_container_id(items_container_ref_t container);
uint32_t items_container_capacity(items_container_ref_t container);
items_container_policy_t items_container_policy(items_container_ref_t container);
items_lifetime_t items_container_lifetime(items_container_ref_t container);

items_result_t items_try_stack_add(
    items_container_ref_t container, const char *def_id, int64_t count,
    uint32_t slot, const char *reason, item_entry_ref_t *out_entry, int64_t *out_applied);
items_result_t items_try_stack_remove(item_entry_ref_t entry, int64_t count, const char *reason);
items_result_t items_try_stack_remove_from_container(
    items_container_ref_t container, const char *def_id, int64_t count, const char *reason);
int64_t items_stack_count(items_container_ref_t container, const char *def_id);
bool items_can_afford(items_container_ref_t container, const char *def_id, int64_t count);
#if (defined(ITEMS_GAME_API_ENABLED) && ITEMS_GAME_API_ENABLED) || \
    (defined(ITEMS_RUNTIME_PACKAGE_ENABLED) && ITEMS_RUNTIME_PACKAGE_ENABLED)
items_result_t items_try_pay_cost(
    item_cost_ref_t cost, items_payment_scope_t scope, const char *reason);
items_result_t items_try_acquire(
    items_container_ref_t destination, item_def_ref_t item,
    items_payment_scope_t payment, const char *reason,
    item_entry_ref_t *out_entry);
items_result_t items_try_upgrade_instance(
    item_entry_ref_t entry, uint32_t target_level,
    items_payment_scope_t payment, const char *reason);
#endif

#if defined(ITEMS_RUNTIME_TESTING) && ITEMS_RUNTIME_TESTING
void items_test_fail_next_commit(void);
#endif

items_result_t items_try_unique_create(
    items_container_ref_t container, const char *def_id, uint32_t slot,
    const char *reason, item_entry_ref_t *out_entry);
items_result_t items_try_entry_destroy(item_entry_ref_t entry, const char *reason);
items_result_t items_try_entry_move(
    item_entry_ref_t entry, items_container_ref_t destination,
    int64_t count, uint32_t destination_slot, const char *reason,
    item_entry_ref_t *out_destination);

bool items_entry_try_from_id(uint32_t entry_id, item_entry_ref_t *out_entry);
uint32_t items_entry_id(item_entry_ref_t entry);
items_container_ref_t items_entry_container(item_entry_ref_t entry);
items_entry_view_t items_entry_view(item_entry_ref_t entry);

items_inspection_result_t items_inspect_container_list(
    const items_container_list_query_t *query,
    items_container_inspection_t *rows,
    uint32_t row_capacity,
    items_inspection_page_t *out_page);
items_inspection_result_t items_inspect_container_entries(
    items_container_ref_t container,
    const items_entry_list_query_t *query,
    items_entry_inspection_t *rows,
    uint32_t row_capacity,
    items_inspection_page_t *out_page);

#endif /* FEATURES_ITEMS_H */
