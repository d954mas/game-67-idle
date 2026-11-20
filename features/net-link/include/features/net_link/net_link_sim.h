#ifndef FEATURES_NET_LINK_NET_LINK_SIM_H
#define FEATURES_NET_LINK_NET_LINK_SIM_H

#include "features/net_link/net_link.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A network inside the process, for tests and stands: one host link, guest links, and
   between the host and each guest a path with its own conditions in each direction. With
   conditions left at zero it is a plain loopback.

   Time belongs to the caller: nothing moves until net_link_sim_advance. The same seed, the
   same calls and the same times give the same deliveries, so a failing run replays. Each
   direction of each path draws from its own stream: changing one guest's conditions or
   traffic does not change what happens to another's.

   All memory is taken in net_link_sim_create. A live Steam link has FakePacketLag and
   FakePacketLoss of its own, so this simulator wraps no other transport. */

typedef struct net_link_sim net_link_sim_t;

enum { NET_LINK_SIM_GUESTS_MAX = 16 };

/* What one direction of a path does to packets. Values out of range are clamped. */
typedef struct net_link_conditions {
    float delay_ms;  /* one way */
    float jitter_ms; /* each packet waits a further even draw from [0, jitter_ms] */
    float loss;      /* share of packets lost, 0..1 */
    float duplicate; /* share of unreliable packets that arrive twice */
    float reorder;   /* share of unreliable packets held back by reorder_ms, so later ones pass */
    float reorder_ms;
    /* A lost reliable packet is sent again after this long, and its lane waits for it.
       Zero: twice delay_ms plus 50. */
    float retransmit_ms;
    uint32_t rate_bytes_per_s; /* zero: no limit */
} net_link_conditions_t;

typedef struct net_link_sim_config {
    uint64_t seed;
    uint32_t max_guests; /* 1..NET_LINK_SIM_GUESTS_MAX */
    net_link_lanes_t lanes;
    /* Zero: 1200. Steam's limit for an unfragmented message is its MTU_DataSize, a little
       under its default packet size. */
    uint32_t max_unreliable_bytes;
    uint32_t max_reliable_bytes; /* zero: 16384 */
    /* Room for messages sent and not yet polled, per direction of a path and per kind.
       Past it an unreliable send is DROPPED and a reliable one is FULL. Zero: 262144. */
    uint32_t reliable_queue_bytes;
    uint32_t unreliable_queue_bytes;
    uint32_t queue_messages; /* per direction, both kinds together; zero: 2048 */
    /* A path cut for this long disconnects both ends with TIMEOUT. Zero: 10. */
    float timeout_s;
} net_link_sim_config_t;

/* NULL when the config is out of range or memory is short. */
net_link_sim_t *net_link_sim_create(const net_link_sim_config_t *config);

/* Releases the network and every link opened from it. */
void net_link_sim_destroy(net_link_sim_t *sim);

/* The host accepts up to `max_peers` guests (1..max_guests) and refuses the rest with
   REFUSED; a peer keeps its place until the host has polled its DISCONNECTED or closed it.
   NULL while another host link is open. A guest that connects while nobody hosts gets
   UNREACHABLE. */
net_link_t *net_link_sim_host(net_link_sim_t *sim, uint64_t identity, uint32_t max_peers);

/* Starts connecting; CONNECTED or DISCONNECTED follows on both ends after the path's delay.
   A disconnected guest link stays dead: open a new one to return. NULL when every guest
   place of the network is taken. net_link_destroy frees a place, except that a link closed
   before it keeps the place until its close has reached the host and the host polled it. */
net_link_t *net_link_sim_guest(net_link_sim_t *sim, uint64_t identity);

void net_link_sim_advance(net_link_sim_t *sim, double seconds);
double net_link_sim_time(const net_link_sim_t *sim);

typedef enum net_link_sim_direction {
    NET_LINK_SIM_TO_HOST = 0,
    NET_LINK_SIM_TO_GUEST = 1,
} net_link_sim_direction_t;

/* Applies to transmissions after the call; packets on the wire keep their fate, and one still
   waiting for the rate or for a retransmit takes the new conditions when it goes. `guest` is a
   link from net_link_sim_guest. */
void net_link_sim_set_conditions(net_link_sim_t *sim, const net_link_t *guest, net_link_sim_direction_t direction, const net_link_conditions_t *conditions);

/* A cut path passes nothing either way: unreliable packets are lost, reliable ones wait and
   arrive in order when the cut ends before the timeout. The cut stops transmissions; a packet
   already on the wire when it starts still arrives. */
void net_link_sim_set_cut(net_link_sim_t *sim, const net_link_t *guest, bool cut);

#ifdef __cplusplus
}
#endif

#endif
