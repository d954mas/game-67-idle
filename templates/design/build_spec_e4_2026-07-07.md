# BUILD-SPEC: локальная аналитика-писатель поверх событийной шины, инкремент E4 (2026-07-07)

Имплементационная спецификация. Переводит принятый дизайн в файлы, сигнатуры, CMake, тесты
и критерии приёмки. НЕ меняет дизайн. При расхождении источник истины —
`templates/design/event_system_design_2026-07-06.md` (главный: §4 «Локальная аналитика» =
файл в build/analytics/ native / память web; §5 события транзиентны; §6 план E4 =
аналитика-писатель + встроенный тип `log`; §7 Q3 f64 / Q4 bytes), затем
`build_spec_e1_2026-07-06.md` (транспорт game_events — ЗАМОРОЖЕН), `build_spec_e2_2026-07-06.md`
(генерённые дескрипторы `<frag>_ev_descs` / FT-enum / оффсеты — E4 их ПОТРЕБИТЕЛЬ),
`build_spec_e3_2026-07-07.md` (рендерер `game_event_render.{c,h}` + реестр `hash→desc` +
NT_HASH_LABELS build-config — **E4 ПЕРЕИСПОЛЬЗУЕТ рендерер, не дублирует**),
`features/game-state/references/state_system_design_2026-07-06.md` (§10/§14 п.3 web-квота
localStorage 5-10МБ ОБЩАЯ на itch → append-поток в localStorage ЗАПРЕЩЁН).

Исполнитель может писать код по этому документу, НЕ открывая историю обсуждений.

---

## 0. Предпосылки и рамки (проверено по дереву HEAD 2026-07-07)

- **E1 + E2 УЖЕ в дереве; E3 ложится ПАРАЛЛЕЛЬНО, E4 реализуется ПОСЛЕ E3.** E4 опирается на
  E3-артефакты как на данность (см. «Зависимость от E3» ниже). Живой путь E1/E2:
  `game_events.{c,h}` (`game_event_emit`, `game_event_log(&n)`, `game_events_tick()`,
  `game_events_dropped()`, двухфазный кадр, `game_event_register_type_name`); shared-контракт
  `src/game_event_desc.h` (`game_event_desc_t {name, payload_size, fields, field_count}`,
  `game_event_field_t {name, type, offset, len_offset}`, FT-enum `BOOL/INT/I64/FLOAT/STRING/
  HASH/BYTES`); генерённый `game_state_events.gen.{h,c}` с таблицей `game_ev_descs[]` /
  `game_ev_desc_count` / `game_ev_register()`; `main.c:363` уже зовёт `game_ev_register()` под
  `#if FEATURE_GAME_STATE`.
- **Двухфазный кадр УЖЕ проведён (main.c:249-261).** Фаза RECORD — дом рекордеров;
  `game_features_record(&s_world)` зовётся РОВНО ОДИН раз/кадр, арена ЖИВА до
  `game_event_frame_reset()` (main.c:261). **E4 садится ВТОРОЙ строкой в
  `game_features_record`** (E3 — первой): по HEAD скелет несёт якорь
  `TODO(E3/E4)` (game_features.c:47); после E3 там будет вызов рекордера-хвоста +
  `/* TODO(E4): analytics recorder (event §6) */` (E3 §E3.6) — **E4 закрывает якорь E4 одной
  гейт-строкой** (§E4.7).
- **Зависимость от E3 (ЯВНАЯ, блокер старта E4).** E4 ПЕРЕИСПОЛЬЗУЕТ из E3:
  1. `src/game_event_render.{c,h}` — универсальный рендер события в компактный JSON по
     дескриптору (`int game_event_render(const game_event_t *e, const game_event_desc_t *desc,
     char *out, int cap)`; всегда валидный JSON; строки/hash/bytes/усечение обработаны). E4 зовёт
     его как есть, НЕ меняет сигнатуру.
  2. `NT_HASH_LABELS=1` на движковый `nt_hash` в devapi-сборках (E3 §E3.7) — читаемые имена
     hash-полей/типов достаются АНАЛИТИКЕ БЕСПЛАТНО (аналитика по умолчанию едет в devapi-семье,
     см. §E4.2). Без флага — хекс-фолбэк, поведенчески корректно.
  3. Публичный API реестра E3 `game_events_devapi_register_descs(descs, count)` — E4 использует
     его ТОЛЬКО чтобы зарегистрировать дескриптор встроенного `log`-типа в хвост (§E4.B); файл
     `game_events_devapi.c` E4 НЕ правит.
  E4 НЕ МОЖЕТ стартовать, пока E3 не приземлил `game_event_render.{c,h}`. Если на момент старта
  E4 рендерера нет — СТОП, доложить лиду (не дублировать рендер).
- **Часы wall-clock (проверено).** Общего движкового wall-clock helper'а в шаблоне нет:
  `nt_time_now()` — МОНОТОННЫЕ секунды (double), не wall. `game_save.c` берёт wall приватным
  `wall_now()` → native `time(NULL)*1000` (game_save.c:95), web `Date.now()` через EM_JS
  (game_save.c:90-92); эти статики ПРИВАТНЫ. `game_save_last_saved_at()` публичен, но = 0 до
  первого сейва → не годится для timestamp старта сессии. `s_save_seq` ПРИВАТЕН, аксессора нет.
  **Вывод: E4 держит СВОИ tiny wall-часы** (native `time(NULL)*1000`, web `Date.now()` EM_JS —
  зеркало game_save.c:90-95, ~5 строк), т.к. game_save.{c,h} ЗАМОРОЖЕН (аксессор save_seq туда
  добавить нельзя). save_seq в заголовок потока НЕ входит (см. §E4.3).
- **game_storage — слот-снапшот, НЕ append (проверено, РЕШЕНО).** `game_storage_write`
  (game_storage.c:328) = атомарный replace (native `write_file_atomic` tmp→primary,
  MoveFileEx REPLACE|WRITE_THROUGH) / web `localStorage.setItem` per-key. Это ПЕРЕЗАПИСЬ ВСЕГО
  значения — неверно для append-потока: (а) native replace каждой записи = O(n²) на растущем
  логе; (б) web localStorage per-key НЕ умеет append и ОБЩАЯ itch-квота 5-10МБ (state §14 п.3)
  → растущий поток убьёт настоящий сейв. **Аналитика = append-only поток, НЕ слот →
  СВОЙ путь записи** (native `fopen(...,"ab")` буфер+flush; web = in-memory ring). `game_storage`
  НЕ используется вовсе.
- **Сборка:** game-таргет `-Werror` + `nt_set_warning_flags` (`-Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -Wundef`, `-Wno-unused-parameter`).
  u64→double в JSON-числах — явные касты; `PRIx64`/`%zu` при snprintf; чтение payload'а —
  через уже готовый `game_event_render` (E4 сам байты payload'а НЕ парсит).
- **Один тред.** Буфер/реестр/файл-хендл — file-static, потокобезопасности не требуют.
- **Среда:** движок READ-ONLY; wasm-devapi линк КРАСНЫЙ на HEAD (движок, доложено лиду) →
  wasm-гейт E4 = КОМПИЛЯЦИЯ нового TU под `wasm-devapi-debug` (не полный линк).

### Что E4 делает (ровно)

**E4.A — аналитика-писатель (основное):**
1. Рукописный TU `src/game_analytics.{c,h}` (гейт `#if FEATURE_GAME_ANALYTICS`): RECORD-фазовый
   рекордер, который по `game_event_log()` рендерит КАЖДОЕ событие кадра **через
   `game_event_render` (E3, переиспользование)** в строку NDJSON и льёт в буфер; буфер сбрасывается
   в sink по порогу (native — append-файл `build/analytics/session-<wall>.ndjson`; web — in-memory
   ring; test — инъекция). Заголовок сессии, политика дропа/переполнения, health-счётчик.
2. Проводка: `game_features.c` (одна гейт-строка рекордера в фазе RECORD, рядом с E3),
   `main.c` (include + регистрация дескрипторов + init/flush/shutdown).
3. CMake: `FEATURE_GAME_ANALYTICS` гейт + TU в game-таргет + ctest `test_game_analytics`.

**E4.B — встроенный тип `log {string}` (малое, отделяемо — см. Q5):**
4. Рукописный TU `src/game_log.{c,h}` (безусловный leaf): emit-хелпер `game_log_emit(const char*)`
   + дескриптор `game_log_desc` (одно STRING-поле `msg`, инлайн-оффсет) + таблица + регистратор
   меток — для ad-hoc отладочных сообщений game glue БЕЗ заведения схемы (дизайн §3). Хвост E3 и
   аналитика E4 рендерят его generic'ом (дескриптор зарегистрирован).

### FROZEN в E4 (не менять)

- `templates/template/src/game_events.{c,h}` — E1 транспорт.
- `templates/template/src/game_event_desc.h` — E2 shared-контракт (E4 ПОТРЕБИТЕЛЬ).
- **Генератор `generate_state.py` + golden `*_state_events.gen.{h,c}` — НЕ трогать.** Дескрипторы
  E2 достаточны (E4 их ПОТРЕБИТЕЛЬ). Если кажется, что нужен новый тип поля / правка генератора —
  **СТОП и вопрос лиду** (E4 не расширяет схему).
- `templates/template/src/game_save.{c,h}`, `game_save_devapi.c` — ЗАМОРОЖЕНЫ (save_seq-аксессор
  НЕ добавлять; wall-часы E4 держит свои, §0).
- **E3-файлы `game_event_render.{c,h}` (переиспользуются как есть, сигнатура НЕ меняется),
  `game_events_devapi.{c,h}` (E4 только ЗОВЁТ публичный `..._register_descs`, файл не правит).**
  Если аналитике «нужно» поменять рендерер (напр. чтобы bytes НЕ писались) — **СТОП и вопрос**
  (см. §E4.4 / Q3): смена сигнатуры рендерера = правка замороженного E3.
- `game_state*.gen.*`, движок, `games/**`.

---

## E4.A. АНАЛИТИКА-ПИСАТЕЛЬ

### E4.1 Файлы

**Новые (рукописные):**
- `templates/template/src/game_analytics.h` — API писателя (§E4.5).
- `templates/template/src/game_analytics.c` — рекордер + буфер + sink + заголовок (§E4.5).
- `templates/template/tests/test_game_analytics.c` — Unity ctest (инъекция sink+часов) (§E4.9).

**Изменяемые:**
- `templates/template/src/features/game_features.c` — одна гейт-строка рекордера + include (§E4.7).
- `templates/template/src/main.c` — include + регистрация дескрипторов + init/shutdown (§E4.7).
- `templates/template/CMakeLists.txt` — `FEATURE_GAME_ANALYTICS` гейт + TU + ctest (§E4.8).

**Не трогать:** всё из FROZEN (§0). Генератор/golden — 0 правок. Смоук-бот — 0 правок (E4 НЕ
добавляет DevAPI-команду, §E4.6).

### E4.2 Гейт `FEATURE_GAME_ANALYTICS` (дефолт: едет в devapi-семье)

**Решение (дефолт, обратимо):** аналитика — dev/балансировочный инструмент. CMake-опция
`GAME_ANALYTICS_ENABLED`, **дефолт = значение `GAME_DEVAPI_ENABLED`** (ON в devapi-сборках
native+web, OFF иначе). Compile-define `FEATURE_GAME_ANALYTICS` определён ВСЕГДА (=1 при ON,
=0 при OFF — HIGH-A; §E4.8 шаг 2). Матрица:

| Пресет | Аналитика | Путь |
|---|---|---|
| `native-debug` (человек) | **OFF** | — (человеческий билд чист, никаких файлов) |
| `devapi-debug` (агент/баланс) | **ON** | `build/analytics/session-<wall>.ndjson` (native append-файл) |
| `native-release` | **OFF** | — (приватность игрока + диск) |
| `wasm-devapi-debug` | **ON** | in-memory ring + export-аксессор (web = память, дизайн §4) |
| `wasm-release` | **OFF** | — |

**Почему дефолт = devapi-семья:** (1) `NT_HASH_LABELS=1` в devapi-сборках уже включён (E3) →
читаемые метки бесплатно; (2) devapi-debug — агентский балансировочный билд, где послематчевый
анализ ценен; человеческий native-debug остаётся чист (меньше сюрпризов, чем аналитика-по-
умолчанию-ON). **Независимая аналитика (без devapi) РЕАЛЬНО работает** (MED-D фикс): рендерер E3
компилится под `(GAME_DEVAPI_ENABLED OR GAME_ANALYTICS_ENABLED)` (§E4.8), поэтому
`-DGAME_ANALYTICS_ENABLED=ON` при `GAME_DEVAPI_ENABLED=OFF` собирает писатель + рендерер (флаг
НЕ немой no-op). Без флага NT_HASH_LABELS (не-devapi) hash-значения/сырые типы уходят в хекс —
поведенчески корректно. См. Q1.

Из этого следует: TU `game_analytics.c` живёт в блоке `if(FEATURE_GAME_STATE)` НА УРОВНЕ
`if(GAME_ANALYTICS_ENABLED)` (НЕ вложен в `if(GAME_DEVAPI_ENABLED)` — иначе флаг мёртв при devapi
OFF, MED-D); рендерер E3 переезжает в общий `if(GAME_DEVAPI_ENABLED OR GAME_ANALYTICS_ENABLED)`
блок (один листинг, без дубля). Аналитике нужны cJSON (линкуется под FEATURE_GAME_STATE),
`game_ev_descs` (генерятся под FEATURE_GAME_STATE) и рендерер E3 → **аналитика КОНСТРУКТИВНО
требует FEATURE_GAME_STATE** (CMake форсит OFF при state OFF, §E4.8; то же обоснование, что у E3
§E3.16 отступление 3). **Макрос `FEATURE_GAME_ANALYTICS` определён ВСЕГДА (1/0)** — game_features.c
и main.c компилятся безусловно и несут `#if FEATURE_GAME_ANALYTICS`; неопределённый макрос под
`-Wundef`+`-Werror` = ошибка компиляции native-debug/release (HIGH-A).

### E4.3 Формат = NDJSON; заголовок сессии; переиспользование рендерера

**Формат: NDJSON** (одна JSON-строка на событие, `\n`-разделитель). Почему: append-дружелюбен,
greppable, стандарт для event-потоков, и — главное — **переиспользует `game_event_render`
(событие→компактный JSON по дескриптору) БЕЗ дублирования рендера** (мандат оркестратора). Каждая
строка события уже самодостаточна: `game_event_render` эмитит `{seq, tick, type, <поля...>}` →
строки самокоррелируемы (seq/tick в каждой).

**Заголовок сессии (ПЕРВАЯ строка потока, kind:"header"):** пишется ОДИН раз в `game_analytics_init`.
Форма (через cJSON — корректный escaping под `-Werror`):
```json
{"schema":"analytics.v1","kind":"header","app":"<GAME_STORAGE_APP_ID>","build":"<GAME_ANALYTICS_BUILD>","started_at":<wall_ms>}
```
- `app` = `GAME_STORAGE_APP_ID` (глобальный compile-define, "template"; проверено CMakeLists:261).
- `build` = `GAME_ANALYTICS_BUILD` (свой `#ifndef … "0"` дефолт; `GAME_SAVE_BUILD` — приватный
  game_save-only define, глобально НЕ виден, §0 grep).
- `started_at` = wall ms из СВОИХ часов E4 (§0; native `time(NULL)*1000`, web `Date.now()`).
- **`save_seq` НЕ включается** — аксессора нет, game_save заморожен (добавить = правка frozen).
  Корреляция с сейвом — по `started_at`+`app`+`build`; при необходимости save_seq — будущий
  аксессор в game_save (вне E4). См. Q4.

### E4.4 Что пишется: ВСЕ события; bytes; приватность/размер

- **Дефолт: ВСЕ события кадра** (каждое из `game_event_log`). Философия дизайна (§1/§4): события =
  единственный источник фактов для аналитики; поток = сырой факт-лог, фильтрация/агрегация =
  downstream-анализ, которым E4 ЯВНО НЕ является («писатель, не пайплайн»). Opt-in фильтр-список
  (аналитический контракт) = лишняя конфиг-поверхность + per-game контракт → **отвергнут для E4**
  («subtract not add»). Тип-фильтр аналитик делает клиентски (`grep '"type":"items.txn"'`).
  Объём под контролем: оффлайн-догон эмитит АГРЕГАТЫ (одно `offline_gold +N`, дизайн §2/Q2) +
  байт-кап сессии (§E4.5). **Подтвердить у лида (LOW-O):** raw-firehose (все события) = замысел —
  §4 читается И как СЕЛЕКТИВНОСТЬ («копирует/агрегирует НУЖНОЕ»); связано с MED-I/Q6 (firehose
  быстрее набивает байт-кап → раньше срабатывает keep-oldest-stop на native). См. Q2.
- **bytes-поля: пишутся как `{size, hex}` (truncated), как их рендерит `game_event_render`
  (E3).** Дизайн §7 Q4 говорит «аналитика игнорирует bytes» — но E3 сделал ОДИН общий рендерер,
  где bytes = `{size, hex}` (hex усечён до `GAME_EVENT_RENDER_HEX_MAX`≈48Б). Заставить аналитику
  ПРОПУСКАТЬ bytes = добавить рендереру режим/флаг = **смена сигнатуры замороженного E3-рендерера**
  → отвергнуто. Реюз как есть: bytes = маленький `{size,hex}` — безвреден. Сознательное
  отступление от буквы §7 Q4, см. §E4.13 / Q3.
- **Приватность/размер:** строки пишутся как есть (escaped рендерером). Аналитический поток —
  dev-артефакт (build/, gitignored, не шипается в release — гейт OFF в release). Игрок-facing
  утечки нет (release не пишет). Крупные строки безопасны — усечение слота ≤`GAME_ANALYTICS_LINE_MAX`
  делает рендерер (валидный `truncated:true`).

### E4.5 `game_analytics.{c,h}` — контракт

**`game_analytics.h` (форма):**
```c
#ifndef GAME_ANALYTICS_H
#define GAME_ANALYTICS_H

#if FEATURE_GAME_ANALYTICS

#include <stddef.h>
#include <stdint.h>
#include "game_event_desc.h" /* game_event_desc_t */

/* Local analytics writer over the event bus (event_system_design §4). RECORD-phase
   recorder: renders EVERY frame event via game_event_render (E3, reused) into an NDJSON
   stream. native -> build/analytics/session-<wall>.ndjson (append); web -> in-memory ring;
   test -> injected sink. Buffered (never a blocking write per frame). Not a save slot. */

/* Register a fragment's generated descriptor table (<frag>_ev_descs / _count) into the
   writer's hash->desc lookup (typed rendering). Call once after nt_hash_init, same site as
   the E3 tail registration. Duplicate type-hash => debug assert (event §1 collision guard). */
void game_analytics_register_descs(const game_event_desc_t *const *descs, int count);

/* Open the stream: write the session header line. Call once after descriptors registered
   (before the frame loop). No-op if already open. */
void game_analytics_init(void);

/* RECORD-phase recorder. Walk game_event_log(), render each event by descriptor into an
   NDJSON line, append to the buffer; flushes to the sink on threshold. Call ONCE per frame
   from game_features_record (arena alive). No-op until init. */
void game_analytics_record(void);

/* Force-flush the buffer to the sink. Call on shutdown; native only needs it (web ring is
   already live in memory). */
void game_analytics_flush(void);

/* Final flush + close (native: fclose). */
void game_analytics_shutdown(void);

/* Cumulative events dropped by a REAL failure (rendered line > buffer, session byte-cap hit,
   or write error) -- health metric, mirror of game_events_dropped(); 0 on a healthy run. */
uint64_t game_analytics_dropped(void);

/* Cumulative ROUTINE web-ring evictions (oldest whole lines rolled off the in-memory ring).
   Kept SEPARATE from dropped() (LOW-J) so "dropped()==0 on a healthy run" stays true even
   as the bounded web ring rolls. Always 0 on native (append-file). Mirror of E3 tail evicted. */
uint64_t game_analytics_evicted(void);

#if defined(__EMSCRIPTEN__)
/* Web export: the in-memory NDJSON ring as a malloc'd NUL string (caller frees), for a game
   "export analytics" affordance / future DevAPI command. NULL if empty. */
char *game_analytics_export(void);
#endif

#ifdef GAME_ANALYTICS_TESTING
/* Test seams (declared only under GAME_ANALYTICS_TESTING): inject the sink (capture instead
   of file/ring) and the wall clock. Call BEFORE game_analytics_init. */
typedef void (*game_analytics_sink_fn)(const char *bytes, size_t len);
void game_analytics__set_sink_for_test(game_analytics_sink_fn sink);
void game_analytics__set_clock_for_test(int64_t (*wall)(void));
#endif

#endif /* FEATURE_GAME_ANALYTICS */
#endif /* GAME_ANALYTICS_H */
```

**`game_analytics.c` — контракт реализации.** Инклюды (под гейтом): `"game_analytics.h"`,
`"game_events.h"` (`game_event_log`), `"game_event_render.h"` (E3), `"hash/nt_hash.h"`
(`nt_hash64_str`), `"log/nt_log.h"` (dev-warn), `"core/nt_assert.h"`, `"cJSON.h"` (заголовок),
`<stdio.h>`/`<string.h>`/`<stdint.h>`/`<stdbool.h>`; native — `<time.h>`; web — `<emscripten.h>`.

**Config (#define с дефолтами, переопределяемы игрой/ctest):**
```c
#ifndef GAME_ANALYTICS_LINE_MAX      /* cap одной отрендеренной строки (== E3 tail entry) */
#define GAME_ANALYTICS_LINE_MAX 512
#endif
#ifndef GAME_ANALYTICS_BUF_BYTES     /* аккумулирующий буфер строк */
#define GAME_ANALYTICS_BUF_BYTES (16u * 1024u)
#endif
#ifndef GAME_ANALYTICS_FLUSH_BYTES   /* порог сброса в sink (< BUF_BYTES) */
#define GAME_ANALYTICS_FLUSH_BYTES (12u * 1024u)
#endif
#ifndef GAME_ANALYTICS_MAX_BYTES     /* байт-кап сессии (native файл); 0 = без капа */
#define GAME_ANALYTICS_MAX_BYTES (256u * 1024u * 1024u)
#endif
#ifndef GAME_ANALYTICS_WEB_RING_BYTES /* in-memory ring (web) */
#define GAME_ANALYTICS_WEB_RING_BYTES (1024u * 1024u)
#endif
[ПОПРАВЛЕНО 2026-07-07, решение лида: кап = чистый предохранитель от разгона для дебаг-артефакта,
дефолт поднят 8МБ → 256МБ (штатно недостижим); web-ring 256КБ → 1МБ (~7к событий, память вкладки
не проблема). Оба остаются per-game `#ifndef`-переопределяемыми.]
#ifndef GAME_ANALYTICS_DESC_REG_CAP
#define GAME_ANALYTICS_DESC_REG_CAP 64
#endif
#ifndef GAME_ANALYTICS_BUILD
#define GAME_ANALYTICS_BUILD "0"
#endif
```

**Реестр hash→desc (file-static, зеркало E3 §E3.5 — ~15 строк).** E3-реестр приватен
(`game_events_devapi.c`), заморожен → E4 держит СВОЙ (сознательное дублирование ~15 строк, §E4.13):
```c
typedef struct { uint64_t hash; const game_event_desc_t *desc; } an_reg_entry_t;
static an_reg_entry_t s_reg[GAME_ANALYTICS_DESC_REG_CAP];
static int s_reg_count;
```
`game_analytics_register_descs`: для каждого `d`: `uint64_t h = nt_hash64_str(d->name).value;`
debug-assert на дубль; `s_reg_count>=CAP` → `nt_log_warn`+стоп; иначе добавить.
`static const game_event_desc_t *reg_find(nt_hash64_t t)`: линейный скан по `t.value` (мал → дёшево).

**Sink (function-pointer; платформа + тест едины):**
```c
static game_analytics_sink_fn s_sink; /* set in init if NULL; test injects before init */
static uint8_t  s_buf[GAME_ANALYTICS_BUF_BYTES];
static size_t   s_buf_len;
static uint64_t s_written_total;  /* для байт-капа сессии (native) */
static uint64_t s_dropped;        /* health: РЕАЛЬНЫЕ сбои (строка>буфера/кап/ошибка записи) */
static uint64_t s_evicted;        /* LOW-J: рутинное вытеснение web-ring (НЕ dropped) */
static bool     s_open, s_capped, s_warned;
static int64_t  s_started_at;
#if !defined(__EMSCRIPTEN__)
static FILE    *s_file;           /* LOW-L: только native (иначе -Wunused на web) */
#endif
```
(`s_reg`/`s_reg_count` — см. реестр выше.)
- **native default sink** (лениво открывает файл на первом flush): `if(!s_file){ ensure_dirs(
  "build/analytics"); s_file=fopen(path,"ab"); if(!s_file){ s_dropped++; warn_once(); return; } }
  fwrite(bytes,1,len,s_file); fflush(s_file);`.
  - **`ensure_dirs` = ПОСЕГМЕНТНЫЙ mkdir (MED-F), зеркало `game_storage.c:149-171`
    `ensure_parent_dirs` — НЕ однодиректорный `make_dir_if_needed` (:136-147).** Причина:
    `game_analytics_init` бежит ДО `game_save_init` → `build/` может ещё не существовать →
    `_mkdir("build/analytics")` = ENOENT → весь поток в дроп. Создаём `build/`, затем
    `build/analytics/`. (game_storage-хелперы static → не переиспользуем, реплицируем ~15 строк.)
  - **имя файла (LOW-K): `build/analytics/session-<started_at>-<pid>.ndjson`** — pid-суффикс
    разводит два запуска в ту же секунду (`time(NULL)` = секундное разрешение → коллизия/
    интерливинг без него). pid: Win `_getpid()` (`<process.h>`), POSIX `getpid()` (`<unistd.h>`)
    под платформенными гардами.
  - fflush на КАЖДОМ flush (flush редок = по порогу/shutdown → амортизировано; ограничивает
    потерю на краше).
- **web default sink**: аппендит в фикс-байт-ring `GAME_ANALYTICS_WEB_RING_BYTES`, вытесняя
  СТАРЕЙШИЕ ЦЕЛЫЕ строки (инвариант: export всегда валидный NDJSON — вытеснение по `\n`); рутинное
  вытеснение → **`s_evicted++` (LOW-J), НЕ `s_dropped`** (dropped = только реальные сбои → контракт
  «0 on healthy run» правда; keep-newest, дизайн §4 «web — память»).
- **test sink** (инъекция): захват в тест-буфер; native fopen НЕ триггерится (s_sink уже задан до
  init → init не переопределяет).

**Инклюды native-пути (MED-F):** `<errno.h>` (EEXIST) + `<direct.h>` (Win `_mkdir`/`_getpid`) |
`<sys/stat.h>` (POSIX `mkdir`) + `<unistd.h>` (POSIX `getpid`) под `#ifdef _WIN32`/`#else` гардами.

**`warn_once` (LOW-N): ЕДИНАЯ сигнатура во всём TU** — `static void warn_once(const char *msg)`
(ставит `s_warned=true`, `nt_log_warn("game_analytics: %s", msg)` один раз за сессию). Все места
(строка>буфера, кап, ошибка открытия) зовут её одинаково.

**wall-часы E4 (§0, зеркало game_save.c:90-95):**
```c
#if defined(GAME_ANALYTICS_TESTING)
static int64_t (*s_wall)(void);
static int64_t wall_now(void){ return s_wall ? s_wall() : 0; }
#elif defined(__EMSCRIPTEN__)
/* clang-format off */
EM_JS(double, game_analytics_web_now_ms, (void), { return Date.now(); }) /* LOW-M: precedent game_save.c:89-91 */
/* clang-format on */
static int64_t wall_now(void){ return (int64_t)game_analytics_web_now_ms(); }
#else
static int64_t wall_now(void){ return (int64_t)time(NULL) * 1000; }
#endif
```

**`game_analytics_init`:**
```c
if (s_open) return;
if (!s_sink) s_sink = default_platform_sink;   /* native lazy-file / web ring; test уже задал */
s_started_at = wall_now();
s_buf_len = 0; s_written_total = 0; s_capped = false; s_warned = false;
/* header via cJSON (§E4.3) -> append в s_buf -> flush() */
s_open = true;
```

**`game_analytics_record`** (ОДИН линейный проход/кадр — лог свежий каждый кадр, курсор НЕ нужен;
каскады уже в логе, react достиг фикспойнта до RECORD):
```c
if (!s_open || s_capped) return;
int n; const game_event_t *log = game_event_log(&n);
for (int i = 0; i < n; ++i) {
    char line[GAME_ANALYTICS_LINE_MAX];
    int len = game_event_render(&log[i], reg_find(log[i].type), line, (int)sizeof line); /* E3 reuse */
    if (len <= 0) continue;
    size_t need = (size_t)len + 1u;                 /* + '\n' */
    if (need > sizeof s_buf) { s_dropped++; warn_once(); continue; } /* строка > буфера */
    if (s_buf_len + need > sizeof s_buf) game_analytics_flush();     /* освободить место */
    memcpy(s_buf + s_buf_len, line, (size_t)len);
    s_buf_len += (size_t)len;
    s_buf[s_buf_len++] = (uint8_t)'\n';
}
if (s_buf_len >= GAME_ANALYTICS_FLUSH_BYTES) game_analytics_flush();
```
**`game_analytics_flush`:** если `s_buf_len==0` → return. Байт-кап сессии:
`if (GAME_ANALYTICS_MAX_BYTES && s_written_total >= GAME_ANALYTICS_MAX_BYTES) { s_capped=true;
warn_once("session byte cap hit"); s_buf_len=0; return; }` иначе
`s_sink(s_buf, s_buf_len); s_written_total += s_buf_len; s_buf_len = 0;`.
**СЕМАНТИКА ПОТЕРИ ПРИ КАПЕ РАЗНАЯ ПО ПЛАТФОРМАМ (MED-I, задокументировано, дефолт):** native
байт-кап = **keep-oldest-stop** (при `MAX_BYTES` пишущий ОСТАНАВЛИВАЕТСЯ → теряется ХВОСТ сессии,
`s_capped`); web-ring = **keep-newest-roll** (вытесняет СТАРЕЙШЕЕ → теряется НАЧАЛО). Это
противоположные политики. Дешёвый override: `GAME_ANALYTICS_MAX_BYTES=0` (native без капа, полный
файл). Дефолт оставлен как есть, ждёт подтверждения лида — см. Q6.
**`game_analytics_shutdown`:** `game_analytics_flush();`
`#if !defined(__EMSCRIPTEN__) if(s_file){ fclose(s_file); s_file=NULL; } #endif` `s_open=false;`
обнулить статики **включая `s_reg_count=0` (MED-G)** — иначе `setUp` второго ctest-кейса дубль-
регистрирует `mini.cell_spawned` → debug-assert коллизии (регистрация идёт ДО init → init реестр
сбросить не может; сброс = дом shutdown). Также `s_buf_len=0, s_written_total=0, s_dropped/
s_evicted` НЕ трогать между сессиями внутри процесса? — в ctest каждый кейс = init/shutdown цикл,
поэтому shutdown обнуляет ВСЁ (реестр, буфер, счётчики, s_capped/s_warned) для чистого следующего
setUp.

Транзиентные cJSON заголовка (build+print) освобождаются в init; `game_event_render` (E3) сам
чистит свои cJSON. Никакой блокирующей записи каждый кадр (обычно только `memcpy` в `s_buf`;
`fwrite`/`fflush` — по порогу).

### E4.6 Никакой DevAPI-команды (нет tool-parity/смоук-churn)

E4 — ПАССИВНЫЙ локальный писатель, НЕ «op»-поверхность для клиентов → **НЕ добавляет DevAPI-команду
→ смоук-бот и его тесты НЕ трогаются** (в отличие от E3). Hard-invariant tool-parity не задет:
аналитика не создаёт операцию, доступную одному клиенту и недоступную другому (наблюдаемая лента =
E3 `game.events.tail`). Web-export = C-аксессор `game_analytics_export`, помечен как **ИНЕРТНЫЙ**
в E4 (не вызывается ни одним клиентом).
**Forward-констрейнт (LOW-P, tool parity — hard invariant):** будущий потребитель web-export'а
ОБЯЗАН прийти на ОБА клиента ОДНОВРЕМЕННО — кнопка/аффорданс site-страницы И DevAPI-команда
(`analytics.export` + смоук triple-sync), одна op-поверхность, два равных клиента. Пока такого
потребителя нет — `game_analytics_export()` честно инертен (объявлен, не разведён по клиентам).

### E4.7 Проводка: game_features.c + main.c

**game_features.c** (после E3; закрытие якоря E4 — НЕ структурная правка). Вверху рядом с
E3-инклюдом:
```c
#if FEATURE_GAME_ANALYTICS
#include "game_analytics.h"
#endif
```
`game_features_record` — E4 ТОЛЬКО ДОБАВЛЯЕТ свой `#if FEATURE_GAME_ANALYTICS`-блок на место
якоря `/* TODO(E4): analytics recorder */`, оставленного E3; **E3-строки НЕ трогает** (HIGH-C).
Пост-фиксный E3 приземлил рекордер-хвост под ДВОЙНЫМ гейтом `#if FEATURE_GAME_STATE &&
NT_DEVAPI_ENABLED` (E3 MED-2, gate-independence) — показано дословно:
```c
void game_features_record(World *w) {
    (void)w;
#if FEATURE_GAME_STATE && NT_DEVAPI_ENABLED
    game_events_devapi_record();     /* E3: DevAPI tail (E3-строка, НЕ трогать) */
#endif
#if FEATURE_GAME_ANALYTICS
    game_analytics_record();         /* E4: local analytics NDJSON writer (ДОБАВЛЯЕТ E4) */
#endif
}
```
Порядок E3/E4 безразличен (оба — чистые читатели `game_event_log`, не эмитят; emit в RECORD =
assert). Арена жива до `frame_reset` (main.c:261).

**main.c:**
1. Include (рядом с `game_events.h`, main.c:44):
```c
#if FEATURE_GAME_ANALYTICS
#include "game_analytics.h"
#endif
```
2. Регистрация дескрипторов + init — ВНУТРИ существующего `#if FEATURE_GAME_STATE` блока
   (main.c:362-364, где уже `game_ev_register()`; `game_ev_descs`/`_count` из
   `game_state_events.gen.h`, включён main.c:54). Аналитический `#if FEATURE_GAME_ANALYTICS`-подблок
   вложен в `#if FEATURE_GAME_STATE` (game_ev_descs существует только там; CMake форсит analytics
   OFF при state OFF, §E4.8):
```c
#if FEATURE_GAME_STATE
    /* ... game_ev_register(); (существующая E2-строка main.c:363) ... */
#if FEATURE_GAME_ANALYTICS
    game_analytics_register_descs(game_ev_descs, game_ev_desc_count); // E4: fragment descs (append)
    game_analytics_init();                                            // E4: open stream + header
#endif
#endif /* FEATURE_GAME_STATE */
```
   (Конструктор-паттерн, генератор-free: шаблон регистрирует только фрагмент `game`. Игра с
   событиями в другом фрагменте добавляет свою строку — как для E3-хвоста. Регистрация log-типа —
   §E4.B.2, тоже в этот блок.)
3. Shutdown (native-ветка, рядом с `game_events_shutdown()` main.c:464 — ПЕРЕД ним, чтобы
   финальный flush прошёл, пока event-инфра ещё есть; аналитика читает только `game_event_log`,
   но порядок симметричен init):
```c
#if FEATURE_GAME_ANALYTICS
    game_analytics_shutdown(); // E4: final flush + close
#endif
```
   (Web: teardown в main.c НЕ исполняется — web-ring живёт в памяти до выгрузки страницы, export
   по требованию; персистентности web-аналитики НЕТ = best-effort, дизайн §4 «web — память».
   Отдельный web visibility-flush НЕ нужен: нечего персистить.)

### E4.8 CMake (`templates/template/CMakeLists.txt`)

1. **Опция — зависимый дефолт как ОБЫЧНАЯ переменная, НЕ FORCE-cache (MED-D cache-footgun)**
   (рядом с блоком GAME_DEVAPI_ENABLED, CMakeLists:44-58):
```cmake
# GAME_ANALYTICS_ENABLED: -D override wins (cache-entry -> DEFINED -> сохраняется между configure);
# иначе дефолт = GAME_DEVAPI_ENABLED как ОБЫЧНАЯ переменная (пересчёт КАЖДЫЙ configure -> переключение
# GAME_DEVAPI_ENABLED в живом дереве обновляет её; CACHE...FORCE протух бы). НЕ CACHE в дефолт-пути.
if(NOT DEFINED GAME_ANALYTICS_ENABLED)
    set(GAME_ANALYTICS_ENABLED ${GAME_DEVAPI_ENABLED})
endif()
# Аналитика КОНСТРУКТИВНО требует FEATURE_GAME_STATE (cJSON + генерённые дескрипторы + рендерер E3).
if(GAME_ANALYTICS_ENABLED AND NOT FEATURE_GAME_STATE)
    message(STATUS "GAME_ANALYTICS_ENABLED requires FEATURE_GAME_STATE; disabling analytics.")
    set(GAME_ANALYTICS_ENABLED OFF)
endif()
```
2. **Макрос `FEATURE_GAME_ANALYTICS` — ОПРЕДЕЛЁН ВСЕГДА (1/0) на УРОВНЕ ТАРГЕТА (HIGH-A)**
   (рядом с `NT_INTROSPECT_ENABLED=$<BOOL:...>` CMakeLists:253-258 — game_features.c/main.c
   компилятся безусловно и несут `#if FEATURE_GAME_ANALYTICS`; неопределённый макрос под
   `-Wundef`+`-Werror` = ошибка компиляции). Зеркалит `NT_DEVAPI_ENABLED=0` (:262-263)/
   `FEATURE_GAME_STATE` (:194/:208):
```cmake
target_compile_definitions(${GAME_TARGET} PRIVATE FEATURE_GAME_ANALYTICS=$<BOOL:${GAME_ANALYTICS_ENABLED}>)
```
   (`game_log.c` — безусловно в `add_executable(${GAME_TARGET} ...)` рядом с `src/game_events.c`
   CMakeLists:118, §E4.B.2; макроса не требует.)
3. **Источники — блок аналитики НА УРОВНЕ `if(FEATURE_GAME_STATE)` (НЕ вложен в devapi, MED-D),
   рендерер E3 под общим `(GAME_DEVAPI_ENABLED OR GAME_ANALYTICS_ENABLED)` с дедуп-гардом**
   (в блок `if(FEATURE_GAME_STATE)`, рядом с E3-правкой CMakeLists:202-206):
```cmake
    # E3-рендерер: общий для хвоста E3 И писателя E4 -> компилить, если хочет ЛЮБОЙ (один листинг,
    # без дубля source). E4 ПЕРЕНОСИТ эту строку из devapi-only блока E3 сюда (SOURCE E3 не тронут).
    if(GAME_DEVAPI_ENABLED OR GAME_ANALYTICS_ENABLED)
        target_sources(${GAME_TARGET} PRIVATE src/game_event_render.c)   # E3 renderer (shared)
    endif()
    if(GAME_DEVAPI_ENABLED)
        target_sources(${GAME_TARGET} PRIVATE
            src/game_save_devapi.c
            src/game_events_devapi.c)   # E3 (game_event_render.c ПЕРЕЕХАЛ в общий блок выше)
    endif()
    if(GAME_ANALYTICS_ENABLED)
        target_sources(${GAME_TARGET} PRIVATE src/game_analytics.c)      # E4 writer
    endif()
```
   При `GAME_ANALYTICS_ENABLED=OFF` + `GAME_DEVAPI_ENABLED=OFF` → рендерер/writer не компилятся,
   `FEATURE_GAME_ANALYTICS=0` (шаг 2) → пустые ветки; при `ANALYTICS=ON`+`DEVAPI=OFF` → рендерер +
   writer компилятся (флаг НЕ немой, MED-D). NT_HASH_LABELS в не-devapi отсутствует → hash/сырьё
   в хекс (корректно).
4. **Ctest `test_game_analytics`** (нативный, БЕЗ devapi-сети; в блок `if(NOT EMSCRIPTEN)` после
   `test_game_events_typed`, CMakeLists:396):
```cmake
    add_executable(test_game_analytics
        tests/test_game_analytics.c
        src/game_analytics.c
        src/game_event_render.c   # E3 renderer (reused)
        src/game_state_json.c     # HIGH-B: game_event_render зовёт gsj_i64_to_string (FT_I64 линк)
        src/game_log.c            # MED-H: кейс #7 зовёт game_log_emit (иначе undefined ref)
        "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini/mini_state_events.gen.c"
        src/game_events.c)
    target_link_libraries(test_game_analytics PRIVATE unity cjson nt_hash nt_log nt_core)
    target_include_directories(test_game_analytics PRIVATE
        src "${CMAKE_CURRENT_SOURCE_DIR}/../../features/game-state/tests/golden/mini")
    target_compile_definitions(test_game_analytics PRIVATE
        FEATURE_GAME_ANALYTICS=1 GAME_ANALYTICS_TESTING=1
        GAME_ANALYTICS_BUF_BYTES=256u        # маленький буфер -> порог/дроп дёшево
        GAME_ANALYTICS_FLUSH_BYTES=192u
        GAME_STORAGE_APP_ID="template_test"  # header app-поле
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_analytics PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_analytics COMMAND test_game_analytics)
```
   (`game_state_json.c` — тот же линк, что E3-тест делает осознанно, E3 §E3.7 шаг 3 / §0:
   `game_event_render` дёргает `gsj_i64_to_string` (`game_state_json.c:222`) для FT_I64-полей;
   `mini.cell_spawned.total`=FT_I64 → без него undefined reference. Транзитивные — `nt_platform`
   и т.п. — по сообщению линкера. Тест инжектит sink → файловый путь НЕ трогается; часы инжектятся.)

**Warning-гейт:** `game_analytics.c` компилится в game-таргете под `nt_set_warning_flags`+`-Werror`
(CMakeLists:265) в devapi-сборках → оффсет/размер-арифметика буфера, u64→double заголовка,
sink-указатель под `-Wconversion`/`-Wformat=2`/`-Wshadow`.

### E4.9 Тесты `test_game_analytics.c` (Unity, ctest, нативно, инъекция sink+часов)

`setUp`: `nt_hash_init(NULL)` (однажды) + `game_events_init()` + сброс тест-захвата + инъекция
часов (фикс wall, напр. 1720000000000) + инъекция capture-sink (аппендит в глобальный тест-буфер)
+ `game_analytics_register_descs(mini_ev_descs, mini_ev_desc_count)` + `game_analytics_init()`.
`tearDown`: `game_analytics_shutdown()` + `game_events_shutdown()`. Фаза EMIT (дефолт).

1. **Заголовок:** после init capture-буфер содержит ПЕРВУЮ строку → `cJSON_Parse` → `kind=="header"`,
   `started_at==1720000000000`, `app=="template_test"`, `schema=="analytics.v1"`.
2. **Сериализация события (реюз рендерера; MED-E — ассерты против ПОСТ-ФИКСНОГО E3):**
   `mini_emit_cell_spawned(42,3.5,nt_hash64_str("Epic"),true,"hello",{1,2,3},3)`;
   `game_analytics_record()`; `game_analytics_flush()`; последняя строка capture парсится:
   - **`type` — label-agnostic:** ожидание = `desc->name` (рендерер берёт имя типа из дескриптора,
     `desc->name`, независимо от NT_HASH_LABELS) → `type == "mini.cell_spawned"` (безопасно), ИЛИ
     сверять через `nt_hash64_label(mini_ev_cell_spawned_type())` в рантайме (зеркало E3 §E3.10 —
     nt_hash наследует флаг пресета).
   - **`total` — СТРОКА `"42"`** (`cJSON_IsString`+`strcmp`): FT_I64 рендерится СТРОКОЙ (пост-фикс
     E3, i64-стрингификация — §14 п.8), НЕ числом.
   - `rate == 3.5` (FT_FLOAT → JSON-число), `label == "hello"` (строка).
   - `blob`/`kind` — глубину/label-чувствительность (hex vs метка) покрывает `test_game_event_render`
     (E3); здесь НЕ дублировать жёсткими ассертами (label-agnostic).
3. **NDJSON-фрейминг:** эмит 3 события в кадре → record → flush → `split('\n')` даёт header + 3
   непустых строки, каждая `cJSON_Parse != NULL`.
4. **Буферизация/порог:** эмит достаточно событий, чтобы перейти `GAME_ANALYTICS_FLUSH_BYTES`
   (буфер 256/порог 192 в ctest) → record БЕЗ явного flush → sink уже вызван (capture непуст),
   `s_buf` не рос без границы.
5. **Политика дропа:** сконструировать/эмитнуть событие, чья строка > `GAME_ANALYTICS_BUF_BYTES`
   (длинный `label` через `mini_emit_cell_spawned`, но ≤ `GAME_EVENT_EMIT_MAX`) → record →
   `game_analytics_dropped() > 0`, НЕ падает, ранее записанные строки целы/валидны.
6. **Мультикадр:** record кадра A (эмит 2) → `game_event_frame_reset()` → эмит 1 → record кадра B →
   flush → capture = header + 3 строки событий, `seq` монотонен между кадрами.
7. **(E4.B, если тип включён) log:** зарегистрировать `game_log_descs`; `game_log_emit("hi")`;
   record → flush → строка парсится `type=="log"`, `msg=="hi"`.

**Компиляционный/рантайм-смоук (вне ctest, гейт приёмки):** сборка `templates/template` в
`native-debug` (аналитика OFF — `#if FEATURE_GAME_ANALYTICS` компилится пусто, файлов нет),
`devapi-debug` (TU + рекордер-строка + регистрация компилятся warning-clean; `--capture`-прогон
пишет `build/analytics/session-*.ndjson` → файл существует, ПЕРВАЯ строка = валидный header-JSON,
последующие (если события были) — валидный NDJSON), `wasm-devapi-debug` (КОМПИЛЯЦИЯ нового TU;
полный линк красный на HEAD — движок, вне E4).

---

## E4.B. ВСТРОЕННЫЙ ТИП `log {string}` (малое; отделяемо — Q5)

Дизайн §3/§6: «Один встроенный generic-тип `log {str}` — для ad-hoc отладочных сообщений game glue
без заведения схемы». Fragment-less (тип = `nt_hash64_str("log")`, НЕ `game.log`), рукописный
(нельзя выразить схемой без правки генератора — заморожен).

### E4.B.1 Файлы + API

**Новые:** `templates/template/src/game_log.{c,h}` — безусловный leaf (нужны только `game_events.h`
+ `game_event_desc.h` + `hash/nt_hash.h`; НЕ cJSON, НЕ devapi → компилится в game-таргете
БЕЗУСЛОВНО, как `game_events.c`; в release emit безвреден — событие живёт кадр и умирает).

**НАЗВАНИЕ (страж коллизии):** `game_event_log(int*)` УЖЕ занят (walk, game_events.h:74) →
emit-хелпер НЕ может зваться `game_event_log`. Использовать `game_log_emit`.

`game_log.h`:
```c
#ifndef GAME_LOG_H
#define GAME_LOG_H
#include <stdbool.h>
#include <stdint.h>
#include "hash/nt_hash.h"
#include "game_event_desc.h"

typedef struct GameLog { uint32_t msg; } GameLog; /* msg = байт-оффсет -> инлайн NUL-строка */

nt_hash64_t game_log_type(void);                  /* nt_hash64_str("log"), cached */
const void *game_log_emit(const char *msg);       /* pack inline string -> game_event_emit */
static inline const char *game_log_msg(const GameLog *e){ return (const char*)e + e->msg; }

extern const game_event_desc_t game_log_desc;               /* {"log", sizeof(GameLog), {STRING msg}} */
extern const game_event_desc_t *const game_log_descs[];     /* 1-элем таблица для register_descs */
extern const int game_log_desc_count;                       /* 1 */
void game_log_register(void);                     /* game_event_register_type_name(type,"log") */
#endif
```
`game_log.c`: emit = union-стейджинг инлайн строки (зеркало E2 §E2.6 `mini_emit_*` string-ветки:
`memset` структа, `off=sizeof(struct)`, копия строки+NUL, `game_event_emit(game_log_type(), &u,
off, _Alignof(GameLog))`, страж `GAME_EVENT_EMIT_MAX`); дескриптор = одно `GAME_EVENT_FT_STRING`
поле `msg` (offset `offsetof(GameLog,msg)`, len_offset 0); `_Static_assert(_Alignof(GameLog) <=
_Alignof(max_align_t))`.

### E4.B.2 Проводка (main.c) + CMake

- **CMake:** `src/game_log.c` — БЕЗУСЛОВНО в `add_executable(${GAME_TARGET} ...)` рядом с
  `src/game_events.c` (CMakeLists:118). Ctest — покрыт кейсом #7 `test_game_analytics` (+ хвост E3
  отрендерит его в своём ctest, если добавить).
- **main.c регистрация (HIGH-C(b)).** `game_log_register()` (метка) БЕЗУСЛОВНА — `game_log.c` leaf,
  вне FEATURE_GAME_STATE может стоять только она. Регистрация дескриптора в реестры — ВНУТРИ
  `#if FEATURE_GAME_STATE` (оба реестра существуют только там: `game_events_devapi_register_descs`
  — под `FEATURE_GAME_STATE && NT_DEVAPI_ENABLED` двойным гейтом E3; `game_analytics_register_descs`
  — под FEATURE_GAME_ANALYTICS ⊆ FEATURE_GAME_STATE):
```c
    game_log_register();  // E4.B: debug label "log" (БЕЗУСЛОВНО; game_log.c — leaf)
#if FEATURE_GAME_STATE
#if NT_DEVAPI_ENABLED
    game_events_devapi_register_descs(game_log_descs, game_log_desc_count); // E3 tail (public API, append)
#endif
#if FEATURE_GAME_ANALYTICS
    game_analytics_register_descs(game_log_descs, game_log_desc_count);     // E4 analytics (append)
#endif
#endif /* FEATURE_GAME_STATE */
```
  (Второй вызов `..._register_descs` = легальный append к реестру — оба реестра аккумулируют
  (`game_ev_descs` + `game_log_descs`). Без регистрации дескриптора `log` рендерился бы
  unknown-фолбэком `{unknown,hex}` — `msg` нечитаем; регистрация в оба реестра обязательна.)

---

## E4.10 Критерии приёмки (бинарные)

- [ ] `game_analytics.c` компилится под devapi-debug + wasm-devapi-debug (компиляция TU)
      warning-clean (`-Werror`+`nt_set_warning_flags`); native-debug/release НЕ компилируют его
      (пустой TU; `#if FEATURE_GAME_ANALYTICS` в game_features.c/main.c — пусто под OFF).
- [ ] **`FEATURE_GAME_ANALYTICS` определён ВСЕГДА (1/0) на уровне таргета (HIGH-A)** — сборка
      native-debug/release (где analytics OFF) warning-clean под `-Wundef`+`-Werror` (game_features.c
      и main.c компилятся безусловно с `#if FEATURE_GAME_ANALYTICS`).
- [ ] **Gate-independence (HIGH-C(c), зеркало E3.11):** конфигурация devapi ON + FEATURE_GAME_STATE
      OFF ЛИНКУЕТСЯ (E3/E4 call-sites под двойным гейтом `FEATURE_GAME_STATE && NT_DEVAPI_ENABLED`,
      E4 их не трогает; analytics форсится OFF при state OFF, CMake §E4.8 шаг 1); конфигурация
      analytics ON + devapi OFF собирает writer+рендерер (флаг не немой, MED-D).
- [ ] **Рендер НЕ дублирован:** `game_analytics.c` зовёт `game_event_render` (E3); своего
      парсинга payload'а/JSON-склейки НЕ содержит.
- [ ] `game_events.{c,h}` / `game_event_desc.h` / генератор / golden / `game_save*.{c}` /
      `game_events_devapi.{c,h}` / `game_event_render.{c,h}` — **0 правок** (`git diff` пуст по ним;
      рендерер и devapi-реестр только ЗОВУТСЯ).
- [ ] Рекордер зовётся ОДИН раз/кадр из `game_features_record` в фазе RECORD (арена жива), ВТОРОЙ
      строкой после E3; НЕ эмитит; НЕ держит указателей арены между кадрами (рендер-в-строку сразу).
- [ ] Поток = NDJSON: ПЕРВАЯ строка = валидный header-JSON (`schema/kind/app/build/started_at`);
      каждое событие = отдельная валидная JSON-строка `{seq,tick,type,<поля>}`; каждая
      `≤ GAME_ANALYTICS_LINE_MAX` (усечение делает рендерер).
- [ ] Путь записи — СВОЙ append (native `fopen "ab"` в `build/analytics/session-<wall>.ndjson`,
      буфер+flush; web = in-memory ring); `game_storage` НЕ используется.
- [ ] Буферизация: НЕТ блокирующей записи каждый кадр (flush по порогу `GAME_ANALYTICS_FLUSH_BYTES`
      + shutdown); байт-кап сессии (`GAME_ANALYTICS_MAX_BYTES`) → стоп+warn; переполнение
      строки/капа/ошибка записи → `game_analytics_dropped()++` + one-time warn, БЕЗ краша (как E1).
- [ ] **`dropped()` = ТОЛЬКО реальные сбои (0 on healthy run); рутинное вытеснение web-ring →
      отдельный `evicted()` (LOW-J)**, не сливается в dropped.
- [ ] Web: `FEATURE_GAME_ANALYTICS` в wasm-devapi = in-memory ring (НЕ localStorage — квота itch);
      `game_analytics_export()` даёт валидный NDJSON (целые строки).
- [ ] Гейт `GAME_ANALYTICS_ENABLED` дефолт = `GAME_DEVAPI_ENABLED`; матрица §E4.2 держится
      (native-debug/release OFF, devapi ON, web-devapi ring).
- [ ] `test_game_analytics` зелёный (заголовок/сериализация/NDJSON-фрейминг/буфер-порог/дроп/
      мультикадр(+log)); **линкует `game_state_json.c` (HIGH-B, gsj_i64_to_string) + `game_log.c`
      (MED-H)**; ассерты label-agnostic, `total` = строка `"42"` (MED-E); `shutdown` сбрасывает
      реестр (`s_reg_count=0`, MED-G — второй setUp не дубль-регистрирует). Все прежние ctest
      (state/save/storage/json/events/events_typed/event_render/roundtrip) зелёные.
- [ ] Смоук-бот НЕ изменён (E4 без DevAPI-команды); tool-parity не задет.
- [ ] (E4.B) `game_log.c` компилится безусловно; `game_log_emit("x")` → событие типа `log`,
      рендерится читаемо (`msg`) в хвосте E3 и в аналитике (дескриптор зарегистрирован в оба
      реестра); имя `game_log_emit` НЕ коллидирует с `game_event_log`.

---

## E4.11 Порядок работ

0. **Гейт зависимости:** убедиться, что E3 приземлил `game_event_render.{c,h}` +
   `game_events_devapi_register_descs` + NT_HASH_LABELS build-config. Нет → СТОП, доложить.
   **Baseline:** собрать native-debug + devapi-debug + wasm-devapi-debug (компиляция) на HEAD,
   прогнать все ctest — зафиксировать зелёный/красный ДО правок (отделить предсущ. красный
   wasm-линк).
1. **Писатель (deep ведёт):** `game_analytics.{c,h}` (§E4.5) — реестр/буфер/sink/заголовок/дроп/
   байт-кап + wall-часы + native/web/test sink. + `test_game_analytics.c` (§E4.9). Прогнать ctest.
2. **(E4.B) log-тип (fast):** `game_log.{c,h}` (§E4.B) — emit-стейджинг+дескриптор+регистратор.
3. **Проводка (fast):** game_features.c (гейт-строка + include), main.c (include + register_descs +
   init/shutdown + log-регистрация), CMake (опция + TU + define + ctest + game_log.c) (§E4.7/E4.8).
4. **Гейт приёмки:** сборки native-debug + devapi-debug + wasm-devapi-debug(компиляция) warning-
   clean; все ctest; `--capture`-прогон devapi-debug → `build/analytics/*.ndjson` валиден (header +
   события); (опц.) ручной `game_log_emit` + `game.events.tail` показывает читаемый `log`.

Зависимость: шаг 3 требует шагов 1-2 (символы определены). Старт всего требует шага 0 (E3).

---

## E4.12 Риски

- **R1 (зависимость от E3).** E4 не собирается без `game_event_render.{c,h}`. Митигация: шаг 0
  гейта; E4 стартует ПОСЛЕ E3 (не параллельно). Если E3 задержан — E4 блокирован (доложить, не
  дублировать рендер).
- **R2 (двойной cJSON-churn в кадре).** И хвост E3, И аналитика E4 рендерят КАЖДОЕ событие
  КАЖДЫЙ кадр (2× транзиентный malloc cJSON) в devapi-debug. Только dev-сборки (release не
  компилит ни то, ни другое). Приемлемо. Будущая оптимизация (рендер ОДИН раз, скормить обоим) —
  вне E4 (связала бы E3+E4). Отмечено.
- **R3 (append-файл: директория/CWD/конкурентность).** `build/analytics/` создаётся ПОСЕГМЕНТНЫМ
  mkdir (MED-F: init бежит ДО game_save_init → `build/` может не существовать → создаём обе); путь
  относителен CWD (как `build/saves/` у game_storage — игра стартует из template-root). Имя несёт
  **pid-суффикс по дефолту** (LOW-K: `session-<wall>-<pid>.ndjson`) → две сессии в ту же секунду
  (`time(NULL)` секундный) НЕ коллизят.
- **R4 (web-квота — не задета).** Поток НЕ идёт в localStorage (in-memory ring) → itch-квота
  (state §14 п.3) не тратится; настоящий сейв game_save не под угрозой. Web-аналитика = best-effort
  память (теряется при выгрузке — дизайн §4 «web — память»).
- **R5 (объём потока).** «ВСЕ события» может быть велик на стресс-объёмах. Байт-кап сессии +
  агрегатные оффлайн-события (дизайн §2/Q2) ограничивают. Если кап бьёт часто — поднять
  `GAME_ANALYTICS_MAX_BYTES` или ввести opt-in фильтр (Q2, будущее).
- **R6 (i64 > 2^53 в JSON-числе).** Наследуется от `game_event_render` (cJSON-числа = double).
  Known-limit dev-потока (тот же, что E3 §R7). Вне E4.
- **R7 (дубль реестра hash→desc с E3).** ~15 строк дублируются (E3-реестр приватен/заморожен).
  Митигация: осознанно (§E4.13); будущий shared `game_event_registry.{c,h}` (рефактор E3+E4) —
  вне E4.
- **R8 (wasm-devapi красный линк на HEAD).** Предсуществующий (движок). E4-гейт для wasm =
  КОМПИЛЯЦИЯ TU. Baseline (шаг 0) отделяет от регрессий E4.

---

## E4.13 Отступления от буквы дизайна (с обоснованием)

1. **bytes пишутся в аналитику как `{size,hex}` (реюз рендерера E3), а не игнорируются.**
   Дизайн §7 Q4: «рекордеры/аналитика игнорируют bytes». Но E3 сделал ОДИН общий рендерер, где
   bytes = `{size,hex}` (hex ≤48Б). Пропуск bytes в аналитике = флаг/режим в сигнатуре
   `game_event_render` = правка ЗАМОРОЖЕННОГО E3. Реюз как есть (bytes малы, усечены) дешевле и
   не рвёт заморозку. Обратимо будущим render-mode параметром (follow-up E3). См. Q3.
2. **Свой file-static реестр hash→desc (дубль E3).** E3-реестр приватен в `game_events_devapi.c`
   (заморожен). E4 держит свой ~15-строчный (тот же паттерн). Альтернатива (shared-реестр) =
   рефактор E3. Осознанный минимум-риск.
3. **Свои wall-часы E4 (native `time(NULL)`, web `Date.now()` EM_JS).** game_save wall-часы
   приватны, `game_save_last_saved_at()`=0 до сейва, save_seq-аксессора нет, game_save заморожен.
   ~5 строк, зеркало game_save.c:90-95. Не дублирование логики — минимальный шов.
4. **Аналитика по умолчанию едет в devapi-семье (гейт = GAME_DEVAPI_ENABLED).** Дизайн §4 трактует
   «Локальная аналитика» как отдельного потребителя. Но её рендерер (E3) компилится только в
   devapi-сборках; «едет там же» = ноль CMake-трения с E3 + читаемые метки бесплатно + чистый
   человеческий native-debug. Независимость — override (§E4.2). Реализационное решение, не смена
   дизайна. См. Q1.
5. **Встроенный `log` = fragment-less `game_log.{c,h}`, emit-хелпер `game_log_emit`.** Дизайн §3
   зовёт тип `log` без фрагмента; выразить схемой = «game.log» + правка генератора (заморожен).
   Рукописный leaf. Имя `game_log_emit` (НЕ `game_event_log` — занят walk'ом, страж коллизии).

---

## E4.14 Вопросы лиду (дефолт применён — спека не блокируется)

- **Q1 [ДЕФОЛТ]. Гейт аналитики.** ДЕФОЛТ: `GAME_ANALYTICS_ENABLED = GAME_DEVAPI_ENABLED`
  (аналитика в devapi-сборках native+web, OFF в human/release). Почему: рендерер E3 там уже есть
  (ноль CMake-трения), NT_HASH_LABELS там уже включён, devapi = агентский балансировочный билд,
  человеческий native-debug чист. Обратимо: `-DGAME_ANALYTICS_ENABLED=ON` (+ 1 CMake-строка на
  компиляцию рендерера) для независимости от devapi.
- **Q2 [ДЕФОЛТ]. Что писать — ВСЕ события или opt-in фильтр.** ДЕФОЛТ: **ВСЕ** (поток = сырой
  факт-лог; фильтрация = downstream-анализ, которым E4 не является; объём держат агрегаты+байт-кап).
  Opt-in аналитический контракт (per-game список типов) = будущее (economy-срез), если объём/шум
  станут болью.
- **Q3 [ДЕФОЛТ]. bytes в аналитике.** ДЕФОЛТ: **`{size,hex}`** (реюз рендерера E3; hex ≤48Б).
  Пропуск bytes (буква §7 Q4) потребовал бы смены сигнатуры замороженного `game_event_render` →
  вне E4. Дёшево обратимо будущим render-mode.
- **Q4 [ДЕФОЛТ]. `save_seq` в заголовке потока.** ДЕФОЛТ: **нет** (аксессора нет, game_save
  заморожен; корреляция по `started_at`+`app`+`build`, а seq/tick — в каждой строке события). Если
  лид хочет — публичный `game_save_save_seq()` в game_save (вне E4, снимает заморозку одной
  строкой).
- **Q5 [ДЕФОЛТ]. Встроенный тип `log` — в E4 или отдельный E4b.** ДЕФОЛТ: **в E4** (дизайн §6
  бандлит его в E4-инкремент; ~1 малый leaf-модуль). Чисто отделяемо: если лид хочет E4 лазерно на
  писателе — `game_log.{c,h}` + его регистрация выносятся в E4b без изменения писателя.
- **Q6 [РАТИФИЦИРОВАНО лидом 2026-07-07]: семантики оставлены как есть; вместо унификации кап
  поднят до штатно недостижимого (256МБ native / 1МБ web-ring) — расхождение академично.
  Firehose (писать все события) тоже ратифицирован; пересмотр — по реальным объёмам T0327.**
  Исходная формулировка: Семантика потери при капе РАЗНАЯ по платформам.
  native байт-кап = **keep-oldest-stop** (пишет до `MAX_BYTES`, дальше стоп → теряется ХВОСТ
  сессии); web-ring = **keep-newest-roll** (вытесняет старейшее → теряется НАЧАЛО). Противоположно.
  ДЕФОЛТ: **оставить как есть** (native — полнота раннего геймплея для дебага краша; web — свежесть
  для послематчевого среза памяти), дешёвый override `GAME_ANALYTICS_MAX_BYTES=0` (native без капа).
  Если лид хочет ЕДИНУЮ политику (напр. keep-newest везде — native ротация в session-2.ndjson) —
  правка native-sink, вне текущего дефолта. Связано с Q2/LOW-O (raw-firehose быстрее упирается в
  кап).

Все дефолты консервативны, обратимы, не блокируют исполнение.
