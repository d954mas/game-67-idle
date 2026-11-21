#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "stub_wire.h"

/* The naive seam: every bound body's raw pose, velocities and motion type to every seat, every
   tick, in as many packets as it takes; copies are kinematic bodies driven to the newest pose.
   It is the reference for the contract and the baseline the real sender and receiver are
   measured against, so it is plain rather than clever. */

_Static_assert(BODY_SYNC_BODIES_MAX == STUB_SLOTS, "stub_wire.c checks slots against STUB_SLOTS");
_Static_assert(BODY_SYNC_TRAFFIC_COUNT == 5, "body_sync_metrics.h repeats the seam's kind count");

enum { RECORD_BYTES = 16, OWED_WORDS = BODY_SYNC_BODIES_MAX / 64 };
/* A copy farther than this from its new pose jumps there instead of being driven at speed. */
#define SNAP_DISTANCE 2.0f

typedef struct entry {
    bool bound;
    uint16_t generation;
    b3BodyId body;
    body_sync_class_t body_class;
    b3BodyType bound_type;
    uint8_t seat;
    uint16_t number;
    uint8_t hold;
    uint32_t holder;
    uint32_t last_holder; /* a settled hold ignores contacts with the character that let it go */
    uint8_t flags;
    bool claim_pending; /* guest: simulating optimistically until the host answers claim_seq */
    uint16_t claim_seq;
    bool holder_mine;     /* guest: the caller set the holder since this endpoint became local */
    uint16_t granted_seq; /* host: the claim the current owner was granted, 0 after any other move */
    bool copy_mode;
    b3MotionLocks bound_locks; /* restored when a copy turns local with no state received */
    bool placed;
    bool resume; /* became local: continue from the newest state at the next apply */
    bool has_out;
    stub_body_state_t out;
    bool has_in;
    bool in_fresh;
    uint8_t in_from;
    uint32_t in_tick;
    stub_body_state_t in;
} entry_t;

/* Host: the newest claim of one seat for one body; a seat has at most one per body. */
typedef struct pending {
    bool active;
    bool polled;
    uint16_t sequence;
    uint8_t hold;
    uint32_t tick;
    uint32_t order;
} pending_t;

typedef struct queue {
    uint8_t data[BODY_SYNC_RECORDS_QUEUED][RECORD_BYTES];
    uint8_t size[BODY_SYNC_RECORDS_QUEUED];
    uint32_t head, count;
} queue_t;

struct body_sync {
    body_sync_config_t config;
    stub_limits_t limits;
    float dt;
    entry_t entries[BODY_SYNC_BODIES_MAX];
    /* guest */
    queue_t to_host;
    uint16_t claim_counter;
    /* host: claims, and what each seat is owed (latest authority of a body, the answer to its claim) */
    pending_t pending[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX];
    uint32_t pending_count;
    uint32_t order;
    uint64_t owed[BODY_SYNC_SEATS_MAX][OWED_WORDS];
    uint16_t answer[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX];
    bool forced[BODY_SYNC_SEATS_MAX][BODY_SYNC_BODIES_MAX]; /* the owed move answers no claim */
    bool joined[BODY_SYNC_SEATS_MAX];
    uint32_t cursor[BODY_SYNC_SEATS_MAX];
    bool writing[BODY_SYNC_SEATS_MAX];
    body_sync_traffic_t traffic[BODY_SYNC_SEATS_MAX];
    stub_body_state_t scratch[BODY_SYNC_BODIES_MAX];
    char error[128];
};

const char *body_sync_implementation(void) { return "stub 0.4.0"; }

const char *body_sync_traffic_kind_name(body_sync_traffic_kind_t kind) {
    static const char *names[] = {"header", "copy", "sim", "held", "authority"};
    return (unsigned)kind < BODY_SYNC_TRAFFIC_COUNT ? names[kind] : "unknown";
}

const char *body_sync_last_error(const body_sync_t *sync) { return sync ? sync->error : ""; }

static body_sync_result_t invalid(body_sync_t *s, const char *what) {
    snprintf(s->error, sizeof(s->error), "%s", what);
    return BODY_SYNC_INVALID;
}

static bool is_host(const body_sync_t *s) { return s->config.seat == BODY_SYNC_SEAT_HOST; }
static bool newer16(uint16_t a, uint16_t b) { return (int16_t)(uint16_t)(a - b) > 0; }
static bool tick_newer(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

static entry_t *entry_for(body_sync_t *s, body_sync_id_t id) {
    uint32_t slot = body_sync_id_slot(id);
    if (slot >= BODY_SYNC_BODIES_MAX || body_sync_id_generation(id) == 0) {
        return NULL;
    }
    entry_t *e = &s->entries[slot];
    return e->bound && e->generation == body_sync_id_generation(id) ? e : NULL;
}

static const entry_t *entry_const(const body_sync_t *s, body_sync_id_t id) { return entry_for((body_sync_t *)s, id); }

static uint32_t slot_of(const body_sync_t *s, const entry_t *e) { return (uint32_t)(e - s->entries); }

static bool local(const body_sync_t *s, const entry_t *e) {
    return e->seat == s->config.seat || (!is_host(s) && e->claim_pending);
}

/* Becoming local continues the last owner's body: its state bits and hold now, its motion type
   and velocities at the next apply. The carrier stays only where the header says (a seat that
   left); a claim or a move starts with no holder. */
static void take_local(entry_t *e, bool keep_holder) {
    if (e->has_in) {
        e->flags = e->in.flags;
        e->hold = e->in.hold;
    }
    e->holder = keep_holder && e->has_in ? e->in.holder : BODY_SYNC_ID_NONE;
    e->holder_mine = false;
    e->resume = true;
}

/* Host: the one place authority changes on the host. `granted` is the claim answered, 0 for a
   forced move. */
static void host_move(body_sync_t *s, entry_t *e, uint8_t seat, uint8_t hold, uint16_t granted, bool keep_holder) {
    bool was_local = local(s, e);
    uint32_t slot = slot_of(s, e);
    e->seat = seat;
    e->number++;
    e->granted_seq = granted;
    if (seat == BODY_SYNC_SEAT_HOST && !was_local) {
        take_local(e, keep_holder);
    }
    if (seat != BODY_SYNC_SEAT_HOST) {
        e->holder = BODY_SYNC_ID_NONE;
    }
    e->hold = hold;
    for (uint8_t to = 1; to < s->config.seats; to++) {
        if (s->joined[to]) {
            s->owed[to][slot / 64] |= 1ull << (slot % 64);
            s->forced[to][slot] = granted == 0;
        }
    }
}

/* Guest: the one place authority changes on a guest, from an announcement or a host state. A
   state never makes this endpoint local: only an announcement or its own claim does, so a
   grant of a claim it has released since cannot hand the body back; a forced move can. */
static void guest_authority(body_sync_t *s, entry_t *e, const stub_record_t *r, bool from_record) {
    uint8_t me = s->config.seat;
    if (!from_record && r->seat == me) {
        return;
    }
    bool was_local = local(s, e);
    bool answers_mine = from_record && r->sequence != 0 && e->claim_pending && r->sequence == e->claim_seq;
    if (answers_mine) {
        e->claim_pending = false;
    }
    if (newer16(r->number, e->number)) {
        e->number = r->number;
        bool stale_grant = r->seat == me && !answers_mine && r->sequence != 0 && !r->forced;
        e->seat = stale_grant ? BODY_SYNC_SEAT_HOST : r->seat;
    }
    bool now_local = local(s, e);
    if (!was_local && now_local) {
        take_local(e, false);
        e->hold = r->hold;
    }
    /* The announcement names the holder; a claimant that set its own since keeps it. */
    if (from_record && now_local && (!was_local || answers_mine) && !e->holder_mine) {
        e->holder = r->holder;
    }
}

body_sync_t *body_sync_create(const body_sync_config_t *config) {
    if (!config || !b3World_IsValid(config->world) || config->seats < 1 || config->seats > BODY_SYNC_SEATS_MAX ||
        config->seat >= config->seats || config->tick_hz < 1 || config->tick_hz > 1000 || !(isfinite(config->max_speed) && config->max_speed > 0.0f) ||
        !(isfinite(config->max_spin) && config->max_spin > 0.0f)) {
        return NULL;
    }
    for (int i = 0; i < 3; i++) {
        if (!(isfinite(config->bounds_min[i]) && isfinite(config->bounds_max[i]) && config->bounds_min[i] < config->bounds_max[i])) {
            return NULL;
        }
    }
    body_sync_t *s = calloc(1, sizeof(*s));
    if (!s) {
        return NULL;
    }
    s->config = *config;
    for (int i = 0; i < 3; i++) {
        s->limits.bounds_min[i] = config->bounds_min[i];
        s->limits.bounds_max[i] = config->bounds_max[i];
    }
    s->limits.max_speed = config->max_speed;
    s->limits.max_spin = config->max_spin;
    s->dt = 1.0f / (float)config->tick_hz;
    return s;
}

void body_sync_destroy(body_sync_t *sync) { free(sync); }

static bool seat_ok(const body_sync_t *s, uint8_t seat) { return seat < s->config.seats && seat != s->config.seat; }

static void forget_seat(body_sync_t *s, uint8_t seat) {
    memset(s->owed[seat], 0, sizeof(s->owed[seat]));
    memset(s->answer[seat], 0, sizeof(s->answer[seat]));
    memset(s->forced[seat], 0, sizeof(s->forced[seat]));
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        if (s->pending[seat][slot].active) {
            s->pending[seat][slot].active = false;
            s->pending_count--;
        }
    }
    s->writing[seat] = false;
    if (seat == BODY_SYNC_SEAT_HOST) {
        memset(&s->to_host, 0, sizeof(s->to_host));
    }
}

void body_sync_seat_joined(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat) || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return;
    }
    forget_seat(s, seat);
    s->joined[seat] = true;
}

void body_sync_seat_left(body_sync_t *s, uint8_t seat) {
    if (!s || !seat_ok(s, seat)) {
        return;
    }
    s->joined[seat] = false;
    forget_seat(s, seat);
    if (!is_host(s)) {
        return;
    }
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &s->entries[slot];
        if (e->bound && e->seat == seat) {
            host_move(s, e, BODY_SYNC_SEAT_HOST, e->has_in ? e->in.hold : BODY_SYNC_HOLD_UNTIL_RELEASE, 0, true);
        }
    }
}

body_sync_result_t body_sync_bind(body_sync_t *s, body_sync_id_t id, const body_sync_bind_t *bind) {
    uint32_t slot = body_sync_id_slot(id);
    if (!bind || slot >= BODY_SYNC_BODIES_MAX || body_sync_id_generation(id) == 0 || !b3Body_IsValid(bind->body) ||
        b3Body_GetType(bind->body) == b3_staticBody || bind->body_class > BODY_SYNC_CLASS_CHARACTER || bind->authority >= s->config.seats) {
        return invalid(s, "bind: id, body, class or authority out of range");
    }
    entry_t *e = &s->entries[slot];
    if (e->bound) {
        return BODY_SYNC_TAKEN;
    }
    memset(e, 0, sizeof(*e));
    e->bound = true;
    e->generation = (uint16_t)body_sync_id_generation(id);
    e->body = bind->body;
    e->body_class = bind->body_class;
    e->bound_type = b3Body_GetType(bind->body);
    e->bound_locks = b3Body_GetMotionLocks(bind->body);
    e->seat = bind->authority;
    e->number = bind->number;
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_unbind(body_sync_t *s, body_sync_id_t id) {
    entry_t *e = entry_for(s, id);
    if (!e) {
        return BODY_SYNC_UNKNOWN;
    }
    uint32_t slot = slot_of(s, e);
    e->bound = false;
    for (uint8_t seat = 0; seat < BODY_SYNC_SEATS_MAX; seat++) {
        s->owed[seat][slot / 64] &= ~(1ull << (slot % 64));
        s->answer[seat][slot] = 0;
        if (s->pending[seat][slot].active) {
            s->pending[seat][slot].active = false;
            s->pending_count--;
        }
    }
    return BODY_SYNC_OK;
}

b3BodyId body_sync_body(const body_sync_t *s, body_sync_id_t id) {
    const entry_t *e = entry_const(s, id);
    return e ? e->body : b3_nullBodyId;
}

bool body_sync_authority(const body_sync_t *s, body_sync_id_t id, body_sync_authority_t *out) {
    const entry_t *e = entry_const(s, id);
    if (!e) {
        return false;
    }
    bool mine = local(s, e);
    out->seat = e->seat;
    out->number = e->number;
    out->hold = (body_sync_hold_t)(mine || !e->has_in ? e->hold : e->in.hold);
    out->holder = mine || !e->has_in ? e->holder : e->in.holder;
    return true;
}

bool body_sync_is_local(const body_sync_t *s, body_sync_id_t id) {
    const entry_t *e = entry_const(s, id);
    return e && local(s, e);
}

static bool queue_push(body_sync_t *s, const stub_record_t *record) {
    queue_t *q = &s->to_host;
    if (q->count >= BODY_SYNC_RECORDS_QUEUED) {
        return false;
    }
    uint32_t at = (q->head + q->count) % BODY_SYNC_RECORDS_QUEUED;
    q->size[at] = (uint8_t)stub_wire_put_record(record, q->data[at], RECORD_BYTES);
    q->count++;
    return true;
}

body_sync_result_t body_sync_claim(body_sync_t *s, body_sync_id_t id, body_sync_hold_t hold) {
    entry_t *e = entry_for(s, id);
    if (!e) {
        return BODY_SYNC_UNKNOWN;
    }
    if (hold > BODY_SYNC_HOLD_UNTIL_SETTLED) {
        return invalid(s, "claim: unknown hold");
    }
    if (is_host(s)) {
        if (e->seat != BODY_SYNC_SEAT_HOST) {
            host_move(s, e, BODY_SYNC_SEAT_HOST, (uint8_t)hold, 0, false);
        }
        e->hold = (uint8_t)hold;
        return BODY_SYNC_OK;
    }
    if (!local(s, e)) {
        if (!s->joined[BODY_SYNC_SEAT_HOST]) {
            return invalid(s, "claim: no host");
        }
        uint16_t sequence = (uint16_t)(s->claim_counter + 1u);
        sequence = sequence == 0 ? 1 : sequence;
        stub_record_t record = {.type = STUB_REC_CLAIM, .id = id, .hold = (uint8_t)hold, .sequence = sequence};
        if (!queue_push(s, &record)) {
            return BODY_SYNC_FULL;
        }
        s->claim_counter = sequence;
        e->claim_pending = true;
        e->claim_seq = sequence;
        take_local(e, false);
    }
    e->hold = (uint8_t)hold;
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_release(body_sync_t *s, body_sync_id_t id) {
    entry_t *e = entry_for(s, id);
    if (!e) {
        return BODY_SYNC_UNKNOWN;
    }
    if (is_host(s)) {
        e->hold = BODY_SYNC_HOLD_UNTIL_RELEASE;
        e->holder = BODY_SYNC_ID_NONE;
        return BODY_SYNC_OK;
    }
    if (!local(s, e) || !s->joined[BODY_SYNC_SEAT_HOST]) {
        return invalid(s, "release: the body is not simulated here, or no host");
    }
    stub_record_t record = {.type = STUB_REC_RELEASE, .id = id, .number = e->number, .sequence = e->claim_seq};
    if (!queue_push(s, &record)) {
        return BODY_SYNC_FULL;
    }
    e->seat = BODY_SYNC_SEAT_HOST;
    e->claim_pending = false;
    e->hold = BODY_SYNC_HOLD_UNTIL_RELEASE;
    e->holder = BODY_SYNC_ID_NONE;
    return BODY_SYNC_OK;
}

bool body_sync_poll_claim(body_sync_t *s, body_sync_claim_event_t *out) {
    if (!is_host(s) || s->pending_count == 0) {
        return false;
    }
    pending_t *best = NULL;
    uint8_t best_seat = 0;
    uint32_t best_slot = 0;
    for (uint8_t seat = 1; seat < s->config.seats; seat++) {
        for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
            pending_t *p = &s->pending[seat][slot];
            if (p->active && !p->polled && (!best || (int32_t)(p->order - best->order) < 0)) {
                best = p;
                best_seat = seat;
                best_slot = slot;
            }
        }
    }
    if (!best) {
        return false;
    }
    best->polled = true;
    *out = (body_sync_claim_event_t){.id = body_sync_id_make(best_slot, s->entries[best_slot].generation),
                                     .seat = best_seat,
                                     .sequence = best->sequence,
                                     .tick = best->tick,
                                     .hold = (body_sync_hold_t)best->hold};
    return true;
}

body_sync_result_t body_sync_answer(body_sync_t *s, const body_sync_claim_event_t *claim, bool granted) {
    if (!is_host(s) || !claim) {
        return invalid(s, "answer: host only");
    }
    entry_t *e = entry_for(s, claim->id);
    if (!e || claim->seat == BODY_SYNC_SEAT_HOST || claim->seat >= s->config.seats) {
        return BODY_SYNC_UNKNOWN;
    }
    uint32_t slot = slot_of(s, e);
    pending_t *p = &s->pending[claim->seat][slot];
    if (!p->active || p->sequence != claim->sequence) {
        return BODY_SYNC_UNKNOWN;
    }
    p->active = false;
    s->pending_count--;
    s->answer[claim->seat][slot] = claim->sequence;
    if (granted) {
        host_move(s, e, claim->seat, p->hold, claim->sequence, false);
        e->holder = BODY_SYNC_ID_NONE;
    } else {
        s->owed[claim->seat][slot / 64] |= 1ull << (slot % 64);
    }
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_assign(body_sync_t *s, body_sync_id_t id, uint8_t seat, body_sync_hold_t hold) {
    if (!is_host(s)) {
        return invalid(s, "assign: host only");
    }
    entry_t *e = entry_for(s, id);
    if (!e) {
        return BODY_SYNC_UNKNOWN;
    }
    if (seat >= s->config.seats || (seat != BODY_SYNC_SEAT_HOST && !s->joined[seat]) || hold > BODY_SYNC_HOLD_UNTIL_SETTLED) {
        return invalid(s, "assign: nobody sits there, or an unknown hold");
    }
    bool moved = seat != e->seat;
    host_move(s, e, seat, (uint8_t)hold, 0, false);
    /* A forced move takes the body out of its carrier's hands. */
    if (moved) {
        e->holder = BODY_SYNC_ID_NONE;
    }
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_set_holder(body_sync_t *s, body_sync_id_t id, body_sync_id_t holder) {
    entry_t *e = entry_for(s, id);
    if (!e) {
        return BODY_SYNC_UNKNOWN;
    }
    if (!local(s, e) || (holder != BODY_SYNC_ID_NONE && (body_sync_id_slot(holder) >= BODY_SYNC_BODIES_MAX || body_sync_id_generation(holder) == 0))) {
        return invalid(s, "set_holder: not local, or the holder is out of range");
    }
    e->holder = holder;
    e->holder_mine = true;
    if (holder != BODY_SYNC_ID_NONE) {
        e->last_holder = holder;
    }
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_set_flags(body_sync_t *s, body_sync_id_t id, uint8_t flags) {
    entry_t *e = entry_for(s, id);
    if (!e) {
        return BODY_SYNC_UNKNOWN;
    }
    if (!local(s, e) || (flags >> BODY_SYNC_FLAG_BITS) != 0) {
        return invalid(s, "set_flags: not local, or bits above BODY_SYNC_FLAG_BITS");
    }
    e->flags = flags;
    return BODY_SYNC_OK;
}

uint8_t body_sync_flags(const body_sync_t *s, body_sync_id_t id) {
    const entry_t *e = entry_const(s, id);
    if (!e) {
        return 0;
    }
    return local(s, e) || !e->has_in ? e->flags : e->in.flags;
}

void body_sync_set_peer_tick(body_sync_t *s, uint8_t seat, double tick) {
    (void)s;
    (void)seat;
    (void)tick;
}

static body_sync_result_t format(body_sync_t *s, uint8_t seat, const char *what) {
    snprintf(s->error, sizeof(s->error), "seat %u: %s", (unsigned)seat, what);
    s->traffic[seat].refused++;
    return BODY_SYNC_FORMAT;
}

static void store_in(entry_t *e, uint8_t from, uint32_t tick, const stub_body_state_t *b, body_sync_traffic_t *t) {
    /* A new sender's clock is not comparable with the old one's; within one sender, older loses. */
    if (e->has_in && e->in_from == from && !tick_newer(tick, e->in_tick)) {
        t->stale++;
        return;
    }
    e->in = *b;
    e->has_in = true;
    e->in_fresh = true;
    e->in_from = from;
    e->in_tick = tick;
}

body_sync_result_t body_sync_receive_state(body_sync_t *s, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size) {
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return invalid(s, "receive_state: not a seat this side hears");
    }
    uint32_t count = 0;
    char why[96];
    if (!stub_wire_read_state(data, size, s->config.seats, &s->limits, s->scratch, BODY_SYNC_BODIES_MAX, &count, why, sizeof(why))) {
        return format(s, seat, why);
    }
    body_sync_traffic_t *t = &s->traffic[seat];
    t->received_bytes[BODY_SYNC_TRAFFIC_HEADER] += STUB_HEADER_BYTES;
    t->received_bytes[BODY_SYNC_TRAFFIC_COPY] += (uint64_t)count * STUB_COPY_BYTES;
    t->received_bytes[BODY_SYNC_TRAFFIC_SIM] += (uint64_t)count * STUB_SIM_BYTES;
    t->received_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += (uint64_t)count * STUB_AUTHORITY_BYTES;
    t->received_bytes[BODY_SYNC_TRAFFIC_HELD] += (uint64_t)count * STUB_HELD_BYTES;
    for (uint32_t i = 0; i < count; i++) {
        const stub_body_state_t *b = &s->scratch[i];
        entry_t *e = entry_for(s, b->id);
        if (!e) {
            t->stale++;
            continue;
        }
        if (is_host(s)) {
            if (e->seat != seat || b->number != e->number) {
                t->rejected++;
                continue;
            }
        } else {
            stub_record_t seen = {.type = STUB_REC_ASSIGN, .seat = b->seat, .number = b->number, .hold = b->hold};
            guest_authority(s, e, &seen, false);
            if (local(s, e)) {
                continue;
            }
        }
        store_in(e, seat, tick, b, t);
    }
    return BODY_SYNC_OK;
}

body_sync_result_t body_sync_receive_record(body_sync_t *s, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size) {
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST)) {
        return invalid(s, "receive_record: not a seat this side hears");
    }
    stub_record_t r;
    char why[96];
    if (!stub_wire_read_record(data, size, s->config.seats, !is_host(s), &r, why, sizeof(why))) {
        return format(s, seat, why);
    }
    body_sync_traffic_t *t = &s->traffic[seat];
    t->received_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += size;
    entry_t *e = entry_for(s, r.id);
    if (!e) {
        t->stale++;
        return BODY_SYNC_OK;
    }
    uint32_t slot = slot_of(s, e);
    pending_t *p = is_host(s) ? &s->pending[seat][slot] : NULL;
    switch (r.type) {
    case STUB_REC_CLAIM:
        if (!p->active) {
            s->pending_count++;
        }
        *p = (pending_t){.active = true, .sequence = r.sequence, .hold = r.hold, .tick = tick, .order = s->order++};
        break;
    case STUB_REC_RELEASE: {
        /* Whatever else it does, a release withdraws its seat's claim of the body. Left
           pending, the claim would be granted to a guest that reads the grant as stale, and
           nobody would simulate the body. */
        const bool withdrew = p->active;
        if (withdrew) {
            p->active = false;
            s->pending_count--;
        }
        /* A release counts at the number its sender saw, or for the grant it crossed; an older
           one (the host moved the body by force since) is stale and changes nothing. */
        if (e->seat == seat && (r.number == e->number || (r.sequence != 0 && r.sequence == e->granted_seq))) {
            host_move(s, e, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE, 0, false);
        } else if (!withdrew) {
            t->stale++;
        }
        break;
    }
    case STUB_REC_ASSIGN:
        guest_authority(s, e, &r, true);
        break;
    }
    return BODY_SYNC_OK;
}

static b3Quat quat_of(const float q[4]) { return (b3Quat){{q[0], q[1], q[2]}, q[3]}; }

static uint8_t locks_bits(b3MotionLocks l) {
    return (uint8_t)(l.linearX | l.linearY << 1 | l.linearZ << 2 | l.angularX << 3 | l.angularY << 4 | l.angularZ << 5);
}

static b3MotionLocks locks_of(uint8_t bits) {
    return (b3MotionLocks){.linearX = (bits & 1u) != 0, .linearY = (bits & 2u) != 0, .linearZ = (bits & 4u) != 0,
                           .angularX = (bits & 8u) != 0, .angularY = (bits & 16u) != 0, .angularZ = (bits & 32u) != 0};
}

void body_sync_apply(body_sync_t *s, uint32_t tick) {
    (void)tick;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &s->entries[slot];
        if (!e->bound || !b3Body_IsValid(e->body)) {
            continue;
        }
        if (local(s, e)) {
            if (e->copy_mode || e->resume) {
                bool was_copy = e->copy_mode;
                e->copy_mode = false;
                e->resume = false;
                if (e->has_in) {
                    b3Body_SetType(e->body, e->in.kinematic ? b3_kinematicBody : b3_dynamicBody);
                    b3Body_SetMotionLocks(e->body, locks_of(e->in.locks));
                    b3Body_SetLinearVelocity(e->body, (b3Vec3){e->in.velocity[0], e->in.velocity[1], e->in.velocity[2]});
                    b3Body_SetAngularVelocity(e->body, (b3Vec3){e->in.angular[0], e->in.angular[1], e->in.angular[2]});
                } else if (was_copy) {
                    b3Body_SetType(e->body, e->bound_type);
                    b3Body_SetMotionLocks(e->body, e->bound_locks);
                }
            }
            continue;
        }
        if (!e->copy_mode) {
            e->copy_mode = true;
            e->placed = false;
            b3Body_SetType(e->body, b3_kinematicBody);
            /* A lock would drop the angular velocity that turns a copy to its owner's rotation
               (a character's facing); the owner's locks travel in its state instead. */
            b3Body_SetMotionLocks(e->body, (b3MotionLocks){0});
        }
        if (e->has_in && e->in_fresh) {
            e->in_fresh = false;
            const stub_body_state_t *in = &e->in;
            b3Vec3 target = {in->position[0], in->position[1], in->position[2]};
            b3Vec3 now = b3Body_GetPosition(e->body);
            b3Vec3 d = {target.x - now.x, target.y - now.y, target.z - now.z};
            float distance = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
            b3Quat q = b3Body_GetRotation(e->body);
            bool same = distance == 0.0f && q.v.x == in->rotation[0] && q.v.y == in->rotation[1] && q.v.z == in->rotation[2] &&
                        q.s == in->rotation[3];
            /* A body asleep at its owner is placed on that exact pose, so worlds at rest agree to the bit. */
            if (!e->placed || distance > SNAP_DISTANCE || (in->asleep && !same)) {
                b3Body_SetTransform(e->body, target, quat_of(in->rotation));
                b3Body_SetLinearVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
                b3Body_SetAngularVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
                e->placed = true;
            } else if (!in->asleep) {
                b3Body_SetTargetTransform(e->body, (b3WorldTransform){target, quat_of(in->rotation)}, s->dt, true);
            } else if (b3Body_IsAwake(e->body)) {
                b3Body_SetLinearVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
                b3Body_SetAngularVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
            }
        } else if (b3Body_IsAwake(e->body)) {
            /* No news this tick: hold still rather than coast on the last target's velocity. */
            b3Body_SetLinearVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
            b3Body_SetAngularVelocity(e->body, (b3Vec3){0.0f, 0.0f, 0.0f});
        }
    }
}

static bool touched_moving(const b3ContactEvents *events, b3BodyId body, b3BodyId ignore) {
    for (int i = 0; i < events->beginCount; i++) {
        b3ShapeId a = events->beginEvents[i].shapeIdA, b = events->beginEvents[i].shapeIdB;
        if (!b3Shape_IsValid(a) || !b3Shape_IsValid(b)) {
            continue;
        }
        b3BodyId ba = b3Shape_GetBody(a), bb = b3Shape_GetBody(b);
        b3BodyId other;
        if (B3_ID_EQUALS(ba, body)) {
            other = bb;
        } else if (B3_ID_EQUALS(bb, body)) {
            other = ba;
        } else {
            continue;
        }
        if (b3Body_GetType(other) != b3_staticBody && !B3_ID_EQUALS(other, ignore)) {
            return true;
        }
    }
    return false;
}

void body_sync_collect(body_sync_t *s, uint32_t tick) {
    (void)tick;
    b3ContactEvents events = b3World_GetContactEvents(s->config.world);
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        entry_t *e = &s->entries[slot];
        if (!e->bound || !local(s, e) || !b3Body_IsValid(e->body)) {
            continue;
        }
        stub_body_state_t *o = &e->out;
        b3Vec3 p = b3Body_GetPosition(e->body);
        b3Quat q = b3Body_GetRotation(e->body);
        b3Vec3 v = b3Body_GetLinearVelocity(e->body);
        b3Vec3 w = b3Body_GetAngularVelocity(e->body);
        o->id = body_sync_id_make(slot, e->generation);
        o->position[0] = p.x;
        o->position[1] = p.y;
        o->position[2] = p.z;
        o->rotation[0] = q.v.x;
        o->rotation[1] = q.v.y;
        o->rotation[2] = q.v.z;
        o->rotation[3] = q.s;
        o->velocity[0] = v.x;
        o->velocity[1] = v.y;
        o->velocity[2] = v.z;
        o->angular[0] = w.x;
        o->angular[1] = w.y;
        o->angular[2] = w.z;
        o->kinematic = b3Body_GetType(e->body) == b3_kinematicBody;
        o->asleep = !b3Body_IsAwake(e->body);
        o->locks = locks_bits(b3Body_GetMotionLocks(e->body));
        o->flags = e->flags;
        o->holder = e->holder;
        o->hold = e->hold;
        o->seat = e->seat;
        o->number = e->number;
        e->has_out = true;
        if (e->hold == BODY_SYNC_HOLD_UNTIL_SETTLED) {
            b3BodyId ignore = b3_nullBodyId;
            const entry_t *h = entry_for(s, e->last_holder);
            if (h) {
                ignore = h->body;
            }
            if (o->asleep || touched_moving(&events, e->body, ignore)) {
                if (is_host(s)) {
                    e->hold = BODY_SYNC_HOLD_UNTIL_RELEASE;
                    o->hold = e->hold;
                } else if (s->to_host.count < BODY_SYNC_RECORDS_QUEUED) {
                    body_sync_release(s, o->id);
                }
            }
        }
    }
}

static bool eligible(const body_sync_t *s, const entry_t *e, uint8_t seat, stub_body_state_t *out) {
    if (!e->bound) {
        return false;
    }
    if (!is_host(s)) {
        if (!local(s, e) || !e->has_out) {
            return false;
        }
        *out = e->out;
        return true;
    }
    if (e->seat == seat) {
        return false;
    }
    if (local(s, e)) {
        if (!e->has_out) {
            return false;
        }
        *out = e->out;
        return true;
    }
    if (!e->has_in) {
        return false;
    }
    *out = e->in;
    out->seat = e->seat;
    out->number = e->number;
    return true;
}

bool body_sync_write_state(body_sync_t *s, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size) {
    *size = 0;
    if (!seat_ok(s, seat) || !s->joined[seat] || (!is_host(s) && seat != BODY_SYNC_SEAT_HOST) || capacity < 64 || !buffer) {
        return false;
    }
    uint32_t fit = stub_wire_bodies_fit(capacity);
    fit = fit > 0xFFFF ? 0xFFFF : fit;
    if (fit == 0) {
        return false;
    }
    if (!s->writing[seat]) {
        s->writing[seat] = true;
        s->cursor[seat] = 0;
    }
    uint32_t n = 0;
    uint32_t slot = s->cursor[seat];
    stub_body_state_t state;
    for (; slot < BODY_SYNC_BODIES_MAX && n < fit; slot++) {
        if (eligible(s, &s->entries[slot], seat, &state) && stub_wire_limit(&s->limits, &state)) {
            stub_wire_put_body(buffer + STUB_HEADER_BYTES + n * STUB_BODY_BYTES, &state);
            n++;
        }
    }
    s->cursor[seat] = slot;
    if (n == 0) {
        s->writing[seat] = false;
        return false;
    }
    stub_wire_put_header(buffer, (uint16_t)n);
    *size = STUB_HEADER_BYTES + n * STUB_BODY_BYTES;
    body_sync_traffic_t *t = &s->traffic[seat];
    t->sent_bytes[BODY_SYNC_TRAFFIC_HEADER] += STUB_HEADER_BYTES;
    t->sent_bytes[BODY_SYNC_TRAFFIC_COPY] += (uint64_t)n * STUB_COPY_BYTES;
    t->sent_bytes[BODY_SYNC_TRAFFIC_SIM] += (uint64_t)n * STUB_SIM_BYTES;
    t->sent_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += (uint64_t)n * STUB_AUTHORITY_BYTES;
    t->sent_bytes[BODY_SYNC_TRAFFIC_HELD] += (uint64_t)n * STUB_HELD_BYTES;
    return true;
}

void body_sync_state_sent(body_sync_t *s, uint8_t seat, uint16_t number) {
    (void)number;
    if (seat < s->config.seats) {
        s->traffic[seat].packets_sent++;
    }
}

uint32_t body_sync_unresolved(const body_sync_t *s, uint8_t seat, uint16_t *numbers, uint32_t max) {
    (void)s;
    (void)seat;
    (void)numbers;
    (void)max;
    return 0;
}

void body_sync_resolve(body_sync_t *s, uint8_t seat, uint16_t number, bool arrived) {
    (void)s;
    (void)seat;
    (void)number;
    (void)arrived;
}

/* The lowest slot owed to `seat`, or BODY_SYNC_BODIES_MAX. */
static uint32_t first_owed(const body_sync_t *s, uint8_t seat) {
    for (uint32_t w = 0; w < OWED_WORDS; w++) {
        uint64_t bits = s->owed[seat][w];
        if (bits) {
            uint32_t bit = 0;
            while (!(bits & 1ull)) {
                bits >>= 1;
                bit++;
            }
            return w * 64 + bit;
        }
    }
    return BODY_SYNC_BODIES_MAX;
}

bool body_sync_peek_record(const body_sync_t *s, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size) {
    *size = 0;
    if (seat >= s->config.seats || !buffer) {
        return false;
    }
    if (!is_host(s)) {
        const queue_t *q = &s->to_host;
        if (seat != BODY_SYNC_SEAT_HOST || q->count == 0 || q->size[q->head] > capacity) {
            return false;
        }
        memcpy(buffer, q->data[q->head], q->size[q->head]);
        *size = q->size[q->head];
        return true;
    }
    uint32_t slot = first_owed(s, seat);
    if (slot >= BODY_SYNC_BODIES_MAX) {
        return false;
    }
    const entry_t *e = &s->entries[slot];
    stub_record_t r = {.type = STUB_REC_ASSIGN,
                       .id = body_sync_id_make(slot, e->generation),
                       .seat = e->seat,
                       .number = e->number,
                       .hold = e->hold,
                       .sequence = s->answer[seat][slot],
                       .holder = e->seat == BODY_SYNC_SEAT_HOST ? e->holder : BODY_SYNC_ID_NONE,
                       .forced = s->forced[seat][slot] ? 1 : 0};
    *size = stub_wire_put_record(&r, buffer, capacity);
    return *size > 0;
}

void body_sync_pop_record(body_sync_t *s, uint8_t seat) {
    if (seat >= s->config.seats) {
        return;
    }
    uint32_t size = 0;
    if (!is_host(s)) {
        queue_t *q = &s->to_host;
        if (seat != BODY_SYNC_SEAT_HOST || q->count == 0) {
            return;
        }
        size = q->size[q->head];
        q->head = (q->head + 1) % BODY_SYNC_RECORDS_QUEUED;
        q->count--;
    } else {
        uint32_t slot = first_owed(s, seat);
        if (slot >= BODY_SYNC_BODIES_MAX) {
            return;
        }
        s->owed[seat][slot / 64] &= ~(1ull << (slot % 64));
        s->answer[seat][slot] = 0;
        s->forced[seat][slot] = false;
        size = 16;
    }
    s->traffic[seat].sent_bytes[BODY_SYNC_TRAFFIC_AUTHORITY] += size;
    s->traffic[seat].records_sent++;
}

bool body_sync_render_offset(const body_sync_t *s, body_sync_id_t id, float position[3], float rotation[4]) {
    if (!entry_const(s, id)) {
        return false;
    }
    position[0] = position[1] = position[2] = 0.0f;
    rotation[0] = rotation[1] = rotation[2] = 0.0f;
    rotation[3] = 1.0f;
    return true;
}

bool body_sync_traffic(const body_sync_t *s, uint8_t seat, body_sync_traffic_t *out) {
    if (seat >= s->config.seats) {
        return false;
    }
    *out = s->traffic[seat];
    return true;
}
