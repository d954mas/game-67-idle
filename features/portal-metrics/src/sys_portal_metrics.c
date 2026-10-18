#include "systems/sys_portal_metrics.h"

#include "features/platform_sdk/platform_sdk_measure.h"

static portal_metrics_map_fn s_map;
static void *s_map_userdata;
static uint32_t s_tick;
static int s_cursor;

void sys_portal_metrics_init(portal_metrics_map_fn map, void *userdata) {
    s_map = map;
    s_map_userdata = userdata;
    s_tick = game_events_tick();
    s_cursor = 0;
}

void sys_portal_metrics_measure(const char *category, const char *what, const char *action) {
    (void)platform_sdk_measure(category, what, action);
}

void sys_portal_metrics_record(void) {
    int count = 0;
    const game_event_t *log;
    if (s_map == NULL) return;
    log = game_event_log(&count);
    if (log == NULL) return;
    /* The cursor survives a frame only through its tick: the log is rebuilt
       every frame, and a count that merely shrank would replay old events. */
    if (game_events_tick() != s_tick) {
        s_tick = game_events_tick();
        s_cursor = 0;
    }
    for (; s_cursor < count; ++s_cursor) {
        s_map(&log[s_cursor], s_map_userdata);
    }
}
