#ifndef FEATURES_STEAM_PLATFORM_STEAM_NET_LINK_COMMON_H
#define FEATURES_STEAM_PLATFORM_STEAM_NET_LINK_COMMON_H

#include "features/steam_platform/steam_net_link.h"

/* The Steam link's decisions that need no Steam: config, reasons both ways, peer handles, what
   may reach the owner, how long a closed host keeps its port. Pure C in both builds, so the
   rules are tested without a Steam client; steam_net_link.cpp checks these numbers against
   Steam's headers. */

#ifdef __cplusplus
extern "C" {
#endif

/* Close codes on the wire (ESteamNetConnectionEnd). An application may send 1000..2999; the
   two of this transport sit outside net-link's app range 1001..1999. */
enum {
    STEAM_NET_LINK_END_GENERIC = 1000,   /* k_ESteamNetConnectionEnd_App_Generic */
    STEAM_NET_LINK_END_REFUSED = 2001,   /* a full host */
    STEAM_NET_LINK_END_VIOLATION = 2002, /* the peer sent what net_link.h says cannot arrive */
    STEAM_NET_LINK_END_SHUTDOWN = 2999,  /* Steam closing every connection at its shutdown */
    STEAM_NET_LINK_END_REMOTE_TIMEOUT = 4001,
    STEAM_NET_LINK_END_TIMEOUT = 5003,
};

/* The code net_link_close sends for `reason`. */
int steam_net_link_reason_to_steam(uint32_t reason);

/* The net-link reason of a connection Steam reports ended: closed by the peer, or a problem
   this side found; `was_connected` tells a failed connect from a lost connection. */
uint32_t steam_net_link_reason_from_steam(bool by_peer, int end_reason, bool was_connected);

/* A config with its defaults applied. */
typedef struct steam_net_link_settings {
    net_link_lanes_t lanes;
    uint32_t max_peers; /* 1 for a guest */
    uint32_t max_reliable;
    uint32_t send_buffer;
    int32_t timeout_ms;
    int32_t connect_timeout_ms;
    int32_t close_wait_ms; /* how long a close waits for Steam's acknowledgement */
    bool local_without_proof;
} steam_net_link_settings_t;

/* False when the config is NULL or out of range; `out` is then untouched. */
bool steam_net_link_settle(const steam_net_link_config_t *config, bool host, steam_net_link_settings_t *out);

/* Microseconds between keepalives: a quarter of the timeout, so a silent peer is noticed
   within the timeout plus one keepalive's round. */
int64_t steam_net_link_keepalive_us(int32_t timeout_ms);

/* How long a close is kept after Steam was told: Steam sends the close once and does not send it
   again on a timer, it only answers the peer's next packet while the closed connection lives
   (answered after 1.5 s and 3 s, gone after 6 s). An idle peer's next packet is its keepalive,
   a quarter of its timeout; the other end is taken to have this end's. So: two keepalives, as
   either may be lost too, plus four round trips; 0.1..3 s for any input. */
int64_t steam_net_link_final_hold_us(int32_t timeout_ms, int32_t ping_ms);

/* The lane the keepalive goes on: the one sent first (lowest priority number, then lowest
   index), so no busier lane holds it back. */
uint8_t steam_net_link_keepalive_lane(const net_link_lanes_t *lanes);

/* True when address text names a port ("1.2.3.4:5", "[::1]:5"); the calls take the port apart. */
bool steam_net_link_address_names_port(const char *text);

enum { STEAM_NET_LINK_SLOT_BITS = 4 };

/* A new handle for `slot` from the link's counter: never NET_LINK_PEER_NONE, never repeated
   before 2^28 handles. */
net_link_peer_t steam_net_link_mint_peer(uint32_t *counter, uint32_t slot);
uint32_t steam_net_link_peer_slot(net_link_peer_t peer);

/* Steam's per-connection user data: which link (registry index and serial, so a reused index
   never matches an old connection) and which peer. Never -1, Steam's "not set". */
int64_t steam_net_link_user_data(uint32_t link_index, uint32_t link_serial, net_link_peer_t peer);
bool steam_net_link_user_data_split(int64_t user_data, uint32_t *link_index, uint32_t *link_serial, net_link_peer_t *peer);
enum { STEAM_NET_LINK_SERIAL_MAX = (1 << 23) - 1 };

/* The transport's keepalive is an empty reliable message: net_link_send refuses empty
   messages, so no app message is ever taken for one. The receiver takes it on any of net-link's
   lanes, also one it does not have itself: the sender's first lane need not be one of its own. (A lane of its own is not possible: Steam
   breaks a connection on a message on lane 8 or above, and an app may use lanes 0..7.) */
typedef enum steam_net_link_verdict {
    STEAM_NET_LINK_ADMIT,     /* hand to the owner */
    STEAM_NET_LINK_KEEPALIVE, /* the other end's keepalive: consume */
    STEAM_NET_LINK_HOSTILE,   /* cannot come from a net_link: close the peer with TRANSPORT */
} steam_net_link_verdict_t;

/* What a received message is, from its lane, size and kind. */
steam_net_link_verdict_t steam_net_link_judge(uint32_t lane, uint32_t lane_count, int64_t size, bool reliable, uint32_t max_unreliable, uint32_t max_reliable);

/* How many keepalives a peer may send. A keepalive yields no event, so nothing else bounds
   them. A peer of this transport sends one each quarter of its timeout, 16 a second with the
   shortest timeout, and they wait in Steam while the owner does not poll: so the allowance
   fills at that rate up to a burst worth over a minute of it. A peer that empties it floods. */
enum { STEAM_NET_LINK_KEEPALIVE_BURST = 1024, STEAM_NET_LINK_KEEPALIVE_PER_S = 16 };

typedef struct steam_net_link_keepalive_allowance {
    int64_t filled_us; /* up to when the rate is already counted in `left` */
    uint32_t left;
} steam_net_link_keepalive_allowance_t;

/* A peer starts with the whole burst. */
void steam_net_link_keepalive_allowance_start(steam_net_link_keepalive_allowance_t *allowance, int64_t now_us);

/* One keepalive read at `now_us`: false when the peer has sent more than it may. */
bool steam_net_link_keepalive_allowed(steam_net_link_keepalive_allowance_t *allowance, int64_t now_us);

/* The events poll owes, in the order they became owed, kept per peer slot. A slot owes at most
   a CONNECTED and a DISCONNECTED; its CONNECTED is always handed out first, and the first
   DISCONNECTED owed is the one kept. */
typedef struct steam_net_link_ledger {
    uint64_t next_stamp;
    uint64_t connected[STEAM_NET_LINK_PEERS_MAX];
    uint64_t disconnected[STEAM_NET_LINK_PEERS_MAX];
} steam_net_link_ledger_t;

void steam_net_link_ledger_owe_connected(steam_net_link_ledger_t *ledger, uint32_t slot);
/* False when the slot already owes a DISCONNECTED: the reason of that one stands. */
bool steam_net_link_ledger_owe_disconnected(steam_net_link_ledger_t *ledger, uint32_t slot);
bool steam_net_link_ledger_owes_connected(const steam_net_link_ledger_t *ledger, uint32_t slot);
/* The event owed longest; false when none is owed. */
bool steam_net_link_ledger_next(const steam_net_link_ledger_t *ledger, uint32_t *slot, net_link_event_type_t *type);
/* `type` was handed out; a DISCONNECTED ends what the slot owed. */
void steam_net_link_ledger_paid(steam_net_link_ledger_t *ledger, uint32_t slot, net_link_event_type_t type);
/* The slot was freed without its events, as by net_link_close. */
void steam_net_link_ledger_forget(steam_net_link_ledger_t *ledger, uint32_t slot);

/* Steam's fake-network values from the public shape: shares in percent, lag in whole ms. */
typedef struct steam_net_link_fake_values {
    float loss_pct;
    int32_t lag_ms;
    float reorder_pct;
    float duplicate_pct;
} steam_net_link_fake_values_t;

steam_net_link_fake_values_t steam_net_link_fake_values(const steam_net_link_fake_t *fake);
int32_t steam_net_link_reorder_ms(float reorder_ms);

#ifdef __cplusplus
}
#endif

#endif
