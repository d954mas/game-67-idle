#include <math.h>
#include <string.h>

#include "sync_replay.h"

static void start(sync_replay_t *r, uint32_t local_tick, uint32_t sample) {
    uint32_t restarts = r->restarts + (r->active ? 1u : 0u);
    memset(r, 0, sizeof(*r));
    r->active = true;
    r->restarts = restarts;
    r->shift = (int32_t)(local_tick - sample - 1u);
    r->newest = sample;
}

sync_replay_action_t sync_replay_decide(sync_replay_t *r, uint32_t local_tick, bool usable, uint32_t sample, const float position[3]) {
    if (!r->active) {
        if (!usable) {
            return SYNC_REPLAY_OFF;
        }
        start(r, local_tick, sample);
        return SYNC_REPLAY_START;
    }
    if (!usable || (int32_t)(sample - r->newest) <= 0) {
        return SYNC_REPLAY_FREE; /* nothing newer: a duplicate, a reordered or a non-flight record */
    }
    r->newest = sample;
    if (r->landed) {
        return SYNC_REPLAY_FREE;
    }
    uint32_t mapped = sample + (uint32_t)r->shift;
    uint32_t k = mapped % SYNC_REPLAY_HISTORY;
    if (!r->known[k] || r->ticks[k] != mapped) {
        return SYNC_REPLAY_FREE; /* not stepped yet (a record faster than the first) or long gone */
    }
    float d[3] = {position[0] - r->history[k][0], position[1] - r->history[k][1], position[2] - r->history[k][2]};
    if (sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) <= SYNC_REPLAY_MISMATCH_M) {
        return SYNC_REPLAY_FREE;
    }
    start(r, local_tick, sample);
    return SYNC_REPLAY_START;
}

void sync_replay_stepped(sync_replay_t *r, uint32_t local_tick, const float position[3], bool touched_body) {
    if (!r->active) {
        return;
    }
    uint32_t k = local_tick % SYNC_REPLAY_HISTORY;
    r->known[k] = true;
    r->ticks[k] = local_tick;
    memcpy(r->history[k], position, sizeof(r->history[k]));
    r->landed = r->landed || touched_body;
}

void sync_replay_stop(sync_replay_t *r) {
    uint32_t restarts = r->restarts;
    memset(r, 0, sizeof(*r));
    r->restarts = restarts;
}

bool sync_round_due(sync_round_t *r, bool flying) {
    bool due = flying && !r->flight;
    r->flight = flying;
    return due;
}
