# BUILD-SPEC: state-toolkit v2, инкременты A1–A3 (2026-07-06)

Имплементационная спецификация. Переводит принятый дизайн в файлы, сигнатуры,
CMake, тесты и критерии приёмки. НЕ меняет дизайн. При расхождении с дизайн-доком
источник истины — `features/game-state/references/state_system_design_2026-07-06.md`
(в нём §14 главнее основного текста).

Исполнитель может писать код по этому документу, НЕ открывая историю обсуждений.

## 0. Предпосылки и рамки

- **Зависимость от A0**: A1–A3 стартуют ПОСЛЕ A0 (ампутация, = S1). Генератор
  `features/game-state/scripts/generate_state.py` уже пост-ампутационный (1686
  строк, один живой путь). A0 дополнительно снимает из шаблона сироту-миграцию
  и её проводку в CMake (см. §A0-контракт ниже) — A1–A3 предполагают это сделанным.
- **Где живёт L0 shell**: по установленной реальности репозитория (copy-then-own,
  `feature.json.copy_model = "manual-copy"`) hand-written shell-файлы (`game_storage.*`,
  и новые `game_state_json.*`, `game_save.*`) живут в `templates/template/src/` как
  канонический установленный образец; фича-пак `features/game-state/` держит только
  генератор + доки. Новые игры копируют shell из шаблона. (Соответствует
  `feature_architecture_2026-07-06.md` §2: «L0 ядро-шелл живёт в шаблоне».)
- **Движковая граница проверена**: движок НЕ даёт API записи сейвов/localStorage.
  `nt_fs` (`external/neotolis-engine/engine/fs/nt_fs.h`) — асинхронное чтение
  ассет-паков только (read-only, `nt_fs_read_file`/`nt_fs_take_data`).
  `nt_platform.h` даёт только `nt_platform_memory_usage`; `nt_platform_web.h` —
  только `nt_platform_web_loading_complete`. Значит: собственный файловый I/O в
  `game_storage` (fopen/MoveFileEx) и `EM_JS` localStorage — это НЕ обход движка,
  а корректная реализация того, чего движок не предоставляет. Никаких правок движка.
- **Тест-инфраструктура**: `unity` и `cjson` — таргеты, которые движок
  безусловно экспортирует при `add_subdirectory` (engine root CMake строки 124/127,
  до top-level-гейта на строке 161). Шаблон уже линкует `cjson`. Значит нативные
  C-тесты через Unity + CTest доступны; шаблон обязан сам вызвать `enable_testing()`
  (engine-овый `enable_testing()` под гейтом `CMAKE_SOURCE_DIR STREQUAL ...` и на
  шаблон не распространяется). Существующих ctest в шаблоне НЕТ — A2 их вводит.
- **Один тред**. Потокобезопасность нигде не требуется.
- **Clean-break шаблона (§14 п.12)**: дев-сейвы шаблона бросаются на A3;
  межинкрементной совместимости нет; компат-шимов не строить. Схема шаблона и
  генератор в A1–A3 НЕ трогаются (v1-форма); переписывание схемы в v2 — A4.
- **Текущие вызовы в шаблоне** (уточнено ревью): напрямую в hand-written шаблоне
  `game_storage_*` не вызывается, НО генерируемый `game_state_devapi.c` зовёт
  `game_storage_save_json` (generate_state.py:310), `game_storage_resolve_key`
  (:317), `game_storage_load_json` (:343); этот TU компилится при
  `GAME_DEVAPI_ENABLED=ON` (агентская сборка devapi-debug, CMakeLists:155-157).
  Значит A2 НЕ может удалить эти три функции — она сохраняет их как тонкие
  compat-обёртки над новым API до A5 (перегенерация DevAPI). `game_state_*` в
  шаблоне вызывается только как `game_state_init()` и `game_state_register_devapi()`
  (main.c 145/343) — save-флоу в шаблоне ещё нет, его вводит A3.

### A0-контракт (не входит в этот спек; A1–A3 от него НЕ зависят как от блокера)

A0 снимает мёртвую монолитную машинерию (в т.ч. проводку
`state/migrations/v0_to_v1.c` в `templates/template/CMakeLists.txt` строка 150,
фикстуры `state/fixtures/*`, чинит workflow/review). Уточнение ревью: это НЕ
блокер для A1. `game_state_migrate_v0_to_v1` — мёртвый external без вызовов, не
ворнит, ни с чем не сталкивается; A1 собирается и ведёт себя идентично при его
наличии. Поэтому правки A1 в CMake — АДДИТИВНЫЕ (вставить строку; существующие,
включая `state/migrations/v0_to_v1.c`, не трогать — их снимет A0/отдельный шаг).
Единственный устаревший хвост, который A1 правит попутно (файл он и так
редактирует) — строка `feature.json:44` `default_template.runtime_sources` со
ссылкой на `v0_to_v1.c`.

---

## A1. game_state_json — общий JSON-тулкит L0 (gsj_*)

**Суть**: вынести общие статик-хелперы генерируемого кода в один компилируемый
L0-модуль + добавить i64-провод (§14 п.8) как часть контракта тулкита. НОЛЬ
изменений поведения существующего кода: генератор и генерируемый выхлоп НЕ
трогаются, свои копии статиков генерируемый код держит до A4.

### A1.1 Файлы

Новые:
- `templates/template/src/game_state_json.h`
- `templates/template/src/game_state_json.c`
- `templates/template/tests/test_game_state_json.c` (Unity)

Изменяемые:
- `templates/template/CMakeLists.txt` (АДДИТИВНАЯ вставка нового TU + ctest-блок;
  существующие строки target_sources не трогать).
- `features/game-state/feature.json` — добавить `game_state_json.c/.h` в
  `default_template.runtime_sources` И убрать устаревшую строку :44 с
  `state/migrations/v0_to_v1.c` (мёртвый хвост A0; файл всё равно редактируется);
  `features/game-state/INSTALL.md` — в список копируемых shell-файлов. (Доки; правки
  в фиче, не в игре.)

Удаляемые: нет.

Не трогать: `generate_state.py`, любые генерируемые файлы, `game_storage.*`.

### A1.2 Хедер `game_state_json.h` (полностью)

```c
#ifndef GAME_STATE_JSON_H
#define GAME_STATE_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"

/* L0 JSON-тулкит (gsj_*). Лист-модуль: не зависит от game_save/game_storage/
   генерируемого GameState. Один тред. Владение: ридеры получают чужое cJSON-
   дерево как const и НЕ владеют им; строки копируются в буфер вызывающего. */

/* Пишет message в error (усечение безопасно). No-op если error==NULL || cap<=0. */
void gsj_set_error(char *error, int error_cap, const char *message);

/* snprintf-копия src→dst с проверкой ёмкости; false если NULL/не влезло. */
bool gsj_copy_text(char *dst, size_t dst_cap, const char *src);

/* CaseSensitive lookup; NULL если obj не объект или ключа нет. */
const cJSON *gsj_object_item(const cJSON *obj, const char *name);

/* Толерантные ридеры (контракт «absent = ok, дефолт остаётся»):
   ключа нет            -> true, *out не трогается;
   есть, но не тот тип  -> false + error;
   есть, вне диапазона  -> false + error;
   есть, валиден        -> true, *out записан. */
bool gsj_read_bool(const cJSON *obj, const char *name, bool *out,
                   char *error, int error_cap);
bool gsj_read_int_range(const cJSON *obj, const char *name,
                        int min_value, int max_value, int *out,
                        char *error, int error_cap);
bool gsj_read_float_range(const cJSON *obj, const char *name,
                          float min_value, float max_value, float *out,
                          char *error, int error_cap);
bool gsj_read_string(const cJSON *obj, const char *name, char *out,
                     size_t out_cap, char *error, int error_cap);

/* enum по таблице имён (совпадение по строке ИЛИ по целому индексу, legacy). */
int  gsj_enum_index(const char *value, const char *const *names, int count);
bool gsj_read_enum(const cJSON *obj, const char *name,
                   const char *const *names, int count, int *out,
                   char *error, int error_cap);

/* Парсеры одного узла (для элементов map/list). */
bool gsj_parse_int_value(const cJSON *item, int min_value, int max_value,
                         int *out, char *error, int error_cap);
bool gsj_parse_enum_value(const cJSON *item, const char *const *names,
                          int count, int *out, char *error, int error_cap);

/* i64-провод (§14 п.8). Большие счётчики едут JSON-СТРОКОЙ (double рвётся >2^53).
   read: принимает строку ВСЕГДА; число — ТОЛЬКО если точно представимо в int64
   (целое и |v| <= 2^53). Absent = true, *out не трогается. */
bool gsj_read_i64(const cJSON *obj, const char *name,
                  int64_t min_value, int64_t max_value, int64_t *out,
                  char *error, int error_cap);
bool gsj_parse_i64_value(const cJSON *item, int64_t min_value, int64_t max_value,
                         int64_t *out, char *error, int error_cap);
/* Кладёт value как JSON-СТРОКУ в obj (узел во владении obj). NULL при OOM. */
cJSON *gsj_add_i64(cJSON *obj, const char *name, int64_t value);
/* value -> десятичная строка в buf (cap >= 21). Возвращает buf. */
char  *gsj_i64_to_string(int64_t value, char *buf, size_t cap);

#endif /* GAME_STATE_JSON_H */
```

Контракты реализации (`game_state_json.c`):
- `gsj_set_error/gsj_copy_text/gsj_object_item/gsj_read_bool/gsj_read_int_range/
  gsj_read_float_range/gsj_read_string/gsj_enum_index/gsj_read_enum/
  gsj_parse_int_value/gsj_parse_enum_value` — **семантически идентичны**
  соответствующим статикам в `render_generic_source` (generate_state.py
  строки 1286–1398: `set_error`, `copy_text`, `object_item`, `read_bool`,
  `read_int_range`, `read_float_range`, `read_string`, `enum_index`, `read_enum`,
  `parse_enum_value`, `parse_int_value`). Копировать поведение один-в-один,
  переименовав в `gsj_`. **При копировании убрать `static` и
  `GAME_STATE_MAYBE_UNUSED`** — здесь функции external, а этот макрос в листовом
  TU не определён.
- `gsj_read_i64`: если узла нет → true. Если строка → `strtoll` (база 10), ошибка
  парса/остаток → false; проверить диапазон. Если число → принять только если
  `v == (double)(int64_t)v` и `|v| <= 9007199254740992.0` (2^53), иначе false
  «i64 must be sent as string». Диапазон [min,max] проверяется всегда.
- `gsj_add_i64`: форматирует через `gsj_i64_to_string` и добавляет как строку.
- **Warning-safety**: game-таргет собирается с `-Werror` + `nt_set_warning_flags`
  (warnings.cmake:4-19, включая `-Wconversion`/`-Wdouble-promotion`). i64-хелперы
  писать с ЯВНЫМИ кастами (`(double)`, `(int64_t)`), не полагаться на неявные
  промоушены int↔double↔int64.
- Зависимости: `<stdlib.h>` (strtoll), `<string.h>`, `<stdio.h>` (snprintf), cJSON.

**gsj_transact НЕ входит в A1** — см. §Отступления. Транзакционный
validate-copy-swap на уровне фрагмента строится в A5 (DevAPI patch) поверх ABI
фрагмента; чистый cJSON-примитив без потребителя в A1–A3 не заводим.

### A1.3 CMake

`templates/template/CMakeLists.txt`, внутри `if(FEATURE_GAME_STATE)` (рядом с
`src/game_storage.c` в `target_sources`, строки 147–151). Правка АДДИТИВНАЯ —
вставить ОДНУ строку `src/game_state_json.c`, остальные (включая
`state/migrations/v0_to_v1.c`) НЕ трогать (их снимет A0 отдельным шагом):
```cmake
target_sources(${GAME_TARGET} PRIVATE
    "${GAME_STATE_GENERATED_SOURCE}"
    src/game_state_json.c        # A1 — вставлено
    src/game_storage.c
    state/migrations/v0_to_v1.c  # существующая строка, не трогать (снимет A0)
)
```
`game_state_json.c` использует `cJSON.h` → таргет уже линкует `cjson` (строка 152)
и уже имеет include движка. Отдельного include-пути не требуется (`src` уже в
`target_include_directories`, строка 161).

Ctest-блок (новый, в конце файла; вводится здесь, расширяется в A2/A3):
```cmake
if(NOT EMSCRIPTEN)
    enable_testing()
    add_executable(test_game_state_json tests/test_game_state_json.c src/game_state_json.c)
    target_link_libraries(test_game_state_json PRIVATE cjson unity)
    target_include_directories(test_game_state_json PRIVATE src)
    target_compile_definitions(test_game_state_json PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_state_json PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_state_json COMMAND test_game_state_json)
endif()
```
Уровень фичи (`features/game-state/`): CMake не имеет — фича = скрипт+доки; правок нет.

### A1.4 Тесты

Unity в движке собран с `UNITY_EXCLUDE_FLOAT/DOUBLE` (PUBLIC) → `TEST_ASSERT_FLOAT_*`
НЕ существуют; `gsj_read_float_range` проверять вручную (`fabsf(out-exp) < eps`).

1. **C-юнит `test_game_state_json.c` (Unity, ctest)** — для каждого ридера:
   absent-key (true, out нетронут), wrong-type (false), out-of-range (false),
   valid (true+значение); enum по имени и по числу; i64 round-trip строкой,
   в т.ч. значение > 2^53 (напр. 9_000_000_000_000_000_000) — читается из строки,
   не читается из числа; `gsj_add_i64`→`gsj_read_i64` эквивалентность.
2. **Байт-идентичность выхлопа (Python)**: расширить
   `features/game-state/scripts/generate_state_test.py` кейсом, генерящим
   `templates/template/state/game_state.schema.json` в tmp и сравнивающим
   `game_state.c/.h/.gen.h/devapi.c` с эталоном, снятым ДО A1 (или: тест просто
   утверждает, что A1 не менял генератор — diff генератора пуст). Цель: доказать
   ноль изменений генерируемого поведения.

### A1.5 Критерии приёмки (бинарные)

- [ ] `ctest -R test_game_state_json` зелёный.
- [ ] `py -3.12 generate_state_test.py` зелёный; генерируемый выхлоп для схемы
      шаблона байт-идентичен до/после A1.
- [ ] `templates/template` game-таргет собирается **warning-clean под `-Werror`** +
      `nt_set_warning_flags` (`-Wconversion`/`-Wdouble-promotion` — i64-касты явные),
      `FEATURE_GAME_STATE=ON`, native-debug и release.
- [ ] `game_state_json.c` не ссылается ни на `game_save`, ни на `game_storage`,
      ни на `GameState`/`g_game_state` (лист-модуль).

### A1.6 Пакет делегирования

**fast-worker (Sonnet) + warning safety-net.** Работа механическая: скопировать 11
статиков под `gsj_` (убрав `static`/`GAME_STATE_MAYBE_UNUSED`), дописать 4 i64-
хелпера по явным контрактам, написать Unity-тест по списку кейсов. Самое рискованное:
(1) краевые случаи `gsj_read_i64` (переполнение `strtoll`, знак, нечисловой хвост,
граница 2^53); (2) `-Werror`-чистота i64-кастов (`-Wconversion`/`-Wdouble-promotion`).
Оба полностью специфицированы выше и покрыты тестом/критерием. Deep-review не нужен,
но приёмка обязана включать warning-clean сборку.

---

## A2. game_storage — atomic+bak native + localStorage web за одной сигнатурой

**Суть**: перенести правильный атомарный рецепт (`replace_file`, MoveFileEx
REPLACE|WRITE_THROUGH) из генерируемого `game_state_save` в `game_storage.c`
(атомарность = обязанность L0); консолидировать web-ветку rb-dark (localStorage,
APP_ID-неймспейс) за той же сигнатурой; краш-тесты. Ротация упрощена до §14 п.1:
ОДИН атомарный replace + .bak пишется один раз за сессию.

### A2.1 Файлы

Изменяемые:
- `templates/template/src/game_storage.h` — новый API (см. A2.2) + СОХРАНЁННЫЕ
  compat-обёртки `game_storage_save_json/load_json/resolve_key` (их зовёт
  генерируемый `game_state_devapi.c`, generate_state.py:310/317/343; TU компилится
  при `GAME_DEVAPI_ENABLED=ON`, CMakeLists:155-157 — удаление = ошибка компиляции).
- `templates/template/src/game_storage.c` — новая реализация (native atomic +
  web EM_JS) + тонкие compat-обёртки. Источник native-атомарности:
  `generate_state.py` строки 1514–1566 (`make_dir_if_needed`, `ensure_parent_dirs`,
  `replace_file`, tmp-write; НЕ забыть `#include <windows.h>` для `MoveFileExA` —
  сейчас в файле только `<direct.h>`). Источник web-ветки:
  `games/rb-dark-rpg/src/game_storage.c` строки 20–72, 145–267 — там ЧЕТЫРЕ EM_JS
  (`web_save`/`web_load`/`web_key_exists`/`web_delete`); `probe` пишется с нуля.
  Ключ строится ЗАНОВО по схеме `"<APP_ID>/save/<slot>"` (это НЕ префикс-свап: у
  rb-dark хардкод `"rb-dark-rpg:"+path`). **rb-dark НЕ править** — только читать.
- `templates/template/CMakeLists.txt` — компиляция `GAME_STORAGE_APP_ID` define +
  ctest `test_game_storage`.
- `templates/template/tests/test_game_storage.c` (новый, Unity).

### A2.2 Хедер `game_storage.h` (полностью)

```c
#ifndef GAME_STORAGE_H
#define GAME_STORAGE_H

#include <stdbool.h>

/* L0 байтовый бэкенд слота: native atomic-файл / web localStorage за одной
   сигнатурой. slot = логическое имя ([a-z0-9_-]+, проверяется). Значения —
   NUL-терминированный ТЕКСТ (бинарь трансформов приходит уже base64, §14 п.15).
   APP_ID = compile define GAME_STORAGE_APP_ID (неймспейс общего web-origin itch).
   Один тред. */

/* Атомарная запись слота.
   native: build/saves/<slot>.json.tmp (WRITE_THROUGH) -> replace_file(tmp->primary);
           primary никогда не отсутствует и не рвётся (§14 п.1).
   web:    localStorage["<APP_ID>/save/<slot>"] = text в try/catch;
           false = квота/Safari-private (наверх как SAVE_UNPERSISTED). */
bool game_storage_write(const char *slot, const char *text, char *error, int error_cap);

/* Чтение primary. *out — malloc'нутая NUL-строка (владелец вызывающий, free()).
   false: слота нет / ошибка. */
bool game_storage_read(const char *slot, char **out, char *error, int error_cap);

/* true если primary слота существует и читаем. */
bool game_storage_exists(const char *slot);

/* Пишет .bak = копию заведомо-хорошего primary (§14 п.1: один раз за сессию,
   после успешной загрузки last-known-good).
   native: primary -> <slot>.bak. web: no-op, true (web .bak вырезан, §14 п.3). */
bool game_storage_write_backup(const char *slot, char *error, int error_cap);

/* Чтение .bak (фолбэк лоадера). native only; web всегда false. */
bool game_storage_read_backup(const char *slot, char **out, char *error, int error_cap);

/* Откладывает битый primary для форензики/ручной починки (Р10, восстанавливает
   .corrupt из §14 п.14 — читатель есть: лид правит сейвы руками).
   native: rename primary -> <slot>.corrupt-<unix_ms>. web: copy value -> "<key>.corrupt". */
bool game_storage_quarantine(const char *slot, char *error, int error_cap);

/* Стартовый пробник персистентности (§14 п.3).
   native: всегда true. web: setItem+getItem+removeItem пробного ключа ->
   false если браузер не сохраняет. */
bool game_storage_probe(char *error, int error_cap);

/* ---- Compat-обёртки до A5 (генерируемый game_state_devapi.c зовёт старый
   key+document API: generate_state.py:310/317/343). Тонкие адаптеры (slot=key,
   document = всегда GAME_STATE_DOCUMENT, сворачивается в slot) над новым API;
   удаляются в A5 при перегенерации DevAPI-эндпоинтов. ---- */
bool game_storage_resolve_key(const char *key, const char *document,
                              char *out, int out_cap, char *error, int error_cap);
bool game_storage_save_json(const char *key, const char *document,
                            const char *json, char *error, int error_cap);
bool game_storage_load_json(const char *key, const char *document,
                            char **out_json, char *error, int error_cap);

#endif /* GAME_STORAGE_H */
```

Контракты реализации:
- **Инклюды native**: добавить `#include <windows.h>` под `#ifdef _WIN32` (для
  `MoveFileExA` — в исходном файле только `<direct.h>`).
- `GAME_STORAGE_APP_ID` — обязателен как compile define (CMake). Внутренний
  резолвер строит native `build/saves/<slot>.json` и web-ключ
  `"<APP_ID>/save/<slot>"` (собирается ЗАНОВО, не префикс-свап rb-dark).
  `is_safe_segment` на slot сохранить (charset `[A-Za-z0-9_-]`).
- native `game_storage_write`: `ensure_parent_dirs` → fopen(`<primary>.tmp`,"wb")
  → fwrite → fclose(проверить оба) → `replace_file(tmp, primary)`; при любой ошибке
  `remove(tmp)`. `replace_file` = `MoveFileExA(tmp,primary,MOVEFILE_REPLACE_EXISTING|
  MOVEFILE_WRITE_THROUGH)` на _WIN32, иначе `rename`. (Двух-rename схема §2
  ОТМЕНЕНА §14 п.1: primary не пишется напрямую, окна отсутствия primary нет,
  POSIX-EEXIST-ловушки Windows нет — REPLACE-флаг её снимает.)
- native `game_storage_write_backup`: читать primary, писать `<slot>.bak` тем же
  atomic-путём (tmp+replace на bak-имя). Если primary нет — true (нечего бэкапить).
- web-ветка: ЧЕТЫРЕ `EM_JS` из rb-dark (`web_save`/`web_load`/`web_key_exists`/
  `web_delete`), ключ пересобран по `"<APP_ID>/save/<slot>"`. Сохранить `EM_JS_DEPS`.
  `game_storage_probe` web — НОВАЯ EM_JS с try/catch на setItem/getItem/removeItem
  ключа `"<APP_ID>/__probe"` (native: return true). web `write_backup` — no-op true.
- **compat-обёртки** (`game_storage_save_json/load_json/resolve_key`): `slot=key`;
  save_json→`game_storage_write(key,json,...)`; load_json→`game_storage_read(key,
  out,...)`; resolve_key→snprintf пути для поля `resolved` DevAPI. Поведенческая
  совместимость (save→load round-trip), не байтовая path-совместимость (dev-only,
  A5 перепишет).
- Cap 1MB на чтение сохранить (`GAME_STORAGE_MAX_BYTES`).

### A2.3 CMake

`templates/template/CMakeLists.txt`:
```cmake
# рядом с прочими target_compile_definitions game-таргета:
target_compile_definitions(${GAME_TARGET} PRIVATE GAME_STORAGE_APP_ID="template")
```
(`"template"` — дефолт шаблона; игра переопределяет. Совпадает с §4.)

Ctest (добавить к блоку A1):
```cmake
add_executable(test_game_storage tests/test_game_storage.c src/game_storage.c)
target_link_libraries(test_game_storage PRIVATE unity)
target_include_directories(test_game_storage PRIVATE src)
target_compile_definitions(test_game_storage PRIVATE
    GAME_STORAGE_APP_ID="template_test" _CRT_SECURE_NO_WARNINGS)
if(WIN32)
    target_link_libraries(test_game_storage PRIVATE)  # MoveFileExA в kernel32 (по умолчанию)
endif()
set_target_properties(test_game_storage PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME test_game_storage COMMAND test_game_storage
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
```

### A2.4 Тесты

(Unity без float/double-макросов — здесь неактуально, storage оперирует байтами/
строками; строки сравнивать `TEST_ASSERT_EQUAL_STRING`.)

1. **Native crash-safety ctest `test_game_storage.c`** (гейт увечности, §9):
   - write→read round-trip; `exists` до/после.
   - **Атомарность**: положить рядом с primary мусорный `<slot>.json.tmp`
     (симуляция краша между write tmp и replace) → `read` возвращает целый
     primary, не мусор (tmp невидим лоадеру).
   - **Bak-фолбэк**: write primary → `write_backup` → испортить primary (перезаписать
     не-JSON байтами напрямую) → `read` primary = мусор (стораджу всё равно, это
     задача game_save парсить), но `read_backup` = валидный last-good.
   - **Карантин**: `quarantine(slot)` → primary исчез, появился ровно один
     `<slot>.corrupt-<ts>`; повторный `read` primary = false.
   - **APP_ID в пути**: не обязателен ассерт пути native (путь без APP_ID native),
     но зафиксировать, что web-ключ строится с APP_ID (проверяется web-чеком).
   - Все тесты работают во временном `WORKING_DIRECTORY` (build/tests), чистят
     `build/saves/` в setUp/tearDown.
2. **Web-персистентность через refresh** (advisory-чек, не блокирующий ctest —
   headless-localStorage-автоматизация капризна). Рецепт по
   memory `web-wasm-headless-verify`: `emcmake` сборка шаблона →
   `python -m http.server` на `bin/` → Chrome `--headless=new` с ПОСТОЯННЫМ
   `--user-data-dir` → триггер записи слота (через DevAPI-web или URL-hash-хук) →
   перезагрузка страницы (тот же user-data-dir) → чтение слота вернуло значение.
   Оформить как `templates/template/tests/web_persistence_check.py` + запись в
   INSTALL.md «Verify»; отметить как CI-optional. Обоснование не-блокирования:
   web-ветка — тонкий EM_JS-зеркал rb-dark (уже отгружен в проде), а атомарная
   логика (единственное сложное) покрыта нативным ctest.

### A2.5 Критерии приёмки

- [ ] `ctest -R test_game_storage` зелёный, включая три краш-сценария.
- [ ] `game_storage.c` native-путь использует `replace_file` (MoveFileEx
      REPLACE|WRITE_THROUGH), primary никогда не открывается на прямую запись "wb".
- [ ] Web-ключ = `"<GAME_STORAGE_APP_ID>/save/<slot>"`; хардкод "rb-dark-rpg:"
      отсутствует.
- [ ] `.bak` пишется отдельной функцией (не на каждый save); web `write_backup` =
      no-op true.
- [ ] **Сборка devapi-debug конфигурится и КОМПИЛИТСЯ** (`GAME_DEVAPI_ENABLED=ON`):
      генерируемый `game_state_devapi.c` линкуется с compat-обёртками
      `game_storage_save_json/load_json/resolve_key` (tool parity — hard invariant).
- [ ] Шаблон собирается native + wasm (`emcmake`) + devapi-debug с `GAME_STORAGE_APP_ID`.
- [ ] Web-персистентность подтверждена рефреш-чеком (advisory, приложить лог).

### A2.6 Пакет делегирования

**fast-worker (Sonnet) после снятия блокера; deep-review дизайна краш-тестов
остаётся.** Блокер (compat-обёртки для генерируемого DevAPI) снят в спеке выше —
исполнителю не удалять старые три функции, а обернуть. Перенос кода механический
(исходники построчно). Самое рискованное: (1) корректность краш-семантики — что
именно симулирует «краш между tmp и replace» и почему read обязан вернуть last-good
(тест это доказывает); (2) Windows MoveFileEx флаги + `<windows.h>` + поведение при
отсутствии primary; (3) compat-маппинг document→slot не должен ломать
save→load round-trip DevAPI. Рекомендация: Sonnet реализует, deep-reasoner (или лид)
просматривает набор краш-кейсов + devapi-debug-сборку перед приёмкой.

---

## A3. game_save — конверт + реестр + load/save/new_game + debounce + web flush

**Суть**: hand-written L0-оркестратор. Реестр фрагментов, единый атомарный
документ-конверт, оркестрация загрузки (FRESH/LOADED/RECOVERED_BAK/CORRUPT_RESET/
NEWER, пофрагментно, никогда не all-or-nothing), on_new_game, dirty/debounce/
MAX_INTERVAL, синхронный web visibility-flush, export/import, transform-шов
(пустой по умолчанию). Старт с ОДНИМ фрагментом `game`, оборачивающим текущий
генерируемый `g_game_state`.

### A3.1 Файлы

Новые:
- `templates/template/src/game_save.h`
- `templates/template/src/game_save.c`
- `templates/template/src/game_fragment.c` — переходный адаптер: определяет
  `const GameSaveFragment game_fragment`, чьи хуки зовут генерируемый API на
  `g_game_state`. (В A4 фрагмент `game` перегенерируется v2 и адаптер уходит.)
- `templates/template/tests/test_game_save.c` (Unity; использует СВОЙ фейковый
  фрагмент, не генерируемый монолит — тесты независимы).

Изменяемые:
- `templates/template/src/main.c` — проводка: регистрация фрагмента → init →
  load(+статус) → `game_save_tick()` в кадре → web `game_save_install_web_flush()`;
  вынуть `--fresh-state`/`--disable-autosave` из no-op в реальную ветку.
- `templates/template/CMakeLists.txt` — компиляция `game_save.c`, `game_fragment.c`,
  compile-defines, ctest `test_game_save`.
- `features/game-state/feature.json` / `INSTALL.md` — runtime_sources + wire-доки.

Не трогать: генерируемые файлы, `game_state_devapi.c` (старые 7 команд
`game.state.*` продолжают работать против `g_game_state` напрямую — их диспатч по
реестру = A5). Генерируемые `game_state_save/load/make_save_doc` остаются
скомпилированными, но НЕ вызываются шеллом (мёртвые до A4; вреда нет).

### A3.2 Хедер `game_save.h` (полностью)

```c
#ifndef GAME_SAVE_H
#define GAME_SAVE_H

#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"

#ifndef GAME_SAVE_MAX_FRAGMENTS
#define GAME_SAVE_MAX_FRAGMENTS 32
#endif

/* ---- Контракт фрагмента: весь ABI между шеллом и фичей (§3 + §14 п.2) ---- */
typedef bool (*GameSaveMigrateFn)(cJSON *frag, char *err, int cap); /* v(i)->v(i+1) */

typedef struct GameSaveFragment {
    const char *id;                  /* ключ в features{} и C-префикс; [a-z_][a-z0-9_]* */
    int version;                     /* = steps_count + 1 */
    const GameSaveMigrateFn *steps;  /* NULL пока миграций нет (90% случаев) */
    void  (*reset)(void);            /* нейтральные ПУСТЫЕ дефолты (§14 п.2) */
    void  (*on_new_game)(void);      /* NULLABLE: стартовый контент, только свежий сейв */
    cJSON*(*to_json)(void);          /* только данные, без "v" (штампует шелл) */
    bool  (*from_json)(const cJSON *frag, char *err, int cap); /* толерантный */
    void  (*reconcile)(void);        /* NULLABLE: пост-load фиксап (карантин items) */
    cJSON*(*get_path_json)(const char *sub, char *err, int cap);       /* NULLABLE: DevAPI read (A5) */
    bool  (*set_path_json)(const char *sub, const cJSON *v, char *err, int cap); /* NULLABLE: DevAPI write (A5) */
    cJSON*(*schema_json)(void);      /* NULLABLE */
} GameSaveFragment;

/* Регистрация ДО первого load; порядок = порядок reconcile/on_new_game.
   Фрагмент game регистрируется ПОСЛЕДНИМ (§14 п.2). Указатель должен пережить рантайм. */
void game_save_register_fragment(const GameSaveFragment *fragment);

/* ---- Статус и результат загрузки (§4, §14 п.10, Р7, Р10) ---- */
typedef enum {
    GAME_SAVE_LOAD_FRESH = 0,      /* файла нет -> reset+on_new_game+save */
    GAME_SAVE_LOAD_LOADED,         /* primary распарсен */
    GAME_SAVE_LOAD_RECOVERED_BAK,  /* primary плох, поднят .bak */
    GAME_SAVE_LOAD_CORRUPT_RESET,  /* primary+bak плохи -> карантин+new_game, автосейв на паузе */
    GAME_SAVE_LOAD_NEWER,          /* сейв новее билда -> НОЛЬ записи, только чтение/экспорт */
} game_save_load_status_t;

typedef struct {
    game_save_load_status_t status;
    int         reset_fragment_count;                       /* сколько фрагментов упало в дефолт */
    const char *reset_fragments[GAME_SAVE_MAX_FRAGMENTS];   /* их id (для тоста «X не загрузился») */
    char        message[128];                               /* краткая диагностика */
} game_save_load_result_t;

/* Инициализация оркестратора (после регистрации ВСЕХ фрагментов). */
void game_save_init(void);

/* Грузит автослот (GAME_SAVE_AUTOSAVE_SLOT). Оркестрация §6/§14; никогда не
   all-or-nothing (плохой фрагмент не роняет остальные). Заполняет *result. */
void game_save_load(game_save_load_result_t *result);

/* Явная новая игра: reset всех -> on_new_game всех -> save -> возобновляет автосейв (Р10). */
bool game_save_new_game(char *error, int error_cap);

/* Синхронный форс-сейв в обход дебаунса (visibility-flush, §14 п.5). */
bool game_save_flush(char *error, int error_cap);

/* Кадровый тик: сейв при (now-dirty_at >= DEBOUNCE) || (now-last_save >= MAX_INTERVAL).
   No-op пока не dirty или автосейв на паузе (CORRUPT_RESET/NEWER до new_game). */
void game_save_tick(void);

/* Пометка «есть несохранённое». dirty_at = момент ПЕРВОЙ пометки после чистого
   (§14 п.6). Зовут мутаторы фич/UI. */
void game_save_mark_dirty(void);

/* Wall-clock ms сохранённого saved_at (оффлайн-Δt для идла; кламп отрицательных —
   задача игры). 0 если ещё не грузили/сохраняли. */
int64_t game_save_last_saved_at(void);

/* Персистентность недоступна (web квота/private, §14 п.3): игра может показать тост. */
bool game_save_is_unpersisted(void);

/* Экспорт/импорт строкой (Р12, §14 п.4) — тем же transform-путём; валидация конверта. */
char *game_save_export_string(char *error, int error_cap);   /* malloc, free вызывающим */
bool  game_save_import_string(const char *text, char *error, int error_cap);

/* Web: ставит visibilitychange(->hidden) + pagehide -> game_save_flush (НЕ
   beforeunload, §4). No-op на native. */
void game_save_install_web_flush(void);

/* ---- Transform-шов (§14 п.15). Дефолт пуст -> сейв = читаемый JSON ('{'). ---- */
typedef struct {
    const char *id;
    char *(*encode)(const char *in, char *error, int error_cap);  /* malloc out; save-порядок */
    char *(*decode)(const char *in, char *error, int error_cap);  /* malloc out; load-реверс */
} game_save_transform_t;

/* Устанавливает упорядоченную цепочку (save: по порядку; load: реверс). Пустая по
   умолчанию. Автодетект на load: префикс "NTSV1:<ids>:" -> трансформы; '{' -> плоский JSON. */
void game_save_set_transforms(const game_save_transform_t *chain, int count);

#endif /* GAME_SAVE_H */
```

Compile-defines (CMake, §4): `GAME_SAVE_AUTOSAVE_SLOT="autosave"`,
`GAME_SAVE_DEBOUNCE_MS=2000`, `GAME_SAVE_MAX_INTERVAL_MS=30000`,
`GAME_SAVE_DOC_VERSION=1`. Внутренние константы `game_save.c`:
`GAME_SAVE_FORMAT=1`, строка билда `GAME_SAVE_BUILD` (опц., дефолт "0").

### A3.3 Конверт (формат на диске, §2 + §14 п.11) + удержание сирот (§14 п.16)

```json
{ "format": 1, "save_version": 1, "saved_at": 1720080000000, "save_seq": 42,
  "app": "template", "build": "0",
  "features": {
    "game": { "v": 1, "shape_index": "cube", "camera_distance": 6.0, ... } } }
```
- `format` — версия контейнера; `save_version` = `GAME_SAVE_DOC_VERSION` (гейт
  редких кросс-фрагментных doc-шагов); `saved_at` — wall-clock ms (число, <2^53);
  `save_seq` — монотонный счётчик (кладётся дёшево для облачного шва без wall-clock-
  конфликтов); `app`=`GAME_STORAGE_APP_ID`; `build` диагностика.
- Незнакомые ключи КОНВЕРТА (верхний уровень) игнорируются с первого дня (шов облака/слотов).
- Каждый фрагмент: плоский payload + `"v"` инжектит ШЕЛЛ (хуки версию не трогают).
- **Удержание сирот (§14 п.16, ОБЯЗАТЕЛЬНО)**: ключ в `features{}` без
  зарегистрированного фрагмента (при версиях ≤ моих) = сирота удалённой/будущей
  фичи → НЕ отказ (Р7), warn в лог + загрузка продолжается. Такие поддеревья
  УДЕРЖИВАЮТСЯ и выводятся обратно при КАЖДОМ save (семантически идентичный JSON),
  иначе пинг-понг версий (старый билд пересейвит → выбросил бы чужие фрагменты)
  терял бы данные. Хранение: `game_save` держит на весь рантайм список
  `{char *id; cJSON *subtree}` (глубокая копия `cJSON_Duplicate` от load до save;
  освобождать/пересобирать при следующем load). save-путь добавляет их в
  `features{}` ПОСЛЕ зарегистрированных фрагментов. Коллизия невозможна (id сироты
  по определению не совпадает ни с одним зарегистрированным).

### A3.4 Оркестрация load (реализация game_save_load, §6 + §14)

```
1. bytes = storage_read(primary) через transform-decode (автодетект).
   нет файла -> FRESH: reset() всех -> on_new_game() всех (в порядке регистрации,
      game последним) -> save() -> status=FRESH. return.
2. parse(bytes) в cJSON. Fail ИЛИ format незнаком/новее ->
      bytes_bak = storage_read_backup + transform-decode; parse.
      bak ок -> грузим по bak-документу (перейти к п.3 с bak-doc), финальный
                status=RECOVERED_BAK (важно для п.8!).
      bak fail/нет -> storage_quarantine(primary); reset() всех (БЕЗ on_new_game,
                      БЕЗ save — Р10: шелл ждёт явного решения игрока);
                      status=CORRUPT_RESET; автосейв на ПАУЗЕ до game_save_new_game().
                      return. (on_new_game достигается ТОЛЬКО через обязательный
                      new_game() из main.c — иначе двойной on_new_game.)
3. NEWER-детект (Р7 + §14 п.16, ТОЛЬКО версии):
      newer = format незнаком/>1 || save_version>DOC_VERSION ||
              (для каждого ЗНАКОМОГО id: features[id].v > frag.version).
      newer -> status=NEWER; НОЛЬ записи; автосейв на паузе; return (экспорт-чтение доступно).
      (Незнакомый фрагмент-ключ из NEWER ИСКЛЮЧЁН — неотличим от сироты удалённой
       фичи; обрабатывается в п.6. Retired-списков нет.)
4. save_seq-восстановление: прочитать save_seq загруженного конверта в счётчик
      (следующий save обязан быть > загруженного — монотонность между сессиями).
5. doc-шаги: если save_version < DOC_VERSION -> прогнать кросс-фрагментные шаги по
      СЫРОМУ features{} (видят сейв как записан, §14 п.9). В A3 DOC_VERSION=1 -> шагов
      нет, но шов на месте (список doc-шагов пуст).
6. Пофрагментно, независимо (в порядке регистрации), НИКОГДА не all-or-nothing:
      Для КАЖДОГО зарегистрированного фрагмента:
        f = features[id];
        absent          -> frag.reset() (новая фича != миграция; on_new_game НЕ зовётся).
        present:
          v = f["v"] (отсутствует -> 1);
          v < frag.version -> прогнать steps[v-1..version-1] над КОПИЕЙ f; при успехе
                              ВСЕХ шагов -> frag.from_json(мигрированный f); при провале
                              ЛЮБОГО шага ИЛИ from_json -> frag.reset() +
                              record(reset_fragments, id) + warn.
          v == frag.version-> frag.from_json(f); фейл -> frag.reset()+record+warn.
          (ветка v > frag.version НЕДОСТИЖИМА — п.3 NEWER её преэмптит; коммент в коде.)
      Для КАЖДОГО ключа features{} БЕЗ зарегистрированного фрагмента (сирота, §14 п.16):
        warn в лог + УДЕРЖАТЬ поддерево (cJSON_Duplicate в список удержания, A3.3) —
        оно выведется обратно при save. Загрузка продолжается (НЕ отказ).
      (шелл снимает "v" перед передачей в from_json, либо from_json его игнорирует.)
7. reconcile()-проход всех (в порядке регистрации; зависимости раньше зависимых).
8. Финализация бэкапа (last-known-good, §14 п.1):
      status==LOADED (primary был валиден) -> storage_write_backup один раз.
      status==RECOVERED_BAK -> СНАЧАЛА обычный атомарный save() (переписать битый
      primary восстановленным состоянием + удержанными сиротами), ЗАТЕМ
      storage_write_backup. НЕЛЬЗЯ звать write_backup сразу: на этом пути primary
      БИТЫЙ, копия primary->.bak затёрла бы последний живой бэкап битым файлом ->
      CORRUPT_RESET на следующем буте.
```

### A3.5 save / dirty / debounce

- `save` (внутренний): `cJSON root; features={}; для каждого зарегистрированного
  frag: j=frag.to_json(); инжектнуть "v"=frag.version; features[id]=j`. ЗАТЕМ
  добавить удержанные сироты (A3.3/A3.4 п.6) в `features{}`. Конверт:
  `format=1/save_version=DOC_VERSION/saved_at=WALL_now/save_seq=++s_seq/app=APP_ID/
  build`. `s_seq` инициализирован из загруженного конверта (A3.4 п.4) → следующий >
  предыдущего даже после рестарта. `cJSON_PrintUnformatted` (hot-path, §2) ->
  transform-encode -> `game_storage_write`. Успех -> clear dirty, last_save=MONO_now,
  last_saved_at=saved_at(WALL). Фейл записи -> dirty ОСТАЁТСЯ (ретрай следующим
  тиком); web-фейл -> выставить unpersisted-флаг.
- `mark_dirty`: если был чистым — `dirty_at = MONO_now` (§14 п.6: момент ПЕРВОЙ пометки).
- `tick`: если автосейв на паузе или не dirty — no-op. Иначе save при
  `(MONO_now - dirty_at >= DEBOUNCE_MS) || (MONO_now - last_save >= MAX_INTERVAL_MS)`.
  (debounce/MAX_INTERVAL — на МОНОТОННЫХ ms; saved_at — на WALL ms; см. A3.6.)
- `flush`: синхронный save независимо от дебаунса (для visibility).
- **Мост старого dirty (переходно, A3)**: генерируемый `game.state.set` (DevAPI)
  зовёт СТАРЫЙ `game_state_mark_dirty()`. Диспатч DevAPI по реестру = A5, поэтому в
  A3 DevAPI-запись не марает НОВЫЙ dirty автоматически. Для живого демо автосейва
  шаблон добавляет ОДНУ строку `game_save_mark_dirty()` в существующий UI-мутатор
  (напр. слайдер громкости в `sys_settings_ui` или кнопка «Cycle»). Автосейв-путь в
  тестах драйвится напрямую `game_save_mark_dirty()`+`game_save_tick()`. Полная
  интеграция DevAPI-write -> dirty — A5.

### A3.6 Время: ДВА независимых источника (тест-шов)

- **Монотонные ms** (`s_mono_ms`) — для debounce/MAX_INTERVAL. native: монотонный
  таймер (`nt_time`/`clock`); web: `emscripten_get_now()` (монотонен от загрузки
  страницы).
- **Wall-clock ms** (`s_wall_ms`) — для конвертного `saved_at` (оффлайн-Δt МЕЖДУ
  сессиями). native: `time()`-базированный ms; web: `Date.now()` через EM_JS.
  `emscripten_get_now` НЕ годится для saved_at (обнуляется на перезагрузке
  страницы) — из кандидатов saved_at убран.

Тест-шим инжектит ОБА: `void game_save__set_clocks_for_test(int64_t (*mono)(void),
int64_t (*wall)(void));` (объявить под `#ifdef GAME_SAVE_TESTING`). debounce-тесты
двигают mono; saved_at/save_seq-проверки двигают wall.

### A3.7 Фрагмент `game` (адаптер, game_fragment.c)

```c
#include "game_save.h"
#include "game_state.h"   /* генерируемый монолит */

static cJSON *game_to_json(void)  { return game_state_to_json(&g_game_state); }
static bool   game_from_json(const cJSON *j, char *e, int c) {
    return game_state_from_json(&g_game_state, j, e, c);
}
static void   game_reset(void)    { game_state_init_defaults(&g_game_state); }
/* on_new_game = NULL: дефолты монолита УЖЕ = стартовое состояние шаблона;
   игра заполняет этот хук контентными решениями (золото, меч, стартовый квест). */
static cJSON *game_get_path(const char *sub, char *e, int c) {
    return game_state_get_path_json(&g_game_state, sub, e, c);   /* готово к A5 */
}
static bool   game_set_path(const char *sub, const cJSON *v, char *e, int c) {
    return game_state_set_path_json(&g_game_state, sub, v, e, c); /* готово к A5 */
}
static cJSON *game_schema(void)   { return game_state_schema_json(); }

const GameSaveFragment game_fragment = {
    .id = "game", .version = GAME_STATE_VERSION, .steps = NULL,
    .reset = game_reset, .on_new_game = NULL,
    .to_json = game_to_json, .from_json = game_from_json, .reconcile = NULL,
    .get_path_json = game_get_path, .set_path_json = game_set_path,
    .schema_json = game_schema,
};
```
Замечание: `game_state_from_json` принимает ВНУТРЕННИЙ state-объект (без конверта) —
конверт теперь строит game_save, так что фрагмент отдаёт/принимает плоский payload.
`"v"` штампует шелл. Хук `on_new_game=NULL` в шаблоне корректен (дефолты монолита =
стартовое состояние); поле существует, чтобы игра положила туда контент.

### A3.8 main.c проводка

```c
#if FEATURE_GAME_STATE
#include "game_save.h"
extern const GameSaveFragment game_fragment;
#endif
...
#if FEATURE_GAME_STATE
game_state_init();
game_save_register_fragment(&game_fragment);   /* game — единственный, значит и последний */
game_save_init();
if (!s_fresh_state) {
    game_save_load_result_t r;
    game_save_load(&r);
    /* дефолт-обработка шаблона (§4/Р10): */
    if (r.status == GAME_SAVE_LOAD_CORRUPT_RESET) {
        /* load уже сделал reset()+карантин, но НЕ on_new_game (Р10); new_game() =
           единственный on_new_game на этом пути + возобновляет автосейв. */
        char err[128]; (void)game_save_new_game(err, sizeof err);  /* дефолт: тост + новая игра */
    }
    /* NEWER/RECOVERED_BAK: шаблон может показать тост (advisory), автосейв уже на паузе при NEWER */
}
#ifdef NT_PLATFORM_WEB
game_save_install_web_flush();
#endif
#endif
...
// в frame(), после game-систем:
#if FEATURE_GAME_STATE
if (!s_disable_autosave) game_save_tick();
#endif
```
`--fresh-state` -> `s_fresh_state=true` (пропустить load, начать с reset — либо явно
new_game); `--disable-autosave` -> `s_disable_autosave=true`. Обе больше не no-op.

### A3.8b CMake (`templates/template/CMakeLists.txt`)

Внутри `if(FEATURE_GAME_STATE)` — компиляция shell + фрагмент-адаптер + defines
(правка target_sources аддитивна; `GAME_STORAGE_APP_ID` уже задан в A2, не дублировать):
```cmake
target_sources(${GAME_TARGET} PRIVATE
    src/game_save.c              # A3
    src/game_fragment.c          # A3
)
target_compile_definitions(${GAME_TARGET} PRIVATE
    GAME_SAVE_AUTOSAVE_SLOT="autosave"
    GAME_SAVE_DEBOUNCE_MS=2000
    GAME_SAVE_MAX_INTERVAL_MS=30000
    GAME_SAVE_DOC_VERSION=1
)
```
Ctest (к блоку A1/A2). `test_game_save` линкует СВОЙ фейк-фрагмент (внутри
test-файла), НЕ генерируемый монолит и НЕ `game_fragment.c`:
```cmake
add_executable(test_game_save
    tests/test_game_save.c
    src/game_save.c src/game_storage.c src/game_state_json.c)
target_link_libraries(test_game_save PRIVATE cjson unity)
target_include_directories(test_game_save PRIVATE src)
target_compile_definitions(test_game_save PRIVATE
    GAME_SAVE_TESTING=1
    GAME_STORAGE_APP_ID="template_test"   # game_save.c читает его для поля app конверта
    GAME_SAVE_AUTOSAVE_SLOT="test_slot"
    GAME_SAVE_DEBOUNCE_MS=2000
    GAME_SAVE_MAX_INTERVAL_MS=30000
    GAME_SAVE_DOC_VERSION=1
    _CRT_SECURE_NO_WARNINGS)
set_target_properties(test_game_save PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
add_test(NAME test_game_save COMMAND test_game_save
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
```

### A3.9 Transform-шов (§14 п.15)

- Дефолт: пустая цепочка -> save пишет плоский `cJSON_PrintUnformatted` (начинается
  с `{`), читаемый/редактируемый руками.
- Непустая цепочка: save применяет encode по порядку, префиксует
  `"NTSV1:<id1,id2>:"`; load читает префикс -> decode в реверсе; без префикса ('{')
  -> плоский JSON всегда грузится (подложенный руками файл ест даже сборка с
  трансформами).
- export/import идут ТЕМ ЖЕ путём; для бинарных трансформов — base64 (в A3 цепочка
  пуста, конкретные трансформы сжатия/шифрования ОТЛОЖЕНЫ, см. §Не-входит).

### A3.10 Тесты `test_game_save.c` (Unity, ctest; свой фейк-фрагмент)

Unity в движке собран с `UNITY_EXCLUDE_FLOAT/DOUBLE` (PUBLIC) → float/double-
макросов НЕТ; числа с плавающей точкой сравнивать вручную
(`TEST_ASSERT_TRUE(fabsf(a-b) < eps)`).

Фейк-фрагмент с полями `{int coins; char name[32];}`, to_json/from_json/reset
(coins=0)/on_new_game (coins=100). Инжектируемые ДВА часа (mono+wall). Слот во
временной папке.
1. **Round-trip конверта**: reset -> mark -> flush -> read файла -> проверить
   format/save_version/saved_at/save_seq/app + features.<id>.v + значения; повторный
   load восстанавливает coins/name.
2. **on_new_game FRESH**: файла нет -> load -> status=FRESH, coins==100 (on_new_game
   отработал в load), файл записан.
3. **on_new_game через new_game**: испорченный state -> `game_save_new_game()` ->
   coins==100, автосейв возобновлён.
4. **CORRUPT_RESET (без двойного on_new_game)**: мусор в primary + мусор в .bak ->
   load -> status=CORRUPT_RESET, появился `<slot>.corrupt-*`, coins СБРОШЕНЫ но
   on_new_game НЕ звался (coins==0, НЕ 100), файл НЕ переписан, автосейв на паузе
   (`mark_dirty()+tick()` не пишет). Затем `game_save_new_game()` -> coins==100,
   автосейв возобновлён.
5. **RECOVERED_BAK + следующий бут**: мусор в primary + валидный .bak -> load ->
   status=RECOVERED_BAK, значения из .bak; ПОСЛЕ load primary переписан валидным
   (A3.4 п.8) — повторный load даёт status=LOADED, а `.bak` по-прежнему парсится
   (не затёрт битым).
6. **NEWER (только версии)**: записать сейв с features.<id>.v = frag.version+1 ->
   load -> status=NEWER, файл на диске НЕ изменился (сравнить содержимое до/после),
   export-чтение работает.
7. **Сирота-round-trip (§14 п.16)**: сейв с лишним ключом features."ghost"={...} при
   версиях ≤ моих -> load -> status=LOADED + warn (НЕ NEWER, НЕ отказ); save ->
   ghost присутствует в файле (семантически идентичный JSON, значения сохранены).
8. **save_seq монотонен между сессиями**: save (seq=N) -> новый load -> save ->
   seq > N (счётчик восстановлен из конверта, A3.4 п.4).
9. **Debounce (§14 п.6, на mono-часах)**: mark в t0; tick в t0+DEBOUNCE-1 -> нет
   сейва; tick в t0+DEBOUNCE -> сейв. mark каждый tick -> сейв не «никогда», а по
   MAX_INTERVAL.
10. **flush синхронен (§14 п.5)**: пишет немедленно независимо от дебаунса — это и
    есть натив-покрытие требования «visibility-flush обязан быть синхронным форс-
    сейвом» (rAF-цикл у скрытой вкладки замерзает; web-часть — advisory refresh-чек,
    headless-localStorage ограничен).
11. **export/import round-trip**: export -> new_game(обнулить) -> import -> значения
    восстановлены; import мусора -> false, состояние нетронуто.
12. **transform-шов**: дефолт -> файл начинается с '{'; поставить identity-трансформ
    (XOR/upcase) -> файл несёт префикс "NTSV1:" и грузится обратно.

Плюс **компиляционный смоук**: полный `templates/template` build (game_fragment
адаптер компилируется и линкуется с генерируемым монолитом) в native-debug,
release И devapi-debug (последний — tool parity, компилит генерируемый
`game_state_devapi.c` с compat-обёртками game_storage); ручной прогон бинаря с
`--capture` не падает на load-пути.

### A3.11 Критерии приёмки

- [ ] `ctest -R test_game_save` зелёный, все 12 сценариев + компиляционный смоук.
- [ ] Дефолт-сейв на native — плоский читаемый JSON-файл в `build/saves/autosave.json`.
- [ ] NEWER (только версии) не пишет на диск ни байта (проверено сравнением файла).
- [ ] Сирота-ключ features{} = LOADED+warn (не отказ) и переживает save round-trip
      (§14 п.16, тест #7).
- [ ] CORRUPT_RESET кладёт `.corrupt-*`, НЕ зовёт on_new_game в load (coins==0),
      ставит автосейв на паузу до new_game; on_new_game срабатывает только в new_game.
- [ ] RECOVERED_BAK: primary переписан валидным ДО write_backup; следующий бут =
      LOADED и `.bak` цел (тест #5).
- [ ] save_seq монотонен между сессиями (тест #8).
- [ ] Плохой один фрагмент не роняет остальные (reset_fragments заполнен, соседи целы).
- [ ] Шаблон собирается native + wasm + devapi-debug; web visibility-flush установлен
      (EM_JS-хук); `game_save_tick()` в кадре; `--fresh-state`/`--disable-autosave` реальны.
- [ ] Генерируемые файлы и `game_state_devapi.c` НЕ изменены; старые
      `game_storage_save_json/load_json/resolve_key` живут как compat-обёртки (A2).

### A3.12 Пакет делегирования

**Смешанный: deep-reasoner ведёт load-оркестрацию, fast-worker добивает.** Значимая
доля A3 — не механика, а конечный автомат загрузки (FRESH/RECOVERED/CORRUPT/NEWER +
пофрагментное «не потерять кошелёк»), составной NEWER-детект (Р7), debounce/dirty_at-
тайминг, синхронность visibility-flush, автодетект трансформов, export/import-
валидация. Самые рискованные места: (1) п.5 пофрагментная ветка absent-vs-old-vs-new
и «никогда не all-or-nothing»; (2) NEWER-детект (легко пропустить «незнакомый
фрагмент-ключ» и «v любого фрагмента > кода»); (3) переходный dirty-мост (не
потерять сигнал, не завязаться на A5); (4) web visibility-flush обязан быть
синхронным форс-сейвом (rAF замерзает — §14 п.5). Рекомендация: deep-reasoner
пишет `game_save.c` (или ревьюит реализацию Sonnet против §A3.4/§A3.5 и краш-
матрицы), fast-worker делает `game_fragment.c`, main.c-проводку, CMake, Unity-тест
по готовому списку кейсов.

---

## Общая секция

### Порядок и зависимости

A0 → **A1 → A2 → A3** (строго последовательно; каждый оставляет шаблон живым и
собираемым native+wasm). A1 создаёт лист-тулкит (первый рантайм-потребитель —
game_save в A3, парсинг конверта через `gsj_object_item`/`gsj_read_*`). A2 даёт
атомарный байт-бэкенд, на который встаёт A3. A3 даёт оркестратор. Дальше: A4
(генератор v2 — переключает генерируемые фрагменты на `gsj_*`, перегенерирует
`game` через v2, `g_game_state` умирает, схема шаблона переписывается в v2-вид),
A5 (DevAPI-диспатч по реестру + транзакционный patch поверх ABI фрагмента), A6
(мультифрагмент settings/items/progression + фикстуры + reconcile + скилл).

### Стык с событиями (E1)

`event_system_design_2026-07-06.md` §6: **E1 (шелл-лог событий: кадровая арена +
emit/walk + кап поколений + debug-канарейка + двухфазный агрегатор) стартует
ПОСЛЕ A3**. В спеке A1–A3 события НЕ спекаются. Предусмотренное место стыка:
двухфазный кадр (`game_features_react()`/`game_features_record()`) — это проводка
АГРЕГАТОРА (feature_architecture §2), НЕ game_save; game_save в кадре — это
`game_save_tick()` после game-систем и (когда придёт E1) после фазы record. Никаких
хуков событий в `GameSaveFragment` не добавлять — событийная схема живёт отдельным
рендерером генератора (E2, после A4). Единственное общее — конвертный `save_seq`
(монотонный) уже заложен, но это не событийный `seq`.

### Что СОЗНАТЕЛЬНО НЕ входит (A4+ / события / прочее)

- **Генератор v2 / переключение генерируемого кода на `gsj_*`** — A4 (см.
  §Отступление 1). В A1–A3 генератор и его выхлоп не трогаются.
- **Переписывание схемы шаблона в v2** (снятие numeric id / collections / lifetime /
  per-field devapi / reserved-с-id) — A4 (§14 п.12). В A1–A3 схема остаётся v1.
- **DevAPI-диспатч по реестру + транзакционный patch (gsj/game_save_transact)** — A5.
  В A3 `get_path_json`/`set_path_json` фрагмента проведены, но старый генерируемый
  `game_state_devapi.c` продолжает обслуживать `game.state.*` против `g_game_state`.
- **Пер-фрагментные миграционные чейны + фикстуры + reconcile реальных фич** — A6/S3.
  В A3 `steps=NULL`, `reconcile=NULL` у фрагмента `game`; doc-шаги — пустой шов.
- **События** (E1–E4) — отдельный трек, старт E1 после A3.
- **Конкретные трансформы** (LZ-сжатие, шифрование) — отложены §14 (по боли
  размера/itch-квоты). В A3 только ПУСТОЙ шов + автодетект.
- **Настоящие фичи** settings/items/progression как фрагменты — A6.
- **IndexedDB / unknown-field retention / thumbnail / playtime** — отложены §14.

---

## Открытые вопросы лиду

**Настоящих блокеров нет.** Два вопроса ревью — оба РЕШЕНЫ, оставлены здесь как
след для аудита:

1. **NEWER-детект vs Р7 «сирота удалённой фичи» — РЕШЕНО лидом (state doc §14 п.16).**
   NEWER = ТОЛЬКО версии (format незнаком / save_version > моего / v ЗНАКОМОГО
   фрагмента > версии его кода). Незнакомый фрагмент-ключ из NEWER исключён; при
   версиях ≤ моих он = сирота → warn + загрузка продолжается (Р7). Сироты
   УДЕРЖИВАЮТСЯ verbatim-блобом и выводятся при каждом save (round-trip, чтобы
   пинг-понг версий не терял данные). Реализовано в A3.3 / A3.4 п.6 / A3.5 + тест #7.
   Retired-списков нет.
2. **Порядок A0→A1 — РЕШЕНО (не блокер).** `game_state_migrate_v0_to_v1` — мёртвый
   external без вызовов; A1 собирается и ведёт себя идентично при его наличии;
   правки A1 в CMake аддитивны, устаревшую строку `feature.json:44` A1 правит
   попутно. A0 может идти до/после без риска для A1.

(Иных расхождений дизайна с кодом, требующих решения лида, не найдено. §Отступления
ниже — зафиксированные инженерные решения в рамках дизайна, а не вопросы.)

---

## Отступления от буквы дизайн-дока (с обоснованием)

1. **Генератор переключается на `gsj_*` в A4, НЕ вторым шагом A1.** Задача просила
   снять этот вопрос. Решение: A1 только СОЗДАЁТ `game_state_json.{c,h}`;
   генерируемый код держит свои копии до A4. Обоснование: §7 относит вынос статик-
   хелперов к «Генератору v2», а §14 п.13 явно скоупит переписывание рендереров
   (~70 литеральных вхождений + новый ABI + параметризация префиксом + обязательный
   golden+compile+roundtrip-гейт) в A4. Трогать `render_generic_source` в A1 значит
   править код, который A4 всё равно переписывает, и ломать чистое «A1 = только
   новые файлы, ноль изменений выхлопа». Дедуп в каждом фрагменте всё равно
   происходит на генераторе, а он рождается заново в A4. Значит: переключение —
   A4.

2. **`gsj_transact` не кладётся в `game_state_json` (A1).** Дизайн §1/§8 называет
   «транзакционный validate-copy-swap» частью `gsj_*` и patch — «через
   gsj_transact». Но реальное состояние фрагмента — типизированный C-struct за
   указателями ABI (`to_json`/`from_json`), а не cJSON-дерево; корректный
   пофрагментный откат = `snapshot=to_json(); ...; from_json(snapshot)` — это
   оркестрация уровня ABI фрагмента, а не чистый JSON-примитив. Положить её в
   `game_state_json` означало бы дать листу-тулкиту зависимость на `GameSaveFragment`
   (инверсия ярусов: `game_save` использует `game_state_json`, не наоборот). А
   чистый cJSON-`gsj_transact` не обслуживает C-struct-patch и остался бы без
   потребителя в A1–A3. Поэтому: транзакционный swap строится в A5 (DevAPI patch)
   как `game_save`-примитив поверх ABI фрагмента (§14 п.7 «patch атомарен
   пофрагментно, кросс-фрагментного отката нет» — что и есть уровень game_save).
   `game_state_json` остаётся зависимость-free листом. Отступление в размещении и
   имени, не в поведении.

3. **`i64`-хелперы кладутся в A1, хотя первый рантайм-потребитель — A4.** §14 п.8
   пригвождает i64-провод, а §1 называет «int64-строки» частью контракта
   `game_state_json`. Кладу их в A1 (при создании модуля, с юнит-тестом), чтобы не
   трогать хедер повторно в A4; в A1–A3 они exercised только тестом (конверт A3 не
   нуждается в i64 — `saved_at` влезает в double). Это не изменение поведения
   существующего кода.

4. **`game_fragment.get_path/set_path` проведены уже в A3** (а не только в A5).
   Дёшево (обёртки над готовым генерируемым API) и убирает повторное касание
   адаптера в A5; сам DevAPI-диспатч по реестру остаётся A5. Это опережающая
   проводка, не расширение скоупа A3.

5. **Transform-шов включён в A3 (пустым), а не отложен.** §12 не называет его в
   списке A3, но §14 п.15 требует, чтобы save/load-путь всегда понимал оба формата
   (плоский '{' и "NTSV1:"). Поскольку save/load-конвейер РОЖДАЕТСЯ в A3, шов должен
   родиться вместе с ним, иначе включение трансформа игрой потребует переплавки
   конвейера. Конкретных трансформов A3 не несёт (цепочка пуста) — только шов +
   автодетект + export/import через тот же путь (§14 п.4 «функции в шелле с первого
   web-инкремента»). Это следование §14, а не отступление от него; помечаю явно,
   т.к. §12 молчит.
