#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sync_authority.h"

static bool newer16(uint16_t a, uint16_t b) { return (int16_t)(uint16_t)(a - b) > 0; }

static body_sync_result_t invalid(sync_authority_t *a, const char *what) {
    if (a->error && a->error_size) {
        snprintf(a->error, a->error_size, "%s", what);
    }
    return BODY_SYNC_INVALID;
}

static uint32_t slot_of(const sync_authority_t *a, const sync_authority_body_t *b) { return (uint32_t)(b - a->bodies); }

void sync_authority_init(sync_authority_t *a, uint8_t self, uint8_t seats, char *error, size_t error_size) {
    memset(a, 0, sizeof(*a));
    a->self = self;
    a->seats = seats;
    a->error = error;
    a->error_size = error_size;
}

bool sync_authority_is_host(const sync_authority_t *a) { return a->self == BODY_SYNC_SEAT_HOST; }

sync_authority_body_t *sync_authority_body(sync_authority_t *a, body_sync_id_t id) {
    uint32_t slot = body_sync_id_slot(id);
    if (slot >= BODY_SYNC_BODIES_MAX || body_sync_id_generation(id) == 0) {
        return NULL;
    }
    sync_authority_body_t *b = &a->bodies[slot];
    return b->bound && b->generation == body_sync_id_generation(id) ? b : NULL;
}

const sync_authority_body_t *sync_authority_body_const(const sync_authority_t *a, body_sync_id_t id) {
    return sync_authority_body((sync_authority_t *)a, id);
}

bool sync_authority_local(const sync_authority_t *a, const sync_authority_body_t *b) {
    return b->seat == a->self || (!sync_authority_is_host(a) && b->claim_pending);
}

void sync_authority_describe(const sync_authority_t *a, const sync_authority_body_t *b, body_sync_authority_t *out) {
    bool mine = sync_authority_local(a, b);
    out->seat = b->seat;
    out->number = b->number;
    out->hold = (body_sync_hold_t)(mine || !b->has_in ? b->hold : b->in_hold);
    out->holder = mine || !b->has_in ? b->holder : b->in_holder;
}

/* Becoming local continues the last owner's body: its state bits and hold now, its motion type
   and velocities at the next apply. The carrier stays only where the header says (a seat that
   left); a claim or a move starts with no holder. */
static void take_local(sync_authority_body_t *b, bool keep_holder) {
    if (b->has_in) {
        b->flags = b->in_flags;
        b->hold = b->in_hold;
        b->data_size = b->in_data_size;
        memcpy(b->data, b->in_data, sizeof(b->data));
    }
    b->data_mine = false;
    b->holder = keep_holder && b->has_in ? b->in_holder : BODY_SYNC_ID_NONE;
    b->holder_mine = false;
    b->flags_mine = 0;
    b->kinematic_mine = false;
    b->locks_mine = false;
    b->take_motion = false;
    b->resume = true;
}

/* A record's flags and motion, for a body that becomes or stays local through it. */
static void take_record(sync_authority_body_t *b, const stub_record_t *r, bool keep_own) {
    bool own_kinematic = keep_own && b->kinematic_mine, own_locks = keep_own && b->locks_mine;
    if (r->motion_known && !(own_kinematic && own_locks)) {
        b->take_motion = true;
        b->take_kinematic = own_kinematic ? b->kinematic : r->kinematic;
        b->take_locks = own_locks ? b->locks : r->locks;
        b->resume = true;
    }
    uint8_t own = keep_own ? b->flags_mine : 0u;
    b->flags = (uint8_t)((r->flags & ~own) | (b->flags & own));
    if (!(keep_own && b->data_mine)) {
        b->data_size = r->data_size;
        memcpy(b->data, r->data, sizeof(b->data));
    }
}

static void record_body(const sync_authority_t *a, const sync_authority_body_t *b, stub_record_t *r) {
    bool mine = sync_authority_local(a, b);
    r->flags = mine || !b->has_in ? b->flags : b->in_flags;
    r->data_size = mine || !b->has_in ? b->data_size : b->in_data_size;
    memcpy(r->data, mine || !b->has_in ? b->data : b->in_data, sizeof(r->data));
    r->motion_known = b->motion_known;
    r->kinematic = b->kinematic;
    r->locks = b->locks;
}

/* Host: the one place authority changes on the host. `granted` is the claim answered, 0 for a
   forced move. `end`: how the current guest owner's ownership ends, for its contact claims. */
static void host_move_to(sync_authority_t *a, sync_authority_body_t *b, uint8_t seat, uint8_t hold, uint16_t granted, bool keep_holder,
                         uint8_t announce, sync_authority_end_t end) {
    bool was_local = sync_authority_local(a, b);
    uint32_t slot = slot_of(a, b);
    b->released = false;
    /* The body leaves a seat whose take grant has not left yet: that answer now names another
       seat, which the seat reads as a refusal and declares two on for its next take (guest
       take_refused), so the host skips the same. */
    if (b->seat != BODY_SYNC_SEAT_HOST && seat != b->seat && b->hold == BODY_SYNC_HOLD_UNTIL_REST && b->granted_seq != 0 &&
        a->answer[b->seat][slot] == b->granted_seq) {
        a->refused[b->seat][slot / 64] |= 1ull << (slot % 64);
    }
    if (b->seat != BODY_SYNC_SEAT_HOST) {
        end = end == SYNC_END_REVOKED && a->plant_revoked_as_arbitrated ? SYNC_END_ARBITRATED : end;
        sync_authority_ended_t *e = &a->ended[b->seat][slot];
        *e = (sync_authority_ended_t){.number = b->number, .how = (uint8_t)end, .tick = a->now};
        if (b->pose_count > 0 && b->pose_seat == b->seat && b->pose_number == b->number) {
            e->has_pose = true;
            e->pose = b->poses[(b->pose_next + SYNC_AUTHORITY_POSES - 1u) % SYNC_AUTHORITY_POSES];
            e->travel = 0.0f;
            for (uint32_t i = 0; i < b->pose_count; i++) {
                const float *d = b->poses[i].step;
                float travel = b->poses[i].span > 0 ? sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) : 0.0f;
                e->travel = travel > e->travel ? travel : e->travel;
            }
        }
    }
    a->owned_by[b->seat] -= b->seat != BODY_SYNC_SEAT_HOST && b->hold == BODY_SYNC_HOLD_UNTIL_REST ? 1u : 0u;
    uint64_t bit = 1ull << (slot % 64);
    bool skip = seat != BODY_SYNC_SEAT_HOST && (a->refused[seat][slot / 64] & bit);
    a->refused[seat][slot / 64] &= ~bit;
    uint8_t from = b->seat;
    b->seat = seat;
    b->number += skip ? 2u : 1u;
    b->granted_seq = granted;
    if (seat == BODY_SYNC_SEAT_HOST && !was_local) {
        take_local(b, keep_holder);
    }
    if (seat != BODY_SYNC_SEAT_HOST) {
        b->holder = BODY_SYNC_ID_NONE;
    }
    /* What the host received of the body was a guest owner's: a new guest owner is read from the
       host's own move (this hold, no holder) until her states come, or another guest's claim of a
       body just granted would read the old flight's hold. From a guest its flags and data stay the
       body's, as the new owner goes on from them; from the host they are the host's own, and what
       it received is an older owner's. */
    if (seat != BODY_SYNC_SEAT_HOST && seat != from && b->has_in && !a->plant_move_keeps_received) {
        if (from != BODY_SYNC_SEAT_HOST) {
            b->flags = b->in_flags;
            b->data_size = b->in_data_size;
            memcpy(b->data, b->in_data, sizeof(b->data));
        }
        b->has_in = false;
    }
    b->hold = hold;
    a->owned_by[seat] += seat != BODY_SYNC_SEAT_HOST && hold == BODY_SYNC_HOLD_UNTIL_REST ? 1u : 0u;
    for (uint8_t to = 1; to < a->seats; to++) {
        if (a->joined[to] && (announce & (1u << to))) {
            a->owed[to][slot / 64] |= 1ull << (slot % 64);
            a->forced[to][slot] = granted == 0;
        }
    }
}

static void host_move(sync_authority_t *a, sync_authority_body_t *b, uint8_t seat, uint8_t hold, uint16_t granted, bool keep_holder,
                      sync_authority_end_t end) {
    host_move_to(a, b, seat, hold, granted, keep_holder, 0xFFu, end);
}

/* Guest: the one place authority changes on a guest, from an announcement or a host state. A
   state never makes this endpoint local: only an announcement or its own claim does, so a
   grant of a claim it has released since cannot hand the body back; a forced move can. */
static void guest_authority(sync_authority_t *a, sync_authority_body_t *b, const stub_record_t *r, bool from_record) {
    uint8_t me = a->self;
    if (!from_record && r->seat == me) {
        return;
    }
    bool was_local = sync_authority_local(a, b);
    bool answers_mine = from_record && r->sequence != 0 && b->claim_pending && r->sequence == b->claim_seq;
    if (answers_mine) {
        b->claim_pending = false;
    }
    /* The host marks every take it refuses of a body this seat does not own, one released while
       it waited too, and a granted take whose answer left after the body moved on (host_move_to). */
    bool answers_take = from_record && r->sequence != 0 && b->claim_take && r->sequence == b->claim_seq;
    b->take_refused = b->take_refused || (answers_take && r->seat != me);
    bool stale_grant = r->seat == me && !answers_mine && r->sequence != 0 && !r->forced;
    if (newer16(r->number, b->number)) {
        b->number = r->number;
        b->number_lag = b->number_lag == SYNC_LAG_ONE ? SYNC_LAG_NONE : b->number_lag;
        b->seat = stale_grant ? BODY_SYNC_SEAT_HOST : r->seat;
        /* Any move to this seat, stale grant or not, is the one the host skipped a number on. */
        b->take_refused = b->take_refused && r->seat != me;
    }
    /* Only a record at the host's number settles an unknown one: a state carries no refusal mark,
       and the take answer that does may still be on its way. After a grant of a claim released
       since, the host still owes the release's move. */
    if (from_record && r->number == b->number) {
        b->number_lag = stale_grant ? SYNC_LAG_UNKNOWN : SYNC_LAG_NONE;
    }
    bool now_local = sync_authority_local(a, b);
    if (!was_local && now_local) {
        take_local(b, false);
        b->hold = r->hold;
    }
    /* The announcement names the holder; a claimant that set its own since keeps it. */
    if (from_record && now_local && (!was_local || answers_mine) && !b->holder_mine) {
        b->holder = r->holder;
    }
    if (from_record && now_local && (!was_local || answers_mine)) {
        take_record(b, r, was_local);
    }
}

static bool queue_push(sync_authority_t *a, const stub_record_t *record);

static void forget_seat(sync_authority_t *a, uint8_t seat) {
    memset(a->owed[seat], 0, sizeof(a->owed[seat]));
    memset(a->answer[seat], 0, sizeof(a->answer[seat]));
    memset(a->forced[seat], 0, sizeof(a->forced[seat]));
    memset(a->refused[seat], 0, sizeof(a->refused[seat]));
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        if (a->pending[seat][slot].active) {
            a->pending[seat][slot].active = false;
            a->pending_count--;
        }
        if (seat == BODY_SYNC_SEAT_HOST) {
            a->bodies[slot].take_refused = false; /* the host forgot this seat's refusals too */
        }
    }
    if (seat == BODY_SYNC_SEAT_HOST) {
        a->queue_head = 0;
        a->queue_count = 0;
    }
}

void sync_authority_seat_joined(sync_authority_t *a, uint8_t seat) {
    if (!a->joined[seat]) {
        memset(a->ended[seat], 0, sizeof(a->ended[seat]));
    }
    forget_seat(a, seat);
    a->joined[seat] = true;
    a->take_budget[seat] = (float)(SYNC_POLICY_TAKES_PER_TICK * SYNC_POLICY_TAKE_BURST_TICKS);
    a->take_budget_tick[seat] = a->now;
}

void sync_authority_seat_left(sync_authority_t *a, uint8_t seat) {
    a->joined[seat] = false;
    for (uint32_t slot = 0; sync_authority_is_host(a) && slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_authority_body_t *b = &a->bodies[slot];
        if (b->bound && b->seat == seat) {
            host_move(a, b, BODY_SYNC_SEAT_HOST, b->has_in ? b->in_hold : BODY_SYNC_HOLD_UNTIL_RELEASE, 0, true, SYNC_END_REVOKED);
        }
    }
    forget_seat(a, seat);
}

body_sync_result_t sync_authority_bind(sync_authority_t *a, body_sync_id_t id, uint8_t seat, uint16_t number) {
    uint32_t slot = body_sync_id_slot(id), generation = body_sync_id_generation(id);
    sync_authority_body_t *b = &a->bodies[slot];
    if (b->bound) {
        return BODY_SYNC_TAKEN;
    }
    uint32_t low = generation & 15u;
    if (sync_authority_is_host(a) && (a->unbound_ever[slot] & (1u << low)) && a->now - a->unbound_tick[slot][low] < a->rebind_ticks) {
        return BODY_SYNC_TAKEN;
    }
    if (b->agent_ring) {
        a->agent_rings[b->agent_ring - 1u].used = false;
    }
    memset(b, 0, sizeof(*b));
    for (uint8_t s = 0; s < BODY_SYNC_SEATS_MAX; s++) {
        a->ended[s][slot] = (sync_authority_ended_t){0};
        a->old_owner_drops[slot][s] = 0;
    }
    a->old_owner_generation[slot] = (uint16_t)generation;
    b->bound = true;
    b->generation = (uint16_t)generation;
    b->seat = seat;
    b->number = number;
    return BODY_SYNC_OK;
}

bool sync_authority_set_agent(sync_authority_t *a, body_sync_id_t id, bool agent) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b || !sync_authority_is_host(a)) {
        return false;
    }
    if (!agent) {
        if (b->agent_ring) {
            a->agent_rings[b->agent_ring - 1u].used = false;
            b->agent_ring = 0;
        }
        return true;
    }
    if (b->agent_ring) {
        return true;
    }
    for (uint32_t i = 0; i < SYNC_AUTHORITY_AGENTS; i++) {
        if (!a->agent_rings[i].used) {
            /* Empty until the body's next state starts its ring over: both hold one stretch. */
            a->agent_rings[i] = (sync_authority_agent_ring_t){.used = true};
            b->agent_ring = (uint8_t)(i + 1u);
            b->pose_count = 0;
            b->pose_next = 0;
            return true;
        }
    }
    a->agents_unringed++;
    return false;
}

body_sync_result_t sync_authority_unbind(sync_authority_t *a, body_sync_id_t id) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_UNKNOWN;
    }
    uint32_t slot = slot_of(a, b);
    if (sync_authority_is_host(a) && b->seat != BODY_SYNC_SEAT_HOST && b->hold == BODY_SYNC_HOLD_UNTIL_REST) {
        a->owned_by[b->seat]--;
    }
    b->bound = false;
    if (b->agent_ring) {
        a->agent_rings[b->agent_ring - 1u].used = false;
        b->agent_ring = 0;
    }
    a->unbound_ever[slot] |= (uint16_t)(1u << (b->generation & 15u));
    a->unbound_tick[slot][b->generation & 15u] = a->now;
    for (uint8_t seat = 0; seat < BODY_SYNC_SEATS_MAX; seat++) {
        a->owed[seat][slot / 64] &= ~(1ull << (slot % 64));
        a->refused[seat][slot / 64] &= ~(1ull << (slot % 64));
        a->answer[seat][slot] = 0;
        a->ended[seat][slot] = (sync_authority_ended_t){0};
        if (a->pending[seat][slot].active) {
            a->pending[seat][slot].active = false;
            a->pending_count--;
        }
    }
    return BODY_SYNC_OK;
}

static bool queue_push(sync_authority_t *a, const stub_record_t *record) {
    if (a->queue_count >= BODY_SYNC_RECORDS_QUEUED) {
        return false;
    }
    uint32_t at = (a->queue_head + a->queue_count) % BODY_SYNC_RECORDS_QUEUED;
    a->queue_size[at] = (uint8_t)stub_wire_put_record(record, a->queue[at], SYNC_AUTHORITY_RECORD_BYTES);
    a->queue_count++;
    return true;
}

body_sync_result_t sync_authority_claim(sync_authority_t *a, body_sync_id_t id, body_sync_hold_t hold) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_UNKNOWN;
    }
    if (hold > BODY_SYNC_HOLD_UNTIL_SETTLED) {
        return invalid(a, "claim: unknown hold");
    }
    if (sync_authority_is_host(a)) {
        if (b->seat != BODY_SYNC_SEAT_HOST) {
            host_move(a, b, BODY_SYNC_SEAT_HOST, (uint8_t)hold, 0, false, SYNC_END_ARBITRATED);
        }
        b->hold = (uint8_t)hold;
        return BODY_SYNC_OK;
    }
    if (!sync_authority_local(a, b)) {
        if (!a->joined[BODY_SYNC_SEAT_HOST]) {
            return invalid(a, "claim: no host");
        }
        uint16_t sequence = (uint16_t)(a->claim_counter + 1u);
        sequence = sequence == 0 ? 1 : sequence;
        stub_record_t record = {.type = STUB_REC_CLAIM, .id = id, .hold = (uint8_t)hold, .sequence = sequence};
        if (!queue_push(a, &record)) {
            return BODY_SYNC_FULL;
        }
        a->claim_counter = sequence;
        b->claim_pending = true;
        b->claim_take = false;
        b->claim_seq = sequence;
        take_local(b, false);
    }
    b->hold = (uint8_t)hold;
    return BODY_SYNC_OK;
}

body_sync_result_t sync_authority_release(sync_authority_t *a, body_sync_id_t id) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_UNKNOWN;
    }
    if (sync_authority_is_host(a)) {
        /* A guest's body is that guest's to give back; its hold here is what owned_by counts. */
        if (!sync_authority_local(a, b)) {
            return invalid(a, "release: the body is not simulated here");
        }
        b->hold = BODY_SYNC_HOLD_UNTIL_RELEASE;
        b->holder = BODY_SYNC_ID_NONE;
        return BODY_SYNC_OK;
    }
    if (!sync_authority_local(a, b) || !a->joined[BODY_SYNC_SEAT_HOST]) {
        return invalid(a, "release: the body is not simulated here, or no host");
    }
    stub_record_t record = {.type = STUB_REC_RELEASE, .id = id, .number = b->number, .sequence = b->claim_seq};
    record_body(a, b, &record);
    if (!queue_push(a, &record)) {
        return BODY_SYNC_FULL;
    }
    b->number_lag = b->seat == a->self ? SYNC_LAG_ONE : SYNC_LAG_UNKNOWN;
    b->seat = BODY_SYNC_SEAT_HOST;
    b->claim_pending = false;
    b->hold = BODY_SYNC_HOLD_UNTIL_RELEASE;
    b->holder = BODY_SYNC_ID_NONE;
    return BODY_SYNC_OK;
}

bool sync_authority_poll(sync_authority_t *a, body_sync_claim_event_t *out) {
    if (!sync_authority_is_host(a) || a->pending_count == 0) {
        return false;
    }
    sync_authority_pending_t *best = NULL;
    uint8_t best_seat = 0;
    uint32_t best_slot = 0;
    for (uint8_t seat = 1; seat < a->seats; seat++) {
        for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
            sync_authority_pending_t *p = &a->pending[seat][slot];
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
    *out = (body_sync_claim_event_t){.id = body_sync_id_make(best_slot, a->bodies[best_slot].generation),
                                     .seat = best_seat,
                                     .sequence = best->sequence,
                                     .tick = best->tick,
                                     .hold = (body_sync_hold_t)best->hold};
    return true;
}

body_sync_result_t sync_authority_answer(sync_authority_t *a, const body_sync_claim_event_t *claim, bool granted, uint8_t grant_as) {
    if (!sync_authority_is_host(a) || !claim) {
        return invalid(a, "answer: host only");
    }
    if (granted && grant_as != SYNC_AUTHORITY_AS_CLAIMED && grant_as != BODY_SYNC_HOLD_UNTIL_RELEASE && grant_as != BODY_SYNC_HOLD_UNTIL_SETTLED) {
        return invalid(a, "answer: a grant under a hold a claim cannot name");
    }
    sync_authority_body_t *b = sync_authority_body(a, claim->id);
    if (!b || claim->seat == BODY_SYNC_SEAT_HOST || claim->seat >= a->seats) {
        return BODY_SYNC_UNKNOWN;
    }
    uint32_t slot = slot_of(a, b);
    sync_authority_pending_t *p = &a->pending[claim->seat][slot];
    if (!p->active || p->sequence != claim->sequence) {
        return BODY_SYNC_UNKNOWN;
    }
    p->active = false;
    a->pending_count--;
    a->answer[claim->seat][slot] = claim->sequence;
    if (granted) {
        /* The claimant keeps simulating under the hold she claimed whatever the host grants it as.
           Granting the owner's own newer claim ends nothing it did not ask for. */
        uint8_t hold = grant_as == SYNC_AUTHORITY_AS_CLAIMED ? p->hold : grant_as;
        host_move(a, b, claim->seat, hold, claim->sequence, false, b->seat == claim->seat ? SYNC_END_VOLUNTARY : SYNC_END_ARBITRATED);
        b->holder = BODY_SYNC_ID_NONE;
    } else {
        a->owed[claim->seat][slot / 64] |= 1ull << (slot % 64);
    }
    return BODY_SYNC_OK;
}

body_sync_result_t sync_authority_assign(sync_authority_t *a, body_sync_id_t id, uint8_t seat, body_sync_hold_t hold) {
    if (!sync_authority_is_host(a)) {
        return invalid(a, "assign: host only");
    }
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_UNKNOWN;
    }
    if (seat >= a->seats || (seat != BODY_SYNC_SEAT_HOST && !a->joined[seat]) || hold > BODY_SYNC_HOLD_UNTIL_SETTLED) {
        return invalid(a, "assign: nobody sits there, or an unknown hold");
    }
    bool moved = seat != b->seat;
    host_move(a, b, seat, (uint8_t)hold, 0, false, SYNC_END_REVOKED);
    /* A forced move takes the body out of its carrier's hands. */
    if (moved) {
        b->holder = BODY_SYNC_ID_NONE;
    }
    return BODY_SYNC_OK;
}

body_sync_result_t sync_authority_set_holder(sync_authority_t *a, body_sync_id_t id, body_sync_id_t holder) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_UNKNOWN;
    }
    if (!sync_authority_local(a, b) ||
        (holder != BODY_SYNC_ID_NONE && (body_sync_id_slot(holder) >= BODY_SYNC_BODIES_MAX || body_sync_id_generation(holder) == 0))) {
        return invalid(a, "set_holder: not local, or the holder is out of range");
    }
    b->holder = holder;
    b->holder_mine = true;
    if (holder != BODY_SYNC_ID_NONE) {
        b->last_holder = holder;
    }
    return BODY_SYNC_OK;
}

body_sync_result_t sync_authority_set_flags(sync_authority_t *a, body_sync_id_t id, uint8_t flags) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_UNKNOWN;
    }
    if (!sync_authority_local(a, b) || (flags >> BODY_SYNC_FLAG_BITS) != 0) {
        return invalid(a, "set_flags: not local, or bits above BODY_SYNC_FLAG_BITS");
    }
    b->flags_mine |= (uint8_t)(b->flags ^ flags);
    b->flags = flags;
    return BODY_SYNC_OK;
}

uint8_t sync_authority_flags(const sync_authority_t *a, body_sync_id_t id) {
    const sync_authority_body_t *b = sync_authority_body_const(a, id);
    if (!b) {
        return 0;
    }
    return sync_authority_local(a, b) || !b->has_in ? b->flags : b->in_flags;
}

body_sync_result_t sync_authority_set_data(sync_authority_t *a, body_sync_id_t id, const void *data, uint32_t size) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_UNKNOWN;
    }
    if (!sync_authority_local(a, b) || size > STUB_DATA_MAX || (size && !data)) {
        return invalid(a, "set_data: not local, or more than BODY_SYNC_DATA_MAX bytes");
    }
    b->data_size = (uint8_t)size;
    memset(b->data, 0, sizeof(b->data));
    if (size) {
        memcpy(b->data, data, size);
    }
    b->data_mine = true;
    return BODY_SYNC_OK;
}

uint32_t sync_authority_data(const sync_authority_t *a, body_sync_id_t id, void *out, uint32_t capacity) {
    const sync_authority_body_t *b = sync_authority_body_const(a, id);
    if (!b || !out) {
        return 0;
    }
    bool mine = sync_authority_local(a, b) || !b->has_in;
    uint32_t size = mine ? b->data_size : b->in_data_size;
    size = size < capacity ? size : capacity;
    memcpy(out, mine ? b->data : b->in_data, size);
    return size;
}

void sync_authority_received_data(sync_authority_body_t *b, uint8_t size, const uint8_t *data) {
    b->in_data_size = size > STUB_DATA_MAX ? (uint8_t)STUB_DATA_MAX : size;
    memcpy(b->in_data, data, sizeof(b->in_data));
}

void sync_authority_received_pose(sync_authority_t *a, sync_authority_body_t *b, uint8_t seat, uint32_t tick, uint32_t jitter_ticks,
                                  float max_step, const float position[3]) {
    uint32_t now = a->now;
    if (seat < BODY_SYNC_SEATS_MAX) {
        a->heard[seat] = now;
    }
    sync_authority_agent_ring_t *older = b->agent_ring ? &a->agent_rings[b->agent_ring - 1u] : NULL;
    if (b->pose_count == 0 || b->pose_seat != seat || b->pose_number != b->number) {
        b->pose_seat = seat;
        b->pose_number = b->number;
        b->pose_count = 0;
        b->pose_next = 0;
    }
    uint32_t span = 0, prev_tick = tick;
    float step[3] = {0.0f, 0.0f, 0.0f};
    if (b->pose_count > 0) {
        const sync_authority_pose_t *q = &b->poses[(b->pose_next + SYNC_AUTHORITY_POSES - 1u) % SYNC_AUTHORITY_POSES];
        prev_tick = q->tick;
        uint32_t sent = tick - q->tick, seen = now - q->host_tick;
        uint32_t floor = seen > jitter_ticks ? seen - jitter_ticks : 0u;
        uint32_t ceiling = seen + jitter_ticks >= seen ? seen + jitter_ticks : UINT32_MAX;
        uint32_t ticks = sent < floor ? floor : (sent > ceiling ? ceiling : sent);
        ticks = ticks > 0 ? ticks : 1u;
        for (int i = 0; i < 3; i++) {
            step[i] = (position[i] - q->position[i]) / (float)ticks;
        }
        float travel = sqrtf(step[0] * step[0] + step[1] * step[1] + step[2] * step[2]);
        if (travel <= max_step) { /* NaN fails too: a broken pose is no step */
            span = ticks;
        } else {
            memset(step, 0, sizeof(step));
            /* A teleport: the states before it are no path to where the body is now, so it
               starts the ring over as a point with no step. */
            if (!a->plant_teleport_keeps_ring) {
                b->pose_count = 0;
                b->pose_next = 0;
            }
        }
    }
    sync_authority_pose_t *p = &b->poses[b->pose_next];
    memcpy(p->step, step, sizeof(step));
    p->span = span;
    p->prev_tick = prev_tick;
    memcpy(p->position, position, sizeof(p->position));
    p->tick = tick;
    p->host_tick = now;
    b->pose_next = (uint8_t)((b->pose_next + 1u) % SYNC_AUTHORITY_POSES);
    b->pose_count += b->pose_count < SYNC_AUTHORITY_POSES ? 1u : 0u;
    if (older) {
        /* The older ring starts over with the ring: what came before is another ownership's, or
           no path to where a teleport put the body. */
        if (b->pose_count == 1u) {
            older->count = 0;
            older->next = 0;
        }
        const sync_authority_pose_t *last =
            older->count ? &older->poses[(older->next + SYNC_AUTHORITY_AGENT_POSES - 1u) % SYNC_AUTHORITY_AGENT_POSES] : NULL;
        uint32_t stride = a->agent_stride ? a->agent_stride : 1u;
        if (!last || tick - last->tick >= stride) {
            older->poses[older->next] = *p;
            older->next = (uint8_t)((older->next + 1u) % SYNC_AUTHORITY_AGENT_POSES);
            older->count += older->count < SYNC_AUTHORITY_AGENT_POSES ? 1u : 0u;
        }
    }
}

void sync_authority_received(sync_authority_body_t *b, uint8_t hold, body_sync_id_t holder, uint8_t flags) {
    b->has_in = true;
    b->in_hold = hold;
    b->in_holder = holder;
    b->in_flags = flags;
}

void sync_authority_from_state(sync_authority_t *a, sync_authority_body_t *b, uint8_t seat, uint16_t number, uint8_t hold) {
    stub_record_t seen = {.type = STUB_REC_ASSIGN, .seat = seat, .number = number, .hold = hold};
    guest_authority(a, b, &seen, false);
}

/* Host: a guest's take, answered here by the policy. Only the claimant hears the grant at once:
   the other guests read the new owner from the state the host relays, so a pile's takes do not
   cost an announcement per guest; one with its own claim pending hears its refusal. */
static void touch_claim(sync_authority_t *a, uint8_t seat, sync_authority_body_t *b, uint16_t sequence, uint32_t tick) {
    uint32_t slot = slot_of(a, b);
    uint32_t elapsed = a->now - a->take_budget_tick[seat];
    elapsed = elapsed < SYNC_POLICY_TAKE_BURST_TICKS ? elapsed : SYNC_POLICY_TAKE_BURST_TICKS;
    float budget = a->take_budget[seat] + (float)(SYNC_POLICY_TAKES_PER_TICK * elapsed), cap = (float)(SYNC_POLICY_TAKES_PER_TICK * SYNC_POLICY_TAKE_BURST_TICKS);
    a->take_budget[seat] = budget < cap ? budget : cap;
    a->take_budget_tick[seat] = a->now;
    sync_authority_pending_t *p = &a->pending[seat][slot];
    if (p->active) {
        p->active = false; /* only the claimant's newest claim of a body counts */
        a->pending_count--;
    }
    sync_policy_grant_in_t in = {.mode = a->mode,
                                 .light = b->takable,
                                 .claimant = seat,
                                 .seat = b->seat,
                                 .hold = b->hold,
                                 .held = b->holder != BODY_SYNC_ID_NONE,
                                 .kinematic = b->motion_known && b->kinematic,
                                 .owned = sync_authority_owned(a, seat),
                                 .budget = a->take_budget[seat]};
    /* The evidence is the one input that scans every body: it is asked only of a take that every
       other rule grants, and a duplicate needs none. */
    bool granted = sync_policy_grants(&in);
    if (granted && b->seat != seat && a->take_gap) {
        in.agent_gap = a->take_gap(a->take_context, seat, slot, tick, &in.agent_reach);
        in.agent_reach = a->plant_take_reach_unbounded ? INFINITY : in.agent_reach;
        granted = sync_policy_grants(&in);
    }
    a->take_budget[seat] -= a->take_budget[seat] >= 1.0f ? 1.0f : 0.0f;
    a->answer[seat][slot] = sequence;
    if (granted && b->seat == seat) {
        /* A duplicate of a take already granted: answered, and nothing moves. */
        a->owed[seat][slot / 64] |= 1ull << (slot % 64);
    } else if (granted) {
        a->granted[seat]++;
        host_move_to(a, b, seat, BODY_SYNC_HOLD_UNTIL_REST, sequence, false, (uint8_t)(1u << seat), SYNC_END_ARBITRATED);
        b->holder = BODY_SYNC_ID_NONE;
    } else {
        a->refused_takes[seat]++;
        /* The number stays (sync_authority_declared_number): the next move to this seat skips the
           one the refused take's states may carry. A seat that owns the body declares no take. */
        a->refused[seat][slot / 64] |= b->seat != seat ? 1ull << (slot % 64) : 0ull;
        a->owed[seat][slot / 64] |= 1ull << (slot % 64);
    }
}

body_sync_result_t sync_authority_receive(sync_authority_t *a, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size, uint32_t *stale,
                                          char *why, size_t why_size) {
    stub_record_t r;
    if (!stub_wire_read_record(data, size, a->seats, !sync_authority_is_host(a), &r, why, why_size)) {
        return BODY_SYNC_FORMAT;
    }
    sync_authority_body_t *b = sync_authority_body(a, r.id);
    if (!b) {
        (*stale)++;
        sync_authority_old_owner_drop(a, seat, body_sync_id_slot(r.id), body_sync_id_generation(r.id), 16u);
        return BODY_SYNC_OK;
    }
    uint32_t slot = slot_of(a, b);
    sync_authority_pending_t *p = sync_authority_is_host(a) ? &a->pending[seat][slot] : NULL;
    switch (r.type) {
    case STUB_REC_CLAIM:
        if (r.hold > BODY_SYNC_HOLD_UNTIL_REST) {
            snprintf(why, why_size, "claim with an unknown hold");
            return BODY_SYNC_FORMAT;
        }
        if (r.hold == BODY_SYNC_HOLD_UNTIL_REST) {
            touch_claim(a, seat, b, r.sequence, tick);
            break;
        }
        if (!p->active) {
            a->pending_count++;
        }
        *p = (sync_authority_pending_t){.active = true, .sequence = r.sequence, .hold = r.hold, .tick = tick, .order = a->order++};
        break;
    case STUB_REC_RELEASE: {
        /* Whatever else it does, a release withdraws its seat's claim of the body. Left
           pending, the claim would be granted to a guest that reads the grant as stale, and
           nobody would simulate the body. */
        const bool withdrew = p->active;
        if (withdrew) {
            p->active = false;
            a->pending_count--;
        }
        /* A release counts at the number its sender saw, or for the grant it crossed; an older
           one (the host moved the body by force since) is stale and changes nothing. */
        if (b->seat == seat && (r.number == b->number || (r.sequence != 0 && r.sequence == b->granted_seq))) {
            host_move(a, b, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_RELEASE, 0, false, SYNC_END_VOLUNTARY);
            take_record(b, &r, false);
            b->released = true;
            b->released_tick = tick;
        } else if (!withdrew) {
            (*stale)++;
            sync_authority_old_owner_drop(a, seat, slot, b->generation, 16u);
        }
        break;
    }
    case STUB_REC_ASSIGN:
        guest_authority(a, b, &r, true);
        break;
    }
    return BODY_SYNC_OK;
}

void sync_authority_received_motion(sync_authority_body_t *b, bool known, bool kinematic, uint8_t locks) {
    if (known) {
        b->motion_known = true;
        b->kinematic = kinematic;
        b->locks = locks;
    }
}

void sync_authority_note_motion(sync_authority_t *a, sync_authority_body_t *b, bool kinematic, uint8_t locks) {
    if (!sync_authority_local(a, b)) {
        return;
    }
    if (b->applied_valid) {
        b->kinematic_mine |= b->applied_kinematic != kinematic;
        b->locks_mine |= b->applied_locks != locks;
    }
    b->motion_known = true;
    b->kinematic = kinematic;
    b->locks = locks;
}

void sync_authority_resume_motion(const sync_authority_body_t *b, bool received_known, bool received_kinematic, uint8_t received_locks,
                                  bool bound_kinematic, uint8_t bound_locks, bool *kinematic, uint8_t *locks) {
    if (b->take_motion) {
        *kinematic = b->take_kinematic;
        *locks = b->take_locks;
    } else if (received_known) {
        *kinematic = received_kinematic;
        *locks = received_locks;
    } else {
        *kinematic = bound_kinematic;
        *locks = bound_locks;
    }
}

void sync_authority_applied(sync_authority_body_t *b, bool kinematic, uint8_t locks) {
    b->take_motion = false;
    b->applied_valid = true;
    b->applied_kinematic = kinematic;
    b->applied_locks = locks;
    b->kinematic_mine = false;
    b->locks_mine = false;
    b->motion_known = true;
    b->kinematic = kinematic;
    b->locks = locks;
}

uint8_t sync_authority_route(const sync_authority_t *a, uint32_t slot, bool have_state) {
    if (slot >= BODY_SYNC_BODIES_MAX || !a->bodies[slot].bound) {
        return 0;
    }
    const sync_authority_body_t *b = &a->bodies[slot];
    uint8_t mask = 0;
    if (sync_authority_local(a, b)) {
        if (!sync_authority_is_host(a)) {
            return a->joined[BODY_SYNC_SEAT_HOST] ? (uint8_t)(1u << BODY_SYNC_SEAT_HOST) : 0u;
        }
        for (uint8_t seat = 1; seat < a->seats; seat++) {
            mask |= a->joined[seat] ? (uint8_t)(1u << seat) : 0u;
        }
        return mask;
    }
    if (!sync_authority_is_host(a) || !have_state) {
        return 0;
    }
    for (uint8_t seat = 1; seat < a->seats; seat++) {
        mask |= a->joined[seat] && seat != b->seat ? (uint8_t)(1u << seat) : 0u;
    }
    return mask;
}

bool sync_authority_sends_sim(const sync_authority_t *a) { return !sync_authority_is_host(a); }

bool sync_authority_holds_dynamic(const sync_authority_t *a, const sync_authority_body_t *b, bool owner_kinematic) {
    return sync_authority_is_host(a) && !sync_authority_local(a, b) && b->seat != BODY_SYNC_SEAT_HOST && !owner_kinematic;
}

sync_hold_kind_t sync_authority_hold_kind(const sync_authority_t *a, const sync_authority_body_t *b, bool owner_kinematic, bool light) {
    if (sync_authority_holds_dynamic(a, b, owner_kinematic)) {
        return SYNC_HOLD_CORRECTED;
    }
    /* A copy's holder is its owner's, as last received. */
    body_sync_id_t holder = b->has_in ? b->in_holder : b->holder;
    if (light && !sync_authority_is_host(a) && !sync_authority_local(a, b) && holder == BODY_SYNC_ID_NONE && !owner_kinematic) {
        return SYNC_HOLD_SOFT;
    }
    return SYNC_HOLD_DRIVEN;
}

bool sync_authority_settle(sync_authority_t *a, body_sync_id_t id, bool asleep, bool touched_moving, bool touched_untakable) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b || !sync_authority_local(a, b) || b->hold != BODY_SYNC_HOLD_UNTIL_SETTLED) {
        return false;
    }
    sync_policy_flight_in_t in = {.mode = a->mode, .asleep = asleep, .touched_body = touched_moving, .touched_untakable = touched_untakable,
                                 .until_rest = a->flight_until_rest};
    switch (sync_policy_flight(&in)) {
    case SYNC_FLIGHT_ON:
        return false;
    case SYNC_FLIGHT_INTERACTS:
        b->hold = BODY_SYNC_HOLD_UNTIL_REST;
        return false;
    case SYNC_FLIGHT_RETURNS:
        break;
    }
    sync_authority_settled(a, id);
    return true;
}

uint32_t sync_authority_owned(const sync_authority_t *a, uint8_t seat) {
    if (sync_authority_is_host(a)) {
        return seat < BODY_SYNC_SEATS_MAX ? a->owned_by[seat] : 0u;
    }
    uint32_t n = 0;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        const sync_authority_body_t *b = &a->bodies[slot];
        /* A guest's own count includes its takes still waiting: over a round trip it would send
           takes far past the bound, and the host would refuse every one beyond it. */
        bool its = b->seat == seat || (seat == a->self && sync_authority_local(a, b));
        n += b->bound && its && b->hold == BODY_SYNC_HOLD_UNTIL_REST ? 1u : 0u;
    }
    return n;
}

body_sync_result_t sync_authority_take(sync_authority_t *a, body_sync_id_t id) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_UNKNOWN;
    }
    if (sync_authority_is_host(a) || sync_authority_local(a, b) || !a->joined[BODY_SYNC_SEAT_HOST]) {
        return invalid(a, "take: a guest's take of a copy, with a host");
    }
    uint16_t sequence = (uint16_t)(a->claim_counter + 1u);
    sequence = sequence == 0 ? 1 : sequence;
    stub_record_t record = {.type = STUB_REC_CLAIM, .id = id, .hold = BODY_SYNC_HOLD_UNTIL_REST, .sequence = sequence};
    if (!queue_push(a, &record)) {
        return BODY_SYNC_FULL;
    }
    a->claim_counter = sequence;
    a->takes_sent++;
    b->claim_pending = true;
    b->claim_take = true;
    b->claim_seq = sequence;
    take_local(b, false);
    b->hold = BODY_SYNC_HOLD_UNTIL_REST;
    return BODY_SYNC_OK;
}

uint16_t sync_authority_declared_number(const sync_authority_t *a, const sync_authority_body_t *b) {
    bool taking = !sync_authority_is_host(a) && b->claim_pending && b->hold == BODY_SYNC_HOLD_UNTIL_REST && b->seat != a->self;
    uint16_t base = (uint16_t)(b->number + (b->number_lag == SYNC_LAG_ONE ? 1u : 0u));
    return taking ? (uint16_t)(base + (b->take_refused ? 2u : 1u)) : b->number;
}

static body_sync_contact_verdict_t ownership_verdict(const sync_authority_t *a, uint8_t seat, const sync_authority_body_t *b, uint16_t number,
                                                     uint32_t grace_ticks, const sync_authority_ended_t **ended) {
    *ended = NULL;
    if (!sync_authority_is_host(a) || seat == BODY_SYNC_SEAT_HOST || seat >= a->seats) {
        return BODY_SYNC_CONTACT_STALE;
    }
    if (b->seat == seat && b->number == number && a->joined[seat]) {
        return BODY_SYNC_CONTACT_OK;
    }
    /* Seat and number both match: a number another seat held, or one a refused take's states
       declared (the host's next move to that seat skips it), was never this seat's. */
    const sync_authority_ended_t *e = &a->ended[seat][slot_of(a, b)];
    if (e->how == SYNC_END_NONE || e->number != number) {
        return BODY_SYNC_CONTACT_STALE;
    }
    /* A claim of an ended ownership is one the guest sent before it heard of the end: within a
       round trip and a delay of it. Later, the window would credit a body somebody else flies. */
    if (e->how != SYNC_END_REVOKED && (!a->joined[seat] || a->now - e->tick > grace_ticks)) {
        return BODY_SYNC_CONTACT_STALE;
    }
    *ended = e;
    return e->how == SYNC_END_REVOKED && !a->plant_recall_keeps_number ? BODY_SYNC_CONTACT_REVOKED : BODY_SYNC_CONTACT_OK;
}

body_sync_contact_verdict_t sync_authority_check_contact(const sync_authority_t *a, uint8_t seat, body_sync_id_t agent, uint16_t number,
                                                         uint32_t grace_ticks, const sync_authority_ended_t **ended) {
    *ended = NULL;
    const sync_authority_body_t *b = sync_authority_body_const(a, agent);
    return b ? ownership_verdict(a, seat, b, number, grace_ticks, ended) : BODY_SYNC_CONTACT_STALE;
}

static float length3(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

/* The point's distance to the box `lo`..`hi`: the target's box grown by the agent's, so a point
   of the agent's path stands for its whole box. */
static float point_gap(const float lo[3], const float hi[3], const float p[3]) {
    float d2 = 0.0f;
    for (int i = 0; i < 3; i++) {
        float d = p[i] < lo[i] ? lo[i] - p[i] : p[i] > hi[i] ? p[i] - hi[i] : 0.0f;
        d2 += d * d;
    }
    return isfinite(d2) ? sqrtf(d2) : INFINITY;
}

/* The nearest the segment from `from` to `from + along` comes to the box. The distance to a convex
   set is convex along a line, so a ternary search finds it. */
static float segment_gap(const float lo[3], const float hi[3], const float from[3], const float along[3]) {
    float t0 = 0.0f, t1 = 1.0f;
    for (int k = 0; k < 40; k++) {
        float m0 = t0 + (t1 - t0) / 3.0f, m1 = t1 - (t1 - t0) / 3.0f, p0[3], p1[3];
        for (int i = 0; i < 3; i++) {
            p0[i] = from[i] + along[i] * m0;
            p1[i] = from[i] + along[i] * m1;
        }
        if (point_gap(lo, hi, p0) <= point_gap(lo, hi, p1)) {
            t1 = m1;
        } else {
            t0 = m0;
        }
    }
    float p[3];
    for (int i = 0; i < 3; i++) {
        p[i] = from[i] + along[i] * 0.5f * (t0 + t1);
    }
    return point_gap(lo, hi, p);
}

typedef struct contact_fit {
    float lo[3], hi[3];
    const sync_contact_scene_t *in;
    float margin, gap, reach;
    float since; /* ticks the host's target may have moved since the contact, its speed now its pace */
    bool rewind; /* the target goes back along its velocity over them (sync_contact_scene_t) */
} contact_fit_t;

/* One path the contact may lie on, `ticks` long: the fit keeps the one whose gap comes nearest
   within its reach. */
static void fit_path(contact_fit_t *f, const float from[3], const float along[3], float ticks) {
    float cap = SYNC_POLICY_EVIDENCE_AGE_MAX_S * (float)f->in->tick_hz;
    float s = (ticks < cap ? ticks : cap) / (float)f->in->tick_hz;
    float moved = ticks > f->in->target_ticks ? ticks : f->in->target_ticks;
    moved = f->since > moved ? f->since : moved;
    float t = (moved < cap ? moved : cap) / (float)f->in->tick_hz;
    /* Rewound, the target is off where its path had it only by a bend of that path, sized as the
       agent's; never more than the travel credit without the rewind, so a resting body gets none. */
    float lo[3], hi[3], travel = f->in->target_speed * t, bent = 0.5f * SYNC_POLICY_CONTACT_BEND_MS2 * t * t;
    for (int i = 0; i < 3; i++) {
        float back = f->rewind ? f->in->target_velocity[i] * t : 0.0f;
        lo[i] = f->lo[i] - back;
        hi[i] = f->hi[i] - back;
    }
    float gap = segment_gap(lo, hi, from, along);
    float reach = SYNC_POLICY_TAKE_SLOP_M + (f->rewind && bent < travel ? bent : travel) + 0.5f * SYNC_POLICY_CONTACT_BEND_MS2 * s * s;
    if (gap - reach < f->margin) {
        f->margin = gap - reach;
        f->gap = gap;
        f->reach = reach;
    }
}

/* The step from `at` along `step` for `ticks`. */
static void fit_ray(contact_fit_t *f, const float at[3], const float step[3], float ticks) {
    const float along[3] = {step[0] * ticks, step[1] * ticks, step[2] * ticks};
    fit_path(f, at, along, ticks);
}

/* A step the host may extend along: it saw the state before within a chord's spacing. */
static bool short_step(const sync_authority_pose_t *p) { return p->span > 0 && p->span <= SYNC_POLICY_CHORD_TICKS_MAX; }

bool sync_authority_contact_measure(const sync_authority_t *a, body_sync_id_t agent, uint8_t seat, const sync_authority_ended_t *ended,
                                    const sync_contact_scene_t *in, float *gap, float *reach, float *approach_max) {
    *gap = INFINITY;
    *reach = SYNC_POLICY_TAKE_SLOP_M;
    *approach_max = in->target_speed;
    const sync_authority_body_t *b = sync_authority_body_const(a, agent);
    return b && sync_authority_path_measure(a, b, seat, ended, in, gap, reach, approach_max);
}

bool sync_authority_path_measure(const sync_authority_t *a, const sync_authority_body_t *b, uint8_t seat, const sync_authority_ended_t *ended,
                                 const sync_contact_scene_t *in, float *gap, float *reach, float *approach_max) {
    *gap = INFINITY;
    *reach = SYNC_POLICY_TAKE_SLOP_M;
    *approach_max = in->target_speed;
    if (in->tick_hz == 0 || seat >= BODY_SYNC_SEATS_MAX) {
        return false;
    }
    contact_fit_t f = {.in = in, .margin = INFINITY, .gap = INFINITY, .reach = SYNC_POLICY_TAKE_SLOP_M,
                       .rewind = in->rewind && !a->plant_target_rewind_off};
    for (int i = 0; i < 3; i++) {
        f.lo[i] = in->target_lo[i] - in->agent_hi[i];
        f.hi[i] = in->target_hi[i] - in->agent_lo[i];
    }
    const bool grows = a->plant_evidence_grows_over_gaps;
    const uint32_t window = in->window_ticks;
    /* The ring still holds that ownership unless another one's states replaced it; then only the
       end's last state is left. */
    const sync_authority_pose_t *poses[SYNC_AUTHORITY_AGENT_POSES + SYNC_AUTHORITY_POSES];
    uint32_t count = 0;
    bool from_end = false;
    uint16_t number = ended ? ended->number : b->number;
    if (b->pose_count > 0 && b->pose_seat == seat && b->pose_number == number && (ended || b->seat == seat)) {
        uint32_t first = (b->pose_next + SYNC_AUTHORITY_POSES - b->pose_count) % SYNC_AUTHORITY_POSES;
        /* An agent's older states the ring no longer holds come first, oldest first; they started
           over with the ring, so they are of this ownership. */
        const sync_authority_agent_ring_t *older = b->agent_ring && !a->plant_agent_ring_off ? &a->agent_rings[b->agent_ring - 1u] : NULL;
        uint32_t oldest = b->poses[first].tick;
        for (uint32_t i = 0; older && i < older->count; i++) {
            const sync_authority_pose_t *o =
                &older->poses[(older->next + SYNC_AUTHORITY_AGENT_POSES - older->count + i) % SYNC_AUTHORITY_AGENT_POSES];
            if (oldest - o->tick - 1u < 0x7FFFFFFFu) {
                poses[count++] = o;
            }
        }
        for (uint32_t i = 0; i < b->pose_count; i++) {
            poses[count++] = &b->poses[(first + i) % SYNC_AUTHORITY_POSES];
        }
    } else if (ended && ended->has_pose) {
        poses[count++] = &ended->pose;
        from_end = true;
    } else {
        return false;
    }
    /* A seat gone silent leaves no evidence: its last state would stand for as long as it owns. */
    if (!grows && a->now - a->heard[seat] > window) {
        return false;
    }
    /* `travel` is the fastest step of the ring, what the planted defects read; `pushing` only the
       window's, since it widens a take's reach and a resting agent pushes nothing. `approach` is
       the steps of the states this contact lies on from: the agent's speed there, not anywhere
       else the ring has been. */
    float travel = 0.0f, pushing = 0.0f, approach = 0.0f;
    bool any = false;
    const bool aged = a->plant_evidence_age_unbounded;
    const float lag = (float)SYNC_POLICY_CLAIM_LAG_GRACES * (float)window, age_cap = SYNC_POLICY_EVIDENCE_AGE_MAX_S * (float)in->tick_hz;
    const uint32_t lag_max = (uint32_t)(lag < age_cap ? lag : age_cap);
    const float none[3] = {0.0f, 0.0f, 0.0f};
    for (uint32_t i = 0; i < count; i++) {
        const sync_authority_pose_t *p = poses[i], *next = i + 1u < count ? poses[i + 1u] : NULL;
        if (p->span > 0) {
            float step = length3(p->step);
            travel = step > travel ? step : travel;
            pushing = a->now - p->host_tick <= window && step > pushing ? step : pushing;
        }
        /* Ticks from this state to the contact on the claimant's clock: the contact lies on from
           it for them, along the step that led to it when the host saw one, with the bend. A
           contact before it, or further on than a chord's spacing, is none of its evidence. */
        uint32_t after = in->contact_tick - p->tick;
        /* A take's agent taken after the touch is where the host shows it while the take is
           judged: a point, nothing grown from it. */
        if (in->later_points && after > 0x7FFFFFFFu) {
            after = 0;
        } else if (after > (grows ? 0x7FFFFFFFu : SYNC_POLICY_CHORD_TICKS_MAX)) {
            continue;
        }
        /* The contact's tick is the claimant's word; the host's own clock says how long ago it
           took the state. An honest claim follows its states within the lag, so an older state
           is no evidence however the claim names its tick. */
        if (!grows && !aged && a->now - p->host_tick > lag_max) {
            continue;
        }
        any = true;
        if (p->span > 0) {
            float step = length3(p->step);
            approach = step > approach ? step : approach;
        }
        float ticks = (float)after;
        /* The claim reads the target's box now: it went on from the contact for the ticks since
           this state reached the host and the contact's on from it, at most the window. */
        uint32_t since = a->now - p->host_tick + after;
        f.since = (float)(since < window ? since : window);
        fit_ray(&f, p->position, grows || short_step(p) ? p->step : none, ticks);
        /* The contact before the next state, the two a chord's spacing apart: on their chord too. */
        if (next && next->span > 0 && (grows || (next->span <= SYNC_POLICY_CHORD_TICKS_MAX && next->prev_tick == p->tick)) &&
            next->tick - in->contact_tick <= 0x7FFFFFFFu) {
            const float chord[3] = {next->position[0] - p->position[0], next->position[1] - p->position[1], next->position[2] - p->position[2]};
            fit_path(&f, p->position, chord, grows ? (float)(next->tick - p->tick) : (float)next->span);
            float step = length3(next->step);
            approach = step > approach ? step : approach;
        }
    }
    if (!any) {
        return false;
    }
    if (from_end) {
        travel = ended->travel;
        pushing = a->now - ended->pose.host_tick <= window ? ended->travel : 0.0f;
    }
    float cap = SYNC_POLICY_EVIDENCE_AGE_MAX_S * (float)in->tick_hz, push = in->push_ticks < cap ? in->push_ticks : cap;
    *gap = f.gap;
    *reach = f.reach + (grows ? travel : pushing) * push;
    *approach_max = (grows || a->plant_approach_from_ring ? travel : approach) * (float)in->tick_hz + in->target_speed;
    return true;
}

const sync_authority_pose_t *sync_authority_pose_at(const sync_authority_t *a, const sync_authority_body_t *b, uint8_t seat, uint32_t tick,
                                                    uint32_t grace_ticks) {
    const sync_authority_ended_t *ended;
    if (b->pose_count == 0 || b->pose_seat != seat ||
        (!a->plant_target_any_ownership && ownership_verdict(a, seat, b, b->pose_number, grace_ticks, &ended) != BODY_SYNC_CONTACT_OK)) {
        return NULL;
    }
    for (uint32_t i = 0; i < b->pose_count; i++) {
        const sync_authority_pose_t *p = &b->poses[(b->pose_next + SYNC_AUTHORITY_POSES - 1u - i) % SYNC_AUTHORITY_POSES];
        if (tick - p->tick <= SYNC_POLICY_CHORD_TICKS_MAX) {
            return p;
        }
    }
    return NULL;
}

bool sync_authority_contact_number(const sync_authority_t *a, const sync_authority_body_t *b, uint16_t *number) {
    if (!sync_authority_local(a, b)) {
        return false;
    }
    uint16_t declared = sync_authority_declared_number(a, b);
    if (!sync_authority_is_host(a) && b->claim_pending && (declared == b->number || b->number_lag == SYNC_LAG_UNKNOWN)) {
        return false;
    }
    *number = declared;
    return true;
}

void sync_authority_host_active(sync_authority_t *a, body_sync_id_t id, bool active) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (b && sync_authority_is_host(a) && b->seat == BODY_SYNC_SEAT_HOST) {
        b->hold = active ? BODY_SYNC_HOLD_UNTIL_REST : BODY_SYNC_HOLD_UNTIL_RELEASE;
    }
}

body_sync_result_t sync_authority_reclaim(sync_authority_t *a, body_sync_id_t id) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_UNKNOWN;
    }
    if (!sync_authority_is_host(a) || b->seat == BODY_SYNC_SEAT_HOST) {
        return invalid(a, "reclaim: host only, of a guest's body");
    }
    a->reclaimed[b->seat]++;
    host_move(a, b, BODY_SYNC_SEAT_HOST, BODY_SYNC_HOLD_UNTIL_REST, 0, false, SYNC_END_ARBITRATED);
    return BODY_SYNC_OK;
}

void sync_authority_old_owner_drop(sync_authority_t *a, uint8_t seat, uint32_t slot, uint32_t generation, uint8_t generation_bits) {
    if (slot >= BODY_SYNC_BODIES_MAX || seat >= BODY_SYNC_SEATS_MAX || a->old_owner_generation[slot] == 0) {
        return;
    }
    uint32_t mask = generation_bits >= 16u ? 0xFFFFu : (1u << generation_bits) - 1u;
    if ((a->old_owner_generation[slot] & mask) == (generation & mask)) {
        a->old_owner_drops[slot][seat]++;
    }
}

uint32_t sync_authority_old_owner_drops(const sync_authority_t *a, body_sync_id_t id, uint8_t seat) {
    uint32_t slot = body_sync_id_slot(id);
    if (slot >= BODY_SYNC_BODIES_MAX || seat >= BODY_SYNC_SEATS_MAX || body_sync_id_generation(id) == 0 ||
        a->old_owner_generation[slot] != body_sync_id_generation(id)) {
        return 0;
    }
    return a->old_owner_drops[slot][seat];
}

body_sync_id_t sync_authority_holder_id(const sync_authority_t *a, uint32_t slot, uint8_t generation_low, uint8_t generation_bits) {
    if (slot >= BODY_SYNC_BODIES_MAX) {
        return BODY_SYNC_ID_NONE;
    }
    const sync_authority_body_t *h = &a->bodies[slot];
    uint32_t mask = (1u << generation_bits) - 1u;
    if (!h->bound || (h->generation & mask) != (generation_low & mask)) {
        return BODY_SYNC_ID_NONE;
    }
    return body_sync_id_make(slot, h->generation);
}

uint8_t sync_authority_settled(sync_authority_t *a, body_sync_id_t id) {
    sync_authority_body_t *b = sync_authority_body(a, id);
    if (!b) {
        return BODY_SYNC_HOLD_UNTIL_RELEASE;
    }
    if (sync_authority_is_host(a)) {
        a->owned_by[b->seat] -= b->seat != BODY_SYNC_SEAT_HOST && b->hold == BODY_SYNC_HOLD_UNTIL_REST ? 1u : 0u;
        b->hold = BODY_SYNC_HOLD_UNTIL_RELEASE;
    } else if (a->queue_count < BODY_SYNC_RECORDS_QUEUED) {
        sync_authority_release(a, id);
    }
    return b->hold;
}

/* The lowest slot owed to `seat`, or BODY_SYNC_BODIES_MAX. */
static uint32_t first_owed(const sync_authority_t *a, uint8_t seat) {
    for (uint32_t w = 0; w < SYNC_AUTHORITY_OWED_WORDS; w++) {
        uint64_t bits = a->owed[seat][w];
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

bool sync_authority_peek(const sync_authority_t *a, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size) {
    *size = 0;
    if (!sync_authority_is_host(a)) {
        if (seat != BODY_SYNC_SEAT_HOST || a->queue_count == 0 || a->queue_size[a->queue_head] > capacity) {
            return false;
        }
        memcpy(buffer, a->queue[a->queue_head], a->queue_size[a->queue_head]);
        *size = a->queue_size[a->queue_head];
        return true;
    }
    uint32_t slot = first_owed(a, seat);
    if (slot >= BODY_SYNC_BODIES_MAX) {
        return false;
    }
    const sync_authority_body_t *b = &a->bodies[slot];
    stub_record_t r = {.type = STUB_REC_ASSIGN,
                       .id = body_sync_id_make(slot, b->generation),
                       .seat = b->seat,
                       .number = b->number,
                       .hold = b->hold,
                       .sequence = a->answer[seat][slot],
                       .holder = b->seat == BODY_SYNC_SEAT_HOST ? b->holder : BODY_SYNC_ID_NONE,
                       .forced = a->forced[seat][slot] ? 1 : 0};
    record_body(a, b, &r);
    *size = stub_wire_put_record(&r, buffer, capacity);
    return *size > 0;
}

uint32_t sync_authority_pop(sync_authority_t *a, uint8_t seat) {
    if (!sync_authority_is_host(a)) {
        if (seat != BODY_SYNC_SEAT_HOST || a->queue_count == 0) {
            return 0;
        }
        uint32_t size = a->queue_size[a->queue_head];
        a->queue_head = (a->queue_head + 1) % BODY_SYNC_RECORDS_QUEUED;
        a->queue_count--;
        return size;
    }
    uint32_t slot = first_owed(a, seat);
    if (slot >= BODY_SYNC_BODIES_MAX) {
        return 0;
    }
    a->owed[seat][slot / 64] &= ~(1ull << (slot % 64));
    a->answer[seat][slot] = 0;
    a->forced[seat][slot] = false;
    return SYNC_AUTHORITY_RECORD_BYTES;
}
