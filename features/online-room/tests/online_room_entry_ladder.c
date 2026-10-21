// The connection ladder under a test clock and random source: tries back off
// with jitter and give way to the player's pick, never to solo on their own;
// only a connection that stood starts over; one 4100 per window re-enters at
// once and the next ones are failed tries, so a room that restarts is waited
// out; a full room and a version refusal stop the ladder at once; no room
// means solo, or nowhere without a solo build.
#include "unity.h"

#include "online_room/entry.h"

void setUp(void) {}
void tearDown(void) {}

// A fixture of its own, so the game's knobs can move without this test.
static const online_entry_knobs k_knobs = {
    .connect_attempts = 3U, .connect_timeout_seconds = 5.0F, .retry_seconds = 1.0F, .retry_max_seconds = 1.5F,
    .stable_online_seconds = 10.0F, .reenter_window_seconds = 30.0F,
};

// The draw the test hands the ladder next.
static double s_draw;
static double draw(void *user) {
    (void)user;
    return s_draw;
}

static online_entry_action start(online_entry *entry, bool room, bool solo, double now) {
    s_draw = 0.5;
    return online_entry_start(entry, k_knobs, room, solo, draw, NULL, now);
}

// Fails every try with `code` until the ladder asks; returns the time it asked at.
static double exhaust(online_entry *entry, uint16_t code, double now) {
    (void)online_entry_closed(entry, code, now);
    while (entry->state != ONLINE_ENTRY_ASKING) {
        if (entry->state == ONLINE_ENTRY_WAITING) {
            now = entry->retry_at;
            TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_step(entry, now));
        }
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECTING, entry->state);
        (void)online_entry_closed(entry, code, now);
    }
    TEST_ASSERT_EQUAL(code, entry->close_code);
    return now;
}

static void test_no_room_plays_solo_or_goes_nowhere(void) {
    online_entry entry;
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_GO_SOLO, start(&entry, false, true, 0.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_SOLO, entry.state);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, start(&entry, false, false, 0.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_NOWHERE, entry.state);
}

static void test_failed_tries_back_off_then_ask(void) {
    online_entry entry;
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, start(&entry, true, true, 0.0));
    double now = 0.0;
    double last_wait = 0.0;
    for (uint32_t attempt = 1U; attempt < k_knobs.connect_attempts; ++attempt) {
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, 0U, now));
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_WAITING, entry.state);
        const double wait = online_entry_retry_in(&entry, now);
        // With the jitter held at its middle: never shorter than the last wait, never past the cap.
        TEST_ASSERT_TRUE(wait >= last_wait && wait > 0.0 && wait <= (double)k_knobs.retry_max_seconds);
        last_wait = wait;
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_step(&entry, now + wait * 0.5));
        now += wait;
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_step(&entry, now));
        TEST_ASSERT_EQUAL(attempt + 1U, entry.attempt);
    }
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, 0U, now));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_ASKING, entry.state);
    // Asking waits for the player however long it takes.
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_step(&entry, now + 3600.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_ASKING, entry.state);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_RETRY, now));
    TEST_ASSERT_EQUAL(1U, entry.attempt);
    now = exhaust(&entry, 0U, now);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_GO_SOLO, online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_SOLO, now));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_SOLO, entry.state);
}

static void test_waits_are_jittered_within_a_quarter(void) {
    online_entry entry;
    (void)start(&entry, true, true, 0.0);
    s_draw = 0.0;
    (void)online_entry_closed(&entry, 0U, 0.0);
    const double low = online_entry_retry_in(&entry, 0.0);
    (void)start(&entry, true, true, 0.0);
    s_draw = 0.999;
    (void)online_entry_closed(&entry, 0U, 0.0);
    const double high = online_entry_retry_in(&entry, 0.0);
    TEST_ASSERT_TRUE(low >= 0.75 * (double)k_knobs.retry_seconds - 1e-9);
    TEST_ASSERT_TRUE(high <= 1.25 * (double)k_knobs.retry_seconds + 1e-9);
    TEST_ASSERT_TRUE(high > low);
}

static void test_without_solo_the_player_can_only_retry(void) {
    online_entry entry;
    (void)start(&entry, true, false, 0.0);
    const double now = exhaust(&entry, 0U, 0.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_SOLO, now));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_ASKING, entry.state);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_RETRY, now));
}

// Between tries the lost window offers Retry and Solo as the first client's
// did: a retry tries now without spending the ladder, a solo leaves.
static void test_a_wait_can_be_cut_short_or_left_for_solo(void) {
    online_entry entry;
    (void)start(&entry, true, true, 0.0);
    (void)online_entry_closed(&entry, 0U, 0.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_WAITING, entry.state);
    const uint32_t attempt = entry.attempt;
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_RETRY, 0.1));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECTING, entry.state);
    TEST_ASSERT_EQUAL_UINT32(attempt + 1U, entry.attempt);
    (void)online_entry_closed(&entry, 0U, 0.2);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_WAITING, entry.state);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_GO_SOLO, online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_SOLO, 0.3));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_SOLO, entry.state);
}

static void test_a_hung_try_is_dropped(void) {
    online_entry entry;
    (void)start(&entry, true, true, 10.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_step(&entry, 10.0 + (double)k_knobs.connect_timeout_seconds * 0.5));
    const double hung = 10.0 + (double)k_knobs.connect_timeout_seconds;
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_DISCONNECT, online_entry_step(&entry, hung));
    // The host reports the dropped try closed with no code: a failed try like any other.
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, 0U, hung));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_WAITING, entry.state);
    TEST_ASSERT_EQUAL(0U, entry.close_code);
}

// The seat refusal says the room is full now; knocking again would only meet it again.
static void test_a_full_room_asks_at_once(void) {
    online_entry entry;
    (void)start(&entry, true, true, 0.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, ONLINE_ENTRY_CLOSE_FULL, 0.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_ASKING, entry.state);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CLOSE_FULL, entry.close_code);
    TEST_ASSERT_EQUAL(1U, entry.connects);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_step(&entry, 1000.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_RETRY, 1000.0));
    // A connection that stood is not started over into a full room either.
    (void)online_entry_opened(&entry, 1000.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, ONLINE_ENTRY_CLOSE_FULL, 2000.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_ASKING, entry.state);
}

// A room that never answers asks with no code, which the panel reads as unreachable.
static void test_an_unreachable_room_asks_with_no_code(void) {
    online_entry entry;
    (void)start(&entry, true, true, 0.0);
    (void)exhaust(&entry, 0U, 0.0);
    TEST_ASSERT_EQUAL(k_knobs.connect_attempts, entry.attempt);
}

// A ladder long enough to wait out a restart: the room drains its seats with
// 4100 for up to 8 s, then is gone until its container starts again.
static const online_entry_knobs k_restart_knobs = {
    .connect_attempts = 6U, .connect_timeout_seconds = 5.0F, .retry_seconds = 1.0F, .retry_max_seconds = 8.0F,
    .stable_online_seconds = 10.0F, .reenter_window_seconds = 30.0F,
};

// A metaserver with no seat to sell refuses before any room opens: ordinary
// failed tries that keep the busy code, never a re-entry, so a refusal that
// lasts past the re-enter window still ends with the player asked.
static void test_a_busy_room_keeps_retrying_with_its_code(void) {
    online_entry entry;
    (void)online_entry_start(&entry, k_restart_knobs, true, true, draw, NULL, 0.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, ONLINE_ROOM_CLOSE_REENTER, 0.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_WAITING, entry.state);
    TEST_ASSERT_EQUAL(ONLINE_ROOM_CLOSE_REENTER, entry.close_code);
    // Each refusal takes the metaserver a few seconds, so the ladder outlasts
    // the re-enter window.
    double now = 0.0;
    while (entry.state != ONLINE_ENTRY_ASKING) {
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_WAITING, entry.state);
        now = entry.retry_at;
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_step(&entry, now));
        now += 4.0;
        (void)online_entry_closed(&entry, ONLINE_ROOM_CLOSE_REENTER, now);
    }
    TEST_ASSERT_TRUE(now > (double)k_restart_knobs.reenter_window_seconds);
    TEST_ASSERT_EQUAL(k_restart_knobs.connect_attempts, entry.attempt);
    TEST_ASSERT_EQUAL(ONLINE_ROOM_CLOSE_REENTER, entry.close_code);
}

static void test_a_restart_is_waited_out(void) {
    const double drain = 8.0;
    const double backs[] = {9.0, 12.0};
    for (uint32_t index = 0U; index < 2U; ++index) {
        online_entry entry;
        s_draw = 0.0; // the shortest waits spend the tries soonest
        (void)online_entry_start(&entry, k_restart_knobs, true, true, draw, NULL, 0.0);
        (void)online_entry_opened(&entry, 0.0);
        const double t0 = 100.0;
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_closed(&entry, ONLINE_ROOM_CLOSE_REENTER, t0));
        double now = t0;
        while (now < t0 + backs[index]) {
            TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECTING, entry.state);
            (void)online_entry_closed(&entry, now < t0 + drain ? ONLINE_ROOM_CLOSE_REENTER : 0U, now);
            TEST_ASSERT_EQUAL(ONLINE_ENTRY_WAITING, entry.state);
            now = entry.retry_at;
            TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_step(&entry, now));
        }
        (void)online_entry_opened(&entry, now);
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_ONLINE, entry.state);
    }
}

// The cap bounds the backoff; the jitter then spreads even the capped waits.
static void test_the_cap_comes_before_the_jitter(void) {
    online_entry entry;
    (void)online_entry_start(&entry, k_restart_knobs, true, true, draw, NULL, 0.0);
    s_draw = 0.999;
    double now = 0.0;
    // The fifth wait would be 16 s uncapped.
    for (uint32_t attempt = 1U; attempt < 5U; ++attempt) {
        (void)online_entry_closed(&entry, 0U, now);
        now = entry.retry_at;
        (void)online_entry_step(&entry, now);
    }
    (void)online_entry_closed(&entry, 0U, now);
    const double cap = (double)k_restart_knobs.retry_max_seconds;
    const double wait = online_entry_retry_in(&entry, now);
    TEST_ASSERT_TRUE(wait > cap && wait <= 1.25 * cap + 1e-9);
}

static void test_only_a_connection_that_stood_starts_over(void) {
    online_entry entry;
    (void)start(&entry, true, true, 0.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_opened(&entry, 0.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_ONLINE, entry.state);
    TEST_ASSERT_TRUE(entry.ever_online);
    // It stood: reconnect at once with every try again.
    const double stood = (double)k_knobs.stable_online_seconds;
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_closed(&entry, 1006U, stood));
    TEST_ASSERT_EQUAL(1U, entry.attempt);
    // Accepted, then dropped at once: a failed try, so the ladder still ends in asking.
    double now = stood;
    for (uint32_t round = 0U; round < k_knobs.connect_attempts; ++round) {
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECTING, entry.state);
        (void)online_entry_opened(&entry, now);
        (void)online_entry_closed(&entry, 1006U, now + 0.1);
        now = entry.state == ONLINE_ENTRY_WAITING ? entry.retry_at : now + 0.1;
        if (entry.state == ONLINE_ENTRY_WAITING) { (void)online_entry_step(&entry, now); }
    }
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_ASKING, entry.state);
}

static void test_one_reenter_per_window(void) {
    online_entry entry;
    (void)start(&entry, true, true, 0.0);
    (void)online_entry_opened(&entry, 0.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_closed(&entry, ONLINE_ROOM_CLOSE_REENTER, 5.0));
    TEST_ASSERT_EQUAL(1U, entry.attempt);
    TEST_ASSERT_EQUAL(ONLINE_ROOM_CLOSE_REENTER, entry.close_code);
    // A room that keeps sending the client away: inside the window every 4100 is a failed try.
    double now = 5.0;
    while (entry.state != ONLINE_ENTRY_ASKING) {
        (void)online_entry_opened(&entry, now);
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, ONLINE_ROOM_CLOSE_REENTER, now + 0.1));
        if (entry.state == ONLINE_ENTRY_WAITING) {
            now = entry.retry_at;
            TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_step(&entry, now));
        }
        TEST_ASSERT_TRUE(now < 5.0 + (double)k_knobs.reenter_window_seconds);
    }
    // Past the window a 4100 re-enters at once again.
    (void)online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_RETRY, 40.0);
    (void)online_entry_opened(&entry, 40.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_closed(&entry, ONLINE_ROOM_CLOSE_REENTER, 40.0));
}

static void test_a_version_refusal_asks_for_a_reload(void) {
    online_entry entry;
    (void)start(&entry, true, true, 0.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, ONLINE_ENTRY_CLOSE_VERSION, 0.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_OUTDATED, entry.state);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_step(&entry, 1000.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_SOLO, 1000.0));
}

// A profile a newer build wrote is a reload too; a room that says the
// account plays elsewhere, or that its profile does not read, is not tried
// again by itself.
static void test_a_room_that_will_not_keep_the_profile_stops_the_ladder(void) {
    online_entry entry;
    (void)start(&entry, true, true, 0.0);
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, ONLINE_ROOM_CLOSE_PROFILE_NEWER, 0.0));
    TEST_ASSERT_EQUAL(ONLINE_ENTRY_OUTDATED, entry.state);
    const uint16_t codes[] = {ONLINE_ROOM_CLOSE_SUPERSEDED, ONLINE_ROOM_CLOSE_PROFILE_BROKEN};
    for (uint32_t index = 0U; index < 2U; ++index) {
        (void)start(&entry, true, true, 0.0);
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_closed(&entry, codes[index], 0.0));
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_ASKING, entry.state);
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_KEEP, online_entry_step(&entry, 1000.0));
        TEST_ASSERT_EQUAL(ONLINE_ENTRY_CONNECT, online_entry_choose(&entry, ONLINE_ENTRY_CHOOSE_RETRY, 1000.0));
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_room_plays_solo_or_goes_nowhere);
    RUN_TEST(test_failed_tries_back_off_then_ask);
    RUN_TEST(test_waits_are_jittered_within_a_quarter);
    RUN_TEST(test_without_solo_the_player_can_only_retry);
    RUN_TEST(test_a_wait_can_be_cut_short_or_left_for_solo);
    RUN_TEST(test_a_hung_try_is_dropped);
    RUN_TEST(test_a_full_room_asks_at_once);
    RUN_TEST(test_an_unreachable_room_asks_with_no_code);
    RUN_TEST(test_a_busy_room_keeps_retrying_with_its_code);
    RUN_TEST(test_a_restart_is_waited_out);
    RUN_TEST(test_the_cap_comes_before_the_jitter);
    RUN_TEST(test_only_a_connection_that_stood_starts_over);
    RUN_TEST(test_one_reenter_per_window);
    RUN_TEST(test_a_version_refusal_asks_for_a_reload);
    RUN_TEST(test_a_room_that_will_not_keep_the_profile_stops_the_ladder);
    return UNITY_END();
}
