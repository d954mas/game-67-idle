#include "features/net_link/net_link_sim.h"
#include "net_link_contract.h"
#include "unity.h"

#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// #region environments for the contract suite

typedef struct sim_env {
    net_link_sim_t *sim;
    const net_link_conditions_t *conditions; /* both directions of every guest, or NULL */
} sim_env_t;

static net_link_sim_config_t config_of(uint64_t seed, uint32_t guests, uint8_t lanes) {
    net_link_sim_config_t config = {.seed = seed, .max_guests = guests};
    config.lanes.count = lanes;
    for (uint8_t lane = 0; lane < lanes; lane++) {
        config.lanes.priority[lane] = lane;
    }
    return config;
}

static void env_begin(void *context) {
    sim_env_t *env = context;
    net_link_sim_config_t config = config_of(7, 4, 3);
    config.timeout_s = 1.0F;
    env->sim = net_link_sim_create(&config);
    TEST_ASSERT_NOT_NULL(env->sim);
}

static void env_end(void *context) {
    sim_env_t *env = context;
    net_link_sim_destroy(env->sim);
    env->sim = NULL;
}

static net_link_t *env_host(void *context, uint64_t identity, uint32_t max_peers) { return net_link_sim_host(((sim_env_t *)context)->sim, identity, max_peers); }

static net_link_t *env_guest(void *context, uint64_t identity) {
    sim_env_t *env = context;
    net_link_t *guest = net_link_sim_guest(env->sim, identity);
    if (guest && env->conditions) {
        net_link_sim_set_conditions(env->sim, guest, NET_LINK_SIM_TO_HOST, env->conditions);
        net_link_sim_set_conditions(env->sim, guest, NET_LINK_SIM_TO_GUEST, env->conditions);
    }
    return guest;
}

static void env_pass(void *context, double seconds) { net_link_sim_advance(((sim_env_t *)context)->sim, seconds); }

static void env_cut(void *context, net_link_t *guest, bool cut) { net_link_sim_set_cut(((sim_env_t *)context)->sim, guest, cut); }

// #endregion

// #region helpers

typedef struct world {
    net_link_sim_t *sim;
    net_link_t *host;
    net_link_t *guest[4];
    net_link_peer_t at_host[4];  /* the host's handle for guest i */
    net_link_peer_t at_guest[4]; /* guest i's handle for the host */
    uint32_t guests;
} world_t;

enum { HOST_ID = 1000, BATCH = 256 };

static void put32(uint8_t *at, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        at[i] = (uint8_t)(value >> (8 * i));
    }
}

static uint32_t get32(const uint8_t *at) { return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24); }

static net_link_send_result_t send_tagged(net_link_t *link, net_link_peer_t peer, uint8_t lane, uint32_t flags, uint32_t tag, uint32_t seq, uint32_t size) {
    uint8_t buffer[2048] = {0};
    TEST_ASSERT_TRUE(size >= 8 && size <= sizeof(buffer));
    put32(buffer, tag);
    put32(buffer + 4, seq);
    for (uint32_t i = 8; i < size; i++) {
        buffer[i] = (uint8_t)(seq + i);
    }
    return net_link_send(link, peer, lane, flags, buffer, size);
}

static void world_flush(world_t *w) {
    net_link_flush(w->host);
    for (uint32_t i = 0; i < w->guests; i++) {
        if (w->guest[i]) {
            net_link_flush(w->guest[i]);
        }
    }
}

/* Opens the host and `guests` guests with identities 1..n and waits for every CONNECTED. */
static void world_open(world_t *w, const net_link_sim_config_t *config, uint32_t guests) {
    memset(w, 0, sizeof(*w));
    w->sim = net_link_sim_create(config);
    TEST_ASSERT_NOT_NULL(w->sim);
    w->host = net_link_sim_host(w->sim, HOST_ID, guests);
    TEST_ASSERT_NOT_NULL(w->host);
    w->guests = guests;
    for (uint32_t i = 0; i < guests; i++) {
        w->guest[i] = net_link_sim_guest(w->sim, i + 1);
        TEST_ASSERT_NOT_NULL(w->guest[i]);
    }
    net_link_event_t events[BATCH];
    for (int step = 0; step < 1000; step++) {
        world_flush(w);
        net_link_sim_advance(w->sim, 0.001);
        const uint32_t n = net_link_poll(w->host, events, BATCH);
        for (uint32_t e = 0; e < n; e++) {
            TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_CONNECTED, events[e].type);
            w->at_host[events[e].identity - 1] = events[e].peer;
        }
        bool all = true;
        for (uint32_t i = 0; i < guests; i++) {
            if (!w->at_guest[i] && net_link_poll(w->guest[i], events, BATCH) == 1) {
                TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_CONNECTED, events[0].type);
                w->at_guest[i] = events[0].peer;
            }
            all = all && w->at_guest[i] && w->at_host[i];
        }
        if (all) {
            return;
        }
    }
    TEST_FAIL_MESSAGE("guests did not connect");
}

static void world_close(world_t *w) { net_link_sim_destroy(w->sim); }

static void set_both(world_t *w, uint32_t guest, const net_link_conditions_t *conditions) {
    net_link_sim_set_conditions(w->sim, w->guest[guest], NET_LINK_SIM_TO_HOST, conditions);
    net_link_sim_set_conditions(w->sim, w->guest[guest], NET_LINK_SIM_TO_GUEST, conditions);
}

static int64_t now_ms(const world_t *w) { return (int64_t)(net_link_sim_time(w->sim) * 1000.0 + 0.5); }

/* Receives reliable sequences per lane and fails on a gap, a repeat or a reorder. */
typedef struct lane_check {
    uint32_t next[NET_LINK_LANES_MAX];
    uint32_t received;
} lane_check_t;

static void lane_check_take(lane_check_t *check, const net_link_event_t *event) {
    TEST_ASSERT_TRUE(event->reliable);
    TEST_ASSERT_EQUAL_UINT32(check->next[event->lane], get32(event->data + 4));
    check->next[event->lane]++;
    check->received++;
}

// #endregion

static void test_a_stalled_reliable_lane_holds_only_itself(void) {
    world_t w;
    net_link_sim_config_t config = config_of(3, 1, 2);
    config.lanes.priority[1] = 0;
    world_open(&w, &config, 1);
    const net_link_conditions_t lossy = {.delay_ms = 20, .loss = 1, .retransmit_ms = 300};
    const net_link_conditions_t clean = {.delay_ms = 20, .retransmit_ms = 300};
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, &lossy);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, 0, 16));
    net_link_flush(w.host);
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, &clean);
    const int64_t sent_ms = now_ms(&w);
    for (uint32_t seq = 1; seq < 3; seq++) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, seq, 16));
    }
    for (uint32_t seq = 0; seq < 2; seq++) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 1, NET_LINK_SEND_RELIABLE, 1, seq, 16));
    }
    net_link_flush(w.host);
    lane_check_t check = {0};
    int64_t arrived_ms[2][3] = {{0}};
    net_link_event_t events[BATCH];
    for (int step = 0; step < 600; step++) {
        net_link_sim_advance(w.sim, 0.001);
        const uint32_t n = net_link_poll(w.guest[0], events, BATCH);
        for (uint32_t e = 0; e < n; e++) {
            TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_MESSAGE, events[e].type);
            arrived_ms[events[e].lane][get32(events[e].data + 4)] = now_ms(&w) - sent_ms;
            lane_check_take(&check, &events[e]);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(5, check.received);
    TEST_ASSERT_TRUE(arrived_ms[1][0] >= 20 && arrived_ms[1][1] < 100);
    TEST_ASSERT_TRUE(arrived_ms[0][0] >= 300);
    TEST_ASSERT_TRUE(arrived_ms[0][1] >= arrived_ms[0][0] && arrived_ms[0][2] >= arrived_ms[0][1]);
    world_close(&w);
}

static void test_reliable_survives_a_hostile_path_exactly_once_in_order(void) {
    world_t w;
    const net_link_sim_config_t config = config_of(11, 2, 3);
    world_open(&w, &config, 2);
    const net_link_conditions_t hostile = {.delay_ms = 30, .jitter_ms = 40, .loss = 0.3F, .duplicate = 0.3F, .reorder = 0.3F, .reorder_ms = 50};
    set_both(&w, 0, &hostile);
    set_both(&w, 1, &hostile);
    enum { PER_DIRECTION = 1500, PER_TICK = 6 };
    /* [guest][0]: host to guest, [guest][1]: guest to host. */
    uint32_t sent[2][2][3] = {{{0}}};
    uint32_t total_sent[2][2] = {{0}};
    lane_check_t checks[2][2];
    memset(checks, 0, sizeof(checks));
    uint32_t noise = 0;
    net_link_event_t events[BATCH];
    bool done = false;
    for (int tick = 0; tick < 6000 && !done; tick++) {
        if (tick == 200) {
            net_link_sim_set_cut(w.sim, w.guest[0], true);
        } else if (tick == 250) {
            net_link_sim_set_cut(w.sim, w.guest[0], false);
        }
        for (uint32_t g = 0; g < 2; g++) {
            for (uint32_t side = 0; side < 2; side++) {
                net_link_t *from = side == 0 ? w.host : w.guest[g];
                const net_link_peer_t to = side == 0 ? w.at_host[g] : w.at_guest[g];
                for (uint32_t k = 0; k < PER_TICK && total_sent[g][side] < PER_DIRECTION; k++) {
                    const uint8_t lane = (uint8_t)((total_sent[g][side] + k) % 3);
                    const net_link_send_result_t result = send_tagged(from, to, lane, NET_LINK_SEND_RELIABLE, lane, sent[g][side][lane], 8 + (sent[g][side][lane] * 13U) % 120U);
                    if (result == NET_LINK_SEND_FULL) {
                        break;
                    }
                    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, result);
                    sent[g][side][lane]++;
                    total_sent[g][side]++;
                }
                (void)send_tagged(from, to, 2, NET_LINK_SEND_UNRELIABLE, 99, noise++, 24);
            }
        }
        world_flush(&w);
        net_link_sim_advance(w.sim, 0.01);
        uint32_t n = net_link_poll(w.host, events, BATCH);
        for (uint32_t e = 0; e < n; e++) {
            TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_MESSAGE, events[e].type);
            if (events[e].reliable) {
                lane_check_take(&checks[events[e].identity - 1][1], &events[e]);
            }
        }
        for (uint32_t g = 0; g < 2; g++) {
            n = net_link_poll(w.guest[g], events, BATCH);
            for (uint32_t e = 0; e < n; e++) {
                TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_MESSAGE, events[e].type);
                if (events[e].reliable) {
                    lane_check_take(&checks[g][0], &events[e]);
                }
            }
        }
        done = true;
        for (uint32_t g = 0; g < 2; g++) {
            done = done && checks[g][0].received == PER_DIRECTION && checks[g][1].received == PER_DIRECTION;
        }
    }
    for (uint32_t g = 0; g < 2; g++) {
        for (uint32_t side = 0; side < 2; side++) {
            TEST_ASSERT_EQUAL_UINT32(PER_DIRECTION, checks[g][side].received);
            for (uint32_t lane = 0; lane < 3; lane++) {
                TEST_ASSERT_EQUAL_UINT32(sent[g][side][lane], checks[g][side].next[lane]);
            }
        }
    }
    world_close(&w);
}

static void test_unreliable_loss_follows_the_setting(void) {
    const float settings[] = {0.0F, 0.1F, 0.3F, 0.6F};
    enum { COUNT = 4000, PER_TICK = 100 };
    for (uint32_t s = 0; s < sizeof(settings) / sizeof(settings[0]); s++) {
        world_t w;
        const net_link_sim_config_t config = config_of(100 + s, 1, 1);
        world_open(&w, &config, 1);
        const net_link_conditions_t lossy = {.delay_ms = 10, .loss = settings[s]};
        net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, &lossy);
        uint32_t received = 0;
        net_link_event_t events[BATCH];
        for (uint32_t tick = 0; tick < COUNT / PER_TICK + 10; tick++) {
            for (uint32_t k = 0; k < PER_TICK && tick * PER_TICK + k < COUNT; k++) {
                TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_UNRELIABLE, 0, tick * PER_TICK + k, 16));
            }
            net_link_flush(w.host);
            net_link_sim_advance(w.sim, 0.01);
            uint32_t n;
            while ((n = net_link_poll(w.guest[0], events, BATCH)) > 0) {
                received += n;
            }
        }
        const double lost = 1.0 - (double)received / (double)COUNT;
        if (settings[s] == 0.0F) {
            TEST_ASSERT_EQUAL_UINT32(COUNT, received);
        } else {
            /* Four standard deviations of a 4000-packet sample at the widest share. */
            TEST_ASSERT_TRUE(lost > (double)settings[s] - 0.032 && lost < (double)settings[s] + 0.032);
        }
        world_close(&w);
    }
}

typedef struct trace {
    uint64_t all;
    uint64_t guest_a;
} trace_t;

static uint64_t fnv(uint64_t hash, const void *data, size_t size) {
    const uint8_t *bytes = data;
    for (size_t i = 0; i < size; i++) {
        hash = (hash ^ bytes[i]) * 0x100000001B3ULL;
    }
    return hash;
}

static uint64_t fold_event(uint64_t hash, int64_t time_ms, uint32_t where, const net_link_event_t *event) {
    const uint64_t fields[] = {(uint64_t)time_ms, where, (uint64_t)event->type, event->identity, event->reason, event->lane, event->reliable, event->size};
    hash = fnv(hash, fields, sizeof(fields));
    return event->type == NET_LINK_EVENT_MESSAGE ? fnv(hash, event->data, event->size) : hash;
}

static trace_t run_script(uint64_t seed, int b_variant) {
    world_t w;
    const net_link_sim_config_t config = config_of(seed, 2, 2);
    memset(&w, 0, sizeof(w));
    w.sim = net_link_sim_create(&config);
    w.host = net_link_sim_host(w.sim, HOST_ID, 2);
    w.guests = 2;
    w.guest[0] = net_link_sim_guest(w.sim, 1);
    w.guest[1] = net_link_sim_guest(w.sim, 2);
    const net_link_conditions_t a = {.delay_ms = 25, .jitter_ms = 30, .loss = 0.2F, .duplicate = 0.2F, .reorder = 0.2F, .reorder_ms = 15};
    const net_link_conditions_t b0 = {.delay_ms = 40, .loss = 0.1F};
    const net_link_conditions_t b1 = {.delay_ms = 90, .jitter_ms = 50, .loss = 0.4F, .duplicate = 0.5F};
    set_both(&w, 0, &a);
    set_both(&w, 1, b_variant ? &b1 : &b0);
    trace_t trace = {0xCBF29CE484222325ULL, 0xCBF29CE484222325ULL};
    net_link_event_t events[BATCH];
    for (uint32_t tick = 0; tick < 300; tick++) {
        for (uint32_t g = 0; g < 2; g++) {
            if (w.at_host[g]) {
                (void)send_tagged(w.host, w.at_host[g], 1, NET_LINK_SEND_UNRELIABLE, g, tick, 8 + tick % 50);
                (void)send_tagged(w.host, w.at_host[g], 0, NET_LINK_SEND_RELIABLE, g, tick, 12);
            }
            if (w.at_guest[g]) {
                (void)send_tagged(w.guest[g], w.at_guest[g], (uint8_t)(tick % 2), tick % 3 ? (uint32_t)NET_LINK_SEND_UNRELIABLE : (uint32_t)NET_LINK_SEND_RELIABLE, 10 + g, tick, 20);
            }
        }
        world_flush(&w);
        net_link_sim_advance(w.sim, 0.016);
        const int64_t time_ms = now_ms(&w);
        uint32_t n = net_link_poll(w.host, events, BATCH);
        for (uint32_t e = 0; e < n; e++) {
            if (events[e].type == NET_LINK_EVENT_CONNECTED) {
                w.at_host[events[e].identity - 1] = events[e].peer;
            }
            trace.all = fold_event(trace.all, time_ms, 0, &events[e]);
            if (events[e].identity == 1) {
                trace.guest_a = fold_event(trace.guest_a, time_ms, 0, &events[e]);
            }
        }
        for (uint32_t g = 0; g < 2; g++) {
            n = net_link_poll(w.guest[g], events, BATCH);
            for (uint32_t e = 0; e < n; e++) {
                if (events[e].type == NET_LINK_EVENT_CONNECTED) {
                    w.at_guest[g] = events[e].peer;
                }
                trace.all = fold_event(trace.all, time_ms, g + 1, &events[e]);
                if (g == 0) {
                    trace.guest_a = fold_event(trace.guest_a, time_ms, 1, &events[e]);
                }
            }
        }
    }
    world_close(&w);
    return trace;
}

static void test_one_seed_gives_one_trace_and_paths_do_not_share_chance(void) {
    const trace_t first = run_script(5, 0), again = run_script(5, 0);
    TEST_ASSERT_EQUAL_UINT64(first.all, again.all);
    TEST_ASSERT_EQUAL_UINT64(first.guest_a, again.guest_a);
    const trace_t other_seed = run_script(6, 0);
    TEST_ASSERT_NOT_EQUAL_UINT64(first.all, other_seed.all);
    TEST_ASSERT_NOT_EQUAL_UINT64(first.guest_a, other_seed.guest_a);
    const trace_t other_b = run_script(5, 1);
    TEST_ASSERT_NOT_EQUAL_UINT64(first.all, other_b.all);
    TEST_ASSERT_EQUAL_UINT64(first.guest_a, other_b.guest_a);
}

static void test_nothing_leaves_before_the_flush(void) {
    world_t w;
    const net_link_sim_config_t config = config_of(1, 1, 1);
    world_open(&w, &config, 1);
    for (uint32_t seq = 0; seq < 3; seq++) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, seq, 16));
    }
    net_link_event_t events[BATCH];
    for (int step = 0; step < 100; step++) {
        net_link_sim_advance(w.sim, 0.01);
        TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(w.guest[0], events, BATCH));
    }
    net_link_flush(w.host);
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(w.guest[0], events, BATCH));
    net_link_sim_advance(w.sim, 0.0);
    TEST_ASSERT_EQUAL_UINT32(3, net_link_poll(w.guest[0], events, BATCH));
    world_close(&w);
}

static void test_connect_takes_one_delay_each_way(void) {
    const net_link_sim_config_t config = config_of(1, 1, 1);
    net_link_sim_t *sim = net_link_sim_create(&config);
    net_link_t *host = net_link_sim_host(sim, HOST_ID, 1);
    net_link_t *guest = net_link_sim_guest(sim, 1);
    net_link_sim_set_conditions(sim, guest, NET_LINK_SIM_TO_HOST, &(net_link_conditions_t){.delay_ms = 30});
    net_link_sim_set_conditions(sim, guest, NET_LINK_SIM_TO_GUEST, &(net_link_conditions_t){.delay_ms = 70});
    net_link_flush(guest);
    int host_ms = -1, guest_ms = -1;
    net_link_event_t event;
    for (int ms = 1; ms <= 200; ms++) {
        net_link_sim_advance(sim, 0.001);
        if (net_link_poll(host, &event, 1) == 1) {
            TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_CONNECTED, event.type);
            host_ms = ms;
        }
        if (net_link_poll(guest, &event, 1) == 1) {
            TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_CONNECTED, event.type);
            TEST_ASSERT_EQUAL_UINT64(HOST_ID, event.identity);
            guest_ms = ms;
        }
    }
    TEST_ASSERT_EQUAL_INT(30, host_ms);
    TEST_ASSERT_EQUAL_INT(100, guest_ms);
    net_link_sim_destroy(sim);
}

typedef struct delay_probe {
    int64_t earliest_ms;
    int64_t latest_unreliable_ms;
    uint32_t first_low;  /* first copies of unreliable messages within 10 ms of the delay */
    uint32_t first_high; /* and 20..31 ms past it */
    uint32_t received;
} delay_probe_t;

/* Sends 200 messages, one per millisecond, alternating unreliable and reliable, under `conditions`. */
static delay_probe_t probe_delays(const net_link_conditions_t *conditions) {
    world_t w;
    const net_link_sim_config_t config = config_of(9, 1, 2);
    world_open(&w, &config, 1);
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, conditions);
    enum { COUNT = 200 };
    int64_t sent_ms[COUNT];
    bool seen[COUNT] = {false};
    delay_probe_t probe = {.earliest_ms = INT64_MAX};
    net_link_event_t events[BATCH];
    for (uint32_t step = 0; step < COUNT + 300; step++) {
        if (step < COUNT) {
            sent_ms[step] = now_ms(&w);
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], (uint8_t)(step % 2), step % 2 ? (uint32_t)NET_LINK_SEND_RELIABLE : 0U, 0, step, 16));
            net_link_flush(w.host);
        }
        net_link_sim_advance(w.sim, 0.001);
        uint32_t n;
        while ((n = net_link_poll(w.guest[0], events, BATCH)) > 0) {
            for (uint32_t e = 0; e < n; e++) {
                const uint32_t seq = get32(events[e].data + 4);
                const int64_t took = now_ms(&w) - sent_ms[seq];
                probe.earliest_ms = took < probe.earliest_ms ? took : probe.earliest_ms;
                if (!events[e].reliable) {
                    probe.latest_unreliable_ms = took > probe.latest_unreliable_ms ? took : probe.latest_unreliable_ms;
                    if (!seen[seq]) {
                        const int64_t past = took - (int64_t)conditions->delay_ms;
                        if (past < 10) {
                            probe.first_low++;
                        } else if (past >= 20 && past <= 31) {
                            probe.first_high++;
                        }
                    }
                }
                seen[seq] = true;
                probe.received++;
            }
        }
    }
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        TEST_ASSERT_TRUE(seen[seq]);
    }
    world_close(&w);
    return probe;
}

static void test_nothing_arrives_before_the_delay_not_even_a_copy_or_a_held_packet(void) {
    const net_link_conditions_t messy = {.delay_ms = 80, .jitter_ms = 30, .duplicate = 0.5F, .reorder = 0.3F, .reorder_ms = 40};
    const delay_probe_t probe = probe_delays(&messy);
    TEST_ASSERT_TRUE(probe.earliest_ms >= 80);
    TEST_ASSERT_TRUE(probe.latest_unreliable_ms <= 80 + 30 + 40 + 1);
    TEST_ASSERT_TRUE(probe.received > 200 + 20); /* copies did arrive */
}

static void test_jitter_spreads_arrival_times(void) {
    const net_link_conditions_t jittery = {.delay_ms = 80, .jitter_ms = 30};
    const delay_probe_t probe = probe_delays(&jittery);
    TEST_ASSERT_TRUE(probe.earliest_ms >= 80);
    TEST_ASSERT_TRUE(probe.latest_unreliable_ms <= 111);
    /* An even draw over 30 ms puts about a third of 100 unreliable messages in each band. */
    TEST_ASSERT_TRUE(probe.first_low >= 15 && probe.first_low <= 55);
    TEST_ASSERT_TRUE(probe.first_high >= 15 && probe.first_high <= 55);
}

static void test_duplicates_and_overtaking_touch_only_unreliable(void) {
    world_t w;
    const net_link_sim_config_t config = config_of(21, 1, 2);
    world_open(&w, &config, 1);
    const net_link_conditions_t messy = {.delay_ms = 20, .jitter_ms = 30, .duplicate = 0.3F, .reorder = 0.3F, .reorder_ms = 40};
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, &messy);
    enum { COUNT = 500 };
    uint32_t copies[COUNT] = {0};
    uint32_t overtaken = 0, highest = 0;
    lane_check_t reliable = {0};
    net_link_event_t events[BATCH];
    for (uint32_t step = 0; step < COUNT + 200; step++) {
        if (step < COUNT) {
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_UNRELIABLE, 0, step, 16));
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 1, NET_LINK_SEND_RELIABLE, 1, step, 16));
            net_link_flush(w.host);
        }
        net_link_sim_advance(w.sim, 0.002);
        const uint32_t n = net_link_poll(w.guest[0], events, BATCH);
        for (uint32_t e = 0; e < n; e++) {
            const uint32_t seq = get32(events[e].data + 4);
            if (events[e].reliable) {
                lane_check_take(&reliable, &events[e]);
                continue;
            }
            copies[seq]++;
            if (seq < highest) {
                overtaken++;
            }
            highest = seq > highest ? seq : highest;
        }
    }
    uint32_t doubled = 0;
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        TEST_ASSERT_TRUE(copies[seq] >= 1 && copies[seq] <= 2);
        if (copies[seq] == 2) {
            doubled++;
        }
    }
    TEST_ASSERT_TRUE(doubled > COUNT / 10 && doubled < COUNT / 2);
    TEST_ASSERT_TRUE(overtaken > COUNT / 20);
    TEST_ASSERT_EQUAL_UINT32(COUNT, reliable.received);
    world_close(&w);
}

static void test_rate_limit_holds_and_the_priority_lane_goes_first(void) {
    world_t w;
    const net_link_sim_config_t config = config_of(4, 1, 2);
    world_open(&w, &config, 1);
    const net_link_conditions_t narrow = {.rate_bytes_per_s = 20000};
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, &narrow);
    enum { BULK = 200, BULK_SIZE = 500, URGENT_SIZE = 100 };
    for (uint32_t seq = 0; seq < BULK; seq++) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 1, NET_LINK_SEND_RELIABLE, 1, seq, BULK_SIZE));
    }
    net_link_flush(w.host);
    const int64_t start_ms = now_ms(&w);
    int64_t urgent_sent_ms[64];
    uint32_t urgent_sent = 0, urgent_received = 0, bulk_received = 0;
    uint64_t window_bytes = 0;
    int64_t worst_urgent_ms = 0;
    net_link_event_t events[BATCH];
    for (int step = 0; step < 3000; step++) {
        const int64_t t = now_ms(&w) - start_ms;
        if (t % 50 == 0 && t < 2000) {
            urgent_sent_ms[urgent_sent] = now_ms(&w);
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, urgent_sent++, URGENT_SIZE));
            net_link_flush(w.host);
        }
        net_link_sim_advance(w.sim, 0.001);
        const uint32_t n = net_link_poll(w.guest[0], events, BATCH);
        for (uint32_t e = 0; e < n; e++) {
            const int64_t at = now_ms(&w) - start_ms;
            if (at > 1000 && at <= 3000) {
                window_bytes += events[e].size;
            }
            if (events[e].lane == 0) {
                const int64_t took = now_ms(&w) - urgent_sent_ms[get32(events[e].data + 4)];
                worst_urgent_ms = took > worst_urgent_ms ? took : worst_urgent_ms;
                urgent_received++;
            } else {
                bulk_received++;
            }
        }
        if (t == 1500) {
            net_link_stats_t stats;
            TEST_ASSERT_TRUE(net_link_stats(w.host, w.at_host[0], &stats));
            TEST_ASSERT_TRUE(stats.out_bytes_per_s > 16000.0F && stats.out_bytes_per_s < 24000.0F);
            TEST_ASSERT_TRUE(stats.pending_reliable_bytes > 0);
        }
    }
    /* Two seconds at 20000 B/s; the band covers the bucket's burst and one message of debt. */
    TEST_ASSERT_TRUE(window_bytes > 36000 && window_bytes < 44000);
    TEST_ASSERT_EQUAL_UINT32(urgent_sent, urgent_received);
    TEST_ASSERT_TRUE(worst_urgent_ms <= 40);
    TEST_ASSERT_TRUE(bulk_received > 50 && bulk_received < BULK);
    world_close(&w);
}

static void test_a_receiver_that_does_not_poll_pushes_back(void) {
    world_t w;
    net_link_sim_config_t config = config_of(2, 1, 1);
    config.reliable_queue_bytes = 20000;
    config.unreliable_queue_bytes = 4000;
    config.queue_messages = 300;
    config.max_reliable_bytes = 1000;
    config.max_unreliable_bytes = 1000;
    world_open(&w, &config, 1);
    uint32_t unreliable_taken = 0, reliable_taken = 0;
    while (send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_UNRELIABLE, 0, unreliable_taken, 100) == NET_LINK_SEND_OK) {
        unreliable_taken++;
        TEST_ASSERT_TRUE(unreliable_taken <= 40);
    }
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_DROPPED, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_UNRELIABLE, 0, 0, 100));
    while (send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, reliable_taken, 100) == NET_LINK_SEND_OK) {
        reliable_taken++;
        TEST_ASSERT_TRUE(reliable_taken <= 200);
    }
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_FULL, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, 0, 100));
    TEST_ASSERT_EQUAL_UINT32(40, unreliable_taken);
    TEST_ASSERT_EQUAL_UINT32(200, reliable_taken);
    for (int step = 0; step < 50; step++) {
        net_link_flush(w.host);
        net_link_sim_advance(w.sim, 0.01);
    }
    net_link_stats_t stats;
    TEST_ASSERT_TRUE(net_link_stats(w.host, w.at_host[0], &stats));
    TEST_ASSERT_EQUAL_UINT32(20000, stats.pending_reliable_bytes);
    TEST_ASSERT_EQUAL_UINT32(4000, stats.pending_unreliable_bytes);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_FULL, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, 0, 100));
    net_link_event_t events[BATCH];
    uint32_t received = 0, n;
    while ((n = net_link_poll(w.guest[0], events, BATCH)) > 0) {
        received += n;
    }
    TEST_ASSERT_EQUAL_UINT32(unreliable_taken + reliable_taken, received);
    TEST_ASSERT_TRUE(net_link_stats(w.host, w.at_host[0], &stats));
    TEST_ASSERT_EQUAL_UINT32(0, stats.pending_reliable_bytes);
    TEST_ASSERT_EQUAL_UINT32(0, stats.pending_unreliable_bytes);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, reliable_taken, 100));
    /* The message count limits both kinds together. */
    uint32_t small = 1;
    while (send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, reliable_taken + small, 8) == NET_LINK_SEND_OK) {
        small++;
    }
    TEST_ASSERT_EQUAL_UINT32(config.queue_messages, small);
    world_close(&w);
}

static void test_a_short_cut_holds_reliable_and_loses_unreliable(void) {
    world_t w;
    net_link_sim_config_t config = config_of(8, 1, 1);
    config.timeout_s = 2.0F;
    world_open(&w, &config, 1);
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, &(net_link_conditions_t){.delay_ms = 10, .retransmit_ms = 100});
    net_link_sim_set_cut(w.sim, w.guest[0], true);
    for (uint32_t seq = 0; seq < 10; seq++) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, seq, 16));
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_UNRELIABLE, 0, seq, 16));
    }
    net_link_event_t events[BATCH];
    for (int step = 0; step < 100; step++) {
        net_link_flush(w.host);
        net_link_sim_advance(w.sim, 0.01);
        TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(w.guest[0], events, BATCH));
        TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(w.host, events, BATCH));
    }
    net_link_sim_set_cut(w.sim, w.guest[0], false);
    lane_check_t check = {0};
    for (int step = 0; step < 50; step++) {
        net_link_sim_advance(w.sim, 0.01);
        const uint32_t n = net_link_poll(w.guest[0], events, BATCH);
        for (uint32_t e = 0; e < n; e++) {
            TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_MESSAGE, events[e].type);
            lane_check_take(&check, &events[e]);
        }
        TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(w.host, events, BATCH));
    }
    TEST_ASSERT_EQUAL_UINT32(10, check.received);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.guest[0], w.at_guest[0], 0, NET_LINK_SEND_RELIABLE, 0, 0, 16));
    world_close(&w);
}

static void test_a_long_cut_times_out_both_ends(void) {
    world_t w;
    net_link_sim_config_t config = config_of(8, 1, 1);
    config.timeout_s = 1.0F;
    world_open(&w, &config, 1);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, 0, 16));
    net_link_sim_set_cut(w.sim, w.guest[0], true);
    net_link_flush(w.host);
    net_link_event_t events[BATCH];
    net_link_sim_advance(w.sim, 0.95);
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(w.host, events, BATCH));
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(w.guest[0], events, BATCH));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, 1, 16));
    net_link_sim_advance(w.sim, 0.1);
    TEST_ASSERT_EQUAL_UINT32(1, net_link_poll(w.host, events, BATCH));
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, events[0].type);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, events[0].reason);
    TEST_ASSERT_EQUAL_UINT64(1, events[0].identity);
    TEST_ASSERT_EQUAL_UINT32(1, net_link_poll(w.guest[0], events, BATCH));
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, events[0].type);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, events[0].reason);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, 2, 16));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, send_tagged(w.guest[0], w.at_guest[0], 0, NET_LINK_SEND_RELIABLE, 0, 2, 16));
    net_link_sim_set_cut(w.sim, w.guest[0], false);
    net_link_sim_advance(w.sim, 1.0);
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(w.guest[0], events, BATCH));
    world_close(&w);
}

static bool finite_float(float value) { return value == value && value < 1e30F && value > -1e30F; }

static void assert_stats_sane(const net_link_stats_t *stats) {
    TEST_ASSERT_TRUE(finite_float(stats->ping_ms) && finite_float(stats->quality));
    TEST_ASSERT_TRUE(finite_float(stats->out_bytes_per_s) && finite_float(stats->in_bytes_per_s));
    TEST_ASSERT_TRUE(stats->quality <= 1.0F && stats->out_bytes_per_s >= 0.0F && stats->in_bytes_per_s >= 0.0F);
}

static void test_stats_follow_the_conditions_and_stay_finite(void) {
    world_t w;
    const net_link_sim_config_t config = config_of(8, 1, 1);
    world_open(&w, &config, 1);
    net_link_stats_t host_view, guest_view;
    TEST_ASSERT_TRUE(net_link_stats(w.host, w.at_host[0], &host_view));
    assert_stats_sane(&host_view);
    TEST_ASSERT_TRUE(host_view.ping_ms >= 0.0F && host_view.quality == 1.0F);
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_HOST, &(net_link_conditions_t){.delay_ms = 30, .jitter_ms = 10});
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, &(net_link_conditions_t){.delay_ms = 50, .jitter_ms = 20, .loss = 0.25F});
    for (int step = 0; step < 100; step++) {
        (void)send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_UNRELIABLE, 0, (uint32_t)step, 100);
        (void)send_tagged(w.guest[0], w.at_guest[0], 0, NET_LINK_SEND_UNRELIABLE, 0, (uint32_t)step, 50);
        world_flush(&w);
        net_link_sim_advance(w.sim, 0.01);
        net_link_event_t events[BATCH];
        (void)net_link_poll(w.host, events, BATCH);
        (void)net_link_poll(w.guest[0], events, BATCH);
    }
    TEST_ASSERT_TRUE(net_link_stats(w.host, w.at_host[0], &host_view));
    TEST_ASSERT_TRUE(net_link_stats(w.guest[0], w.at_guest[0], &guest_view));
    assert_stats_sane(&host_view);
    assert_stats_sane(&guest_view);
    TEST_ASSERT_TRUE(host_view.ping_ms >= 80.0F && host_view.ping_ms <= 110.0F);
    TEST_ASSERT_TRUE(guest_view.ping_ms == host_view.ping_ms);
    TEST_ASSERT_TRUE(guest_view.quality > 0.7F && guest_view.quality < 0.8F);
    TEST_ASSERT_TRUE(host_view.quality == 1.0F);
    TEST_ASSERT_TRUE(host_view.out_bytes_per_s > 5000.0F && host_view.out_bytes_per_s < 15000.0F);
    TEST_ASSERT_TRUE(guest_view.in_bytes_per_s > 3000.0F && guest_view.in_bytes_per_s < host_view.out_bytes_per_s);
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST,
                                &(net_link_conditions_t){.delay_ms = NAN, .jitter_ms = -5, .loss = NAN, .duplicate = 7, .reorder_ms = 1e30F, .retransmit_ms = NAN});
    TEST_ASSERT_TRUE(net_link_stats(w.host, w.at_host[0], &host_view));
    assert_stats_sane(&host_view);
    net_link_sim_set_cut(w.sim, w.guest[0], true);
    TEST_ASSERT_TRUE(net_link_stats(w.guest[0], w.at_guest[0], &guest_view));
    TEST_ASSERT_TRUE(guest_view.quality == 0.0F);
    world_close(&w);
}

static void test_handles_are_never_handed_out_twice(void) {
    const net_link_sim_config_t config = config_of(1, 2, 1);
    net_link_sim_t *sim = net_link_sim_create(&config);
    net_link_t *host = net_link_sim_host(sim, HOST_ID, 2);
    enum { ROUNDS = 40 };
    net_link_peer_t seen[2 * ROUNDS];
    uint32_t seen_count = 0;
    net_link_event_t events[BATCH];
    for (uint32_t round = 0; round < ROUNDS; round++) {
        net_link_t *guest = net_link_sim_guest(sim, 77);
        TEST_ASSERT_NOT_NULL(guest);
        net_link_peer_t at_host = 0, at_guest = 0;
        for (int step = 0; step < 10 && !(at_host && at_guest); step++) {
            net_link_flush(guest);
            net_link_sim_advance(sim, 0.001);
            if (net_link_poll(host, events, BATCH) == 1) {
                TEST_ASSERT_EQUAL_UINT64(77, events[0].identity);
                at_host = events[0].peer;
            }
            if (net_link_poll(guest, events, BATCH) == 1) {
                at_guest = events[0].peer;
            }
        }
        TEST_ASSERT_TRUE(at_host && at_guest);
        for (uint32_t i = 0; i < seen_count; i++) {
            TEST_ASSERT_NOT_EQUAL_UINT32(seen[i], at_host);
            TEST_ASSERT_NOT_EQUAL_UINT32(seen[i], at_guest);
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, send_tagged(host, seen[i], 0, 0, 0, 0, 8));
        }
        seen[seen_count++] = at_host;
        seen[seen_count++] = at_guest;
        if (round % 2) {
            net_link_close(guest, at_guest, NET_LINK_REASON_APP_FIRST);
            net_link_flush(guest);
        }
        net_link_destroy(guest);
        net_link_sim_advance(sim, 0.001);
        TEST_ASSERT_EQUAL_UINT32(1, net_link_poll(host, events, BATCH));
        TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, events[0].type);
        TEST_ASSERT_EQUAL_UINT32(at_host, events[0].peer);
        TEST_ASSERT_EQUAL_UINT32(round % 2 ? NET_LINK_REASON_APP_FIRST : NET_LINK_REASON_REMOTE_CLOSE, events[0].reason);
    }
    net_link_sim_destroy(sim);
}

static void test_unreliable_traffic_obeys_the_rate(void) {
    world_t w;
    const net_link_sim_config_t config = config_of(14, 1, 1);
    world_open(&w, &config, 1);
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, &(net_link_conditions_t){.rate_bytes_per_s = 20000});
    uint64_t window_bytes = 0;
    net_link_event_t events[BATCH];
    for (uint32_t step = 0; step < 300; step++) {
        /* Five times the rate is offered; what the queue cannot hold is DROPPED. */
        for (uint32_t k = 0; k < 2; k++) {
            (void)send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_UNRELIABLE, 0, step * 2 + k, 500);
        }
        net_link_flush(w.host);
        net_link_sim_advance(w.sim, 0.01);
        uint32_t n;
        while ((n = net_link_poll(w.guest[0], events, BATCH)) > 0) {
            for (uint32_t e = 0; e < n; e++) {
                if (step >= 100) {
                    window_bytes += events[e].size;
                }
            }
        }
    }
    TEST_ASSERT_TRUE(window_bytes > 36000 && window_bytes < 44000);
    world_close(&w);
}

static void test_reliable_that_overtakes_the_accept_waits_for_connected(void) {
    net_link_sim_config_t config = config_of(15, 1, 2);
    net_link_sim_t *sim = net_link_sim_create(&config);
    net_link_t *host = net_link_sim_host(sim, HOST_ID, 1);
    net_link_t *guest = net_link_sim_guest(sim, 1);
    net_link_sim_set_conditions(sim, guest, NET_LINK_SIM_TO_GUEST, &(net_link_conditions_t){.delay_ms = 100});
    net_link_flush(guest);
    net_link_sim_advance(sim, 0.001);
    net_link_event_t events[BATCH];
    TEST_ASSERT_EQUAL_UINT32(1, net_link_poll(host, events, BATCH));
    const net_link_peer_t to_guest = events[0].peer;
    /* The accept is on its way with 100 ms; what the host sends now takes none. */
    net_link_sim_set_conditions(sim, guest, NET_LINK_SIM_TO_GUEST, NULL);
    for (uint32_t seq = 0; seq < 5; seq++) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(host, to_guest, 0, NET_LINK_SEND_RELIABLE, 0, seq, 16));
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(host, to_guest, 1, NET_LINK_SEND_RELIABLE, 1, seq, 16));
    }
    net_link_flush(host);
    net_link_sim_advance(sim, 0.05);
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(guest, events, BATCH));
    net_link_sim_advance(sim, 0.06);
    lane_check_t check = {0};
    bool connected = false;
    uint32_t n;
    while ((n = net_link_poll(guest, events, BATCH)) > 0) {
        for (uint32_t e = 0; e < n; e++) {
            if (events[e].type == NET_LINK_EVENT_CONNECTED) {
                TEST_ASSERT_FALSE(connected);
                TEST_ASSERT_EQUAL_UINT32(0, check.received);
                connected = true;
                continue;
            }
            lane_check_take(&check, &events[e]);
        }
    }
    TEST_ASSERT_TRUE(connected);
    TEST_ASSERT_EQUAL_UINT32(10, check.received);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(host, to_guest, 0, NET_LINK_SEND_RELIABLE, 0, 5, 16));
    net_link_flush(host);
    net_link_sim_advance(sim, 0.001);
    TEST_ASSERT_EQUAL_UINT32(1, net_link_poll(guest, events, BATCH));
    lane_check_take(&check, &events[0]);
    net_link_sim_destroy(sim);
}

static void test_a_guest_that_closed_hears_nothing_more(void) {
    world_t w;
    const net_link_sim_config_t config = config_of(16, 1, 1);
    world_open(&w, &config, 1);
    set_both(&w, 0, &(net_link_conditions_t){.delay_ms = 30});
    net_link_close(w.guest[0], w.at_guest[0], NET_LINK_REASON_APP_FIRST);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, send_tagged(w.guest[0], w.at_guest[0], 0, NET_LINK_SEND_RELIABLE, 0, 0, 16));
    TEST_ASSERT_FALSE(net_link_stats(w.guest[0], w.at_guest[0], &(net_link_stats_t){0}));
    TEST_ASSERT_EQUAL_UINT64(0, net_link_identity(w.guest[0], w.at_guest[0]));
    net_link_event_t events[BATCH];
    bool host_heard = false;
    for (uint32_t step = 0; step < 30; step++) {
        const net_link_send_result_t sent = send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_RELIABLE, 0, step, 16);
        TEST_ASSERT_TRUE(sent == (host_heard ? NET_LINK_SEND_NO_PEER : NET_LINK_SEND_OK));
        (void)send_tagged(w.host, w.at_host[0], 0, NET_LINK_SEND_UNRELIABLE, 0, step, 16);
        world_flush(&w);
        net_link_sim_advance(w.sim, 0.01);
        TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(w.guest[0], events, BATCH));
        const uint32_t n = net_link_poll(w.host, events, BATCH);
        if (n > 0) {
            TEST_ASSERT_EQUAL_UINT32(1, n);
            TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, events[0].type);
            TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_APP_FIRST, events[0].reason);
            host_heard = true;
        }
    }
    TEST_ASSERT_TRUE(host_heard);
    world_close(&w);
}

static void test_guests_learn_the_host_left_after_the_path_delay(void) {
    world_t w;
    const net_link_sim_config_t config = config_of(17, 2, 1);
    world_open(&w, &config, 2);
    net_link_sim_set_conditions(w.sim, w.guest[0], NET_LINK_SIM_TO_GUEST, &(net_link_conditions_t){.delay_ms = 50});
    net_link_sim_set_conditions(w.sim, w.guest[1], NET_LINK_SIM_TO_GUEST, &(net_link_conditions_t){.delay_ms = 20});
    net_link_destroy(w.host);
    int told_ms[2] = {-1, -1};
    net_link_event_t event;
    for (int ms = 1; ms <= 100; ms++) {
        net_link_sim_advance(w.sim, 0.001);
        for (uint32_t g = 0; g < 2; g++) {
            if (net_link_poll(w.guest[g], &event, 1) == 1) {
                TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, event.type);
                TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, event.reason);
                told_ms[g] = ms;
            }
        }
    }
    TEST_ASSERT_EQUAL_INT(50, told_ms[0]);
    TEST_ASSERT_EQUAL_INT(20, told_ms[1]);
    world_close(&w);
}

/* Reliable messages and the reason of a close reach the other side through loss, though the
   closing side destroys its link at once. */
static void test_a_close_lingers_past_destroy_on_a_lossy_path(void) {
    for (int closer = 0; closer < 2; closer++) {
        world_t w;
        const net_link_sim_config_t config = config_of(18 + (uint64_t)closer, 2, 1);
        world_open(&w, &config, 1);
        set_both(&w, 0, &(net_link_conditions_t){.delay_ms = 20, .jitter_ms = 10, .loss = 0.3F});
        net_link_t *from = closer ? w.host : w.guest[0];
        net_link_t *to = closer ? w.guest[0] : w.host;
        const net_link_peer_t peer = closer ? w.at_host[0] : w.at_guest[0];
        for (uint32_t seq = 0; seq < 20; seq++) {
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, send_tagged(from, peer, 0, NET_LINK_SEND_RELIABLE, 0, seq, 16));
        }
        net_link_close(from, peer, NET_LINK_REASON_APP_FIRST + 42);
        net_link_destroy(from);
        if (!closer) {
            /* The closed guest's place is still owed to its close. */
            TEST_ASSERT_NOT_NULL(net_link_sim_guest(w.sim, 2));
            TEST_ASSERT_NULL(net_link_sim_guest(w.sim, 3));
        }
        lane_check_t check = {0};
        uint32_t reason = 0;
        net_link_event_t events[BATCH];
        for (int step = 0; step < 300 && !reason; step++) {
            net_link_sim_advance(w.sim, 0.01);
            uint32_t n;
            while ((n = net_link_poll(to, events, BATCH)) > 0) {
                for (uint32_t e = 0; e < n; e++) {
                    if (events[e].type == NET_LINK_EVENT_DISCONNECTED) {
                        TEST_ASSERT_EQUAL_UINT32(0, reason);
                        reason = events[e].reason;
                    } else {
                        TEST_ASSERT_EQUAL_UINT32(0, reason);
                        lane_check_take(&check, &events[e]);
                    }
                }
            }
        }
        TEST_ASSERT_EQUAL_UINT32(20, check.received);
        TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_APP_FIRST + 42, reason);
        if (!closer) {
            TEST_ASSERT_NOT_NULL(net_link_sim_guest(w.sim, 3)); /* polled: the place is free */
        }
        world_close(&w);
    }
}

static void test_bad_configs_and_full_networks_are_refused(void) {
    TEST_ASSERT_NULL(net_link_sim_create(NULL));
    net_link_sim_config_t config = config_of(1, 0, 1);
    TEST_ASSERT_NULL(net_link_sim_create(&config));
    config = config_of(1, NET_LINK_SIM_GUESTS_MAX + 1, 1);
    TEST_ASSERT_NULL(net_link_sim_create(&config));
    config = config_of(1, 1, 0);
    TEST_ASSERT_NULL(net_link_sim_create(&config));
    config = config_of(1, 1, NET_LINK_LANES_MAX);
    config.lanes.count = NET_LINK_LANES_MAX + 1;
    TEST_ASSERT_NULL(net_link_sim_create(&config));
    config = config_of(1, 1, 1);
    config.timeout_s = -1.0F;
    TEST_ASSERT_NULL(net_link_sim_create(&config));
    config.timeout_s = NAN;
    TEST_ASSERT_NULL(net_link_sim_create(&config));
    config = config_of(1, 1, 1);
    config.reliable_queue_bytes = 100;
    TEST_ASSERT_NULL(net_link_sim_create(&config));
    config = config_of(1, 2, 1);
    net_link_sim_t *sim = net_link_sim_create(&config);
    TEST_ASSERT_NOT_NULL(sim);
    TEST_ASSERT_NULL(net_link_sim_host(sim, 1, 0));
    TEST_ASSERT_NULL(net_link_sim_host(sim, 1, 3));
    net_link_t *host = net_link_sim_host(sim, 1, 2);
    TEST_ASSERT_NOT_NULL(host);
    TEST_ASSERT_NULL(net_link_sim_host(sim, 2, 1));
    net_link_t *a = net_link_sim_guest(sim, 10);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(net_link_sim_guest(sim, 11));
    TEST_ASSERT_NULL(net_link_sim_guest(sim, 12));
    /* Accepted but not yet polled: a destroyed link must not hand out that CONNECTED. */
    net_link_flush(a);
    net_link_sim_advance(sim, 0.0);
    net_link_destroy(a);
    net_link_event_t event;
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(a, &event, 1));
    net_link_flush(a);
    net_link_destroy(a);
    TEST_ASSERT_NOT_NULL(net_link_sim_guest(sim, 12));
    /* A host link is not a guest: the calls that take a guest ignore it. */
    net_link_sim_set_conditions(sim, host, NET_LINK_SIM_TO_HOST, &(net_link_conditions_t){.loss = 1});
    net_link_sim_set_cut(sim, host, true);
    net_link_sim_advance(NULL, 1.0);
    net_link_sim_advance(sim, -1.0);
    net_link_sim_advance(sim, (double)NAN);
    TEST_ASSERT_TRUE(net_link_sim_time(sim) == 0.0);
    TEST_ASSERT_TRUE(net_link_sim_time(NULL) == 0.0);
    TEST_ASSERT_NULL(net_link_sim_host(NULL, 1, 1));
    TEST_ASSERT_NULL(net_link_sim_guest(NULL, 1));
    net_link_destroy(host);
    TEST_ASSERT_NOT_NULL(net_link_sim_host(sim, 3, 1));
    for (int i = 0; i < 10000; i++) {
        net_link_sim_advance(sim, 1e18);
    }
    const double late = net_link_sim_time(sim);
    TEST_ASSERT_TRUE(late > 1e12 && late < 1e14);
    net_link_sim_advance(sim, -5.0);
    net_link_sim_advance(sim, (double)NAN);
    TEST_ASSERT_TRUE(net_link_sim_time(sim) == late);
    net_link_sim_destroy(sim);
    net_link_sim_destroy(NULL);
}

static void test_a_guest_that_destroys_its_link_frees_the_place_after_the_host_polls(void) {
    world_t w;
    const net_link_sim_config_t config = config_of(1, 2, 1);
    world_open(&w, &config, 1);
    net_link_destroy(w.guest[0]);
    net_link_t *again = net_link_sim_guest(w.sim, 1);
    TEST_ASSERT_NOT_NULL(again);
    net_link_flush(again);
    net_link_sim_advance(w.sim, 0.01);
    net_link_event_t events[BATCH];
    TEST_ASSERT_EQUAL_UINT32(1, net_link_poll(again, events, BATCH));
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, events[0].type);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REFUSED, events[0].reason);
    TEST_ASSERT_EQUAL_UINT32(1, net_link_poll(w.host, events, BATCH));
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, events[0].type);
    net_link_destroy(again);
    again = net_link_sim_guest(w.sim, 1);
    net_link_flush(again);
    net_link_sim_advance(w.sim, 0.01);
    TEST_ASSERT_EQUAL_UINT32(1, net_link_poll(again, events, BATCH));
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_CONNECTED, events[0].type);
    world_close(&w);
}

int main(void) {
    UNITY_BEGIN();
    sim_env_t clean = {0};
    const net_link_contract_env_t clean_env = {.context = &clean,
                                               .begin = env_begin,
                                               .end = env_end,
                                               .open_host = env_host,
                                               .open_guest = env_guest,
                                               .pass = env_pass,
                                               .step_s = 0.005,
                                               .settle_s = 0.5,
                                               .fail_s = 0.5,
                                               .exact_identity = true,
                                               .refused_names_host = true,
                                               .unreliable_arrives = true,
                                               .guest_links = 4,
                                               .cut = env_cut,
                                               .timeout_s = 1.0};
    net_link_contract_use(&clean_env);
    net_link_contract_run_all();
    /* The same cases on a slow, jittery path: they must not lean on zero latency. */
    const net_link_conditions_t slow = {.delay_ms = 40, .jitter_ms = 25};
    sim_env_t lagged = {.conditions = &slow};
    const net_link_contract_env_t lagged_env = {.context = &lagged,
                                                .begin = env_begin,
                                                .end = env_end,
                                                .open_host = env_host,
                                                .open_guest = env_guest,
                                                .pass = env_pass,
                                                .step_s = 0.01,
                                                .settle_s = 1.0,
                                                .fail_s = 1.0,
                                                .exact_identity = true,
                                               .refused_names_host = true,
                                                .unreliable_arrives = true,
                                                .guest_links = 4,
                                                .cut = env_cut,
                                                .timeout_s = 1.0};
    net_link_contract_use(&lagged_env);
    net_link_contract_run_all();
    RUN_TEST(test_a_stalled_reliable_lane_holds_only_itself);
    RUN_TEST(test_reliable_survives_a_hostile_path_exactly_once_in_order);
    RUN_TEST(test_unreliable_loss_follows_the_setting);
    RUN_TEST(test_one_seed_gives_one_trace_and_paths_do_not_share_chance);
    RUN_TEST(test_nothing_leaves_before_the_flush);
    RUN_TEST(test_connect_takes_one_delay_each_way);
    RUN_TEST(test_nothing_arrives_before_the_delay_not_even_a_copy_or_a_held_packet);
    RUN_TEST(test_jitter_spreads_arrival_times);
    RUN_TEST(test_duplicates_and_overtaking_touch_only_unreliable);
    RUN_TEST(test_rate_limit_holds_and_the_priority_lane_goes_first);
    RUN_TEST(test_a_receiver_that_does_not_poll_pushes_back);
    RUN_TEST(test_a_short_cut_holds_reliable_and_loses_unreliable);
    RUN_TEST(test_a_long_cut_times_out_both_ends);
    RUN_TEST(test_stats_follow_the_conditions_and_stay_finite);
    RUN_TEST(test_handles_are_never_handed_out_twice);
    RUN_TEST(test_unreliable_traffic_obeys_the_rate);
    RUN_TEST(test_reliable_that_overtakes_the_accept_waits_for_connected);
    RUN_TEST(test_a_guest_that_closed_hears_nothing_more);
    RUN_TEST(test_guests_learn_the_host_left_after_the_path_delay);
    RUN_TEST(test_a_close_lingers_past_destroy_on_a_lossy_path);
    RUN_TEST(test_bad_configs_and_full_networks_are_refused);
    RUN_TEST(test_a_guest_that_destroys_its_link_frees_the_place_after_the_host_polls);
    return UNITY_END();
}
