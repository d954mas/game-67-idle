/* Unity ctest for the standalone game_events -> nt_log mirror. This intentionally
   does not enable DevAPI: ordinary debug/local builds should be able to show
   `[ev] ...` without the DevAPI tail command. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "game_events.h"
#include "game_events_log_mirror.h"
#include "hash/nt_hash.h"
#include "log/nt_log.h"
#include "mini_state_events.gen.h"
#include "unity.h"

static char s_last_line[NT_LOG_BUF_SIZE];
static int s_mirror_lines;

static void capture_sink(nt_log_level_t level, const char *domain, const char *msg, void *user) {
    (void)domain;
    (void)user;
    if (level != NT_LOG_LEVEL_INFO || strncmp(msg, "[ev] ", 5) != 0) {
        return;
    }
    (void)snprintf(s_last_line, sizeof s_last_line, "%s", msg);
    s_mirror_lines++;
}

void setUp(void) {
    game_events_init();
    s_last_line[0] = '\0';
    s_mirror_lines = 0;
    nt_log_set_level(NT_LOG_LEVEL_INFO);
    nt_log_add_sink(capture_sink, NULL);
}

void tearDown(void) {
    nt_log_remove_sink(capture_sink, NULL);
    game_events_shutdown();
}

static void test_mirror_logs_typed_event_without_devapi(void) {
    (void)mini_emit_ticked(7);

    game_events_log_mirror_record();

    TEST_ASSERT_EQUAL_INT(1, s_mirror_lines);
    TEST_ASSERT_NOT_NULL(strstr(s_last_line, "[ev] "));
    TEST_ASSERT_NOT_NULL(strstr(s_last_line, mini_ev_ticked_desc.name));
    TEST_ASSERT_NOT_NULL(strstr(s_last_line, "\"count\":7"));
}

int main(void) {
    nt_hash_init(&(nt_hash_desc_t){0});
    game_events_log_mirror_register_descs(mini_ev_descs, mini_ev_desc_count);

    UNITY_BEGIN();
    RUN_TEST(test_mirror_logs_typed_event_without_devapi);
    const int result = UNITY_END();

    nt_hash_shutdown();
    return result;
}
