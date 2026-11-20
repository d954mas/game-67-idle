#ifndef FEATURES_NET_LINK_NET_LINK_TRANSPORT_H
#define FEATURES_NET_LINK_NET_LINK_TRANSPORT_H

#include "features/net_link/net_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/* For the author of a transport. A transport embeds net_link_t as the first member of its
   own state, fills it with net_link_init and returns the pointer; users of a link include
   only net_link.h.

   The calls in net_link.h check what is the same for every transport before they reach an
   op: the lane, the flags, an empty message and the size limits on send, a null link, a zero
   capacity. An op still answers for peers: it is the only one who knows them. */

typedef struct net_link_ops {
    net_link_send_result_t (*send)(net_link_t *link, net_link_peer_t peer, uint8_t lane, uint32_t flags, const void *data, uint32_t size);
    uint32_t (*poll)(net_link_t *link, net_link_event_t *events, uint32_t capacity);
    void (*flush)(net_link_t *link);
    void (*close)(net_link_t *link, net_link_peer_t peer, uint32_t reason);
    void (*destroy)(net_link_t *link);
    bool (*stats)(const net_link_t *link, net_link_peer_t peer, net_link_stats_t *out);
    uint64_t (*identity)(const net_link_t *link, net_link_peer_t peer);
} net_link_ops_t;

struct net_link_capture;

struct net_link {
    const net_link_ops_t *ops;
    uint32_t max_unreliable;
    uint32_t max_reliable;
    uint8_t lane_count;
    /* Set through net_link_capture_attach; a transport never touches these. */
    struct net_link_capture *capture;
    uint8_t capture_tag;
};

void net_link_init(net_link_t *link, const net_link_ops_t *ops, uint8_t lane_count, uint32_t max_unreliable, uint32_t max_reliable);

#ifdef __cplusplus
}
#endif

#endif
