/* Two windows on one PC: this test hosts on 127.0.0.1 and starts itself again as guest
   processes under the same Steam account. They exchange reliable and unreliable messages on
   every lane and the guest's close code arrives; a guest's burst arrives whole when it closes,
   destroys and stops in the documented order; a guest that stops Steam with its link open reads
   as REMOTE_CLOSE; a killed guest reads TIMEOUT within the configured time; a fresh process
   with a short connect deadline gives up on time. Exit 77 is a skip.

   `--guest <port> <mode>` is a child's side. Children belong to the test: on Windows a job
   object kills them when this process ends on any path, and tearDown kills what a failed case
   left running. */

#include "features/steam_platform/steam_net_link.h"
#include "steam_live.hpp"
#include "unity.h"

#include <steam/isteamnetworkingsockets.h>
#include <steam/steam_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#endif

namespace {

constexpr uint8_t LANES = 3;
constexpr uint32_t PER_LANE = 20;
constexpr float TIMEOUT_S = 2.0F;
constexpr uint32_t GUEST_CLOSE = NET_LINK_REASON_APP_FIRST + 233;
constexpr uint32_t BURST_CLOSE = NET_LINK_REASON_APP_FIRST + 234;
constexpr uint32_t LOST_CLOSE = NET_LINK_REASON_APP_FIRST + 235;
constexpr uint32_t BURST = 400;
constexpr uint32_t BURST_ITEM = 1000;
constexpr uint8_t VERDICT = 0xEE;
/* What a child answers when its case went as expected. */
constexpr int CHILD_OK = 0;

steam_net_link_config_t config_of(uint32_t max_peers) {
    steam_net_link_config_t config{};
    config.lanes.count = LANES;
    for (uint8_t lane = 0; lane < LANES; lane++) {
        config.lanes.priority[lane] = lane;
    }
    config.max_peers = max_peers;
    config.timeout_s = TIMEOUT_S;
    /* Covers the child's Steam start and its certificate. */
    config.connect_timeout_s = 20.0F;
    return config;
}

/* [lane, kind, seq] then a pattern, so either side checks what the other sent. */
std::vector<uint8_t> message(uint8_t lane, bool reliable, uint32_t seq, uint32_t size = 0) {
    std::vector<uint8_t> bytes(size ? size : 16 + (seq * 29U) % 300U);
    bytes[0] = lane;
    bytes[1] = reliable ? 1 : 0;
    memcpy(&bytes[2], &seq, sizeof seq);
    for (size_t i = 6; i < bytes.size(); i++) {
        bytes[i] = static_cast<uint8_t>(i * 3U + seq + lane);
    }
    return bytes;
}

void send_all(net_link_t *link, net_link_peer_t peer) {
    for (uint32_t seq = 0; seq < PER_LANE; seq++) {
        for (uint8_t lane = 0; lane < LANES; lane++) {
            const std::vector<uint8_t> reliable = message(lane, true, seq), unreliable = message(lane, false, seq);
            net_link_send(link, peer, lane, NET_LINK_SEND_RELIABLE, reliable.data(), static_cast<uint32_t>(reliable.size()));
            net_link_send(link, peer, lane, NET_LINK_SEND_UNRELIABLE, unreliable.data(), static_cast<uint32_t>(unreliable.size()));
        }
    }
    net_link_flush(link);
}

/* What one side read of the other's messages. */
struct tally {
    uint32_t next_reliable[LANES] = {};
    uint32_t unreliable[LANES] = {};
    bool broken = false;

    void take(const net_link_event_t &event) {
        if (event.size < 6 || event.lane >= LANES || event.data[0] != event.lane || (event.data[1] != 0) != event.reliable) {
            broken = true;
            return;
        }
        uint32_t seq;
        memcpy(&seq, event.data + 2, sizeof seq);
        const std::vector<uint8_t> expected = message(event.lane, event.reliable, seq, event.size);
        if (memcmp(expected.data(), event.data, event.size) != 0) {
            broken = true;
        } else if (event.reliable) {
            broken |= seq != next_reliable[event.lane]++;
        } else {
            unreliable[event.lane]++;
        }
    }
    bool complete() const {
        for (uint8_t lane = 0; lane < LANES; lane++) {
            if (next_reliable[lane] != PER_LANE || unreliable[lane] == 0) {
                return false;
            }
        }
        return !broken;
    }
};

// #region the guest processes

/* The documented shutdown: links are destroyed already; frames while a close may be leaving. */
void shut_down() {
    const double start = steam_live::now_s();
    steam_live::pass_until(10.0, [] { return !steam_net_link_lingering(); });
    printf("guest: lingered %.2f s before the stop\n", steam_live::now_s() - start);
    steam_platform_stop();
}

struct guest_side {
    net_link_t *link = nullptr;
    net_link_peer_t host = NET_LINK_PEER_NONE;
    uint64_t host_identity = 0;
    tally got;
    bool left = false;

    void poll() {
        net_link_event_t events[32];
        uint32_t n;
        while ((n = net_link_poll(link, events, 32)) > 0) {
            for (uint32_t i = 0; i < n; i++) {
                if (events[i].type == NET_LINK_EVENT_CONNECTED) {
                    host = events[i].peer;
                    host_identity = events[i].identity;
                } else if (events[i].type == NET_LINK_EVENT_MESSAGE) {
                    got.take(events[i]);
                } else {
                    printf("guest: disconnected with %u\n", events[i].reason);
                    left = true;
                }
            }
        }
    }
    bool connect(uint16_t port) {
        const steam_net_link_config_t config = config_of(1);
        link = steam_net_link_join_ip(&config, "127.0.0.1", port);
        return link != nullptr && steam_live::pass_until(30.0, [&] {
                   poll();
                   return host != NET_LINK_PEER_NONE || left;
               }) && host != NET_LINK_PEER_NONE;
    }
};

int guest_exchange(guest_side *g) {
    send_all(g->link, g->host);
    const bool done = steam_live::pass_until(30.0, [&] {
        g->poll();
        net_link_flush(g->link);
        return g->got.complete() || g->left;
    }) && g->got.complete();
    if (!done) {
        printf("guest: gave up\n");
        return 3;
    }
    /* [verdict, the host's identity is this account] */
    const uint8_t verdict[2] = {VERDICT, static_cast<uint8_t>(g->host_identity == steam_platform_user() ? 1 : 0)};
    net_link_send(g->link, g->host, 0, NET_LINK_SEND_RELIABLE, verdict, sizeof verdict);
    net_link_close(g->link, g->host, GUEST_CLOSE);
    net_link_destroy(g->link);
    shut_down();
    printf("guest: exchanged and closed\n");
    return CHILD_OK;
}

int guest_burst(guest_side *g) {
    for (uint32_t seq = 0; seq < BURST; seq++) {
        const std::vector<uint8_t> item = message(1, true, seq, BURST_ITEM);
        if (net_link_send(g->link, g->host, 1, NET_LINK_SEND_RELIABLE, item.data(), BURST_ITEM) != NET_LINK_SEND_OK) {
            printf("guest: burst send %u refused\n", seq);
            return 4;
        }
    }
    net_link_close(g->link, g->host, BURST_CLOSE);
    net_link_destroy(g->link);
    if (!steam_net_link_lingering()) {
        printf("guest: a close of %u KB did not linger\n", BURST * BURST_ITEM / 1000);
        return 5;
    }
    shut_down();
    return CHILD_OK;
}

/* The close packet itself is lost: everything this process sends is dropped from the close
   until 0.1 s later. Steam does not send a close again; the host's next keepalive reaches the
   closed connection, which answers with the reason, if this process still runs then. The hold
   after the close is what keeps it running through the documented shutdown loop. */
int guest_lost_close(guest_side *g) {
    steam_live::pass_until(0.3, [] { return false; });
    net_link_close(g->link, g->host, LOST_CLOSE);
    const steam_net_link_fake_t all{1.0F, 0.0F, 0.0F, 0.0F};
    steam_net_link_set_fake(&all, nullptr, 0.0F);
    const double closed = steam_live::now_s();
    net_link_destroy(g->link);
    steam_live::pass_until(10.0, [&] {
        if (steam_live::now_s() - closed >= 0.1) {
            steam_net_link_set_fake(nullptr, nullptr, 0.0F);
        }
        return !steam_net_link_lingering();
    });
    steam_net_link_set_fake(nullptr, nullptr, 0.0F);
    printf("guest: a close sent into loss lingered %.2f s before the stop\n", steam_live::now_s() - closed);
    steam_platform_stop();
    return CHILD_OK;
}

int guest_stop_open(guest_side *g) {
    steam_live::pass_until(0.3, [] { return false; });
    steam_platform_stop();
    net_link_destroy(g->link);
    return CHILD_OK;
}

int guest_idle(guest_side *g) {
    steam_live::pass_until(60.0, [&] {
        g->poll();
        return g->left;
    });
    net_link_destroy(g->link);
    steam_platform_stop();
    return CHILD_OK;
}

bool certificate_current() { return SteamNetworkingSockets()->GetAuthenticationStatus(nullptr) == k_ESteamNetworkingAvailability_Current; }

/* A fresh process waits for its certificate; a deadline that passes during that wait reads
   TRANSPORT, on time. When the certificate comes inside the window the link dials with what is
   left, and `port` has nobody hosting, so the honest answer is then UNREACHABLE. The child
   watches the certificate frame by frame; it sees it a frame after the link may have, so
   within two of its longest frames of the deadline either answer holds. */
int guest_deadline(uint16_t port) {
    constexpr double DEADLINE_S = 0.25;
    steam_net_link_config_t config = config_of(1);
    config.connect_timeout_s = static_cast<float>(DEADLINE_S);
    guest_side g;
    g.link = steam_net_link_join_ip(&config, "127.0.0.1", port);
    if (g.link == nullptr) {
        return 6;
    }
    const double start = steam_live::now_s();
    double proven_after = certificate_current() ? 0.0 : -1.0;
    double last_frame = start, longest_frame = 0.0;
    uint32_t reason = 0;
    steam_live::pass_until(5.0, [&] {
        const double now = steam_live::now_s();
        longest_frame = now - last_frame > longest_frame ? now - last_frame : longest_frame;
        last_frame = now;
        if (proven_after < 0 && certificate_current()) {
            proven_after = now - start;
        }
        net_link_event_t events[4];
        const uint32_t n = net_link_poll(g.link, events, 4);
        for (uint32_t i = 0; i < n; i++) {
            if (events[i].type == NET_LINK_EVENT_DISCONNECTED) {
                reason = events[i].reason;
            }
        }
        return reason != 0;
    });
    const double took = steam_live::now_s() - start;
    printf("guest: certificate current after %.2f s (-1: not by the end); a %.2f s deadline read reason %u after %.2f s\n", proven_after, DEADLINE_S, reason, took);
    net_link_destroy(g.link);
    steam_platform_stop();
    const double window = 2.0 * longest_frame;
    const bool waited_out = proven_after < 0 || proven_after > DEADLINE_S + window;
    const bool dialled = proven_after >= 0 && proven_after < DEADLINE_S - window;
    const bool honest = waited_out ? reason == NET_LINK_REASON_TRANSPORT
                        : dialled  ? reason == NET_LINK_REASON_UNREACHABLE
                                   : reason == NET_LINK_REASON_TRANSPORT || reason == NET_LINK_REASON_UNREACHABLE;
    return honest && took < 1.0 ? CHILD_OK : 7;
}

constexpr int CHILD_PROVEN_TOO_SOON = 9;
constexpr int CHILD_CERTIFICATE_TOO_LATE = 10;

/* The certificate comes inside a 3 s deadline and nobody hosts: the dial gets what is left, so
   the guest reads UNREACHABLE at the deadline itself, not the certificate's time plus the
   deadline. Steam's connect timeout fired at its configured time in the experiments (2.005 s
   for 2 s), and frames and Steam's timer step add a little; 0.4 s covers both. */
int guest_certificate_in_time(uint16_t port) {
    constexpr double DEADLINE_S = 3.0, MARGIN_S = 0.4;
    steam_net_link_config_t config = config_of(1);
    config.connect_timeout_s = static_cast<float>(DEADLINE_S);
    guest_side g;
    g.link = steam_net_link_join_ip(&config, "127.0.0.1", port);
    if (g.link == nullptr) {
        return 6;
    }
    const double start = steam_live::now_s();
    double proven_after = certificate_current() ? 0.0 : -1.0;
    uint32_t reason = 0;
    double took = -1.0;
    steam_live::pass_until(DEADLINE_S + 3.0, [&] {
        if (proven_after < 0 && certificate_current()) {
            proven_after = steam_live::now_s() - start;
        }
        net_link_event_t events[4];
        const uint32_t n = net_link_poll(g.link, events, 4);
        for (uint32_t i = 0; i < n; i++) {
            if (events[i].type == NET_LINK_EVENT_DISCONNECTED) {
                reason = events[i].reason;
                took = steam_live::now_s() - start;
            }
        }
        return reason != 0;
    });
    printf("guest: certificate current after %.2f s; a %.1f s deadline read reason %u after %.2f s\n", proven_after, DEADLINE_S, reason, took);
    net_link_destroy(g.link);
    steam_platform_stop();
    if (proven_after < 0 || proven_after >= DEADLINE_S) {
        printf("guest: the certificate did not come in time; this case is for that branch\n");
        return CHILD_CERTIFICATE_TOO_LATE;
    }
    return reason == NET_LINK_REASON_UNREACHABLE && took >= DEADLINE_S - 0.05 && took <= DEADLINE_S + MARGIN_S ? CHILD_OK : 11;
}

/* local_without_proof on both ends: a fresh process connects before its certificate exists.
   Without the flag the guest would wait for the certificate before it even dials (the deadline
   case), so it could not be connected while the certificate is not current. */
int guest_without_proof(uint16_t port) {
    if (certificate_current()) {
        printf("guest: the certificate was ready at the start; nothing to prove\n");
        return CHILD_PROVEN_TOO_SOON;
    }
    steam_net_link_config_t config = config_of(1);
    config.local_without_proof = true;
    guest_side g;
    g.link = steam_net_link_join_ip(&config, "127.0.0.1", port);
    if (g.link == nullptr) {
        return 6;
    }
    bool proven_first = false;
    steam_live::pass_until(10.0, [&] {
        proven_first |= certificate_current();
        g.poll();
        return g.host != NET_LINK_PEER_NONE || g.left;
    });
    printf("guest: without proof %s, certificate %s at that moment, the host named as this account: %d\n", g.host != NET_LINK_PEER_NONE ? "connected" : "did not connect",
           proven_first ? "already current" : "not current", g.host_identity == steam_platform_user() ? 1 : 0);
    const bool ok = g.host != NET_LINK_PEER_NONE && !proven_first && g.host_identity == steam_platform_user();
    net_link_destroy(g.link);
    shut_down();
    return ok ? CHILD_OK : 8;
}

/* The host role of the burst: this child hosts, the test's process joins; a burst, a close with
   an app code, destroy, frames while lingering, stop. */
int host_burst(uint16_t port) {
    const steam_net_link_config_t config = config_of(1);
    net_link_t *link = steam_net_link_host_ip(&config, "127.0.0.1", port);
    if (link == nullptr) {
        return 6;
    }
    net_link_peer_t guest = NET_LINK_PEER_NONE;
    steam_live::pass_until(30.0, [&] {
        net_link_event_t events[8];
        const uint32_t n = net_link_poll(link, events, 8);
        for (uint32_t i = 0; i < n; i++) {
            if (events[i].type == NET_LINK_EVENT_CONNECTED) {
                guest = events[i].peer;
            }
        }
        return guest != NET_LINK_PEER_NONE;
    });
    if (guest == NET_LINK_PEER_NONE) {
        return 2;
    }
    for (uint32_t seq = 0; seq < BURST; seq++) {
        const std::vector<uint8_t> item = message(1, true, seq, BURST_ITEM);
        if (net_link_send(link, guest, 1, NET_LINK_SEND_RELIABLE, item.data(), BURST_ITEM) != NET_LINK_SEND_OK) {
            return 4;
        }
    }
    net_link_close(link, guest, BURST_CLOSE);
    net_link_destroy(link);
    shut_down();
    return CHILD_OK;
}

int guest_main(uint16_t port, const char *mode) {
    if (const int started = steam_live::start()) {
        return started;
    }
    if (strcmp(mode, "deadline") == 0) {
        return guest_deadline(port);
    }
    if (strcmp(mode, "certificate_in_time") == 0) {
        return guest_certificate_in_time(port);
    }
    if (strcmp(mode, "without_proof") == 0) {
        return guest_without_proof(port);
    }
    if (strcmp(mode, "host_burst") == 0) {
        return host_burst(port);
    }
    guest_side g;
    if (!g.connect(port)) {
        printf("guest: no connect\n");
        return 2;
    }
    if (strcmp(mode, "exchange") == 0) {
        return guest_exchange(&g);
    }
    if (strcmp(mode, "burst") == 0) {
        return guest_burst(&g);
    }
    if (strcmp(mode, "stop_open") == 0) {
        return guest_stop_open(&g);
    }
    if (strcmp(mode, "lost_close") == 0) {
        return guest_lost_close(&g);
    }
    return guest_idle(&g);
}

// #endregion

// #region the host process

struct child {
    bool running = false;
#ifdef _WIN32
    PROCESS_INFORMATION process{};
#else
    pid_t pid = -1;
#endif
};

#ifdef _WIN32
HANDLE s_job;
#endif
const char *s_self;
child s_children[4];

/* The exit code, or -1 when it has not exited within `limit_s`. */
int wait_child(child *c, double limit_s) {
    if (!c->running) {
        return -1;
    }
#ifdef _WIN32
    if (WaitForSingleObject(c->process.hProcess, static_cast<DWORD>(limit_s * 1000.0)) != WAIT_OBJECT_0) {
        return -1;
    }
    DWORD code = 0;
    GetExitCodeProcess(c->process.hProcess, &code);
    CloseHandle(c->process.hProcess);
    CloseHandle(c->process.hThread);
    c->running = false;
    return static_cast<int>(code);
#else
    const double stop = steam_live::now_s() + limit_s;
    int status = 0;
    while (steam_live::now_s() < stop) {
        if (waitpid(c->pid, &status, WNOHANG) == c->pid) {
            c->running = false;
            return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
        }
        steam_live::pass(0.05);
    }
    return -1;
#endif
}

void kill_child(child *c) {
    if (!c->running) {
        return;
    }
#ifdef _WIN32
    TerminateProcess(c->process.hProcess, 9);
#else
    kill(c->pid, SIGKILL);
#endif
}

child *spawn(uint16_t port, const char *mode) {
    child *c = nullptr;
    for (child &slot : s_children) {
        if (!slot.running) {
            c = &slot;
            break;
        }
    }
    if (c == nullptr) {
        return nullptr;
    }
#ifdef _WIN32
    wchar_t exe[1024];
    if (GetModuleFileNameW(nullptr, exe, 1024) == 0) {
        return nullptr;
    }
    wchar_t command[1400];
    swprintf(command, 1400, L"\"%ls\" --guest %u %hs", exe, static_cast<unsigned>(port), mode);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    /* Into the job before it runs a single instruction. */
    if (CreateProcessW(exe, command, nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, nullptr, &startup, &c->process) == 0) {
        return nullptr;
    }
    c->running = true;
    if (AssignProcessToJobObject(s_job, c->process.hProcess) == 0) {
        kill_child(c);
        wait_child(c, 5.0);
        return nullptr;
    }
    ResumeThread(c->process.hThread);
#else
    char port_text[16];
    snprintf(port_text, sizeof port_text, "%u", static_cast<unsigned>(port));
    c->pid = fork();
    if (c->pid == 0) {
#ifdef __linux__
        prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
        execl(s_self, s_self, "--guest", port_text, mode, static_cast<char *>(nullptr));
        _exit(127);
    }
    if (c->pid < 0) {
        return nullptr;
    }
    c->running = true;
#endif
    return c;
}

net_link_t *s_host;
uint16_t s_port;

struct host_log {
    std::vector<net_link_event_t> events;
    tally got;
    uint32_t burst_next = 0;
    bool burst_broken = false;
    bool verdict_seen = false;
    bool verdict_ok = false;

    void poll() {
        net_link_event_t batch[32];
        uint32_t n;
        while ((n = net_link_poll(s_host, batch, 32)) > 0) {
            for (uint32_t i = 0; i < n; i++) {
                net_link_event_t e = batch[i];
                if (e.type == NET_LINK_EVENT_MESSAGE && e.size == 2 && e.data[0] == VERDICT) {
                    verdict_seen = true;
                    verdict_ok = e.data[1] == 1;
                } else if (e.type == NET_LINK_EVENT_MESSAGE && e.size == BURST_ITEM) {
                    const std::vector<uint8_t> expected = message(1, true, burst_next, BURST_ITEM);
                    burst_broken |= !e.reliable || e.lane != 1 || memcmp(expected.data(), e.data, BURST_ITEM) != 0;
                    burst_next++;
                } else if (e.type == NET_LINK_EVENT_MESSAGE) {
                    got.take(e);
                }
                e.data = nullptr;
                events.push_back(e);
            }
        }
    }
    const net_link_event_t *last(net_link_event_type_t type) const {
        for (size_t i = events.size(); i-- > 0;) {
            if (events[i].type == type) {
                return &events[i];
            }
        }
        return nullptr;
    }
};

template <typename Done> bool host_until(host_log *log, double limit_s, Done done) {
    return steam_live::pass_until(limit_s, [&] {
        log->poll();
        net_link_flush(s_host);
        return done();
    });
}

net_link_event_t connected_guest(host_log *log, const char *mode) {
    TEST_ASSERT_NOT_NULL_MESSAGE(spawn(s_port, mode), "the guest process did not start");
    TEST_ASSERT_TRUE_MESSAGE(host_until(log, 30.0, [&] { return log->last(NET_LINK_EVENT_CONNECTED) != nullptr; }), "the guest process never connected");
    return *log->last(NET_LINK_EVENT_CONNECTED);
}

child *last_spawned() {
    for (size_t i = sizeof s_children / sizeof s_children[0]; i-- > 0;) {
        if (s_children[i].running) {
            return &s_children[i];
        }
    }
    return nullptr;
}

void test_two_processes_exchange_on_every_lane_and_the_guest_close_code_arrives(void) {
    host_log log;
    const net_link_event_t connected = connected_guest(&log, "exchange");
    child *guest = last_spawned();
    /* Two windows of one account: the guest is this same SteamID. */
    TEST_ASSERT_EQUAL_UINT64(steam_platform_user(), connected.identity);
    send_all(s_host, connected.peer);
    TEST_ASSERT_TRUE_MESSAGE(host_until(&log, 20.0, [&] { return log.last(NET_LINK_EVENT_DISCONNECTED) != nullptr; }), "the guest never closed");
    TEST_ASSERT_TRUE(log.got.complete());
    TEST_ASSERT_TRUE_MESSAGE(log.verdict_seen, "the guest's verdict did not arrive before its close");
    TEST_ASSERT_TRUE_MESSAGE(log.verdict_ok, "the guest saw another identity than this account");
    const net_link_event_t *left = log.last(NET_LINK_EVENT_DISCONNECTED);
    TEST_ASSERT_EQUAL_UINT32(connected.peer, left->peer);
    TEST_ASSERT_EQUAL_UINT32(GUEST_CLOSE, left->reason);
    TEST_ASSERT_EQUAL_INT(CHILD_OK, wait_child(guest, 20.0));
}

void test_a_guest_that_closes_destroys_and_lingers_before_its_stop_delivers_its_burst(void) {
    host_log log;
    const net_link_event_t connected = connected_guest(&log, "burst");
    child *guest = last_spawned();
    TEST_ASSERT_TRUE_MESSAGE(host_until(&log, 20.0, [&] { return log.last(NET_LINK_EVENT_DISCONNECTED) != nullptr; }), "the guest never closed");
    printf("host: burst %u of %u messages, then reason %u\n", log.burst_next, BURST, log.last(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_FALSE(log.burst_broken);
    TEST_ASSERT_EQUAL_UINT32(BURST, log.burst_next);
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, log.events.back().type);
    TEST_ASSERT_EQUAL_UINT32(connected.peer, log.events.back().peer);
    TEST_ASSERT_EQUAL_UINT32(BURST_CLOSE, log.events.back().reason);
    TEST_ASSERT_EQUAL_INT(CHILD_OK, wait_child(guest, 20.0));
}

void test_a_guest_that_stops_steam_with_its_link_open_reads_as_remote_close(void) {
    host_log log;
    const net_link_event_t connected = connected_guest(&log, "stop_open");
    child *guest = last_spawned();
    TEST_ASSERT_TRUE_MESSAGE(host_until(&log, 10.0, [&] { return log.last(NET_LINK_EVENT_DISCONNECTED) != nullptr; }), "the host never noticed");
    TEST_ASSERT_EQUAL_UINT32(connected.peer, log.last(NET_LINK_EVENT_DISCONNECTED)->peer);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, log.last(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_EQUAL_INT(CHILD_OK, wait_child(guest, 20.0));
}

void test_a_guest_whose_close_packet_is_lost_still_delivers_its_code(void) {
    host_log log;
    const net_link_event_t connected = connected_guest(&log, "lost_close");
    child *guest = last_spawned();
    TEST_ASSERT_TRUE_MESSAGE(host_until(&log, 10.0, [&] { return log.last(NET_LINK_EVENT_DISCONNECTED) != nullptr; }), "the host never noticed");
    TEST_ASSERT_EQUAL_UINT32(connected.peer, log.last(NET_LINK_EVENT_DISCONNECTED)->peer);
    TEST_ASSERT_EQUAL_UINT32(LOST_CLOSE, log.last(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_EQUAL_INT(CHILD_OK, wait_child(guest, 20.0));
}

void test_a_killed_guest_process_times_out_within_the_configured_time(void) {
    host_log log;
    connected_guest(&log, "idle");
    child *guest = last_spawned();
    /* Some time connected, so the kill is not mistaken for a failed connect. */
    host_until(&log, 1.0, [] { return false; });
    kill_child(guest);
    const double killed = steam_live::now_s();
    TEST_ASSERT_TRUE_MESSAGE(host_until(&log, 15.0, [&] { return log.last(NET_LINK_EVENT_DISCONNECTED) != nullptr; }), "the host never noticed");
    const double after = steam_live::now_s() - killed;
    printf("host: killed guest read after %.2f s (timeout %.1f s)\n", after, static_cast<double>(TIMEOUT_S));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, log.last(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_TRUE(after >= static_cast<double>(TIMEOUT_S) * 0.5 && after <= static_cast<double>(TIMEOUT_S) + 2.0);
    wait_child(guest, 10.0);
}

/* A port of this test's block nobody hosts on: taken and given back at once. */
uint16_t s_next_free;
uint16_t free_port() {
    const steam_net_link_config_t config = config_of(1);
    net_link_t *probe = nullptr;
    uint16_t port = 0;
    for (int attempt = 0; attempt < 32 && probe == nullptr; attempt++) {
        port = s_next_free++;
        TEST_ASSERT_TRUE_MESSAGE(steam_live::port_in_block(steam_live::BLOCK_TWO_WINDOWS, port), "this test ran out of its block of ports");
        probe = steam_net_link_host_ip(&config, "127.0.0.1", port);
    }
    TEST_ASSERT_NOT_NULL(probe);
    net_link_destroy(probe);
    return port;
}

void test_a_fresh_process_with_a_short_deadline_gives_up_on_time(void) {
    child *guest = spawn(free_port(), "deadline");
    TEST_ASSERT_NOT_NULL(guest);
    TEST_ASSERT_EQUAL_INT(CHILD_OK, wait_child(guest, 30.0));
}

void test_a_fresh_process_whose_certificate_comes_in_time_dials_and_reads_unreachable_at_the_deadline(void) {
    child *guest = spawn(free_port(), "certificate_in_time");
    TEST_ASSERT_NOT_NULL(guest);
    TEST_ASSERT_EQUAL_INT(CHILD_OK, wait_child(guest, 30.0));
}

void test_a_fresh_process_without_proof_on_both_ends_connects_before_its_certificate(void) {
    steam_net_link_config_t config = config_of(1);
    config.local_without_proof = true;
    net_link_t *host = nullptr;
    uint16_t port = 0;
    for (int attempt = 0; attempt < 32 && host == nullptr; attempt++) {
        port = s_next_free++;
        TEST_ASSERT_TRUE_MESSAGE(steam_live::port_in_block(steam_live::BLOCK_TWO_WINDOWS, port), "this test ran out of its block of ports");
        host = steam_net_link_host_ip(&config, "127.0.0.1", port);
    }
    TEST_ASSERT_NOT_NULL(host);
    child *guest = spawn(port, "without_proof");
    TEST_ASSERT_NOT_NULL(guest);
    int code = -1;
    steam_live::pass_until(30.0, [&] {
        net_link_event_t events[8];
        while (net_link_poll(host, events, 8) > 0) {
        }
        code = wait_child(guest, 0.0);
        return code >= 0;
    });
    net_link_destroy(host);
    if (code == CHILD_PROVEN_TOO_SOON) {
        TEST_IGNORE_MESSAGE("the child's certificate was ready at its start");
    }
    TEST_ASSERT_EQUAL_INT(CHILD_OK, code);
}

void test_a_host_that_closes_destroys_and_lingers_before_its_stop_delivers_its_burst(void) {
    const uint16_t port = free_port();
    child *host = spawn(port, "host_burst");
    TEST_ASSERT_NOT_NULL(host);
    steam_net_link_config_t config = config_of(1);
    config.connect_timeout_s = 30.0F;
    net_link_t *link = steam_net_link_join_ip(&config, "127.0.0.1", port);
    TEST_ASSERT_NOT_NULL(link);
    uint32_t got = 0, reason = 0;
    bool broken = false;
    steam_live::pass_until(40.0, [&] {
        net_link_event_t events[32];
        uint32_t n;
        while ((n = net_link_poll(link, events, 32)) > 0) {
            for (uint32_t i = 0; i < n; i++) {
                if (events[i].type == NET_LINK_EVENT_MESSAGE) {
                    const std::vector<uint8_t> expected = message(1, true, got, BURST_ITEM);
                    broken |= events[i].size != BURST_ITEM || memcmp(expected.data(), events[i].data, BURST_ITEM) != 0;
                    got++;
                } else if (events[i].type == NET_LINK_EVENT_DISCONNECTED) {
                    reason = events[i].reason;
                }
            }
        }
        return reason != 0;
    });
    printf("guest: the host's burst %u of %u, then reason %u\n", got, BURST, reason);
    net_link_destroy(link);
    TEST_ASSERT_FALSE(broken);
    TEST_ASSERT_EQUAL_UINT32(BURST, got);
    TEST_ASSERT_EQUAL_UINT32(BURST_CLOSE, reason);
    TEST_ASSERT_EQUAL_INT(CHILD_OK, wait_child(host, 20.0));
}

// #endregion

} // namespace

void setUp(void) { steam_live::case_began(); }

/* A case that failed may leave its child running. */
void tearDown(void) {
    steam_live::case_ended(Unity.CurrentTestName);
    for (child &c : s_children) {
        if (c.running && wait_child(&c, 0.0) < 0) {
            kill_child(&c);
            wait_child(&c, 10.0);
        }
    }
}

int main(int argc, char **argv) {
    /* Several processes write to one console; unbuffered, their lines keep their order. */
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc == 4 && strcmp(argv[1], "--guest") == 0) {
        return guest_main(static_cast<uint16_t>(atoi(argv[2])), argv[3]);
    }
    if (const int started = steam_live::start(steam_live::simulate_no_client(argc, argv))) {
        return started;
    }
    s_self = argv[0];
#ifdef _WIN32
    s_job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (s_job == nullptr || SetInformationJobObject(s_job, JobObjectExtendedLimitInformation, &limits, sizeof limits) == 0) {
        printf("FAIL: no job object for the guest processes\n");
        steam_platform_stop();
        return 1;
    }
#endif
    const steam_net_link_config_t config = config_of(2);
    uint16_t port = steam_live::port_block(steam_live::BLOCK_TWO_WINDOWS);
    for (int attempt = 0; attempt < 64 && s_host == nullptr; attempt++, port++) {
        s_host = steam_net_link_host_ip(&config, "127.0.0.1", port);
        s_port = port;
    }
    s_next_free = static_cast<uint16_t>(s_port + 1);
    if (s_host == nullptr) {
        printf("FAIL: no free port to host on\n");
        steam_platform_stop();
        return 1;
    }
    UNITY_BEGIN();
    RUN_TEST(test_two_processes_exchange_on_every_lane_and_the_guest_close_code_arrives);
    RUN_TEST(test_a_guest_that_closes_destroys_and_lingers_before_its_stop_delivers_its_burst);
    RUN_TEST(test_a_guest_that_stops_steam_with_its_link_open_reads_as_remote_close);
    RUN_TEST(test_a_guest_whose_close_packet_is_lost_still_delivers_its_code);
    RUN_TEST(test_a_killed_guest_process_times_out_within_the_configured_time);
    RUN_TEST(test_a_fresh_process_with_a_short_deadline_gives_up_on_time);
    RUN_TEST(test_a_fresh_process_whose_certificate_comes_in_time_dials_and_reads_unreachable_at_the_deadline);
    RUN_TEST(test_a_fresh_process_without_proof_on_both_ends_connects_before_its_certificate);
    RUN_TEST(test_a_host_that_closes_destroys_and_lingers_before_its_stop_delivers_its_burst);
    const int failures = UNITY_END();
    net_link_destroy(s_host);
    steam_platform_stop();
    return failures;
}
