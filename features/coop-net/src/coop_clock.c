#include "coop_clock.h"

#include <math.h>
#include <string.h>

static int64_t bucket_of(double now_s) { return (int64_t)floor(now_s / COOP_CLOCK_BUCKET_S); }

void coop_clock_reset(coop_clock_t *clock, uint32_t hz) {
    memset(clock, 0, sizeof(*clock));
    clock->hz = (double)hz;
}

void coop_clock_sample(coop_clock_t *clock, uint32_t tick, double now_s) {
    if (clock->hz <= 0.0 || !isfinite(now_s)) {
        return;
    }
    double stamped = (double)tick;
    if (clock->any) {
        uint32_t near = (uint32_t)(int64_t)clock->reference;
        stamped = clock->reference + (double)(int32_t)(tick - near);
    }
    clock->any = true;
    clock->reference = stamped;
    double offset = stamped - now_s * clock->hz;
    int64_t id = bucket_of(now_s);
    uint32_t slot = (uint32_t)(((id % COOP_CLOCK_BUCKETS) + COOP_CLOCK_BUCKETS) % COOP_CLOCK_BUCKETS);
    if (!clock->has[slot] || clock->bucket_id[slot] != id) {
        clock->has[slot] = true;
        clock->bucket_id[slot] = id;
        clock->best_offset[slot] = offset;
    } else if (offset > clock->best_offset[slot]) {
        clock->best_offset[slot] = offset;
    }
}

double coop_clock_estimate(const coop_clock_t *clock, double now_s) {
    if (!clock->any || clock->hz <= 0.0) {
        return -1.0;
    }
    int64_t now_id = bucket_of(now_s);
    bool found = false;
    double best = 0.0;
    int64_t newest_id = INT64_MIN;
    double newest_offset = 0.0;
    for (uint32_t slot = 0; slot < COOP_CLOCK_BUCKETS; slot++) {
        if (!clock->has[slot]) {
            continue;
        }
        if (clock->bucket_id[slot] > newest_id) {
            newest_id = clock->bucket_id[slot];
            newest_offset = clock->best_offset[slot];
        }
        if (clock->bucket_id[slot] > now_id - COOP_CLOCK_BUCKETS && (!found || clock->best_offset[slot] > best)) {
            found = true;
            best = clock->best_offset[slot];
        }
    }
    /* Nothing recent: the last offset seen is still the best guess, so the value runs on. */
    double value = (found ? best : newest_offset) + now_s * clock->hz;
    return value >= 0.0 ? value : 0.0;
}
