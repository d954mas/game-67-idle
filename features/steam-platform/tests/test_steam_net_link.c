/* The Steam link without Steam: every open refused while the platform is not started (in both
   builds), and the rules that need no Steam client: config, reasons both ways, peer handles,
   connection user data, what a received message may be, the port hold after a linger and the
   fake-network values. */

#include "features/steam_platform/steam_net_link.h"
#include "features/steam_platform/steam_platform.h"
#include "steam_net_link_closes.h"
#include "steam_net_link_common.h"
#include "steam_platform_internal.h"
#include "unity.h"

#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static steam_net_link_config_t valid_config(void) {
    steam_net_link_config_t config = {.max_peers = 4};
    config.lanes.count = 3;
    config.lanes.priority[0] = 0;
    config.lanes.priority[1] = 1;
    config.lanes.priority[2] = 1;
    return config;
}

static void assert_every_open_refused(void) {
    const steam_net_link_config_t config = valid_config();
    TEST_ASSERT_NULL(steam_net_link_host_ip(&config, NULL, 27500));
    TEST_ASSERT_NULL(steam_net_link_host_ip(&config, "127.0.0.1", 27500));
    TEST_ASSERT_NULL(steam_net_link_join_ip(&config, "127.0.0.1", 27500));
    TEST_ASSERT_NULL(steam_net_link_host_ip(NULL, NULL, 27500));
    TEST_ASSERT_NULL(steam_net_link_join_ip(&config, NULL, 27500));
    TEST_ASSERT_NULL(steam_net_link_join_ip(NULL, "127.0.0.1", 27500));
    const steam_net_link_fake_t fake = {.loss = 1.0F};
    steam_net_link_set_fake(&fake, &fake, 50.0F);
    steam_net_link_set_fake(NULL, NULL, 0.0F);
    TEST_ASSERT_FALSE(steam_net_link_lingering());
}

static void test_every_open_is_null_while_the_platform_is_stopped(void) {
    TEST_ASSERT_FALSE(steam_platform_ready());
    assert_every_open_refused();
}

static void test_every_open_is_null_after_a_failed_start(void) {
#ifdef STEAM_PLATFORM_HAS_SDK
    steam_platform_internal_simulate_failure(false, STEAM_PLATFORM_INTERNAL_INIT_NO_STEAM_CLIENT, "simulated: Steam is not running");
#endif
    const steam_platform_config_t start = {.app_id = 480};
    TEST_ASSERT_NOT_EQUAL_INT(STEAM_PLATFORM_READY, steam_platform_start(&start, NULL, 0));
    assert_every_open_refused();
}

static void test_zero_fields_take_the_defaults_and_ranges_hold(void) {
    steam_net_link_config_t config = valid_config();
    steam_net_link_settings_t s;
    TEST_ASSERT_TRUE(steam_net_link_settle(&config, true, &s));
    TEST_ASSERT_EQUAL_UINT8(3, s.lanes.count);
    TEST_ASSERT_EQUAL_UINT32(4, s.max_peers);
    TEST_ASSERT_TRUE(s.max_reliable >= 8 && s.max_reliable <= 524288);
    TEST_ASSERT_TRUE(s.send_buffer >= s.max_reliable);
    TEST_ASSERT_TRUE(s.timeout_ms > 0 && s.connect_timeout_ms > 0);
    TEST_ASSERT_FALSE(s.local_without_proof);

    config.max_peers = 0;
    TEST_ASSERT_FALSE(steam_net_link_settle(&config, true, &s));
    TEST_ASSERT_TRUE(steam_net_link_settle(&config, false, &s));
    TEST_ASSERT_EQUAL_UINT32(1, s.max_peers);
    config.max_peers = STEAM_NET_LINK_PEERS_MAX;
    TEST_ASSERT_TRUE(steam_net_link_settle(&config, true, &s));
    config.max_peers = STEAM_NET_LINK_PEERS_MAX + 1;
    TEST_ASSERT_FALSE(steam_net_link_settle(&config, true, &s));

    config = valid_config();
    config.lanes.count = 0;
    TEST_ASSERT_FALSE(steam_net_link_settle(&config, true, &s));
    config.lanes.count = NET_LINK_LANES_MAX + 1;
    TEST_ASSERT_FALSE(steam_net_link_settle(&config, false, &s));

    config = valid_config();
    config.max_reliable_bytes = 524288;
    TEST_ASSERT_TRUE(steam_net_link_settle(&config, true, &s));
    TEST_ASSERT_EQUAL_UINT32(524288, s.max_reliable);
    config.max_reliable_bytes = 524289;
    TEST_ASSERT_FALSE(steam_net_link_settle(&config, true, &s));
    config.max_reliable_bytes = 4096;
    config.send_buffer_bytes = 4095;
    TEST_ASSERT_FALSE(steam_net_link_settle(&config, true, &s));
    config.send_buffer_bytes = 4096;
    TEST_ASSERT_TRUE(steam_net_link_settle(&config, true, &s));

    const float bad[] = {-1.0F, 0.1F, 601.0F, NAN, INFINITY};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        config = valid_config();
        config.timeout_s = bad[i];
        TEST_ASSERT_FALSE(steam_net_link_settle(&config, true, &s));
        config = valid_config();
        config.connect_timeout_s = bad[i];
        TEST_ASSERT_FALSE(steam_net_link_settle(&config, false, &s));
    }
    TEST_ASSERT_TRUE(s.close_wait_ms > 0);
    const float close_waits[] = {0.25F, 60.0F};
    for (unsigned i = 0; i < 2; i++) {
        config = valid_config();
        config.close_wait_s = close_waits[i];
        TEST_ASSERT_TRUE(steam_net_link_settle(&config, true, &s));
        TEST_ASSERT_EQUAL_INT32((int32_t)(close_waits[i] * 1000.0F), s.close_wait_ms);
    }
    const float bad_close_waits[] = {0.2F, 60.5F, -1.0F, NAN};
    for (unsigned i = 0; i < 4; i++) {
        config = valid_config();
        config.close_wait_s = bad_close_waits[i];
        TEST_ASSERT_FALSE(steam_net_link_settle(&config, false, &s));
    }
    config = valid_config();
    config.timeout_s = 1.5F;
    TEST_ASSERT_TRUE(steam_net_link_settle(&config, true, &s));
    TEST_ASSERT_EQUAL_INT32(1500, s.timeout_ms);
    TEST_ASSERT_FALSE(steam_net_link_settle(NULL, true, &s));
}

static void test_app_codes_cross_unchanged_and_others_read_as_remote_close(void) {
    for (uint32_t reason = NET_LINK_REASON_APP_FIRST; reason <= NET_LINK_REASON_APP_LAST; reason++) {
        const int code = steam_net_link_reason_to_steam(reason);
        TEST_ASSERT_EQUAL_INT((int)reason, code);
        TEST_ASSERT_EQUAL_UINT32(reason, steam_net_link_reason_from_steam(true, code, true));
    }
    const uint32_t others[] = {0, 1, 5, 999, 1000, 2000, 2001, 2999, 5003, UINT32_MAX};
    for (unsigned i = 0; i < sizeof others / sizeof others[0]; i++) {
        const int code = steam_net_link_reason_to_steam(others[i]);
        /* An application may send 1000..2999; the other side reads its generic close. */
        TEST_ASSERT_TRUE(code >= 1000 && code <= 2999);
        TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, steam_net_link_reason_from_steam(true, code, true));
    }
}

static void test_steam_end_reasons_map_to_net_link_reasons(void) {
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REFUSED, steam_net_link_reason_from_steam(true, STEAM_NET_LINK_END_REFUSED, false));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TRANSPORT, steam_net_link_reason_from_steam(true, STEAM_NET_LINK_END_VIOLATION, true));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_REMOTE_CLOSE, steam_net_link_reason_from_steam(true, STEAM_NET_LINK_END_SHUTDOWN, true));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_UNREACHABLE, steam_net_link_reason_from_steam(true, STEAM_NET_LINK_END_GENERIC, false));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, steam_net_link_reason_from_steam(false, STEAM_NET_LINK_END_TIMEOUT, true));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_UNREACHABLE, steam_net_link_reason_from_steam(false, STEAM_NET_LINK_END_TIMEOUT, false));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TIMEOUT, steam_net_link_reason_from_steam(false, STEAM_NET_LINK_END_REMOTE_TIMEOUT, true));
    /* Everything else of Steam's: a bad certificate, an internal error, an oversized message. */
    const int steam_own[] = {0, 3001, 4002, 4003, 4006, 5001, 5002, 5008, 5010};
    for (unsigned i = 0; i < sizeof steam_own / sizeof steam_own[0]; i++) {
        TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TRANSPORT, steam_net_link_reason_from_steam(false, steam_own[i], true));
        TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TRANSPORT, steam_net_link_reason_from_steam(true, steam_own[i], true));
    }
    /* A code from another application's range is not this transport's. */
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_TRANSPORT, steam_net_link_reason_from_steam(true, 2500, true));
}

static void test_peer_handles_are_never_none_never_repeat_and_keep_their_slot(void) {
    uint32_t counter = 0;
    net_link_peer_t previous = NET_LINK_PEER_NONE;
    for (uint32_t i = 0; i < 1000; i++) {
        const uint32_t slot = i % STEAM_NET_LINK_PEERS_MAX;
        const net_link_peer_t handle = steam_net_link_mint_peer(&counter, slot);
        TEST_ASSERT_NOT_EQUAL_UINT32(NET_LINK_PEER_NONE, handle);
        TEST_ASSERT_NOT_EQUAL_UINT32(previous, handle);
        TEST_ASSERT_EQUAL_UINT32(slot, steam_net_link_peer_slot(handle));
        previous = handle;
    }
    counter = UINT32_MAX;
    TEST_ASSERT_NOT_EQUAL_UINT32(NET_LINK_PEER_NONE, steam_net_link_mint_peer(&counter, 0));
    /* The counter wraps where the slot bits would push it out of 32 bits. */
    counter = 1U << (32 - STEAM_NET_LINK_SLOT_BITS);
    const net_link_peer_t wrapped = steam_net_link_mint_peer(&counter, 0);
    TEST_ASSERT_NOT_EQUAL_UINT32(NET_LINK_PEER_NONE, wrapped);
    TEST_ASSERT_EQUAL_UINT32(0, steam_net_link_peer_slot(wrapped));
    counter = (1U << (32 - STEAM_NET_LINK_SLOT_BITS)) - 1;
    const net_link_peer_t last = steam_net_link_mint_peer(&counter, 0);
    TEST_ASSERT_NOT_EQUAL_UINT32(NET_LINK_PEER_NONE, last);
    TEST_ASSERT_NOT_EQUAL_UINT32(last, steam_net_link_mint_peer(&counter, 0));
}

static void test_user_data_is_never_steams_unset_value_and_splits_back(void) {
    const uint32_t serials[] = {1, 2, 77, STEAM_NET_LINK_SERIAL_MAX};
    const net_link_peer_t peers[] = {NET_LINK_PEER_NONE, 1, 0x12345671U, UINT32_MAX};
    for (unsigned s = 0; s < 4; s++) {
        for (uint32_t index = 0; index < STEAM_NET_LINK_LINKS_MAX; index++) {
            for (unsigned p = 0; p < 4; p++) {
                const int64_t data = steam_net_link_user_data(index, serials[s], peers[p]);
                TEST_ASSERT_TRUE(data >= 0);
                uint32_t back_index = 99, back_serial = 0;
                net_link_peer_t back_peer = 7;
                TEST_ASSERT_TRUE(steam_net_link_user_data_split(data, &back_index, &back_serial, &back_peer));
                TEST_ASSERT_EQUAL_UINT32(index, back_index);
                TEST_ASSERT_EQUAL_UINT32(serials[s], back_serial);
                TEST_ASSERT_EQUAL_UINT32(peers[p], back_peer);
            }
        }
    }
    uint32_t index, serial;
    net_link_peer_t peer;
    TEST_ASSERT_FALSE(steam_net_link_user_data_split(-1, &index, &serial, &peer));
    TEST_ASSERT_FALSE(steam_net_link_user_data_split(steam_net_link_user_data(0, 0, 5), &index, &serial, &peer));
}

static void test_what_a_net_link_cannot_send_never_reaches_the_owner(void) {
    const uint32_t lanes = 3, max_u = 1200, max_r = 16384;
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_ADMIT, steam_net_link_judge(0, lanes, 1, false, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_ADMIT, steam_net_link_judge(2, lanes, max_u, false, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_ADMIT, steam_net_link_judge(2, lanes, max_r, true, max_u, max_r));
    /* The keepalive is the one empty message: reliable, on any of net-link's lanes, also one
       this link does not have (the sender's first lane need not be one of its own). */
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_KEEPALIVE, steam_net_link_judge(0, lanes, 0, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_KEEPALIVE, steam_net_link_judge(lanes - 1, lanes, 0, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_KEEPALIVE, steam_net_link_judge(lanes, lanes, 0, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_KEEPALIVE, steam_net_link_judge(2, 1, 0, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_KEEPALIVE, steam_net_link_judge(NET_LINK_LANES_MAX - 1, 1, 0, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_HOSTILE, steam_net_link_judge(NET_LINK_LANES_MAX, 1, 0, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_HOSTILE, steam_net_link_judge(0, lanes, 0, false, max_u, max_r));
    /* A message with bytes on a lane the link lacks is still a rule break. */
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_HOSTILE, steam_net_link_judge(2, 1, 1, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_HOSTILE, steam_net_link_judge(lanes, lanes, 8, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_HOSTILE, steam_net_link_judge(NET_LINK_LANES_MAX, NET_LINK_LANES_MAX, 8, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_HOSTILE, steam_net_link_judge(65535, lanes, 8, false, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_HOSTILE, steam_net_link_judge(0, lanes, max_u + 1, false, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_HOSTILE, steam_net_link_judge(0, lanes, max_r + 1, true, max_u, max_r));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_HOSTILE, steam_net_link_judge(0, lanes, -1, true, max_u, max_r));
}

static void test_a_peer_may_send_keepalives_at_the_fastest_rate_and_a_burst_not_more(void) {
    const int64_t each_us = 1000000 / STEAM_NET_LINK_KEEPALIVE_PER_S;
    steam_net_link_keepalive_allowance_t a;
    steam_net_link_keepalive_allowance_start(&a, 5000);
    /* The whole burst at once, as after a long stretch without a poll; one more is a flood. */
    for (uint32_t i = 0; i < STEAM_NET_LINK_KEEPALIVE_BURST; i++) {
        TEST_ASSERT_TRUE(steam_net_link_keepalive_allowed(&a, 5000));
    }
    TEST_ASSERT_FALSE(steam_net_link_keepalive_allowed(&a, 5000));
    TEST_ASSERT_FALSE(steam_net_link_keepalive_allowed(&a, 5000 + each_us - 1));
    /* The fastest honest sender, read as it arrives, is never refused. */
    int64_t now = 5000 + each_us - 1;
    for (uint32_t i = 0; i < 4 * STEAM_NET_LINK_KEEPALIVE_BURST; i++) {
        now += each_us;
        TEST_ASSERT_TRUE(steam_net_link_keepalive_allowed(&a, now));
    }
    /* A second of silence earns a second of them, not more. */
    TEST_ASSERT_FALSE(steam_net_link_keepalive_allowed(&a, now));
    now += 1000000;
    for (uint32_t i = 0; i < STEAM_NET_LINK_KEEPALIVE_PER_S; i++) {
        TEST_ASSERT_TRUE(steam_net_link_keepalive_allowed(&a, now));
    }
    TEST_ASSERT_FALSE(steam_net_link_keepalive_allowed(&a, now));
    /* However long the silence, the burst is the most. */
    now += (int64_t)3600 * 1000000;
    for (uint32_t i = 0; i < STEAM_NET_LINK_KEEPALIVE_BURST; i++) {
        TEST_ASSERT_TRUE(steam_net_link_keepalive_allowed(&a, now));
    }
    TEST_ASSERT_FALSE(steam_net_link_keepalive_allowed(&a, now));
    /* A clock that steps back earns nothing and breaks nothing; the far future does not wrap. */
    TEST_ASSERT_FALSE(steam_net_link_keepalive_allowed(&a, now - 10 * 1000000));
    TEST_ASSERT_TRUE(steam_net_link_keepalive_allowed(&a, INT64_MAX));
    TEST_ASSERT_EQUAL_UINT32(STEAM_NET_LINK_KEEPALIVE_BURST - 1, a.left);
}

static void test_the_hold_after_a_close_covers_a_keepalive_and_stays_bounded(void) {
    int64_t previous = 0;
    for (int32_t timeout_ms = 250; timeout_ms <= 600000; timeout_ms *= 2) {
        const int64_t hold = steam_net_link_final_hold_us(timeout_ms, 1);
        TEST_ASSERT_TRUE(hold >= 100000 && hold <= 3000000);
        TEST_ASSERT_TRUE(hold >= previous);
        /* Two keepalives of a peer with the same timeout, where the cap allows. */
        TEST_ASSERT_TRUE(hold >= 2 * steam_net_link_keepalive_us(timeout_ms) || hold == 3000000);
        previous = hold;
    }
    TEST_ASSERT_TRUE(steam_net_link_final_hold_us(2000, 100) > steam_net_link_final_hold_us(2000, 0));
    TEST_ASSERT_TRUE(steam_net_link_final_hold_us(2000, -1) == steam_net_link_final_hold_us(2000, 0));
    /* Any input stays in range; a product that wrapped would land outside it or below the cap. */
    TEST_ASSERT_TRUE(steam_net_link_final_hold_us(INT32_MAX, INT32_MAX) == 3000000);
    TEST_ASSERT_TRUE(steam_net_link_final_hold_us(INT32_MAX, 0) == 3000000);
    TEST_ASSERT_TRUE(steam_net_link_final_hold_us(0, INT32_MAX) == 3000000);
    TEST_ASSERT_TRUE(steam_net_link_final_hold_us(INT32_MIN, INT32_MIN) == 100000);
}

static void test_the_keepalive_goes_on_the_lane_sent_first(void) {
    net_link_lanes_t lanes = {.count = 3, .priority = {2, 0, 0}};
    TEST_ASSERT_EQUAL_UINT8(1, steam_net_link_keepalive_lane(&lanes));
    lanes = (net_link_lanes_t){.count = 1, .priority = {9}};
    TEST_ASSERT_EQUAL_UINT8(0, steam_net_link_keepalive_lane(&lanes));
    lanes = (net_link_lanes_t){.count = NET_LINK_LANES_MAX, .priority = {7, 6, 5, 4, 3, 2, 1, 0}};
    TEST_ASSERT_EQUAL_UINT8(NET_LINK_LANES_MAX - 1, steam_net_link_keepalive_lane(&lanes));
    /* A priority past the count does not count. */
    lanes = (net_link_lanes_t){.count = 2, .priority = {5, 5, 0}};
    TEST_ASSERT_EQUAL_UINT8(0, steam_net_link_keepalive_lane(&lanes));
}

static void test_address_text_that_names_a_port_is_told_apart(void) {
    const char *with_port[] = {"127.0.0.1:27000", "1.2.3.4:0", "[::1]:5", "[::1]", "::ffff:1.2.3.4:80"};
    const char *plain[] = {"127.0.0.1", "192.168.1.20", "::1", "::", "::ffff:1.2.3.4", "fe80::1", ""};
    for (unsigned i = 0; i < sizeof with_port / sizeof with_port[0]; i++) {
        TEST_ASSERT_TRUE_MESSAGE(steam_net_link_address_names_port(with_port[i]), with_port[i]);
    }
    for (unsigned i = 0; i < sizeof plain / sizeof plain[0]; i++) {
        TEST_ASSERT_FALSE_MESSAGE(steam_net_link_address_names_port(plain[i]), plain[i]);
    }
    TEST_ASSERT_FALSE(steam_net_link_address_names_port(NULL));
}

static void test_the_ledger_hands_out_in_order_and_connected_before_its_disconnected(void) {
    steam_net_link_ledger_t ledger = {0};
    uint32_t slot = 99;
    net_link_event_type_t type = NET_LINK_EVENT_MESSAGE;
    TEST_ASSERT_FALSE(steam_net_link_ledger_next(&ledger, &slot, &type));
    /* Slot 5 connects, slot 2 connects, slot 5 leaves, slot 2 leaves. */
    steam_net_link_ledger_owe_connected(&ledger, 5);
    steam_net_link_ledger_owe_connected(&ledger, 2);
    TEST_ASSERT_TRUE(steam_net_link_ledger_owe_disconnected(&ledger, 5));
    TEST_ASSERT_TRUE(steam_net_link_ledger_owe_disconnected(&ledger, 2));
    /* A second DISCONNECTED owed does not move or replace the first. */
    TEST_ASSERT_FALSE(steam_net_link_ledger_owe_disconnected(&ledger, 5));
    const uint32_t want_slot[] = {5, 2, 5, 2};
    const net_link_event_type_t want_type[] = {NET_LINK_EVENT_CONNECTED, NET_LINK_EVENT_CONNECTED, NET_LINK_EVENT_DISCONNECTED, NET_LINK_EVENT_DISCONNECTED};
    for (unsigned i = 0; i < 4; i++) {
        TEST_ASSERT_TRUE(steam_net_link_ledger_next(&ledger, &slot, &type));
        TEST_ASSERT_EQUAL_UINT32(want_slot[i], slot);
        TEST_ASSERT_EQUAL_INT(want_type[i], type);
        steam_net_link_ledger_paid(&ledger, slot, type);
    }
    TEST_ASSERT_FALSE(steam_net_link_ledger_next(&ledger, &slot, &type));

    /* Owed the other way round, CONNECTED still comes first for its slot. */
    TEST_ASSERT_TRUE(steam_net_link_ledger_owe_disconnected(&ledger, 7));
    steam_net_link_ledger_owe_connected(&ledger, 7);
    TEST_ASSERT_TRUE(steam_net_link_ledger_owes_connected(&ledger, 7));
    TEST_ASSERT_TRUE(steam_net_link_ledger_next(&ledger, &slot, &type));
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_CONNECTED, type);
    steam_net_link_ledger_paid(&ledger, 7, NET_LINK_EVENT_CONNECTED);
    TEST_ASSERT_FALSE(steam_net_link_ledger_owes_connected(&ledger, 7));
    TEST_ASSERT_TRUE(steam_net_link_ledger_next(&ledger, &slot, &type));
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, type);

    /* A forgotten slot owes nothing, and a fresh peer there starts clean. */
    steam_net_link_ledger_forget(&ledger, 7);
    TEST_ASSERT_FALSE(steam_net_link_ledger_next(&ledger, &slot, &type));
    TEST_ASSERT_TRUE(steam_net_link_ledger_owe_disconnected(&ledger, 7));

    /* Slots out of range are refused without harm. */
    steam_net_link_ledger_owe_connected(&ledger, STEAM_NET_LINK_PEERS_MAX);
    TEST_ASSERT_FALSE(steam_net_link_ledger_owe_disconnected(&ledger, STEAM_NET_LINK_PEERS_MAX));
    TEST_ASSERT_FALSE(steam_net_link_ledger_owes_connected(&ledger, STEAM_NET_LINK_PEERS_MAX));
}

static void test_every_slot_of_a_full_ledger_comes_out_once(void) {
    steam_net_link_ledger_t ledger = {0};
    for (uint32_t s = 0; s < STEAM_NET_LINK_PEERS_MAX; s++) {
        steam_net_link_ledger_owe_connected(&ledger, STEAM_NET_LINK_PEERS_MAX - 1 - s);
        steam_net_link_ledger_owe_disconnected(&ledger, STEAM_NET_LINK_PEERS_MAX - 1 - s);
    }
    uint32_t seen = 0, slot = 0, connected_of[STEAM_NET_LINK_PEERS_MAX] = {0};
    net_link_event_type_t type;
    while (steam_net_link_ledger_next(&ledger, &slot, &type)) {
        if (type == NET_LINK_EVENT_CONNECTED) {
            connected_of[slot]++;
        } else {
            TEST_ASSERT_EQUAL_UINT32(1, connected_of[slot]);
        }
        steam_net_link_ledger_paid(&ledger, slot, type);
        TEST_ASSERT_TRUE(++seen <= 2 * STEAM_NET_LINK_PEERS_MAX);
    }
    TEST_ASSERT_EQUAL_UINT32(2 * STEAM_NET_LINK_PEERS_MAX, seen);
}

static void test_keepalives_come_well_inside_the_timeout(void) {
    for (int32_t timeout_ms = 250; timeout_ms <= 600000; timeout_ms *= 2) {
        const int64_t every = steam_net_link_keepalive_us(timeout_ms);
        TEST_ASSERT_TRUE(every > 0 && every * 2 <= (int64_t)timeout_ms * 1000);
    }
}

static void test_fake_values_are_clamped(void) {
    const steam_net_link_fake_t wild = {.loss = 3.0F, .lag_ms = 1e9F, .reorder = -1.0F, .duplicate = NAN};
    const steam_net_link_fake_values_t v = steam_net_link_fake_values(&wild);
    TEST_ASSERT_TRUE(v.loss_pct == 100.0F);
    TEST_ASSERT_TRUE(v.lag_ms > 0 && v.lag_ms <= 10000);
    TEST_ASSERT_TRUE(v.reorder_pct == 0.0F && v.duplicate_pct == 0.0F);
    const steam_net_link_fake_t mild = {.loss = 0.25F, .lag_ms = 40.4F, .reorder = 0.5F, .duplicate = 0.1F};
    const steam_net_link_fake_values_t m = steam_net_link_fake_values(&mild);
    TEST_ASSERT_TRUE(fabsf(m.loss_pct - 25.0F) < 0.01F && m.lag_ms == 40 && fabsf(m.reorder_pct - 50.0F) < 0.01F);
    const steam_net_link_fake_values_t none = steam_net_link_fake_values(NULL);
    TEST_ASSERT_TRUE(none.loss_pct == 0.0F && none.lag_ms == 0);
    TEST_ASSERT_EQUAL_INT32(0, steam_net_link_reorder_ms(-3.0F));
    TEST_ASSERT_EQUAL_INT32(25, steam_net_link_reorder_ms(25.0F));
}

// #region closes in hand

/* A probe whose answer the case sets per connection. */
static steam_net_link_close_state_t s_state[64];
static steam_net_link_close_probe_t probe_of(void *user, uint32_t conn) {
    (void)user;
    return (steam_net_link_close_probe_t){.state = conn < 64 && s_state[conn] ? s_state[conn] : STEAM_NET_LINK_CLOSE_PENDING, .ping_ms = 0};
}

static const steam_net_link_address_t HERE = {{1, 2, 3}}, THERE = {{4, 5, 6}};
enum { WAIT_US = 5000000, SECOND = 1000000 };

static void fresh(steam_net_link_closes_t *c) {
    memset(c, 0, sizeof *c);
    memset(s_state, 0, sizeof s_state);
}

static uint32_t count_of(const steam_net_link_close_actions_t *a, steam_net_link_close_do_t what, uint32_t handle) {
    uint32_t n = 0;
    for (uint32_t i = 0; i < a->count; i++) {
        n += a->items[i].what == what && a->items[i].handle == handle;
    }
    return n;
}

static const steam_net_link_close_action_t *find_action(const steam_net_link_close_actions_t *a, steam_net_link_close_do_t what, uint32_t handle) {
    for (uint32_t i = 0; i < a->count; i++) {
        if (a->items[i].what == what && a->items[i].handle == handle) {
            return &a->items[i];
        }
    }
    return NULL;
}

static void frame_at(steam_net_link_closes_t *c, int64_t now, steam_net_link_close_actions_t *out) {
    memset(out, 0, sizeof *out);
    steam_net_link_closes_frame(c, now, probe_of, NULL, out);
}

static void test_a_close_waits_for_the_acknowledgement_then_holds(void) {
    steam_net_link_closes_t c;
    steam_net_link_close_actions_t a = {0};
    fresh(&c);
    steam_net_link_closes_take(&c, 7, 1234, 2000, WAIT_US, 0, &HERE, 0, &a);
    TEST_ASSERT_EQUAL_UINT32(0, a.count);
    frame_at(&c, SECOND, &a);
    TEST_ASSERT_EQUAL_UINT32(0, a.count);
    TEST_ASSERT_TRUE(steam_net_link_closes_any(&c));
    s_state[7] = STEAM_NET_LINK_CLOSE_ACKED;
    frame_at(&c, 2 * SECOND, &a);
    const steam_net_link_close_action_t *close = find_action(&a, STEAM_NET_LINK_DO_CLOSE, 7);
    TEST_ASSERT_NOT_NULL(close);
    TEST_ASSERT_EQUAL_INT(1234, close->code);
    /* Held for the answer to the close: two keepalives of a 2 s timeout. */
    const int64_t hold = steam_net_link_final_hold_us(2000, 0);
    frame_at(&c, 2 * SECOND + hold - 1, &a);
    TEST_ASSERT_TRUE(steam_net_link_closes_any(&c));
    TEST_ASSERT_EQUAL_UINT32(0, count_of(&a, STEAM_NET_LINK_DO_CLOSE, 7));
    frame_at(&c, 2 * SECOND + hold, &a);
    TEST_ASSERT_FALSE(steam_net_link_closes_any(&c));
}

static void test_the_deadline_closes_with_the_code_at_once(void) {
    steam_net_link_closes_t c;
    steam_net_link_close_actions_t a = {0};
    fresh(&c);
    steam_net_link_closes_take(&c, 9, 1500, 2000, WAIT_US, 0, &HERE, 0, &a);
    frame_at(&c, WAIT_US - 1, &a);
    TEST_ASSERT_EQUAL_UINT32(0, a.count);
    frame_at(&c, WAIT_US, &a);
    const steam_net_link_close_action_t *close = find_action(&a, STEAM_NET_LINK_DO_CLOSE, 9);
    TEST_ASSERT_NOT_NULL_MESSAGE(close, "the deadline did not cut");
    TEST_ASSERT_EQUAL_INT(1500, close->code);
    /* Told once: the hold does not tell again. */
    frame_at(&c, WAIT_US + 1, &a);
    TEST_ASSERT_EQUAL_UINT32(0, count_of(&a, STEAM_NET_LINK_DO_CLOSE, 9));
    frame_at(&c, WAIT_US + 4 * SECOND, &a);
    TEST_ASSERT_FALSE(steam_net_link_closes_any(&c));
}

static void test_a_peer_that_leaves_first_frees_its_close(void) {
    steam_net_link_closes_t c;
    steam_net_link_close_actions_t a = {0};
    fresh(&c);
    steam_net_link_closes_take(&c, 3, 1001, 2000, WAIT_US, 0, &HERE, 0, &a);
    TEST_ASSERT_FALSE(steam_net_link_closes_peer_left(&c, 4, &a));
    TEST_ASSERT_TRUE(steam_net_link_closes_peer_left(&c, 3, &a));
    TEST_ASSERT_EQUAL_UINT32(1, count_of(&a, STEAM_NET_LINK_DO_FREE, 3));
    TEST_ASSERT_EQUAL_UINT32(0, count_of(&a, STEAM_NET_LINK_DO_CLOSE, 3));
    TEST_ASSERT_FALSE(steam_net_link_closes_any(&c));
    /* Steam no longer having it frees it the same way. */
    steam_net_link_closes_take(&c, 5, 1001, 2000, WAIT_US, 0, &HERE, 0, &a);
    s_state[5] = STEAM_NET_LINK_CLOSE_GONE;
    frame_at(&c, 1, &a);
    TEST_ASSERT_EQUAL_UINT32(1, count_of(&a, STEAM_NET_LINK_DO_FREE, 5));
    TEST_ASSERT_FALSE(steam_net_link_closes_any(&c));
}

static void test_a_full_table_cuts_the_close_due_first(void) {
    steam_net_link_closes_t c;
    steam_net_link_close_actions_t a = {0};
    fresh(&c);
    /* Taken one microsecond apart: the first taken is due first. */
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX; i++) {
        steam_net_link_closes_take(&c, 10 + i, 1001, 2000, WAIT_US, 0, &HERE, i, &a);
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, a.count, "the table cut a close before it was full");
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX; i++) {
        TEST_ASSERT_TRUE(c.entries[i].used);
    }
    steam_net_link_closes_take(&c, 99, 1002, 2000, WAIT_US, 0, &HERE, 100, &a);
    TEST_ASSERT_EQUAL_UINT32(1, a.count);
    const steam_net_link_close_action_t *cut = find_action(&a, STEAM_NET_LINK_DO_CLOSE, 10);
    TEST_ASSERT_NOT_NULL_MESSAGE(cut, "the evicted close was not the one due first, or was never closed");
    TEST_ASSERT_EQUAL_INT(1001, cut->code);
    /* An evicted close takes its orphaned socket with it, the connection first. */
    fresh(&c);
    memset(&a, 0, sizeof a);
    for (uint32_t i = 0; i < STEAM_NET_LINK_CLOSES_MAX; i++) {
        steam_net_link_closes_take(&c, 10 + i, 1001, 2000, WAIT_US, i == 0 ? 88 : 0, &HERE, i, &a);
    }
    steam_net_link_closes_keep_socket(&c, 88);
    steam_net_link_closes_take(&c, 99, 1002, 2000, WAIT_US, 0, &HERE, 100, &a);
    TEST_ASSERT_NOT_NULL(find_action(&a, STEAM_NET_LINK_DO_CLOSE, 10));
    TEST_ASSERT_EQUAL_UINT32(1, count_of(&a, STEAM_NET_LINK_DO_CLOSE_SOCKET, 88));
    TEST_ASSERT_TRUE(find_action(&a, STEAM_NET_LINK_DO_CLOSE, 10) < find_action(&a, STEAM_NET_LINK_DO_CLOSE_SOCKET, 88));
    /* The newcomer waits like the others. */
    frame_at(&c, 200, &a);
    TEST_ASSERT_EQUAL_UINT32(0, a.count);
}

static void test_an_orphaned_socket_closes_with_its_last_close_and_not_before(void) {
    steam_net_link_closes_t c;
    steam_net_link_close_actions_t a = {0};
    fresh(&c);
    steam_net_link_closes_take(&c, 21, 1001, 2000, WAIT_US, 77, &HERE, 0, &a);
    steam_net_link_closes_take(&c, 22, 1002, 2000, WAIT_US, 77, &HERE, 0, &a);
    steam_net_link_closes_take(&c, 23, 1003, 2000, WAIT_US, 0, &HERE, 0, &a);
    /* A live host's socket is not turned away from or closed. */
    TEST_ASSERT_FALSE(steam_net_link_closes_turns_away(&c, 77));
    TEST_ASSERT_FALSE(steam_net_link_closes_keep_socket(&c, 78));
    TEST_ASSERT_TRUE(steam_net_link_closes_keep_socket(&c, 77));
    TEST_ASSERT_TRUE(steam_net_link_closes_turns_away(&c, 77));
    TEST_ASSERT_FALSE(steam_net_link_closes_turns_away(&c, 0));
    s_state[21] = STEAM_NET_LINK_CLOSE_ACKED;
    frame_at(&c, SECOND, &a);
    frame_at(&c, 3 * SECOND, &a); /* 21 told at 1 s, its hold over at 2 s */
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, count_of(&a, STEAM_NET_LINK_DO_CLOSE_SOCKET, 77), "the socket closed while a close still needs it");
    TEST_ASSERT_TRUE(steam_net_link_closes_turns_away(&c, 77));
    s_state[22] = STEAM_NET_LINK_CLOSE_GONE;
    frame_at(&c, 4 * SECOND, &a); /* before the deadline of 22 and 23 */
    TEST_ASSERT_EQUAL_UINT32(1, count_of(&a, STEAM_NET_LINK_DO_CLOSE_SOCKET, 77));
    TEST_ASSERT_FALSE(steam_net_link_closes_turns_away(&c, 77));
    TEST_ASSERT_TRUE(steam_net_link_closes_any(&c));
}

static void test_hosting_again_on_an_address_cuts_its_closes_with_the_hand_over(void) {
    steam_net_link_closes_t c;
    steam_net_link_close_actions_t a = {0};
    fresh(&c);
    steam_net_link_closes_take(&c, 31, 1001, 2000, WAIT_US, 55, &HERE, 0, &a);
    steam_net_link_closes_take(&c, 32, 1002, 2000, WAIT_US, 55, &HERE, 0, &a);
    steam_net_link_closes_take(&c, 33, 1003, 2000, WAIT_US, 56, &THERE, 0, &a);
    steam_net_link_closes_keep_socket(&c, 55);
    steam_net_link_closes_keep_socket(&c, 56);
    s_state[32] = STEAM_NET_LINK_CLOSE_ACKED;
    frame_at(&c, SECOND, &a); /* 32 is told, 31 still waits */
    memset(&a, 0, sizeof a);
    TEST_ASSERT_TRUE(steam_net_link_closes_take_port(&c, &HERE, &a));
    const steam_net_link_close_action_t *cut = find_action(&a, STEAM_NET_LINK_DO_CLOSE, 31);
    TEST_ASSERT_NOT_NULL_MESSAGE(cut, "a waiting close was dropped without being closed");
    TEST_ASSERT_EQUAL_INT(1001, cut->code);
    TEST_ASSERT_EQUAL_UINT32(0, count_of(&a, STEAM_NET_LINK_DO_CLOSE, 32));
    TEST_ASSERT_EQUAL_UINT32(1, count_of(&a, STEAM_NET_LINK_DO_CLOSE_SOCKET, 55));
    /* Another address keeps its wait. */
    TEST_ASSERT_TRUE(steam_net_link_closes_turns_away(&c, 56));
    memset(&a, 0, sizeof a);
    TEST_ASSERT_FALSE(steam_net_link_closes_take_port(&c, &HERE, &a));
    TEST_ASSERT_EQUAL_UINT32(0, a.count);
}

static void test_forget_drops_every_close(void) {
    steam_net_link_closes_t c;
    steam_net_link_close_actions_t a = {0};
    fresh(&c);
    steam_net_link_closes_take(&c, 41, 1001, 2000, WAIT_US, 9, &HERE, 0, &a);
    steam_net_link_closes_keep_socket(&c, 9);
    steam_net_link_closes_forget(&c);
    TEST_ASSERT_FALSE(steam_net_link_closes_any(&c));
    TEST_ASSERT_FALSE(steam_net_link_closes_turns_away(&c, 9));
    frame_at(&c, 10 * SECOND, &a);
    TEST_ASSERT_EQUAL_UINT32(0, a.count);
}

static void test_the_deadline_comes_before_a_certificate_of_the_same_frame(void) {
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_DIAL_GIVE_UP, steam_net_link_dial_decision(100, 100, STEAM_NET_LINK_CERTIFICATE_CURRENT));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_DIAL_GIVE_UP, steam_net_link_dial_decision(101, 100, STEAM_NET_LINK_CERTIFICATE_CURRENT));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_DIAL_NOW, steam_net_link_dial_decision(99, 100, STEAM_NET_LINK_CERTIFICATE_CURRENT));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_DIAL_WAIT, steam_net_link_dial_decision(99, 100, STEAM_NET_LINK_CERTIFICATE_PENDING));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_DIAL_GIVE_UP, steam_net_link_dial_decision(100, 100, STEAM_NET_LINK_CERTIFICATE_PENDING));
    TEST_ASSERT_EQUAL_INT(STEAM_NET_LINK_DIAL_GIVE_UP, steam_net_link_dial_decision(0, 100, STEAM_NET_LINK_CERTIFICATE_FAILED));
}

// #endregion

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_every_open_is_null_while_the_platform_is_stopped);
    RUN_TEST(test_every_open_is_null_after_a_failed_start);
    RUN_TEST(test_zero_fields_take_the_defaults_and_ranges_hold);
    RUN_TEST(test_app_codes_cross_unchanged_and_others_read_as_remote_close);
    RUN_TEST(test_steam_end_reasons_map_to_net_link_reasons);
    RUN_TEST(test_peer_handles_are_never_none_never_repeat_and_keep_their_slot);
    RUN_TEST(test_user_data_is_never_steams_unset_value_and_splits_back);
    RUN_TEST(test_what_a_net_link_cannot_send_never_reaches_the_owner);
    RUN_TEST(test_a_peer_may_send_keepalives_at_the_fastest_rate_and_a_burst_not_more);
    RUN_TEST(test_the_hold_after_a_close_covers_a_keepalive_and_stays_bounded);
    RUN_TEST(test_keepalives_come_well_inside_the_timeout);
    RUN_TEST(test_the_keepalive_goes_on_the_lane_sent_first);
    RUN_TEST(test_address_text_that_names_a_port_is_told_apart);
    RUN_TEST(test_the_ledger_hands_out_in_order_and_connected_before_its_disconnected);
    RUN_TEST(test_every_slot_of_a_full_ledger_comes_out_once);
    RUN_TEST(test_a_close_waits_for_the_acknowledgement_then_holds);
    RUN_TEST(test_the_deadline_closes_with_the_code_at_once);
    RUN_TEST(test_a_peer_that_leaves_first_frees_its_close);
    RUN_TEST(test_a_full_table_cuts_the_close_due_first);
    RUN_TEST(test_an_orphaned_socket_closes_with_its_last_close_and_not_before);
    RUN_TEST(test_hosting_again_on_an_address_cuts_its_closes_with_the_hand_over);
    RUN_TEST(test_forget_drops_every_close);
    RUN_TEST(test_the_deadline_comes_before_a_certificate_of_the_same_frame);
    RUN_TEST(test_fake_values_are_clamped);
    return UNITY_END();
}
