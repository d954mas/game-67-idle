#ifndef FEATURES_COOP_NET_COOP_ACKS_H
#define FEATURES_COOP_NET_COOP_ACKS_H

#include <stdbool.h>
#include <stdint.h>

#include "features/coop_net/coop_session.h"

/* Numbers, acknowledgements and fates of the state packets on one path, and the round trip
   they measure. Numbers are 16 bits and compared modulo 2^16: `a` is newer than `b` when
   (int16_t)(a - b) > 0. */

enum {
    COOP_ACK_MASK_BITS = 32, /* packets before `ack` that one acknowledgement names */
};

/* What this side sent: the fate of the last COOP_FATE_WINDOW packets. */
typedef struct coop_send_window {
    uint16_t next;        /* number of the next packet */
    uint32_t sent;        /* packets sent, saturating at the window */
    bool has_newest_ack;
    uint16_t newest_ack;  /* the newest packet the peer named as its newest received */
    double send_time[COOP_FATE_WINDOW];
    uint8_t fate[COOP_FATE_WINDOW]; /* coop_packet_fate_t */
} coop_send_window_t;

/* What this side received: the newest number and which of the 32 before it arrived. */
typedef struct coop_recv_window {
    bool any;
    uint16_t newest;
    uint32_t mask;      /* bit i: packet newest - 1 - i arrived */
    double newest_time; /* when `newest` was received */
    uint32_t newest_tick; /* the newest sender tick of any packet taken, by signed 32-bit difference */
} coop_recv_window_t;

/* RFC 6298 smoothing of round-trip samples, in seconds. */
typedef struct coop_rtt {
    uint32_t samples;
    double smoothed;
    double deviation;
} coop_rtt_t;

typedef enum coop_recv_verdict {
    COOP_RECV_NEW = 0,
    COOP_RECV_AGAIN, /* received before */
    COOP_RECV_OLD,   /* older than the window: cannot be placed */
} coop_recv_verdict_t;

void coop_send_window_reset(coop_send_window_t *window);
/* Records a packet leaving at `now_s` and returns its number. */
uint16_t coop_send_window_push(coop_send_window_t *window, double now_s);
coop_packet_fate_t coop_send_window_fate(const coop_send_window_t *window, uint16_t number);
/* Applies one acknowledgement. Returns the round-trip sample it gives, in seconds, or a
   negative value: only the first word for `ack` itself, with a wait that fit, gives one. An
   acknowledgement naming a packet this side never sent, or one past the window, is ignored. */
double coop_send_window_ack(coop_send_window_t *window, uint16_t ack, uint32_t mask, uint16_t wait, double now_s);
/* Share of resolved packets in the window that were lost; negative when none resolved. */
float coop_send_window_loss(const coop_send_window_t *window);

void coop_recv_window_reset(coop_recv_window_t *window);
/* `tick` is the sender's stamp: a packet too old to place by its number restarts the window
   only when it was stamped after every packet taken so far. A late copy carries an old stamp,
   so the network can never make a packet come twice; a sender back from a silence long enough
   for its numbers to come round carries newer stamps and is taken at once. */
coop_recv_verdict_t coop_recv_window_accept(coop_recv_window_t *window, uint16_t number, uint32_t tick, double now_s);
/* The acknowledgement fields of the next packet this side sends at `now_s`. */
void coop_recv_window_ack(const coop_recv_window_t *window, double now_s, bool *has_ack, uint16_t *ack, uint32_t *mask, uint16_t *wait);

void coop_rtt_reset(coop_rtt_t *rtt);
void coop_rtt_sample(coop_rtt_t *rtt, double sample_s);

#endif
