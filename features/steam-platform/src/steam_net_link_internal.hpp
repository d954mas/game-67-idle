#ifndef FEATURES_STEAM_PLATFORM_STEAM_NET_LINK_INTERNAL_HPP
#define FEATURES_STEAM_PLATFORM_STEAM_NET_LINK_INTERNAL_HPP

/* Shared by the Steam link's C++ files: steam_net_link.cpp is one link (open, poll, send,
   close), steam_net_link_module.cpp is what the process has once (the links' registry, Steam's
   callbacks and the per-frame hook, the fake network), steam_net_link_closing.cpp the closes
   that outlive their peer. */

#include "steam_net_link_common.h"

#include "features/net_link/net_link_transport.h"

#include <steam/steam_api.h>

namespace steam_net_link {

/* Steam messages one link holds: those the last poll handed out plus those fetched for the
   next. Bounds a poll's batch; the rest waits in Steam. */
constexpr uint32_t HELD_MAX = 64;

enum class peer_state : uint8_t {
    free,
    waiting_auth, /* guest: Steam has no certificate yet, nothing dialled */
    connecting,
    live,
    ending, /* Steam says it ended; its last messages and DISCONNECTED wait for poll */
};

struct peer {
    peer_state state;
    /* k_HSteamNetConnection_Invalid once this side has closed it or the platform stopped. */
    HSteamNetConnection conn;
    net_link_peer_t handle;
    uint64_t identity;
    uint32_t reason; /* of the DISCONNECTED the ledger owes */
    SteamNetworkingMicroseconds last_reliable_us;
    steam_net_link_keepalive_allowance_t keepalives; /* what it may still send of them */
};

struct link {
    net_link_t base; /* first: net_link.h's calls hand this pointer back */
    bool host;
    bool orphaned; /* the platform stopped under it: no Steam call is valid any more */
    uint8_t keepalive_lane;
    uint32_t index;
    uint32_t serial;
    steam_net_link_settings_t settings;
    HSteamListenSocket listen;
    SteamNetworkingIPAddr local; /* host */
    HSteamNetPollGroup group;
    SteamNetworkingIPAddr remote; /* guest */
    SteamNetworkingMicroseconds connect_deadline_us;
    uint32_t next_handle;
    steam_net_link_ledger_t ledger;
    SteamNetworkingMessage_t *held[HELD_MAX];
    uint32_t held_count;
    SteamNetworkingMessage_t *carry[HELD_MAX];
    uint32_t carry_next;
    uint32_t carry_count;
    peer peers[STEAM_NET_LINK_PEERS_MAX];
};

/* steam_net_link_module.cpp */
bool module_attach(link *l);
void module_detach(link *l);

/* steam_net_link_closing.cpp: closes that outlive their peer. */
/* Takes over a connection net_link_close ended: no longer read, closed with `code` once Steam has
   everything acknowledged. `socket` is the host's listen socket it came through, or invalid. */
void closing_take(HSteamNetConnection conn, int code, int32_t timeout_ms, int32_t close_wait_ms, HSteamListenSocket socket, const SteamNetworkingIPAddr &local);
/* True when `conn` is a close in hand; a report that it ended frees it. */
bool closing_status(HSteamNetConnection conn, const SteamNetConnectionInfo_t &info);
void closing_frame(SteamNetworkingMicroseconds now);
/* Its link is destroyed: true when a close still needs `socket`, which is then closed by the
   last of them. */
bool closing_keeps(HSteamListenSocket socket);
/* A destroyed host's socket, kept for its closes: newcomers are turned away. */
bool closing_turns_away(HSteamListenSocket socket);
/* Hosting again on `local`: cuts the closes that keep a socket there; true when there were any. */
bool closing_take_port(const SteamNetworkingIPAddr &local);
bool closing_any();
void closing_forget();
/* Closes a listen socket. Steam frees its port a moment later, from its own thread: a bind there
   may fail until closing_port_due(), which is zero while no socket was closed lately. */
void closing_close_socket(HSteamListenSocket socket);
SteamNetworkingMicroseconds closing_port_due();

/* steam_net_link.cpp, called by the module */
void link_incoming(link *l, HSteamNetConnection conn, const SteamNetConnectionInfo_t &info);
void link_status(link *l, net_link_peer_t handle, HSteamNetConnection conn, const SteamNetConnectionInfo_t &info);
void link_frame(link *l, SteamNetworkingMicroseconds now);
void link_orphan(link *l);

} // namespace steam_net_link

#endif
