# BUILD-SPEC: события, инкремент E1 — шелл-лог + фичевый агрегатор (2026-07-06)

Имплементационная спецификация. Переводит принятый дизайн в файлы, сигнатуры,
CMake, тесты и критерии приёмки. НЕ меняет дизайн. При расхождении источник
истины — `templates/design/event_system_design_2026-07-06.md` (главный),
`templates/design/feature_architecture_2026-07-06.md` (агрегатор), затем
`features/game-state/references/state_system_design_2026-07-06.md` (стык автосейва).

Исполнитель может писать код по этому документу, НЕ открывая историю обсуждений.

## 0. Предпосылки и рамки

- **A1–A3 УЖЕ в дереве**. `game_state_json.*`, `game_storage.*`, `game_save.*`,
  `game_fragment.c` установлены в `templates/template/src/`; CMake несёт три
  ctest-таргета (`test_game_state_json`/`test_game_storage`/`test_game_save`,
  CMakeLists:264–303); `main.c` уже проведён на `game_save_*` и НЕСЁТ якорь E1:
  комментарий `main.c:250–252` «when E1 lands, after the record phase». E1
  встаёт поверх этой реальности; A1–A3 не трогает.
- **E1 = универсальный шелл шаблона**, НЕ фича и НЕ часть game-state. Событийный
  лог живёт независимо от `FEATURE_GAME_STATE`; единственная связь с game_save —
  порядок вызовов в кадре (`game_save_tick()` после фазы record). Поэтому доки и
  код E1 живут в шаблоне (`templates/`), а не в `features/game-state/`.
- **Движковая граница проверена (СВОЯ арена — НЕ обход движка)**:
  - `nt_mem_scratch` (`external/neotolis-engine/engine/memory/nt_mem_scratch.h`) —
    bump-арена БЕЗ отравления, сбрасывается движком раз в кадр, бюджет 512КБ делит
    с nt_ui (main.c:359). Нужна ОТДЕЛЬНАЯ game-side арена: отравляемую на reset, с
    бюджетом, отвязанным от UI. Движок такого не даёт → собственная арена в
    `game_events.c` (malloc в init, отравление в reset) — корректная реализация
    того, чего движок не предоставляет, а НЕ обход. Никаких правок движка.
  - **Размер ФИКСИРОВАН (решение лида 2026-07-06, поверх дизайна §2 «растяжимая»)**:
    один malloc `GAME_EVENTS_ARENA_BYTES` на init, роста НЕТ — точный паттерн
    `nt_mem_scratch` (fixed bump + assert на переполнении, `nt_mem_scratch.h:38`).
    Класс UAF от переезда исключён; переполнение = debug-assert / release-дроп
    (§E1.3, §Отступление 4). Заведён engine-issue на generic `nt_arena_t`; наш
    аллокатор потом свопнется на движковый тип (§E1.9).
- **Движковый хеш проверен**: `nt_hash64_t` = `{uint64_t value}`
  (`engine/hash/nt_hash.h:26-28`); `nt_hash64_str(const char*)` даёт тип-хеш
  (nt_hash_init уже вызван, main.c:354); `nt_hash_register_label64(nt_hash64_t,
  const char*)` СУЩЕСТВУЕТ (`nt_hash.h:47`), сигнатура подтверждена. ВАЖНО:
  функция компилируется в `nt_hash.c` под `#if NT_HASH_LABELS` (дефолт 0,
  `nt_hash.h:10-12`); при выключенном флаге тело — **no-op**
  (`nt_hash.c:376-379`), а `nt_hash64_label` возвращает NULL (`:386-389`).
  Следствие для E1 см. §E1.9 «Шов имён типов» (E1 даёт только тонкий wrapper;
  реальные метки — build-config-решение E2/E3, не блокер E1).
- **Отравление/канарейка/фаза-assert — под `NT_DEBUG`**. Движок определяет
  `NT_DEBUG` во всех не-Release конфигах (`nt_platform.h:17-21`: `#ifdef NDEBUG →
  NT_RELEASE, иначе NT_DEBUG`), и `NT_ASSERT` в этих сборках = FULL
  (`nt_assert.h:20-26`). Значит debug-стражи гейтятся на `NT_DEBUG`, а фазовый
  assert — на `NT_ASSERT` (сам вычищается в release/off).
- **Сборка**: game-таргет собирается `-Werror` + `nt_set_warning_flags`
  (`cmake/warnings.cmake:4-19`: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wdouble-promotion -Wformat=2 -Wundef`, `-Wno-unused-parameter`). Ноль float/
  double в E1 → `-Wdouble-promotion` неактуален; арифметика размеров/выравнивания
  — `size_t`/`uintptr_t` с ЯВНЫМИ кастами (для `-Wconversion`), поле `size` =
  `uint32_t` кастуется явно. Unity в движке собран `UNITY_EXCLUDE_FLOAT/DOUBLE`
  → float-макросов НЕТ (в E1 не нужны).
- **Один тред**. Потокобезопасность нигде не требуется (как у event-лога,
  так и у арены).
- **Ноль зависимостей от генератора** (event-док §6): `game_events.*` не знает ни
  схем, ни типизированных структов, ни cJSON. Типизированные emit-хелперы и
  дескрипторы — E2 (после A4). E1 даёт только СЫРОЙ транспорт (emit по
  `type_hash`+байтам+`size`+`align`).

---

## E1. Скоуп (ровно)

Два новых модуля + проводка кадра + тесты:

1. **`game_events.{c,h}`** (L0 шелл, корень `src/`) — кадровый лог событий: своя
   отравляемая ФИКСИРОВАННАЯ арена (переполнение = debug-assert / release-дроп +
   счётчик); `emit` сырым payload+type_hash с выравниванием; walk по индексу;
   глобальный `seq` (uint64) + кадровый `tick` (uint32); фикспойнт-драйвер react с
   капом поколений; фазовый assert; `frame_reset` с отравлением в debug; тонкий шов
   регистрации имён типов.
2. **`features/game_features.{c,h}`** (L0 шелл, НОВАЯ папка `src/features/`) —
   агрегатор: СЕМЬ фазовых функций (init/update/draw_world/draw_ui/shutdown +
   react/record), явные вызовы по строке на систему, НИКАКИХ hook-таблиц.
   Двухфазный кадр живёт в ОДНОМ месте — шелл (`main.c`) гонит цикл через API
   `game_events`.
3. **`main.c`** — переезд `frame()` на агрегатор + установка двухфазного цикла;
   `game_events_init/shutdown`; `game_features_init/shutdown`; `game_save_tick()`
   переезжает на место «после record».
4. **`CMakeLists.txt`** — аддитивно: два TU в game-таргет + ctest `test_game_events`
   (+ `test_game_events_overflow` для release-дропа).
5. **`tests/test_game_events.c`** (Unity, ДВА ctest-бинаря) — эмит/walk,
   монотонность seq между кадрами, каскад-в-том-же-кадре, стабильность указателей,
   кап поколений, выравнивание, tick-сброс курсора, переполнение арены/лога
   (debug-assert), release-дроп, фазовый assert (death-test).

### E1.1 Файлы

Новые:
- `templates/template/src/game_events.h`
- `templates/template/src/game_events.c`
- `templates/template/src/features/game_features.h`
- `templates/template/src/features/game_features.c`
- `templates/template/tests/test_game_events.c` (Unity)

Изменяемые:
- `templates/template/src/main.c` — проводка кадра + lifecycle (§E1.5).
- `templates/template/CMakeLists.txt` — аддитивно (§E1.6).

Удаляемые: нет.

Не трогать: `game_save.*`, `game_storage.*`, `game_state_json.*`, `game_fragment.c`,
генератор и генерируемые файлы, движок.

---

### E1.2 Хедер `game_events.h` (полностью)

```c
#ifndef GAME_EVENTS_H
#define GAME_EVENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hash/nt_hash.h" /* nt_hash64_t */

/* Кадровый лог событий фич (event_system_design §2, вариант D + РЕШЕНИЕ ЛИДА
   2026-07-06: арена ФИКСИРОВАННАЯ, растяжимость вырезана — паттерн движка
   nt_mem_scratch = fixed bump + assert; класс UAF-багов от переезда исключён
   навсегда; каскады ограничены капом поколений). События живут ОДИН кадр в
   приватной, отравляемой, ФИКСИРОВАННОЙ арене game-side. Производители — только
   emit-хелперы; потребители дренят В ТОМ ЖЕ кадре по индексу. НЕ сейв, НЕ источник
   истины (истина = стейт). Один тред. Ноль зависимостей от генератора. */

/* Фиксированный размер арены payload'ов (ОДИН malloc на init, НЕ растёт). Щедрый
   дефолт; игра переопределяет compile-define'ом при нужде. */
#ifndef GAME_EVENTS_ARENA_BYTES
#define GAME_EVENTS_ARENA_BYTES (1u * 1024u * 1024u)
#endif

/* Фиксированный кап массива конвертов (максимум событий за кадр). */
#ifndef GAME_EVENTS_LOG_CAP
#define GAME_EVENTS_LOG_CAP 8192
#endif

/* Кап поколений react-фикспойнта за кадр (граница петель, event §2 страж 1/Q1).
   Достижение = dev-warn + остановка. */
#ifndef GAME_EVENTS_MAX_GENERATIONS
#define GAME_EVENTS_MAX_GENERATIONS 16
#endif

/* Конверт события (event_system_design §2). payload позиционно-независим:
   str-поля внутри него — байт-оффсеты, не указатели (это забота emit-хелперов
   E2); сам конверт хранит АБСОЛЮТНЫЙ указатель в живую арену кадра. */
typedef struct {
    uint64_t    seq;     /* ГЛОБАЛЬНЫЙ монотонный; переживает frame_reset */
    uint32_t    tick;    /* КАДРОВЫЙ счётчик шелла (не wall-clock) */
    nt_hash64_t type;    /* hash имени события (nt_hash64_str("items.txn")) */
    const void *payload; /* в арене событий; произвольный размер; живёт 1 кадр */
    uint32_t    size;    /* байт payload'а */
} game_event_t;

/* ---- Lifecycle (шелл; из main.c вокруг кадрового цикла) ---- */
void game_events_init(void);      /* ОДИН malloc фиксированной арены + лога; seq=0, tick=0 */
void game_events_shutdown(void);  /* free арены/лога */

/* Сырой emit: копирует `size` байт `payload` в ФИКСИРОВАННУЮ арену по выравниванию
   `align`, дописывает конверт в лог. Возвращает указатель на арену-копию
   (позиционно-независимую — str-поля обязаны быть уже инлайн-оффсетами; сырой
   payload НЕ должен содержать указателей в память вызывающего).
   `align` — POWER OF 2 в [1, `_Alignof(max_align_t)`] (зеркало `nt_mem_scratch.h:38`);
   вне диапазона — NT_ASSERT. База арены выровнена malloc'ом на max_align_t →
   смещение-по-align даёт реально выровненный указатель. Сверх-выравнивание
   > max_align_t НЕ поддержано.
   ПЕРЕПОЛНЕНИЕ (арены ИЛИ капа лога): debug — NT_ASSERT (немедленно видно, «подними
   GAME_EVENTS_ARENA_BYTES/LOG_CAP»); release — событие ДРОПАЕТСЯ, возвращает NULL,
   счётчик дропов++ + одноразовый (за кадр) dev-warn. Emit в фазе RECORD — баг:
   debug-assert. */
const void *game_event_emit(nt_hash64_t type, const void *payload,
                            uint32_t size, size_t align);

/* Walk лога текущего кадра по индексу (0..*count-1). Каскадные события, рождённые
   реакторами, дописываются в конец и достижимы в том же кадре (следующее
   react-поколение видит выросший *count). Возвращает backing-массив; *count =
   живое число на момент вызова.
   УКАЗАТЕЛИ СТАБИЛЬНЫ ВЕСЬ КАДР: арена/лог фиксированы и НЕ переезжают → возвращённый
   указатель и любой game_event_t*/payload валидны до game_event_frame_reset();
   удержание через game_event_emit ЗАКОННО (переезда нет). Единственное правило —
   не держать указатель ПОСЛЕ frame_reset (арена отравляется 0xDD в debug). Смену
   кадра детектить через game_events_tick(), НЕ через падение *count (§E1.4). */
const game_event_t *game_event_log(int *count);

/* Кадровый счётчик текущего кадра (== game_event_t.tick событий, эмитнутых
   сейчас). Потребитель сбрасывает свой курсор s_pos при СМЕНЕ tick — надёжный
   детектор нового кадра (сравнение числа событий ненадёжно: новый кадр может
   иметь ≥ событий, чем осталось курсору, → тихая потеря). */
uint32_t game_events_tick(void);

/* Кумулятивное число событий, ДРОПНУТЫХ при переполнении (release-путь; в debug
   переполнение = assert, до дропа не доходит). Ноль в норме → для DevAPI/E3 и
   смоуков (assert «dropped == 0» на здоровом прогоне). НЕ сбрасывается frame_reset. */
uint32_t game_events_dropped(void);

/* ---- Двухфазный кадр (гонит ТОЛЬКО шелл/агрегатор) ---- */
typedef enum {
    GAME_EVENT_PHASE_EMIT = 0, /* update + react: emit разрешён */
    GAME_EVENT_PHASE_RECORD    /* рекордеры: emit = баг (assert) */
} game_event_phase_t;

void game_events_set_phase(game_event_phase_t phase);

/* Отметить вход в react-фазу: базлайн фикспойнта = текущий count (ПОСЛЕ update).
   Зовётся ОДИН раз между update и react-циклом; убирает холостой второй проход
   react при нуле каскадов. Сбрасывает счётчик поколений. */
void game_events_react_begin(void);

/* Драйвер react-фикспойнта. Возвращает true, пока за прошлый проход родились
   новые события И не превышен кап поколений; false = фикспойнт достигнут ИЛИ кап
   исчерпан (тогда dev-warn один раз; см. §E1.3). Использование в шелле:
       game_features_update(w, dt);        // phase=EMIT (дефолт после frame_reset)
       game_events_react_begin();          // базлайн = count после update
       do { game_features_react(w); } while (game_events_react_progressed()); */
bool game_events_react_progressed(void);

/* Конец кадра, ПОСЛЕ record: bump tick, обнулить лог+арену, сбросить счётчики
   react. В debug — отравить использованный регион арены паттерном (use-after-
   frame падает громко). */
void game_event_frame_reset(void);

/* ---- Шов имён типов (event §8; ПОТРЕБИТЕЛЬ — генератор в E2) ----
   Тонкая обёртка над nt_hash_register_label64: в debug-сборках с NT_HASH_LABELS
   DevAPI/лог показывают "items.txn" вместо хекса. No-op, если nt_hash собран без
   NT_HASH_LABELS (движковый дефолт) — см. §E1.9. E1 предоставляет только шов;
   пер-типовую регистрацию из схем делает генератор E2. */
void game_event_register_type_name(nt_hash64_t type, const char *name);

#endif /* GAME_EVENTS_H */
```

### E1.3 Контракты реализации `game_events.c`

Файловые статики (владение — модуль):
```c
static uint8_t *s_arena; static size_t s_arena_used;   /* cap = GAME_EVENTS_ARENA_BYTES (const) */
static game_event_t *s_log; static uint32_t s_count;   /* cap = GAME_EVENTS_LOG_CAP (const) */
static uint64_t s_seq;   /* переживает frame_reset */
static uint32_t s_tick;  /* bump в frame_reset */
static game_event_phase_t s_phase;
static uint32_t s_react_last_count; static int s_react_gen;
static uint32_t s_dropped;        /* кумулятивно; НЕ сбрасывается (health-метрика) */
static bool s_overflow_warned;    /* per-frame: сбрасывается в frame_reset */
static bool s_soft_warned;        /* per-session: сбрасывается только в init (75%-аларм) */
```
Инклюды: `"game_events.h"`, `"hash/nt_hash.h"`, `"log/nt_log.h"` (dev-warn через
`nt_log_warn(...)` — плоские макросы без домена, `nt_log.h:52`), `"core/nt_assert.h"`
(NT_ASSERT), `<stdlib.h>` (malloc/free — по одному разу на init/shutdown, роста нет),
`<string.h>` (memcpy/memset), `<stddef.h>` (max_align_t), `<stdint.h>`.

- **`game_events_init`**: `s_arena = malloc(GAME_EVENTS_ARENA_BYTES)`, `s_arena_used
  = 0`; `s_log = malloc(GAME_EVENTS_LOG_CAP * sizeof(game_event_t))`, `s_count = 0`;
  `s_seq = 0`, `s_tick = 0`, `s_phase = EMIT`, счётчики react = 0, `s_dropped = 0`,
  `s_overflow_warned = false`, `s_soft_warned = false`. OOM аллокаций — `NT_ASSERT`
  (не runtime-ошибка, а сломанное окружение). Обе аллокации — ЕДИНСТВЕННЫЕ за жизнь
  модуля (роста нет; свап на движковый nt_arena_t — §E1.9).
- **`game_event_emit`**:
  1. `NT_ASSERT(s_phase == GAME_EVENT_PHASE_EMIT)` — **emit в RECORD = баг**
     (страж симметрии, event §7).
  2. **Контракт выравнивания** (MEDIUM-3): `NT_ASSERT(align >= 1 && align <=
     _Alignof(max_align_t) && (align & (align - 1)) == 0)` — power-of-2 в
     [1, max_align_t] (зеркало `nt_mem_scratch.h:38`; `<stddef.h>` для
     max_align_t). Выравнивание бампа в `size_t`: `size_t off = (s_arena_used +
     (align - 1)) & ~(align - 1)` — касты явные, чтобы `-Wconversion` молчал.
     База арены выровнена malloc'ом (>= max_align_t) и НЕ меняется (фикс-арена),
     значит смещение-по-align даёт реально выровненный указатель весь срок жизни.
     Сверх-выравнивание > max_align_t запрещено контрактом (malloc-база его не
     гарантирует) — отсюда потолок.
  3. **Проверка переполнения** (арены ИЛИ капа лога — оба ФИКСИРОВАНЫ, роста НЕТ):
     `size_t need = off + size;`
     `if (need > GAME_EVENTS_ARENA_BYTES || s_count >= GAME_EVENTS_LOG_CAP) { ... }`
     — тело переполнения:
     ```
     #if defined(NT_DEBUG) && !defined(GAME_EVENTS_SOFT_OVERFLOW)
         NT_ASSERT(0 && "game_events overflow: raise GAME_EVENTS_ARENA_BYTES/LOG_CAP");
     #endif
         s_dropped++;                          /* release-путь: дроп, не краш */
         if (!s_overflow_warned) {
             nt_log_warn("game_events: overflow at tick %u -> event dropped "
                         "(arena %zu/%u, log %u/%u); raise GAME_EVENTS_ARENA_BYTES/LOG_CAP",
                         s_tick, s_arena_used, (unsigned)GAME_EVENTS_ARENA_BYTES,
                         s_count, (unsigned)GAME_EVENTS_LOG_CAP);
             s_overflow_warned = true;         /* один warn за кадр */
         }
         return NULL;
     ```
     Debug падает сразу (класс «событий больше бюджета» виден в деве); release —
     мягкий дроп (emit->NULL, `s_dropped++`). `GAME_EVENTS_SOFT_OVERFLOW` — ТЕСТ-ШОВ
     (даёт debug-ctest проверить release-путь дропа, §E1.7 тест #10); прод-код его
     НЕ определяет — это единственная тестовая уступка прод-кода, минимальная.
  4. `void *p = s_arena + off; memcpy(p, payload, size); s_arena_used = off + size;`
     Мягкий 75%-аларм (один раз за сессию, dev): `if (!s_soft_warned && s_arena_used
     > (GAME_EVENTS_ARENA_BYTES / 4u) * 3u) { nt_log_warn("game_events: arena over "
     "75%% (%zu/%u B) at tick %u; consider raising GAME_EVENTS_ARENA_BYTES",
     s_arena_used, (unsigned)GAME_EVENTS_ARENA_BYTES, s_tick); s_soft_warned = true; }`
     (`%%` — литеральный процент; сбрасывается в init, НЕ в frame_reset).
  5. Дописать конверт (кап уже проверен в шаге 3 → `s_count < GAME_EVENTS_LOG_CAP`):
     `s_log[s_count] = (game_event_t){ .seq = s_seq++, .tick = s_tick,
     .type = type, .payload = p, .size = size };` `s_count++; return p;`
- **`game_event_log`**: `*count = (int)s_count; return s_log;` (count ≤
  GAME_EVENTS_LOG_CAP → всегда влезает в int).
- **`game_events_set_phase`**: `s_phase = phase;`
- **`game_events_tick`**: `return s_tick;`
- **`game_events_dropped`**: `return s_dropped;`
- **`game_events_react_begin`**: `s_react_last_count = s_count; s_react_gen = 0;`
  (базлайн фикспойнта = count ПОСЛЕ update; убирает холостой второй проход при
  нуле каскадов — LOW-8). Зовётся шеллом между update и do/while.
- **`game_events_react_progressed`** (фикспойнт-драйвер):
  ```
  if (s_count == s_react_last_count) return false;   /* фикспойнт: новое не родилось */
  s_react_last_count = s_count;
  if (++s_react_gen >= GAME_EVENTS_MAX_GENERATIONS) {
      nt_log_warn("game_events: react generation cap %d hit at tick %u (%u events)",
                  GAME_EVENTS_MAX_GENERATIONS, s_tick, s_count);
      return false;
  }
  return true;
  ```
  Гарантия: не более `GAME_EVENTS_MAX_GENERATIONS` проходов react за кадр.
  **Кап-варн — count-based** (тик + число событий): достаточно для критерия и
  гарантированно warning-clean. Пер-событийный дамп имён (опционально) НЕ вводить
  наивно: `nt_hash64_label(type)` = NULL без NT_HASH_LABELS → нужен hex-фолбэк
  `PRIx64` + `<inttypes.h>` (иначе `-Wformat=2` ругнётся на `%s` от NULL / голый
  `%lx`). По умолчанию оставляем count-варн (LOW-7).
- **`game_event_frame_reset`** (конец кадра, после record):
  ```
  #ifdef NT_DEBUG
      if (s_arena_used) memset(s_arena, 0xDD, s_arena_used); /* отравление used-части */
  #endif
  s_arena_used = 0; s_count = 0;
  s_tick++;                       /* следующий кадр */
  s_react_last_count = 0; s_react_gen = 0;
  s_overflow_warned = false;      /* per-frame drop-warn */
  s_phase = GAME_EVENT_PHASE_EMIT;
  ```
  `s_seq` (глобальный монотонный), `s_dropped` (кумулятивная health-метрика) и
  `s_soft_warned` (аларм на сессию) — НЕ трогаются.
- **`game_event_register_type_name`**: `nt_hash_register_label64(type, name);`
  (тонкий шов; no-op без NT_HASH_LABELS, §E1.9).
- **`game_events_shutdown`**: `free(s_arena); free(s_log);` обнулить статики.
- **Анти-баг (главный, event §5)**: НИКОГДА не хранить payload/log-указатели
  ПОСЛЕ frame_reset. Фиксированная арена НЕ переезжает → ВНУТРИ кадра указатели
  стабильны, удержание через emit законно; переезда, ребейза и ре-фетча БОЛЬШЕ
  НЕТ (класс UAF от переезда исключён решением лида). Единственный оставшийся
  инвариант: указатель мёртв после frame_reset — отравление 0xDD в debug ловит
  удержание МЕЖДУ кадрами громко.

### E1.4 Хедер + скелет `features/game_features.{c,h}`

`game_features.h` (полностью):
```c
#ifndef GAME_FEATURES_H
#define GAME_FEATURES_H

#include "world/world.h"

/* L0 агрегатор фич шаблона (feature_architecture §2). СЕМЬ фазовых функций,
   явные вызовы по одной строке на систему/фичу — НИКАКИХ hook-таблиц/шедулера.
   Двухфазный кадр событий гонит шелл (main.c) через game_events API:
     update эмитит -> react до фикспойнта -> record одним проходом -> reset.
   react/record несут World* как остальные фазы; события читаются глобально через
   game_event_log() (event §7). Список draw_ui сверху вниз = z-order.
   Игра добавляет потребителя ОДНОЙ строкой в react- или record-список. */

void game_features_init(World *w);
void game_features_update(World *w, float dt); /* фаза ЭМИССИИ (системы/фичи эмитят) */
void game_features_react(World *w);            /* реакторы-потребители (могут каскадить) */
void game_features_record(World *w);           /* чистые рекордеры (аналитика/лог/DevAPI) */
void game_features_draw_world(World *w);        /* 3D-слой фич (см. §E1.5: пока прямой шелл) */
void game_features_draw_ui(World *w);           /* UI-слой фич, z-order (см. §E1.5) */
void game_features_shutdown(World *w);

#endif /* GAME_FEATURES_H */
```
`game_features.c` — минимальный скелет (`#include "features/game_features.h"`,
`#include "systems/sys_move.h"`):
```c
void game_features_init(World *w) {
    (void)w; /* TODO(feature-migration): per-feature <id>_init(w) здесь */
}
void game_features_update(World *w, float dt) {
    sys_move(w, dt); /* мировая симуляция шаблона; здесь же фичи эмитят события */
    /* TODO(feature-migration): <id>_update(w, dt) по строке на фичу */
}
void game_features_react(World *w) {
    (void)w; /* TODO(E2+): реакторы читают game_event_log(&n), канон-идиом (§E1.4) */
}
void game_features_record(World *w) {
    (void)w; /* TODO(E3/E4): рекордеры (DevAPI tail E3, аналитика E4) */
}
void game_features_draw_world(World *w) { (void)w; /* TODO: см. §E1.5 */ }
void game_features_draw_ui(World *w)    { (void)w; /* TODO: см. §E1.5 */ }
void game_features_shutdown(World *w)   { (void)w; /* TODO: per-feature shutdown */ }
```

**Что стало «фичей» в скелете (решение)**: НИЧТО формально (папок `src/features/
<id>/` E1 не создаёт — это отдельный трек, feature_architecture §4 п.1 «settings
как первая фича»). Единственный существующий game-системный вызов, продетый через
агрегатор, — `sys_move` в фазе update (мировая симуляция = естественный дом
эмиттеров). Рендер-системы (`render_mesh_draw`, `hud_draw`, `sys_settings_ui`)
ОСТАЮТСЯ прямыми вызовами шелла в `frame()` с TODO-якорем: они завязаны на
шелловские GPU-хендлы (material/font/ubo/ctx), а финализация того, как draw-фазы
достают эти хендлы, — забота трека feature-migration, не E1. Поэтому
`game_features_draw_world/draw_ui` объявлены (замыкают 7 функций §2), определены
пустыми, а их место вызова в `frame()` — TODO-якорь (не рвём рендер-путь). Это
явно разрешено ТЗ («существующие системы можно оставить прямыми вызовами с
TODO-якорем»). Сигнатуры draw-фаз пока `(World *w)`; трек миграции расширит их
шелловскими хендлами при переносе рендер-систем.

**КАНОН-ИДИОМ потребителя (комментарий в react-скелете; образец для E2+)**. Один
обязательный инвариант: сброс курсора по СМЕНЕ tick, НЕ по числу событий (HIGH-2).
Указатели стабильны весь кадр (фиксированная арена) → `log` фетчится ОДИН раз,
удержание `e`/payload через emit ЗАКОННО:
```c
static int      s_pos;            /* курсор потребителя */
static uint32_t s_last_tick;      /* для детекта нового кадра */

uint32_t tick = game_events_tick();
if (tick != s_last_tick) { s_last_tick = tick; s_pos = 0; }  /* новый кадр -> сброс */

int n; const game_event_t *log = game_event_log(&n);   /* стабилен весь кадр */
for (; s_pos < n; ++s_pos) {
    const game_event_t *e = &log[s_pos];
    if (e->type.value != WANTED.value) continue;
    ... react ...   /* может эмитить каскад: следующее react-поколение увидит его */
}
```
Почему сброс по `tick`, а НЕ по числу событий (`if (s_pos > n) s_pos = 0`): старый
не срабатывает, когда новый кадр имеет ≥ событий, чем осталось курсору (кадр A
дренит 5; кадр B эмитит 8 → `s_pos` остаётся 5, события B[0..5) молча теряются —
предаёт «тихой потери нет», event §7). Сброс по смене `tick` надёжен всегда.

Каскады: `game_features_react` вызывается шеллом до фикспойнта; НОВЫЙ `n`
(выросший) виден на СЛЕДУЮЩЕМ react-поколении (потребитель перефетчит `log` при
входе в функцию заново). Ре-фетч ВНУТРИ одного прохода больше НЕ обязателен
(переезда нет) — можно оставить как безвредный стиль, контракт не требует.

`s_pos` — это и есть «подписка»: читаешь лог = подписан, не читаешь = нет;
межкадровых курсоров не существует. Гарантии: сброс по tick → потребитель видит
ВСЕ события своего кадра (нет тихой потери); курсор `s_pos` продвигается монотонно
в пределах кадра, повторный react-проход не переобрабатывает; реактор-в-record
ловит фазовый assert.

### E1.5 Проводка `main.c`

**Инклюды** (рядом с существующими, БЕЗ гейта FEATURE_GAME_STATE — события
универсальны):
```c
#include "game_events.h"
#include "features/game_features.h"
```
**Setup** (в `main()`, после `nt_hash_init(...)` — main.c:354, т.к. type-хеши/
метки требуют инициализированный hash; арена событий не зависит от gfx):
```c
game_events_init();
game_features_init(&s_world);   /* после того как s_world сконструирован */
```
Разместить `game_features_init(&s_world)` там, где мир уже валиден (после блока
game_state/спавнов — безопасно у main.c:431 рядом с ui_runtime_init, либо сразу
после render_mesh_spawn_*; init скелета пуст, но фиксируем корректное место).
`game_events_init()` — раньше, сразу после `nt_hash_init`.

**Кадр** (`frame()`): заменить текущий блок (МАТЧИТЬ ПО СМЫСЛУ, не побайтово — в
реальном `main.c:248-256` есть комментарий-якорь 251-252 «when E1 lands…» и
фигурные скобки; исполнитель находит блок «`sys_move(...)` + gated
`game_save_tick()`» и заменяет целиком, снимая и якорный комментарий):
```c
    sys_move(&s_world, g_nt_app.dt);
#if FEATURE_GAME_STATE
    // ... anchor comment 251-252 ...
    if (!s_disable_autosave) { game_save_tick(); }
#endif
```
на двухфазный цикл (event §2/§7, feature_architecture §2):
```c
    // ---- feature two-phase event frame (event_system_design §2/§7) ----
    game_features_update(&s_world, g_nt_app.dt);   // emit phase (sys_move moved in); phase=EMIT default
    game_events_react_begin();                     // fixpoint baseline = count after update
    do {
        game_features_react(&s_world);             // reactors (may cascade-emit)
    } while (game_events_react_progressed());       // fixpoint under generation cap
    game_events_set_phase(GAME_EVENT_PHASE_RECORD);
    game_features_record(&s_world);                // recorders (E3/E4 fill; empty now)
#if FEATURE_GAME_STATE
    // Autosave: after the RECORD phase (the anchor at the old sys_move site).
    if (!s_disable_autosave) { game_save_tick(); }
#endif
    game_event_frame_reset();                       // close the event frame (poison in debug); phase->EMIT
```
**Убрать ставший лишним `#include "systems/sys_move.h"` из `main.c`** (main.c:45):
вызов `sys_move` уехал в `game_features_update`; sys_move.h теперь инклюдит
`game_features.c`. (`sys_move` больше нигде в main.c не зовётся.) `set_phase(EMIT)`
перед update НЕ нужен — `frame_reset` предыдущего кадра оставляет фазу EMIT.
Рендер-блок (`nt_gfx_begin_frame` … `render_mesh_draw` / `hud_draw` /
`sys_settings_ui` … `nt_gfx_end_pass`) — БЕЗ изменений в E1. Добавить ОДИН
TODO-якорь перед `render_mesh_draw`:
`// TODO(feature-migration): game_features_draw_world/draw_ui land here once render systems become features.`

**Teardown** (в native-ветке, рядом с `ui_runtime_shutdown()` — main.c:446):
```c
    game_features_shutdown(&s_world);
    game_events_shutdown();
```
(порядок: features раньше events, симметрично init; на web ветка teardown не
исполняется — как и весь блок shutdown, main.c:444).

**Инвариант**: событийный/агрегаторный цикл — БЕЗУСЛОВНЫЙ (не под
`#if FEATURE_GAME_STATE`). Только `game_save_tick()` остаётся под гейтом.

### E1.6 CMake (`templates/template/CMakeLists.txt`)

Аддитивно в `add_executable(${GAME_TARGET} ...)` (main.c:116-125) — добавить два TU
(вне блока FEATURE_GAME_STATE, события универсальны):
```cmake
add_executable(${GAME_TARGET}
    src/main.c
    src/game_events.c            # E1
    src/features/game_features.c # E1
    src/ui/hud.c
    ...
)
```
`src` уже в `target_include_directories` (CMakeLists:178) → `#include
"game_events.h"` и `"features/game_features.h"` резолвятся; движковые инклюды
(hash/log/core) уже доступны game-таргету. Отдельных defines E1 не требует
(`GAME_EVENTS_*` имеют дефолты в хедере; переопределяются игрой при нужде).

Ctest (к блоку CMakeLists:264-303, внутри `if(NOT EMSCRIPTEN)`) — ДВА таргета из
ОДНОГО тест-файла: основной (debug-assert путь) + overflow-drop (release-путь через
тест-шов `GAME_EVENTS_SOFT_OVERFLOW`). Малые каперы форсируют переполнение дёшево:
```cmake
    # (1) основной: позитив + death-тесты переполнения/фазы (NT_ASSERT ФИРИТ)
    add_executable(test_game_events tests/test_game_events.c src/game_events.c)
    target_link_libraries(test_game_events PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events PRIVATE src)
    target_compile_definitions(test_game_events PRIVATE
        GAME_EVENTS_ARENA_BYTES=1024u   # маленькая арена -> переполнение дёшево (позитив влезает)
        GAME_EVENTS_LOG_CAP=64          # маленький кап лога -> переполнение дёшево
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events COMMAND test_game_events)

    # (2) overflow-drop: тот же файл + GAME_EVENTS_SOFT_OVERFLOW=1 -> emit ДРОПАЕТ
    # (не assert'ит) в debug-ctest, проверяет release-семантику (тест #10)
    add_executable(test_game_events_overflow tests/test_game_events.c src/game_events.c)
    target_link_libraries(test_game_events_overflow PRIVATE unity nt_hash nt_log nt_core)
    target_include_directories(test_game_events_overflow PRIVATE src)
    target_compile_definitions(test_game_events_overflow PRIVATE
        GAME_EVENTS_SOFT_OVERFLOW=1     # выключить debug-assert переполнения -> тестировать дроп
        GAME_EVENTS_ARENA_BYTES=1024u GAME_EVENTS_LOG_CAP=64
        _CRT_SECURE_NO_WARNINGS)
    set_target_properties(test_game_events_overflow PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME test_game_events_overflow COMMAND test_game_events_overflow)
```
Тест-файл гейтит наборы: `#ifdef GAME_EVENTS_SOFT_OVERFLOW` → RUN_TEST только
дроп-кейса #10; иначе → позитив 1-7 + death-кейсы #8/#9/#11 (переполнение/фаза).
Линковка `nt_hash`/`nt_log`/`nt_core` — экспортируются движком (game-таргет их уже
линкует, CMakeLists:180-184); транзитивные (напр. `nt_platform` для nt_assert) —
добавить по сообщению линкера. `game_features.c` в ctest НЕ входит (тянет
world/systems/gfx); двухфазная ЛОГИКА тестируется в `game_events` изолированно,
проводка `game_features`/`main.c` покрыта компиляционным смоуком.

### E1.7 Тесты `test_game_events.c` (Unity, ctest)

`setUp`: `game_events_init()`; `tearDown`: `game_events_shutdown()`. Тип-хеши —
`nt_hash64_str("test.a")` и т.п. (nt_hash_init один раз в `main()` теста или в
первом setUp; `nt_hash_init` идемпотентен-возвращает-ошибку при повторе — звать
однажды).

1. **emit → walk**: `set_phase(EMIT)`; эмитнуть 3 разных payload'а; `game_event_log
   (&n)` → n==3; `seq` == {0,1,2}, `tick` одинаков у всех, `type`/`size`
   совпадают, `memcmp(payload, expected, size)==0`.
2. **seq глобально монотонен между кадрами**: эмит (seq 0,1) → `frame_reset` →
   эмит → seq продолжает (2), `tick` инкрементнулся (был 0, стал 1).
3. **frame_reset очищает**: эмит → `frame_reset` → `log(&n)` даёт n==0.
4. **каскад в том же кадре (указатели стабильны)**: эмит A; `const void *pa =`
   результат emit A; взять `log(&n)`, n==1; эмитнуть B (симуляция реактора);
   `log(&n2)`, n2==2; B достижим по индексу 1, seq(B)=seq(A)+1; ПРОВЕРИТЬ, что `pa`
   и `log[0]` читаются корректно ПОСЛЕ emit B — фиксированная арена не переезжает,
   удержание указателя через emit законно (§E1.2).
5. **выравнивание (потолок)**: `emit(type, payload, size, A)` для A ∈ {1, 8, 16}
   → возвращённый указатель выровнен по A (`((uintptr_t)p & (A-1))==0`). БЕЗ A=64:
   сверх-max_align_t запрещено контрактом (шаг 2 NT_ASSERT). Опц. death-тест A=64
   → assert (в основном бинаре).
6. **кап поколений**: `game_events_react_begin();` затем `do {/* эмитить одно
   событие */} while (game_events_react_progressed());` — цикл ЗАВЕРШАЕТСЯ, проходов
   ≤ `GAME_EVENTS_MAX_GENERATIONS` (счётчик в тесте). Фикспойнт: после
   `react_begin()`, если в теле НЕ эмитить — `react_progressed()` false сразу (ноль
   холостых проходов, LOW-8).
7. **сброс курсора между кадрами (HIGH-2, тихая потеря)**: кадр A эмит 5,
   продренить канон-идиомом (`s_pos`→5); `frame_reset`. Кадр B эмит 8; тем же
   потребителем продренить — assert, что видит ВСЕ 8 событий кадра B (сброс по
   смене `game_events_tick()`). Со СТАРЫМ `if (s_pos > n)` упал бы (s_pos=5 не
   сбросился при n=8 → B[0..5) потеряны молча).

Death-тесты (ОСНОВНОЙ бинарь `test_game_events`, гейт `NT_ASSERT_MODE==NT_ASSERT_FULL`,
через `NT_TEST_EXPECT_ASSERT` — хелпер см. кейс #11; при недоступности хелпера все
три — advisory):
8. **переполнение АРЕНЫ → assert**: с `GAME_EVENTS_ARENA_BYTES=1024` (§E1.6) эмитить
   payload'ы, пока арена почти полна, затем `NT_TEST_EXPECT_ASSERT(game_event_emit(
   type, big, size, 1))` — emit, не влезающий в арену, обязан выстрелить NT_ASSERT.
9. **переполнение КАПА ЛОГА → assert**: с `GAME_EVENTS_LOG_CAP=64` эмитнуть 64
   крошечных события, затем `NT_TEST_EXPECT_ASSERT(game_event_emit(...))` — 65-е
   обязано выстрелить NT_ASSERT.
10. **release-семантика дропа (бинарь `test_game_events_overflow`,
   `GAME_EVENTS_SOFT_OVERFLOW=1`)**: переполнить арену ИЛИ кап лога → `game_event_emit`
   возвращает NULL, `game_events_dropped()` вырос (был 0), а РАНЕЕ записанные
   события ЦЕЛЫ (`log[0..cap)` читаются, seq/значения не тронуты). Гейтится
   `#ifdef GAME_EVENTS_SOFT_OVERFLOW` в тест-файле (в основном бинаре переполнение
   = assert, кейс не запускается).
11. **фазовый assert (death-test, ОПЦИОНАЛЬНО, гейт `NT_ASSERT_MODE==NT_ASSERT_FULL`)**:
   `set_phase(RECORD)`; `NT_TEST_EXPECT_ASSERT(game_event_emit(...))` — emit в
   RECORD обязан выстрелить NT_ASSERT. Хелпер `tests/unit/test_helpers/nt_assert_trap.{h,c}`
   (добавить его .c в исходники таргета + include-путь
   `external/neotolis-engine/tests/unit/test_helpers`) ЛИБО локальный setjmp-trap
   (`nt_assert_trap.h:30-40`). Если хелпер недоступен в шаблонном ctest — вынести
   death-тесты #8/#9/#11 в advisory-заметку INSTALL/README; позитив 1-7 + дроп-тест
   #10 покрывают основной контракт.

Плюс **компиляционный смоук**: полный `templates/template` build в native-debug,
release и devapi-debug — `main.c` компилится с двухфазным циклом, `game_features.c`
и `game_events.c` линкуются; ручной прогон бинаря с `--capture` не падает
(двухфазный кадр с пустыми react/record проходит, `frame_reset` отравляет в debug
без use-after-frame, т.к. потребителей ещё нет).

### E1.8 Критерии приёмки (бинарные)

- [ ] `ctest -R test_game_events` зелёный (осн. бинарь: позитив 1-7 + death
      #8/#9/#11); `ctest -R test_game_events_overflow` зелёный (дроп-кейс #10).
- [ ] `game_events.c` НЕ зависит ни от генератора, ни от cJSON, ни от
      `game_save`/`game_storage`/`GameState` (лист-модуль: только nt_hash/nt_log/
      nt_assert + libc).
- [ ] Арена — СВОЯ (в `game_events.c`), НЕ `nt_mem_scratch`; **ФИКСИРОВАННАЯ**
      (один malloc `GAME_EVENTS_ARENA_BYTES` на init), НЕ растёт; `realloc`/ребейз
      ОТСУТСТВУЮТ; лог — фиксированный кап `GAME_EVENTS_LOG_CAP`.
- [ ] Переполнение (арены ИЛИ капа лога): debug = NT_ASSERT (тесты #8/#9);
      release = дроп (`emit`→NULL, `game_events_dropped()`++, per-frame warn),
      остальные события целы (тест #10). Аллокатор изолирован (свап на nt_arena_t
      = замена ~20 строк, §E1.9).
- [ ] `align` контракт: power-of-2 в [1, `_Alignof(max_align_t)`], вне — NT_ASSERT
      (тест #5 покрывает 1/8/16, без сверх-выравнивания).
- [ ] Указатели `game_event_log`/`emit` СТАБИЛЬНЫ весь кадр (нет переезда);
      удержание через emit законно (тест #4). Курсор потребителя сбрасывается по
      СМЕНЕ `game_events_tick()`, не по числу событий (тест #7, нет тихой потери).
- [ ] Отравление арены на `frame_reset` активно под `NT_DEBUG` (memset 0xDD
      used-части фиксированного буфера).
- [ ] Emit в фазе RECORD упирается в NT_ASSERT (страж симметрии).
- [ ] React-фикспойнт завершается ≤ `GAME_EVENTS_MAX_GENERATIONS` поколений, при
      исчерпании — dev-warn; событийный цикл не зацикливается.
- [ ] `seq` глобально монотонен и переживает `frame_reset`; `tick` инкрементится
      на кадр.
- [ ] `main.c`: двухфазный цикл (update→react-до-фикспойнта→record→reset) на месте
      старого `sys_move`; `game_save_tick()` вызывается ПОСЛЕ фазы record (якорь
      main.c:250-252 закрыт); цикл безусловен (не под FEATURE_GAME_STATE).
- [ ] `game_features.c` несёт 7 фазовых функций; `sys_move` продет через
      `game_features_update`; рендер-системы остались прямыми вызовами с
      TODO-якорем.
- [ ] Шаблон собирается **warning-clean под `-Werror` + `nt_set_warning_flags`**
      (`-Wconversion` — касты выравнивания/size явные) в native-debug, release и
      devapi-debug (все компилят `game_events.c`+`game_features.c`).
- [ ] `--capture` прогон не падает; смоук devapi-debug конфигурится и компилится
      (tool parity — hard invariant: агентская сборка несёт тот же кадровый цикл).

### E1.9 Что СОЗНАТЕЛЬНО НЕ входит (E2+ / швы)

- **Генератор секции events в схеме → структ + emit-хелпер + type_hash +
  дескриптор** — **E2** (после A4/v2 генератора, event §6). E1 даёт только сырой
  `game_event_emit(type, bytes, size, align)`; типизированный `items_emit_txn(...)`
  — E2. Шов оставлен: emit-хелпер E2 = обёртка, кладущая структ (str-поля инлайн-
  оффсетами) в арену через сырой emit + регистрирующая имя через
  `game_event_register_type_name`.
- **DevAPI-лента `game.events.tail`** (приватный ринг-копир + generic-рендер по
  дескрипторам, render-at-copy) — **E3** (event §4/§6). Шов: рекордер E3 живёт в
  `game_features_record` (фаза RECORD, арена жива), одной строкой. E1 запись
  оставляет пустой.
- **Локальная аналитика-писатель + встроенный тип `log {str}`** — **E4**
  (event §6). E1 не заводит `log`-тип; сырой emit его поддержит без спец-кейса.
- **Движковый generic-аллокатор `nt_arena_t`** — **отдельный engine-issue заведён**
  (nt_mem_scratch станет его инстансом). Аллокатор арены/лога ИЗОЛИРОВАН внутри
  `game_events.c` (два `malloc` в init, `free` в shutdown, bump в emit, `memset`-
  отравление в reset — всё под приватными статиками). Свап на движковый `nt_arena_t`
  = замена ~20 строк ВНУТРИ `game_events.c`; ПУБЛИЧНЫЙ API `game_events` (emit/walk/
  tick/dropped/phase/reset) НЕ меняется. Поэтому E1 не блокируется движковым тредом
  и не создаёт долга миграции сверх этих ~20 строк.
- **Шов имён типов / NT_HASH_LABELS (ВАЖНО, не блокер E1)**:
  `game_event_register_type_name` компилируется, но `nt_hash_register_label64`
  реально пишет метку ТОЛЬКО если ДВИЖКОВЫЙ таргет `nt_hash` собран с
  `NT_HASH_LABELS=1` (тело функции в `nt_hash.c`; game-side define не влияет на
  уже скомпилированную либу — прецедент: движковый тест `tests/CMakeLists.txt:417`
  `target_compile_definitions(nt_hash PRIVATE NT_HASH_LABELS=1)`). Чтобы E3 DevAPI
  показывал «items.txn», а не хекс, debug/devapi-сборка шаблона должна добавить
  `NT_HASH_LABELS=1` на движковый `nt_hash` таргет (правка CONSUMING CMake, НЕ
  движка — в рамках инварианта). E1 это НЕ включает (лишний rebuild общей либы +
  метки ассетов; решение build-config уровня E2/E3). Помечено швом, а не открытым
  вопросом: E1 поведенчески корректен без меток (лог/DevAPI покажут хеш).
- **Формальные фичи-папки `src/features/<id>/` + settings как первая фича** —
  трек feature-migration (feature_architecture §4 п.1), НЕ E1.
- **Расширение draw-фаз шелловскими хендлами + перенос render_mesh/hud/settings в
  агрегатор** — трек feature-migration (§E1.4).
- **hook-таблица/шедулер, авто-дискавери, per-feature арены** — отвергнуто
  (feature_architecture §5); только явные вызовы.

### E1.10 Пакет делегирования

**Пересмотрено под фикс-арену: fast-worker (Sonnet) пишет весь E1 + deep-review
перед приёмкой.** Вырезание растяжимости (grow/rebase/hook) убрало самый рисковый
кусок — остаётся прямолинейный fixed-bump + счётчик дропов. Механика простая;
точечные риски (все специфицированы):
1. **Гейт assert-vs-drop** — `#if defined(NT_DEBUG) && !defined(GAME_EVENTS_SOFT_OVERFLOW)`
   вокруг NT_ASSERT переполнения (§E1.3 шаг 3); перепутать condition = либо краш в
   release, либо нет death-теста. Покрыто тестами #8/#9 (assert) и #10 (drop).
2. **Канон-идиом потребителя (§E1.4)** — образец, который скопируют ВСЕ будущие
   потребители (E2+); ОДИН инвариант критичен: сброс курсора по `tick`, не по count
   (иначе тихая потеря, HIGH-2). Ре-фетч больше НЕ требуется (фикс-арена). Тест #7.
3. **Фикспойнт-драйвер** — off-by-one кап / незавершение / `react_begin` базлайн
   (§E1.3 `react_progressed`/`react_begin`, тест #6).
4. **`-Werror`/`-Wconversion`/`-Wformat=2`** на арифметике выравнивания,
   `(uint32_t)size`/`(unsigned)` кастах и `%zu`/`%%` в warn'ах (§0, §E1.3) — явные
   касты; критерий требует warning-clean.
5. **Место `game_save_tick()`, безусловность цикла, снятие `sys_move.h`-инклюда**
   в `main.c` (§E1.5) — механично, но легко загейтить цикл под FEATURE_GAME_STATE
   или забыть инклюд.
Рекомендация: **fast-worker (Sonnet)** пишет `game_events.c` + `game_features.{c,h}`
скелет + канон-идиом + `main.c`-проводку + CMake + оба тест-бинаря по §E1.3/§E1.4 и
списку кейсов; **deep-reasoner (или лид) ревьюит** гейт assert-vs-drop, канон-идиом
и фазовый цикл перед приёмкой. Приёмка ОБЯЗАНА включать warning-clean сборку трёх
конфигов + `--capture`-смоук + оба ctest-бинаря.

---

## Открытые вопросы лиду

**Настоящих блокеров нет.** Один пункт вынесен как ЯВНО ЗАФИКСИРОВАННОЕ решение
(не вопрос), т.к. всплывёт в E2/E3 — оставлен следом для аудита:

1. **NT_HASH_LABELS для имён типов — РЕШЕНО (шов, не блокер E1).** Реальные метки
   требуют пересборки движкового `nt_hash` с `NT_HASH_LABELS=1` (§E1.9). E1 даёт
   только регистрационный шов и поведенчески корректен без меток. Включать ли
   метки в debug/devapi-сборке — build-config-решение E3 (когда DevAPI-лента
   начнёт их показывать). Если лид хочет метки раньше — это одна строка
   `target_compile_definitions(nt_hash PRIVATE NT_HASH_LABELS=1)` в шаблонном
   CMake под devapi-гейтом; но это уже E3-скоуп, не E1.

(Иных расхождений дизайна с кодом, требующих решения лида, не найдено.
§Отступления ниже — инженерные решения в рамках дизайна, не вопросы.)

---

## Отступления от буквы дизайн-дока (с обоснованием)

1. **Draw-фазы объявлены, но их место вызова в `frame()` — TODO-якорь (не
   вызываются в E1).** feature_architecture §2 требует 7 фазовых функций в
   `game_features.c` — они ЕСТЬ (объявлены+определены). Но ТЗ явно разрешает
   оставить существующие рендер-системы прямыми вызовами шелла; а перенос
   `render_mesh_draw`/`hud_draw`/`sys_settings_ui` в draw-фазы требует решить, как
   draw-фаза достаёт шелловские GPU-хендлы (material/font/ubo/ctx) — это скоуп
   трека feature-migration, не E1. Значит E1: 7 функций объявлены (контракт §2
   соблюдён), 5 event/lifecycle-фаз проведены в кадр, 2 draw-фазы пусты и их
   вызов — якорь. Отступление в ПОЛНОТЕ проводки, не в форме агрегатора.

2. **`game_events.{c,h}` в корне `src/`, а не в `src/features/`.** feature_arch §2
   пригвождает `src/features/game_features.c` — там он и лежит. Но событийный
   ТРАНСПОРТ — шелловский сервис (глобальный лог, «shell-owned», event §2),
   параллельный `game_save`/`game_storage` (тоже корень `src/`), а не фича и не
   агрегатор. Кладу его рядом с прочими L0-шелл-сервисами в корень `src/`; папку
   `src/features/` заводит `game_features` (агрегатор). Отступление в размещении
   одного файла, обосновано ярусом (транспорт = сервис, не композиция).

3. **`tick` — счётчик, приватный модулю событий, а не `g_nt_app.frame`.** Дизайн:
   «tick = КАДРОВЫЙ счётчик шелла». В движке есть `g_nt_app.frame`
   (`nt_app.h:22`), НО он замораживается на паузе/manual-idle (sim-advance
   счётчик). Событийный лог гоняется КАЖДЫЙ `frame()` независимо от sim-паузы
   (события эмитятся и на dt=0), поэтому беру собственный `s_tick`, инкрементящийся
   в `frame_reset` — монотонный по кадрам лога, развязанный с sim-семантикой
   движка. Отступление в источнике счётчика, обосновано семантикой паузы.

4. **Арена ФИКСИРОВАННАЯ, растяжимость вырезана (РЕШЕНИЕ ЛИДА 2026-07-06, поверх
   дизайна §2 «растяжимая»).** event §2 звал арену растяжимой (growable backstop);
   лид финально отменил это в пользу фиксированной. Обоснование лида: (а) паттерн
   движка — `nt_mem_scratch` = fixed bump + assert на переполнении (`nt_mem_scratch.h:38`),
   E1 ему следует; (б) класс UAF-багов от переезда (ребейз указателей, ре-фетч
   потребителя) исчезает НАВСЕГДА — указатели стабильны весь кадр; (в) каскады и так
   ограничены капом поколений, а не размером арены. Механика: один malloc
   `GAME_EVENTS_ARENA_BYTES` (щедрый дефолт 1МБ, игра переопределяет) + фикс-кап
   лога `GAME_EVENTS_LOG_CAP`; переполнение = debug-assert / release-дроп(+NULL+
   счётчик+per-frame-warn) + soft-75%-аларм. Параллельно заведён engine-issue на
   generic `nt_arena_t` (`nt_mem_scratch` станет его инстансом); наш аллокатор
   свопнется на движковый тип — свап ≈ 20 строк ВНУТРИ `game_events.c`, публичный
   API не меняется (§E1.9). Это ЯВНОЕ override дизайна лидом, не инженерная
   вольность; растяжимый backstop для оффлайн-догона (event §2/Q2) заменён
   щедрым фикс-бюджетом + видимым дропом.

5. **`game_event_register_type_name` (шов имён) введён в E1, хотя потребитель —
   E2.** event §6 относит регистрацию имён к генератору (E2). Ввожу тонкую
   обёртку (2 строки) сразу, чтобы E2 имел готовую шелл-точку входа и не трогал
   `game_events.h` повторно; в E1 не вызывается из шаблона (no-op-семантика без
   NT_HASH_LABELS в любом случае). Не изменение поведения — только шов.
