#include "online_room/schedule.h"

void online_schedule_init(online_tick_schedule *schedule, double now, uint32_t hz) {
    *schedule = (online_tick_schedule){.start = now, .hz = hz};
}

double online_schedule_time(const online_tick_schedule *schedule, uint64_t tick) {
    return schedule->start + (double)tick / (double)schedule->hz;
}

uint32_t online_schedule_take(online_tick_schedule *schedule, double now, uint32_t catch_up, uint64_t *first) {
    *first = schedule->next;
    uint32_t count = 0U;
    while (count < catch_up && now >= online_schedule_time(schedule, schedule->next)) {
        if (now - online_schedule_time(schedule, schedule->next) >= 1.0 / (double)schedule->hz) { schedule->late += 1U; }
        schedule->next += 1U;
        count += 1U;
    }
    if (count == catch_up && now >= online_schedule_time(schedule, schedule->next)) {
        // Still behind after the catch-up: give the rest up and start over at now.
        const double behind = now - online_schedule_time(schedule, schedule->next);
        const uint64_t skip = (uint64_t)(behind * (double)schedule->hz) + 1U;
        schedule->dropped += skip;
        schedule->next += skip;
    }
    return count;
}

double online_schedule_delay(const online_tick_schedule *schedule, double now) {
    const double delay = online_schedule_time(schedule, schedule->next) - now;
    return delay > 0.0 ? delay : 0.0;
}
