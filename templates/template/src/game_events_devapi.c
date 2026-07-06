/* game_events_devapi.c -- DevAPI tail for the per-frame feature event log.

   Registry (hash -> descriptor) + a private fixed ring of render-at-copy JSON slots +
   the RECORD-phase recorder + the game.events.tail command. render-at-copy (design §4):
   the recorder renders each event to a self-contained JSON string while the event arena
   is alive (RECORD phase), so the bot can poll asynchronously and never touches arena
   pointers. Mirrors the game_save_devapi.c model.

   Compiled ONLY under GAME_DEVAPI_ENABLED (CMake target_sources guard); native-debug/
   release never pull it in. Dev-only, single thread. */

#if NT_DEVAPI_ENABLED

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "core/nt_assert.h"
#include "devapi/nt_devapi.h"
#include "game_event_render.h"
#include "game_events.h"
#include "hash/nt_hash.h" /* nt_hash64_str */
#include "log/nt_log.h"

#ifndef GAME_EVENTS_TAIL_RING_CAP /* design §4: N=256 */
#define GAME_EVENTS_TAIL_RING_CAP 256
#endif
#ifndef GAME_EVENTS_TAIL_ENTRY_MAX /* design §4: <=512B with slice-marker */
#define GAME_EVENTS_TAIL_ENTRY_MAX 512
#endif
#ifndef GAME_EVENTS_DESC_REG_CAP /* max registered descriptors */
#define GAME_EVENTS_DESC_REG_CAP 64
#endif

/* Dev-only, single-threaded static buffer for handler error messages (must outlive the
   call). Local copy of the game_save_devapi.c helper -- not shared across TUs. */
static char s_events_err[256];

static bool state_fail(nt_devapi_error *err, const char *code, const char *message) {
    (void)snprintf(s_events_err, sizeof s_events_err, "%s", message);
    err->code = code;
    err->message = s_events_err;
    return false;
}

/* ---- registry: type-hash -> descriptor ---- */
typedef struct {
    uint64_t hash;
    const game_event_desc_t *desc;
} tail_reg_entry_t;

static tail_reg_entry_t s_reg[GAME_EVENTS_DESC_REG_CAP];
static int s_reg_count;

void game_events_devapi_register_descs(const game_event_desc_t *const *descs, int count) {
    for (int i = 0; i < count; ++i) {
        const game_event_desc_t *d = descs[i];
        if (!d || !d->name) {
            continue;
        }
        const uint64_t h = nt_hash64_str(d->name).value; /* == <frag>_ev_<evt>_type().value */
        for (int j = 0; j < s_reg_count; ++j) {
            NT_ASSERT(s_reg[j].hash != h && "duplicate event type hash in tail registry");
        }
        if (s_reg_count >= GAME_EVENTS_DESC_REG_CAP) {
            nt_log_warn("game.events.tail: descriptor registry full (%d) -> dropping %s",
                        GAME_EVENTS_DESC_REG_CAP, d->name);
            return;
        }
        s_reg[s_reg_count].hash = h;
        s_reg[s_reg_count].desc = d;
        s_reg_count++;
    }
}

static const game_event_desc_t *reg_find(nt_hash64_t type) {
    for (int i = 0; i < s_reg_count; ++i) {
        if (s_reg[i].hash == type.value) {
            return s_reg[i].desc;
        }
    }
    return NULL;
}

/* ---- render-at-copy ring ---- */
static char s_ring[GAME_EVENTS_TAIL_RING_CAP][GAME_EVENTS_TAIL_ENTRY_MAX];
static uint64_t s_ring_seq[GAME_EVENTS_TAIL_RING_CAP];
static uint32_t s_ring_head;
static uint32_t s_ring_count;
static uint64_t s_ring_evicted;
static bool s_enabled; /* set by game_events_register_devapi (only when devapi started) */

void game_events_devapi_record(void) {
    if (!s_enabled) {
        return;
    }
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    /* >CAP events this frame: only the last CAP fit the ring -> render from (n-CAP); the
       earlier ones are lost and counted in evicted (observable via the tail response). */
    const int start = (n > GAME_EVENTS_TAIL_RING_CAP) ? (n - GAME_EVENTS_TAIL_RING_CAP) : 0;
    s_ring_evicted += (uint64_t)start;
    for (int i = start; i < n; ++i) {
        const game_event_t *e = &log[i];
        const game_event_desc_t *d = reg_find(e->type);
        (void)game_event_render(e, d, s_ring[s_ring_head], GAME_EVENTS_TAIL_ENTRY_MAX);
        s_ring_seq[s_ring_head] = e->seq;
        s_ring_head = (s_ring_head + 1u) % (uint32_t)GAME_EVENTS_TAIL_RING_CAP;
        if (s_ring_count < (uint32_t)GAME_EVENTS_TAIL_RING_CAP) {
            s_ring_count++;
        } else {
            s_ring_evicted++;
        }
    }
}

/* ---- command: game.events.tail ---- */
static bool ep_events_tail(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
    /* MED-4: absent param -> default; provided-but-malformed -> bad_params (strictness
       aligned with game.state.*). */
    uint64_t since = 0;
    int limit = GAME_EVENTS_TAIL_RING_CAP;
    const cJSON *ps = cJSON_GetObjectItemCaseSensitive(params, "since_seq");
    if (ps) {
        if (!cJSON_IsNumber(ps) || ps->valuedouble < 0.0) {
            return state_fail(err, "bad_params", "since_seq must be a non-negative number");
        }
        since = (uint64_t)ps->valuedouble;
    }
    const cJSON *pl = cJSON_GetObjectItemCaseSensitive(params, "limit");
    if (pl) {
        if (!cJSON_IsNumber(pl)) {
            return state_fail(err, "bad_params", "limit must be a number");
        }
        const int v = (int)pl->valuedouble;
        if (v < 1 || v > GAME_EVENTS_TAIL_RING_CAP) { /* [1,256] inclusive */
            return state_fail(err, "bad_params", "limit out of range [1,256]");
        }
        limit = v;
    }

    cJSON *events = cJSON_AddArrayToObject(result_obj, "events");
    if (!events) {
        return state_fail(err, "internal", "failed to build events array");
    }
    /* Offset cursor (amended 2026-07-07): since_seq is an INCLUSIVE lower bound (seq >=
       since) and next_seq = last_returned_seq + 1 (the next offset to request); empty ->
       echo since. This delivers the 0-based first event (seq 0) under the default cursor
       (`seq > since` would silently drop it) with no dup and no loss. */
    uint64_t next = since;
    int emitted = 0;
    const uint32_t cap = (uint32_t)GAME_EVENTS_TAIL_RING_CAP;
    /* oldest -> newest: oldest index = (head - count + CAP) % CAP */
    uint32_t idx = (s_ring_head + cap - s_ring_count) % cap;
    for (uint32_t k = 0; k < s_ring_count && emitted < limit; ++k) {
        const uint64_t sq = s_ring_seq[idx];
        if (sq >= since) {
            /* Every slot is valid JSON (renderer invariant) -> Parse is the second gate. */
            cJSON *obj = cJSON_Parse(s_ring[idx]);
            if (obj) {
                cJSON_AddItemToArray(events, obj);
                next = sq + 1u;
                emitted++;
            }
        }
        idx = (idx + 1u) % cap;
    }
    cJSON_AddNumberToObject(result_obj, "next_seq", (double)next);
    cJSON_AddNumberToObject(result_obj, "dropped", (double)game_events_dropped());
    cJSON_AddNumberToObject(result_obj, "evicted", (double)s_ring_evicted);
    return true;
}

void game_events_register_devapi(void) {
    static const nt_devapi_command_desc desc = {
        "game.events.tail", "game",
        "Tail the per-frame feature event log (render-at-copy ring).",
        "since_seq?, limit?", "events, next_seq, dropped, evicted", "immediate", "none"};
    (void)nt_devapi_register(&desc, ep_events_tail, NULL);
    s_enabled = true; /* enable the recorder only once devapi actually started */
}

#endif /* NT_DEVAPI_ENABLED */
