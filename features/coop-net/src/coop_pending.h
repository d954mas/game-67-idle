#ifndef FEATURES_COOP_NET_COOP_PENDING_H
#define FEATURES_COOP_NET_COOP_PENDING_H

#include <stdint.h>

#include "features/net_link/net_link.h"

/* A host's peers that connected and have not sent a valid hello yet. One non-zero identity
   holds one entry, its newest connection; with every entry taken the oldest waiting peer gives
   way. Either way the peer pushed out is the caller's to close, so a crowd of silent
   connections never makes the host refuse a newcomer. */

enum {
    COOP_PENDING_MAX = 32,
};

typedef struct coop_pending {
    net_link_peer_t peer; /* NET_LINK_PEER_NONE: a free entry */
    uint64_t identity;
    double since_s;
} coop_pending_t;

/* Takes `peer` in and returns the peer it pushed out, or NET_LINK_PEER_NONE. */
net_link_peer_t coop_pending_admit(coop_pending_t table[COOP_PENDING_MAX], net_link_peer_t peer, uint64_t identity, double now_s);

/* The entry of `peer`, or -1. */
int coop_pending_find(const coop_pending_t table[COOP_PENDING_MAX], net_link_peer_t peer);

#endif
