/* One net_link over ISteamNetworkingSockets in IP mode: a host's listen socket or a guest's
   connection, the peers on it and the events poll owes for them. Steam's callbacks reach a link
   through steam_net_link_module.cpp. */

#include "features/steam_platform/steam_net_link.h"

#include "steam_net_link_closes.h"
#include "steam_net_link_internal.hpp"
#include "steam_platform_internal.h"

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>

#include <chrono>
#include <stdlib.h>
#include <string.h>
#include <thread>

static_assert(int{STEAM_NET_LINK_END_GENERIC} == int{k_ESteamNetConnectionEnd_App_Generic}, "close code");
static_assert(int{STEAM_NET_LINK_END_SHUTDOWN} == int{k_ESteamNetConnectionEnd_AppException_Max}, "close code");
static_assert(int{STEAM_NET_LINK_END_REFUSED} > int{k_ESteamNetConnectionEnd_App_Max} && int{STEAM_NET_LINK_END_VIOLATION} < int{k_ESteamNetConnectionEnd_AppException_Max}, "close code");
static_assert(int{STEAM_NET_LINK_END_REMOTE_TIMEOUT} == int{k_ESteamNetConnectionEnd_Remote_Timeout}, "close code");
static_assert(int{STEAM_NET_LINK_END_TIMEOUT} == int{k_ESteamNetConnectionEnd_Misc_Timeout}, "close code");
static_assert(int{NET_LINK_REASON_APP_FIRST} == int{k_ESteamNetConnectionEnd_App_Min} + 1 && int{NET_LINK_REASON_APP_LAST} == int{k_ESteamNetConnectionEnd_App_Max}, "app range");
static_assert(STEAM_NET_LINK_PEERS_MAX <= (1 << STEAM_NET_LINK_SLOT_BITS), "slot bits");
static_assert(STEAM_NET_LINK_LINKS_MAX <= 8, "link index bits");

namespace steam_net_link {
namespace {

ISteamNetworkingSockets *sockets() { return SteamNetworkingSockets(); }
ISteamNetworkingUtils *utils() { return SteamNetworkingUtils(); }
SteamNetworkingMicroseconds now_us() { return utils()->GetLocalTimestamp(); }

link *of(net_link_t *base) { return reinterpret_cast<link *>(base); }
const link *of(const net_link_t *base) { return reinterpret_cast<const link *>(base); }

uint32_t slot_of(const link *l, const peer *p) { return static_cast<uint32_t>(p - l->peers); }

peer *find(link *l, net_link_peer_t handle) {
    if (handle == NET_LINK_PEER_NONE) {
        return nullptr;
    }
    peer *p = &l->peers[steam_net_link_peer_slot(handle)];
    return p->state != peer_state::free && p->handle == handle ? p : nullptr;
}

const peer *find(const link *l, net_link_peer_t handle) { return find(const_cast<link *>(l), handle); }

int64 user_data(const link *l, net_link_peer_t handle) { return steam_net_link_user_data(l->index, l->serial, handle); }

/* Unread messages are Steam's to discard; a closed connection posts one more status change,
   which no longer matches this peer. */
void free_peer(link *l, peer *p) {
    steam_net_link_ledger_forget(&l->ledger, slot_of(l, p));
    *p = peer{};
}

peer fresh_peer(peer_state state, HSteamNetConnection conn, net_link_peer_t handle, uint64_t identity) {
    peer p{};
    p.state = state;
    p.conn = conn;
    p.handle = handle;
    p.identity = identity;
    steam_net_link_keepalive_allowance_start(&p.keepalives, now_us());
    return p;
}

net_link_event_t event_of(net_link_event_type_t type, const peer &p) {
    net_link_event_t event{};
    event.type = type;
    event.peer = p.handle;
    event.identity = p.identity;
    return event;
}

void close_conn(link *l, peer *p, int code, bool linger) {
    if (p->conn != k_HSteamNetConnection_Invalid && !l->orphaned) {
        sockets()->CloseConnection(p->conn, code, nullptr, linger);
    }
    p->conn = k_HSteamNetConnection_Invalid;
}

void release(SteamNetworkingMessage_t **messages, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        messages[i]->Release();
    }
}

void release_messages(link *l) {
    release(l->held, l->held_count);
    l->held_count = 0;
    release(l->carry + l->carry_next, l->carry_count - l->carry_next);
    l->carry_next = l->carry_count = 0;
}

/* Lanes are this side's sending order, so each end configures its own on each connection. */
bool configure_lanes(const link *l, HSteamNetConnection conn) {
    int priorities[NET_LINK_LANES_MAX];
    for (uint8_t lane = 0; lane < l->settings.lanes.count; lane++) {
        /* Steam also sends a lower number first. */
        priorities[lane] = l->settings.lanes.priority[lane];
    }
    return sockets()->ConfigureConnectionLanes(conn, l->settings.lanes.count, priorities, nullptr) == k_EResultOK;
}

void send_keepalive(const link *l, peer *p, SteamNetworkingMicroseconds now) {
    SteamNetworkingMessage_t *msg = utils()->AllocateMessage(0);
    if (msg != nullptr) {
        msg->m_conn = p->conn;
        msg->m_nFlags = k_nSteamNetworkingSend_ReliableNoNagle;
        msg->m_idxLane = l->keepalive_lane;
        int64 result = 0;
        sockets()->SendMessages(1, &msg, &result, true);
    }
    p->last_reliable_us = now;
}

void owe_disconnect(link *l, peer *p, uint32_t reason) {
    if (steam_net_link_ledger_owe_disconnected(&l->ledger, slot_of(l, p))) {
        p->reason = reason;
    }
    p->state = peer_state::ending;
}

/* Closes the connection from this side. A host's peer that was never announced leaves without
   an event; any other peer owes its DISCONNECTED, unless it already owes one. */
void end_peer(link *l, peer *p, int code, uint32_t reason) {
    close_conn(l, p, code, false);
    if (l->host && p->state == peer_state::connecting) {
        free_peer(l, p);
    } else if (p->state != peer_state::ending) {
        owe_disconnect(l, p, reason);
    }
}

/* The peer is connected from now on, by Steam's callback or by its first message, whichever
   this side sees first. */
bool make_live(link *l, peer *p) {
    if (p->state == peer_state::live) {
        return true;
    }
    SteamNetConnectionInfo_t info;
    if (!configure_lanes(l, p->conn) || !sockets()->GetConnectionInfo(p->conn, &info)) {
        /* With the transport's own code, so the other end reads TRANSPORT too. */
        end_peer(l, p, STEAM_NET_LINK_END_VIOLATION, NET_LINK_REASON_TRANSPORT);
        return false;
    }
    p->identity = info.m_identityRemote.GetSteamID64();
    p->state = peer_state::live;
    steam_net_link_ledger_owe_connected(&l->ledger, slot_of(l, p));
    p->last_reliable_us = now_us();
    return true;
}

uint32_t options(const link *l, int64 data, int32 connect_timeout_ms, SteamNetworkingConfigValue_t *out) {
    uint32_t n = 0;
    out[n++].SetInt64(k_ESteamNetworkingConfig_ConnectionUserData, data);
    out[n++].SetInt32(k_ESteamNetworkingConfig_TimeoutInitial, connect_timeout_ms);
    out[n++].SetInt32(k_ESteamNetworkingConfig_TimeoutConnected, l->settings.timeout_ms);
    out[n++].SetInt32(k_ESteamNetworkingConfig_SendBufferSize, static_cast<int32>(l->settings.send_buffer));
    /* Steam closes a peer that sends more; an unreliable message needs room too. */
    const uint32_t largest = l->settings.max_reliable > l->base.max_unreliable ? l->settings.max_reliable : l->base.max_unreliable;
    out[n++].SetInt32(k_ESteamNetworkingConfig_RecvMaxMessageSize, static_cast<int32>(largest));
    if (l->settings.local_without_proof) {
        out[n++].SetInt32(k_ESteamNetworkingConfig_IPLocalHost_AllowWithoutAuth, 1);
    }
    return n;
}
constexpr uint32_t OPTIONS_MAX = 6;

void dial(link *l) {
    peer *p = &l->peers[0];
    const SteamNetworkingMicroseconds left_us = l->connect_deadline_us - now_us();
    /* Steam's own connect timeout gets what the wait for a certificate left of it. */
    const int32 left_ms = left_us > 1000 ? static_cast<int32>(left_us / 1000) : 1;
    SteamNetworkingConfigValue_t opts[OPTIONS_MAX];
    const uint32_t n = options(l, user_data(l, p->handle), left_ms, opts);
    p->conn = sockets()->ConnectByIPAddress(l->remote, static_cast<int>(n), opts);
    if (p->conn == k_HSteamNetConnection_Invalid || !sockets()->SetConnectionPollGroup(p->conn, l->group)) {
        end_peer(l, p, STEAM_NET_LINK_END_GENERIC, NET_LINK_REASON_TRANSPORT);
        return;
    }
    p->state = peer_state::connecting;
}

bool parse_address(const char *text, uint16_t port, SteamNetworkingIPAddr *out) {
    out->Clear();
    if (text == nullptr) {
        out->m_port = port;
        return true;
    }
    if (steam_net_link_address_names_port(text) || !out->ParseString(text) || out->m_port != 0) {
        return false;
    }
    out->m_port = port;
    return true;
}

uint32_t unsplit_unreliable() {
    int32 size = 0;
    size_t cb = sizeof size;
    /* Steam reports this read-only value without filling in its type. */
    if (utils()->GetConfigValue(k_ESteamNetworkingConfig_MTU_DataSize, k_ESteamNetworkingConfig_Global, 0, nullptr, &size, &cb) != k_ESteamNetworkingGetConfigValue_OK
        || cb != sizeof size) {
        return 0;
    }
    return size > 0 ? static_cast<uint32_t>(size) : 0;
}

// #region poll

void emit(net_link_event_t *events, uint32_t *n, const net_link_event_t &event) { events[(*n)++] = event; }

/* One fetched message: dropped when its peer is gone, consumed when it is a keepalive, the peer
   closed when it breaks the rules or floods keepalives. A message that arrives before Steam's
   "connected" callback was read first makes its peer live and yields CONNECTED; the message
   itself comes next. */
void take_carried(link *l, net_link_event_t *events, uint32_t *n, SteamNetworkingMicroseconds now) {
    SteamNetworkingMessage_t *msg = l->carry[l->carry_next];
    uint32_t index = 0, serial = 0;
    net_link_peer_t handle = NET_LINK_PEER_NONE;
    peer *p = nullptr;
    if (steam_net_link_user_data_split(msg->m_nConnUserData, &index, &serial, &handle) && index == l->index && serial == l->serial) {
        p = find(l, handle);
    }
    if (p == nullptr || p->conn != msg->m_conn || p->conn == k_HSteamNetConnection_Invalid) {
        l->carry_next++;
        msg->Release();
        return;
    }
    const bool reliable = (msg->m_nFlags & k_nSteamNetworkingSend_Reliable) != 0;
    const steam_net_link_verdict_t verdict = steam_net_link_judge(msg->m_idxLane, l->base.lane_count, msg->m_cbSize, reliable, l->base.max_unreliable, l->base.max_reliable);
    if (verdict != STEAM_NET_LINK_ADMIT) {
        l->carry_next++;
        msg->Release();
        if (verdict == STEAM_NET_LINK_HOSTILE || !steam_net_link_keepalive_allowed(&p->keepalives, now)) {
            end_peer(l, p, STEAM_NET_LINK_END_VIOLATION, NET_LINK_REASON_TRANSPORT);
        }
        return;
    }
    if (p->state == peer_state::connecting && !make_live(l, p)) {
        return;
    }
    const uint32_t slot = slot_of(l, p);
    if (steam_net_link_ledger_owes_connected(&l->ledger, slot)) {
        steam_net_link_ledger_paid(&l->ledger, slot, NET_LINK_EVENT_CONNECTED);
        emit(events, n, event_of(NET_LINK_EVENT_CONNECTED, *p));
        return;
    }
    l->carry_next++;
    l->held[l->held_count++] = msg;
    net_link_event_t event = event_of(NET_LINK_EVENT_MESSAGE, *p);
    event.lane = static_cast<uint8_t>(msg->m_idxLane);
    event.reliable = reliable;
    event.data = static_cast<const uint8_t *>(msg->m_pData);
    event.size = static_cast<uint32_t>(msg->m_cbSize);
    emit(events, n, event);
}

bool fetch(link *l, uint32_t room, HSteamNetConnection conn) {
    if (room == 0 || l->orphaned) {
        return false;
    }
    const int got = conn != k_HSteamNetConnection_Invalid ? sockets()->ReceiveMessagesOnConnection(conn, l->carry, static_cast<int>(room))
                                                          : sockets()->ReceiveMessagesOnPollGroup(l->group, l->carry, static_cast<int>(room));
    l->carry_next = 0;
    l->carry_count = got > 0 ? static_cast<uint32_t>(got) : 0;
    return l->carry_count > 0;
}

/* Fetched messages go out first, then the events owed in their order, then what waits in the
   poll group. A peer's DISCONNECTED waits until its connection has nothing left to read.
   A consumed message yields no event and the fetching goes on past it, so a poll that
   returns zero left nothing waiting; each peer's keepalive allowance bounds that work. */
uint32_t link_poll(net_link_t *base, net_link_event_t *events, uint32_t capacity) {
    link *l = of(base);
    release(l->held, l->held_count);
    l->held_count = 0;
    uint32_t n = 0;
    const SteamNetworkingMicroseconds now = l->orphaned ? 0 : now_us();
    while (n < capacity) {
        if (l->carry_next < l->carry_count) {
            take_carried(l, events, &n, now);
            continue;
        }
        const uint32_t room = capacity - n < HELD_MAX - l->held_count ? capacity - n : HELD_MAX - l->held_count;
        uint32_t slot = 0;
        net_link_event_type_t type = NET_LINK_EVENT_CONNECTED;
        if (!steam_net_link_ledger_next(&l->ledger, &slot, &type)) {
            if (!fetch(l, room, k_HSteamNetConnection_Invalid)) {
                break;
            }
            continue;
        }
        peer *due = &l->peers[slot];
        if (type == NET_LINK_EVENT_CONNECTED) {
            steam_net_link_ledger_paid(&l->ledger, slot, type);
            emit(events, &n, event_of(NET_LINK_EVENT_CONNECTED, *due));
            continue;
        }
        if (due->conn != k_HSteamNetConnection_Invalid && fetch(l, room, due->conn)) {
            continue;
        }
        if (due->conn != k_HSteamNetConnection_Invalid && room == 0) {
            break;
        }
        net_link_event_t event = event_of(NET_LINK_EVENT_DISCONNECTED, *due);
        event.reason = due->reason;
        emit(events, &n, event);
        close_conn(l, due, STEAM_NET_LINK_END_GENERIC, false);
        free_peer(l, due);
    }
    return n;
}

// #endregion

// #region ops

net_link_send_result_t link_send(net_link_t *base, net_link_peer_t handle, uint8_t lane, uint32_t flags, const void *data, uint32_t size) {
    link *l = of(base);
    peer *p = l->orphaned ? nullptr : find(l, handle);
    if (p == nullptr || p->state != peer_state::live || p->conn == k_HSteamNetConnection_Invalid) {
        return NET_LINK_SEND_NO_PEER;
    }
    const bool reliable = (flags & NET_LINK_SEND_RELIABLE) != 0;
    SteamNetworkingMessage_t *msg = utils()->AllocateMessage(static_cast<int>(size));
    if (msg == nullptr) {
        return reliable ? NET_LINK_SEND_FULL : NET_LINK_SEND_DROPPED;
    }
    memcpy(msg->m_pData, data, size);
    msg->m_conn = p->conn;
    msg->m_nFlags = reliable ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;
    msg->m_idxLane = lane;
    int64 result = 0;
    sockets()->SendMessages(1, &msg, &result, true);
    if (result > 0) {
        if (reliable) {
            p->last_reliable_us = now_us();
        }
        return NET_LINK_SEND_OK;
    }
    /* No room in the send buffer is the peer not keeping up; anything else is a connection
       Steam already ended, whose callback this side has not read yet. */
    if (static_cast<EResult>(-result) == k_EResultLimitExceeded) {
        return reliable ? NET_LINK_SEND_FULL : NET_LINK_SEND_DROPPED;
    }
    return NET_LINK_SEND_NO_PEER;
}

void link_flush(net_link_t *base) {
    link *l = of(base);
    if (l->orphaned) {
        return;
    }
    for (const peer &p : l->peers) {
        if (p.state == peer_state::live && p.conn != k_HSteamNetConnection_Invalid) {
            sockets()->FlushMessagesOnConnection(p.conn);
        }
    }
}

void link_close(net_link_t *base, net_link_peer_t handle, uint32_t reason) {
    link *l = of(base);
    peer *p = find(l, handle);
    if (p == nullptr) {
        return;
    }
    if (p->state == peer_state::live && p->conn != k_HSteamNetConnection_Invalid && !l->orphaned) {
        closing_take(p->conn, steam_net_link_reason_to_steam(reason), l->settings.timeout_ms, l->settings.close_wait_ms, l->host ? l->listen : k_HSteamListenSocket_Invalid,
                     l->local);
        p->conn = k_HSteamNetConnection_Invalid;
    } else {
        close_conn(l, p, STEAM_NET_LINK_END_GENERIC, false);
    }
    free_peer(l, p);
}

void link_destroy(net_link_t *base) {
    link *l = of(base);
    if (!l->orphaned) {
        release_messages(l);
        for (peer &p : l->peers) {
            close_conn(l, &p, STEAM_NET_LINK_END_GENERIC, false);
        }
        sockets()->DestroyPollGroup(l->group);
        /* Closing the listen socket would cut a close still on it. */
        if (l->listen != k_HSteamListenSocket_Invalid && !closing_keeps(l->listen)) {
            closing_close_socket(l->listen);
        }
    }
    module_detach(l);
    free(l);
}

bool link_stats(const net_link_t *base, net_link_peer_t handle, net_link_stats_t *out) {
    const link *l = of(base);
    const peer *p = l->orphaned ? nullptr : find(l, handle);
    SteamNetConnectionRealTimeStatus_t status;
    if (p == nullptr || p->state != peer_state::live || p->conn == k_HSteamNetConnection_Invalid
        || sockets()->GetConnectionRealTimeStatus(p->conn, &status, 0, nullptr) != k_EResultOK) {
        return false;
    }
    out->ping_ms = status.m_nPing >= 0 ? static_cast<float>(status.m_nPing) : -1.0F;
    out->quality = status.m_flConnectionQualityLocal >= 0.0F ? (status.m_flConnectionQualityLocal < 1.0F ? status.m_flConnectionQualityLocal : 1.0F) : -1.0F;
    out->out_bytes_per_s = status.m_flOutBytesPerSec >= 0.0F ? status.m_flOutBytesPerSec : -1.0F;
    out->in_bytes_per_s = status.m_flInBytesPerSec >= 0.0F ? status.m_flInBytesPerSec : -1.0F;
    const int pending = (status.m_cbPendingReliable > 0 ? status.m_cbPendingReliable : 0) + (status.m_cbSentUnackedReliable > 0 ? status.m_cbSentUnackedReliable : 0);
    out->pending_reliable_bytes = static_cast<uint32_t>(pending);
    out->pending_unreliable_bytes = static_cast<uint32_t>(status.m_cbPendingUnreliable > 0 ? status.m_cbPendingUnreliable : 0);
    return true;
}

uint64_t link_identity(const net_link_t *base, net_link_peer_t handle) {
    const peer *p = find(of(base), handle);
    return p != nullptr && (p->state == peer_state::live || p->state == peer_state::ending) ? p->identity : 0;
}

const net_link_ops_t LINK_OPS = {link_send, link_poll, link_flush, link_close, link_destroy, link_stats, link_identity};

// #endregion

link *open(const steam_net_link_config_t *config, bool host) {
    steam_net_link_settings_t settings;
    if (!steam_net_link_settle(config, host, &settings) || !steam_platform_ready()) {
        return nullptr;
    }
    const uint32_t max_unreliable = unsplit_unreliable();
    if (max_unreliable == 0) {
        return nullptr;
    }
    link *l = static_cast<link *>(calloc(1, sizeof(link)));
    if (l == nullptr) {
        return nullptr;
    }
    l->host = host;
    l->settings = settings;
    l->keepalive_lane = steam_net_link_keepalive_lane(&settings.lanes);
    net_link_init(&l->base, &LINK_OPS, settings.lanes.count, max_unreliable, settings.max_reliable);
    if (!module_attach(l)) {
        free(l);
        return nullptr;
    }
    l->group = sockets()->CreatePollGroup();
    if (l->group == k_HSteamNetPollGroup_Invalid) {
        module_detach(l);
        free(l);
        return nullptr;
    }
    /* Starts fetching the certificate and Steam's keys now if this start has not yet. */
    sockets()->InitAuthentication();
    return l;
}

void drop(link *l) {
    sockets()->DestroyPollGroup(l->group);
    module_detach(l);
    free(l);
}

} // namespace

// #region calls from the module

void link_incoming(link *l, HSteamNetConnection conn, const SteamNetConnectionInfo_t &info) {
    uint32_t taken = 0;
    peer *slot = nullptr;
    for (peer &p : l->peers) {
        if (p.state != peer_state::free) {
            taken++;
        } else if (slot == nullptr) {
            slot = &p;
        }
    }
    if (taken >= l->settings.max_peers || slot == nullptr) {
        sockets()->CloseConnection(conn, STEAM_NET_LINK_END_REFUSED, "host full", false);
        return;
    }
    *slot = fresh_peer(peer_state::connecting, conn, steam_net_link_mint_peer(&l->next_handle, slot_of(l, slot)), info.m_identityRemote.GetSteamID64());
    /* User data and poll group before the accept: no message can arrive without them. */
    if (!sockets()->SetConnectionUserData(conn, user_data(l, slot->handle)) || !sockets()->SetConnectionPollGroup(conn, l->group)
        || sockets()->AcceptConnection(conn) != k_EResultOK) {
        sockets()->CloseConnection(conn, STEAM_NET_LINK_END_GENERIC, nullptr, false);
        free_peer(l, slot);
    }
}

void link_status(link *l, net_link_peer_t handle, HSteamNetConnection conn, const SteamNetConnectionInfo_t &info) {
    peer *p = find(l, handle);
    if (p == nullptr || p->conn != conn || p->state == peer_state::ending) {
        return;
    }
    switch (info.m_eState) {
    case k_ESteamNetworkingConnectionState_Connected:
        make_live(l, p);
        break;
    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
        const bool was_live = p->state == peer_state::live;
        if (l->host && !was_live) {
            /* Never announced, so it leaves without an event. */
            close_conn(l, p, STEAM_NET_LINK_END_GENERIC, false);
            free_peer(l, p);
            break;
        }
        owe_disconnect(l, p, steam_net_link_reason_from_steam(info.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer, info.m_eEndReason, was_live));
        break;
    }
    default:
        break;
    }
}

void link_frame(link *l, SteamNetworkingMicroseconds now) {
    peer *guest = &l->peers[0];
    if (!l->host && guest->state == peer_state::waiting_auth) {
        const ESteamNetworkingAvailability auth = sockets()->GetAuthenticationStatus(nullptr);
        const steam_net_link_certificate_t certificate = auth == k_ESteamNetworkingAvailability_Current ? STEAM_NET_LINK_CERTIFICATE_CURRENT
                                                         : auth == k_ESteamNetworkingAvailability_Failed || auth == k_ESteamNetworkingAvailability_CannotTry
                                                             ? STEAM_NET_LINK_CERTIFICATE_FAILED
                                                             : STEAM_NET_LINK_CERTIFICATE_PENDING;
        switch (steam_net_link_dial_decision(now, l->connect_deadline_us, certificate)) {
        case STEAM_NET_LINK_DIAL_NOW:
            dial(l);
            break;
        case STEAM_NET_LINK_DIAL_GIVE_UP:
            /* This side could not prove itself: not the same as nobody hosting. */
            owe_disconnect(l, guest, NET_LINK_REASON_TRANSPORT);
            break;
        case STEAM_NET_LINK_DIAL_WAIT:
            break;
        }
    }
    const int64_t every = steam_net_link_keepalive_us(l->settings.timeout_ms);
    for (peer &p : l->peers) {
        if (p.state == peer_state::live && p.conn != k_HSteamNetConnection_Invalid && now - p.last_reliable_us >= every) {
            send_keepalive(l, &p, now);
        }
    }
}

void link_orphan(link *l) {
    release_messages(l);
    l->orphaned = true;
    l->listen = k_HSteamListenSocket_Invalid;
    l->group = k_HSteamNetPollGroup_Invalid;
    for (peer &p : l->peers) {
        p.conn = k_HSteamNetConnection_Invalid;
        if (p.state == peer_state::free || p.state == peer_state::ending) {
            continue;
        }
        if (l->host && p.state == peer_state::connecting) {
            free_peer(l, &p);
            continue;
        }
        owe_disconnect(l, &p, NET_LINK_REASON_TRANSPORT);
    }
}

// #endregion

} // namespace steam_net_link

using namespace steam_net_link;

extern "C" {

net_link_t *steam_net_link_host_ip(const steam_net_link_config_t *config, const char *address, uint16_t port) {
    SteamNetworkingIPAddr local;
    if (port == 0 || !steam_platform_ready() || !parse_address(address, port, &local)) {
        return nullptr;
    }
    link *l = open(config, true);
    if (l == nullptr) {
        return nullptr;
    }
    l->local = local;
    SteamNetworkingConfigValue_t opts[OPTIONS_MAX];
    const uint32_t n = options(l, user_data(l, NET_LINK_PEER_NONE), l->settings.connect_timeout_ms, opts);
    /* A socket kept on this address for its closes goes now. Steam frees a port a moment after
       its socket closes, so a bind that follows one is retried until then. */
    closing_take_port(local);
    while ((l->listen = sockets()->CreateListenSocketIP(local, static_cast<int>(n), opts)) == k_HSteamListenSocket_Invalid && now_us() < closing_port_due()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (l->listen == k_HSteamListenSocket_Invalid) {
        drop(l);
        return nullptr;
    }
    return &l->base;
}

net_link_t *steam_net_link_join_ip(const steam_net_link_config_t *config, const char *address, uint16_t port) {
    SteamNetworkingIPAddr remote;
    if (port == 0 || address == nullptr || !steam_platform_ready() || !parse_address(address, port, &remote)) {
        return nullptr;
    }
    link *l = open(config, false);
    if (l == nullptr) {
        return nullptr;
    }
    l->remote = remote;
    l->connect_deadline_us = now_us() + static_cast<SteamNetworkingMicroseconds>(l->settings.connect_timeout_ms) * 1000;
    peer *p = &l->peers[0];
    *p = fresh_peer(peer_state::waiting_auth, k_HSteamNetConnection_Invalid, steam_net_link_mint_peer(&l->next_handle, 0), 0);
    /* A connect before Steam has its keys fails on the certificate. */
    if (l->settings.local_without_proof || sockets()->GetAuthenticationStatus(nullptr) == k_ESteamNetworkingAvailability_Current) {
        dial(l);
        if (p->state != peer_state::connecting) {
            drop(l);
            return nullptr;
        }
    }
    return &l->base;
}

} // extern "C"
