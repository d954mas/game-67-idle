/* net-link's contract suite on the Steam link, host and guests in this one process over real
   UDP on 127.0.0.1. Needs a running Steam client; exit 77 is a skip. */

#include "features/steam_platform/steam_net_link.h"
#include "features/net_link/net_link_transport.h"
#include "net_link_contract.h"
#include "steam_live.hpp"
#include "unity.h"

#include <stdlib.h>

void setUp(void) { steam_live::case_began(); }
void tearDown(void) { steam_live::case_ended(Unity.CurrentTestName); }

namespace {

/* The suite destroys some links itself and leaves the rest to `end`, so each Steam link is
   handed out inside a wrapper that remembers whether it is gone. */
struct wrapped {
    net_link_t base;
    net_link_t *inner;
    bool destroyed;
};

net_link_t *inner(net_link_t *link) { return reinterpret_cast<wrapped *>(link)->inner; }
const net_link_t *inner(const net_link_t *link) { return reinterpret_cast<const wrapped *>(link)->inner; }

const net_link_ops_t WRAPPED_OPS = {
    [](net_link_t *l, net_link_peer_t p, uint8_t lane, uint32_t flags, const void *data, uint32_t size) { return net_link_send(inner(l), p, lane, flags, data, size); },
    [](net_link_t *l, net_link_event_t *events, uint32_t capacity) { return net_link_poll(inner(l), events, capacity); },
    [](net_link_t *l) { net_link_flush(inner(l)); },
    [](net_link_t *l, net_link_peer_t p, uint32_t reason) { net_link_close(inner(l), p, reason); },
    [](net_link_t *l) {
        auto *w = reinterpret_cast<wrapped *>(l);
        if (!w->destroyed) {
            net_link_destroy(w->inner);
            w->destroyed = true;
        }
    },
    [](const net_link_t *l, net_link_peer_t p, net_link_stats_t *out) { return net_link_stats(inner(l), p, out); },
    [](const net_link_t *l, net_link_peer_t p) { return net_link_identity(inner(l), p); },
};

constexpr int OPENED_MAX = 64;
constexpr double TIMEOUT_S = 2.0;

struct env_state {
    wrapped opened[OPENED_MAX];
    int count;
    uint16_t next_port;
    uint16_t host_port;
};

env_state s_env;

steam_net_link_config_t config_for(uint32_t max_peers) {
    steam_net_link_config_t config{};
    config.lanes.count = 3;
    config.lanes.priority[0] = 0;
    config.lanes.priority[1] = 1;
    config.lanes.priority[2] = 1;
    config.max_peers = max_peers;
    config.timeout_s = static_cast<float>(TIMEOUT_S);
    config.connect_timeout_s = 3.0F;
    return config;
}

net_link_t *wrap(net_link_t *link) {
    if (link == nullptr || s_env.count >= OPENED_MAX) {
        return link;
    }
    wrapped *w = &s_env.opened[s_env.count++];
    *w = wrapped{};
    w->inner = link;
    net_link_init(&w->base, &WRAPPED_OPS, net_link_lane_count(link), net_link_max_unreliable(link), net_link_max_reliable(link));
    return &w->base;
}

/* Every link still open and the fake network. A failed case leaves without its `end`, so
   `begin` calls this too. */
void release_all() {
    for (int i = 0; i < s_env.count; i++) {
        net_link_destroy(&s_env.opened[i].base);
    }
    s_env.count = 0;
    steam_net_link_set_fake(nullptr, nullptr, 0.0F);
}

void env_begin(void *) {
    if (s_env.count > 0) {
        release_all();
        steam_live::pass(0.05);
    }
    /* A case without a host dials a port nobody was given. */
    s_env.host_port = s_env.next_port++;
}

void env_end(void *) {
    release_all();
    /* Lets the closes leave before the next case. */
    steam_live::pass(0.05);
}

net_link_t *env_host(void *, uint64_t, uint32_t max_peers) {
    const steam_net_link_config_t config = config_for(max_peers);
    for (int attempt = 0; attempt < 64; attempt++) {
        const uint16_t port = s_env.next_port++;
        if (!steam_live::port_in_block(steam_live::BLOCK_CONTRACT, port)) {
            TEST_FAIL_MESSAGE("the contract ran out of its block of ports");
        }
        if (net_link_t *host = steam_net_link_host_ip(&config, "127.0.0.1", port)) {
            s_env.host_port = port;
            return wrap(host);
        }
    }
    return nullptr;
}

net_link_t *env_guest(void *, uint64_t) {
    const steam_net_link_config_t config = config_for(1);
    return wrap(steam_net_link_join_ip(&config, "127.0.0.1", s_env.host_port));
}

void env_pass(void *, double seconds) { steam_live::pass(seconds); }

/* Steam's fake loss is process-wide: it cuts every path, which the cut cases allow, as they hold
   one guest. */
void env_cut(void *, net_link_t *, bool cut) {
    const steam_net_link_fake_t all{1.0F, 0.0F, 0.0F, 0.0F};
    steam_net_link_set_fake(cut ? &all : nullptr, cut ? &all : nullptr, 0.0F);
}

} // namespace

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int started = steam_live::start(steam_live::simulate_no_client(argc, argv));
    if (started != 0) {
        return started;
    }
    s_env.next_port = steam_live::port_block(steam_live::BLOCK_CONTRACT);
    UNITY_BEGIN();
    net_link_contract_env_t env{};
    env.begin = env_begin;
    env.end = env_end;
    env.open_host = env_host;
    env.open_guest = env_guest;
    env.pass = env_pass;
    env.step_s = 0.01;
    /* A fresh process waits one to two seconds for its Steam certificate before the first connect. */
    env.settle_s = 6.0;
    env.fail_s = 6.0;
    /* What was sent has landed after this long on loopback with 10 ms frames and a flush each
       tick: over 100 sends each the worst was 12 ms for a small reliable message, 32 ms for a
       small unreliable one and 108 ms for a 16 KB one (Steam paces at 256 KB/s); 0.3 s is near
       three times the worst. */
    env.quiet_s = 0.3;
    /* A late event would come from a timer of the transport: the longest are the timeout and
       the keepalive, a quarter of it. */
    env.silence_s = TIMEOUT_S * 1.25;
    /* Two windows of one account share a SteamID. */
    env.exact_identity = false;
    /* A refusing host tells the guest nothing about itself. */
    env.refused_names_host = false;
    env.unreliable_arrives = true;
    env.guest_links = 4;
    env.cut = env_cut;
    /* The configured timeout plus one keepalive round. */
    env.timeout_s = TIMEOUT_S + 1.0;
    net_link_contract_use(&env);
    /* STEAM_NET_LINK_CONTRACT_ONLY: a part of the case names, to check one rule in seconds. */
    const std::string only = steam_live::env("STEAM_NET_LINK_CONTRACT_ONLY");
    net_link_contract_run_matching(only.empty() ? nullptr : only.c_str());
    const int failures = UNITY_END();
    release_all();
    steam_platform_stop();
    return failures;
}
