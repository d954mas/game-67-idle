#include "game_events.h"

#include "core/nt_assert.h"
#include "hash/nt_hash.h"
#include "log/nt_log.h"

#include <stddef.h> /* max_align_t */
#include <stdint.h>
#include <stdlib.h> /* malloc/free -- once each, on init/shutdown; no growth */
#include <string.h> /* memcpy/memset */

/* ---- Файловые статики (владение -- модуль, §E1.3) ---- */
static uint8_t *s_arena;
static size_t s_arena_used; /* cap = GAME_EVENTS_ARENA_BYTES (const) */
static game_event_t *s_log;
static uint32_t s_count; /* cap = GAME_EVENTS_LOG_CAP (const) */
static uint64_t s_seq;   /* переживает frame_reset */
static uint32_t s_tick;  /* bump в frame_reset */
static game_event_phase_t s_phase;
static uint32_t s_react_last_count;
static int s_react_gen;
static uint32_t s_dropped;     /* кумулятивно; НЕ сбрасывается (health-метрика) */
static bool s_overflow_warned; /* per-frame: сбрасывается в frame_reset */
static bool s_soft_warned;     /* per-session: сбрасывается только в init (75%-аларм) */

void game_events_init(void) {
    s_arena = (uint8_t *)malloc((size_t)GAME_EVENTS_ARENA_BYTES);
    NT_ASSERT(s_arena != NULL); /* OOM -- broken environment, not a runtime error */
    s_arena_used = 0;
    s_log = (game_event_t *)malloc((size_t)GAME_EVENTS_LOG_CAP * sizeof(game_event_t));
    NT_ASSERT(s_log != NULL);
    s_count = 0;
    s_seq = 0;
    s_tick = 0;
    s_phase = GAME_EVENT_PHASE_EMIT;
    s_react_last_count = 0;
    s_react_gen = 0;
    s_dropped = 0;
    s_overflow_warned = false;
    s_soft_warned = false;
}

void game_events_shutdown(void) {
    free(s_arena);
    free(s_log);
    s_arena = NULL;
    s_log = NULL;
    s_arena_used = 0;
    s_count = 0;
}

const void *game_event_emit(nt_hash64_t type, const void *payload, uint32_t size, size_t align) {
    /* 1. emit в RECORD = баг (страж симметрии, event §7). */
    NT_ASSERT(s_phase == GAME_EVENT_PHASE_EMIT);

    /* 2. Контракт выравнивания (MEDIUM-3): power-of-2 в [1, max_align_t] (зеркало
       nt_mem_scratch.h:38). База арены выровнена malloc'ом (>= max_align_t) и НЕ
       меняется (фикс-арена) -> смещение-по-align даёт реально выровненный указатель
       весь срок жизни. Сверх-выравнивание > max_align_t запрещено контрактом. */
    NT_ASSERT(align >= 1 && align <= _Alignof(max_align_t) && (align & (align - 1)) == 0);
    size_t off = (s_arena_used + (align - 1)) & ~(align - 1);

    /* 3. Проверка переполнения (арены ИЛИ капа лога -- оба ФИКСИРОВАНЫ, роста НЕТ). */
    size_t need = off + (size_t)size;
    if (need > (size_t)GAME_EVENTS_ARENA_BYTES || s_count >= (uint32_t)GAME_EVENTS_LOG_CAP) {
#if defined(NT_DEBUG) && !defined(GAME_EVENTS_SOFT_OVERFLOW)
        NT_ASSERT(0 && "game_events overflow: raise GAME_EVENTS_ARENA_BYTES/LOG_CAP");
#endif
        s_dropped++; /* release-путь: дроп, не краш */
        if (!s_overflow_warned) {
            nt_log_warn("game_events: overflow at tick %u -> event dropped "
                        "(arena %zu/%u, log %u/%u); raise GAME_EVENTS_ARENA_BYTES/LOG_CAP",
                        s_tick, s_arena_used, (unsigned)GAME_EVENTS_ARENA_BYTES,
                        s_count, (unsigned)GAME_EVENTS_LOG_CAP);
            s_overflow_warned = true; /* один warn за кадр */
        }
        return NULL;
    }

    /* 4. Копия payload'а в арену; мягкий 75%-аларм (один раз за сессию, dev). */
    void *p = s_arena + off;
    memcpy(p, payload, size);
    s_arena_used = off + size;
    if (!s_soft_warned && s_arena_used > ((size_t)GAME_EVENTS_ARENA_BYTES / 4u) * 3u) {
        nt_log_warn("game_events: arena over 75%% (%zu/%u B) at tick %u; consider raising "
                    "GAME_EVENTS_ARENA_BYTES",
                    s_arena_used, (unsigned)GAME_EVENTS_ARENA_BYTES, s_tick);
        s_soft_warned = true;
    }

    /* 5. Дописать конверт (кап уже проверен в шаге 3 -> s_count < GAME_EVENTS_LOG_CAP). */
    s_log[s_count] = (game_event_t){.seq = s_seq++, .tick = s_tick, .type = type, .payload = p, .size = size};
    s_count++;
    return p;
}

const game_event_t *game_event_log(int *count) {
    *count = (int)s_count; /* count <= GAME_EVENTS_LOG_CAP -> всегда влезает в int */
    return s_log;
}

void game_events_set_phase(game_event_phase_t phase) { s_phase = phase; }

uint32_t game_events_tick(void) { return s_tick; }

uint32_t game_events_dropped(void) { return s_dropped; }

void game_events_react_begin(void) {
    /* базлайн фикспойнта = count ПОСЛЕ update; убирает холостой второй проход
       при нуле каскадов (LOW-8). Зовётся шеллом между update и do/while. */
    s_react_last_count = s_count;
    s_react_gen = 0;
}

bool game_events_react_progressed(void) {
    if (s_count == s_react_last_count) {
        return false; /* фикспойнт: новое не родилось */
    }
    s_react_last_count = s_count;
    if (++s_react_gen >= GAME_EVENTS_MAX_GENERATIONS) {
        nt_log_warn("game_events: react generation cap %d hit at tick %u (%u events)",
                    GAME_EVENTS_MAX_GENERATIONS, s_tick, s_count);
        return false;
    }
    return true;
}

void game_event_frame_reset(void) {
#ifdef NT_DEBUG
    if (s_arena_used) {
        memset(s_arena, 0xDD, s_arena_used); /* отравление used-части */
    }
#endif
    s_arena_used = 0;
    s_count = 0;
    s_tick++; /* следующий кадр */
    s_react_last_count = 0;
    s_react_gen = 0;
    s_overflow_warned = false; /* per-frame drop-warn */
    s_phase = GAME_EVENT_PHASE_EMIT;
    /* s_seq (глобальный монотонный), s_dropped (кумулятивная health-метрика) и
       s_soft_warned (аларм на сессию) -- НЕ трогаются. */
}

void game_event_register_type_name(nt_hash64_t type, const char *name) {
    nt_hash_register_label64(type, name); /* тонкий шов; no-op без NT_HASH_LABELS, §E1.9 */
}
