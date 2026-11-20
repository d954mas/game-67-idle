/* The Steam link's closes on the developer's Steam client, in one process over 127.0.0.1: a
   close waits for Steam's acknowledgement and holds for its answer, under loss too and in
   either role; the deadline closes it with its code; a destroyed host keeps its socket
   for its closes and turns newcomers away; hosting again cuts them; a peer that leaves first
   frees its close; a stop forgets them all. Exit 77 is a skip (1 with
   STEAM_PLATFORM_REQUIRE_CLIENT); --simulate-no-client takes that path on purpose. */

#include "steam_net_link_harness.hpp"

void setUp(void) { steam_live::case_began(); }
void tearDown(void) {
    steam_net_link_set_fake(nullptr, nullptr, 0.0F);
    steam_live::case_ended(Unity.CurrentTestName);
}

namespace {

using namespace live;

/* Lets closes of an earlier case finish, so a case reads only its own. */
void no_closes_in_hand() {
    tick_until({}, 15.0, [] { return !steam_net_link_lingering(); });
    TEST_ASSERT_FALSE_MESSAGE(steam_net_link_lingering(), "an earlier case left a close in hand");
}

/* Long enough at Steam's default rate that closing the listen socket at the destroy would cut it. */
constexpr uint32_t LONG_QUEUE = 30;
constexpr uint32_t QUEUE_ITEM = 16384;

void queue_and_close(side *host, uint32_t reason) {
    for (uint32_t seq = 0; seq < LONG_QUEUE; seq++) {
        std::vector<uint8_t> item = pattern(QUEUE_ITEM, seq);
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(host->link, host->peer(), 0, NET_LINK_SEND_RELIABLE, item.data(), QUEUE_ITEM));
    }
    net_link_close(host->link, host->peer(), reason);
    host->destroy();
}

void test_a_long_host_close_lingers_past_destroy_and_its_port_turns_guests_away(void) {
    const steam_net_link_config_t config = config_of(1, 2);
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    queue_and_close(&host, NET_LINK_REASON_APP_FIRST + 41);
    TEST_ASSERT_TRUE(steam_net_link_lingering());
    /* A guest knocking while the port is kept finds nobody hosting, at once. */
    side late = join(config, guest.port);
    const double knocked = steam_live::now_s();
    double turned_away = -1.0;
    TEST_ASSERT_TRUE(tick_until({&guest, &late}, 15.0, [&] {
        if (turned_away < 0 && late.first(NET_LINK_EVENT_DISCONNECTED)) {
            turned_away = steam_live::now_s() - knocked;
        }
        return guest.first(NET_LINK_EVENT_DISCONNECTED) && late.first(NET_LINK_EVENT_DISCONNECTED);
    }));
    printf("kept port: a late guest was turned away after %.2f s (connect timeout %.0f s)\n", turned_away, static_cast<double>(config.connect_timeout_s));
    TEST_ASSERT_TRUE(turned_away >= 0 && turned_away < 1.0);
    uint32_t seq = 0;
    for (const got &g : guest.log) {
        if (g.type == NET_LINK_EVENT_MESSAGE) {
            TEST_ASSERT_TRUE(g.data == pattern(QUEUE_ITEM, seq));
            seq++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(LONG_QUEUE, seq);
    TEST_ASSERT_EQUAL_INT(NET_LINK_EVENT_DISCONNECTED, guest.log.back().type);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_APP_FIRST + 41, guest.log.back().reason);
    TEST_ASSERT_NULL(late.first(NET_LINK_EVENT_CONNECTED));
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_UNREACHABLE, late.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    guest.destroy();
    late.destroy();
}

void test_hosting_again_on_the_port_of_a_lingering_destroy_starts_at_once(void) {
    steam_net_link_config_t config = config_of(1, 2);

    side host = host_on_free_port(config);
    const uint16_t port = host.port;
    side guest = join(config, port);
    connect(&host, &guest);
    queue_and_close(&host, NET_LINK_REASON_APP_FIRST + 42);
    const double before = steam_live::now_s();
    side again;
    again.port = port;
    again.link = steam_net_link_host_ip(&config, "127.0.0.1", port);
    const double took = steam_live::now_s() - before;
    printf("hosting again on a kept port took %.3f s\n", took);
    TEST_ASSERT_NOT_NULL(again.link);
    TEST_ASSERT_TRUE(took < 1.0);
    side next = join(config, port);
    TEST_ASSERT_TRUE(tick_until({&again, &next}, 10.0, [&] { return again.peer() && next.peer(); }));
    /* The old close was cut: closed at once, so the old guest reads its code with whatever
       had arrived, in order, and not the whole queue. */
    const double hosted = steam_live::now_s();
    tick_until({&again, &next, &guest}, 5.0, [&] { return guest.first(NET_LINK_EVENT_DISCONNECTED) != nullptr; });
    printf("hosting again: the old guest left %s after %.2f s, with %u messages\n", guest.first(NET_LINK_EVENT_DISCONNECTED) ? "" : "NEVER", steam_live::now_s() - hosted,
           static_cast<unsigned>(guest.count(NET_LINK_EVENT_MESSAGE)));
    TEST_ASSERT_NOT_NULL(guest.first(NET_LINK_EVENT_DISCONNECTED));
    uint32_t seq = 0;
    for (const got &g : guest.log) {
        if (g.type == NET_LINK_EVENT_MESSAGE) {
            TEST_ASSERT_TRUE(g.data == pattern(QUEUE_ITEM, seq));
            seq++;
        }
    }
    printf("hosting again: the old guest read %u of %u messages, then reason %u\n", seq, LONG_QUEUE, guest.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_TRUE(seq < LONG_QUEUE);
    TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_APP_FIRST + 42, guest.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    again.destroy();
    next.destroy();
    guest.destroy();
}

/* Steam frees a port a moment after its listen socket closes: an open right after a host's
   destroy waits for that instead of answering NULL. */
void test_hosting_again_right_after_a_destroy_gets_the_port(void) {
    no_closes_in_hand();
    const steam_net_link_config_t config = config_of(1, 1);
    side host = host_on_free_port(config);
    const uint16_t port = host.port;
    double slowest = 0.0;
    for (int round = 0; round < 5; round++) {
        side guest = join(config, port);
        connect(&host, &guest);
        host.destroy();
        const double before = steam_live::now_s();
        host.link = steam_net_link_host_ip(&config, "127.0.0.1", port);
        const double took = steam_live::now_s() - before;
        slowest = took > slowest ? took : slowest;
        TEST_ASSERT_NOT_NULL_MESSAGE(host.link, "the port of a host just destroyed was refused");
        host.log.clear();
        guest.destroy();
    }
    printf("port: hosting again right after a destroy took %.3f s at most\n", slowest);
    TEST_ASSERT_TRUE(slowest < 0.5);
    host.destroy();
}

/* A destroyed host whose queue outlasts close_wait_s: the close goes out at the deadline, so
   its guest reads the code with what had arrived and is gone before the port is hosted again.
   Left to Steam's own linger, that close died with the listen socket and the guest stayed
   connected and silent while the new host held the port. */
void test_a_destroyed_hosts_close_past_its_deadline_still_delivers_the_code(void) {
    constexpr uint32_t COUNT = 200, CODE = NET_LINK_REASON_APP_FIRST + 45;
    no_closes_in_hand();
    steam_net_link_config_t config = config_of(1, 2);
    config.timeout_s = 1.0F;
    config.close_wait_s = 1.0F;
    /* Twelve seconds of sending at Steam's default rate. */
    config.send_buffer_bytes = 4 * 1024 * 1024;
    side host = host_on_free_port(config);
    const uint16_t port = host.port;
    side guest = join(config, port);
    connect(&host, &guest);
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        const std::vector<uint8_t> item = pattern(QUEUE_ITEM, seq);
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_RELIABLE, item.data(), QUEUE_ITEM));
    }
    net_link_close(host.link, host.peer(), CODE);
    host.destroy();
    const double closed = steam_live::now_s();
    const double wait = static_cast<double>(config.close_wait_s);
    TEST_ASSERT_TRUE_MESSAGE(tick_until({&guest}, wait + 1.5, [&] { return guest.first(NET_LINK_EVENT_DISCONNECTED) != nullptr; }), "the close past its deadline never reached the guest");
    const double heard = steam_live::now_s() - closed;
    uint32_t seq = 0;
    for (const got &g : guest.log) {
        if (g.type == NET_LINK_EVENT_MESSAGE) {
            TEST_ASSERT_TRUE(g.data == pattern(QUEUE_ITEM, seq));
            seq++;
        }
    }
    printf("destroyed host: the guest read %u of %u messages, then the code after %.2f s (close_wait_s %.1f)\n", seq, COUNT, heard, wait);
    TEST_ASSERT_EQUAL_UINT32(CODE, guest.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    TEST_ASSERT_TRUE(seq < COUNT);
    TEST_ASSERT_TRUE_MESSAGE(heard >= wait * 0.8, "the close did not wait for its acknowledgement");
    TEST_ASSERT_TRUE(tick_until({&guest}, 3.0, [] { return !steam_net_link_lingering(); }));
    side again;
    again.port = port;
    again.link = steam_net_link_host_ip(&config, "127.0.0.1", port);
    TEST_ASSERT_NOT_NULL(again.link);
    tick_until({&again, &guest}, 0.3, [] { return false; });
    TEST_ASSERT_EQUAL_size_t(1, guest.count(NET_LINK_EVENT_DISCONNECTED));
    again.destroy();
    guest.destroy();
}

/* A small close on a clean path: acknowledged at once, then only the hold after the close
   (two keepalives of a 1 s timeout). */
void test_a_small_close_ends_its_linger_quickly_in_either_role(void) {
    steam_net_link_config_t config = config_of(1, 1);
    config.timeout_s = 1.0F;
    tick_until({}, 10.0, [] { return !steam_net_link_lingering(); });
    TEST_ASSERT_FALSE(steam_net_link_lingering());
    for (int host_closes = 0; host_closes < 2; host_closes++) {
        side host = host_on_free_port(config);
        side guest = join(config, host.port);
        connect(&host, &guest);
        side *closer = host_closes ? &host : &guest;
        const std::vector<uint8_t> small = pattern(64, 9);
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(closer->link, closer->peer(), 0, NET_LINK_SEND_RELIABLE, small.data(), 64));
        net_link_close(closer->link, closer->peer(), NET_LINK_REASON_APP_FIRST + 7);
        const double closed = steam_live::now_s();
        TEST_ASSERT_TRUE_MESSAGE(steam_net_link_lingering(), "a close is in hand from net_link_close on");
        closer->destroy();
        TEST_ASSERT_TRUE(steam_net_link_lingering());
        side *other = host_closes ? &guest : &host;
        TEST_ASSERT_TRUE(tick_until({other}, 6.0, [] { return !steam_net_link_lingering(); }));
        const double lasted = steam_live::now_s() - closed;
        printf("lingering: a small %s close lasted %.2f s\n", host_closes ? "host" : "guest", lasted);
        TEST_ASSERT_TRUE(lasted >= 0.3 && lasted < 1.0);
        TEST_ASSERT_TRUE(tick_until({other}, 3.0, [&] { return other->first(NET_LINK_EVENT_DISCONNECTED) != nullptr; }));
        TEST_ASSERT_EQUAL_size_t(1, other->count(NET_LINK_EVENT_MESSAGE));
        TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_APP_FIRST + 7, other->first(NET_LINK_EVENT_DISCONNECTED)->reason);
        other->destroy();
    }
}

/* Under loss and lag a burst takes longer than any fixed guess; the close waits for Steam's
   acknowledgement, so the other side has every message before lingering ends, then the code. */
void test_a_close_under_loss_lingers_until_its_burst_is_acknowledged_in_either_role(void) {
    constexpr uint32_t COUNT = 10, ITEM = 16384;
    constexpr uint32_t CODE = NET_LINK_REASON_APP_FIRST + 77;
    steam_net_link_config_t config = config_of(1, 1);
    config.timeout_s = 3.0F;
    config.close_wait_s = 8.0F;
    for (int host_closes = 0; host_closes < 2; host_closes++) {
        side host = host_on_free_port(config);
        side guest = join(config, host.port);
        connect(&host, &guest);
        side *closer = host_closes ? &host : &guest;
        side *other = host_closes ? &guest : &host;
        const steam_net_link_fake_t bad{0.1F, 80.0F, 0.0F, 0.0F};
        steam_net_link_set_fake(&bad, nullptr, 0.0F);
        for (uint32_t seq = 0; seq < COUNT; seq++) {
            const std::vector<uint8_t> item = pattern(ITEM, seq);
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(closer->link, closer->peer(), 0, NET_LINK_SEND_RELIABLE, item.data(), ITEM));
        }
        net_link_close(closer->link, closer->peer(), CODE);
        closer->destroy();
        const double closed = steam_live::now_s();
        size_t messages_when_done = 0;
        double lasted = -1.0;
        tick_until({other}, 20.0, [&] {
            if (lasted < 0 && !steam_net_link_lingering()) {
                lasted = steam_live::now_s() - closed;
                messages_when_done = other->count(NET_LINK_EVENT_MESSAGE);
            }
            return lasted >= 0 && other->first(NET_LINK_EVENT_DISCONNECTED) != nullptr;
        });
        steam_net_link_set_fake(nullptr, nullptr, 0.0F);
        printf("lossy: a %s close of %u KB lingered %.2f s; the other side had %u messages then\n", host_closes ? "host" : "guest", COUNT * ITEM / 1024,
               lasted, static_cast<unsigned>(messages_when_done));
        TEST_ASSERT_TRUE(lasted >= 0);
        /* Unless the wait for the acknowledgement is what cut it, everything had landed. */
        if (lasted < static_cast<double>(config.close_wait_s)) {
            TEST_ASSERT_EQUAL_size_t(COUNT, messages_when_done);
        }
        uint32_t seq = 0;
        for (const got &g : other->log) {
            if (g.type == NET_LINK_EVENT_MESSAGE) {
                TEST_ASSERT_TRUE(g.data == pattern(ITEM, seq));
                seq++;
            }
        }
        TEST_ASSERT_EQUAL_UINT32(COUNT, seq);
        TEST_ASSERT_NOT_NULL(other->first(NET_LINK_EVENT_DISCONNECTED));
        TEST_ASSERT_EQUAL_UINT32(CODE, other->first(NET_LINK_EVENT_DISCONNECTED)->reason);
        other->destroy();
    }
}


/* Everything this process sends is dropped from the close on: no acknowledgement can come, so
   close_wait_s is what ends the wait. The close then goes out at once and what was not
   delivered is lost; its packet is dropped too, so the guest hears the code only when the
   loss is lifted and it sends something the closed connection answers, inside the hold. */
void test_the_acknowledgement_deadline_closes_with_the_code_and_drops_the_rest(void) {
    constexpr uint32_t COUNT = 10, ITEM = 16384, CODE = NET_LINK_REASON_APP_FIRST + 88;
    no_closes_in_hand();
    steam_net_link_config_t config = config_of(1, 1);
    /* Long enough that a timeout cannot be what ends anything here. */
    config.timeout_s = 30.0F;
    config.close_wait_s = 1.0F;
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    for (uint32_t seq = 0; seq < COUNT; seq++) {
        const std::vector<uint8_t> item = pattern(ITEM, seq);
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_RELIABLE, item.data(), ITEM));
    }
    const steam_net_link_fake_t all{1.0F, 0.0F, 0.0F, 0.0F};
    steam_net_link_set_fake(&all, nullptr, 0.0F);
    net_link_close(host.link, host.peer(), CODE);
    const double closed = steam_live::now_s();
    const double wait = static_cast<double>(config.close_wait_s);
    tick_until({&host, &guest}, wait - 0.2, [] { return false; });
    TEST_ASSERT_TRUE_MESSAGE(steam_net_link_lingering(), "the close was done before its deadline with nothing acknowledged");
    TEST_ASSERT_NULL(guest.first(NET_LINK_EVENT_DISCONNECTED));
    /* Past the deadline the connection is closed; the hold (3 s here) is what still answers. */
    tick_until({&host, &guest}, 0.5, [] { return false; });
    steam_net_link_set_fake(nullptr, nullptr, 0.0F);
    TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(guest.link, guest.peer(), 0, NET_LINK_SEND_RELIABLE, "?", 1));
    TEST_ASSERT_TRUE_MESSAGE(tick_until({&host, &guest}, 2.0, [&] { return guest.first(NET_LINK_EVENT_DISCONNECTED) != nullptr; }), "the close past its deadline never reached the guest");
    TEST_ASSERT_EQUAL_UINT32(CODE, guest.first(NET_LINK_EVENT_DISCONNECTED)->reason);
    uint32_t seq = 0;
    for (const got &g : guest.log) {
        if (g.type == NET_LINK_EVENT_MESSAGE) {
            TEST_ASSERT_TRUE(g.data == pattern(ITEM, seq));
            seq++;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(seq < COUNT, "everything arrived although nothing could be sent");
    /* The hold after it: two keepalives of a 30 s timeout are past the 3 s cap. */
    const double bound = wait + 3.0 + 0.5;
    TEST_ASSERT_TRUE(tick_until({&host, &guest}, bound, [] { return !steam_net_link_lingering(); }));
    const double lasted = steam_live::now_s() - closed;
    printf("deadline: the guest read the code with %u of %u messages; the close was in hand %.2f s (close_wait_s %.1f, bound %.1f)\n", seq, COUNT, lasted, wait, bound);
    TEST_ASSERT_TRUE(lasted >= wait && lasted <= bound);
    host.destroy();
    guest.destroy();
}

/* A stop cuts every connection; the closes in hand are forgotten with them. */
void test_a_stop_forgets_the_closes_in_hand(void) {
    no_closes_in_hand();
    const steam_net_link_config_t config = config_of(1, 1);
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    const std::vector<uint8_t> item = pattern(16384, 1);
    for (int i = 0; i < 20; i++) {
        net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_RELIABLE, item.data(), 16384);
    }
    net_link_close(host.link, host.peer(), NET_LINK_REASON_APP_FIRST + 3);
    host.destroy();
    TEST_ASSERT_TRUE(steam_net_link_lingering());
    steam_platform_stop();
    guest.destroy();
    TEST_ASSERT_EQUAL_INT(0, steam_live::start());
    TEST_ASSERT_FALSE_MESSAGE(steam_net_link_lingering(), "a close survived the stop");
}

/* The guest leaves while the host's close still waits for a burst that needs seconds: the
   host's close is freed by the peer's leaving, long before its close_wait_s. */
void test_a_peer_that_leaves_first_ends_the_close_waiting_for_it(void) {
    no_closes_in_hand();
    steam_net_link_config_t config = config_of(1, 1);
    config.timeout_s = 1.0F;
    config.close_wait_s = 20.0F;
    side host = host_on_free_port(config);
    side guest = join(config, host.port);
    connect(&host, &guest);
    const std::vector<uint8_t> item = pattern(16384, 2);
    for (int i = 0; i < 30; i++) {
        TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(host.link, host.peer(), 0, NET_LINK_SEND_RELIABLE, item.data(), 16384));
    }
    net_link_close(host.link, host.peer(), NET_LINK_REASON_APP_FIRST + 4);
    const double closed = steam_live::now_s();
    /* The guest's own close has nothing to deliver: it is done after its hold. */
    net_link_close(guest.link, guest.peer(), NET_LINK_REASON_APP_FIRST + 5);
    guest.destroy();
    TEST_ASSERT_TRUE(tick_until({&host}, 10.0, [] { return !steam_net_link_lingering(); }));
    const double lasted = steam_live::now_s() - closed;
    printf("peer left first: the closes were done after %.2f s (the burst needs about 2 s, close_wait_s 20)\n", lasted);
    TEST_ASSERT_TRUE(lasted < 1.5);
    host.destroy();
}

/* Two closes in hand at once on one destroyed host, under loss and lag: each guest gets all of
   its own burst and its own code. */
void test_two_closes_on_one_destroyed_host_both_arrive_under_loss(void) {
    constexpr uint32_t COUNT = 6, ITEM = 16384;
    no_closes_in_hand();
    steam_net_link_config_t config = config_of(1, 2);
    config.timeout_s = 3.0F;
    config.close_wait_s = 8.0F;
    side host = host_on_free_port(config);
    side first = join(config, host.port);
    side second = join(config, host.port);
    TEST_ASSERT_TRUE(tick_until({&host, &first, &second}, 10.0, [&] { return host.count(NET_LINK_EVENT_CONNECTED) == 2 && first.peer() && second.peer(); }));
    /* Each guest says hello, so the host knows which peer is which. */
    const uint8_t one = 1, two = 2;
    net_link_send(first.link, first.peer(), 0, NET_LINK_SEND_RELIABLE, &one, 1);
    net_link_send(second.link, second.peer(), 0, NET_LINK_SEND_RELIABLE, &two, 1);
    TEST_ASSERT_TRUE(tick_until({&host, &first, &second}, 5.0, [&] { return host.count(NET_LINK_EVENT_MESSAGE) == 2; }));
    net_link_peer_t to[3] = {};
    for (const got &g : host.log) {
        if (g.type == NET_LINK_EVENT_MESSAGE) {
            to[g.data[0]] = g.peer;
        }
    }
    const steam_net_link_fake_t bad{0.1F, 80.0F, 0.0F, 0.0F};
    steam_net_link_set_fake(&bad, nullptr, 0.0F);
    for (uint32_t who = 1; who <= 2; who++) {
        for (uint32_t seq = 0; seq < COUNT; seq++) {
            const std::vector<uint8_t> item = pattern(ITEM, seq + 100 * who);
            TEST_ASSERT_EQUAL_INT(NET_LINK_SEND_OK, net_link_send(host.link, to[who], 0, NET_LINK_SEND_RELIABLE, item.data(), ITEM));
        }
        net_link_close(host.link, to[who], NET_LINK_REASON_APP_FIRST + 60 + who);
    }
    host.destroy();
    TEST_ASSERT_TRUE(tick_until({&first, &second}, 20.0, [&] { return first.first(NET_LINK_EVENT_DISCONNECTED) && second.first(NET_LINK_EVENT_DISCONNECTED); }));
    side *guests[2] = {&first, &second};
    for (uint32_t who = 1; who <= 2; who++) {
        uint32_t seq = 0;
        for (const got &g : guests[who - 1]->log) {
            if (g.type == NET_LINK_EVENT_MESSAGE) {
                TEST_ASSERT_TRUE(g.data == pattern(ITEM, seq + 100 * who));
                seq++;
            }
        }
        printf("two closes: guest %u read %u of %u, then reason %u\n", who, seq, COUNT, guests[who - 1]->first(NET_LINK_EVENT_DISCONNECTED)->reason);
        TEST_ASSERT_EQUAL_UINT32(COUNT, seq);
        TEST_ASSERT_EQUAL_UINT32(NET_LINK_REASON_APP_FIRST + 60 + who, guests[who - 1]->first(NET_LINK_EVENT_DISCONNECTED)->reason);
    }
    first.destroy();
    second.destroy();
}

} // namespace

int main(int argc, char **argv) {
    /* Steam writes its own complaints to stderr; unbuffered, the log shows them where they happen. */
    setvbuf(stdout, nullptr, _IONBF, 0);
    const int started = steam_live::start(steam_live::simulate_no_client(argc, argv));
    if (started != 0) {
        return started;
    }
    use_block(steam_live::BLOCK_CLOSES);
    s_filter_prefix = "STEAM_NET_LINK_CLOSES";
    UNITY_BEGIN();
    RUN_CASE(test_a_small_close_ends_its_linger_quickly_in_either_role);
    RUN_CASE(test_a_close_under_loss_lingers_until_its_burst_is_acknowledged_in_either_role);
    RUN_CASE(test_the_acknowledgement_deadline_closes_with_the_code_and_drops_the_rest);
    RUN_CASE(test_a_peer_that_leaves_first_ends_the_close_waiting_for_it);
    RUN_CASE(test_two_closes_on_one_destroyed_host_both_arrive_under_loss);
    RUN_CASE(test_a_long_host_close_lingers_past_destroy_and_its_port_turns_guests_away);
    RUN_CASE(test_hosting_again_on_the_port_of_a_lingering_destroy_starts_at_once);
    RUN_CASE(test_a_destroyed_hosts_close_past_its_deadline_still_delivers_the_code);
    RUN_CASE(test_hosting_again_right_after_a_destroy_gets_the_port);
    RUN_CASE(test_a_stop_forgets_the_closes_in_hand);
    const int failures = UNITY_END();
    steam_platform_stop();
    return failures;
}
