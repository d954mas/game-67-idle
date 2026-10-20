#include "online_room/rtt.h"

#include <string.h>

// A move older than this when acknowledged came before a stall or a reconnect,
// not over the wire: no sample.
#define RTT_MAX_AGE_SECONDS 2.0
// Each sample moves the reading this share of the way: one slow trip nudges
// the line instead of making it jump.
#define RTT_SMOOTHING 0.2F

void online_rtt_reset(online_rtt *rtt) {
    memset(rtt, 0, sizeof *rtt);
    rtt->smooth_ms = -1.0F;
}

void online_rtt_sent(online_rtt *rtt, uint32_t seq, double now) {
    rtt->seqs[rtt->next] = seq;
    rtt->sent_at[rtt->next] = now;
    rtt->next = (rtt->next + 1U) % ONLINE_RTT_SENT;
}

void online_rtt_acked(online_rtt *rtt, uint32_t acked, double now, float held_seconds) {
    // Only a newer ack is a sample: an older one (a packet out of order) would
    // read an age the move no longer has. Sequence numbers wrap.
    if (acked == 0U || (rtt->acked != 0U && (int32_t)(acked - rtt->acked) <= 0)) { return; }
    for (uint32_t index = 0U; index < ONLINE_RTT_SENT; ++index) {
        if (rtt->seqs[index] != acked) { continue; }
        rtt->acked = acked;
        const double age = now - rtt->sent_at[index];
        // A wait longer than the move's age is a bad report, not a zero trip.
        if (held_seconds < 0.0F || age < (double)held_seconds || age > RTT_MAX_AGE_SECONDS) { return; }
        const float ms = (float)((age - (double)held_seconds) * 1000.0);
        rtt->smooth_ms = rtt->smooth_ms < 0.0F ? ms : rtt->smooth_ms + ((ms - rtt->smooth_ms) * RTT_SMOOTHING);
        return;
    }
}

float online_rtt_ms(const online_rtt *rtt) { return rtt->smooth_ms; }
