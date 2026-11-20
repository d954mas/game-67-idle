/* The shim's own parse of Steam callbacks, fed as raw bytes in Steam's real structs: no Steam
   client is touched. */

#include "steam_platform_internal.h"

#include <steam/steam_api.h>

extern "C" {
#include "unity.h"
}

extern "C" void setUp(void) { steam_platform_stop(); }
extern "C" void tearDown(void) { steam_platform_stop(); }

namespace {

void feed_overlay(bool active) {
    GameOverlayActivated_t overlay{};
    overlay.m_bActive = active ? 1 : 0;
    overlay.m_nAppID = 480;
    TEST_ASSERT_TRUE(steam_platform_internal_feed(GameOverlayActivated_t::k_iCallback, &overlay, sizeof overlay));
}

int drain(bool *last_active) {
    int count = 0;
    steam_platform_event_t event;
    while (steam_platform_next_event(&event)) {
        TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_EVENT_OVERLAY, event.type);
        if (last_active != nullptr) {
            *last_active = event.active;
        }
        count++;
    }
    return count;
}

void test_overlay_callback_becomes_events_on_changes_only() {
    feed_overlay(true);
    feed_overlay(true);
    feed_overlay(false);
    feed_overlay(false);
    steam_platform_event_t event;
    TEST_ASSERT_TRUE(steam_platform_next_event(&event));
    TEST_ASSERT_TRUE(event.active);
    TEST_ASSERT_TRUE(steam_platform_next_event(&event));
    TEST_ASSERT_FALSE(event.active);
    TEST_ASSERT_FALSE(steam_platform_next_event(&event));
}

void test_events_stay_queued_and_null_out_takes_nothing() {
    feed_overlay(true);
    steam_platform_run_frame();
    TEST_ASSERT_FALSE(steam_platform_next_event(nullptr));
    bool active = false;
    TEST_ASSERT_EQUAL_INT(1, drain(&active));
    TEST_ASSERT_TRUE(active);
}

void test_full_queue_drops_the_oldest() {
    steam_platform_internal_counters_t before;
    steam_platform_internal_counters(&before);
    const int fed = STEAM_PLATFORM_EVENT_CAPACITY + 3;
    for (int i = 0; i < fed; i++) {
        feed_overlay(i % 2 == 0);
    }
    steam_platform_internal_counters_t after;
    steam_platform_internal_counters(&after);
    TEST_ASSERT_EQUAL_UINT64(3, after.events_dropped - before.events_dropped);
    steam_platform_event_t event;
    TEST_ASSERT_TRUE(steam_platform_next_event(&event));
    TEST_ASSERT_FALSE(event.active); /* the fourth fed, index 3 */
    bool last = true;
    TEST_ASSERT_EQUAL_INT(STEAM_PLATFORM_EVENT_CAPACITY - 1, drain(&last));
    TEST_ASSERT_TRUE(last);
}

void test_stop_drops_events_and_forgets_the_overlay_state() {
    feed_overlay(true);
    steam_platform_stop();
    TEST_ASSERT_EQUAL_INT(0, drain(nullptr));
    /* After stop the overlay counts as closed again, so "shown" is news. */
    feed_overlay(true);
    TEST_ASSERT_EQUAL_INT(1, drain(nullptr));
}

void test_short_unknown_and_call_result_callbacks() {
    steam_platform_internal_counters_t before;
    steam_platform_internal_counters(&before);
    const unsigned char shown = 1;
    TEST_ASSERT_TRUE(steam_platform_internal_feed(GameOverlayActivated_t::k_iCallback, &shown, sizeof shown));
    TEST_ASSERT_EQUAL_INT(0, drain(nullptr));
    const int payload = 7;
    TEST_ASSERT_TRUE(steam_platform_internal_feed(424242, &payload, sizeof payload));
    SteamAPICallCompleted_t completed{};
    TEST_ASSERT_FALSE(steam_platform_internal_feed(SteamAPICallCompleted_t::k_iCallback, &completed, sizeof completed));
    TEST_ASSERT_FALSE(steam_platform_internal_feed(424242, nullptr, 4));
    steam_platform_internal_counters_t after;
    steam_platform_internal_counters(&after);
    TEST_ASSERT_EQUAL_UINT64(2, after.callbacks - before.callbacks);
    TEST_ASSERT_EQUAL_UINT64(2, after.unhandled - before.unhandled);
}

} // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_overlay_callback_becomes_events_on_changes_only);
    RUN_TEST(test_events_stay_queued_and_null_out_takes_nothing);
    RUN_TEST(test_full_queue_drops_the_oldest);
    RUN_TEST(test_stop_drops_events_and_forgets_the_overlay_state);
    RUN_TEST(test_short_unknown_and_call_result_callbacks);
    return UNITY_END();
}
