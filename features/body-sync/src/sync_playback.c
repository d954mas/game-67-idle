#include <math.h>
#include <string.h>

#include "sync_playback.h"

void sync_playback_init(sync_playback_t *p, uint32_t tick_hz, uint32_t delay_min, uint32_t delay_max) {
    memset(p, 0, sizeof(*p));
    p->dt = 1.0f / (float)(tick_hz ? tick_hz : 60u);
    p->delay_min = (uint8_t)(delay_min ? delay_min : SYNC_DELAY_MIN);
    p->delay_max = (uint8_t)(delay_max ? delay_max : SYNC_DELAY_MAX);
    if (p->delay_max < p->delay_min) {
        p->delay_max = p->delay_min;
    }
}

void sync_playback_forget_body(sync_playback_t *p, uint16_t slot) {
    if (slot < BODY_SYNC_BODIES_MAX) {
        memset(&p->bodies[slot], 0, sizeof(p->bodies[slot]));
    }
}

bool sync_playback_newest(const sync_playback_t *p, uint16_t slot, uint32_t *tick) {
    if (slot >= BODY_SYNC_BODIES_MAX || !p->bodies[slot].has || p->bodies[slot].count == 0) {
        return false;
    }
    *tick = p->bodies[slot].snapshots[p->bodies[slot].count - 1].tick;
    return true;
}

void sync_playback_forget_until(sync_playback_t *p, uint16_t slot, uint32_t tick) {
    if (slot >= BODY_SYNC_BODIES_MAX) {
        return;
    }
    sync_buffered_t *b = &p->bodies[slot];
    uint8_t keep = 0;
    for (uint8_t k = 0; k < b->count; k++) {
        if ((int32_t)(b->snapshots[k].tick - tick) > 0) {
            b->snapshots[keep++] = b->snapshots[k];
        }
    }
    b->count = keep;
}

void sync_playback_forget_seat(sync_playback_t *p, uint8_t seat, bool keep_states) {
    if (seat >= BODY_SYNC_SEATS_MAX) {
        return;
    }
    memset(&p->timelines[seat], 0, sizeof(p->timelines[seat]));
    if (keep_states) {
        return;
    }
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        if (p->bodies[slot].has && p->bodies[slot].timeline == seat) {
            memset(&p->bodies[slot], 0, sizeof(p->bodies[slot]));
        }
    }
}

/* A tick of the timeline as a number: relative to its base, so time keeps one arithmetic
   across the 32-bit wrap and far from zero. */
static double at(const sync_timeline_t *t, uint32_t tick) { return (double)(int32_t)(tick - t->base); }

/* Sample ticks against local ticks, relative to the oldest local in the window so the doubles
   stay small and every endpoint computes the same numbers. */
typedef struct clock {
    double rate;
    double line; /* max of (s - rate * l) */
    double jitter;
    uint32_t base_local;
    uint32_t base_sample;
} clock_t_;

static void measure(const sync_timeline_t *t, clock_t_ *c) {
    uint32_t oldest = 0, newest = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        uint32_t k = (t->head + i) % SYNC_CLOCK_SAMPLES;
        if (i == 0 || (int32_t)(t->locals[k] - t->locals[oldest]) < 0) {
            oldest = k;
        }
        if (i == 0 || (int32_t)(t->locals[k] - t->locals[newest]) >= 0) {
            newest = k;
        }
    }
    c->base_local = t->locals[oldest];
    c->base_sample = t->samples[oldest];
    double span = (double)(int32_t)(t->locals[newest] - t->locals[oldest]);
    c->rate = 1.0;
    if (span >= (double)SYNC_RATE_SPAN) {
        /* Least squares over the window: two end samples would carry their jitter whole. */
        double ml = 0.0, ms = 0.0;
        for (uint32_t i = 0; i < t->count; i++) {
            uint32_t k = (t->head + i) % SYNC_CLOCK_SAMPLES;
            ml += (double)(int32_t)(t->locals[k] - c->base_local);
            ms += (double)(int32_t)(t->samples[k] - c->base_sample);
        }
        ml /= (double)t->count;
        ms /= (double)t->count;
        double sll = 0.0, sls = 0.0;
        for (uint32_t i = 0; i < t->count; i++) {
            uint32_t k = (t->head + i) % SYNC_CLOCK_SAMPLES;
            double dl = (double)(int32_t)(t->locals[k] - c->base_local) - ml, ds = (double)(int32_t)(t->samples[k] - c->base_sample) - ms;
            sll += dl * dl;
            sls += dl * ds;
        }
        double r = sll > 0.0 ? sls / sll : 1.0;
        c->rate = r < SYNC_RATE_LOW ? SYNC_RATE_LOW : r > SYNC_RATE_HIGH ? SYNC_RATE_HIGH : r;
    }
    double terms[SYNC_CLOCK_SAMPLES];
    c->line = -1e300;
    for (uint32_t i = 0; i < t->count; i++) {
        uint32_t k = (t->head + i) % SYNC_CLOCK_SAMPLES;
        double s = (double)(int32_t)(t->samples[k] - c->base_sample), l = (double)(int32_t)(t->locals[k] - c->base_local);
        terms[i] = s - c->rate * l;
        c->line = terms[i] > c->line ? terms[i] : c->line;
    }
    /* The 95th percentile of the lags behind the fastest path, by insertion sort (64 at most). */
    for (uint32_t i = 0; i < t->count; i++) {
        terms[i] = c->line - terms[i];
    }
    for (uint32_t i = 1; i < t->count; i++) {
        double v = terms[i];
        uint32_t j = i;
        for (; j > 0 && terms[j - 1] > v; j--) {
            terms[j] = terms[j - 1];
        }
        terms[j] = v;
    }
    c->jitter = t->count ? terms[(t->count * 95u) / 100u] : 0.0;
}

static void push_sample(sync_timeline_t *t, uint32_t sample, uint32_t local) {
    uint32_t at = (t->head + t->count) % SYNC_CLOCK_SAMPLES;
    if (t->count == SYNC_CLOCK_SAMPLES) {
        at = t->head;
        t->head = (t->head + 1u) % SYNC_CLOCK_SAMPLES;
    } else {
        t->count++;
    }
    t->samples[at] = sample;
    t->locals[at] = local;
    t->total++;
}

bool sync_playback_admit(sync_playback_t *p, uint8_t timeline, uint32_t sample, uint32_t local) {
    if (timeline >= BODY_SYNC_SEATS_MAX) {
        return false;
    }
    sync_timeline_t *t = &p->timelines[timeline];
    if (t->verdict_valid && t->verdict_sample == sample) {
        return t->verdict; /* the packet's other records share its sample */
    }
    bool take = true;
    if (t->active && t->count > 0 && !sync_playback_plausible(p, timeline, sample, local)) {
        clock_t_ c;
        measure(t, &c);
        double now = (double)(int32_t)(c.base_sample - t->newest) + c.line + c.rate * (double)(int32_t)(local - c.base_local);
        double off = (double)(int32_t)(sample - t->newest) - now;
        if (t->far_run > 0 && fabs(off - t->far_offset) <= (double)p->delay_max) {
            t->far_run++;
        } else {
            t->far_run = 1;
            t->far_offset = off;
        }
        if (t->far_run >= SYNC_FAR_RUN) {
            /* The owner's time really moved: the clock starts over on the new samples. Playback
               stays and catches up by a reported jump, which the render offset absorbs. */
            t->count = 0;
            t->head = 0;
            t->seen = 0;
            t->far_run = 0;
            t->restarts++;
        } else {
            t->far_dropped++;
            take = false;
        }
    } else {
        t->far_run = 0;
    }
    t->verdict_valid = true;
    t->verdict_sample = sample;
    t->verdict = take;
    return take;
}

void sync_playback_arrival(sync_playback_t *p, uint8_t timeline, uint32_t sample, uint32_t local) {
    if (timeline >= BODY_SYNC_SEATS_MAX || !sync_playback_admit(p, timeline, sample, local)) {
        return;
    }
    sync_timeline_t *t = &p->timelines[timeline];
    if (!t->active) {
        t->active = true;
        t->rate = 1.0f;
        t->pace = 1.0f;
        t->newest = sample;
        t->base = sample;
        t->seen = 1u;
        push_sample(t, sample, local);
        return;
    }
    int32_t ahead = (int32_t)(sample - t->newest);
    if (ahead < -SYNC_REBASE_TICKS) {
        return; /* so far behind no window holds it: it would wreck the clock's arithmetic */
    }
    if (ahead > 0) {
        t->seen = ahead >= 64 ? 0u : t->seen << ahead;
        t->seen |= 1u;
        t->newest = sample;
    } else {
        uint32_t back = (uint32_t)(-ahead);
        if (back < 64u) {
            if (t->seen & (1ull << back)) {
                return; /* a duplicate: no new information, no jitter */
            }
            t->seen |= 1ull << back;
        }
    }
    push_sample(t, sample, local);
}

static void play_groups(sync_buffered_t *b, const sync_rare_t *rare, uint32_t tick) {
    for (int g = 0; g < SYNC_RARE_GROUPS; g++) {
        uint8_t bit = (uint8_t)(1u << g);
        if (!(rare->groups & bit) || ((b->known & bit) && (int32_t)(tick - b->rare_tick[g]) <= 0)) {
            continue;
        }
        switch (bit) {
        case SYNC_RARE_MOTION:
            b->rare.kinematic = rare->kinematic;
            b->rare.locks = rare->locks;
            break;
        case SYNC_RARE_FLAGS:
            b->rare.flags = rare->flags;
            break;
        case SYNC_RARE_AUTHORITY:
            b->rare.seat = rare->seat;
            b->rare.number = rare->number;
            b->rare.hold = rare->hold;
            break;
        case SYNC_RARE_HOLDER:
            b->rare.holder_slot = rare->holder_slot;
            b->rare.holder_generation_low = rare->holder_generation_low;
            break;
        default:
            b->rare.data_size = rare->data_size;
            memcpy(b->rare.data, rare->data, sizeof(b->rare.data));
            break;
        }
        b->known |= bit;
        b->rare_tick[g] = tick;
    }
    b->rare.groups = b->known;
}

void sync_playback_advance(sync_playback_t *p, uint32_t local) {
    p->local_tick = local;
    for (uint8_t i = 0; i < BODY_SYNC_SEATS_MAX; i++) {
        sync_timeline_t *t = &p->timelines[i];
        if (!t->active) {
            continue;
        }
        if ((int32_t)(t->newest - t->base) > SYNC_REBASE_TICKS) {
            t->playback -= at(t, t->newest);
            t->base = t->newest;
        }
        clock_t_ c;
        measure(t, &c);
        uint8_t delay = p->delay_max;
        if (t->total >= SYNC_CLOCK_SETTLE) {
            double d = ceil(c.jitter) + (double)SYNC_DELAY_MARGIN;
            delay = (uint8_t)(d < p->delay_min ? p->delay_min : d > p->delay_max ? p->delay_max : d);
        }
        t->delay = delay;
        t->rate = (float)c.rate;
        double now = c.line + c.rate * (double)(int32_t)(local - c.base_local);
        double target = at(t, c.base_sample) + now - (double)delay;
        double newest = at(t, t->newest);
        if (!t->started) {
            t->playback = target < newest ? target : newest;
            t->started = true;
            t->pace = 1.0f;
        } else if (target - t->playback > (double)SYNC_JUMP_TICKS) {
            /* Waiting at the newest sample through a cut is no jump: only a move forward is. */
            double to = target < newest ? target : newest;
            if (to > t->playback) {
                t->playback = to;
                t->jumps++;
                t->jumped_at = local + 1u;
            }
        } else {
            /* The gap after this tick's step: before it, playback would settle a tick late. */
            double pace = 1.0 + (target - t->playback - c.rate) / 20.0;
            pace = pace < SYNC_PACE_SLOW ? SYNC_PACE_SLOW : pace > SYNC_PACE_FAST ? SYNC_PACE_FAST : pace;
            t->pace = (float)pace;
            t->playback += c.rate * pace;
        }
        if (t->playback > newest) {
            t->playback = newest;
        }
    }
    for (uint32_t slot = 0; slot < BODY_SYNC_BODIES_MAX; slot++) {
        sync_buffered_t *b = &p->bodies[slot];
        const sync_timeline_t *t = &p->timelines[b->timeline];
        if (!b->has || !t->started) {
            continue;
        }
        for (uint8_t k = 0; k < b->count; k++) {
            sync_snapshot_t *s = &b->snapshots[k];
            if (!s->played && at(t, s->tick) <= t->playback) {
                play_groups(b, &s->rare, s->tick);
                s->played = true;
            }
        }
    }
}

/* Rare groups `from` carried that `to` did not: what a dropped snapshot hands on. */
static void merge_rare(sync_rare_t *to, const sync_rare_t *from) {
    uint8_t missing = (uint8_t)(from->groups & ~to->groups);
    if (missing & SYNC_RARE_MOTION) {
        to->kinematic = from->kinematic;
        to->locks = from->locks;
    }
    if (missing & SYNC_RARE_FLAGS) {
        to->flags = from->flags;
    }
    if (missing & SYNC_RARE_AUTHORITY) {
        to->seat = from->seat;
        to->number = from->number;
        to->hold = from->hold;
    }
    if (missing & SYNC_RARE_HOLDER) {
        to->holder_slot = from->holder_slot;
        to->holder_generation_low = from->holder_generation_low;
    }
    if (missing & SYNC_RARE_DATA) {
        to->data_size = from->data_size;
        memcpy(to->data, from->data, sizeof(to->data));
    }
    to->groups |= missing;
}

/* Room for one more snapshot in a full buffer. */
static void evict(sync_buffered_t *b, const sync_timeline_t *t) {
    uint8_t drop = 0;
    if (t->started && b->count >= 3 && at(t, b->snapshots[1].tick) > t->playback) {
        /* Every snapshot after the first is still to play: thin the densest stretch. */
        int32_t best = INT32_MAX;
        for (uint8_t k = 1; k + 1 < b->count; k++) {
            int32_t gap = (int32_t)(b->snapshots[k + 1].tick - b->snapshots[k - 1].tick);
            if (gap < best) {
                best = gap;
                drop = k;
            }
        }
        if (!b->snapshots[drop].played) {
            merge_rare(&b->snapshots[drop + 1].rare, &b->snapshots[drop].rare);
        }
    } else if (!b->snapshots[0].played) {
        play_groups(b, &b->snapshots[0].rare, b->snapshots[0].tick);
    }
    memmove(&b->snapshots[drop], &b->snapshots[drop + 1], sizeof(b->snapshots[0]) * (size_t)(b->count - 1u - drop));
    b->count--;
}

sync_file_t sync_playback_file(sync_playback_t *p, uint8_t timeline, uint32_t sample, const sync_record_t *r) {
    if (timeline >= BODY_SYNC_SEATS_MAX || r->slot >= BODY_SYNC_BODIES_MAX) {
        return SYNC_FILE_LATE;
    }
    sync_buffered_t *b = &p->bodies[r->slot];
    if (b->has && b->timeline != timeline) {
        b->count = 0; /* a new owner: its timeline is another clock */
    }
    b->has = true;
    b->timeline = timeline;
    for (uint8_t k = 0; k < b->count; k++) {
        if (b->snapshots[k].tick == sample) {
            return SYNC_FILE_DUPLICATE;
        }
    }
    const sync_timeline_t *t = &p->timelines[timeline];
    if ((t->started && at(t, sample) < t->playback) || (b->count == SYNC_PLAYBACK_SNAPSHOTS && (int32_t)(sample - b->snapshots[0].tick) < 0)) {
        play_groups(b, &r->rare, sample);
        return SYNC_FILE_LATE;
    }
    if (b->count == SYNC_PLAYBACK_SNAPSHOTS) {
        evict(b, t);
    }
    uint8_t at = b->count;
    while (at > 0 && (int32_t)(b->snapshots[at - 1].tick - sample) > 0) {
        b->snapshots[at] = b->snapshots[at - 1];
        at--;
    }
    sync_snapshot_t *s = &b->snapshots[at];
    memset(s, 0, sizeof(*s));
    s->tick = sample;
    s->sim = r->sim;
    s->at_rest = r->at_rest;
    memcpy(s->position, r->position, sizeof(s->position));
    memcpy(s->rotation, r->rotation, sizeof(s->rotation));
    memcpy(s->velocity, r->velocity, sizeof(s->velocity));
    memcpy(s->angular, r->angular, sizeof(s->angular));
    s->rare = r->rare;
    b->count++;
    return SYNC_FILE_TAKEN;
}

static void normalize(float q[4]) {
    float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (n > 0.0f) {
        for (int i = 0; i < 4; i++) {
            q[i] /= n;
        }
    } else {
        q[0] = q[1] = q[2] = 0.0f;
        q[3] = 1.0f;
    }
}

static void hold(const sync_snapshot_t *s, sync_play_t *out) {
    memcpy(out->position, s->position, sizeof(out->position));
    memcpy(out->rotation, s->rotation, sizeof(out->rotation));
    memset(out->velocity, 0, sizeof(out->velocity));
    memset(out->angular, 0, sizeof(out->angular));
}

void sync_playback_sample(const sync_playback_t *p, uint16_t slot, sync_play_t *out) {
    memset(out, 0, sizeof(*out));
    out->rotation[3] = 1.0f;
    if (slot >= BODY_SYNC_BODIES_MAX) {
        return;
    }
    const sync_buffered_t *b = &p->bodies[slot];
    const sync_timeline_t *t = &p->timelines[b->timeline];
    if (!b->has || b->count == 0 || !t->started) {
        return;
    }
    out->known = true;
    out->jumped = t->jumped_at == p->local_tick + 1u;
    double now = t->playback;
    int last = -1;
    for (int k = 0; k < b->count; k++) {
        if (at(t, b->snapshots[k].tick) <= now) {
            last = k;
        }
    }
    if (last < 0) {
        hold(&b->snapshots[0], out);
        return;
    }
    const sync_snapshot_t *s0 = &b->snapshots[last];
    if (s0->at_rest) {
        hold(s0, out);
        out->at_rest = true;
        return;
    }
    if (last + 1 >= b->count) {
        hold(s0, out);
        out->frozen = now - at(t, s0->tick) > (double)SYNC_FREEZE_TICKS;
        if (!out->frozen) {
            /* Waiting for the next snapshot the pose holds; the owner's last motion is kept for
               a dynamic hold, which would otherwise brake every time playback catches up. */
            memcpy(out->velocity, s0->velocity, sizeof(out->velocity));
            memcpy(out->angular, s0->angular, sizeof(out->angular));
        }
        return;
    }
    const sync_snapshot_t *s1 = &b->snapshots[last + 1];
    double span = (double)(int32_t)(s1->tick - s0->tick);
    float u = (float)((now - at(t, s0->tick)) / span);
    if (s0->sim && s1->sim) {
        /* Cubic Hermite with the owner's velocities as tangents over the snapshots' span. */
        float h = (float)span * p->dt, u2 = u * u, u3 = u2 * u;
        float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f, h10 = u3 - 2.0f * u2 + u, h01 = -2.0f * u3 + 3.0f * u2, h11 = u3 - u2;
        for (int i = 0; i < 3; i++) {
            out->position[i] = h00 * s0->position[i] + h10 * h * s0->velocity[i] + h01 * s1->position[i] + h11 * h * s1->velocity[i];
            /* Inside the box of the two snapshots: a fast body that stopped dead between them
               (a wall) would otherwise overshoot into what stopped it. */
            float lo = fminf(s0->position[i], s1->position[i]), hi = fmaxf(s0->position[i], s1->position[i]);
            out->position[i] = out->position[i] < lo ? lo : out->position[i] > hi ? hi : out->position[i];
        }
    } else {
        for (int i = 0; i < 3; i++) {
            out->position[i] = s0->position[i] + (s1->position[i] - s0->position[i]) * u;
        }
    }
    float dot = 0.0f;
    for (int i = 0; i < 4; i++) {
        dot += s0->rotation[i] * s1->rotation[i];
    }
    float sign = dot < 0.0f ? -1.0f : 1.0f; /* the shorter arc */
    for (int i = 0; i < 4; i++) {
        out->rotation[i] = s0->rotation[i] + (sign * s1->rotation[i] - s0->rotation[i]) * u;
    }
    normalize(out->rotation);
    for (int i = 0; i < 3; i++) {
        out->velocity[i] = s0->velocity[i] + (s1->velocity[i] - s0->velocity[i]) * u;
        out->angular[i] = s0->angular[i] + (s1->angular[i] - s0->angular[i]) * u;
    }
}

const sync_rare_t *sync_playback_rare(const sync_playback_t *p, uint16_t slot, uint8_t *known) {
    const sync_buffered_t *b = &p->bodies[slot < BODY_SYNC_BODIES_MAX ? slot : 0];
    *known = slot < BODY_SYNC_BODIES_MAX && b->has ? b->known : 0u;
    return &b->rare;
}

bool sync_playback_tick(const sync_playback_t *p, uint16_t slot, double *owner_tick) {
    if (slot >= BODY_SYNC_BODIES_MAX || !p->bodies[slot].has) {
        return false;
    }
    const sync_timeline_t *t = &p->timelines[p->bodies[slot].timeline];
    if (!t->started) {
        return false;
    }
    double tick = (double)t->base + t->playback; /* in the owner's count, which wraps at 2^32 */
    *owner_tick = tick < 0.0 ? tick + 4294967296.0 : tick >= 4294967296.0 ? tick - 4294967296.0 : tick;
    return true;
}

bool sync_playback_plausible(const sync_playback_t *p, uint8_t timeline, uint32_t sample, uint32_t local) {
    if (timeline >= BODY_SYNC_SEATS_MAX) {
        return false;
    }
    const sync_timeline_t *t = &p->timelines[timeline];
    if (!t->active || t->count == 0) {
        return true;
    }
    clock_t_ c;
    measure(t, &c);
    double now = (double)(int32_t)(c.base_sample - t->newest) + c.line + c.rate * (double)(int32_t)(local - c.base_local);
    return (double)(int32_t)(sample - t->newest) - now <= (double)SYNC_FAR_TICKS;
}

bool sync_playback_line_tick(const sync_playback_t *p, uint8_t timeline, uint32_t sample, uint32_t *local) {
    if (timeline >= BODY_SYNC_SEATS_MAX || !p->timelines[timeline].active) {
        return false;
    }
    clock_t_ c;
    measure(&p->timelines[timeline], &c);
    double l = ((double)(int32_t)(sample - c.base_sample) - c.line) / c.rate;
    *local = c.base_local + (uint32_t)(int32_t)floor(l + 0.5);
    return true;
}

/* The rotation taking `from` to `to` on the shorter arc, as axis times angle. */
static void turn_between(const float from[4], const float to[4], float out[3]) {
    /* to * conj(from) */
    float ax = -from[0], ay = -from[1], az = -from[2], aw = from[3];
    float x = to[3] * ax + to[0] * aw + to[1] * az - to[2] * ay;
    float y = to[3] * ay - to[0] * az + to[1] * aw + to[2] * ax;
    float z = to[3] * az + to[0] * ay - to[1] * ax + to[2] * aw;
    float w = to[3] * aw - to[0] * ax - to[1] * ay - to[2] * az;
    if (w < 0.0f) {
        x = -x;
        y = -y;
        z = -z;
        w = -w;
    }
    float s = sqrtf(x * x + y * y + z * z);
    float angle = 2.0f * atan2f(s, w);
    float k = s > 1e-9f ? angle / s : 2.0f;
    out[0] = x * k;
    out[1] = y * k;
    out[2] = z * k;
}

static float length3(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

bool sync_drive_reachable(const float now_position[3], const float now_rotation[4], const sync_play_t *play, float dt, float max_speed,
                          float max_spin) {
    float d[3] = {play->position[0] - now_position[0], play->position[1] - now_position[1], play->position[2] - now_position[2]};
    float w[3];
    turn_between(now_rotation, play->rotation, w);
    return length3(d) <= max_speed * dt && length3(w) <= max_spin * dt;
}

static void clamp3(float v[3], float limit) {
    float n = length3(v);
    if (n > limit) {
        for (int i = 0; i < 3; i++) {
            v[i] *= limit / n;
        }
    }
}

void sync_drive_dynamic(const float now_position[3], const float now_rotation[4], const sync_play_t *play, float dt, float velocity[3],
                        float angular[3]) {
    float fix[3], turn[3];
    for (int i = 0; i < 3; i++) {
        fix[i] = (play->position[i] - now_position[i]) / (SYNC_CORRECT_TICKS * dt);
    }
    clamp3(fix, SYNC_CORRECT_SPEED);
    turn_between(now_rotation, play->rotation, turn);
    for (int i = 0; i < 3; i++) {
        turn[i] /= SYNC_CORRECT_TICKS * dt;
    }
    clamp3(turn, SYNC_CORRECT_SPIN);
    for (int i = 0; i < 3; i++) {
        velocity[i] = play->velocity[i] + fix[i];
        angular[i] = play->angular[i] + turn[i];
    }
}

static void quat_mul(const float a[4], const float b[4], float out[4]);

bool sync_drive_locked_turn(const float now[4], const float target[4], uint8_t locked_axes, float out[4]) {
    float w[3];
    turn_between(now, target, w);
    for (int i = 0; i < 3; i++) {
        w[i] = (locked_axes & (1u << i)) ? w[i] : 0.0f;
    }
    float angle = length3(w);
    if (angle <= SYNC_FACING_SET_RAD) {
        return false;
    }
    float k = sinf(0.5f * angle) / angle;
    float e[4] = {w[0] * k, w[1] * k, w[2] * k, cosf(0.5f * angle)};
    quat_mul(e, now, out);
    normalize(out);
    return true;
}

void sync_offset_clear(sync_offset_t *o) {
    memset(o, 0, sizeof(*o));
    o->rotation[3] = 1.0f;
}

static void quat_mul(const float a[4], const float b[4], float out[4]) {
    float r[4] = {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
                  a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
    memcpy(out, r, sizeof(r));
}

void sync_offset_absorb(sync_offset_t *o, const float before_position[3], const float before_rotation[4], const float after_position[3],
                        const float after_rotation[4]) {
    /* drawn = offset + physics, drawn rotation = offset rotation * physics rotation */
    for (int i = 0; i < 3; i++) {
        o->position[i] += before_position[i] - after_position[i];
    }
    float drawn[4], inverse[4] = {-after_rotation[0], -after_rotation[1], -after_rotation[2], after_rotation[3]};
    quat_mul(o->rotation, before_rotation, drawn);
    quat_mul(drawn, inverse, o->rotation);
    normalize(o->rotation);
    if (length3(o->position) > SYNC_OFFSET_SNAP_M) {
        sync_offset_clear(o);
    }
}

void sync_offset_decay(sync_offset_t *o) {
    float d = length3(o->position);
    if (d > SYNC_OFFSET_SNAP_M) {
        sync_offset_clear(o);
        return;
    }
    float k = d <= SYNC_OFFSET_NEAR_M ? SYNC_OFFSET_SLOW
              : d >= SYNC_OFFSET_FAR_M ? SYNC_OFFSET_FAST
                                       : SYNC_OFFSET_SLOW + (SYNC_OFFSET_FAST - SYNC_OFFSET_SLOW) * (d - SYNC_OFFSET_NEAR_M) / (SYNC_OFFSET_FAR_M - SYNC_OFFSET_NEAR_M);
    for (int i = 0; i < 3; i++) {
        o->position[i] *= k;
    }
    float sign = o->rotation[3] < 0.0f ? -1.0f : 1.0f;
    for (int i = 0; i < 3; i++) {
        o->rotation[i] *= sign * k;
    }
    o->rotation[3] = sign * o->rotation[3] * k + (1.0f - k);
    normalize(o->rotation);
}
