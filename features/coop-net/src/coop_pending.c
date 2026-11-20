#include "coop_pending.h"

net_link_peer_t coop_pending_admit(coop_pending_t table[COOP_PENDING_MAX], net_link_peer_t peer, uint64_t identity, double now_s) {
    int chosen = -1;
    int oldest = -1;
    for (int slot = 0; slot < COOP_PENDING_MAX; slot++) {
        const coop_pending_t *entry = &table[slot];
        if (entry->peer == NET_LINK_PEER_NONE) {
            chosen = chosen < 0 ? slot : chosen;
            continue;
        }
        if (identity != 0 && entry->identity == identity) {
            chosen = slot;
            break;
        }
        if (oldest < 0 || entry->since_s < table[oldest].since_s) {
            oldest = slot;
        }
    }
    if (chosen < 0) {
        chosen = oldest;
    }
    net_link_peer_t pushed = table[chosen].peer;
    table[chosen] = (coop_pending_t){.peer = peer, .identity = identity, .since_s = now_s};
    return pushed;
}

int coop_pending_find(const coop_pending_t table[COOP_PENDING_MAX], net_link_peer_t peer) {
    for (int slot = 0; slot < COOP_PENDING_MAX; slot++) {
        if (peer != NET_LINK_PEER_NONE && table[slot].peer == peer) {
            return slot;
        }
    }
    return -1;
}
