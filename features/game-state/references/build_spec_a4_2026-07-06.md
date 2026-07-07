# BUILD-SPEC: генератор стейта v2 (--fragment), инкремент A4 (2026-07-06)

Имплементационная спецификация. Переводит принятый дизайн в файлы, сигнатуры
генерируемого кода, схему v2, изменения `generate_state.py`, golden/compile/roundtrip-
гейт, CMake и критерии приёмки. НЕ меняет дизайн. При расхождении источник истины —
`features/game-state/references/state_system_design_2026-07-06.md` (в нём §14 главнее
основного текста), затем `build_spec_a1_a3_2026-07-06.md` (ABI фрагмента и `gsj_*`,
уже в дереве).

Исполнитель может писать код по этому документу, НЕ открывая историю обсуждений.

---

## 0. Предпосылки и рамки (проверено по дереву)

- **A0–A3 + E1 УЖЕ в дереве.** Установлены `templates/template/src/game_state_json.{c,h}`
  (`gsj_*` + i64-провод), `game_storage.{c,h}`, `game_save.{c,h}` (шелл: конверт,
  реестр, оркестрация load, dirty/debounce, export/import, transform-шов),
  `game_fragment.c` (переходный адаптер `game` вокруг `g_game_state`), два-фазный
  кадр E1. CMake несёт генерацию стейта (CMakeLists:131–176) и пять ctest-таргетов.
- **Генератор — единственный живой путь**, `features/game-state/scripts/generate_state.py`
  (1686 строк, пост-ампутация). Тесты генератора: `generate_state_test.py`.
- **Выхлоп генератора СЕЙЧАС** (`templates/template/build/native-debug/generated/game-state/`):
  `game_state.h` (монолит `GameState` + `extern GameState g_game_state` + весь
  `game_state_*` API), `game_state.c` (реализация + СВОИ static-копии хелперов
  `set_error/copy_text/object_item/read_*/parse_*/enum_index` + конверт `make_save_doc`
  + файловый I/O `game_state_save/load/replace_file/...` + `s_dirty`/`notify_changed` +
  глобал `g_game_state`), `game_state_schema.gen.h` (сырая схема чанками),
  `game_state_devapi.c` (7 команд `game.state.*` против `g_game_state` + compat-обёртки
  `game_storage_save_json/load_json/resolve_key`).
- **КЛЮЧЕВОЙ ФАКТ (проверен grep'ом):** во всём рукописном коде шаблона `g_game_state`
  и `game_state_*` НЕ потребляются НИГДЕ, кроме `game_fragment.c` (адаптер) и `main.c`
  (две строки: `game_state_init()` :371, `game_state_register_devapi()` :152). Рендер-
  системы шаблона (`sys_settings`, `hud`, `render_mesh`, `sys_move`) стейт НЕ читают.
  Значит смерть монолита В A4 ЛОКАЛИЗОВАНА: затрагивает только генератор, его выхлоп,
  `game_fragment.c` (удаляется) и две строки `main.c`.
- **Смоук-бот шаблона** (`templates/template/devapi/smoke_bot.py`) зовёт ТОЛЬКО
  `game.state.schema` и `game.state.get {path:""}`. Его жёсткие проверки:
  `schema.schema == "game_seed.state"`, `schema.document == "game"`, `schema.fields`
  — МАССИВ; `get` возвращает `{path:"", value:{...}}`, где `value.settings`,
  `value.tutorial`, `value.inventory` — объекты. Save/load/set/patch/reset бот НЕ
  зовёт. **Р8: переписывание бота на фрагментные ключи = A5; A4 обязан оставить бот
  ЗЕЛЁНЫМ** (эти проверки должны продолжать проходить).
- **Один тред.** Потокобезопасность нигде не требуется.
- **Сборка:** game-таргет `-Werror` + `nt_set_warning_flags` (`-Wconversion`/
  `-Wdouble-promotion`/`-Wshadow`/`-Wformat=2`); i64-арифметика и касты — ЯВНЫЕ.
  Unity собран `UNITY_EXCLUDE_FLOAT/DOUBLE` → float-макросов нет (сравнивать вручную).
- **Хвосты A0 в дереве:** `templates/template/state/migrations/v0_to_v1.c` (мёртвый
  external, `#include "game_state.h"`, зовёт `game_state_shape_name`) ВСЁ ЕЩЁ
  скомпилирован (CMakeLists:163) + `state/fixtures/{v0_save,v1_from_v0_expected,
  wrong_document}.json`. A1–A3 отложили их снятие «A0/отдельным шагом»; A0 их не снял.
  A4 владеет переписыванием миграционной модели → снимает их (§A4.9, Отступление 5).

### Что A4 делает (ровно)

1. Переписывает `generate_state.py` в v2: пер-фрагментный неймспейс, схема v2
   (идентичность = имя ключа, числовых id НЕТ), тип `i64` (строкой), валидация
   коллизий/`"v"`/i64-границ/charset id, `gsj_*` вместо static-копий, генерируемый
   ДЕСКРИПТОР `GameSaveFragment` + миграционная ТАБЛИЦА, встроенная НОРМАЛИЗОВАННАЯ
   схема, минимальный transitional DevAPI-рендерер (одно-фрагментный, save/load → шелл).
   Монолит (`g_game_state`, конверт, файловый I/O, 7 старых команд против глобала,
   `s_dirty`/`notify_changed`) — УМИРАЕТ. Legacy-режима НЕТ.
2. Переписывает `templates/template/state/game_state.schema.json` в v2-вид.
3. Удаляет `templates/template/src/game_fragment.c` (дескриптор теперь генерится).
4. Правит `main.c` (2 строки) и CMake (флаг `--fragment`, снятие мёртвых хвостов,
   golden/roundtrip ctest).
5. Заводит golden+compile+roundtrip-гейт (`features/game-state/tests/golden/`,
   расширение `generate_state_test.py`, ctest `test_game_state_roundtrip`).

**НЕ входит (границы, §A4.10):** DevAPI-диспатч по реестру (get ""=агрегат features,
роутинг по голове пути, транзакционный patch через `gsj_transact`) — A5; переписывание
смоук-бота — A5; events-рендерер (struct/emit/type_hash/дескриптор) — E2; реальные
фрагменты settings/items/progression + фикстуры + reconcile — A6.

---

## A4.1 Файлы

**Изменяемые:**
- `features/game-state/scripts/generate_state.py` — переписывание v2 (§A4.6, ядро A4).
- `features/game-state/scripts/generate_state_test.py` — golden + property + validation
  тесты (§A4.8).
- `templates/template/state/game_state.schema.json` — переписать в v2 (§A4.5).
- `templates/template/src/main.c` — 2 строки (§A4.7).
- `templates/template/CMakeLists.txt` — `--fragment` флаг, снятие `v0_to_v1.c`,
  roundtrip-ctest (§A4.7).
- `features/game-state/feature.json` — `default_template.runtime_sources`: снять
  `templates/template/src/game_fragment.c` (последняя запись, :49; файл удаляется в A4,
  дескриптор теперь генерится). **M1:** `state/migrations/v0_to_v1.c` в runtime_sources
  УЖЕ НЕТ (A1 снял) — трогать нечего; его снятие только из CMake (§A4.9). При появлении
  hand-written логики фрагмента (в шаблоне её НЕТ) сюда добавлялись бы
  `<id>_migrations.c`/`<id>_hooks.c` — для шаблона не нужно.
- `features/game-state/INSTALL.md` — снять `src/game_fragment.c` из copy-list (:27) и из
  «compile … `game_fragment.c`» (:80); перенаправить `game_save_register_fragment(&game_fragment)`
  (:52) на `&game_state_fragment`; обновить описание v2-контракта (доки). **ГАРД M1:**
  copy-model новой игры копирует runtime_sources/copy-list построчно и НЕ гейтит
  существование файла — оставшаяся ссылка на удалённый `game_fragment.c` тихо сломает
  установку новой игры (template-гейт этого не ловит).

**Новые:**
- `features/game-state/tests/golden/game/{game_state.h,game_state.c,
  game_state_schema.gen.h,game_state_devapi.c}` — эталонный v2-выхлоп для схемы шаблона.
- `features/game-state/tests/golden/mini/{mini_state.h,mini_state.c,
  mini_state_schema.gen.h,mini_state_devapi.c}` — эталон синтетического фрагмента `mini`
  (id ≠ "game", несёт enum + map<string,Object> + list<string> + i64 + string, **M4**) —
  ПРИГВОЖДАЕТ параметризацию неймспейса (`mini_state`/`MiniState`/`MINI_STATE_` +
  `<Id>State<Enum>` + `MINI_STATE_MAX_*` + object-префиксы), которую фрагмент `game`
  (совпадающий с legacy-именами) НЕ ловит. Полный литерал схемы — §A4.8.
- `features/game-state/tests/mini_state.schema.json` — вход golden `mini` (литерал в §A4.8).
- `templates/template/tests/test_game_state_roundtrip.c` (Unity) — roundtrip-гейт (§A4.8).

**Удаляемые:**
- `templates/template/src/game_fragment.c` (адаптер; дескриптор теперь генерится).
- `templates/template/state/migrations/v0_to_v1.c` (мёртвый монолит-хвост A0).
- `templates/template/state/fixtures/{v0_save,v1_from_v0_expected,wrong_document}.json`
  (фикстуры мёртвого монолитного envelope-пути).

**Не трогать:** `game_state_json.*`, `game_storage.*`, `game_save.*` (шелл A1–A3 стабилен),
`game_events.*`/`game_features.*` (E1), движок, `games/rb-dark-rpg/**` (закрытая игра —
см. §A4.11 clean-break).

---

## A4.2 Судьба legacy-режима, DevAPI-шва, владения структом (ключевые решения)

### Legacy-режим — УДАЛЁН
Генератор ВСЕГДА пер-фрагментный (state doc §7). Legacy-монолит-режима (общий
`GameState`/`g_game_state`, конверт `make_save_doc`, файловый I/O `game_state_save/load`,
7 команд против глобала) в v2 НЕТ. `--fragment` — не «включает» режим, а именует
фрагмент. Обоснование: §7 «Всегда пер-фрагментный», §14 п.12 clean-break, §12 A4
«g_game_state умирает». Следствие для rb-dark (v1-схема + монолит-ABI) — §A4.11.

### Владение структом — НАЗВАННЫЙ instance в TU фрагмента (extern)
Генерируемый `<id>_state.c` ОПРЕДЕЛЯЕТ инстанс `<Id>State <id>_state;` и владеет им;
генерируемый заголовок объявляет `extern <Id>State <id>_state;` для рукописной ЛОГИКИ
фичи. «g_game_state умирает» = умирает ОБЩИЙ монолит-глобал, на котором висела вся игра;
каждый фрагмент теперь несёт СВОЙ неймспейснутый инстанс, ОПРЕДЕЛЁННЫЙ в СВОЁМ TU
(«владение уходит из глобала в TU фрагмента»). Для `game`: `GameState game_state;`
вместо `g_game_state`.

Отвергнуто (Model B): полностью приватный static + гет/сет-аксессоры. Причина: мандат
лида «гибко и удобно для использования мной в играх» + в шаблоне НОЛЬ рукописных
потребителей, а игры на базе шаблона будут писать `game_state.hero_gold` напрямую.
Инкапсуляцию игра выбирает сама (обёртки-API фичи), шелл её не навязывает. Смена решения
позже дешёвая (спрятать инстанс, добавить `<id>_state_get(void)`).

### DevAPI-шов — минимальная перегенерация transitional-рендерера (7 команд), save/load → шелл; g_game_state НЕ доживает
Меньшее зло из двух названных в ТЗ. Причина отклонения «g_game_state как алиас до A5»:
алиас ТРЕБУЕТ сохранить в генерируемом коде мёртвый монолит-envelope + файловый I/O
(`make_save_doc`, `game_state_save/load`), который A3 УЖЕ заменил шеллом → два писателя
конверта, ровно тот техдолг, ради удаления которого существует A4 (паттерн «обход,
оставляющий дефект»). Поэтому:

`<id>_state_devapi.c` перегенерируется в transitional одно-фрагментную форму, роутящую
через ДЕСКРИПТОР фрагмента (`<id>_state_fragment`) + шелл `game_save`, БЕЗ `g_game_state`
и БЕЗ монолит-файлового-I/O:
- `game.state.schema` → `<id>_state_schema_json()` (нормализованная схема; несёт
  `schema`/`document`=fragment/`fields`-массив → смоук-бот зелёный, см. §A4.4).
- `game.state.get {path}` → `path==""` оборачивает `<id>_state_to_json(&<id>_state)`;
  иначе `<id>_state_get_path_json(&<id>_state, path, ...)`. **get "" = ПЛОСКИЙ payload
  фрагмента `game`** (НЕ агрегат features — агрегат = A5) → смоук-бот зелёный.
- `game.state.set {path,value}` → `<id>_state_set_path_json(&<id>_state, ...)` +
  `game_save_mark_dirty()`; ответ = `<id>_state_to_json`.
- `game.state.patch {values}` → `<id>_state_patch_json(&<id>_state, values, ...)`
  (генерируемый validate-copy-swap на инстансе) + `game_save_mark_dirty()`.
  (Транзакционный patch через `gsj_transact` кросс-ключево — A5; A4 использует
  готовый пофрагментный copy-swap.)
- `game.state.reset {}` → `<id>_state_init_defaults(&<id>_state)` + `game_save_mark_dirty()`.
- `game.state.save {}` → `game_save_flush(...)` (шелл владеет конвертом+сторажем).
  Параметры `key`/`unsafe_path`/`resolved` СНЯТЫ (монолит-file-IO мёртв). Бот их не
  зовёт → безопасно.
- `game.state.load {}` → `game_save_load(&result)`; ответ = `<id>_state_to_json`.

7 команд сохранены (континуитет тулинга, §8). В A5 этот генерируемый файл УДАЛЯЕТСЯ и
заменяется рукописным шелл-DevAPI с настоящим реестр-диспатчем (get ""=агрегат features,
роутинг по голове пути, транзакционный patch). Единственная «дотяжка» A4 к A5 —
роутинг save/load на `game_save` (генерируемый devapi транзитно инклюдит `game_save.h`);
альтернатива «урезать до 5 команд» отклонена ради 7-командного континуитета §8
(см. Отступление 2). **РЕШЕНО лидом 2026-07-06: 7 команд (save/load транзитно через шелл)
— tool parity.** Вопрос закрыт.

---

## A4.3 Полный контракт генерируемого фрагмента (для id="game")

Неймспейс (state doc §7): тип `<Id>State`, инстанс `<id>_state`, функции `<id>_state_*`,
макросы `<ID>_STATE_*`, где `<Id>`=PascalCase(id), `<id>`/`<ID>` = c_ident(id) в lower/upper.
Для id="game" → `GameState`/`game_state`/`game_state_*`/`GAME_STATE_*` (СОВПАДАЕТ с legacy —
минимизирует golden-дифф; неймспейс проверяется golden `mini`, §A4.1). Charset id:
`[a-z_][a-z0-9_]*` (assert генератора).

### Генерируемый `game_state.h` (форма целиком)

```c
#ifndef GAME_STATE_GENERATED_H
#define GAME_STATE_GENERATED_H

/* Generated by features/game-state/scripts/generate_state.py from <schema_label>. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>              /* NEW: int64_t для i64-полей */

#include "cJSON.h"
#include "game_save.h"           /* NEW: GameSaveFragment (дескриптор ниже) */

#define GAME_STATE_SCHEMA_ID   "game_seed.state"   /* из schema.schema (метка) */
#define GAME_STATE_FRAGMENT_ID "game"              /* из schema.fragment (= ключ в features{}) */
#define GAME_STATE_VERSION     1                   /* = len(migrations)+1 */
#define GAME_STATE_STRING_MAX  64

/* ... per-field DEFAULT/MIN/MAX макросы (i64 — с суффиксом LL, см. §A4.6) ... */
/* ... GAME_STATE_MAX_<COLLECTION> из max_count ... */

/* ... enums: typedef enum GameStateShape {...} ... (без изменений формы) ... */
/* ... object structs: typedef struct GameItemInstance {...} ... */

typedef struct GameState {
    /* ... поля; i64-поле -> int64_t; остальное как v1 ... */
} GameState;

/* Инстанс, которым владеет TU фрагмента (общий g_game_state УМЕР). Рукописная
   ЛОГИКА фичи работает с ним напрямую или через свой API. */
extern GameState game_state;

const char *game_state_shape_name(int value);   /* по одному на enum */
/* ... прочие _name ... */

void   game_state_init_defaults(GameState *state);
bool   game_state_validate(const GameState *state, char *error, int error_cap);
cJSON *game_state_schema_json(void);                       /* НОРМАЛИЗОВАННАЯ схема */
cJSON *game_state_to_json(const GameState *state);          /* только данные, без "v" */
cJSON *game_state_get_path_json(const GameState *state, const char *path, char *error, int error_cap);
bool   game_state_set_path_json(GameState *state, const char *path, const cJSON *value, char *error, int error_cap);
bool   game_state_patch_json(GameState *state, const cJSON *values, char *error, int error_cap); /* validate-copy-swap; БЕЗ dirty/notify */
bool   game_state_from_json(GameState *state, const cJSON *json, char *error, int error_cap);     /* толерантный */

/* Генерируемый дескриптор — ЗАМЕНЯЕТ рукописный game_fragment.c. */
extern const GameSaveFragment game_state_fragment;

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);   /* transitional одно-фрагментный (A5 заменит) */
#endif

#endif
```

**СНЯТО из v1-заголовка (умирает вместе с монолитом):**
`extern GameState g_game_state` (→ `game_state`); `game_state_changed_fn` typedef +
`game_state_set_changed_callback`; `game_state_mark_dirty/is_dirty/clear_dirty` (dirty —
забота `game_save`); `game_state_save_json_string/load_json_string/save/load/reset`
(конверт+файловый I/O — забота `game_save`); `game_state_init(void)` (init — через
`init_defaults` инстанса, зовёт `game_save`/`main.c`); `GAME_STATE_DOCUMENT` (→ FRAGMENT_ID).

### Генерируемый `game_state.c` (форма)

```c
#include "game_state.h"
#include "game_state_schema.gen.h"
#include "game_state_json.h"     /* NEW: gsj_* вместо static-копий */

/* Generated by ... */
#include <stdlib.h>
#include <string.h>
/* НЕТ <windows.h>/<direct.h>/<sys/stat.h> — файлового I/O больше нет */

/* ... k_<enum>_names[] + game_state_<enum>_name(int) ... (без изменений) ... */

GameState game_state;   /* инстанс фрагмента (владение здесь) */

/* object-helpers + collection-helpers: как v1, но低-уровневые вызовы -> gsj_* */
/* ... game_state_init_defaults / _validate / _schema_json / _to_json /
       _get_path_json / _set_path_json / _patch_json / _from_json ...
   ВСЕ вызовы set_error/copy_text/object_item/read_*/parse_*/enum_index заменены
   на gsj_set_error/gsj_copy_text/gsj_object_item/gsj_read_*/gsj_parse_*/gsj_enum_index;
   i64 -> gsj_read_i64/gsj_add_i64/gsj_parse_i64_value. patch НЕ трогает dirty/notify. */

/* --- дескриптор: тонкие статик-обёртки на &game_state --- */
static void   frag_reset(void)                 { game_state_init_defaults(&game_state); }
static cJSON *frag_to_json(void)               { return game_state_to_json(&game_state); }
static bool   frag_from_json(const cJSON *j, char *e, int c) { return game_state_from_json(&game_state, j, e, c); }
static cJSON *frag_get_path(const char *s, char *e, int c)   { return game_state_get_path_json(&game_state, s, e, c); }
static bool   frag_set_path(const char *s, const cJSON *v, char *e, int c) { return game_state_set_path_json(&game_state, s, v, e, c); }
static cJSON *frag_schema(void)                { return game_state_schema_json(); }

/* --- миграционная ТАБЛИЦА (генерится; ШАГИ пишутся руками) --- §A4.6 «Миграции» --- */
/*   при migrations=[]: строк ниже НЕТ, .steps = NULL                                 */
/*   иначе:                                                                            */
/* extern bool game_migrate_v1_to_v2(cJSON *frag, char *err, int cap);  (тело — рукопись)*/
/* static const GameSaveMigrateFn game_state_migration_steps[] = { game_migrate_v1_to_v2, ... }; */

/* --- рукописные ЛОГИКА-хуки (только если объявлены в schema.hooks) --- */
/* extern void game_on_new_game(void);   (тело — рукопись, если hooks.on_new_game) */
/* extern void game_reconcile(void);     (тело — рукопись, если hooks.reconcile)   */

const GameSaveFragment game_state_fragment = {
    .id            = GAME_STATE_FRAGMENT_ID,
    .version       = GAME_STATE_VERSION,
    .steps         = NULL,          /* или game_state_migration_steps */
    .reset         = frag_reset,
    .on_new_game   = NULL,          /* или game_on_new_game */
    .to_json       = frag_to_json,
    .from_json     = frag_from_json,
    .reconcile     = NULL,          /* или game_reconcile */
    .get_path_json = frag_get_path,
    .set_path_json = frag_set_path,
    .schema_json   = frag_schema,
};
```

**СНЯТО из v1-source:** блок static-копий хелперов (`set_error`…`parse_int_value`,
generate_state.py:1286–1398); `GAME_STATE_MAYBE_UNUSED`; `GameState g_game_state`
(→ named); `s_changed`/`s_changed_user`/`s_dirty` + `notify_changed` +
`game_state_mark_dirty/is_dirty/clear_dirty` + `game_state_init/set_changed_callback`;
`make_save_doc`/`game_state_save_json_string`; `make_dir_if_needed`/`ensure_parent_dirs`/
`replace_file`/`game_state_save`/`read_file`/`game_state_load_doc`/`game_state_load_json_string`/
`game_state_load`/`game_state_reset` (весь envelope+файловый I/O).

### Разделение генерируемое / рукописное (реализует Р9)

| Сущность | Кто | Где |
|---|---|---|
| struct `<Id>State`, enums, DEFAULT/MIN/MAX, инстанс `<id>_state` | генерится | `<id>_state.h/.c` |
| defaults / validate / to_json / from_json / get_path / set_path / patch / schema_json | генерится | `<id>_state.c` |
| дескриптор `<id>_state_fragment` + обёртки | генерится | `<id>_state.c` |
| миграционная ТАБЛИЦА (массив указателей) | генерится | `<id>_state.c` |
| встроенная НОРМАЛИЗОВАННАЯ схема | генерится | `<id>_state_schema.gen.h` |
| transitional DevAPI (7 команд) | генерится | `<id>_state_devapi.c` |
| ТЕЛА миграционных шагов `v(i)->v(i+1)` | руками | `<id>_migrations.c` (в шаблоне НЕТ) |
| тело `on_new_game` (стартовый контент) | руками | `<id>_hooks.c` (в шаблоне НЕТ) |
| тело `reconcile` (пост-load фиксап) | руками | `<id>_hooks.c` (в шаблоне НЕТ) |
| API фичи (мутаторы, читают/пишут инстанс) | руками | фича |

Для фрагмента `game` шаблона: `migrations=[]`, `hooks={}` → `.steps/.on_new_game/
.reconcile = NULL`, НУЛЬ рукописных файлов. `game_fragment.c` УДАЛЯЕТСЯ, ничем не
заменяется (дескриптор `game_state_fragment` генерится).

---

## A4.4 Встроенная схема = НОРМАЛИЗОВАННАЯ (чтобы не сломать смоук-бот)

`render_schema_header`/`game_state_schema_json()` встраивают НЕ сырой v2-файл, а
НОРМАЛИЗОВАННУЮ канон-форму (её же генератор строит внутри для рендереров):
- `fields`/`types.<T>.fields` — СПИСКИ объектов `{ "path": "<key>", ...spec }` (ключ
  инжектится как `path`). ← смоук-бот проверяет `isinstance(schema.fields, list)`.
- Верхнеуровневые: `schema` (id-метка, сохраняется), `document` = `fragment`
  (compat-дубликат для transitional-бота), `fragment`, `version`, `schema_version`,
  `string_max`, `enums`, `types`, нормализованные `fields`.
- `ui:{}`/`deprecated`/`min`/`max`/`max_length`/`max_count`/`default`/`enum` — проносятся
  в нормализованную форму как есть (сквозные для будущего редактора, §5).

Смоук-бот (§0) зелёный: `schema=="game_seed.state"`, `document=="game"`, `fields`-массив,
`get ""` = payload `game` с `settings`/`tutorial`/`inventory`-объектами. Р8-переписывание
на фрагментные ключи (get ""=агрегат) откладывается на A5.

Обоснование отступления от «встраивать сырую схему»: v2-файл держит `fields` МАПОЙ и
`fragment` вместо `document`; сырое встраивание сломало бы обе бот-проверки, а бот
переписывается только в A5 (Отступление 3).

**Semantic-change фиксация (L6):** transitional `game.state.set` теперь = `set_path_json`
на ЖИВОМ инстансе `<id>_state` (было: patch copy-swap + полный `game_state_validate`
перед свапом). Т.е. отдельное set больше не гоняет валидацию всего стейта до применения
(per-field validate внутри set_path остаётся). Осознанно — минимальный transitional
роутинг; A5 (реестр-диспатч + транзакционный patch через `gsj_transact`) восстанавливает
атомарность/валидацию на уровне шелла. Смоук-бот set не зовёт → зелёности не влияет.

---

## A4.5 Схема v2: формат + переписывание схемы шаблона

### Формат v2 (state doc §5 + §14 п.11)

Верхний уровень:
```jsonc
{
  "schema": "game_seed.state",     // метка-id (сохранена; удобна логам/редактору)
  "schema_version": 2,             // §14 п.11: версия ДИАЛЕКТА схемы; +1 assert генератора
  "fragment": "game",              // ИДЕНТИЧНОСТЬ фрагмента (= ключ features{} = C-префикс), [a-z_][a-z0-9_]*
  "version": 1,                    // версия КОДА фрагмента = len(migrations)+1
  "string_max": 64,
  "reserved": ["grabbed"],         // СПИСОК ИМЁН мёртвых полей (числовых id НЕТ); переиспользовать нельзя
  "hooks": {                       // опц.: какие ЛОГИКА-хуки фича предоставит
    "on_new_game": false,          //   true -> генерится .on_new_game=game_on_new_game (тело руками)
    "reconcile":   false           //   true -> .reconcile=game_reconcile
  },
  "migrations": [],                // опц.: чейн шагов (см. ниже); []/absent -> .steps=NULL
  "enums": { "Shape": ["cube","sphere",...], ... },
  "types": {
    "ItemInstance": { "kind": "object", "fields": {
        "def_id":     { "type": "string", "max_length": 63 },
        "count":      { "type": "i64", "default": 1, "min": 1, "max": 999999999999 },
        "level":      { "type": "int", "default": 1, "min": 1, "max": 9999 },
        "durability": { "type": "float", "default": 1, "min": 0, "max": 1 }
    } }
  },
  "fields": {                      // МАП path -> спека; порядок файла = порядок struct/golden
    "shape_index":        { "type": "enum", "enum": "Shape", "default": "cube" },
    "camera_distance":    { "type": "float", "default": 6, "min": 2.5, "max": 10 },
    "test_ui_clicks":     { "type": "int", "default": 0, "min": 0, "max": 1000000 },
    "test_label_text":    { "type": "string", "default": "Template ready", "max_length": 63 },
    "settings.master_volume": { "type": "float", "default": 0.75, "min": 0, "max": 1 },
    "tutorial.done":      { "type": "bool", "default": false },
    "wallet.soft":        { "type": "i64", "default": 0, "min": 0, "max": 9000000000000000000 },
    "wallet.hard":        { "type": "i64", "default": 0, "min": 0, "max": 9000000000000000000 },
    "items":              { "type": "map<string,ItemInstance>", "max_count": 32 },
    "inventory.item_ids": { "type": "list<string>", "max_count": 32 },
    "equipment.hand_item_id": { "type": "string?", "default": null, "max_length": 63 }
    // ... все прочие поля v1 в ТОМ ЖЕ порядке ...
  }
}
```

**Типы (§5):** `int`(int32), **`i64`**(int64_t, JSON-строкой), `float`, `bool`, `string`,
`string?`, `enum`, `list<string>`, `map<string,Object>`. Пер-поле: `default`, `min`/`max`,
`max_length`, `max_count`, `deprecated`(читается-не-пишется, редактор прячет), `ui:{}`
(сквозной). **`v` как имя поля ЗАПРЕЩЕНО** (штампует шелл).

**ВЫРЕЗАНО из v1** (§5): числовые `id` полей + id-диапазоны (идентичность = имя ключа);
`collections{}` (→ пер-поле `max_count`); `lifetime`; per-field `devapi` (всё читаемо);
`document`/GameState-binding (→ `fragment`); `reserved` с id (→ список имён).

### Миграции v2 (контракт; §A4.3 таблица; state doc §14 п.9, Р2/Р9)

```jsonc
"version": 3,
"migrations": [
  { "to_version": 2, "fn": "game_migrate_v1_to_v2" },
  { "to_version": 3, "fn": "game_migrate_v2_to_v3" }
]
```
- Генератор ASSERT: `version == len(migrations)+1`; `to_version` монотонны 2..version;
  `fn` — валидный C-идентификатор, уникальны.
- Генерирует ТАБЛИЦУ `static const GameSaveMigrateFn <id>_state_migration_steps[] =
  { game_migrate_v1_to_v2, game_migrate_v2_to_v3 };` + `extern bool <fn>(cJSON *frag,
  char *err, int cap);` на каждый шаг; дескриптор `.steps = <table>`, `.version`.
- ТЕЛА шагов пишет фича в `<id>_migrations.c` (сигнатура = `GameSaveMigrateFn`:
  трансформирует СЫРОЙ payload фрагмента `v(i)->v(i+1)`, БЕЗ `"v"` — его снимает шелл).
  Забыл тело → link-error (здоровый контракт). Оркестрацию чейна (`steps[v-1..version-1]`
  над копией → `from_json`; фейл шага → reset фрагмента, не all-or-nothing) УЖЕ несёт
  `game_save` (A3.4 п.6) — A4 ничего в шелле не меняет.
- Переименование поля = шаг + имя в `reserved` (дисциплина в скилл, §14 п.9).

### Переписывание схемы шаблона (`game_state.schema.json`)

Механический v1→v2 transform всех 50 полей + 4 полей `types.ItemInstance`:
1. `fields`/`types.*.fields`: список → МАП по `path`; на каждом поле СНЯТЬ `id` и
   `devapi`; ключ = бывший `path`, порядок сохранить.
2. Верхний уровень: `document`+`lifetime` → удалить; добавить `"fragment":"game"`,
   `"schema_version":2`; `reserved` `[{id:99,path:"grabbed",...}]` → `["grabbed"]`;
   `collections{}` → удалить (значения уже дублированы в пер-поле `max_count`).
3. **i64-конверсии (упражняют i64-путь под golden+roundtrip):** `wallet.soft`,
   `wallet.hard` → `type:"i64"`, `max: 9000000000000000000` (>2^53, чтобы roundtrip
   реально гонял строковый путь); `types.ItemInstance.count` → `i64`. Остальные счётчики
   (`hero.*`, `battle.*`, `old_mine.*` — мелкие капы) остаются `int`.
   ЗАМЕЧАНИЕ СОВМЕСТИМОСТИ: `gsj_read_i64` принимает и ЧИСЛО (если ≤2^53) → старый A3-сейв,
   писавший `wallet.soft` числом, грузится (§A4.8 old-save-гейт).
4. `hooks`/`migrations` — не добавлять (у шаблона логики нет).

---

## A4.6 generate_state.py: список изменений по рендерерам

Механически, но это ПЕРЕПИСЫВАНИЕ (~70 литеральных вхождений `GameState`/`GAME_STATE_`/
`game_state_`/`g_game_state` + новый ABI, §14 п.13). Обязательный golden+compile+roundtrip-
гейт ловит промахи. Ввести helper-объект `Ns` (namespace), считанный из `fragment`:
`Ns.type`=`<Id>State`, `Ns.fn`=`<id>_state_`, `Ns.macro`=`<ID>_STATE_`, `Ns.inst`=`<id>_state`,
`Ns.frag`=`<id>_state_fragment`; протянуть в рендереры вместо литералов.

**Загрузка/валидация схемы (`load_schema`,`validate_supported_shape`,`validate_field_ids`,
`validate_field_shape`,`validate_string_length`,`validate_max_count`):**
- **ГАРД legacy-формы (M3, ПЕРВОЙ проверкой в `load_schema`):** если у схемы НЕТ
  `schema_version` ИЛИ есть ключ `document` (v1-маркеры) → `raise SystemExit(
  "v1 schema unsupported by v2 generator; rebuild rb-dark from its shipping tag")`.
  Так CMake-шаг генерации (rb-dark, v1) падает с ПОНЯТНЫМ сообщением, а не KeyError-
  трейсбеком на отсутствующем `fragment`/на `fields`-мапе. Негативный python-тест
  обязателен (§A4.8 (3)).
- Принять `fragment` (charset `[a-z_][a-z0-9_]*`, assert), `schema_version==2` (assert),
  опц. `schema`-метку. Снять требование `document` (взять `fragment`).
- НОРМАЛИЗАЦИЯ: `fields`-мап → внутренний список `{path=<key>, **spec}`; так же
  `types.*.fields`. Порядок ключей = порядок struct.
- `validate_field_ids` → переименовать в `validate_field_names`: снять всю id-логику;
  ловить дубли `path`; **коллизии c_ident** (два разных `path` → один C-идентификатор =
  ошибка `SystemExit`, сегодня ловит лишь компилятор); запрет `path=="v"`;
  `reserved` = список ИМЁН (не переиспользовать); enum-default в enum-значениях.
- Добавить тип `i64` в `SCALAR_TYPES`; для `i64` требовать `default/min/max` (как int),
  проверить `INT64_MIN <= min <= max <= INT64_MAX` и default в [min,max].
- Валидация `hooks` (bool-флаги) и `migrations` (см. §A4.5), `version==len(migrations)+1`.

**`field_c_type`:** `i64 -> "int64_t"`.

**`render_state_constants`:** ветка `i64`: `..._DEFAULT/_MIN/_MAX` как ДЕСЯТИЧНЫЕ int64
с суффиксом `LL` (напр. `#define GAME_STATE_WALLET_SOFT_MAX 9000000000000000000LL`).
Хелпер `c_i64(value)` (аналог `c_int`, добавляет `LL`).

**`render_cjson_add_scalar`** (to_json): `i64 -> gsj_add_i64(target, "<key>", state->ident);`
(строкой). Прочее без изменений формы.

**`render_get_scalar_expr`** (get_path И object get_field — M2): `i64` = ОДНО выражение
через compound literal, чтобы подставлялось в ОБА call-site (`render_get_scalar_if` :867
`return {expr};` и top-level get_path):
`cJSON_CreateString(gsj_i64_to_string(<state_expr>->{ident}, (char[21]){0}, 21))`.
Compound literal `(char[21]){0}` живёт до конца полного выражения — `gsj_i64_to_string`
пишет в него и возвращает указатель, `cJSON_CreateString` копирует строку немедленно →
корректно, `-Wconversion`-чисто. **Блочная форма из прежней редакции НЕ нужна — заменена
этим единым выражением** (иначе object get_field через `render_get_scalar_if`→`get_scalar_expr`
упёрся бы в `raise` на неизвестном типе / эмитил невалидный C).

**`render_set_scalar_if`** (set_path) и **`render_read_scalar`** (from_json): `i64` →
`gsj_parse_i64_value(value, MIN, MAX, &state->ident, ...)` / `gsj_read_i64(source,"<key>",
MIN, MAX, &target->ident, ...)`. `MIN`/`MAX` — i64-макросы (LL).

**`render_scalar_validation`** (:740–758, M5): добавить ветку `i64` — КАК `int`, но с
i64-макросами (LL) и `gsj_set_error`:
`if (state->{ident} < {MACRO}_MIN || state->{ident} > {MACRO}_MAX) { gsj_set_error(...,
"<path> out of range"); return false; }`. БЕЗ этой ветки `i64` падает в `else`→`return []`
→ `game_state_validate` МОЛЧА теряет range-check i64-полей, а golden это закрепит. То же
покрыть в `render_scalar_default_assignment` (:722–738) — ветка `i64` присваивает
`state->{ident} = {MACRO}_DEFAULT;` (LL-литерал), иначе дефолт i64 не эмитится.

**Замена низкоуровневых хелперов на `gsj_*`** во ВСЕХ рендерерах, эмитящих код
(`render_cjson_add_scalar`,`render_get_scalar_expr`,`render_set_scalar_if`,
`render_read_scalar`,`render_scalar_default_assignment` (:728/734, `copy_text` — L2),
`render_scalar_validation` (:756, `set_error` — L2),`render_object_helpers`,
`render_collection_helpers`,`render_defaults`,`render_validate`,`render_get_path`,
`render_set_path`,`render_from_json`,`render_generic_source`): `set_error→gsj_set_error`,
`copy_text→gsj_copy_text`, `object_item→gsj_object_item`, `read_bool→gsj_read_bool`,
`read_int_range→gsj_read_int_range`, `read_float_range→gsj_read_float_range`,
`read_string→gsj_read_string`, `read_enum→gsj_read_enum`, `enum_index→gsj_enum_index`,
`parse_int_value→gsj_parse_int_value`, `parse_enum_value→gsj_parse_enum_value`.
**L2:** после снятия static-блока исполнитель ОБЯЗАН грепнуть весь генерируемый текст на
`copy_text|set_error|read_|parse_|enum_index|object_item` без `gsj_`-префикса — остаток =
промах (компайл-ловится линкером, но грep дешевле цикла сборки).

**`render_generic_source`:**
- Инклюды: `+ "game_state_json.h"`; снять `<errno.h>/<stdio.h>/<direct.h>/<windows.h>/
  <sys/stat.h>` (файлового I/O нет; оставить `<stdlib.h>`/`<string.h>` для malloc/memcpy).
- УДАЛИТЬ static-блок хелперов (1286–1398) и `GAME_STATE_MAYBE_UNUSED`.
- `GameState g_game_state;` → `<Ns.type> <Ns.inst>;`.
- УДАЛИТЬ `s_changed/s_changed_user/s_dirty/notify_changed/game_state_mark_dirty/
  is_dirty/clear_dirty/game_state_init/game_state_set_changed_callback`.
- `patch_json`: снять `notify_changed`/`mark_dirty`, оставить чистый validate-copy-swap.
- `from_json`: снять финальный `notify` (его не было — ок), оставить defaults→read→validate→swap.
- УДАЛИТЬ `make_save_doc/save_json_string/make_dir_if_needed/ensure_parent_dirs/
  replace_file/save/read_file/load_doc/load_json_string/load/reset`.
- ДОБАВИТЬ: обёртки `frag_*`, миграционную таблицу+extern (если есть), extern-хуки
  (если есть), дескриптор `<Ns.frag>` (§A4.3).

**`render_header`:** `#include "game_save.h"`, `#include <stdint.h>`; `GAME_STATE_DOCUMENT`
→ `GAME_STATE_FRAGMENT_ID`; `extern GameState g_game_state` → `extern <Ns.type> <Ns.inst>`;
снять changed-callback/dirty/save/load/reset/init/save_json_string-декларации; добавить
`extern const GameSaveFragment <Ns.frag>;`.

**`render_schema_header` + `game_state_schema_json`:** встраивать НОРМАЛИЗОВАННУЮ схему
(§A4.4), не сырой `schema`-dict.

**`render_devapi_source`:** переписать в transitional одно-фрагментную форму (§A4.2):
роутинг schema/get/set/patch/reset через `<Ns.fn>*(&<Ns.inst>,...)`; save→`game_save_flush`,
load→`game_save_load`; `+ #include "game_save.h"`; снять `#include "game_storage.h"` и
все `game_storage_save_json/load_json/resolve_key`/`unsafe_path`/`key`/`resolved`.
Дескрипторы команд обновить (save/load без key/unsafe_path).

**`render_enum_tables`/`render_enum_name_decls`/`render_enum`/`render_state_struct`/
`render_object_structs`/`render_struct_scalar_field`/collection-helpers:** протянуть `Ns`
(параметризовать префиксы). Форма без изменений (кроме i64-полей).

**`main()`:** `+ --fragment <id>` (опц.): id берётся из `schema.fragment`; при заданном
`--fragment` — assert совпадения. Имена выхлопа = `<id>_state.{h,c}`, `<id>_state_schema.gen.h`,
`<id>_state_devapi.c`. Для `game` → `game_state.{h,c}`/`game_state_schema.gen.h`/
`game_state_devapi.c` (СОВПАДАЕТ с текущими CMake-ожиданиями — правок путей в CMake нет).
Legacy-режима нет.

---

## A4.7 Интеграция: main.c + CMake

### main.c (2 строки + фреш-путь)
- Убрать `game_state_init();` (:371). Инстанс `game_state` — статик-глобал (0-init);
  нормальный путь: `game_save_load` делает reset/from_json каждого фрагмента (A3.4).
- `game_save_register_fragment(&game_fragment)` (:372) → `&game_state_fragment`
  (генерируемый дескриптор). **L3:** `#include "game_state.h"` УЖЕ есть (:53) → символ
  `game_state_fragment` виден из генерируемого заголовка; просто УДАЛИТЬ строку
  `extern const GameSaveFragment game_fragment;` (:54) и перенаправить регистрацию.
  Никакого добавления include/extern не нужно.
- **Фреш-путь:** ветка `s_fresh_state` (:315/374) пропускает load → инстанс остался бы
  0-init. Добавить в неё явный сброс: `if (s_fresh_state) { game_state_fragment.reset(); }`
  (или reset всех зарегистрированных — но фрагмент один). `game_state_register_devapi()`
  (:152) остаётся (transitional devapi несёт его).

### CMakeLists.txt
- Генерация: добавить `--fragment game` в COMMAND (:147–149) — опц., но фиксирует
  контракт. Имена выхлопа не меняются.
- Снять `state/migrations/v0_to_v1.c` из `target_sources` (:163). Снять `game_fragment.c`
  (:162) — файл удалён; дескриптор теперь в генерируемом `game_state.c`.
- Генерируемый `game_state.c` теперь инклюдит `game_save.h`/`game_state_json.h` — `src/`
  уже в include-path (:180), `game_state_json.c` уже линкуется (:159). ОК без правок.
- Roundtrip-ctest (к блоку :266–333, внутри `if(NOT EMSCRIPTEN)`). **L1: обернуть в
  `if(FEATURE_GAME_STATE)`** — переменные `GAME_STATE_GENERATED_*` определены только там
  (при OFF таргет получил бы пустой сорец). Зависимость — от ВЫХЛОПА custom-command
  генерации (`${GAME_STATE_GENERATED_SOURCE}` уже OUTPUT того add_custom_command,
  CMakeLists:140–156, → CMake сам делает генерацию пререквизитом сборки этого TU), а НЕ
  `add_dependencies(... ${GAME_TARGET})` (не тянуть полный движок ради engine-free теста):
```cmake
    if(FEATURE_GAME_STATE)
        add_executable(test_game_state_roundtrip
            tests/test_game_state_roundtrip.c
            "${GAME_STATE_GENERATED_SOURCE}" src/game_state_json.c)
        target_link_libraries(test_game_state_roundtrip PRIVATE cjson unity)
        target_include_directories(test_game_state_roundtrip PRIVATE src "${GAME_STATE_GENERATED_DIR}")
        target_compile_definitions(test_game_state_roundtrip PRIVATE _CRT_SECURE_NO_WARNINGS)
        set_target_properties(test_game_state_roundtrip PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
        add_test(NAME test_game_state_roundtrip COMMAND test_game_state_roundtrip)
    endif()
```
  ЗАМЕЧАНИЕ: тест линкует ГЕНЕРИРУЕМЫЙ `game_state.c` → он инклюдит `game_save.h`
  (только тип `GameSaveFragment`, без тел game_save) — линковать `game_save.c` НЕ надо,
  дескриптор — данные + статик-обёртки. Devapi в тест не входит (тянет `nt_devapi`).
  `${GAME_STATE_GENERATED_SOURCE}` как исходник таргета делает регенерацию его
  автоматическим пререквизитом (движок не тянется).

---

## A4.8 Гейт: golden + compile + roundtrip + old-save (§14 п.13; выхлоп МЕНЯЕТСЯ осознанно)

Выхлоп A4 НЕ байт-идентичен A3 (осознанная смена ABI). Три гейта + old-save-компат:

**(1) Golden (`generate_state_test.py`, расширить):**
- `test_v2_template_golden`: генерить `templates/template/state/game_state.schema.json`
  (v2) в tmp, сравнить 4 файла БАЙТ-в-БАЙТ с `features/game-state/tests/golden/game/*`.
- `test_v2_namespace_golden` (mini): генерить `features/game-state/tests/mini_state.schema.json`
  → сравнить с `tests/golden/mini/*`. **M4: mini НЕ должен быть только скалярным** — иначе
  `render_enum` (:131 хардкодит `GameState<Enum>`/`GAME_STATE_<ENUM>_`), `collection_macro`
  (:437 `GAME_STATE_MAX_`), `object_type_c_name` (:441 `Game<Type>`) не упражнятся, а для
  id=`game` их параметризованный выхлоп совпадает с хардкодом → промах неймспейса
  (кросс-фрагментная коллизия символов) НЕ ловится ни одной фикстурой. Поэтому mini несёт
  enum + map + list + i64 + string. Полный литерал входа (исполнитель пишет ровно это):
```json
{
  "schema": "mini.state",
  "schema_version": 2,
  "fragment": "mini",
  "version": 1,
  "string_max": 32,
  "enums": { "Mode": ["off", "on"] },
  "types": {
    "Cell": { "kind": "object", "fields": {
      "def_id": { "type": "string", "max_length": 15 },
      "count":  { "type": "i64", "default": 0, "min": 0, "max": 9000000000000000000 }
    } }
  },
  "fields": {
    "mode_index": { "type": "enum", "enum": "Mode", "default": "off" },
    "label":      { "type": "string", "default": "hi", "max_length": 15 },
    "total":      { "type": "i64", "default": 0, "min": 0, "max": 9000000000000000000 },
    "cells":      { "type": "map<string,Cell>", "max_count": 8 },
    "order":      { "type": "list<string>", "max_count": 8 }
  }
}
```
  ПРИГВОЖДАЕТ: `mini_state`/`MiniState`/`MINI_STATE_`, `MiniStateMode`/`MINI_STATE_MODE_*`
  (enum), `MINI_STATE_MAX_CELLS`/`MINI_STATE_MAX_ORDER` (collection_macro), `MiniCell`
  (object_type_c_name), `mini_state_fragment` (дескриптор).

**Golden-процедура (L5):** первичный захват — `generate_state.py --schema
  templates/template/state/game_state.schema.json --out-dir features/game-state/tests/golden/game
  --fragment game` и `... --schema features/game-state/tests/mini_state.schema.json --out-dir
  features/game-state/tests/golden/mini --fragment mini`. `schema_label` в комментарии
  выхлопа ЧУВСТВИТЕЛЕН к пути `--schema` → захват из этих точных путей (иначе golden-дифф
  на комментарии). **Первичный захват проходит РЕВЬЮ ГЛАЗАМИ ДО коммита** (процессное
  требование обоих ревью). Осознанное обновление golden (после намеренной смены выхлопа) =
  перегенерация в те же golden-дир теми же командами + повторное ревью глазами; тест —
  только сравнение с закоммиченным эталоном.

**(2) Property (`generate_state_test.py`):** в тексте выхлопа game —
`NOT IN "g_game_state"`, `NOT IN "make_save_doc"`, `NOT IN "game_state_save("`,
`NOT IN "static ... read_int_range"` (static-копий нет); `IN "extern GameState game_state;"`,
`IN "const GameSaveFragment game_state_fragment"`, `IN "gsj_read_int_range"`,
`IN "gsj_add_i64"`/`"gsj_read_i64"` (i64-поле), `IN "#include \"game_state_json.h\""`.

**(3) Validation (`generate_state_test.py`, негативные — `assertRaises(SystemExit)`):**
коллизия c_ident (два `path` → один идент); поле `path=="v"`; i64 `max > INT64_MAX`;
`fragment` с плохим charset; `version != len(migrations)+1`; `reserved`-имя переиспользовано;
дубль `path`; **legacy-форма (M3):** схема БЕЗ `schema_version` → SystemExit с сообщением
про «rebuild from shipping tag»; схема С `document` → тот же SystemExit (не KeyError).

**(4) Compile-гейт:** реген шаблона → сборка `templates/template` (native-debug +
devapi-debug) warning-clean под `-Werror`. devapi-debug доказывает, что transitional
`game_state_devapi.c` компилится с `game_save`/дескриптором (tool parity — hard invariant).

**(5) Roundtrip-ctest (`test_game_state_roundtrip.c`, Unity):**
- `init_defaults → to_json → from_json(→другой инстанс) → to_json`; сравнить строки
  `cJSON_PrintUnformatted` — равны.
- i64: выставить `game_state.wallet_soft = 9_000_000_000_000_000_000LL` → to_json →
  в JSON это СТРОКА (`cJSON_IsString` у `wallet.soft`) → from_json восстанавливает точно
  (никакой double-порчи).
- Толерантность: from_json пустого `{}` → дефолты; from_json с unknown-ключом →
  игнор, остальные читаются; absent-поле → дефолт.
- **get/set-путь i64 + transitional-devapi семантика (L6):** `game_state_get_path_json(
  &game_state, "wallet.soft", ...)` → `cJSON_IsString` (строка, §14 п.8); один цикл
  `set_path_json("wallet.soft", <строка "42">)` → `get_path_json` даёт "42"; плюс
  set числом ≤2^53 принимается, а нечисловой строкой — отвергается. (Покрывает роутинг,
  на который опирается transitional `game.state.get/set`.)
- **Old-save компат (§задача, L4):** толерантный `from_json` не требует всех полей —
  достаточен МИНИМАЛЬНЫЙ A3-эры литерал `{"wallet":{"soft":123}}` (числовой i64!) →
  `game_state_from_json` грузит, `wallet_soft == 123` (`gsj_read_i64` принял число ≤2^53),
  прочие поля = дефолты. Доказывает, что фрагмент ест A3-payload с ЧИСЛОВЫМ i64 после
  конверсии в i64. (Формулировка «все прежние поля» снята — толерантности достаточно.)
- Конвертный old-save (плоский A3-документ `{format,save_version,...,features:{game:{v:1,
  ...}}}`) → грузится `game_save` — покрыт СУЩЕСТВУЮЩИМ `test_game_save` (A3, остаётся
  зелёным; game_save в A4 не менялся). Доп. проверка НЕ требуется, но зафиксировать в
  критериях, что `test_game_save` зелёный после A4.

Плюс **компиляционный+смоук:** devapi-debug сборка → `devapi_smoke` (smoke_bot.py)
ЗЕЛЁНЫЙ без правок бота (§0/§A4.4).

---

## A4.9 Снятие мёртвых хвостов A0

A4 владеет переписыванием миграционной модели → снимает то, что A0 не снял:
`state/migrations/v0_to_v1.c` (инклюдит мёртвый `game_state.h`-монолит, зовёт
`game_state_shape_name` со старой сигнатурой) + `state/fixtures/{v0_save,
v1_from_v0_expected,wrong_document}.json` (тестировали монолит-envelope-путь, который
умер). Снять из CMake (:163) и `feature.json.runtime_sources`. Если A0 уже снял — no-op.

---

## A4.10 Критерии приёмки (бинарные)

- [ ] Генератор v2-only: `--fragment` работает, id из `schema.fragment`; legacy-монолит-
      режима нет; выхлоп для `game` = `game_state.{h,c}`/`game_state_schema.gen.h`/
      `game_state_devapi.c`.
- [ ] Генерируемый `game_state.c` НЕ содержит `g_game_state`, `make_save_doc`,
      `game_state_save/load` (файловый I/O), static-копий хелперов; содержит
      `GameState game_state;`, `const GameSaveFragment game_state_fragment`,
      `#include "game_state_json.h"` + вызовы `gsj_*`.
- [ ] `game_fragment.c` удалён; `main.c` регистрирует `&game_state_fragment`; `--fresh-state`
      сбрасывает фрагмент; сборка native-debug + release + devapi-debug warning-clean под
      `-Werror`+`nt_set_warning_flags`.
- [ ] i64: `wallet.soft/hard` (+`ItemInstance.count`) едут JSON-СТРОКОЙ; значение >2^53
      round-trip'ит точно (roundtrip-ctest); старый числовой A3-payload грузится.
- [ ] Golden game + golden mini совпадают байт-в-байт; property-проверки зелёные;
      негативные validation-тесты фейлятся как ожидается (коллизия/`"v"`/i64-границы/
      charset/version-mismatch).
- [ ] `ctest -R test_game_state_roundtrip` зелёный; `test_game_save` + `test_game_state_json`
      + `test_game_storage` + `test_game_events`(+overflow) остаются зелёными.
- [ ] Схема шаблона переписана в v2 (fragment/schema_version/reserved-имена/fields-мап/
      без id/без devapi/collections); `game.state.schema` несёт `schema`/`document`/
      `fields`-массив; `game.state.get {path:""}` = payload `game` с
      settings/tutorial/inventory.
- [ ] **`devapi_smoke` (smoke_bot.py) ЗЕЛЁНЫЙ БЕЗ правок бота** (Р8-перепись = A5).
- [ ] `v0_to_v1.c` + монолит-фикстуры сняты из дерева/CMake/feature.json.
- [ ] Дескриптор корректно проводит миграции/хуки: при `migrations`/`hooks` в схеме
      генерится таблица/extern + `.steps/.on_new_game/.reconcile` заполнены; при их
      отсутствии — `NULL` (проверить синтетическим тестом-фрагментом с миграцией+хуком:
      компилится + link требует тела).
- [ ] Golden mini упражняет enum/collection/object-неймспейс (`MiniStateMode`/
      `MINI_STATE_MAX_*`/`MiniCell`); legacy-схема (без `schema_version` / с `document`)
      даёт ПОНЯТНЫЙ SystemExit, не KeyError (M3).
- [ ] **Первичный golden-захват (game + mini) прошёл РЕВЬЮ ГЛАЗАМИ до коммита** (L5) —
      процессное требование, не автоматизируется.
- [ ] `game_fragment.c` снят из `feature.json.default_template.runtime_sources` (:49) и
      `INSTALL.md` (copy-list :27, compile-строка :80, register :52) — иначе copy-model
      новой игры сломается на несуществующем файле (M1).

---

## A4.11 rb-dark-rpg: документированный clean-break

rb-dark (`games/rb-dark-rpg`) — ЗАКРЫТАЯ игра, тест пайплайна (SHIPPED 2026-07-06). Факты:
её генерируемые файлы НЕ закоммичены (регенерируются билдом через ОБЩИЙ
`features/game-state/scripts/generate_state.py`, `games/rb-dark-rpg/CMakeLists.txt:314`);
её схема — v1 (`document`, numeric id); её рукописный код (`game_actions.c`,
`game_persistence.c`, `main.c`) потребляет `g_game_state` и монолит-ABI.

Значит v2-only генератор ломает сборку rb-dark из HEAD (и генерацию v1-схемы, и
монолит-ABI её кода). Хак сохранить v1-совместимость противоречил бы всему A4
(«g_game_state умирает»). Резолюция, согласная с репозиторием («Closed prototypes are
git tags», AGENTS.md): **A4 не поддерживает v1-путь rb-dark; это ЗАДОКУМЕНТИРОВАННЫЙ
clean-break. rb-dark не собирается из HEAD; для пересборки — checkout её shipping-тега
(там v1-генератор).** rb-dark файлы A4 НЕ трогает. **РЕШЕНО лидом 2026-07-06: принято
с ГАРДОМ (M3)** — v2-загрузчик схемы при легаси-форме (нет `schema_version` ИЛИ есть
`document`) даёт ПОНЯТНЫЙ `SystemExit` («v1 schema unsupported by v2 generator; rebuild
rb-dark from its shipping tag»), а не KeyError-трейсбек из CMake-шага генерации (§A4.6
контракт загрузчика + негативный тест §A4.8 (3)). Так закрытая игра, случайно собранная
из HEAD, падает с диагностикой, а не с непонятным стеком.

---

## A4.12 Открытые вопросы лиду — НЕТ (оба решены)

Оба кандидата закрыты решениями лида 2026-07-06, внесены как принятые:
1. **DevAPI = 7 команд** (save/load транзитно через шелл; `g_game_state` не доживает,
   алиас отклонён) — tool parity, рекомендация принята (§A4.2).
2. **rb-dark = документированный clean-break С ГАРДОМ (M3)** — генератор v2-only; legacy-
   схема даёт понятный `SystemExit`, не KeyError; пересборка rb-dark через shipping-тег
   (§A4.11). Собираемость rb-dark из HEAD — вне A4.

(Иных расхождений, требующих решения лида, не найдено. §A4.13 — инженерные решения в
рамках дизайна, не вопросы.)

---

## A4.13 Отступления от буквы дизайна (с обоснованием)

1. **Инстанс фрагмента — названный `extern` глобал `<id>_state`, а не приватный static+
   аксессоры.** «Владение уходит из глобала в TU фрагмента» прочитано как «пер-фрагментный
   названный инстанс, определённый в СВОЁМ TU», а не «инстанса-глобала нет». Умирает ОБЩИЙ
   монолит `g_game_state`. Обоснование: мандат «гибко и удобно», ноль рукописных
   потребителей в шаблоне, игры хотят прямой доступ к полям. Инкапсуляцию выбирает игра.
2. **DevAPI save/load транзитно роутятся на `game_save` в A4** (§A4.2; РЕШЕНО лидом:
   7 команд, tool parity) — лёгкая дотяжка к A5 ради 7-командного континуитета §8 без
   воскрешения мёртвого монолит-file-IO. Реестр-диспатч (get ""=агрегат, роутинг по
   голове пути, транзакционный patch) остаётся A5. Побочно (L6): отдельный
   `game.state.set` теперь = `set_path_json` на живом инстансе (без patch copy-swap +
   полного validate) — осознанно, A5 восстанавливает.
3. **Встроенная схема НОРМАЛИЗОВАНА** (fields-список, `document`=fragment, `schema`-id
   сохранён), а не сырой v2-файл (§A4.4). Требование §Р8 «A4 не ломает смоук-бот»: сырое
   встраивание сломало бы бот-проверки `fields`-массив/`document`; бот переписывается в A5.
4. **Хуки (`on_new_game`/`reconcile`) и `migrations` объявляются в СХЕМЕ; генератор
   эмитит дескриптор со ссылками на extern рукописные тела** (§A4.3/§A4.5). Операционализует
   Р9/§7 «дескриптор генерится, логика — руками»: схема = единственный источник истины о
   том, какие логика-хуки существуют. Не буква §5 (список полей), но чистейшая реализация
   §7 «генерится … дескриптор; руками — только логика».
5. **Снятие мёртвого `state/migrations/v0_to_v1.c` + монолит-фикстур свёрнуто в A4**
   (§A4.9). A1–A3 отнесли их «A0/отдельному шагу»; A0 не снял, а A4 владеет переписыванием
   миграционной модели. Если A0 уже снял — no-op.
6. **i64 упражняется в шаблоне конверсией `wallet.soft/hard` (+`ItemInstance.count`) в
   i64** (§A4.5) — даёт golden+roundtrip реальное строковое покрытие >2^53. `gsj_read_i64`
   принимает и старый числовой A3-вид → A3-сейвы грузятся (§A4.8 old-save-гейт), конверт-
   совместимость не рвётся.
7. **Контракт для A6 (L7, форвард-нота, чтобы A6-исполнитель не рукоделил сериализацию):**
   «settings руками» из §12 A6 = ЛОГИКА руками (API фичи + при нужде on_new_game/reconcile/
   миграции) + ГЕНЕРИРУЕМЫЙ стейт-слой (struct/defaults/validate/to_json/from_json/schema/
   дескриптор). Р9 (у ВСЕХ фрагментов схема, руками только логика) главнее буквы §12 «руками»
   (§14 приоритет). settings в A6 = ещё один `--fragment settings` через ЭТОТ генератор, а
   не рукописный `settings_state_*`.

---

## A4.14 Пакет делегирования

**Смешанный: deep-reasoner ведёт ядро генератора, fast-worker добивает механику
(подтверждено ревью, L8).**
- **deep-reasoner** (или лид-ревью реализации Sonnet): переписывание `render_generic_source`/
  `render_devapi_source`/`render_header` под новый ABI + дескриптор + миграционная таблица +
  extern-хуки; загрузчик/валидатор v2 (**M3** legacy-гард, нормализация мап→список, коллизии
  c_ident, i64-границы, charset, version/migrations-консистентность); i64-рендеринг —
  **M2** (get_scalar_expr одним выражением для обоих call-site) и **M5** (validation/default
  i64-ветки), LL-суффиксы, `-Wconversion`-чистота строкового провода. Самые рисковые места:
  (1) параметризация неймспейса (~70 вхождений — промах ловит golden mini M4);
  (2) transitional-devapi-роутинг save/load на шелл + смоук-бот-зелёность (нормализованная
  встроенная схема); (3) i64-касты под `-Werror`/`-Wconversion`/`-Wdouble-promotion`.
- **fast-worker**: v1→v2 transform схемы шаблона (механический), синтетический
  `mini_state.schema.json` (M4-литерал §A4.8), `test_game_state_roundtrip.c` по списку
  кейсов, расширение `generate_state_test.py` (golden/property/validation), main.c/**M1**
  feature.json+INSTALL/**L1** CMake-правки, снятие мёртвых хвостов.
- **Порядок:** golden-захват game+mini — ПОСЛЕ ревью ядра генератора (иначе закрепишь
  баг в эталоне); первичный захват проходит ревью глазами до коммита (L5).
- Приёмка ОБЯЗАНА включать: golden game+mini байт-в-байт, все ctest зелёные, warning-clean
  сборка трёх конфигов, `devapi_smoke` зелёный БЕЗ правок бота.
