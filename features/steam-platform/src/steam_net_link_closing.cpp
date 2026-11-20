/* Performs the decisions of steam_net_link_closes.c on Steam: the table there says when a close
   in hand is closed and with which code, and when a kept listen socket goes; this file only
   reads Steam's status for it and makes the calls it asks for. */

#include "steam_net_link_closes.h"
#include "steam_net_link_internal.hpp"

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include <string.h>

static_assert(sizeof(SteamNetworkingIPAddr) <= STEAM_NET_LINK_ADDRESS_BYTES, "address key");
static_assert(sizeof(HSteamNetConnection) == sizeof(uint32_t) && sizeof(HSteamListenSocket) == sizeof(uint32_t), "opaque handles");

namespace steam_net_link {
namespace {

steam_net_link_closes_t s_closes;

/* Measured: a bind right after the close failed, one some milliseconds later succeeded. */
constexpr SteamNetworkingMicroseconds PORT_FREED_WITHIN_US = 250000;
SteamNetworkingMicroseconds s_port_due_us;

ISteamNetworkingSockets *sockets() { return SteamNetworkingSockets(); }
SteamNetworkingMicroseconds now_us() { return SteamNetworkingUtils()->GetLocalTimestamp(); }

steam_net_link_address_t key_of(const SteamNetworkingIPAddr &local) {
    steam_net_link_address_t key{};
    memcpy(key.bytes, &local, sizeof local);
    return key;
}

steam_net_link_close_probe_t probe(void *, uint32_t conn) {
    SteamNetConnectionRealTimeStatus_t status{};
    if (sockets()->GetConnectionRealTimeStatus(conn, &status, 0, nullptr) != k_EResultOK) {
        return {STEAM_NET_LINK_CLOSE_GONE, -1};
    }
    const bool acked = status.m_cbPendingReliable <= 0 && status.m_cbSentUnackedReliable <= 0 && status.m_cbPendingUnreliable <= 0;
    return {acked ? STEAM_NET_LINK_CLOSE_ACKED : STEAM_NET_LINK_CLOSE_PENDING, status.m_nPing};
}

void perform(const steam_net_link_close_actions_t &todo) {
    for (uint32_t i = 0; i < todo.count; i++) {
        const steam_net_link_close_action_t &a = todo.items[i];
        switch (a.what) {
        case STEAM_NET_LINK_DO_CLOSE:
            sockets()->CloseConnection(a.handle, a.code, nullptr, false);
            break;
        case STEAM_NET_LINK_DO_FREE:
            sockets()->CloseConnection(a.handle, 0, nullptr, false);
            break;
        case STEAM_NET_LINK_DO_CLOSE_SOCKET:
            closing_close_socket(a.handle);
            break;
        }
    }
}

} // namespace

void closing_take(HSteamNetConnection conn, int code, int32_t timeout_ms, int32_t close_wait_ms, HSteamListenSocket socket, const SteamNetworkingIPAddr &local) {
    /* Nothing more is read from it: its messages stay in Steam until the close discards them. */
    sockets()->SetConnectionPollGroup(conn, k_HSteamNetPollGroup_Invalid);
    sockets()->FlushMessagesOnConnection(conn);
    steam_net_link_close_actions_t todo{};
    const steam_net_link_address_t key = key_of(local);
    steam_net_link_closes_take(&s_closes, conn, code, timeout_ms, static_cast<int64_t>(close_wait_ms) * 1000, socket, &key, now_us(), &todo);
    perform(todo);
}

bool closing_status(HSteamNetConnection conn, const SteamNetConnectionInfo_t &info) {
    const bool ended = info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer || info.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally;
    for (const steam_net_link_close_entry_t &e : s_closes.entries) {
        if (e.used && !e.told && e.conn == conn) {
            if (ended) {
                steam_net_link_close_actions_t todo{};
                steam_net_link_closes_peer_left(&s_closes, conn, &todo);
                perform(todo);
            }
            return true;
        }
    }
    return false;
}

void closing_frame(SteamNetworkingMicroseconds now) {
    steam_net_link_close_actions_t todo{};
    steam_net_link_closes_frame(&s_closes, now, probe, nullptr, &todo);
    perform(todo);
}

bool closing_keeps(HSteamListenSocket socket) { return steam_net_link_closes_keep_socket(&s_closes, socket); }

bool closing_turns_away(HSteamListenSocket socket) { return steam_net_link_closes_turns_away(&s_closes, socket); }

bool closing_take_port(const SteamNetworkingIPAddr &local) {
    steam_net_link_close_actions_t todo{};
    const steam_net_link_address_t key = key_of(local);
    const bool took = steam_net_link_closes_take_port(&s_closes, &key, &todo);
    perform(todo);
    return took;
}

bool closing_any() { return steam_net_link_closes_any(&s_closes); }

void closing_forget() {
    steam_net_link_closes_forget(&s_closes);
    s_port_due_us = 0;
}

void closing_close_socket(HSteamListenSocket socket) {
    sockets()->CloseListenSocket(socket);
    s_port_due_us = now_us() + PORT_FREED_WITHIN_US;
}

SteamNetworkingMicroseconds closing_port_due() { return s_port_due_us; }

} // namespace steam_net_link
