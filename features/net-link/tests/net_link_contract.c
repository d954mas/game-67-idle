#include "net_link_contract.h"

#include "unity.h"

#include <string.h>

enum { LINKS_MAX = 8, LOG_MAX = 1024, PEERS_MAX = 16, BATCH = 64 };

typedef struct logged {
    net_link_event_type_t type;
    net_link_peer_t peer;
    uint64_t identity;
    uint32_t reason;
    uint8_t lane;
    bool reliable;
    uint32_t size;
    uint32_t tag;
    uint32_t seq;
    bool intact;
} logged_t;

typedef struct tracked {
    net_link_t *link;
    logged_t log[LOG_MAX];
    uint32_t count;
    bool overflow;
} tracked_t;

static const net_link_contract_env_t *env;
static tracked_t tracked[LINKS_MAX];
static uint32_t tracked_count;

void net_link_contract_use(const net_link_contract_env_t *use) { env = use; }

// #region helpers

static void put32(uint8_t *at, uint32_t value) {
    for (int i = 0; i < 4; i++) {
        at[i] = (uint8_t)(value >> (8 * i));
    }
}

static uint32_t get32(const uint8_t *at) { return (uint32_t)at[0] | ((uint32_t)at[1] << 8) | ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24); }

static uint8_t fill_byte(uint32_t tag, uint32_t seq, uint32_t i) { return (uint8_t)(seq * 31U + i * 7U + tag); }

/* `size` >= 8: a tag, a sequence number and a pattern that shows any damaged byte. */
static void make_message(uint8_t *out, uint32_t size, uint32_t tag, uint32_t seq) {
    put32(out, tag);
    put32(out + 4, seq);
    for (uint32_t i = 8; i < size; i++) {
        out[i] = fill_byte(tag, seq, i);
    }
}

static bool message_intact(const uint8_t *data, uint32_t size) {
    if (!data || size < 8) {
        return false;
    }
    const uint32_t tag = get32(data), seq = get32(data + 4);
    for (uint32_t i = 8; i < size; i++) {
        if (data[i] != fill_byte(tag, seq, i)) {
            return false;
        }
    }
    return true;
}

static uint32_t message_size(uint32_t seq) { return 8 + (seq * 37U) % 200U; }

static tracked_t *track(net_link_t *link) {
    TEST_ASSERT_NOT_NULL(link);
    TEST_ASSERT_TRUE(tracked_count < LINKS_MAX);
    tracked_t *t = &tracked[tracked_count++];
    memset(t, 0, sizeof(*t));
    t->link = link;
    return t;
}

static void untrack(tracked_t *t) { t->link = NULL; }

static void record(tracked_t *t, const net_link_event_t *event) {
    if (t->count >= LOG_MAX) {
        t->overflow = true;
        return;
    }
    logged_t *entry = &t->log[t->count++];
    entry->type = event->type;
    entry->peer = event->peer;
    entry->identity = event->identity;
    entry->reason = event->reason;
    entry->lane = event->lane;
    entry->reliable = event->reliable;
    entry->size = event->size;
    if (event->type == NET_LINK_EVENT_MESSAGE) {
        entry->intact = message_intact(event->data, event->size);
        entry->tag = event->size >= 8 ? get32(event->data) : 0;
        entry->seq = event->size >= 8 ? get32(event->data + 4) : 0;
    }
}

static void poll_all(void) {
    net_link_event_t events[BATCH];
    for (uint32_t i = 0; i < tracked_count; i++) {
        if (!tracked[i].link) {
            continue;
        }
        uint32_t n;
        while ((n = net_link_poll(tracked[i].link, events, BATCH)) > 0) {
            for (uint32_t e = 0; e < n; e++) {
                record(&tracked[i], &events[e]);
            }
        }
    }
}

static void tick(void) {
    for (uint32_t i = 0; i < tracked_count; i++) {
        if (tracked[i].link) {
            net_link_flush(tracked[i].link);
        }
    }
    env->pass(env->context, env->step_s);
    poll_all();
}

typedef bool (*condition_fn)(void);

static bool tick_until(condition_fn condition, double limit_s) {
    for (double waited = 0.0; waited < limit_s; waited += env->step_s) {
        tick();
        if (condition()) {
            return true;
        }
    }
    return condition();
}

static double quiet_s(void) { return env->quiet_s > 0.0 ? env->quiet_s : env->settle_s; }
static double silence_s(void) { return env->silence_s > 0.0 ? env->silence_s : env->settle_s; }

static void tick_for(double seconds) {
    for (double waited = 0.0; waited < seconds; waited += env->step_s) {
        tick();
    }
}

static const logged_t *find(const tracked_t *t, net_link_event_type_t type, uint32_t from) {
    for (uint32_t i = from; i < t->count; i++) {
        if (t->log[i].type == type) {
            return &t->log[i];
        }
    }
    return NULL;
}

static uint32_t count_type(const tracked_t *t, net_link_event_type_t type) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        if (t->log[i].type == type) {
            n++;
        }
    }
    return n;
}

/* Per peer: CONNECTED before any message, nothing after DISCONNECTED. */
static void assert_peer_order(const tracked_t *t) {
    net_link_peer_t peers[PEERS_MAX];
    uint8_t state[PEERS_MAX]; /* 1 connected, 2 disconnected */
    uint32_t known = 0;
    TEST_ASSERT_FALSE(t->overflow);
    for (uint32_t i = 0; i < t->count; i++) {
        const logged_t *e = &t->log[i];
        TEST_ASSERT_NOT_EQUAL_UINT32(NET_LINK_PEER_NONE, e->peer);
        uint32_t k = 0;
        while (k < known && peers[k] != e->peer) {
            k++;
        }
        if (k == known) {
            TEST_ASSERT_TRUE(known < PEERS_MAX);
            peers[known] = e->peer;
            state[known++] = 0;
        }
        TEST_ASSERT_NOT_EQUAL_UINT8_MESSAGE(2, state[k], "an event after DISCONNECTED");
        if (e->type == NET_LINK_EVENT_CONNECTED) {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(0, state[k], "CONNECTED twice");
            state[k] = 1;
        } else if (e->type == NET_LINK_EVENT_MESSAGE) {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, state[k], "a message before CONNECTED");
            TEST_ASSERT_TRUE(e->intact);
        } else {
            state[k] = 2;
        }
    }
}

static void case_begin(void) {
    tracked_count = 0;
    env->begin(env->context);
}

static void case_end(void) {
    for (uint32_t i = 0; i < tracked_count; i++) {
        assert_peer_order(&tracked[i]);
    }
    env->end(env->context);
    tracked_count = 0;
}

static tracked_t *host_t, *guest_t, *guest2_t;

static bool host_and_guest_connected(void) { return find(host_t, NET_LINK_EVENT_CONNECTED, 0) && find(guest_t, NET_LINK_EVENT_CONNECTED, 0); }

static void connect_pair(uint32_t max_peers) {
    host_t = track(env->open_host(env->context, 100, max_peers));
    guest_t = track(env->open_guest(env->context, 200));
    TEST_ASSERT_TRUE_MESSAGE(tick_until(host_and_guest_connected, env->settle_s), "no connect within settle_s");
}

static net_link_peer_t peer_of(const tracked_t *t) { return find(t, NET_LINK_EVENT_CONNECTED, 0)->peer; }

static void send_ok(net_link_t *link, net_link_peer_t peer, uint8_t lane, uint32_t flags, uint32_t tag, uint32_t seq) {
    uint8_t buffer[256];
    const uint32_t size = message_size(seq);
    make_message(buffer, size, tag, seq);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(link, peer, lane, flags, buffer, size));
}

// #endregion

static uint32_t expected_messages;
static bool all_messages_in(void) { return count_type(host_t, NET_LINK_EVENT_MESSAGE) >= expected_messages && count_type(guest_t, NET_LINK_EVENT_MESSAGE) >= expected_messages; }

static void contract_connect_reports_identities_on_both_ends(void) {
    case_begin();
    connect_pair(2);
    const logged_t *at_host = find(host_t, NET_LINK_EVENT_CONNECTED, 0), *at_guest = find(guest_t, NET_LINK_EVENT_CONNECTED, 0);
    TEST_ASSERT_EQUAL_UINT64(at_host->identity, net_link_identity(host_t->link, at_host->peer));
    TEST_ASSERT_EQUAL_UINT64(at_guest->identity, net_link_identity(guest_t->link, at_guest->peer));
    TEST_ASSERT_NOT_EQUAL_UINT64(0, at_host->identity);
    TEST_ASSERT_NOT_EQUAL_UINT64(0, at_guest->identity);
    if (env->exact_identity) {
        TEST_ASSERT_EQUAL_UINT64(200, at_host->identity);
        TEST_ASSERT_EQUAL_UINT64(100, at_guest->identity);
    }
    TEST_ASSERT_TRUE(net_link_lane_count(host_t->link) >= 1 && net_link_lane_count(host_t->link) <= NET_LINK_LANES_MAX);
    TEST_ASSERT_EQUAL_UINT8(net_link_lane_count(host_t->link), net_link_lane_count(guest_t->link));
    TEST_ASSERT_TRUE(net_link_max_unreliable(host_t->link) >= 8 && net_link_max_reliable(host_t->link) >= 8);
    net_link_stats_t stats;
    TEST_ASSERT_TRUE(net_link_stats(host_t->link, at_host->peer, &stats));
    TEST_ASSERT_TRUE(stats.ping_ms == stats.ping_ms && stats.quality <= 1.0F);
    TEST_ASSERT_TRUE(net_link_stats(guest_t->link, at_guest->peer, &stats));
    case_end();
}

static void contract_reliable_lanes_keep_order_and_arrive_once(void) {
    case_begin();
    connect_pair(2);
    const net_link_peer_t to_guest = peer_of(host_t), to_host = peer_of(guest_t);
    const uint8_t lanes = net_link_lane_count(host_t->link) < 4 ? net_link_lane_count(host_t->link) : 4;
    enum { PER_LANE = 50 };
    for (uint32_t seq = 0; seq < PER_LANE; seq++) {
        for (uint8_t lane = 0; lane < lanes; lane++) {
            send_ok(host_t->link, to_guest, lane, NET_LINK_SEND_RELIABLE, lane, seq);
            send_ok(guest_t->link, to_host, lane, NET_LINK_SEND_RELIABLE, lane, seq);
        }
        if (seq % 10 == 9) {
            tick();
        }
    }
    expected_messages = (uint32_t)PER_LANE * lanes;
    TEST_ASSERT_TRUE_MESSAGE(tick_until(all_messages_in, env->settle_s * 4), "reliable messages missing");
    tick_for(env->step_s * 4);
    const tracked_t *ends[2] = {host_t, guest_t};
    for (int side = 0; side < 2; side++) {
        uint32_t next[NET_LINK_LANES_MAX] = {0};
        for (uint32_t i = 0; i < ends[side]->count; i++) {
            const logged_t *e = &ends[side]->log[i];
            if (e->type != NET_LINK_EVENT_MESSAGE) {
                continue;
            }
            TEST_ASSERT_TRUE(e->reliable);
            TEST_ASSERT_EQUAL_UINT32(e->lane, e->tag);
            TEST_ASSERT_EQUAL_UINT32(next[e->lane], e->seq);
            TEST_ASSERT_EQUAL_UINT32(message_size(e->seq), e->size);
            next[e->lane]++;
        }
        for (uint8_t lane = 0; lane < lanes; lane++) {
            TEST_ASSERT_EQUAL_UINT32(PER_LANE, next[lane]);
        }
    }
    case_end();
}

static void contract_unreliable_arrives_whole_or_not_at_all(void) {
    case_begin();
    connect_pair(2);
    enum { COUNT = 40 };
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        send_ok(host_t->link, peer_of(host_t), 0, NET_LINK_SEND_UNRELIABLE, 7, seq);
        send_ok(guest_t->link, peer_of(guest_t), 0, NET_LINK_SEND_UNRELIABLE, 7, seq);
    }
    tick_for(quiet_s());
    const tracked_t *ends[2] = {host_t, guest_t};
    for (int side = 0; side < 2; side++) {
        bool seen[COUNT] = {false};
        for (uint32_t i = 0; i < ends[side]->count; i++) {
            const logged_t *e = &ends[side]->log[i];
            if (e->type == NET_LINK_EVENT_MESSAGE) {
                TEST_ASSERT_FALSE(e->reliable);
                TEST_ASSERT_EQUAL_UINT32(7, e->tag);
                TEST_ASSERT_TRUE(e->seq < COUNT);
                seen[e->seq] = true;
            }
        }
        for (uint32_t seq = 0; env->unreliable_arrives && seq < COUNT; seq++) {
            TEST_ASSERT_TRUE(seen[seq]);
        }
    }
    case_end();
}

static void contract_size_limits_hold_for_each_kind(void) {
    case_begin();
    connect_pair(2);
    const net_link_peer_t peer = peer_of(host_t);
    static uint8_t big[1 << 20];
    const uint32_t max_u = net_link_max_unreliable(host_t->link), max_r = net_link_max_reliable(host_t->link);
    TEST_ASSERT_TRUE(max_u <= sizeof(big) - 1 && max_r <= sizeof(big) - 1);
    make_message(big, max_r + 1, 3, 1);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_TOO_LARGE, net_link_send(host_t->link, peer, 0, NET_LINK_SEND_UNRELIABLE, big, max_u + 1));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_TOO_LARGE, net_link_send(host_t->link, peer, 0, NET_LINK_SEND_RELIABLE, big, max_r + 1));
    make_message(big, max_r, 3, 1);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(host_t->link, peer, 0, NET_LINK_SEND_RELIABLE, big, max_r));
    make_message(big, max_u, 4, 2);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(host_t->link, peer, 0, NET_LINK_SEND_UNRELIABLE, big, max_u));
    tick_for(quiet_s());
    bool reliable_in = false, unreliable_in = false;
    for (uint32_t i = 0; i < guest_t->count; i++) {
        const logged_t *e = &guest_t->log[i];
        if (e->type == NET_LINK_EVENT_MESSAGE) {
            reliable_in |= e->reliable && e->size == max_r && e->tag == 3;
            unreliable_in |= !e->reliable && e->size == max_u && e->tag == 4;
        }
    }
    TEST_ASSERT_TRUE(reliable_in);
    TEST_ASSERT_TRUE(unreliable_in || !env->unreliable_arrives);
    case_end();
}

static void contract_bad_arguments_are_refused_without_harm(void) {
    case_begin();
    connect_pair(2);
    net_link_t *host = host_t->link, *guest = guest_t->link;
    const net_link_peer_t to_guest = peer_of(host_t), to_host = peer_of(guest_t);
    const uint8_t payload[16] = {1, 2, 3};
    const uint8_t lanes = net_link_lane_count(host);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_INVALID, net_link_send(host, to_guest, lanes, NET_LINK_SEND_RELIABLE, payload, 8));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_INVALID, net_link_send(host, to_guest, 255, NET_LINK_SEND_UNRELIABLE, payload, 8));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_INVALID, net_link_send(host, to_guest, 0, 1U << 1, payload, 8));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_INVALID, net_link_send(host, to_guest, 0, NET_LINK_SEND_RELIABLE, NULL, 8));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_INVALID, net_link_send(host, to_guest, 0, NET_LINK_SEND_RELIABLE, payload, 0));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, net_link_send(host, NET_LINK_PEER_NONE, 0, NET_LINK_SEND_RELIABLE, payload, 8));
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, net_link_send(host, to_guest ^ 0x40000000U, 0, NET_LINK_SEND_RELIABLE, payload, 8));
    if (to_guest != to_host) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, net_link_send(guest, to_guest, 0, NET_LINK_SEND_RELIABLE, payload, 8));
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, net_link_send(host, to_host, 0, NET_LINK_SEND_RELIABLE, payload, 8));
    }
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_INVALID, net_link_send(NULL, to_guest, 0, NET_LINK_SEND_RELIABLE, payload, 8));
    net_link_event_t events[2];
    net_link_stats_t stats;
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(NULL, events, 2));
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(host, NULL, 2));
    TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(host, events, 0));
    net_link_flush(NULL);
    net_link_close(NULL, to_guest, 0);
    net_link_destroy(NULL);
    TEST_ASSERT_EQUAL_UINT32(0, net_link_max_unreliable(NULL));
    TEST_ASSERT_EQUAL_UINT32(0, net_link_max_reliable(NULL));
    TEST_ASSERT_EQUAL_UINT8(0, net_link_lane_count(NULL));
    TEST_ASSERT_FALSE(net_link_stats(NULL, to_guest, &stats));
    TEST_ASSERT_FALSE(net_link_stats(host, to_guest, NULL));
    TEST_ASSERT_FALSE(net_link_stats(host, NET_LINK_PEER_NONE, &stats));
    TEST_ASSERT_EQUAL_UINT64(0, net_link_identity(NULL, to_guest));
    TEST_ASSERT_EQUAL_UINT64(0, net_link_identity(host, NET_LINK_PEER_NONE));
    net_link_close(host, to_guest ^ 0x40000000U, NET_LINK_REASON_APP_FIRST);
    net_link_close(host, NET_LINK_PEER_NONE, NET_LINK_REASON_APP_FIRST);
    /* None of that touched the real connection. */
    send_ok(host, to_guest, 0, NET_LINK_SEND_RELIABLE, 9, 1);
    tick_for(silence_s());
    TEST_ASSERT_EQUAL_UINT32(1, count_type(guest_t, NET_LINK_EVENT_MESSAGE));
    TEST_ASSERT_NULL(find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0));
    case_end();
}

static bool all_three_connected(void) { return count_type(host_t, NET_LINK_EVENT_CONNECTED) >= 2 && find(guest_t, NET_LINK_EVENT_CONNECTED, 0) && find(guest2_t, NET_LINK_EVENT_CONNECTED, 0); }

static bool both_guests_disconnected(void) { return find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0) && find(guest2_t, NET_LINK_EVENT_DISCONNECTED, 0); }

static void skip_without_two_guests(void) {
    if (env->guest_links < 2) {
        TEST_IGNORE_MESSAGE("the environment holds one guest link");
    }
}

static void open_three(void) {
    host_t = track(env->open_host(env->context, 100, 2));
    guest_t = track(env->open_guest(env->context, 200));
    guest2_t = track(env->open_guest(env->context, 300));
    TEST_ASSERT_TRUE_MESSAGE(tick_until(all_three_connected, env->settle_s), "no connect within settle_s");
}

enum { HELLO_SEQ = 0xFFFF };

/* Identities may repeat on a real platform, so the host learns which peer is which from a hello. */
static net_link_peer_t host_peer_saying(uint32_t tag) {
    for (uint32_t i = 0; i < host_t->count; i++) {
        if (host_t->log[i].type == NET_LINK_EVENT_MESSAGE && host_t->log[i].tag == tag && host_t->log[i].seq == HELLO_SEQ) {
            return host_t->log[i].peer;
        }
    }
    return NET_LINK_PEER_NONE;
}

static bool hellos_in(void) { return host_peer_saying(61) != NET_LINK_PEER_NONE && host_peer_saying(62) != NET_LINK_PEER_NONE; }

static bool close_reported(void) { return find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0) && count_type(host_t, NET_LINK_EVENT_DISCONNECTED) >= 1; }

static void contract_close_delivers_reliable_first_then_the_reason(void) {
    skip_without_two_guests();
    case_begin();
    open_three();
    send_ok(guest_t->link, peer_of(guest_t), 0, NET_LINK_SEND_RELIABLE, 61, HELLO_SEQ);
    send_ok(guest2_t->link, peer_of(guest2_t), 0, NET_LINK_SEND_RELIABLE, 62, HELLO_SEQ);
    TEST_ASSERT_TRUE(tick_until(hellos_in, env->settle_s));
    const net_link_peer_t to_first = host_peer_saying(61);
    enum { COUNT = 20 };
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        send_ok(host_t->link, to_first, 0, NET_LINK_SEND_RELIABLE, 0, seq);
    }
    const uint32_t host_events_before = host_t->count;
    net_link_close(host_t->link, to_first, NET_LINK_REASON_APP_FIRST + 500);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, net_link_send(host_t->link, to_first, 0, NET_LINK_SEND_RELIABLE, "late", 4));
    TEST_ASSERT_FALSE(net_link_stats(host_t->link, to_first, &(net_link_stats_t){0}));
    TEST_ASSERT_EQUAL_UINT64(0, net_link_identity(host_t->link, to_first));
    net_link_close(guest2_t->link, peer_of(guest2_t), 7); /* not an app code: the host reads REMOTE_CLOSE */
    TEST_ASSERT_TRUE_MESSAGE(tick_until(close_reported, env->settle_s * 2), "close not reported");
    tick_for(silence_s());
    uint32_t got = 0;
    for (uint32_t i = 0; i < guest_t->count; i++) {
        const logged_t *e = &guest_t->log[i];
        if (e->type == NET_LINK_EVENT_MESSAGE) {
            TEST_ASSERT_EQUAL_UINT32(got++, e->seq);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(COUNT, got);
    const logged_t *last = &guest_t->log[guest_t->count - 1];
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, last->type);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_APP_FIRST + 500, last->reason);
    for (uint32_t i = host_events_before; i < host_t->count; i++) {
        TEST_ASSERT_NOT_EQUAL_UINT32(to_first, host_t->log[i].peer);
    }
    const logged_t *left = find(host_t, NET_LINK_EVENT_DISCONNECTED, host_events_before);
    TEST_ASSERT_NOT_NULL(left);
    TEST_ASSERT_EQUAL_UINT32(host_peer_saying(62), left->peer);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, left->reason);
    TEST_ASSERT_NULL(find(guest2_t, NET_LINK_EVENT_DISCONNECTED, 0));
    case_end();
}

static bool guest_failed(void) { return find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0) != NULL; }

static void contract_full_host_refuses_the_next_guest(void) {
    skip_without_two_guests();
    case_begin();
    connect_pair(1);
    tracked_t *first = guest_t;
    guest_t = track(env->open_guest(env->context, 300));
    TEST_ASSERT_TRUE_MESSAGE(tick_until(guest_failed, env->fail_s), "no refusal within fail_s");
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REFUSED, find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0)->reason);
    const uint64_t refused_by = find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0)->identity;
    if (!env->refused_names_host) {
        TEST_ASSERT_EQUAL_UINT64(0, refused_by);
    } else if (env->exact_identity) {
        TEST_ASSERT_EQUAL_UINT64(100, refused_by);
    } else {
        TEST_ASSERT_NOT_EQUAL_UINT64(0, refused_by);
    }
    TEST_ASSERT_NULL(find(guest_t, NET_LINK_EVENT_CONNECTED, 0));
    TEST_ASSERT_EQUAL_UINT32(1, count_type(host_t, NET_LINK_EVENT_CONNECTED));
    TEST_ASSERT_NULL(find(first, NET_LINK_EVENT_DISCONNECTED, 0));
    send_ok(first->link, peer_of(first), 0, NET_LINK_SEND_RELIABLE, 1, 1);
    case_end();
}

static void contract_nobody_hosting_is_unreachable(void) {
    case_begin();
    guest_t = track(env->open_guest(env->context, 200));
    TEST_ASSERT_TRUE_MESSAGE(tick_until(guest_failed, env->fail_s), "no failure within fail_s");
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_UNREACHABLE, find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0)->reason);
    TEST_ASSERT_EQUAL_UINT64(0, find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0)->identity);
    TEST_ASSERT_EQUAL_UINT32(1, guest_t->count);
    case_end();
}

static void contract_host_leaving_closes_every_guest(void) {
    skip_without_two_guests();
    case_begin();
    open_three();
    send_ok(host_t->link, find(host_t, NET_LINK_EVENT_CONNECTED, 0)->peer, 0, NET_LINK_SEND_RELIABLE, 1, 1);
    net_link_destroy(host_t->link);
    untrack(host_t);
    TEST_ASSERT_TRUE_MESSAGE(tick_until(both_guests_disconnected, env->settle_s * 2), "guests not told the host left");
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0)->reason);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, find(guest2_t, NET_LINK_EVENT_DISCONNECTED, 0)->reason);
    net_link_destroy(guest_t->link);
    untrack(guest_t);
    net_link_destroy(guest2_t->link);
    untrack(guest2_t);
    host_t = track(env->open_host(env->context, 101, 2));
    guest_t = track(env->open_guest(env->context, 400));
    TEST_ASSERT_TRUE_MESSAGE(tick_until(host_and_guest_connected, env->settle_s), "a new host takes no guest");
    case_end();
}

static bool host_saw_leave(void) { return find(host_t, NET_LINK_EVENT_DISCONNECTED, 0) != NULL; }
static bool host_saw_return(void) { return count_type(host_t, NET_LINK_EVENT_CONNECTED) >= 2 && find(guest_t, NET_LINK_EVENT_CONNECTED, 0); }

static void contract_returning_guest_is_a_new_peer(void) {
    case_begin();
    connect_pair(1);
    const logged_t first = *find(host_t, NET_LINK_EVENT_CONNECTED, 0);
    net_link_destroy(guest_t->link);
    untrack(guest_t);
    TEST_ASSERT_TRUE_MESSAGE(tick_until(host_saw_leave, env->settle_s * 2), "host not told the guest left");
    TEST_ASSERT_EQUAL_UINT32(first.peer, find(host_t, NET_LINK_EVENT_DISCONNECTED, 0)->peer);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, find(host_t, NET_LINK_EVENT_DISCONNECTED, 0)->reason);
    guest_t = track(env->open_guest(env->context, 200));
    TEST_ASSERT_TRUE_MESSAGE(tick_until(host_saw_return, env->settle_s), "the place was not freed");
    const logged_t *again = find(host_t, NET_LINK_EVENT_CONNECTED, 1);
    TEST_ASSERT_NOT_EQUAL_UINT32(first.peer, again->peer);
    TEST_ASSERT_EQUAL_UINT64(first.identity, again->identity);
    const uint8_t payload[8] = {0};
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, net_link_send(host_t->link, first.peer, 0, NET_LINK_SEND_RELIABLE, payload, 8));
    TEST_ASSERT_FALSE(net_link_stats(host_t->link, first.peer, &(net_link_stats_t){0}));
    TEST_ASSERT_EQUAL_UINT64(0, net_link_identity(host_t->link, first.peer));
    TEST_ASSERT_EQUAL_UINT64(first.identity, net_link_identity(host_t->link, again->peer));
    case_end();
}

static void contract_batch_payloads_stay_readable_until_the_next_poll(void) {
    case_begin();
    connect_pair(2);
    enum { COUNT = 30 };
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        send_ok(guest_t->link, peer_of(guest_t), 0, NET_LINK_SEND_RELIABLE, 5, seq);
    }
    uint32_t got = 0;
    net_link_event_t events[BATCH];
    for (double waited = 0.0; waited < env->settle_s * 2 && got < COUNT; waited += env->step_s) {
        net_link_flush(guest_t->link);
        env->pass(env->context, env->step_s);
        TEST_ASSERT_EQUAL_UINT32(0, net_link_poll(guest_t->link, events, BATCH));
        uint32_t n;
        while ((n = net_link_poll(host_t->link, events, BATCH)) > 0) {
            /* Read the whole batch first, then check every payload: none may be overwritten yet. */
            for (uint32_t e = 0; e < n; e++) {
                TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_MESSAGE, events[e].type);
            }
            for (uint32_t e = 0; e < n; e++) {
                TEST_ASSERT_TRUE(message_intact(events[e].data, events[e].size));
                TEST_ASSERT_EQUAL_UINT32(got++, get32(events[e].data + 4));
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(COUNT, got);
    case_end();
}

static uint32_t host_from;
static bool fresh_pair_connected(void) { return find(host_t, NET_LINK_EVENT_CONNECTED, host_from) && find(guest_t, NET_LINK_EVENT_CONNECTED, 0); }
static bool fresh_host_leave(void) { return find(host_t, NET_LINK_EVENT_DISCONNECTED, host_from) != NULL; }
static bool guest_saw_leave(void) { return find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0) != NULL; }

/* The receiving side read `count` reliable messages in order, then DISCONNECTED with `reason`, last. */
static void assert_all_then_reason(const tracked_t *t, uint32_t count, uint32_t reason) {
    uint32_t got = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        if (t->log[i].type == NET_LINK_EVENT_MESSAGE) {
            TEST_ASSERT_EQUAL_UINT32(got++, t->log[i].seq);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(count, got);
    TEST_ASSERT_TRUE(t->count > 0);
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, t->log[t->count - 1].type);
    TEST_ASSERT_EQUAL_UINT32(reason, t->log[t->count - 1].reason);
}

static void contract_guest_close_lingers_past_destroy(void) {
    case_begin();
    connect_pair(1);
    enum { COUNT = 20 };
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        send_ok(guest_t->link, peer_of(guest_t), 0, NET_LINK_SEND_RELIABLE, 0, seq);
    }
    net_link_close(guest_t->link, peer_of(guest_t), NET_LINK_REASON_APP_FIRST + 7);
    net_link_destroy(guest_t->link);
    untrack(guest_t);
    host_from = 0;
    TEST_ASSERT_TRUE_MESSAGE(tick_until(fresh_host_leave, env->settle_s * 4), "the closed guest's reason never arrived");
    assert_all_then_reason(host_t, COUNT, NET_LINK_REASON_APP_FIRST + 7);
    case_end();
}

static void contract_host_close_lingers_past_destroy(void) {
    case_begin();
    connect_pair(1);
    enum { COUNT = 20 };
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        send_ok(host_t->link, peer_of(host_t), 0, NET_LINK_SEND_RELIABLE, 0, seq);
    }
    net_link_close(host_t->link, peer_of(host_t), NET_LINK_REASON_APP_FIRST + 9);
    net_link_destroy(host_t->link);
    untrack(host_t);
    TEST_ASSERT_TRUE_MESSAGE(tick_until(guest_saw_leave, env->settle_s * 4), "the host's reason never arrived");
    assert_all_then_reason(guest_t, COUNT, NET_LINK_REASON_APP_FIRST + 9);
    case_end();
}

static void contract_codes_outside_the_app_range_read_as_remote_close(void) {
    case_begin();
    host_t = track(env->open_host(env->context, 100, 1));
    const uint32_t codes[] = {0, 1000, 2000};
    for (uint32_t c = 0; c < sizeof(codes) / sizeof(codes[0]); c++) {
        host_from = host_t->count;
        guest_t = track(env->open_guest(env->context, 200));
        TEST_ASSERT_TRUE_MESSAGE(tick_until(fresh_pair_connected, env->settle_s), "no connect within settle_s");
        net_link_close(guest_t->link, peer_of(guest_t), codes[c]);
        net_link_destroy(guest_t->link);
        untrack(guest_t);
        TEST_ASSERT_TRUE(tick_until(fresh_host_leave, env->settle_s * 2));
        TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, find(host_t, NET_LINK_EVENT_DISCONNECTED, host_from)->reason);
    }
    host_from = host_t->count;
    guest_t = track(env->open_guest(env->context, 200));
    TEST_ASSERT_TRUE(tick_until(fresh_pair_connected, env->settle_s));
    net_link_close(host_t->link, find(host_t, NET_LINK_EVENT_CONNECTED, host_from)->peer, 2000);
    TEST_ASSERT_TRUE(tick_until(guest_saw_leave, env->settle_s * 2));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0)->reason);
    case_end();
}

static bool both_ends_left(void) { return find(host_t, NET_LINK_EVENT_DISCONNECTED, 0) && find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0); }

static void contract_a_long_cut_times_out_both_ends(void) {
    if (!env->cut) {
        TEST_IGNORE_MESSAGE("the environment cannot cut a path");
    }
    case_begin();
    connect_pair(1);
    env->cut(env->context, guest_t->link, true);
    tick_for(env->timeout_s * 0.5);
    TEST_ASSERT_NULL(find(host_t, NET_LINK_EVENT_DISCONNECTED, 0));
    TEST_ASSERT_TRUE_MESSAGE(tick_until(both_ends_left, env->timeout_s + env->settle_s), "no timeout");
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, find(host_t, NET_LINK_EVENT_DISCONNECTED, 0)->reason);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0)->reason);
    const uint8_t payload[8] = {0};
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_NO_PEER, net_link_send(host_t->link, peer_of(host_t), 0, NET_LINK_SEND_RELIABLE, payload, 8));
    env->cut(env->context, guest_t->link, false);
    case_end();
}

static void contract_a_short_cut_loses_nothing_reliable(void) {
    if (!env->cut) {
        TEST_IGNORE_MESSAGE("the environment cannot cut a path");
    }
    case_begin();
    connect_pair(1);
    env->cut(env->context, guest_t->link, true);
    enum { COUNT = 10 };
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        send_ok(host_t->link, peer_of(host_t), 0, NET_LINK_SEND_RELIABLE, 0, seq);
        send_ok(guest_t->link, peer_of(guest_t), 0, NET_LINK_SEND_RELIABLE, 0, seq);
    }
    tick_for(env->timeout_s * 0.3);
    env->cut(env->context, guest_t->link, false);
    expected_messages = COUNT;
    TEST_ASSERT_TRUE_MESSAGE(tick_until(all_messages_in, env->settle_s * 4), "reliable messages lost to a short cut");
    tick_for(env->step_s * 4);
    TEST_ASSERT_EQUAL_UINT32(COUNT, count_type(host_t, NET_LINK_EVENT_MESSAGE));
    TEST_ASSERT_EQUAL_UINT32(COUNT, count_type(guest_t, NET_LINK_EVENT_MESSAGE));
    TEST_ASSERT_NULL(find(host_t, NET_LINK_EVENT_DISCONNECTED, 0));
    TEST_ASSERT_NULL(find(guest_t, NET_LINK_EVENT_DISCONNECTED, 0));
    case_end();
}

static const char *only_matching;

#define RUN_CASE(case_fn) \
    do { \
        if (only_matching == NULL || strstr(#case_fn, only_matching) != NULL) { \
            RUN_TEST(case_fn); \
        } \
    } while (0)

void net_link_contract_run_matching(const char *name_part) {
    only_matching = name_part;
    RUN_CASE(contract_connect_reports_identities_on_both_ends);
    RUN_CASE(contract_reliable_lanes_keep_order_and_arrive_once);
    RUN_CASE(contract_unreliable_arrives_whole_or_not_at_all);
    RUN_CASE(contract_size_limits_hold_for_each_kind);
    RUN_CASE(contract_bad_arguments_are_refused_without_harm);
    RUN_CASE(contract_close_delivers_reliable_first_then_the_reason);
    RUN_CASE(contract_full_host_refuses_the_next_guest);
    RUN_CASE(contract_nobody_hosting_is_unreachable);
    RUN_CASE(contract_host_leaving_closes_every_guest);
    RUN_CASE(contract_returning_guest_is_a_new_peer);
    RUN_CASE(contract_batch_payloads_stay_readable_until_the_next_poll);
    RUN_CASE(contract_guest_close_lingers_past_destroy);
    RUN_CASE(contract_host_close_lingers_past_destroy);
    RUN_CASE(contract_codes_outside_the_app_range_read_as_remote_close);
    RUN_CASE(contract_a_long_cut_times_out_both_ends);
    RUN_CASE(contract_a_short_cut_loses_nothing_reliable);
}

void net_link_contract_run_all(void) { net_link_contract_run_matching(NULL); }
