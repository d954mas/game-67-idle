/* What only the Steam link promises, on the developer's Steam client in one process over
   127.0.0.1: the unreliable limit sent alone by Steam's own packet trace, Steam's largest
   reliable message, a full send buffer, stats, the order of events against raw Steam peers,
   hostile peers, a flood of keepalives, timeouts, the keepalive lane, address text, idle
   traffic, the fake network across a stop, and a link that outlives the platform (the closes
   are steam_net_link_closes_live's). Exit 77 is a skip (1 with
   STEAM_PLATFORM_REQUIRE_CLIENT); --simulate-no-client takes that path on purpose.

   Steam's sender prints an assertion (steamnetworkingsockets_snp.cpp 2590) for the hostile
   raw peer's empty unreliable message: once when it is sent and once when Steam's shutdown
   frees that connection, which shows inside a later steam_platform_stop. Nothing the link
   itself sends causes it. */

#include "steam_net_link_harness.hpp"
#include "steam_platform_internal.h"

void setUp(void) { steam_live::case_began(); }
void tearDown(void) { steam_live::case_ended(Unity.CurrentTestName); }

namespace {

using namespace live;

void test_unreliable_at_the_limit_sent_alone_leaves_in_one_packet_and_one_byte_more_is_refused(void) {
    const steam_net_link_config_t config = config_of(NET_LINK_LANES_MAX, 1);
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    const uint32_t max_u = net_link_max_unreliable(guest.link);
    TEST_ASSERT_TRUE(max_u >= 1000 && max_u <= 1400);
    const std::vector<uint8_t> over = pattern(max_u + 1, 1);
    for (uint8_t lane = 0; lane < NET_LINK_LANES_MAX; lane++) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_TOO_LARGE, net_link_send(guest.link, guest.peer(), lane, NET_LINK_SEND_UNRELIABLE, over.data(), max_u + 1));
    }
    tick_until({&host, &guest}, 0.3, [] { return false; });
    /* The last lane costs the most bytes of framing. Each message alone in its flush: what
       net_link_max_unreliable promises. Packed behind other data of one flush, Steam may split
       it; that is stated, not tested here. */
    constexpr int COUNT = 20;
    const std::vector<uint8_t> limit = pattern(max_u, 2);
    trace(host.port);
    for (int i = 0; i < COUNT; i++) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(guest.link, guest.peer(), NET_LINK_LANES_MAX - 1, NET_LINK_SEND_UNRELIABLE, limit.data(), max_u));
        tick_until({&host, &guest}, 0.02, [] { return false; });
    }
    tick_until({&host, &guest}, 0.3, [] { return false; });
    const int packets = s_trace_packets;
    trace(0);
    printf("unreliable %u bytes on lane %d: %d messages, %d packets to the host\n", max_u, NET_LINK_LANES_MAX - 1, COUNT, packets);
    TEST_ASSERT_TRUE_MESSAGE(packets >= COUNT, "the packet trace saw fewer packets than messages");
    TEST_ASSERT_TRUE_MESSAGE(packets < COUNT * 3 / 2, "messages at net_link_max_unreliable sent alone were split");
    size_t whole = 0;
    for (const got &g : host.log) {
        whole += g.type == NET_LINK_EVENT_MESSAGE && !g.reliable && g.data == limit;
    }
    TEST_ASSERT_TRUE(whole >= 1);
    host.destroy();
    guest.destroy();
}

void test_steams_largest_reliable_message_arrives(void) {
    steam_net_link_config_t config = config_of(2, 1);
    config.max_reliable_bytes = 524288;
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    TEST_ASSERT_EQUAL_UINT32(524288, net_link_max_reliable(host.link));
    const std::vector<uint8_t> big = pattern(524288, 3);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(host.link, host.peer(), 1, NET_LINK_SEND_RELIABLE, big.data(), 524288));
    TEST_ASSERT_TRUE(tick_until({&host, &guest}, 15.0, [&] { return guest.count(NET_LINK_EVENT_MESSAGE) >= 1; }));
    const got *in = guest.first(NET_LINK_EVENT_MESSAGE);
    TEST_ASSERT_TRUE(in->reliable && in->lane == 1 && in->data == big);
    host.destroy();
    guest.destroy();
}

void test_a_full_send_buffer_answers_full_and_dropped_then_recovers(void) {
    steam_net_link_config_t config = config_of(1, 1);
    config.max_reliable_bytes = 4096;
    config.send_buffer_bytes = 16384;
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    const std::vector<uint8_t> chunk = pattern(4096, 4);
    uint32_t accepted = 0;
    net_link_send_result_t result = NET_LINK_SEND_OK;
    while (accepted < 1000 && (result = net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_RELIABLE, chunk.data(), 4096)) == NET_LINK_SEND_OK) {
        accepted++;
    }
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_FULL, result);
    TEST_ASSERT_TRUE(accepted >= 1 && accepted * 4096 <= 16384);
    /* Steam drains the buffer while this runs, so the first unreliable send may still fit. */
    bool dropped = false;
    for (int i = 0; i < 64 && !dropped; i++) {
        const net_link_send_result_t unreliable = net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_UNRELIABLE, chunk.data(), 1000);
        TEST_ASSERT_TRUE(unreliable == NET_LINK_SEND_OK || unreliable == NET_LINK_SEND_DROPPED);
        dropped = unreliable == NET_LINK_SEND_DROPPED;
    }
    TEST_ASSERT_TRUE_MESSAGE(dropped, "a full buffer never dropped an unreliable message");
    bool again = false;
    TEST_ASSERT_TRUE_MESSAGE(tick_until({&host, &guest}, 5.0, [&] {
        again = net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_RELIABLE, chunk.data(), 4096) == NET_LINK_SEND_OK;
        return again;
    }), "the buffer never drained");
    TEST_ASSERT_TRUE(tick_until({&host, &guest}, 5.0, [&] { return guest.count(NET_LINK_EVENT_MESSAGE) >= accepted + 1; }));
    TEST_ASSERT_NULL(guest.first(NET_LINK_EVENT_DISCONNECTED));
    host.destroy();
    guest.destroy();
}

void test_stats_become_measured_after_traffic(void) {
    const steam_net_link_config_t config = config_of(1, 1);
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    const std::vector<uint8_t> small = pattern(64, 5);
    auto measured = [&](const side &e) {
        net_link_stats_t s;
        return net_link_stats(e.link, e.peer(), &s) && s.ping_ms >= 0.0F && s.quality >= 0.0F && s.quality <= 1.0F && s.out_bytes_per_s >= 0.0F && s.in_bytes_per_s >= 0.0F;
    };
    const bool done = tick_until({&host, &guest}, 15.0, [&] {
        net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_RELIABLE, small.data(), 64);
        net_link_send(guest.link, guest.peer(), 0, NET_LINK_SEND_UNRELIABLE, small.data(), 64);
        return measured(host) && measured(guest);
    });
    TEST_ASSERT_TRUE_MESSAGE(done, "stats stayed unmeasured");
    host.destroy();
    guest.destroy();
}

// #region raw Steam peers

struct hostile_case {
    const char *what;
    uint16_t lane;
    bool reliable;
    uint32_t size;
    uint8_t fill;
    int raw_end; /* what the raw peer reads: the link's rule-break code, or Steam's own */
};


constexpr int LINK_VIOLATION = 2002;
constexpr int STEAM_OWN_LIMIT = k_ESteamNetConnectionEnd_Misc_InternalError;

/* One lane past what net_link allows, so the raw peer can send on any lane and one Steam
   itself cannot carry. */
constexpr int RAW_LANES = NET_LINK_LANES_MAX + 1;

void raw_lanes(HSteamNetConnection conn) {
    int priorities[RAW_LANES] = {};
    TEST_ASSERT_EQUAL_INT(k_EResultOK, raw()->ConfigureConnectionLanes(conn, RAW_LANES, priorities, nullptr));
}

void test_a_raw_peer_that_breaks_the_rules_is_closed_with_transport(void) {
    steam_net_link_config_t config = config_of(1, 1);
    config.max_reliable_bytes = 4096;
    side host = host_on_free_port(config);
    const uint32_t max_u = net_link_max_unreliable(host.link);
    const hostile_case cases[] = {
        {"a lane the link does not have", 2, true, 8, 0x5A, LINK_VIOLATION},
        {"the last lane net_link allows, unreliable", NET_LINK_LANES_MAX - 1, false, 8, 0x5A, LINK_VIOLATION},
        {"a lane past net_link's (Steam itself cuts it)", NET_LINK_LANES_MAX, true, 8, 0x5A, STEAM_OWN_LIMIT},
        /* Steam's own sender prints an assertion for this one; it is the raw peer's complaint. */
        {"an empty unreliable message", 0, false, 0, 0, LINK_VIOLATION},
        /* Steam's RecvMaxMessageSize, set from max_reliable_bytes, cuts this before the link sees it. */
        {"a reliable message over max_reliable", 0, true, 4097, 0x5A, STEAM_OWN_LIMIT},
        {"an unreliable message over max_unreliable", 0, false, max_u + 100, 0x5A, LINK_VIOLATION},
    };
    for (const hostile_case &c : cases) {
        host.log.clear();
        const HSteamNetConnection conn = raw_connect(host.port);
        TEST_ASSERT_TRUE(tick_until({&host}, 10.0, [&] { return host.peer() != NET_LINK_PEER_NONE && raw_state(conn) == k_ESteamNetworkingConnectionState_Connected; }));
        raw_lanes(conn);
        /* A keepalive, then a good message: neither closes the peer, only the message is seen. */
        raw_send(conn, 0, true, 0);
        raw_send(conn, 0, true, 8);
        TEST_ASSERT_TRUE(tick_until({&host}, 5.0, [&] { return host.count(NET_LINK_EVENT_MESSAGE) == 1; }));
        raw_send(conn, c.lane, c.reliable, c.size, c.fill);
        const bool closed = tick_until({&host}, 5.0, [&] { return host.first(NET_LINK_EVENT_DISCONNECTED) != nullptr; });
        printf("hostile: %s -> %s\n", c.what, closed ? "closed" : "NOT closed");
        TEST_ASSERT_TRUE_MESSAGE(closed, c.what);
        TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TRANSPORT, host.first(NET_LINK_EVENT_DISCONNECTED)->reason);
        TEST_ASSERT_EQUAL_size_t(1, host.count(NET_LINK_EVENT_MESSAGE));
        TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, host.log.back().type);
        tick_until({&host}, 2.0, [&] { return raw_state(conn) != k_ESteamNetworkingConnectionState_Connected; });
        printf("hostile: the raw peer read end reason %d\n", raw_end_reason(conn));
        TEST_ASSERT_EQUAL_INT_MESSAGE(c.raw_end, raw_end_reason(conn), c.what);
        raw()->CloseConnection(conn, 0, nullptr, false);
    }
    host.destroy();
}

/* A raw host: accepts whatever knocks on its socket, and may send at once from inside the
   callback that accepts. */
HSteamListenSocket s_raw_listen;
HSteamNetConnection s_raw_accepted;
uint32_t s_raw_send_on_accept;
constexpr int STATUS_ID = SteamNetConnectionStatusChangedCallback_t::k_iCallback;

void on_raw_status(void *, int, const void *data, size_t size) {
    if (size < sizeof(SteamNetConnectionStatusChangedCallback_t)) {
        return;
    }
    const auto *change = static_cast<const SteamNetConnectionStatusChangedCallback_t *>(data);
    if (change->m_info.m_hListenSocket == s_raw_listen && change->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting) {
        s_raw_accepted = change->m_hConn;
        raw()->AcceptConnection(change->m_hConn);
        for (uint32_t seq = 0; seq < s_raw_send_on_accept; seq++) {
            raw()->SendMessageToConnection(change->m_hConn, &seq, sizeof seq, k_nSteamNetworkingSend_ReliableNoNagle, nullptr);
        }
    }
}

uint16_t raw_host_open(uint32_t send_on_accept) {
    s_raw_accepted = k_HSteamNetConnection_Invalid;
    s_raw_send_on_accept = send_on_accept;
    TEST_ASSERT_TRUE(steam_platform_internal_listen(STATUS_ID, on_raw_status, nullptr));
    SteamNetworkingIPAddr local;
    local.Clear();
    uint16_t port = 0;
    for (int attempt = 0; attempt < 64 && s_raw_listen == k_HSteamListenSocket_Invalid; attempt++) {
        port = next_port();
        local.SetIPv4(0x7f000001, port);
        s_raw_listen = raw()->CreateListenSocketIP(local, 0, nullptr);
    }
    TEST_ASSERT_NOT_EQUAL_UINT32(k_HSteamListenSocket_Invalid, s_raw_listen);
    return port;
}

void raw_host_close() {
    raw()->CloseConnection(s_raw_accepted, 0, nullptr, false);
    raw()->CloseListenSocket(s_raw_listen);
    s_raw_listen = k_HSteamListenSocket_Invalid;
    steam_platform_internal_unlisten(STATUS_ID, on_raw_status, nullptr);
}

void test_a_raw_host_that_breaks_the_rules_is_closed_with_transport(void) {
    const uint16_t port = raw_host_open(0);
    side guest = join(config_of(1, 1), port);
    TEST_ASSERT_TRUE(tick_until({&guest}, 10.0, [&] { return guest.peer() != NET_LINK_PEER_NONE; }));
    raw_lanes(s_raw_accepted);
    raw_send(s_raw_accepted, 3, false, 16);
    TEST_ASSERT_TRUE(tick_until({&guest}, 5.0, [&] { return guest.first(NET_LINK_EVENT_DISCONNECTED) != nullptr; }));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TRANSPORT, guest.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_EQUAL_size_t(0, guest.count(NET_LINK_EVENT_MESSAGE));
    guest.destroy();
    raw_host_close();
}

/* The host's messages reach the guest's Steam before the guest's "connected" callback is read:
   the guest polls without a frame and must still see CONNECTED first. */
void test_a_guest_reads_connected_before_messages_that_overtake_its_callback(void) {
    constexpr uint32_t COUNT = 5;
    const uint16_t port = raw_host_open(COUNT);
    side guest = join(config_of(1, 1), port);
    TEST_ASSERT_TRUE(steam_live::pass_until(10.0, [] { return s_raw_accepted != k_HSteamNetConnection_Invalid; }));
    /* No frame from here: the messages land, the callback waits in Steam's queue. */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    guest.poll();
    const size_t first_poll = guest.log.size();
    TEST_ASSERT_TRUE(tick_until({&guest}, 5.0, [&] { return guest.count(NET_LINK_EVENT_MESSAGE) >= COUNT; }));
    printf("ordering: the frameless poll gave %u events\n", static_cast<unsigned>(first_poll));
    TEST_ASSERT_TRUE(first_poll >= 2);
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_CONNECTED, guest.log[0].type);
    TEST_ASSERT_EQUAL_size_t(1, guest.count(NET_LINK_EVENT_CONNECTED));
    for (size_t i = 1; i <= COUNT; i++) {
        uint32_t seq = 0;
        TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_MESSAGE, guest.log[i].type);
        memcpy(&seq, guest.log[i].data.data(), sizeof seq);
        TEST_ASSERT_EQUAL_UINT32(i - 1, seq);
    }
    guest.destroy();
    raw_host_close();
}

/* The guest's close reaches the host's Steam while the host does not poll: its messages are
   still read before its DISCONNECTED. */
void test_a_peers_last_messages_come_before_its_disconnected_when_the_host_polls_late(void) {
    constexpr uint32_t COUNT = 50;
    constexpr int CODE = NET_LINK_REASON_APP_FIRST + 554;
    side host = host_on_free_port(config_of(1, 1));
    const HSteamNetConnection conn = raw_connect(host.port);
    TEST_ASSERT_TRUE(tick_until({&host}, 10.0, [&] { return host.peer() != NET_LINK_PEER_NONE && raw_state(conn) == k_ESteamNetworkingConnectionState_Connected; }));
    host.log.clear();
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        raw()->SendMessageToConnection(conn, &seq, sizeof seq, k_nSteamNetworkingSend_Reliable, nullptr);
    }
    raw()->CloseConnection(conn, CODE, "done", true);
    /* Frames without a poll: Steam reports the close while the messages wait. */
    steam_live::pass_until(1.0, [] { return false; });
    host.poll();
    TEST_ASSERT_EQUAL_size_t(COUNT + 1, host.log.size());
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        uint32_t got = 0;
        TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_MESSAGE, host.log[seq].type);
        memcpy(&got, host.log[seq].data.data(), sizeof got);
        TEST_ASSERT_EQUAL_UINT32(seq, got);
    }
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, host.log.back().type);
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(CODE), host.log.back().reason);
    host.destroy();
}

/* Keepalives yield no event, so only the peer's allowance bounds them. A burst an honest peer
   could have queued while the host did not poll is consumed by one poll, which returns the
   message behind it; a peer that goes on sending them is cut as breaking the rules. */
void test_a_burst_of_keepalives_is_consumed_and_a_flood_of_them_is_cut(void) {
    constexpr uint32_t BURST = 600;  /* inside the allowance */
    constexpr uint32_t FLOOD = 3000; /* well past it */
    side host = host_on_free_port(config_of(1, 1));
    const HSteamNetConnection conn = raw_connect(host.port);
    TEST_ASSERT_TRUE(tick_until({&host}, 10.0, [&] { return host.peer() != NET_LINK_PEER_NONE && raw_state(conn) == k_ESteamNetworkingConnectionState_Connected; }));
    host.log.clear();
    const uint32_t marker = 0xC0FFEE;
    for (uint32_t i = 0; i < BURST; i++) {
        TEST_ASSERT_EQUAL_INT(k_EResultOK, raw()->SendMessageToConnection(conn, &marker, 0, k_nSteamNetworkingSend_Reliable, nullptr));
    }
    TEST_ASSERT_EQUAL_INT(k_EResultOK, raw()->SendMessageToConnection(conn, &marker, sizeof marker, k_nSteamNetworkingSend_Reliable, nullptr));
    /* Frames without a poll, so the whole burst waits in Steam. */
    steam_live::pass_until(1.0, [] { return false; });
    net_link_event_t events[8];
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1, net_link_poll(host.link, events, 8), "a poll returned before the message behind the keepalives");
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_MESSAGE, events[0].type);
    TEST_ASSERT_EQUAL_UINT32(sizeof marker, events[0].size);
    TEST_ASSERT_EQUAL_MEMORY(&marker, events[0].data, sizeof marker);
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(host.link, events, 8));
    TEST_ASSERT_EQUAL_INT(k_ESteamNetworkingConnectionState_Connected, raw_state(conn));
    for (uint32_t i = 0; i < FLOOD; i++) {
        raw()->SendMessageToConnection(conn, &marker, 0, k_nSteamNetworkingSend_Reliable, nullptr);
    }
    const double began = steam_live::now_s();
    TEST_ASSERT_TRUE_MESSAGE(tick_until({&host}, 10.0, [&] { return host.first(NET_LINK_EVENT_DISCONNECTED) != nullptr; }), "a flood of keepalives was never cut");
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TRANSPORT, host.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    tick_until({&host}, 2.0, [&] { return raw_state(conn) != k_ESteamNetworkingConnectionState_Connected; });
    printf("flood: %u keepalives were consumed in one poll; %u more were cut after %.2f s, the raw peer read %d\n", BURST, FLOOD, steam_live::now_s() - began,
           raw_end_reason(conn));
    TEST_ASSERT_EQUAL_INT(LINK_VIOLATION, raw_end_reason(conn));
    raw()->CloseConnection(conn, 0, nullptr, false);
    host.destroy();
}

// #endregion

void test_silence_times_out_within_the_configured_time(void) {
    steam_net_link_config_t config = config_of(1, 1);
    config.timeout_s = 1.5F;
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    /* Idle on purpose: without keepalives Steam would notice only after its own ten seconds. */
    tick_until({&host, &guest}, 1.0, [] { return false; });
    const steam_net_link_fake_t all{1.0F, 0.0F, 0.0F, 0.0F};
    steam_net_link_set_fake(&all, &all, 0.0F);
    const double cut = steam_live::now_s();
    double host_left = -1.0, guest_left = -1.0;
    tick_until({&host, &guest}, 10.0, [&] {
        if (host_left < 0 && host.first(NET_LINK_EVENT_DISCONNECTED)) {
            host_left = steam_live::now_s() - cut;
        }
        if (guest_left < 0 && guest.first(NET_LINK_EVENT_DISCONNECTED)) {
            guest_left = steam_live::now_s() - cut;
        }
        return host_left >= 0 && guest_left >= 0;
    });
    steam_net_link_set_fake(nullptr, nullptr, 0.0F);
    printf("timeout 1.5 s: host left after %.2f s, guest after %.2f s\n", host_left, guest_left);
    TEST_ASSERT_TRUE(host_left >= 1.0 && host_left <= 3.0);
    TEST_ASSERT_TRUE(guest_left >= 1.0 && guest_left <= 3.0);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, host.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, guest.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    host.destroy();
    guest.destroy();
}

void test_a_link_that_outlives_the_stop_reports_transport_once_and_stays_inert(void) {
    const steam_net_link_config_t config = config_of(2, 2);
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    const std::vector<uint8_t> payload = pattern(32, 6);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(guest.link, guest.peer(), 1, NET_LINK_SEND_RELIABLE, payload.data(), 32));
    tick_until({&host, &guest}, 1.0, [&] { return host.count(NET_LINK_EVENT_MESSAGE) >= 1; });
    const net_link_peer_t to_guest = host.peer(), to_host = guest.peer();
    steam_platform_stop();
    host.poll();
    guest.poll();
    host.poll();
    guest.poll();
    TEST_ASSERT_EQUAL_size_t(1, host.count(NET_LINK_EVENT_DISCONNECTED));
    TEST_ASSERT_EQUAL_size_t(1, guest.count(NET_LINK_EVENT_DISCONNECTED));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TRANSPORT, host.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TRANSPORT, guest.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_EQUAL_UINT32(to_guest, host.first(NET_LINK_EVENT_DISCONNECTED)->peer);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, net_link_send(host.link, to_guest, 0, NET_LINK_SEND_RELIABLE, payload.data(), 32));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, net_link_send(guest.link, to_host, 0, NET_LINK_SEND_UNRELIABLE, payload.data(), 32));
    net_link_stats_t stats;
    TEST_ASSERT_FALSE(net_link_stats(host.link, to_guest, &stats));
    TEST_ASSERT_EQUAL_UINT64(0, net_link_identity(host.link, to_guest));
    net_link_flush(host.link);
    net_link_close(guest.link, to_host, NET_LINK_REASON_APP_FIRST);
    host.destroy();
    guest.destroy();
    /* A new start serves new links. */
    TEST_ASSERT_EQUAL_INT(0, steam_live::start());
    side again = host_on_free_port(config);
    side back = join(config, again.port);
    connect(&again, &back);
    again.destroy();
    back.destroy();
}

/* The keepalive goes on the sender's first lane, which may be a lane the receiver lacks. */
void test_the_keepalive_rides_the_first_lane_even_one_the_other_end_lacks(void) {
    steam_net_link_config_t three = config_of(3, 1);
    three.lanes.priority[0] = 1;
    three.lanes.priority[1] = 1;
    three.lanes.priority[2] = 0;
    three.timeout_s = 1.0F;
    side host = host_on_free_port(three);
    const HSteamNetConnection conn = raw_connect(host.port);
    TEST_ASSERT_TRUE(tick_until({&host}, 10.0, [&] { return host.peer() != NET_LINK_PEER_NONE && raw_state(conn) == k_ESteamNetworkingConnectionState_Connected; }));
    int lane_seen = -1;
    TEST_ASSERT_TRUE_MESSAGE(tick_until({&host}, 3.0, [&] {
        SteamNetworkingMessage_t *in[16];
        const int n = raw()->ReceiveMessagesOnConnection(conn, in, 16);
        for (int i = 0; i < n; i++) {
            if (in[i]->m_cbSize == 0 && (in[i]->m_nFlags & k_nSteamNetworkingSend_Reliable) != 0) {
                lane_seen = in[i]->m_idxLane;
            }
            in[i]->Release();
        }
        return lane_seen >= 0;
    }), "no keepalive reached the raw peer");
    printf("keepalive: arrived on lane %d (the host's first lane is 2)\n", lane_seen);
    TEST_ASSERT_EQUAL_INT(2, lane_seen);
    raw()->CloseConnection(conn, 0, nullptr, false);
    host.destroy();
    /* A one-lane guest stays connected to it through several keepalives each way. */
    host = host_on_free_port(three);
    steam_net_link_config_t one = config_of(1, 1);
    one.timeout_s = 1.0F;
    side guest = join(one, host.port);
    connect(&host, &guest);
    tick_until({&host, &guest}, 2.0, [] { return false; });
    TEST_ASSERT_NULL(host.first(NET_LINK_EVENT_DISCONNECTED));
    TEST_ASSERT_NULL(guest.first(NET_LINK_EVENT_DISCONNECTED));
    host.destroy();
    guest.destroy();
}

void test_address_text_that_names_a_port_is_refused(void) {
    const steam_net_link_config_t config = config_of(1, 1);
    const char *named[] = {"127.0.0.1:27000", "127.0.0.1:0", "[::1]:27000"};
    for (const char *text : named) {
        TEST_ASSERT_NULL_MESSAGE(steam_net_link_host_ip(&config, text, s_next_port), text);
        TEST_ASSERT_NULL_MESSAGE(steam_net_link_join_ip(&config, text, s_next_port), text);
    }
    TEST_ASSERT_NULL(steam_net_link_join_ip(&config, "not an address", s_next_port));
}

void test_an_idle_link_sends_a_keepalive_a_quarter_timeout_not_a_frame(void) {
    const steam_net_link_config_t config = config_of(1, 1);
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    tick_until({&host, &guest}, 0.5, [] { return false; });
    trace(host.port);
    tick_until({&host, &guest}, 2.0, [] { return false; });
    const int packets = s_trace_packets;
    trace(0);
    /* A keepalive each frame would be some 200 packets here. */
    printf("idle: %d packets from the guest in 2 s (timeout 10 s, keepalive every 2.5 s)\n", packets);
    TEST_ASSERT_TRUE(packets <= 10);
    host.destroy();
    guest.destroy();
}

void test_fake_loss_set_before_a_stop_is_gone_after_the_next_start(void) {
    const steam_net_link_fake_t all{1.0F, 0.0F, 0.0F, 0.0F};
    steam_net_link_set_fake(&all, &all, 0.0F);
    steam_platform_stop();
    TEST_ASSERT_EQUAL_INT(0, steam_live::start());
    const steam_net_link_config_t config = config_of(1, 1);
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    host.destroy();
    guest.destroy();
}

void test_fake_lag_shows_in_the_ping_and_clears(void) {
    const steam_net_link_config_t config = config_of(1, 1);
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    const std::vector<uint8_t> small = pattern(16, 7);
    auto ping_beyond = [&](float ms) {
        net_link_stats_t s;
        net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_RELIABLE, small.data(), 16);
        return net_link_stats(host.link, host.peer(), &s) && s.ping_ms >= ms;
    };
    const steam_net_link_fake_t lag{0.0F, 60.0F, 0.0F, 0.0F};
    steam_net_link_set_fake(&lag, nullptr, 0.0F);
    TEST_ASSERT_TRUE_MESSAGE(tick_until({&host, &guest}, 15.0, [&] { return ping_beyond(50.0F); }), "fake lag never showed in the ping");
    steam_net_link_set_fake(nullptr, nullptr, 0.0F);
    TEST_ASSERT_TRUE_MESSAGE(tick_until({&host, &guest}, 15.0, [&] {
        net_link_stats_t s;
        net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_RELIABLE, small.data(), 16);
        return net_link_stats(host.link, host.peer(), &s) && s.ping_ms >= 0.0F && s.ping_ms < 30.0F;
    }), "the ping did not come back after clearing");
    host.destroy();
    guest.destroy();
}

} // namespace

int main(int argc, char **argv) {
    /* Steam writes its own complaints to stderr; unbuffered, the log shows them where they happen. */
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int started = steam_live::start(steam_live::simulate_no_client(argc, argv));
    if (started != 0) {
        return started;
    }
    use_block(steam_live::BLOCK_PROBE);
    UNITY_BEGIN();
    RUN_CASE(test_unreliable_at_the_limit_sent_alone_leaves_in_one_packet_and_one_byte_more_is_refused);
    RUN_CASE(test_steams_largest_reliable_message_arrives);
    RUN_CASE(test_a_full_send_buffer_answers_full_and_dropped_then_recovers);
    RUN_CASE(test_stats_become_measured_after_traffic);
    RUN_CASE(test_a_raw_peer_that_breaks_the_rules_is_closed_with_transport);
    RUN_CASE(test_a_raw_host_that_breaks_the_rules_is_closed_with_transport);
    RUN_CASE(test_a_guest_reads_connected_before_messages_that_overtake_its_callback);
    RUN_CASE(test_a_peers_last_messages_come_before_its_disconnected_when_the_host_polls_late);
    RUN_CASE(test_a_burst_of_keepalives_is_consumed_and_a_flood_of_them_is_cut);
    RUN_CASE(test_silence_times_out_within_the_configured_time);
    RUN_CASE(test_the_keepalive_rides_the_first_lane_even_one_the_other_end_lacks);
    RUN_CASE(test_address_text_that_names_a_port_is_refused);
    RUN_CASE(test_an_idle_link_sends_a_keepalive_a_quarter_timeout_not_a_frame);
    RUN_CASE(test_fake_lag_shows_in_the_ping_and_clears);
    RUN_CASE(test_fake_loss_set_before_a_stop_is_gone_after_the_next_start);
    RUN_CASE(test_a_link_that_outlives_the_stop_reports_transport_once_and_stays_inert);
    const int failures = UNITY_END();
    steam_platform_stop();
    return failures;
}
