# BUILD-SPEC: типизированные события из схем, инкремент E2 — генератор (2026-07-06)

Имплементационная спецификация. Переводит принятый дизайн в файлы, формат схемы,
сигнатуры генерируемого кода, изменения `generate_state.py`, golden+compile+run-гейт,
CMake и критерии приёмки. НЕ меняет дизайн. При расхождении источник истины —
`templates/design/event_system_design_2026-07-06.md` (главный: §3 генератор, §5 правила,
§6 план, §7 Q3/Q4, §8), затем `features/game-state/references/state_system_design_2026-07-06.md`
(§7/§14 п.13: события = ОТДЕЛЬНЫЙ рендерер, E2 после A4), затем
`templates/design/build_spec_e1_2026-07-06.md` (контракт `game_events.h` — E2 генерит
ПОВЕРХ него, НЕ меняет). Исполнитель может писать код по этому документу, НЕ открывая
историю обсуждений.

---

## 0. Предпосылки и рамки (проверено по дереву)

- **A4 (генератор v2) УЖЕ в дереве.** `features/game-state/scripts/generate_state.py`
  переписан в v2 (коммит 82980750): `Ns`-параметризация неймспейса из `schema.fragment`
  (`generate_state.py:67-84`), `load_schema` v2 с нормализацией мап→список + валидациями
  (`:186-293`), per-renderer конвейер (`render_header`/`render_generic_source`/
  `render_devapi_source`/`render_schema_header`), golden+property+validation тесты
  (`generate_state_test.py`, 18 кейсов). `main()` (`:1568-1603`) пишет 4 файла:
  `<frag>_state.{h,c}`, `<frag>_state_schema.gen.h`, `<frag>_state_devapi.c`. Golden-эталон:
  `features/game-state/tests/golden/{game,mini}/`. **E2 добавляет ОТДЕЛЬНЫЙ рендерер
  событий в ЭТУ архитектуру** (state doc §14 п.13), не трогая state-рендереры.
- **E1 (транспорт) УЖЕ в дереве и ЗАМОРОЖЕН.** `templates/template/src/game_events.{c,h}`:
  сырой `game_event_emit(nt_hash64_t type, const void *payload, uint32_t size, size_t align)`
  копирует `size` байт из `payload` в ФИКСИРОВАННУЮ арену по выравниванию и дописывает
  конверт `game_event_t {seq,tick,type,payload,size}` (`game_events.h:38-44`, `.c:52-95`);
  `game_event_log(&count)`, `game_events_tick()`, двухфазный кадр, `game_event_register_type_name`
  (шов имён, `.c:145-147`). **E2 НЕ правит `game_events.{c,h}`** — генерит ПОВЕРХ.
- **Потолок выравнивания = `_Alignof(max_align_t)` = 8 на этом хосте** (`game_events.c:60`:
  `NT_ASSERT(align <= _Alignof(max_align_t))`). Все типы полей событий максимум 8-байтные
  (i64/f64/hash) → структ события НИКОГДА не получает align > 8 → сырой emit всегда
  принимает `_Alignof(<struct>)`. Генератор эмитит `_Static_assert(_Alignof(<Struct>) <=
  _Alignof(max_align_t), ...)` как компайл-страж (§E2.6).
- **`nt_hash64_str(const char *)` — РАНТАЙМ-функция** (`nt_hash.h:54`), НЕ constexpr;
  `nt_hash64_t = {uint64_t value}` (`nt_hash.h:26-28`). Компайл-тайм хеш-константы в C нет
  → тип события = лениво-кэшируемый аксессор `<frag>_ev_<evt>_type()` (§E2.5, Отступление 6).
  `nt_hash_register_label64` реально пишет метку только при движковом `NT_HASH_LABELS=1`
  (дефолт 0, `nt_hash.h:10-12`) — E2 генерит регистрацию, эффект отложен на E3 (build-config),
  корректность E2 от меток НЕ зависит.
- **Сборка:** game-таргет `-Werror` + `nt_set_warning_flags` (`-Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -Wundef`, `-Wno-unused-parameter`).
  События считают в `double` (f64) → `-Wdouble-promotion` не активен (нет float→double);
  арифметика оффсетов/размеров — `size_t`/`uint32_t` с ЯВНЫМИ кастами. `_Alignas`/`_Alignof`/
  `_Static_assert` — ключевые слова C11/C17 (движок C17), без хедеров; `max_align_t`/`offsetof`
  из `<stddef.h>`.
- **Один тред.** Ленивый кэш хеша (`static nt_hash64_t`) потокобезопасности не требует.
- **Ключевой факт совместимости:** текущий `load_schema` НЕ отвергает неизвестные
  верхнеуровневые ключи (строит фиксированный dict из известных, `:275-287`). Значит
  добавление `"events"` в `game_state.schema.json` СЕГОДНЯ игнорируется загрузчиком → ГОСУДАРСТВЕННЫЙ
  (state) выхлоп НЕ меняется. **E2 добавляет парсинг `events` + новые файлы; state-golden
  остаётся байт-в-байт** (события НЕ входят во встроенную нормализованную схему —
  Отступление 7). Смоук-бот, DevAPI-схема, `game_state_schema.gen.h` не затронуты.

### Что E2 делает (ровно)

1. `load_schema`: парсит+валидирует секцию `events` (линт имён, коллизии, словарь типов).
2. Два НОВЫХ рендерера: `render_events_header` → `<frag>_state_events.gen.h`,
   `render_events_source` → `<frag>_state_events.gen.c` (per-событие: структ payload'а +
   emit-хелпер + type_hash-аксессор + str/bytes-аксессоры + дескриптор {name,type,offset};
   пер-фрагмент: таблица дескрипторов + регистратор меток).
3. Один НОВЫЙ рукописный shared-хедер `templates/template/src/game_event_desc.h` (контракт
   дескриптора для generic-рендеринга E3 — аддитивная инфра, НЕ правка E1).
4. `main()` генератора: пишет два новых файла (всегда, даже при пустых events — стаб).
5. Секция `events` в `game_state.schema.json` (1 событие) и `mini_state.schema.json`
   (2 события: rich со string+f64+bytes+hash + scalar-only) + golden-эталоны.
6. CMake: два TU в custom-command OUTPUT + генерируемый events-source в game-таргет +
   ctest `test_game_events_typed` (компилит golden mini-events + `game_events.c`).
7. Расширение `generate_state_test.py`: golden (авто через новые суффиксы) + property +
   negative-валидации событий.

**НЕ входит (границы, §E2.13):** DevAPI-лента `game.events.tail` + generic-рендер по
дескрипторам (E3); `NT_HASH_LABELS` build-config (E3); аналитика + встроенный тип
`log {string}` (E4); события в items/progression схемах (T0327 — формат закладывается
сейчас); двухфазная react/record-проводка (E1, готова).

---

## E2.1 Файлы

**Изменяемые:**
- `features/game-state/scripts/generate_state.py` — `load_schema` (events-парсинг+валидации),
  два новых рендерера, `main()` (§E2.8).
- `features/game-state/scripts/generate_state_test.py` — OUTPUT_SUFFIXES + property +
  negative-тесты событий (§E2.10).
- `templates/template/state/game_state.schema.json` — добавить секцию `events` (§E2.2).
- `features/game-state/tests/mini_state.schema.json` — добавить секцию `events` (§E2.2).
- `templates/template/CMakeLists.txt` — custom-command OUTPUT + events-source в game-таргет +
  ctest (§E2.9).
- `templates/template/src/main.c` — ОДНА строка (опц.-рекоменд.): `game_ev_register()` после
  `nt_hash_init` под `#if FEATURE_GAME_STATE` (§E2.9).

**Новые (рукописные):**
- `templates/template/src/game_event_desc.h` — shared контракт дескриптора (§E2.4).
- `templates/template/tests/test_game_events_typed.c` — ctest (Unity) на golden mini-events (§E2.10).

**Новые (генерируемые, коммитятся как golden-эталон):**
- `features/game-state/tests/golden/game/{game_state_events.gen.h,game_state_events.gen.c}`.
- `features/game-state/tests/golden/mini/{mini_state_events.gen.h,mini_state_events.gen.c}`.

**Не трогать:** `game_events.{c,h}` (E1 транспорт — ЗАМОРОЖЕН), `game_features.{c,h}` (E1),
state-рендереры/state-golden (A4, не затрагиваются), `game_state.{h,c}`/`_schema.gen.h`/
`_devapi.c` выхлоп (события — отдельный файл-family), движок.

---

## E2.2 Формат схемы: секция `events`

Секция `events` — верхнеуровневый ключ v2-схемы (сосед `fields`/`types`/`enums`), рядом
со стейт-фрагментом (event doc §3). Карта: **имя события (БЕЗ префикса фрагмента) →
`{ "fields": { имя-поля → { "type": <t> } } }`**. Полное имя-на-проводе, которое хешируется
и регистрируется как метка, = `"<fragment>.<event>"` (генератор компонует префикс).

Событийные поля МИНИМАЛЬНЫ: только `type` (+ опц. `doc`). НЕТ `default`/`min`/`max`/
`max_length` — события транзиентны, не валидируются, не имеют дефолтов (эмиттер передаёт
значения напрямую); string-поля произвольной длины (страж — `GAME_EVENT_EMIT_MAX` на emit).
Это ОСОЗНАННО проще стейт-полей.

### `game_state.schema.json` — добавить (в конец, после `fields`)

```jsonc
"events": {
  "shape_changed": {
    "fields": {
      "from_index": { "type": "int" },
      "to_index":   { "type": "int" },
      "note":       { "type": "string" }
    }
  }
}
```

### `mini_state.schema.json` — добавить (после `fields`)

```jsonc
"events": {
  "cell_spawned": {
    "fields": {
      "total":  { "type": "i64" },
      "rate":   { "type": "float" },
      "kind":   { "type": "hash" },
      "urgent": { "type": "bool" },
      "label":  { "type": "string" },
      "blob":   { "type": "bytes" }
    }
  },
  "ticked": {
    "fields": {
      "count": { "type": "int" }
    }
  }
}
```

`mini` ПРИГВОЖДАЕТ: неймспейс (`MiniEvCellSpawned`/`mini_emit_cell_spawned`), оффсет-паковку
двух инлайн-полей (string `label` + bytes `blob`), все типы (i64/f64/hash/bool), и
scalar-only путь (`ticked`). `game` пригвождает `game`-неймспейс + один string.

### Словарь типов событийных полей (event doc §3 + §7 Q3/Q4)

Имена ВЫРОВНЕНЫ со стейт-схемой (`float`/`string`, не f32/str — чтобы агент не расщепил
словари), НО ширина `float` РАЗНАЯ (см. Отступление 1):

| Тип схемы | C-тип в структе | Дескриптор | emit-арг | Примечание |
|---|---|---|---|---|
| `bool`   | `bool`               | `GAME_EVENT_FT_BOOL`   | `bool`                  | |
| `int`    | `int32_t`            | `GAME_EVENT_FT_INT`    | `int32_t`               | явная ширина (Отступление 2) |
| `i64`    | `int64_t`            | `GAME_EVENT_FT_I64`    | `int64_t`               | |
| `float`  | **`double`**         | `GAME_EVENT_FT_FLOAT`  | `double`                | **f64** (§7 Q3); ≠ стейт `float`=C `float` |
| `string` | `uint32_t` (оффсет)  | `GAME_EVENT_FT_STRING` | `const char *`          | инлайн NUL-строка; читать ТОЛЬКО через аксессор |
| `hash`   | `nt_hash64_t`        | `GAME_EVENT_FT_HASH`   | `nt_hash64_t`           | event-only; enum-как-hash благословлён |
| `bytes`  | `uint32_t`+`uint32_t`| `GAME_EVENT_FT_BYTES`  | `const void *`,`uint32_t`| event-only; оффсет+len; страж §7 Q4 |

Событийных `enum`/`string?`/`list`/`map` НЕТ (идиома «эмить N событий, не массив»;
enum-как-hash покрывает перечисления). Неизвестный тип поля → `SystemExit` (ловит
`str`/`f64`-расщепление словаря — negative-тест §E2.10).

### Линт имён (event doc §5; в загрузчике §E2.8)

- Имя события: `[a-z][a-z0-9_]*` (нижний регистр, старт с буквы, БЕЗ точки/префикса).
  Точка/верхний регистр/старт-с-цифры → `SystemExit`.
- Коллизия c_ident между именами событий → `SystemExit`.
- Имя поля: `[a-z][a-z0-9_]*`; ЗАРЕЗЕРВИРОВАНЫ `type`/`seq`/`tick` (конверт/аксессор) →
  `SystemExit`; коллизия c_ident внутри события → `SystemExit`.
- Спека события/поля: только `fields`/`type` (+ опц. `doc`); лишний ключ → `SystemExit`.
- `fields` может быть пустым (сигнал без payload) — разрешено.
- **Прошедшее время `noun_verbed` — СОВЕТ (doc/скилл), НЕ хард-линт** (РЕШЕНО лидом 2026-07-07,
  §E2.14): собственные примеры дизайна (`items.txn`) не в прошедшем; механически детект
  ненадёжен. Хард-линт = charset+коллизии+резерв+тип; правило двух таксономий (имя=ЧТО,
  reason=ПОЧЕМУ) — дисциплина скилла.

---

## E2.3 Раскладка payload'а (позиционная независимость, event doc §2 страж 2)

payload события = ОДНА непрерывная аллокация `[структ | инлайн строки | инлайн байты]`:
- Скаляры (bool/int/i64/float/hash) — прямые поля структа.
- string-поле `<f>` — `uint32_t <f>` в структе = БАЙТ-ОФФСЕТ от начала payload'а до
  инлайн NUL-строки, скопированной ПОСЛЕ структа. Указателей НЕТ. Доступ:
  `(const char *)e + e-><f>` (генерённый аксессор).
- bytes-поле `<f>` — `uint32_t <f>` (оффсет) + `uint32_t <f>_len` (длина); байты инлайн
  после структа. Доступ: `(const uint8_t *)e + e-><f>`, длина `e-><f>_len`.

Все оффсеты payload-относительны → `event_retain` = слепой `memcpy` корректен (нет висячих
указателей). Инлайн-поля пакуются в порядке ОБЪЯВЛЕНИЯ в схеме, начиная с `sizeof(struct)`
(структ выровнен, char'ы align 1 → зазора нет). ИНВАРИАНТ: str/bytes-поля читать ТОЛЬКО
через аксессор, НИКОГДА `e-><f>` напрямую (это uint32-оффсет, не данные).

---

## E2.4 Shared-хедер `templates/template/src/game_event_desc.h` (полностью)

Контракт дескриптора для generic-рендеринга (event doc §3: «дескриптор {имя,тип,offset} →
DevAPI/лог/аналитика рендерят ЛЮБОЕ событие»). Рукописная аддитивная инфра — параллель E1,
НЕ правка `game_events.{c,h}`. Один тип на ВСЕ фрагменты (E3 включает его один раз;
пер-фрагментные `.gen.h` — тоже).

```c
#ifndef GAME_EVENT_DESC_H
#define GAME_EVENT_DESC_H

#include <stdint.h>

/* Shared descriptor contract for generic event rendering (event_system_design §3).
   The generator emits one descriptor per event; DevAPI/log/analytics (E3/E4) walk it to
   render ANY event with no per-feature code. Positional-independent: every offset is
   relative to the payload base. */

/* Staging cap for emit helpers that pack inline strings/bytes. Oversized payload:
   debug NT_ASSERT / release nt_log_warn + drop (emit helper returns NULL). The staging
   union lives on the STACK per emit (default 4 KiB); raise per-game WITH CARE -- web/wasm
   stacks are small (~64 KiB) and react cascades nest emits. */
#ifndef GAME_EVENT_EMIT_MAX
#define GAME_EVENT_EMIT_MAX 4096u
#endif

typedef enum {
    GAME_EVENT_FT_BOOL = 0,
    GAME_EVENT_FT_INT,     /* int32_t */
    GAME_EVENT_FT_I64,     /* int64_t */
    GAME_EVENT_FT_FLOAT,   /* double (f64, event_system_design §7 Q3) */
    GAME_EVENT_FT_STRING,  /* uint32 byte-offset -> inline NUL-terminated string */
    GAME_EVENT_FT_HASH,    /* nt_hash64_t; render via nt_hash64_label */
    GAME_EVENT_FT_BYTES    /* uint32 byte-offset + uint32 length; recorders ignore, DevAPI size+hex */
} game_event_field_type_t;

typedef struct {
    const char             *name;
    game_event_field_type_t type;
    uint32_t                offset;     /* offsetof within the payload struct */
    uint32_t                len_offset; /* BYTES: offsetof of the paired uint32 length; else 0 */
} game_event_field_t;

typedef struct {
    const char               *name;         /* "<fragment>.<event>" */
    uint32_t                  payload_size; /* sizeof the payload struct */
    const game_event_field_t *fields;
    int                       field_count;
} game_event_desc_t;

#endif /* GAME_EVENT_DESC_H */
```

---

## E2.5 Генерируемый хедер `<frag>_state_events.gen.h` (полностью, пример = mini)

Форма (исполнитель параметризует по `Ns` + список событий). str/bytes-аксессоры —
`static inline` в хедере (крошечные, чистые; `static inline` освобождён от
`-Wunused-function`). Тип-аксессор/emit/дескриптор — extern (определены в `.c`).

```c
#ifndef MINI_STATE_EVENTS_GEN_H
#define MINI_STATE_EVENTS_GEN_H

/* Generated by features/game-state/scripts/generate_state.py from features/game-state/tests/mini_state.schema.json. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hash/nt_hash.h"    /* nt_hash64_t */
#include "game_event_desc.h" /* game_event_desc_t + field-type enum */

/* ---- mini.cell_spawned ---- */
typedef struct MiniEvCellSpawned {
    int64_t     total;
    double      rate;     /* schema 'float' == C double (f64); NOTE: event float != state float(C float) */
    nt_hash64_t kind;
    bool        urgent;
    uint32_t    label;    /* byte offset -> inline NUL string (read via accessor) */
    uint32_t    blob;     /* byte offset -> inline bytes (read via accessor) */
    uint32_t    blob_len; /* length of the inline bytes */
} MiniEvCellSpawned;

nt_hash64_t mini_ev_cell_spawned_type(void); /* nt_hash64_str("mini.cell_spawned"), cached */

const void *mini_emit_cell_spawned(int64_t total, double rate, nt_hash64_t kind,
                                   bool urgent, const char *label,
                                   const void *blob, uint32_t blob_len);

static inline const char *mini_ev_cell_spawned_label(const MiniEvCellSpawned *e) {
    return (const char *)e + e->label;
}
static inline const void *mini_ev_cell_spawned_blob(const MiniEvCellSpawned *e) {
    return (const uint8_t *)e + e->blob;
}
static inline uint32_t mini_ev_cell_spawned_blob_len(const MiniEvCellSpawned *e) {
    return e->blob_len;
}

extern const game_event_desc_t mini_ev_cell_spawned_desc;

/* ---- mini.ticked ---- */
typedef struct MiniEvTicked {
    int32_t count;
} MiniEvTicked;

nt_hash64_t mini_ev_ticked_type(void);

const void *mini_emit_ticked(int32_t count);

extern const game_event_desc_t mini_ev_ticked_desc;

/* ---- fragment event table + label registration ---- */
extern const game_event_desc_t *const mini_ev_descs[];
extern const int mini_ev_desc_count;

void mini_ev_register(void); /* register debug labels; call once after nt_hash_init */

#endif /* MINI_STATE_EVENTS_GEN_H */
```

**Неймспейс (по `Ns`):** структ `<Ns.pascal>Ev<Pascal(evt)>`; emit `<Ns.id>_emit_<evt>`;
тип-аксессор/дескриптор/аксессоры/таблица/регистратор с префиксом `<Ns.id>_ev_`. Для `game`:
`GameEvShapeChanged`, `game_emit_shape_changed`, `game_ev_shape_changed_type`,
`game_ev_descs`, `game_ev_register`. **ВНИМАНИЕ (Отступление 3):** символьный префикс
`game_ev_`/`game_emit_` отличается от E1 `game_event_*` (транспорт) — 2 разных family;
файл-имя `game_state_events.gen.*` намеренно под state-family (тест-инфра + разведение с
`src/game_events.h`).

---

## E2.6 Генерируемый source `<frag>_state_events.gen.c` (полностью, пример = mini)

Тип-аксессор — extern с ленивым file-static кэшем (`nt_hash64_str` рантайм; ОДИН кэш на
событие; single-thread; НЕТ зависимости от порядка init). emit-хелпер: событие с инлайн-
данными → union-стейджинг (позиционно-независимая паковка, портируемо-корректно без
strict-aliasing проблем); scalar-only → прямой локальный структ. Оба идут через сырой
`game_event_emit`. `memset` структ-части → детерминированный паддинг (для memcmp-тестов).

```c
#include "mini_state_events.gen.h"

/* Generated by features/game-state/scripts/generate_state.py from features/game-state/tests/mini_state.schema.json. */

#include <stddef.h> /* offsetof, max_align_t */
#include <string.h> /* memcpy, memset, strlen */

#include "core/nt_assert.h"
#include "game_events.h" /* game_event_emit, game_event_register_type_name */
#include "log/nt_log.h"  /* nt_log_warn on staging overflow (release-visible) */

_Static_assert(_Alignof(MiniEvCellSpawned) <= _Alignof(max_align_t),
               "MiniEvCellSpawned over-aligned for game_event_emit");
_Static_assert(_Alignof(MiniEvTicked) <= _Alignof(max_align_t),
               "MiniEvTicked over-aligned for game_event_emit");

/* ---- mini.cell_spawned ---- */
nt_hash64_t mini_ev_cell_spawned_type(void) {
    static nt_hash64_t h;
    if (!h.value) { h = nt_hash64_str("mini.cell_spawned"); }
    return h;
}

const void *mini_emit_cell_spawned(int64_t total, double rate, nt_hash64_t kind,
                                   bool urgent, const char *label,
                                   const void *blob, uint32_t blob_len) {
    union {
        MiniEvCellSpawned ev;
        uint8_t bytes[GAME_EVENT_EMIT_MAX];
    } u;
    memset(&u, 0, sizeof(u.ev)); /* deterministic struct padding; strings written below */
    u.ev.total = total;
    u.ev.rate = rate;
    u.ev.kind = kind;
    u.ev.urgent = urgent;

    uint32_t off = (uint32_t)sizeof(u.ev);
    const char *label_s = label ? label : "";
    size_t label_n = strlen(label_s) + 1u; /* incl. NUL */
    if ((size_t)off + label_n + (size_t)blob_len > sizeof(u.bytes)) {
        NT_ASSERT(0 && "mini_emit_cell_spawned payload exceeds GAME_EVENT_EMIT_MAX");
        nt_log_warn("mini_emit_cell_spawned: payload exceeds GAME_EVENT_EMIT_MAX (%u B) -> dropped",
                    (unsigned)GAME_EVENT_EMIT_MAX);
        return NULL; /* release: warned drop (no dropped-counter -- E1's counter is private/frozen) */
    }
    u.ev.label = off;
    memcpy(u.bytes + off, label_s, label_n);
    off += (uint32_t)label_n;

    u.ev.blob = off;
    u.ev.blob_len = blob_len;
    if (blob_len != 0u && blob != NULL) { memcpy(u.bytes + off, blob, blob_len); }
    off += blob_len;

    return game_event_emit(mini_ev_cell_spawned_type(), &u, off, _Alignof(MiniEvCellSpawned));
}

static const game_event_field_t mini_ev_cell_spawned_fields[] = {
    { "total", GAME_EVENT_FT_I64, (uint32_t)offsetof(MiniEvCellSpawned, total), 0u },
    { "rate", GAME_EVENT_FT_FLOAT, (uint32_t)offsetof(MiniEvCellSpawned, rate), 0u },
    { "kind", GAME_EVENT_FT_HASH, (uint32_t)offsetof(MiniEvCellSpawned, kind), 0u },
    { "urgent", GAME_EVENT_FT_BOOL, (uint32_t)offsetof(MiniEvCellSpawned, urgent), 0u },
    { "label", GAME_EVENT_FT_STRING, (uint32_t)offsetof(MiniEvCellSpawned, label), 0u },
    { "blob", GAME_EVENT_FT_BYTES, (uint32_t)offsetof(MiniEvCellSpawned, blob),
      (uint32_t)offsetof(MiniEvCellSpawned, blob_len) },
};
const game_event_desc_t mini_ev_cell_spawned_desc = {
    "mini.cell_spawned",
    (uint32_t)sizeof(MiniEvCellSpawned),
    mini_ev_cell_spawned_fields,
    (int)(sizeof(mini_ev_cell_spawned_fields) / sizeof(mini_ev_cell_spawned_fields[0])),
};

/* ---- mini.ticked ---- */
nt_hash64_t mini_ev_ticked_type(void) {
    static nt_hash64_t h;
    if (!h.value) { h = nt_hash64_str("mini.ticked"); }
    return h;
}

const void *mini_emit_ticked(int32_t count) {
    MiniEvTicked ev;
    memset(&ev, 0, sizeof(ev));
    ev.count = count;
    return game_event_emit(mini_ev_ticked_type(), &ev, (uint32_t)sizeof(ev), _Alignof(MiniEvTicked));
}

static const game_event_field_t mini_ev_ticked_fields[] = {
    { "count", GAME_EVENT_FT_INT, (uint32_t)offsetof(MiniEvTicked, count), 0u },
};
const game_event_desc_t mini_ev_ticked_desc = {
    "mini.ticked",
    (uint32_t)sizeof(MiniEvTicked),
    mini_ev_ticked_fields,
    (int)(sizeof(mini_ev_ticked_fields) / sizeof(mini_ev_ticked_fields[0])),
};

/* ---- fragment event table ---- */
const game_event_desc_t *const mini_ev_descs[] = {
    &mini_ev_cell_spawned_desc,
    &mini_ev_ticked_desc,
};
const int mini_ev_desc_count = 2;

void mini_ev_register(void) {
    game_event_register_type_name(mini_ev_cell_spawned_type(), "mini.cell_spawned");
    game_event_register_type_name(mini_ev_ticked_type(), "mini.ticked");
}
```

**Пустой fragment (нет events):** оба файла эмитятся ВСЕГДА (детерминизм CMake OUTPUT).
Хедер — только декларации таблицы/счётчика/регистратора; source:
```c
const game_event_desc_t *const <frag>_ev_descs[1] = { NULL };
const int <frag>_ev_desc_count = 0;
void <frag>_ev_register(void) { }
```
(zero-length массив невалиден в C → 1-элем NULL-стаб; потребитель по `count==0` не дерефает).

---

## E2.7 Канон-идиом потребителя (типизированное чтение + доказательство retain)

Строит на E1 §E1.4 (сброс курсора по СМЕНЕ `game_events_tick()`, НЕ по числу событий;
указатели стабильны весь кадр). str/bytes — ТОЛЬКО через аксессор.

```c
/* Typed consumer (system or game_features_react). */
static int      s_pos;
static uint32_t s_last_tick;

uint32_t tick = game_events_tick();
if (tick != s_last_tick) { s_last_tick = tick; s_pos = 0; } /* new frame -> reset cursor */

int n;
const game_event_t *log = game_event_log(&n);
const nt_hash64_t want = mini_ev_cell_spawned_type();
for (; s_pos < n; ++s_pos) {
    const game_event_t *e = &log[s_pos];
    if (e->type.value != want.value) { continue; }
    const MiniEvCellSpawned *ev = (const MiniEvCellSpawned *)e->payload;
    int64_t     total = ev->total;
    double      rate  = ev->rate;
    const char *label = mini_ev_cell_spawned_label(ev); /* accessor, NEVER ev->label */
    const void *blob  = mini_ev_cell_spawned_blob(ev);
    uint32_t    blen  = mini_ev_cell_spawned_blob_len(ev);
    /* react ... (may cascade-emit; next react generation sees the grown log) */
}
```

Доказательство позиционной независимости (retain = слепой memcpy). ВНИМАНИЕ: буфер-приёмник
обязан быть ВЫРОВНЕН как payload (union / `_Alignas`) — сырой `uint8_t keep[]` имеет align 1,
каст его к структу с align 8 = UB (C11 6.3.2.3p7), невидимое на x86/wasm и для `-Wcast-align`.
Тот же выровненный приёмник наследует E3-ринг.
```c
union { MiniEvCellSpawned ev; uint8_t bytes[GAME_EVENT_EMIT_MAX]; } keep; /* aligned like payload */
memcpy(&keep, e->payload, e->size);                          /* copy event out of the arena */
const char *label_copy = mini_ev_cell_spawned_label(&keep.ev); /* (char*)&keep.ev + offset -> valid in COPY */
/* label_copy == оригинал: оффсет payload-относителен, переезд не ломает */
```

---

## E2.8 Изменения `generate_state.py`

**Верхнеуровневые константы (рядом с `SCALAR_TYPES`, `:46`):**
```python
EVENT_NAME_RE = re.compile(r"[a-z][a-z0-9_]*")
EVENT_FIELD_TYPES = {"bool", "int", "i64", "float", "string", "hash", "bytes"}
EVENT_RESERVED_FIELDS = {"type", "seq", "tick"}
```

**`load_schema` (после парсинга `fields`, `:273`; перед сборкой итогового dict `:275`):**
парсить `events`:
- `events_raw = raw.get("events", {})`; не dict → `SystemExit("events must be a map of name -> spec")`.
- Для каждого `evt_name -> evt_spec` (сохранить ПОРЯДОК):
  - `EVENT_NAME_RE.fullmatch(evt_name)` иначе `SystemExit` (charset; ловит точку/регистр).
  - `c_ident(evt_name)` коллизия с ранее виденным → `SystemExit`.
  - `evt_spec` dict; ключи ⊆ `{"fields","doc"}` иначе `SystemExit` (лишний ключ).
  - `fields_raw = evt_spec.get("fields", {})`; не dict → `SystemExit`. Пустой — ОК.
  - Нормализовать в список `[{"name":k, **spec}]`; для каждого поля:
    - `EVENT_NAME_RE.fullmatch(name)` иначе `SystemExit`.
    - `name in EVENT_RESERVED_FIELDS` → `SystemExit` (резерв конверта/аксессора).
    - `c_ident(name)` коллизия внутри события → `SystemExit`.
    - ключи спеки ⊆ `{"type","doc"}` иначе `SystemExit`.
    - `spec["type"] in EVENT_FIELD_TYPES` иначе `SystemExit(f"unknown event field type {type!r}")`.
  - ПОСЛЕ прохода полей (L3): для каждого bytes-поля `<name>` СИНТЕЗИРУЕМЫЙ член `<name>_len`
    НЕ должен коллидировать (по имени ИЛИ по `c_ident`) с объявленным полем события →
    `SystemExit(f"bytes field {name} synthesizes {name}_len which collides with a declared field")`
    (иначе дубль C-члена = грязная compile-ошибка вместо чистого SystemExit).
- `schema["events"] = events` (нормализованный dict `name -> {"fields":[...]}`).

state-рендереры и `normalized_schema_for_embed` НЕ читают `schema["events"]` → state-выхлоп
и встроенная схема БАЙТ-ИДЕНТИЧНЫ (Отступление 7).

**Новые хелперы (рядом с type/collection-хелперами):**
```python
def event_struct_c_name(evt): return f"{NS.pascal}Ev{_pascal(evt)}"      # MiniEvCellSpawned
def event_emit_fn(evt):       return f"{NS.id}_emit_{evt}"                # mini_emit_cell_spawned
def event_type_fn(evt):       return f"{NS.id}_ev_{evt}_type"            # mini_ev_cell_spawned_type
def event_desc_name(evt):     return f"{NS.id}_ev_{evt}_desc"
def event_full_name(evt):     return f"{NS.id}.{evt}"                    # "mini.cell_spawned"
def event_field_c_type(t):    # bool/int32_t/int64_t/double/nt_hash64_t/uint32_t
def event_field_ft_enum(t):   # GAME_EVENT_FT_BOOL/INT/I64/FLOAT/STRING/HASH/BYTES
def event_has_inline(fields): return any(f["type"] in ("string","bytes") for f in fields)
```
- Структ-поля: string → `uint32_t <name>;`; bytes → `uint32_t <name>;` + `uint32_t <name>_len;`;
  иначе `event_field_c_type(type) <name>;`. Порядок = порядок объявления. **L5:** на `float`-поле
  эмитить хвостовой коммент `/* schema 'float' == C double (f64); event float != state float */`
  (агент, читающий `.gen.h`, не сверяется со спекой).
- emit-арги (порядок объявления): scalar/hash по значению; string → `const char *<name>`;
  bytes → `const void *<name>, uint32_t <name>_len`.
- emit-тело: `event_has_inline` → union-стейджинг (§E2.6): memset структа, присвоить скаляры,
  для string/bytes в порядке объявления — оффсет+`memcpy`+продвинуть `off`, страж
  `GAME_EVENT_EMIT_MAX` (переполнение = `NT_ASSERT` debug + `nt_log_warn`+`return NULL` release,
  БЕЗ dropped-счётчика — E1-счётчик приватен/заморожен); иначе прямой локальный структ. Оба:
  `game_event_emit(<type_fn>(), ..., off/sizeof, _Alignof(<Struct>))`.
- str/bytes-аксессоры — `static inline` в хедере (§E2.5).
- Дескриптор-поля: `{ "<name>", <FT>, (uint32_t)offsetof(<Struct>, <name>), <len_offset> }`;
  для bytes `len_offset = (uint32_t)offsetof(<Struct>, <name>_len)`, иначе `0u`.

**`render_events_header(schema, schema_label)`** → `<frag>_state_events.gen.h` (форма §E2.5;
guard `<NS.macro>EVENTS_GEN_H`; инклюды `hash/nt_hash.h` + `game_event_desc.h`).

**`render_events_source(schema, schema_label)`** → `<frag>_state_events.gen.c` (форма §E2.6;
`_Static_assert` на каждый структ; инклюды `<stddef.h>`/`<string.h>`/`core/nt_assert.h`/
`game_events.h`/`log/nt_log.h` — последний для warn на переполнении стейджинга).

**`main()` (`:1584-1597`):** добавить два выхода:
```python
events_header_path = out_dir / f"{fragment}_state_events.gen.h"
events_source_path = out_dir / f"{fragment}_state_events.gen.c"
# write_if_changed(...) для обоих, в changed[]
```
(имена — под state-family, тест-инфра переиспользует prefix `<frag>_state`; символы —
feature-level `<frag>_ev_`/`<frag>_emit_`, Отступление 3.)

---

## E2.9 CMake (`templates/template/CMakeLists.txt`)

**Custom-command OUTPUT (`:140-157`, блок `FEATURE_GAME_STATE`):** добавить два выхода +
переменные:
```cmake
set(GAME_STATE_GENERATED_EVENTS_HEADER "${GAME_STATE_GENERATED_DIR}/game_state_events.gen.h")
set(GAME_STATE_GENERATED_EVENTS_SOURCE "${GAME_STATE_GENERATED_DIR}/game_state_events.gen.c")
# ...в add_custom_command OUTPUT добавить обе...
```

**Game-таргет (`:158-163`, блок `FEATURE_GAME_STATE`):** добавить генерируемый events-source:
```cmake
target_sources(${GAME_TARGET} PRIVATE
    "${GAME_STATE_GENERATED_SOURCE}"
    "${GAME_STATE_GENERATED_EVENTS_SOURCE}"   # E2: typed event structs/emit/descriptors
    src/game_state_json.c
    ...)
```
`src/` уже в include-path (`:179`) → `game_event_desc.h`/`game_events.h` резолвятся;
generated-dir уже в include-path (`:172`). `game_events.c` (E1) уже в game-таргете. Событийный
source зависит ТОЛЬКО от `game_events.h`+`game_event_desc.h`+`hash/nt_hash.h` (НЕ cJSON, НЕ
game_save) — линкуется дёшево.

**main.c (опц.-рекоменд., §E2.1):** после `nt_hash_init` (main.c:354), под
`#if FEATURE_GAME_STATE`, `#include "game_state_events.gen.h"` + `game_ev_register();` —
регистрирует debug-метки (эффект под `NT_HASH_LABELS`, E3). Опционально: тип-аксессоры
ленивы, корректность без него. Символы extern → без-warning если не звать; но проводка
закрывает шов для E3. Рекомендуется одна строка.

**Ctest `test_game_events_typed` (безусловно, к блоку `:264-303`, `if(NOT EMSCRIPTEN)`):**
компилит КОММИЧЕННЫЙ golden mini-events (rich-фикстура: i64/f64/hash/bool/string/bytes +
scalar-only) + E1 транспорт — без генерации на build-тайме, покрывает все типы, double-служит
компайл-чеком golden:
```cmake
    add_executable(test_game_events_typed
        tests/test_game_events_typed.c
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        src/game_events.c)
    target_link_libraries(test_game_events_typed PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events_typed PRIVATE
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_events_typed PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_typed PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_typed COMMAND test_game_events_typed)
```
Транзитивные (`nt_platform` для `nt_assert`) — по сообщению линкера. Тест безусловен: golden —
коммиченная фикстура, `game_event_desc.h`/`game_events.{c,h}` существуют независимо от
`FEATURE_GAME_STATE`.

**Warning-gated компайл-чек богатых веток (M3, ОБЯЗАТЕЛЬНО).** Дыра: богатые ветки типов
(i64/f64/hash/bool/bytes+len, union-стейджинг, оффсет-арифметика) живут ТОЛЬКО в `mini`, а
`test_game_events_typed` собирается БЕЗ `nt_set_warning_flags` → самый рисковый генерируемый код
не под `-Werror`/`-Wconversion` (Unity-макросы к тому же воюют с `-Wconversion`). Фикс —
ОТДЕЛЬНЫЙ OBJECT-таргет, компилящий golden `mini_state_events.gen.c` ПОД `nt_set_warning_flags`
БЕЗ Unity (только компиляция, без линка/рантайма — не воюем с тест-фреймворком):
```cmake
    add_library(check_mini_state_events OBJECT
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c")
    target_include_directories(check_mini_state_events PRIVATE
        src
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_link_libraries(check_mini_state_events PRIVATE nt_hash nt_log nt_core)  # заголовки-only (OBJECT не линкует)
    nt_set_warning_flags(check_mini_state_events)          # тот же набор -W, что у game-таргета
    # + тот же -Werror/-WX toggle, что применяет game-таргет (зеркалить его строку)
```
`game`-события уже под `-Werror` в game-таргете (компилит `game_state_events.gen.c`), но их
ветки — только int+string; `mini`-чек добавляет i64/f64/hash/bool/bytes/scalar-only под полный
warning-набор. `target_link_libraries` тянет только include-пути движковых заголовков (OBJECT-
таргет не линкует). Обоснование выбора (а) vs (б)/(в): (а) изолирует богатейший генерируемый код
под `-Werror` без макро-шума Unity и без churn'а продуктовой схемы; (б) рисковал бы Unity vs
`-Wconversion`; (в) добавил бы искусственное i64/bytes-поле в продуктовую `game`-схему.

---

## E2.10 Тесты

**(1) Golden (`generate_state_test.py`):** `OUTPUT_SUFFIXES += ("_events.gen.h",
"_events.gen.c")` (`:15`). `test_v2_template_golden`/`test_v2_namespace_golden` тогда
АВТО-покрывают новые файлы (`read_outputs` берёт `<prefix>_events.gen.{h,c}` через prefix
`<frag>_state`). Первичный захват golden — теми же командами (`--schema … --out-dir
features/game-state/tests/golden/{game,mini} --fragment {game,mini}`), РЕВЬЮ ГЛАЗАМИ ДО
коммита (процессное требование, как A4 L5). **АТОМАРНОСТЬ (L4):** расширение `OUTPUT_SUFFIXES`
МГНОВЕННО требует все 4 новых golden-файла (`{game,mini}_state_events.gen.{h,c}`), иначе оба
golden-теста красные. `render_events_*` + `main()`-запись двух файлов + смена `OUTPUT_SUFFIXES` +
захват+ревью golden = ОДИН атомарный шаг исполнителя (не коммитить промежуточные состояния).

**(2) Property (`generate_state_test.py`, расширить `test_property_game_output`):** ЧИТАТЬ
ИМЕННО event-ключи из `read_outputs` (НЕ `files["game_state.c"]`): `evh =
files["game_state_events.gen.h"]`, `evc = files["game_state_events.gen.c"]`. В `evh/evc` —
`IN "typedef struct GameEvShapeChanged"`, `IN "game_emit_shape_changed"`,
`IN "game_ev_shape_changed_type"`, `IN "const game_event_desc_t *const game_ev_descs[]"`,
`IN '#include "game_event_desc.h"'`, `IN "_Static_assert"`. Новый `test_property_mini_events`
(ключи `files["mini_state_events.gen.h"]`/`["mini_state_events.gen.c"]`): `IN "double rate;"`
(f64-доказательство ≠ стейт-float), `IN "uint32_t label;"` (string-оффсет),
`IN "GAME_EVENT_FT_BYTES"`, `IN "union {"` (стейджинг), `NOT IN "char label["` (строки НЕ
инлайн-массивы).

**(3) Negative (`generate_state_test.py`, `assertRaises(SystemExit)`):**
- имя события плохой charset (`"Foo"`, `"a.b"`, `"1x"`);
- коллизия c_ident имён событий;
- неизвестный тип поля (`"str"`, `"f64"`, `"enum"`) — ловит расщепление словаря;
- зарезервированное имя поля (`"type"`/`"seq"`/`"tick"`);
- коллизия c_ident полей внутри события;
- синтезируемый `<bytes>_len` коллизия (L3): событие с полями `blob:{type:bytes}` +
  `blob_len:{type:int}` → `SystemExit` (не грязный дубль C-члена);
- лишний ключ в спеке события/поля (`{"fields":{...},"lifetime":...}` / `{"type":"int","min":0}`).

**(4) Compile-гейт:** реген шаблона → сборка `templates/template` (native-debug + release +
devapi-debug) warning-clean под `-Werror`. Доказывает: `game_state_events.gen.c` компилится в
game-таргете во всех конфигах (tool parity — генерируемый событийный слой есть и в агентской
devapi-сборке).

**(5) Ctest `test_game_events_typed.c` (Unity):** `setUp`: `nt_hash_init(NULL)` (однажды) +
`game_events_init()`; `tearDown`: `game_events_shutdown()`. Фаза EMIT (дефолт после init).
1. **emit + typed read:** `mini_emit_cell_spawned(42, 3.5, nt_hash64_str("Epic"), true,
   "hello", (const uint8_t[]){1,2,3}, 3)`; `game_event_log(&n)` n==1; `e->type.value ==
   mini_ev_cell_spawned_type().value`; каст payload; `total==42`, `rate==3.5` (точно),
   `kind.value==nt_hash64_str("Epic").value`, `urgent==true`; `strcmp(mini_ev_cell_spawned_label
   (ev), "hello")==0`; `blob_len==3`, байты blob совпадают.
2. **retain / позиционная независимость:** приёмник — ВЫРОВНЕННЫЙ union `{MiniEvCellSpawned
   ev; uint8_t bytes[GAME_EVENT_EMIT_MAX];} keep;` (НЕ `uint8_t keep[]` — align 1, каст к
   структу = UB, M1); `memcpy(&keep, e->payload, e->size)`; читать
   `mini_ev_cell_spawned_label(&keep.ev)` из КОПИИ → "hello" (доказывает слепой memcpy).
3. **scalar-only:** `mini_emit_ticked(7)`; `count==7`; `e->size == sizeof(MiniEvTicked)`.
4. **дескриптор:** `mini_ev_cell_spawned_desc.name=="mini.cell_spawned"`, `field_count==6`,
   `fields[4].type==GAME_EVENT_FT_STRING`, `fields[4].offset==offsetof(...,label)`,
   `fields[5].type==GAME_EVENT_FT_BYTES` + `len_offset==offsetof(...,blob_len)`,
   `payload_size==sizeof(MiniEvCellSpawned)`; `mini_ev_desc_count==2`.
5. **пустая строка/байты:** emit с `label==""` (или NULL) и `blob_len==0` → аксессор строки
   даёт "", `blob_len==0`.
6. **порядок:** emit `ticked` затем `cell_spawned`; walk по индексу; `seq` монотонен, типы
   совпадают.

---

## E2.11 Критерии приёмки (бинарные)

- [ ] `load_schema` парсит+валидирует `events`; state-выхлоп (`game_state.{h,c}`/
      `_schema.gen.h`/`_devapi.c`) БАЙТ-ИДЕНТИЧЕН A4-golden (события в отдельном family,
      НЕ во встроенной схеме).
- [ ] Генерируются `<frag>_state_events.gen.{h,c}` (game + mini); golden game+mini
      совпадают байт-в-байт; первичный захват прошёл ревью глазами до коммита.
- [ ] `float`-поле события → C `double` (f64); property-тест `"double rate;"` зелёный.
      string-поле → `uint32_t`-оффсет (`"uint32_t label;"`, НЕ `char[]`); bytes → оффсет+len.
- [ ] emit-хелпер укладывает инлайн-строки/байты позиционно-независимо через union-стейджинг →
      сырой `game_event_emit`; scalar-only — прямой структ; НЕ трогает `game_events.{c,h}`.
      Переполнение стейджинга: `NT_ASSERT` debug + `nt_log_warn`+`return NULL` release (M2).
- [ ] Retain-приёмник (канон §E2.7 + ctest #2) — ВЫРОВНЕННЫЙ union/`_Alignas`, НЕ `uint8_t[]`
      (каст align-1 к структу = UB, M1).
- [ ] Warning-gated компайл-чек: OBJECT-таргет `check_mini_state_events` компилит golden
      `mini_state_events.gen.c` под `nt_set_warning_flags`+`-Werror` БЕЗ Unity → i64/f64/hash/
      bool/bytes/union-ветки под `-Wconversion` (M3).
- [ ] `_Static_assert(_Alignof(<Struct>) <= _Alignof(max_align_t))` эмитится и держится
      (структ ≤ align 8; сырой emit принимает `_Alignof(<Struct>)`).
- [ ] Тип события = ленивый кэш `<frag>_ev_<evt>_type()` (`nt_hash64_str("<frag>.<evt>")`);
      дескриптор {name,type,offset(+len_offset)} + пер-фрагмент таблица + регистратор меток.
- [ ] Линт имён: событие/поле `[a-z][a-z0-9_]*`, коллизии c_ident, резерв `type/seq/tick`,
      неизвестный тип, лишний ключ — negative-тесты фейлятся как ожидается.
- [ ] `ctest -R test_game_events_typed` зелёный (emit→typed read→аксессор строки; retain из
      копии; bytes; scalar-only; дескриптор; порядок). Все прежние ctest (state/save/storage/
      json/events(+overflow)) остаются зелёными.
- [ ] Шаблон собирается warning-clean под `-Werror`+`nt_set_warning_flags` в native-debug +
      release + devapi-debug (все компилят `game_state_events.gen.c`).
- [ ] Секция `events` в `game_state.schema.json` (1) и `mini_state.schema.json` (2); mini
      несёт i64/f64/hash/bool/string/bytes + scalar-only.
- [ ] `game_event_desc.h` (shared контракт) добавлен в `src/`; НЕ правит E1.

---

## E2.12 Пакет делегирования

**Смешанный: deep-reasoner ведёт рендереры/раскладку, fast-worker добивает механику.**
- **deep-reasoner** (или лид-ревью Sonnet-реализации): `render_events_header`/
  `render_events_source` (union-стейджинг + оффсет-паковка + `_Static_assert` +
  `-Wconversion`-чистота кастов оффсетов); `load_schema` events-валидатор (линт, коллизии,
  словарь, резерв). Рисковые места: (1) позиционно-независимая паковка (строки/байты инлайн,
  оффсеты payload-относительны — промах ловит retain-ctest #2); (2) union-стейджинг +
  `_Alignof`/страж `GAME_EVENT_EMIT_MAX` (портируемость без strict-aliasing); (3) `float`→
  `double` (НЕ путать со стейт-`float`); (4) неймспейс `GameEv…`/`game_emit_…` vs E1
  `game_event_*` (промах ловит golden mini).
- **fast-worker:** `game_event_desc.h` (по §E2.4 литералу), секции `events` в двух схемах,
  `test_game_events_typed.c` по списку кейсов (retain-приёмник — ВЫРОВНЕННЫЙ union, M1),
  расширение `generate_state_test.py` (суффиксы+property с ТОЧНЫМИ event-ключами L2 +
  negative вкл. `<bytes>_len`-коллизию L3), CMake-правки (custom-command OUTPUT + target_sources +
  ctest + OBJECT-таргет `check_mini_state_events` под `nt_set_warning_flags` M3), опц. main.c-строка.
- **Порядок:** golden-захват game+mini — ПОСЛЕ ревью рендереров (иначе закрепишь баг в
  эталоне); `render_events_*`+`main()`-запись+`OUTPUT_SUFFIXES`+захват = ОДИН атомарный шаг
  (L4). Приёмка ОБЯЗАНА включать golden байт-в-байт + все ctest зелёные + warning-clean три
  конфига + `check_mini_state_events` собирается под `-Werror` + компайл-чек golden через
  `test_game_events_typed`.

---

## E2.13 Что СОЗНАТЕЛЬНО НЕ входит (E3/E4/T0327 / швы)

- **DevAPI-лента `game.events.tail`** (приватный ринг-копир + generic-рендер по дескрипторам,
  render-at-copy) — **E3** (event §4/§6). Шов готов: `game_event_desc_t`-таблицы
  (`<frag>_ev_descs`/`_desc_count`) + FT-enum + payload-относительные оффсеты — E3 итерирует
  дескрипторы, рендерит ЛЮБОЕ событие без фичевого кода.
- **`NT_HASH_LABELS` build-config** — **E3**. `<frag>_ev_register` регистрирует метки, но
  `nt_hash_register_label64` no-op без движкового `NT_HASH_LABELS=1` (E1 §E1.9). E2
  поведенчески корректен без меток (хеш вместо имени в дампах).
- **Локальная аналитика-писатель + встроенный тип `log {string}`** — **E4** (event §6). E2 не
  заводит `log`-тип; его форма совместима (игра могла бы объявить `log {string}` уже сейчас).
  bytes-страж (рекордеры/аналитика игнорируют, DevAPI size+hex) реализует E3/E4 по
  `GAME_EVENT_FT_BYTES`.
- **События в items/progression схемах (T0327)** — рождаются с этим форматом при их
  создании; E2 закладывает механику.
- **Двухфазный react/record + агрегатор** — E1 (готово); потребители событий садятся в
  `game_features_react`/`_record` одной строкой.
- **Reserve-примитив в `game_events.c`** (zero-copy emit без стейджинга) — ОТВЕРГНУТ:
  потребовал бы правки E1 (Отступление 5).

---

## E2.14 Открытые вопросы лиду — НЕТ (единственный закрыт)

**Открытых вопросов нет.** Единственный кандидат закрыт решением лида 2026-07-07:

1. **Прошедшее время имён событий — РЕШЕНО лидом: СОВЕТ, не хард-фейл.** event doc §5 просит
   «линт `feature.noun_verbed` прошедшее время», НО собственные примеры дизайна (`items.txn`,
   `progression.levelup`) НЕ в прошедшем, а механическая детекция времени ненадёжна (отвергла
   бы `txn`). **Принят дефолт: хард-линт = charset + коллизии c_ident + резерв (`type/seq/tick`,
   синтезируемый `<bytes>_len`) + словарь типов; прошедшее время `noun_verbed` — СОВЕТ в скилле,
   не хард-фейл.** Правило двух таксономий (имя=ЧТО, reason=ПОЧЕМУ) — дисциплина скилла.

(Иных расхождений дизайна с кодом, требующих решения лида, не найдено. §E2.15 — инженерные
решения в рамках дизайна.)

---

## E2.15 Отступления от буквы дизайна (с обоснованием)

1. **Событийный `float` = C `double` (f64), в отличие от стейт-`float` = C `float`.**
   event doc §3/§7 Q3 РЕШИЛ f64 (идл-магнитуды, аналитика в double). Имя выровнено со стейтом
   (§5, «чтобы агент не расщепил словари»), но ШИРИНА иная. Прямое следование дизайну;
   помечено выпукло (таблица §E2.2, критерий, property-тест), т.к. одинаковое имя/разная
   ширина — источник путаницы.
2. **Событийный `int` = `int32_t` (явная ширина), стейт-`int` = `int`.** Дескриптор/провод —
   фиксированной ширины (E3/аналитика читают по оффсету известный тип); `int32_t` однозначен.
   На таргетах хоста `int`==`int32_t`, поведенчески идентично.
3. **Файлы `<frag>_state_events.gen.{h,c}` (под state-family), символы `<frag>_emit_*`/
   `<frag>_ev_*` (feature-level).** Дизайн-примеры дают `items_emit_txn` (feature-level). Имя
   ФАЙЛА взято под state-family, чтобы (а) тест-инфра переиспользовала prefix `<frag>_state`
   (`OUTPUT_SUFFIXES`), (б) развести с E1 `src/game_events.{c,h}` (транспорт). Символьный
   префикс `game_ev_`/`game_emit_` отличается от E1 `game_event_*`. Разные family, разные
   директории — коллизии нет.
4. **Shared рукописный `src/game_event_desc.h` введён E2.** Дескриптор-ЗНАЧЕНИЯ генерятся
   (per-событие), но дескриптор-ТИП (схема дескриптора) — общая инфра для E3 (один тип на все
   фрагменты; два `.gen.h` в одном TU иначе редефайнили бы тип). Параллель E1 (`game_events.h`),
   аддитивно, НЕ правка E1.
5. **emit укладывает payload через union-стейджинг поверх сырого emit (не reserve-примитив).**
   Сырой `game_event_emit` копирует из `payload` (E1 заморожен: «E2 генерит ПОВЕРХ, не
   меняет»). Позиционная независимость требует непрерывного `[структ|строки|байты]` → его надо
   собрать ДО copy-emit. Reserve-примитив (zero-copy) потребовал бы правки `game_events.c` →
   ОТВЕРГНУТ. Стейджинг: union `{struct; uint8_t[GAME_EVENT_EMIT_MAX]}` (портируемо-корректно,
   без strict-aliasing UB). Страж-cap: **debug-класс ЕДИН с E1** (`NT_ASSERT`), но **release
   АСИММЕТРИЧЕН и это осознанно** — E1 при переполнении арены делает `s_dropped++` + per-frame
   warn; стейджинг-переполнение делает `nt_log_warn` (per-emit) + `return NULL` БЕЗ счётчика
   (`game_events_dropped()` приватен E1, инкрементить нельзя — E1 заморожен). Реалистичный
   триггер — bytes-блоб >4КБ (программерская ошибка/огромный payload). scalar-only событие
   стейджинг НЕ использует (прямой структ). Цена: одна лишняя копия + cap на инлайн-payload
   (события — крошечные факты, 4КБ дефолт с запасом).
6. **Тип события = ленивый аксессор `<frag>_ev_<evt>_type()`, не compile-time константа.**
   event doc §3 зовёт «type_hash-константа», НО `nt_hash64_str` — рантайм-функция (нет
   constexpr-хеша в C; зеркалить FNV макросом хрупко — молча ломается при смене движкового
   хеша). Ленивый file-static кэш (single-thread) вычисляет ОДИН раз, без зависимости от
   порядка init; `<frag>_ev_register` (метки) — опционален для корректности. E1-канон
   `e->type.value == <type>().value` работает.
7. **События НЕ входят во встроенную нормализованную схему (`<frag>_state_schema.gen.h`).**
   Встроенная схема — для стейт-редактора/смоук-бота; события рендерит generic-потребитель по
   C-ДЕСКРИПТОРУ (E3), не по встроенному JSON. Держим события снаружи → A4 state-golden +
   DevAPI-схема + смоук-бот БАЙТ-ИДЕНТИЧНЫ (нулевой churn state-выхлопа). Чистое разделение
   families.
8. **Прошедшее время — совет, не хард-линт** (РЕШЕНО лидом 2026-07-07; §E2.14).
9. **bytes: пара оффсет+len в структе, страж в дескрипторе.** event doc §7 Q4: bytes остаётся
   со стражем (рекордеры/аналитика игнорируют, DevAPI size+hex). Реализовано `GAME_EVENT_FT_BYTES`
   + `len_offset` в дескрипторе; страж-семантику исполняет generic-рендер (E3/E4), E2 даёт
   разметку.
