#include "game_events.h"

#include "core/nt_assert.h"
#include "hash/nt_hash.h"
#include "log/nt_log.h"

#include <stddef.h> /* max_align_t */
#include <stdint.h>
#include <stdlib.h> /* malloc/free -- once each, on init/shutdown; no growth */
#include <string.h> /* memcpy/memset */

/* The global API is this context with no prev buffer; phases and the react loop
   belong to it alone. */
static game_events_ctx_t s_default;
static game_event_phase_t s_phase;
static uint32_t s_react_last_count;
static int s_react_gen;

static void buffer_poison_and_clear(game_events_buffer_t *buffer) {
#ifdef NT_DEBUG
    /* Use-after-frame reads 0xDD instead of a plausible stale event. */
    if (buffer->arena_used) {
        memset(buffer->arena, 0xDD, buffer->arena_used);
    }
    if (buffer->count) {
        memset(buffer->log, 0xDD, (size_t)buffer->count * sizeof(game_event_t));
    }
#endif
    buffer->arena_used = 0;
    buffer->count = 0;
}

static const void *ctx_emit(game_events_ctx_t *ctx, nt_hash64_t type, const void *payload, uint32_t size,
                            size_t align) {
    /* The arena base is malloc- or caller-aligned to max_align_t and never moves, so
       an offset rounded to `align` is an aligned pointer for the arena's lifetime. */
    NT_ASSERT(align >= 1 && align <= _Alignof(max_align_t) && (align & (align - 1)) == 0);
    game_events_buffer_t *cur = &ctx->cur;
    const size_t off = (cur->arena_used + (align - 1)) & ~(align - 1);
    const bool fits = off <= ctx->arena_bytes && (size_t)size <= ctx->arena_bytes - off && cur->count < ctx->log_cap;
    if (!fits) {
#if defined(NT_DEBUG) && !defined(GAME_EVENTS_SOFT_OVERFLOW)
        NT_ASSERT(0 && "game_events overflow: raise the arena bytes or the log cap");
#endif
        /* Release drops: the log is telemetry and state is the truth. */
        ctx->dropped++;
        if (!ctx->overflow_warned) {
            nt_log_warn("game_events: overflow at tick %u -> event dropped "
                        "(arena %zu/%zu, log %u/%u); raise the arena bytes or the log cap",
                        ctx->tick, cur->arena_used, ctx->arena_bytes, cur->count, ctx->log_cap);
            ctx->overflow_warned = true;
        }
        return NULL;
    }

    void *p = cur->arena + off;
    memcpy(p, payload, size);
    cur->arena_used = off + size;
    if (!ctx->soft_warned && cur->arena_used > (ctx->arena_bytes / 4u) * 3u) {
        nt_log_warn("game_events: arena over 75%% (%zu/%zu B) at tick %u; consider raising it",
                    cur->arena_used, ctx->arena_bytes, ctx->tick);
        ctx->soft_warned = true;
    }
    cur->log[cur->count] = (game_event_t){.seq = ctx->seq++, .tick = ctx->tick, .type = type, .payload = p, .size = size};
    cur->count++;
    return p;
}

/* ---- Default context ---- */

void game_events_init(void) {
    s_default = (game_events_ctx_t){
        .arena_bytes = (size_t)GAME_EVENTS_ARENA_BYTES,
        .log_cap = (uint32_t)GAME_EVENTS_LOG_CAP,
    };
    s_default.cur.arena = (uint8_t *)malloc((size_t)GAME_EVENTS_ARENA_BYTES);
    NT_ASSERT(s_default.cur.arena != NULL); /* OOM -- broken environment, not a runtime error */
    s_default.cur.log = (game_event_t *)malloc((size_t)GAME_EVENTS_LOG_CAP * sizeof(game_event_t));
    NT_ASSERT(s_default.cur.log != NULL);
    s_phase = GAME_EVENT_PHASE_EMIT;
    s_react_last_count = 0;
    s_react_gen = 0;
}

void game_events_shutdown(void) {
    free(s_default.cur.arena);
    free(s_default.cur.log);
    s_default.cur = (game_events_buffer_t){0};
}

const void *game_event_emit(nt_hash64_t type, const void *payload, uint32_t size, size_t align) {
    /* Emitting during RECORD violates phase symmetry. */
    NT_ASSERT(s_phase == GAME_EVENT_PHASE_EMIT);
    return ctx_emit(&s_default, type, payload, size, align);
}

const game_event_t *game_event_log(int *count) {
    *count = (int)s_default.cur.count; /* count <= GAME_EVENTS_LOG_CAP fits an int */
    return s_default.cur.log;
}

void game_events_set_phase(game_event_phase_t phase) { s_phase = phase; }

uint32_t game_events_tick(void) { return s_default.tick; }

uint32_t game_events_dropped(void) { return s_default.dropped; }

void game_events_react_begin(void) {
    /* The baseline is the count after update, so zero cascades cost no second pass. */
    s_react_last_count = s_default.cur.count;
    s_react_gen = 0;
}

bool game_events_react_progressed(void) {
    if (s_default.cur.count == s_react_last_count) {
        return false;
    }
    s_react_last_count = s_default.cur.count;
    if (++s_react_gen >= GAME_EVENTS_MAX_GENERATIONS) {
        nt_log_warn("game_events: react generation cap %d hit at tick %u (%u events)", GAME_EVENTS_MAX_GENERATIONS,
                    s_default.tick, s_default.cur.count);
        return false;
    }
    return true;
}

void game_event_frame_reset(void) {
    buffer_poison_and_clear(&s_default.cur);
    s_default.tick++;
    s_default.overflow_warned = false;
    s_react_last_count = 0;
    s_react_gen = 0;
    s_phase = GAME_EVENT_PHASE_EMIT;
}

/* ---- Caller-owned contexts ---- */

static size_t arena_stride(size_t arena_bytes) {
    const size_t a = _Alignof(max_align_t);
    return (arena_bytes + (a - 1)) & ~(a - 1);
}

size_t game_events_ctx_memory_bytes(size_t arena_bytes, uint32_t log_cap) {
    return 2u * arena_stride(arena_bytes) + 2u * (size_t)log_cap * sizeof(game_event_t);
}

void game_events_ctx_init(game_events_ctx_t *ctx, void *memory, size_t memory_bytes, size_t arena_bytes,
                          uint32_t log_cap) {
    NT_ASSERT(ctx != NULL && memory != NULL && log_cap > 0);
    NT_ASSERT(((uintptr_t)memory & (_Alignof(max_align_t) - 1)) == 0);
    NT_ASSERT(memory_bytes >= game_events_ctx_memory_bytes(arena_bytes, log_cap));
    const size_t stride = arena_stride(arena_bytes);
    uint8_t *base = (uint8_t *)memory;
    /* Logs follow both arenas; a stride of max_align_t keeps them aligned. */
    game_event_t *logs = (game_event_t *)(void *)(base + 2u * stride);
    *ctx = (game_events_ctx_t){
        .cur = {.arena = base, .log = logs},
        .prev = {.arena = base + stride, .log = logs + log_cap},
        .arena_bytes = arena_bytes,
        .log_cap = log_cap,
    };
}

const void *game_events_ctx_emit(game_events_ctx_t *ctx, nt_hash64_t type, const void *payload, uint32_t size,
                                 size_t align) {
    NT_ASSERT(ctx->prev.arena != NULL); /* an initialized context, not the default */
    return ctx_emit(ctx, type, payload, size, align);
}

const game_event_t *game_events_ctx_log(const game_events_ctx_t *ctx, int *count) {
    *count = (int)ctx->cur.count;
    return ctx->cur.log;
}

const game_event_t *game_events_ctx_prev(const game_events_ctx_t *ctx, int *count) {
    *count = (int)ctx->prev.count;
    return ctx->prev.log;
}

void game_events_ctx_swap(game_events_ctx_t *ctx) {
    NT_ASSERT(ctx->prev.arena != NULL);
    game_events_buffer_t expired = ctx->prev;
    buffer_poison_and_clear(&expired);
    ctx->prev = ctx->cur;
    ctx->cur = expired;
    ctx->tick++;
    ctx->overflow_warned = false;
}

uint32_t game_events_ctx_tick(const game_events_ctx_t *ctx) { return ctx->tick; }

uint32_t game_events_ctx_dropped(const game_events_ctx_t *ctx) { return ctx->dropped; }

void game_event_register_type_name(nt_hash64_t type, const char *name) {
    nt_hash_register_label64(type, name); /* No-op without NT_HASH_LABELS. */
}
