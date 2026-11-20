#include "coop_acks.h"

#include "coop_wire.h"

#include <math.h>
#include <string.h>

static uint32_t slot_of(uint16_t number) { return (uint32_t)number % COOP_FATE_WINDOW; }

/* How many packets before the newest one sent `number` is; the window holds distances below
   `sent`. */
static uint32_t age_of(const coop_send_window_t *window, uint16_t number) { return (uint16_t)(window->next - 1U - number); }

static bool in_window(const coop_send_window_t *window, uint16_t number) { return window->sent > 0 && age_of(window, number) < window->sent; }

/* A newest ack that aged out of the window says nothing any more: after a long one-way
   silence it could even read as newer than every fresh one. */
static bool has_newest(const coop_send_window_t *window) { return window->has_newest_ack && in_window(window, window->newest_ack); }

void coop_send_window_reset(coop_send_window_t *window) { memset(window, 0, sizeof(*window)); }

uint16_t coop_send_window_push(coop_send_window_t *window, double now_s) {
    uint16_t number = window->next;
    window->fate[slot_of(number)] = COOP_PACKET_PENDING;
    window->send_time[slot_of(number)] = now_s;
    window->next = (uint16_t)(number + 1U);
    if (window->sent < COOP_FATE_WINDOW) {
        window->sent++;
    }
    return number;
}

coop_packet_fate_t coop_send_window_fate(const coop_send_window_t *window, uint16_t number) {
    if (!in_window(window, number)) {
        return COOP_PACKET_LOST;
    }
    return (coop_packet_fate_t)window->fate[slot_of(number)];
}

static void arrive(coop_send_window_t *window, uint16_t number) {
    if (in_window(window, number) && window->fate[slot_of(number)] == COOP_PACKET_PENDING) {
        window->fate[slot_of(number)] = COOP_PACKET_ARRIVED;
    }
}

double coop_send_window_ack(coop_send_window_t *window, uint16_t ack, uint32_t mask, uint16_t wait, double now_s) {
    if (!in_window(window, ack)) {
        return -1.0;
    }
    if (!has_newest(window) || (int16_t)(uint16_t)(ack - window->newest_ack) > 0) {
        window->has_newest_ack = true;
        window->newest_ack = ack;
    }
    double sample = -1.0;
    uint32_t slot = slot_of(ack);
    if (window->fate[slot] == COOP_PACKET_PENDING) {
        window->fate[slot] = COOP_PACKET_ARRIVED;
        if (wait != COOP_WIRE_ACK_WAIT_TOO_LONG) {
            double rtt = now_s - window->send_time[slot] - (double)wait / (double)COOP_WIRE_ACK_WAIT_UNITS_PER_S;
            sample = rtt >= 0.0 ? rtt : -1.0;
        }
    }
    for (uint32_t bit = 0; bit < COOP_ACK_MASK_BITS; bit++) {
        if (mask & (1U << bit)) {
            arrive(window, (uint16_t)(ack - 1U - bit));
        }
    }
    /* The peer's newest only moves forward, so only a reordered, older acknowledgement could
       still name a packet more than the mask behind it: calling it LOST errs on the safe side. */
    for (uint32_t age = 0; age < window->sent && has_newest(window); age++) {
        uint16_t number = (uint16_t)(window->next - 1U - age);
        uint16_t behind = (uint16_t)(window->newest_ack - number);
        if (window->fate[slot_of(number)] == COOP_PACKET_PENDING && behind > COOP_ACK_MASK_BITS && behind < 0x8000U) {
            window->fate[slot_of(number)] = COOP_PACKET_LOST;
        }
    }
    return sample;
}

float coop_send_window_loss(const coop_send_window_t *window) {
    uint32_t arrived = 0;
    uint32_t lost = 0;
    for (uint32_t age = 0; age < window->sent; age++) {
        uint8_t fate = window->fate[slot_of((uint16_t)(window->next - 1U - age))];
        arrived += fate == COOP_PACKET_ARRIVED ? 1U : 0U;
        lost += fate == COOP_PACKET_LOST ? 1U : 0U;
    }
    if (arrived + lost == 0) {
        return -1.0F;
    }
    return (float)lost / (float)(arrived + lost);
}

void coop_recv_window_reset(coop_recv_window_t *window) { memset(window, 0, sizeof(*window)); }

static void take_tick(coop_recv_window_t *window, uint32_t tick) {
    if ((int32_t)(tick - window->newest_tick) > 0) {
        window->newest_tick = tick;
    }
}

coop_recv_verdict_t coop_recv_window_accept(coop_recv_window_t *window, uint16_t number, uint32_t tick, double now_s) {
    if (!window->any) {
        window->any = true;
        window->newest = number;
        window->mask = 0;
        window->newest_time = now_s;
        window->newest_tick = tick;
        return COOP_RECV_NEW;
    }
    int32_t ahead = (int16_t)(uint16_t)(number - window->newest);
    /* Ahead by its number yet stamped before what was taken: a copy from more than 2^15
       packets ago, its number come round. */
    if (ahead > 0 && (int32_t)(tick - window->newest_tick) < 0) {
        return COOP_RECV_OLD;
    }
    if (ahead > 0) {
        uint64_t mask = (uint64_t)window->mask;
        mask = ahead < 64 ? (mask << ahead) | (1ULL << (ahead - 1)) : 0;
        window->mask = (uint32_t)mask;
        window->newest = number;
        window->newest_time = now_s;
        take_tick(window, tick);
        return COOP_RECV_NEW;
    }
    if (ahead == 0) {
        return COOP_RECV_AGAIN;
    }
    int32_t bit = -ahead - 1;
    if (bit >= COOP_ACK_MASK_BITS) {
        /* Stamped after everything taken: the sender's numbers came round while nothing
           reached this side, and without a restart every new packet would read as old. */
        if ((int32_t)(tick - window->newest_tick) > 0) {
            window->newest = number;
            window->mask = 0;
            window->newest_time = now_s;
            window->newest_tick = tick;
            return COOP_RECV_NEW;
        }
        return COOP_RECV_OLD;
    }
    if (window->mask & (1U << bit)) {
        return COOP_RECV_AGAIN;
    }
    window->mask |= 1U << bit;
    take_tick(window, tick);
    return COOP_RECV_NEW;
}

void coop_recv_window_ack(const coop_recv_window_t *window, double now_s, bool *has_ack, uint16_t *ack, uint32_t *mask, uint16_t *wait) {
    *has_ack = window->any;
    *ack = window->any ? window->newest : 0;
    *mask = window->any ? window->mask : 0;
    *wait = 0;
    if (window->any) {
        double units = floor((now_s - window->newest_time) * (double)COOP_WIRE_ACK_WAIT_UNITS_PER_S);
        if (!(units >= 0.0)) {
            units = 0.0;
        }
        *wait = units >= (double)COOP_WIRE_ACK_WAIT_TOO_LONG ? (uint16_t)COOP_WIRE_ACK_WAIT_TOO_LONG : (uint16_t)units;
    }
}

void coop_rtt_reset(coop_rtt_t *rtt) { memset(rtt, 0, sizeof(*rtt)); }

void coop_rtt_sample(coop_rtt_t *rtt, double sample_s) {
    if (!(sample_s >= 0.0) || !isfinite(sample_s)) {
        return;
    }
    if (rtt->samples == 0) {
        rtt->smoothed = sample_s;
        rtt->deviation = sample_s / 2.0;
    } else {
        rtt->deviation = 0.75 * rtt->deviation + 0.25 * fabs(rtt->smoothed - sample_s);
        rtt->smoothed = 0.875 * rtt->smoothed + 0.125 * sample_s;
    }
    if (rtt->samples < UINT32_MAX) {
        rtt->samples++;
    }
}
