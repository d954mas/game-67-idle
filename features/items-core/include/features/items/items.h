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

/* ---- Typed game catalog API (proof opt-in until the T0386 cutover) ---- */
#if defined(ITEMS_GAME_API_ENABLED) && ITEMS_GAME_API_ENABLED
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

/* The build selects exactly one game-generated capability header. Consumers
   continue to include only features/items/items.h. */
#include "items_game.gen.h"
#endif

/* ---- Каталог (const-таблица из content/items.json; компилируется ВСЕГДА) ---- */
typedef enum item_accept_policy_t {
    ITEM_ACCEPT_ANY = 0,
    ITEM_ACCEPT_CURRENCY_ONLY,
    ITEM_ACCEPT_SLOT_FILTER,
    ITEM_ACCEPT_CAPACITY_1,
} item_accept_policy_t;

typedef struct item_use_block_t {
    const char *effect_id; /* плоские use-params — по мере нужды (эпоха эффектов) */
} item_use_block_t;

typedef struct item_equip_block_t {
    const char *slot;
} item_equip_block_t;

typedef struct item_currency_block_t {
    const char *hud_hint;
    int64_t cap; /* 0 = безлимит */
} item_currency_block_t;

typedef struct game_item_def_t {
    const char *id;            /* <namespace>.<slug>; НИКОГДА не ключуемся по display_name */
    const char *display_name;  /* Display only; never a behavior key. */
    const char *icon_asset_id; /* логический id; хендл арта даёт игра (И3) */
    const char *kind;          /* витрина-категория (item_kinds) */
    const char *const *tags;
    int tag_count;
    int64_t base_value;
    /* derived from content items.json "stack" int: max_stack 0=unlimited, 1=unique, N=cap */
    bool stackable;
    int64_t max_stack;
    bool unlimited;
    const item_equip_block_t *equip;       /* NULL если нет блока */
    const item_use_block_t *use;           /* NULL если нет блока */
    const item_currency_block_t *currency; /* NULL если нет блока */
} game_item_def_t;

typedef struct game_container_def_t {
    const char *id;
    int64_t capacity; /* 0 = безлимит */
    item_accept_policy_t accept_policy;
    bool hidden;
} game_container_def_t;

const game_item_def_t *item_core(const char *def_id); /* NULL если неизвестен */
const game_item_def_t *item_at(int index);
int items_def_count(void);
int items_with_tag(const char *tag, const game_item_def_t **out, int out_cap);
bool item_is_currency(const game_item_def_t *def);
const game_container_def_t *item_container_def(const char *container_id);

/* ---- Владение (поверх генерируемого фрагмента items_state) ----
   Единый глагол add/remove: потратить золото / съесть зелье / израсходовать
   3 дерева / потратить опыт — ОДИН код. reason обязателен:
   формат verb:subject, verb из закрытого списка reason_tags.h (debug-assert,
   И2b). L1-нота: для СТАКОВ per-copy поля level/durability — игнорируемые
   дефолты (плоская форма под генератор); смысловы только для УНИКОВ
   (equip-блок). Тела реализованы в items_containers.c (И2b); объявлены здесь
   как единственная публичная поверхность фичи. */
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
