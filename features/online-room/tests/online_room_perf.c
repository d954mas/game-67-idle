// The room's perf window: log2 buckets, growth of counters that restart with
// a seat's occupant, the shape the metaserver decodes (room_perf_v1.json),
// and a window that does not fit is refused whole, never cut.
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "online_room/perf.h"

void setUp(void) {}
void tearDown(void) {}

static void test_buckets_are_log2_microseconds(void) {
    TEST_ASSERT_EQUAL_UINT32(0U, online_perf_bucket(0U));
    TEST_ASSERT_EQUAL_UINT32(0U, online_perf_bucket(15U));
    TEST_ASSERT_EQUAL_UINT32(1U, online_perf_bucket(16U));
    TEST_ASSERT_EQUAL_UINT32(1U, online_perf_bucket(31U));
    TEST_ASSERT_EQUAL_UINT32(2U, online_perf_bucket(32U));
    TEST_ASSERT_EQUAL_UINT32(16U, online_perf_bucket((1U << 20U) - 1U));
    TEST_ASSERT_EQUAL_UINT32(ONLINE_PERF_BUCKETS - 1U, online_perf_bucket(1U << 20U));
    TEST_ASSERT_EQUAL_UINT32(ONLINE_PERF_BUCKETS - 1U, online_perf_bucket(UINT32_MAX));
    online_perf_hist hist = {0};
    online_perf_hist_add(&hist, 20U);
    online_perf_hist_add(&hist, 5000U);
    TEST_ASSERT_EQUAL_UINT32(1U, hist.counts[1]);
    TEST_ASSERT_EQUAL_UINT32(1U, hist.counts[online_perf_bucket(5000U)]);
    TEST_ASSERT_EQUAL_UINT32(5000U, hist.max_us);
    TEST_ASSERT_EQUAL_UINT64(5020U, hist.sum_us);
}

static void window(online_perf_window *w, online_perf_totals *now, online_perf_totals *before) {
    online_perf_window_reset(w);
    memset(now, 0, sizeof *now);
    memset(before, 0, sizeof *before);
    online_perf_hist_add(&w->stages[ONLINE_PERF_STEP], 100U);
    online_perf_credit(w, 300);
    online_perf_credit(w, -20);
    now->ticks = 160U;
    before->ticks = 100U;
    now->net.rx_messages = 90U;
    before->net.rx_messages = 30U;
    now->underruns = 2U;
    before->underruns = 5U; // restarted: counts from zero
    now->has_process = before->has_process = true;
    now->process.user_us = 1500U;
    before->process.user_us = 500U;
    now->process.rss_kb = 9000U;
    now->has_machine = before->has_machine = true;
}

// The keys of a JSON text, nested ones included, in order, as "k1,k2,...".
static void keys_of(const char *json, char *out, size_t cap) {
    out[0] = '\0';
    size_t used = 0U;
    for (const char *at = json; (at = strchr(at, '"')) != NULL;) {
        const char *end = strchr(at + 1, '"');
        TEST_ASSERT_NOT_NULL(end);
        if (end[1] == ':') { used += (size_t)snprintf(out + used, cap - used, "%s%.*s", used > 0U ? "," : "", (int)(end - at - 1), at + 1); }
        at = end + 1;
    }
}

static void test_the_window_has_the_fixtures_shape_and_sends_growth(void) {
    online_perf_window w;
    online_perf_totals now, before;
    window(&w, &now, &before);
    char json[ONLINE_PERF_JSON_MAX];
    TEST_ASSERT_TRUE(online_perf_json(&w, &now, &before, 2000U, json, sizeof json) > 0U);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"window_ms\":2000,\"ticks\":60,"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"rx_messages\":60,"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"underruns\":2,"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"user_us\":1000,"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"rss_kb\":9000}"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"credit_min\":-20,\"credit_max\":300}"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"step\":{\"b\":[0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0],\"max\":100,\"sum\":100}"));
    FILE *file = fopen(ONLINE_ROOM_PERF_FIXTURE, "rb");
    TEST_ASSERT_NOT_NULL(file);
    char fixture[ONLINE_PERF_JSON_MAX];
    const size_t size = fread(fixture, 1U, sizeof fixture - 1U, file);
    fclose(file);
    fixture[size] = '\0';
    char want[2048];
    char got[2048];
    keys_of(fixture, want, sizeof want);
    keys_of(json, got, sizeof got);
    TEST_ASSERT_EQUAL_STRING(want, got);
    // A platform without the OS counters leaves those objects out.
    now.has_process = now.has_machine = false;
    TEST_ASSERT_TRUE(online_perf_json(&w, &now, &before, 2000U, json, sizeof json) > 0U);
    TEST_ASSERT_NULL(strstr(json, "\"process\""));
    TEST_ASSERT_NULL(strstr(json, "\"machine\""));
    TEST_ASSERT_NULL(strstr(json, "\"plan\""));
    // A room of a machine's plan says which rooms the machine runs.
    now.plan_rooms = 32U;
    now.plan_room_base = 1000U;
    TEST_ASSERT_TRUE(online_perf_json(&w, &now, &before, 2000U, json, sizeof json) > 0U);
    TEST_ASSERT_NOT_NULL(strstr(json, ",\"plan\":{\"rooms\":32,\"room_id_base\":1000}}"));
}

static void test_a_window_that_does_not_fit_is_refused_whole(void) {
    online_perf_window w;
    online_perf_totals now, before;
    window(&w, &now, &before);
    char json[ONLINE_PERF_JSON_MAX];
    const size_t size = online_perf_json(&w, &now, &before, 2000U, json, sizeof json);
    TEST_ASSERT_TRUE(size > 0U);
    TEST_ASSERT_EQUAL_size_t(0U, online_perf_json(&w, &now, &before, 2000U, json, size));
    TEST_ASSERT_EQUAL_size_t(size, online_perf_json(&w, &now, &before, 2000U, json, size + 1U));
    // Far past the busiest window a room sends (2 s, 32 seats): every bucket
    // and counter an order of magnitude over it, the interfaces' totals at
    // months of traffic, still fits.
    for (uint32_t stage = 0U; stage < ONLINE_PERF_STAGES; ++stage) {
        for (uint32_t bucket = 0U; bucket < ONLINE_PERF_BUCKETS; ++bucket) { w.stages[stage].counts[bucket] = 99999U; }
        w.stages[stage].max_us = 4000000000U;
        w.stages[stage].sum_us = 9999999999ULL;
    }
    for (uint32_t bucket = 0U; bucket < ONLINE_PERF_BUCKETS; ++bucket) { w.clients.rtt.counts[bucket] = 9999U; }
    w.clients.reports = 9999U;
    now.net.tx_bytes = now.net.rx_bytes = 99999999999ULL;
    now.machine.tx_bytes = now.machine.rx_bytes = 999999999999999ULL;
    now.plan_rooms = 4096U;
    now.plan_room_base = UINT32_MAX - 4096U;
    TEST_ASSERT_TRUE(online_perf_json(&w, &now, &before, 4000000000U, json, sizeof json) > 0U);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_buckets_are_log2_microseconds);
    RUN_TEST(test_the_window_has_the_fixtures_shape_and_sends_growth);
    RUN_TEST(test_a_window_that_does_not_fit_is_refused_whole);
    return UNITY_END();
}
