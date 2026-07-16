#ifndef FEATURES_ITEMS_H
#define FEATURES_ITEMS_H
// feature-layer: L1
/* Единственный публичный хедер фичи items — вся публичная поверхность; остальное
   в папке static. L1 foundation: зависит только от L0-шелла (game_save-toolkit +
   gsj_ + движок), НЕ от других фич. Владение ведётся в int64 ВЕЗДЕ (валюты тоже
   int64, НЕ double; большие счётчики в JSON — строкой). Дробное
   производство копит аккумулятор в game glue, НЕ в count (Р1; паттерн задокументирован
   в game glue и описана в скилле nt-game-items). */
#include <stdbool.h>
#include <stdint.h>

/* ---- Typed catalog API (generated-C proof or runtime package) ---- */
#if (defined(ITEMS_GAME_API_ENABLED) && ITEMS_GAME_API_ENABLED) || \
    (defined(ITEMS_RUNTIME_PACKAGE_ENABLED) && ITEMS_RUNTIME_PACKAGE_ENABLED)
typedef struct item_id_t {
    uint64_t value;
} item_id_t;

typedef struct item_def_ref_t {
    uint32_t _index;
} item_def_ref_t;

/* Opaque index into generated immutable cost spans. */
typedef struct item_cost_ref_t {
    uint32_t _opaque;
} item_cost_ref_t;

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

item_def_ref_t items_get(item_id_t id); /* required: asserts when absent */
bool items_exists(item_id_t id);
bool items_try_get(item_id_t id, item_def_ref_t *out);
bool items_try_get_string(const char *def_id, item_def_ref_t *out);
item_core_t items_core(item_def_ref_t ref); /* copy; asserts on invalid ref */
item_transition_t items_acquire_transition(item_def_ref_t ref);
uint32_t items_cost_count(item_cost_ref_t cost);
item_cost_entry_t items_cost_at(item_cost_ref_t cost, uint32_t index); /* copy; asserts on invalid range */
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
/* Looks up a ready blob resource and binds an owned copy of its bytes. The
   caller owns pack placement/request timing; this function only consumes it. */
bool items_catalog_try_bind_resource(
    uint64_t asset_id, items_catalog_bind_error_t *out_error);
/* Startup/shutdown API: all bind/read/shutdown calls are main-thread-only.
   A host using another thread must serialize the complete catalog lifetime. */
void items_catalog_shutdown(void);
bool items_catalog_is_bound(void);
uint32_t items_catalog_item_count(void);
uint64_t items_catalog_schema_abi(void);
uint64_t items_catalog_content_fingerprint(void);
bool items_has_currency(item_def_ref_t ref);
int64_t items_currency_cap(item_def_ref_t ref); /* 0 = unlimited; asserts unless currency */
#endif

#if defined(ITEMS_GAME_API_ENABLED) && ITEMS_GAME_API_ENABLED
/* The build selects exactly one game-generated capability header. Consumers
   continue to include only features/items/items.h. */
#include "items_game.gen.h"
#endif

/* ---- Владение (поверх генерируемого фрагмента items_state) ----
   Единый глагол add/remove: потратить золото / съесть зелье / израсходовать
   3 дерева / потратить опыт — ОДИН код. reason обязателен:
   формат verb:subject, verb из закрытого списка reason_tags.h (debug-assert,
   И2b). L1-нота: для СТАКОВ per-copy поля level/durability — игнорируемые
   дефолты (плоская форма под генератор); смысловы только для УНИКОВ
   (equip-блок). Тела реализованы в items_containers.c (И2b); объявлены здесь
   как единственная публичная поверхность фичи. */
/* Stack APIs reject unique definitions; instance APIs reject stack definitions. */
bool items_add(const char *container_id, const char *def_id, int64_t count, const char *reason);
bool items_remove(const char *container_id, const char *def_id, int64_t count, const char *reason);
int64_t items_count(const char *container_id, const char *def_id);
bool items_can_afford(const char *container_id, const char *def_id, int64_t n);
bool items_move(const char *from, const char *to, const char *entry_key, int64_t count, const char *reason);
const char *items_instance_create(const char *container_id, const char *def_id, const char *reason); /* уник -> instance_id */
bool items_instance_destroy(const char *instance_id, const char *reason);
/* purse-удобства: валюты по умолчанию едут в purse (accept-policy). */
int64_t items_purse(const char *def_id);

#endif /* FEATURES_ITEMS_H */
