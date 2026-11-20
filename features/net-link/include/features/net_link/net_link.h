#ifndef FEATURES_NET_LINK_NET_LINK_H
#define FEATURES_NET_LINK_NET_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A link is one endpoint of a message transport: a host that accepts peers, or a guest that
   reaches one host. It moves opaque bytes on numbered lanes; seats, sessions and game data
   live above it. The shape is ISteamNetworkingSockets', so a Steam link is a thin mapping.

   Each transport has its own way to open a link (net_link_sim.h is the in-process one) and
   every link is used through the calls below. All calls come from one thread, and a link
   never calls back: its owner polls. */

typedef struct net_link net_link_t;

/* One remote endpoint on one link. A handle dies with its DISCONNECTED event or with
   net_link_close, and a later peer never gets the same value: a guest that returns is a new
   peer with the same identity. */
typedef uint32_t net_link_peer_t;
#define NET_LINK_PEER_NONE 0U

enum { NET_LINK_LANES_MAX = 8 };

/* Reliable messages keep their order inside a lane and never wait for another lane. When the
   path is short of room, a lane with a lower priority number goes first; lanes with the same
   number share the path. */
typedef struct net_link_lanes {
    uint8_t count; /* 1..NET_LINK_LANES_MAX */
    uint8_t priority[NET_LINK_LANES_MAX];
} net_link_lanes_t;

enum {
    /* May be lost, doubled or overtaken, and is never larger than net_link_max_unreliable,
       the largest message the transport sends in one packet on its own: a message split
       over packets is lost with any one of them. A transport that packs several messages
       into packets may still split one that shares a flush with others. */
    NET_LINK_SEND_UNRELIABLE = 0U,
    /* Arrives once and in the order sent on its lane, or the peer disconnects. */
    NET_LINK_SEND_RELIABLE = 1U << 0,
};

typedef enum net_link_send_result {
    NET_LINK_SEND_OK = 0,
    NET_LINK_SEND_DROPPED,   /* unreliable and the path had no room: gone, as if lost */
    NET_LINK_SEND_FULL,      /* reliable and the queue is full: not taken, the peer is not keeping up */
    NET_LINK_SEND_TOO_LARGE, /* over the limit of its kind */
    NET_LINK_SEND_NO_PEER,   /* the handle is not a connected peer of this link */
    NET_LINK_SEND_INVALID,   /* lane out of range, no bytes, or an unknown flag */
} net_link_send_result_t;

/* Why a peer left. A session closes with a code of its own in the app range and the other
   side reads the same number. The range is Steam's without its generic code 1000, which a
   Steam link reads as REMOTE_CLOSE, so codes pass through unchanged. REFUSED travels as the
   transport's own code outside the app range. */
enum {
    NET_LINK_REASON_NONE = 0,
    NET_LINK_REASON_REMOTE_CLOSE = 1, /* the other side closed or shut its link down */
    NET_LINK_REASON_TIMEOUT = 2,      /* nothing crossed the path for the link's timeout */
    NET_LINK_REASON_REFUSED = 3,      /* the host had no free place */
    NET_LINK_REASON_UNREACHABLE = 4,  /* nobody is hosting there */
    NET_LINK_REASON_TRANSPORT = 5,    /* the transport failed in a way of its own */
    NET_LINK_REASON_APP_FIRST = 1001,
    NET_LINK_REASON_APP_LAST = 1999,
};

typedef enum net_link_event_type {
    NET_LINK_EVENT_CONNECTED = 1,
    NET_LINK_EVENT_DISCONNECTED,
    NET_LINK_EVENT_MESSAGE,
} net_link_event_type_t;

/* CONNECTED comes before any message of its peer and DISCONNECTED is the peer's last event;
   a guest whose connection fails sees only DISCONNECTED. The handle may be dead by the time a
   batch is read, so each event carries the peer's identity: zero when the other side never
   answered (UNREACHABLE, TIMEOUT while connecting). REFUSED carries the host's where the
   transport learns it and zero where a refusal names nobody. */
typedef struct net_link_event {
    net_link_event_type_t type;
    net_link_peer_t peer;
    uint64_t identity;   /* as net_link_identity */
    uint32_t reason;     /* DISCONNECTED */
    uint8_t lane;        /* MESSAGE */
    bool reliable;       /* MESSAGE */
    const uint8_t *data; /* MESSAGE: readable until the next net_link_poll of this link */
    uint32_t size;       /* MESSAGE */
} net_link_event_t;

/* Unknown values are negative: a transport may need traffic before it can measure. */
typedef struct net_link_stats {
    float ping_ms; /* round trip */
    float quality; /* share of packets that arrive, 0..1 */
    float out_bytes_per_s;
    float in_bytes_per_s;
    uint32_t pending_reliable_bytes; /* taken by send, not yet acknowledged or consumed by the peer */
    uint32_t pending_unreliable_bytes;
} net_link_stats_t;

net_link_send_result_t net_link_send(net_link_t *link, net_link_peer_t peer, uint8_t lane, uint32_t flags, const void *data, uint32_t size);

/* Writes up to `capacity` events in the order they arrived and returns how many. A link may
   return fewer while more wait (a transport with fixed memory caps its batch): poll until it
   returns zero. */
uint32_t net_link_poll(net_link_t *link, net_link_event_t *events, uint32_t capacity);

/* A link may hold what was sent until the flush; call it once at the end of the tick. */
void net_link_flush(net_link_t *link);

/* Ends a peer from this side and sends what is queued for it, as a flush would. Reliable
   messages sent before the call still reach it, then it sees DISCONNECTED with `reason` when
   that is an app code, REMOTE_CLOSE otherwise. The linger belongs to the connection: it holds
   when this side destroys its link right after. A transport bounds how long it lingers and
   says what must keep running meanwhile; past that bound the rest is lost, and the peer reads
   the reason all the same or, where the transport cannot deliver it then, one of its own.
   This side gets no event and nothing more from the peer. */
void net_link_close(net_link_t *link, net_link_peer_t peer, uint32_t reason);

/* Closes every peer with REMOTE_CLOSE and releases the link. For peers it was not asked to
   close with net_link_close it does not linger: what is still on its way may be lost. */
void net_link_destroy(net_link_t *link);

uint32_t net_link_max_unreliable(const net_link_t *link);
uint32_t net_link_max_reliable(const net_link_t *link);
uint8_t net_link_lane_count(const net_link_t *link);

/* False when `peer` is not connected. */
bool net_link_stats(const net_link_t *link, net_link_peer_t peer, net_link_stats_t *out);

/* Who the peer is beyond this connection: a SteamID on Steam, the number its link was opened
   with in the simulator. Zero when `peer` is not connected. */
uint64_t net_link_identity(const net_link_t *link, net_link_peer_t peer);

#ifdef __cplusplus
}
#endif

#endif
