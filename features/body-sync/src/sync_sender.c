#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sync_sender.h"

bool sync_sender_budget_ok(const sync_codec_t *codec, uint8_t seats, uint32_t budget) {
    uint64_t bytes = budget ? budget : SYNC_DEFAULT_BUDGET;
    return bytes * 8u >= (uint64_t)(seats + 1u) * codec->record_bits_max + 1u;
}

void sync_sender_init(sync_sender_t *s, const sync_codec_t *codec, uint8_t seats, uint32_t budget) {
    memset(s, 0, sizeof(*s));
    s->codec = codec;
    s->seats = seats;
    s->budget = budget ? budget : SYNC_DEFAULT_BUDGET;
}

/* A body's memory for one seat starts over. Versions and the sleep epoch move on instead of
   going back to zero, so a packet still in the window cannot acknowledge the new state. */
static void reset_body(sync_seat_body_t *b) {
    uint8_t epoch = (uint8_t)(b->sleep_epoch + 1u);
    uint8_t version[SYNC_RARE_GROUPS];
    for (int g = 0; g < SYNC_RARE_GROUPS; g++) {
        version[g] = (uint8_t)(b->version[g] + 1u);
    }
    memset(b, 0, sizeof(*b));
    b->sleep_epoch = epoch;
    for (int g = 0; g < SYNC_RARE_GROUPS; g++) {
        b->version[g] = version[g];
        b->acked_version[g] = (uint8_t)(version[g] - 1u);
    }
}

void sync_sender_seat(sync_sender_t *s, uint8_t seat, bool joined) {
    if (seat >= BODY_SYNC_SEATS_MAX) {
        return;
    }
    s->joined[seat] = joined;
    s->staged_valid[seat] = false;
    memset(s->sent[seat], 0, sizeof(s->sent[seat]));
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        reset_body(&s->bodies[seat][slot]);
    }
}

void sync_sender_forget(sync_sender_t *s, uint16_t slot) {
    if (slot >= BODY_SYNC_BODIES_MAX) {
        return;
    }
    for (uint8_t seat = 0; seat < BODY_SYNC_SEATS_MAX; seat++) {
        reset_body(&s->bodies[seat][slot]);
    }
}

static void pose_of(const sync_record_t *r, float out[7]) {
    memcpy(out, r->position, 3 * sizeof(float));
    memcpy(out + 3, r->rotation, 4 * sizeof(float));
}

static bool same_value(const sync_rare_t *a, const sync_rare_t *b, uint8_t group) {
    switch (group) {
    case SYNC_RARE_MOTION:
        return a->kinematic == b->kinematic && a->locks == b->locks;
    case SYNC_RARE_FLAGS:
        return a->flags == b->flags;
    case SYNC_RARE_AUTHORITY:
        return a->seat == b->seat && a->number == b->number && a->hold == b->hold;
    case SYNC_RARE_DATA:
        return a->data_size == b->data_size && memcmp(a->data, b->data, a->data_size) == 0;
    default:
        return a->holder_slot == b->holder_slot && a->holder_generation_low == b->holder_generation_low;
    }
}

static uint8_t pending_groups(const sync_seat_body_t *b) {
    uint8_t g = 0;
    for (int i = 0; i < SYNC_RARE_GROUPS; i++) {
        if (b->version[i] != b->acked_version[i]) {
            g |= (uint8_t)(1u << i);
        }
    }
    return g;
}

static float speed_of(const sync_record_t *r) {
    float v = sqrtf(r->velocity[0] * r->velocity[0] + r->velocity[1] * r->velocity[1] + r->velocity[2] * r->velocity[2]);
    float w = sqrtf(r->angular[0] * r->angular[0] + r->angular[1] * r->angular[1] + r->angular[2] * r->angular[2]);
    float speed = v + SYNC_SPIN_RADIUS * w;
    return isfinite(speed) ? speed : SYNC_FAST_SPEED;
}

static float rate_of(const sync_body_input_t *in, const sync_seat_body_t *b, bool hit) {
    if (pending_groups(b)) {
        return 1.0f;
    }
    if (in->asleep) {
        return b->rest_acked ? 0.0f : SYNC_RATE_REST;
    }
    if (in->held || in->flying || hit) {
        return 1.0f;
    }
    float k = speed_of(&in->state) / SYNC_FAST_SPEED;
    return SYNC_RATE_MIN + (1.0f - SYNC_RATE_MIN) * (k < 1.0f ? k : 1.0f);
}

/* The quantized rest pose a sleep is sent as. */
static void rest_pose(const sync_codec_t *c, const sync_body_input_t *in, float out[7]) {
    sync_record_t r = in->state;
    r.sim = true;
    r.at_rest = true;
    r.rare.groups = 0;
    if (!sync_codec_limit(c, &r)) {
        memset(out, 0, 7 * sizeof(float));
        return;
    }
    sync_codec_quantize(c, &r);
    pose_of(&r, out);
}

void sync_sender_tick(sync_sender_t *s, uint32_t tick, const sync_body_input_t *bodies, const uint8_t *send_to) {
    s->tick = tick;
    s->inputs = bodies;
    s->cursor = (uint16_t)((s->cursor + 1u) % BODY_SYNC_BODIES_MAX);
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        const float *v = bodies[slot].state.velocity;
        float d[3] = {v[0] - s->velocity[slot][0], v[1] - s->velocity[slot][1], v[2] - s->velocity[slot][2]};
        s->hit[slot] = bodies[slot].present && s->has_velocity[slot] && d[0] * d[0] + d[1] * d[1] + d[2] * d[2] >= SYNC_HIT_SPEED * SYNC_HIT_SPEED;
        s->has_velocity[slot] = bodies[slot].present;
        memcpy(s->velocity[slot], v, sizeof(s->velocity[slot]));
    }
    for (uint8_t seat = 0; seat < s->seats; seat++) {
        s->wrote[seat] = false;
        if (!s->joined[seat]) {
            continue;
        }
        for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
            sync_seat_body_t *b = &s->bodies[seat][slot];
            const sync_body_input_t *in = &bodies[slot];
            bool eligible = in->present && (send_to[slot] & (1u << seat)) != 0;
            if (!eligible) {
                if (b->eligible) {
                    reset_body(b);
                }
                continue;
            }
            if (!b->eligible) {
                b->eligible = true;
                b->last_sent_tick = tick;
            }
            /* A body that needed nothing starts its age when it wants a packet again. */
            bool was_idle = b->asleep && b->rest_acked && !pending_groups(b);
            sync_rare_t now = in->state.rare;
            for (int g = 0; g < SYNC_RARE_GROUPS; g++) {
                if (!b->groups_known || !same_value(&b->last_value, &now, (uint8_t)(1u << g))) {
                    b->version[g]++;
                }
            }
            b->last_value = now;
            b->groups_known = true;
            if (in->asleep && !b->asleep) {
                b->sleep_epoch++;
                rest_pose(s->codec, in, b->sleep_pose);
                b->rest_acked = b->rest_pose_known && memcmp(b->sleep_pose, b->acked_pose, sizeof(b->acked_pose)) == 0;
            } else if (!in->asleep) {
                b->rest_acked = false;
            }
            b->asleep = in->asleep;
            if (was_idle && !(b->asleep && b->rest_acked && !pending_groups(b))) {
                b->last_sent_tick = tick;
            }
            float rate = rate_of(in, b, s->hit[slot]);
            b->acc += rate;
            if (rate > 0.0f) {
                uint32_t age = tick - b->last_sent_tick;
                if (age > s->stats[seat].stale_max_ticks && !in->asleep) {
                    s->stats[seat].stale_max_ticks = age;
                }
            }
        }
    }
}

static int by_rank(const void *pa, const void *pb) {
    const sync_candidate_t *a = pa, *b = pb;
    if (a->rank != b->rank) {
        return a->rank < b->rank ? -1 : 1;
    }
    if (a->key != b->key) {
        return a->key > b->key ? -1 : 1;
    }
    return a->slot < b->slot ? -1 : a->slot > b->slot;
}

static void count_bits(sync_sender_stats_t *st, const sync_codec_t *c, const sync_record_t *r, bool held) {
    uint32_t total = sync_codec_record_bits(c, r);
    uint32_t rare = sync_codec_rare_bits(c, &r->rare);
    st->bits[BODY_SYNC_TRAFFIC_HEADER] += 1;
    st->bits[BODY_SYNC_TRAFFIC_AUTHORITY] += rare;
    body_sync_traffic_kind_t kind = held ? BODY_SYNC_TRAFFIC_HELD : r->sim ? BODY_SYNC_TRAFFIC_SIM : BODY_SYNC_TRAFFIC_COPY;
    st->bits[kind] += total - 1 - rare;
}

uint32_t sync_sender_write(sync_sender_t *s, uint8_t seat, bool sim, uint8_t *buffer, uint32_t capacity) {
    if (seat >= s->seats || !s->joined[seat] || s->wrote[seat] || !s->inputs || capacity < BODY_SYNC_RECORD_MAX || !buffer) {
        return 0;
    }
    s->wrote[seat] = true;
    s->staged_valid[seat] = false;
    sync_candidate_t *candidates = s->candidates;
    uint32_t n = 0;
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        const sync_seat_body_t *b = &s->bodies[seat][slot];
        const sync_body_input_t *in = &s->inputs[slot];
        if (!b->eligible) {
            continue;
        }
        uint32_t age = s->tick - b->last_sent_tick;
        bool idle = in->asleep && b->rest_acked && !pending_groups(b);
        if (in->character) {
            candidates[n++] = (sync_candidate_t){(uint16_t)slot, 0, 0.0f};
        } else if (!idle && age >= SYNC_AGE_MAX) {
            candidates[n++] = (sync_candidate_t){(uint16_t)slot, 1, (float)age};
        } else if (!idle && b->acc >= 1.0f) {
            /* Ties go round: a turning cursor decides which equal body leads this tick. */
            float turn = (float)((slot + BODY_SYNC_BODIES_MAX - s->cursor) % BODY_SYNC_BODIES_MAX) / (float)BODY_SYNC_BODIES_MAX;
            candidates[n++] = (sync_candidate_t){(uint16_t)slot, 2, b->acc - 1e-3f * turn};
        }
    }
    if (n == 0) {
        return 0;
    }
    qsort(candidates, n, sizeof(candidates[0]), by_rank);
    uint32_t room = capacity < s->budget ? capacity : s->budget;
    sync_packet_writer_t w;
    sync_packet_begin(&w, buffer, room);
    sync_sent_packet_t *staged = &s->staged[seat];
    staged->count = 0;
    staged->tick = s->tick;
    s->staged_slot_count[seat] = 0;
    sync_sender_stats_t *st = &s->stats[seat];
    uint64_t used_bits = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint16_t slot = candidates[i].slot;
        sync_seat_body_t *b = &s->bodies[seat][slot];
        const sync_body_input_t *in = &s->inputs[slot];
        sync_record_t r = in->state;
        r.slot = slot;
        r.at_rest = in->asleep;
        r.sim = sim || in->asleep;
        r.rare.groups = pending_groups(b);
        if (!sync_codec_limit(s->codec, &r)) {
            continue;
        }
        if (!sync_packet_add(&w, s->codec, &r)) {
            st->deferred++;
            continue;
        }
        uint16_t at = s->staged_slot_count[seat]++;
        s->staged_slots[seat][at] = slot;
        s->staged_rest[seat][at] = in->asleep;
        count_bits(st, s->codec, &r, in->held);
        used_bits += sync_codec_record_bits(s->codec, &r);
        if ((in->asleep || r.rare.groups) && staged->count < SYNC_SENT_RECORDS) {
            sync_sent_record_t *e = &staged->records[staged->count++];
            *e = (sync_sent_record_t){.slot = slot, .rest = in->asleep, .sleep_epoch = b->sleep_epoch, .groups = r.rare.groups};
            memcpy(e->version, b->version, sizeof(e->version));
        }
    }
    uint32_t size = sync_packet_finish(&w);
    if (size) {
        st->bits[BODY_SYNC_TRAFFIC_HEADER] += (uint64_t)size * 8u - used_bits; /* the end bit and the padding */
        s->staged_valid[seat] = true;
        st->packets++;
    }
    return size;
}

void sync_sender_sent(sync_sender_t *s, uint8_t seat, uint16_t number) {
    if (seat >= s->seats || !s->staged_valid[seat]) {
        return;
    }
    s->staged_valid[seat] = false;
    sync_sent_packet_t *p = &s->sent[seat][s->sent_next[seat]];
    *p = s->staged[seat];
    p->used = true;
    p->number = number;
    for (uint32_t k = 0; k < s->staged_slot_count[seat]; k++) {
        bool rest = s->staged_rest[seat][k];
        sync_seat_body_t *b = &s->bodies[seat][s->staged_slots[seat][k]];
        uint32_t age = p->tick - b->last_sent_tick;
        if (!rest && age > s->stats[seat].stale_max_ticks) {
            s->stats[seat].stale_max_ticks = age;
        }
        b->acc = 0.0f;
        b->last_sent_tick = p->tick;
        if (!rest) {
            b->rest_pose_known = false; /* the peer's newest pose of it is no longer the rest */
        }
    }
    s->sent_next[seat] = (s->sent_next[seat] + 1u) % SYNC_SENT_WINDOW;
}

float sync_sender_round_trip(const sync_sender_t *s, uint8_t seat) { return seat < s->seats ? s->round_trip[seat] : 0.0f; }

uint32_t sync_sender_unresolved(const sync_sender_t *s, uint8_t seat, uint16_t *numbers, uint32_t max) {
    if (seat >= s->seats) {
        return 0;
    }
    uint32_t n = 0;
    for (uint32_t i = 0; i < SYNC_SENT_WINDOW && n < max; i++) {
        const sync_sent_packet_t *p = &s->sent[seat][(s->sent_next[seat] + i) % SYNC_SENT_WINDOW];
        if (p->used) {
            numbers[n++] = p->number;
        }
    }
    return n;
}

void sync_sender_resolve(sync_sender_t *s, uint8_t seat, uint16_t number, bool arrived) {
    if (seat >= s->seats) {
        return;
    }
    for (uint32_t i = 0; i < SYNC_SENT_WINDOW; i++) {
        sync_sent_packet_t *p = &s->sent[seat][i];
        if (!p->used || p->number != number) {
            continue;
        }
        p->used = false;
        if (!arrived) {
            return;
        }
        float sample = (float)(int32_t)(s->tick - p->tick);
        float *rtt = &s->round_trip[seat];
        *rtt = *rtt <= 0.0f ? sample : *rtt + (sample - *rtt) / 8.0f; /* TCP's smoothing */
        for (uint32_t k = 0; k < p->count; k++) {
            const sync_sent_record_t *e = &p->records[k];
            sync_seat_body_t *b = &s->bodies[seat][e->slot];
            if (e->rest && b->asleep && e->sleep_epoch == b->sleep_epoch) {
                b->rest_acked = true;
                b->rest_pose_known = true;
                memcpy(b->acked_pose, b->sleep_pose, sizeof(b->acked_pose));
            }
            for (int g = 0; g < SYNC_RARE_GROUPS; g++) {
                if ((e->groups & (1u << g)) && e->version[g] == b->version[g]) {
                    b->acked_version[g] = e->version[g];
                }
            }
        }
        return;
    }
}

bool sync_sender_rest_acked(const sync_sender_t *s, uint8_t seat, uint16_t slot) {
    if (seat >= BODY_SYNC_SEATS_MAX || slot >= BODY_SYNC_BODIES_MAX) {
        return false;
    }
    const sync_seat_body_t *b = &s->bodies[seat][slot];
    return b->eligible && b->asleep && b->rest_acked && !pending_groups(b);
}
