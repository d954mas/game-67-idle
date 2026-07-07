# BUILD-SPEC: рукописный DevAPI-диспатч над реестром фрагментов, инкремент A5 (2026-07-07)

Имплементационная спецификация. Переводит принятый дизайн в файлы, сигнатуры и
критерии приёмки. НЕ меняет дизайн, НИЧЕГО не реализует — только спека.

Источник истины (при расхождении — новее побеждает, §14 главнее основного текста):
1. `features/game-state/references/state_system_design_2026-07-06.md` (§8 DevAPI, §1
   роли, §14 п.7 patch, Р8 смоук-бот, Р10 reset).
2. `features/game-state/references/build_spec_a4_2026-07-06.md` (стиль, транзишн-devapi,
   golden-механика, §A4.2 «g_game_state не доживает», §A4.11 rb-dark clean-break).
3. `features/game-state/references/build_spec_a1_a3_2026-07-06.md` (ABI фрагмента, `gsj_*`,
   game_save/game_storage).

Исполнитель может писать код по этому документу, НЕ открывая историю обсуждений.

---

## 0. Предпосылки и рамки (проверено по дереву 2026-07-07)

- **A0–A4 + E1/E2 УЖЕ в дереве.** Живой путь: генератор v2 пер-фрагментный
  (`generate_state.py`), шелл `game_save.{c,h}` (реестр `s_fragments[]`, конверт,
  оркестрация, dirty/debounce, export/import, transform, orphan-retention),
  `game_storage.{c,h}` (atomic+bak native / localStorage web), `game_state_json.{c,h}`
  (`gsj_*`), генерируемые `game_state.{h,c}` + `game_state_schema.gen.h` +
  `game_state_devapi.c` + `game_state_events.gen.{h,c}`.
- **Дескриптор фрагмента `GameSaveFragment` — 11 членов** (game_save.h:16-28):
  `id, version, steps, reset, on_new_game, to_json, from_json, reconcile,
  get_path_json, set_path_json, schema_json`. **ПРОВЕРЕНО: этого хватает для
  универсального диспатча A5.** Диспатч работает ТОЛЬКО над этим vtable — ему НЕ
  нужны пер-полевые метаданные, `game_state_schema.gen.h` в диспатч НЕ инклюдится
  (get/set — через `get_path_json/set_path_json`; агрегат — через `to_json`; схема —
  через `schema_json`). Новых членов дескриптора НЕ требуется (см. §A5.2 «patch»).
- **Транзишн-devapi (A4) живёт** — `render_devapi_source` (generate_state.py:1512-1671)
  эмитит `game_state_devapi.c`: 7 команд `game.state.*`, роутящих через ОДИН фрагмент
  `game_state` (`&game_state` инстанс) + шелл (save/load). Регистрируется
  `game_state_register_devapi()` (main.c:152, под `#if NT_DEVAPI_ENABLED && FEATURE_GAME_STATE`).
  A5 этот файл УБИВАЕТ и заменяет рукописным реестр-диспатчем.
- **КЛЮЧЕВОЙ ФАКТ (проверен grep'ом, правит премиссу ТЗ):** транзишн-devapi (A4) уже
  НЕ зовёт compat-обёртки `game_storage_save_json/load_json/resolve_key`. Их
  ЕДИНСТВЕННЫЙ оставшийся потребитель в шаблоне — тесты
  `templates/template/tests/test_game_storage.c` (`test_compat_wrappers_round_trip`
  :448-467 + две строки в `test_write_rejects_path_traversal` :225-227). Комментарии в
  `game_storage.{h,c}` («их зовёт генерируемый devapi») УСТАРЕЛИ post-A4. Значит удаление
  обёрток в A5 требует правки ТЕСТА, а не devapi (§A5.7). `games/rb-dark-rpg/**` несёт
  СВОЙ `game_storage.c` (не потребляет шаблонные обёртки — clean-break, §A4.11; A5 его не трогает).
- **Реестр — приватный static в game_save.c** (`s_fragments[]`/`s_fragment_count`, :58-59).
  Публичного аксессора для итерации/поиска НЕТ (есть приватный `is_registered`, :120).
  Универсальный диспатч ОБЯЗАН читать реестр → A5 добавляет минимальный read-API в
  шелл (§A5.3). Это ЕДИНСТВЕННОЕ расширение «замороженного» шелла — аддитивное,
  безповеденческое, прямо в мандате A5 (§8 «диспатч по реестру … регистрирует шелл»).
- **Смоук-бот** (`templates/template/devapi/smoke_bot.py`) зовёт `game.state.schema` и
  `game.state.get {path:""}`; жёстко проверяет старую ПЛОСКУЮ форму (`schema.schema/
  document/fields`; `value.settings/tutorial/inventory`). Р8: переписывается на
  фрагментные ключи (§A5.8).
- **Один тред. Сборка** game-таргета: `-Werror` + `nt_set_warning_flags` (`-Wall -Wextra
  -Wpedantic -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -Wundef`). u64 hex —
  `PRIx64`/`<inttypes.h>`. MSVC ABI: `_Alignof(max_align_t)==8`.
- **Среда:** движок `external/neotolis-engine` READ-ONLY (public API `devapi/nt_devapi.h`,
  `cJSON.h`); clangd-диагностика = шум (истина — ninja); сендбокс запрещает `rm -rf`
  (удаление файлов — `cmake -E rm` / `git rm` / правка на месте).

### Что A5 делает (ровно)

1. Пишет рукописный универсальный DevAPI-диспатч **`templates/template/src/game_save_devapi.c`**
   над реестром фрагментов + шеллом (§A5.4): 7 команд `game.state.*`, `get ""`=агрегат
   features, роутинг по голове пути, пофрагментно-атомарный patch (§14 п.7), save/load/reset→шелл.
2. Добавляет в шелл минимальный read-API реестра + `game_save_register_devapi()`
   декларацию (§A5.3).
3. Убивает эмит `game_state_devapi.c` в генераторе: снимает `render_devapi_source`,
   `devapi_source_path`, devapi-декларацию из `render_header` (§A5.5).
4. Удаляет golden `*_state_devapi.c` (game+mini), правит `OUTPUT_SUFFIXES`, ре-захватывает
   `*_state.h` golden (теряет 3-строчный devapi-блок), правит CMake custom command OUTPUT
   (§A5.6).
5. Удаляет compat-обёртки A2 из `game_storage.{h,c}` + их тесты (§A5.7).
6. Переписывает смоук-бот на фрагментные ключи по Р8 (§A5.8).
7. Правит интеграцию: `main.c` (1 строка), CMake target_sources, `feature.json`,
   `INSTALL.md` (§A5.9).

**НЕ входит (границы):** реальные фрагменты settings/items/progression + фикстуры +
reconcile — A6; КРОСС-фрагментный транзакционный откат patch (§8 смягчён §14 п.7 — patch
пофрагментно-атомарен, но не кросс-фрагментно) — не строится; события/editor-схема — вне
A5; изменение оркестрации load/save шелла — не трогается.

### FROZEN в A5 (не менять, кроме явно перечисленного)

- `templates/template/src/game_state_json.{c,h}` — целиком.
- `templates/template/src/game_save.c` — ТОЛЬКО добавить 3 аксессора реестра (§A5.3),
  оркестрация load/save/dirty/orphan — байт-стабильна.
- `templates/template/src/game_storage.{c,h}` — ТОЛЬКО удалить compat-секцию (§A5.7),
  ядро (write/read/exists/backup/quarantine/probe) — байт-стабильно.
- Генерируемые `game_state.c`, `*_schema.gen.h`, `*_events.gen.{h,c}` — golden байт-в-байт
  (гейт, §A5.6).
- `game_events.{c,h}`, `game_features.*` (E1/E2), движок, `games/rb-dark-rpg/**`.

---

## A5.1 Файлы

**Новые:**
- `templates/template/src/game_save_devapi.c` — рукописный реестр-диспатч (§A5.4).
  Компилируется ТОЛЬКО под `GAME_DEVAPI_ENABLED` (как раньше генерируемый devapi).

**Изменяемые:**
- `templates/template/src/game_save.h` — +3 read-аксессора реестра, +декларация
  `game_save_register_devapi()` под `#if NT_DEVAPI_ENABLED` (§A5.3).
- `templates/template/src/game_save.c` — +3 тела аксессоров (§A5.3). Больше ничего.
- `templates/template/src/game_storage.h` — снять compat-секцию :44-53 (§A5.7).
- `templates/template/src/game_storage.c` — снять compat-секцию :506-564 (§A5.7).
- `templates/template/tests/test_game_storage.c` — снять `test_compat_wrappers_round_trip`
  (:445-467) + его `RUN_TEST` (:494) + две compat-строки в `test_write_rejects_path_traversal`
  (:225-227, оставить сам write-тест) (§A5.7).
- `templates/template/src/main.c` — :152 `game_state_register_devapi();` →
  `game_save_register_devapi();` (§A5.9).
- `templates/template/CMakeLists.txt` — снять `GAME_STATE_GENERATED_DEVAPI` из
  `set()` (:139) и custom command OUTPUT (:147); target_sources под GAME_DEVAPI_ENABLED
  (:179) `${GAME_STATE_GENERATED_DEVAPI}` → `src/game_save_devapi.c` (§A5.9).
- `templates/template/devapi/smoke_bot.py` — валидаторы под фрагментные ключи (§A5.8).
- `templates/template/devapi/smoke_bot_test.py` — FakeGame-стабы + валидатор-тесты под
  фрагментную форму (§A5.8 HIGH-1).
- `features/game-state/scripts/generate_state.py` — снять devapi-эмит + devapi-декларацию
  header (§A5.5).
- `features/game-state/scripts/generate_state_test.py` — `OUTPUT_SUFFIXES` минус
  `_devapi.c`; снять/поправить devapi-property-проверки (§A5.6).
- `features/game-state/feature.json` — `outputs` минус `game_state_devapi.c` (:36);
  `default_template.runtime_sources` + `src/game_save_devapi.c` (§A5.9).
- `features/game-state/INSTALL.md` — полный список правок в §A5.9 (LOW-8).
- **grep-свип доков** на `game_state_register_devapi`/`_state_devapi`: `features/game-state/README.md`,
  `features/game-state/references/contract.md` (это manuals-ссылка `feature.json`!),
  `templates/TEMPLATE.md` — обновить любые упоминания под реестр-диспатч (§A5.9).

**Удаляемые (через `git rm` / `cmake -E rm`, НЕ `rm -rf`):**
- `features/game-state/tests/golden/game/game_state_devapi.c`.
- `features/game-state/tests/golden/mini/mini_state_devapi.c`.

**Ре-захватываемые golden (дифф ревьюится глазами = ровно снятие devapi-блока, §A5.6):**
- `features/game-state/tests/golden/game/game_state.h`.
- `features/game-state/tests/golden/mini/mini_state.h`.

---

## A5.2 Ключевые решения

### Дом диспатча — НОВЫЙ рукописный TU `game_save_devapi.c` (не game_save.c)
Диспатч живёт отдельным файлом в `templates/template/src/`, компилируемым только под
`GAME_DEVAPI_ENABLED`, ТОЧНО как раньше генерируемый `game_state_devapi.c`. Причина
(vs. «встроить в game_save.c»): (1) сохраняет модель компиляции — native-debug/release
НЕ тянут `devapi/nt_devapi.h` и devapi-код; (2) держит core-шелл `game_save.c`
развязанным с движковым devapi; (3) минимальный диф в «замороженном» game_save.c (только
3 pure-getter). §1 «game_save владеет DevAPI-диспатчем» удовлетворён: диспатч —
шелл-код (рукописный, в шаблоне, читает шелл-реестр), просто в своём TU.

### Read-API реестра — минимальные 3 pure-getter в шелле (§A5.3)
`game_save_fragment_count/at/find` — единственное расширение game_save. Аддитивно,
безповеденческое (константное чтение static-массива), unconditional (не под devapi —
безвредно). Отвергнуто: «шелл-функция снапшота `game_save_snapshot()`» — добавляла бы
НОВУЮ семантику в шелл; get-by-path всё равно требует `find`, а агрегат тривиально
строится из `count/at` в TU диспатча. Отвергнуто: сделать `s_fragments` extern — течёт
инкапсуляция шелла.

### 7 команд СОХРАНЕНЫ (жёсткое решение лида — tool parity, hard invariant)
`game.state.{schema,get,set,patch,save,load,reset}` — те же имена, та же группа `"game"`.
Никакого сокращения поверхности. Смоук-бот `REQUIRED_METHODS` (`game.state.schema`,
`game.state.get`) остаётся валиден — имена не меняются, меняется только ФОРМА ответа
`get ""`/`schema` (агрегат) → бот переписывается (§A5.8).

### `doc`-параметр СНЯТ (чистая новая схема, Р8)
Транзишн-devapi проверял `params.doc=="game"` (артефакт одного фрагмента). В реестр-модели
селектор — ГОЛОВА пути (`path`), не `doc`. Команды get/set/patch/schema больше не читают
`doc`. Смоук-бот `doc` не шлёт → безопасно. (Р8: «без слоёв совместимости и фолбеков».)

### patch — ПОФРАГМЕНТНО АТОМАРЕН, без кросс-фрагментного отката (§14 п.7 уточняет §8)
§8 говорил «patch транзакционно через gsj_transact»; §14 п.7 смягчил ТОЛЬКО кросс-фрагментную
часть: «patch атомарен ПОФРАГМЕНТНО, результат по-ключево; кросс-фрагментного отката нет
(боты в основном читают — приемлемо)». Пофрагментную атомарность НЕЛЬЗЯ ронять (это был пол
A4-реализации, validate-copy-swap). Реализация (§MED-2, код в §A5.4): группировать `values`
по владельцу; на фрагмент — снапшот `frag->to_json` ДО применения группы → применить все
ключи через `frag->set_path_json` → при любом фейле откатить группу `frag->from_json(snapshot)`.
gsj_transact НЕ нужен; дескриптор НЕ расширяется 12-м членом (`patch_json` НЕ выносится в
vtable) — 11 членов достаточно. Слабее только кросс-фрагментная атомарность — принято §14 п.7.

### reset → `game_save_new_game()` (Р10, семантический апгрейд от транзишна)
Транзишн-reset = `init_defaults` одного инстанса (нейтральные пустые, без on_new_game,
без save). A5-reset (§8 «reset → шелл») = `game_save_new_game()`: reset всех → on_new_game
всех (стартовый контент) → save → возобновление автосейва. Это ПРАВИЛЬНАЯ семантика «новой
игры» для бота и матчит Р10. Флаг: осознанное усиление (см. §A5.15).

### Агрегат `get ""` — только ЗАРЕГИСТРИРОВАННЫЕ фрагменты, `to_json` без `"v"`
`value = { <frag.id>: frag->to_json(), ... }` по `count/at`. НЕ штампует `"v"` (это
read-view, не запись — §8 «снапшот = save-путь МИНУС запись»). Orphan-блобы (retained
незнакомые ключи) в DevAPI-агрегат НЕ включаются (у них нет хендлера; сырой блоб = шум
для ботов) — ОСОЗНАННОЕ расхождение с §8 «агрегат = features-map», ждёт подтверждения
лида, дешёвый реверс: см. Q1 (§A5.14).

---

## A5.3 Расширение шелла: read-API реестра + devapi-декларация

### `game_save.h` (добавить после `game_save_register_fragment`, :32)

```c
/* ---- Registry read-access for the DevAPI dispatch (§8 «диспатч по реестру»).
   Read-only view of the registry filled by game_save_register_fragment. ---- */
int  game_save_fragment_count(void);                 /* число зарегистрированных */
const GameSaveFragment *game_save_fragment_at(int index);        /* NULL если вне диапазона */
const GameSaveFragment *game_save_find_fragment(const char *id); /* NULL если ключ неизвестен */

#if NT_DEVAPI_ENABLED
/* Регистрирует 7 команд game.state.* над реестром фрагментов (A5; заменяет
   генерируемый <id>_state_register_devapi). Хендлеры читают реестр ЛЕНИВО в момент
   ВЫЗОВА команды (бот подключается в кадровом цикле, много позже init), поэтому
   порядок регистрации команд относительно game_save_register_fragment НЕ важен —
   сохранённая точка вызова (main.c:152, внутри devapi_start()) корректна как есть.
   Звать один раз после nt_devapi_init(). Тело —
   templates/template/src/game_save_devapi.c. */
void game_save_register_devapi(void);
#endif
```

### `game_save.c` (добавить в блок public API, рядом с `game_save_register_fragment`)

```c
int game_save_fragment_count(void) { return s_fragment_count; }

const GameSaveFragment *game_save_fragment_at(int index) {
    return (index >= 0 && index < s_fragment_count) ? s_fragments[index] : NULL;
}

const GameSaveFragment *game_save_find_fragment(const char *id) {
    if (!id) { return NULL; }
    for (int i = 0; i < s_fragment_count; i++) {
        if (strcmp(s_fragments[i]->id, id) == 0) { return s_fragments[i]; }
    }
    return NULL;
}
```

Ничего больше в game_save.c не меняется. (Опц. рефактор: `is_registered` может звать
`game_save_find_fragment(id) != NULL` — НЕ обязателен, оставить как есть, чтобы диф был
чисто аддитивным и golden-нейтральным по поведению.)

---

## A5.4 `game_save_devapi.c` — контракт (полная форма)

Файл начинается с `#if NT_DEVAPI_ENABLED` … `#endif` (весь TU под гардом — при OFF
пустой объектник). Инклюды: `<stdio.h>`, `<string.h>`, `"devapi/nt_devapi.h"`,
`"game_save.h"`, `"cJSON.h"`. НЕ инклюдит `game_state.h`/`*_schema.gen.h` (диспатч
универсален над vtable). Один static-буфер ошибки `s_state_err[256]` (dev-only,
single-thread) + хелперы `state_fail/state_fail_buf/state_emit` — переносятся 1:1 из
транзишн-devapi (golden game_state_devapi.c:18-49; они уже универсальны, без имён фрагмента).

### Хелпер роутинга пути

```c
/* Разбивает "head.sub.sub" -> фрагмент по голове + остаток sub ("" если голова = весь
   путь). head_buf — локальный буфер вызывающего. Возвращает NULL если фрагмент не найден
   (вызывающий эмитит "unknown fragment"). */
static const GameSaveFragment *route_path(const char *path, char *head_buf, size_t head_cap,
                                          const char **out_sub) {
    const char *dot = strchr(path, '.');
    size_t head_len = dot ? (size_t)(dot - path) : strlen(path);
    if (head_len >= head_cap) { return NULL; }             /* защита буфера */
    memcpy(head_buf, path, head_len);
    head_buf[head_len] = '\0';
    *out_sub = dot ? dot + 1 : "";                          /* "" = весь фрагмент */
    return game_save_find_fragment(head_buf);
}
```

### Хелпер агрегата (get "" / load value)

```c
/* { <frag.id>: frag->to_json() } по зарегистрированным фрагментам; БЕЗ "v". */
static cJSON *build_aggregate(void) {
    cJSON *agg = cJSON_CreateObject();
    if (!agg) { return NULL; }
    const int n = game_save_fragment_count();
    for (int i = 0; i < n; i++) {
        const GameSaveFragment *f = game_save_fragment_at(i);
        cJSON *payload = f->to_json ? f->to_json() : NULL;
        if (!payload) { payload = cJSON_CreateObject(); }
        cJSON_AddItemToObject(agg, f->id, payload);
    }
    return agg;
}
```

### Замороженный набор `error.code` (tool parity на канале ошибок — MED-3, hard invariant)

A4-devapi эмитит РОВНО два `err->code`: **`"bad_params"`** (кривые параметры/пути/
неизвестный фрагмент/read-only/set-без-sub/фейл set_path) и **`"internal"`** (фейл
save/load/reset/сборки json). A5 СОХРАНЯЕТ этот набор байт-в-байт. Все новые описательные
строки (`"unknown fragment"`, `"read-only fragment"`, `"set requires a sub-path"`,
`"fragment does not support path reads"`) идут ТОЛЬКО в `err->message` (через
`state_fail(err,"bad_params",<message>)` или `state_fail_buf` когда message уже в
`s_state_err`), НИКОГДА в `code`. Новых кодов не вводить — боты/тулинг матчат `code`, его
набор = замороженный контракт tool parity.

### 7 хендлеров (форма и ответ) — код при ошибке всегда `bad_params`/`internal` (§MED-3)

| Команда | Логика | Ответ (result_obj) |
|---|---|---|
| `game.state.schema` | по всем фрагментам: если `f->schema_json` → добавить `f->schema_json()` под `f->id` | `{ "<id>": <нормализованная схема>, ... }` |
| `game.state.get {path}` | `path==""` → `{path:"", value:build_aggregate()}`. Иначе `route_path`; `!frag`→`bad_params`("unknown fragment"); `sub==""` → `value=frag->to_json()`; иначе `!frag->get_path_json`→`bad_params`("fragment does not support path reads"); `value=frag->get_path_json(sub,s_state_err)`; `!value`→`bad_params` (message уже в s_state_err) | `{ "path":<path>, "value":<json> }` |
| `game.state.set {path,value}` | require `path`(str,непустой)+`value`→иначе `bad_params`; `route_path`; `!frag`→`bad_params`("unknown fragment"); `sub==""`→`bad_params`("set requires a sub-path"); `!frag->set_path_json`→`bad_params`("read-only fragment"); `frag->set_path_json(sub,value,s_state_err)`→фейл `bad_params`; `mark_dirty()` | `{ "path":<path>, "value":<эхо> }` — эхо ТОЛЬКО `cJSON_Duplicate(frag->get_path_json(sub) ?? входной value, true)` (§MED-4) |
| `game.state.patch {values}` | require `values`(object)→иначе `bad_params`; пофрагментная атомарность (код ниже, §MED-2) | `{ "results": { "<path>": true|false, ... } }` |
| `game.state.save {}` | `game_save_flush(s_state_err)`; `!ok`→`internal` | `{ "saved": true }` |
| `game.state.load {}` | `game_save_load(&r)`; ответ = статус-строка + свежий агрегат | `{ "status":"<строка>", "value":build_aggregate() }` |
| `game.state.reset {}` | `game_save_new_game(s_state_err)`; `!ok`→`internal` (Р10: reset всех + on_new_game + save + возобновление автосейва) | `{ "reset": true }` |

### patch — ПОФРАГМЕНТНАЯ атомарность (MED-2; НЕ ниже пола дизайна)

§14 п.7 отменил ТОЛЬКО кросс-фрагментную транзакционность; пофрагментную атомарность
дизайн СОХРАНЯЕТ («patch атомарен пофрагментно»), и A4 давал validate-copy-swap. A5 держит
этот пол на 11-членном vtable (12-й член НЕ нужен): группируем `values` по владеющему
фрагменту, снапшотим фрагмент до применения его группы, при любом фейле ключа группы —
откат ВСЕЙ группы через `frag->from_json(snapshot)`.

```c
static bool ep_state_patch(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
    const cJSON *values = cJSON_GetObjectItemCaseSensitive(params, "values");
    if (!cJSON_IsObject(values)) { return state_fail(err, "bad_params", "values object is required"); }
    cJSON *results = cJSON_AddObjectToObject(result_obj, "results");
    bool any_ok = false;
    const int n = game_save_fragment_count();
    for (int i = 0; i < n; i++) {                    /* внешний цикл = фрагмент => группа атомарна */
        const GameSaveFragment *f = game_save_fragment_at(i);
        bool group_has = false, group_ok = true;
        cJSON *snapshot = NULL;
        for (const cJSON *m = values->child; m; m = m->next) {
            char head[64]; const char *sub;
            if (route_path(m->string, head, sizeof head, &sub) != f) { continue; }
            if (!group_has) {                        /* первый ключ группы: снапшот ДО мутации */
                group_has = true;
                snapshot = f->to_json ? f->to_json() : NULL;
            }
            if (sub[0] == '\0' || !f->set_path_json ||
                !f->set_path_json(sub, m, s_state_err, (int)sizeof s_state_err)) {
                group_ok = false; break;             /* фейл ключа => откат всей группы */
            }
        }
        if (group_has && !group_ok && snapshot && f->from_json) {
            (void)f->from_json(snapshot, s_state_err, (int)sizeof s_state_err); /* restore */
        }
        cJSON_Delete(snapshot);                      /* снапшот всегда освобождается */
        if (group_has) {                             /* per-key результат: вся группа = group_ok */
            for (const cJSON *m = values->child; m; m = m->next) {
                char head[64]; const char *sub;
                if (route_path(m->string, head, sizeof head, &sub) != f) { continue; }
                cJSON_AddBoolToObject(results, m->string, group_ok);
            }
            any_ok = any_ok || group_ok;
        }
    }
    for (const cJSON *m = values->child; m; m = m->next) {   /* неизвестная голова => false */
        if (!cJSON_GetObjectItemCaseSensitive(results, m->string)) {
            cJSON_AddBoolToObject(results, m->string, false);
        }
    }
    if (any_ok) { game_save_mark_dirty(); }
    return true;                                     /* patch сам не фейлит канал; результат по-ключево */
}
```

O(фрагменты × ключи) — дёшево. Пофрагментно атомарно (снапшот/откат), кросс-фрагментного
отката нет (§14 п.7). Read-only фрагмент (`!set_path_json`) → группа фейлит на первом ключе,
откат в no-op (снапшот == текущее) → все ключи группы `false`, канал ошибок не трогается
(patch возвращает `true`).

Хелпер статус→строка для load (map по `game_save_load_status_t`):
`FRESH→"fresh", LOADED→"loaded", RECOVERED_BAK→"recovered_bak", CORRUPT_RESET→"corrupt_reset",
NEWER→"newer"`.

### Регистрация (описания команд — обновить под реестр-модель, снять `doc`)

```c
void game_save_register_devapi(void) {
    static const nt_devapi_command_desc descs[] = {
        {"game.state.schema","game","Return per-fragment state schemas.","none","{<fragment>: schema}","immediate","none"},
        {"game.state.get","game","Get state by path (\"\"=all fragments).","path","path, value","immediate","none"},
        {"game.state.set","game","Set a state value by path (<fragment>.<sub>).","path, value","path, value","immediate","mutates state"},
        {"game.state.patch","game","Patch multiple paths.","values","results","immediate","mutates state"},
        {"game.state.save","game","Flush state to storage.","none","saved","immediate","writes file"},
        {"game.state.load","game","Reload state from storage.","none","status, value","immediate","mutates state"},
        {"game.state.reset","game","New game: reset all fragments + starting content.","none","reset","immediate","mutates state"},
    };
    const nt_devapi_handler_fn fns[] = {
        ep_state_schema, ep_state_get, ep_state_set, ep_state_patch,
        ep_state_save, ep_state_load, ep_state_reset,
    };
    for (size_t i = 0; i < sizeof(fns)/sizeof(fns[0]); ++i) {
        (void)nt_devapi_register(&descs[i], fns[i], NULL);
    }
}
```

Warning-заметки (-Wconversion/-Wshadow): `head_len` — `size_t`, касты явные;
`s_state_err` пишется `snprintf`, `(int)sizeof(...)` при передаче в `*_json`-хелперы;
никаких shadow с `path`/`value`; в `state_emit` детач-транспланты — как в golden.

**Ownership (MED-4, единственный дешёвый гейт — ручной ASan/leak-прогон, §A5.11):** любой
cJSON от `frag->to_json/get_path_json/schema_json` на КАЖДОЙ ветке (включая error) ЛИБО
вставляется (`AddItemToObject`/`state_emit`), ЛИБО удаляется (`cJSON_Delete`) — не течёт.
Входной `value = cJSON_GetObjectItemCaseSensitive(params,"value")` ПРИНАДЛЕЖИТ `params`:
его НЕЛЬЗЯ отдавать в `AddItemToObject(result_obj,"value",value)` (движок удалит `result_obj`
→ double-free при удалении `params`). Set-эхо кладёт ТОЛЬКО `cJSON_Duplicate(value, true)`
(или Duplicate результата `get_path_json`). Патч-снапшот всегда `cJSON_Delete` (см. код).

---

## A5.5 Генератор: убить эмит devapi + devapi-декларацию header

`features/game-state/scripts/generate_state.py`:
- **`render_header` (:697-701, :745):** удалить переменную `devapi_block` (:697-701) И её
  подстановку — строку-плейсхолдер `{devapi_block}` (:745) целиком, ВМЕСТЕ с её `\n`, чтобы
  хвост шаблона стал `...{NS.frag};\n\n#endif` (одна пустая строка перед include-гардом), а
  НЕ `\n\n\n#endif`. Заголовок больше НЕ объявляет `<fn>register_devapi`. Результат в
  golden .h — чистое удаление 4 строк (3 строки devapi-блока + строка плейсхолдера/пустая),
  см. §A5.6. Инклюд `"game_save.h"` (:712) ОСТАЁТСЯ.
- **`render_devapi_source` (:1512-1671):** удалить функцию целиком.
- **`main()` (:1986, :1998-1999):** удалить `devapi_source_path = …` и блок
  `if write_if_changed(devapi_source_path, render_devapi_source(...)): changed.append(...)`.
- `render_source`/`render_generic_source` (game_state.c) — НЕ трогать (golden .c байт-в-байт).
- Инклюд `"game_save.h"` в header (:712) ОСТАЁТСЯ (нужен для `GameSaveFragment`).

Итог: генератор эмитит 5 файлов на фрагмент (`.h`, `.c`, `_schema.gen.h`,
`_events.gen.h`, `_events.gen.c`) — devapi-путь мёртв.

---

## A5.6 Golden-гейт: что байт-идентично, что меняется

**Осознанная НЕ-полная байт-идентичность.** Убийство devapi-эмита затрагивает и .c-файл
(удаляется), и .h-файл (теряет 3-строчный блок). Точно:

- **БАЙТ-В-БАЙТ (гейт, `git diff` пуст):** `golden/game/game_state.c`,
  `golden/game/game_state_schema.gen.h`, `golden/game/game_state_events.gen.{h,c}`;
  те же для `golden/mini/mini_state.*`.
- **УДАЛЯЮТСЯ:** `golden/game/game_state_devapi.c`, `golden/mini/mini_state_devapi.c`.
- **МЕНЯЮТСЯ ровно на снятие devapi-блока (ре-захват + ревью глазами):**
  `golden/game/game_state.h`, `golden/mini/mini_state.h`. Ожидаемый диф каждого — удаление
  ТОЛЬКО:
  ```
  #if NT_DEVAPI_ENABLED
  void <id>_state_register_devapi(void);
  #endif
  ```
  плюс одна прилегающая пустая строка (плейсхолдер, §A5.5 LOW-6) → в сумме чистое удаление
  4 строк. НИКАКИХ иных изменений в .h. Если диф шире — промах в `render_header`, чинить до
  коммита.

**Процедура ре-захвата (как §A4.8 L5):** после правки генератора —
`generate_state.py --schema templates/template/state/game_state.schema.json --out-dir
features/game-state/tests/golden/game --fragment game` и `… --schema
features/game-state/tests/mini_state.schema.json --out-dir
features/game-state/tests/golden/mini --fragment mini`; удалить оставшиеся
`*_state_devapi.c` в этих дирах; проверить `git diff` = ровно (.h: снятие блока; .c-devapi:
удаление файла; всё прочее — 0). Ревью глазами до коммита.

`generate_state_test.py`:
- **`OUTPUT_SUFFIXES` (:15) — ЕДИНСТВЕННАЯ devapi-ссылка в тесте (проверено):** удалить
  `"_devapi.c"` → `(".h", ".c", "_schema.gen.h", "_events.gen.h", "_events.gen.c")`. Это
  каскадно чинит `read_outputs`, `test_v2_template_golden`, `test_v2_namespace_golden`,
  `test_clean_schema_generates_all_outputs` (:54-59 больше не ждёт devapi-выход). Никаких
  devapi-ассертов в `test_property_game_output`/`test_property_mini_events` НЕТ — снимать
  там нечего (ревью подтвердило). Позитивные property (`g_game_state` NOT IN,
  `game_state_fragment` IN, `gsj_*` IN) — оставить.

---

## A5.7 Удаление compat-обёрток A2

**Мотив (правит премиссу ТЗ):** обёртки уже НЕ зовёт devapi (A4). Реальный потребитель —
тесты. Удаляем обёртки + тесты вместе.

- **`game_storage.h`:** удалить блок :44-53 (комментарий «Compat-обёртки до A5» +
  декларации `game_storage_resolve_key/save_json/load_json`).
- **`game_storage.c`:** удалить блок :506-564 (комментарий + `compat_fold_slot` +
  три функции). Проверить, что `is_safe_segment`/`resolve_web_key`/`resolve_native_paths`
  остаются используемыми public-API (`write/read/exists/...`) — да, остаются. Никаких
  «osиротевших static» (-Wunused-function под -Werror): `compat_fold_slot` уходит вместе
  с потребителями. **L2-грепом** убедиться, что после удаления в game_storage.c нет
  оставшихся ссылок на удалённые символы.
- **`test_game_storage.c`:**
  - Снять `test_compat_wrappers_round_trip` (:445-467) и его `RUN_TEST(...)` (:494).
  - В `test_write_rejects_path_traversal` снять две compat-строки (:225-227, вызов
    `game_storage_save_json` + ассерт), оставив `game_storage_write`-проверку (:222-223) и
    сам тест в `RUN_TEST` (:477). Поправить комментарий (:210-213: убрать «and the
    save_json compat wrapper»).
  - Проверить, что после правок в файле НЕ осталось ссылок на
    `game_storage_save_json/load_json/resolve_key` (grep).

Гейт: `test_game_storage` компилится и зелёный без обёрток.

---

## A5.8 Смоук-бот на фрагментные ключи (Р8)

`templates/template/devapi/smoke_bot.py`. Имена команд/`REQUIRED_METHODS` НЕ меняются
(`game.state.schema`, `game.state.get` — валидны). Меняется ФОРМА ответа → валидаторы:

- **`validate_game_state_schema` (:104-113):** ответ теперь агрегат
  `{ "game": <нормализованная схема>, ... }`. Проверять:
  ```python
  frag = schema.get("game")
  if not isinstance(frag, dict): raise DevApiError("game.state.schema missing 'game' fragment")
  if frag.get("schema") != "game_seed.state": raise DevApiError(...)
  if frag.get("fragment") != "game" and frag.get("document") != "game": raise DevApiError(...)
  if not isinstance(frag.get("fields"), list): raise DevApiError("game.state.schema 'game' missing fields array")
  ```
  (Нормализованная схема A4 несёт И `fragment`, И `document` — принять любой; §A4.4.)
- **`validate_game_state` (:116-127):** `get {path:""}` теперь `{path:"", value:{game:{...}}}`.
  Проверять:
  ```python
  if state.get("path") != "": raise ...
  value = state.get("value")
  if not isinstance(value, dict): raise ...
  frag = value.get("game")
  if not isinstance(frag, dict): raise DevApiError("game.state.get missing value.game fragment")
  for key in ("settings", "tutorial", "inventory"):
      if not isinstance(frag.get(key), dict): raise DevApiError(f"game.state.get missing value.game.{key} object")
  ```
- Остальной бот (endpoints/ui/render/capture) — без изменений. `summary` schema-строку
  можно бампнуть `"template.devapi_smoke.v2"` (опц., для трассировки; не гейт).

### `smoke_bot_test.py` — ОБЯЗАТЕЛЬНАЯ синхронная правка (HIGH-1, не ctest → не ловится гейтом «все ctest зелёные»)

`templates/template/devapi/smoke_bot_test.py` (ref в INSTALL.md:191) юнит-тестит валидаторы
против FakeGame-стабов, возвращающих СТАРУЮ плоскую форму. После §A5.8 три теста падают.
Правки (все в этом файле):
- **FakeGame-стабы (:31-41):**
  - `game.state.schema` (:32): `{"schema": "game_seed.state", "document": "game", "fields": []}`
    → `{"game": {"schema": "game_seed.state", "document": "game", "fields": []}}`.
  - `game.state.get` (:33-41): `"value": {"settings":..., "tutorial":..., "inventory":...}`
    → `"value": {"game": {"settings":..., "tutorial":..., "inventory":...}}`.
- **`test_validate_game_state_requires_template_snapshot_shape` (:75-79):** позитив
  `{"path":"","value":{"settings":{},"tutorial":{},"inventory":{}}}` →
  `{"path":"","value":{"game":{"settings":{},"tutorial":{},"inventory":{}}}}`; негатив
  `{"path":"","value":{"settings":{}}}` → `{"path":"","value":{"game":{"settings":{}}}}`
  (нет tutorial/inventory под game → всё ещё `DevApiError`).
- **`test_validate_game_state_schema_requires_template_schema` (:81-85):** позитив
  `{"schema":"game_seed.state","document":"game","fields":[]}` →
  `{"game":{"schema":"game_seed.state","document":"game","fields":[]}}`; негатив
  `{"schema":"wrong"}` → `{}` (нет ключа `game` → `DevApiError`).
- **`test_run_smoke_toggles_render_and_writes_summary` (:98-108):** ассерт :104
  `summary["game_state_schema"]["schema"] == "game_seed.state"` →
  `summary["game_state_schema"]["game"]["schema"] == "game_seed.state"`.

Гейт: `py -3.12 templates/template/devapi/smoke_bot_test.py` зелёный (offline, без сборки) И
`devapi_smoke` (ctest-таргет, CMake:245) зелёный на devapi-debug сборке (живой рантайм).

---

## A5.9 Интеграция: main.c + CMake + feature.json + INSTALL.md

### main.c (1 строка)
- :152 `game_state_register_devapi();` → `game_save_register_devapi();`. Остаётся под
  `#if FEATURE_GAME_STATE` внутри `devapi_start()` (уже под `#if NT_DEVAPI_ENABLED`).
  `#include "game_save.h"` (:52) уже есть → декларация видна. Регистрация фрагмента
  (:374 `game_save_register_fragment(&game_state_fragment)`) и `#include "game_state.h"`
  (:53) — БЕЗ изменений (фрагмент по-прежнему нужен).

### CMakeLists.txt
- Снять строку `set(GAME_STATE_GENERATED_DEVAPI …)` (:139).
- Снять `"${GAME_STATE_GENERATED_DEVAPI}"` из OUTPUT custom-command (:147).
- target_sources под `if(GAME_DEVAPI_ENABLED)` (:178-180):
  `target_sources(${GAME_TARGET} PRIVATE "${GAME_STATE_GENERATED_DEVAPI}")` →
  `target_sources(${GAME_TARGET} PRIVATE src/game_save_devapi.c)`.
  (`src/` уже в include-path :184; `game_save_devapi.c` инклюдит `devapi/nt_devapi.h` —
  движковые devapi-хедеры уже в include-path под GAME_DEVAPI_ENABLED-линковке
  `nt_devapi_*`, :210-223, как раньше для генерируемого devapi.)
- `test_game_state_roundtrip` (:343-353) — НЕ трогать (линкует game_state.c, не devapi).

### feature.json
- `outputs` (:33-38): удалить `"game_state_devapi.c"`.
- `default_template.runtime_sources` (:42-49): добавить
  `"templates/template/src/game_save_devapi.c"`. (Своего .h нет — декларация в game_save.h;
  добавлять .h не нужно.) **M1-гард:** copy-model новой игры копирует runtime_sources
  построчно и не гейтит существование — оставленная ссылка на удалённый файл сломала бы
  установку; здесь только ДОБАВЛЕНИЕ существующего файла — безопасно.

### INSTALL.md (полный список — LOW-8)
- **:40** (CMake compile-list): «compile generated `game_state_devapi.c` only when both
  `FEATURE_GAME_STATE` and `GAME_DEVAPI_ENABLED`» → «compile `src/game_save_devapi.c` only
  when both …» (файл теперь рукописный, не генерируемый).
- **:80** и **:233** (runtime wiring + Uninstall): `game_state_register_devapi()` →
  `game_save_register_devapi()`.
- **:110-119** (Generated Files): убрать `game_state_devapi.c` из списка «The generator
  writes» (генератор пишет 4 файла: `game_state.h`/`.c`/`_schema.gen.h` + events; devapi
  больше не генерится). Добавить примечание, что DevAPI-диспатч теперь рукописный
  `src/game_save_devapi.c`.
- **:181-186** («Verify release excludes generated DevAPI source»): заголовок/тело →
  «excludes the DevAPI dispatch source (`src/game_save_devapi.c`)». Сама release-проверка
  (GAME_DEVAPI_ENABLED=OFF) валидна как есть.
- **:215-221 (MED-5, устаревшая A2-заметка):** заметка утверждает, что любая
  `__EMSCRIPTEN__`/`NT_PLATFORM_WEB` сборка падает `-Werror` в `main.c` из-за unused
  `devapi_shutdown_runtime`. `main.c:214-230` УЖЕ несёт web-стаб под `#ifndef NT_PLATFORM_WEB`
  → заметка, вероятно, протухла. Освежить ПОСЛЕ baseline-шага (§A5.12 шаг 0): если
  wasm-devapi-debug на HEAD собирается — снять/переписать заметку на актуальный статус;
  если всё ещё падает по другой причине — зафиксировать точную.
- Copy-list (:14-27): добавить `src/game_save_devapi.c` в перечень копируемых пиеcов.

---

## A5.10 План тестов

**Python (`generate_state_test.py`):**
- ПРАВКА: `OUTPUT_SUFFIXES` минус `_devapi.c` (§A5.6) → golden-тесты game+mini больше не
  сравнивают devapi.
- ПРАВКА: снять devapi-property-ассерты (grep, §A5.6).
- ДОБАВИТЬ (позитив, дёшево): `test_header_has_no_devapi_decl` — сгенерить game в tmp,
  проверить `"register_devapi" NOT IN game_state.h` и что devapi-файл НЕ создан
  (`not (out_dir/"game_state_devapi.c").exists()`).
- Негативные validation-тесты (§A4.8 (3)) — без изменений, остаются зелёными.

**Ctest (существующие — остаются зелёными):** `test_game_state_json`, `test_game_storage`
(после снятия compat-тестов), `test_game_save`, `test_game_events`(+overflow),
`test_game_events_typed`, `test_game_state_roundtrip`, `check_mini_state_events`.

**Ctest — нового C-теста НЕ добавляется:** диспатч тестируется через `devapi_smoke`
(живой рантайм, реальные 7 команд). Юнит-тест диспатча потребовал бы линковать
`nt_devapi` + мок-фрагменты — дороже и хрупче, чем смоук. (Если лид захочет — отдельный
`test_game_save_devapi.c` с мок-`GameSaveFragment` можно добавить в A6; вне A5.)

**Бот-юнит (`smoke_bot_test.py`, offline, НЕ ctest — HIGH-1):** `py -3.12
templates/template/devapi/smoke_bot_test.py` зелёный после правки стабов+валидаторов+ассерта
:104 (§A5.8). Отдельный гейт — «все ctest зелёные» его НЕ покрывает.

**Смоук (`devapi_smoke`, CMake:245):** devapi-debug сборка → бот зелёный с НОВЫМИ
фрагментными валидаторами (§A5.8). Основной функциональный гейт 7 команд + агрегата.

**Ownership (ручной, MED-4):** ASan/leak devapi-debug прогон 7 команд + error-путей
(§A5.11) — единственный дешёвый гейт на ручной cJSON-ownership диспатча.

---

## A5.11 Критерии приёмки (бинарные)

- [ ] `game_save_devapi.c` существует, компилится под devapi-debug + wasm-devapi-debug
      warning-clean (`-Werror`+`nt_set_warning_flags`); native-debug + native-release
      НЕ компилируют devapi-код (пустой TU под `#if NT_DEVAPI_ENABLED`), warning-clean.
- [ ] Генератор эмитит 5 файлов/фрагмент; `*_state_devapi.c` НЕ создаётся;
      `game_state.h` не содержит `register_devapi`.
- [ ] Golden: `game_state.c`/`*_schema.gen.h`/`*_events.gen.{h,c}` (game+mini) байт-в-байт;
      `*_state.h` (game+mini) диф = ровно снятие devapi-блока (ревью глазами);
      `*_state_devapi.c` golden удалены; `OUTPUT_SUFFIXES` без `_devapi.c`; py-тесты зелёные.
- [ ] Шелл: `game_save.h`/`.c` +3 аксессора + devapi-декларация; оркестрация
      load/save/dirty/orphan НЕ изменена (диф game_save.c — чисто аддитивный).
- [ ] main.c зовёт `game_save_register_devapi()`; регистрация фрагмента не тронута.
- [ ] **7 команд DevAPI работают** на живой devapi-debug сборке:
      `get {path:""}`=агрегат `{game:{...}}`; `get {path:"game.wallet.soft"}`=строка (i64);
      `get {path:"game"}`=payload фрагмента; `set {path:"game.<f>",value}` мутирует +
      dirty; `patch {values}` — ПОФРАГМЕНТНО АТОМАРНО (снапшот/откат, §MED-2), per-key
      results; `schema`=агрегат `{game:{...fields[]}}`; `save`/`load`/`reset` через шелл
      (reset=new_game).
- [ ] **Набор `error.code` замороженный (§MED-3):** только `bad_params`/`internal`;
      описательные строки — в `message`. Проверить на error-путях (unknown fragment,
      read-only, bad path, missing params).
- [ ] **Ownership-гейт (§MED-4):** ручной прогон 7 команд ВКЛЮЧАЯ error-пути (unknown
      fragment, read-only фрагмент, кривой путь, patch с частичным фейлом → откат) под
      ASan/leak-сборкой devapi-debug — ноль утечек/double-free (cJSON без санитайзера →
      это единственный дешёвый гейт на ручной ownership; set-эхо через Duplicate).
- [ ] **`devapi_smoke` ЗЕЛЁНЫЙ** с фрагментными валидаторами (Р8); **`py -3.12
      smoke_bot_test.py` ЗЕЛЁНЫЙ** (offline юнит-тесты валидаторов, §A5.8 HIGH-1).
- [ ] **Baseline зафиксирован (§MED-5):** статус wasm-devapi-debug + всех сборок/ctest на
      HEAD снят ДО правок; регрессии A5 отделены от предсуществующих web-хвостов;
      заметка INSTALL.md:215-221 освежена по факту.
- [ ] ПРИМЕЧАНИЕ (Q1): на момент приёмки A5 orphan-ключи в агрегат НЕ входили.
      [РЕШЕНО лидом 2026-07-07: включаются отдельной секцией `"orphans"` — реализовано
      follow-up инкрементом после ратификации, см. §A5.14 Q1.]
- [ ] Compat-обёртки удалены из `game_storage.{h,c}`; `test_game_storage` компилится +
      зелёный без них; grep не находит `game_storage_save_json/load_json/resolve_key` в
      шаблоне (кроме удалённых мест).
- [ ] `feature.json.outputs` без devapi; `runtime_sources` + `game_save_devapi.c`;
      `INSTALL.md` перенаправлен на `game_save_register_devapi()` + новый файл (M1).
- [ ] Все существующие ctest зелёные (список §A5.10).

---

## A5.12 Порядок работ

0. **Baseline (MED-5, ДО любых правок):** собрать шаблон `wasm-devapi-debug` на HEAD
   (`emcmake cmake … -DGAME_DEVAPI_ENABLED=ON` + `cmake --build … --target game`, см.
   §A5.11 web-заметку) и зафиксировать зелёный/красный статус. Это отделяет предсуществующие
   web-`-Werror`-хвосты (INSTALL.md:215-221 намекает на такой в main.c) от регрессий A5 и
   даёт истину для освежения заметки INSTALL.md (§A5.9). Заодно baseline native-debug +
   devapi-debug + все ctest, чтобы «зелёный до» был доказан.
1. **Шелл-API (deep/fast):** game_save.h +аксессоры/decl, game_save.c +тела (§A5.3).
   Компиляционно нейтрально (никто ещё не зовёт).
2. **Диспатч (deep-reasoner ведёт):** `game_save_devapi.c` (§A5.4) — router/aggregate/7
   хендлеров; перенос `state_fail/emit` из golden. Собрать devapi-debug (без main-переключения
   ещё нельзя вызвать, но TU компилится).
3. **Генератор (deep):** снять devapi-эмит + header-блок (§A5.5). Ре-захват golden .h +
   удаление devapi golden (§A5.6). Ревью дифа глазами.
4. **Тесты генератора (fast):** `OUTPUT_SUFFIXES`, снятие devapi-ассертов, +header-no-devapi
   тест (§A5.10). Прогнать `generate_state_test.py` зелёным.
5. **Compat-снос (fast):** game_storage.{h,c} + test_game_storage.c (§A5.7).
6. **Интеграция (fast):** main.c :152, CMake, feature.json, INSTALL.md (§A5.9).
7. **Смоук (deep/fast):** переписать валидаторы бота (§A5.8).
8. **Гейт:** сборки native-debug + devapi-debug + wasm-devapi-debug (-Werror); все ctest;
   `devapi_smoke`; 7 команд руками через devapi-клиент.

Зависимость: шаг 6 (main переключает регистрацию) требует шага 2 (символ определён) и
шага 5 не требует. Шаг 3 ↔ шаг 4 (golden ре-захват ПОСЛЕ правки генератора и ревью ядра).

---

## A5.13 Риски

- **R1 (golden .h не 100% байт-идентичен).** Премисса ТЗ «state-golden байт-идентичен»
  верна для .c/schema/events, но .h ТЕРЯЕТ devapi-блок. Митигация: диф .h ревьюится
  глазами = ровно снятие блока; всё прочее — 0-диф гейт. (Явно зафиксировано §A5.6.)
- **R2 (расширение «замороженного» game_save).** Read-аксессоры аддитивны; оркестрация
  байт-стабильна; диф game_save.c проверяется = только 3 функции. Риск низкий.
- **R3 (память в диспатче).** `to_json`/`get_path_json`/`schema_json` возвращают
  malloc'нутый cJSON, который диспатч ОБЯЗАН либо вставить (`AddItemToObject`), либо
  удалить. Путь ошибки (`!value`) не должен течь. Гейт: смоук + ручной прогон под
  отсутствием ассерта на double-free (cJSON без санитайзера — внимателен исполнитель).
- **R4 (wasm-devapi).** `game_save_devapi.c` на web линкуется `nt_devapi_web`; cJSON/шелл
  доступны. Гейт: wasm-devapi-debug сборка + (опц.) headless-проверка. Риск: EM_JS/шим —
  диспатч их не трогает (чистый C+cJSON), риск низкий.
- **R5 (reset=new_game триггерит on_new_game+save).** В шаблоне on_new_game пуст, save
  безвреден; но для игр reset через DevAPI теперь = полноценная новая игра (стартовый
  контент, запись). Осознанно (§A5.15). Бот reset не зовёт.
- **R6 (rb-dark).** `games/rb-dark-rpg/src/main.c:159` зовёт `game_state_register_devapi()`
  + свой game_storage с compat-обёртками. rb-dark = clean-break (§A4.11), из HEAD не
  собирается; A5 его НЕ трогает и НЕ чинит. Не риск для A5-гейтов.

---

## A5.14 Вопросы лиду (дефолт применён — спека не блокируется)

- **Q1 [РЕШЕНО лидом 2026-07-07: вариант «б» — сироты ВКЛЮЧАЮТСЯ в `get ""` ОТДЕЛЬНОЙ
  секцией `"orphans": {...}` (не вперемешку с живыми фрагментами; секция опускается,
  когда сирот нет). `schema` не трогается — у сирот схемы нет. Реализация = «дешёвый
  реверс» ниже + отдельная секция.]** Исходный дефолт был: НЕТ — агрегат
  только по зарегистрированным фрагментам.
  **Напряжение с §8 (реально, отмечено ревью):** §8 говорит «агрегат = features-map», а
  on-disk features-map ВКЛЮЧАЕТ orphan-ключи verbatim (§14 п.16 их обязательно
  round-trip'ит при save). Значит дефолт-`get ""` СЛЕПНЕТ ровно к тем данным, которые
  §14 п.16 защищает (сирота удалённой/будущей фичи) — а бот баг-репорта, снимающий
  «весь стейт», их не увидит.
  Почему дефолт всё же «без сирот»: у orphan нет хендлера (`to_json`/`schema_json`) — это
  сырой retained-блоб, не живой типизированный стейт; боты в основном читают живые
  фрагменты; сырой блоб = шум в типизированном ответе.
  **Дешёвый реверс (если лид скажет «включать»):** добавить шелл-геттер
  `game_save_orphan_count/at` (симметрично `fragment_count/at` над `s_orphans`), в
  `build_aggregate` дописать orphan-субтри (`cJSON_Duplicate`) под их id ПОСЛЕ
  зарегистрированных — точно как `build_root` кладёт их в features (game_save.c:319-324).
  ~10 строк, без изменения контракта. Ставки низкие, но расхождение с §8 явное.
- **Q2. Семантика `game.state.reset` — `game_save_new_game()` (reset+on_new_game+save+resume)
  или «мягкий» reset_all без save?** ДЕФОЛТ: `game_save_new_game()`. Почему: §8 «reset →
  шелл» + Р10 (new_game — единственная точка on_new_game/resume); «новая игра» — ожидаемая
  дев-семантика; матчит поведение попапа CORRUPT. Если лид хочет неразрушающий reset —
  добавить отдельную команду позже (surface растёт, tool parity против сокращения — но
  reset-как-new_game уже покрывает основной кейс).
- **Q3. Ответ `set`/`patch` — эхо значения/per-key results (как §A5.4) или полный агрегат
  как раньше `to_json`?** ДЕФОЛТ: эхо/results (компактно, §14 п.7 «результат по-ключево»).
  Почему: полный агрегат на каждый set дорог для мульти-фрагментной игры; бот set/patch не
  проверяет. Тривиально сменить на агрегат, если лид предпочтёт симметрию с `get`.
- **Q4. `doc`-параметр — снять (дефолт) или сохранить как алиас-фильтр фрагмента?**
  ДЕФОЛТ: снять (Р8 «чистая схема без фолбеков»; селектор = голова пути). Бот `doc` не
  шлёт. Сохранение = техдолг одного-фрагмента.

Все дефолты консервативны, обратимы и не блокируют исполнение.

---

## A5.15 Отступления от буквы дизайна (с обоснованием)

1. **Диспатч — отдельный рукописный TU `game_save_devapi.c`, а не тело в game_save.c.**
   §1 «game_save владеет DevAPI-диспатчем» реализовано как «шелл-код в своём TU, читающий
   шелл-реестр» — сохраняет модель компиляции (devapi только под GAME_DEVAPI_ENABLED) и
   развязку core-шелла с движковым devapi. Смысл §1/§8 (диспатч = ответственность шелла)
   соблюдён.
2. **patch — пофрагментно атомарен, БЕЗ кросс-фрагментного отката (§8 → §14 п.7).** §8
   «транзакционно через gsj_transact» смягчён §14 п.7: пофрагментная атомарность СОХРАНЕНА
   (снапшот `to_json` → apply группы → откат `from_json` при фейле, §MED-2), кросс-
   фрагментного отката нет. Держится на 11-членном vtable — дескриптор НЕ расширяется
   `patch_json`. Новее побеждает; §14 главнее.
3. **reset → new_game (усиление от транзишн-init_defaults).** Реализует §8 «reset → шелл»
   через единственную шелл-точку new_game (Р10). Транзишн-devapi (A4) делал init_defaults
   одного инстанса; A5 даёт правильную «новую игру».
4. **Golden .h не байт-идентичен (теряет devapi-декларацию).** Неизбежно при убийстве
   devapi-эмита; диф минимальный, ревьюится глазами; .c/schema/events — байт-в-байт гейт.
5. **Расширение «замороженного» game_save 3 аксессорами.** A4 замораживал шелл; A5's мандат
   (§8 реестр-диспатч) прямо требует read-доступ к реестру — аддитивно, безповеденческое.
6. **doc-параметр снят.** Р8 «без слоёв совместимости»; реестр-модель роутит по пути.
