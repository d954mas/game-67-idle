#ifndef FEATURES_COOP_NET_COOP_CLOCK_H
#define FEATURES_COOP_NET_COOP_CLOCK_H

#include <stdbool.h>
#include <stdint.h>

/* One side's view of a peer's tick. Each state packet stamped with tick T and polled at local
   time t gives an offset T - t * hz; the least-delayed packet gives the largest. The clock keeps
   the largest offset of each of the last few buckets of time, so it follows a path that got
   slower within one window, and reads that offset plus now * hz. No round-trip term: it trails
   the sender by the shortest one-way delay. */

enum {
    COOP_CLOCK_BUCKETS = 8,
};
#define COOP_CLOCK_BUCKET_S 0.125

typedef struct coop_clock {
    double hz;
    bool any;
    double reference; /* the last stamp, unwrapped: the next one is read relative to it */
    int64_t bucket_id[COOP_CLOCK_BUCKETS];
    double best_offset[COOP_CLOCK_BUCKETS];
    bool has[COOP_CLOCK_BUCKETS];
} coop_clock_t;

void coop_clock_reset(coop_clock_t *clock, uint32_t hz);
void coop_clock_sample(coop_clock_t *clock, uint32_t tick, double now_s);
/* The peer's tick at `now_s`, unwrapped; negative before the first sample or with no rate. */
double coop_clock_estimate(const coop_clock_t *clock, double now_s);

#endif
