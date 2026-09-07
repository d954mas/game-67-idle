/* The wire format is a contract with a collector nobody in the build can see,
   and a flush that never fires fails silently for a whole playtest. Both are
   pinned here on an injected transport. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "features/telemetry/telemetry.h"
#include "unity.h"

#define CAP 8

static struct {
    char body[CAP][16384];
    uint32_t length[CAP];
    int sent;
    int answer;      /* what poll returns once a request is polled */
    int poll_calls;
    int released;
    bool refuse;     /* send returns NULL */
} g;

static void *fake_send(const char *url, const char *body, uint32_t length, void *userdata) {
    (void)userdata;
    TEST_ASSERT_EQUAL_STRING("https://collector.test/ingest", url);
    if (g.sent < CAP) {
        memcpy(g.body[g.sent], body, length);
        g.body[g.sent][length] = '\0';
        g.length[g.sent] = length;
    }
    g.sent += 1;
    return g.refuse ? NULL : &g;
}

static int fake_poll(void *handle, void *userdata) {
    (void)handle;
    (void)userdata;
    g.poll_calls += 1;
    return g.answer;
}

static void fake_release(void *handle, void *userdata) {
    (void)handle;
    (void)userdata;
    g.released += 1;
}

static const telemetry_transport_t k_fake = {.send = fake_send, .poll = fake_poll, .release = fake_release};

static telemetry_config_t config(void) {
    return (telemetry_config_t){.url = "https://collector.test/ingest", .key = "k1", .game = "example-game",
                                .build = "v11", .platform = "poki", .player = "0123456789abcdef0123456789abcdef",
                                .flush_interval_s = 5.0F, .heartbeat_s = 10.0F};
}

void setUp(void) {
    memset(&g, 0, sizeof g);
    g.answer = 1;
}

void tearDown(void) { telemetry_shutdown(); }

static void test_dormant_without_a_collector(void) {
    telemetry_config_t c = config();
    c.url = "";
    telemetry_init(&c, &k_fake);
    TEST_ASSERT_FALSE(telemetry_enabled());
    telemetry_event_begin("level");
    telemetry_int("level", 3);
    telemetry_event_end();
    for (int i = 0; i < 600; ++i) telemetry_update(0.1F, true);
    telemetry_flush();
    TEST_ASSERT_EQUAL_INT(0, g.sent);
    TEST_ASSERT_EQUAL_STRING("", telemetry_session_id());
}

static void test_batch_wire_format(void) {
    telemetry_config_t c = config();
    telemetry_init(&c, &k_fake);
    TEST_ASSERT_TRUE(telemetry_enabled());
    TEST_ASSERT_EQUAL_UINT(32, strlen(telemetry_session_id()));
    telemetry_update(1.5F, true);
    telemetry_event_begin("level");
    telemetry_int("level", 3);
    telemetry_str("a", "complete");
    telemetry_float("sec", 12.345);
    telemetry_int("boss", 0);
    telemetry_event_end();
    telemetry_event_begin("buy");
    telemetry_str("note", "quote\"back\\slash");
    telemetry_event_end();
    telemetry_flush();
    TEST_ASSERT_EQUAL_INT(1, g.sent);
    const char *b = g.body[0];
    TEST_ASSERT_NOT_NULL(strstr(b, "{\"v\":1,\"key\":\"k1\",\"game\":\"example-game\",\"build\":\"v11\",\"platform\":\"poki\","
                                  "\"player\":\"0123456789abcdef0123456789abcdef\",\"session\":\""));
    TEST_ASSERT_NOT_NULL(strstr(b, "\"sent_at\":"));
    TEST_ASSERT_NOT_NULL(strstr(b, "\"events\":[{\"t\":1.50,\"n\":\"level\",\"level\":3,\"a\":\"complete\",\"sec\":12.35,\"boss\":0},"
                                  "{\"t\":1.50,\"n\":\"buy\",\"note\":\"quote\\\"back\\\\slash\"}]}"));
    TEST_ASSERT_EQUAL_UINT(strlen(b), g.length[0]);
    TEST_ASSERT_EQUAL_UINT64(0U, telemetry_dropped());
}

static void test_flush_on_interval_and_heartbeat_while_playing(void) {
    telemetry_config_t c = config();
    telemetry_init(&c, &k_fake);
    /* Not playing: no heartbeat, nothing to send, no request. */
    for (int i = 0; i < 100; ++i) telemetry_update(0.1F, false);
    TEST_ASSERT_EQUAL_INT(0, g.sent);
    /* Playing 10 s: one heartbeat with play=10, flushed at the 5 s interval
       that follows it. */
    for (int i = 0; i < 100; ++i) telemetry_update(0.1F, true);
    for (int i = 0; i < 60; ++i) telemetry_update(0.1F, false);
    TEST_ASSERT_EQUAL_INT(1, g.sent);
    TEST_ASSERT_NOT_NULL(strstr(g.body[0], "\"n\":\"heartbeat\",\"play\":10.00}"));
    TEST_ASSERT_EQUAL_INT(1, g.released);
}

static void test_one_request_in_flight_and_retry_then_give_up(void) {
    telemetry_config_t c = config();
    telemetry_init(&c, &k_fake);
    g.answer = 0; /* pending forever for now */
    telemetry_event_begin("merge");
    telemetry_event_end();
    telemetry_flush();
    TEST_ASSERT_EQUAL_INT(1, g.sent);
    telemetry_event_begin("merge");
    telemetry_event_end();
    telemetry_flush();
    for (int i = 0; i < 200; ++i) telemetry_update(0.1F, true);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, g.sent, "a second POST went out while the first was in flight");
    /* The answer is a failure: the same body is retried after a backoff,
       three times in all, then dropped -- and the events buffered meanwhile
       go out afterwards (not playing, so no heartbeat keeps the queue fed). */
    g.answer = -1;
    for (int i = 0; i < 400; ++i) telemetry_update(0.1F, false);
    /* Three for the first batch; the batch buffered meanwhile then takes its own three. */
    TEST_ASSERT_TRUE(g.sent >= 3);
    TEST_ASSERT_EQUAL_STRING(g.body[0], g.body[1]);
    TEST_ASSERT_EQUAL_STRING(g.body[0], g.body[2]);
    TEST_ASSERT_TRUE(telemetry_dropped() >= 1U);
    g.answer = 1;
    for (int i = 0; i < 100; ++i) telemetry_update(0.1F, false);
    TEST_ASSERT_TRUE(g.sent >= 4);
    TEST_ASSERT_NOT_NULL(strstr(g.body[3], "\"n\":\"merge\""));
}

static void test_overflow_drops_whole_events_and_counts_them(void) {
    telemetry_config_t c = config();
    telemetry_init(&c, &k_fake);
    for (int i = 0; i < 2000; ++i) {
        telemetry_event_begin("level");
        telemetry_int("level", i);
        telemetry_str("a", "start");
        telemetry_event_end();
    }
    TEST_ASSERT_TRUE(telemetry_dropped() > 0U);
    telemetry_flush();
    TEST_ASSERT_EQUAL_INT(1, g.sent);
    /* Every event that made it is whole: braces balance and the array closes. */
    int depth = 0;
    for (const char *p = g.body[0]; *p; ++p) {
        if (*p == '{') ++depth;
        if (*p == '}') --depth;
        TEST_ASSERT_TRUE(depth >= 0);
    }
    TEST_ASSERT_EQUAL_INT(0, depth);
    TEST_ASSERT_EQUAL_STRING("]}", g.body[0] + strlen(g.body[0]) - 2);
}

static void test_send_refused_drops_and_stays_quiet(void) {
    telemetry_config_t c = config();
    telemetry_init(&c, &k_fake);
    g.refuse = true;
    telemetry_event_begin("level");
    telemetry_event_end();
    telemetry_flush();
    TEST_ASSERT_EQUAL_INT(1, g.sent);
    TEST_ASSERT_EQUAL_UINT64(1U, telemetry_dropped());
    for (int i = 0; i < 100; ++i) telemetry_update(0.1F, false);
    TEST_ASSERT_EQUAL_INT(1, g.sent);
}

static void test_ids_are_32_hex_and_differ(void) {
    char a[33], b[33];
    telemetry_make_id(a);
    telemetry_make_id(b);
    TEST_ASSERT_EQUAL_UINT(32, strlen(a));
    TEST_ASSERT_EQUAL_UINT(32, strlen(b));
    for (int i = 0; i < 32; ++i) TEST_ASSERT_TRUE((a[i] >= '0' && a[i] <= '9') || (a[i] >= 'a' && a[i] <= 'f'));
    TEST_ASSERT_TRUE(strcmp(a, b) != 0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_dormant_without_a_collector);
    RUN_TEST(test_batch_wire_format);
    RUN_TEST(test_flush_on_interval_and_heartbeat_while_playing);
    RUN_TEST(test_one_request_in_flight_and_retry_then_give_up);
    RUN_TEST(test_overflow_drops_whole_events_and_counts_them);
    RUN_TEST(test_send_refused_drops_and_stays_quiet);
    RUN_TEST(test_ids_are_32_hex_and_differ);
    return UNITY_END();
}
