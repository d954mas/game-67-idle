#include "net_link_sim_fate.h"

enum {
    FATE_MAX_MS = 60000,
    FATE_MIN_RETRANSMIT_US = 1000,
    FATE_BUCKET_DEPTH_US = 20000, /* the bucket holds this long of the rate: the largest burst */
};

static uint64_t mix64(uint64_t z) {
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void sim_rng_seed(sim_rng_t *rng, uint64_t seed, uint64_t stream) { rng->state = mix64(seed ^ mix64(stream + 0x9E3779B97F4A7C15ULL)); }

double sim_rng_unit(sim_rng_t *rng) {
    rng->state += 0x9E3779B97F4A7C15ULL;
    return (double)(mix64(rng->state) >> 11) * (1.0 / 9007199254740992.0);
}

static double clamp_share(float value) {
    if (!(value > 0.0F)) {
        return 0.0;
    }
    return value >= 1.0F ? 1.0 : (double)value;
}

static int64_t clamp_us(float ms) {
    if (!(ms > 0.0F)) {
        return 0;
    }
    if (ms >= (float)FATE_MAX_MS) {
        return (int64_t)FATE_MAX_MS * 1000;
    }
    return (int64_t)((double)ms * 1000.0 + 0.5);
}

void sim_conditions_set(sim_conditions_t *out, const net_link_conditions_t *in) {
    const net_link_conditions_t none = {0};
    if (!in) {
        in = &none;
    }
    out->delay_us = clamp_us(in->delay_ms);
    out->jitter_us = clamp_us(in->jitter_ms);
    out->reorder_us = clamp_us(in->reorder_ms);
    out->retransmit_us = in->retransmit_ms > 0.0F ? clamp_us(in->retransmit_ms) : 2 * out->delay_us + 50000;
    if (out->retransmit_us < FATE_MIN_RETRANSMIT_US) {
        out->retransmit_us = FATE_MIN_RETRANSMIT_US;
    }
    out->loss = clamp_share(in->loss);
    out->duplicate = clamp_share(in->duplicate);
    out->reorder = clamp_share(in->reorder);
    out->rate_bytes_per_s = in->rate_bytes_per_s;
}

static int64_t even_draw(double unit, int64_t span_us) { return span_us > 0 ? (int64_t)(unit * (double)(span_us + 1)) : 0; }

void sim_fate_draw(sim_rng_t *rng, const sim_conditions_t *conditions, bool reliable, sim_fate_t *out) {
    /* Always five draws, so one packet's fate never shifts the stream by its conditions. */
    const double u_loss = sim_rng_unit(rng);
    const double u_jitter = sim_rng_unit(rng);
    const double u_reorder = sim_rng_unit(rng);
    const double u_twice = sim_rng_unit(rng);
    const double u_twice_jitter = sim_rng_unit(rng);
    out->lost = u_loss < conditions->loss;
    out->delay_us = conditions->delay_us + even_draw(u_jitter, conditions->jitter_us);
    out->twice = false;
    out->twice_delay_us = 0;
    if (reliable || out->lost) {
        return;
    }
    if (u_reorder < conditions->reorder) {
        out->delay_us += conditions->reorder_us;
    }
    if (u_twice < conditions->duplicate) {
        out->twice = true;
        out->twice_delay_us = conditions->delay_us + even_draw(u_twice_jitter, conditions->jitter_us);
    }
}

static int64_t bucket_depth(uint32_t rate) { return (int64_t)rate * FATE_BUCKET_DEPTH_US; }

static void bucket_refill(sim_bucket_t *bucket, uint32_t rate, int64_t now_us) {
    if (now_us <= bucket->at_us) {
        return;
    }
    const int64_t elapsed = now_us - bucket->at_us;
    const int64_t depth = bucket_depth(rate);
    /* Comparing against the time to fill keeps rate * elapsed inside int64 after long idles. */
    const int64_t to_full_us = depth > bucket->tokens ? (depth - bucket->tokens + (int64_t)rate - 1) / (int64_t)rate : 0;
    bucket->tokens = elapsed >= to_full_us ? depth : bucket->tokens + (int64_t)rate * elapsed;
    bucket->at_us = now_us;
}

void sim_bucket_reset(sim_bucket_t *bucket, uint32_t rate, int64_t now_us) {
    bucket->tokens = bucket_depth(rate);
    bucket->at_us = now_us;
}

void sim_bucket_set_rate(sim_bucket_t *bucket, uint32_t old_rate, uint32_t new_rate, int64_t now_us) {
    if (old_rate == 0 || new_rate == 0) {
        sim_bucket_reset(bucket, new_rate, now_us);
        return;
    }
    bucket_refill(bucket, old_rate, now_us);
    if (bucket->tokens > bucket_depth(new_rate)) {
        bucket->tokens = bucket_depth(new_rate);
    }
}

int64_t sim_bucket_ready_us(const sim_bucket_t *bucket, uint32_t rate) {
    if (rate == 0 || bucket->tokens >= 0) {
        return bucket->at_us;
    }
    return bucket->at_us + (-bucket->tokens + (int64_t)rate - 1) / (int64_t)rate;
}

void sim_bucket_take(sim_bucket_t *bucket, uint32_t rate, int64_t now_us, uint32_t bytes) {
    if (rate == 0) {
        return;
    }
    bucket_refill(bucket, rate, now_us);
    bucket->tokens -= (int64_t)bytes * 1000000;
}
