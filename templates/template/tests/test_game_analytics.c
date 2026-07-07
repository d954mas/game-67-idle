/* Unity ctest for the E4 local analytics writer (game_analytics). Native, sink + wall clock
   injected (GAME_ANALYTICS_TESTING). Emits through the COMMITTED golden mini events + the
   built-in log type, drives the recorder, and asserts by re-parsing the captured NDJSON.

   Label-agnostic (like test_game_event_render, HIGH-1): nt_hash inherits the preset's
   NT_HASH_LABELS. For REGISTERED events the renderer emits desc->name unconditionally (a
   literal), so type/msg expectations are exact; hash-VALUE fields (kind/blob depth) are left
   to test_game_event_render. i64 rides as a STRING ("42"), never a double. */
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* clang-format off */
#include "cJSON.h"
#include "game_analytics.h"
#include "game_events.h"
#include "game_log.h"
#include "hash/nt_hash.h"
#include "mini_state_events.gen.h"
#include "unity.h"
/* clang-format on */

/* ---- injected capture sink ---- */
static char g_cap[64 * 1024];
static size_t g_cap_len;

static void capture_sink(const char *bytes, size_t len) {
    if (g_cap_len + len < sizeof g_cap) {
        memcpy(g_cap + g_cap_len, bytes, len);
        g_cap_len += len;
        g_cap[g_cap_len] = '\0';
    }
}

static int64_t fixed_wall(void) { return 1720000000000LL; }

/* Copy the first '\n'-delimited line of the capture into dst (NUL-terminated). */
static void first_line(char *dst, size_t cap) {
    size_t i = 0;
    while (i < g_cap_len && g_cap[i] != '\n') {
        ++i;
    }
    size_t n = i;
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(dst, g_cap, n);
    dst[n] = '\0';
}

/* Copy the last non-empty '\n'-delimited line of the capture into dst. */
static void last_line(char *dst, size_t cap) {
    size_t end = g_cap_len;
    while (end > 0 && g_cap[end - 1u] == '\n') {
        --end;
    }
    size_t start = end;
    while (start > 0 && g_cap[start - 1u] != '\n') {
        --start;
    }
    size_t n = end - start;
    if (n >= cap) {
        n = cap - 1u;
    }
    memcpy(dst, g_cap + start, n);
    dst[n] = '\0';
}

/* Count non-empty lines in the capture. */
static int line_count(void) {
    int count = 0;
    size_t i = 0;
    while (i < g_cap_len) {
        if (g_cap[i] != '\n') {
            ++count;
            while (i < g_cap_len && g_cap[i] != '\n') {
                ++i;
            }
        } else {
            ++i;
        }
    }
    return count;
}

void setUp(void) {
    game_events_init();
    g_cap_len = 0;
    g_cap[0] = '\0';
    game_analytics__set_sink_for_test(capture_sink);
    game_analytics__set_clock_for_test(fixed_wall);
    game_analytics_register_descs(mini_ev_descs, mini_ev_desc_count);
    game_analytics_init();
}

void tearDown(void) {
    game_analytics_shutdown();
    game_events_shutdown();
}

/* 1. session header is the first captured line. */
static void test_header(void) {
    char hdr[512];
    first_line(hdr, sizeof hdr);
    cJSON *root = cJSON_Parse(hdr);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("analytics.v1", cJSON_GetObjectItem(root, "schema")->valuestring);
    TEST_ASSERT_EQUAL_STRING("header", cJSON_GetObjectItem(root, "kind")->valuestring);
    TEST_ASSERT_EQUAL_STRING("template_test", cJSON_GetObjectItem(root, "app")->valuestring);
    TEST_ASSERT_TRUE(cJSON_GetObjectItem(root, "started_at")->valuedouble == 1720000000000.0);
    cJSON_Delete(root);
}

/* 2. event serialization reuses the E3 renderer; assertions are post-fix E3 (i64 -> string). */
static void test_event_serialization(void) {
    const uint8_t blob[3] = {1, 2, 3};
    (void)mini_emit_cell_spawned(42, 3.5, nt_hash64_str("Epic"), true, "hello", blob, 3);
    game_analytics_record();
    game_analytics_flush();

    char line[1024];
    last_line(line, sizeof line);
    cJSON *root = cJSON_Parse(line);
    TEST_ASSERT_NOT_NULL(root);

    /* type is desc->name unconditionally (renderer literal), independent of NT_HASH_LABELS. */
    TEST_ASSERT_EQUAL_STRING(mini_ev_cell_spawned_desc.name,
                             cJSON_GetObjectItem(root, "type")->valuestring);

    const cJSON *total = cJSON_GetObjectItem(root, "total"); /* i64 -> STRING */
    TEST_ASSERT_TRUE(cJSON_IsString(total));
    TEST_ASSERT_EQUAL_STRING("42", total->valuestring);

    const cJSON *rate = cJSON_GetObjectItem(root, "rate"); /* float -> number */
    TEST_ASSERT_TRUE(cJSON_IsNumber(rate));
    TEST_ASSERT_TRUE(fabs(rate->valuedouble - 3.5) < 1e-9);

    const cJSON *label = cJSON_GetObjectItem(root, "label");
    TEST_ASSERT_TRUE(cJSON_IsString(label));
    TEST_ASSERT_EQUAL_STRING("hello", label->valuestring);

    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(root, "seq")));
    TEST_ASSERT_TRUE(cJSON_IsNumber(cJSON_GetObjectItem(root, "tick")));
    cJSON_Delete(root);
}

/* 3. NDJSON framing: 3 events -> header + 3 lines, each valid JSON. */
static void test_ndjson_framing(void) {
    (void)mini_emit_ticked(1);
    (void)mini_emit_ticked(2);
    (void)mini_emit_ticked(3);
    game_analytics_record();
    game_analytics_flush();

    TEST_ASSERT_EQUAL_INT(4, line_count()); /* header + 3 */

    /* every non-empty line parses */
    char work[64 * 1024];
    memcpy(work, g_cap, g_cap_len + 1u);
    int parsed = 0;
    for (char *tok = strtok(work, "\n"); tok; tok = strtok(NULL, "\n")) {
        cJSON *o = cJSON_Parse(tok);
        TEST_ASSERT_NOT_NULL(o);
        cJSON_Delete(o);
        ++parsed;
    }
    TEST_ASSERT_EQUAL_INT(4, parsed);
}

/* 4. buffering: enough events to cross GAME_ANALYTICS_FLUSH_BYTES -> record flushes to the
   sink WITHOUT an explicit flush call, and the buffer never grows unbounded. */
static void test_buffer_threshold(void) {
    const size_t before = g_cap_len; /* header already flushed at init */
    for (int i = 0; i < 5; ++i) {
        (void)mini_emit_ticked(i);
    }
    game_analytics_record(); /* no explicit flush */
    TEST_ASSERT_TRUE(g_cap_len > before);       /* threshold flush happened during record */
    TEST_ASSERT_EQUAL_UINT64(0, game_analytics_dropped());
}

/* 5. drop policy: a rendered line larger than the buffer is dropped, no crash, earlier lines
   intact. GAME_ANALYTICS_BUF_BYTES=256 in the ctest; a ~300-char label renders > 256 yet
   < GAME_ANALYTICS_LINE_MAX (512) so the renderer returns a full line, not a truncated stub. */
static void test_drop_policy(void) {
    char big[301];
    memset(big, 'a', sizeof big - 1u);
    big[sizeof big - 1u] = '\0';
    (void)mini_emit_cell_spawned(1, 1.0, nt_hash64_str("Rare"), false, big, NULL, 0);
    game_analytics_record();
    TEST_ASSERT_TRUE(game_analytics_dropped() > 0);

    /* earlier data (the header) is intact and valid */
    char hdr[512];
    first_line(hdr, sizeof hdr);
    cJSON *root = cJSON_Parse(hdr);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("header", cJSON_GetObjectItem(root, "kind")->valuestring);
    cJSON_Delete(root);
}

/* 6. multi-frame: record frame A (2 events) -> frame_reset -> record frame B (1) -> flush.
   Capture = header + 3 event lines; seq is monotonic across the frame boundary. */
static void test_multi_frame(void) {
    (void)mini_emit_ticked(10);
    (void)mini_emit_ticked(11);
    game_analytics_record();
    game_event_frame_reset();
    (void)mini_emit_ticked(12);
    game_analytics_record();
    game_analytics_flush();

    TEST_ASSERT_EQUAL_INT(4, line_count()); /* header + 3 */

    /* parse the 3 event lines, assert seq strictly increasing */
    char work[64 * 1024];
    memcpy(work, g_cap, g_cap_len + 1u);
    double prev_seq = -1.0;
    int events = 0;
    for (char *tok = strtok(work, "\n"); tok; tok = strtok(NULL, "\n")) {
        cJSON *o = cJSON_Parse(tok);
        TEST_ASSERT_NOT_NULL(o);
        const cJSON *kind = cJSON_GetObjectItem(o, "kind");
        if (kind && cJSON_IsString(kind) && strcmp(kind->valuestring, "header") == 0) {
            cJSON_Delete(o);
            continue; /* skip the header line */
        }
        const cJSON *seq = cJSON_GetObjectItem(o, "seq");
        TEST_ASSERT_TRUE(cJSON_IsNumber(seq));
        TEST_ASSERT_TRUE(seq->valuedouble > prev_seq);
        prev_seq = seq->valuedouble;
        ++events;
        cJSON_Delete(o);
    }
    TEST_ASSERT_EQUAL_INT(3, events);
}

/* 7. built-in log type: register game_log_descs, emit, render as { type:"log", msg:"hi" }. */
static void test_log_type(void) {
    game_analytics_register_descs(game_log_descs, game_log_desc_count);
    (void)game_log_emit("hi");
    game_analytics_record();
    game_analytics_flush();

    char line[1024];
    last_line(line, sizeof line);
    cJSON *root = cJSON_Parse(line);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_EQUAL_STRING("log", cJSON_GetObjectItem(root, "type")->valuestring);
    TEST_ASSERT_EQUAL_STRING("hi", cJSON_GetObjectItem(root, "msg")->valuestring);
    cJSON_Delete(root);
}

int main(void) {
    nt_hash_init(&(nt_hash_desc_t){0}); /* once: type-hash accessors + labels need hash init */

    UNITY_BEGIN();
    RUN_TEST(test_header);
    RUN_TEST(test_event_serialization);
    RUN_TEST(test_ndjson_framing);
    RUN_TEST(test_buffer_threshold);
    RUN_TEST(test_drop_policy);
    RUN_TEST(test_multi_frame);
    RUN_TEST(test_log_type);
    const int result = UNITY_END();

    nt_hash_shutdown();
    return result;
}
