#ifndef FEATURES_NET_LINK_SIM_FATE_H
#define FEATURES_NET_LINK_SIM_FATE_H

#include "features/net_link/net_link_sim.h"

/* What happens to one packet on one direction of a path: the stream it draws from, the
   conditions in whole microseconds, its fate and the rate budget it waits for. Links, paths
   and queues live in net_link_sim.c. */

typedef struct sim_rng {
    uint64_t state;
} sim_rng_t;

void sim_rng_seed(sim_rng_t *rng, uint64_t seed, uint64_t stream);
double sim_rng_unit(sim_rng_t *rng); /* [0, 1) */

typedef struct sim_conditions {
    int64_t delay_us;
    int64_t jitter_us;
    int64_t reorder_us;
    int64_t retransmit_us; /* resolved: never zero */
    double loss;
    double duplicate;
    double reorder;
    uint32_t rate_bytes_per_s;
} sim_conditions_t;

/* Clamps every value into range; NULL gives a plain loopback. */
void sim_conditions_set(sim_conditions_t *out, const net_link_conditions_t *in);

typedef struct sim_fate {
    bool lost;
    bool twice;
    int64_t delay_us;
    int64_t twice_delay_us;
} sim_fate_t;

void sim_fate_draw(sim_rng_t *rng, const sim_conditions_t *conditions, bool reliable, sim_fate_t *out);

/* Token bucket in micro-bytes. A packet may leave while the balance is not negative and takes
   its whole size, so a message larger than the bucket still goes and the debt delays the next. */
typedef struct sim_bucket {
    int64_t tokens;
    int64_t at_us;
} sim_bucket_t;

void sim_bucket_reset(sim_bucket_t *bucket, uint32_t rate, int64_t now_us);
void sim_bucket_set_rate(sim_bucket_t *bucket, uint32_t old_rate, uint32_t new_rate, int64_t now_us);
int64_t sim_bucket_ready_us(const sim_bucket_t *bucket, uint32_t rate);
void sim_bucket_take(sim_bucket_t *bucket, uint32_t rate, int64_t now_us, uint32_t bytes);

#endif
