#ifndef FEATURES_STEAM_PLATFORM_TESTS_STEAM_NET_LINK_HARNESS_HPP
#define FEATURES_STEAM_PLATFORM_TESTS_STEAM_NET_LINK_HARNESS_HPP

/* What the in-process live tests of the Steam link share: an end that records its events, host
   and guest opens on this test's ports, real-time ticks, payload patterns, Steam's packet trace,
   raw Steam peers, and a case filter. */

#include "features/steam_platform/steam_net_link.h"
#include "steam_live.hpp"
#include "unity.h"

#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <steam/steam_api.h>

#include <string.h>
#include <string>
#include <vector>

namespace live {

/* This test's next port; leaving its block fails the case rather than dialling another test. */
inline uint16_t s_next_port;
inline uint16_t s_block_end;

inline void use_block(steam_live::block which) {
    s_next_port = steam_live::port_block(which);
    s_block_end = static_cast<uint16_t>(s_next_port + steam_live::BLOCK_SIZE);
}

inline uint16_t next_port() {
    if (s_next_port >= s_block_end) {
        TEST_FAIL_MESSAGE("this test ran out of its block of ports");
    }
    return s_next_port++;
}

struct got {
    net_link_event_type_t type;
    net_link_peer_t peer;
    uint32_t reason;
    uint8_t lane;
    bool reliable;
    std::vector<uint8_t> data;
};

struct side {
    net_link_t *link = nullptr;
    std::vector<got> log;
    uint16_t port = 0;

    void poll() {
        net_link_event_t events[32];
        uint32_t n;
        while (link != nullptr && (n = net_link_poll(link, events, 32)) > 0) {
            for (uint32_t i = 0; i < n; i++) {
                got g{events[i].type, events[i].peer, events[i].reason, events[i].lane, events[i].reliable, {}};
                if (events[i].type == NET_LINK_EVENT_MESSAGE) {
                    g.data.assign(events[i].data, events[i].data + events[i].size);
                }
                log.push_back(g);
            }
        }
    }
    const got *first(net_link_event_type_t type) const {
        for (const got &g : log) {
            if (g.type == type) {
                return &g;
            }
        }
        return nullptr;
    }
    size_t count(net_link_event_type_t type) const {
        size_t n = 0;
        for (const got &g : log) {
            n += g.type == type;
        }
        return n;
    }
    net_link_peer_t peer() const { return first(NET_LINK_EVENT_CONNECTED) ? first(NET_LINK_EVENT_CONNECTED)->peer : NET_LINK_PEER_NONE; }
    void destroy() {
        net_link_destroy(link);
        link = nullptr;
    }
};

inline steam_net_link_config_t config_of(uint8_t lanes, uint32_t max_peers) {
    steam_net_link_config_t config{};
    config.lanes.count = lanes;
    for (uint8_t lane = 0; lane < lanes; lane++) {
        config.lanes.priority[lane] = lane;
    }
    config.max_peers = max_peers;
    config.connect_timeout_s = 5.0F;
    return config;
}

inline side host_on_free_port(const steam_net_link_config_t &config) {
    side host;
    for (int attempt = 0; attempt < 64 && host.link == nullptr; attempt++) {
        host.port = next_port();
        host.link = steam_net_link_host_ip(&config, "127.0.0.1", host.port);
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(host.link, "no free port to host on");
    return host;
}

inline side join(const steam_net_link_config_t &config, uint16_t port) {
    side guest;
    guest.port = port;
    guest.link = steam_net_link_join_ip(&config, "127.0.0.1", port);
    TEST_ASSERT_NOT_NULL(guest.link);
    return guest;
}

inline void tick(std::initializer_list<side *> ends) {
    for (side *e : ends) {
        if (e->link) {
            net_link_flush(e->link);
        }
    }
    steam_live::pass(0.01);
    for (side *e : ends) {
        e->poll();
    }
}

template <typename Done> inline bool tick_until(std::initializer_list<side *> ends, double limit_s, Done done) {
    const double stop = steam_live::now_s() + limit_s;
    while (steam_live::now_s() < stop) {
        tick(ends);
        if (done()) {
            return true;
        }
    }
    return done();
}

inline void connect(side *host, side *guest) {
    TEST_ASSERT_TRUE_MESSAGE(tick_until({host, guest}, 10.0, [&] { return host->peer() && guest->peer(); }), "no connect");
}

inline std::vector<uint8_t> pattern(uint32_t size, uint32_t seed) {
    std::vector<uint8_t> bytes(size);
    for (uint32_t i = 0; i < size; i++) {
        bytes[i] = static_cast<uint8_t>(i * 7U + seed * 13U + (i >> 8));
    }
    return bytes;
}

// #region packet trace

/* Steam's packet trace goes through its debug output; this counts the UDP packets sent to one
   port and prints nothing (the lines carry full SteamIDs). */
inline uint16_t s_trace_port;
inline int s_trace_packets;

inline void on_debug(ESteamNetworkingSocketsDebugOutputType, const char *text) {
    char to[32];
    snprintf(to, sizeof to, "-> 127.0.0.1:%u ", static_cast<unsigned>(s_trace_port));
    if (s_trace_port != 0 && strstr(text, "[Trace Send]") != nullptr && strstr(text, to) != nullptr) {
        s_trace_packets++;
    }
}

inline void trace(uint16_t port) {
    s_trace_port = port;
    s_trace_packets = 0;
    SteamNetworkingUtils()->SetDebugOutputFunction(port ? k_ESteamNetworkingSocketsDebugOutputType_Msg : k_ESteamNetworkingSocketsDebugOutputType_None, port ? on_debug : nullptr);
    SteamNetworkingUtils()->SetGlobalConfigValueInt32(k_ESteamNetworkingConfig_PacketTraceMaxBytes, port ? 0 : -1);
}

// #endregion

// #region raw Steam peers

ISteamNetworkingSockets *raw() { return SteamNetworkingSockets(); }

inline HSteamNetConnection raw_connect(uint16_t port) {
    SteamNetworkingIPAddr address;
    address.Clear();
    address.SetIPv4(0x7f000001, port);
    return raw()->ConnectByIPAddress(address, 0, nullptr);
}

inline ESteamNetworkingConnectionState raw_state(HSteamNetConnection conn) {
    SteamNetConnectionInfo_t info;
    return raw()->GetConnectionInfo(conn, &info) ? info.m_eState : k_ESteamNetworkingConnectionState_None;
}

inline void raw_send(HSteamNetConnection conn, uint16_t lane, bool reliable, uint32_t size, uint8_t fill = 0x5A) {
    SteamNetworkingMessage_t *msg = SteamNetworkingUtils()->AllocateMessage(static_cast<int>(size));
    memset(msg->m_pData, fill, size);
    msg->m_conn = conn;
    msg->m_nFlags = reliable ? k_nSteamNetworkingSend_ReliableNoNagle : k_nSteamNetworkingSend_UnreliableNoNagle;
    msg->m_idxLane = lane;
    int64 result = 0;
    raw()->SendMessages(1, &msg, &result, true);
    TEST_ASSERT_TRUE_MESSAGE(result > 0, "the raw peer could not send");
}

inline int raw_end_reason(HSteamNetConnection conn) {
    SteamNetConnectionInfo_t info;
    return raw()->GetConnectionInfo(conn, &info) ? info.m_eEndReason : -1;
}

// #endregion

/* `<PREFIX>_ONLY` / `<PREFIX>_SKIP`: a part of the case names to run alone or to leave out. */
inline const char *s_filter_prefix = "STEAM_NET_LINK_PROBE";

inline void run_case(UnityTestFunction fn, const char *name, int line) {
    const std::string prefix = s_filter_prefix;
    const std::string only = steam_live::env((prefix + "_ONLY").c_str()), skip = steam_live::env((prefix + "_SKIP").c_str());
    if ((!only.empty() && !strstr(name, only.c_str())) || (!skip.empty() && strstr(name, skip.c_str()))) {
        return;
    }
    UnityDefaultTestRun(fn, name, line);
}

} // namespace live

#define RUN_CASE(fn) live::run_case(fn, #fn, __LINE__)

#endif
