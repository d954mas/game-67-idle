#ifndef ONLINE_ROOM_RTT_H
#define ONLINE_ROOM_RTT_H

// The round trip to the room, from the moves it already acknowledges: the
// packet names the newest move the room took (sync input_seq) and how long it
// sat in the room (sync input_wait); the move's age at that moment less that
// wait is what the wire costs. No packet of its own.

#include <stdbool.h>
#include <stdint.h>

#define ONLINE_RTT_SENT 64U

typedef struct online_rtt {
    uint32_t seqs[ONLINE_RTT_SENT];
    double sent_at[ONLINE_RTT_SENT]; // seconds, the caller's clock
    uint32_t next;                // the slot the next send overwrites
    uint32_t acked;               // the newest move sampled
    float smooth_ms;              // negative until the first sample
} online_rtt;

void online_rtt_reset(online_rtt *rtt);
// Move `seq` went on the wire at `now`.
void online_rtt_sent(online_rtt *rtt, uint32_t seq, double now);
// A packet at `now` says the room took move `acked` after holding it
// `held_seconds`; negative when the room does not know (no sample).
void online_rtt_acked(online_rtt *rtt, uint32_t acked, double now, float held_seconds);
// Milliseconds, smoothed; negative while there is no sample.
float online_rtt_ms(const online_rtt *rtt);

#endif
