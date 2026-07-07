#ifndef GAME_EVENTS_H
#define GAME_EVENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hash/nt_hash.h" /* nt_hash64_t */

/* Кадровый лог событий фич (event_system_design §2, вариант D + РЕШЕНИЕ ЛИДА
   2026-07-06: арена ФИКСИРОВАННАЯ, растяжимость вырезана — паттерн движка
   nt_mem_scratch = fixed bump + assert на переполнении; класс UAF-багов от
   переезда исключён навсегда; каскады ограничены капом поколений). События живут
   ОДИН кадр в приватной, отравляемой, ФИКСИРОВАННОЙ арене game-side. Производители
   — только emit-хелперы; потребители дренят В ТОМ ЖЕ кадре по индексу. НЕ сейв, НЕ
   источник истины (истина = стейт). Один тред. Ноль зависимостей от генератора. */

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
   указатель и любой game_event_t* / payload валидны до game_event_frame_reset();
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
